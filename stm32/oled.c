/**
 * @file    oled.c
 * @brief   OLED 驱动（SSD1306，软件I2C，PB6=SCL / PB7=SDA）
 *          直接操作 BSRR/BRR 寄存器，不依赖 HAL，避免 I2C1 复用冲突
 *          完全对照中景园例程风格
 */
#include "oled.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// =============== 宏定义 ===============
// 直接操作 BSRR/BRR 寄存器（对照例程的 GPIO_SetBits/GPIO_ResetBits）
#define OLED_SCL_Clr()   (GPIOB->BRR = GPIO_PIN_6)   // PB6 = 0
#define OLED_SCL_Set()   (GPIOB->BSRR = GPIO_PIN_6)  // PB6 = 1
#define OLED_SDA_Clr()   (GPIOB->BRR = GPIO_PIN_7)   // PB7 = 0
#define OLED_SDA_Set()   (GPIOB->BSRR = GPIO_PIN_7) // PB7 = 1
#define OLED_RES_Clr()   /* 4针模块无RES */
#define OLED_RES_Set()   /* 4针模块无RES */
#define OLED_CMD  0
#define OLED_DATA 1

// I2C 地址：0x7A (部分0.91寸模块用这个地址，0x78不响应时请试这个)
#define OLED_I2C_ADDR  0x7A

// 0.91寸: 128列 x 32行 -> 4页
#define OLED_WIDTH   128
#define OLED_HEIGHT  32
#define OLED_PAGE    4

// =============== 调试计数器（供 main.c 通过 UART 打印）===============
// 记录顺序：init_enter, gpio_ok, cmd_AE, AF, clear, refresh_call, refresh_done, wr_byte, send_byte, show_confirm, show_result, confirm_refresh, result_refresh
extern volatile uint32_t OLED_DBG_CNT;
volatile uint32_t OLED_DBG_CNT = 0;
#define DBG_INC(n) do { OLED_DBG_CNT = (n); } while(0)

// 单独计数：I2C发送字节数
volatile uint32_t OLED_I2C_BYTES = 0;
#define DBG_BYTE() do { OLED_I2C_BYTES++; } while(0)

// =============== GRAM ===============
static uint8_t OLED_GRAM[OLED_WIDTH][OLED_PAGE];

// =============== 基础延时（对照例程）===============
// 72MHz CPU：1次循环约4个周期(4/72e6≈55ns)
// t=100 -> ~5.5us（满足I2C标准速率）
// t=10  -> ~550ns（快速模式）
static void IIC_Delay(void)
{
    uint8_t t = 100;
    while (t--);
}

// =============== I2C 模拟时序（对照例程）===============

static void I2C_Start(void)
{
    OLED_SDA_Set();
    OLED_SCL_Set();
    IIC_Delay();
    OLED_SDA_Clr();
    IIC_Delay();
    OLED_SCL_Clr();
    IIC_Delay();
}

static void I2C_Stop(void)
{
    OLED_SDA_Clr();
    OLED_SCL_Set();
    IIC_Delay();
    OLED_SDA_Set();
    IIC_Delay();
}

static void I2C_WaitAck(void)
{
    OLED_SDA_Set();
    IIC_Delay();
    OLED_SCL_Set();
    IIC_Delay();
    OLED_SCL_Clr();
    IIC_Delay();
}

static void Send_Byte(uint8_t dat)
{
    uint8_t i;
    for (i = 0; i < 8; i++) {
        if (dat & 0x80) {
            OLED_SDA_Set();
        } else {
            OLED_SDA_Clr();
        }
        IIC_Delay();
        OLED_SCL_Set();
        IIC_Delay();
        OLED_SCL_Clr();
        dat <<= 1;
        IIC_Delay();
    }
    DBG_BYTE();
}

static void OLED_WR_Byte(uint8_t dat, uint8_t mode)
{
    I2C_Start();
    Send_Byte(OLED_I2C_ADDR);
    I2C_WaitAck();
    if (mode) {
        Send_Byte(0x40);
    } else {
        Send_Byte(0x00);
    }
    I2C_WaitAck();
    Send_Byte(dat);
    I2C_WaitAck();
    I2C_Stop();
}

// =============== 公共 API ===============

void OLED_ColorTurn(uint8_t i)
{
    if (i == 0) {
        OLED_WR_Byte(0xA6, OLED_CMD);
    }
    if (i == 1) {
        OLED_WR_Byte(0xA7, OLED_CMD);
    }
}

void OLED_DisplayTurn(uint8_t i)
{
    if (i == 0) {
        OLED_WR_Byte(0xC8, OLED_CMD);
        OLED_WR_Byte(0xA1, OLED_CMD);
    }
    if (i == 1) {
        OLED_WR_Byte(0xC0, OLED_CMD);
        OLED_WR_Byte(0xA0, OLED_CMD);
    }
}

void OLED_Refresh(void)
{
    uint8_t i, n;
    DBG_INC(5);  // refresh called
    for (i = 0; i < OLED_PAGE; i++) {
        OLED_WR_Byte(0xB0 + i, OLED_CMD);
        OLED_WR_Byte(0x00, OLED_CMD);
        OLED_WR_Byte(0x10, OLED_CMD);
        I2C_Start();
        Send_Byte(OLED_I2C_ADDR);
        I2C_WaitAck();
        Send_Byte(0x40);
        I2C_WaitAck();
        for (n = 0; n < OLED_WIDTH; n++) {
            Send_Byte(OLED_GRAM[n][i]);
            I2C_WaitAck();
        }
        I2C_Stop();
    }
    DBG_INC(6);  // refresh complete
}

void OLED_Clear(void)
{
    uint8_t i, n;
    for (i = 0; i < OLED_PAGE; i++) {
        for (n = 0; n < OLED_WIDTH; n++) {
            OLED_GRAM[n][i] = 0;
        }
    }
    DBG_INC(4);  // clear done
    OLED_Refresh();
}

void OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t t)
{
    uint8_t i, m, n;
    i = y / 8;
    m = y % 8;
    n = 1 << m;
    if (t) {
        OLED_GRAM[x][i] |= n;
    } else {
        OLED_GRAM[x][i] = ~OLED_GRAM[x][i];
        OLED_GRAM[x][i] |= n;
        OLED_GRAM[x][i] = ~OLED_GRAM[x][i];
    }
}

// =============== 初始化 ===============
void OLED_Init(void)
{
    DBG_INC(0);  // init_enter

    // 关闭 I2C1 外设，避免 PB6/PB7 被复用为 I2C1 功能
    // main.c 里 MX_I2C1_Init() 配置了 PB6/PB7 为 I2C1_SCL/SDA，
    // 必须在 OLED_Init 之前关闭 I2C1，才能让软件 I2C 正常工作
    __HAL_RCC_I2C1_CLK_ENABLE();
    I2C1->CR1 |= I2C_CR1_SWRST;
    I2C1->CR1 &= ~I2C_CR1_SWRST;
    I2C1->CR1 = 0;
    I2C1->CR2 = 0;
    // 将 PB6/PB7 恢复为普通 GPIO（取消复用）
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // 配置 PB6/PB7 为开漏输出，速度 50MHz（对照例程）
    // 开漏模式：SDA/SCL 总线需要能被别人拉低，必须用 OD
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitStruct.Pin   = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_OD;  // 开漏！不能换成 PP
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_SET);

    DBG_INC(1);  // gpio_ok

    // 等待 OLED 上电稳定（1000ms确保SSD1306内部POR完成）
    HAL_Delay(1000);

    // SSD1306 初始化序列（严格对照例程）
    OLED_WR_Byte(0xAE, OLED_CMD);  // 关闭显示
    DBG_INC(2);  // cmd_AE sent

    OLED_WR_Byte(0x00, OLED_CMD);  // 列低地址
    OLED_WR_Byte(0x10, OLED_CMD);  // 列高地址
    OLED_WR_Byte(0x00, OLED_CMD);  // 起始行
    OLED_WR_Byte(0xB0, OLED_CMD);  // 页地址
    OLED_WR_Byte(0x81, OLED_CMD);  // 对比度
    OLED_WR_Byte(0xff, OLED_CMD);  // 最大
    OLED_WR_Byte(0xA1, OLED_CMD);  // 段重映射
    OLED_WR_Byte(0xA6, OLED_CMD);  // 正常显示
    OLED_WR_Byte(0xA8, OLED_CMD);  // 多路比率
    OLED_WR_Byte(0x1F, OLED_CMD);  // 1/32 (0.91寸)
    OLED_WR_Byte(0xC8, OLED_CMD);  // COM扫描方向
    OLED_WR_Byte(0xD3, OLED_CMD);  // 显示偏移
    OLED_WR_Byte(0x00, OLED_CMD);
    OLED_WR_Byte(0xD5, OLED_CMD);  // 时钟分频
    OLED_WR_Byte(0x80, OLED_CMD);
    OLED_WR_Byte(0xD9, OLED_CMD);  // 预充电
    OLED_WR_Byte(0x1f, OLED_CMD);
    OLED_WR_Byte(0xDA, OLED_CMD);  // COM引脚
    OLED_WR_Byte(0x00, OLED_CMD);
    OLED_WR_Byte(0xdb, OLED_CMD);  // VCOMH
    OLED_WR_Byte(0x40, OLED_CMD);
    OLED_WR_Byte(0x8d, OLED_CMD);  // 充电泵
    OLED_WR_Byte(0x14, OLED_CMD);

    OLED_Clear();
    OLED_WR_Byte(0xAF, OLED_CMD);  // 开启显示
    DBG_INC(3);  // cmd_AF sent

    DBG_INC(9);  // init_exit
}

// =============== 显示函数 ===============

extern const uint8_t OLED_F8X16[];

void OLED_ShowChar(uint8_t x, uint8_t y, char chr)
{
    uint8_t i, m;
    char c = chr - ' ';
    if (x > OLED_WIDTH - 8 || y > OLED_HEIGHT - 16) return;

    const uint8_t *p = &OLED_F8X16[c * 16];
    for (i = 0; i < 16; i++) {
        for (m = 0; m < 8; m++) {
            if (p[i] & (0x80 >> m)) {
                OLED_DrawPoint(x + m, y + i, 1);
            }
        }
    }
}

void OLED_ShowString(uint8_t x, uint8_t y, const char *str)
{
    while (*str) {
        OLED_ShowChar(x, y, *str);
        x += 8;
        if (x >= OLED_WIDTH) {
            x = 0;
            y += 16;
        }
        str++;
    }
}

void OLED_ShowLine(uint8_t line, const char *str)
{
    uint8_t len = 0;
    while (str[len] && len < 16) len++;
    uint8_t col = (OLED_WIDTH - len * 8) / 2;
    OLED_ShowString(col, line * 8, str);
}

void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num)
{
    char buf[12];
    uint8_t i = 0, j;
    if (num == 0) {
        OLED_ShowChar(x, y, '0');
        return;
    }
    while (num > 0) {
        buf[i++] = '0' + (num % 10);
        num /= 10;
    }
    for (j = i; j > 0; j--) {
        OLED_ShowChar(x + (i - j) * 8, y, buf[j - 1]);
    }
}

void OLED_ShowAssetCode(uint8_t x, uint8_t y, const char *code)
{
    OLED_ShowString(x, y, code);
}

// =============== 界面 ===============

void OLED_ShowConfirmUI(const char *action, const char *asset_code, uint8_t sec)
{
    DBG_INC(10);  // show_confirm called
    OLED_Clear();
    OLED_ShowLine(0, "=== Confirm ===");
    char line2[24];
    if (strcmp(action, "IN") == 0)       snprintf(line2, sizeof(line2), ">>> IN <<<");
    else if (strcmp(action, "OUT") == 0) snprintf(line2, sizeof(line2), ">>> OUT <<<");
    else if (strcmp(action, "RET") == 0) snprintf(line2, sizeof(line2), ">>> RET <<<");
    else                                 snprintf(line2, sizeof(line2), ">>> %s <<<", action);
    OLED_ShowLine(1, line2);
    char line3[24];
    snprintf(line3, sizeof(line3), "%s T:%ds", asset_code, sec);
    OLED_ShowLine(2, line3);
    OLED_Refresh();
    DBG_INC(11);  // show_confirm done
}

void OLED_ShowResultUI(const char *result)
{
    DBG_INC(12);  // show_result called
    OLED_Clear();
    if (strcmp(result, "OK") == 0) {
        OLED_ShowLine(1, "==== [OK] ====");
    } else if (strcmp(result, "NO") == 0) {
        OLED_ShowLine(1, "=== [CANCEL] ===");
    } else {
        OLED_ShowLine(1, "== [TIMEOUT] ==");
    }
    OLED_Refresh();
    DBG_INC(13);  // show_result done
}
