#include "oled.h"
#include "oled_font.h"

/* 寄存器定义 */
#define RCC_BASE      (0x40021000UL)
#define RCC_APB2ENR   (*(volatile uint32_t *)(RCC_BASE + 0x18UL))

#define GPIOB_BASE    (0x40010C00UL)
#define GPIOB_CRL     (*(volatile uint32_t *)(GPIOB_BASE + 0x00UL))
#define GPIOB_CRH     (*(volatile uint32_t *)(GPIOB_BASE + 0x04UL))
#define GPIOB_BSRR    (*(volatile uint32_t *)(GPIOB_BASE + 0x10UL))

/* 引脚定义:
 * 屏幕1 (左眼): SCL1 = PB6,  SDA1 = PB7
 * 屏幕2 (右眼): SCL2 = PB11, SDA2 = PB8
 */
#define SCL1_H()  (GPIOB_BSRR = (1UL << 6))
#define SCL1_L()  (GPIOB_BSRR = (1UL << (6 + 16)))
#define SDA1_H()  (GPIOB_BSRR = (1UL << 7))
#define SDA1_L()  (GPIOB_BSRR = (1UL << (7 + 16)))

#define SCL2_H()  (GPIOB_BSRR = (1UL << 11))
#define SCL2_L()  (GPIOB_BSRR = (1UL << (11 + 16)))
#define SDA2_H()  (GPIOB_BSRR = (1UL << 8))
#define SDA2_L()  (GPIOB_BSRR = (1UL << (8 + 16)))

/* 双屏独立显存缓冲区: 2 x 8页 x 128字节 = 2048 字节 */
static uint8_t OLED_Gram[2][8][128];
static uint8_t g_current_screen = OLED_SCREEN_L;

/* 软件微延时周期 */
static volatile uint8_t g_i2c_delay = 0;

void OLED_SetDelay(uint8_t delay)
{
    g_i2c_delay = delay;
}

static inline void I2C_Delay(void)
{
    for (volatile uint8_t i = 0; i < g_i2c_delay; i++) {
        __asm__ volatile ("nop");
    }
}

static inline void SCL_Set(uint8_t ch, uint8_t level)
{
    if (ch == 0) {
        if (level) SCL1_H(); else SCL1_L();
    } else if (ch == 1) {
        if (level) SCL2_H(); else SCL2_L();
    } else {
        if (level) { SCL1_H(); SCL2_H(); }
        else       { SCL1_L(); SCL2_L(); }
    }
}

static inline void SDA_Set(uint8_t ch, uint8_t level)
{
    if (ch == 0) {
        if (level) SDA1_H(); else SDA1_L();
    } else if (ch == 1) {
        if (level) SDA2_H(); else SDA2_L();
    } else {
        if (level) { SDA1_H(); SDA2_H(); }
        else       { SDA1_L(); SDA2_L(); }
    }
}

static void I2C_Start(uint8_t ch)
{
    SDA_Set(ch, 1);
    SCL_Set(ch, 1);
    I2C_Delay();
    SDA_Set(ch, 0);
    I2C_Delay();
    SCL_Set(ch, 0);
    I2C_Delay();
}

static void I2C_Stop(uint8_t ch)
{
    SDA_Set(ch, 0);
    SCL_Set(ch, 1);
    I2C_Delay();
    SDA_Set(ch, 1);
    I2C_Delay();
}

static void I2C_SendByte(uint8_t ch, uint8_t byte)
{
    for (uint8_t i = 0; i < 8; i++) {
        SDA_Set(ch, (byte & 0x80) ? 1 : 0);
        I2C_Delay();
        SCL_Set(ch, 1);
        I2C_Delay();
        SCL_Set(ch, 0);
        I2C_Delay();
        byte <<= 1;
    }
    /* ACK 第9周期 */
    SDA_Set(ch, 1);
    I2C_Delay();
    SCL_Set(ch, 1);
    I2C_Delay();
    SCL_Set(ch, 0);
    I2C_Delay();
}

static void OLED_WriteCmd(uint8_t ch, uint8_t cmd)
{
    I2C_Start(ch);
    I2C_SendByte(ch, 0x78); /* 7位地址 0x3C << 1 */
    I2C_SendByte(ch, 0x00); /* 命令模式 */
    I2C_SendByte(ch, cmd);
    I2C_Stop(ch);
}

void OLED_SelectScreen(uint8_t screen)
{
    if (screen > OLED_SCREEN_BOTH) screen = OLED_SCREEN_BOTH;
    g_current_screen = screen;
}

uint8_t OLED_GetScreen(void)
{
    return g_current_screen;
}

void OLED_Init(void)
{
    /* 1. 使能 GPIOB 时钟 */
    RCC_APB2ENR |= (1UL << 3);

    /* 2. 配置 PB6, PB7 为通用推挽输出 50MHz (CRL bits 31..24) */
    GPIOB_CRL &= ~(0xFFUL << 24);
    GPIOB_CRL |=  (0x33UL << 24);

    /* 配置 PB8, PB11 为通用推挽输出 50MHz (CRH bits 3..0 及 15..12) */
    GPIOB_CRH &= ~((0xFUL << 0) | (0xFUL << 12));
    GPIOB_CRH |=  ((0x3UL << 0) | (0x3UL << 12));

    SCL1_H(); SDA1_H();
    SCL2_H(); SDA2_H();

    /* 上电稳定微延时 */
    for (volatile int i = 0; i < 50000; i++) {
        __asm__ volatile ("nop");
    }

    /* 3. 同时初始化两块 SSD1306 OLED 屏幕 (ch = BOTH) */
    uint8_t init_cmds[] = {
        0xAE, /* 关闭显示 */
        0xD5, 0xF0, /* 最高振荡器频率，极限刷新 */
        0xA8, 0x3F, /* 1/64 duty */
        0xD3, 0x00, /* 无显示偏移 */
        0x40,       /* 起始行 0 */
        0x8D, 0x14, /* 开启内部电荷泵 */
        0x20, 0x00, /* 水平内存寻址模式 */
        0xA1,       /* 段重映射 */
        0xC8,       /* COM 输出反向扫描 */
        0xDA, 0x12, /* COM 引脚配置 */
        0x81, 0xCF, /* 对比度 */
        0xD9, 0xF1, /* 预充电周期 */
        0xDB, 0x40, /* VCOMH */
        0xA4,       /* 全局跟随 RAM */
        0xA6,       /* 正常显示模式 */
        0xAF        /* 开启显示 */
    };

    for (uint8_t i = 0; i < sizeof(init_cmds); i++) {
        OLED_WriteCmd(OLED_SCREEN_BOTH, init_cmds[i]);
    }

    OLED_ClearScreen(0);
    OLED_ClearScreen(1);
    OLED_UpdateAll();
}

void OLED_ClearScreen(uint8_t s)
{
    if (s > 1) return;
    uint32_t *p32 = (uint32_t *)OLED_Gram[s];
    for (uint16_t i = 0; i < 256; i++) {
        p32[i] = 0;
    }
}

void OLED_Clear(void)
{
    if (g_current_screen == OLED_SCREEN_BOTH) {
        OLED_ClearScreen(0);
        OLED_ClearScreen(1);
    } else {
        OLED_ClearScreen(g_current_screen);
    }
}

void OLED_UpdateScreen(uint8_t s)
{
    if (s > 1) return;

    OLED_WriteCmd(s, 0x21);
    OLED_WriteCmd(s, 0x00);
    OLED_WriteCmd(s, 0x7F);

    OLED_WriteCmd(s, 0x22);
    OLED_WriteCmd(s, 0x00);
    OLED_WriteCmd(s, 0x07);

    I2C_Start(s);
    I2C_SendByte(s, 0x78);
    I2C_SendByte(s, 0x40);
    const uint8_t *ptr = (const uint8_t *)OLED_Gram[s];
    for (uint16_t i = 0; i < 1024; i++) {
        I2C_SendByte(s, *ptr++);
    }
    I2C_Stop(s);
}

void OLED_UpdateAll(void)
{
    OLED_UpdateScreen(0);
    OLED_UpdateScreen(1);
}

void OLED_Update(void)
{
    if (g_current_screen == OLED_SCREEN_BOTH) {
        OLED_UpdateAll();
    } else {
        OLED_UpdateScreen(g_current_screen);
    }
}

void OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t color)
{
    if (x >= OLED_WIDTH || y >= OLED_HEIGHT) return;
    uint8_t page = y / 8;
    uint8_t bit  = y % 8;

    if (g_current_screen == OLED_SCREEN_BOTH) {
        for (uint8_t s = 0; s < 2; s++) {
            if (color) OLED_Gram[s][page][x] |=  (1 << bit);
            else       OLED_Gram[s][page][x] &= ~(1 << bit);
        }
    } else {
        uint8_t s = g_current_screen;
        if (color) OLED_Gram[s][page][x] |=  (1 << bit);
        else       OLED_Gram[s][page][x] &= ~(1 << bit);
    }
}

void OLED_DrawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color)
{
    int dx = (x2 > x1) ? (x2 - x1) : (x1 - x2);
    int dy = (y2 > y1) ? (y2 - y1) : (y1 - y2);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    while (1) {
        OLED_DrawPoint(x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

void OLED_DrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color)
{
    if (w == 0 || h == 0) return;
    OLED_DrawLine(x, y, x + w - 1, y, color);
    OLED_DrawLine(x, y + h - 1, x + w - 1, y + h - 1, color);
    OLED_DrawLine(x, y, x, y + h - 1, color);
    OLED_DrawLine(x + w - 1, y, x + w - 1, y + h - 1, color);
}

void OLED_FillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color)
{
    for (uint8_t i = 0; i < w; i++) {
        for (uint8_t j = 0; j < h; j++) {
            OLED_DrawPoint(x + i, y + j, color);
        }
    }
}

void OLED_DrawCircle(uint8_t x0, uint8_t y0, uint8_t r, uint8_t color)
{
    int16_t x = 0;
    int16_t y = r;
    int16_t d = 3 - 2 * r;
    while (x <= y) {
        OLED_DrawPoint(x0 + x, y0 + y, color);
        OLED_DrawPoint(x0 - x, y0 + y, color);
        OLED_DrawPoint(x0 + x, y0 - y, color);
        OLED_DrawPoint(x0 - x, y0 - y, color);
        OLED_DrawPoint(x0 + y, y0 + x, color);
        OLED_DrawPoint(x0 - y, y0 + x, color);
        OLED_DrawPoint(x0 + y, y0 - x, color);
        OLED_DrawPoint(x0 - y, y0 - x, color);
        if (d < 0) {
            d = d + 4 * x + 6;
        } else {
            d = d + 4 * (x - y) + 10;
            y--;
        }
        x++;
    }
}

void OLED_FillCircle(uint8_t x0, uint8_t y0, uint8_t r, uint8_t color)
{
    for (int16_t y = -r; y <= r; y++) {
        for (int16_t x = -r; x <= r; x++) {
            if (x * x + y * y <= r * r) {
                OLED_DrawPoint(x0 + x, y0 + y, color);
            }
        }
    }
}

void OLED_DrawRoundRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t r, uint8_t color)
{
    if (w == 0 || h == 0) return;
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;

    OLED_DrawLine(x + r, y, x + w - r - 1, y, color);
    OLED_DrawLine(x + r, y + h - 1, x + w - r - 1, y + h - 1, color);
    OLED_DrawLine(x, y + r, x, y + h - r - 1, color);
    OLED_DrawLine(x + w - 1, y + r, x + w - 1, y + h - r - 1, color);

    int16_t cx_l = x + r;
    int16_t cx_r = x + w - r - 1;
    int16_t cy_t = y + r;
    int16_t cy_b = y + h - r - 1;

    int16_t fx = 0;
    int16_t fy = r;
    int16_t d  = 3 - 2 * r;
    while (fx <= fy) {
        OLED_DrawPoint(cx_l - fx, cy_t - fy, color);
        OLED_DrawPoint(cx_l - fy, cy_t - fx, color);
        OLED_DrawPoint(cx_r + fx, cy_t - fy, color);
        OLED_DrawPoint(cx_r + fy, cy_t - fx, color);
        OLED_DrawPoint(cx_l - fx, cy_b + fy, color);
        OLED_DrawPoint(cx_l - fy, cy_b + fx, color);
        OLED_DrawPoint(cx_r + fx, cy_b + fy, color);
        OLED_DrawPoint(cx_r + fy, cy_b + fx, color);

        if (d < 0) {
            d += 4 * fx + 6;
        } else {
            d += 4 * (fx - fy) + 10;
            fy--;
        }
        fx++;
    }
}

void OLED_FillRoundRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t r, uint8_t color)
{
    if (w == 0 || h == 0) return;
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;

    OLED_FillRect(x + r, y, w - 2 * r, h, color);
    OLED_FillRect(x, y + r, r, h - 2 * r, color);
    OLED_FillRect(x + w - r, y + r, r, h - 2 * r, color);

    int16_t cx_l = x + r;
    int16_t cx_r = x + w - r - 1;
    int16_t cy_t = y + r;
    int16_t cy_b = y + h - r - 1;

    for (int16_t dy = 1; dy <= r; dy++) {
        for (int16_t dx = 1; dx <= r; dx++) {
            if (dx * dx + dy * dy <= r * r) {
                OLED_DrawPoint(cx_l - dx, cy_t - dy, color);
                OLED_DrawPoint(cx_r + dx, cy_t - dy, color);
                OLED_DrawPoint(cx_l - dx, cy_b + dy, color);
                OLED_DrawPoint(cx_r + dx, cy_b + dy, color);
            }
        }
    }
}

void OLED_ShowChar(uint8_t x, uint8_t y, char chr, uint8_t size, uint8_t color)
{
    if (chr < ' ' || chr > '~') chr = ' ';
    uint8_t c_idx = chr - ' ';

    if (size == 8) {
        for (uint8_t i = 0; i < 6; i++) {
            uint8_t temp = OLED_F6x8[c_idx][i];
            for (uint8_t j = 0; j < 8; j++) {
                if (temp & (1 << j)) {
                    OLED_DrawPoint(x + i, y + j, color);
                } else {
                    OLED_DrawPoint(x + i, y + j, !color);
                }
            }
        }
    } else {
        for (uint8_t i = 0; i < 8; i++) {
            uint8_t temp_up = OLED_F8x16[c_idx][i];
            uint8_t temp_dn = OLED_F8x16[c_idx][i + 8];
            for (uint8_t j = 0; j < 8; j++) {
                if (temp_up & (1 << j)) {
                    OLED_DrawPoint(x + i, y + j, color);
                } else {
                    OLED_DrawPoint(x + i, y + j, !color);
                }
                if (temp_dn & (1 << j)) {
                    OLED_DrawPoint(x + i, y + j + 8, color);
                } else {
                    OLED_DrawPoint(x + i, y + j + 8, !color);
                }
            }
        }
    }
}

void OLED_ShowString(uint8_t x, uint8_t y, const char *str, uint8_t size, uint8_t color)
{
    uint8_t step = (size == 8) ? 6 : 8;
    while (*str) {
        if (x + step > OLED_WIDTH) {
            x = 0;
            y += size;
        }
        if (y + size > OLED_HEIGHT) break;
        OLED_ShowChar(x, y, *str, size, color);
        x += step;
        str++;
    }
}

void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size, uint8_t color)
{
    uint8_t step = (size == 8) ? 6 : 8;
    char buf[12];
    for (int8_t i = len - 1; i >= 0; i--) {
        buf[i] = '0' + (num % 10);
        num /= 10;
    }
    for (uint8_t i = 0; i < len; i++) {
        OLED_ShowChar(x + i * step, y, buf[i], size, color);
    }
}

void OLED_ShowCharScale(uint8_t x, uint8_t y, char chr, uint8_t scale, uint8_t color)
{
    if (chr < ' ' || chr > '~') chr = ' ';
    uint8_t c_idx = chr - ' ';

    for (uint8_t i = 0; i < 8; i++) {
        uint8_t temp_up = OLED_F8x16[c_idx][i];
        uint8_t temp_dn = OLED_F8x16[c_idx][i + 8];
        for (uint8_t j = 0; j < 8; j++) {
            uint8_t pixel_up = (temp_up & (1 << j)) ? color : !color;
            uint8_t pixel_dn = (temp_dn & (1 << j)) ? color : !color;
            for (uint8_t sx = 0; sx < scale; sx++) {
                for (uint8_t sy = 0; sy < scale; sy++) {
                    OLED_DrawPoint(x + i * scale + sx, y + j * scale + sy, pixel_up);
                    OLED_DrawPoint(x + i * scale + sx, y + (j + 8) * scale + sy, pixel_dn);
                }
            }
        }
    }
}

void OLED_ShowStringScale(uint8_t x, uint8_t y, const char *str, uint8_t scale, uint8_t color)
{
    while (*str) {
        if (x + 8 * scale > OLED_WIDTH) break;
        OLED_ShowCharScale(x, y, *str, scale, color);
        x += 8 * scale;
        str++;
    }
}

void OLED_DrawBitmap(uint8_t x, uint8_t y, const uint8_t *bmp, uint8_t w, uint8_t h, uint8_t color)
{
    uint8_t byte_w = (w + 7) / 8;
    for (uint8_t j = 0; j < h; j++) {
        for (uint8_t i = 0; i < w; i++) {
            uint8_t byte_val = bmp[j * byte_w + (i / 8)];
            if (byte_val & (0x80 >> (i % 8))) {
                OLED_DrawPoint(x + i, y + j, color);
            }
        }
    }
}
