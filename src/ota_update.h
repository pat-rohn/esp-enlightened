#ifndef OTA_UPDATE_H
#define OTA_UPDATE_H

#include "config.h"

namespace ota_update
{
    void begin(const configman::Configuration &configuration);
    void handle();
}

#endif
