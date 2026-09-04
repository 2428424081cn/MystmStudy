#include "key.h"

#define RCC_BASE      (0x40021000UL)
#define RCC_APB2ENR   (*(volatile uint32_t *)(RCC_BASE + 0x18UL))

#define GPIOA_BASE    (0x40010800UL)
#define GPIOA_CRL     (*(volatile uint32_t *)(GPIOA_BASE + 0x00UL))
#define GPIOA_IDR     (*(volatile uint32_t *)(GPIOA_BASE + 0x08UL))
#define GPIOA_ODR     (*(volatile uint32_t *)(GPIOA_BASE + 0x0CUL))

void KEY_Init(void)
{
    /* 1. 使能 GPIOA 外设时钟 */
    RCC_APB2ENR |= (1UL << 2);

    /* 2. 配置 PA0, PA1, PA2, PA3 为带上拉/下拉输入模式 (MODE=00, CNF=10 -> 0x8) */
    GPIOA_CRL &= ~(0x0000FFFFUL);
    GPIOA_CRL |=  (0x00008888UL);

    /* 3. 初始配置为上拉 (ODR=1) */
    GPIOA_ODR |=  (0x0000000FUL);
}

/**
 * @brief 获取 PA0~PA3 实时电平 (0 或 1)
 */
uint8_t KEY_GetPin(uint8_t index)
{
    if (index > 3) return 0;
    return (GPIOA_IDR & (1UL << index)) ? 1 : 0;
}

/**
 * @brief 自适应按键扫描函数
 *        每个按键独立跟踪状态，无论按键是接 GND (低电平有效)
 *        还是接 VCC (高电平有效)，都能精准触发！
 */
uint8_t KEY_Scan(void)
{
    /* 记录每个按键按下的状态 (0: 松开, 1: 已按下) */
    static uint8_t key_pressed[4] = {0, 0, 0, 0};
    /* 初始空闲电平，默认为 1 (上拉) */
    static uint8_t idle_level[4] = {1, 1, 1, 1};
    static uint8_t initialized = 0;

    uint32_t idr = GPIOA_IDR;

    /* 第一次运行时，将当前电平记录为基准空闲电平 */
    if (!initialized) {
        for (uint8_t i = 0; i < 4; i++) {
            idle_level[i] = (idr & (1UL << i)) ? 1 : 0;
        }
        initialized = 1;
    }

    for (uint8_t i = 0; i < 4; i++) {
        uint8_t curr = (idr & (1UL << i)) ? 1 : 0;

        /* 当电平偏离空闲电平时，即视为按下 */
        if (curr != idle_level[i]) {
            if (key_pressed[i] == 0) {
                key_pressed[i] = 1;
                return (i + 1); /* 返回 KEY_1 ~ KEY_4 */
            }
        } else {
            /* 回归空闲电平，标记松开 */
            key_pressed[i] = 0;
        }
    }

    return KEY_NONE;
}
