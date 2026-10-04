// Minimal powered rigid wheel on uniform SCM soil, for Project Chrono 10.0.0.
// Adapted from Chrono's BSD-licensed rigid-wheel SCM example; see CHRONO_LICENSE.txt.
// Coordinates: X forward, Y along the axle, Z up. All quantities use SI units.
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>

#include "chrono/core/ChDataPath.h"
#include "chrono/core/ChRealtimeStep.h"
#include "chrono/functions/ChFunctionRamp.h"
#include "chrono/physics/ChBodyEasy.h"
#include "chrono/physics/ChLinkMotorRotationAngle.h"
#include "chrono/physics/ChSystemSMC.h"
#include "chrono_vehicle/terrain/SCMTerrain.h"

#ifdef WHEEL_GUI
#include "chrono_irrlicht/ChVisualSystemIrrlicht.h"
#endif

using namespace chrono;

int main(int argc, char* argv[]) {
#ifdef CHRONO_DATA_DIR
    SetChronoDataPath(CHRONO_DATA_DIR);
#endif
    bool headless = false;
    if (argc == 2 && std::string(argv[1]) == "--headless") {
        headless = true;
    } else if (argc != 1) {
        std::cout << "Usage: single_wheel [--headless]\n";
        return 1;
    }
#ifndef WHEEL_GUI
    headless = true;
#endif

    // Change only these values when experimenting with the first simulation.
    const double radius = 0.35;       // m
    const double width = 0.20;        // m
    const double mass = 100.0;        // kg; gravity provides the normal load
    const double angular_speed = 0.8; // rad/s; translation is NOT prescribed
    const double step = 0.001;        // s
    const double duration = 10.0;     // s
    const double start_x = -2.0;      // m
    const double grid_spacing = 0.02; // m

    ChSystemSMC system;
    system.SetGravitationalAcceleration(ChVector3d(0, 0, -9.81));
    system.SetCollisionSystemType(ChCollisionSystem::Type::BULLET);
    system.SetNumThreads(2);

    // Invisible reference body for the motor. It has no collision geometry.
    auto reference = chrono_types::make_shared<ChBody>();
    reference->SetFixed(true);
    system.AddBody(reference);

    // A solid cylindrical wheel. Density gives the requested mass and inertia.
    const double density = mass / (CH_PI * radius * radius * width);
    auto material = chrono_types::make_shared<ChContactMaterialSMC>();
    auto wheel = chrono_types::make_shared<ChBodyEasyCylinder>(
        ChAxis::Y, radius, width, density, true, true, material);
    wheel->SetPos(ChVector3d(start_x, 0, radius + 0.01));
    wheel->SetAngVelParent(ChVector3d(0, angular_speed, 0));
    wheel->GetVisualShape(0)->SetColor(ChColor(0.25f, 0.25f, 0.25f));
    system.AddBody(wheel);

    // As in Chrono's single-wheel example, OLDHAM lets the wheel translate in
    // the plane perpendicular to the axle: X travel and Z sinkage remain free.
    // Lateral motion and tipping are constrained. The motor only drives spin.
    auto motor = chrono_types::make_shared<ChLinkMotorRotationAngle>();
    motor->SetSpindleConstraint(ChLinkMotorRotation::SpindleConstraint::OLDHAM);
    motor->Initialize(wheel, reference,
                      ChFrame<>(wheel->GetPos(), QuatFromAngleX(-CH_PI_2)));
    motor->SetAngleFunction(chrono_types::make_shared<ChFunctionRamp>(0, angular_speed));
    system.AddLink(motor);

    // Flat, uniform soil. These are demonstration values from Chrono's demo,
    // not calibrated properties of a particular real soil.
    vehicle::SCMTerrain terrain(&system);
    terrain.SetSoilParameters(0.2e6, // Bekker Kphi
                              0.0,  // Bekker Kc
                              1.1,  // Bekker exponent n
                              0.0,  // cohesion, Pa
                              30.0, // friction angle, degrees
                              0.01, // Janosi shear displacement, m
                              4e7,  // elastic stiffness, Pa/m
                              3e4); // damping, Pa s/m
    terrain.EnableBulldozing(false);
    terrain.SetPlotType(vehicle::SCMTerrain::PLOT_SINKAGE, 0, 0.25);
    terrain.Initialize(6.0, 1.5, grid_spacing);
    terrain.SetMeshWireframe(false);

    std::cout << "Running one powered wheel on SCM soil.\n";

    // A snapshot at completion also allows inspection without a GUI.
    auto save_result = [&]() {
        std::filesystem::create_directories("output");
        terrain.WriteMesh("output/soil_after.obj");
        double max_rut = 0;
        const double passed_end = wheel->GetPos().x() - radius - 0.10;
        for (double x = start_x + radius; x < passed_end; x += grid_spacing) {
            const ChVector3d location(x, 0, 0);
            max_rut = std::max(max_rut,
                              terrain.GetInitHeight(location) - terrain.GetHeight(location));
        }
        std::cout << std::fixed << std::setprecision(4)
                  << "Time: " << system.GetChTime() << " s\n"
                  << "Forward travel: " << wheel->GetPos().x() - start_x << " m\n"
                  << "Wheel centre height: " << wheel->GetPos().z() << " m\n"
                  << "Largest rut depth along the passed centreline: " << max_rut << " m\n"
                  << "Saved output/soil_after.obj\n";
    };

    const int total_steps = static_cast<int>(std::lround(duration / step));
    if (headless) {
        for (int i = 0; i < total_steps; ++i)
            system.DoStepDynamics(step);
        save_result();
        return 0;
    }

#ifdef WHEEL_GUI
    auto visual = chrono_types::make_shared<irrlicht::ChVisualSystemIrrlicht>();
    visual->AttachSystem(&system);
    visual->SetWindowSize(1100, 700);
    visual->SetWindowTitle("One wheel on deformable soil");
    visual->SetCameraVertical(CameraVerticalDir::Z);
    visual->Initialize();
    visual->AddCamera(ChVector3d(1.0, -3.5, 2.2), ChVector3d(-0.7, 0, 0));
    visual->AddTypicalLights();

    ChRealtimeStepTimer timer;
    int completed_steps = 0;
    bool saved = false;
    while (visual->Run()) {
        // Render at 50 frames/s, with twenty 1-ms physics steps per frame.
        for (int i = 0; i < 20 && completed_steps < total_steps; ++i) {
            system.DoStepDynamics(step);
            ++completed_steps;
        }
        if (completed_steps == total_steps && !saved) {
            save_result();
            saved = true;
            std::cout << "Simulation complete. Inspect the rut; close the window to exit.\n";
        }
        visual->BeginScene();
        visual->Render();
        visual->EndScene();
        timer.Spin(0.02);
    }
    if (!saved)
        save_result();
#endif
    return 0;
}
