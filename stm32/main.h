/**
 * @file    main.h
 */
#ifndef __MAIN_H__
#define __MAIN_H__

#include "stm32f1xx_hal.h"

/* 声明外部句柄 */
extern I2C_HandleTypeDef hi2c1;
extern UART_HandleTypeDef huart1;
extern TIM_HandleTypeDef htim2;

#endif
