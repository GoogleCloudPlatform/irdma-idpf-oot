#!/bin/bash

# Copyright 2026 Google LLC.
# SPDX-License-Identifier: Apache-2.0

###########################################################
# Helper script to build and install the PSM3 RV module and a version
# of libfabric with PSM3 RV support.
###########################################################
# To use PSM3 RV/mode 1:
# * Run this script on all VMs.
# * Configure Intel MPI as per your normal workflow.
# * Add the following parameters to the environment (or as mpirun -genv args):
#
#   LD_LIBRARY_PATH=/opt/libfabric/lib:$LD_LIBRARY_PATH
#   I_MPI_OFI_LIBRARY_INTERNAL=0
#   I_MPI_OFI_PROVIDER=psm3
#   FI_PROVIDER=psm3
#   FI_PROVIDER_PATH=/opt/libfabric/lib/libfabric
#   PSM3_ALLOW_ROUTERS=1
#   PSM3_HAL=verbs
#   PSM3_RDMA=1
#   IRDMA_TRANSPARENT_UD_QD_OVERRIDE=2
#   IRDMA_SHARED_UD_CREDITS=96
#   PSM3_SEND_REAP_THRESH=1
#   PSM3_ERRCHK_TIMEOUT=2000:2000
#   PSM3_MQ_RNDV_NIC_THRESH=16384
#   PSM3_RV_FR_PAGE_LIST_LEN=256
###########################################################
# Example mpirun command:
# mpirun -np 2 \
#   -ppn 1 \
#   -genv LD_LIBRARY_PATH=/opt/libfabric/lib:$LD_LIBRARY_PATH \
#   -genv I_MPI_OFI_LIBRARY_INTERNAL=0 \
#   -genv I_MPI_OFI_PROVIDER=psm3 \
#   -genv FI_PROVIDER=psm3 \
#   -genv FI_PROVIDER_PATH=/opt/libfabric/lib/libfabric \
#   -genv PSM3_ALLOW_ROUTERS=1 \
#   -genv PSM3_HAL=verbs \
#   -genv PSM3_RDMA=1 \
#   -genv IRDMA_TRANSPARENT_UD_QD_OVERRIDE=2 \
#   -genv IRDMA_SHARED_UD_CREDITS=96 \
#   -genv PSM3_SEND_REAP_THRESH=1 \
#   -genv PSM3_ERRCHK_TIMEOUT=2000:2000 \
#   -genv PSM3_MQ_RNDV_NIC_THRESH=16384 \
#   -genv PSM3_RV_FR_PAGE_LIST_LEN=256 \
#   -hostfile mpihosts \
#   IMB-MPI1 sendrecv -npmin 2 -iter 100
###########################################################

set -e

echo "Installing build dependencies..."
sudo dnf install -y kernel-rpm-macros libuuid-devel rpm-build make gcc autoconf automake libtool kernel-devel-$(uname -r)

echo "Building and installing iefs-kernel-updates..."
git clone https://github.com/intel/iefs-kernel-updates.git
pushd iefs-kernel-updates/
./do-update-makerpm.sh -S "${PWD}" -w "${PWD}/tmp"
rpmbuild --rebuild --define "_topdir $(pwd)" --nodeps tmp/rpmbuild/SRPMS/*.src.rpm
KVER_MANGLED=$(uname -r | tr '-' '_')
sudo dnf install -y RPMS/x86_64/kmod-iefs-kernel-updates-${KVER_MANGLED}-*.x86_64.rpm
sudo dnf install -y RPMS/x86_64/iefs-kernel-updates-devel-${KVER_MANGLED}-*.x86_64.rpm
popd

echo "Building and installing libfabric..."
git clone https://github.com/ofiwg/libfabric.git --depth 1
pushd libfabric
./autogen.sh
./configure --prefix=/opt/libfabric \
    --enable-rxm=yes \
    --enable-rxd=yes \
    --enable-lnx=yes \
    --enable-shm=yes \
    --enable-udp=yes \
    --enable-tcp=yes \
    --enable-verbs=yes \
    --enable-psm3=dl \
    --with-psm3-rv=yes
make -j"$(nproc)"
sudo make install
popd

sudo modprobe rv

echo "Installation complete!"
