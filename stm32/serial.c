/**
 * @file    serial.c
 * @brief   USART1 串口通信（中断接收，DMA可选）
 *          波特率9600，8数据位，1停止位，无校验
 *          PA9=TX, PA10=RX
 *
 * 通信协议：
 *   PC -> STM32:  IN,<asset_code>\n  /  OUT,<asset_code>\n  /  RET,<asset_code>\n  /  PING\n
 *   STM32 -> PC:  OK,<asset_code>,<timestamp>\n  /  NO,<asset_code>,<timestamp>\n  /  TIMEOUT,<asset_code>\n  /  PONG\n
 * 使用方法：
 *   1. 调用 Serial_Init() 初始化
 *   2. 在主循环或定时器中调用 Serial_ProcessLoop()
 *   3. 收到命令后自动解析，更新 g_pending_cmd 结构体
 */
#include "serial.h"
#include "core.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

// =============== 硬件定义 ===============
#define USART_INSTANCE       USART1
#define USART_BAUD           9600
#define USART_TX_PIN         GPIO_PIN_9
#define USART_RX_PIN         GPIO_PIN_10

// =============== 环形缓冲区 ===============
#define RX_BUF_SIZE   128

typedef struct {
    uint8_t data[RX_BUF_SIZE];
    volatile uint16_t head;   // 写指针
    volatile uint16_t tail;   // 读指针
} RingBuffer;

static RingBuffer g_rx_buf;

// =============== 命令解析 ===============
static char g_rx_line[RX_BUF_SIZE];     // 当前正在组包的行
static uint8_t g_rx_line_len = 0;
static bool g_line_ready = false;

PendingCommand g_pending_cmd;  // 解析后的命令（外部可读）

// =============== 发送函数 ===============
void Serial_SendString(const char *str)
{
    while (*str) {
        while (!__HAL_UART_GET_FLAG(&huart1, UART_FLAG_TXE));
        USART_INSTANCE->DR = (uint8_t)(*str++);
    }
}

void Serial_SendLine(const char *str)
{
    Serial_SendString(str);
    Serial_SendString("\r\n");
}

// =============== USART1 中断处理（放在 stm32f1xx_it.c 的 USART1_IRQHandler 中调用）===============
void Serial_IRQHandler(void)
{
    // 接收中断
    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE)) {
        uint8_t byte = (uint8_t)(USART_INSTANCE->DR & 0xFF);

        // 存入环形缓冲区
        uint16_t next = (g_rx_buf.head + 1) % RX_BUF_SIZE;
        if (next != g_rx_buf.tail) {
            g_rx_buf.data[g_rx_buf.head] = byte;
            g_rx_buf.head = next;
        }
    }
}

// =============== 读取一行（阻塞直到遇到换行）===============
static bool Serial_ReadLine(char *out, uint8_t max_len)
{
    while (g_rx_buf.tail != g_rx_buf.head) {
        uint8_t byte = g_rx_buf.data[g_rx_buf.tail];
        g_rx_buf.tail = (g_rx_buf.tail + 1) % RX_BUF_SIZE;

        // 忽略 \r
        if (byte == '\r') continue;

        // 遇到 \n 表示行结束
        if (byte == '\n') {
            out[g_rx_line_len] = '\0';
            g_rx_line_len = 0;
            return true;
        }

        // 累积字符
        if (g_rx_line_len < max_len - 1) {
            out[g_rx_line_len++] = (char)byte;
        } else {
            // 行太长，截断
            g_rx_line_len = 0;
            return false;
        }
    }
    return false;
}

// =============== 解析一行命令 ===============
static bool ParseCommand(const char *line)
{
    // 格式: CMD,ASSETCODE
    // 例如: IN,ASSET001
    if (strlen(line) < 3) return false;

    char cmd[8] = {0};

    // 查找逗号
    const char *comma = strchr(line, ',');
    if (comma == NULL) {
        // PING 等无参数命令
        if (strcmp(line, "PING") == 0) {
            strcpy(g_pending_cmd.cmd, "PING");
            g_pending_cmd.asset_code[0] = '\0';
            g_pending_cmd.valid = true;
            return true;
        }
        return false;
    }

    // 分离命令和资产编号
    size_t cmd_len = comma - line;
    if (cmd_len > 7) cmd_len = 7;
    strncpy(cmd, line, cmd_len);
    cmd[cmd_len] = '\0';

    strncpy(g_pending_cmd.asset_code, comma + 1, 63);
    g_pending_cmd.asset_code[63] = '\0';

    strcpy(g_pending_cmd.cmd, cmd);
    g_pending_cmd.valid = true;
    return true;
}

// =============== 获取当前时间戳字符串 ===============
static void GetTimestamp(char *buf, size_t len)
{
    // 使用RTC或HAL_GetTick()生成时间戳
    // 这里简化为 HAL_GetTick() 累计毫秒数
    uint32_t ms = HAL_GetTick();
    uint32_t sec = ms / 1000;
    uint32_t min = sec / 60;
    uint32_t hr = min / 60;
    uint32_t day = hr / 24;

    // 格式：YYYYMMDDHHMMSS（简化版，用运行时间代替）
    snprintf(buf, len, "%03d%02d%02d%02d%02d",
             (unsigned int)(day % 10),
             (unsigned int)(hr % 24),
             (unsigned int)(min % 60),
             (unsigned int)(sec % 60),
             (unsigned int)(ms % 1000) / 10);
}

// =============== 初始化 ===============
void Serial_Init(void)
{
    // 初始化环形缓冲区
    g_rx_buf.head = 0;
    g_rx_buf.tail = 0;
    g_rx_line_len = 0;
    g_pending_cmd.valid = false;
    g_pending_cmd.cmd[0] = '\0';
    g_pending_cmd.asset_code[0] = '\0';

    // USART1 配置：9600bps, 8N1
    __HAL_RCC_USART1_CLK_ENABLE();

    huart1.Instance = USART1;
    huart1.Init.BaudRate = USART_BAUD;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart1) != HAL_OK) {
        while (1);  // 初始化失败
    }

    // 使能接收中断
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_RXNE);

    // 发送就绪提示
    HAL_Delay(100);
    Serial_SendLine("STM32 READY");
}

// =============== 主循环处理 ===============
void Serial_ProcessLoop(void)
{
    if (g_pending_cmd.valid) return;  // 上条命令未处理

    char line[RX_BUF_SIZE];
    if (Serial_ReadLine(line, sizeof(line))) {
        if (ParseCommand(line)) {
            // 命令已解析，保存到 g_pending_cmd
            // 主循环检测到 valid=true 后处理
        }
    }
}

// =============== 发送确认响应 ===============
void Serial_SendOK(const char *asset_code)
{
    char ts[32];
    GetTimestamp(ts, sizeof(ts));
    char resp[128];
    snprintf(resp, sizeof(resp), "OK,%s,%s", asset_code, ts);
    Serial_SendLine(resp);
}

void Serial_SendNO(const char *asset_code)
{
    char ts[32];
    GetTimestamp(ts, sizeof(ts));
    char resp[128];
    snprintf(resp, sizeof(resp), "NO,%s,%s", asset_code, ts);
    Serial_SendLine(resp);
}

void Serial_SendTimeout(const char *asset_code)
{
    char resp[128];
    snprintf(resp, sizeof(resp), "TIMEOUT,%s", asset_code);
    Serial_SendLine(resp);
}

void Serial_SendPong(void)
{
    Serial_SendLine("PONG");
}

// =============== 清除当前命令 ===============
void Serial_ClearCommand(void)
{
    g_pending_cmd.valid = false;
}
