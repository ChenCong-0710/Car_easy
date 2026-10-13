# Car_easy

电设任务 1：基于 STM32G474VET6、HC-SR04 和 SSD1315 OLED 的超声追踪小车测试工程。

## 接线

| 模块 | 信号 | STM32 引脚 |
| --- | --- | --- |
| HC-SR04 | TRIG | PA1 |
| HC-SR04 | ECHO | PA0 |
| OLED | SCL | PC8 / I2C3_SCL |
| OLED | SDA | PC9 / I2C3_SDA |
| OLED | VCC | 3.3 V |
| OLED | GND | GND |

OLED 驱动会自动探测 7 位 I2C 地址 `0x3C` 和 `0x3D`。USART1 保留为备用调试接口，
使用 PA9/PA10、9600 baud、8N1。

## OLED 页面

复位后先显示基础硬件自检页。自检通过后，`STATUS: OK` 保持约 1 秒，屏幕空白约
200 ms，然后进入实时测距页。测距数据每 200 ms 更新一次。

实时状态使用未取整的毫米值判断：

| 距离 | STATE |
| --- | --- |
| 小于等于 3.0 cm | `--` |
| 大于 3.0 cm、小于 15.0 cm | `CLOSE` |
| 大于等于 15.0 cm、小于 25.0 cm | `OK` |
| 大于等于 25.0 cm、小于 50.0 cm | `FAR` |
| 大于等于 50.0 cm、小于等于 100.0 cm | `FIND` |
| 大于 100.0 cm 或测量错误 | `--` |

## 编译与烧录

1. 在 STM32CubeIDE 中导入或打开本目录。
2. 选择 `Debug` 构建配置。
3. 将 ST-LINK 连接到开发板 SWD 接口。
4. 点击 **Run**，完成编译、下载和复位。

工程使用 STM32 HAL API 完成 GPIO、I2C、UART、系统时间和外设初始化。DWT 周期计数器
仅用于满足 HC-SR04 所需的微秒级触发和回波计时。

## Git 仓库

提交源代码、`Drivers`、`.ioc`、链接脚本和 CubeIDE 工程文件。`Debug`、`Release`
目录中的编译产物已通过 `.gitignore` 排除。
