# 办公室资产租借管理与可视化运营系统

## 系统概述

一套软硬件结合的资产租借管理系统。硬件端基于 STM32F103C8T6，通过 OLED 和按键与用户交互；PC 端提供 Web 前后端系统，实现资产入库、借出、归还全流程，并具备可视化运营看板。

---

## 目录结构

```
赛题/
├── README.md               # 本文件
├── requirements.txt        # Python 依赖
├── init_db.py             # 数据库初始化脚本
│
├── stm32/                 # ========== STM32 Keil5 工程 ==========
│   ├── main.c / main.h    # 主程序入口 + 状态机
│   ├── core.c / core.h    # 系统初始化（时钟、GPIO）
│   ├── oled.c / oled.h    # OLED 驱动（SSD1306, I2C）
│   ├── oled_font.c        # ASCII 8x16 点阵字模
│   ├── button.c / button.h # 4按键扫描（消抖）
│   └── serial.c / serial.h # USART1 串口收发中断
│
└── pc/                    # ========== PC 端 Flask 应用 ==========
    ├── app.py             # Flask 主程序（路由 + 模型）
    ├── hardware/
    │   ├── serial_comm.py # 串口通信（含模拟模式）
    │   └── qr_scanner.py  # OpenCV 摄像头扫码
    ├── templates/         # HTML 页面
    │   ├── base.html      # Bootstrap 母版
    │   ├── index.html     # 登录页
    │   ├── asset_list.html
    │   ├── asset_form.html
    │   ├── asset_stockin.html
    │   ├── borrow.html
    │   ├── reports.html
    │   ├── dashboard.html # ECharts 可视化看板
    │   └── user_list.html
    └── static/
        └── style.css
```

---

## 一、快速启动（PC 端，无需硬件即可运行）

### 1. 安装 Python 依赖

```powershell
cd C:\Users\Subway\Desktop\赛题
python -m pip install -r requirements.txt
```

> 如果安装 opencv-python 报错，尝试单独安装：
> `pip install opencv-python-headless`（无需图形界面，适合服务器/无显示器环境）

### 2. 初始化数据库

```powershell
python init_db.py
```

运行后会创建 `pc/app.db` SQLite 数据库，并插入：
- 3 个测试账号
- 8 条示例资产数据

### 3. 启动 Web 服务

```powershell
python pc/app.py
```

浏览器打开 http://127.0.0.1:5000

| 角色 | 用户名 | 密码 |
|------|--------|------|
| 管理员 | admin | admin123 |
| 普通用户 | zhangsan | user123 |
| 普通用户 | lisi | user123 |

> **模拟模式**：PC 端检测到没有连接 STM32 时，会自动切换到模拟模式，硬件确认操作自动模拟（1.5秒后随机返回结果），无需硬件也能完整演示所有功能。

---

## 二、硬件接线说明（STM32 部分）

### 接线表

| STM32F103C8T6 | 外设 | 说明 |
|---------------|------|------|
| PA9  (USART1_TX) | → CP2102 RX | 串口发送 |
| PA10 (USART1_RX) | → CP2102 TX | 串口接收 |
| PB6  (I2C1_SCL)  | → OLED SCL | I2C 时钟 |
| PB7  (I2C1_SDA)  | → OLED SDA | I2C 数据 |
| PA0              | → 按键1（确认） | 一端接 PA0，一端接地 |
| PA1              | → 按键2（取消） | 一端接 PA1，一端接地 |
| PA2              | → 按键3（上翻） | 一端接 PA2，一端接地 |
| PA3              | → 按键4（下翻） | 一端接 PA3，一端接地 |
| 3.3V             | → OLED VCC, CP2102 VCC | 供电 |
| GND              | → OLED GND, CP2102 GND, 所有按键 | 共地 |

### CP2102 USB转TTL 模块连接 PC

```
STM32 PA9  →  CP2102 RX
STM32 PA10 →  CP2102 TX
STM32 GND  →  CP2102 GND
CP2102 USB →  PC USB
```

> 注意：STM32 和 CP2102 必须共地（否则串口通信异常）

---

## 三、Keil5 工程创建步骤

> 如果你之前有用 CubeMX 生成代码的经验，可以用 CubeMX 来初始化外设。以下是纯寄存器/CubeMX 混用的方式。

### 步骤 1：新建 Keil5 工程

1. Keil5 → Project → New uVision Project → 选择 `stm32/` 文件夹
2. 芯片选型：`STM32F103C8`
3. 弹出管理运行时环境：只勾选 `CORE`，其他取消 → OK

### 步骤 2：添加源文件

将 `stm32/` 下的所有 `.c` 和 `.h` 文件拖入 Keil 左侧 Project 窗口。

### 步骤 3：配置 Target

1. Project → Options for Target
2. **C/C++** → **Define**: `USE_HAL_LIBRARY,STM32F103xB`
3. **C/C++** → **Include Paths**: 添加 `stm32/` 和 `C:\Keil_v5\ARM\PACK\Keil\STM32F1xx_DFP\2.2.0\Device\Include`（路径根据实际安装位置）
4. **Debug** → 使用 **ST-Link Debugger** → Settings → Port 选 **SWD**
5. **Utilities** → 勾选 "Update Target before Debugging"

### 步骤 4：添加 HAL 库

如果没有使用 CubeMX，需要手动添加 STM32F1xx HAL 库文件到工程：
- `stm32f1xx_hal.c`
- `stm32f1xx_hal_uart.c`
- `stm32f1xx_hal_i2c.c`
- `stm32f1xx_hal_gpio.c`
- `stm32f1xx_hal_rcc.c`
- `stm32f1xx_hal_tim.c`
- `stm32f1xx_hal_cortex.c`
- `startup_stm32f103xb.s`（启动文件）

> **推荐方式**：使用 CubeMX 生成初始化代码，然后用本项目的 `main.c` 替换主逻辑。按键、OLED、串口初始化代码可以直接复制使用。

### 步骤 5：编译与下载

1. 点击 **Rebuild** 编译
2. 点击 **Download** 烧录
3. 连接 ST-Link，PC 端 Flask 打开串口连接

---

## 四、工作流程演示

### 入库流程

1. 管理员登录 Web 系统
2. 进入「入库」页面，输入资产编号（如 `ASSET009`）和名称
3. 点击「提交入库申请」
4. PC 端通过串口发送 `IN,ASSET009\n` 到 STM32
5. STM32 OLED 显示：「入库确认 / 资产: ASSET009 / 倒计时: 30秒」
6. 用户按「确认」键 → STM32 返回 `OK,ASSET009,20260327143052\n`
7. PC 端收到确认，数据库状态更新为「在库」
8. Web 页面显示成功

### 借出流程

1. 任何人登录 → 「借还」页面 → 输入资产编号 → 借出
2. STM32 显示：「借出确认 / 资产: ASSET001 / 倒计时」
3. 用户按「确认」 → 返回 OK → 数据库更新为「借出」状态

### 归还流程

1. 任何人登录 → 「借还」页面 → 输入资产编号 → 归还
2. STM32 显示：「归还确认」 → 用户确认 → 返回 OK → 数据库更新为「在库」

### 异常申报

1. 逾期：选择借出记录，填写说明，提交申报
2. 损坏：选择资产，描述损坏情况，提交后资产自动进入「维修」状态
3. 管理员可在「异常申报」页面处理（填写处理结果，设置资产新状态）

---

## 五、通信协议详解

### PC → STM32

| 命令 | 格式 | 说明 |
|------|------|------|
| 入库确认 | `IN,<asset_code>\n` | 请求确认入库 |
| 借出确认 | `OUT,<asset_code>\n` | 请求确认借出 |
| 归还确认 | `RET,<asset_code>\n` | 请求确认归还 |
| 心跳检测 | `PING\n` | 检测连接 |

### STM32 → PC

| 响应 | 格式 | 说明 |
|------|------|------|
| 确认 | `OK,<asset_code>,<timestamp>\n` | 用户按了确认 |
| 取消 | `NO,<asset_code>,<timestamp>\n` | 用户按了取消 |
| 超时 | `TIMEOUT,<asset_code>\n` | 30秒无操作 |
| 心跳回复 | `PONG\n` | 连接正常 |

### 波特率

- 9600 bps，8 数据位，1 停止位，无校验

---

## 六、可视化看板说明

进入「数据看板」（仅管理员可见），包含：

1. **KPI 卡片**：资产总数、在库数量、当前借出、逾期数量
2. **资产状态分布饼图**：在库/借出/维修/报废占比
3. **近30天借用趋势折线图**：每天借出次数
4. **各类别资产数量柱状图**：按分类统计
5. **借用频次 Top 10**：最受欢迎的资产排行
6. **硬件确认率**：所有操作中硬件确认通过的比例
7. **操作日志**：最近20条系统操作记录

---

## 七、常见问题排查

### PC 端提示"串口无法打开"

- 检查 CP2102 驱动是否安装（设备管理器中是否识别为 COM 端口）
- 检查 USB 线是否支持数据通信（部分 USB 线仅供电）
- 确认 STM32 串口 TX/RX 没有接反（PA9→CP2102 RX, PA10→CP2102 TX）
- 无硬件时系统会自动进入模拟模式，不影响演示

### OLED 不显示

- 检查 OLED 的 SCL（PB6）和 SDA（PB7）接线是否正确
- 确认 OLED 模块是 I2C 接口（而非 SPI）
- 检查 OLED 供电是否为 3.3V（有些模块是 5V，接 3.3V 可能亮度不够）
- 可以在 OLED_Init() 前加 HAL_Delay(500) 增加上电等待时间

### STM32 串口无响应

- 确认 PC 端串口参数：9600, 8N1
- 检查 STM32 和 CP2102 共地
- 在 Serial_Init() 中加入断点，确认初始化顺序正确
- 可以在 PC 端用串口调试助手（如 SSCOM）直接发送命令测试

### Keil 编译报错找不到 stm32f1xx_hal.h

- 需要添加 STM32F1 HAL 库文件，或在 CubeMX 中重新生成代码框架
- 确保 Keil 中安装了 ARM::CMSIS 和 Keil::STM32F1xx_DFP -Pack

---

## 八、竞赛加分项（可选实现）

- [ ] 将 SQLite 替换为 MySQL，支持多用户并发
- [ ] 添加资产二维码生成功能（PC端生成二维码图片打印贴附）
- [ ] 添加短信/邮件通知（资产逾期自动提醒）
- [ ] 添加操作记录导出Excel功能
- [ ] STM32 添加指示灯（不同颜色表示不同操作）
- [ ] 添加借期续借功能
- [ ] 使用 WiFi 模块（ESP8266/ESP32）替代 USB 串口，实现无线通信
