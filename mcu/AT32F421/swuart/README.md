# Software UART Library for ESCape32

This library provides high-performance DMA-based software UART functionality for ESCape32 motor controllers using libopencm3.

## Features

- **DMA-based**: Efficient data transfer without CPU intervention
- **High Performance**: Optimized for real-time motor control applications
- **Multi-MCU Support**: Compatible with AT32F421, STM32F051, STM32G071, etc.
- **libopencm3 Integration**: Uses libopencm3 hardware abstraction layer
- **Compile-time Optimization**: Pre-configured structures for optimal performance

## Directory Structure

```
mcu/
├── AT32F421/
│   └── swuart/
│       ├── inc/
│       │   └── singlewire_sw_uart.h
│       ├── src/
│       │   └── [implementation files]
│       └── CMakeLists.txt
├── STM32F051/
│   └── swuart/
│       └── [MCU-specific implementation]
└── [other MCU families]
```

## Usage in CMakeLists.txt

### Method 1: Using the USE_SWUART flag

Add `USE_SWUART` to any target definition:

```cmake
add_target(MYESC AT32F421 DEAD_TIME=66 COMP_MAP=123 USE_SWUART)
```

### Method 2: Manual integration

For more control, manually add swuart to specific targets:

```cmake
# Include the swuart library
include(swuart.cmake)

# Add swuart to a specific target
add_swuart_to_target(my_target_elf AT32F421)
```

### Method 3: Direct library usage

Use the MCU-specific CMakeLists.txt:

```cmake
# Add the swuart subdirectory
add_subdirectory(mcu/AT32F421/swuart)

# Link against the swuart library
target_link_libraries(my_target swuart)
```

## Configuration

The library can be configured through compile-time definitions:

```cmake
target_compile_definitions(my_target PRIVATE
    SWUART_ENABLED=1
    SWUART_BAUD_RATE=57600
    SWUART_BUFFER_SIZE=16
)
```

## Hardware Requirements

- **Timer**: One timer peripheral for bit timing
- **DMA**: Two DMA channels (TX and RX)
- **GPIO**: One GPIO pin for single-wire communication
- **Memory**: Minimal RAM footprint (~32 bytes)

## MCU-Specific Implementation

Each MCU family has its own swuart directory with:
- Hardware-specific configurations
- Timer and DMA mappings
- GPIO pin assignments
- Clock source definitions

## Example Code

```c
#ifdef SWUART_ENABLED
#include "singlewire_sw_uart.h"

void init_swuart(void) {
    // Initialize software UART
    swuart_init();
}

void send_data(uint8_t *data, uint16_t length) {
    // Send data via software UART
    swuart_transmit(data, length);
}
#endif
```

## Performance Characteristics

- **Baud Rate**: Up to 57600 bps
- **CPU Overhead**: Minimal (DMA-based)
- **Latency**: < 10μs for small packets
- **Reliability**: Hardware-validated timing

## Adding New MCU Support

1. Create directory: `mcu/[MCU_FAMILY]/swuart/`
2. Add header file: `inc/singlewire_sw_uart.h`
3. Implement MCU-specific code in `src/`
4. Create `CMakeLists.txt` with MCU-specific settings
5. Update timer/DMA mappings in config files

## Troubleshooting

- **Build Errors**: Ensure libopencm3 is properly configured
- **Runtime Issues**: Check timer and DMA channel conflicts
- **Performance**: Verify clock configuration and optimization level

## License

This library follows the same license as the ESCape32 project (GPL v3).
