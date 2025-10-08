# PA2 Pin Usage Analysis in ESCape32 Project

## Overview

This document provides a detailed analysis of how pin **PA2** is used in the ESCape32 project, including its dual functionality, dynamic switching mechanisms, and resource conflicts.

## Primary Functions of PA2

Pin PA2 serves two primary functions that switch dynamically based on the detected input protocol:

1. **TIM15 Channel 1 Input (TIM15_CH1)** - Default mode for input protocol detection
2. **USART2 TX** - Serial communication when `IO_PA2` is enabled

## Dual Functionality & Dynamic Switching

PA2 has **dual functionality** that switches dynamically based on the detected input protocol:

### Input Capture Mode (Default)
- **Timer**: TIM15 Channel 1 (TIM15_CH1) 
- **Purpose**: Input capture for protocol detection (DSHOT, Servo PWM, OneShot125)
- **Pin Config**: `IOTIM_IDR (GPIOA_IDR & 0x4) // A2`
- **DMA**: Uses DMA1_CH5 for DSHOT bidirectional communication

### Serial Communication Mode (Dynamic)
- **UART**: USART2 TX (transmit only in half-duplex mode)
- **Purpose**: Serial telemetry protocols (iBUS, SBUS, CRSF, Custom Serial)
- **Pin Config**: `GPIOA_AFRL |= 0x100; // A2 (USART2_TX)`
- **Companion**: PA15 serves as USART2_RX

## Configuration Control

### Compile-time Enabling
```c
#define IO_PA2  // Enables PA2 serial functionality
```

### Runtime Switching
The `io_serial()` function dynamically reconfigures PA2 from timer input to USART2 TX:

```c
void io_serial(void) {
    RCC_APB2RSTR = RCC_APB2RSTR_TIM15RST;  // Reset TIM15
    RCC_APB2RSTR = 0;
    nvic_clear_pending_irq(NVIC_TIM15_IRQ);
    RCC_APB1ENR |= RCC_APB1ENR_USART2EN;   // Enable USART2
    GPIOA_AFRL |= 0x100;                   // A2 (USART2_TX)
    GPIOA_AFRH |= 0x10000000;             // A15 (USART2_RX)
}
```

## Input Protocol Detection Sequence

1. **Initial State**: PA2 configured as TIM15_CH1 input capture
2. **Protocol Detection**: Analyzes input signal timing on PA2
3. **Dynamic Switch**: If serial protocol detected (`cfg.input_mode >= 2`), calls `io_serial()` to reconfigure PA2 as USART2_TX

## Supported Serial Protocols on PA2

When switched to USART2_TX mode, PA2 supports:

- **Mode 2**: Custom Serial (57600 baud)
- **Mode 3**: iBUS (115200 baud) 
- **Mode 4**: SBUS/SBUS2 (100000 baud, inverted logic)
- **Mode 5**: CRSF (416666 baud)

## Hardware Resource Conflict Resolution

### PA2 Conflict Management
- **TIM15_CH1** vs **USART2_TX** - Resolved by dynamic switching via `io_serial()` function
- **Pull Configuration**: For SBUS mode, PA2 gets pull-down resistor:
  ```c
  GPIOA_PUPDR = (GPIOA_PUPDR & ~0x30) | 0x20; // A2 (pull-down)
  ```
- **Signal Inversion**: SBUS mode uses inverted TX/RX signals:
  ```c
  USART2_CR2 |= USART_CR2_RXINV | USART_CR2_TXINV;
  ```

### Protocol Switching Logic

The switching logic in `io.c` shows how PA2 transitions between modes:

```c
#ifdef IO_PA2
if (cfg.input_mode >= 2) {
    ioirq = serialirq;
    io_serial();
    switch (cfg.input_mode) {
        case 2: // Serial
            iodma = serialdma;
            rxlen = 4;
            USART2_BRR = CLK_CNT(SERIAL_BR);
            break;
        case 3: // iBUS
            iodma = ibusdma;
            rxlen = 32;
            USART2_BRR = CLK_CNT(115200);
            break;
        case 4: // SBUS/SBUS2
            iodma = sbusdma;
            rxlen = 25;
            USART2_BRR = CLK_CNT(100000);
            // Additional SBUS configuration...
            break;
        case 5: // CRSF
            ioirq = crsfirq;
            USART2_BRR = CLK_CNT(416666);
            break;
    }
}
#endif
```

## DMA Integration

### TIM15 Mode (Input Capture)
- **DMA1_CH5** for DSHOT frame capture and bidirectional communication
- **Circular mode** for continuous bit timing capture
- **32 × 16-bit samples** for DSHOT protocol decoding

### USART2 Mode (Serial TX)
- **DMA1_CH4** for USART2_TX transmission
- **DMA1_CH5** for USART2_RX reception (PA15)
- **Variable length transfers** based on protocol requirements

## MCU-Specific Configuration

### AT32F421 Configuration
```c
#define IOTIM TIM15
#define IOTIM_IDR (GPIOA_IDR & 0x4) // A2
#define IOTIM_DMA 5
#define iotim_isr tim15_isr

#define USART2_RX_DMA 5
#define USART2_TX_DMA 4
```

### STM32F051 Configuration
```c
void io_serial(void) {
    RCC_APB2RSTR = RCC_APB2RSTR_TIM15RST;
    RCC_APB2RSTR = 0;
    nvic_clear_pending_irq(NVIC_TIM15_IRQ);
    RCC_APB1ENR |= RCC_APB1ENR_USART2EN;
    GPIOA_AFRL |= 0x100; // A2 (USART2_TX)
    GPIOA_AFRH |= 0x10000000; // A15 (USART2_RX)
}
```

### STM32G071 Configuration
```c
void io_serial(void) {
    RCC_APBRSTR2 = RCC_APBRSTR2_TIM15RST;
    RCC_APBRSTR2 = 0;
    nvic_clear_pending_irq(NVIC_TIM15_IRQ);
    RCC_APBENR1 |= RCC_APBENR1_USART2EN;
    GPIOA_AFRL = (GPIOA_AFRL & ~0xf00) | 0x100; // A2 (USART2_TX)
    GPIOA_AFRH |= 0x10000000; // A15 (USART2_RX)
    DMAMUX1_CxCR(1) = DMAMUX_CxCR_DMAREQ_ID_USART2_RX;
    DMAMUX1_CxCR(5) = DMAMUX_CxCR_DMAREQ_ID_USART2_TX;
}
```

## Target Hardware Usage

PA2 is enabled on multiple ESC targets as defined in `CMakeLists.txt`:

```cmake
add_target(AIRBOT2 STM32F051 DEAD_TIME=26 COMP_MAP=321 SENS_MAP=0xA3 VOLT_MUL=738 IO_PA2)
add_target(DYS1 STM32F051 DEAD_TIME=26 COMP_MAP=123 SENS_MAP=0xA6A3 CURR_MUL=60 LED_MAP=0xAFB3B4 LED_INV IO_PA2)
add_target(EMAX1 STM32F051 DEAD_TIME=26 COMP_MAP=123 IO_PA2)
add_target(ESCAPE1 STM32G071 DEAD_TIME=35 COMP_MAP=123 HALL_MAP=0xAFB35 SENS_MAP=0xA6A5A4 VOLT_MUL=1100 CURR_MUL=30 BEC_MAP=0xCEF LED_WS2812 LED_STAT IO_PA2)
add_target(ESCAPE2 STM32G071 DEAD_TIME=35 COMP_MAP=123 SENS_MAP=0xA6 VOLT_MUL=1100 BEC_MAP=0xADE IO_PA2)
add_target(ESCAPE4 STM32G071 DEAD_TIME=35 COMP_MAP=123 HALL_MAP=0xB358 SENS_MAP=0xA6A5A4 VOLT_MUL=1100 CURR_MUL=30 BEC_MAP=0xCEF LED_MAP=0xF2AF LED1_INV LED_STAT IO_PA2)
add_target(ESCAPE5 STM32G071 DEAD_TIME=35 COMP_MAP=123 SENS_MAP=0xA6 VOLT_MUL=1100 BEC_MAP=0xA45 LED_WS2812 LED_STAT IO_PA2)
add_target(FLYCOLOR1 STM32F051 DEAD_TIME=26 COMP_MAP=123 SENS_MAP=0xA6 VOLT_MUL=1100 LED_MAP=0xB5B4B3 IO_PA2)
# And many more...
```

## Key Implementation Details

1. **Half-Duplex**: USART2 uses half-duplex mode (`USART_CR3_HDSEL`)
2. **Analog Mode**: PA2 can also be switched to analog mode for throttle input
3. **CLI Mode**: PA2 participates in command-line interface for configuration
4. **Priority Handling**: Input protocol detection takes priority, serial mode is secondary
5. **Error Handling**: Robust error detection and recovery for all protocols
6. **Timing Critical**: Precise timing requirements for DSHOT and servo protocols

## Protocol Timing Analysis

### DSHOT Protocol on PA2
- **Bit Timing**: 125ns-2µs depending on DSHOT variant (150/300/600/1200)
- **Frame Length**: 16 bits + telemetry request bit
- **Bidirectional**: Supports ESC telemetry response transmission

### Servo PWM on PA2
- **Pulse Width**: 1000-2000µs (1-2ms)
- **Period**: 20ms (50Hz) typical
- **Resolution**: 1µs timing precision

### Serial Protocols on PA2
- **SBUS**: 100,000 baud, inverted, 2 stop bits, even parity
- **iBUS**: 115,200 baud, 8N1 format
- **CRSF**: 416,666 baud, 8N1 format

## PB4 Alternative Pin Options

### Overview of PB4 Usage

**PB4** serves as an alternative input pin when PA2 is not available or conflicts with other peripherals. Unlike PA2's dual functionality, PB4 is primarily used for input protocol detection only.

### PB4 Support Status by MCU Family

| MCU Family | PB4 Support | Timer Used | DMA Channel | Configuration Required |
|------------|-------------|------------|-------------|----------------------|
| STM32F051  | ✅ **Ready** | TIM3_CH1 | DMA1_CH4 | Automatic fallback |
| STM32G071  | ✅ **Ready** | TIM3_CH1 | DMA1_CH1 | Third priority option |
| STM32G431  | ✅ **Ready** | TIM3_CH1 | DMA1_CH1 | Automatic fallback |
| STM32L431  | ❌ **Not Implemented** | TIM3_CH1* | DMA1_CH1* | Code changes needed |
| AT32F421   | ❌ **Resource Conflict** | TIM3_CH1* | DMA1_CH5* | Timer conflict with IFTIM |

*Theoretical capability - not currently implemented

### PB4 vs PA2 Functionality Comparison

| Feature | PA2 (TIM15_CH1) | PB4 (TIM3_CH1) | Advantage |
|---------|-----------------|----------------|-----------|
| **Input Protocols** | DSHOT, Servo, Oneshot | DSHOT, Servo, Oneshot | Equal |
| **Serial Communication** | ✅ USART2_TX | ❌ Input only | PA2 |
| **Dynamic Switching** | ✅ Timer ↔ UART | ❌ Timer only | PA2 |
| **Timer Channels** | 2 channels | 4 channels | PB4 |
| **MCU Compatibility** | Newer MCUs | Universal | PB4 |
| **Resource Conflicts** | Rare | Varies by MCU | Depends |

### PB4 Configuration Details

#### STM32F051 PB4 Setup
```c
// Automatic when IO_PA2 not defined
#define IOTIM TIM3
#define IOTIM_IDR (GPIOB_IDR & 0x10) // B4
#define IOTIM_DMA 4
#define iotim_isr tim3_isr

// Hardware configuration
GPIOB_AFRL |= 0x10000;    // B4 (AF1 - TIM3_CH1)
GPIOB_PUPDR |= 0x100;     // B4 (pull-up)
GPIOB_MODER &= ~0x100;    // B4 (alternate function mode)
```

#### STM32G071 PB4 Setup
```c
// Pin priority: PA2 → PA6 → PB4 (fallback)
#ifndef IO_PA2
    #ifndef IO_PA6
        #define IOTIM TIM3
        #define IOTIM_IDR (GPIOB_IDR & 0x10) // B4
        GPIOB_AFRL |= 0x10000;    // B4 (AF1 - TIM3_CH1)
        GPIOB_PUPDR |= 0x100;     // B4 (pull-up)
        GPIOB_MODER &= ~0x100;    // B4 (alternate function mode)
    #endif
#endif
```

#### STM32G431 PB4 Setup
```c
// High-performance with DMAMUX
#define IOTIM TIM3
#define IOTIM_IDR (GPIOB_IDR & 0x10) // B4
#define IOTIM_DMA 1

GPIOB_AFRL |= 0x20000;    // B4 (AF2 - TIM3_CH1)
GPIOB_MODER &= ~0x100;    // B4 (alternate function mode)
DMAMUX1_CxCR(1) = DMAMUX_CxCR_DMAREQ_ID_TIM3_CH1;
```

### AT32F421 Resource Conflict Analysis

**Problem**: TIM3_CH1 is already allocated to IFTIM (BEMF detection)
```c
// Current AT32F421 allocation
#define IFTIM TIM3        // BEMF detection uses TIM3_CH1
#define IOTIM TIM15       // PA2 input uses TIM15_CH1

// PB4 would conflict with IFTIM on TIM3_CH1
```

**Alternative Solutions for AT32F421**:
1. **Use PA4 + TIM14_CH1** (recommended)
2. **Use PB5 + TIM3_CH2** (different channel)
3. **Stay with PA2 + TIM15_CH1** (current default)

### How to Enable PB4 on Supported MCUs

For MCUs with built-in PB4 support (F051, G071, G431):

1. **Comment out IO_PA2 definition**:
```c
// In mcu/{MCU_FAMILY}/config.h
// #define IO_PA2  // Comment this out to use PB4
```

2. **Recompile the firmware**
3. **Connect signal to PB4** instead of PA2

### PB4 Protocol Support

**Supported Input Protocols on PB4**:
- ✅ **Servo PWM**: 1-2ms pulse width detection
- ✅ **Oneshot125**: 125-250µs pulse timing  
- ✅ **DSHOT**: All variants (150/300/600/1200)
- ❌ **Serial Protocols**: PB4 is input-only, no UART capability

**Not Supported on PB4**:
- ❌ iBUS, SBUS, CRSF serial communication
- ❌ Command Line Interface (CLI)
- ❌ Dynamic switching between timer and UART modes

### Performance Characteristics

**Timer Performance (PB4 vs PA2)**:
- **Identical timing precision** for input capture
- **Same DMA capabilities** for DSHOT processing
- **Equal protocol detection** performance
- **TIM3 has 4 channels** vs TIM15's 2 channels (potential for expansion)

**Resource Usage**:
- **Lower DMA conflicts** on some MCUs (dedicated channels)
- **Different interrupt priorities** may affect system performance
- **Timer resource isolation** from other peripherals

### Implementation Recommendations

**Choose PA2 when**:
- Need serial telemetry (iBUS, SBUS, CRSF)
- Require CLI for configuration
- Using newer MCU families (G071, G431, L431)
- Need dynamic protocol switching

**Choose PB4 when**:
- Only need input protocols (Servo, DSHOT, Oneshot)
- PA2 conflicts with other peripherals
- Using STM32F051 or STM32G431 hardware
- Want timer resource isolation

**Avoid PB4 when**:
- Using AT32F421 (resource conflicts)
- Using STM32L431 (not implemented)
- Need serial communication capabilities

## Conclusion

This design allows a single pin (PA2) to serve multiple critical functions while maintaining compatibility with various input protocols and providing serial telemetry capabilities. The dynamic switching mechanism ensures optimal resource utilization and protocol flexibility without requiring additional hardware pins.

**PB4 provides a robust alternative** for input-only applications, offering identical protocol detection performance with the advantage of timer resource isolation. The choice between PA2 and PB4 depends on your specific requirements for serial communication and MCU family support.

The implementation demonstrates sophisticated firmware design principles:
- **Resource Sharing**: Efficient use of limited MCU pins (PA2 dual-mode)
- **Alternative Options**: PB4 fallback for input-only applications
- **Dynamic Configuration**: Runtime adaptation to protocol requirements
- **Robust Operation**: Error handling and recovery mechanisms
- **Performance Optimization**: Hardware-accelerated protocol processing with DMA
- **Flexible Architecture**: Support for multiple pin configurations across MCU families
