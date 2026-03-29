/**
 * @file    core.c
 * @brief   系统初始化：时钟配置、GPIO初始化
 */
#include "core.h"
#include <stdint.h>

/**
 * @brief  配置所有外设时钟
 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* 初始化内部高速/低速振荡器 */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState       = RCC_HSE_ON;          // 使用外部8MHz晶振
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;  // HSE 1分频
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL     = RCC_PLL_MUL9;        // 8MHz * 9 = 72MHz
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        while (1);  // 时钟配置失败，死循环
    }

    /* 初始化总线时钟 */
    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;     // HCLK = 72MHz
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;       // APB1 = 36MHz
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;       // APB2 = 72MHz
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
        while (1);
    }
}

/**
 * @brief  初始化所有GPIO
 *  PA9  = USART1_TX  (复用推挽输出)
 *  PA10 = USART1_RX  (浮空输入)
 *  PA0-PA3 = 按键1-4  (上拉输入)
 *  注意：PB6/PB7 不在 MX_GPIO_Init 中处理，
 *        由 OLED_Init 单独配置，避免与硬件 I2C1 引脚冲突
 */
void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* GPIO时钟 */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();

    /* USART1 TX (PA9) - 复用推挽 */
    GPIO_InitStruct.Pin   = GPIO_PIN_9;
    GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* USART1 RX (PA10) - 浮空输入 */
    GPIO_InitStruct.Pin   = GPIO_PIN_10;
    GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* 按键 (PA0-PA3) - 上拉输入，默认高电平，按下为低 */
    GPIO_InitStruct.Pin   = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull  = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}
