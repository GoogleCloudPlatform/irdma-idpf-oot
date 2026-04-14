// SPDX-License-Identifier: GPL-2.0 or Linux-OpenIB
/* Copyright (c) 2018 - 2023 Intel Corporation */
#include "osdep.h"
#include "type.h"
#include "protos.h"
#include "ig3rdma_regs.h"
#include "ig3rdma_regs_apf.h"
#include "ig3rdma_regs_avf.h"
#include "ig3rdma_hw.h"

extern bool irdma_rca_ena;

struct ig3_rdma_reg {
	u32 reg_offset;
	bool privileged;
};

static struct ig3_rdma_reg ig3rdma_regs[IRDMA_MAX_REGS] = {
	{IG3_APF_CQPTAIL, false},
	{IG3_APF_DBL_CQPDB, false},
	{IG3_APF_CCQPSTATUS, false},
	{IG3_APF_CCQPHIGH, false},
	{IG3_APF_CCQPLOW, false},
	{IG3_APF_DBL_CQARM, false},
	{IG3_APF_DBL_CQACK, false},
	{IG3_APF_DBL_AEQALLOC, false},
	{IG3_APF_CQPERRCODES, false},
	{IG3_APF_DBL_WQEALLOC, false},
	{IG3_GLINT_DYN_CTL(0), false},
	{IG3_DB_ADDR_OFFSET, false },
	/* APF/AVF use from here up only */
	{IG3_GLPCI_LBARCTRL, true},
	{IG3_CPUW_GLPE_CPUSTATUS0, true},
	{IG3_CPUW_GLPE_CPUSTATUS1, true},
	{IG3_CPUW_GLPE_CPUSTATUS2, true},
	{RSVD_OFFSET, true},
	{RSVD_OFFSET, true},
	{RSVD_OFFSET, true},
	{RSVD_OFFSET, true},
	{RSVD_OFFSET, true},
	{RSVD_OFFSET, true},
	{RSVD_OFFSET, true},
};

static u64 ig3rdma_masks[IRDMA_MAX_MASKS] = {
	IG3_APF_CCQPSTATUS_CCQP_DONE,
	IG3_APF_CCQPSTATUS_CCQP_ERR,
	IG3_CQPSQ_STAG_PDID,
	IG3_CQPSQ_CQ_CEQID,
	IG3_CQPSQ_CQ_CQID,
	IG3_COMMIT_FPM_CQCNT,
	IG3_CQPSQ_UPESD_HMCFNID,
};

static u8 ig3rdma_shifts[IRDMA_MAX_SHIFTS] = {
	IG3_APF_CCQPSTATUS_CCQP_DONE_S,
	IG3_APF_CCQPSTATUS_CCQP_ERR_S,
	IG3_CQPSQ_STAG_PDID_S,
	IG3_CQPSQ_CQ_CEQID_S,
	IG3_CQPSQ_CQ_CQID_S,
	IG3_COMMIT_FPM_CQCNT_S,
	IG3_CQPSQ_UPESD_HMCFNID_S,
};

/**
 * ig3rdma_ena_irq - Enable interrupt
 * @dev: pointer to the device structure
 * @idx: vector index
 */
static void ig3rdma_ena_irq(struct irdma_sc_dev *dev, u32 idx)
{
	u32 val;
	u32 int_stride = 1; /* one u32 per register */

	if (dev->is_pf)
		int_stride = 0x400;
	else
		idx--; /* VFs use DYN_CTL_N */

	val = FIELD_PREP(IRDMA_GLINT_DYN_CTL_INTENA, 1) |
	      FIELD_PREP(IRDMA_GLINT_DYN_CTL_CLEARPBA, 1);

	writel(val, dev->hw_regs[IRDMA_GLINT_DYN_CTL] + (idx * int_stride));
}

/**
 * ig3rdma_disable_irq - Disable interrupt
 * @dev: pointer to the device structure
 * @idx: vector index
 */
static void ig3rdma_disable_irq(struct irdma_sc_dev *dev, u32 idx)
{
	u32 int_stride = 1; /* one u32 per register */

	if (dev->is_pf)
		int_stride = 0x400;
	else
		idx--; /* VFs use DYN_CTL_N */

	writel(0, dev->hw_regs[IRDMA_GLINT_DYN_CTL] + (idx * int_stride));
}

static const struct irdma_irq_ops ig3rdma_irq_ops = {
	.irdma_dis_irq = ig3rdma_disable_irq,
	.irdma_en_irq = ig3rdma_ena_irq,
};

static const struct irdma_hw_stat_map ig3rdma_hw_stat_map[] = {
	[IRDMA_HW_STAT_INDEX_RXVLANERR] =	{   0, 0, 0 },
	[IRDMA_HW_STAT_INDEX_IP4RXOCTS] =	{   8, 0, 0 },
	[IRDMA_HW_STAT_INDEX_IP4RXPKTS] =	{  16, 0, 0 },
	[IRDMA_HW_STAT_INDEX_IP4RXDISCARD] =	{  24, 0, 0 },
	[IRDMA_HW_STAT_INDEX_IP4RXTRUNC] =	{  32, 0, 0 },
	[IRDMA_HW_STAT_INDEX_IP4RXFRAGS] =	{  40, 0, 0 },
	[IRDMA_HW_STAT_INDEX_IP4RXMCOCTS] =	{  48, 0, 0 },
	[IRDMA_HW_STAT_INDEX_IP4RXMCPKTS] =	{  56, 0, 0 },
	[IRDMA_HW_STAT_INDEX_IP6RXOCTS] =	{  64, 0, 0 },
	[IRDMA_HW_STAT_INDEX_IP6RXPKTS] =	{  72, 0, 0 },
	[IRDMA_HW_STAT_INDEX_IP6RXDISCARD] =	{  80, 0, 0 },
	[IRDMA_HW_STAT_INDEX_IP6RXTRUNC] =	{  88, 0, 0 },
	[IRDMA_HW_STAT_INDEX_IP6RXFRAGS] =	{  96, 0, 0 },
	[IRDMA_HW_STAT_INDEX_IP6RXMCOCTS] =	{ 104, 0, 0 },
	[IRDMA_HW_STAT_INDEX_IP6RXMCPKTS] =	{ 112, 0, 0 },
	[IRDMA_HW_STAT_INDEX_IP4TXOCTS] =	{ 120, 0, 0 },
	[IRDMA_HW_STAT_INDEX_IP4TXPKTS] =	{ 128, 0, 0 },
	[IRDMA_HW_STAT_INDEX_IP4TXFRAGS] =	{ 136, 0, 0 },
	[IRDMA_HW_STAT_INDEX_IP4TXMCOCTS] =	{ 144, 0, 0 },
	[IRDMA_HW_STAT_INDEX_IP4TXMCPKTS] =	{ 152, 0, 0 },
	[IRDMA_HW_STAT_INDEX_IP6TXOCTS] =	{ 160, 0, 0 },
	[IRDMA_HW_STAT_INDEX_IP6TXPKTS] =	{ 168, 0, 0 },
	[IRDMA_HW_STAT_INDEX_IP6TXFRAGS] =	{ 176, 0, 0 },
	[IRDMA_HW_STAT_INDEX_IP6TXMCOCTS] =	{ 184, 0, 0 },
	[IRDMA_HW_STAT_INDEX_IP6TXMCPKTS] =	{ 192, 0, 0 },
	[IRDMA_HW_STAT_INDEX_IP4TXNOROUTE] =	{ 200, 0, 0 },
	[IRDMA_HW_STAT_INDEX_IP6TXNOROUTE] =	{ 208, 0, 0 },
	[IRDMA_HW_STAT_INDEX_TCPRTXSEG] =	{ 216, 0, 0 },
	[IRDMA_HW_STAT_INDEX_TCPRXOPTERR] =	{ 224, 0, 0 },
	[IRDMA_HW_STAT_INDEX_TCPRXPROTOERR] =	{ 232, 0, 0 },
	[IRDMA_HW_STAT_INDEX_TCPTXSEG] =	{ 240, 0, 0 },
	[IRDMA_HW_STAT_INDEX_TCPRXSEGS] =	{ 248, 0, 0 },
	[IRDMA_HW_STAT_INDEX_UDPRXPKTS] =	{ 256, 0, 0 },
	[IRDMA_HW_STAT_INDEX_UDPTXPKTS] =	{ 264, 0, 0 },
	[IRDMA_HW_STAT_INDEX_RDMARXWRS] =	{ 272, 0, 0 },
	[IRDMA_HW_STAT_INDEX_RDMARXRDS] =	{ 280, 0, 0 },
	[IRDMA_HW_STAT_INDEX_RDMARXSNDS] =	{ 288, 0, 0 },
	[IRDMA_HW_STAT_INDEX_RDMATXWRS] =	{ 296, 0, 0 },
	[IRDMA_HW_STAT_INDEX_RDMATXRDS] =	{ 304, 0, 0 },
	[IRDMA_HW_STAT_INDEX_RDMATXSNDS] =	{ 312, 0, 0 },
	[IRDMA_HW_STAT_INDEX_RDMAVBND] =	{ 320, 0, 0 },
	[IRDMA_HW_STAT_INDEX_RDMAVINV] =	{ 328, 0, 0 },
	[IRDMA_HW_STAT_INDEX_RXNPECNMARKEDPKTS] = { 336, 0, 0 },
	[IRDMA_HW_STAT_INDEX_RXRPCNPHANDLED] =	{ 344, 0, 0 },
	[IRDMA_HW_STAT_INDEX_RXRPCNPIGNORED] =	{ 352, 0, 0 },
	[IRDMA_HW_STAT_INDEX_TXNPCNPSENT] =	{ 360, 0, 0 },
	[IRDMA_HW_STAT_INDEX_RNR_SENT] =	{ 368, 0, 0 },
	[IRDMA_HW_STAT_INDEX_RNR_RCVD] =	{ 376, 0, 0 },
	[IRDMA_HW_STAT_INDEX_RDMAORDLMTCNT] =	{ 384, 0, 0 },
	[IRDMA_HW_STAT_INDEX_RDMAIRDLMTCNT] =	{ 392, 0, 0 },
	[IRDMA_HW_STAT_INDEX_RDMARXATS] =	{ 408, 0, 0 },
	[IRDMA_HW_STAT_INDEX_RDMATXATS] =	{ 416, 0, 0 },
	[IRDMA_HW_STAT_INDEX_NAKSEQERR] =	{ 424, 0, 0 },
	[IRDMA_HW_STAT_INDEX_NAKSEQERR_IMPLIED] = { 432, 0, 0 },
	[IRDMA_HW_STAT_INDEX_RTO] =		{ 440, 0, 0 },
	[IRDMA_HW_STAT_INDEX_RXOOOPKTS] =	{ 448, 0, 0 },
	[IRDMA_HW_STAT_INDEX_ICRCERR] =		{ 456, 0, 0 },
	[IRDMA_HW_STAT_INDEX_RDMARXFLUSH] =	{ 472, 0, 0 },
	[IRDMA_HW_STAT_INDEX_RDMATXFLUSH] =	{ 480, 0, 0 },
	[IRDMA_HW_STAT_INDEX_RDMARXATOMICWRITE] =	{ 488, 0, 0 },
	[IRDMA_HW_STAT_INDEX_RDMATXATOMICWRITE] =	{ 496, 0, 0 },
};

void ig3rdma_init_hw(struct irdma_sc_dev *dev)
{
	int i;
	u8 __iomem *hw_addr;
	u32 db_page_offset = 0;
	u64 db_map_len;

	if (dev->hw_attrs.uk_attrs.hw_rev == IRDMA_GEN_4) {
		ig3rdma_regs[IRDMA_GLPE_CPUSTATUS0].reg_offset = IG4_CPUW_GLPE_CPUSTATUS0;
		ig3rdma_regs[IRDMA_GLPE_CPUSTATUS1].reg_offset = IG4_CPUW_GLPE_CPUSTATUS1;
		ig3rdma_regs[IRDMA_GLPE_CPUSTATUS2].reg_offset = IG4_CPUW_GLPE_CPUSTATUS2;
	}

	/* Map RCA CQP DB page.
	 * In practice, remap entire region again, but with additional 8K.
	 * Use db_addr for HW reg address calculation.
	 * Keep the original mapping unmodified, so it will be unmapped
	 * by IDPF.
	 */
	if (irdma_rca_ena && dev->is_pf) {
		db_page_offset = 2 * SZ_4K;
		db_map_len = ig3rdma_regs[IRDMA_DB_ADDR_OFFSET].reg_offset +
			3 * SZ_4K;
		dev->db_addr = irdma_ioremap_db(dev, db_map_len);
	}

	for (i = 0; i < IRDMA_MAX_REGS; ++i) {
		hw_addr = dev->hw->hw_addr;

		if (i == IRDMA_DB_ADDR_OFFSET)
			hw_addr = NULL;

		if (i == IRDMA_GLINT_DYN_CTL)
			dev->hw_regs[i] = (u32 __iomem *)
				(hw_addr + ig3rdma_regs[i].reg_offset);
		else
			dev->hw_regs[i] = (u32 __iomem *)
				(hw_addr + ig3rdma_regs[i].reg_offset +
				 db_page_offset);
	}
	for (i = 0; i < IRDMA_MAX_SHIFTS; ++i)
		dev->hw_shifts[i] = ig3rdma_shifts[i];

	for (i = 0; i < IRDMA_MAX_MASKS; ++i)
		dev->hw_masks[i] = ig3rdma_masks[i];

	dev->irq_ops = &ig3rdma_irq_ops;
	dev->hw_stats_map = ig3rdma_hw_stat_map;

	dev->hw_attrs.uk_attrs.max_hw_wq_frags = IG3RDMA_MAX_WQ_FRAGMENT_COUNT;
	dev->hw_attrs.uk_attrs.max_hw_read_sges = IG3RDMA_MAX_SGE_RD;
	dev->hw_attrs.uk_attrs.max_hw_sq_chunk = IRDMA_MAX_QUANTA_PER_WR;
	dev->hw_attrs.first_hw_vf_fpm_id = 0;
	dev->hw_attrs.max_hw_vf_fpm_id = IG3_MAX_APFS + IG3_MAX_AVFS;
	dev->hw_attrs.uk_attrs.feature_flags |= IRDMA_FEATURE_64_BYTE_CQE;
#ifdef MEV_HVL // TBD: MEV_SUPPORT?
	dev->hw_attrs.uk_attrs.feature_flags |= IRDMA_FEATURE_ATOMIC_OPS;
#endif
	dev->hw_attrs.uk_attrs.feature_flags |= IRDMA_FEATURE_CQE_TIMESTAMPING;

	/* Currently only affects user space */
	dev->hw_attrs.uk_attrs.feature_flags |= IRDMA_FEATURE_ENFORCE_SQ_SIZE;
	dev->hw_attrs.uk_attrs.feature_flags |= IRDMA_FEATURE_SRQ;
	dev->hw_attrs.uk_attrs.feature_flags |= IRDMA_FEATURE_RTS_AE |
						IRDMA_FEATURE_CQ_RESIZE;
	dev->hw_attrs.page_size_cap = SZ_4K | SZ_2M | SZ_1G;
	dev->hw_attrs.max_hw_ird = IG3RDMA_MAX_IRD_SIZE;
	dev->hw_attrs.max_hw_ord = IG3RDMA_MAX_ORD_SIZE;
	dev->hw_attrs.max_stat_inst = IG3RDMA_MAX_STATS_COUNT;
	if (dev->hw_attrs.uk_attrs.hw_rev == IRDMA_GEN_4)
		dev->hw_attrs.max_stat_idx = IRDMA_HW_STAT_INDEX_MAX_GEN_4;
	else
		dev->hw_attrs.max_stat_idx = IRDMA_HW_STAT_INDEX_MAX_GEN_3;
	if (dev->hw_wa & REDUCE_ORD_IRD) {
		dev->hw_attrs.max_hw_ird = 64;
		dev->hw_attrs.max_hw_ord = 64;
	}
	if (dev->hw_wa & REDUCE_ORD_IRD_SIMICS) {
		dev->hw_attrs.max_hw_ird = 64;
		dev->hw_attrs.max_hw_ord = 64;
	}
	if (dev->hw_wa & CQP_COMPL_WAIT)
		dev->hw_attrs.max_cqp_compl_wait_time_ms *= 50;
	dev->hw_attrs.uk_attrs.min_hw_wq_size = IG3RDMA_MIN_WQ_SIZE;
	dev->hw_attrs.uk_attrs.max_hw_srq_quanta = IRDMA_SRQ_MAX_QUANTA;
	dev->hw_attrs.uk_attrs.max_hw_inline = IG3RDMA_MAX_INLINE_DATA_SIZE;
	dev->hw_attrs.max_hw_device_pages =
		dev->is_pf ? IG3RDMA_MAX_PF_PUSH_PAGE_COUNT : IG3RDMA_MAX_VF_PUSH_PAGE_COUNT;
}
