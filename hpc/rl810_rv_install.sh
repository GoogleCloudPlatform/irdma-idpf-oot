#!/bin/bash

# Copyright 2026 Google LLC.
# SPDX-License-Identifier: Apache-2.0

###########################################################
# Helper script to build and install the PSM3 RV module and a version
# of libfabric with PSM3 RV support.
#
# Pass --no-libfabric to skip building libfabric.
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

LIBFABRIC_VERSION=v2.5.0
IEFS_KERNEL_UPDATES_VERSION=c9be17c
IEFS_CHERRY_PICK_COMMITS=(
	"8c31582ade608d2927d7faddb35d557fe25fa6f7"
)

BUILD_LIBFABRIC=true
for arg in "$@"; do
	if [[ "$arg" == "--no-libfabric" ]]; then
		BUILD_LIBFABRIC=false
		break
	fi
done

RV_INSTALL_TEMP_DIR=$(mktemp -d -p "/run/user/$(id -u)" -t rl810-rv-install-XXXXXXXXXX)
function cleanup() {
	echo "Cleaning up temporary directory ${RV_INSTALL_TEMP_DIR}..."
	rm -rf "${RV_INSTALL_TEMP_DIR}"
}
trap cleanup EXIT SIGINT SIGTERM SIGHUP
cd "${RV_INSTALL_TEMP_DIR}"
echo "Working in temporary directory ${RV_INSTALL_TEMP_DIR}"

echo "Installing build dependencies..."
sudo dnf install -y kernel-rpm-macros libuuid-devel rpm-build make gcc autoconf automake libtool kernel-devel-$(uname -r)

echo "Building and installing iefs-kernel-updates..."
git clone https://github.com/intel/iefs-kernel-updates.git
pushd iefs-kernel-updates/
git checkout "${IEFS_KERNEL_UPDATES_VERSION}"
for commit in "${IEFS_CHERRY_PICK_COMMITS[@]}"; do
	git -c user.name="rv-build" -c user.email="rv-build@localhost" cherry-pick "${commit}"
done
./do-update-makerpm.sh -S "${PWD}" -w "${PWD}/tmp"
rpmbuild --rebuild --define "_topdir $(pwd)" --nodeps tmp/rpmbuild/SRPMS/*.src.rpm
KVER_MANGLED=$(uname -r | tr '-' '_')
sudo dnf install -y RPMS/x86_64/kmod-iefs-kernel-updates-${KVER_MANGLED}-*.x86_64.rpm
sudo dnf install -y RPMS/x86_64/iefs-kernel-updates-devel-${KVER_MANGLED}-*.x86_64.rpm
popd

if ${BUILD_LIBFABRIC}; then
	echo "Building and installing libfabric..."
	git clone https://github.com/ofiwg/libfabric.git
	pushd libfabric
	git checkout "${LIBFABRIC_VERSION}"
	./autogen.sh
	./configure --prefix=/opt/libfabric \
	    --enable-rxm=yes \
	    --enable-rxd=yes \
	    --enable-lnx=yes \
	    --enable-shm=yes \
	    --enable-udp=yes \
	    --enable-tcp=yes \
	    --enable-verbs=yes \
	    --enable-psm3=yes \
	    --with-psm3-rv=yes \
		--disable-efa \
		--disable-mrail \
		--disable-xpmem \
		--disable-opx \
		--disable-usnic
	make -j"$(nproc)"
	sudo make install
	popd
fi

if lsmod | grep -q "^rv "; then
	echo "Unloading existing rv module..."
	if ! sudo modprobe -r rv; then
		echo "WARNING: Failed to unload existing 'rv' module because it is currently in use." >&2
		echo "The updated rv package has been installed on disk, but the old module is still loaded in memory." >&2
		echo "Please stop any processes using 'rv' and run 'sudo modprobe -r rv && sudo modprobe rv' (or reboot) to load the new module." >&2
	fi
fi
sudo modprobe rv

echo "Installation complete!"
