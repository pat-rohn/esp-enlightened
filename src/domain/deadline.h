#ifndef DEADLINE_H
#define DEADLINE_H

#include <stdint.h>

namespace timing
{
    inline bool deadlineReached(uint32_t now, uint32_t deadline)
    {
        return static_cast<int32_t>(now - deadline) >= 0;
    }
}

#endif
