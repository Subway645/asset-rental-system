/**
 * @file    oled.c
 * @brief   0.96寸 OLED 驱动（SSD1306，I2C接口，128x64像素）
 *          软件I2C（SCL=PB6, SDA=PB7）
 */
#include "oled.h"
#include "core.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

// =============== I2C 底层操作 ===============

#define OLED_I2C_ADDR   0x78    // 7位地址0x3C，左移1位+R/W位=0x78
#define I2C_TIMEOUT     0xFFFF

static void I2C_Start(void)
{
    HAL_I2C_Master_Transmit(&hi2c1, OLED_I2C_ADDR,
                            (uint8_t[]){0}, 1, I2C_TIMEOUT);
}

static void I2C_Stop(void)
{
    uint8_t dummy[1] = {0};
    HAL_I2C_Master_Transmit(&hi2c1, OLED_I2C_ADDR | 0x01, dummy, 1, I2C_TIMEOUT);
}

static void I2C_SendByte(uint8_t byte)
{
    uint8_t buf[2] = {0x40, byte};  // 0x40 = Co=0, D/C#=1（数据）
    HAL_I2C_Master_Transmit(&hi2c1, OLED_I2C_ADDR, buf, 2, I2C_TIMEOUT);
}

static void I2C_SendCmd(uint8_t cmd)
{
    uint8_t buf[2] = {0x00, cmd};    // 0x00 = Co=0, D/C#=0（命令）
    HAL_I2C_Master_Transmit(&hi2c1, OLED_I2C_ADDR, buf, 2, I2C_TIMEOUT);
}

// =============== OLED 操作 ===============

#define OLED_WIDTH   128
#define OLED_HEIGHT  64

static uint8_t OLED_GRAM[OLED_HEIGHT / 8][OLED_WIDTH];  // 128x64 bits

/**
 * @brief  写一字节到GRAM（不刷新屏幕）
 */
static void OLED_WriteByte(uint8_t page, uint8_t col, uint8_t data)
{
    if (page >= 8 || col >= OLED_WIDTH) return;
    OLED_GRAM[page][col] = data;
}

/**
 * @brief  刷新GRAM到屏幕
 */
void OLED_Refresh(void)
{
    for (uint8_t page = 0; page < 8; page++) {
        I2C_SendCmd(0xB0 + page);          // 设置页地址
        I2C_SendCmd(0x00);                 // 列低4位
        I2C_SendCmd(0x10);                 // 列高4位
        for (uint8_t col = 0; col < OLED_WIDTH; col++) {
            I2C_SendByte(OLED_GRAM[page][col]);
        }
    }
}

/**
 * @brief  清屏
 */
void OLED_Clear(void)
{
    memset(OLED_GRAM, 0, sizeof(OLED_GRAM));
    OLED_Refresh();
}

/**
 * @brief  OLED初始化（SSD1306默认序列）
 */
void OLED_Init(void)
{
    HAL_Delay(100);  // 上电等待

    I2C_SendCmd(0xAE);  // 关闭显示
    I2C_SendCmd(0xD5); I2C_SendCmd(0x80);  // 设置时钟
    I2C_SendCmd(0xA8); I2C_SendCmd(0x3F);  // 1/64占空比
    I2C_SendCmd(0xD3); I2C_SendCmd(0x00);  // 显示偏移
    I2C_SendCmd(0x40);                     // 起始行=0
    I2C_SendCmd(0xA1);                     // 段重映射（不翻转）
    I2C_SendCmd(0xC8);                     // 扫描方向（逆向）
    I2C_SendCmd(0xDA); I2C_SendCmd(0x12); // COM引脚配置
    I2C_SendCmd(0x81); I2C_SendCmd(0xCF); // 对比度设置
    I2C_SendCmd(0xD9); I2C_SendCmd(0xF1); // 预充电周期
    I2C_SendCmd(0xDB); I2C_SendCmd(0x30); // VCOMH等级
    I2C_SendCmd(0x8D); I2C_SendCmd(0x14); // 充电泵使能
    I2C_SendCmd(0xAF);                     // 开启显示

    OLED_Clear();
}

/**
 * @brief  显示一个ASCII字符（8x16像素点阵，可显示ASCII 0x20-0x7E）
 */
static void OLED_ShowChar(uint8_t page, uint8_t col, char ch)
{
    if (page >= 8 || col >= OLED_WIDTH - 8) return;
    if (ch < 0x20 || ch > 0x7E) ch = '?';

    // 8x16点阵（简化版：直接用字模数组）
    extern const uint8_t OLED_F8X16[];  // 在oled_font.c中定义
    const uint8_t *p = &OLED_F8X16[(ch - 0x20) * 16];
    for (uint8_t i = 0; i < 16; i++) {
        OLED_GRAM[page + (i / 8)][col + i % 8 ? 0 : i / 8] = p[i];
    }
}

/**
 * @brief  显示字符串（自动换行）
 */
void OLED_ShowString(uint8_t page, uint8_t col, const char *str)
{
    while (*str) {
        OLED_ShowChar(page, col, *str);
        str++;
        col += 8;
        if (col >= OLED_WIDTH) {
            col = 0;
            page += 2;
            if (page >= 8) break;
        }
    }
}

/**
 * @brief  显示一行居中对齐文本（不超过20字符）
 */
void OLED_ShowLine(uint8_t line, const char *str)
{
    // 每行16像素，每行显示约21个字符（6px字体时）
    uint8_t len = 0;
    while (str[len] && len < 21) len++;
    uint8_t col = (OLED_WIDTH - len * 6) / 2;
    OLED_ShowString(line, col, str);
}

/**
 * @brief  显示数字（简单实现）
 */
void OLED_ShowNum(uint8_t page, uint8_t col, uint32_t num)
{
    char buf[12];
    uint8_t i = 0;
    if (num == 0) {
        OLED_ShowChar(page, col, '0');
        return;
    }
    while (num > 0) {
        buf[i++] = '0' + (num % 10);
        num /= 10;
    }
    while (i--) {
        OLED_ShowChar(page, col, buf[i]);
        col += 6;
    }
}

/**
 * @brief  显示HEX格式的资产编号
 */
void OLED_ShowAssetCode(uint8_t page, uint8_t col, const char *code)
{
    // 资产编号用大字体居中显示
    OLED_ShowString(page, col, code);
}

/**
 * @brief  显示进度条（倒计时用）
 */
void OLED_ShowProgressBar(uint8_t page, uint8_t total_sec, uint8_t remain_sec)
{
    uint8_t bar_width = (OLED_WIDTH - 10) * remain_sec / total_sec;
    OLED_GRAM[page][4] = 0x81;  // 左端
    for (uint8_t i = 0; i < bar_width; i++) {
        OLED_GRAM[page][5 + i] = 0xFF;
    }
    OLED_GRAM[page][4 + bar_width + 1] = 0x01;  // 右端
}

/**
 * @brief  显示状态图标（确认=绿色勾/取消=红色叉，超时=黄色叹号）
 */
void OLED_ShowStatusIcon(uint8_t status)
{
    // status: 0=OK, 1=NO, 2=TIMEOUT
    if (status == 0) {  // OK - 简单对勾
        uint8_t y = 52;
        uint8_t cx = OLED_WIDTH - 20;
        OLED_GRAM[y/8][cx]     = 0x06;
        OLED_GRAM[y/8][cx+1]   = 0x08;
        OLED_GRAM[y/8][cx+2]   = 0x10;
        OLED_GRAM[y/8][cx+3]   = 0x28;
        OLED_GRAM[y/8][cx+4]   = 0x44;
        OLED_GRAM[y/8][cx+5]   = 0x82;
    }
}

/**
 * @brief  绘制确认界面（带倒计时）
 */
void OLED_ShowConfirmUI(const char *action, const char *asset_code, uint8_t sec)
{
    OLED_Clear();

    // 第0行：大标题
    OLED_ShowLine(0, "====== 确认 ======");

    // 第2行：操作类型
    char line2[22];
    if (strcmp(action, "IN") == 0)    snprintf(line2, sizeof(line2), ">>> 入库确认 <<<");
    else if (strcmp(action, "OUT") == 0) snprintf(line2, sizeof(line2), ">>> 借出确认 <<<");
    else if (strcmp(action, "RET") == 0) snprintf(line2, sizeof(line2), ">>> 归还确认 <<<");
    else snprintf(line2, sizeof(line2), ">>> %s <<<", action);
    OLED_ShowLine(2, line2);

    // 第4行：资产编号
    char code_line[22];
    snprintf(code_line, sizeof(code_line), "资产: %s", asset_code);
    OLED_ShowLine(4, code_line);

    // 第6行：倒计时
    char timer_line[22];
    snprintf(timer_line, sizeof(timer_line), "倒计时: %d 秒", sec);
    OLED_ShowLine(6, timer_line);

    OLED_Refresh();
}

/**
 * @brief  显示结果界面
 */
void OLED_ShowResultUI(const char *result)
{
    OLED_Clear();
    if (strcmp(result, "OK") == 0) {
        OLED_ShowLine(2, "==== [OK] ====");
        OLED_ShowLine(4, "已确认!");
    } else if (strcmp(result, "NO") == 0) {
        OLED_ShowLine(2, "=== [CANCEL] ===");
        OLED_ShowLine(4, "已取消!");
    } else {
        OLED_ShowLine(2, "== [TIMEOUT] ==");
        OLED_ShowLine(4, "超时自动取消!");
    }
    OLED_Refresh();
}
