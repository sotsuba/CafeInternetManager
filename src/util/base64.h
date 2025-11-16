#ifndef __base64_h__
#define __base64_h__

#include <string>

static std::string base64_encode(const unsigned char* data, size_t len) {
    static const char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string out;
    out.reserve(((len + 2) / 3) * 4);

    size_t i = 0;

    while (i + 2 < len) {
        unsigned int triple = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
        i += 3;

        out.push_back(table[(triple >> 18) & 0x3F]);
        out.push_back(table[(triple >> 12) & 0x3F]);
        out.push_back(table[(triple >> 6)  & 0x3F]);
        out.push_back(table[ triple        & 0x3F]);
    }

    if (i < len) {
        unsigned int triple = data[i] << 16;
        if (i + 1 < len) {
            triple |= data[i + 1] << 8;
        }

        out.push_back(table[(triple >> 18) & 0x3F]);
        out.push_back(table[(triple >> 12) & 0x3F]);

        if (i + 1 < len) {
            out.push_back(table[(triple >> 6) & 0x3F]);
            out.push_back('=');
        } else {
            out.push_back('=');
            out.push_back('=');
        }
    }

    return out;
}

#endif