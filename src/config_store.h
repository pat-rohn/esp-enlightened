#ifndef CONFIG_STORE_H
#define CONFIG_STORE_H

#include <Arduino.h>

namespace configstore
{
    void begin();
    String read(const char *path);
    bool write(const char *path, const char *message);
}

#endif
