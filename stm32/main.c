/**
 * @file    main.c
 * @brief   STM32F103C8T6 主程序
 *          资产租借管理硬件端主入口
 *
 * 功能概述：
 *   - 上电初始化 OLED、按键、串口
 *   - 主循环：接收 PC 串口命令 -> OLED 显示确认界面 -> 等待按键 -> 返回结果
 *
 * 接线说明（重要）：
 *   STM32          <->  外设
 *   PA9  (USART1_TX) <->  CP2102 RX
 *   PA10 (USART1_RX) <->  CP2102 TX
 *   PB6  (I2C1_SCL)  <->  OLED SCL
 *   PB7  (I2C1_SDA)  <->  OLED SDA
 *   PA0              <->  按键1（确认）
 *   PA1              <->  按键2（取消）
 *   PA2              <->  按键3（上翻）
 *   PA3              <->  按键4（下翻）
 *   3.3V             <->  OLED VCC
 *   GND              <->  OLED GND
 *   (CP2102 模块也要共地)
 */

#include "main.h"
#include "core.h"
#include "oled.h"
#include "button.h"
#include "serial.h"
#include <string.h>
#include <stdio.h>

// =============== 全局变量 ===============
UART_HandleTypeDef huart1;  // USART1 句柄（串口通信）
I2C_HandleTypeDef hi2c1;    // I2C1 句柄（OLED用）

// 状态机
typedef enum {
    STATE_IDLE,           // 空闲：显示主界面
    STATE_WAIT_CONFIRM,   // 等待用户按键确认
} SystemState;

static SystemState g_state = STATE_IDLE;
static char g_pending_asset[64] = {0};
static char g_pending_action[8] = {0};
static uint32_t g_confirm_start_tick = 0;
static uint32_t g_idle_start_tick = 0;

// =============== 定时器（用于按键扫描）===============
static void MX_TIM2_Init(void);
TIM_HandleTypeDef htim2;

// =============== I2C1 初始化（OLED用）===============
static void MX_I2C1_Init(void)
{
    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 400000;       // 400kHz 快速模式
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

    if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
        // 初始化失败，LED快闪提示
        while (1) {
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
            HAL_Delay(100);
        }
    }
}

// =============== USART1 初始化 ===============
static void MX_USART1_Init(void)
{
    // 串口初始化由 Serial_Init() 完成
    // 此处仅声明，供 HAL 使用
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 9600;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK) {
        while (1);
    }
}

// =============== USART1 中断处理函数 ===============
void USART1_IRQHandler(void)
{
    Serial_IRQHandler();
}

// =============== 显示主界面 ===============
static void ShowIdleScreen(void)
{
    OLED_Clear();
    OLED_ShowLine(0, "=== Asset Sys ===");
    OLED_ShowLine(1, "System Ready");
    OLED_Refresh();
}

// =============== 状态机处理 ===============
static void HandleIdleState(void)
{
    // 检查是否有新命令
    Serial_ProcessLoop();

    if (g_pending_cmd.valid) {
        if (strcmp(g_pending_cmd.cmd, "PING") == 0) {
            Serial_SendPong();
            Serial_ClearCommand();
            return;
        }

        // 有效操作命令
        strncpy(g_pending_action, g_pending_cmd.cmd, 7);
        g_pending_action[7] = '\0';
        strncpy(g_pending_asset, g_pending_cmd.asset_code, 63);
        g_pending_asset[63] = '\0';

        Serial_ClearCommand();

        // 切换到等待确认状态
        g_state = STATE_WAIT_CONFIRM;
        g_confirm_start_tick = HAL_GetTick();

        // 显示确认界面（从30秒倒计时）
        OLED_ShowConfirmUI(g_pending_action, g_pending_asset, 30);
        return;
    }
}

static void HandleWaitConfirmState(void)
{
    // 更新按键状态
    Button_Update();

    // 实时倒计时（每秒刷新一次屏幕，避免I2C过载）
    uint32_t elapsed = (HAL_GetTick() - g_confirm_start_tick) / 1000;
    static uint32_t last_sec = 999;  // 初始化为一个不可能的值
    if (elapsed < 30) {
        if (elapsed != last_sec) {
            last_sec = elapsed;
            uint8_t sec = (uint8_t)(30 - elapsed);
            OLED_ShowConfirmUI(g_pending_action, g_pending_asset, sec);
        }
    }

    // 检查按键
    if (Button_IsJustPressed(BTN_CONFIRM)) {
        // 用户按了确认键
        OLED_ShowResultUI("OK");
        HAL_Delay(2000);
        Serial_SendOK(g_pending_asset);
        g_state = STATE_IDLE;
        return;
    }

    if (Button_IsJustPressed(BTN_CANCEL)) {
        // 用户按了取消键
        OLED_ShowResultUI("NO");
        HAL_Delay(2000);
        Serial_SendNO(g_pending_asset);
        g_state = STATE_IDLE;
        return;
    }

    // 超时30秒
    if (elapsed >= 30) {
        OLED_ShowResultUI("TIMEOUT");
        HAL_Delay(2000);
        Serial_SendTimeout(g_pending_asset);
        g_state = STATE_IDLE;
        return;
    }
}

// =============== 主程序入口 ===============
int main(void)
{
    // HAL 库初始化
    HAL_Init();

    // 系统时钟配置
    SystemClock_Config();

    // GPIO初始化
    MX_GPIO_Init();

    // 外设初始化
    MX_USART1_Init();   // USART1（串口通信，Serial_Init 依赖它）
    MX_TIM2_Init();     // TIM2（按键扫描，10ms 周期中断）

    Button_Init();
    Serial_Init();

    // OLED 使用软件I2C，无需 I2C1 硬件外设（PB6/PB7 由 OLED_Init 单独配置为GPIO开漏）
    OLED_Init();

    // 调试打印：OLED 初始化检查（通过 UART 输出）
    char dbgbuf[128];
    int dbglen = snprintf(dbgbuf, sizeof(dbgbuf),
        "[DBG] OLED_DBG_CNT=%lu OLED_I2C_BYTES=%lu\r\n",
        (unsigned long)OLED_DBG_CNT, (unsigned long)OLED_I2C_BYTES);
    HAL_UART_Transmit(&huart1, (uint8_t*)dbgbuf, dbglen, 100);

    // 启动定时器2（用于按键扫描，10ms周期）
    HAL_TIM_Base_Start_IT(&htim2);

    // 初始界面
    ShowIdleScreen();
    g_idle_start_tick = HAL_GetTick();

    // 主循环
    while (1) {
        switch (g_state) {
            case STATE_IDLE:
                HandleIdleState();
                break;
            case STATE_WAIT_CONFIRM:
                HandleWaitConfirmState();
                break;
        }

        // 每秒更新一次空闲界面（显示等待信息）
        if (g_state == STATE_IDLE) {
            if (HAL_GetTick() - g_idle_start_tick > 5000) {
                ShowIdleScreen();
                g_idle_start_tick = HAL_GetTick();
            }
        }

        // 调试：每5秒通过 UART 打印 OLED 计数器（验证 OLED 是否持续工作）
        static uint32_t last_dbg_tick = 0;
        if (HAL_GetTick() - last_dbg_tick > 5000) {
            last_dbg_tick = HAL_GetTick();
            char dbgbuf[128];
            int dbglen = snprintf(dbgbuf, sizeof(dbgbuf),
                "[DBG] OLED_DBG_CNT=%lu OLED_I2C_BYTES=%lu\r\n",
                (unsigned long)OLED_DBG_CNT, (unsigned long)OLED_I2C_BYTES);
            HAL_UART_Transmit(&huart1, (uint8_t*)dbgbuf, dbglen, 100);
        }

        // 小的延时，防止CPU空转
        HAL_Delay(50);
    }
}

// =============== 定时器2初始化（10ms中断，用于按键扫描）===============
static void MX_TIM2_Init(void)
{
    TIM_ClockConfigTypeDef sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig = {0};

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 720 - 1;       // 72MHz / 720 = 100kHz
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 1000 - 1;          // 100kHz / 1000 = 100Hz -> 10ms
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    if (HAL_TIM_Base_Init(&htim2) != HAL_OK) {
        while (1);
    }

    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK) {
        while (1);
    }

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK) {
        while (1);
    }
}

// =============== 定时器2中断回调（按键扫描）===============
void TIM2_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim2);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim == &htim2) {
        // 10ms 定时到了，更新按键状态
        Button_Update();
    }
}

// =============== 弱定义：HAL_I2C_Init 需要此函数 ===============
void HAL_I2C_MspInit(I2C_HandleTypeDef* hi2c)
{
    if (hi2c->Instance == I2C1) {
        __HAL_RCC_I2C1_CLK_ENABLE();
        // PB6, PB7 由 MX_GPIO_Init() 配置
    }
}

void HAL_UART_MspInit(UART_HandleTypeDef* huart)
{
    if (huart->Instance == USART1) {
        __HAL_RCC_USART1_CLK_ENABLE();
        // PA9, PA10 由 MX_GPIO_Init() 配置
    }
}
