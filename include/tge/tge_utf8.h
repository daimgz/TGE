#ifndef TGE_UTF8_H_
#define TGE_UTF8_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int tge_utf8_decode(const char *data, int size, uint32_t *out_codepoint);
int tge_utf8_char_width(uint32_t codepoint);

#ifdef __cplusplus
}
#endif

#endif
