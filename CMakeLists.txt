cmake_minimum_required(VERSION 3.16)
project(FleetFlow VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_library(fleetflow_core
    src/core/Date.cpp
    src/core/DateRange.cpp
    src/models/Vehicle.cpp
    src/models/Car.cpp
    src/models/Bike.cpp
    src/models/Truck.cpp
    src/models/User.cpp
    src/models/Customer.cpp
    src/models/Employee.cpp
    src/models/Admin.cpp
    src/models/RentalRecord.cpp
    src/models/PaymentRecord.cpp
    src/models/MaintenanceRecord.cpp
)

target_include_directories(fleetflow_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(fleetflow_factories
    src/factories/VehicleFactory.cpp
    src/factories/UserFactory.cpp
)

target_link_libraries(fleetflow_factories PUBLIC fleetflow_core)

target_include_directories(fleetflow_factories PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(fleetflow_storage
    src/storage/CsvHelper.cpp
    src/storage/CsvSerializers.cpp
    src/storage/FileStorage.cpp
)

target_link_libraries(fleetflow_storage PUBLIC fleetflow_factories fleetflow_core)

target_include_directories(fleetflow_storage PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(fleetflow_services
    src/services/Validation.cpp
    src/services/FleetService.cpp
    src/services/RentalService.cpp
    src/services/PaymentService.cpp
    src/services/MaintenanceService.cpp
)

target_link_libraries(fleetflow_services PUBLIC fleetflow_core)

target_include_directories(fleetflow_services PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Runnable menu-driven FleetFlow management application
add_executable(fleetflow_app
    src/app/main.cpp
)
target_link_libraries(fleetflow_app PRIVATE fleetflow_services fleetflow_storage)

enable_testing()
add_subdirectory(tests)
