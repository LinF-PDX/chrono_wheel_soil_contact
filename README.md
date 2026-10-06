# One powered wheel on deformable soil

One C++ source and one shared `src/CMakeLists.txt` build on macOS and Linux with
**Project Chrono 10.0.0**. Windows is outside this project's scope. GUI builds
use Irrlicht; `WHEEL_GUI=OFF` builds require only Chrono core, Vehicle, and Eigen.
Only macOS GUI builds produce `single_wheel.app`.

The wheel is a rigid cylinder. A motor prescribes axle rotation and prevents
tipping; soil traction determines forward motion, and gravity supplies the
wheel load. Chrono 10.0.0's `OLDHAM` setting leaves all three translations free,
including lateral movement. The straight track relies on the symmetric setup,
rather than an explicit lateral guide. The physics and numerical parameters
are shared unchanged between platforms and between GUI and headless runs.

| Setting | Value |
| --- | --- |
| Wheel radius / width | 0.35 m / 0.20 m |
| Wheel mass | 100 kg (about 981 N gravitational load) |
| Axle angular speed | 0.8 rad/s |
| Soil patch | 6 m long by 1.5 m wide |
| Soil grid spacing | 0.02 m |
| Physics step / duration | 0.001 s / 10 s |

Soil parameters come from Chrono's rigid-wheel SCM example and are demonstration
values, not calibrated agricultural-soil properties. Bulldozing is disabled:
the soil deforms downward without modeling piles beside the rut. There are no
sensors or estimator.

## Dependency installation: macOS

On the M3 Mac, use a native arm64 terminal and native libraries. This requirement
belongs to this machine's setup; the application does not force arm64 on other
Macs or on Linux. Install Xcode or its Command Line Tools if needed, then check:

```bash
sw_vers
uname -m
sysctl -in sysctl.proc_translated  # 0, or unavailable on an Intel Mac
xcode-select -p
xcrun --sdk macosx --show-sdk-path
clang++ --version
```

With Homebrew installed, reuse working packages and install only missing ones:

```bash
export HOMEBREW_NO_AUTO_UPDATE=1 HOMEBREW_NO_INSTALL_CLEANUP=1
command -v cmake >/dev/null || brew install cmake
command -v git >/dev/null || brew install git
brew list --versions eigen >/dev/null 2>&1 || \
    brew list --versions eigen@3 >/dev/null 2>&1 || brew install eigen@3
# GUI only; omit for a fully headless setup:
brew list --versions irrlicht >/dev/null 2>&1 || brew install irrlicht
```

Homebrew may install or update dependencies required by a missing package.
Use `brew --prefix` to discover prefixes; do not assume a fixed Homebrew path.
Eigen is header-only, so it has no library architecture to check. After CMake
reports the actual Irrlicht and Chrono libraries, check them with `file` or
`lipo -archs`; on the M3 they must include arm64.

## Dependency installation: Ubuntu 24.04 Linux

Ubuntu 24.04 is the distribution recorded by this repository's previous Linux
run. If using another distribution, install its equivalents. These commands
use the package manager's native architecture; there is no macOS architecture
flag and no hardcoded multiarch library directory.

```bash
sudo apt update
sudo apt install --no-upgrade build-essential cmake git libeigen3-dev
# GUI only; omit for a fully headless setup:
sudo apt install --no-upgrade libirrlicht-dev libxxf86vm-dev freeglut3-dev libgl1-mesa-dev
```

A GUI run needs a desktop graphics session. A headless build or `--headless` run
does not need a display. CUDA, MPI, Python bindings, and OpenMP are not needed.
Python 3 is optional for the result checker below.

## Repeatable dependency and application build

From the repository root, after installing prerequisites:

```bash
./scripts/build.sh ON    # Chrono Vehicle + Irrlicht, then the GUI application
# Or, with no Irrlicht dependency at all:
./scripts/build.sh OFF   # Chrono Vehicle, then the headless application
```

The script clones the official tag **10.0.0**, verifies the exact commit
`9faf13dd8f1128dd75ed233a9627027b0422c3f7`, and refuses a different or modified
checkout. It reuses existing sources, CMake caches, and successful build outputs.
It does not install packages or change global shell configuration.

Defaults:

| Artifact | macOS | Linux |
| --- | --- | --- |
| Chrono source | `~/chrono/10.0.0/source` | same |
| Chrono GUI dependency build | `~/chrono/10.0.0/build-mac` | `~/chrono/10.0.0/build-linux` |
| Application GUI build | `build-mac` | `build-linux` |
| Headless builds | append `-headless` to build directories | same |
| Logs | `.logs/build-*.log` | same |

Optional environment variables: `CHRONO_HOME` changes the dependency root;
`CHRONO_BUILD` and `WHEEL_BUILD` change individual build directories; `JOBS`
changes parallelism (default **2**). Use separate dependency directories for GUI
and headless configurations so reconfiguration does not invalidate another
application's package. On this 16 GB Mac, start with two jobs.

Chrono remains outside this repository. Retain its source and build data
folders. Local paths live in ignored CMake caches and generated headers, not in
tracked configuration. `.gitignore` also excludes builds, bundles, editor
settings, local presets, logs, and simulation output. Share the source, never
copy a build directory from one machine to the other.

## Explicit Chrono configuration

These are the commands underlying the script, if you prefer manual setup.
Clone only if the source is absent; never silently replace another checkout:

```bash
CHRONO_HOME="$HOME/chrono/10.0.0"
git clone --depth 1 --branch 10.0.0 https://github.com/projectchrono/chrono.git "$CHRONO_HOME/source"
git -C "$CHRONO_HOME/source" describe --tags --exact-match
git -C "$CHRONO_HOME/source" rev-parse HEAD
```

The outputs must be `10.0.0` and the pinned commit above.

On the **M3 Mac**, configure using Apple Clang and the selected Xcode SDK:

```bash
# Use eigen@3 here if that is the compatible package you already have.
EIGEN_PREFIX=$(brew --prefix eigen)
IRRLICHT_PREFIX=$(brew --prefix irrlicht)
MAC_SDK=$(xcrun --sdk macosx --show-sdk-path)
cmake -S "$CHRONO_HOME/source" -B "$CHRONO_HOME/build-mac" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_OSX_SYSROOT="$MAC_SDK" \
    -DEigen3_DIR="$EIGEN_PREFIX/share/eigen3/cmake" -DIrrlicht_ROOT="$IRRLICHT_PREFIX" \
    -DCH_ENABLE_MODULE_VEHICLE=ON -DCH_ENABLE_MODULE_IRRLICHT=ON \
    -DCH_ENABLE_MODULE_VEHICLE_COSIM=OFF -DCH_ENABLE_OPENMP=OFF \
    -DBUILD_DEMOS=OFF -DBUILD_TESTING=OFF
cmake --build "$CHRONO_HOME/build-mac" --target Chrono_vehicle Chrono_irrlicht --parallel 2
```

On **Ubuntu**, let CMake discover the distribution's dependencies and use the
compiler's native architecture:

```bash
cmake -S "$CHRONO_HOME/source" -B "$CHRONO_HOME/build-linux" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCH_ENABLE_MODULE_VEHICLE=ON -DCH_ENABLE_MODULE_IRRLICHT=ON \
    -DCH_ENABLE_MODULE_VEHICLE_COSIM=OFF -DCH_ENABLE_OPENMP=OFF \
    -DBUILD_DEMOS=OFF -DBUILD_TESTING=OFF
cmake --build "$CHRONO_HOME/build-linux" --target Chrono_vehicle Chrono_irrlicht --parallel 2
```

For a headless-only dependency build on either platform, use a separate
`*-headless` directory, change `CH_ENABLE_MODULE_IRRLICHT` to `OFF`, omit
Irrlicht discovery arguments, and build only `Chrono_vehicle` (its core
dependency is built automatically).

Chrono supplies the `FindIrrlicht.cmake` module and the `Irrlicht::Irrlicht`
target; distribution Irrlicht packages need not ship their own CMake config.
The application uses `find_package(Chrono 10.0.0 EXACT CONFIG ...)`,
`find_package(Eigen3 CONFIG ...)`, and Chrono's Irrlicht discovery module.
It links exported targets, never guessed library filenames. For dependencies
outside standard locations, use `CMAKE_PREFIX_PATH`, `Eigen3_DIR`, or
`Irrlicht_ROOT` as local configure hints.

The minimal Chrono build leaves `ChronoModels_robot`, `ChronoModels_vehicle`,
and `Chrono_vehicle_irrlicht` unbuilt. Chrono 10.0.0's build-tree package reports
these unused libraries as missing. The application allows only those omissions
and still checks the required modules, dependencies, targets, and actual
exported library locations. Missing core, Vehicle, or requested Irrlicht
libraries remain errors. A partial installed SDK may fail upstream's export
checks; use the documented build-tree package for a minimal build.

## Subsequent application builds and runs

Run launch commands from the repository root so output goes to
`output/soil_after.obj`. The following commands reuse the existing Chrono build.

**M3 Mac GUI:**

```bash
cmake -S src -B build-mac -DCMAKE_BUILD_TYPE=Release -DWHEEL_GUI=ON \
    -DChrono_DIR="$HOME/chrono/10.0.0/build-mac/cmake" \
    -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_SYSROOT="$(xcrun --sdk macosx --show-sdk-path)"
cmake --build build-mac --parallel 2
./build-mac/single_wheel.app/Contents/MacOS/single_wheel
# Same binary, same physics, without opening a window:
./build-mac/single_wheel.app/Contents/MacOS/single_wheel --headless
```

The bundle executable can also be launched through the desktop:

```bash
open -n -W build-mac/single_wheel.app --args
```

`open`/Finder may use a different working directory. Use the direct executable
command above for a predictable output location and terminal diagnostics.
The bundle is for local launching: it references the local Chrono libraries and
data, and is not a standalone distributable application.

**Ubuntu GUI:**

```bash
cmake -S src -B build-linux -DCMAKE_BUILD_TYPE=Release -DWHEEL_GUI=ON \
    -DChrono_DIR="$HOME/chrono/10.0.0/build-linux/cmake"
cmake --build build-linux --parallel 2
./build-linux/single_wheel
./build-linux/single_wheel --headless
```

**Headless-only application:** use the same CMake file, with `WHEEL_GUI=OFF`.
A GUI-enabled Chrono SDK can also supply this build without linking Irrlicht.
For example, on the M3 Mac with the already-built SDK:

```bash
cmake -S src -B build-mac-headless -DCMAKE_BUILD_TYPE=Release -DWHEEL_GUI=OFF \
    -DChrono_DIR="$HOME/chrono/10.0.0/build-mac/cmake" \
    -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_SYSROOT="$(xcrun --sdk macosx --show-sdk-path)"
cmake --build build-mac-headless --parallel 2
mkdir -p .logs
./build-mac-headless/single_wheel --headless | tee .logs/headless-mac.log
python3 scripts/verify_result.py .logs/headless-mac.log output/soil_after.obj
```

On Ubuntu with the existing Linux SDK:

```bash
cmake -S src -B build-linux-headless -DCMAKE_BUILD_TYPE=Release -DWHEEL_GUI=OFF \
    -DChrono_DIR="$HOME/chrono/10.0.0/build-linux/cmake"
cmake --build build-linux-headless --parallel 2
mkdir -p .logs
./build-linux-headless/single_wheel --headless | tee .logs/headless-linux.log
python3 scripts/verify_result.py .logs/headless-linux.log output/soil_after.obj
```

`./scripts/build.sh OFF` builds an independent headless Chrono SDK as well.
When using that SDK, set `Chrono_DIR` to its `build-*-headless/cmake` directory.
A headless-only executable runs headlessly even without `--headless`.

The checker verifies finite reported values, ten simulated seconds, forward
travel, valid mesh vertices/faces, an extended depressed centreline behind the
wheel, and flat soil beside the track. Historical travel/rut values are
comparisons, not exact cross-platform assertions.

## Visualization assets and output

CMake resolves the absolute `CHRONO_DATA_DIR` reported by the package. Both
builds check `colormaps/jet-table-float-0512.csv`: SCM loads it to color its mesh
even in headless mode. GUI builds additionally check
`fonts/jetbrainmono6_bold.png`, `fonts/arial8.xml`, and the XML's
`fonts/arial80.bmp` texture. Override with
`-DWHEEL_CHRONO_DATA_DIR=/your/retained/chrono/data` if needed. The generated path
is local to the build. The application also checks the colormap at startup and
fonts before opening a GUI, and gives Chrono a path with its required trailing
separator. Assets do not depend on the launch working directory. Headless
builds need the colormap but do not need fonts, Irrlicht, or a display.

The soil is colored by downward deformation, from 0 to 0.25 m. After ten
simulated seconds physics stops, the deformed mesh is saved, and the GUI stays
open for inspection. Close the window to exit. Output paths use
`std::filesystem`; the OBJ location is relative to the launch directory.

## Verification record

Available machine on **2026-10-05**: MacBook Air M3, macOS 27.0.1 (26A434),
native arm64 terminal (Rosetta translation flag 0), 16 GB RAM, about 188 GiB
free disk space before setup. Xcode 16.1 / Apple Clang 16.0.0, selected SDK
macOS 15.1; a C++17 `std::filesystem` probe compiled and ran as arm64. Explicit
`xcrun --sdk macosx` resolves the selected Xcode SDK; bare SDK discovery initially
returned an inconsistent Command Line Tools path. No global Xcode selection
was changed.

Already installed: Git 2.53.0, CMake 4.3.0, Homebrew 5.1.0, Eigen 5.0.1. Installed:
Irrlicht 1.8.5 (Homebrew revision 1), and Chrono 10.0.0 from the pinned checkout
above. Homebrew also upgraded Irrlicht's jpeg-turbo dependency to 3.2.0 and libpng
to 1.6.59. The initial `brew install irrlicht` also auto-updated Homebrew to
7.0.8, downloaded its portable Ruby runtime, and cleaned old caches. The
installation commands above disable auto-update and auto-cleanup for future
runs. Chrono configure/build logs from initial setup are under
`~/chrono/10.0.0/logs/`; later script logs are under `.logs/`.

Passed on this Mac:

- Built Chrono's core, Vehicle, and Irrlicht libraries; checked their actual
  architectures, Irrlicht's framework, and its JPEG/PNG dependencies as arm64.
  The reusable `./scripts/build.sh ON` reused the SDK successfully.
- Built a fresh macOS GUI bundle and a headless executable from the shared
  CMake file. The headless executable has no Irrlicht runtime linkage and
  produces no bundle. Both executables' headless runs completed successfully.
- With Irrlicht discovery explicitly disabled, another headless application
  configured, built, and ran using only the colormap in a path containing
  spaces, with no font files. This check used the existing Chrono SDK; it
  does not claim a separate Irrlicht-disabled Chrono SDK was compiled.
- All completed runs (including the desktop GUI) reported **10.0000 s**,
  **2.6278 m** forward travel, **0.1758 m** wheel centre height, and **0.1834 m**
  maximum sampled centreline rut depth. All values were finite.
- Inspected `output/soil_after.obj`: **23,177 vertices**, **45,600 faces**,
  **91 depressed passed-centreline vertices**, maximum passed-centreline mesh
  depression **0.183008 m**, and untouched flat soil beside the track.
  `scripts/verify_result.py` passed for each run.
- Observed the wheel and colored track in the desktop GUI after completion;
  left its window open. Irrlicht selected its OpenGL fallback. The captured
  `.logs/gui-mac.log` contains no missing-library or missing-asset error.
- Checked failure handling: unfinished required Chrono libraries were rejected,
  and a GUI configuration using the font-free data directory failed with an
  explicit missing-font error. Executable physics setup and the simulation loop
  remain unchanged from the original source; the `OLDHAM` comment was corrected
  to describe its free lateral translation accurately. `git diff --check` and the
  build script's Bash syntax check passed.

**Linux was not available in this session.** Ubuntu package discovery, native
Linux compilation of both GUI and headless builds, runtime library resolution,
headless numerical/mesh checks, and desktop rendering still require an Ubuntu
machine. Previous README evidence reports an Ubuntu 24.04 / GCC 13.3 headless
run with 2.6255 m travel and 0.1843 m sampled rut depth; the supplied setup brief
also mentions roughly 2.63 m and 0.177 m. Neither is new Linux verification of
these changes, and neither is used as an exact pass/fail threshold.

## Sources

- [Chrono 10.0.0 source release](https://github.com/projectchrono/chrono/tree/10.0.0)
- [Chrono 10.0.0 rigid-wheel SCM example](https://github.com/projectchrono/chrono/blob/10.0.0/src/demos/vehicle/terrain/demo_VEH_SCMTerrain_RigidTire.cpp)
- [Chrono 10.0.0 SCM terrain API](https://api.projectchrono.org/10.0.0/classchrono_1_1vehicle_1_1_s_c_m_terrain.html)
- [Chrono package configuration](https://github.com/projectchrono/chrono/blob/10.0.0/cmake/ChronoConfig.cmake.in)
- [Chrono Irrlicht discovery module](https://github.com/projectchrono/chrono/blob/10.0.0/cmake/FindIrrlicht.cmake)
- [Chrono installation guide](https://api.projectchrono.org/10.0.0/tutorial_install_chrono.html)
