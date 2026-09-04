/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : 双屏呆萌冷脸萌 (Dual-Screen Fumo ᗜ - ᗜ) 活体桌宠
 *                   物理摆放:
 *                     左侧屏 (屏幕2): SCL=PB11, SDA=PB8  (OLED_SCREEN_L)
 *                     右侧屏 (屏幕1): SCL=PB6,  SDA=PB7  (OLED_SCREEN_R)
 *                   双屏双子萌互动 / 跨屏合体大脸 / 80+ FPS 极限刷新
 ******************************************************************************
 */

#include <stdint.h>
#include "oled.h"
#include "key.h"

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

/* 10 种经典纯正冷脸萌表情 */
#define MOOD_NORMAL     0   /* 平常的时候: ᗜ - ᗜ */
#define MOOD_SHY        1   /* 害羞的时候: ,,ᗜ - ᗜ,, */
#define MOOD_HAPPY      2   /* 开心的时候: ᗜ ᴗ ᗜ */
#define MOOD_POUT       3   /* 不开心的时候: ᗜ ‸ ᗜ */
#define MOOD_ANGRY      4   /* 生气的时候: ᗜ-ᗜ */
#define MOOD_FLIP_TABLE 5   /* 爆发掀桌: ( ╯－︿－)╯╧╧ */
#define MOOD_SLEEP      6   /* 睡着的时候: ᶻz₍ _ _ ₎ */
#define MOOD_PEACE      7   /* 比耶的时候: ᗜ ֊ ᗜ◞ ✌ */
#define MOOD_KAWAII     8   /* 卖萌的时候: ᗜ 𖥦 ᗜ */
#define MOOD_SMUG       9   /* 被夸暗爽: （ᗜ _ ᗜ）➔（ᗜ ֊ ᗜ） */
#define MOOD_TOTAL      10

/* 表情停留时长参数 (单位: ms) */
#define MOOD_HOLD_BASE_MS   8000   /* 基础持续时间: 8秒 */
#define MOOD_HOLD_RAND_MS   6000   /* 随机浮动区间: 0~6秒 (每个表情驻留 8 ~ 14 秒) */

/* 显示模式: 0: 双子冷脸萌 (双屏各一只完整萌宠) | 1: 合体大脸 (双屏拼成一张大脸) */
#define DISPLAY_MODE_TWIN   0
#define DISPLAY_MODE_MERGE  1

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

/* 经典标志性纯空心 ᗜ 字眼框 (22x18, 内部 100% 镂空纯白留白, 绝无恐怖实心填充) */
static const uint32_t FUMO_EYE_BORDER[18] = {
    0x003F00, /* row 0:  ......######...... (22-bit, bits 21..0) */
    0x00FFC0, /* row 1:  ....##########.... */
    0x038070, /* row 2:  ..###........###.. */
    0x070038, /* row 3:  .###..........###. */
    0x0C000C, /* row 4:  ##..............## */
    0x180006, /* row 5:  #................# */
    0x180006, /* row 6:  #................# */
    0x300003, /* row 7:  ##..............## */
    0x300003, /* row 8:  ##..............## */
    0x300003, /* row 9:  ##..............## */
    0x300003, /* row 10: ##..............## */
    0x300003, /* row 11: ##..............## */
    0x300003, /* row 12: ##..............## */
    0x300003, /* row 13: ##..............## */
    0x300003, /* row 14: ##..............## */
    0x300003, /* row 15: ##..............## */
    0x3FFFFF, /* row 16: #################### (平底边框 2px) */
    0x3FFFFF, /* row 17: #################### */
};

/**
 * @brief 绘制标志性的冷脸萌【ᗜ】纯空心边框 (内部完全镂空留白)
 */
static void Draw_Fumo_Eye(uint8_t x0, uint8_t y0, uint8_t blink)
{
    if (blink >= 16) {
        /* 完全闭上眼: 纯平底闭眼线 '_' (2px 粗) */
        OLED_DrawLine(x0, y0 + 16, x0 + 21, y0 + 16, OLED_COLOR_WHITE);
        OLED_DrawLine(x0, y0 + 17, x0 + 21, y0 + 17, OLED_COLOR_WHITE);
        return;
    }

    for (uint8_t r = 0; r < 18; r++) {
        if (r < blink) {
            continue; /* 眨眼时眼皮下遮 */
        }

        uint32_t row_bits = FUMO_EYE_BORDER[r];
        if (blink > 0 && r == blink) {
            row_bits = 0x3FFFFF; /* 闭合眼皮线 */
        }

        for (uint8_t c = 0; c < 22; c++) {
            if (row_bits & (1UL << (21 - c))) {
                OLED_DrawPoint(x0 + c, y0 + r, OLED_COLOR_WHITE);
            }
        }
    }
}

/**
 * @brief 绘制怒筋 💢 (生气质感)
 */
static void Draw_Anger_Mark(uint8_t x, uint8_t y)
{
    OLED_DrawLine(x - 3, y - 1, x + 3, y - 1, OLED_COLOR_WHITE);
    OLED_DrawLine(x - 3, y + 1, x + 3, y + 1, OLED_COLOR_WHITE);
    OLED_DrawLine(x - 1, y - 3, x - 1, y + 3, OLED_COLOR_WHITE);
    OLED_DrawLine(x + 1, y - 3, x + 1, y + 3, OLED_COLOR_WHITE);
}

/**
 * @brief 绘制腮红 marks (,, 或斜线)
 */
static void Draw_Blush(uint8_t x, uint8_t y)
{
    OLED_DrawLine(x,     y + 4, x + 2, y, OLED_COLOR_WHITE);
    OLED_DrawLine(x + 4, y + 4, x + 6, y, OLED_COLOR_WHITE);
}

/**
 * @brief 绘制飞舞的掀翻桌子 (╧╧ 旋转翻滚动画)
 */
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

/**
 * @brief 绘制比耶剪刀手 ✌
 * @param mirror 0: 靠右侧手, 1: 靠左侧手 (镜像)
 */
static void Draw_Peace_Hand(uint8_t x, uint8_t y, uint8_t mirror)
{
    if (!mirror) {
        /* 手掌肉球 */
        OLED_FillCircle(x + 5, y + 8, 3, OLED_COLOR_WHITE);
        /* 食指 */
        OLED_DrawLine(x + 3, y + 6, x + 1, y, OLED_COLOR_WHITE);
        OLED_DrawLine(x + 4, y + 6, x + 2, y, OLED_COLOR_WHITE);
        /* 中指 */
        OLED_DrawLine(x + 6, y + 6, x + 8, y, OLED_COLOR_WHITE);
        OLED_DrawLine(x + 7, y + 6, x + 9, y, OLED_COLOR_WHITE);
    } else {
        /* 镜像左手 */
        OLED_FillCircle(x + 5, y + 8, 3, OLED_COLOR_WHITE);
        OLED_DrawLine(x + 6, y + 6, x + 8, y, OLED_COLOR_WHITE);
        OLED_DrawLine(x + 5, y + 6, x + 7, y, OLED_COLOR_WHITE);
        OLED_DrawLine(x + 3, y + 6, x + 1, y, OLED_COLOR_WHITE);
        OLED_DrawLine(x + 2, y + 6, x,     y, OLED_COLOR_WHITE);
    }
}

/**
 * @brief 渲染单屏完整的呆萌冷脸萌面孔
 * @param screen OLED_SCREEN_L (屏幕2, 左) 或 OLED_SCREEN_R (屏幕1, 右)
 */
static void Draw_Single_Fumo_Face(uint8_t screen, uint8_t current_mood,
                                  int8_t look_x, int8_t look_y,
                                  uint8_t is_blinking, uint8_t blink_depth,
                                  uint8_t smug_phase, uint32_t frame_tick,
                                  uint8_t auto_ai_mode)
{
    OLED_SelectScreen(screen);

    /* --- 顶部精致标题胶囊 --- */
    OLED_FillRect(0, 0, 128, 9, OLED_COLOR_WHITE);
    OLED_ShowString(4, 1, MOOD_NAMES[current_mood], 8, OLED_COLOR_BLACK);
    if (screen == OLED_SCREEN_L) {
        OLED_ShowString(96, 1, "FUMO-L", 8, OLED_COLOR_BLACK);
    } else {
        if (auto_ai_mode) OLED_ShowString(96, 1, "AUTO", 8, OLED_COLOR_BLACK);
        else              OLED_ShowString(96, 1, "MANU", 8, OLED_COLOR_BLACK);
    }

    /* 双眼固定基准坐标 (居中排布) */
    const uint8_t left_eye_x  = 24;
    const uint8_t right_eye_x = 82;
    const uint8_t eye_y       = 14;
    const uint8_t eye_w       = 22;

    /* --- 渲染两只标志性的【ᗜ】大眼睛 (内部完全镂空留白) --- */
    if (current_mood == MOOD_SLEEP) {
        /* 睡着: ᶻz₍ _ _ ₎ 舒缓闭眼曲线 */
        OLED_DrawLine(left_eye_x + 2, eye_y + 14, left_eye_x + eye_w - 2, eye_y + 14, OLED_COLOR_WHITE);
        OLED_DrawLine(left_eye_x + 2, eye_y + 15, left_eye_x + eye_w - 2, eye_y + 15, OLED_COLOR_WHITE);

        OLED_DrawLine(right_eye_x + 2, eye_y + 14, right_eye_x + eye_w - 2, eye_y + 14, OLED_COLOR_WHITE);
        OLED_DrawLine(right_eye_x + 2, eye_y + 15, right_eye_x + eye_w - 2, eye_y + 15, OLED_COLOR_WHITE);

        /* 飘出的 z Z 气泡 */
        uint8_t zy = (frame_tick / 4) % 20;
        OLED_ShowString(104, 28 - zy, "z", 8, OLED_COLOR_WHITE);
        if (zy > 6) OLED_ShowString(112, 28 - zy - 6, "Z", 8, OLED_COLOR_WHITE);
    } else if (current_mood == MOOD_FLIP_TABLE) {
        /* 爆发掀桌: 紧闭的愤怒眼 ( ╯－︿－)╯ */
        OLED_DrawLine(left_eye_x + 4, eye_y + 4, left_eye_x + 16, eye_y + 10, OLED_COLOR_WHITE);
        OLED_DrawLine(left_eye_x + 16, eye_y + 10, left_eye_x + 4, eye_y + 16, OLED_COLOR_WHITE);
        OLED_DrawLine(right_eye_x + 16, eye_y + 4, right_eye_x + 4, eye_y + 10, OLED_COLOR_WHITE);
        OLED_DrawLine(right_eye_x + 4, eye_y + 10, right_eye_x + 16, eye_y + 16, OLED_COLOR_WHITE);

        /* 抬起的双手 ╯ ╯ */
        OLED_DrawLine(left_eye_x - 10, eye_y + 18, left_eye_x - 4, eye_y + 8, OLED_COLOR_WHITE);
        OLED_DrawLine(right_eye_x + eye_w + 4, eye_y + 18, right_eye_x + eye_w + 10, eye_y + 8, OLED_COLOR_WHITE);

        /* 飞起的桌子 ╧╧ */
        uint8_t fly_x = 94 + (frame_tick % 8);
        uint8_t fly_y = 12 + ((frame_tick / 4) % 6);
        Draw_Flying_Table(fly_x, fly_y, frame_tick / 8);
    } else {
        /* 经典纯空心 ᗜ 边框眼睛 (完全镂空留白，绝不恐怖) */
        Draw_Fumo_Eye(left_eye_x + look_x, eye_y + look_y, is_blinking ? blink_depth : 0);
        Draw_Fumo_Eye(right_eye_x + look_x, eye_y + look_y, is_blinking ? blink_depth : 0);

        if (current_mood == MOOD_ANGRY) {
            /* 生气 ᗜ-ᗜ: 微微蹙起的怒眉 + 额头怒筋 💢 */
            OLED_DrawLine(left_eye_x + look_x + 2, eye_y - 2, left_eye_x + look_x + eye_w, eye_y + 2, OLED_COLOR_WHITE);
            OLED_DrawLine(right_eye_x + look_x, eye_y + 2, right_eye_x + look_x + eye_w - 2, eye_y - 2, OLED_COLOR_WHITE);
            Draw_Anger_Mark(right_eye_x + eye_w + 8, eye_y - 1);
        }
    }

    /* --- 嘴巴与面部细节 --- */
    const uint8_t mouth_x = 64 + look_x;
    const uint8_t mouth_y = 33 + look_y;

    switch (current_mood) {
        case MOOD_NORMAL:
        case MOOD_ANGRY:
            /* 经典冷脸平板嘴: ᗜ - ᗜ */
            OLED_DrawLine(mouth_x - 5, mouth_y, mouth_x + 5, mouth_y, OLED_COLOR_WHITE);
            OLED_DrawLine(mouth_x - 5, mouth_y + 1, mouth_x + 5, mouth_y + 1, OLED_COLOR_WHITE);
            break;

        case MOOD_SHY:
            /* 害羞: ,, ᗜ - ᗜ ,, 脸红加呆直嘴 */
            OLED_DrawLine(mouth_x - 4, mouth_y + 1, mouth_x + 4, mouth_y + 1, OLED_COLOR_WHITE);
            Draw_Blush(left_eye_x + look_x - 12, mouth_y - 4);
            Draw_Blush(right_eye_x + look_x + eye_w + 4, mouth_y - 4);
            break;

        case MOOD_HAPPY:
            /* 开心微笑: ᗜ ᴗ ᗜ 微微上扬的小弯嘴角 */
            OLED_DrawLine(mouth_x - 5, mouth_y - 1, mouth_x - 3, mouth_y + 2, OLED_COLOR_WHITE);
            OLED_DrawLine(mouth_x - 3, mouth_y + 2, mouth_x + 3, mouth_y + 2, OLED_COLOR_WHITE);
            OLED_DrawLine(mouth_x + 3, mouth_y + 2, mouth_x + 5, mouth_y - 1, OLED_COLOR_WHITE);
            Draw_Blush(left_eye_x + look_x - 10, mouth_y - 2);
            Draw_Blush(right_eye_x + look_x + eye_w + 3, mouth_y - 2);
            break;

        case MOOD_POUT:
            /* 不开心委屈嘟嘴: ᗜ ‸ ᗜ 倒 V 嘴角 */
            OLED_DrawLine(mouth_x - 4, mouth_y + 3, mouth_x, mouth_y - 1, OLED_COLOR_WHITE);
            OLED_DrawLine(mouth_x, mouth_y - 1, mouth_x + 4, mouth_y + 3, OLED_COLOR_WHITE);
            /* 额头委屈汗滴 */
            OLED_DrawCircle(left_eye_x + look_x - 8, eye_y + 4, 3, OLED_COLOR_WHITE);
            break;

        case MOOD_FLIP_TABLE:
            /* 掀桌波浪大嘴: －︿－ */
            OLED_DrawLine(mouth_x - 6, mouth_y + 3, mouth_x, mouth_y - 2, OLED_COLOR_WHITE);
            OLED_DrawLine(mouth_x, mouth_y - 2, mouth_x + 6, mouth_y + 3, OLED_COLOR_WHITE);
            break;

        case MOOD_SLEEP:
            /* 睡眠平嘴 */
            OLED_DrawLine(mouth_x - 4, mouth_y + 2, mouth_x + 4, mouth_y + 2, OLED_COLOR_WHITE);
            break;

        case MOOD_PEACE:
            /* 比耶: ᗜ ֊ ᗜ 嘴角上扬，比出剪刀手 ✌ */
            OLED_DrawLine(mouth_x - 4, mouth_y + 1, mouth_x + 4, mouth_y + 1, OLED_COLOR_WHITE);
            OLED_DrawPoint(mouth_x + 5, mouth_y, OLED_COLOR_WHITE);
            if (screen == OLED_SCREEN_L) {
                /* 物理左屏 (屏幕2): 右手朝内侧比耶 ✌ */
                Draw_Peace_Hand(right_eye_x + look_x + eye_w + 3, mouth_y - 10, 0);
            } else {
                /* 物理右屏 (屏幕1): 左手朝内侧对称比耶 ✌ */
                Draw_Peace_Hand(left_eye_x + look_x - 14, mouth_y - 10, 1);
            }
            break;

        case MOOD_KAWAII:
            /* 卖萌: ᗜ 𖥦 ᗜ 猫咪波浪嘴 :3 */
            OLED_DrawLine(mouth_x - 5, mouth_y + 1, mouth_x - 3, mouth_y - 1, OLED_COLOR_WHITE);
            OLED_DrawLine(mouth_x - 3, mouth_y - 1, mouth_x, mouth_y + 1, OLED_COLOR_WHITE);
            OLED_DrawLine(mouth_x, mouth_y + 1, mouth_x + 3, mouth_y - 1, OLED_COLOR_WHITE);
            OLED_DrawLine(mouth_x + 3, mouth_y - 1, mouth_x + 5, mouth_y + 1, OLED_COLOR_WHITE);
            Draw_Blush(left_eye_x + look_x - 10, mouth_y - 2);
            Draw_Blush(right_eye_x + look_x + eye_w + 3, mouth_y - 2);
            break;

        case MOOD_SMUG:
            /* 暗爽两段式: （ᗜ _ ᗜ）➔（ᗜ ֊ ᗜ）*/
            if (smug_phase == 0) {
                OLED_DrawLine(mouth_x - 5, mouth_y, mouth_x + 5, mouth_y, OLED_COLOR_WHITE);
            } else {
                OLED_DrawLine(mouth_x - 4, mouth_y + 2, mouth_x + 1, mouth_y + 2, OLED_COLOR_WHITE);
                OLED_DrawLine(mouth_x + 1, mouth_y + 2, mouth_x + 6, mouth_y - 1, OLED_COLOR_WHITE);
                Draw_Blush(right_eye_x + look_x + eye_w + 4, mouth_y - 2);
            }
            /* 两侧括号 （ ） */
            OLED_ShowChar(6, eye_y + 4, '(', 16, OLED_COLOR_WHITE);
            OLED_ShowChar(114, eye_y + 4, ')', 16, OLED_COLOR_WHITE);
            break;
    }

    /* --- 底部快捷提示 --- */
    OLED_DrawLine(0, 52, 127, 52, OLED_COLOR_WHITE);
    if (screen == OLED_SCREEN_L) {
        OLED_ShowString(2, 54, "K1/2:Mood  K3:Poke", 8, OLED_COLOR_WHITE);
    } else {
        OLED_ShowString(2, 54, "K4:Mode [TWIN] 80FPS", 8, OLED_COLOR_WHITE);
    }
}

/**
 * @brief 渲染跨屏合体大脸冷脸萌 (MERGE 模式)
 *        屏幕2 (OLED_SCREEN_L) 在左侧 -> 显示左半脸
 *        屏幕1 (OLED_SCREEN_R) 在右侧 -> 显示右半脸
 */
static void Draw_Merged_Fumo_Face(uint8_t screen, uint8_t current_mood,
                                  int8_t look_x, int8_t look_y,
                                  uint8_t is_blinking, uint8_t blink_depth,
                                  uint8_t smug_phase, uint32_t frame_tick)
{
    OLED_SelectScreen(screen);

    /* 顶部标题栏 */
    OLED_FillRect(0, 0, 128, 9, OLED_COLOR_WHITE);
    OLED_ShowString(4, 1, MOOD_NAMES[current_mood], 8, OLED_COLOR_BLACK);
    if (screen == OLED_SCREEN_L) {
        OLED_ShowString(96, 1, "LEFT", 8, OLED_COLOR_BLACK);
    } else {
        OLED_ShowString(96, 1, "RIGHT", 8, OLED_COLOR_BLACK);
    }

    /* 大眼坐标 (物理左屏靠右放，物理右屏靠左放，贴近中缝无缝成对) */
    uint8_t eye_x = (screen == OLED_SCREEN_L) ? (68 + look_x) : (38 + look_x);
    uint8_t eye_y = 18 + look_y;

    if (current_mood == MOOD_SLEEP) {
        /* 闭眼曲线 */
        OLED_DrawLine(eye_x, eye_y + 16, eye_x + 22, eye_y + 16, OLED_COLOR_WHITE);
        OLED_DrawLine(eye_x, eye_y + 17, eye_x + 22, eye_y + 17, OLED_COLOR_WHITE);
        if (screen == OLED_SCREEN_R) {
            uint8_t zy = (frame_tick / 4) % 20;
            OLED_ShowString(104, 30 - zy, "z", 8, OLED_COLOR_WHITE);
            if (zy > 6) OLED_ShowString(112, 30 - zy - 6, "Z", 8, OLED_COLOR_WHITE);
        }
    } else if (current_mood == MOOD_FLIP_TABLE) {
        /* 爆发掀桌 */
        if (screen == OLED_SCREEN_L) {
            /* 左半脸: 左眼 > 与 左手 ╯ */
            OLED_DrawLine(eye_x + 2, eye_y + 4, eye_x + 18, eye_y + 11, OLED_COLOR_WHITE);
            OLED_DrawLine(eye_x + 18, eye_y + 11, eye_x + 2, eye_y + 18, OLED_COLOR_WHITE);
            OLED_DrawLine(20, eye_y + 18, 30, eye_y + 6, OLED_COLOR_WHITE);
        } else {
            /* 右半脸: 右眼 < 与 右手 ╯ 及飞舞桌子 ╧╧ */
            OLED_DrawLine(eye_x + 18, eye_y + 4, eye_x + 2, eye_y + 11, OLED_COLOR_WHITE);
            OLED_DrawLine(eye_x + 2, eye_y + 11, eye_x + 18, eye_y + 18, OLED_COLOR_WHITE);
            OLED_DrawLine(eye_x + 26, eye_y + 18, eye_x + 36, eye_y + 6, OLED_COLOR_WHITE);
            Draw_Flying_Table(90, 14, frame_tick / 8);
        }
    } else {
        /* 纯空心大眼 ᗜ */
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

    /* 中间贯通嘴巴 (左屏画到最右边缘，右屏从最左边缘起始，物理拼合严丝合缝) */
    uint8_t mouth_y = 38 + look_y;
    if (screen == OLED_SCREEN_L) {
        /* 左半嘴 (延伸至左屏右边界) */
        OLED_DrawLine(118, mouth_y, 127, mouth_y, OLED_COLOR_WHITE);
        OLED_DrawLine(118, mouth_y + 1, 127, mouth_y + 1, OLED_COLOR_WHITE);
        if (current_mood == MOOD_SHY || current_mood == MOOD_KAWAII || current_mood == MOOD_HAPPY) {
            Draw_Blush(24, mouth_y - 4);
        }
        if (current_mood == MOOD_SMUG) {
            OLED_ShowChar(10, eye_y + 2, '(', 16, OLED_COLOR_WHITE);
        }
    } else {
        /* 右半嘴 (从右屏左边界延伸) */
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

int main(void)
{
    /* 硬件时钟超频与初始化 */
    SystemClock_Set64M_HSI();
    SysTick_Init_64M();
    LED_Init();
    KEY_Init();
    OLED_Init();
    OLED_SetDelay(0); /* 80+ FPS 极限刷新 */

    uint8_t current_mood   = MOOD_NORMAL;
    uint8_t display_mode   = DISPLAY_MODE_TWIN; /* 默认双子冷脸萌模式 */
    uint8_t auto_ai_mode   = 1;
    uint32_t next_mood_ms  = MOOD_HOLD_BASE_MS;

    /* 注视微调平滑插值 */
    int8_t look_x = 0;
    int8_t look_y = 0;
    int8_t target_x = 0;
    int8_t target_y = 0;
    uint32_t next_look_ms = 2500;

    /* 眨眼控制 */
    uint8_t is_blinking = 0;
    uint8_t blink_depth = 0;
    uint32_t next_blink_ms = 2000;

    uint32_t frame_tick = 0;
    uint8_t  smug_phase = 0;

    while (1) {
        g_seed += 31;
        uint8_t key = KEY_Scan();

        /* ================= 1. 按键响应 ================= */
        if (key == KEY_1) {
            /* K1: 下一个冷脸萌表情 */
            current_mood = (current_mood + 1) % MOOD_TOTAL;
            smug_phase = 0;
            next_mood_ms = g_ms_ticks + MOOD_HOLD_BASE_MS + (Rand() % MOOD_HOLD_RAND_MS);
        } else if (key == KEY_2) {
            /* K2: 上一个冷脸萌表情 */
            current_mood = (current_mood == 0) ? (MOOD_TOTAL - 1) : (current_mood - 1);
            smug_phase = 0;
            next_mood_ms = g_ms_ticks + MOOD_HOLD_BASE_MS + (Rand() % MOOD_HOLD_RAND_MS);
        } else if (key == KEY_3) {
            /* K3 (#): 戳一戳冷脸! 触发立即眨眼与表情演进 */
            is_blinking = 1;
            blink_depth = 1;
            if (current_mood == MOOD_NORMAL)      current_mood = MOOD_SHY;
            else if (current_mood == MOOD_SHY)    current_mood = MOOD_KAWAII;
            else if (current_mood == MOOD_KAWAII) current_mood = MOOD_HAPPY;
            else if (current_mood == MOOD_HAPPY)  current_mood = MOOD_SMUG;
            else                                  current_mood = MOOD_NORMAL;
            smug_phase = 0;
            next_mood_ms = g_ms_ticks + MOOD_HOLD_BASE_MS + (Rand() % MOOD_HOLD_RAND_MS);
        } else if (key == KEY_4) {
            /* K4 (*): 切换 [双子萌 TWIN] 与 [合体大脸 MERGE] 模式 */
            display_mode = !display_mode;
        }

        /* ================= 2. 自动生活状态机 ================= */
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

        /* 独立注视微动周期 */
        if (g_ms_ticks > next_look_ms && current_mood != MOOD_SLEEP && current_mood != MOOD_FLIP_TABLE) {
            next_look_ms = g_ms_ticks + 2500 + (Rand() % 2500);
            target_x = (int8_t)(Rand() % 5) - 2;
            target_y = (int8_t)(Rand() % 3) - 1;
        }

        /* 自然眨眼周期 */
        if (g_ms_ticks > next_blink_ms && !is_blinking && current_mood != MOOD_SLEEP) {
            is_blinking = 1;
            blink_depth = 1;
            next_blink_ms = g_ms_ticks + 2000 + (Rand() % 3000);
        }

        /* 眨眼动作步进 */
        if (is_blinking) {
            blink_depth += 2;
            if (blink_depth >= 18) {
                is_blinking = 0;
                blink_depth = 0;
            }
        }

        /* 暗爽两段式表情动画 */
        if (current_mood == MOOD_SMUG) {
            if ((frame_tick % 60) == 0) {
                smug_phase = !smug_phase;
            }
        }

        /* 注视平滑插值 */
        if (look_x < target_x) look_x++;
        else if (look_x > target_x) look_x--;
        if (look_y < target_y) look_y++;
        else if (look_y > target_y) look_y--;

        /* 板载 LED 状态联动 */
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

        /* ================= 3. 双屏画面渲染 ================= */
        OLED_ClearScreen(OLED_SCREEN_L);
        OLED_ClearScreen(OLED_SCREEN_R);

        if (display_mode == DISPLAY_MODE_TWIN) {
            /* 双子萌模式:
             * 物理左屏 (屏幕2: OLED_SCREEN_L): 绘制 Fumo-L
             * 物理右屏 (屏幕1: OLED_SCREEN_R): 绘制 Fumo-R
             */
            Draw_Single_Fumo_Face(OLED_SCREEN_L, current_mood, look_x, look_y,
                                  is_blinking, blink_depth, smug_phase, frame_tick, auto_ai_mode);
            Draw_Single_Fumo_Face(OLED_SCREEN_R, current_mood, -look_x, look_y,
                                  is_blinking, blink_depth, smug_phase, frame_tick, auto_ai_mode);
        } else {
            /* 跨屏合体大脸模式 (K4 切换):
             * 物理左屏 (屏幕2: OLED_SCREEN_L): 绘制左半脸
             * 物理右屏 (屏幕1: OLED_SCREEN_R): 绘制右半脸
             */
            Draw_Merged_Fumo_Face(OLED_SCREEN_L, current_mood, look_x, look_y,
                                  is_blinking, blink_depth, smug_phase, frame_tick);
            Draw_Merged_Fumo_Face(OLED_SCREEN_R, current_mood, look_x, look_y,
                                  is_blinking, blink_depth, smug_phase, frame_tick);
        }

        /* 80+ FPS 双屏极速刷新 */
        OLED_UpdateAll();
        frame_tick++;
    }

    return 0;
}
