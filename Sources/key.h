#ifndef __KEY_H
#define __KEY_H

#include <stdint.h>

#define KEY_NONE   0
#define KEY_1      1   /* K1: 上 (PA0) */
#define KEY_2      2   /* K2: 下 (PA1) */
#define KEY_3      3   /* K3: # 确认/开关 (PA2) */
#define KEY_4      4   /* K4: * 重置/辅助 (PA3) */

void KEY_Init(void);
uint8_t KEY_Scan(void);
uint8_t KEY_GetPin(uint8_t index);

#endif /* __KEY_H */
