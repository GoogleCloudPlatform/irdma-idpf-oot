#!/bin/bash

# Copyright 2026 Google LLC.
# SPDX-License-Identifier: Apache-2.0

###########################################################
# Helper script to build and install the out-of-tree idpf and irdma
# kernel drivers on Rocky Linux.
###########################################################

set -e

REPO_URL="https://github.com/GoogleCloudPlatform/irdma-idpf-oot.git"
BRANCH="v2.2-stable"
KVER="$(uname -r)"
CLONE_DIR="$(mktemp -d /tmp/irdma-idpf-oot.XXXXXX)"
trap 'sudo rm -rf "${CLONE_DIR}"' EXIT

echo "Installing build dependencies..."
sudo dnf install -y git make gcc gawk findutils bc elfutils-libelf-devel openssl-devel perl "kernel-devel-${KVER}"

echo "Cloning ${REPO_URL} (${BRANCH})..."
git clone --depth 1 --branch "${BRANCH}" "${REPO_URL}" "${CLONE_DIR}"

echo "Building and installing idpf driver..."
pushd "${CLONE_DIR}/idpf-driver"
make clean
make -j"$(nproc)" compile
sudo make install
popd

echo "Building and installing irdma driver..."
pushd "${CLONE_DIR}/rdma-driver"
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
