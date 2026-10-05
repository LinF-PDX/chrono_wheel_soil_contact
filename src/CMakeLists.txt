cmake_minimum_required(VERSION 3.18)
project(chrono_single_wheel LANGUAGES CXX)

option(WHEEL_GUI "Show the wheel and rut using Irrlicht" ON)

# All machines use the same release and exported targets. Architecture and
# dependency prefixes belong in configure commands, not in this project.
if(WHEEL_GUI)
    find_package(Chrono 10.0.0 EXACT CONFIG QUIET COMPONENTS Vehicle Irrlicht)
else()
    find_package(Chrono 10.0.0 EXACT CONFIG QUIET COMPONENTS Vehicle)
endif()

# Chrono 10.0.0's build-tree package checks unused models and vehicle-specific
# visualization, even though this program uses only SCM and a generic visual
# system. Accept ONLY those omissions, after checking dependencies and modules.
if(NOT Chrono_FOUND)
    set(missing_required ${CHRONO_MISSING_LIBS_NDEBUG})
    list(REMOVE_ITEM missing_required ChronoModels_robot ChronoModels_vehicle Chrono_vehicle_irrlicht)
    if(NOT CHRONO_VERSION VERSION_EQUAL "10.0.0" OR
       NOT CHRONO_MISSING_LIBS_NDEBUG OR missing_required OR
       NOT CHRONO_VEHICLE_FOUND OR NOT EIGEN3_FOUND)
        message(FATAL_ERROR "A usable Chrono 10.0.0 package with Vehicle is required. Set Chrono_DIR; missing libraries: ${CHRONO_MISSING_LIBS_NDEBUG}")
    endif()
    if(WHEEL_GUI AND (NOT CHRONO_IRRLICHT_FOUND OR NOT Irrlicht_FOUND))
        message(FATAL_ERROR "WHEEL_GUI requires Chrono's Irrlicht module and Irrlicht dependency.")
    endif()
    message(STATUS "Accepting only unused Chrono 10.0.0 libraries: ${CHRONO_MISSING_LIBS_NDEBUG}")
endif()

# Use Chrono's shipped find module for Irrlicht (it supplies Irrlicht::Irrlicht).
# Eigen packages from both the 3.x and 5.x series are supported by Chrono 10.
find_package(Eigen3 CONFIG REQUIRED)
if(WHEEL_GUI)
    find_package(Irrlicht MODULE REQUIRED)
    if(NOT Irrlicht_FOUND OR NOT TARGET Irrlicht::Irrlicht)
        message(FATAL_ERROR "Irrlicht was not found. Set Irrlicht_ROOT to its installation prefix.")
    endif()
endif()

# Imported targets may exist before their libraries have been built. Check
# their exported locations without constructing platform-specific filenames.
set(required_targets Chrono::Chrono_core Chrono::Chrono_vehicle)
if(WHEEL_GUI)
    list(APPEND required_targets Chrono::Chrono_irrlicht Irrlicht::Irrlicht)
endif()
foreach(library IN LISTS required_targets)
    if(NOT TARGET ${library})
        message(FATAL_ERROR "Required dependency target is unavailable: ${library}")
    endif()
    get_target_property(configurations ${library} IMPORTED_CONFIGURATIONS)
    get_target_property(location ${library} IMPORTED_LOCATION)
    if(NOT EXISTS "${location}")
        set(location "")
        foreach(configuration IN LISTS configurations)
            get_target_property(candidate ${library} IMPORTED_LOCATION_${configuration})
            if(EXISTS "${candidate}")
                set(location "${candidate}")
                break()
            endif()
        endforeach()
        if(NOT location)
            message(FATAL_ERROR "Build or install the required dependency: ${library}")
        endif()
    endif()
endforeach()

add_executable(single_wheel main.cpp)
target_compile_features(single_wheel PRIVATE cxx_std_17)
set_target_properties(single_wheel PROPERTIES CXX_EXTENSIONS OFF)
# No vehicle model or vehicle-specific visualization is needed for one body.
target_link_libraries(single_wheel PRIVATE Chrono::Chrono_vehicle Eigen3::Eigen)

# SCM colors its exported mesh even without a GUI, so both modes need the
# default colormap. Irrlicht additionally loads the fonts listed below.
set(WHEEL_CHRONO_DATA_DIR "${CHRONO_DATA_DIR}" CACHE PATH "Chrono 10.0.0 data directory")
if(NOT WHEEL_CHRONO_DATA_DIR)
    message(FATAL_ERROR "Set WHEEL_CHRONO_DATA_DIR to the retained Chrono data directory.")
endif()
get_filename_component(WHEEL_CHRONO_DATA_DIR "${WHEEL_CHRONO_DATA_DIR}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
file(TO_CMAKE_PATH "${WHEEL_CHRONO_DATA_DIR}" WHEEL_CHRONO_DATA_DIR)
set(required_assets colormaps/jet-table-float-0512.csv)
if(WHEEL_GUI)
    list(APPEND required_assets fonts/jetbrainmono6_bold.png fonts/arial8.xml fonts/arial80.bmp)
endif()
foreach(asset IN LISTS required_assets)
    if(NOT EXISTS "${WHEEL_CHRONO_DATA_DIR}/${asset}")
        message(FATAL_ERROR "Missing Chrono asset: ${WHEEL_CHRONO_DATA_DIR}/${asset}. Set WHEEL_CHRONO_DATA_DIR.")
    endif()
endforeach()
configure_file(wheel_paths.h.in generated/wheel_paths.h @ONLY)
target_include_directories(single_wheel PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
message(STATUS "Wheel Chrono data: ${WHEEL_CHRONO_DATA_DIR}")

if(WHEEL_GUI)
    target_link_libraries(single_wheel PRIVATE Chrono::Chrono_irrlicht Irrlicht::Irrlicht)
    target_compile_definitions(single_wheel PRIVATE WHEEL_GUI)
    if(APPLE)
        set_target_properties(single_wheel PROPERTIES MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_GUI_IDENTIFIER "org.projectchrono.single-wheel"
            MACOSX_BUNDLE_BUNDLE_NAME "Single Wheel")
    endif()
endif()
