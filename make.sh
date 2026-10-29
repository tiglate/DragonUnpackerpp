#!/usr/bin/env bash
#
# make.sh -- one entry point to build, run, test, install and package Dragon UnPACKer++.
#
#   driver/   the format driver (C++20, a plain C ABI shared library, "DUDI 7")
#   app/      the Qt 6 interface (C++20, CMake + Ninja)
#
# See README.md for the prerequisites and CLAUDE.md for the architecture.
set -euo pipefail

SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DRIVER_DIR="$SOURCE_DIR/driver"
APP_DIR="$SOURCE_DIR/app"
BUILD_DIR="$SOURCE_DIR/build"
APP_BUILD_DIR="$APP_DIR/build"
INSTALL_PREFIX="$SOURCE_DIR/install"
PLUGIN="$BUILD_DIR/plugins/libdrv_default.d6d.so"
CLI="$BUILD_DIR/tools/dudi7cli"
JOBS="$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
VERBOSE=0
REBUILD=0

usage() {
    cat <<EOT
Usage: ./make.sh <action> [target] [options] [-- application arguments]

Actions:
  build [target]   Compile. Targets: "all" (default: driver, then the interface), "driver" (the plugin
                    libdrv_default.d6d.so and the dudi7cli test tool), "ui" (the Qt interface), "cli" (the
                    same build as "driver").
  rebuild [target] Same as "build" but recompiles everything.
  run              Build, then start the interface. Everything after "--" is passed to it:
                    --open=FILE --folder=a/b --select=NAME --theme=dark|light --recursive --sort=COL
                    --extract-all=DIR | --extract-selected=DIR --screenshot=F.png --quit
                    (add -platform offscreen when there is no display).
  test             Build, then run every automated test that needs no game files: the driver
                    (driver/tests/run_all.sh) and the interface (ctest in app/).
  test-driver      Only the driver tests.
  test-ui          Only the interface tests (unit tests and smoke tests that start the application offscreen).
  e2e FILE...      End to end: the interface extracts each archive (offscreen) and the MD5 of every
                    file is compared with the driver listing.
  sweep DIR...     False-positive sweep of the driver on the files under DIR.
  bench FILE       Time the opening and indexing of an archive (no window).
  list FILE        List an archive with the command line tool (name, offset, size, MD5).
  extract FILE DIR Extract every entry of an archive into DIR with the command line tool.
  configure        Check the prerequisites.
  clean            Remove build outputs.
  purge            Same as clean, plus install/, dist/ and Python caches.
  declutter        Remove the Python caches.
  install          Build and install under the prefix (bin/dragonunpacker, lib/dragonunpacker/plugins/,
                    desktop file and icons in share/).
  package          Build and make a tar.gz of the installation (dist/).

Options:
  -p, --prefix DIR   Install prefix (default: ./install).
  -j, --jobs N       Parallel compiler processes (default: all cores, currently $JOBS).
  -B, --rebuild-all  Recompile everything.
  -v, --verbose      Show the compiler output instead of the last lines.
  -h, --help         Show this help.

Examples:
  ./make.sh build
  ./make.sh run -- --open=/tmp/pak0.pak
  ./make.sh test
  ./make.sh list ~/games/Quake/id1/pak0.pak
  ./make.sh install -p /opt/dragonunpacker
EOT
}

die() {
    echo "make.sh: error: $*" >&2
    exit 1
}

need_prog() {
    command -v "$1" >/dev/null 2>&1 || die "'$1' not found on PATH (see README.md, section Building)"
}

# the tools are quiet when they work; show everything with -v, otherwise only on failure
run_quiet() {
    local log rc=0
    if [ "$VERBOSE" -eq 1 ]; then
        "$@"
        return
    fi
    log="$(mktemp)"
    "$@" >"$log" 2>&1 || rc=$?
    if [ "$rc" -ne 0 ]; then
        cat "$log" >&2
        rm -f "$log"
        die "command failed ($rc): $*"
    fi
    grep -E "Error|Fatal|Linking|lines compiled" "$log" | tail -4 || true
    rm -f "$log"
}

do_configure() {
    need_prog python3
    need_prog cmake
    need_prog ninja
    command -v qmake6 >/dev/null 2>&1 || command -v qtpaths6 >/dev/null 2>&1 || echo "make.sh: note: Qt 6 development packages not found on PATH" >&2
    command -v gcc >/dev/null 2>&1 || echo "make.sh: note: gcc not found, the C ABI harness of the driver tests needs it" >&2
    command -v lzop >/dev/null 2>&1 || echo "make.sh: note: lzop not found, driver/tests/verify_lzo.py will fail" >&2
    echo "prerequisites checked: cmake $(cmake --version | head -1 | cut -d' ' -f3), g++ $(g++ -dumpversion 2>/dev/null)"
}

build_driver() {
    need_prog cmake
    need_prog ninja
    run_quiet cmake -S "$DRIVER_DIR/cpp" -B "$BUILD_DIR/cpp" -G Ninja -DCMAKE_BUILD_TYPE=Release
    local a=(--parallel "$JOBS")
    [ "$REBUILD" -eq 1 ] && a+=(--clean-first)
    run_quiet cmake --build "$BUILD_DIR/cpp" "${a[@]}"
}

build_cli() {
    build_driver
}

build_ui() {
    need_prog cmake
    need_prog ninja
    if [ ! -f "$APP_BUILD_DIR/build.ninja" ]; then
        run_quiet cmake -S "$APP_DIR" -B "$APP_BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release
    fi
    local a=(--parallel "$JOBS")
    [ "$REBUILD" -eq 1 ] && a+=(--clean-first)
    run_quiet cmake --build "$APP_BUILD_DIR" "${a[@]}"
}

do_build() {
    case "${1:-all}" in
        all)    build_driver; build_ui ;;
        driver) build_driver ;;
        ui)     build_ui ;;
        cli)    build_cli ;;
        *)      die "unknown build target: $1 (all, driver, ui, cli)" ;;
    esac
    echo "built: ${1:-all}"
}

do_run() {
    do_build all
    echo "starting the interface"
    DUP_PLUGIN="$PLUGIN" exec "$APP_BUILD_DIR/src/dragonunpacker" "$@"
}

do_test_driver() {
    build_driver
    "$DRIVER_DIR/tests/run_all.sh"
}

do_test_ui() {
    build_driver
    build_ui
    ctest --test-dir "$APP_BUILD_DIR" --output-on-failure
}

do_test() {
    do_test_driver
    ctest --test-dir "$APP_BUILD_DIR" --output-on-failure
}

do_e2e() {
    [ $# -gt 0 ] || die "e2e needs at least one archive"
    build_driver
    build_cli
    build_ui
    "$APP_DIR/tests/e2e.sh" "$@"
}

do_sweep() {
    [ $# -gt 0 ] || die "sweep needs at least one directory"
    build_driver
    build_cli
    python3 -I "$DRIVER_DIR/tests/sweep.py" "$PLUGIN" "$CLI" "$@"
}

do_bench() {
    [ $# -eq 1 ] || die "usage: ./make.sh bench FILE"
    build_driver
    build_ui
    DUP_PLUGIN="$PLUGIN" "$APP_BUILD_DIR/src/dragonunpacker" --benchmark="$1"
}

do_list() {
    [ $# -eq 1 ] || die "usage: ./make.sh list FILE"
    build_driver
    build_cli
    "$CLI" "$PLUGIN" "$1"
}

do_extract() {
    [ $# -eq 2 ] || die "usage: ./make.sh extract FILE DIR"
    build_driver
    build_cli
    mkdir -p "$2"
    "$CLI" "$PLUGIN" "$1" x "$2"
}

do_clean() {
    rm -rf "$BUILD_DIR" "$APP_BUILD_DIR"
    echo "removed the build outputs"
}

do_purge() {
    do_clean
    rm -rf "$INSTALL_PREFIX" "$SOURCE_DIR/dist"
    find "$DRIVER_DIR/tests" -name __pycache__ -type d -prune -exec rm -rf {} +
    echo "removed install/ and dist/"
}

do_declutter() {
    find "$DRIVER_DIR/tests" -name __pycache__ -type d -prune -exec rm -rf {} +
    echo "removed the Python caches"
}

do_install() {
    build_driver
    # configure again so that the install rules see the driver that was just built
    run_quiet cmake -S "$APP_DIR" -B "$APP_BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release
    build_ui
    cmake --install "$APP_BUILD_DIR" --prefix "$INSTALL_PREFIX" >/dev/null
    echo "installed in $INSTALL_PREFIX (run $INSTALL_PREFIX/bin/dragonunpacker)"
}

do_package() {
    local saved="$INSTALL_PREFIX" stage out
    stage="$(mktemp -d)/dragonunpacker"
    INSTALL_PREFIX="$stage"
    do_install
    INSTALL_PREFIX="$saved"
    mkdir -p "$SOURCE_DIR/dist"
    out="$SOURCE_DIR/dist/dragonunpacker-linux-$(uname -m).tar.gz"
    tar -C "$(dirname "$stage")" -czf "$out" dragonunpacker
    rm -rf "$(dirname "$stage")"
    echo "package: $out"
}

ACTION="${1:-}"
[ -n "$ACTION" ] || { usage; exit 1; }
shift

case "$ACTION" in
    -h|--help|help) usage; exit 0 ;;
esac

TARGET=""
if { [ "$ACTION" = "build" ] || [ "$ACTION" = "rebuild" ]; } && [ $# -gt 0 ] && [[ "${1:-}" != -* ]]; then
    TARGET="$1"
    shift
fi

POSITIONAL=()
PASS=()
while [ $# -gt 0 ]; do
    case "$1" in
        --)
            shift
            PASS=("$@")
            break
            ;;
        -p|--prefix)
            [ $# -ge 2 ] || die "$1 requires an argument"
            INSTALL_PREFIX="$(cd "$(dirname "$2")" 2>/dev/null && pwd)/$(basename "$2")" || INSTALL_PREFIX="$2"
            shift 2
            ;;
        -j|--jobs)
            [ $# -ge 2 ] || die "$1 requires an argument"
            JOBS="$2"
            shift 2
            ;;
        -B|--rebuild-all) REBUILD=1; shift ;;
        -v|--verbose) VERBOSE=1; shift ;;
        -h|--help) usage; exit 0 ;;
        -*)
            # for "run", anything that looks like an option belongs to the application
            if [ "$ACTION" = "run" ]; then
                PASS+=("$1")
                shift
            else
                die "unknown option: $1"
            fi
            ;;
        *) POSITIONAL+=("$1"); shift ;;
    esac
done

case "$ACTION" in
    configure)   do_configure ;;
    build)       do_build "$TARGET" ;;
    rebuild)     REBUILD=1; do_build "$TARGET" ;;
    run)         do_run "${POSITIONAL[@]+"${POSITIONAL[@]}"}" "${PASS[@]+"${PASS[@]}"}" ;;
    test)        do_test ;;
    test-driver) do_test_driver ;;
    test-ui)     do_test_ui ;;
    e2e)         do_e2e "${POSITIONAL[@]+"${POSITIONAL[@]}"}" ;;
    sweep)       do_sweep "${POSITIONAL[@]+"${POSITIONAL[@]}"}" ;;
    bench)       do_bench "${POSITIONAL[@]+"${POSITIONAL[@]}"}" ;;
    list)        do_list "${POSITIONAL[@]+"${POSITIONAL[@]}"}" ;;
    extract)     do_extract "${POSITIONAL[@]+"${POSITIONAL[@]}"}" ;;
    clean)       do_clean ;;
    purge)       do_purge ;;
    declutter)   do_declutter ;;
    install)     do_install ;;
    package)     do_package ;;
    *)           die "unknown action: $ACTION (see ./make.sh --help)" ;;
esac
