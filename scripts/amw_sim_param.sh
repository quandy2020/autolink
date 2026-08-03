#!/usr/bin/env bash
# Same-machine DIFF_HOST Parameter simulation (different AUTOLINK_IP → RTPS).
#
# Usage:
#   ./scripts/amw_sim_param.sh [impl=amw_cyclonedds] [domain=0] [seconds=15]

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
if [[ -n "${AUTOLINK_BUILD_DIR:-}" ]]; then
  BUILD="${AUTOLINK_BUILD_DIR}"
elif [[ -x "${ROOT}/build-macos/bin/examples/autolink_example_amw_param_server" ]]; then
  BUILD="${ROOT}/build-macos"
elif [[ -x "${ROOT}/build/bin/examples/autolink_example_amw_param_server" ]]; then
  BUILD="${ROOT}/build"
else
  BUILD="${ROOT}/build"
fi
BIN="${BUILD}/bin/examples"
IMPL="${1:-amw_cyclonedds}"
DOMAIN="${2:-0}"
SECONDS_RUN="${3:-15}"
IP_A="${AUTOLINK_SIM_IP_A:-10.255.0.1}"
IP_B="${AUTOLINK_SIM_IP_B:-10.255.0.2}"
LOG_DIR="${TMPDIR:-/tmp}/amw_sim_param_$$"
mkdir -p "${LOG_DIR}"

cleanup() {
  if [[ -n "${SERVER_PID:-}" ]] && kill -0 "${SERVER_PID}" 2>/dev/null; then
    kill "${SERVER_PID}" 2>/dev/null || true
  fi
  if [[ -n "${CLIENT_PID:-}" ]] && kill -0 "${CLIENT_PID}" 2>/dev/null; then
    kill "${CLIENT_PID}" 2>/dev/null || true
  fi
}
trap cleanup EXIT

"${ROOT}/scripts/amw_preflight.sh" "${BUILD}" all || true

if [[ ! -x "${BIN}/autolink_example_amw_param_server" ||
      ! -x "${BIN}/autolink_example_amw_param_client" ]]; then
  echo "FAIL: build amw_param_server/client examples first" >&2
  exit 1
fi

export AUTOLINK_AMW_IMPLEMENTATION="${IMPL}"
export AUTOLINK_DOMAIN_ID="${DOMAIN}"
export ROS_DOMAIN_ID="${DOMAIN}"
case "${IMPL}" in
  *cyclone*) export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp ;;
  *) export RMW_IMPLEMENTATION=rmw_fastrtps_cpp ;;
esac

if [[ -d "${HOME}/.local/cyclonedds/lib" ]]; then
  export DYLD_LIBRARY_PATH="${HOME}/.local/cyclonedds/lib:${DYLD_LIBRARY_PATH:-}"
  export LD_LIBRARY_PATH="${HOME}/.local/cyclonedds/lib:${LD_LIBRARY_PATH:-}"
fi
if [[ -d /opt/ros/humble/lib/x86_64-linux-gnu ]]; then
  export LD_LIBRARY_PATH="/opt/ros/humble/lib/x86_64-linux-gnu:/opt/ros/humble/lib:${LD_LIBRARY_PATH:-}"
fi

export GLOG_logtostderr=1
export GLOG_alsologtostderr=1
export GLOG_colorlogtostderr=0

echo "Sim param dual-host: A=${IP_A}(client) B=${IP_B}(server) impl=${IMPL} domain=${DOMAIN}"
echo "Logs: ${LOG_DIR}"

(
  cd "${LOG_DIR}"
  AUTOLINK_IP="${IP_B}" "${BIN}/autolink_example_amw_param_server" \
    >"${LOG_DIR}/server.log" 2>&1 &
  echo $! >"${LOG_DIR}/server.pid"
)
SERVER_PID="$(cat "${LOG_DIR}/server.pid")"
sleep 1

(
  cd "${LOG_DIR}"
  AUTOLINK_IP="${IP_A}" "${BIN}/autolink_example_amw_param_client" \
    >"${LOG_DIR}/client.log" 2>&1 &
  echo $! >"${LOG_DIR}/client.pid"
)
CLIENT_PID="$(cat "${LOG_DIR}/client.pid")"

ok=0
for ((i = 0; i < SECONDS_RUN * 2; ++i)); do
  if grep -q "amw_param_client set amw_demo_str=hello ok" "${LOG_DIR}/client.log" 2>/dev/null &&
     grep -q "amw_param_client get amw_demo_int=42" "${LOG_DIR}/client.log" 2>/dev/null &&
     grep -q "amw_param_client get amw_demo_str=hello" "${LOG_DIR}/client.log" 2>/dev/null; then
    ok=1
    break
  fi
  if grep -qiE "network middleware is STUB|network middleware is stub" \
      "${LOG_DIR}/client.log" "${LOG_DIR}/server.log" 2>/dev/null; then
    echo "FAIL: DDS stub (see ${LOG_DIR})" >&2
    exit 2
  fi
  if [[ -n "${CLIENT_PID:-}" ]] && ! kill -0 "${CLIENT_PID}" 2>/dev/null; then
    if grep -q "amw_param_client set amw_demo_str=hello ok" "${LOG_DIR}/client.log" 2>/dev/null &&
       grep -q "amw_param_client get amw_demo_int=42" "${LOG_DIR}/client.log" 2>/dev/null &&
       grep -q "amw_param_client get amw_demo_str=hello" "${LOG_DIR}/client.log" 2>/dev/null; then
      ok=1
    fi
    break
  fi
  sleep 0.5
done

cleanup
trap - EXIT

if [[ "${ok}" -eq 1 ]]; then
  echo "PASS: parameter set/get over simulated DIFF_HOST (RTPS)"
  grep "amw_param_client " "${LOG_DIR}/client.log" | grep -E "set |get " | head -10 || true
  exit 0
fi

echo "FAIL: no parameter set/get within ${SECONDS_RUN}s (logs: ${LOG_DIR})" >&2
echo "--- server ---" >&2
tail -60 "${LOG_DIR}/server.log" >&2 || true
echo "--- client ---" >&2
tail -60 "${LOG_DIR}/client.log" >&2 || true
exit 1
