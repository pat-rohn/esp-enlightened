#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include "timehelper.h"

namespace logging
{
    class CLogger
    {

    public:
        CLogger(const String& timeseriesAddress, const String& deviceID);
        virtual ~CLogger(){};

        void logMessage(const String &msg);

    public:
        bool m_IsOnline;

    private:
        bool postData(const String &root, const String &url);

    private:
        String m_ServerAddress;
        String m_DeviceID;

    };
}

#endif