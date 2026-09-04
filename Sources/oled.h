#ifndef __OLED_H
#define __OLED_H

#include <stdint.h>

#define OLED_WIDTH   128
#define OLED_HEIGHT  64

/* 屏幕通道选择 (根据硬件物理摆放: 屏幕2在左侧, 屏幕1在右侧) */
#define OLED_SCREEN_L     1   /* 物理左屏: 屏幕2 (SCL=PB11, SDA=PB8) */
#define OLED_SCREEN_R     0   /* 物理右屏: 屏幕1 (SCL=PB6,  SDA=PB7) */
#define OLED_SCREEN_BOTH  2   /* 双屏同时操作 */

/* 颜色定义 (单色 OLED) */
#define OLED_COLOR_BLACK  0
#define OLED_COLOR_WHITE  1

/* 函数声明 */
void OLED_Init(void);
void OLED_SelectScreen(uint8_t screen);
uint8_t OLED_GetScreen(void);
void OLED_Clear(void);
void OLED_ClearScreen(uint8_t screen);
void OLED_Update(void);
void OLED_UpdateScreen(uint8_t screen);
void OLED_UpdateAll(void);
void OLED_SetDelay(uint8_t delay);

/* 基础图元绘制 (操作当前选中的 Screen) */
void OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t color);
void OLED_DrawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color);
void OLED_DrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color);
void OLED_FillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color);
void OLED_DrawCircle(uint8_t x0, uint8_t y0, uint8_t r, uint8_t color);
void OLED_FillCircle(uint8_t x0, uint8_t y0, uint8_t r, uint8_t color);
void OLED_DrawRoundRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t r, uint8_t color);
void OLED_FillRoundRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t r, uint8_t color);
void OLED_ShowChar(uint8_t x, uint8_t y, char chr, uint8_t size, uint8_t color);
void OLED_ShowString(uint8_t x, uint8_t y, const char *str, uint8_t size, uint8_t color);
void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size, uint8_t color);
void OLED_ShowCharScale(uint8_t x, uint8_t y, char chr, uint8_t scale, uint8_t color);
void OLED_ShowStringScale(uint8_t x, uint8_t y, const char *str, uint8_t scale, uint8_t color);
void OLED_DrawBitmap(uint8_t x, uint8_t y, const uint8_t *bmp, uint8_t w, uint8_t h, uint8_t color);

#endif /* __OLED_H */
