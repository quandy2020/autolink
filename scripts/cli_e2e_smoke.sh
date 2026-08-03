#!/usr/bin/env bash
# End-to-end smoke for unified autolink CLI (P1 debug path).
# Usage:
#   ./scripts/cli_e2e_smoke.sh [build_dir=build]
#
# Requires examples + autolink_cli built. Cleans up background processes on exit.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${1:-${AUTOLINK_BUILD_DIR:-${ROOT}/build}}"
BIN="${BUILD}/bin/autolink"
EX="${BUILD}/bin/examples"
PROTO_DIR="${ROOT}/examples/cpp/proto"
FDSET="$(mktemp /tmp/autolink_examples.XXXXXX.pb)"
LOGDIR="$(mktemp -d /tmp/autolink_cli_e2e.XXXXXX)"

export AUTOLINK_PATH="${AUTOLINK_PATH:-${ROOT}}"
export LD_LIBRARY_PATH="${BUILD}/lib:${LD_LIBRARY_PATH:-}"
export AUTOLINK_AMW_IMPLEMENTATION="${AUTOLINK_AMW_IMPLEMENTATION:-amw_cyclonedds}"

PIDS=()
cleanup() {
  local p
  for p in "${PIDS[@]:-}"; do
    kill "$p" 2>/dev/null || true
  done
  for p in "${PIDS[@]:-}"; do
    wait "$p" 2>/dev/null || true
  done
  PIDS=()
  rm -f "${FDSET}"
  rm -rf "${LOGDIR}"
}
trap cleanup EXIT

stop_bg() {
  local p
  for p in "${PIDS[@]:-}"; do
    kill "$p" 2>/dev/null || true
  done
  for p in "${PIDS[@]:-}"; do
    wait "$p" 2>/dev/null || true
  done
  PIDS=()
  sleep 1
}

die() { echo "FAIL: $*" >&2; exit 1; }
ok() { echo "OK: $*"; }

wait_cli_grep() {
  local pattern="$1"
  shift
  local i
  for i in $(seq 1 40); do
    if "${BIN}" --wait 1 "$@" 2>/dev/null | grep -q -- "${pattern}"; then
      return 0
    fi
    sleep 1
  done
  return 1
}

[[ -x "${BIN}" ]] || die "missing ${BIN}"
[[ -x "${EX}/autolink_example_talker" ]] || die "missing talker example"
command -v protoc >/dev/null || die "protoc not found"

protoc -I "${PROTO_DIR}" --include_imports \
  --descriptor_set_out="${FDSET}" examples.proto \
  || die "protoc descriptor_set_out failed"
ok "descriptor set ${FDSET}"

# --- completion / doctor (no runtime peers) ---
"${BIN}" completion bash | grep -q "_autolink_completions" || die "bash completion"
"${BIN}" completion zsh | grep -q "_autolink" || die "zsh completion"
ok "completion scripts"
"${BIN}" --wait 0 doctor >/dev/null || die "doctor"
ok "doctor"

# --- channel ---
"${EX}/autolink_example_talker" >"${LOGDIR}/talker.log" 2>&1 &
PIDS+=($!)
wait_cli_grep "channel/chatter" channel list || die "channel/chatter not discovered"
ok "channel list"

out="$("${BIN}" --wait 2 channel type channel/chatter)" || die "channel type"
echo "${out}" | grep -qi "Chatter" || die "unexpected type: ${out}"
ok "channel type"

out="$("${BIN}" --wait 2 channel echo channel/chatter --once)" || die "channel echo"
[[ -n "${out}" ]] || die "empty echo"
ok "channel echo --once"

"${BIN}" --wait 1 channel pub channel/chatter \
  '{"seq":99,"content":"aGk="}' \
  --type autolink.examples.Chatter \
  --descriptor-set "${FDSET}" \
  --times 1 || die "channel pub"
ok "channel pub"
stop_bg

# --- action (before AMW service/param to avoid discovery churn) ---
"${EX}/autolink_example_action_listener" >"${LOGDIR}/action.log" 2>&1 &
PIDS+=($!)
accepted=0
for _ in $(seq 1 40); do
  if out="$("${BIN}" --wait 1 action send_goal examples/simple_message_action \
      '{"text":"cli-e2e"}' \
      --type autolink.examples.SimpleMessageAction.Goal \
      --descriptor-set "${FDSET}" 2>/dev/null)" \
      && echo "${out}" | grep -qi "accepted: true"; then
    accepted=1
    break
  fi
  sleep 1
done
[[ "${accepted}" -eq 1 ]] || {
  tail -80 "${LOGDIR}/action.log" >&2 || true
  die "action send_goal not accepted"
}
ok "action send_goal"
out="$("${BIN}" --wait 2 action list)" || true
echo "${out}" | grep -q "examples/simple_message_action" && ok "action list" \
  || echo "WARN: action list empty after send_goal (discovery lag)"
stop_bg

# --- service ---
"${EX}/autolink_example_amw_service" >"${LOGDIR}/service.log" 2>&1 &
PIDS+=($!)
wait_cli_grep "amw/driver" service list || {
  tail -40 "${LOGDIR}/service.log" >&2 || true
  die "amw/driver not discovered"
}
ok "service list"

out="$("${BIN}" --wait 2 service call amw/driver '{"msg_id":7}' \
  --type autolink.examples.Driver \
  --descriptor-set "${FDSET}" \
  --timeout 8)" || die "service call"
echo "${out}" | grep -Eq 'msgId|msg_id|7' || die "bad service response: ${out}"
ok "service call"
stop_bg

# --- param ---
"${EX}/autolink_example_amw_param_server" >"${LOGDIR}/param.log" 2>&1 &
PIDS+=($!)
ready=0
for _ in $(seq 1 40); do
  if out="$("${BIN}" --wait 1 param list amw_param_server 2>/dev/null)" \
      && echo "${out}" | grep -q "amw_demo_int"; then
    ready=1
    break
  fi
  sleep 1
done
[[ "${ready}" -eq 1 ]] || {
  tail -40 "${LOGDIR}/param.log" >&2 || true
  die "param server not ready"
}
ok "param list"

out="$("${BIN}" --wait 2 param get amw_param_server amw_demo_int)" || die "param get"
ok "param get: ${out}"

"${BIN}" --wait 2 param set amw_param_server amw_demo_int 100 || die "param set"
out="$("${BIN}" --wait 2 param get amw_param_server amw_demo_int)" || die "param get after set"
echo "${out}" | grep -q "100" || die "param set not applied: ${out}"
ok "param set/get"
stop_bg

echo
echo "All CLI e2e checks passed."
