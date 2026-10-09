#!/usr/bin/env bash
#
# autolink.sh — one-shot CLI for the Autolink Bazel workspace.
#
# Wraps `bazel build / test / clean` and installs into an FHS-like prefix
# (lib/, bin/, include/, share/) matching the historical CMake layout.
#
# Quick start:
#   ./autolink.sh help
#   ./autolink.sh build
#   ./autolink.sh test
#   ./autolink.sh install --prefix /usr/local
#   ./autolink.sh all --prefix ./install
#   ./autolink.sh clean
#
# Global flags may appear before or after the subcommand:
#   ./autolink.sh -j 16 build
#   ./autolink.sh install -j 8 --prefix /opt/autolink
#
# Install layout (PREFIX):
#   lib/libautolink.so
#   bin/autolink              # CLI (Bazel target //:autolink_cli)
#   bin/mainboard
#   include/autolink/...
#   share/autolink/conf/*.conf
#
# Environment:
#   PREFIX   Install root                  (default: <workspace>/install)
#   JOBS     Bazel --jobs                  (default: nproc)
#   BAZEL    Bazel / Bazelisk binary       (default: bazel)
#   VERBOSE  Print bazel command lines     (1 = on)
#
set -euo pipefail

# ---------------------------------------------------------------------------
# Workspace + defaults
# ---------------------------------------------------------------------------

readonly SCRIPT_NAME="$(basename "${BASH_SOURCE[0]}")"
readonly ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

cd "${ROOT}"

BAZEL="${BAZEL:-bazel}"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 8)}"
PREFIX="${PREFIX:-${ROOT}/install}"
VERBOSE="${VERBOSE:-0}"

readonly BUILD_TARGETS=(
  //:autolink
  //:mainboard
  //:autolink_cli
)

# ---------------------------------------------------------------------------
# Logging
# ---------------------------------------------------------------------------

die() {
  echo "${SCRIPT_NAME}: error: $*" >&2
  exit 1
}

info() {
  echo "==> $*"
}

ok() {
  echo "ok  $*"
}

note() {
  echo "    $*"
}

# ---------------------------------------------------------------------------
# Help text (one function per topic)
# ---------------------------------------------------------------------------

usage_summary() {
  cat <<EOF
Usage: ${SCRIPT_NAME} [global options] <command> [command options]

Commands:
  build       Compile libautolink, mainboard, and the CLI
  install     Build (if needed) and install into PREFIX
  test        Build, smoke-check binaries, run bazel tests when present
  clean       Wipe the Bazel cache (and ./install when PREFIX is default)
  all         Run test, then install (full local pipeline)
  status      Show workspace / toolchain / artifact summary
  help        Show this help, or help for a command

Global options:
  -j, --jobs N         Bazel parallel jobs          [default: ${JOBS}]
      --prefix DIR     Install prefix               [default: ${PREFIX}]
      --bazel PATH     Bazel binary                 [default: ${BAZEL}]
  -v, --verbose        Print underlying bazel commands
  -h, --help           Same as: ${SCRIPT_NAME} help

Command help:
  ${SCRIPT_NAME} help <command>
  ${SCRIPT_NAME} <command> --help

Examples:
  ${SCRIPT_NAME} build
  ${SCRIPT_NAME} -j 16 build
  ${SCRIPT_NAME} test
  ${SCRIPT_NAME} install --prefix /usr/local
  ${SCRIPT_NAME} all --prefix ${ROOT}/install
  ${SCRIPT_NAME} clean
  PREFIX=/opt/autolink ${SCRIPT_NAME} install

Environment:
  PREFIX   JOBS   BAZEL   VERBOSE
EOF
}

usage_build() {
  cat <<EOF
Usage: ${SCRIPT_NAME} build [global options]

Build Autolink with Bazel:

  ${BUILD_TARGETS[*]}

Artifacts land under bazel-bin/:

  libautolink.so
  mainboard
  autolink_cli

Options: see global options (-j / --bazel / -v).
EOF
}

usage_install() {
  cat <<EOF
Usage: ${SCRIPT_NAME} install [global options] [--prefix DIR]

Build (if needed), then install into PREFIX.

Layout:
  PREFIX/lib/libautolink.so
  PREFIX/bin/autolink
  PREFIX/bin/mainboard
  PREFIX/include/autolink/**
  PREFIX/share/autolink/conf/*.conf

After install:
  export LD_LIBRARY_PATH=PREFIX/lib:\$LD_LIBRARY_PATH
  export PATH=PREFIX/bin:\$PATH

Options:
      --prefix DIR     Install root (overrides PREFIX)
  -h, --help           Show this help

Examples:
  ${SCRIPT_NAME} install
  ${SCRIPT_NAME} install --prefix /usr/local
  ${SCRIPT_NAME} --prefix /opt/autolink install
EOF
}

usage_test() {
  cat <<EOF
Usage: ${SCRIPT_NAME} test [global options]

1. Build core targets
2. Smoke-check bazel-bin/{autolink_cli,mainboard,libautolink.so}
3. If any bazel test targets exist under //..., run: bazel test //...

Options: see global options (-j / --bazel / -v).
EOF
}

usage_clean() {
  cat <<EOF
Usage: ${SCRIPT_NAME} clean [global options]

Runs: bazel clean --expunge

Also removes <workspace>/install when PREFIX still points at the default
local install directory.

Does not delete a custom --prefix (for example /usr/local).

Options:
  -h, --help           Show this help
EOF
}

usage_all() {
  cat <<EOF
Usage: ${SCRIPT_NAME} all [global options] [--prefix DIR]

Full local pipeline:

  1. test      (build + smoke + bazel test if any)
  2. install   (into PREFIX)

Equivalent to:
  ${SCRIPT_NAME} test && ${SCRIPT_NAME} install [--prefix DIR]
EOF
}

usage_status() {
  cat <<EOF
Usage: ${SCRIPT_NAME} status

Print workspace path, Bazel version, configured JOBS/PREFIX, and whether
bazel-bin artifacts / PREFIX contents currently exist.
EOF
}

usage_command() {
  case "${1:-}" in
    build)   usage_build ;;
    install) usage_install ;;
    test)    usage_test ;;
    clean)   usage_clean ;;
    all)     usage_all ;;
    status)  usage_status ;;
    help|"") usage_summary ;;
    *)
      usage_summary >&2
      die "no help for unknown command: $1"
      ;;
  esac
}

# ---------------------------------------------------------------------------
# Shared helpers
# ---------------------------------------------------------------------------

require_bazel() {
  command -v "${BAZEL}" >/dev/null 2>&1 \
    || die "'${BAZEL}' not found in PATH (set --bazel or BAZEL=)"
  [[ -f "${ROOT}/MODULE.bazel" ]] \
    || die "not an Autolink Bazel workspace: ${ROOT}"
}

run_bazel() {
  if [[ "${VERBOSE}" == "1" ]]; then
    info "+ ${BAZEL} $*"
  fi
  "${BAZEL}" "$@"
}

bazel_bin_dir() {
  if [[ -d "${ROOT}/bazel-bin" ]]; then
    printf '%s\n' "${ROOT}/bazel-bin"
  else
    run_bazel info bazel-bin
  fi
}

# Reject unknown args; honor -h/--help via caller-supplied usage fn name.
parse_no_args() {
  local usage_fn="$1"
  shift
  while [[ $# -gt 0 ]]; do
    case "$1" in
      -h|--help)
        "${usage_fn}"
        return 1
        ;;
      *)
        die "${usage_fn#usage_}: unexpected argument: $1 (try --help)"
        ;;
    esac
  done
  return 0
}

parse_prefix_arg() {
  case "$1" in
    --prefix)
      [[ $# -ge 2 ]] || die "--prefix needs a directory"
      PREFIX="$2"
      return 2
      ;;
    --prefix=*)
      PREFIX="${1#--prefix=}"
      return 1
      ;;
    *)
      return 0
      ;;
  esac
}

# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------

cmd_build() {
  parse_no_args usage_build "$@" || return 0
  require_bazel
  info "build (${JOBS} jobs)"
  note "${BUILD_TARGETS[*]}"
  run_bazel build --jobs="${JOBS}" "${BUILD_TARGETS[@]}"
  ok "build → $(bazel_bin_dir)"
}

# ---------------------------------------------------------------------------
# Test
# ---------------------------------------------------------------------------

smoke_cli() {
  local cli="$1"
  [[ -x "${cli}" ]] || die "missing executable: ${cli}"
  info "smoke: autolink_cli --help"
  "${cli}" --help >/dev/null
}

smoke_mainboard() {
  local board="$1"
  [[ -x "${board}" ]] || die "missing executable: ${board}"
  info "smoke: mainboard linkage"
  if ! "${board}" --help >/dev/null 2>&1 && ! "${board}" -h >/dev/null 2>&1; then
    ldd "${board}" >/dev/null
  fi
}

smoke_library() {
  local lib="$1"
  [[ -f "${lib}" ]] || die "missing library: ${lib}"
  info "smoke: libautolink.so"
  file "${lib}" | grep -qi 'shared object' || die "${lib} is not a shared object"
}

run_bazel_tests_if_any() {
  if run_bazel query 'kind(".*_test rule", //...)' 2>/dev/null | grep -q .; then
    info "bazel test //..."
    run_bazel test --jobs="${JOBS}" //...
  else
    info "no bazel test targets under // — smoke only"
  fi
}

cmd_test() {
  parse_no_args usage_test "$@" || return 0
  require_bazel
  cmd_build

  local bin
  bin="$(bazel_bin_dir)"
  smoke_cli "${bin}/autolink_cli"
  smoke_mainboard "${bin}/mainboard"
  smoke_library "${bin}/libautolink.so"
  run_bazel_tests_if_any
  ok "test passed"
}

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------

install_binaries() {
  local bin="$1"
  local dest="$2"
  install -m 755 "${bin}/libautolink.so" "${dest}/lib/libautolink.so"
  install -m 755 "${bin}/mainboard" "${dest}/bin/mainboard"
  # CMake historically used OUTPUT_NAME "autolink" for the CLI.
  install -m 755 "${bin}/autolink_cli" "${dest}/bin/autolink"
}

install_source_headers() {
  local dest="$1"
  if command -v rsync >/dev/null 2>&1; then
    rsync -a --delete \
      --include='*/' \
      --include='*.hpp' \
      --include='*.h' \
      --exclude='*' \
      "${ROOT}/autolink/" "${dest}/include/autolink/"
    return
  fi
  rm -rf "${dest}/include/autolink"
  mkdir -p "${dest}/include/autolink"
  (cd "${ROOT}/autolink" && find . -type f \( -name '*.hpp' -o -name '*.h' \) -print0 \
    | cpio -0pd "${dest}/include/autolink")
}

install_generated_headers() {
  local bin="$1"
  local dest="$2"
  if [[ -f "${bin}/autolink/conf/conf.hpp" ]]; then
    mkdir -p "${dest}/include/autolink/conf"
    install -m 644 "${bin}/autolink/conf/conf.hpp" \
      "${dest}/include/autolink/conf/conf.hpp"
  fi
  if [[ -d "${bin}/autolink/proto" ]]; then
    mkdir -p "${dest}/include/autolink/proto"
    find "${bin}/autolink/proto" -maxdepth 1 -name '*.pb.h' -exec \
      install -m 644 {} "${dest}/include/autolink/proto/" \;
  fi
}

install_conf_files() {
  local dest="$1"
  if compgen -G "${ROOT}/autolink/conf/*.conf" >/dev/null; then
    install -m 644 "${ROOT}/autolink/conf/"*.conf "${dest}/share/autolink/conf/"
  fi
}

parse_install_args() {
  while [[ $# -gt 0 ]]; do
    case "$1" in
      -h|--help)
        usage_install
        return 1
        ;;
      --prefix|--prefix=*)
        local n=0
        parse_prefix_arg "$@" || n=$?
        [[ "${n}" -gt 0 ]] || die "install: bad --prefix"
        shift "${n}"
        ;;
      *)
        die "install: unexpected argument: $1 (try --help)"
        ;;
    esac
  done
  return 0
}

cmd_install() {
  parse_install_args "$@" || return 0
  require_bazel
  cmd_build

  local bin dest
  bin="$(bazel_bin_dir)"
  dest="${PREFIX}"
  [[ -d "${bin}" ]] || die "bazel-bin missing; build failed?"

  info "install → ${dest}"
  mkdir -p \
    "${dest}/lib" \
    "${dest}/bin" \
    "${dest}/include" \
    "${dest}/share/autolink/conf"

  install_binaries "${bin}" "${dest}"
  install_source_headers "${dest}"
  install_generated_headers "${bin}" "${dest}"
  install_conf_files "${dest}"

  ok "installed under ${dest}"
  note "export LD_LIBRARY_PATH=${dest}/lib:\${LD_LIBRARY_PATH}"
  note "export PATH=${dest}/bin:\${PATH}"
}

# ---------------------------------------------------------------------------
# Clean / all / status
# ---------------------------------------------------------------------------

cmd_clean() {
  parse_no_args usage_clean "$@" || return 0
  require_bazel
  info "bazel clean --expunge"
  run_bazel clean --expunge
  if [[ -d "${ROOT}/install" && "${PREFIX}" == "${ROOT}/install" ]]; then
    info "remove default install tree: ${ROOT}/install"
    rm -rf "${ROOT}/install"
  fi
  ok "clean complete"
}

cmd_all() {
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --prefix|--prefix=*) break ;;
      -h|--help) usage_all; return 0 ;;
      *) die "all: unexpected argument: $1 (try --help)" ;;
    esac
  done
  cmd_test
  cmd_install "$@"
}

status_print_config() {
  echo "workspace : ${ROOT}"
  echo "bazel     : ${BAZEL} ($(command -v "${BAZEL}" 2>/dev/null || echo 'not found'))"
  if command -v "${BAZEL}" >/dev/null 2>&1; then
    echo "version   : $(${BAZEL} --version 2>/dev/null | head -1 || echo unknown)"
  fi
  echo "jobs      : ${JOBS}"
  echo "prefix    : ${PREFIX}"
  echo "verbose   : ${VERBOSE}"
}

status_print_bazel_bin() {
  local bin=""
  [[ -d "${ROOT}/bazel-bin" ]] && bin="${ROOT}/bazel-bin"
  echo "bazel-bin : ${bin:-'(not built yet)'}"
  [[ -n "${bin}" ]] || return 0
  local f
  for f in libautolink.so mainboard autolink_cli; do
    if [[ -e "${bin}/${f}" ]]; then
      echo "  artifact: ${f}  ($(du -h "${bin}/${f}" | awk '{print $1}'))"
    else
      echo "  missing : ${f}"
    fi
  done
}

status_print_prefix() {
  if [[ ! -d "${PREFIX}" ]]; then
    echo "install   : (no PREFIX directory yet)"
    return 0
  fi
  echo "install   : present"
  local f
  for f in lib/libautolink.so bin/autolink bin/mainboard; do
    if [[ -e "${PREFIX}/${f}" ]]; then
      echo "  present : ${f}"
    else
      echo "  missing : ${f}"
    fi
  done
}

cmd_status() {
  parse_no_args usage_status "$@" || return 0
  status_print_config
  status_print_bazel_bin
  status_print_prefix
}

# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------

# Sets PARSE_SHIFT / PARSE_HELP. Returns 0 if $1 is a global flag.
parse_global_flag() {
  PARSE_SHIFT=0
  PARSE_HELP=0
  case "$1" in
    -j|--jobs)
      [[ $# -ge 2 ]] || die "$1 needs a value"
      JOBS="$2"
      PARSE_SHIFT=2
      ;;
    --jobs=*)
      JOBS="${1#--jobs=}"
      PARSE_SHIFT=1
      ;;
    --prefix|--prefix=*)
      local n=0
      parse_prefix_arg "$@" || n=$?
      [[ "${n}" -gt 0 ]] || return 1
      PARSE_SHIFT="${n}"
      ;;
    --bazel)
      [[ $# -ge 2 ]] || die "--bazel needs a path"
      BAZEL="$2"
      PARSE_SHIFT=2
      ;;
    --bazel=*)
      BAZEL="${1#--bazel=}"
      PARSE_SHIFT=1
      ;;
    -v|--verbose)
      VERBOSE=1
      PARSE_SHIFT=1
      ;;
    -h|--help)
      PARSE_SHIFT=1
      PARSE_HELP=1
      ;;
    *)
      return 1
      ;;
  esac
  return 0
}

dispatch_command() {
  local cmd="$1"
  shift
  case "${cmd}" in
    build)   cmd_build "$@" ;;
    install) cmd_install "$@" ;;
    test)    cmd_test "$@" ;;
    clean)   cmd_clean "$@" ;;
    all)     cmd_all "$@" ;;
    status)  cmd_status "$@" ;;
    help|-h|--help)
      if [[ $# -eq 0 ]]; then
        usage_summary
      else
        usage_command "$1"
      fi
      ;;
    *)
      usage_summary >&2
      die "unknown command: ${cmd}"
      ;;
  esac
}

main() {
  local cmd=""
  local -a cmd_args=()

  while [[ $# -gt 0 ]]; do
    if parse_global_flag "$@"; then
      if [[ "${PARSE_HELP}" == "1" && $# -eq "${PARSE_SHIFT}" ]]; then
        usage_summary
        return 0
      fi
      shift "${PARSE_SHIFT}"
      continue
    fi
    break
  done

  cmd="${1:-help}"
  shift || true

  while [[ $# -gt 0 ]]; do
    if parse_global_flag "$@"; then
      if [[ "${PARSE_HELP}" == "1" ]]; then
        usage_command "${cmd}"
        return 0
      fi
      shift "${PARSE_SHIFT}"
      continue
    fi
    cmd_args+=("$1")
    shift
  done

  dispatch_command "${cmd}" "${cmd_args[@]+"${cmd_args[@]}"}"
}

main "$@"
