# One powered wheel on deformable soil

This is a small C++ starter project for Ubuntu/Linux and Project Chrono 10.0.0.
Run it to see a wheel drive across flat soil and leave a rut. The animation
pauses after ten simulated seconds so you can inspect the ground.

The wheel is a rigid cylinder. Its axle is guided straight, but its forward
position and vertical position remain free. A motor prescribes axle rotation;
traction from the soil determines forward motion. Gravity supplies the wheel
load. The guide prevents lateral movement and tipping.

| Setting | Initial value |
| --- | --- |
| Wheel radius | 0.35 m |
| Wheel width | 0.20 m |
| Wheel mass | 100 kg, giving a gravitational load of about 981 N |
| Axle angular speed | 0.8 rad/s |
| Visible soil patch | 6 m long by 1.5 m wide |
| Soil grid spacing | 0.02 m |
| Physics time step | 0.001 s |
| Simulation duration | 10 s |

The soil parameters are demonstration values from Chrono's official rigid-wheel
example. They are not calibrated agricultural-soil properties. Bulldozing is
disabled for this first version: the ground deforms downward without modeling
soil piles beside the rut. This project contains no sensors or estimator.

## Install the dependencies on Ubuntu

Skip packages you already have. Run these commands in a terminal on your machine:

```bash
sudo apt update
sudo apt install build-essential cmake git pkg-config libeigen3-dev \
    libirrlicht-dev libxxf86vm-dev freeglut3-dev libgl1-mesa-dev
```

## Build the needed Chrono libraries

Run these commands from this repository's root directory. Chrono's sources and
libraries stay in the ignored `.chrono/` directory. You do not need CUDA or a
discrete GPU for the soil mechanics. The animation needs a working desktop
graphics environment.

```bash
mkdir -p .chrono
git clone --depth 1 --branch 10.0.0 https://github.com/projectchrono/chrono.git .chrono/chrono

cmake -S .chrono/chrono -B .chrono/chrono-build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCH_ENABLE_MODULE_VEHICLE=ON \
    -DCH_ENABLE_MODULE_IRRLICHT=ON \
    -DCH_ENABLE_MODULE_VEHICLE_COSIM=OFF \
    -DBUILD_DEMOS=OFF \
    -DBUILD_TESTING=OFF

cmake --build .chrono/chrono-build --target Chrono_vehicle Chrono_irrlicht -j 4
```

Keep `-j 4` for a modest memory footprint. This builds Chrono's core, vehicle
and display libraries. This project's CMake file also supports this partial
Chrono build; the unused predefined robot and vehicle model libraries are optional.
The supplied demos and tests are disabled.

If Chrono cannot find Irrlicht, add these arguments to its configure command:

```bash
-DIrrlicht_INCLUDE_DIR=/usr/include/irrlicht \
-DIrrlicht_LIBRARY=/usr/lib/x86_64-linux-gnu/libIrrlicht.so
```

Those paths apply to a typical Ubuntu x86_64 installation. If your CPU is ARM,
use the library path returned by `dpkg -L libirrlicht-dev`.

## Build and run this simulation

The simulation's CMake project and `main.cpp` are in `src/`. From the repository
root, configure and build it against the local Chrono build:

```bash
cmake -S src -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DChrono_DIR="$PWD/.chrono/chrono-build/cmake"

cmake --build build -j 4
./build/single_wheel
```

The soil is coloured by its downward deformation, from 0 to 0.25 m. You should
see the wheel sink into the ground, move along X, and leave a depressed strip
behind it. After ten simulated seconds the physics stops and the window stays
open. Close the window to exit.

The program also prints forward travel and the largest rut depth sampled along
the centreline behind the wheel. It writes the deformed ground mesh to
`output/soil_after.obj`, relative to the directory from which you run it.

To run the same physics without opening a window:

```bash
./build/single_wheel --headless
```

If you want to build entirely without the visualization module, configure
this project with `-DWHEEL_GUI=OFF`. Only Chrono's Vehicle module is then required.

## Where to change the first simulation

Open `src/main.cpp`. The wheel size, mass, speed, duration and grid spacing are
together near the beginning. The uniform soil parameters appear in the
`terrain.SetSoilParameters(...)` call below the motor setup.

Keep the initial values for your first run. Success means the wheel moves
forward and the passed ground remains below its original height. The forward
speed is a simulation result; it is not forced to equal radius times axle speed.

A ten-second headless run with Chrono 10.0.0 on Ubuntu 24.04 (GCC 13.3) completed
successfully: the wheel travelled 2.6255 m, and the largest sampled rut depth
behind it was 0.1843 m. The desktop animation requires a graphics display.

## Sources

- [Chrono 10.0.0 rigid-wheel SCM example](https://github.com/projectchrono/chrono/blob/10.0.0/src/demos/vehicle/terrain/demo_VEH_SCMTerrain_RigidTire.cpp)
- [Chrono 10.0.0 SCM terrain API](https://api.projectchrono.org/10.0.0/classchrono_1_1vehicle_1_1_s_c_m_terrain.html)
- [Chrono external-project template](https://github.com/projectchrono/chrono/blob/10.0.0/template_project/CMakeLists.txt)
- [Chrono installation guide](https://api.projectchrono.org/10.0.0/tutorial_install_chrono.html)
