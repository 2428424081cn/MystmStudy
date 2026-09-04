#include "oled.h"
#include "oled_font.h"

/* 寄存器定义 */
#define RCC_BASE      (0x40021000UL)
#define RCC_APB2ENR   (*(volatile uint32_t *)(RCC_BASE + 0x18UL))

#define GPIOB_BASE    (0x40010C00UL)
#define GPIOB_CRL     (*(volatile uint32_t *)(GPIOB_BASE + 0x00UL))
#define GPIOB_BSRR    (*(volatile uint32_t *)(GPIOB_BASE + 0x10UL))

/* 引脚定义: SCL = PB6, SDA = PB7 */
#define SCL_H()  (GPIOB_BSRR = (1UL << 6))
#define SCL_L()  (GPIOB_BSRR = (1UL << (6 + 16)))
#define SDA_H()  (GPIOB_BSRR = (1UL << 7))
#define SDA_L()  (GPIOB_BSRR = (1UL << (7 + 16)))

/* 显存缓冲区: 128 x 64 位 = 8 页 x 128 字节 = 1024 字节 */
static uint8_t OLED_Gram[8][128];

/* 软件微延时周期 (可动态调节) */
static volatile uint8_t g_i2c_delay = 1;

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

static void I2C_Start(void)
{
    SDA_H();
    SCL_H();
    I2C_Delay();
    SDA_L();
    I2C_Delay();
    SCL_L();
    I2C_Delay();
}

static void I2C_Stop(void)
{
    SDA_L();
    SCL_H();
    I2C_Delay();
    SDA_H();
    I2C_Delay();
}

static void I2C_SendByte(uint8_t byte)
{
    for (uint8_t i = 0; i < 8; i++) {
        if (byte & 0x80) {
            SDA_H();
        } else {
            SDA_L();
        }
        I2C_Delay();
        SCL_H();
        I2C_Delay();
        SCL_L();
        I2C_Delay();
        byte <<= 1;
    }
    /* 第 9 个时钟周期 ACK */
    SDA_H();
    I2C_Delay();
    SCL_H();
    I2C_Delay();
    SCL_L();
    I2C_Delay();
}

static void OLED_WriteCmd(uint8_t cmd)
{
    I2C_Start();
    I2C_SendByte(0x78); /* I2C 7位地址 0x3C, 左移一位 0x78 */
    I2C_SendByte(0x00); /* 写入命令 */
    I2C_SendByte(cmd);
    I2C_Stop();
}

void OLED_Init(void)
{
    /* 1. 使能 GPIOB 时钟 */
    RCC_APB2ENR |= (1UL << 3);

    /* 2. 配置 PB6, PB7 为通用推挽输出 50MHz (0x3) */
    GPIOB_CRL &= ~(0xFFUL << 24);
    GPIOB_CRL |=  (0x33UL << 24);

    SCL_H();
    SDA_H();

    /* 上电延时稳定 */
    for (volatile int i = 0; i < 40000; i++) {
        __asm__ volatile ("nop");
    }

    /* 3. SSD1306 初始化序列 */
    OLED_WriteCmd(0xAE); /* 关闭显示 */
    OLED_WriteCmd(0xD5); /* 设置显示时钟分频比/振荡器频率 */
    OLED_WriteCmd(0xF0); /* 设置为最高振荡器频率 (最大化硬件刷新率) */
    OLED_WriteCmd(0xA8); /* 设置多路复用率 */
    OLED_WriteCmd(0x3F); /* 1/64 duty */
    OLED_WriteCmd(0xD3); /* 设置显示偏移 */
    OLED_WriteCmd(0x00);
    OLED_WriteCmd(0x40); /* 设置显示起始行 (0) */
    OLED_WriteCmd(0x8D); /* 电荷泵使能设置 */
    OLED_WriteCmd(0x14); /* 开启电荷泵 (关键! 否则屏幕不亮) */
    OLED_WriteCmd(0x20); /* 内存寻址模式 */
    OLED_WriteCmd(0x00); /* 水平寻址模式 (允许单次 I2C 突发连续传输 1024 字节) */
    OLED_WriteCmd(0xA1); /* 段重映射 (A0:左右反置, A1:正常) */
    OLED_WriteCmd(0xC8); /* COM 扫描方向 (C0:上下反置, C8:正常) */
    OLED_WriteCmd(0xDA); /* COM 引脚配置 */
    OLED_WriteCmd(0x12);
    OLED_WriteCmd(0x81); /* 对比度设置 */
    OLED_WriteCmd(0xCF);
    OLED_WriteCmd(0xD9); /* 预充电周期 */
    OLED_WriteCmd(0xF1);
    OLED_WriteCmd(0xDB); /* VCOMH 反选电平 */
    OLED_WriteCmd(0x40);
    OLED_WriteCmd(0xA4); /* 全局显示跟随 RAM */
    OLED_WriteCmd(0xA6); /* 正常显示 (A7 为反色) */
    OLED_WriteCmd(0xAF); /* 开启显示 */

    OLED_Clear();
    OLED_Update();
}

void OLED_Clear(void)
{
    uint32_t *p32 = (uint32_t *)OLED_Gram;
    for (uint16_t i = 0; i < 256; i++) {
        p32[i] = 0;
    }
}

void OLED_Update(void)
{
    /* 设置列地址范围: 0 ~ 127 */
    OLED_WriteCmd(0x21);
    OLED_WriteCmd(0x00);
    OLED_WriteCmd(0x7F);

    /* 设置页地址范围: 0 ~ 7 */
    OLED_WriteCmd(0x22);
    OLED_WriteCmd(0x00);
    OLED_WriteCmd(0x07);

    /* 单次 I2C 启动，连续突发传输整块 1024 字节显存 */
    I2C_Start();
    I2C_SendByte(0x78);
    I2C_SendByte(0x40);
    const uint8_t *ptr = (const uint8_t *)OLED_Gram;
    for (uint16_t i = 0; i < 1024; i++) {
        I2C_SendByte(*ptr++);
    }
    I2C_Stop();
}

void OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t color)
{
    if (x >= OLED_WIDTH || y >= OLED_HEIGHT) return;
    uint8_t page = y / 8;
    uint8_t bit  = y % 8;

    if (color) {
        OLED_Gram[page][x] |=  (1 << bit);
    } else {
        OLED_Gram[page][x] &= ~(1 << bit);
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

void OLED_ShowChar(uint8_t x, uint8_t y, char chr, uint8_t size, uint8_t color)
{
    if (chr < ' ' || chr > '~') chr = ' ';
    uint8_t c_idx = chr - ' ';

    if (size == 8) {
        /* 6x8 字符 */
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
        /* 8x16 字符 */
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
