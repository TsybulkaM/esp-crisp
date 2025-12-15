#include "fota_adapter.h"
#include "fota.h"

void fota_check_now() {
    FotaService* service = FotaService::getInstance();
    if (service) {
        service->checkNow();
    }
}

const char* fota_get_current_version() {
    return FotaService::getCurrentVersion();
}
