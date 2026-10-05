#!/usr/bin/env bash
# Install prerequisites as documented in README.md, then run from any directory.
set -euo pipefail

repo=$(cd "$(dirname "$0")/.." && pwd)
gui=${1:-ON}
case "$gui" in ON) suffix= ;; OFF) suffix=-headless ;; *) echo "Usage: $0 [ON|OFF]" >&2; exit 1 ;; esac

platform_args=()
discovery_args=()
case "$(uname -s)" in
    Darwin)
        platform=mac
        if [[ $(sysctl -in sysctl.proc_translated 2>/dev/null || true) == 1 ]]; then
            echo "Run from a native terminal, rather than Rosetta." >&2
            exit 1
        fi
        # Native on Macs (arm64 on the user's M3), with the selected Xcode SDK.
        platform_args=(-DCMAKE_OSX_ARCHITECTURES="$(uname -m)"
                       -DCMAKE_OSX_SYSROOT="$(xcrun --sdk macosx --show-sdk-path)"
                       -DCMAKE_CXX_COMPILER=clang++)
        for formula in eigen eigen@3; do
            eigen_prefix=$(brew --prefix "$formula" 2>/dev/null || true)
            if [[ -d "$eigen_prefix/share/eigen3/cmake" ]]; then
                discovery_args+=(-DEigen3_DIR="$eigen_prefix/share/eigen3/cmake")
                break
            fi
        done
        if [[ "$gui" == ON ]]; then
            discovery_args+=(-DIrrlicht_ROOT="$(brew --prefix irrlicht)")
        fi
        ;;
    Linux) platform=linux ;; # The compiler selects this machine's native architecture.
    *) echo "This setup script supports macOS and Linux." >&2; exit 1 ;;
esac

chrono_home=${CHRONO_HOME:-"$HOME/chrono/10.0.0"}
chrono_source="$chrono_home/source"
chrono_build=${CHRONO_BUILD:-"$chrono_home/build-$platform$suffix"}
wheel_build=${WHEEL_BUILD:-"$repo/build-$platform$suffix"}
mkdir -p "$repo/.logs"
log="$repo/.logs/build-$platform$suffix-$(date +%Y%m%d-%H%M%S)-$$.log"
run() { "$@" 2>&1 | tee -a "$log"; }

if [[ ! -d "$chrono_source" ]]; then
    run git clone --depth 1 --branch 10.0.0 https://github.com/projectchrono/chrono.git "$chrono_source"
fi
revision=9faf13dd8f1128dd75ed233a9627027b0422c3f7
if [[ $(git -C "$chrono_source" rev-parse HEAD) != "$revision" ]] ||
   ! git -C "$chrono_source" diff --quiet ||
   ! git -C "$chrono_source" diff --cached --quiet; then
    echo "Expected an unmodified Chrono 10.0.0 checkout ($revision) at $chrono_source." >&2
    exit 1
fi

run cmake -S "$chrono_source" -B "$chrono_build" \
    -DCMAKE_BUILD_TYPE=Release "${platform_args[@]}" "${discovery_args[@]}" \
    -DCH_ENABLE_MODULE_VEHICLE=ON -DCH_ENABLE_MODULE_IRRLICHT="$gui" \
    -DCH_ENABLE_MODULE_VEHICLE_COSIM=OFF -DCH_ENABLE_OPENMP=OFF \
    -DBUILD_DEMOS=OFF -DBUILD_TESTING=OFF
targets=(Chrono_vehicle)
if [[ "$gui" == ON ]]; then targets+=(Chrono_irrlicht); fi
run cmake --build "$chrono_build" --target "${targets[@]}" --parallel "${JOBS:-2}"
run cmake -S "$repo/src" -B "$wheel_build" \
    -DCMAKE_BUILD_TYPE=Release -DWHEEL_GUI="$gui" -DChrono_DIR="$chrono_build/cmake" \
    "${platform_args[@]}" "${discovery_args[@]}"
run cmake --build "$wheel_build" --parallel "${JOBS:-2}"
echo "Built in $wheel_build. Log: $log"
