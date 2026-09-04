/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : 双目赛博机器人巨眼 (Dual-Screen Giant Cyber Robot Eyes)
 *                   屏幕1 (左眼): SCL=PB6,  SDA=PB7
 *                   屏幕2 (右眼): SCL=PB11, SDA=PB8
 *                   超大尺寸 68x46 高精发光圆角大眼, 10 种生动表情, 80+ FPS
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

/* 10 种双目赛博机器人大眼表情模式 */
#define ROBOT_NEUTRAL     0   /* 1. 灵动大眼·活体环视 (Cozmo/Vector 标志性大圆角高光眼) */
#define ROBOT_HAPPY       1   /* 2. 喜笑颜开·月牙弯弯 (下眼睑大幅上弯为治愈笑月弧) */
#define ROBOT_ANGRY       2   /* 3. 机械狂暴·鹰目斜瞪 (上眼睑内八字向下重压，凌厉锋芒) */
#define ROBOT_LOVE        3   /* 4. 心花怒放·爱心桃目 (双屏巨型红心跳动缩放动画) */
#define ROBOT_CROSS       5   /* 5. 斗鸡眩晕·对眼转圈 (两眼紧贴中间缝隙斗鸡对视，眩晕螺旋) */
#define ROBOT_SUSPICIOUS  4   /* 6. 挑眉审视·半信半疑 (左眼高挑圆睁，右眼眯成细缝怀疑) */
#define ROBOT_SAD         6   /* 7. 委屈落泪·八字下垂 (上眼睑外八字耷拉，泪滴滑落) */
#define ROBOT_SURPRISE    7   /* 8. 震惊圆目·目瞪口呆 (巨型瞳孔放大，圆目惊愕) */
#define ROBOT_SCAN        8   /* 9. 赛博雷达·红外扫描 (科幻准星网格 + 贯穿高速水平扫描光束) */
#define ROBOT_SLEEP       9   /* 10.待机休眠·沉睡呼吸 (双眼合闭为微弧呼吸光条，飘起 zZ 气泡) */
#define ROBOT_TOTAL       10

/* 表情停留时长参数 (单位: ms) */
#define MOOD_HOLD_BASE_MS   8000   /* 基础持续时间: 8秒 */
#define MOOD_HOLD_RAND_MS   6000   /* 随机浮动区间: 0~6秒 (每个表情驻留 8 ~ 14 秒) */

static const char *ROBOT_NAMES[ROBOT_TOTAL] = {
    "1.NEUTRAL",
    "2.HAPPY",
    "3.ANGRY",
    "4.LOVE",
    "5.SKEPTIC",
    "6.CROSS",
    "7.SAD",
    "8.SHOCK",
    "9.SCANNER",
    "10.SLEEP"
};

/**
 * @brief 绘制赛博科技感四角包角支架
 */
static void Draw_Corner_Brackets(void)
{
    /* 左上 */
    OLED_DrawLine(2, 2, 8, 2, OLED_COLOR_WHITE);
    OLED_DrawLine(2, 2, 2, 8, OLED_COLOR_WHITE);
    /* 右上 */
    OLED_DrawLine(125, 2, 119, 2, OLED_COLOR_WHITE);
    OLED_DrawLine(125, 2, 125, 8, OLED_COLOR_WHITE);
    /* 左下 */
    OLED_DrawLine(2, 61, 8, 61, OLED_COLOR_WHITE);
    OLED_DrawLine(2, 61, 2, 55, OLED_COLOR_WHITE);
    /* 右下 */
    OLED_DrawLine(125, 61, 119, 61, OLED_COLOR_WHITE);
    OLED_DrawLine(125, 61, 125, 55, OLED_COLOR_WHITE);
}

/**
 * @brief 绘制参数化机器人巨眼
 * @param screen 0: 左屏(左眼), 1: 右屏(右眼)
 * @param cx, cy 眼睛中心坐标
 * @param w, h   眼睛宽度与高度 (默认 68 x 46)
 * @param r      圆角半径 (默认 14)
 * @param top_lid 上眼睑闭合深度 (0: 全开, >=h: 完全闭合)
 * @param top_slant 上眼睑倾斜度 (正数: 内眼角下压/凶狠怒视; 负数: 外眼角下垂/委屈伤心)
 * @param bottom_lid 下眼睑提升高度 (正数: 下眼睑上提/微笑)
 * @param bottom_slant 下眼睑倾斜度
 * @param pupil_style 0: 纯发光实心眼, 1: 动漫高光镂空瞳, 2: 缩小惊愕黑瞳, 3: 眩晕螺旋瞳
 */
static void Draw_Robot_Eye(uint8_t screen, int16_t cx, int16_t cy, int16_t w, int16_t h, int16_t r,
                           int16_t top_lid, int16_t top_slant,
                           int16_t bottom_lid, int16_t bottom_slant,
                           uint8_t pupil_style, uint32_t frame_tick)
{
    if (w <= 0 || h <= 0) return;
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;

    int16_t x0 = cx - w / 2;
    int16_t x1 = x0 + w - 1;
    int16_t y0 = cy - h / 2;
    int16_t y1 = y0 + h - 1;

    /* 内侧符号: 屏幕0 (左眼) 内侧朝右 (+1); 屏幕1 (右眼) 内侧朝左 (-1) */
    int16_t inner_sign = (screen == 0) ? 1 : -1;

    int16_t r_sq = r * r;
    int16_t cx_l = x0 + r;
    int16_t cx_r = x1 - r;
    int16_t cy_t = y0 + r;
    int16_t cy_b = y1 - r;

    /* 眼睛全闭状态: 绘制优雅的 2px 科技闭眼线 */
    if (top_lid >= h - 2) {
        int16_t line_w = w - 12;
        OLED_DrawLine(cx - line_w / 2, cy, cx + line_w / 2, cy, OLED_COLOR_WHITE);
        OLED_DrawLine(cx - line_w / 2, cy + 1, cx + line_w / 2, cy + 1, OLED_COLOR_WHITE);
        return;
    }

    for (int16_t y = y0; y <= y1; y++) {
        if (y < 0 || y >= OLED_HEIGHT) continue;

        for (int16_t x = x0; x <= x1; x++) {
            if (x < 0 || x >= OLED_WIDTH) continue;

            /* 1. 四角平滑圆角裁切 */
            if (x < cx_l && y < cy_t) {
                int16_t dx = cx_l - x, dy = cy_t - y;
                if (dx * dx + dy * dy > r_sq) continue;
            } else if (x > cx_r && y < cy_t) {
                int16_t dx = x - cx_r, dy = cy_t - y;
                if (dx * dx + dy * dy > r_sq) continue;
            } else if (x < cx_l && y > cy_b) {
                int16_t dx = cx_l - x, dy = y - cy_b;
                if (dx * dx + dy * dy > r_sq) continue;
            } else if (x > cx_r && y > cy_b) {
                int16_t dx = x - cx_r, dy = y - cy_b;
                if (dx * dx + dy * dy > r_sq) continue;
            }

            /* 2. 上眼睑倾角裁切 */
            int16_t y_cut_top = y0 + top_lid + (top_slant * inner_sign * (x - cx)) / 16;
            if (y <= y_cut_top) continue;

            /* 3. 下眼睑倾角裁切 */
            int16_t y_cut_bot = y1 - bottom_lid - (bottom_slant * inner_sign * (x - cx)) / 16;
            if (y >= y_cut_bot) continue;

            /* 4. 瞳孔与高光特效 */
            if (pupil_style == 1) {
                /* 标志性右上/左上双高光 (萌系机器人光泽) */
                int16_t gx = cx + ((screen == 0) ? 8 : -8);
                int16_t gy = cy - 8;
                int16_t d2 = (x - gx) * (x - gx) + (y - gy) * (y - gy);
                if (d2 <= 16) {
                    continue; /* 主反射高光镂空 */
                }
                int16_t gx2 = gx + ((screen == 0) ? -7 : 7);
                int16_t gy2 = gy + 8;
                if ((x - gx2) * (x - gx2) + (y - gy2) * (y - gy2) <= 3) {
                    continue; /* 次级细小反光点 */
                }
            } else if (pupil_style == 2) {
                /* 震惊收缩黑瞳孔 (中心黑色镂空 + 核心白亮点) */
                int16_t d2 = (x - cx) * (x - cx) + (y - cy) * (y - cy);
                if (d2 <= 36 && d2 > 4) {
                    continue;
                }
            } else if (pupil_style == 3) {
                /* 斗鸡眼眩晕同心圆螺旋纹 */
                int16_t d = (x - cx) * (x - cx) + (y - cy) * (y - cy);
                uint8_t ring = ((frame_tick / 3) + d / 20) % 2;
                if (ring == 0 && d <= 300) {
                    continue;
                }
            }

            OLED_DrawPoint(x, y, OLED_COLOR_WHITE);
        }
    }
}

/**
 * @brief 绘制饱满跳动的赛博爱心
 */
static void Draw_Heart(uint8_t cx, uint8_t cy, uint8_t r)
{
    int16_t lx = cx - r;
    int16_t rx = cx + r;
    int16_t cy_lobe = cy - r / 2;

    /* 两个上半圆瓣 */
    OLED_FillCircle(lx, cy_lobe, r, OLED_COLOR_WHITE);
    OLED_FillCircle(rx, cy_lobe, r, OLED_COLOR_WHITE);

    /* 下半三角形延伸 */
    int16_t bottom_y = cy + 2 * r - 4;
    for (int16_t y = cy_lobe; y <= bottom_y; y++) {
        int16_t half_w = (bottom_y - y) * (2 * r) / (bottom_y - cy_lobe);
        if (half_w > 2 * r) half_w = 2 * r;
        OLED_DrawLine(cx - half_w, y, cx + half_w, y, OLED_COLOR_WHITE);
    }

    /* 内部小高光点 */
    OLED_DrawPoint(cx - r + 3, cy_lobe - 3, OLED_COLOR_BLACK);
    OLED_DrawPoint(cx - r + 4, cy_lobe - 3, OLED_COLOR_BLACK);
    OLED_DrawPoint(cx - r + 3, cy_lobe - 2, OLED_COLOR_BLACK);
}

int main(void)
{
    /* 硬件时钟超频与初始化 */
    SystemClock_Set64M_HSI();
    SysTick_Init_64M();
    LED_Init();
    KEY_Init();
    OLED_Init();
    OLED_SetDelay(0); /* 极限 80+ FPS 刷新 */

    uint8_t current_mode   = ROBOT_NEUTRAL;
    uint8_t auto_ai_mode   = 1;
    uint32_t next_mode_ms  = MOOD_HOLD_BASE_MS;

    /* 注视中心平滑追踪 (-16 ~ +16) */
    int8_t look_x = 0;
    int8_t look_y = 0;
    int8_t target_look_x = 0;
    int8_t target_look_y = 0;
    uint32_t next_look_ms = 2500;

    /* 眨眼系统 */
    uint8_t is_blinking = 0;
    uint8_t blink_depth = 0;
    uint32_t next_blink_ms = 2200;

    uint32_t frame_tick = 0;

    while (1) {
        g_seed += 37;
        uint8_t key = KEY_Scan();

        /* ================= 1. 按键响应 ================= */
        if (key == KEY_1) {
            /* K1: 下一个机器人表情 */
            current_mode = (current_mode + 1) % ROBOT_TOTAL;
            next_mode_ms = g_ms_ticks + MOOD_HOLD_BASE_MS + (Rand() % MOOD_HOLD_RAND_MS);
        } else if (key == KEY_2) {
            /* K2: 上一个机器人表情 */
            current_mode = (current_mode == 0) ? (ROBOT_TOTAL - 1) : (current_mode - 1);
            next_mode_ms = g_ms_ticks + MOOD_HOLD_BASE_MS + (Rand() % MOOD_HOLD_RAND_MS);
        } else if (key == KEY_3) {
            /* K3 (#): 戳一戳互动 (Poke)！触发立即眨眼与表情演进 */
            is_blinking = 1;
            blink_depth = 2;
            if (current_mode == ROBOT_NEUTRAL)         current_mode = ROBOT_LOVE;
            else if (current_mode == ROBOT_LOVE)       current_mode = ROBOT_HAPPY;
            else if (current_mode == ROBOT_HAPPY)      current_mode = ROBOT_SURPRISE;
            else if (current_mode == ROBOT_SURPRISE)    current_mode = ROBOT_SUSPICIOUS;
            else                                       current_mode = ROBOT_NEUTRAL;
            next_mode_ms = g_ms_ticks + MOOD_HOLD_BASE_MS + (Rand() % MOOD_HOLD_RAND_MS);
        } else if (key == KEY_4) {
            /* K4 (*): 切换 AUTO (自动漫游生活) / MANU (手动保持) */
            auto_ai_mode = !auto_ai_mode;
        }

        /* ================= 2. 自动生活状态机 ================= */
        if (auto_ai_mode && (g_ms_ticks > next_mode_ms)) {
            uint32_t hold_base = (current_mode == ROBOT_SLEEP) ? 14000 : MOOD_HOLD_BASE_MS;
            next_mode_ms = g_ms_ticks + hold_base + (Rand() % MOOD_HOLD_RAND_MS);

            uint8_t dice = Rand() % 10;
            if (dice <= 3) current_mode = ROBOT_NEUTRAL;
            else if (dice == 4) current_mode = ROBOT_HAPPY;
            else if (dice == 5) current_mode = ROBOT_LOVE;
            else if (dice == 6) current_mode = ROBOT_SUSPICIOUS;
            else if (dice == 7) current_mode = ROBOT_SCAN;
            else if (dice == 8) current_mode = ROBOT_ANGRY;
            else                current_mode = ROBOT_CROSS;
        }

        /* 独立注视微动周期 (每隔 2.5~4.5 秒自主环视) */
        if (g_ms_ticks > next_look_ms && current_mode != ROBOT_SLEEP && current_mode != ROBOT_CROSS) {
            next_look_ms = g_ms_ticks + 2500 + (Rand() % 2500);
            target_look_x = (int8_t)(Rand() % 29) - 14; /* -14 ~ +14 */
            target_look_y = (int8_t)(Rand() % 17) - 8;  /* -8 ~ +8 */
        }

        /* 自然眨眼周期 */
        if (g_ms_ticks > next_blink_ms && !is_blinking && current_mode != ROBOT_SLEEP && current_mode != ROBOT_SCAN) {
            is_blinking = 1;
            blink_depth = 2;
            next_blink_ms = g_ms_ticks + 2200 + (Rand() % 3200);
        }

        /* 眨眼步进 */
        if (is_blinking) {
            blink_depth += 6;
            if (blink_depth >= 46) {
                is_blinking = 0;
                blink_depth = 0;
            }
        }

        /* 注视平滑阻尼逼近 */
        if (look_x < target_look_x) look_x++;
        else if (look_x > target_look_x) look_x--;
        if (look_y < target_look_y) look_y++;
        else if (look_y > target_look_y) look_y--;

        /* 板载 LED 状态指示联动 */
        if (current_mode == ROBOT_ANGRY) {
            LED_Set((frame_tick % 6) < 3); /* 狂暴频闪 */
        } else if (current_mode == ROBOT_LOVE) {
            uint32_t t = g_ms_ticks % 1000;
            LED_Set((t < 90) || (t > 200 && t < 290)); /* 心跳律动双闪 */
        } else if (current_mode == ROBOT_SCAN) {
            LED_Set((frame_tick % 16) < 4);
        } else if (current_mode == ROBOT_SLEEP) {
            LED_Set(0);
        } else {
            LED_Set(is_blinking);
        }

        /* ================= 3. 双屏画面渲染 ================= */
        OLED_ClearScreen(OLED_SCREEN_L);
        OLED_ClearScreen(OLED_SCREEN_R);

        /* 呼吸律动量 (微小呼吸缩放 0~2px) */
        int8_t breath = ((frame_tick / 8) % 4 < 2) ? 1 : 0;

        /* ---------- 逐屏渲染大眼 ---------- */
        for (uint8_t screen = 0; screen < 2; screen++) {
            OLED_SelectScreen(screen);

            /* 绘制四角赛博包角 */
            Draw_Corner_Brackets();

            /* 顶部极简信息指示 */
            if (screen == OLED_SCREEN_L) {
                OLED_ShowString(10, 4, "[L] CYBER-EYE", 8, OLED_COLOR_WHITE);
            } else {
                OLED_ShowString(8, 4, ROBOT_NAMES[current_mode], 8, OLED_COLOR_WHITE);
                if (auto_ai_mode) OLED_ShowString(96, 4, "AUTO", 8, OLED_COLOR_WHITE);
                else              OLED_ShowString(96, 4, "MANU", 8, OLED_COLOR_WHITE);
            }

            /* 基准大眼中心与尺寸 */
            int16_t cx = 64 + look_x;
            int16_t cy = 34 + look_y;
            int16_t eye_w = 68 + breath;
            int16_t eye_h = 44 + breath;
            int16_t eye_r = 14;

            switch (current_mode) {
                case ROBOT_NEUTRAL:
                    /* 1. 经典赛博大眼: 灵动注视 + 萌系双高光 */
                    Draw_Robot_Eye(screen, cx, cy, eye_w, eye_h, eye_r,
                                   is_blinking ? blink_depth : 0, 0,
                                   0, 0, 1, frame_tick);
                    break;

                case ROBOT_HAPPY: {
                    /* 2. 喜笑颜开: 下眼睑大幅上提成月牙弯笑弧 */
                    int16_t bottom_smile = is_blinking ? 44 : 24;
                    Draw_Robot_Eye(screen, cx, cy - 2, eye_w, eye_h, eye_r,
                                   is_blinking ? blink_depth : 2, 0,
                                   bottom_smile, -6, 0, frame_tick);
                    /* 双颊萌萌斜杠红晕 */
                    int16_t blush_x = (screen == 0) ? 14 : 100;
                    OLED_DrawLine(blush_x,     48, blush_x + 3, 44, OLED_COLOR_WHITE);
                    OLED_DrawLine(blush_x + 5, 48, blush_x + 8, 44, OLED_COLOR_WHITE);
                    break;
                }

                case ROBOT_ANGRY:
                    /* 3. 机械狂暴: 上眼睑内八字下压 (怒眉刀锋眼神) */
                    Draw_Robot_Eye(screen, cx + ((screen == 0) ? 4 : -4), cy, eye_w, eye_h, eye_r,
                                   is_blinking ? blink_depth : 18, 20,
                                   4, -4, 0, frame_tick);
                    /* 额头怒筋 💢 */
                    if (screen == OLED_SCREEN_R) {
                        OLED_DrawLine(108, 14, 116, 14, OLED_COLOR_WHITE);
                        OLED_DrawLine(108, 18, 116, 18, OLED_COLOR_WHITE);
                        OLED_DrawLine(110, 12, 110, 20, OLED_COLOR_WHITE);
                        OLED_DrawLine(114, 12, 114, 20, OLED_COLOR_WHITE);
                    }
                    break;

                case ROBOT_LOVE: {
                    /* 4. 爱心桃目: 双屏跳动大爱心动画 */
                    uint32_t t = g_ms_ticks % 1000;
                    uint8_t heart_r = 11;
                    if (t < 120)       heart_r = 13;
                    else if (t < 220)  heart_r = 11;
                    else if (t < 340)  heart_r = 14;
                    Draw_Heart(64, 34, heart_r);
                    break;
                }

                case ROBOT_CROSS: {
                    /* 5. 斗鸡眩晕: 左眼极限往右，右眼极限往左对眼靠近 + 螺旋眩晕纹 */
                    int16_t cross_cx = (screen == 0) ? (64 + 20) : (64 - 20);
                    int16_t cross_cy = 34 + ((frame_tick % 16 < 8) ? 1 : -1);
                    Draw_Robot_Eye(screen, cross_cx, cross_cy, 60, 44, 16,
                                   is_blinking ? blink_depth : 0, 0,
                                   0, 0, 3, frame_tick);
                    break;
                }

                case ROBOT_SUSPICIOUS:
                    /* 6. 挑眉审视: 左眼大睁挑起，右眼眯成细长横缝 */
                    if (screen == 0) {
                        /* 左眼挑起圆睁 */
                        Draw_Robot_Eye(screen, 64, 30, 64, 46, 14,
                                       is_blinking ? blink_depth : 0, 0,
                                       0, 0, 1, frame_tick);
                        /* 挑起的眉毛 */
                        OLED_DrawLine(32, 8, 96, 6, OLED_COLOR_WHITE);
                        OLED_DrawLine(32, 9, 96, 7, OLED_COLOR_WHITE);
                    } else {
                        /* 右眼眯眼审视 */
                        Draw_Robot_Eye(screen, 64, 36, 68, 20, 6,
                                       is_blinking ? blink_depth : 2, 0,
                                       2, 0, 0, frame_tick);
                        /* 压低的平眉毛 */
                        OLED_DrawLine(30, 20, 98, 22, OLED_COLOR_WHITE);
                        OLED_DrawLine(30, 21, 98, 23, OLED_COLOR_WHITE);
                    }
                    break;

                case ROBOT_SAD: {
                    /* 7. 委屈落泪: 外八字耷拉眼角 (萌系悲伤) */
                    Draw_Robot_Eye(screen, cx, cy + 2, eye_w, eye_h, eye_r,
                                   is_blinking ? blink_depth : 18, -16,
                                   6, 0, 1, frame_tick);
                    /* 左眼滑落的像素泪珠 */
                    if (screen == 0) {
                        uint8_t tear_y = 38 + ((g_ms_ticks / 40) % 20);
                        OLED_DrawLine(92, tear_y, 92, tear_y + 3, OLED_COLOR_WHITE);
                        OLED_DrawPoint(91, tear_y + 2, OLED_COLOR_WHITE);
                        OLED_DrawPoint(93, tear_y + 2, OLED_COLOR_WHITE);
                    }
                    break;
                }

                case ROBOT_SURPRISE:
                    /* 8. 震惊目瞪: 撑大至极限的惊愕圆目 + 中心收缩惊吓瞳 */
                    Draw_Robot_Eye(screen, 64, 34, 76, 52, 18,
                                   is_blinking ? blink_depth : 0, 0,
                                   0, 0, 2, frame_tick);
                    break;

                case ROBOT_SCAN: {
                    /* 9. 赛博红外雷达扫描模式: 准星十字 + 往复扫射光束 */
                    /* 中心十字瞄准镜 */
                    OLED_DrawCircle(64, 34, 18, OLED_COLOR_WHITE);
                    OLED_DrawCircle(64, 34, 10, OLED_COLOR_WHITE);
                    OLED_DrawLine(64, 10, 64, 58, OLED_COLOR_WHITE);
                    OLED_DrawLine(24, 34, 104, 34, OLED_COLOR_WHITE);
                    /* 扫描光束 */
                    uint8_t scan_y = 12 + ((frame_tick * 2) % 42);
                    OLED_DrawLine(16, scan_y, 112, scan_y, OLED_COLOR_WHITE);
                    if (scan_y > 12) {
                        OLED_DrawLine(24, scan_y - 1, 104, scan_y - 1, OLED_COLOR_WHITE);
                    }
                    break;
                }

                case ROBOT_SLEEP: {
                    /* 10. 待机休眠: 平缓微弧的闭合发光呼吸光条 + 飘出 zZ 气泡 */
                    int16_t sleep_y = 34 + ((frame_tick / 16) % 3);
                    OLED_DrawLine(36, sleep_y, 92, sleep_y, OLED_COLOR_WHITE);
                    OLED_DrawLine(36, sleep_y + 1, 92, sleep_y + 1, OLED_COLOR_WHITE);
                    OLED_DrawLine(38, sleep_y + 2, 90, sleep_y + 2, OLED_COLOR_WHITE);

                    /* 漂浮睡眠气泡 */
                    if (screen == OLED_SCREEN_R) {
                        uint8_t zy = (frame_tick / 4) % 22;
                        OLED_ShowString(98, 30 - zy, "z", 8, OLED_COLOR_WHITE);
                        if (zy > 7) OLED_ShowString(108, 30 - zy - 7, "Z", 8, OLED_COLOR_WHITE);
                    }
                    break;
                }
            }

            /* 底部按键指引状态栏 */
            OLED_DrawLine(0, 55, 127, 55, OLED_COLOR_WHITE);
            if (screen == OLED_SCREEN_L) {
                OLED_ShowString(4, 56, "K1/2:Mood  K3:Poke", 8, OLED_COLOR_WHITE);
            } else {
                OLED_ShowString(4, 56, "K4:Auto/Manu  80FPS", 8, OLED_COLOR_WHITE);
            }
        }

        /* 极速刷新双屏 */
        OLED_UpdateAll();
        frame_tick++;
    }

    return 0;
}
