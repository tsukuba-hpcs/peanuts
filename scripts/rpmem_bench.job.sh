#!/bin/bash

set -eu
SCRIPT_DIR="$(readlink -f "$( dirname "${BASH_SOURCE[0]}" )")"
EXE_PATH="$(readlink -f "${SCRIPT_DIR}/../build/src/benchmarks/rpmem_bench")"

get_active_spack_env_name() {
    status_output=$(spack env status)
    if [[ "$status_output" =~ In\ environment\ ([^\ ]+) ]]; then
        env_name="${BASH_REMATCH[1]}"
        echo "$env_name"
    else
        echo ""
    fi
}

if [ -z "$(get_active_spack_env_name)" ]; then
    spack env activate rpmbb
fi

nnodes=$(wc --lines "${PBS_NODEFILE}" | awk '{print $1}')
ppn=48
np=$((nnodes * ppn))
iter=182733
xfer_size=$((47008))
block_size=$((iter*xfer_size))

cmd_args=(
  mpirun
  $NQSII_MPIOPTS
  -map-by "ppr:${ppn}:node"
  -np "$np"
  # --report-bindings
  --mca osc ucx
  --mca btl ^uct
  # --mca osc_ucx_acc_single_intrinsic true
  # --mca osc_base_verbose 100
  # --mca btl_openib_allow_ib 1
  # -x UCX_LOG_LEVEL=info
  # -x UCX_PROTO_ENABLE=y
  # -x UCX_PROTO_INFO=y
  # -x "UCX_TLS=dc"
  # -x UCX_IB_MLX5_DEVX=n
  # -x "UCX_IB_REG_METHODS=odp,direct"
  # -x "UCX_IB_REG_METHODS=odp"
  # -x "UCX_IB_REG_METHODS=rcache"
  # -x UCX_IB_RCACHE_MAX_REGIONS=1000
  # -x RDMAV_HUGEPAGES_SAFE=1
  # -x UCX_MEMTYPE_CACHE=n
  # -x "UCX_NUM_EPS=$np"
  "${EXE_PATH}"
  --prettify
  -b "$block_size"
  -t "$xfer_size"
)

echo "${cmd_args[@]}"
time "${cmd_args[@]}"
