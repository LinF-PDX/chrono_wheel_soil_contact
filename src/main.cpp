// Minimal powered rigid wheel on uniform SCM soil, for Project Chrono 10.0.0.
// Adapted from Chrono's BSD-licensed rigid-wheel SCM example; see CHRONO_LICENSE.txt.
// Coordinates: X forward, Y along the axle, Z up. All quantities use SI units.
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "chrono/core/ChDataPath.h"
#include "chrono/core/ChRealtimeStep.h"
#include "chrono/functions/ChFunctionRamp.h"
#include "chrono/physics/ChBodyEasy.h"
#include "chrono/physics/ChLinkMotorRotationAngle.h"
#include "chrono/physics/ChSystemSMC.h"
#include "chrono_vehicle/terrain/SCMTerrain.h"
#include "wheel_paths.h"

#ifdef WHEEL_GUI
#include "chrono_irrlicht/ChVisualSystemIrrlicht.h"
#endif

using namespace chrono;

int main(int argc, char* argv[]) {
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
    const std::filesystem::path data_dir(WHEEL_CHRONO_DATA_DIR);
    const auto colormap = data_dir / "colormaps" / "jet-table-float-0512.csv";
    if (!std::filesystem::is_regular_file(colormap)) {
        std::cerr << "Missing Chrono colormap: " << colormap << '\n';
        return 2;
    }
    // Chrono's GetChronoDataFile concatenates strings, so retain the final slash.
    SetChronoDataPath(data_dir.generic_string() + "/");
#ifdef WHEEL_GUI
    if (!headless) {
        for (const auto* asset : {"fonts/jetbrainmono6_bold.png", "fonts/arial8.xml", "fonts/arial80.bmp"}) {
            if (!std::filesystem::is_regular_file(data_dir / asset)) {
                std::cerr << "Missing Chrono visualization asset: " << (data_dir / asset) << '\n';
                return 2;
            }
        }
    }
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

    // Chrono 10.0.0's OLDHAM leaves all translations free, including lateral Y.
    // It prevents tipping; the angle motor prescribes spin about the Y axle.
    // Straight travel here relies on the symmetric geometry and soil setup.
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

    // Sample only passed ground, leaving the wheel's current contact area out.
    auto max_passed_rut = [&]() {
        double max_rut = 0;
        const double passed_end = wheel->GetPos().x() - radius - 0.10;
        for (double x = start_x + radius; x < passed_end; x += grid_spacing) {
            const ChVector3d location(x, 0, 0);
            max_rut = std::max(max_rut,
                              terrain.GetInitHeight(location) - terrain.GetHeight(location));
        }
        return max_rut;
    };

    // A snapshot at completion also allows inspection without a GUI.
    // Irrlicht may change the working directory; retain the launch location.
    const auto output_dir = std::filesystem::current_path() / "output";
    const auto mesh_path = output_dir / "soil_after.obj";
    auto save_result = [&]() {
        std::filesystem::create_directories(output_dir);
        terrain.WriteMesh(mesh_path.string());
        std::cout << std::fixed << std::setprecision(4)
                  << "Time: " << system.GetChTime() << " s\n"
                  << "Forward travel: " << wheel->GetPos().x() - start_x << " m\n"
                  << "Wheel centre height: " << wheel->GetPos().z() << " m\n"
                  << "Largest rut depth along the passed centreline: " << max_passed_rut() << " m\n"
                  << "Saved " << mesh_path.generic_string() << '\n';
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
    if (!visual->GetDevice()) {
        std::cerr << "Cannot initialize visualization. Use --headless without a graphics display.\n";
        return 2;
    }
    visual->AddCamera(ChVector3d(1.0, -3.5, 2.2), ChVector3d(-0.7, 0, 0));
    visual->AddTypicalLights();

    // Irrlicht owns these elements and renders them with the existing GUI.
    auto* gui = visual->GetGUIEnvironment();
    auto* status_panel = gui->addStaticText(L"", irr::core::rect<irr::s32>(16, 16, 436, 340),
                                          false, false, nullptr, -1, true);
    status_panel->setBackgroundColor(irr::video::SColor(230, 20, 25, 32));
    auto* status_text = gui->addStaticText(L"", irr::core::rect<irr::s32>(12, 12, 408, 312),
                                         false, false, status_panel);
    status_text->setOverrideFont(gui->getFont(GetChronoDataFile("fonts/arial8.xml").c_str()));
    status_text->setOverrideColor(irr::video::SColor(255, 245, 245, 245));

    ChRealtimeStepTimer timer;
    int completed_steps = 0;
    bool saved = false;
    auto refresh_status = [&]() {
        const auto& position = wheel->GetPos();
        const double speed = wheel->GetPosDt().x();
        const double omega = wheel->GetAngVelParent().y();
        const double rolling_speed = radius * omega;
        const double sinkage = std::max(0.0, terrain.GetInitHeight(position) + radius - position.z());
        ChVector3d soil_force(0, 0, 0);
        ChVector3d soil_torque(0, 0, 0);
        const bool in_contact = terrain.GetContactForceBody(wheel, soil_force, soil_torque);

        std::wostringstream text;
        text << L"Wheel / SCM - "
             << (completed_steps == total_steps ? L"COMPLETE (physics paused)" : L"RUNNING") << L'\n'
             << std::fixed << std::setprecision(2)
             << L"Time: " << system.GetChTime() << L" / " << duration << L" s\n"
             << std::setprecision(3)
             << L"Travel X: " << position.x() - start_x << L" m\n"
             << L"Position Y / Z: " << position.y() << L" / " << position.z() << L" m\n"
             << L"Forward speed: " << speed << L" m/s\n"
             << L"Angular speed: " << omega << L" rad/s\n"
             << L"Nominal slip: ";
        if (in_contact && std::abs(rolling_speed) > 1e-6)
            text << std::setprecision(1) << 100.0 * (1.0 - speed / rolling_speed) << L" %\n";
        else
            text << L"N/A (no contact or rotation)\n";
        text << std::setprecision(3)
             << L"Wheel sinkage: " << sinkage << L" m\n"
             << L"Passed centreline rut max: " << max_passed_rut() << L" m\n"
             << std::setprecision(2)
             << L"SCM force X: " << soil_force.x() << L" N\n"
             << L"SCM force Y: " << soil_force.y() << L" N\n"
             << L"SCM force Z: " << soil_force.z() << L" N\n"
             << L"Motor torque (signed): " << motor->GetMotorTorque() << L" N m\n\n"
             << L"Radius / width: " << radius << L" / " << width << L" m\n"
             << L"Mass: " << mass << L" kg\n"
             << std::setprecision(3)
             << L"Time step / grid: " << step << L" s / " << grid_spacing << L" m";
        status_text->setText(text.str().c_str());
    };
    refresh_status();
    const int text_height = status_text->getTextHeight();
    status_text->setRelativePosition(irr::core::rect<irr::s32>(12, 12, 408, text_height + 12));
    status_panel->setRelativePosition(irr::core::rect<irr::s32>(16, 16, 436, text_height + 40));
    int last_status_step = 0;
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
        // Read-only display at 10 Hz of simulation time, plus the final state.
        if (completed_steps - last_status_step >= 100 ||
            (completed_steps == total_steps && last_status_step != completed_steps)) {
            refresh_status();
            last_status_step = completed_steps;
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
