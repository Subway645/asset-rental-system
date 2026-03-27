/**
 * @file    button.c
 * @brief   4个按键扫描（消抖处理，10ms定时轮询）
 *          按键连接：PA0=确认, PA1=取消, PA2=上, PA3=下
 */
#include "button.h"
#include "core.h"
#include <stdbool.h>

// =============== 按键配置 ===============
#define BUTTON_COUNT    4
#define DEBOUNCE_MS     10   // 消抖阈值（ms）
#define LONGPRESS_MS    1000 // 长按阈值（ms）

// 按键GPIO引脚定义
static const uint16_t BUTTON_PINS[BUTTON_COUNT] = {
    GPIO_PIN_0,   // BTN_CONFIRM  - PA0
    GPIO_PIN_1,   // BTN_CANCEL   - PA1
    GPIO_PIN_2,   // BTN_UP       - PA2
    GPIO_PIN_3,   // BTN_DOWN     - PA3
};

// 按键状态
typedef struct {
    GPIO_PinState last_state;     // 上次读取状态
    GPIO_PinState current_state;  // 当前稳定状态
    uint16_t pressed_ms;          // 按下持续时间
    bool just_pressed;            // 刚按下（单次触发）
    bool just_released;           // 刚释放
    bool long_pressed;            // 长按触发
} ButtonState;

static ButtonState g_buttons[BUTTON_COUNT];

// =============== 初始化 ===============
void Button_Init(void)
{
    for (int i = 0; i < BUTTON_COUNT; i++) {
        g_buttons[i].last_state = GPIO_PIN_SET;
        g_buttons[i].current_state = GPIO_PIN_SET;
        g_buttons[i].pressed_ms = 0;
        g_buttons[i].just_pressed = false;
        g_buttons[i].just_released = false;
        g_buttons[i].long_pressed = false;
    }
}

// =============== 轮询更新（每10ms调用一次）===============
void Button_Update(void)
{
    for (int i = 0; i < BUTTON_COUNT; i++) {
        GPIO_PinState raw = HAL_GPIO_ReadPin(GPIOA, BUTTON_PINS[i]);

        if (raw != g_buttons[i].last_state) {
            // 状态变化，重置消抖计时
            g_buttons[i].pressed_ms = 0;
            g_buttons[i].last_state = raw;
        } else {
            // 状态稳定，累加计时
            if (raw == GPIO_PIN_RESET) {  // 按下为低电平
                g_buttons[i].pressed_ms += DEBOUNCE_MS;

                if (g_buttons[i].pressed_ms >= DEBOUNCE_MS && !g_buttons[i].just_pressed) {
                    g_buttons[i].just_pressed = true;
                }

                if (g_buttons[i].pressed_ms >= LONGPRESS_MS && !g_buttons[i].long_pressed) {
                    g_buttons[i].long_pressed = true;
                }
            }
        }

        // 状态同步（消抖后的稳定状态）
        if (g_buttons[i].pressed_ms >= DEBOUNCE_MS) {
            g_buttons[i].current_state = GPIO_PIN_RESET;
        } else {
            g_buttons[i].current_state = GPIO_PIN_SET;
        }

        // 刚释放检测
        if (g_buttons[i].current_state == GPIO_PIN_SET &&
            g_buttons[i].pressed_ms > 0 &&
            !g_buttons[i].just_released) {
            g_buttons[i].just_released = true;
        } else if (g_buttons[i].current_state == GPIO_PIN_RESET) {
            g_buttons[i].just_released = false;
        }
    }
}

// =============== 查询接口 ===============

bool Button_IsPressed(Button_ID id)
{
    if (id >= BUTTON_COUNT) return false;
    return g_buttons[id].current_state == GPIO_PIN_RESET;
}

bool Button_IsJustPressed(Button_ID id)
{
    if (id >= BUTTON_COUNT) return false;
    if (g_buttons[id].just_pressed) {
        g_buttons[id].just_pressed = false;  // 清除标志
        return true;
    }
    return false;
}

bool Button_IsJustReleased(Button_ID id)
{
    if (id >= BUTTON_COUNT) return false;
    if (g_buttons[id].just_released) {
        g_buttons[id].just_released = false;
        return true;
    }
    return false;
}

bool Button_IsLongPressed(Button_ID id)
{
    if (id >= BUTTON_COUNT) return false;
    return g_buttons[id].long_pressed;
}

// =============== 阻塞等待按键（用于确认界面）===============
Button_ID Button_WaitForPress(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    while (HAL_GetTick() - start < timeout_ms) {
        Button_Update();
        for (int i = 0; i < BUTTON_COUNT; i++) {
            if (Button_IsJustPressed((Button_ID)i)) {
                return (Button_ID)i;
            }
        }
        HAL_Delay(10);
    }
    return BTN_NONE;  // 超时
}
