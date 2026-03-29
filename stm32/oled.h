/**
 * @file    oled.h
 */
#ifndef __OLED_H__
#define __OLED_H__

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "stm32f1xx_hal.h"

/* 调试计数器（main.c 可读取并通过 UART 打印）*/
extern volatile uint32_t OLED_DBG_CNT;
extern volatile uint32_t OLED_I2C_BYTES;

/* 初始化 */
void OLED_Init(void);

/* 显示模式 */
void OLED_ColorTurn(uint8_t i);    // 0正常 1反色
void OLED_DisplayTurn(uint8_t i); // 0正常 1翻转180度

/* 清屏和刷新 */
void OLED_Clear(void);
void OLED_Refresh(void);

/* 文本显示 */
void OLED_ShowChar(uint8_t x, uint8_t y, char chr);
void OLED_ShowString(uint8_t x, uint8_t y, const char *str);
void OLED_ShowLine(uint8_t line, const char *str);
void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num);
void OLED_ShowAssetCode(uint8_t x, uint8_t y, const char *code);

/* 确认界面 */
void OLED_ShowConfirmUI(const char *action, const char *asset_code, uint8_t sec);
void OLED_ShowResultUI(const char *result);

#endif
