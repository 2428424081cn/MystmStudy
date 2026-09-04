#ifndef __OLED_FONT_H
#define __OLED_FONT_H

#include <stdint.h>

/* 6x8 常用 ASCII 字符点阵 (ASCII 32 ' ' 到 126 '~') */
/* 每个字符 6 字节宽度 (前 5 字节为点阵，第 6 字节为 0x00 空白留白间隔) */
extern const uint8_t OLED_F6x8[][6];

/* 8x16 常用 ASCII 字符点阵 (ASCII 32 ' ' 到 126 '~') */
/* 每个字符 16 字节 (前 8 字节上半部，后 8 字节下半部) */
extern const uint8_t OLED_F8x16[][16];

#endif /* __OLED_FONT_H */
