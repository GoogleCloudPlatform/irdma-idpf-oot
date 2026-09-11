// SPDX-License-Identifier: GPL-2.0 or Linux-OpenIB
/* Copyright (c) 2015 - 2024 Intel Corporation */
#include "main.h"

#define DRV_VER_MAJOR 0
#define DRV_VER_MINOR 0
#define DRV_VER_BUILD 129
#define DRV_VER	__stringify(DRV_VER_MAJOR) "."		\
	__stringify(DRV_VER_MINOR) "." __stringify(DRV_VER_BUILD) "-hvl"

static u8 resource_profile;
module_param(resource_profile, byte, 0444);
MODULE_PARM_DESC(resource_profile, "Resource Profile: 0=PF only(default), 1=Weighted VF, 2=Even Distribution");

static unsigned short max_rdma_vfs = IRDMA_MAX_PE_ENA_VF_COUNT;
module_param(max_rdma_vfs, ushort, 0444);
MODULE_PARM_DESC(max_rdma_vfs, "Maximum VF count: 0-32, default=32");

/* Used for testing in CNV */
bool mod_rdpu_bw;
module_param(mod_rdpu_bw, bool, 0644);
MODULE_PARM_DESC(mod_rdpu_bw, "mod_rdpu_bw, default=false");

bool irdma_upload_context;
module_param(irdma_upload_context, bool, 0644);
MODULE_PARM_DESC(irdma_upload_context, "Upload QP context, default=false");

static unsigned int limits_sel = 3;
module_param(limits_sel, uint, 0444);
MODULE_PARM_DESC(limits_sel, "Resource limits selector, Range: 0-7, default=3");

static unsigned int gen1_limits_sel = 1;
module_param(gen1_limits_sel, uint, 0444);
MODULE_PARM_DESC(gen1_limits_sel, "x722 resource limits selector, Range: 0-5, default=1");

unsigned int dbg_opt;
module_param(dbg_opt, uint, 0444);
MODULE_PARM_DESC(dbg_opt, "set dbg_opt to enable a specific debug option, currently only iWarp Partial FPDUs dump (set bit #1 in dbg_opt) is supported, default=0");

static unsigned int roce_ena;
module_param(roce_ena, uint, 0444);
MODULE_PARM_DESC(roce_ena, "RoCE enable: 1=enable RoCEv2 on all ports (not supported on x722), 0=iWARP(default)");

static ulong roce_port_cfg;
module_param(roce_port_cfg, ulong, 0444);
MODULE_PARM_DESC(roce_port_cfg, "RoCEv2 per port enable: 1=port0 RoCEv2 all others iWARP, 2=port1 RoCEv2 etc. not supported on X722");

static int arr_argc = 4;
static u8 roce_pci_cfg[4];
module_param_array(roce_pci_cfg, byte, &arr_argc, 0444);
MODULE_PARM_DESC(roce_pci_cfg, "RoCEv2 per PCIe bus number enable: RoCEv2 overridden for all devices with given bus number (up to 4).");

static bool en_rem_endpoint_trk;
module_param(en_rem_endpoint_trk, bool, 0444);
MODULE_PARM_DESC(en_rem_endpoint_trk, "Remote Endpoint Tracking: 1=enabled (not supported on x722), 0=disabled(default)");

static u8 timer_bucket_slots = 8;
module_param(timer_bucket_slots, byte, 0444);
MODULE_PARM_DESC(timer_bucket_slots, "timer_bucket_slots, Range: 3-13, default=8");

static u8 fragment_count_limit = 6;
module_param(fragment_count_limit, byte, 0444);
MODULE_PARM_DESC(fragment_count_limit, "adjust maximum values for queue depth and inline data size, default=6, Range: 2-13");

u8 rrf_m = 8;
module_param(rrf_m, byte, 0444);
MODULE_PARM_DESC(rrf_m, "rrf_multiplier for HMC resource default to 8");

u8 xf_m = 16;
module_param(xf_m, byte, 0444);
MODULE_PARM_DESC(xf_m, "xf_multiplier for HMC resource default to 16");

u8 min_ird = 8;
module_param(min_ird, byte, 0444);
MODULE_PARM_DESC(min_ird, "minimum ird for Q1 resources");

bool host_mem_mrte;
module_param(host_mem_mrte, bool, 0444);
MODULE_PARM_DESC(host_mem_mrte, "true if mrte host memory false local memory default = false");

bool irdma_rca_ena;
module_param(irdma_rca_ena, bool, 0444);
MODULE_PARM_DESC(irdma_rca_ena, "driver enable rca default=false");

bool irdma_rca_rq_post = true;
module_param(irdma_rca_rq_post, bool, 0444);
MODULE_PARM_DESC(irdma_rca_rq_post, "Post RQEs at RCA's RQ initialization, default=true");

bool irdma_rca_rq_polarity = true;
module_param(irdma_rca_rq_polarity, bool, 0444);
MODULE_PARM_DESC(irdma_rca_rq_polarity, "Use polarity as Frag_Valid in RCA's RQEs, default=true");

unsigned int irdma_rca_rq_size = IRDMA_CQP_SW_RQSIZE_2048;
module_param(irdma_rca_rq_size, uint, 0444);
MODULE_PARM_DESC(irdma_rca_rq_size, "RCA CQP RQ size, default=2048");

unsigned int irdma_rca_config = IRDMA_RCA_CFG_EXECUTE;
module_param(irdma_rca_config, uint, 0444);
MODULE_PARM_DESC(irdma_rca_config, "RCA configuration, default=EXECUTE");
 
unsigned int wa_mem_pages;
module_param(wa_mem_pages, uint, 0444);
MODULE_PARM_DESC(wa_mem_pages, "to override memory pages in local memory default = 0");

unsigned int hw_type_wa = 0x45;
module_param(hw_type_wa, uint, 0444);
MODULE_PARM_DESC(hw_type_wa, "to setup hw work around for specific release default = Veloce SWR39, if set to 0xFFFF, take hw_wa_bitmask value.");

ulong hw_wa_bitmask;
module_param(hw_wa_bitmask, ulong, 0444);
MODULE_PARM_DESC(hw_wa_bitmask, "Used to manually set HW WAs; default=0");

/*
 * rdma_key - allow rdma load if key is success
 * default is rdma load fail if proper key is not entered
 */
unsigned int rdma_key;
module_param(rdma_key, uint, 0444);
MODULE_PARM_DESC(rdma_key, "Driver probe on GEN3 B0 will fail by default if proper key is not used");

/******************Advanced RoCEv2 congestion knobs***********************************************/
static bool dcqcn_enable;
module_param(dcqcn_enable, bool, 0444);
MODULE_PARM_DESC(dcqcn_enable, "enables DCQCN algorithm for RoCEv2 on all ports, default=false ");

static bool dcqcn_cc_cfg_valid;
module_param(dcqcn_cc_cfg_valid, bool, 0444);
MODULE_PARM_DESC(dcqcn_cc_cfg_valid, "set DCQCN parameters to be valid, default=false");

static u8 dcqcn_min_dec_factor = 1;
module_param(dcqcn_min_dec_factor, byte, 0444);
MODULE_PARM_DESC(dcqcn_min_dec_factor, "set minimum percentage factor by which tx rate can be changed for CNP, Range: 1-100, default=1");

static u8 dcqcn_min_rate_MBps;
module_param(dcqcn_min_rate_MBps, byte, 0444);
MODULE_PARM_DESC(dcqcn_min_rate_MBps, "set minimum rate limit value, in MBits per second, default=0");

static u8 dcqcn_F = 5;
module_param(dcqcn_F, byte, 0444);
MODULE_PARM_DESC(dcqcn_F, "set number of times to stay in each stage of bandwidth recovery, default=5");

static unsigned short dcqcn_T = 0x37;
module_param(dcqcn_T, ushort, 0444);
MODULE_PARM_DESC(dcqcn_T, "set number of usecs that should elapse before increasing the CWND in DCQCN mode, default=0x37");

static unsigned int dcqcn_B = 0x249f0;
module_param(dcqcn_B, uint, 0444);
MODULE_PARM_DESC(dcqcn_B, "The number of bytes to transmit before updating CWND in DCQCN mode. default=0x249f0");

static unsigned short dcqcn_rai_factor = 1;
module_param(dcqcn_rai_factor, ushort, 0444);
MODULE_PARM_DESC(dcqcn_rai_factor, "set number of MSS to add to the congestion window in additive increase mode, default=1");

static unsigned short dcqcn_hai_factor = 5;
module_param(dcqcn_hai_factor, ushort, 0444);
MODULE_PARM_DESC(dcqcn_hai_factor, "set number of MSS to add to the congestion window in hyperactive increase mode, default=5");

static unsigned int dcqcn_rreduce_mperiod = 50;
module_param(dcqcn_rreduce_mperiod, uint, 0444);
MODULE_PARM_DESC(dcqcn_rreduce_mperiod, "set minimum time between 2 consecutive rate reductions for a single flow, default=50");

/**************************************************************************************************/

static unsigned int rtomin_qp_cnt_thresh = 512;
module_param(rtomin_qp_cnt_thresh, uint, 0444);
MODULE_PARM_DESC(rtomin_qp_cnt_thresh, "QP count threshold for switching between roce_rtomin_lo and roce_rtomin_hi, 0=disabled (512 QPs default)");

static u8 roce_rtomin_lo = 0x1A;
module_param(roce_rtomin_lo, byte, 0444);
MODULE_PARM_DESC(roce_rtomin_lo, "RoCEv2 rtomin value when QP count is at or below rtomin_qp_cnt_thresh, default=0x1A");

static u8 roce_rtomin_hi = 0x1A;
module_param(roce_rtomin_hi, byte, 0444);
MODULE_PARM_DESC(roce_rtomin_hi, "RoCEv2 rtomin value when QP count exceeds rtomin_qp_cnt_thresh, default=0x1A");

MODULE_ALIAS("i40iw");
MODULE_AUTHOR("Intel Corporation, <linux.nics@intel.com>");
MODULE_DESCRIPTION("Intel(R) Ethernet Protocol Driver for RDMA");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION(DRV_VER);

/**
 * set_protocol_used - set protocol_used against HW generation and roce_ena flag
 * @rf: RDMA PCI function
 * @roce_ena: RoCE enabled bit flag
 */
static inline void set_protocol_used(struct irdma_pci_f *rf, uint roce_ena)
{
	int i;

	switch (rf->rdma_ver) {
	case IRDMA_GEN_3:
	case IRDMA_GEN_4:
		rf->protocol_used = IRDMA_ROCE_PROTOCOL_ONLY;
		break;
	case IRDMA_GEN_2:
		rf->protocol_used = roce_ena & BIT(PCI_FUNC(rf->pcidev->devfn)) ?
			IRDMA_ROCE_PROTOCOL_ONLY : IRDMA_IWARP_PROTOCOL_ONLY;
		for (i = 0; i < 4; i++) {
			if (roce_pci_cfg[i] == rf->pcidev->bus->number)
				rf->protocol_used = IRDMA_ROCE_PROTOCOL_ONLY;
		}

		break;
	case IRDMA_GEN_1:
		rf->protocol_used = IRDMA_IWARP_PROTOCOL_ONLY;
		break;
	}
}

/**
 * irdma_set_rf_user_cfg_params - Setup RF configurations from module parameters
 * @rf: RDMA PCI function
 */
void irdma_set_rf_user_cfg_params(struct irdma_pci_f *rf)
{
	if (limits_sel > 7)
		limits_sel = 7;

	if (gen1_limits_sel > 5)
		gen1_limits_sel = 5;

	rf->limits_sel = (rf->rdma_ver == IRDMA_GEN_1) ? gen1_limits_sel :
							 limits_sel;
	if (roce_ena)
		pr_warn_once("irdma: Because roce_ena is ENABLED, roce_port_cfg will be ignored.");
	set_protocol_used(rf, roce_ena ? 0xFFFFFFFF : roce_port_cfg);
	rf->rsrc_profile = (resource_profile < IRDMA_HMC_PROFILE_EQUAL) ?
			    (u8)resource_profile + IRDMA_HMC_PROFILE_DEFAULT :
			    IRDMA_HMC_PROFILE_DEFAULT;
	if (max_rdma_vfs > IRDMA_MAX_PE_ENA_VF_COUNT) {
		pr_warn_once("irdma: Requested VF count [%d] is above max supported. Setting to %d.",
			     max_rdma_vfs, IRDMA_MAX_PE_ENA_VF_COUNT);
		max_rdma_vfs = IRDMA_MAX_PE_ENA_VF_COUNT;
	}
	rf->max_rdma_vfs = (rf->rsrc_profile != IRDMA_HMC_PROFILE_DEFAULT) ?
				max_rdma_vfs : 0;
	rf->en_rem_endpoint_trk = en_rem_endpoint_trk;
	rf->timer_slots = timer_bucket_slots;
	if (!rf->timer_slots)
		rf->timer_slots = 8;
	else if (rf->timer_slots < 3)
		rf->timer_slots = 3;
	else if (rf->timer_slots > 13)
		rf->timer_slots = 13;
	rf->fragcnt_limit = fragment_count_limit;
	if (rf->fragcnt_limit > 13 || rf->fragcnt_limit < 2) {
		rf->fragcnt_limit = 6;
		pr_warn_once("irdma: Requested [%d] fragment count limit out of range (2-13), setting to default=6.",
			     fragment_count_limit);
	}
	rf->dcqcn_ena = dcqcn_enable;

	/* Skip over all checking if no dcqcn */
	if (!dcqcn_enable)
		return;

	rf->dcqcn_params.cc_cfg_valid = dcqcn_cc_cfg_valid;
	rf->dcqcn_params.dcqcn_b = dcqcn_B;

#define DCQCN_B_MAX GENMASK(25, 0)
	if (rf->dcqcn_params.dcqcn_b > DCQCN_B_MAX) {
		rf->dcqcn_params.dcqcn_b = DCQCN_B_MAX;
		pr_warn_once("irdma: Requested [%d] dcqcn_b value too high, setting to %d.",
			     dcqcn_B, rf->dcqcn_params.dcqcn_b);
	}

#define DCQCN_F_MAX 8
	rf->dcqcn_params.dcqcn_f = dcqcn_F;
	if (dcqcn_F > DCQCN_F_MAX) {
		rf->dcqcn_params.dcqcn_f = DCQCN_F_MAX;
		pr_warn_once("irdma: Requested [%d] dcqcn_f value too high, setting to %d.",
			     dcqcn_F, DCQCN_F_MAX);
	}

	rf->dcqcn_params.dcqcn_t = dcqcn_T;
	rf->dcqcn_params.hai_factor = dcqcn_hai_factor;
	rf->dcqcn_params.min_dec_factor = dcqcn_min_dec_factor;
	if (dcqcn_min_dec_factor < 1 || dcqcn_min_dec_factor > 100) {
		rf->dcqcn_params.dcqcn_b = 1;
		pr_warn_once("irdma: Requested [%d] dcqcn_min_dec_factor out of range (1-100) , setting to default=1",
			     dcqcn_min_dec_factor);
	}

	rf->dcqcn_params.min_rate = dcqcn_min_rate_MBps;
	rf->dcqcn_params.rai_factor = dcqcn_rai_factor;
	rf->dcqcn_params.rreduce_mperiod = dcqcn_rreduce_mperiod;
	rf->rtomin_qp_cnt_thresh = rtomin_qp_cnt_thresh;
	rf->roce_rtomin_lo = roce_rtomin_lo;
	rf->roce_rtomin_hi = roce_rtomin_hi;
}

static int irdma_init_dbg_and_configfs(void)
{
#if IS_ENABLED(CONFIG_CONFIGFS_FS)
	int ret;

#endif /* CONFIG_CONFIGFS_FS */
#ifdef CONFIG_DEBUG_FS
	irdma_dbg_init();
#endif
#if IS_ENABLED(CONFIG_CONFIGFS_FS)
	ret = irdma_configfs_init();
	if (ret) {
		pr_err("Failed to register irdma to configfs subsystem\n");
#ifdef CONFIG_DEBUG_FS
		irdma_dbg_exit();
#endif
		return ret;
	}
#endif /* CONFIG_CONFIGFS_FS */
	return 0;
}

static inline void irdma_deinit_dbg_and_configfs(void)
{
#if IS_ENABLED(CONFIG_CONFIGFS_FS)
	irdma_configfs_exit();
#endif
#ifdef CONFIG_DEBUG_FS
	irdma_dbg_exit();
#endif
}

int irdma_vchnl_receive(struct iidc_core_dev_info *cdev_info, u32 vf_id,
			       u8 *msg, u16 len)
{
	struct irdma_device *iwdev = dev_get_drvdata(&cdev_info->adev->dev);
	struct irdma_sc_dev *dev;

	if (WARN_ON(!len || !msg))
		return -EINVAL;

	if (!iwdev)
		return -ENODEV;

	dev = &iwdev->rf->sc_dev;

	return dev->vchnl_if->vchnl_recv(dev, (u16)vf_id, msg, len);
}

int irdma_vchnl_send_pf(struct irdma_sc_dev *dev, u16 vf_id, u8 *msg, u16 len)
{
	struct iidc_core_dev_info *cdev_info = dev_to_rf(dev)->cdev;

	cdev_info->ops->vc_send(cdev_info, vf_id, msg, len);

	return 0;
}

int irdma_vchnl_send_sync(struct irdma_sc_dev *dev, u8 *msg, u16 len,
			  u8 *recv_msg, u16 *recv_len)
{
	struct iidc_core_dev_info *cdev_info = dev_to_rf(dev)->cdev;
	int ret;

	if (dev->hw_attrs.uk_attrs.hw_rev >= IRDMA_GEN_3)
		ret = dev_to_rf(dev)->idpf_idc_rdma_vc_send_sync_func(cdev_info, msg, len, recv_msg,
								      recv_len);
	else
		ret = cdev_info->ops->vc_send_sync(cdev_info, msg, len, recv_msg,
						   recv_len);
	if (ret == -ETIMEDOUT) {
		ibdev_err(&(dev_to_rf(dev)->iwdev->ibdev),
			  "Virtual channel Req <-> Resp completion timeout = 0x%x\n", ret);
		dev->vchnl_up = false;
	}

	return ret;
}

void irdma_log_invalid_mtu(u16 mtu, struct irdma_sc_dev *dev)
{
	if (mtu < IRDMA_MIN_MTU_IPV4)
		ibdev_warn(to_ibdev(dev),
			   "MTU setting [%d] too low for RDMA traffic. Minimum MTU is 576 for IPv4\n",
			   mtu);
	else if (mtu < IRDMA_MIN_MTU_IPV6)
		ibdev_warn(to_ibdev(dev),
			   "MTU setting [%d] too low for RDMA traffic. Minimum MTU is 1280 for IPv6\\n",
			   mtu);
}

void irdma_fill_qos_info(struct irdma_l2params *l2params,
			 struct iidc_qos_params *qos_info)
{
	int i;

	l2params->num_tc = qos_info->num_tc;
	l2params->vsi_prio_type = qos_info->vport_priority_type;
	l2params->vsi_rel_bw = qos_info->vport_relative_bw;
	for (i = 0; i < l2params->num_tc; i++) {
		l2params->tc_info[i].egress_virt_up =
			qos_info->tc_info[i].egress_virt_up;
		l2params->tc_info[i].ingress_virt_up =
			qos_info->tc_info[i].ingress_virt_up;
		l2params->tc_info[i].prio_type = qos_info->tc_info[i].prio_type;
		l2params->tc_info[i].rel_bw = qos_info->tc_info[i].rel_bw;
		l2params->tc_info[i].tc_ctx = qos_info->tc_info[i].tc_ctx;
	}
	for (i = 0; i < IIDC_MAX_USER_PRIORITY; i++)
		l2params->up2tc[i] = qos_info->up2tc[i];

	if (qos_info->pfc_mode == IIDC_DSCP_PFC_MODE) {
		l2params->dscp_mode = true;
		memcpy(l2params->dscp_map, qos_info->dscp_map,
		       sizeof(l2params->dscp_map));
	}
}

/**
 * irdma_request_reset - Request a reset
 * @rf: RDMA PCI function
 */
void irdma_request_reset(struct irdma_pci_f *rf)
{
	struct iidc_core_dev_info *cdev_info = rf->cdev;

	ibdev_warn(&rf->iwdev->ibdev, "Requesting a reset\n");
	rf->sc_dev.vchnl_up = false;
	if (rf->sc_dev.hw_attrs.uk_attrs.hw_rev >= IRDMA_GEN_3)
		rf->idpf_idc_request_reset_func(rf->cdev, IIDC_CORER);
	else
		cdev_info->ops->request_reset(rf->cdev, IIDC_CORER);
}

/*
 * irdma_vchnl_req_aeq_vec_map_gen2 - Virt channel AEQ configuration
 * @dev: device
 * @idx: function relative MSI-X vector
 *
 * Call the IDC to send a AEQ configuration request.
 * Return 0 if successful, otherwise return error
 */
int irdma_vchnl_req_aeq_vec_map_gen2(struct irdma_sc_dev *dev, u32 idx)
{
	struct iidc_core_dev_info *cdev_info = dev_to_rf(dev)->cdev;
	struct iidc_qvlist_info qvl_info = {};
	struct iidc_qv_info *qvinfo = &qvl_info.qv_info[0];

	qvl_info.num_vectors = 1;
	qvinfo->ceq_idx = IRDMA_Q_INVALID_IDX;
	qvinfo->v_idx = idx;
	qvinfo->itr_idx = IRDMA_IDX_ITR0;

	return cdev_info->ops->vc_queue_vec_map_unmap(cdev_info, &qvl_info,
						      true);
}

/*
 * irdma_vchnl_req_ceq_vec_map_gen2 - Virt channel CEQ configuration
 * @dev: shared code device
 * @ceq_id: function relative CEQ id
 * @idx: function relative MSI-X vector
 *
 * Call the IDC to send a CEQ configuration request.
 * Return 0 if successful, otherwise return error
 */
int irdma_vchnl_req_ceq_vec_map_gen2(struct irdma_sc_dev *dev, u16 ceq_id, u32 idx)
{
	struct iidc_core_dev_info *cdev_info = dev_to_rf(dev)->cdev;
	struct iidc_qvlist_info qvl_info = {};
	struct iidc_qv_info *qvinfo = &qvl_info.qv_info[0];

	qvl_info.num_vectors = 1;
	qvinfo->aeq_idx = IRDMA_Q_INVALID_IDX;
	qvinfo->ceq_idx = ceq_id;
	qvinfo->v_idx = idx;
	qvinfo->itr_idx = IRDMA_IDX_ITR0;

	return cdev_info->ops->vc_queue_vec_map_unmap(cdev_info, &qvl_info,
						      true);
}

/*
 * irdma_lan_register_qset - Register qset with LAN driver
 * @vsi: vsi structure
 * @tc_node: Traffic class node
 */
int irdma_lan_register_qset(struct irdma_sc_vsi *vsi,
			    struct irdma_ws_node *tc_node)
{
	struct irdma_device *iwdev = vsi->back_vsi;
	struct iidc_core_dev_info *cdev_info = iwdev->rf->cdev;
	struct iidc_rdma_qset_params qset = {};
	int ret;

	qset.qs_handle = tc_node->qs_handle;
	qset.tc = tc_node->traffic_class;
	qset.vport_id = vsi->vsi_idx;
	ret = cdev_info->ops->alloc_res(cdev_info, &qset);
	if (ret) {
		ibdev_dbg(&iwdev->ibdev, "WS: LAN alloc_res for rdma qset failed.\n");
		return ret;
	}

	tc_node->l2_sched_node_id = qset.teid;
	vsi->qos[tc_node->user_pri].l2_sched_node_id = qset.teid;

	return 0;
}

/**
 * irdma_lan_unregister_qset - Unregister qset with LAN driver
 * @vsi: vsi structure
 * @tc_node: Traffic class node
 */
void irdma_lan_unregister_qset(struct irdma_sc_vsi *vsi,
			       struct irdma_ws_node *tc_node)
{
	struct irdma_device *iwdev = vsi->back_vsi;
	struct iidc_core_dev_info *cdev_info = iwdev->rf->cdev;
	struct iidc_rdma_qset_params qset = {};

	qset.qs_handle = tc_node->qs_handle;
	qset.tc = tc_node->traffic_class;
	qset.vport_id = vsi->vsi_idx;
	qset.teid = tc_node->l2_sched_node_id;

	if (cdev_info->ops->free_res(cdev_info, &qset))
		ibdev_dbg(&iwdev->ibdev, "WS: LAN free_res for rdma qset failed.\n");
}
void irdma_cleanup_dead_qps(struct irdma_sc_vsi *vsi)
{
	struct irdma_sc_qp *qp = NULL;
	struct irdma_qp *iwqp;
	struct irdma_pci_f *rf;
	u8 i;

	for (i = 0; i < IRDMA_MAX_USER_PRIORITY; i++) {
		qp = irdma_get_qp_from_list(&vsi->qos[i].qplist, qp);
		while (qp) {
			if (qp->qp_uk.qp_type == IRDMA_QP_TYPE_UDA) {
				qp = irdma_get_qp_from_list(&vsi->qos[i].qplist, qp);
				continue;
			}
			iwqp = qp->qp_uk.back_qp;
			rf = iwqp->iwdev->rf;
			dma_free_coherent(rf->hw.device,
					  iwqp->q2_ctx_mem.size,
					  iwqp->q2_ctx_mem.va,
					  iwqp->q2_ctx_mem.pa);
			dma_free_coherent(rf->hw.device,
					  iwqp->kqp.dma_mem.size,
					  iwqp->kqp.dma_mem.va,
					  iwqp->kqp.dma_mem.pa);
			kfree(iwqp->kqp.sq_wrid_mem);
			kfree(iwqp->kqp.rq_wrid_mem);
			qp = irdma_get_qp_from_list(&vsi->qos[i].qplist, qp);
			kfree(iwqp);
		}
	}
}

static int __init irdma_init_module(void)
{
	int ret;

	pr_info("irdma driver version: %d.%d.%d\n", DRV_VER_MAJOR,
		DRV_VER_MINOR, DRV_VER_BUILD);
	ret = irdma_init_dbg_and_configfs();
	if (ret)
		return ret;

	ret = auxiliary_driver_register(&i40iw_auxiliary_drv);
	if (ret) {
		pr_err("Failed i40iw(gen_1) auxiliary_driver_register() ret=%d\n",
		       ret);
		irdma_deinit_dbg_and_configfs();
		return ret;
	}

	ret = auxiliary_driver_register(&icrdma_auxiliary_drv.adrv);
	if (ret) {
		auxiliary_driver_unregister(&i40iw_auxiliary_drv);
		pr_err("Failed irdma auxiliary_driver_register() ret=%d\n",
		       ret);
		irdma_deinit_dbg_and_configfs();
		return ret;
	}

	ret = auxiliary_driver_register(&ig3rdma_auxiliary_drv.adrv);
	if (ret) {
		auxiliary_driver_unregister(&i40iw_auxiliary_drv);
		auxiliary_driver_unregister(&icrdma_auxiliary_drv.adrv);
		pr_err("Failed irdma auxiliary_driver_register() ret=%d\n",
		       ret);
		irdma_deinit_dbg_and_configfs();
		return ret;
	}

	ret = auxiliary_driver_register(&ig3rdma_vport_auxiliary_drv.adrv);
	if (ret) {
		auxiliary_driver_unregister(&ig3rdma_auxiliary_drv.adrv);
		auxiliary_driver_unregister(&icrdma_auxiliary_drv.adrv);
		auxiliary_driver_unregister(&i40iw_auxiliary_drv);
		pr_err("Failed irdma auxiliary_driver_register() ret=%d\n",
		       ret);
		irdma_deinit_dbg_and_configfs();
		return ret;
	}

	return 0;
}

static void __exit irdma_exit_module(void)
{
	auxiliary_driver_unregister(&ig3rdma_vport_auxiliary_drv.adrv);
	auxiliary_driver_unregister(&ig3rdma_auxiliary_drv.adrv);
	auxiliary_driver_unregister(&icrdma_auxiliary_drv.adrv);
	auxiliary_driver_unregister(&i40iw_auxiliary_drv);
	irdma_deinit_dbg_and_configfs();
}

module_init(irdma_init_module);
module_exit(irdma_exit_module);
