# STM32 锂电池测试项目 (3-31_lidianchi)

## 📌 项目简介

这是一个基于 STM32 微控制器的嵌入式项目，主要围绕**锂电池（lidianchi）**相关的控制或监测进行开发。
项目底层代码由 **STM32CubeMX** 自动生成，并使用 **Keil MDK-ARM** 作为主要开发环境。

## 🧱 项目结构

- `3-31_lidianchi.ioc`：STM32CubeMX 的工程配置文件
- `Inc/`：C 语言头文件（包含 `main.h` 等）
- `Src/`：C 语言源文件（包含 `main.c` 等业务逻辑）
- `MDK-ARM/`：Keil uVision 工程目录
- `Drivers/`：STM32 HAL 库及 CMSIS 驱动文件
- `code/`：用户自定义代码模块

## ⚙️ 开发环境

- **硬件平台**：STM32 系列单片机
- **配置工具**：STM32CubeMX
- **IDE/编译器**：Keil uVision 5 (MDK-ARM)
- **语言**：C 语言

## 🚀 如何运行与编译

1. 使用 **STM32CubeMX** 打开 `3-31_lidianchi.ioc` 可以重新配置引脚和时钟，并重新生成代码。
2. 进入 `MDK-ARM/` 文件夹，双击 `.uvprojx` 文件在 Keil 中打开工程。
3. 点击 **Build (F7)** 编译代码。
4. 连接仿真器（如 ST-Link / J-Link），点击 **Download (F8)** 将程序烧录到单片机中运行。