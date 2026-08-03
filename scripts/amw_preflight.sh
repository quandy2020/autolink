#!/usr/bin/env bash
# Verify AMW binary/env is ready for multi-host (or simulated DIFF_HOST) runs.
# Usage:
#   ./scripts/amw_preflight.sh [build_dir=build]

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${1:-${AUTOLINK_BUILD_DIR:-${ROOT}/build}}"
BIN="${BUILD}/bin/examples"
MODE="${2:-all}"  # all | pubsub

need() {
  local p="$1"
  if [[ ! -x "${p}" ]]; then
    echo "FAIL: missing executable ${p}" >&2
    return 1
  fi
  echo "OK: ${p}"
}

need "${BIN}/autolink_example_amw_talker"
need "${BIN}/autolink_example_amw_listener"
if [[ "${MODE}" == "all" ]]; then
  need "${BIN}/autolink_example_amw_service"
  need "${BIN}/autolink_example_amw_client"
  need "${BIN}/autolink_example_amw_action_server"
  need "${BIN}/autolink_example_amw_action_client"
  need "${BIN}/autolink_example_amw_param_server"
  need "${BIN}/autolink_example_amw_param_client"
fi

LIB="${BUILD}/lib/libautolink.dylib"
[[ -f "${LIB}" ]] || LIB="${BUILD}/lib/libautolink.so"
if [[ ! -f "${LIB}" ]]; then
  echo "FAIL: libautolink not found under ${BUILD}/lib" >&2
  exit 1
fi

has_dds=0
if command -v nm >/dev/null 2>&1; then
  if nm -gU "${LIB}" 2>/dev/null | grep -qE 'FastDds|CycloneDds|dds_create_participant|DomainParticipant'; then
    has_dds=1
  fi
fi
if command -v otool >/dev/null 2>&1; then
  if otool -L "${LIB}" 2>/dev/null | grep -qiE 'fastdds|ddsc|fastcdr'; then
    has_dds=1
  fi
fi
if command -v ldd >/dev/null 2>&1; then
  if ldd "${LIB}" 2>/dev/null | grep -qiE 'fastdds|ddsc|fastcdr'; then
    has_dds=1
  fi
fi

if [[ "${has_dds}" -eq 1 ]]; then
  echo "OK: libautolink appears linked against a real DDS vendor"
else
  echo "WARN: could not confirm DDS linkage on libautolink" >&2
  echo "      Rebuild with -DAUTOLINK_ENABLE_FASTDDS=ON and/or" >&2
  echo "      -DAUTOLINK_ENABLE_CYCLONEDDS=ON" >&2
fi

echo "Env hints:"
echo "  AUTOLINK_AMW_IMPLEMENTATION=${AUTOLINK_AMW_IMPLEMENTATION:-<unset>}"
echo "  AUTOLINK_DOMAIN_ID=${AUTOLINK_DOMAIN_ID:-${ROS_DOMAIN_ID:-<unset>}}"
echo "  AUTOLINK_IP=${AUTOLINK_IP:-<unset>}"
echo "  CYCLONEDDS_URI=${CYCLONEDDS_URI:-<unset>}"
echo "Tips: peers must share the same AUTOLINK_DOMAIN_ID/ROS_DOMAIN_ID;"
echo "      AUTOLINK_IP should be a real NIC address on each host (not a sim IP)."
echo "Preflight done."
