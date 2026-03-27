/**
 * @file    serial.h
 */
#ifndef __SERIAL_H__
#define __SERIAL_H__

#include <stdint.h>
#include <stdbool.h>
#include "stm32f1xx_hal.h"

/* 待处理的命令结构 */
typedef struct {
    char cmd[8];          // 命令类型：IN / OUT / RET / PING
    char asset_code[64];  // 资产编号
    bool valid;           // 是否有有效命令
} PendingCommand;

/* 外部声明 uart句柄（在 main.c 中由 CubeMX 生成） */
extern UART_HandleTypeDef huart1;

/* 全局命令对象（主循环读取） */
extern PendingCommand g_pending_cmd;

/* 初始化 */
void Serial_Init(void);

/* 主循环调用，处理接收数据 */
void Serial_ProcessLoop(void);

/* 中断处理（由 USART1_IRQHandler 调用）*/
void Serial_IRQHandler(void);

/* 发送响应 */
void Serial_SendString(const char *str);
void Serial_SendLine(const char *str);
void Serial_SendOK(const char *asset_code);
void Serial_SendNO(const char *asset_code);
void Serial_SendTimeout(const char *asset_code);
void Serial_SendPong(void);

/* 清除命令标志 */
void Serial_ClearCommand(void);

#endif
