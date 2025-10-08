# PB4 Pin Configuration Guide for ESCape32

## Overview

This guide explains how to use **PB4** instead of **PA2** for input protocol detection and serial communication in the ESCape32 project. PB4 is already implemented as an alternative input pin on several MCU variants.

## Current PB4 Support Status

### MCUs with Built-in PB4 Support

| MCU Family | PB4 Support | Timer Used | DMA Channel | Configuration Required |
|------------|-------------|------------|-------------|----------------------|
| **STM32F051** | ✅ **Native** | TIM3_CH1 | DMA1_CH4 | Remove `IO_PA2` define |
| **STM32G071** | ✅ **Native** | TIM3_CH1 | DMA1_CH1 | Remove `IO_PA2` and `IO_PA6` |
| **STM32G431** | ✅ **Native** | TIM3_CH1 | DMA1_CH1 | Remove `IO_PA2` define |
| **STM32L431** | ❌ **Not implemented** | - | - | Manual configuration required |
| **AT32F421** | ❌ **Not implemented** | - | - | Manual configuration required |
| **GD32E230** | ❌ **Not implemented** | - | - | Manual configuration required |

## How to Enable PB4 Support

### Method 1: For MCUs with Native Support (STM32F051, STM32G071, STM32G431)

Simply **remove** the `IO_PA2` define from the MCU configuration:

#### STM32F051 Configuration
```c
// In mcu/STM32F051/config.h
#define CLK 48000000
// Remove this line: #define IO_PA2

// This automatically enables PB4 support:
// #define IOTIM TIM3
// #define IOTIM_IDR (GPIOB_IDR & 0x10) // B4
// #define IOTIM_DMA 4
```

#### STM32G071 Configuration  
```c
// In mcu/STM32G071/config.h
#define CLK 64000000
// Remove these lines:
// #define IO_PA2
// #define IO_PA6

// This automatically enables PB4 support:
// #define IOTIM TIM3
// #define IOTIM_IDR (GPIOB_IDR & 0x10) // B4
// #define IOTIM_DMA 1
```

#### STM32G431 Configuration
```c
// In mcu/STM32G431/config.h  
#define CLK 120000000
// Remove this line: #define IO_PA2

// This automatically enables PB4 support:
// #define IOTIM TIM3
// #define IOTIM_IDR (GPIOB_IDR & 0x10) // B4
// #define IOTIM_DMA 1
```

### Method 2: For MCUs Requiring Manual Configuration (AT32F421, STM32L431)

#### AT32F421 Manual PB4 Configuration

**Step 1**: Modify `mcu/AT32F421/config.h`:
```c
#pragma once

#define CLK 120000000
// Remove: #define IO_PA2
#define IO_PB4  // Add new define for PB4 support

#define IFTIM TIM3
// ...existing IFTIM configuration...

// Change IOTIM configuration from PA2 to PB4:
#ifdef IO_PB4
#define IOTIM TIM3  // Use TIM3 instead of TIM15
#define IOTIM_IDR (GPIOB_IDR & 0x10) // B4
#define IOTIM_DMA 3  // Available DMA channel for TIM3
#define iotim_isr tim3_isr
#else
#define IOTIM TIM15
#define IOTIM_IDR (GPIOA_IDR & 0x4) // A2
#define IOTIM_DMA 5
#define iotim_isr tim15_isr
#endif
#define iodma_isr dma1_channel4_7_dma2_channel3_5_isr

// Rest of configuration remains the same...
```

**Step 2**: Modify `mcu/AT32F421/config.c` to add PB4 initialization:
```c
void initio(void) {
#ifdef IO_PB4
    // Configure PB4 as TIM3_CH1 input
    RCC_AHB1ENR |= RCC_AHB1ENR_GPIOBEN;  // Enable GPIOB clock
    GPIOB_AFRL |= 0x10000;               // B4 (TIM3_CH1) - AF1
    GPIOB_PUPDR |= 0x100;                // B4 (pull-up)
    GPIOB_MODER &= ~0x300;               // B4 (alternate function)
    GPIOB_MODER |= 0x200;                // B4 (alternate function)
#else
    // Existing PA2 configuration...
#endif
}

#ifdef IO_PB4
void io_serial(void) {
    RCC_APB1RSTR = RCC_APB1RSTR_TIM3RST;  // Reset TIM3
    RCC_APB1RSTR = 0;
    nvic_clear_pending_irq(NVIC_TIM3_IRQ);
    RCC_APB1ENR |= RCC_APB1ENR_USART2EN;
    // Note: PB4 cannot be used for USART2_TX, keep PA2 for serial mode
    GPIOA_AFRL |= 0x100;                  // A2 (USART2_TX)
    GPIOA_AFRH |= 0x10000000;             // A15 (USART2_RX)
}
#endif
```

## Functional Differences: PA2 vs PB4

### Timer and DMA Mapping Changes

| Aspect | PA2 Configuration | PB4 Configuration |
|--------|------------------|-------------------|
| **Timer** | TIM15_CH1 | TIM3_CH1 |
| **DMA Channel** | DMA1_CH5 | DMA1_CH4 (STM32F051) / DMA1_CH1 (STM32G071/G431) |
| **Interrupt** | `tim15_isr` | `tim3_isr` |
| **GPIO Config** | `GPIOA_IDR & 0x4` | `GPIOB_IDR & 0x10` |
| **AF Function** | AF0 (TIM15_CH1) | AF1 (TIM3_CH1) |

### Protocol Support Comparison

| Protocol | PA2 Support | PB4 Support | Notes |
|----------|-------------|-------------|-------|
| **DSHOT** | ✅ Full support | ✅ Full support | Same functionality |
| **Servo PWM** | ✅ Full support | ✅ Full support | Same functionality |
| **OneShot125** | ✅ Full support | ✅ Full support | Same functionality |
| **Serial Protocols** | ✅ Dynamic switch to USART2_TX | ⚠️ **Limited** | PB4 cannot be USART2_TX |

### Critical Limitation: Serial Communication

**Important**: PB4 **cannot** be used as USART2_TX, so serial protocols have limitations:

```c
// PA2 can switch between TIM15_CH1 and USART2_TX
PA2: TIM15_CH1 ↔ USART2_TX (dynamic switching)

// PB4 can only be used for timer input, not USART
PB4: TIM3_CH1 only (no USART capability)
```

## Recommended PB4 Implementation Strategy

### Option 1: PB4 for Input Only (Recommended)
- **PB4**: Input protocol detection (DSHOT, Servo, OneShot125)
- **PA2**: Reserved for USART2_TX when serial protocols needed
- **Benefit**: Full protocol compatibility

### Option 2: Hybrid Configuration
- **PB4**: Primary input pin for timer-based protocols
- **PA15**: USART2_RX for serial reception
- **Alternative TX**: Use USART1_TX (PB6) for serial transmission
- **Limitation**: Different USART for TX/RX

### Option 3: Software UART Alternative
- **PB4**: Input protocol detection via TIM3
- **PB6**: Software UART for serial communication
- **Benefit**: Independent serial communication

## Configuration Examples

### STM32F051 with PB4 (Working Example)
```c
// mcu/STM32F051/config.h
#define CLK 48000000
// #define IO_PA2  // Comment out to enable PB4

// Automatically configured:
#define IOTIM TIM3
#define IOTIM_IDR (GPIOB_IDR & 0x10) // B4
#define IOTIM_DMA 4
#define iotim_isr tim3_isr
```

### AT32F421 Custom PB4 Configuration
```c
// mcu/AT32F421/config.h
#define CLK 120000000
#define IO_PB4  // Custom define for PB4 support

#ifdef IO_PB4
#define IOTIM TIM3
#define IOTIM_IDR (GPIOB_IDR & 0x10) // B4
#define IOTIM_DMA 3
#define iotim_isr tim3_isr
#else
#define IOTIM TIM15
#define IOTIM_IDR (GPIOA_IDR & 0x4) // A2
#define IOTIM_DMA 5
#define iotim_isr tim15_isr
#endif
```

## Hardware Considerations

### Pin Conflicts to Check

1. **Motor Outputs**: Ensure PB4 doesn't conflict with motor phase outputs
2. **Hall Sensors**: Check if PB4 is used for Hall sensor inputs
3. **Other Peripherals**: Verify no conflicts with SPI, I2C, or other functions

### GPIO Configuration Requirements

```c
// PB4 Timer Input Configuration
GPIOB_AFRL |= 0x10000;     // B4 alternate function (AF1 for TIM3_CH1)
GPIOB_PUPDR |= 0x100;      // B4 pull-up resistor
GPIOB_MODER &= ~0x300;     // Clear mode bits
GPIOB_MODER |= 0x200;      // B4 alternate function mode
```

## Testing and Validation

### Validation Steps
1. **Signal Detection**: Confirm DSHOT/servo signals detected on PB4
2. **Timing Accuracy**: Verify pulse width measurements are correct
3. **Protocol Switching**: Test automatic protocol detection
4. **Serial Fallback**: Ensure serial protocols work with PA2 fallback

### Debug Points
- Monitor `IOTIM_IDR` register for PB4 state changes
- Check TIM3 input capture values vs expected timing
- Verify DMA transfer completion for protocol decoding

## Migration Steps

### From PA2 to PB4 (STM32F051/G071/G431)
1. Remove `#define IO_PA2` from config.h
2. Recompile firmware
3. Connect input signal to PB4 instead of PA2
4. Test all supported protocols

### For AT32F421 (Manual Implementation)
1. Add PB4 configuration to config.h and config.c
2. Implement GPIO initialization for PB4
3. Handle TIM3 vs TIM15 differences in timing
4. Test with oscilloscope to verify signal integrity

## Conclusion

**PB4 is an excellent alternative to PA2** for input protocol detection, especially for:
- DSHOT protocols (all variants)
- Servo PWM signals
- OneShot125 protocols

**Limitations**:
- Cannot directly support serial protocols (iBUS, SBUS, CRSF) without additional configuration
- Requires different timer (TIM3 vs TIM15) affecting DMA channel allocation

**Best Use Cases**:
- ESCs that only need DSHOT/servo input protocols
- Boards where PA2 is needed for other functions
- Applications requiring separation of input detection and serial communication

The choice between PA2 and PB4 depends on your specific protocol requirements and pin availability constraints.
