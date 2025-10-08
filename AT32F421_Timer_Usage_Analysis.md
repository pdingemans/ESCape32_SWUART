# AT32F421 Timer Usage Analysis - ESCape32_SWUART

## Overview

This document analyzes the timer peripheral usage in the ESCape32_SWUART firmware for the AT32F421 microcontroller. The AT32F421 is an ARM Cortex-M4 MCU running at 120MHz with multiple timer peripherals for motor control applications.

## AT32F421 Available Timers

The AT32F421 microcontroller includes the following timer peripherals:
- **TIM1**: Advanced-control timer (16-bit)
- **TIM3**: General-purpose timer (16-bit) 
- **TIM6**: Basic timer (16-bit)
- **TIM14**: General-purpose timer (16-bit)
- **TIM15**: General-purpose timer (16-bit)
- **TIM16**: General-purpose timer (16-bit)
- **TIM17**: General-purpose timer (16-bit)

## Timer Usage in ESCape32_SWUART

### Used Timers

#### TIM1 - Motor PWM Generation ✅ **USED**
- **Purpose**: Primary motor control PWM generation
- **Configuration**: Advanced-control timer for 3-phase brushless motor control
- **Channels Used**: 
  - CH1: Phase A PWM (PA8)
  - CH2: Phase B PWM (PA9) 
  - CH3: Phase C PWM (PA10)
  - CH1N: Phase A Low-side (PA7)
  - CH2N: Phase B Low-side (PB0)
  - CH3N: Phase C Low-side (PB1)
- **Interrupts**: 
  - TIM1_BRK_UP_TRG_COM_IRQ (Break, Update, Trigger, COM)
  - TIM1_CC_IRQ (Capture/Compare)
- **Clock**: APB2 (120MHz)
- **Special Features**: 
  - Complementary PWM outputs with dead-time insertion
  - ADC trigger via CC1 event
  - Slave mode triggered by TIM3 (TRGI=ITR2)

#### TIM3 - Input Frequency Timer (IFTIM) ✅ **USED** 
- **Purpose**: Input capture and frequency measurement
- **Configuration**: Input capture on CH1, Output compare on CH3
- **Channels Used**:
  - CH1: Input capture (IC1) for comparator output timing
  - CH3: Output compare (OC3) for trigger generation
- **Interrupts**: TIM3_IRQ (tim3_isr)
- **Clock**: APB1 (120MHz)
- **Special Features**:
  - Master mode - TRGO output on OC3REF
  - Input capture with digital filtering
  - Triggers TIM1 for synchronization

#### TIM6 - ADC Calibration Timer ✅ **USED**
- **Purpose**: ADC calibration delay timing
- **Configuration**: Basic timer for precise timing delays
- **Usage**: 
  - Provides 3μs delay for ADC calibration (RM 18.4.2.1)
  - One-pulse mode (OPM) operation
  - Auto-reload value: CLK_MHZ * 3 - 1 (359 at 120MHz)
- **Clock**: APB1 (120MHz)
- **Mode**: One-shot timing for ADC setup

#### TIM15 - I/O Timer (IOTIM) ✅ **USED**
- **Purpose**: Serial input/output timing and control
- **Configuration**: Input/output timing control on PA2
- **Usage**:
  - Conditional compilation - only enabled when not in ANALOG mode
  - DMA channel 5 for data transfer
  - Input monitoring on PA2 (IO_PA2 mode)
- **Interrupts**: TIM15_IRQ (tim15_isr) 
- **Clock**: APB2 (120MHz)
- **GPIO**: PA2 configured as TIM15_CH1

#### TIM17 - LED Control Timer ✅ **USED** (Conditional)
- **Purpose**: WS2812 LED strip control
- **Configuration**: PWM generation for WS2812 protocol timing
- **Usage**: Only compiled when LED_WS2812 is defined
- **Channels Used**: CH1N on PB7 (TIM17_CH1N)
- **Features**:
  - DMA-driven PWM for precise timing
  - Repetition counter for LED protocol
  - Clock frequency: 800kHz for WS2812 timing
- **Clock**: APB2 (120MHz)
- **Special**: Complementary output with MOE (Master Output Enable)

### Unused Timers

#### TIM14 - ❌ **UNUSED**
- **Status**: Available for additional functionality
- **Potential Uses**: 
  - Additional PWM output
  - General timing functions
  - Encoder input
  - Timeout monitoring
- **Clock**: APB1 (120MHz)
- **Channels**: 1 channel available

#### TIM16 - ❌ **UNUSED** 
- **Status**: Available for additional functionality  
- **Potential Uses**:
  - Additional PWM generation
  - Input capture
  - One-pulse generation
  - Break input functionality
- **Clock**: APB2 (120MHz)
- **Channels**: 1 channel with complementary output

## Clock Configuration

### Timer Clock Sources
- **APB1 Timers** (TIM3, TIM6, TIM14): 120MHz
- **APB2 Timers** (TIM1, TIM15, TIM16, TIM17): 120MHz

### Clock Enable Configuration
```c
// APB2 Timer Clocks
RCC_APB2ENR = RCC_APB2ENR_TIM1EN | RCC_APB2ENR_TIM15EN | RCC_APB2ENR_TIM17EN;

// APB1 Timer Clocks  
RCC_APB1ENR = RCC_APB1ENR_TIM3EN | RCC_APB1ENR_TIM6EN;
```

## Pin Assignments

### TIM1 (Motor Control)
| Channel | Pin | Function | Description |
|---------|-----|----------|-------------|
| CH1 | PA8 | TIM1_CH1 | Phase A High-side PWM |
| CH2 | PA9 | TIM1_CH2 | Phase B High-side PWM |
| CH3 | PA10 | TIM1_CH3 | Phase C High-side PWM |
| CH1N | PA7 | TIM1_CH1N | Phase A Low-side PWM |
| CH2N | PB0 | TIM1_CH2N | Phase B Low-side PWM |
| CH3N | PB1 | TIM1_CH3N | Phase C Low-side PWM |

### TIM15 (I/O Control)
| Channel | Pin | Function | Description |
|---------|-----|----------|-------------|
| CH1 | PA2 | TIM15_CH1 | Serial I/O Control |

### TIM17 (LED Control - Optional)
| Channel | Pin | Function | Description |
|---------|-----|----------|-------------|
| CH1N | PB7 | TIM17_CH1N | WS2812 LED Data |

## Interrupt Configuration

### Active Interrupts
- **NVIC_TIM1_BRK_UP_TRG_COM_IRQ**: TIM1 advanced events
- **NVIC_TIM1_CC_IRQ**: TIM1 capture/compare events  
- **NVIC_TIM3_IRQ**: TIM3 input capture and timing
- **NVIC_TIM15_IRQ**: TIM15 I/O timing (when enabled)

### Interrupt Priorities
```c
nvic_set_priority(NVIC_TIM15_IRQ, 0x40);  // Medium priority
// TIM1, TIM3 use default priorities
```

## DMA Usage

### Timer-DMA Assignments
- **TIM15**: DMA Channel 5 (when enabled)
- **TIM17**: DMA Channel 1 (for LED control)

## DMA Channel Configuration

### AT32F421 DMA Channels Overview
The AT32F421 has DMA1 with 7 channels. Each channel can be assigned to different peripherals through a request mapping system.

### Used DMA Channels

#### DMA1 Channel 1 - ADC1 / TIM17_CH1 ✅ **USED**
- **Primary Function**: ADC data transfer for sensor readings
- **Secondary Function**: TIM17 LED control (WS2812) when LED_WS2812 is enabled
- **Configuration**: 
  - ADC mode: Peripheral-to-Memory, 16-bit transfers
  - LED mode: Memory-to-Peripheral, 16-bit transfers to TIM17_CCR1
- **Buffer**: `buf[6]` for ADC data, `led[5]` for WS2812 data
- **ISR**: `dma1_channel1_isr`
- **Features**: Shared channel with priority switching between ADC and LED

#### DMA1 Channel 2 - USART1_TX ✅ **USED**
- **Function**: USART1 telemetry transmission 
- **Configuration**: Memory-to-Peripheral, 8-bit transfers
- **Target**: USART1_TDR register
- **Protocols**: KISS, iBUS, S.Port, CRSF telemetry
- **ISR**: `usart1_tx_dma_isr`
- **Buffer**: `iobuf[16]` shared with RX

#### DMA1 Channel 3 - USART1_RX ✅ **USED**
- **Function**: USART1 telemetry reception
- **Configuration**: Peripheral-to-Memory, 8-bit transfers  
- **Source**: USART1_RDR register
- **Protocols**: iBUS, S.Port bidirectional telemetry
- **Buffer**: `iobuf[16]` shared with TX
- **Features**: Circular mode for continuous reception

#### DMA1 Channel 4 - USART2_TX ✅ **USED** (Conditional)
- **Function**: USART2 transmission for CLI and protocols
- **Configuration**: Memory-to-Peripheral, 8-bit transfers
- **Protocols**: CLI, Serial, SBUS telemetry responses
- **Condition**: Only when IO_PA2 is defined
- **Buffer**: `iobuf[1024]` large buffer for CLI and protocol data

#### DMA1 Channel 5 - TIM15/USART2_RX ✅ **USED** (Conditional)
- **Primary Function**: TIM15 input capture for DSHOT timing
- **Secondary Function**: USART2_RX for protocol reception
- **Configuration**:
  - TIM15: Peripheral-to-Memory, 16-bit transfers from TIM15_CCR1
  - USART2: Peripheral-to-Memory, 8-bit transfers from USART2_RDR
- **DSHOT Buffer**: `dshotbuf1[32]` for timing capture
- **Protocol Buffer**: `iobuf[1024]` for data reception
- **ISR**: `iodma_isr` (shared handler)

### Unused DMA Channels

#### DMA1 Channel 6 - ❌ **UNUSED**
- **Status**: Available for expansion
- **Potential Uses**:
  - Additional timer capture/compare
  - SPI/I2C peripheral support
  - Additional USART channel

#### DMA1 Channel 7 - ❌ **UNUSED**
- **Status**: Available for expansion  
- **Potential Uses**:
  - TIM1 update events for advanced motor control
  - Additional peripheral support
  - Memory-to-memory transfers

### DMA Interrupt Routing

#### DMA1_Channel1_IRQ
- **Handler**: `dma1_channel1_isr`
- **Priority**: 0x80 (Medium)
- **Functions**: ADC completion, LED control completion

#### DMA1_Channel2_3_DMA2_Channel1_2_IRQ  
- **Handlers**: `usart1_tx_dma_isr`
- **Priority**: 0x80 (Medium)
- **Functions**: USART1 TX completion

#### DMA1_Channel4_7_DMA2_Channel3_5_IRQ
- **Handlers**: `iodma_isr` (TIM15/USART2)
- **Priority**: 0x40 (High)
- **Functions**: DSHOT frame capture, protocol reception

### DMA Resource Sharing Strategies

#### Channel 1 Sharing (ADC/LED)
```c
// ADC has priority, LED uses channel when ADC is idle
if (DMA1_CCR(1) & DMA_CCR_EN) { 
    y = x; // Queue LED command
    return;
}
// Switch to LED mode when ADC completes
```

#### Channel 5 Dual Function (TIM15/USART2)
- **TIM15 Mode**: Input capture for DSHOT protocol timing
- **USART2 Mode**: Serial protocol reception (iBUS, SBUS, CRSF)
- **Switching**: Runtime reconfiguration based on detected protocol

### DMA Performance Optimization

#### Burst Transfers
- **USART1**: Single byte transfers for telemetry
- **USART2**: Variable length for protocols (4-64 bytes)
- **TIM15**: 32×16-bit captures for DSHOT frames
- **ADC**: 6×16-bit sensor readings burst

#### Memory Alignment
- **16-bit buffers**: 4-byte aligned for optimal performance
- **Circular buffers**: Used for continuous data streams
- **Shared buffers**: Reduce memory usage for bidirectional protocols

## Timer Synchronization

### Master-Slave Configuration
- **TIM3** → **TIM1**: TIM3 provides trigger (TRGO) to TIM1 (TRGI=ITR2)
- **TIM1**: Slave mode enabled, triggered by TIM3 for ADC timing synchronization

## Resource Utilization Summary

### Timer Resources
| Timer | Status | Usage | Clock | Pins Used | Pin Purpose | DMA Channel | Special Features |
|-------|--------|--------|-------|-----------|-------------|-------------|------------------|
| TIM1 | ✅ Used | Motor PWM Control | APB2 | PA8, PA9, PA10, PA7, PB0, PB1 | 3-Phase PWM (H+L sides) | None | Advanced control, Dead-time, ADC trigger |
| TIM3 | ✅ Used | Input Capture/Timing | APB1 | Internal | Comparator timing, Master trigger | None | Master mode, Trigger output |
| TIM6 | ✅ Used | ADC Calibration | APB1 | Internal | ADC calibration delay | None | Basic timer, One-pulse |
| TIM14 | ❌ Unused | Available | APB1 | Available | - | Available | General purpose |
| TIM15 | ✅ Used | I/O Control | APB2 | PA2 | Serial I/O control | DMA1_CH5 | Conditional (non-ANALOG) |
| TIM16 | ❌ Unused | Available | APB2 | Available | - | Available | General purpose, Complementary |
| TIM17 | ✅ Used | LED Control | APB2 | PB7 | WS2812 LED data | DMA1_CH1 | Conditional (LED_WS2812) |

### DMA Resources
| Channel | Status | Primary Use | Secondary Use | Buffer Size | Transfer Type |
|---------|--------|-------------|---------------|-------------|---------------|
| DMA1_CH1 | ✅ Used | ADC Data | TIM17 LED Control | 6×16-bit / 5×16-bit | P2M / M2P |
| DMA1_CH2 | ✅ Used | USART1 TX | - | 16×8-bit | M2P |
| DMA1_CH3 | ✅ Used | USART1 RX | - | 16×8-bit | P2M |
| DMA1_CH4 | ✅ Used | USART2 TX | - | 1024×8-bit | M2P |
| DMA1_CH5 | ✅ Used | TIM15 IC | USART2 RX | 32×16-bit / 1024×8-bit | P2M |
| DMA1_CH6 | ❌ Unused | Available | - | - | - |
| DMA1_CH7 | ❌ Unused | Available | - | - | - |

**Timer Utilization**: 5 out of 7 timers (71% utilization)  
**DMA Utilization**: 5 out of 7 channels (71% utilization)  
**Available Resources**: 2 timers + 2 DMA channels for additional features

### Detailed Pin Usage by Timer

#### TIM1 - Motor Control (6 pins used)
| Pin | Function | Channel | Purpose | Signal Type |
|-----|----------|---------|---------|-------------|
| PA8 | TIM1_CH1 | CH1 | Phase A High-side PWM | PWM Output |
| PA9 | TIM1_CH2 | CH2 | Phase B High-side PWM | PWM Output |
| PA10 | TIM1_CH3 | CH3 | Phase C High-side PWM | PWM Output |
| PA7 | TIM1_CH1N | CH1N | Phase A Low-side PWM | Complementary PWM |
| PB0 | TIM1_CH2N | CH2N | Phase B Low-side PWM | Complementary PWM |
| PB1 | TIM1_CH3N | CH3N | Phase C Low-side PWM | Complementary PWM |

#### TIM15 - I/O Control (1 pin used)
| Pin | Function | Channel | Purpose | Signal Type |
|-----|----------|---------|---------|-------------|
| PA2 | TIM15_CH1 | CH1 | Serial I/O timing control | Input Capture/PWM |

#### TIM17 - LED Control (1 pin used)
| Pin | Function | Channel | Purpose | Signal Type |
|-----|----------|---------|---------|-------------|
| PB7 | TIM17_CH1N | CH1N | WS2812 LED data output | PWM Output |

#### Available Timer Pins (Unused)
| Timer | Available Pins | Potential Functions |
|-------|----------------|-------------------|
| TIM14 | PA4, PA7*, PB1* | General PWM, Input capture |
| TIM16 | PA6, PB8 | PWM, Complementary PWM |

*Note: PA7 and PB1 are used by TIM1, so TIM14 would need different pin configuration

## Expansion Possibilities

The unused timers (TIM14, TIM16) and DMA channels (CH6, CH7) provide opportunities for:

### Timer Expansion (TIM14, TIM16)
1. **Additional PWM Outputs**: Servo control, auxiliary motor control
2. **Sensor Inputs**: Hall sensor decoding, encoder feedback
3. **Communication Timing**: Additional serial protocols, timeout monitoring  
4. **Protection Features**: Overcurrent monitoring, thermal protection timing
5. **User Interface**: LED PWM control, buzzer control

### DMA Expansion (CH6, CH7)
1. **High-Speed Data Transfer**: 
   - SPI sensor communication (IMU, magnetometer)
   - I2C peripheral management
   - Memory-to-memory data processing
2. **Advanced Timer Features**:
   - TIM1 update events for advanced motor control
   - Multi-channel PWM generation
   - Synchronized timer operations
3. **Communication Enhancement**:
   - Additional USART channels
   - Parallel protocol handling
   - Buffer management for high-throughput data

### Combined Timer+DMA Applications
1. **Advanced Motor Control**: 
   - Field-oriented control with DMA-driven calculations
   - High-frequency PWM with automatic dead-time adjustment
2. **Multi-Protocol Communication**:
   - Simultaneous protocol handling
   - Protocol bridging capabilities
3. **Real-Time Data Processing**:
   - Sensor fusion with DMA-based data collection
   - Automated calibration routines

## Conclusion

The ESCape32_SWUART firmware efficiently utilizes the AT32F421's timer and DMA resources for its core motor control functionality while maintaining flexibility for future enhancements. The resource architecture provides:

### Timer Utilization
- Robust 3-phase motor control with dead-time protection
- Precise input frequency measurement and timing
- Conditional LED control capability
- Synchronized ADC triggering
- Room for expansion with 2 unused timers

### DMA Utilization  
- Zero-overhead data transfers for all high-frequency operations
- Intelligent resource sharing (ADC/LED on CH1, TIM15/USART2 on CH5)
- Protocol-specific buffer management
- Efficient interrupt handling with DMA-driven transfers
- Strategic channel allocation supporting multiple communication protocols

### Resource Efficiency
- **71% timer utilization** (5/7 timers) with conditional usage optimization
- **71% DMA utilization** (5/7 channels) with intelligent sharing strategies
- **Minimal CPU overhead** through DMA-driven peripheral operations
- **Flexible architecture** supporting multiple input protocols and telemetry modes

### Key Architectural Strengths
1. **Shared Resource Strategy**: DMA channels serve dual purposes without conflicts
2. **Protocol Flexibility**: Runtime switching between input protocols and telemetry modes
3. **Performance Optimization**: DMA handles all high-frequency data movement
4. **Development Headroom**: 2 timers + 2 DMA channels available for expansion

This implementation demonstrates optimal resource allocation for a brushless motor controller while preserving significant development flexibility for advanced features like sensor fusion, multi-protocol communication, and enhanced motor control algorithms.
