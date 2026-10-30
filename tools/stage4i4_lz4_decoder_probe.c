#include <stddef.h>
#include <stdint.h>

/* Bounded raw LZ4-block decoder used only to measure a credible RP2350
 * integration.  The destination may be the existing 64 KiB CPC RAM buffer;
 * no history or work buffer is allocated. */
__attribute__((noinline))
int stage4i4_lz4_decode(const uint8_t *src, size_t src_len,
                        uint8_t *dst, size_t dst_len) {
    const uint8_t *ip = src;
    const uint8_t *const iend = src + src_len;
    uint8_t *op = dst;
    uint8_t *const oend = dst + dst_len;

    while (ip < iend) {
        uint8_t token = *ip++;
        size_t literal_len = token >> 4;
        if (literal_len == 15) {
            uint8_t extension;
            do {
                if (ip >= iend) return -1;
                extension = *ip++;
                if (literal_len > SIZE_MAX - extension) return -1;
                literal_len += extension;
            } while (extension == 255);
        }
        if (literal_len > (size_t)(iend - ip) ||
            literal_len > (size_t)(oend - op)) return -1;
        for (size_t i = 0; i < literal_len; ++i) op[i] = ip[i];
        ip += literal_len;
        op += literal_len;
        if (ip == iend) break;
        if ((size_t)(iend - ip) < 2) return -1;
        size_t offset = (size_t)ip[0] | ((size_t)ip[1] << 8);
        ip += 2;
        if (offset == 0 || offset > (size_t)(op - dst)) return -1;

        size_t match_len = token & 15;
        if (match_len == 15) {
            uint8_t extension;
            do {
                if (ip >= iend) return -1;
                extension = *ip++;
                if (match_len > SIZE_MAX - extension) return -1;
                match_len += extension;
            } while (extension == 255);
        }
        if (match_len > SIZE_MAX - 4) return -1;
        match_len += 4;
        if (match_len > (size_t)(oend - op)) return -1;
        const uint8_t *match = op - offset;
        for (size_t i = 0; i < match_len; ++i) op[i] = match[i];
        op += match_len;
    }
    return op == oend ? (int)dst_len : -1;
}
