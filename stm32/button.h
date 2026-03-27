/**
 * @file    button.h
 */
#ifndef __BUTTON_H__
#define __BUTTON_H__

#include <stdint.h>
#include <stdbool.h>
#include "stm32f1xx_hal.h"

/* 按键ID */
typedef enum {
    BTN_CONFIRM = 0,  // PA0 - 确认/确定
    BTN_CANCEL  = 1,  // PA1 - 取消/返回
    BTN_UP      = 2,  // PA2 - 上翻
    BTN_DOWN    = 3,  // PA3 - 下翻
    BTN_NONE    = 4,  // 无按键
} Button_ID;

/* 初始化 */
void Button_Init(void);

/* 轮询更新（每10ms调用一次）*/
void Button_Update(void);

/* 查询接口 */
bool Button_IsPressed(Button_ID id);
bool Button_IsJustPressed(Button_ID id);
bool Button_IsJustReleased(Button_ID id);
bool Button_IsLongPressed(Button_ID id);

/* 阻塞等待按键（用于确认界面）*/
Button_ID Button_WaitForPress(uint32_t timeout_ms);

#endif
