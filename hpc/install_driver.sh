#!/bin/bash

# Copyright 2026 Google LLC.
# SPDX-License-Identifier: Apache-2.0

###########################################################
# Helper script to build and install the out-of-tree idpf and irdma
# kernel drivers on Rocky Linux.
###########################################################

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
KVER="$(uname -r)"

echo "Installing build dependencies..."
sudo dnf install -y make gcc gawk findutils bc elfutils-libelf-devel openssl-devel perl "kernel-devel-${KVER}"

echo "Building and installing idpf driver..."
pushd "${REPO_ROOT}/idpf-driver"
make clean
make -j"$(nproc)" compile
sudo make install
popd

echo "Building and installing irdma driver..."
pushd "${REPO_ROOT}/rdma-driver"
sudo bash ./build.sh
popd

# Ensure depmod prefers updates/ over pre-installed weak-updates/
sudo rm -f /etc/depmod.d/kmod-idpf-irdma.conf
sudo depmod -a "${KVER}"
sudo dracut --force --omit-drivers "irdma i40iw"

echo "Reloading drivers..."
sudo modprobe -r irdma idpf || true
sudo modprobe idpf
sudo modprobe irdma

echo "Installation complete!"
