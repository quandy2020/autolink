#!/usr/bin/env bash
# Same-machine DIFF_HOST simulation: two processes with different AUTOLINK_IP
# so Hybrid routes over RTPS/DDS (not SHM).
#
# Prerequisites:
#   cmake -B build -DAUTOLINK_ENABLE_FASTDDS=ON ... && cmake --build build -j
#   (or CYCLONEDDS=ON)
#
# Usage:
#   ./scripts/amw_sim_dual_host.sh [impl=amw_fastdds] [domain=0] [seconds=8]

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
if [[ -n "${AUTOLINK_BUILD_DIR:-}" ]]; then
  BUILD="${AUTOLINK_BUILD_DIR}"
elif [[ -x "${ROOT}/build-macos/bin/examples/autolink_example_amw_talker" ]]; then
  BUILD="${ROOT}/build-macos"
elif [[ -x "${ROOT}/build/bin/examples/autolink_example_amw_talker" ]]; then
  BUILD="${ROOT}/build"
else
  BUILD="${ROOT}/build"
fi
BIN="${BUILD}/bin/examples"
IMPL="${1:-amw_fastdds}"
DOMAIN="${2:-0}"
SECONDS_RUN="${3:-8}"
# Distinct fake LAN IPs → RoleAttributes.host_ip differ → DIFF_HOST → RTPS
IP_A="${AUTOLINK_SIM_IP_A:-10.255.0.1}"
IP_B="${AUTOLINK_SIM_IP_B:-10.255.0.2}"
LOG_DIR="${TMPDIR:-/tmp}/amw_sim_dual_$$"
mkdir -p "${LOG_DIR}"

cleanup() {
  if [[ -n "${LISTENER_PID:-}" ]] && kill -0 "${LISTENER_PID}" 2>/dev/null; then
    kill "${LISTENER_PID}" 2>/dev/null || true
  fi
  if [[ -n "${TALKER_PID:-}" ]] && kill -0 "${TALKER_PID}" 2>/dev/null; then
    kill "${TALKER_PID}" 2>/dev/null || true
  fi
}
trap cleanup EXIT

"${ROOT}/scripts/amw_preflight.sh" "${BUILD}" pubsub || true

if [[ ! -x "${BIN}/autolink_example_amw_talker" ||
      ! -x "${BIN}/autolink_example_amw_listener" ]]; then
  echo "FAIL: build examples first" >&2
  exit 1
fi

export AUTOLINK_AMW_IMPLEMENTATION="${IMPL}"
export AUTOLINK_DOMAIN_ID="${DOMAIN}"
export ROS_DOMAIN_ID="${DOMAIN}"
case "${IMPL}" in
  *cyclone*) export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp ;;
  *) export RMW_IMPLEMENTATION=rmw_fastrtps_cpp ;;
esac

# Prefer user-installed Cyclone runtime.
if [[ -d "${HOME}/.local/cyclonedds/lib" ]]; then
  export DYLD_LIBRARY_PATH="${HOME}/.local/cyclonedds/lib:${DYLD_LIBRARY_PATH:-}"
  export LD_LIBRARY_PATH="${HOME}/.local/cyclonedds/lib:${LD_LIBRARY_PATH:-}"
fi
if [[ -d /usr/local/cyclonedds/lib ]]; then
  export LD_LIBRARY_PATH="/usr/local/cyclonedds/lib:${LD_LIBRARY_PATH:-}"
fi

# Autolink glog writes to CWD by default; force stderr so redirected logs work.
export GLOG_logtostderr=1
export GLOG_alsologtostderr=1
export GLOG_colorlogtostderr=0

echo "Sim dual-host: A=${IP_A} B=${IP_B} impl=${IMPL} domain=${DOMAIN}"
echo "Logs: ${LOG_DIR}"

(
  cd "${LOG_DIR}"
  AUTOLINK_IP="${IP_B}" "${BIN}/autolink_example_amw_listener" \
    >"${LOG_DIR}/listener.log" 2>&1 &
  echo $! >"${LOG_DIR}/listener.pid"
)
LISTENER_PID="$(cat "${LOG_DIR}/listener.pid")"
sleep 1

(
  cd "${LOG_DIR}"
  AUTOLINK_IP="${IP_A}" "${BIN}/autolink_example_amw_talker" \
    >"${LOG_DIR}/talker.log" 2>&1 &
  echo $! >"${LOG_DIR}/talker.pid"
)
TALKER_PID="$(cat "${LOG_DIR}/talker.pid")"

log_has() {
  local pattern="$1"
  # stderr capture + any leftover glog files in LOG_DIR
  grep -q "${pattern}" "${LOG_DIR}/listener.log" "${LOG_DIR}/talker.log" \
    "${LOG_DIR}"/autolink_example_amw_*.log.INFO* 2>/dev/null
}

ok=0
for ((i = 0; i < SECONDS_RUN * 2; ++i)); do
  if log_has "amw_listener seq="; then
    ok=1
    break
  fi
  if grep -qiE "network middleware is STUB|network middleware is stub|RTPS unavailable for DIFF_HOST" \
      "${LOG_DIR}/talker.log" "${LOG_DIR}/listener.log" 2>/dev/null; then
    echo "FAIL: DDS stub / RTPS unavailable (see ${LOG_DIR})" >&2
    grep -iE "STUB|stub|RTPS unavailable|AMW initialized" \
      "${LOG_DIR}/talker.log" "${LOG_DIR}/listener.log" 2>/dev/null | head -20 >&2
    exit 2
  fi
  sleep 0.5
done

cleanup
trap - EXIT

if [[ "${ok}" -eq 1 ]]; then
  echo "PASS: listener received chatter over simulated DIFF_HOST (RTPS)"
  grep "amw_listener seq=" "${LOG_DIR}/listener.log" \
    "${LOG_DIR}"/autolink_example_amw_*.log.INFO* 2>/dev/null | head -3 || true
  exit 0
fi

echo "FAIL: no listener seq within ${SECONDS_RUN}s (logs: ${LOG_DIR})" >&2
echo "--- listener ---" >&2
tail -60 "${LOG_DIR}/listener.log" >&2 || true
echo "--- talker ---" >&2
tail -60 "${LOG_DIR}/talker.log" >&2 || true
exit 1
