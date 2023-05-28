#ifndef TIMERSERIES_HTTP_H
#define TIMERSERIES_HTTP_H

#include <Arduino.h>
#include "timeseries.h"
#include "timehelper.h"
#include <map>

using namespace timeseries;

namespace ts_http
{

    class CTimeseriesHttp : public CTimeseries
    {

    public:
        CTimeseriesHttp(const String& timeseriesAddress, CTimeHelper *timehelper);
        virtual ~CTimeseriesHttp(){};

        void newValue(const String &name, const double &value) override;
        bool sendData() override;

    private:
        bool postData(const String &root, const String &url);

    };

}

#endif