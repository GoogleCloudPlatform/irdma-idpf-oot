/* SPDX-License-Identifier: GPL-2.0 or Linux-OpenIB */
/* Copyright (c) 2020 - 2025 Intel Corporation */
#ifndef DOCA_OFED_KCOMPAT_H
#define DOCA_OFED_KCOMPAT_H

#include <linux/sizes.h>

#ifndef OFED_VERSION
#define OFED_VERSION(a, b) (((a) << 16) + ((b) << 8))
#endif

#ifdef OFED_VERSION_CODE
#if OFED_VERSION_CODE == OFED_VERSION(26, 1)
#define ALLOC_HW_STATS_V3
#define ALLOC_HW_STATS_STRUCT_V2
#define ALLOC_PD_VER_3
#define ALLOC_UCONTEXT_VER_2
#define CREATE_AH_VER_5
#define CREATE_QP_VER_2
#define CREATE_CQ_VER_4
#define DESTROY_AH_VER_4
#define DEALLOC_PD_VER_4
#define DESTROY_QP_VER_2
#define DEALLOC_UCONTEXT_VER_2
#define DEREG_MR_VER_2
#define GET_HW_STATS_V2
#define GET_LINK_LAYER_V2
#define HAS_IB_SET_DEVICE_OP
#define IB_DEV_CAPS_VER_2
#define IB_DEALLOC_DRIVER_SUPPORT
#define IW_PORT_IMMUTABLE_V2
#define IB_UMEM_GET_V3
#define IRDMA_ALLOC_MW_VER_2
#define IRDMA_DESTROY_CQ_VER_4
#define IRDMA_DESTROY_SRQ_VER_3
#define IRDMA_ALLOC_MR_VER_0
#define MODIFY_PORT_V2
#define NETDEV_TO_IBDEV_SUPPORT
#define QUERY_GID_V2
#define QUERY_GID_ROCE_V2
#define QUERY_PKEY_V2
#define QUERY_PORT_V2
#define REG_USER_MR_VER_2
#define REREG_MR_VER_2
#define REG_USER_MR_DMABUF_VER_3
#define SET_DMABUF
#define ROCE_PORT_IMMUTABLE_V2
#define RDMA_MMAP_DB_SUPPORT
#define SET_BEST_PAGE_SZ_V2
#define SET_ROCE_CM_INFO_VER_3

// If DOCA defines HAVE_SG_APPEND_TABLE then
// we don't need to use raw sg tables
#ifdef HAVE_SG_APPEND_TABLE
#undef HAVE_IB_UMEM_SG_HEAD
#endif

#define kc_set_ibdev_add_del_gid(ibdev)
#define kc_deref_sgid_attr(sgid_attr) ((sgid_attr)->ndev)
#define kc_set_props_ip_gid_caps(props) ((props)->ip_gids = true)
#define kc_ib_register_device(device, name, dev)                               \
  ib_register_device(device, name, dev)
#define kc_rdma_udata_to_drv_context(ibpd, udata)                              \
  rdma_udata_to_drv_context(udata, struct irdma_ucontext, ibucontext)
#define kc_get_ucontext(udata)                                                 \
  rdma_udata_to_drv_context(udata, struct irdma_ucontext, ibucontext)
#define set_ibdev_dma_device(ibdev, dev)
#define ah_attr_to_dmac(attr) ((attr).roce.dmac)
#define kc_typeq_ib_wr const

#define kc_rdma_gid_attr_network_type(sgid_attr, gid_type, gid)                \
  rdma_gid_attr_network_type(sgid_attr)

#define set_max_sge(props, rf)                                                 \
  do {                                                                         \
    ((props)->max_send_sge = (rf)->sc_dev.hw_attrs.uk_attrs.max_hw_wq_frags);  \
    ((props)->max_recv_sge = (rf)->sc_dev.hw_attrs.uk_attrs.max_hw_wq_frags);  \
  } while (0)

#define kc_ib_modify_qp_is_ok(cur_state, next_state, type, mask, ll)           \
  ib_modify_qp_is_ok(cur_state, next_state, type, mask)
#endif
#endif

#if defined(RHEL_9_6) || defined(RHEL_10_0)
#define GLOBAL_QP_MEM
#define IN_IFADDR
#define IP_ROUTE_OUTPUT_VER_2
#endif

#if defined(RHEL_9_1) || defined(RHEL_9_2) || defined(RHEL_9_3) || defined(RHEL_9_4) || defined(RHEL_9_5)
#define GLOBAL_QP_MEM
#define IN_IFADDR
#endif

#ifdef RHEL_9_0
#define IN_IFADDR
#endif
#endif /* DOCA_OFED_KCOMPAT_H */
