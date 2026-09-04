/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : 双屏冷脸萌 (Fumo ᗜ - ᗜ) + 极速赛车狂飙游戏 (Outrun Racing)
 *                   物理摆放:
 *                     左侧屏 (屏幕2): SCL=PB11, SDA=PB8  (OLED_SCREEN_L) -> 仪表盘 & Fumo副驾
 *                     右侧屏 (屏幕1): SCL=PB6,  SDA=PB7  (OLED_SCREEN_R) -> 极速赛道主视界
 *                   按 K4 可在 【1.双子萌】 / 【2.合体大脸】 / 【3.极速赛车】 自由切换！
 ******************************************************************************
 */

#include <stdint.h>
#include "oled.h"
#include "key.h"
#include "racing_assets.h"

/* 寄存器定义 */
#define RCC_BASE            (0x40021000UL)
#define RCC_CR              (*(volatile uint32_t *)(RCC_BASE + 0x00UL))
#define RCC_CFGR            (*(volatile uint32_t *)(RCC_BASE + 0x04UL))
#define RCC_APB2ENR         (*(volatile uint32_t *)(RCC_BASE + 0x18UL))

#define FLASH_BASE          (0x40022000UL)
#define FLASH_ACR           (*(volatile uint32_t *)(FLASH_BASE + 0x00UL))

#define GPIOC_BASE          (0x40011000UL)
#define GPIOC_CRH           (*(volatile uint32_t *)(GPIOC_BASE + 0x04UL))
#define GPIOC_BSRR          (*(volatile uint32_t *)(GPIOC_BASE + 0x10UL))
#define GPIOC_BRR           (*(volatile uint32_t *)(GPIOC_BASE + 0x14UL))
#define LED_PIN             (13UL)

/* SysTick */
#define SYSTICK_CSR         (*(volatile uint32_t *)0xE000E010UL)
#define SYSTICK_RVR         (*(volatile uint32_t *)0xE000E014UL)
#define SYSTICK_CVR         (*(volatile uint32_t *)0xE000E018UL)

static volatile uint32_t g_ms_ticks = 0;

static void LED_Init(void)
{
    RCC_APB2ENR |= (1UL << 4);
    GPIOC_CRH &= ~(0xFUL << 20);
    GPIOC_CRH |=  (0x2UL << 20);
    GPIOC_BSRR = (1UL << LED_PIN);
}

static void LED_Set(uint8_t state)
{
    if (state) GPIOC_BRR = (1UL << LED_PIN);
    else       GPIOC_BSRR = (1UL << LED_PIN);
}

static void SystemClock_Set64M_HSI(void)
{
    FLASH_ACR = 0x32;
    RCC_CFGR &= ~(0xFUL << 18);
    RCC_CFGR |=  (0xEUL << 18); /* 64MHz */
    RCC_CR |= (1UL << 24);
    while (!(RCC_CR & (1UL << 25)));
    RCC_CFGR &= ~(0x3UL);
    RCC_CFGR |=  (0x2UL);
    while ((RCC_CFGR & (0xCUL)) != (0x2UL << 2));
}

static void SysTick_Init_64M(void)
{
    SYSTICK_CSR = 0;
    SYSTICK_RVR = 64000 - 1; /* 1ms */
    SYSTICK_CVR = 0;
    SYSTICK_CSR = 7;
}

void SysTick_Handler(void)
{
    g_ms_ticks++;
}

static uint32_t g_seed = 1234567;
static uint16_t Rand(void)
{
    g_seed = g_seed * 1103515245UL + 12345UL;
    return (g_seed >> 16) & 0x7FFF;
}

/* =========================================================================
 * 顶层运行模式 (按 K4 循环切换)
 * ========================================================================= */
#define MODE_FUMO_TWIN    0   /* 1. 双子冷脸萌 (双屏各一只完整萌宠) */
#define MODE_FUMO_MERGE   1   /* 2. 跨屏合体大脸 (双屏拼合超宽冷脸) */
#define MODE_RACING       2   /* 3. 极速赛车游戏 (右屏赛道狂飙 + 左屏仪表与Fumo陪驾) */
#define MODE_TOTAL_COUNT  3

/* 10 种经典冷脸萌表情定义 */
#define MOOD_NORMAL     0
#define MOOD_SHY        1
#define MOOD_HAPPY      2
#define MOOD_POUT       3
#define MOOD_ANGRY      4
#define MOOD_FLIP_TABLE 5
#define MOOD_SLEEP      6
#define MOOD_PEACE      7
#define MOOD_KAWAII     8
#define MOOD_SMUG       9
#define MOOD_TOTAL      10

#define MOOD_HOLD_BASE_MS   8000
#define MOOD_HOLD_RAND_MS   6000

static const char *MOOD_NAMES[MOOD_TOTAL] = {
    "1.NORMAL  [-_-]",
    "2.SHY     ,,--,,",
    "3.HAPPY   [^v^]",
    "4.POUT    [>_<]",
    "5.ANGRY   [!-!]",
    "6.FLIP    [_/]",
    "7.SLEEP   [zZZ]",
    "8.PEACE   [^o^]v",
    "9.KAWAII  [:3]",
    "10.SMUG   [~_~]"
};

/* 纯空心 ᗜ 边框点阵 */
static const uint32_t FUMO_EYE_BORDER[18] = {
    0x003F00, 0x00FFC0, 0x038070, 0x070038, 0x0C000C, 0x180006,
    0x180006, 0x300003, 0x300003, 0x300003, 0x300003, 0x300003,
    0x300003, 0x300003, 0x300003, 0x300003, 0x3FFFFF, 0x3FFFFF
};

static void Draw_Fumo_Eye(uint8_t x0, uint8_t y0, uint8_t blink)
{
    if (blink >= 16) {
        OLED_DrawLine(x0, y0 + 16, x0 + 21, y0 + 16, OLED_COLOR_WHITE);
        OLED_DrawLine(x0, y0 + 17, x0 + 21, y0 + 17, OLED_COLOR_WHITE);
        return;
    }
    for (uint8_t r = 0; r < 18; r++) {
        if (r < blink) continue;
        uint32_t row_bits = FUMO_EYE_BORDER[r];
        if (blink > 0 && r == blink) row_bits = 0x3FFFFF;
        for (uint8_t c = 0; c < 22; c++) {
            if (row_bits & (1UL << (21 - c))) {
                OLED_DrawPoint(x0 + c, y0 + r, OLED_COLOR_WHITE);
            }
        }
    }
}

static void Draw_Anger_Mark(uint8_t x, uint8_t y)
{
    OLED_DrawLine(x - 3, y - 1, x + 3, y - 1, OLED_COLOR_WHITE);
    OLED_DrawLine(x - 3, y + 1, x + 3, y + 1, OLED_COLOR_WHITE);
    OLED_DrawLine(x - 1, y - 3, x - 1, y + 3, OLED_COLOR_WHITE);
    OLED_DrawLine(x + 1, y - 3, x + 1, y + 3, OLED_COLOR_WHITE);
}

static void Draw_Blush(uint8_t x, uint8_t y)
{
    OLED_DrawLine(x,     y + 4, x + 2, y, OLED_COLOR_WHITE);
    OLED_DrawLine(x + 4, y + 4, x + 6, y, OLED_COLOR_WHITE);
}

static void Draw_Flying_Table(uint8_t x, uint8_t y, uint8_t angle)
{
    OLED_FillRect(x, y, 22, 4, OLED_COLOR_WHITE);
    if ((angle % 2) == 0) {
        OLED_DrawLine(x + 2,  y - 1, x + 2,  y - 12, OLED_COLOR_WHITE);
        OLED_DrawLine(x + 3,  y - 1, x + 3,  y - 12, OLED_COLOR_WHITE);
        OLED_DrawLine(x + 18, y - 1, x + 18, y - 12, OLED_COLOR_WHITE);
        OLED_DrawLine(x + 19, y - 1, x + 19, y - 12, OLED_COLOR_WHITE);
    } else {
        OLED_DrawLine(x + 2,  y + 4, x + 2,  y + 14, OLED_COLOR_WHITE);
        OLED_DrawLine(x + 3,  y + 4, x + 3,  y + 14, OLED_COLOR_WHITE);
        OLED_DrawLine(x + 18, y + 4, x + 18, y + 14, OLED_COLOR_WHITE);
        OLED_DrawLine(x + 19, y + 4, x + 19, y + 14, OLED_COLOR_WHITE);
    }
}

static void Draw_Peace_Hand(uint8_t x, uint8_t y, uint8_t mirror)
{
    if (!mirror) {
        OLED_FillCircle(x + 5, y + 8, 3, OLED_COLOR_WHITE);
        OLED_DrawLine(x + 3, y + 6, x + 1, y, OLED_COLOR_WHITE);
        OLED_DrawLine(x + 4, y + 6, x + 2, y, OLED_COLOR_WHITE);
        OLED_DrawLine(x + 6, y + 6, x + 8, y, OLED_COLOR_WHITE);
        OLED_DrawLine(x + 7, y + 6, x + 9, y, OLED_COLOR_WHITE);
    } else {
        OLED_FillCircle(x + 5, y + 8, 3, OLED_COLOR_WHITE);
        OLED_DrawLine(x + 6, y + 6, x + 8, y, OLED_COLOR_WHITE);
        OLED_DrawLine(x + 5, y + 6, x + 7, y, OLED_COLOR_WHITE);
        OLED_DrawLine(x + 3, y + 6, x + 1, y, OLED_COLOR_WHITE);
        OLED_DrawLine(x + 2, y + 6, x,     y, OLED_COLOR_WHITE);
    }
}

/* =========================================================================
 * 冷脸萌绘制逻辑 (Twin & Merge 模式)
 * ========================================================================= */
static void Draw_Single_Fumo_Face(uint8_t screen, uint8_t current_mood,
                                  int8_t look_x, int8_t look_y,
                                  uint8_t is_blinking, uint8_t blink_depth,
                                  uint8_t smug_phase, uint32_t frame_tick,
                                  uint8_t auto_ai_mode)
{
    OLED_SelectScreen(screen);
    OLED_FillRect(0, 0, 128, 9, OLED_COLOR_WHITE);
    OLED_ShowString(4, 1, MOOD_NAMES[current_mood], 8, OLED_COLOR_BLACK);
    if (screen == OLED_SCREEN_L) {
        OLED_ShowString(96, 1, "FUMO-L", 8, OLED_COLOR_BLACK);
    } else {
        if (auto_ai_mode) OLED_ShowString(96, 1, "AUTO", 8, OLED_COLOR_BLACK);
        else              OLED_ShowString(96, 1, "MANU", 8, OLED_COLOR_BLACK);
    }

    const uint8_t left_eye_x  = 24;
    const uint8_t right_eye_x = 82;
    const uint8_t eye_y       = 14;
    const uint8_t eye_w       = 22;

    if (current_mood == MOOD_SLEEP) {
        OLED_DrawLine(left_eye_x + 2, eye_y + 14, left_eye_x + eye_w - 2, eye_y + 14, OLED_COLOR_WHITE);
        OLED_DrawLine(left_eye_x + 2, eye_y + 15, left_eye_x + eye_w - 2, eye_y + 15, OLED_COLOR_WHITE);
        OLED_DrawLine(right_eye_x + 2, eye_y + 14, right_eye_x + eye_w - 2, eye_y + 14, OLED_COLOR_WHITE);
        OLED_DrawLine(right_eye_x + 2, eye_y + 15, right_eye_x + eye_w - 2, eye_y + 15, OLED_COLOR_WHITE);
        uint8_t zy = (frame_tick / 4) % 20;
        OLED_ShowString(104, 28 - zy, "z", 8, OLED_COLOR_WHITE);
        if (zy > 6) OLED_ShowString(112, 28 - zy - 6, "Z", 8, OLED_COLOR_WHITE);
    } else if (current_mood == MOOD_FLIP_TABLE) {
        OLED_DrawLine(left_eye_x + 4, eye_y + 4, left_eye_x + 16, eye_y + 10, OLED_COLOR_WHITE);
        OLED_DrawLine(left_eye_x + 16, eye_y + 10, left_eye_x + 4, eye_y + 16, OLED_COLOR_WHITE);
        OLED_DrawLine(right_eye_x + 16, eye_y + 4, right_eye_x + 4, eye_y + 10, OLED_COLOR_WHITE);
        OLED_DrawLine(right_eye_x + 4, eye_y + 10, right_eye_x + 16, eye_y + 16, OLED_COLOR_WHITE);
        OLED_DrawLine(left_eye_x - 10, eye_y + 18, left_eye_x - 4, eye_y + 8, OLED_COLOR_WHITE);
        OLED_DrawLine(right_eye_x + eye_w + 4, eye_y + 18, right_eye_x + eye_w + 10, eye_y + 8, OLED_COLOR_WHITE);
        uint8_t fly_x = 94 + (frame_tick % 8);
        uint8_t fly_y = 12 + ((frame_tick / 4) % 6);
        Draw_Flying_Table(fly_x, fly_y, frame_tick / 8);
    } else {
        Draw_Fumo_Eye(left_eye_x + look_x, eye_y + look_y, is_blinking ? blink_depth : 0);
        Draw_Fumo_Eye(right_eye_x + look_x, eye_y + look_y, is_blinking ? blink_depth : 0);
        if (current_mood == MOOD_ANGRY) {
            OLED_DrawLine(left_eye_x + look_x + 2, eye_y - 2, left_eye_x + look_x + eye_w, eye_y + 2, OLED_COLOR_WHITE);
            OLED_DrawLine(right_eye_x + look_x, eye_y + 2, right_eye_x + look_x + eye_w - 2, eye_y - 2, OLED_COLOR_WHITE);
            Draw_Anger_Mark(right_eye_x + eye_w + 8, eye_y - 1);
        }
    }

    const uint8_t mouth_x = 64 + look_x;
    const uint8_t mouth_y = 33 + look_y;

    switch (current_mood) {
        case MOOD_NORMAL:
        case MOOD_ANGRY:
            OLED_DrawLine(mouth_x - 5, mouth_y, mouth_x + 5, mouth_y, OLED_COLOR_WHITE);
            OLED_DrawLine(mouth_x - 5, mouth_y + 1, mouth_x + 5, mouth_y + 1, OLED_COLOR_WHITE);
            break;
        case MOOD_SHY:
            OLED_DrawLine(mouth_x - 4, mouth_y + 1, mouth_x + 4, mouth_y + 1, OLED_COLOR_WHITE);
            Draw_Blush(left_eye_x + look_x - 12, mouth_y - 4);
            Draw_Blush(right_eye_x + look_x + eye_w + 4, mouth_y - 4);
            break;
        case MOOD_HAPPY:
            OLED_DrawLine(mouth_x - 5, mouth_y - 1, mouth_x - 3, mouth_y + 2, OLED_COLOR_WHITE);
            OLED_DrawLine(mouth_x - 3, mouth_y + 2, mouth_x + 3, mouth_y + 2, OLED_COLOR_WHITE);
            OLED_DrawLine(mouth_x + 3, mouth_y + 2, mouth_x + 5, mouth_y - 1, OLED_COLOR_WHITE);
            Draw_Blush(left_eye_x + look_x - 10, mouth_y - 2);
            Draw_Blush(right_eye_x + look_x + eye_w + 3, mouth_y - 2);
            break;
        case MOOD_POUT:
            OLED_DrawLine(mouth_x - 4, mouth_y + 3, mouth_x, mouth_y - 1, OLED_COLOR_WHITE);
            OLED_DrawLine(mouth_x, mouth_y - 1, mouth_x + 4, mouth_y + 3, OLED_COLOR_WHITE);
            OLED_DrawCircle(left_eye_x + look_x - 8, eye_y + 4, 3, OLED_COLOR_WHITE);
            break;
        case MOOD_FLIP_TABLE:
            OLED_DrawLine(mouth_x - 6, mouth_y + 3, mouth_x, mouth_y - 2, OLED_COLOR_WHITE);
            OLED_DrawLine(mouth_x, mouth_y - 2, mouth_x + 6, mouth_y + 3, OLED_COLOR_WHITE);
            break;
        case MOOD_SLEEP:
            OLED_DrawLine(mouth_x - 4, mouth_y + 2, mouth_x + 4, mouth_y + 2, OLED_COLOR_WHITE);
            break;
        case MOOD_PEACE:
            OLED_DrawLine(mouth_x - 4, mouth_y + 1, mouth_x + 4, mouth_y + 1, OLED_COLOR_WHITE);
            OLED_DrawPoint(mouth_x + 5, mouth_y, OLED_COLOR_WHITE);
            if (screen == OLED_SCREEN_L) {
                Draw_Peace_Hand(right_eye_x + look_x + eye_w + 3, mouth_y - 10, 0);
            } else {
                Draw_Peace_Hand(left_eye_x + look_x - 14, mouth_y - 10, 1);
            }
            break;
        case MOOD_KAWAII:
            OLED_DrawLine(mouth_x - 5, mouth_y + 1, mouth_x - 3, mouth_y - 1, OLED_COLOR_WHITE);
            OLED_DrawLine(mouth_x - 3, mouth_y - 1, mouth_x, mouth_y + 1, OLED_COLOR_WHITE);
            OLED_DrawLine(mouth_x, mouth_y + 1, mouth_x + 3, mouth_y - 1, OLED_COLOR_WHITE);
            OLED_DrawLine(mouth_x + 3, mouth_y - 1, mouth_x + 5, mouth_y + 1, OLED_COLOR_WHITE);
            Draw_Blush(left_eye_x + look_x - 10, mouth_y - 2);
            Draw_Blush(right_eye_x + look_x + eye_w + 3, mouth_y - 2);
            break;
        case MOOD_SMUG:
            if (smug_phase == 0) {
                OLED_DrawLine(mouth_x - 5, mouth_y, mouth_x + 5, mouth_y, OLED_COLOR_WHITE);
            } else {
                OLED_DrawLine(mouth_x - 4, mouth_y + 2, mouth_x + 1, mouth_y + 2, OLED_COLOR_WHITE);
                OLED_DrawLine(mouth_x + 1, mouth_y + 2, mouth_x + 6, mouth_y - 1, OLED_COLOR_WHITE);
                Draw_Blush(right_eye_x + look_x + eye_w + 4, mouth_y - 2);
            }
            OLED_ShowChar(6, eye_y + 4, '(', 16, OLED_COLOR_WHITE);
            OLED_ShowChar(114, eye_y + 4, ')', 16, OLED_COLOR_WHITE);
            break;
    }

    OLED_DrawLine(0, 52, 127, 52, OLED_COLOR_WHITE);
    if (screen == OLED_SCREEN_L) {
        OLED_ShowString(2, 54, "K1/2:Mood  K3:Poke", 8, OLED_COLOR_WHITE);
    } else {
        OLED_ShowString(2, 54, "K4:Mode [TWIN] 80FPS", 8, OLED_COLOR_WHITE);
    }
}

static void Draw_Merged_Fumo_Face(uint8_t screen, uint8_t current_mood,
                                  int8_t look_x, int8_t look_y,
                                  uint8_t is_blinking, uint8_t blink_depth,
                                  uint8_t smug_phase, uint32_t frame_tick)
{
    OLED_SelectScreen(screen);
    OLED_FillRect(0, 0, 128, 9, OLED_COLOR_WHITE);
    OLED_ShowString(4, 1, MOOD_NAMES[current_mood], 8, OLED_COLOR_BLACK);
    if (screen == OLED_SCREEN_L) OLED_ShowString(96, 1, "LEFT", 8, OLED_COLOR_BLACK);
    else                         OLED_ShowString(96, 1, "RIGHT", 8, OLED_COLOR_BLACK);

    uint8_t eye_x = (screen == OLED_SCREEN_L) ? (68 + look_x) : (38 + look_x);
    uint8_t eye_y = 18 + look_y;

    if (current_mood == MOOD_SLEEP) {
        OLED_DrawLine(eye_x, eye_y + 16, eye_x + 22, eye_y + 16, OLED_COLOR_WHITE);
        OLED_DrawLine(eye_x, eye_y + 17, eye_x + 22, eye_y + 17, OLED_COLOR_WHITE);
        if (screen == OLED_SCREEN_R) {
            uint8_t zy = (frame_tick / 4) % 20;
            OLED_ShowString(104, 30 - zy, "z", 8, OLED_COLOR_WHITE);
            if (zy > 6) OLED_ShowString(112, 30 - zy - 6, "Z", 8, OLED_COLOR_WHITE);
        }
    } else if (current_mood == MOOD_FLIP_TABLE) {
        if (screen == OLED_SCREEN_L) {
            OLED_DrawLine(eye_x + 2, eye_y + 4, eye_x + 18, eye_y + 11, OLED_COLOR_WHITE);
            OLED_DrawLine(eye_x + 18, eye_y + 11, eye_x + 2, eye_y + 18, OLED_COLOR_WHITE);
            OLED_DrawLine(20, eye_y + 18, 30, eye_y + 6, OLED_COLOR_WHITE);
        } else {
            OLED_DrawLine(eye_x + 18, eye_y + 4, eye_x + 2, eye_y + 11, OLED_COLOR_WHITE);
            OLED_DrawLine(eye_x + 2, eye_y + 11, eye_x + 18, eye_y + 18, OLED_COLOR_WHITE);
            OLED_DrawLine(eye_x + 26, eye_y + 18, eye_x + 36, eye_y + 6, OLED_COLOR_WHITE);
            Draw_Flying_Table(90, 14, frame_tick / 8);
        }
    } else {
        Draw_Fumo_Eye(eye_x, eye_y, is_blinking ? blink_depth : 0);
        if (current_mood == MOOD_ANGRY) {
            if (screen == OLED_SCREEN_L) {
                OLED_DrawLine(eye_x, eye_y - 2, eye_x + 22, eye_y + 2, OLED_COLOR_WHITE);
            } else {
                OLED_DrawLine(eye_x, eye_y + 2, eye_x + 22, eye_y - 2, OLED_COLOR_WHITE);
                Draw_Anger_Mark(100, eye_y);
            }
        }
    }

    uint8_t mouth_y = 38 + look_y;
    if (screen == OLED_SCREEN_L) {
        OLED_DrawLine(118, mouth_y, 127, mouth_y, OLED_COLOR_WHITE);
        OLED_DrawLine(118, mouth_y + 1, 127, mouth_y + 1, OLED_COLOR_WHITE);
        if (current_mood == MOOD_SHY || current_mood == MOOD_KAWAII || current_mood == MOOD_HAPPY) {
            Draw_Blush(24, mouth_y - 4);
        }
        if (current_mood == MOOD_SMUG) {
            OLED_ShowChar(10, eye_y + 2, '(', 16, OLED_COLOR_WHITE);
        }
    } else {
        OLED_DrawLine(0, mouth_y, 10, mouth_y, OLED_COLOR_WHITE);
        OLED_DrawLine(0, mouth_y + 1, 10, mouth_y + 1, OLED_COLOR_WHITE);
        if (current_mood == MOOD_SHY || current_mood == MOOD_KAWAII || current_mood == MOOD_HAPPY) {
            Draw_Blush(96, mouth_y - 4);
        }
        if (current_mood == MOOD_PEACE) {
            Draw_Peace_Hand(80, mouth_y - 10, 0);
        }
        if (current_mood == MOOD_SMUG) {
            OLED_ShowChar(110, eye_y + 2, ')', 16, OLED_COLOR_WHITE);
        }
    }

    OLED_DrawLine(0, 52, 127, 52, OLED_COLOR_WHITE);
    if (screen == OLED_SCREEN_L) {
        OLED_ShowString(2, 54, "K1/2:Mood  K3:Poke", 8, OLED_COLOR_WHITE);
    } else {
        OLED_ShowString(2, 54, "K4:Mode [MERGE] 80FPS", 8, OLED_COLOR_WHITE);
    }
}

/* =========================================================================
 * 双屏赛车引擎核心状态与数据 (Racing Game Engine)
 * ========================================================================= */
typedef struct {
    uint8_t active;
    uint8_t type;     /* 0: 轿车, 1: 大货车 */
    uint8_t lane;     /* 0: 左车道, 1: 中车道, 2: 右车道 */
    int16_t y;
    int16_t speed;
} TrafficCar;

typedef struct {
    uint8_t active;
    uint8_t type;     /* 0: 金币 (+100分), 1: 闪电氮气罐 (+35% 氮气) */
    uint8_t lane;
    int16_t y;
} PropItem;

/* 赛车全局运行状态 */
static uint8_t  g_player_lane    = 1;   /* 当前车道: 0(左), 1(中), 2(右) */
static int16_t  g_player_x       = 64;  /* 玩家平滑 X 坐标 */
static int16_t  g_player_speed   = 140; /* 玩家车速 (km/h) */
static uint8_t  g_nitro_level    = 60;  /* 氮气槽百分比 (0~100) */
static uint8_t  g_is_boosting    = 0;   /* 是否正处于氮气极速狂飙中 */
static uint8_t  g_is_crashed     = 0;   /* 是否发生碰撞 */
static uint8_t  g_crash_tick     = 0;   /* 碰撞翻车爆炸动画步进 */
static uint32_t g_race_score     = 0;   /* 竞速得分 */
static uint32_t g_race_dist_m    = 0;   /* 行驶总米数 */
static uint32_t g_high_score     = 0;   /* 历史最高分 */
static uint8_t  g_road_scroll    = 0;   /* 道路标线滚动偏移 */

#define ENEMY_MAX 3
static TrafficCar g_traffic[ENEMY_MAX];
static PropItem   g_item;

/* 3 车道中心 X 坐标 */
static const uint8_t LANE_X_POS[3] = { 32, 64, 96 };

static void Racing_Reset(void)
{
    g_player_lane   = 1;
    g_player_x      = 64;
    g_player_speed  = 130;
    g_nitro_level   = 70;
    g_is_boosting   = 0;
    g_is_crashed    = 0;
    g_crash_tick    = 0;
    g_race_score    = 0;
    g_race_dist_m   = 0;

    /* 初始化敌车池 */
    g_traffic[0].active = 1;
    g_traffic[0].type   = 0;
    g_traffic[0].lane   = 0;
    g_traffic[0].y      = -20;
    g_traffic[0].speed  = 80;

    g_traffic[1].active = 1;
    g_traffic[1].type   = 1;
    g_traffic[1].lane   = 2;
    g_traffic[1].y      = -70;
    g_traffic[1].speed  = 70;

    g_traffic[2].active = 0;

    g_item.active = 1;
    g_item.type   = 0; /* 金币 */
    g_item.lane   = 1;
    g_item.y      = -120;
}

/**
 * @brief 赛车物理与游戏逻辑步进
 */
static void Racing_Update(uint8_t key)
{
    if (g_is_crashed) {
        g_crash_tick++;
        g_player_speed = 0;
        /* 撞车后，按 K1, K2 或 K3 重新开始极速挑战 */
        if (key == KEY_1 || key == KEY_2 || key == KEY_3) {
            Racing_Reset();
        }
        return;
    }

    /* 1. 按键响应与车道转向 */
    if (key == KEY_1) {
        /* 向左变道 */
        if (g_player_lane > 0) g_player_lane--;
    } else if (key == KEY_2) {
        /* 向右变道 */
        if (g_player_lane < 2) g_player_lane++;
    } else if (key == KEY_3) {
        /* 氮气加速冲刺 */
        if (g_nitro_level >= 15 && !g_is_boosting) {
            g_is_boosting = 1;
        }
    }

    /* 玩家赛车 X 轴平滑微调转向 */
    int16_t target_x = LANE_X_POS[g_player_lane];
    if (g_player_x < target_x) g_player_x += 4;
    else if (g_player_x > target_x) g_player_x -= 4;

    /* 2. 车速与氮气动力学计算 */
    if (g_is_boosting) {
        if (g_player_speed < 235) g_player_speed += 5;
        if (g_nitro_level > 0) {
            g_nitro_level--;
        } else {
            g_is_boosting = 0;
        }
    } else {
        /* 常规巡航加速 */
        if (g_player_speed < 175) g_player_speed += 1;
        else if (g_player_speed > 175) g_player_speed -= 2;
        /* 自动回充少量氮气 */
        if ((g_ms_ticks % 15) == 0 && g_nitro_level < 100) {
            g_nitro_level++;
        }
    }

    /* 滚动道路 */
    g_road_scroll += (g_player_speed / 16);

    /* 行驶距离与基础得分累加 */
    g_race_dist_m += (g_player_speed / 20);
    g_race_score  += (g_player_speed / 40);
    if (g_race_score > g_high_score) {
        g_high_score = g_race_score;
    }

    /* 3. 敌方车流运动与刷新 */
    for (uint8_t i = 0; i < ENEMY_MAX; i++) {
        if (!g_traffic[i].active) {
            /* 随机重新激活敌车 */
            if ((Rand() % 30) == 0) {
                g_traffic[i].active = 1;
                g_traffic[i].type   = (Rand() % 3 == 0) ? 1 : 0;
                g_traffic[i].lane   = Rand() % 3;
                g_traffic[i].y      = -(28 + (Rand() % 40));
                g_traffic[i].speed  = (g_traffic[i].type == 1) ? (65 + Rand() % 20) : (80 + Rand() % 35);
            }
            continue;
        }

        /* 相对下落速度 (玩家时速减敌车时速) */
        int16_t delta_v = g_player_speed - g_traffic[i].speed;
        if (delta_v < 15) delta_v = 15;
        g_traffic[i].y += (delta_v / 18);

        /* 超出屏幕下方，重置到上方 */
        if (g_traffic[i].y > 68) {
            g_traffic[i].active = 0;
            g_race_score += 50; /* 成功超车奖励得分 */
        }

        /* 碰撞检测 (AABB 边界盒) */
        int16_t ex = LANE_X_POS[g_traffic[i].lane];
        int16_t ey = g_traffic[i].y;
        int16_t ew = (g_traffic[i].type == 1) ? 14 : 12;
        int16_t eh = (g_traffic[i].type == 1) ? 26 : 18;

        /* 玩家赛车在 Y=40, 尺寸 14x20 */
        if ((g_player_x + 6 >= ex - ew/2) && (g_player_x - 6 <= ex + ew/2) &&
            (40 + 18 >= ey) && (40 <= ey + eh)) {
            /* 发生剧烈追尾碰撞！*/
            g_is_crashed = 1;
            g_crash_tick = 0;
        }
    }

    /* 4. 拾取道具运动与检测 */
    if (g_item.active) {
        g_item.y += (g_player_speed / 20);
        if (g_item.y > 66) {
            g_item.active = 0;
        }

        /* 玩家拾取碰撞 */
        int16_t ix = LANE_X_POS[g_item.lane];
        if ((g_player_x + 7 >= ix - 4) && (g_player_x - 7 <= ix + 4) &&
            (40 + 18 >= g_item.y) && (40 <= g_item.y + 8)) {
            if (g_item.type == 0) {
                /* 金币奖励 */
                g_race_score += 200;
            } else {
                /* 充沛氮气瓶 */
                if (g_nitro_level + 35 > 100) g_nitro_level = 100;
                else g_nitro_level += 35;
            }
            g_item.active = 0;
        }
    } else {
        /* 随机刷新新道具 */
        if ((Rand() % 80) == 0) {
            g_item.active = 1;
            g_item.type   = (Rand() % 2);
            g_item.lane   = Rand() % 3;
            g_item.y      = -20;
        }
    }

    /* LED 随车速与氮气闪烁联动 */
    if (g_is_boosting) {
        LED_Set((g_ms_ticks % 100) < 50);
    } else {
        LED_Set(0);
    }
}

/**
 * @brief 渲染右屏：高速公路主赛道视界
 */
static void Racing_Render_Track(void)
{
    OLED_SelectScreen(OLED_SCREEN_R);

    /* --- 1. 道路两旁草地与赛道护栏边缘 --- */
    /* 左侧护栏 (X=14) 与 右侧护栏 (X=114) */
    OLED_DrawLine(14, 0, 14, 63, OLED_COLOR_WHITE);
    OLED_DrawLine(114, 0, 114, 63, OLED_COLOR_WHITE);

    /* 红白路肩交替斑马线 (路缘石高速流动) */
    for (uint8_t y = 0; y < 64; y += 8) {
        uint8_t py = (y + g_road_scroll) % 64;
        if (((y / 8) % 2) == 0) {
            OLED_FillRect(11, py, 3, 4, OLED_COLOR_WHITE);
            OLED_FillRect(115, py, 3, 4, OLED_COLOR_WHITE);
        }
    }

    /* 车道虚线 (X=48, X=80) 飞速向后流动 */
    for (int16_t y = -8; y < 72; y += 12) {
        int16_t py = y + (g_road_scroll % 12);
        if (py >= 0 && py + 5 < 64) {
            OLED_DrawLine(48, py, 48, py + 5, OLED_COLOR_WHITE);
            OLED_DrawLine(80, py, 80, py + 5, OLED_COLOR_WHITE);
        }
    }

    /* --- 2. 绘制掉落的道具 (金币 / 氮气瓶) --- */
    if (g_item.active && g_item.y >= -10 && g_item.y < 64) {
        int16_t ix = LANE_X_POS[g_item.lane] - 4;
        if (g_item.type == 0) {
            OLED_DrawBitmap(ix, g_item.y, SPRITE_COIN, 8, 8, OLED_COLOR_WHITE);
        } else {
            OLED_DrawBitmap(ix, g_item.y, SPRITE_NITRO_CAN, 8, 10, OLED_COLOR_WHITE);
        }
    }

    /* --- 3. 绘制敌方车流 --- */
    for (uint8_t i = 0; i < ENEMY_MAX; i++) {
        if (!g_traffic[i].active) continue;
        if (g_traffic[i].y >= -26 && g_traffic[i].y < 64) {
            int16_t ex = LANE_X_POS[g_traffic[i].lane];
            if (g_traffic[i].type == 0) {
                OLED_DrawBitmap(ex - 6, g_traffic[i].y, SPRITE_ENEMY_SEDAN, 12, 18, OLED_COLOR_WHITE);
            } else {
                OLED_DrawBitmap(ex - 7, g_traffic[i].y, SPRITE_ENEMY_TRUCK, 14, 26, OLED_COLOR_WHITE);
            }
        }
    }

    /* --- 4. 绘制玩家超跑赛车 (位于底部 Y=40) --- */
    if (!g_is_crashed) {
        OLED_DrawBitmap(g_player_x - 7, 40, SPRITE_PLAYER_CAR, 14, 20, OLED_COLOR_WHITE);

        /* 氮气喷火特效 (车尾喷出烈焰) */
        if (g_is_boosting) {
            uint8_t fire_offset = (g_ms_ticks / 30) % 2;
            OLED_DrawBitmap(g_player_x - 5, 59 + fire_offset, SPRITE_NITRO_FIRE, 10, 8, OLED_COLOR_WHITE);
        }
    } else {
        /* 撞车翻车爆炸动画 */
        if (g_crash_tick < 15) {
            OLED_DrawBitmap(g_player_x - 8, 38, SPRITE_BOOM1, 16, 16, OLED_COLOR_WHITE);
        } else {
            OLED_DrawBitmap(g_player_x - 8, 38, SPRITE_BOOM2, 16, 16, OLED_COLOR_WHITE);
        }

        /* 游戏结束警报提示胶囊 */
        OLED_FillRect(20, 18, 88, 22, OLED_COLOR_BLACK);
        OLED_DrawRect(20, 18, 88, 22, OLED_COLOR_WHITE);
        OLED_ShowString(26, 21, "CRASH! OVER", 8, OLED_COLOR_WHITE);
        OLED_ShowString(24, 30, "K1/2/3: Restart", 8, OLED_COLOR_WHITE);
    }
}

/**
 * @brief 渲染左屏：科技仪表盘 + Fumo 副驾驶灵魂联动
 */
static void Racing_Render_Dashboard(void)
{
    OLED_SelectScreen(OLED_SCREEN_L);

    /* --- 1. 顶部标题 --- */
    OLED_FillRect(0, 0, 128, 9, OLED_COLOR_WHITE);
    OLED_ShowString(4, 1, "RACING DASHBOARD", 8, OLED_COLOR_BLACK);
    OLED_ShowString(98, 1, "80FPS", 8, OLED_COLOR_BLACK);

    /* --- 2. 速度与仪表数据展示 --- */
    /* 速度大字显示 */
    OLED_ShowNum(4, 12, g_player_speed, 3, 16, OLED_COLOR_WHITE);
    OLED_ShowString(30, 18, "KM/H", 8, OLED_COLOR_WHITE);

    /* 档位判断与显示 */
    uint8_t gear = 1;
    if (g_player_speed >= 195)      gear = 6;
    else if (g_player_speed >= 155) gear = 5;
    else if (g_player_speed >= 115) gear = 4;
    else if (g_player_speed >= 75)  gear = 3;
    else if (g_player_speed >= 35)  gear = 2;
    if (g_is_crashed) gear = 0;

    OLED_ShowString(68, 12, "G:", 8, OLED_COLOR_WHITE);
    if (gear == 0) OLED_ShowChar(80, 12, 'R', 8, OLED_COLOR_WHITE);
    else           OLED_ShowChar(80, 12, '0' + gear, 8, OLED_COLOR_WHITE);

    /* 得分展示 */
    OLED_ShowString(68, 21, "S:", 8, OLED_COLOR_WHITE);
    OLED_ShowNum(80, 21, g_race_score, 6, 8, OLED_COLOR_WHITE);

    /* 氮气能量进度条 (NITRO) */
    OLED_ShowString(4, 30, "NITRO", 8, OLED_COLOR_WHITE);
    OLED_DrawRect(36, 30, 88, 7, OLED_COLOR_WHITE);
    uint8_t bar_w = (g_nitro_level * 84) / 100;
    if (bar_w > 84) bar_w = 84;
    if (bar_w > 0) {
        OLED_FillRect(38, 32, bar_w, 3, OLED_COLOR_WHITE);
    }

    /* 中部分割线 */
    OLED_DrawLine(0, 39, 127, 39, OLED_COLOR_WHITE);

    /* --- 3. Fumo 冷脸萌副驾驶联动面孔 --- */
    const uint8_t fumo_x = 44;
    const uint8_t fumo_y = 42;

    if (g_is_crashed) {
        /* 撞车翻车: Fumo 愤怒掀桌爆发 ( ╯－︿－)╯╧╧ */
        OLED_ShowString(6, 42, "FLIP!", 8, OLED_COLOR_WHITE);
        /* 左眼 > */
        OLED_DrawLine(fumo_x + 4, fumo_y + 2, fumo_x + 10, fumo_y + 5, OLED_COLOR_WHITE);
        OLED_DrawLine(fumo_x + 10, fumo_y + 5, fumo_x + 4, fumo_y + 8, OLED_COLOR_WHITE);
        /* 右眼 < */
        OLED_DrawLine(fumo_x + 22, fumo_y + 2, fumo_x + 16, fumo_y + 5, OLED_COLOR_WHITE);
        OLED_DrawLine(fumo_x + 16, fumo_y + 5, fumo_x + 22, fumo_y + 8, OLED_COLOR_WHITE);
        /* 嘴巴 －︿－ */
        OLED_DrawLine(fumo_x + 11, fumo_y + 7, fumo_x + 13, fumo_y + 5, OLED_COLOR_WHITE);
        OLED_DrawLine(fumo_x + 13, fumo_y + 5, fumo_x + 15, fumo_y + 7, OLED_COLOR_WHITE);
        /* 飞起的微型桌子 */
        Draw_Flying_Table(fumo_x + 28, fumo_y + 2, (g_crash_tick / 4));
        /* 怒筋 💢 */
        Draw_Anger_Mark(fumo_x + 48, fumo_y);
    } else if (g_is_boosting || g_player_speed > 185) {
        /* 极速狂飙/氮气冲刺: Fumo 兴奋大喜 ᗜ ᴗ ᗜ，伴随风阻流线 */
        Draw_Fumo_Eye(fumo_x, fumo_y, 0);
        Draw_Fumo_Eye(fumo_x + 24, fumo_y, 0);
        /* 微笑嘴 ᴗ */
        OLED_DrawLine(fumo_x + 17, fumo_y + 11, fumo_x + 20, fumo_y + 13, OLED_COLOR_WHITE);
        OLED_DrawLine(fumo_x + 20, fumo_y + 13, fumo_x + 23, fumo_y + 13, OLED_COLOR_WHITE);
        OLED_DrawLine(fumo_x + 23, fumo_y + 13, fumo_x + 26, fumo_y + 11, OLED_COLOR_WHITE);
        /* 左右急速气流线 */
        uint8_t wind_offset = (g_ms_ticks / 20) % 8;
        OLED_DrawLine(10 + wind_offset, 46, 24 + wind_offset, 46, OLED_COLOR_WHITE);
        OLED_DrawLine(6 + wind_offset, 51, 18 + wind_offset, 51, OLED_COLOR_WHITE);
        OLED_DrawLine(96 + wind_offset, 46, 110 + wind_offset, 46, OLED_COLOR_WHITE);
        OLED_DrawLine(102 + wind_offset, 51, 116 + wind_offset, 51, OLED_COLOR_WHITE);
    } else {
        /* 正常巡航: 标志性戴墨镜的酷酷冷脸萌 ( ᗜ - ᗜ ) */
        Draw_Fumo_Eye(fumo_x, fumo_y, 0);
        Draw_Fumo_Eye(fumo_x + 24, fumo_y, 0);
        /* 平板冷嘴 */
        OLED_DrawLine(fumo_x + 18, fumo_y + 12, fumo_x + 24, fumo_y + 12, OLED_COLOR_WHITE);
        /* 赛车酷炫墨镜镜框 (跨过两只眼睛) */
        OLED_DrawLine(fumo_x - 1, fumo_y + 4, fumo_x + 46, fumo_y + 4, OLED_COLOR_WHITE);
        OLED_DrawLine(fumo_x - 1, fumo_y + 5, fumo_x + 46, fumo_y + 5, OLED_COLOR_WHITE);
        OLED_DrawLine(fumo_x + 21, fumo_y + 6, fumo_x + 24, fumo_y + 6, OLED_COLOR_WHITE);
    }

    /* 底部操作指引 */
    OLED_DrawLine(0, 56, 127, 56, OLED_COLOR_WHITE);
    OLED_ShowString(2, 57, "K1/2:Steer K3:Nitro K4:Exit", 8, OLED_COLOR_WHITE);
}

/* =========================================================================
 * 主函数与主循环
 * ========================================================================= */
int main(void)
{
    /* 硬件时钟超频与初始化 */
    SystemClock_Set64M_HSI();
    SysTick_Init_64M();
    LED_Init();
    KEY_Init();
    OLED_Init();
    OLED_SetDelay(0); /* 80+ FPS 极限刷新 */

    uint8_t current_app_mode = MODE_RACING; /* 启动直接进入崭新的赛车游戏！(按 K4 可自由切换) */
    uint8_t current_mood     = MOOD_NORMAL;
    uint8_t auto_ai_mode     = 1;
    uint32_t next_mood_ms    = MOOD_HOLD_BASE_MS;

    /* 冷脸萌注视微调平滑插值 */
    int8_t look_x = 0;
    int8_t look_y = 0;
    int8_t target_x = 0;
    int8_t target_y = 0;
    uint32_t next_look_ms = 2500;

    /* 冷脸萌眨眼控制 */
    uint8_t is_blinking = 0;
    uint8_t blink_depth = 0;
    uint32_t next_blink_ms = 2000;

    uint32_t frame_tick = 0;
    uint8_t  smug_phase = 0;

    /* 初始化赛车引擎 */
    Racing_Reset();

    while (1) {
        g_seed += 31;
        uint8_t key = KEY_Scan();

        /* K4: 随时在【1.双子萌】/【2.合体大脸】/【3.极速赛车】之间循环切换！*/
        if (key == KEY_4) {
            current_app_mode = (current_app_mode + 1) % MODE_TOTAL_COUNT;
            if (current_app_mode == MODE_RACING) {
                Racing_Reset();
            }
        }

        /* ================= 模式一与二: 冷脸萌桌宠逻辑 ================= */
        if (current_app_mode == MODE_FUMO_TWIN || current_app_mode == MODE_FUMO_MERGE) {
            if (key == KEY_1) {
                current_mood = (current_mood + 1) % MOOD_TOTAL;
                smug_phase = 0;
                next_mood_ms = g_ms_ticks + MOOD_HOLD_BASE_MS + (Rand() % MOOD_HOLD_RAND_MS);
            } else if (key == KEY_2) {
                current_mood = (current_mood == 0) ? (MOOD_TOTAL - 1) : (current_mood - 1);
                smug_phase = 0;
                next_mood_ms = g_ms_ticks + MOOD_HOLD_BASE_MS + (Rand() % MOOD_HOLD_RAND_MS);
            } else if (key == KEY_3) {
                /* 戳一戳互动 */
                is_blinking = 1;
                blink_depth = 1;
                if (current_mood == MOOD_NORMAL)      current_mood = MOOD_SHY;
                else if (current_mood == MOOD_SHY)    current_mood = MOOD_KAWAII;
                else if (current_mood == MOOD_KAWAII) current_mood = MOOD_HAPPY;
                else if (current_mood == MOOD_HAPPY)  current_mood = MOOD_SMUG;
                else                                  current_mood = MOOD_NORMAL;
                smug_phase = 0;
                next_mood_ms = g_ms_ticks + MOOD_HOLD_BASE_MS + (Rand() % MOOD_HOLD_RAND_MS);
            }

            /* 自动生活状态机 */
            if (auto_ai_mode && (g_ms_ticks > next_mood_ms)) {
                uint32_t hold_base = (current_mood == MOOD_SLEEP) ? 14000 : MOOD_HOLD_BASE_MS;
                next_mood_ms = g_ms_ticks + hold_base + (Rand() % MOOD_HOLD_RAND_MS);
                uint8_t dice = Rand() % 10;
                if (dice <= 3) current_mood = MOOD_NORMAL;
                else if (dice == 4) current_mood = MOOD_HAPPY;
                else if (dice == 5) current_mood = MOOD_KAWAII;
                else if (dice == 6) current_mood = MOOD_SHY;
                else if (dice == 7) current_mood = MOOD_PEACE;
                else if (dice == 8) current_mood = MOOD_SMUG;
                else                current_mood = MOOD_POUT;
            }

            /* 注视微动 */
            if (g_ms_ticks > next_look_ms && current_mood != MOOD_SLEEP && current_mood != MOOD_FLIP_TABLE) {
                next_look_ms = g_ms_ticks + 2500 + (Rand() % 2500);
                target_x = (int8_t)(Rand() % 5) - 2;
                target_y = (int8_t)(Rand() % 3) - 1;
            }

            /* 眨眼 */
            if (g_ms_ticks > next_blink_ms && !is_blinking && current_mood != MOOD_SLEEP) {
                is_blinking = 1;
                blink_depth = 1;
                next_blink_ms = g_ms_ticks + 2000 + (Rand() % 3000);
            }
            if (is_blinking) {
                blink_depth += 2;
                if (blink_depth >= 18) {
                    is_blinking = 0;
                    blink_depth = 0;
                }
            }

            if (current_mood == MOOD_SMUG && (frame_tick % 60) == 0) {
                smug_phase = !smug_phase;
            }

            if (look_x < target_x) look_x++;
            else if (look_x > target_x) look_x--;
            if (look_y < target_y) look_y++;
            else if (look_y > target_y) look_y--;

            /* LED 指示 */
            if (current_mood == MOOD_FLIP_TABLE) {
                LED_Set((frame_tick % 8) < 4);
            } else if (current_mood == MOOD_SHY || current_mood == MOOD_SMUG) {
                uint32_t t = g_ms_ticks % 1200;
                LED_Set((t < 80) || (t > 180 && t < 260));
            } else if (current_mood == MOOD_SLEEP) {
                LED_Set(0);
            } else {
                LED_Set(is_blinking);
            }

            /* 双屏冷脸萌渲染 */
            OLED_ClearScreen(OLED_SCREEN_L);
            OLED_ClearScreen(OLED_SCREEN_R);

            if (current_app_mode == MODE_FUMO_TWIN) {
                Draw_Single_Fumo_Face(OLED_SCREEN_L, current_mood, look_x, look_y,
                                      is_blinking, blink_depth, smug_phase, frame_tick, auto_ai_mode);
                Draw_Single_Fumo_Face(OLED_SCREEN_R, current_mood, -look_x, look_y,
                                      is_blinking, blink_depth, smug_phase, frame_tick, auto_ai_mode);
            } else {
                Draw_Merged_Fumo_Face(OLED_SCREEN_L, current_mood, look_x, look_y,
                                      is_blinking, blink_depth, smug_phase, frame_tick);
                Draw_Merged_Fumo_Face(OLED_SCREEN_R, current_mood, look_x, look_y,
                                      is_blinking, blink_depth, smug_phase, frame_tick);
            }

        } else {
            /* ================= 模式三: 极速赛车游戏逻辑 ================= */
            Racing_Update(key);

            OLED_ClearScreen(OLED_SCREEN_L);
            OLED_ClearScreen(OLED_SCREEN_R);

            /* 物理左屏 (屏幕2: PB11/PB8): 渲染赛车仪表盘与 Fumo 陪驾反应 */
            Racing_Render_Dashboard();

            /* 物理右屏 (屏幕1: PB6/PB7): 渲染高速公路主赛道视界与车辆 */
            Racing_Render_Track();
        }

        /* 80+ FPS 双屏极速刷新 */
        OLED_UpdateAll();
        frame_tick++;
    }

    return 0;
}
