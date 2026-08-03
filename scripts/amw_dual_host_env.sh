#!/usr/bin/env bash
# Source on each host before running AMW multi-host examples.
# Usage:
#   source scripts/amw_dual_host_env.sh <lan_ip> [domain_id] [implementation]
#
# Example (host A = 192.168.10.6, host B = 192.168.10.7):
#   source scripts/amw_dual_host_env.sh 192.168.10.6 0 amw_fastdds

set -euo pipefail

LAN_IP="${1:-}"
DOMAIN_ID="${2:-0}"
IMPL="${3:-amw_cyclonedds}"

if [[ -z "${LAN_IP}" ]]; then
  echo "usage: source $0 <lan_ip> [domain_id=0] [implementation=amw_cyclonedds|amw_fastdds]" >&2
  return 1 2>/dev/null || exit 1
fi

export AUTOLINK_IP="${LAN_IP}"
export AUTOLINK_DOMAIN_ID="${DOMAIN_ID}"
export ROS_DOMAIN_ID="${DOMAIN_ID}"
export AUTOLINK_AMW_IMPLEMENTATION="${IMPL}"
# ROS 2 compatible alias
export RMW_IMPLEMENTATION="${RMW_IMPLEMENTATION:-}"
case "${IMPL}" in
  amw_fastdds|fastdds|rmw_fastrtps_cpp)
    export RMW_IMPLEMENTATION="${RMW_IMPLEMENTATION:-rmw_fastrtps_cpp}"
    ;;
  amw_cyclonedds|cyclonedds|rmw_cyclonedds_cpp)
    export RMW_IMPLEMENTATION="${RMW_IMPLEMENTATION:-rmw_cyclonedds_cpp}"
    ;;
esac

# Runtime libs (best-effort)
if [[ -d "${HOME}/.local/cyclonedds/lib" ]]; then
  export DYLD_LIBRARY_PATH="${HOME}/.local/cyclonedds/lib:${DYLD_LIBRARY_PATH:-}"
  export LD_LIBRARY_PATH="${HOME}/.local/cyclonedds/lib:${LD_LIBRARY_PATH:-}"
fi
if [[ -d /opt/ros/humble/lib/x86_64-linux-gnu ]]; then
  export LD_LIBRARY_PATH="/opt/ros/humble/lib/x86_64-linux-gnu:/opt/ros/humble/lib:${LD_LIBRARY_PATH:-}"
fi

export GLOG_logtostderr="${GLOG_logtostderr:-1}"

echo "AMW dual-host env:"
echo "  AUTOLINK_IP=${AUTOLINK_IP}"
echo "  AUTOLINK_DOMAIN_ID=${AUTOLINK_DOMAIN_ID}"
echo "  AUTOLINK_AMW_IMPLEMENTATION=${AUTOLINK_AMW_IMPLEMENTATION}"
echo "  RMW_IMPLEMENTATION=${RMW_IMPLEMENTATION}"
echo "  LD_LIBRARY_PATH=${LD_LIBRARY_PATH:-<unset>}"
