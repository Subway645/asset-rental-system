/**
 * @file    oled.c
 * @brief   OLED 驱动（SSD1306，软件I2C）
 *          基于中景园例程，完全复制，逐行对照
 *          PB6=SCL, PB7=SDA（4针I2C模块，无RES）
 *          0.91寸 OLED 专用
 */
#include "oled.h"
#include <stdlib.h>
#include <string.h>

// =============== 软件 I2C GPIO 定义 ===============
// PB6 = SCL, PB7 = SDA

#define OLED_SCL_Clr()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET)
#define OLED_SCL_Set()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET)

#define OLED_SDA_Clr()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET)
#define OLED_SDA_Set()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET)

#define OLED_RES_Clr()  /* 无RES引脚，空定义 */
#define OLED_RES_Set()  /* 无RES引脚，空定义 */

#define OLED_CMD  0
#define OLED_DATA 1

// =============== GRAM（显存） ===============
// 0.91寸: 128x32 -> 4 pages x 128 columns
#define OLED_WIDTH   128
#define OLED_HEIGHT  32
#define OLED_PAGE    4

static uint8_t OLED_GRAM[OLED_WIDTH][OLED_PAGE];  // [列][页]

// =============== 内部延时 ===============

static void IIC_Delay(void)
{
    uint8_t t = 10;
    while (t--);
}

// =============== I2C 模拟时序 ===============

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
}

static void OLED_WR_Byte(uint8_t dat, uint8_t mode)
{
    I2C_Start();
    Send_Byte(0x78);   // I2C 从机地址
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

// 反显
void OLED_ColorTurn(uint8_t i)
{
    if (i == 0) {
        OLED_WR_Byte(0xA6, OLED_CMD);  // 正常显示
    } else {
        OLED_WR_Byte(0xA7, OLED_CMD);  // 反色显示
    }
}

// 屏幕旋转
void OLED_DisplayTurn(uint8_t i)
{
    if (i == 0) {
        OLED_WR_Byte(0xC8, OLED_CMD);
        OLED_WR_Byte(0xA1, OLED_CMD);
    } else {
        OLED_WR_Byte(0xC0, OLED_CMD);
        OLED_WR_Byte(0xA0, OLED_CMD);
    }
}

// 更新显存到屏幕
void OLED_Refresh(void)
{
    uint8_t i, n;
    for (i = 0; i < OLED_PAGE; i++) {
        OLED_WR_Byte(0xB0 + i, OLED_CMD);  // 设置页地址
        OLED_WR_Byte(0x00, OLED_CMD);        // 列低4位
        OLED_WR_Byte(0x10, OLED_CMD);        // 列高4位
        I2C_Start();
        Send_Byte(0x78);
        I2C_WaitAck();
        Send_Byte(0x40);
        I2C_WaitAck();
        for (n = 0; n < OLED_WIDTH; n++) {
            Send_Byte(OLED_GRAM[n][i]);
            I2C_WaitAck();
        }
        I2C_Stop();
    }
}

// 清屏
void OLED_Clear(void)
{
    uint8_t i, n;
    for (i = 0; i < OLED_PAGE; i++) {
        for (n = 0; n < OLED_WIDTH; n++) {
            OLED_GRAM[n][i] = 0;
        }
    }
    OLED_Refresh();
}

// 画点
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

// OLED 初始化
void OLED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // 1. 使能 GPIO 时钟并初始化 PB6/PB7 为开漏输出
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitStruct.Pin   = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_SET);

    // 2. 等待 OLED 上电稳定
    HAL_Delay(200);

    // 3. SSD1306 初始化序列（逐条照抄例程）
    OLED_WR_Byte(0xAE, OLED_CMD);  // 关闭显示
    OLED_WR_Byte(0x00, OLED_CMD);  // 设置低列地址
    OLED_WR_Byte(0x10, OLED_CMD);  // 设置高列地址
    OLED_WR_Byte(0x00, OLED_CMD);  // 设置显示起始行
    OLED_WR_Byte(0xB0, OLED_CMD);  // 设置页地址
    OLED_WR_Byte(0x81, OLED_CMD);
    OLED_WR_Byte(0xff, OLED_CMD);  // 对比度（最大）
    OLED_WR_Byte(0xA1, OLED_CMD);  // 段重映射
    OLED_WR_Byte(0xA6, OLED_CMD);  // 正常/反色
    OLED_WR_Byte(0xA8, OLED_CMD);
    OLED_WR_Byte(0x1F, OLED_CMD);  // 占空比 1/32（0.91寸）
    OLED_WR_Byte(0xC8, OLED_CMD);  // 扫描方向
    OLED_WR_Byte(0xD3, OLED_CMD);
    OLED_WR_Byte(0x00, OLED_CMD);  // 显示偏移
    OLED_WR_Byte(0xD5, OLED_CMD);
    OLED_WR_Byte(0x80, OLED_CMD);  // 时钟分频
    OLED_WR_Byte(0xD9, OLED_CMD);
    OLED_WR_Byte(0x1f, OLED_CMD);  // 预充电周期
    OLED_WR_Byte(0xDA, OLED_CMD);
    OLED_WR_Byte(0x00, OLED_CMD);  // COM引脚配置
    OLED_WR_Byte(0xdb, OLED_CMD);
    OLED_WR_Byte(0x40, OLED_CMD);  // VCOMH等级
    OLED_WR_Byte(0x8d, OLED_CMD);
    OLED_WR_Byte(0x14, OLED_CMD);  // 充电泵使能
    OLED_Clear();
    OLED_WR_Byte(0xAF, OLED_CMD);  // 开启显示
    // 初始化 GRAM 为 0，防止随机数据导致花屏
    memset(OLED_GRAM, 0, sizeof(OLED_GRAM));
}

// =============== 显示函数（简化版） ===============

// 8x16 ASCII 点阵
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

// =============== 确认界面 ===============

void OLED_ShowConfirmUI(const char *action, const char *asset_code, uint8_t sec)
{
    OLED_Clear();
    OLED_ShowLine(0, "=== Confirm ===");
    char line2[24];
    if (strcmp(action, "IN") == 0)      snprintf(line2, sizeof(line2), ">>> IN <<<");
    else if (strcmp(action, "OUT") == 0) snprintf(line2, sizeof(line2), ">>> OUT <<<");
    else if (strcmp(action, "RET") == 0) snprintf(line2, sizeof(line2), ">>> RET <<<");
    else                                  snprintf(line2, sizeof(line2), ">>> %s <<<", action);
    OLED_ShowLine(1, line2);
    char line3[24];
    snprintf(line3, sizeof(line3), "%s T:%ds", asset_code, sec);
    OLED_ShowLine(2, line3);
    OLED_Refresh();
}

void OLED_ShowResultUI(const char *result)
{
    OLED_Clear();
    if (strcmp(result, "OK") == 0) {
        OLED_ShowLine(1, "==== [OK] ====");
    } else if (strcmp(result, "NO") == 0) {
        OLED_ShowLine(1, "=== [CANCEL] ===");
    } else {
        OLED_ShowLine(1, "== [TIMEOUT] ==");
    }
    OLED_Refresh();
}
