/* SPDX-License-Identifier: GPL-2.0 or Linux-OpenIB */
/* Copyright (C) 2019-2026 Intel Corporation */
#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/atomic.h>
#include <linux/workqueue.h>

#include "type.h"
#include "virtchnl.h"

#define IRDMA_TEL_QUEUE_SIZE 32
#ifdef static_assert
static_assert((IRDMA_TEL_QUEUE_SIZE & (IRDMA_TEL_QUEUE_SIZE - 1)) == 0,
              "IRDMA_TEL_QUEUE_SIZE must be a power of 2");
#endif
#define IRDMA_TEL_FLUSH_INTERVAL_MS 1000
#define IRDMA_TEL_MAX_EVENTS_PER_FLUSH 20
#define IRDMA_TEL_MAX_EVENT_DATA_LEN 480

struct irdma_tel_aeqe_raw_event {
	u16 ae_id;
	u16 qp_cq_id;
	u8  ae_src;
	u8  reserved[3];
	u64 raw_aeqe[2];
} __packed;

#ifdef static_assert
static_assert(sizeof(struct irdma_tel_aeqe_raw_event) == 24,
             "Irdma telemetry aeqe raw event size mismatch");
#endif

enum irdma_tel_event_type {
    IRDMA_TEL_EVT_DROP_NOTIFY = 0,
    IRDMA_TEL_EVT_AE = 1,
};

struct irdma_tel_event {
    u8 type; /* enum irdma_tel_event_type */
    u32 seq_num;
    u64 timestamp_ns;
    u16 data_len;
    u8 rsvd;
    char data[];
} __packed;

#ifdef static_assert
static_assert(sizeof(struct irdma_tel_event) + IRDMA_TEL_MAX_EVENT_DATA_LEN +
              sizeof(struct irdma_vchnl_op_buf) <=
             (int)IRDMA_VCHNL_MAX_MSG_SIZE,
	         "Irdma virtchnl messsage len mismatch");
#endif

struct irdma_tel_event_entry {
    struct irdma_tel_event hdr;
    char data_buf[IRDMA_TEL_MAX_EVENT_DATA_LEN];
};

struct irdma_telemetry { /* Ring buffer */
    struct irdma_tel_event_entry telq[IRDMA_TEL_QUEUE_SIZE];
    u32 head;
    u32 tail;
    spinlock_t lock;

    /* Counters */
    atomic_t next_seq;
    atomic_t dropped;

    /* Async flush */
    struct delayed_work flush_work;
    struct irdma_pci_f *rf;
    bool enabled;
};

struct irdma_pci_f;

int irdma_tel_init(struct irdma_pci_f *rf);

void irdma_tel_deinit(struct irdma_pci_f *rf);

void irdma_tel_push_event(struct irdma_pci_f *rf,
                          enum irdma_tel_event_type type,
                          const char* data, u16 data_len);

int irdma_tel_send_aeq_events(struct irdma_pci_f *rf,
                             struct irdma_aeqe_info *aeqe_info);

#endif /* TELEMETRY_H */
