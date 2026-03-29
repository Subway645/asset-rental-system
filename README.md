# 资产租借管理系统

一套软硬件结合的资产租借管理系统。硬件端基于 STM32F103C8T6，通过 OLED 和按键与用户交互；PC 端提供 Web 系统，实现资产入库、借出、归还全流程，并具备可视化运营看板。

---

## 目录结构

```
asset-rental-system/
├── README.md                # 本文件
├── requirements.txt         # Python 依赖
├── init_db.py               # 数据库初始化
│
├── stm32/                   # ===== STM32 Keil5 工程 =====
│   ├── main.c / main.h      # 主程序 + 状态机
│   ├── core.c / core.h      # 系统时钟 + GPIO 初始化
│   ├── oled.c / oled.h      # OLED 驱动（SSD1306, I2C 软件模拟）
│   ├── oled_font.c          # ASCII 8x16 点阵字模
│   ├── button.c / button.h   # 4 按键扫描（消抖 + 10ms 定时）
│   ├── serial.c / serial.h   # USART1 中断收发（9600bps）
│   └── startup_stm32f103xb.s # 启动文件
│
└── pc/                      # ===== PC 端 Flask Web 应用 =====
    ├── app.py               # Flask 主程序（路由 + 数据库模型）
    ├── hardware/
    │   ├── serial_comm.py   # 串口通信（含自动模拟模式）
    │   └── qr_scanner.py    # OpenCV 摄像头扫码
    ├── templates/            # HTML 页面
    │   ├── base.html         # Bootstrap 母版
    │   ├── index.html        # 登录页
    │   ├── asset_list.html   # 资产列表
    │   ├── asset_form.html   # 新增/编辑资产
    │   ├── borrow.html       # 借还操作
    │   ├── reports.html      # 异常申报
    │   ├── dashboard.html     # ECharts 可视化看板
    │   └── user_list.html   # 用户管理
    └── static/
        └── style.css
```

---

## 一、PC 端快速启动（无需硬件即可运行）

### 1. 安装 Python 依赖

```powershell
cd C:\Users\Subway\Desktop\asset-rental-system
python -m pip install -r requirements.txt
```

> opencv-python 安装失败时，改为：
> `pip install opencv-python-headless`（无图形界面环境推荐）

### 2. 初始化数据库

```powershell
python init_db.py
```

运行后在 `pc/app.db` 生成 SQLite 数据库，包含 3 个测试账号和 8 条示例资产数据。

### 3. 启动 Web 服务

```powershell
python pc/app.py
```

浏览器打开 **http://127.0.0.1:5000**

| 角色 | 用户名 | 密码 |
|------|--------|------|
| 管理员 | admin | admin123 |
| 普通用户 | zhangsan | user123 |
| 普通用户 | lisi | user123 |

> **模拟模式**：PC 端检测到未连接 STM32 时，自动切换模拟模式，硬件确认操作在 1.5 秒后自动返回结果，无需硬件即可完整演示。

---

## 二、硬件接线（ATK-MB000 BEE BLOCK）

本项目推荐使用 **正点原子 ATK-MB000 BEE BLOCK 仿真器**，一根 USB 线同时搞定烧录和串口通信。

### BEE BLOCK 接口说明

```
  ┌──────────────────────────┐
  │  USB ──────────────────────→ PC USB
  │  SWD ──────────────────────→ STM32 SWD 接口
  │  虚拟串口 ─────────────────→ STM32 USART1
  │  3.3V/5V 供电输出 ────────→ 目标板电源（可选）
  └──────────────────────────┘
```

### 接线表（BEE BLOCK 全家桶接线）

| STM32F103C8T6 | BEE BLOCK | 说明 |
|--------------|-----------|------|
| PA13 (SWDIO) | SWDIO | SWD 调试数据线 |
| PA14 (SWCLK) | SWCLK | SWD 调试时钟线 |
| GND | GND | 共地 |
| PA9  (USART1_TX) | RXD（虚拟串口） | 串口发送 → BEE BLOCK 接收 |
| PA10 (USART1_RX) | TXD（虚拟串口） | 串口接收 ← BEE BLOCK 发送 |
| PB6  (I2C1_SCL)  | OLED SCL | I2C 时钟线 |
| PB7  (I2C1_SDA)  | OLED SDA | I2C 数据线 |
| PA0 | 按键1（确认） | 一端接 PA0，一端接地 |
| PA1 | 按键2（取消） | 一端接 PA1，一端接地 |
| PA2 | 按键3（上翻） | 一端接 PA2，一端接地 |
| PA3 | 按键4（下翻） | 一端接 PA3，一端接地 |
| 3.3V | OLED VCC | 供电 |

> **BEE BLOCK 的 3.3V 输出接口**（按下 K1）可以直接给目标板供电，但最大 200mA，如目标板功耗较大请单独供电。

### BEE BLOCK 指示灯含义

| 灯 | 状态 | 含义 |
|----|------|------|
| STA 蓝灯 | 常亮 | USB 连接正常 |
| STA 蓝灯 | 闪烁 | USB 未识别，检查驱动 |
| STA 红灯 | 亮起 | 正在下载或程序运行中 |
| TXD（绿） | 闪烁 | 串口正在发送数据 |
| RXD（蓝） | 闪烁 | 串口正在接收数据 |
| DS1（红） | 常亮 | 3.3V 电源输出已开启 |
| DS2（红） | 常亮 | 5V 电源输出已开启 |

---

## 三、STM32 固件烧录（BEE BLOCK）

### 3.1 Keil 配置（CMSIS-DAP Debugger）

> 只需要配置一次，之后直接点 Download 即可烧录，无需跳线和 BOOT0 设置。

1. **Project → Options for Target → Debug**
   - 选 **CMSIS-DAP Debugger**
   - 点右侧 **Settings**：
     - `CMSIS-DAP-JTAG/SW Adapter` → 选 **ATK-CMSIS-DAP**
     - 勾 **SWJ**
     - **Port** → **SW**
     - **Reset** → **SYSRESETREQ**
     - **Max Clock** → 先用 **1MHz**（不稳定时可降到 100kHz）

2. 切换到 **Flash Download**：
   - 添加 `STM32F103C8` 下载算法
   - 勾选 **Reset and Run**

3. 点 **OK** 保存

4. 点击 Keil 工具栏 **Download**（或 F8）开始烧录

### 3.2 验证运行

烧录完成后 OLED 应立即显示：

```
=== Asset Mgmt Sys ===
System Ready
Waiting for PC...
BTN1:OK  BTN2:Cancel
```

> 注意：烧录时目标板需要有电（BEE BLOCK 按 K1 输出 3.3V 供电，或目标板单独供电）。

### 3.3 设备管理器中的 COM 口

BEE BLOCK 插上后，设备管理器会出现 **两个 COM 口**：
- **COMx（CMSIS-DAP）** — 烧录用（自动使用，不需要手动选）
- **COMy（USB Serial）** — 虚拟串口，PC 端软件连接时选这个

---

## 四、备选烧录方式：串口 Bootloader（无仿真器时）

> 若只有 CP2102 / CH340 等 USB 转串口模块，没有仿真器，可用此方式烧录。

### 4.1 准备工具

下载 ST 官方免费工具 **STM32 Flash Loader Demonstrator**：
https://www.st.com/content/st_com/en/products/development-tools/software-development-tools/stm32-software-development-tools/stm32-programmers/fl-stm32flashloaderexp.html

### 4.2 设置 Boot 跳线

1. 断开电源
2. 将 **BOOT0** 跳线帽插到 **3.3V**（切换到 System Memory 启动模式）
3. 连接 CP2102（TX/RX/GND）
4. 上电

### 4.3 烧录

1. 运行 Flash Loader Demonstrator
2. 选择 CP2102 对应的 COM 口，波特率选 **115200**
3. 一路 Next，确认芯片型号
4. 选择固件：`stm32/Objects/STM32.hex`
5. 点击 Next 开始烧录
6. 烧录完成后断电，将 **BOOT0 跳线帽插回 GND**
7. 重新上电即可运行

---

## 五、Keil5 工程配置说明

> 如果只是烧录运行，不需要修改代码，跳过本节即可。

### Keil5 参数配置（与本工程一致）

| 设置项 | 值 |
|--------|-----|
| Target → Crystal | 8.000 |
| C/C++ → Define | `USE_HAL_LIBRARY,STM32F103xB` |
| C/C++ → Include Paths | `stm32/`（根据 Keil 安装位置补充 Pack 路径） |
| Debug → Driver | **CMSIS-DAP Debugger**（使用 BEE BLOCK 时选此项）|
| Debug → Settings → Port | **SW** |
| Debug → Settings → Adapter | **ATK-CMSIS-DAP** |
| Debug → Settings → Max Clock | **1MHz**（不稳定时可降低） |
| Flash Download → Algorithm | 添加 `STM32F103C8` 下载算法 |
| Utilities → Update Target | 勾选 |

### HAL 驱动文件清单

| 文件 | 说明 |
|------|------|
| `stm32f1xx_hal.c` | HAL 核心 |
| `stm32f1xx_hal_uart.c` | USART 驱动 |
| `stm32f1xx_hal_i2c.c` | I2C 驱动 |
| `stm32f1xx_hal_gpio.c` | GPIO 驱动 |
| `stm32f1xx_hal_rcc.c` | 时钟驱动 |
| `stm32f1xx_hal_tim.c` | 定时器驱动 |
| `stm32f1xx_hal_tim_ex.c` | 定时器扩展驱动 |
| `stm32f1xx_hal_cortex.c` | Cortex-M3 内核驱动 |
| `stm32f1xx_hal_flash.c` | Flash 驱动 |
| `stm32f1xx_hal_flash_ex.c` | Flash 扩展 |
| `stm32f1xx_hal_dma.c` | DMA 驱动 |
| `stm32f1xx_hal_pwr.c` | 电源管理 |
| `stm32f1xx_hal_exti.c` | 外部中断 |
| `system_stm32f1xx.c` | SystemInit |
| `startup_stm32f103xb.s` | 启动文件 |

### 编译输出

- `.axf` — Keil 调试文件
- `.hex` — 用于串口 Bootloader 烧录：`stm32/Objects/STM32.hex`（使用 BEE BLOCK 时不需要，Keil 直接通过 SWD 烧录）

---

## 五、通信协议

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
| 超时 | `TIMEOUT,<asset_code>\n` | 30 秒无操作 |
| 心跳回复 | `PONG\n` | 连接正常 |

### 串口参数

- **波特率**：9600 bps
- **数据位**：8
- **停止位**：1
- **校验**：无

---

## 六、系统使用流程

### 入库

1. 管理员登录 Web → 「资产入库」→ 输入编号（如 ASSET009）和名称 → 提交
2. STM32 OLED 显示：
   ```
   ====== 确认 ======
   >>> 入库确认 <<<
   资产: ASSET009
   CntDwn: 30 s
   ```
3. 用户按 **BTN1（确认）** → OLED 显示 `[OK] Confirmed!`（2秒）→ 自动返回
4. PC 端收到 `OK` → 数据库状态更新为"在库"

### 借出

1. 登录 Web → 「借还」→ 输入资产编号 → 借出
2. STM32 显示 `>>> 借出确认 <<<`
3. 用户确认 → 数据库更新为"借出"

### 归还

1. 登录 Web → 「借还」→ 输入资产编号 → 归还
2. STM32 显示 `>>> 归还确认 <<<`
3. 用户确认 → 数据库更新为"在库"

### 超时

30 秒无按键操作，STM32 自动返回 `TIMEOUT`，PC 端取消操作。

---

## 七、OLED 屏幕内容一览

| 场景 | 第0行 | 第2行 | 第4行 | 第6行 |
|------|-------|-------|-------|-------|
| 主界面 | === Asset Mgmt Sys === | System Ready | Waiting for PC... | BTN1:OK  BTN2:Cancel |
| 入库确认 | ====== 确认 ====== | >>> 入库确认 <<< | 资产: ASSET009 | CntDwn: 25 s |
| 借出确认 | ====== 确认 ====== | >>> 借出确认 <<< | 资产: ASSET009 | CntDwn: 25 s |
| 归还确认 | ====== 确认 ====== | >>> 归还确认 <<< | 资产: ASSET009 | CntDwn: 25 s |
| 确认成功 | ==== [OK] ==== | Confirmed! | — | — |
| 取消 | ==== [CANCEL] ==== | Cancelled! | — | — |
| 超时 | == [TIMEOUT] == | Timeout! | — | — |

---

## 九、常见问题排查

### Keil 提示"No CMSIS-DAP Device found"

- 检查 BEE BLOCK 是否已通过 USB 连接电脑
- 检查设备管理器中是否出现 BEE BLOCK 设备（无驱动时需手动安装）
- Win7 可能需要手动安装驱动；Win8/10/11 一般自动识别
- 驱动下载：正点原子官网或设备管理器右键 → 更新驱动

### PC 端提示"串口无法打开"

- 确认使用的是 BEE BLOCK 的 **USB Serial（虚拟串口）** COM 口（设备管理器里第二个 COM）
- 不要选 CMSIS-DAP 那个 COM，那是烧录用的
- USB 线是否支持数据通信（部分线只能充电）
- TX/RX 是否接反（STM32 PA9 → BEE BLOCK RXD，PA10 → BEE BLOCK TXD）
- 无硬件时 PC 端会自动进入模拟模式，不影响演示

### OLED 不显示

- 检查 PB6（接 SCL）和 PB7（接 SDA）接线
- 确认 OLED 是 I2C 接口（而非 SPI）
- 确认 OLED 供电为 3.3V
- 上电后等待 500ms 再初始化（OLED_Init 前加 `HAL_Delay(500)`）

### STM32 串口无响应

- PC 端串口参数：9600, 8N1
- BEE BLOCK 和 STM32 是否共地（GND 相连）
- 用串口调试助手（如 SSCOM）直接发送 `PING\n` 测试

### Keil 编译报错

- 确认 `USE_HAL_LIBRARY,STM32F103xB` 两个宏都填入 Define 框
- Include Paths 中需包含 `stm32/` 目录
- HAL 头文件版本需与 .c 源文件版本一致（不混用不同版本的 HAL）

### 烧录时识别不到芯片 IDCODE

- 将 Max Clock 降低到 100kHz 或 1MHz 再试
- 检查 SWDIO / SWCLK 接线是否正确（注意不是接在 PA9/PA10 上）
- 确保目标板已供电（BEE BLOCK K1 开启 3.3V 输出，或单独供电）

---

## 九、后续扩展方向

- [ ] 接入 MySQL，支持多用户并发操作
- [ ] PC 端生成资产二维码并打印贴附
- [ ] 逾期自动短信/邮件提醒
- [ ] 操作日志导出 Excel
- [ ] 添加状态指示灯（不同颜色代表不同操作）
- [ ] 添加借期续借功能
- [ ] 换用 ESP8266/ESP32 WiFi 模块替代 USB 串口
