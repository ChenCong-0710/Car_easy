# STM32G474 HC-SR04 + SSD1315 test

STM32CubeIDE/HAL test project for an STM32G474VET6 board, an HC-SR04
ultrasonic module and a 128 x 64 I2C OLED using an SSD1315 controller.

## Connections

| Module | Signal | STM32 pin |
| --- | --- | --- |
| HC-SR04 | TRIG | PA1 |
| HC-SR04 | ECHO | PA0 |
| OLED | SCL | PC8 / I2C3_SCL |
| OLED | SDA | PC9 / I2C3_SDA |
| OLED | VCC | 3.3 V |
| OLED | GND | GND |

The OLED driver automatically probes the 7-bit addresses `0x3C` and `0x3D`.
Measurements are refreshed every 200 ms. USART1 output on PA9/PA10 at 9600
baud is retained as an optional diagnostic channel.

## Build and flash

1. Import or open this directory in STM32CubeIDE.
2. Select the `Debug` build configuration.
3. Connect ST-LINK to the SWD interface.
4. Press **Run** to build, download and reset the target.

The application uses STM32 HAL APIs for GPIO, I2C, UART, timing and peripheral
initialization. The DWT cycle counter is used only to obtain the microsecond
timing required by the HC-SR04 trigger and echo pulse.

## Repository contents

Commit the source code, `Drivers`, `.ioc`, linker scripts and CubeIDE project
files. Build products under `Debug` and `Release` are intentionally ignored.

