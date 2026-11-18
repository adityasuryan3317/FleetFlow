#include <gtest/gtest.h>
#include "fleetflow/services/Exceptions.hpp"
#include "fleetflow/core/Exceptions.hpp"

TEST(BusinessExceptionTest, InheritsFromFleetFlowException) {
    fleetflow::services::BusinessException ex("test message");
    // Verify inheritance via catch of base type
    try {
        throw ex;
    } catch (const fleetflow::core::FleetFlowException& base) {
        SUCCEED();
    } catch (...) {
        FAIL() << "BusinessException not caught as FleetFlowException";
    }
    // Verify message content
    EXPECT_STREQ(ex.what(), "Business Error: test message");
}

