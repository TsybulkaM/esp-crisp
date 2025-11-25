#include "core_service.h"

extern "C" {
    #include "cglp.h"
}

// C linkage
extern "C" {
    void md_consoleLog(char *msg) {
        auto service = CoreService::getInstance();
        if (service) {
            service->log_info(msg, "CGLP");
        }
    }
}