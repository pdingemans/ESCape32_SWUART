# ESCape32_SWUART Hardware Resources Analysis

## Overview

This document provides a comprehensive analysis of all timers, interrupts, and DMA channels used in the ESCape32_SWUART electronic speed controller (ESC) firmware project. The analysis covers multiple MCU families supported by the project.

## Project Architecture

ESCape32_SWUART is a brushless motor controller firmware using libopencm3 library, supporting multiple MCU families:
- **STM32F051** (48MHz, ARM Cortex-M0)
- **STM32G071** (64MHz, ARM Cortex-M0+)  
- **STM32G431** (120MHz, ARM Cortex-M4)
- **STM32L431** (80MHz, ARM Cortex-M4)
- **AT32F421** (120MHz, ARM Cortex-M4)
- **GD32E230** (72MHz, ARM Cortex-M23)
- **GD32F350** (108MHz, ARM Cortex-M4)

## Timer Resources

### Primary Motor Control Timers

#### TIM1 - Motor PWM Generation
**Function**: Six-step commutation PWM generation for BLDC motor control

**Configuration**:
- **Frequency**: Variable (16-96 kHz based on configuration)
- **Resolution**: CLK_KHZ / 24 - 1 (for sine mode)
- **Dead Time**: Configurable via `DEAD_TIME` constant
- **Channels**: CC1, CC2, CC3 for three-phase PWM
- **Advanced Features**:
  - Complementary outputs (CCxN) for low-side switching
  - Break input for safety shutdown
  - Dead-time insertion for shoot-through protection

**Interrupts Used**:
- `TIM_DIER_COMIE` - Commutation interrupt
- `TIM_DIER_UIE` - Update interrupt (software blanking)
- `TIM_DIER_CC4IE` - Compare 4 interrupt (software blanking)

**MCU-Specific Notes**:
- **STM32G4/AT32F4**: Different register handling for commutation
- **TIM1_CCR5**: Available on advanced timers for additional functionality

### AT32F421 Timer Allocation Summary

**AT32F421 Complete Timer Usage**:
| Timer | Function | Channels Used | DMA Channel | Priority | GPIO Pins | Notes |
|-------|----------|---------------|-------------|----------|-----------|--------|
| TIM1 | Motor PWM | CC1, CC2, CC3 + CCxN | - | Critical | PA8,9,10/PB13,14,15 | Advanced timer with dead-time |
| TIM3 | BEMF Detection | CC1 (Input Capture) | - | Critical | PA6 (BEMF input) | IFTIM - Zero-crossing detection |
| TIM6 | System Delays | - | - | Medium | - | Basic timer for timeouts |
| TIM15 | I/O Protocol | CC1 (Input Capture) | DMA1_CH5 | High | PA2 (Signal input) | IOTIM - DSHOT/Servo/CLI |
| TIM14 | **Available** | All channels free | - | Low | PA4/PA7/PF0 | Available for auxiliary functions |
| TIM16 | **SW UART** | CC1 (TX/RX timing) | DMA1_CH3 | High | PA6/PB8 | **Software UART for SPORT** |
| TIM17 | Beep/LED | CC1 (PWM Output) | - | Low | PA7/PB9 | Audio feedback, LED control |

**AT32F421 DMA Channel Allocation**:
| DMA Channel | Current Usage | Direction | Data Size | Mode | Available for SW UART |
|-------------|---------------|-----------|-----------|------|---------------------|
| DMA1_CH1 | ADC1 | P→M | 16-bit | Circular | ✅ **Available** (when ADC not used) |
| DMA1_CH2 | USART1_TX | M→P | 8-bit | Linear | ⚠️ **Conflicts with Serial** |
| DMA1_CH3 | USART1_RX | P→M | 8-bit | Circular | ⚠️ **Conflicts with Serial** |
| DMA1_CH4 | USART2_TX | M→P | 8-bit | Linear | ⚠️ **Conflicts with Serial2** |
| DMA1_CH5 | IOTIM + USART2_RX | P→M | 16/8-bit | Circular | ❌ **Critical - IOTIM** |
| DMA1_CH6 | **Available** | - | - | - | ✅ **Available** |
| DMA1_CH7 | **Available** | - | - | - | ✅ **Available** |

**AT32F421 Peripheral Resource Mapping**:
| Peripheral | DMA Request | Timer Trigger | GPIO Pins | Usage Priority | Function |
|------------|-------------|---------------|-----------|----------------|----------|
| ADC1 | DMA1_CH1 | TIM1_TRGO, TIM15_TRGO | PA0-PA7, PB0-PB1 | Medium | Sensor readings (voltage, current, temperature) |
| USART1 | DMA1_CH2/CH3 | - | PB6 (TX), PB7 (RX) | Medium | **Debug/CLI Communication** (38.4k baud) |
| USART2 | DMA1_CH4/CH5 | - | PA2 (TX), PA15 (RX) | High | **Serial Telemetry** (iBUS/SBUS/CRSF protocols) |
| TIM14 | **No DMA** | - | PA4, PA7, PF0 | **Available** | **Recommended for SW UART** |
| TIM17 | **No DMA** | - | PA7, PB9 | Low | Audio/LED (WS2812 or beep generation) |
| SPI1 | DMA1_CH2/CH3 | - | PA5/PA6/PA7 | Available | Not currently used |
| I2C1 | DMA1_CH6/CH7 | - | PB6/PB7, PB8/PB9 | Available | Not currently used |

**AT32F421 USART Usage Details**:
| USART | TX Pin | RX Pin | DMA TX | DMA RX | Purpose | Priority | Baud Rate |
|-------|--------|--------|--------|--------|---------|----------|-----------|
| USART1 | PB6 | PB7 | DMA1_CH2 | DMA1_CH3 | **CLI/Debug Interface** | 0x80 | 38400 bps |
| USART2 | PA2* | PA15 | DMA1_CH4 | DMA1_CH5 | **Telemetry Protocols** | 0x40 | Variable |

**📝 Notes**:
- **PA2 Conflict**: USART2_TX (PA2) conflicts with TIM15_CH1 (IOTIM) - switched via `io_serial()` function
- **Protocol Switching**: PA2 dynamically switches between timer input capture and USART2 TX
- **USART1**: Dedicated debug interface, always available on PB6/PB7
- **USART2**: Used for telemetry when in serial mode (iBUS, SBUS, CRSF)
- **SW UART Recommendation**: Use different pins (PA4, PA7, or PF0) to avoid conflicts

**AT32F421 Resource Availability**:
- ✅ **TIM16**: Present and used for software UART implementation
- ⚠️ **DMA1_CH3**: Conflicts with USART1_RX (see conflict analysis below)
- ✅ **TIM14**: Completely available for auxiliary functions
- ⚠️ **TIM17**: Currently used for audio/LED but can be shared

**AT32F421 Current SW UART Configuration**:
```c
// AT32F421 Software UART - SPORT Telemetry
#define SWUART_TIMER         TIM16
#define SWUART_DMA_CHANNEL   DMA_CHANNEL3  
#define SWUART_GPIO_PORT     GPIOB
#define SWUART_GPIO_PIN      GPIO6
#define SWUART_BAUD_RATE     57600
#define SWUART_EXTI_LINE     EXTI6
#define SWUART_EXTI_IRQ      NVIC_EXTI4_15_IRQ
#define SWUART_DMA_IRQ       NVIC_DMA1_CHANNEL2_3_DMA2_CHANNEL1_2_IRQ
```

**AT32F421 Timer Capabilities**:
- **TIM1**: Advanced-control timer (16-bit, complementary outputs, break input)
- **TIM3**: General-purpose timer (16-bit, 4 channels)  
- **TIM6**: Basic timer (16-bit, no input/output)
- **TIM14**: General-purpose timer (16-bit, 1 channel)
- **TIM15**: General-purpose timer (16-bit, 2 channels)
- **TIM16**: General-purpose timer (16-bit, 1 channel) - **Used for SW UART**
- **TIM17**: General-purpose timer (16-bit, 1 channel)

---

#### IFTIM (Input Frequency Timer) - BEMF Detection
**Function**: Back-EMF zero-crossing detection for sensorless motor control

**MCU Mapping**:
| MCU Family | Timer | Channel | Resolution | Filter |
|------------|-------|---------|------------|---------|
| STM32F051 | TIM2 | CC4 | 2x (500ns) | 64 cycles |
| STM32G071 | TIM2 | CC1/CC2 | 2x (500ns) | 64 cycles |
| STM32G431 | TIM3 | CC1 | 0x (125ns) | 128 cycles |
| STM32L431 | TIM2 | CC4 | 2x (500ns) | 128 cycles |
| AT32F421 | TIM3 | CC1 | 0x (125ns) | 128 cycles |

**Configuration Details**:
- **Input Capture**: Digital filter for noise immunity
- **Prescaler**: Dynamic based on ERPM (electrical RPM)
  - High speed: No prescaler (125ns resolution)
  - Medium speed: /2 prescaler (250ns resolution) 
  - Low speed: /4 prescaler (500ns resolution)
- **Timeout**: Update interrupt for desynchronization detection

**Interrupts**:
- `IFTIM_ICIE` - Input capture interrupt (zero-crossing detection)
- `TIM_DIER_UIE` - Update interrupt (timeout/desync detection)

---

#### IOTIM (Input/Output Timer) - Protocol Handling
**Function**: Multi-protocol input signal processing and CLI communication

**MCU Mapping**:
| MCU Family | Timer | I/O Pin | DMA Channel | Purpose |
|------------|-------|---------|-------------|---------|
| STM32F051 | TIM15/TIM3 | PA2/PB4 | 5/4 | Input capture + CLI |
| STM32G071 | TIM15/TIM3 | PA2/PA6/PB4 | 1 | Input capture + CLI |
| STM32G431 | TIM15 | PA2 | 5 | Input capture + CLI |
| STM32L431 | TIM15 | PA2 | 5 | Input capture + CLI |
| AT32F421 | TIM15 | PA2 | 5 | Input capture + CLI |

**Protocol Support**:
1. **Servo PWM**: 1-2ms pulse width, 50-500Hz
2. **Oneshot125**: 125-250µs pulse width  
3. **DSHOT**: Digital shot protocol (150/300/600/1200)
4. **CLI**: Software UART for configuration (38.4k baud)

**DMA Usage**:
- **RX**: Circular DMA for DSHOT bit capture (32 samples)
- **TX**: Linear DMA for DSHOT telemetry transmission (23 bits)

---

### Communication Timers

#### TIM15 - Timing Control
**Function**: Precision timing for SBUS telemetry and protocol switching

**Usage Scenarios**:
- **SBUS Mode**: Slot timing for telemetry transmission
  - 50µs detection window for A15 pin
  - 7280µs RX disable timing
  - 3980µs TX delay timing
  - 660µs slot intervals

**Configuration**:
- **Resolution**: 125ns (CLK_MHZ / 8 - 1 prescaler)
- **Mode**: One-pulse mode (OPM) for precise timing
- **Interrupt**: Update interrupt for timing completion

---

#### TIM3 (Hall Sensor Mode) - Sensor Input
**Function**: Hall sensor input processing for sensored operation

**Configuration** (when Hall sensors enabled):
- **Input Capture**: Any edge detection on Hall inputs
- **XOR Gate**: `TIM_CR2_TI1S` for multi-sensor combining
- **Resolution**: 500ns (CLK_MHZ / 2 - 1 prescaler) 
- **Timeout**: Update interrupt for sensor failure detection

**Interrupts**:
- `TIM_DIER_CC1IE` - Hall state change interrupt
- `TIM_DIER_UIE` - Hall sensor timeout interrupt

---

### System Timers

#### TIM6 - System Utilities
**Function**: Delays, timeouts, and system timing

**Usage**:
- **Arming Delays**: 250ms zero-throttle requirement
- **Error Handling**: 1s delay in fault conditions  
- **Beep Generation**: Variable timing for audio feedback

**Configuration**:
- **Flexible Prescaler**: Configurable for different time bases
  - 0.1ms resolution (CLK_KHZ / 10 - 1)
  - 1ms resolution for longer delays
- **One-Pulse Mode**: For precise delay generation

---

#### SysTick Timer - System Heartbeat
**Function**: Real-time operating system tick and scheduling

**Configuration**:
- **Frequency**: 16 kHz (CLK_KHZ / 16 - 1)
- **Priority**: Highest interrupt priority
- **Purpose**: 
  - 16 kHz servo update rate
  - 1 kHz system management tasks
  - ADC trigger coordination
  - LED update scheduling

---

### Utility Timers

#### TIM14/TIM17 - Auxiliary Functions
**Function**: Utility timing and auxiliary PWM generation

**Applications**:
- **Beep Generation**: Audio feedback and status indication
- **LED Control**: PWM for status LED brightness
- **Auxiliary Outputs**: Additional PWM channels if available

---

## USART1 Usage in ESCape32_SWUART

### USART1 Overview
USART1 is the primary telemetry interface on AT32F421, configured on **PB6** (single-wire half-duplex mode) for various telemetry protocols.

### USART1 Configuration by Telemetry Mode

#### **Mode 2: iBUS Telemetry** (`src/telem.c` lines 39-48)
```c
case 2: // iBUS
    iofunc = ibusfunc;
    USART1_BRR = CLK_CNT(115200);    // 115200 baud
    USART1_RTOR = 12;                 // TX delay ~100µs
    USART1_CR2 = USART_CR2_RTOEN;
    USART1_CR3 = USART_CR3_HDSEL | USART_CR3_DMAT | USART_CR3_DMAR;
    USART1_CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_IDLEIE;
```

**Configuration**:
- **Baud Rate**: 115200 bps
- **Mode**: Half-duplex (single-wire)
- **DMA**: TX (CH2) and RX (CH3) enabled
- **Interrupts**: Idle line detection for frame completion
- **Function**: Responds to iBUS sensor queries with ESC telemetry data

#### **Mode 3: S.Port Telemetry** (`src/telem.c` lines 49-64)

**Non-AT32F4 Path** (STM32 variants):
```c
case 3: // S.Port
    iofunc = sportfunc;
    USART1_BRR = CLK_CNT(57600);      // 57600 baud (FrSky standard)
    USART1_RTOR = 26;                  // TX delay ~450µs
    USART1_CR2 = USART_CR2_RTOEN | USART_CR2_RXINV | USART_CR2_TXINV;
    GPIOB_PUPDR = (GPIOB_PUPDR & ~0x3000) | 0x2000; // B6 (pull-down)
    USART1_CR3 = USART_CR3_HDSEL | USART_CR3_DMAT | USART_CR3_DMAR;
    USART1_CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_IDLEIE;
```

**Configuration**:
- **Baud Rate**: 57600 bps (FrSky SPORT standard)
- **Signal**: Inverted RX/TX (SPORT protocol requirement)
- **Pull**: Pull-down on PB6 (idle state low)
- **Timeout**: 450µs receiver timeout for frame detection
- **Function**: USART hardware handles SPORT protocol natively

**AT32F4 Path** (uses Software UART instead):
```c
#ifdef AT32F4
    singlewire_uart_init();           // SW UART replaces USART1
    sw_uart_set_rx_callback(sportcallback, NULL);
    return;                           // Exit - SW UART handles everything
#endif
```

**Configuration**:
- **USART1**: Not used for SPORT on AT32F421
- **Implementation**: Pure software UART using TIM16 + DMA (see `mcu/AT32F421/swuart/`)
- **Reason**: Hardware USART limitations on AT32F421 require software implementation

#### **Mode 4: CRSF Telemetry** (`src/telem.c` lines 65-72)
```c
case 4: // CRSF
    USART1_BRR = CLK_CNT(416666);     // 416666 baud (CRSF standard)
    USART1_CR3 = USART_CR3_HDSEL | USART_CR3_DMAT;
    USART1_CR1 = USART_CR1_UE | USART_CR1_TE;
```

**Configuration**:
- **Baud Rate**: 416666 bps (CRSF protocol standard)
- **Mode**: TX-only (no DMA RX)
- **Function**: Sends CRSF telemetry frames to receiver

### USART1 DMA Configuration (`src/telem.c` lines 80-84)

```c
DMA_CPAR(USART1_DMA_BASE, USART1_RX_DMA) = (uint32_t)&USART1_RDR;
DMA_CMAR(USART1_DMA_BASE, USART1_RX_DMA) = (uint32_t)iobuf;
DMA_CPAR(USART1_DMA_BASE, USART1_TX_DMA) = (uint32_t)&USART1_TDR;
DMA_CMAR(USART1_DMA_BASE, USART1_TX_DMA) = (uint32_t)iobuf;
```

**DMA Channel Allocation**:
- **RX**: DMA1_CH3 (peripheral → memory, circular/linear)
- **TX**: DMA1_CH2 (memory → peripheral, linear)
- **Buffer**: 16-byte shared buffer `iobuf`

### USART1 Interrupt Handler (`src/telem.c` lines 86-116)

```c
void usart1_isr(void) {
    // Dual-mode handler: transmission complete or idle line detection
    
    // TX Complete: Switch back to RX mode
    if (cr & USART_CR1_TCIE) goto reading;
    
    // RX Complete: Process received data
    USART1_ICR = USART_ICR_IDLECF | USART_ICR_ORECF | USART_ICR_RTOCF;
    int len = iofunc(sizeof iobuf - DMA_CNDTR(USART1_DMA_BASE, USART1_RX_DMA));
    
    // If response needed: Switch to TX mode
    if (len) {
        USART1_CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_TCIE;
        DMA_CCR(USART1_DMA_BASE, USART1_TX_DMA) = DMA_CCR_EN | ...;
    }
    
reading:
    // Return to RX mode
    USART1_CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_RTOIE;
    DMA_CCR(USART1_DMA_BASE, USART1_RX_DMA) = DMA_CCR_EN | ...;
}
```

**Handler Behavior**:
1. **Idle Line Interrupt**: Triggered when frame received (gap detected)
2. **Calculate Length**: `received = buffer_size - DMA_CNDTR`
3. **Protocol Processing**: Call `iofunc()` (iBUS/SPORT/CRSF handler)
4. **Response**: If needed, enable TX DMA and wait for completion
5. **Return to RX**: Re-enable RX DMA for next frame

### USART1 Hardware Initialization (`mcu/AT32F421/config.c` lines 56, 83, 93)

```c
// Clock enable
RCC_APB2ENR = ... | RCC_APB2ENR_USART1EN;

// GPIO configuration (already shown in config section)
GPIOB_MODER = 0xffffeffa; // B6 (USART1_TX/RX half-duplex)
GPIOB_PUPDR = 0x00001000; // B6 (pull-up by default)

// NVIC configuration
nvic_set_priority(NVIC_USART1_IRQ, 0x80);         // Medium priority
nvic_enable_irq(NVIC_USART1_IRQ);
```

**Interrupt Priority**:
- **USART1_IRQ**: Priority 0x80 (medium)
- **DMA1_CH2_3_IRQ**: Priority 0x80 (shared with ADC)
- **Lower than**: Motor control (priority 0)
- **Higher than**: System utilities (priority varies)

### USART1 Resource Conflicts

#### **DMA Channel 3 Conflict with SW UART**
- **USART1_RX**: Uses DMA1_CH3 for iBUS/non-AT32 SPORT
- **SW UART**: Also configured for DMA1_CH3 (AT32F421 config)
- **Resolution**: On AT32F421 SPORT mode, USART1 is **completely disabled** and replaced by SW UART

#### **Protocol Switching**
```c
void inittelem(void) {
    switch (telmode) {
        case 2: /* iBUS - USART1 active */
        case 3: /* SPORT - SW UART on AT32F421, USART1 on others */
        case 4: /* CRSF - USART1 active */
    }
}
```

**Key Points**:
- **iBUS/CRSF**: Always use hardware USART1
- **SPORT on STM32**: Uses hardware USART1 with signal inversion
- **SPORT on AT32F421**: Uses SW UART (TIM16 + DMA), USART1 unused

### USART1 Pin Configuration

**AT32F421 Pin Assignment**:
| Signal | Pin | Function | Mode | Pull |
|--------|-----|----------|------|------|
| USART1_TX/RX | PB6 | Half-duplex telemetry | Alternate function | Pull-up (default), Pull-down (SPORT) |

**Pin Conflicts**:
- No conflicts on AT32F421 (PB6 dedicated to telemetry)
- On STM32G071: PB6 may share with I2C1_SCL (not used in this project)

---

## DMA Resources

### DMA1 Channel Allocation

#### Channel Mapping by MCU

**STM32F051**:
| Channel | Function | Direction | Size | Circular |
|---------|----------|-----------|------|----------|
| DMA1_CH1 | ADC1 | P→M | 16-bit | Yes |
| DMA1_CH2 | USART1_TX | M→P | 8-bit | No |
| DMA1_CH3 | USART1_RX | P→M | 8-bit | Yes |
| DMA1_CH4 | IOTIM (TIM3) | P→M | 16-bit | Yes |
| DMA1_CH5 | IOTIM (TIM15) | P→M | 16-bit | Yes |

**STM32G071**:
| Channel | Function | Direction | Size | Circular |
|---------|----------|-----------|------|----------|
| DMA1_CH1 | IOTIM + USART2_RX | P→M | 16/8-bit | Yes |
| DMA1_CH2 | USART1_RX + ADC1 | P→M | 8/16-bit | Yes |
| DMA1_CH3 | USART1_TX | M→P | 8-bit | No |
| DMA1_CH5 | USART2_TX | M→P | 8-bit | No |

**AT32F421**:
| Channel | Function | Direction | Size | Circular |
|---------|----------|-----------|------|----------|
| DMA1_CH2 | USART1_TX | M→P | 8-bit | No |
| DMA1_CH3 | USART1_RX | P→M | 8-bit | Yes |
| DMA1_CH4 | USART2_TX | M→P | 8-bit | No |
| DMA1_CH5 | IOTIM + USART2_RX | P→M | 16/8-bit | Yes |

**STM32G431/L431**:
| Channel | Function | Direction | Size | Circular |
|---------|----------|-----------|------|----------|
| DMA1_CH1 | USART2_RX + ADC1 | P→M | 8/16-bit | Yes |
| DMA1_CH3 | USART1_TX | M→P | 8-bit | No |
| DMA1_CH5 | IOTIM | P→M | 16-bit | Yes |
| DMA1_CH6 | USART2_TX | M→P | 8-bit | No |

---

### DMA Usage Patterns

#### Protocol-Specific DMA Usage

**DSHOT Protocol**:
- **RX Phase**: 32 × 16-bit circular DMA for bit timing capture
- **TX Phase**: 23 × 16-bit linear DMA for telemetry transmission  
- **Bidirectional**: Automatic switching between RX/TX modes

**Serial Protocols (iBUS/SBUS/CRSF)**:
- **RX**: Variable length circular DMA (4-64 bytes)
- **TX**: Fixed length linear DMA for responses
- **Error Recovery**: DMA restart on protocol errors

**ADC Sampling**:
- **Continuous**: Circular DMA for sensor readings
- **Channels**: Temperature, voltage, current, analog input
- **Trigger**: Software or timer-triggered conversion

---

## Interrupt Structure

### Interrupt Priority Levels

| Priority | Interrupts | Purpose |
|----------|------------|---------|
| 0 (Highest) | TIM1_COM, IFTIM | Motor control critical timing |
| 1 | DMA transfers, TIM6 | Data movement and delays |
| 2 | System utilities | Non-critical system functions |
| 0x80 | PendSV | Low-priority background tasks |

### Interrupt Service Routines

#### Motor Control Interrupts
- **`tim1_com_isr()`**: Commutation event handling
- **`iftim_isr()`**: BEMF zero-crossing detection
- **`tim3_isr()`**: Hall sensor state changes (if enabled)

#### Communication Interrupts  
- **`iotim_isr()`**: Input protocol processing
- **`iodma_isr()`**: DMA completion for I/O operations
- **`usart2_isr()`**: Serial communication events

#### System Interrupts
- **`sys_tick_handler()`**: 16kHz system tick
- **`pend_sv_handler()`**: Background task processing  
- **`hard_fault_handler()`**: Error handling and recovery

---

## Software UART Integration

### SW UART Module (Newly Added)

**Location**: `mcu/AT32F421/swuart/`

**Resources Used**:
- **Timer**: TIM3 for bit timing generation
- **DMA**: DMA1_Channel3 for data transfer
- **GPIO**: GPIOB Pin 6 for single-wire communication
- **Interrupts**: TIM3_IRQ, DMA1_Channel2_3_IRQ

**Features**:
- **Pure DMA Operation**: Zero CPU overhead during transmission
- **Compile-time Optimization**: Pre-calculated timing constants
- **Fast Configuration**: <500ns reconfiguration time
- **FIFO Buffering**: 64-byte circular buffers
- **libopencm3 Compatible**: STM32F1 family includes

### SW UART Resource Recommendations

#### Available Timer/DMA Combinations for Software UART

Based on the current resource allocation, the following timer and DMA combinations are **recommended for software UART implementation**:

**Primary Recommendation - TIM14 + Available DMA Channel**:
| MCU Family | Timer | DMA Channel | Rationale |
|------------|-------|-------------|-----------|
| STM32F051 | TIM14 | DMA1_CH7* | Utility timer, no conflicts |
| STM32G071 | TIM14 | DMA1_CH4 | Available utility timer |
| STM32G431 | TIM14 | DMA1_CH2 | Utility timer, minimal conflicts |
| STM32L431 | TIM14 | DMA1_CH2 | Available utility timer |
| AT32F421 | TIM14 | DMA1_CH1 | Currently unused timer |

**Alternative Recommendation - TIM17 + Available DMA Channel**:
| MCU Family | Timer | DMA Channel | Rationale |
|------------|-------|-------------|-----------|
| STM32F051 | TIM17 | DMA1_CH7* | Auxiliary timer, audio conflicts only |
| STM32G071 | TIM17 | DMA1_CH4 | Available with minimal usage |
| STM32G431 | TIM17 | DMA1_CH2 | Low-priority auxiliary timer |
| STM32L431 | TIM17 | DMA1_CH2 | Available auxiliary timer |
| AT32F421 | TIM17 | DMA1_CH1 | Currently underutilized |

**⚠️ Conflict Analysis**:
- **TIM14/TIM17**: Currently used for beep generation and LED control
- **Impact**: SW UART usage will disable audio feedback during transmission
- **Mitigation**: Implement priority system (motor control > SW UART > audio)

**✅ Advantages of TIM14/TIM17 Selection**:
1. **Non-Critical**: No impact on motor control or safety systems
2. **Flexible Timing**: Support for various baud rates (9600-115200)
3. **Low Interrupt Priority**: Won't interfere with real-time operations
4. **Available Channels**: Multiple compare/capture channels for TX/RX

**📝 Implementation Notes**:
- **STM32F051**: DMA1_CH7 may need verification in reference manual
- **Baud Rate Support**: 9600, 19200, 38400, 57600, 115200 bps recommended
- **GPIO Selection**: Choose pins not conflicting with motor outputs (A, B, C phases)
- **Buffer Management**: Use circular DMA for RX, linear for TX
- **Error Handling**: Implement frame error detection and recovery

**🔧 Configuration Template**:
```c
// Example for TIM14 + DMA1_CH2 (STM32G431/L431)
#define SWUART_TIMER         TIM14
#define SWUART_DMA_CHANNEL   DMA1_Channel2
#define SWUART_GPIO_PORT     GPIOB
#define SWUART_GPIO_PIN      GPIO6
#define SWUART_BAUD_RATE     38400
#define SWUART_PRESCALER     (CLK_MHZ / (16 * SWUART_BAUD_RATE) - 1)
```

This recommendation ensures minimal impact on existing motor control functionality while providing robust software UART capabilities.

---

## SPORT Telemetry Protocol Analysis

### SPORT 2-Byte Reception Mechanism

The ESCape32_SWUART firmware implements a sophisticated mechanism to ensure SPORT telemetry functions are called only after exactly 2 bytes are received. This combines hardware timing, DMA, and interrupt handling.

#### **USART1 Configuration for SPORT**
```c
case 3: // S.Port
    iofunc = sportfunc;
    USART1_BRR = CLK_CNT(57600);           // 57.6k baud rate
    USART1_RTOR = 26;                      // TX delay ~450µs
    USART1_CR2 = USART_CR2_RTOEN |         // Enable receiver timeout
                 USART_CR2_RXINV |         // Invert RX signal
                 USART_CR2_TXINV;          // Invert TX signal
    GPIOB_PUPDR = (GPIOB_PUPDR & ~0x3000) | 0x2000; // B6 (pull-down)
    USART1_CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_IDLEIE;
```

**Key Configuration Elements**:
- **Baud Rate**: 57,600 bps (standard FrSky SPORT rate)
- **Signal Inversion**: Both RX/TX inverted for SPORT protocol
- **Idle Line Detection**: `USART_CR1_IDLEIE` enables idle line interrupts
- **Receiver Timeout**: 450µs timeout for frame completion detection
- **Pull-down**: GPIO configured for inverted signaling (idle low)

#### **DMA Reception Setup**
```c
DMA_CNDTR(USART1_DMA_BASE, USART1_RX_DMA) = sizeof iobuf;  // 16 bytes buffer
DMA_CCR(USART1_DMA_BASE, USART1_RX_DMA) = DMA_CCR_EN | 
                                           DMA_CCR_MINC | 
                                           DMA_CCR_PSIZE_8BIT | 
                                           DMA_CCR_MSIZE_8BIT;
```

**Important**: No circular mode (`DMA_CCR_CIRC`) - DMA stops on timeout or completion.

#### **Interrupt Trigger Logic**

The `usart1_isr()` is triggered by **idle line detection** or **receiver timeout**:

```c
void usart1_isr(void) {
    // Clear interrupt flags (idle, overrun, timeout)
    USART1_ICR = USART_ICR_IDLECF | USART_ICR_ORECF | USART_ICR_RTOCF;
    
    // Calculate received bytes: initial_count - remaining_count
    int len = iofunc(sizeof iobuf - DMA_CNDTR(USART1_DMA_BASE, USART1_RX_DMA));
    //                   16      -              (14 after 2 bytes)
    // Result: len = 16 - 14 = 2 bytes received
}
```

#### **SPORT Frame Validation**
```c
static int sportfunc(int len) {
    static const uint16_t type[] = {0xb70, 0x400, 0x210, 0x200, 0xb30, 0x500};
    static int n;
    
    // Strict validation: exactly 2 bytes with valid header
    if (len != 2 || iobuf[0] != 0x7e) return 0;
    
    // Check sensor ID match (lower 5 bits)
    if ((iobuf[1] & 0x1f) != cfg.telem_phid - 1) return 0;
    
    // Cycle through telemetry types
    if (n == 6) n = 0;
    int t = type[n];
    switch (n++) {
        case 0: return sportresp(t, temp1);        // ESC temperature
        case 1: return sportresp(t, temp2);        // Motor temperature  
        case 2: return sportresp(t, volt);         // Voltage
        case 3: return sportresp(t, curr * 205 >> 11); // Current
        case 4: return sportresp(t, csum);         // Consumption
        case 5: return sportresp(t, erpm / (cfg.telem_poles >> 1)); // RPM
    }
    return 0;
}
```

#### **SPORT Protocol Timing Analysis**

**Bit-Level Timing @ 57600 baud**:
- **Bit Duration**: ~17.4µs per bit
- **Byte Duration**: ~174µs (10 bits: start + 8 data + stop)
- **2-Byte Frame**: ~348µs total transmission time
- **Idle Detection**: 450µs timeout ensures frame completion

**Frame Structure**:
```
SPORT Request: [0x7E] [Sensor_ID] |<-- 450µs idle -->| Next frame
               Byte 0   Byte 1
```

**Reception Flow**:
1. **DMA Reception**: Hardware automatically stores bytes in buffer
2. **Idle Detection**: USART detects 450µs gap after 2 bytes
3. **Interrupt Trigger**: `usart1_isr()` called on idle line
4. **Length Calculation**: `len = 16 - DMA_CNDTR = 2`
5. **Function Call**: `sportfunc(2)` processes the request
6. **Validation**: Confirms exactly 2 bytes with valid SPORT header
7. **Response**: Generates telemetry response if sensor ID matches

#### **Hardware-Software Coordination**

**Why Exactly 2 Bytes Work**:
1. **SPORT Protocol**: Receiver always sends 2-byte polling frames
2. **Idle Line Detection**: Hardware automatically detects frame end
3. **Timeout Protection**: 450µs timeout prevents incomplete frames
4. **Strict Validation**: Software rejects any frame ≠ 2 bytes
5. **Signal Inversion**: Proper idle state detection for SPORT

**Robustness Features**:
- **Length Validation**: Rejects malformed frames
- **Header Validation**: Confirms SPORT frame marker (0x7E)
- **ID Validation**: Only responds to matching sensor IDs
- **Timeout Protection**: Prevents hanging on incomplete data
- **Error Recovery**: Automatic restart after invalid frames

This mechanism elegantly handles SPORT's request-response pattern where receivers send exactly 2-byte polls, and ESCs must respond only to their specific sensor ID with appropriate telemetry data.

---

## Resource Utilization Summary

### Per-MCU Resource Usage

| MCU | Timers Used | DMA Channels | Interrupts | Clock Speed |
|-----|-------------|--------------|------------|-------------|
| STM32F051 | TIM1,2,3,6,14,15,17 | 5 channels | 12 handlers | 48 MHz |
| STM32G071 | TIM1,2,3,6,14,15,17 | 4 channels | 10 handlers | 64 MHz |
| STM32G431 | TIM1,2,6,15,17 | 4 channels | 9 handlers | 120 MHz |
| STM32L431 | TIM1,2,6,15,17 | 4 channels | 9 handlers | 80 MHz |
| AT32F421 | TIM1,3,6,15,17 | 4 channels | 9 handlers | 120 MHz |

### Critical Timing Requirements

| Function | Timing Constraint | Implementation |
|----------|------------------|----------------|
| DSHOT Decode | <500ns bit decode | DMA + hardware filtering |
| Commutation | <1µs switching | Hardware timer + interrupt |
| BEMF Detection | 125-500ns resolution | Input capture with filtering |
| PWM Generation | 16-96 kHz | Advanced timer with dead-time |

---

## Configuration Flexibility

### Compile-Time Options

**I/O Configuration**:
- `IO_PA2`: Enable PA2 input with full protocol support
- `IO_PA6`: Alternative input pin for STM32G071
- `SW_BLANKING`: Software-based switching noise blanking
- `HALL_MAP`: Enable Hall sensor support with hybrid operation

**Protocol Selection**:
- Input modes: Servo, Analog, Serial, iBUS, SBUS, CRSF
- Telemetry modes: KISS, iBUS, S.Port, CRSF
- Automatic protocol detection and switching

**Performance Tuning**:
- Variable PWM frequency based on motor speed
- Dynamic timer resolution for optimal precision
- Configurable dead-time and blanking periods

---

## Development Notes

### Debugging Resources
- **Error Handling**: Comprehensive fault detection and recovery
- **Status Indication**: LED patterns for different operating states  
- **CLI Interface**: Real-time configuration and monitoring
- **Telemetry**: Detailed motor and ESC status reporting

### Extension Points
- **Additional Timers**: TIM14/TIM17 available for custom features
- **Spare DMA Channels**: Available for additional data streaming
- **GPIO Expansion**: Unused pins available for sensors/outputs
- **Software UART**: Modular design for additional communication channels

This analysis demonstrates the sophisticated hardware resource management in ESCape32_SWUART, showing efficient utilization of MCU peripherals for high-performance motor control applications.
