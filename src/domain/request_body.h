#ifndef REQUEST_BODY_H
#define REQUEST_BODY_H

#include <stddef.h>

namespace http_body
{
    // How much of an incoming body chunk may be written into a buffer that was
    // sized from the request's Content-Length.
    //
    // The chunk bounds are peer controlled. ESPAsyncWebServer >= 3.11 clamps
    // them itself (`len = std::min(len, _contentLength - _parsedLength)`), but
    // 3.6.x — the floor of our `^3.6.0` range, and what the ESP8266 build
    // resolves to — forwards whatever arrived. There, a request declaring a
    // short Content-Length and then sending more would run the copy off the end
    // of the allocation, and body collection happens before any handler (so
    // before isAuthorized) ever sees the request.
    //
    // Returns 0 when the chunk starts at or past the declared end, so callers
    // can drop it. Never returns more than `total - index`, which keeps the
    // trailing NUL of a `total + 1` byte buffer intact.
    inline size_t writableChunk(size_t index, size_t len, size_t total)
    {
        if (index >= total)
        {
            return 0;
        }
        const size_t writable = total - index;
        return len < writable ? len : writable;
    }
}

#endif
