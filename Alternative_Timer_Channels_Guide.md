# Alternative Timer Channels for PB4 When TIM3_CH1 Is Unavailable

## Problem Analysis

When **TIM3_CH1 is not available** (as in AT32F421 where it's used for IFTIM BEMF detection), you need alternative timer channels for PB4 input protocol detection. This document provides comprehensive alternatives.

## AT32F421 Specific Conflict

In AT32F421, TIM3 is fully utilized for BEMF detection:
```c
#define IFTIM TIM3                    // BEMF Timer
#define IFTIM_ICR TIM3_CCR1          // Uses CH1 for input capture
#define IFTIM_OCR TIM3_CCR3          // Uses CH3 for output compare
#define iftim_isr tim3_isr           // TIM3 interrupt handler
```

**Conclusion**: TIM3 is **completely unavailable** for PB4 input detection on AT32F421.

## Alternative Timer Solutions

### Option 1: Use Different Timer Channels (Recommended)

#### TIM3_CH2 Alternative (If Available)
If only TIM3_CH1 and TIM3_CH3 are used, **TIM3_CH2** might be available:

**GPIO Pin Options for TIM3_CH2**:
- **PB5** (AF1) - Primary option
- **PA7** (AF1) - Alternative (may conflict with SPI1_MOSI)

**Implementation**:
```c
// For PB5 as TIM3_CH2 input
#define IOTIM TIM3
#define IOTIM_CHANNEL 2                    // Use CH2 instead of CH1
#define IOTIM_IDR (GPIOB_IDR & 0x20)      // B5 instead of B4
#define IOTIM_DMA 4                        // TIM3_CH2 DMA
#define IOTIM_CCMR TIM3_CCMR1
#define IOTIM_ICM (TIM_CCMR1_CC2S_IN_TI2 | TIM_CCMR1_IC2F_CK_INT_N_8)
#define IOTIM_CCER TIM_CCER_CC2E
#define IOTIM_CCR TIM3_CCR2
#define IOTIM_ICIE TIM_DIER_CC2IE
```

**GPIO Configuration for PB5**:
```c
void initio_pb5(void) {
    RCC_AHB1ENR |= RCC_AHB1ENR_GPIOBEN;    // Enable GPIOB clock
    GPIOB_AFRL |= 0x100000;                // B5 (TIM3_CH2) - AF1
    GPIOB_PUPDR |= 0x400;                  // B5 (pull-up)
    GPIOB_MODER &= ~0xC00;                 // Clear mode bits for B5
    GPIOB_MODER |= 0x800;                  // B5 (alternate function)
}
```

#### TIM3_CH4 Alternative
If TIM3_CH4 is available:

**GPIO Pin Options for TIM3_CH4**:
- **PB1** (AF1) - Primary option
- **PC9** (AF1) - Alternative (if available on package)

**Implementation**:
```c
// For PB1 as TIM3_CH4 input
#define IOTIM TIM3
#define IOTIM_CHANNEL 4
#define IOTIM_IDR (GPIOB_IDR & 0x2)       // B1
#define IOTIM_DMA 2                        // TIM3_CH4 DMA
#define IOTIM_CCMR TIM3_CCMR2
#define IOTIM_ICM (TIM_CCMR2_CC4S_IN_TI4 | TIM_CCMR2_IC4F_CK_INT_N_8)
#define IOTIM_CCER TIM_CCER_CC4E
#define IOTIM_CCR TIM3_CCR4
#define IOTIM_ICIE TIM_DIER_CC4IE
```

### Option 2: Use Different Timers

#### TIM14 Solution (Highly Recommended)

**Advantages**:
- **Completely independent** from BEMF detection
- **Single channel timer** - perfect for input capture
- **Available on all MCU variants**
- **No conflicts** with existing functionality

**GPIO Pin Options for TIM14_CH1**:
- **PA4** (AF4) - Primary option
- **PA7** (AF4) - Alternative  
- **PF0** (AF2) - If available on package

**Complete TIM14 Implementation**:
```c
// AT32F421 TIM14 Configuration for Input Detection
#define IOTIM TIM14
#define IOTIM_CHANNEL 1
#define IOTIM_IDR (GPIOA_IDR & 0x10)      // A4
#define IOTIM_DMA 7                        // Available DMA channel
#define iotim_isr tim14_isr
#define IOTIM_CCMR TIM14_CCMR1
#define IOTIM_ICM (TIM_CCMR1_CC1S_IN_TI1 | TIM_CCMR1_IC1F_CK_INT_N_8)
#define IOTIM_CCER TIM_CCER_CC1E
#define IOTIM_CCR TIM14_CCR1
#define IOTIM_ICIE TIM_DIER_CC1IE

// GPIO Configuration for PA4
void initio_tim14_pa4(void) {
    RCC_AHB1ENR |= RCC_AHB1ENR_GPIOAEN;    // Enable GPIOA clock
    RCC_APB1ENR |= RCC_APB1ENR_TIM14EN;    // Enable TIM14 clock
    GPIOA_AFRL |= 0x40000;                 // A4 (TIM14_CH1) - AF4
    GPIOA_PUPDR |= 0x100;                  // A4 (pull-up)
    GPIOA_MODER &= ~0x300;                 // Clear mode bits for A4
    GPIOA_MODER |= 0x200;                  // A4 (alternate function)
}
```

#### TIM17 Solution

**Limitations**: 
- Currently used for beep/LED generation
- Would require sharing or disabling audio feedback

**GPIO Pin Options for TIM17_CH1**:
- **PA7** (AF1) - Primary option
- **PB9** (AF1) - Alternative

**Implementation** (if audio conflicts are acceptable):
```c
#define IOTIM TIM17
#define IOTIM_CHANNEL 1
#define IOTIM_IDR (GPIOA_IDR & 0x80)      // A7
#define IOTIM_DMA 1                        // Available DMA channel
#define iotim_isr tim17_isr
// Rest similar to TIM14...
```

### Option 3: Software Input Detection

If no suitable timer channels are available, implement software-based input detection:

**Concept**: Use GPIO interrupt + software timing
```c
// GPIO interrupt-based detection (for emergency use)
void gpio_pb4_interrupt_setup(void) {
    // Configure PB4 as GPIO input with interrupt
    RCC_AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    GPIOB_PUPDR |= 0x100;                  // B4 pull-up
    GPIOB_MODER &= ~0x300;                 // B4 input mode
    
    // Configure EXTI for PB4
    EXTI_IMR |= EXTI4;                     // Enable interrupt
    EXTI_RTSR |= EXTI4;                    // Rising edge trigger
    EXTI_FTSR |= EXTI4;                    // Falling edge trigger
    
    nvic_enable_irq(NVIC_EXTI4_IRQ);
}

void exti4_isr(void) {
    static uint32_t last_time;
    uint32_t current_time = systick_get_value();
    uint32_t pulse_width = current_time - last_time;
    
    // Process pulse width for protocol detection
    // (Less accurate than hardware timer capture)
    
    EXTI_PR |= EXTI4;  // Clear interrupt flag
    last_time = current_time;
}
```

## Recommended Solution by MCU

### AT32F421 Optimal Configuration

**Primary Recommendation**: Use **TIM14 + PA4**
```c
// mcu/AT32F421/config.h modifications
#ifdef IO_PB4_ALT  // New define for alternative PB4 functionality
#define IOTIM TIM14
#define IOTIM_IDR (GPIOA_IDR & 0x10)      // A4 (not PB4, but similar function)
#define IOTIM_DMA 7
#define iotim_isr tim14_isr
#else
// Keep existing PA2/TIM15 configuration
#define IOTIM TIM15
#define IOTIM_IDR (GPIOA_IDR & 0x4)       // A2
#define IOTIM_DMA 5
#define iotim_isr tim15_isr
#endif
```

**Alternative**: If PA4 conflicts exist, use **TIM3_CH2 + PB5**
```c
#ifdef IO_PB5_CH2  // Use PB5 as TIM3_CH2 if CH1 unavailable
#define IOTIM TIM3
#define IOTIM_CHANNEL 2
#define IOTIM_IDR (GPIOB_IDR & 0x20)      // B5
#define IOTIM_DMA 4
#define iotim_isr tim3_isr
// Special handling needed to avoid conflicts with IFTIM
#endif
```

## Pin Conflict Resolution Matrix

| Pin | Timer Option | Conflicts | Viability | Notes |
|-----|-------------|-----------|-----------|-------|
| **PB4** | TIM3_CH1 | ❌ IFTIM (BEMF) | **Not viable** | Already used for BEMF |
| **PB5** | TIM3_CH2 | ⚠️ Shared timer | **Possible** | Requires careful timing |
| **PB1** | TIM3_CH4 | ⚠️ Shared timer | **Possible** | Requires careful timing |
| **PA4** | TIM14_CH1 | ✅ None | **Excellent** | Completely independent |
| **PA7** | TIM14_CH1 or TIM17_CH1 | ⚠️ SPI1_MOSI or Audio | **Good** | Choose based on usage |
| **PB9** | TIM17_CH1 | ⚠️ Audio feedback | **Fair** | Disables beep generation |

## Implementation Guidelines

### Step 1: Choose Alternative Pin/Timer
1. **First choice**: TIM14_CH1 on PA4 (cleanest solution)
2. **Second choice**: TIM3_CH2 on PB5 (requires shared timer handling)
3. **Third choice**: TIM17_CH1 on PA7 (requires audio conflict resolution)

### Step 2: Modify Configuration Files
Update MCU-specific config.h and config.c files with chosen alternative.

### Step 3: Handle Timer Sharing (if applicable)
If using TIM3_CH2, implement careful interrupt and DMA management to avoid conflicts with IFTIM.

### Step 4: Test Protocol Compatibility
Verify all input protocols (DSHOT, Servo, OneShot125) work correctly with the new timer/pin combination.

## Code Example: Complete TIM14 Solution

```c
// mcu/AT32F421/config.h - Alternative configuration
#pragma once

#define CLK 120000000

// Choose input pin configuration
#ifdef IO_PA4_TIM14
#define IOTIM TIM14
#define IOTIM_IDR (GPIOA_IDR & 0x10)      // A4
#define IOTIM_DMA 7
#define iotim_isr tim14_isr
#define INPUT_PIN_NAME "PA4"
#elif defined IO_PB5_TIM3CH2  
#define IOTIM TIM3
#define IOTIM_CHANNEL 2
#define IOTIM_IDR (GPIOB_IDR & 0x20)      // B5
#define IOTIM_DMA 4
#define iotim_isr tim3_ch2_isr            // Custom handler
#define INPUT_PIN_NAME "PB5"
#else
// Default PA2 configuration
#define IOTIM TIM15
#define IOTIM_IDR (GPIOA_IDR & 0x4)       // A2
#define IOTIM_DMA 5
#define iotim_isr tim15_isr
#define INPUT_PIN_NAME "PA2"
#endif

// Rest of configuration...
```

```c
// mcu/AT32F421/config.c - GPIO initialization
void initio(void) {
#ifdef IO_PA4_TIM14
    // Configure PA4 as TIM14_CH1 input
    RCC_AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC_APB1ENR |= RCC_APB1ENR_TIM14EN;
    GPIOA_AFRL |= 0x40000;                // A4 (TIM14_CH1) - AF4
    GPIOA_PUPDR |= 0x100;                 // A4 (pull-up)
    GPIOA_MODER &= ~0x300;                // Clear mode bits
    GPIOA_MODER |= 0x200;                 // A4 (alternate function)
#elif defined IO_PB5_TIM3CH2
    // Configure PB5 as TIM3_CH2 input
    RCC_AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    GPIOB_AFRL |= 0x100000;               // B5 (TIM3_CH2) - AF1
    GPIOB_PUPDR |= 0x400;                 // B5 (pull-up)
    GPIOB_MODER &= ~0xC00;                // Clear mode bits
    GPIOB_MODER |= 0x800;                 // B5 (alternate function)
#else
    // Default PA2 configuration
    // ... existing code ...
#endif
}
```

## Summary

When **TIM3_CH1 is unavailable** due to BEMF detection usage:

1. **Best Solution**: Use **TIM14_CH1 on PA4** - completely independent, no conflicts
2. **Alternative**: Use **TIM3_CH2 on PB5** - requires shared timer management  
3. **Fallback**: Use **TIM17_CH1 on PA7** - conflicts with audio, but workable

The **TIM14 solution is strongly recommended** as it provides the cleanest implementation with no resource conflicts.
