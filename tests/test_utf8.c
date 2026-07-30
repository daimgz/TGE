#include "tge/tge_utf8.h"
#include "tge_test.h"

TGE_TEST(utf8_ascii)
{
    uint32_t cp;
    int n;

    n = tge_utf8_decode("A", 1, &cp);
    TGE_ASSERT(n == 1, "ASCII 'A' length");
    TGE_ASSERT(cp == 0x41, "ASCII 'A' codepoint");

    n = tge_utf8_decode("z", 1, &cp);
    TGE_ASSERT(n == 1, "ASCII 'z' length");
    TGE_ASSERT(cp == 0x7A, "ASCII 'z' codepoint");

    n = tge_utf8_decode("0", 1, &cp);
    TGE_ASSERT(n == 1, "ASCII '0' length");
    TGE_ASSERT(cp == 0x30, "ASCII '0' codepoint");

    n = tge_utf8_decode("\x7F", 1, &cp);
    TGE_ASSERT(n == 1, "ASCII DEL length");
    TGE_ASSERT(cp == 0x7F, "ASCII DEL codepoint");
}

TGE_TEST(utf8_2byte)
{
    uint32_t cp;
    int n;

    n = tge_utf8_decode("\xC3\xB1", 2, &cp);
    TGE_ASSERT(n == 2, "'ñ' length");
    TGE_ASSERT(cp == 0xF1, "'ñ' codepoint");

    n = tge_utf8_decode("\xC3\xA9", 2, &cp);
    TGE_ASSERT(n == 2, "'é' length");
    TGE_ASSERT(cp == 0xE9, "'é' codepoint");

    n = tge_utf8_decode("\xC2\xA9", 2, &cp);
    TGE_ASSERT(n == 2, "'©' length");
    TGE_ASSERT(cp == 0xA9, "'©' codepoint");

    n = tge_utf8_decode("\xD0\xB0", 2, &cp);
    TGE_ASSERT(n == 2, "'а' length");
    TGE_ASSERT(cp == 0x0430, "'а' codepoint");
}

TGE_TEST(utf8_3byte)
{
    uint32_t cp;
    int n;

    n = tge_utf8_decode("\xE2\x82\xAC", 3, &cp);
    TGE_ASSERT(n == 3, "'€' length");
    TGE_ASSERT(cp == 0x20AC, "'€' codepoint");

    n = tge_utf8_decode("\xE2\x99\xA5", 3, &cp);
    TGE_ASSERT(n == 3, "'♥' length");
    TGE_ASSERT(cp == 0x2665, "'♥' codepoint");

    n = tge_utf8_decode("\xE3\x81\x82", 3, &cp);
    TGE_ASSERT(n == 3, "'あ' length");
    TGE_ASSERT(cp == 0x3042, "'あ' codepoint");
}

TGE_TEST(utf8_4byte)
{
    uint32_t cp;
    int n;

    n = tge_utf8_decode("\xF0\x9D\x84\x9E", 4, &cp);
    TGE_ASSERT(n == 4, "'𝄞' length");
    TGE_ASSERT(cp == 0x1D11E, "'𝄞' codepoint");

    n = tge_utf8_decode("\xF0\x9F\x98\x80", 4, &cp);
    TGE_ASSERT(n == 4, "'😀' length");
    TGE_ASSERT(cp == 0x1F600, "'😀' codepoint");

    n = tge_utf8_decode("\xF0\xA4\xAD\xA2", 4, &cp);
    TGE_ASSERT(n == 4, "'𤭢' length");
    TGE_ASSERT(cp == 0x24B62, "'𤭢' codepoint");
}

TGE_TEST(utf8_overlong)
{
    uint32_t cp;
    int n;

    n = tge_utf8_decode("\xC0\x80", 2, &cp);
    TGE_ASSERT(n == -1, "overlong 2-byte U+0000");

    n = tge_utf8_decode("\xE0\x80\x80", 3, &cp);
    TGE_ASSERT(n == -1, "overlong 3-byte U+0000");

    n = tge_utf8_decode("\xF0\x80\x80\x80", 4, &cp);
    TGE_ASSERT(n == -1, "overlong 4-byte U+0000");

    n = tge_utf8_decode("\xE0\x9F\xBF", 3, &cp);
    TGE_ASSERT(n == -1, "overlong 3-byte just below U+0800");
}

TGE_TEST(utf8_surrogate)
{
    uint32_t cp;
    int n;

    n = tge_utf8_decode("\xED\xA0\x80", 3, &cp);
    TGE_ASSERT(n == -1, "surrogate U+D800");

    n = tge_utf8_decode("\xED\xBF\xBF", 3, &cp);
    TGE_ASSERT(n == -1, "surrogate U+DFFF");

    n = tge_utf8_decode("\xED\xAF\xBF", 3, &cp);
    TGE_ASSERT(n == -1, "surrogate U+DBFF");
}

TGE_TEST(utf8_out_of_range)
{
    uint32_t cp;
    int n;

    n = tge_utf8_decode("\xF4\x90\x80\x80", 4, &cp);
    TGE_ASSERT(n == -1, "> U+10FFFF (U+110000)");

    n = tge_utf8_decode("\xF7\xBF\xBF\xBF", 4, &cp);
    TGE_ASSERT(n == -1, "invalid starter 0xF7");
}

TGE_TEST(utf8_truncated)
{
    uint32_t cp;
    int n;

    n = tge_utf8_decode("\xC3", 1, &cp);
    TGE_ASSERT(n == -1, "truncated 2-byte (no continuation)");

    n = tge_utf8_decode("\xE2\x82", 2, &cp);
    TGE_ASSERT(n == -1, "truncated 3-byte (1 continuation missing)");

    n = tge_utf8_decode("\xF0\x9D\x84", 3, &cp);
    TGE_ASSERT(n == -1, "truncated 4-byte (1 continuation missing)");
}

TGE_TEST(utf8_invalid_bytes)
{
    uint32_t cp;
    int n;

    n = tge_utf8_decode("\x80", 1, &cp);
    TGE_ASSERT(n == -1, "continuation byte without starter (0x80)");

    n = tge_utf8_decode("\xFF", 1, &cp);
    TGE_ASSERT(n == -1, "invalid byte 0xFF");

    n = tge_utf8_decode("\xFE", 1, &cp);
    TGE_ASSERT(n == -1, "invalid byte 0xFE");

    n = tge_utf8_decode("", 0, &cp);
    TGE_ASSERT(n == -1, "empty buffer");
}

TGE_TEST(utf8_invalid_continuation)
{
    uint32_t cp;
    int n;

    n = tge_utf8_decode("\xC3\x20", 2, &cp);
    TGE_ASSERT(n == -1, "2-byte with non-continuation second byte");

    n = tge_utf8_decode("\xE2\x82\x20", 3, &cp);
    TGE_ASSERT(n == -1, "3-byte with non-continuation third byte");

    n = tge_utf8_decode("\xF0\x9D\x84\x20", 4, &cp);
    TGE_ASSERT(n == -1, "4-byte with non-continuation fourth byte");
}

TGE_TEST(utf8_boundaries)
{
    uint32_t cp;
    int n;

    n = tge_utf8_decode("\x7F", 1, &cp);
    TGE_ASSERT(n == 1 && cp == 0x7F, "boundary U+007F (last 1-byte)");

    n = tge_utf8_decode("\xC2\x80", 2, &cp);
    TGE_ASSERT(n == 2 && cp == 0x80, "boundary U+0080 (first 2-byte)");

    n = tge_utf8_decode("\xDF\xBF", 2, &cp);
    TGE_ASSERT(n == 2 && cp == 0x7FF, "boundary U+07FF (last 2-byte)");

    n = tge_utf8_decode("\xE0\xA0\x80", 3, &cp);
    TGE_ASSERT(n == 3 && cp == 0x800, "boundary U+0800 (first 3-byte)");

    n = tge_utf8_decode("\xEF\xBF\xBF", 3, &cp);
    TGE_ASSERT(n == 3 && cp == 0xFFFF, "boundary U+FFFF (last 3-byte)");

    n = tge_utf8_decode("\xF0\x90\x80\x80", 4, &cp);
    TGE_ASSERT(n == 4 && cp == 0x10000, "boundary U+10000 (first 4-byte)");

    n = tge_utf8_decode("\xF4\x8F\xBF\xBF", 4, &cp);
    TGE_ASSERT(n == 4 && cp == 0x10FFFF, "boundary U+10FFFF (last valid)");
}

TGE_TEST(width_latin)
{
    TGE_ASSERT(tge_utf8_char_width(0x0041) == 1, "Latin A width=1");
    TGE_ASSERT(tge_utf8_char_width(0x007A) == 1, "Latin z width=1");
    TGE_ASSERT(tge_utf8_char_width(0x00E9) == 1, "é width=1");
    TGE_ASSERT(tge_utf8_char_width(0x00F1) == 1, "ñ width=1");
    TGE_ASSERT(tge_utf8_char_width(0x01B5) == 1, "Ƶ width=1");
    TGE_ASSERT(tge_utf8_char_width(0x20AC) == 1, "€ width=1");
}

TGE_TEST(width_cjk)
{
    TGE_ASSERT(tge_utf8_char_width(0x4E2D) == 2, "中 width=2");
    TGE_ASSERT(tge_utf8_char_width(0x56FD) == 2, "国 width=2");
    TGE_ASSERT(tge_utf8_char_width(0x3042) == 2, "あ width=2");
    TGE_ASSERT(tge_utf8_char_width(0x30A2) == 2, "ア width=2");
    TGE_ASSERT(tge_utf8_char_width(0xAC00) == 2, "가 width=2");
    TGE_ASSERT(tge_utf8_char_width(0xFF21) == 2, "fullwidth A width=2");
    TGE_ASSERT(tge_utf8_char_width(0x3000) == 2, "ideographic space width=2");
}

TGE_TEST(width_emoji)
{
    TGE_ASSERT(tge_utf8_char_width(0x1F600) == 2, "😀 width=2");
    TGE_ASSERT(tge_utf8_char_width(0x1F4A9) == 2, "💩 width=2");
    TGE_ASSERT(tge_utf8_char_width(0x2764) == 1, "❤ width=1");
}

TGE_TEST(width_combining)
{
    TGE_ASSERT(tge_utf8_char_width(0x0300) == 0, "combining grave width=0");
    TGE_ASSERT(tge_utf8_char_width(0x0301) == 0, "combining acute width=0");
    TGE_ASSERT(tge_utf8_char_width(0x0308) == 0, "combining diaeresis width=0");
    TGE_ASSERT(tge_utf8_char_width(0x0327) == 0, "combining cedilla width=0");
    TGE_ASSERT(tge_utf8_char_width(0xFE0F) == 0, "variation selector-16 width=0");
}

TGE_TEST(width_control)
{
    TGE_ASSERT(tge_utf8_char_width(0x0000) == 0, "NUL width=0");
    TGE_ASSERT(tge_utf8_char_width(0x0001) == 0, "SOH width=0");
    TGE_ASSERT(tge_utf8_char_width(0x000A) == 0, "LF width=0");
    TGE_ASSERT(tge_utf8_char_width(0x000D) == 0, "CR width=0");
    TGE_ASSERT(tge_utf8_char_width(0x001B) == 0, "ESC width=0");
    TGE_ASSERT(tge_utf8_char_width(0x0009) == 1, "TAB width=1");
    TGE_ASSERT(tge_utf8_char_width(0x007F) == 0, "DEL width=0");
    TGE_ASSERT(tge_utf8_char_width(0x0080) == 0, "C1 control U+0080 width=0");
    TGE_ASSERT(tge_utf8_char_width(0x009F) == 0, "C1 control U+009F width=0");
}

TGE_TEST(width_hangul_jamo)
{
    TGE_ASSERT(tge_utf8_char_width(0x1100) == 2, "Hangul Choseong Kiyeok width=2");
    TGE_ASSERT(tge_utf8_char_width(0x115F) == 2, "Hangul Choseong Filler width=2");
    TGE_ASSERT(tge_utf8_char_width(0x1160) == 0, "Hangul Jungseong Filler width=0 (combining)");
}

int main(void)
{
    test_utf8_ascii();
    test_utf8_2byte();
    test_utf8_3byte();
    test_utf8_4byte();
    test_utf8_overlong();
    test_utf8_surrogate();
    test_utf8_out_of_range();
    test_utf8_truncated();
    test_utf8_invalid_bytes();
    test_utf8_invalid_continuation();
    test_utf8_boundaries();
    test_width_latin();
    test_width_cjk();
    test_width_emoji();
    test_width_combining();
    test_width_control();
    test_width_hangul_jamo();
    return tge_test_report();
}
