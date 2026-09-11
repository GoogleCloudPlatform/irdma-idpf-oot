// SPDX-License-Identifier: GPL-2.0 or Linux-OpenIB
// SPDX-License-Identifier: GPL-2.0 or Linux-OpenIB
/* Copyright (c) 2023 - 2024 Intel Corporation */

#include "main.h"
#include "ig3rdma_hw.h"

extern unsigned int rdma_key;

static const struct auxiliary_device_id ig3rdma_auxiliary_id_table[] = {
	{.name = "idpf.8086.rdma.core", },
	{},
};

static struct irdma_vchnl_if ig3rdma_vchnl_if_req = {
	.vchnl_recv = irdma_vchnl_req_recv,
};

static void ig3rdma_put_exported_symbols(struct irdma_pci_f *rf)
{
	if (rf->idpf_idc_vport_dev_ctrl_func)
		symbol_put_addr(rf->idpf_idc_vport_dev_ctrl_func);
	if (rf->idpf_idc_request_reset_func)
		symbol_put_addr(rf->idpf_idc_request_reset_func);
	if (rf->idpf_idc_rdma_vc_send_sync_func)
		symbol_put_addr(rf->idpf_idc_rdma_vc_send_sync_func);
}

static int ig3rdma_get_exported_symbols(struct irdma_pci_f *rf)
{
	rf->idpf_idc_vport_dev_ctrl_func = symbol_get(idpf_idc_vport_dev_ctrl);
	if (!rf->idpf_idc_vport_dev_ctrl_func)
		goto err;
	rf->idpf_idc_request_reset_func = symbol_get(idpf_idc_request_reset);
	if (!rf->idpf_idc_request_reset_func)
		goto err;
	rf->idpf_idc_rdma_vc_send_sync_func = symbol_get(idpf_idc_rdma_vc_send_sync);
	if (!rf->idpf_idc_rdma_vc_send_sync_func)
		goto err;

	return 0;
err:
	ig3rdma_put_exported_symbols(rf);
	return -ENOENT;
}

static void ig3rdma_idc_core_event_handler(struct iidc_core_dev_info *cdev_info,
					   struct iidc_event *event)
{
	struct irdma_pci_f *rf = auxiliary_get_drvdata(cdev_info->adev);

	if (*event->type & BIT(IIDC_EVENT_WARN_RESET)) {
		rf->reset = true;
		rf->sc_dev.vchnl_up = false;
		pr_debug("%s: Received event warn reset", __func__);
	}
}

static int ig3rdma_cfg_regions(struct irdma_hw *hw,
			       struct iidc_core_dev_info *cdev_info)
{
	struct iidc_rdma_priv_dev_info *iidc_priv = cdev_info->iidc_priv;
	struct pci_dev *pdev = cdev_info->pdev;
	int i;

	switch (iidc_priv->ftype) {
	case IIDC_FUNCTION_TYPE_PF:
		hw->rdma_reg.len = IG3_PF_RDMA_REGION_LEN;
		hw->rdma_reg.offset = IG3_PF_RDMA_REGION_OFFSET;
		break;
	case IIDC_FUNCTION_TYPE_VF:
		hw->rdma_reg.len = IG3_VF_RDMA_REGION_LEN;
		hw->rdma_reg.offset = IG3_VF_RDMA_REGION_OFFSET;
		break;
	default:
		return -ENODEV;
	}

	hw->rdma_reg.addr = ioremap(pci_resource_start(pdev, 0) + hw->rdma_reg.offset,
				    hw->rdma_reg.len);

	if (!hw->rdma_reg.addr)
		return -ENOMEM;

	hw->num_io_regions = le16_to_cpu(iidc_priv->num_memory_regions);
	hw->io_regs = kcalloc(hw->num_io_regions,
			      sizeof(struct irdma_mmio_region), GFP_KERNEL);

	if (!hw->io_regs) {
		iounmap(hw->rdma_reg.addr);
		return -ENOMEM;
	}

	for (i = 0; i < hw->num_io_regions; i++) {
		hw->io_regs[i].addr =
			iidc_priv->mapped_mem_regions[i].region_addr;
		hw->io_regs[i].len =
			le64_to_cpu(iidc_priv->mapped_mem_regions[i].size);
		hw->io_regs[i].offset =
			le64_to_cpu(iidc_priv->mapped_mem_regions[i].start_offset);
	}

	return 0;
}

static int ig3rdma_vchnl_init(struct irdma_pci_f *rf,
			    struct iidc_core_dev_info *cdev_info, u8 *rdma_ver)
{
	struct iidc_rdma_priv_dev_info *iidc_priv = cdev_info->iidc_priv;
	struct irdma_vchnl_init_info virt_info = {};
	u8 gen = rf->rdma_ver;
	int ret;

	rf->vchnl_wq = alloc_ordered_workqueue("irdma-virtchnl-wq", 0);
	if (!rf->vchnl_wq)
		return -ENOMEM;

	mutex_init(&rf->sc_dev.vchnl_mutex);

	virt_info.hw_rev = gen;
	virt_info.is_pf =
		(iidc_priv->ftype == IIDC_FUNCTION_TYPE_PF) ? true : false;
	virt_info.privileged = false;
	virt_info.vchnl_if = &ig3rdma_vchnl_if_req;
	virt_info.vchnl_wq = rf->vchnl_wq;
	ret = irdma_sc_vchnl_init(&rf->sc_dev, &virt_info);
	if (ret) {
		destroy_workqueue(rf->vchnl_wq);
		return ret;
	}

	*rdma_ver = rf->sc_dev.hw_attrs.uk_attrs.hw_rev;
	return 0;
}

static int ig3rdma_core_fill_device_info(struct irdma_pci_f *rf,
				  struct iidc_core_dev_info *cdev_info)
{
	struct iidc_rdma_priv_dev_info *iidc_priv = cdev_info->iidc_priv;
	int err;

	rf->sc_dev.hw = &rf->hw;
	rf->cdev = cdev_info;
	rf->pcidev = cdev_info->pdev;
	rf->hw.device = &rf->pcidev->dev;
	rf->rdma_ver = IRDMA_GEN_3;
	rf->ftype = iidc_priv->ftype;
	rf->msix_count =  iidc_priv->msix_count;
	rf->msix_entries = iidc_priv->msix_entries;
	err = ig3rdma_vchnl_init(rf, cdev_info, &rf->rdma_ver);
	if (err)
		return err;

	err = ig3rdma_cfg_regions(&rf->hw, cdev_info);
	if (err) {
		destroy_workqueue(rf->vchnl_wq);
		mutex_destroy(&rf->sc_dev.vchnl_mutex);
		return err;
	}

	if (rf->rdma_ver == IRDMA_GEN_3 && cdev_info->pdev->revision < MEV_PCI_VER_C0) {
#define IRDMA_MEV_B0_RDMA_KEY	0xb
		if (rdma_key != IRDMA_MEV_B0_RDMA_KEY) {
			dev_err(rf->hw.device,
				"IRDMA: Invalid RDMA key used for B0\n");
			return -EINVAL;
		}
	}

	mev_enable_hw_wa(&rf->sc_dev, hw_type_wa, wa_mem_pages,
			 hw_wa_bitmask, host_mem_mrte);

	rf->sc_dev.rrf_multiplier = rrf_m;
	rf->sc_dev.xf_multiplier = xf_m;
	rf->sc_dev.min_ird = min_ird;

	if (rf->sc_dev.hw_wa & CEQ_POLL)
		rf->msix_count = 1;

	rf->protocol_used = IRDMA_ROCE_PROTOCOL_ONLY;

	rf->rsrc_profile = IRDMA_HMC_PROFILE_DEFAULT;
	rf->gen_ops.request_reset = irdma_request_reset;
	/* Can override limits_sel, protocol_used */
	irdma_set_rf_user_cfg_params(rf);

	rf->rca_config = irdma_rca_config;
	pr_info("RCA: cfg=%x\n", irdma_rca_config);

	dev_info(rf->hw.device, "%s: feature_cap 0x%016llx\n",
		 __func__, rf->sc_dev.vc_caps.feature_cap);
	if (FIELD_GET(IRDMA_NEED_PERIODIC_FLUSH_BIT, rf->sc_dev.vc_caps.feature_cap)) {
		dev_info(rf->hw.device, "%s: periodic flush enabled\n", __func__);
		rf->sc_dev.periodic_flush = true;
	} else {
		dev_info(rf->hw.device, "%s: periodic flush disabled\n", __func__);
		rf->sc_dev.periodic_flush = false;
	}

	return 0;
}


static void irdma_poll_cq3(struct irdma_pci_f *rf)
{
	struct irdma_cq *cq = rf->cq_id_3;
	struct irdma_cq_uk *ukcq  = &cq->sc_cq.cq_uk;
	u64 qword3;
	__le64 *cqe;
	u8 polarity;

	cqe = IRDMA_GET_CURRENT_CQ_ELEM(ukcq);
	get_64bit_val(cqe, 24, &qword3);
	polarity = (u8)FIELD_GET(IRDMA_CQ_VALID, qword3);

	if (polarity == ukcq->polarity && cq->ibcq.comp_handler)
		cq->ibcq.comp_handler(&cq->ibcq, cq->ibcq.cq_context);
}

#define LOW_FREQ_MICROS  1000
#define HIGH_FREQ_MICROS 25

static int poll_thread(void *context)
{
	struct irdma_pci_f *rf = context;
	u32 sleep_micros = LOW_FREQ_MICROS;

	msleep(200);
	do {
		usleep_range(sleep_micros, sleep_micros + HIGH_FREQ_MICROS);
		if (rf->sc_dev.hw_wa & AEQ_POLL) {
			irdma_process_aeq(rf);
			continue;
		}
		if (rf->sc_dev.hw_wa & CCQ_CQ3_POLL) {
			struct irdma_sc_cq *ccq = &rf->ccq.sc_cq;

			if (ccq)
				irdma_cqp_ce_handler(rf, ccq);
			if (rf->cq_id_3)
				irdma_poll_cq3(rf);
			continue;
		}
		if (rf->sc_dev.hw_wa & CEQ_POLL) {
			if (rf->ceqlist)
				irdma_process_ceq(rf, rf->ceqlist);
			irdma_process_aeq(rf);
		}
		if (atomic_read(&rf->ceq0_wa_enable)) {
			struct irdma_sc_cq *ccq = &rf->ccq.sc_cq;

			/* If there is a backlog, poll faster. The high freq
			 * delay is just enough to allow the user to react to a
			 * completed request and issue another.
			 */
			if (READ_ONCE(rf->sc_dev.cqp->requested_ops) !=
			    atomic64_read(&rf->sc_dev.cqp->completed_ops)) {
				sleep_micros = HIGH_FREQ_MICROS;
			} else {
				sleep_micros = LOW_FREQ_MICROS;
				continue;
			}

			irdma_process_ceq(rf, rf->ceqlist);
			irdma_cqp_ce_handler(rf, ccq);
		}
	} while (!kthread_should_stop());

	return 0;
}

static int ig3rdma_probe(struct auxiliary_device *aux_dev, const struct auxiliary_device_id *id)
{
	struct iidc_auxiliary_dev *iidc_adev = container_of(aux_dev,
							    struct iidc_auxiliary_dev,
							    adev);
	struct iidc_core_dev_info *cdev_info = iidc_adev->cdev_info;
	struct irdma_pci_f *rf;
	int err;

	rf = kzalloc(sizeof(*rf), GFP_KERNEL);
	if (!rf)
		return -ENOMEM;

	err = ig3rdma_get_exported_symbols(rf);
	if (err)
		goto err_get_exported_symbols;

	err = ig3rdma_core_fill_device_info(rf, cdev_info);
	if (err)
		goto err_fill_devinfo;

	err = irdma_ctrl_init_hw(rf);
	if (err)
		goto err_ctrl_init;

	if (rf->rdma_ver >= IRDMA_GEN_3 &&
	    rf->sc_dev.hw_wa & TIMER_NEEDED)
		rf->poll_thread =
			kthread_run(poll_thread, rf, "dpc polling thread");

	dev_info(rf->hw.device, "%s:INIT: Gen[%d] PF[%d] device probe success\n",
		 __func__, rf->rdma_ver, PCI_FUNC(rf->pcidev->devfn));

	auxiliary_set_drvdata(aux_dev, rf);

	err = rf->idpf_idc_vport_dev_ctrl_func(cdev_info, true);
	if (err)
		goto err_vport_ctrl;

	return 0;

err_vport_ctrl:
err_ctrl_init:
	destroy_workqueue(rf->vchnl_wq);
err_fill_devinfo:
	ig3rdma_put_exported_symbols(rf);
err_get_exported_symbols:
	kfree(rf);

	return err;
}

#ifdef HAVE_AUXILIARY_DRIVER_INT_REMOVE
static int ig3rdma_remove(struct auxiliary_device *aux_dev)
#else /* HAVE_AUXILIARY_DRIVER_INT_REMOVE */
static void ig3rdma_remove(struct auxiliary_device *aux_dev)
#endif /* HAVE_AUXILIARY_DRIVER_INT_REMOVE */
{
	struct irdma_pci_f *rf = auxiliary_get_drvdata(aux_dev);
	u8 rdma_ver = rf->rdma_ver;
	struct iidc_auxiliary_dev *iidc_adev = container_of(aux_dev,
							    struct iidc_auxiliary_dev,
							    adev);
	struct iidc_core_dev_info *cdev_info = iidc_adev->cdev_info;

	rf->idpf_idc_vport_dev_ctrl_func(cdev_info, false);

	irdma_ctrl_deinit_hw(rf);
	if (rf->vchnl_wq)
		destroy_workqueue(rf->vchnl_wq);
	ig3rdma_put_exported_symbols(rf);
	kfree(rf);

	pr_debug("INIT: Gen[%d] func[%d] device remove success\n",
		 rdma_ver, PCI_FUNC(cdev_info->pdev->devfn));
#ifdef HAVE_AUXILIARY_DRIVER_INT_REMOVE
	return 0;
#endif /* HAVE_AUXILIARY_DRIVER_INT_REMOVE */
}

MODULE_DEVICE_TABLE(auxiliary, ig3rdma_auxiliary_id_table);

struct iidc_auxiliary_drv ig3rdma_auxiliary_drv = {
	.adrv = {
	    .name = "core",
	    .id_table = ig3rdma_auxiliary_id_table,
	    .probe = ig3rdma_probe,
	    .remove = ig3rdma_remove,
	},
	.event_handler = ig3rdma_idc_core_event_handler,
	.vc_receive = irdma_vchnl_receive,
};

static int ig3rdma_vport_probe(struct auxiliary_device *aux_dev, const struct auxiliary_device_id *id)
{
	struct iidc_rdma_vport_auxiliary_dev *idc_adev =
		container_of(aux_dev, struct iidc_rdma_vport_auxiliary_dev, adev);
	struct auxiliary_device *aux_core_dev = idc_adev->vdev_info->core_adev;
	struct irdma_pci_f *rf = auxiliary_get_drvdata(aux_core_dev);
	struct irdma_l2params l2params = {};
	struct irdma_device *iwdev;
	int err;
	struct irdma_handler *hdl;

	iwdev = ib_alloc_device(irdma_device, ibdev);
	if (!iwdev)
		return -ENOMEM;

	spin_lock_init(&iwdev->ae_info.info_lock);
	mutex_init(&iwdev->ah_tbl_lock);
	spin_lock_init(&iwdev->ah_nosleep_tbl_lock);

	/* Fill iwdev info */
	iwdev->is_vport = true;
	iwdev->rf = rf;
	if (!idc_adev->vdev_info->vport_id)
		rf->iwdev = iwdev;
	iwdev->vport_id = idc_adev->vdev_info->vport_id;
	iwdev->netdev = idc_adev->vdev_info->netdev;
	iwdev->init_state = INITIAL_STATE;
	iwdev->roce_cwnd = IRDMA_ROCE_CWND_DEFAULT;
	iwdev->roce_ackcreds = IRDMA_ROCE_ACKCREDS_DEFAULT;
	iwdev->rcv_wnd = IRDMA_CM_DEFAULT_RCV_WND_SCALED;
	iwdev->rcv_wscale = IRDMA_CM_DEFAULT_RCV_WND_SCALE;
	iwdev->roce_mode = true;
	iwdev->push_mode = false;

	iwdev->aux_dev = aux_dev;

	hdl = kzalloc(sizeof(*hdl), GFP_KERNEL);
	if (!hdl)
		goto err_hdl;

	hdl->iwdev = iwdev;
	iwdev->hdl = hdl;

	l2params.mtu = iwdev->netdev->mtu;
	err = irdma_rt_init_hw(iwdev, &l2params);
	if (err)
		goto err_rt_init;

	irdma_add_handler(hdl);
#ifdef CONFIG_DEBUG_FS
	irdma_dbg_pf_init(hdl);
#endif

	err = irdma_ib_register_device(iwdev);
	if (err)
		goto err_ibreg;

	ibdev_dbg(&iwdev->ibdev, "INIT: Gen[%d] PF[%d] vport_id[%d] vport probe success\n",
		  rf->rdma_ver, PCI_FUNC(rf->pcidev->devfn), iwdev->vport_id);

	auxiliary_set_drvdata(aux_dev, iwdev);

	err = irdma_register_notifiers(iwdev);
	if (err)
		goto err_all;
	return 0;

err_all:
	irdma_ib_unregister_device(iwdev);
err_ibreg:
#ifdef CONFIG_DEBUG_FS
	irdma_dbg_pf_exit(iwdev->hdl);
#endif
	irdma_del_handler(iwdev->hdl);
	irdma_unregister_notifiers(iwdev);
	irdma_rt_deinit_hw(iwdev);
err_rt_init:
	kfree(hdl);
err_hdl:
	ib_dealloc_device(&iwdev->ibdev);

	return err;
}

#ifdef HAVE_AUXILIARY_DRIVER_INT_REMOVE
static int ig3rdma_vport_remove(struct auxiliary_device *aux_dev)
#else /* HAVE_AUXILIARY_DRIVER_INT_REMOVE */
static void ig3rdma_vport_remove(struct auxiliary_device *aux_dev)
#endif /* HAVE_AUXILIARY_DRIVER_INT_REMOVE */
{
	struct iidc_rdma_vport_auxiliary_dev *idc_adev =
		container_of(aux_dev, struct iidc_rdma_vport_auxiliary_dev, adev);
	struct irdma_device *iwdev = auxiliary_get_drvdata(aux_dev);

	ibdev_dbg(&iwdev->ibdev,
		  "INIT: Gen[%d] dev_name = %s, core_dev_name = %s, netdev=%s\n",
		  iwdev->rf->rdma_ver, dev_name(&aux_dev->dev),
		  dev_name(&idc_adev->vdev_info->core_adev->dev),
		  netdev_name(idc_adev->vdev_info->netdev));

	irdma_ib_unregister_device(iwdev);
	irdma_unregister_notifiers(iwdev);
	irdma_deinit_device(iwdev);
	ib_dealloc_device(&iwdev->ibdev);

#ifdef HAVE_AUXILIARY_DRIVER_INT_REMOVE
	return 0;
#endif /* HAVE_AUXILIARY_DRIVER_INT_REMOVE */
}

static void ig3rdma_idc_vport_event_handler(struct iidc_rdma_vport_dev_info *cdev_info,
					    struct iidc_event *event)
{
	struct irdma_device *iwdev = auxiliary_get_drvdata(cdev_info->adev);
	struct irdma_l2params l2params = {};

	if (*event->type & BIT(IIDC_EVENT_AFTER_MTU_CHANGE)) {
		ibdev_dbg(&iwdev->ibdev, "CLNT: new MTU = %d\n", iwdev->netdev->mtu);
		if (iwdev->vsi.mtu != iwdev->netdev->mtu) {
			l2params.mtu = iwdev->netdev->mtu;
			l2params.mtu_changed = true;
			irdma_log_invalid_mtu(l2params.mtu, &iwdev->rf->sc_dev);
			irdma_change_l2params(&iwdev->vsi, &l2params);
		}
	} else {
		ibdev_dbg(&iwdev->ibdev, "Got event 0x%08lx\n", *event->type);
	}
}

static const struct auxiliary_device_id ig3rdma_vport_auxiliary_id_table[] = {
	{.name = "idpf.8086.rdma.vdev", },
	{},
};

MODULE_DEVICE_TABLE(auxiliary, ig3rdma_vport_auxiliary_id_table);

struct iidc_rdma_vport_auxiliary_drv ig3rdma_vport_auxiliary_drv = {
	.adrv = {
		.name = "vdev",
		.id_table = ig3rdma_vport_auxiliary_id_table,
		.probe = ig3rdma_vport_probe,
		.remove = ig3rdma_vport_remove,
	},
	.event_handler = ig3rdma_idc_vport_event_handler,
};
