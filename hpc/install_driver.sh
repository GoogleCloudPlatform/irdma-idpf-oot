#!/bin/bash

# Copyright 2026 Google LLC.
# SPDX-License-Identifier: Apache-2.0

###########################################################
# Standalone helper script to build and install the out-of-tree idpf and
# irdma kernel drivers on Rocky Linux.
#
# The script is self-contained: it clones the irdma-idpf-oot repository
# itself, so it can be run on a fresh VM without a pre-existing checkout, e.g.
#
#   curl -fsSL https://raw.githubusercontent.com/GoogleCloudPlatform/irdma-idpf-oot/main/hpc/install_driver.sh | bash
#
# Usage:
#   install_driver.sh [-r REPO_URL] [-b BRANCH_OR_TAG] [-d CLONE_DIR] [-k]
#
#   -r REPO_URL       Git repository to clone.
#                     (default: https://github.com/GoogleCloudPlatform/irdma-idpf-oot.git)
#   -b BRANCH_OR_TAG  Branch or tag to check out. (default: main)
#   -d CLONE_DIR      Directory to clone into. If it already contains a
#                     checkout it is reused. (default: temporary directory)
#   -k                Keep the clone directory after installation
#                     (always kept when -d is given).
#   -h                Show this help.
###########################################################

set -euo pipefail

REPO_URL="https://github.com/GoogleCloudPlatform/irdma-idpf-oot.git"
BRANCH="main"
CLONE_DIR=""
KEEP_CLONE=0

usage() {
  sed -n '/^# Usage:/,/^####/p' "${BASH_SOURCE[0]}" | sed -e '/^####/d' -e 's/^# \{0,1\}//'
}

while getopts "r:b:d:kh" opt; do
  case "${opt}" in
    r) REPO_URL="${OPTARG}" ;;
    b) BRANCH="${OPTARG}" ;;
    d) CLONE_DIR="${OPTARG}"; KEEP_CLONE=1 ;;
    k) KEEP_CLONE=1 ;;
    h) usage; exit 0 ;;
    *) usage; exit 1 ;;
  esac
done

KVER="$(uname -r)"

echo "Installing build dependencies..."
sudo dnf install -y git make gcc gawk findutils bc elfutils-libelf-devel \
  openssl-devel perl "kernel-devel-${KVER}"

if [[ -z "${CLONE_DIR}" ]]; then
  CLONE_DIR="$(mktemp -d /tmp/irdma-idpf-oot.XXXXXX)"
fi

cleanup() {
  if [[ "${KEEP_CLONE}" -eq 0 ]]; then
    # Build steps run under sudo and leave root-owned files behind.
    sudo rm -rf "${CLONE_DIR}"
  fi
}
trap cleanup EXIT

if [[ -d "${CLONE_DIR}/.git" ]]; then
  echo "Reusing existing checkout in ${CLONE_DIR} (fetching ${BRANCH})..."
  git -C "${CLONE_DIR}" fetch --depth 1 origin "${BRANCH}"
  git -C "${CLONE_DIR}" checkout --force FETCH_HEAD
else
  echo "Cloning ${REPO_URL} (${BRANCH}) into ${CLONE_DIR}..."
  git clone --depth 1 --branch "${BRANCH}" "${REPO_URL}" "${CLONE_DIR}"
fi
echo "Using commit: $(git -C "${CLONE_DIR}" rev-parse --short HEAD)"

echo "Building and installing idpf driver..."
pushd "${CLONE_DIR}/idpf-driver" >/dev/null
make clean
make -j"$(nproc)" compile
sudo make install
popd >/dev/null

echo "Building and installing irdma driver..."
pushd "${CLONE_DIR}/rdma-driver" >/dev/null
sudo bash ./build.sh
popd >/dev/null

# Ensure depmod prefers updates/ over pre-installed weak-updates/
sudo rm -f /etc/depmod.d/kmod-idpf-irdma.conf
sudo depmod -a "${KVER}"
sudo dracut --force --omit-drivers "irdma i40iw"

echo "Reloading drivers..."
sudo modprobe -r irdma idpf || true
sudo modprobe idpf
sudo modprobe irdma

echo "Installed modules:"
modinfo -n idpf irdma
modinfo -F version idpf irdma

if [[ "${KEEP_CLONE}" -eq 1 ]]; then
  echo "Source checkout kept at ${CLONE_DIR}"
fi
echo "Installation complete!"
