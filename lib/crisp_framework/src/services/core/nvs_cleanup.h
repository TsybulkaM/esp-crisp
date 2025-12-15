#pragma once

namespace NVSCleanup {
    /**
     * Clear test data from NVS (test URLs, test versions, etc)
     */
    void clearTestData();
    
    /**
     * Validate and fix production configuration
     */
    void validateProductionConfig();
}
