#!/usr/bin/env bash
#
# Build TangoQ locally on macOS.
#
#   ./tools/tangoq_build_macos.sh            configure if needed, then build
#   ./tools/tangoq_build_macos.sh configure  force a fresh configure
#   ./tools/tangoq_build_macos.sh build      build only (the usual dev loop)
#   ./tools/tangoq_build_macos.sh run        launch the built app
#   ./tools/tangoq_build_macos.sh install    install a complete .app bundle
#   ./tools/tangoq_build_macos.sh clean      remove the build directory
#
# 'build' produces a runnable but incomplete bundle: the binary only, with no
# icon, and resources read from the source tree. That is fine for development.
# 'install' runs CMake's install rules, which copy application.icns and the
# rest of the resources in, giving a self-contained, double-clickable app.
#
# Environment overrides:
#   VCPKG_ROOT    dependency tree      (default: ~/mixxx-vcpkg)
#   BUILD_DIR     build output         (default: <repo>/build)
#   BUILD_TYPE    Release or Debug     (default: Release)
#   JOBS          parallel build jobs  (default: number of CPUs)
#   PREFIX        install location     (default: ~/Applications)

set -euo pipefail

MIXXX_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VCPKG_ROOT="${VCPKG_ROOT:-${HOME}/mixxx-vcpkg}"
BUILD_DIR="${BUILD_DIR:-${MIXXX_ROOT}/build}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
JOBS="${JOBS:-$(sysctl -n hw.ncpu)}"

TRIPLET="arm64-osx-min1100-release"
DEPLOYMENT_TARGET="11.0"

# Fork identity. Kept distinct from upstream Mixxx so both can be installed side
# by side: a different bundle identifier means macOS treats them as separate apps
# for Launch Services and privacy (TCC) permissions. The settings directory is
# derived separately, from kMixxx in src/util/versionstore.cpp -- keep the two in
# sync if you rename. Deliberately not under org.mixxx.*, which upstream owns.
# These now match the defaults in CMakeLists.txt, so passing them below is
# redundant for an ordinary build. Kept deliberately: the script still works if
# the defaults change, it documents which values it expects, and it lets you
# build a differently-named bundle by exporting BUNDLE_NAME.
BUNDLE_NAME="${BUNDLE_NAME:-TangoQ}"
BUNDLE_IDENTIFIER="${BUNDLE_IDENTIFIER:-io.github.seemantadutta.tangoq}"

die() { echo "error: $*" >&2; exit 1; }

# --- cmake -------------------------------------------------------------------
# Prefer one on PATH; otherwise fall back to the copy vcpkg downloads for itself.
find_cmake() {
    if command -v cmake >/dev/null 2>&1; then
        command -v cmake
        return
    fi
    local bundled
    bundled=$(ls -d "${VCPKG_ROOT}"/downloads/tools/cmake-*/*/CMake.app/Contents/bin/cmake 2>/dev/null | head -1 || true)
    [ -n "${bundled}" ] && echo "${bundled}"
}

CMAKE="$(find_cmake)"
[ -n "${CMAKE}" ] || die "no cmake found. Install one with 'brew install cmake', \
or run vcpkg once so it downloads its own copy."

# --- SDK ---------------------------------------------------------------------
# The macOS 26.5 SDK removed AGL.framework, which Qt 6.5.3 still lists in
# Qt6Gui's link interface. Linking against a newer SDK therefore dies with
# "ld: framework 'AGL' not found", so pin to an SDK that still ships it.
# This must match VCPKG_OSX_SYSROOT in the triplet the dependencies were built
# with, or Mixxx and its dependencies disagree about what the platform offers.
SDK_CANDIDATES=(
    "/Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk"
    "/Library/Developer/CommandLineTools/SDKs/MacOSX15.sdk"
    "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX15.4.sdk"
)

OSX_SYSROOT="${OSX_SYSROOT:-}"
if [ -z "${OSX_SYSROOT}" ]; then
    for sdk in "${SDK_CANDIDATES[@]}"; do
        if [ -d "${sdk}/System/Library/Frameworks/AGL.framework" ]; then
            OSX_SYSROOT="${sdk}"
            break
        fi
    done
fi
[ -n "${OSX_SYSROOT}" ] || die "no SDK containing AGL.framework found. Qt 6.5.3 needs it \
at link time. Checked: ${SDK_CANDIDATES[*]}. Set OSX_SYSROOT=/path/to/MacOSX<ver>.sdk to override."

# --- dependencies ------------------------------------------------------------
TOOLCHAIN="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
[ -f "${TOOLCHAIN}" ] || die "vcpkg toolchain not found at ${TOOLCHAIN}. \
Set VCPKG_ROOT, or build the dependencies first."
[ -d "${VCPKG_ROOT}/installed/${TRIPLET}" ] || die "no dependencies installed for ${TRIPLET}. \
Run this in ${VCPKG_ROOT} first:
  ./vcpkg --x-install-root=installed --triplet=${TRIPLET} --host-triplet=${TRIPLET} install"

do_configure() {
    echo "==> configuring (${BUILD_TYPE}) with SDK ${OSX_SYSROOT}"
    "${CMAKE}" -S "${MIXXX_ROOT}" -B "${BUILD_DIR}" -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN}" \
        -DVCPKG_TARGET_TRIPLET="${TRIPLET}" \
        -DVCPKG_HOST_TRIPLET="${TRIPLET}" \
        -DCMAKE_OSX_SYSROOT="${OSX_SYSROOT}" \
        -DCMAKE_OSX_DEPLOYMENT_TARGET="${DEPLOYMENT_TARGET}" \
        -DCMAKE_OSX_ARCHITECTURES=arm64 \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DMACOS_BUNDLE_NAME="${BUNDLE_NAME}" \
        -DMACOS_BUNDLE_IDENTIFIER="${BUNDLE_IDENTIFIER}" \
        -DBULK=ON -DCOREAUDIO=ON -DHSS1394=ON -DMACOS_BUNDLE=ON \
        -DMODPLUG=ON -DQT6=ON -DWAVPACK=ON
}

do_build() {
    [ -f "${BUILD_DIR}/build.ninja" ] || do_configure
    echo "==> building with ${JOBS} jobs"
    "${CMAKE}" --build "${BUILD_DIR}" --parallel "${JOBS}"
    echo "==> built ${BUILD_DIR}/${BUNDLE_NAME}.app"
}

do_install() {
    [ -f "${BUILD_DIR}/build.ninja" ] || die "nothing built yet. Run '$(basename "$0") build' first."
    local prefix="${PREFIX:-${HOME}/Applications}"
    mkdir -p "${prefix}"
    echo "==> installing to ${prefix}"
    "${CMAKE}" --install "${BUILD_DIR}" --prefix "${prefix}"
    echo "==> installed ${prefix}/${BUNDLE_NAME}.app"
    echo "    launch with: open '${prefix}/${BUNDLE_NAME}.app'"
}

do_run() {
    local app="${BUILD_DIR}/${BUNDLE_NAME}.app"
    [ -d "${app}" ] || die "${BUNDLE_NAME}.app not found. Build it first."
    # Run the binary rather than 'open' so console output lands in this terminal.
    exec "${app}/Contents/MacOS/${BUNDLE_NAME}" "$@"
}

case "${1:-build}" in
    configure) do_configure ;;
    build)     do_build ;;
    run)       shift; do_run "$@" ;;
    install)   do_install ;;
    clean)     echo "==> removing ${BUILD_DIR}"; rm -rf "${BUILD_DIR}" ;;
    -h|--help|help) awk 'NR>1 && /^#/ {sub(/^# ?/, ""); print; next} NR>1 {exit}' "${BASH_SOURCE[0]}" ;;
    *)         die "unknown command '${1}'. Try: configure, build, run, clean, help" ;;
esac
