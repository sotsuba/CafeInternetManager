#ifndef __sha1_h__
#define __sha1_h__

#include <cstdint>
#include <cstdlib>

class SHA1 {
public:
    SHA1() { reset(); }

    void reset() {
        length_low = 0;
        length_high = 0;
        message_block_index = 0;

        H[0] = 0x67452301;
        H[1] = 0xEFCDAB89;
        H[2] = 0x98BADCFE;
        H[3] = 0x10325476;
        H[4] = 0xC3D2E1F0;

        corrupted = false;
        computed = false;
    }

    void input(const unsigned char* data, size_t len) {
        if (computed) {
            corrupted = true;
            return;
        }
        while (len-- && !corrupted) {
            message_block[message_block_index++] = *data & 0xFF;
            length_low += 8;
            if (length_low == 0) {
                length_high++;
                if (length_high == 0) corrupted = true;
            }
            if (message_block_index == 64) process_message_block();
            ++data;
        }
    }

    void result(unsigned char digest[20]) {
        if (corrupted) return;
        if (!computed) {
            pad_message();
            computed = true;
        }

        for (int i = 0; i < 20; ++i) {
            digest[i] = (unsigned char)(H[i >> 2] >> ((3 - (i & 0x3)) * 8));
        }
    }

private:
    uint32_t H[5];
    uint32_t length_low, length_high;
    unsigned char message_block[64];
    int message_block_index;
    bool corrupted, computed;

    static uint32_t circular_shift(int bits, uint32_t word) {
        return (word << bits) | (word >> (32 - bits));
    }

    void process_message_block() {
        const uint32_t K[] = {
            0x5A827999,
            0x6ED9EBA1,
            0x8F1BBCDC,
            0xCA62C1D6
        };

        uint32_t W[80];
        for (int t = 0; t < 16; ++t) {
            W[t] = (message_block[t * 4] << 24)
                 | (message_block[t * 4 + 1] << 16)
                 | (message_block[t * 4 + 2] << 8)
                 | (message_block[t * 4 + 3]);
        }
        for (int t = 16; t < 80; ++t)
            W[t] = circular_shift(1, W[t - 3] ^ W[t - 8] ^ W[t - 14] ^ W[t - 16]);

        uint32_t a = H[0];
        uint32_t b = H[1];
        uint32_t c = H[2];
        uint32_t d = H[3];
        uint32_t e = H[4];

        for (int t = 0; t < 80; ++t) {
            uint32_t temp =
                circular_shift(5, a) +
                f(t, b, c, d) +
                e +
                W[t] +
                K[t / 20];

            e = d;
            d = c;
            c = circular_shift(30, b);
            b = a;
            a = temp;
        }

        H[0] += a;
        H[1] += b;
        H[2] += c;
        H[3] += d;
        H[4] += e;

        message_block_index = 0;
    }

    static uint32_t f(int t, uint32_t b, uint32_t c, uint32_t d) {
        if (t < 20) return (b & c) | ((~b) & d);
        if (t < 40) return b ^ c ^ d;
        if (t < 60) return (b & c) | (b & d) | (c & d);
        return b ^ c ^ d;
    }

    void pad_message() {
        message_block[message_block_index++] = 0x80;

        if (message_block_index > 56) {
            while (message_block_index < 64) {
                message_block[message_block_index++] = 0;
            }
            process_message_block();
        }

        while (message_block_index < 56) {
            message_block[message_block_index++] = 0;
        }

        message_block[56] = (length_high >> 24) & 0xFF;
        message_block[57] = (length_high >> 16) & 0xFF;
        message_block[58] = (length_high >> 8) & 0xFF;
        message_block[59] = (length_high >> 0) & 0xFF;

        message_block[60] = (length_low >> 24) & 0xFF;
        message_block[61] = (length_low >> 16) & 0xFF;
        message_block[62] = (length_low >> 8) & 0xFF;
        message_block[63] = (length_low >> 0) & 0xFF;

        process_message_block();
    }
};

#endif