#ifndef TGE_UTF8_H_
#define TGE_UTF8_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Decodes one UTF-8 sequence from data[0..size). Returns the number of bytes
 * consumed, or -1 when the input is truncated/invalid, and stores the
 * codepoint in *out_codepoint. */
int tge_utf8_decode(const char *data, int size, uint32_t *out_codepoint);
/* Returns the terminal display width of a codepoint (0 for combining marks,
 * 2 for wide/CJK glyphs, 1 otherwise). */
int tge_utf8_char_width(uint32_t codepoint);

#ifdef __cplusplus
}
#endif

#endif
