/**
 * @file    oled.h
 */
#ifndef __OLED_H__
#define __OLED_H__

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* 外部声明 hi2c1（在 main.c 中由 HAL 库生成） */
extern I2C_HandleTypeDef hi2c1;

/* 初始化 */
void OLED_Init(void);

/* 清屏和刷新 */
void OLED_Clear(void);
void OLED_Refresh(void);

/* 文本显示 */
void OLED_ShowString(uint8_t page, uint8_t col, const char *str);
void OLED_ShowLine(uint8_t line, const char *str);
void OLED_ShowNum(uint8_t page, uint8_t col, uint32_t num);
void OLED_ShowAssetCode(uint8_t page, uint8_t col, const char *code);

/* 确认界面 */
void OLED_ShowConfirmUI(const char *action, const char *asset_code, uint8_t sec);
void OLED_ShowResultUI(const char *result);

/* 进度条 */
void OLED_ShowProgressBar(uint8_t page, uint8_t total_sec, uint8_t remain_sec);

#endif
