#ifndef __OLED_H
#define __OLED_H

#include <stdint.h>

#define OLED_WIDTH   128
#define OLED_HEIGHT  64

/* 颜色定义 (单色 OLED) */
#define OLED_COLOR_BLACK  0
#define OLED_COLOR_WHITE  1

/* 函数声明 */
void OLED_Init(void);
void OLED_Clear(void);
void OLED_Update(void);
void OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t color);
void OLED_DrawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color);
void OLED_DrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color);
void OLED_FillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color);
void OLED_ShowChar(uint8_t x, uint8_t y, char chr, uint8_t size, uint8_t color);
void OLED_ShowString(uint8_t x, uint8_t y, const char *str, uint8_t size, uint8_t color);
void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size, uint8_t color);
void OLED_ShowCharScale(uint8_t x, uint8_t y, char chr, uint8_t scale, uint8_t color);
void OLED_ShowStringScale(uint8_t x, uint8_t y, const char *str, uint8_t scale, uint8_t color);
void OLED_DrawBitmap(uint8_t x, uint8_t y, const uint8_t *bmp, uint8_t w, uint8_t h, uint8_t color);
void OLED_SetDelay(uint8_t delay);
void OLED_DrawCircle(uint8_t x0, uint8_t y0, uint8_t r, uint8_t color);
void OLED_FillCircle(uint8_t x0, uint8_t y0, uint8_t r, uint8_t color);

#endif /* __OLED_H */
