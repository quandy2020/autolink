#!/usr/bin/env bash
# Build and install Eclipse Cyclone DDS into a user prefix (default: ~/.local/cyclonedds).
# Homebrew currently has no cyclonedds formula.
#
# Usage:
#   ./scripts/install_cyclonedds.sh [prefix] [version_tag]
#
# Then configure autolink:
#   cmake -B build \
#     -DAUTOLINK_ENABLE_CYCLONEDDS=ON \
#     -DCMAKE_PREFIX_PATH="$HOME/.local/cyclonedds;/usr/local"

set -euo pipefail

PREFIX="${1:-${HOME}/.local/cyclonedds}"
TAG="${2:-0.10.5}"
SRC_ROOT="${TMPDIR:-/tmp}/cyclonedds-src"
SRC_DIR="${SRC_ROOT}/cyclonedds"

mkdir -p "${SRC_ROOT}"
if [[ ! -d "${SRC_DIR}/.git" ]]; then
  git clone --depth 1 --branch "${TAG}" \
    https://github.com/eclipse-cyclonedds/cyclonedds.git "${SRC_DIR}"
fi

cmake -S "${SRC_DIR}" -B "${SRC_DIR}/build" \
  -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_EXAMPLES=OFF \
  -DBUILD_TESTING=OFF \
  -DENABLE_SSL=OFF \
  -DENABLE_SECURITY=OFF

cmake --build "${SRC_DIR}/build" -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"
cmake --install "${SRC_DIR}/build"

echo
echo "CycloneDDS installed to ${PREFIX}"
echo "Add to CMAKE_PREFIX_PATH and (macOS) DYLD_LIBRARY_PATH:"
echo "  export PATH=\"${PREFIX}/bin:\$PATH\""
echo "  export DYLD_LIBRARY_PATH=\"${PREFIX}/lib:\${DYLD_LIBRARY_PATH:-}\""
echo "  export LD_LIBRARY_PATH=\"${PREFIX}/lib:\${LD_LIBRARY_PATH:-}\""
