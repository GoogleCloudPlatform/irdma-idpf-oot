#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/atomic.h>
#include <linux/workqueue.h>
#include <linux/kernel.h>

#include "type.h"
#include "main.h"
#include "virtchnl.h"
#include "telemetry.h"

static void tel_pack_aeq_event(const struct irdma_aeqe_info *src,
                               struct irdma_tel_aeqe_raw_event *dest) {
    memset(dest, 0, sizeof(*dest));
    dest->ae_id = src->ae_id;
    dest->qp_cq_id = (u16)src->qp_cq_id;
    dest->ae_src = src->ae_src;
    dest->raw_aeqe[0] = src->raw_aeqe[0];
    dest->raw_aeqe[1] = src->raw_aeqe[1];
}

/**
 * tel_ring_push_locked - Write one event into the ring buffer
 * @tel: telemetry ring
 * @type: type of the event
 * @data_str: event message
 * @data_len: length of the message
 * Returns true if the event is inserted, false if the queue is full
 */
static bool tel_ring_push_locked(struct irdma_telemetry *tel,
                                enum irdma_tel_event_type type,
                                const char* data_str, u16 data_len) {
    struct irdma_tel_event_entry *entry;
    struct irdma_tel_event *evt;
    u16 copy_len = 0;

    if (tel->head - tel->tail == IRDMA_TEL_QUEUE_SIZE) {
        atomic_inc(&tel->dropped);
        return false;
    }

    copy_len = min_t(u16, data_len, IRDMA_TEL_MAX_EVENT_DATA_LEN);
    entry = &tel->telq[tel->head & (IRDMA_TEL_QUEUE_SIZE - 1)];
    evt = &entry->hdr;
    evt->type = (u8)type;
    evt->seq_num = atomic_fetch_add(1, &tel->next_seq);
    evt->timestamp_ns = ktime_get_real_ns();
    evt->data_len = copy_len;

    if (copy_len)
        memcpy(evt->data, data_str, copy_len);

    tel->head++;

    return true;
}

/**
 * tel_ring_pop_locked - Read one event from the ring buffer
 * @tel: Telemetry ring
 * @evt: Dequeued Event
 * Returns false if ring is empty, true is event is read successfully
 */
static bool tel_ring_pop_locked(struct irdma_telemetry *tel,
                                struct irdma_tel_event *evt) {
    struct irdma_tel_event_entry *entry;
    size_t total_size;

    if (tel->head == tel->tail)
        return false;

    entry = &tel->telq[tel->tail & (IRDMA_TEL_QUEUE_SIZE - 1)];
    total_size = sizeof(*evt) + entry->hdr.data_len;
    memcpy(evt, &entry->hdr, total_size);
    tel->tail++;

    return true;
}

/**
 * irdma_tel_send_aeq_events - Push AEQ events to control plane
 * @rf: RDMA PCI Function
 * @aeqe_info: AEQ event info
 * Returns 0 on success, negative errno on failure
 */
int irdma_tel_send_aeq_events(struct irdma_pci_f *rf,
                             struct irdma_aeqe_info *aeqe_info) {
    struct irdma_telemetry *tel = &rf->telemetry;
    struct irdma_tel_aeqe_raw_event buffer;

    if (!READ_ONCE(tel->enabled))
        return 0;

    tel_pack_aeq_event(aeqe_info, &buffer);
    irdma_tel_push_event(rf, IRDMA_TEL_EVT_AE, (void *)&buffer, sizeof(buffer));

    return 0;
}

/**
 * irdma_tel_push_event - Format and enqueue a telemetry event
 * If the ring buffer is full it drops the incoming event
 * increment the dropped counter.
 *
 * @rf: RDMA PCI Function
 * @type: Telemetry Event Type
 * @data: Error/Event data
 * @data_len: Event data length
 */
void irdma_tel_push_event(struct irdma_pci_f *rf,
                          enum irdma_tel_event_type type,
                          const char* data, u16 data_len) {
    struct irdma_telemetry *tel = &rf->telemetry;
    unsigned long flags;

    if (!READ_ONCE(tel->enabled))
        return;

    spin_lock_irqsave(&tel->lock, flags);
    if (!tel_ring_push_locked(tel, type, data, data_len)) {
        spin_unlock_irqrestore(&tel->lock, flags);
        return;
    }
    spin_unlock_irqrestore(&tel->lock, flags);
}

/**
 * tel_get_drop_event - Get a event, informing number of drops
 * to HMA
 * @tel: telemetry ring
 * @evt: Event
 * @drops: Number of dropped events
 */
static void tel_get_drop_event(struct irdma_telemetry *tel,
                               struct irdma_tel_event *evt,
                               int drops) {
    memset(evt, 0, sizeof(*evt));
    evt->type = IRDMA_TEL_EVT_DROP_NOTIFY;
    evt->seq_num = atomic_fetch_add(1, &tel->next_seq);
    evt->timestamp_ns = ktime_get_real_ns();
    evt->data_len = sizeof(drops);
    memcpy(evt->data, &drops, sizeof(drops));
}

/**
 * irdma_tel_flush_worker() - Drain the telemetry ring and send the
 * events to control plane.
 * @work: Work to perform on action required
 */
static void irdma_tel_flush_worker(struct work_struct *work){
    struct irdma_tel_event *evt = NULL;
    struct irdma_telemetry *tel;
    struct irdma_pci_f *rf;
    unsigned long flags;
    int drops = 0;
    u32 sent = 0;

    tel = container_of(work, struct irdma_telemetry, flush_work.work);
    rf = tel->rf;
    drops = atomic_read(&tel->dropped);
    spin_lock_irqsave(&tel->lock, flags);
    if (tel->head == tel->tail && !drops){
        spin_unlock_irqrestore(&tel->lock, flags);
        goto reschedule;
    }
    spin_unlock_irqrestore(&tel->lock, flags);

    if (!READ_ONCE(tel->enabled))
        return;

    evt = kzalloc(sizeof(*evt) + IRDMA_TEL_MAX_EVENT_DATA_LEN, GFP_KERNEL);
    if(!evt){
        goto reschedule;
    }

    while (sent < IRDMA_TEL_MAX_EVENTS_PER_FLUSH){
        spin_lock_irqsave(&tel->lock, flags);
        if (!tel_ring_pop_locked(tel, evt)) {
            spin_unlock_irqrestore(&tel->lock, flags);
            break;
        }
        spin_unlock_irqrestore(&tel->lock, flags);
        if (irdma_vchnl_push_tel_events(&rf->sc_dev, evt)){
            break;
        }
        sent++;
    }

    drops = atomic_xchg(&tel->dropped, 0);
    if (drops) {
        tel_get_drop_event(tel, evt, drops);
        if (irdma_vchnl_push_tel_events(&rf->sc_dev, evt)){
            atomic_add(drops, &tel->dropped);
        }
    }

reschedule:
    /* kfree(NULL) is a no-op if evt was not allocated */
    kfree(evt);
    if (READ_ONCE(tel->enabled))
        schedule_delayed_work(&tel->flush_work,
                msecs_to_jiffies(IRDMA_TEL_FLUSH_INTERVAL_MS));
}

/**
 * irdma_tel_init - Initialises the telemetry queue
 *
 * @rf: RDMA PCI Function
 * Must be called during probe after virtchnl mailbox is setup.
 * Returns 0 on success, negative errno on failure
*/
int irdma_tel_init(struct irdma_pci_f *rf) {
    struct irdma_telemetry *tel = &rf->telemetry;
    struct irdma_sc_dev *dev = &rf->sc_dev;

    if(!rf)
        return -EINVAL;

    if (!FIELD_GET(IRDMA_TEL_EVENTS_EN, dev->vc_caps.feature_cap)) {
	    pr_info("irdma: Pushing telemetry events is disabled.\n");
	    return 0;
	}

    memset(tel, 0, sizeof(*tel));

    spin_lock_init(&tel->lock);
    atomic_set(&tel->next_seq, 0);
    atomic_set(&tel->dropped, 0);

    tel->rf = rf;

    INIT_DELAYED_WORK(&tel->flush_work, irdma_tel_flush_worker);

    /* Ring size must be power of 2 */
    BUILD_BUG_ON(IRDMA_TEL_QUEUE_SIZE & (IRDMA_TEL_QUEUE_SIZE - 1));

    WRITE_ONCE(tel->enabled, true);
    if (READ_ONCE(tel->enabled))
        schedule_delayed_work(&tel->flush_work,
                    msecs_to_jiffies(IRDMA_TEL_FLUSH_INTERVAL_MS));
    pr_info("irdma: Telemetry queue initialized.\n");
    return 0;
}

/**
 * irdma_tel_deinit - Tear down the telemetry queue and
 * cancels flush worker. Any event remaining in the queue
 * is silently dropped.
 * @rf: RDMA PCI Function
*/
void irdma_tel_deinit(struct irdma_pci_f *rf){
    struct irdma_telemetry *tel;

    if (!rf)
        return;

    tel = &rf->telemetry;
    if (!READ_ONCE(tel->enabled))
        return;

    WRITE_ONCE(tel->enabled, false);
    cancel_delayed_work_sync(&tel->flush_work);
    pr_info("irdma: Telemetry queue deinitialized.\n");
}
