# CMake configuration for test executables

# GoogleTest targets (GTest::gtest, GTest::gtest_main) are provided by FetchContent in top-level CMakeLists

# Helper macro to create a gtest executable and register it
macro(add_gtest_executable name src)
    add_executable(${name} ${src})
    target_link_libraries(${name} PRIVATE GTest::gtest GTest::gtest_main)
    add_test(NAME ${name}Tests COMMAND ${name})
endmacro()

# Date and range tests
add_gtest_executable(test_dates test_dates.cpp)
target_link_libraries(test_dates PRIVATE fleetflow_core)

# Vehicle hierarchy tests
add_gtest_executable(test_vehicles test_vehicles.cpp)
target_link_libraries(test_vehicles PRIVATE fleetflow_core)

# User hierarchy tests
add_gtest_executable(test_users test_users.cpp)
target_link_libraries(test_users PRIVATE fleetflow_core)

# Operational records tests
add_gtest_executable(test_records test_records.cpp)
target_link_libraries(test_records PRIVATE fleetflow_core)

# Repository tests
add_gtest_executable(test_repository test_repository.cpp)
target_link_libraries(test_repository PRIVATE fleetflow_core)

# Factory tests
add_gtest_executable(test_factories test_factories.cpp)
target_link_libraries(test_factories PRIVATE fleetflow_factories fleetflow_core)

# CSV helper tests
add_gtest_executable(test_csv_helper test_csv_helper.cpp)
target_link_libraries(test_csv_helper PRIVATE fleetflow_storage fleetflow_core)

# CSV serializers tests
add_gtest_executable(test_csv_serializers test_csv_serializers.cpp)
target_link_libraries(test_csv_serializers PRIVATE fleetflow_storage fleetflow_factories fleetflow_core)

# File storage tests (custom macros, no GTest)
add_executable(test_file_storage test_file_storage.cpp)
target_link_libraries(test_file_storage PRIVATE fleetflow_storage fleetflow_factories fleetflow_core)
add_test(NAME FileStorageTests COMMAND test_file_storage)

# Service layer tests (use GTest)
add_executable(test_service_exceptions test_service_exceptions.cpp)
target_link_libraries(test_service_exceptions PRIVATE fleetflow_services GTest::gtest GTest::gtest_main)
add_test(NAME ServiceExceptionTests COMMAND test_service_exceptions)

add_executable(test_service_validation test_service_validation.cpp)
target_link_libraries(test_service_validation PRIVATE fleetflow_services GTest::gtest GTest::gtest_main)
add_test(NAME ServiceValidationTests COMMAND test_service_validation)

# FleetService tests
add_gtest_executable(test_fleet_service test_fleet_service.cpp)
target_link_libraries(test_fleet_service PRIVATE fleetflow_services fleetflow_core)

# RentalService tests
add_gtest_executable(test_rental_service test_rental_service.cpp)
target_link_libraries(test_rental_service PRIVATE fleetflow_services fleetflow_core)

# PaymentService tests
add_gtest_executable(test_payment_service test_payment_service.cpp)
target_link_libraries(test_payment_service PRIVATE fleetflow_services fleetflow_core)

# MaintenanceService tests
add_gtest_executable(test_maintenance_service test_maintenance_service.cpp)
target_link_libraries(test_maintenance_service PRIVATE fleetflow_services fleetflow_core)
