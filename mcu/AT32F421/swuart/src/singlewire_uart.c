/**
 * Single-wire Software UART implementation with pure DMA operation
 * High-performance UART for inverted and non-inverted protocols
 *
 * Features:
 * - AT32F421 GPIO enums for configuration (GPIO_MODE_*, GPIO_PULL_*)
 * - Pure DMA timer-triggered GPIO sampling for RX (zero CPU overhead)
 * - Pure DMA timer-triggered GPIO set/clear for TX (zero CPU overhead)
 * - Pin number based configuration (vs bitmask)
 * - Half-duplex operation with single DMA channel (TX/RX optimized)
 * - Direct DMA buffer filling for immediate transmission
 * - High-speed DMA sampling buffer for RX with 8x oversampling
 * - Edge-triggered RX start with 1.5 bit delay for frame synchronization
 * - 2 stop bits for better receiver alignment and reliability
 * - Inverted protocol support for FrSky Sport telemetry
 * - Race condition protection using specific interrupt disabling (not global)
 */
#include "common.h"
#include <libopencm3/cm3/common.h>

#include "singlewire_uart.h"
#include "stdbool.h"
#include "config.h"

// libopencm3 includes
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/stm32/exti.h>
#include <libopencm3/cm3/nvic.h>

#include <libopencm3/cm3/cortex.h>

// UART frame bit definitions
#define SW_UART_START_BIT (1 << 0) /*!< Start bit position */
#define SW_UART_DATA_BITS_SHIFT 1  /*!< Data bits start position */
#define SW_UART_BYTE_MASK 0xFF     /*!< Byte mask for data */
#define SW_UART_FRAME_BITS 11      /*!< Total bits: 1 start + 8 data + 2 stop */
#define SW_UART_RX_SAMPLE_BITS 9   /*!< RX sampling bits: 8 data + 1 stop bit */

// DMA sampling configuration
#define SW_UART_SAMPLES_PER_BIT 1                         /*!< One sample per bit (no oversampling) */
#define SW_UART_RX_BUFFER_SIZE SW_UART_RX_SAMPLE_BITS * 2 /*!< RX DMA buffer size for 9 bits (8 data + 1 stop) */
#define SW_UART_MIN_FRAME_SAMPLES SW_UART_RX_SAMPLE_BITS  /*!< Min samples for RX frame (9 bits) */

// TX DMA configuration
#define SW_UART_TX_MAX_FRAME_SIZE 20                                /*!< Maximum frame size in bytes */
#define SW_UART_TX_DMA_BUFFER_SIZE (SW_UART_TX_MAX_FRAME_SIZE * 11) /*!< TX DMA buffer size (1 byte = 11 bits: 1 start + 8 data + 2 stop) */

#define TX_USEC_DELAY 200
#define TX_START_DELAY ((120 * TX_USEC_DELAY) - 1) // this will calculate timer reload value
// Calculate 0.5 bit period delay to reach middle of start bit
const uint32_t bit_period = 120000000 / 57600;

// UART state enumeration
typedef enum
{
    IDLE = 0,
    SENDING,
    RECEIVING
} uart_state_t;

// Atomic EXTI bit manipulation using libopencm3 bit-banding
static inline void libopencm3_atomic_exti_bit_modify(uint32_t pin_num, uint8_t state)
{
    // Use libopencm3 bit-banding for atomic EXTI interrupt mask register access
    // pin_num is the GPIO pin number (0-15), not the EXTI line mask
    BBIO_PERIPH(EXTI_BASE, pin_num) = state;
}

// Callback function types
typedef void (*sw_uart_rx_callback_t)(void *context, uint8_t data);
typedef void (*sw_uart_tx_complete_callback_t)(void);

// Internal function prototypes (not exposed in header)
typedef struct
{
    uint32_t ccr;   // Channel configuration register (DMA_CCR)
    uint32_t cndtr; // Channel number of data register (DMA_CNDTR)
    uint32_t cpar;  // Channel peripheral address register (DMA_CPAR)
    uint32_t cmar;  // Channel memory address register (DMA_CMAR)
} dma_fast_config_t;
// Pre-configured DMA settings for RX and TX
// direct register manipulation for fast configuration and bring down interrupt overhead
// libopencm3 GPIO register addresses
#define GPIOB_IDR_ADDR (GPIOB + 0x10)  // GPIO Input Data Register
#define GPIOB_BSRR_ADDR (GPIOB + 0x18) // GPIO Bit Set/Reset Register
typedef struct
{
    uint32_t gpio_port;   // GPIO port (e.g., GPIOB)
    uint16_t gpio_pin;    // GPIO pin (e.g., GPIO6)
    uint8_t gpio_pin_num; // GPIO pin number (e.g., 6 for pin 6) - for compatibility
    uint32_t exti_line;   // EXTI line (e.g., EXTI6)
    uint8_t exti_irq;     // EXTI IRQ (e.g., NVIC_EXTI9_5_IRQ)

    uint32_t baud_rate; // Baud rate (e.g., 57600)

    uint32_t timer;                  // Timer for both TX and RX (e.g., TIM15)
    enum rcc_periph_clken timer_clk; // Timer clock enable (e.g., RCC_TIM15)

    uint32_t dma;        // DMA controller (e.g., DMA1)
    uint8_t dma_channel; // DMA channel (e.g., DMA_CHANNEL5)
    uint8_t dma_irq;     // DMA IRQ (e.g., NVIC_DMA1_CHANNEL4_5_6_7_IRQ)

    // added other stuff to be able to instantiate more than one instance
    uint32_t bit_period;
    uart_state_t uart_state;
    sw_uart_rx_callback_t rx_callback;
    void *rx_context;

    // DMA sampling variables
    uint32_t sw_uart_rx_buffer[SW_UART_RX_BUFFER_SIZE] __attribute__((aligned(4))); // Buffer for RX (9 bits: 8 data + 1 stop)
    // TX DMA variables
    uint32_t sw_uart_tx_dma_buffer[SW_UART_TX_DMA_BUFFER_SIZE] __attribute__((aligned(4)));

    uint16_t sw_uart_tx_bits_count;

    // Callbacks

    sw_uart_rx_callback_t sw_uart_rx_callback;
    void *sw_uart_rx_context;
    sw_uart_tx_complete_callback_t sw_uart_tx_complete_callback;

    dma_fast_config_t rx_config __attribute__((aligned(4)));
    dma_fast_config_t tx_config __attribute__((aligned(4)));

} sw_uart_config_t;

sw_uart_config_t uarts[1] = {
    {
    .gpio_port = GPIOB,
    .gpio_pin = GPIO6,
    .gpio_pin_num = 6,
    .exti_line = EXTI6,
    .exti_irq = NVIC_EXTI4_15_IRQ,

    .baud_rate = 57600,

    .timer = TIM16,
    .timer_clk = RCC_TIM16,

    .dma = DMA1,
    .dma_channel = DMA_CHANNEL3,
    .dma_irq = NVIC_DMA1_CHANNEL2_3_DMA2_CHANNEL1_2_IRQ,

    .bit_period = 120000000 / 57600, // Use baud_rate value directly
    .uart_state = IDLE,
    .rx_callback = NULL,
    .rx_context = NULL,

    .sw_uart_rx_buffer = {0},
    .sw_uart_tx_dma_buffer = {0},

    .sw_uart_tx_bits_count = 0,

    .sw_uart_rx_callback = NULL,
    .sw_uart_rx_context = NULL,
    .sw_uart_tx_complete_callback = NULL,
    .rx_config = {
        .ccr = DMA_CCR_PL_VERY_HIGH |              // Priority: Very High
               DMA_CCR_MSIZE_32BIT |               // Memory: 32-bit
               DMA_CCR_PSIZE_32BIT |               // Peripheral: 32-bit
               DMA_CCR_MINC |                      // Memory increment
               DMA_CCR_CIRC |                      // Circular mode enabled
               DMA_CCR_TCIE |                      // Transfer complete interrupt
               DMA_CCR_HTIE |                      // Half transfer interrupt enabled
               DMA_CCR_TEIE,                       // Transfer error interrupt
        .cndtr = SW_UART_RX_BUFFER_SIZE,           // Number of data transfers
        .cpar = GPIOB_IDR_ADDR,                    // GPIOB IDR address
        .cmar = (uint32_t)&uarts[0].sw_uart_rx_buffer // RX buffer address
    },

    .tx_config = {
        .ccr = DMA_CCR_PL_VERY_HIGH | // Priority: Very High
               DMA_CCR_MSIZE_32BIT |  // Memory: 32-bit
               DMA_CCR_PSIZE_32BIT |  // Peripheral: 32-bit
               DMA_CCR_MINC |         // Memory increment
               DMA_CCR_DIR |          // Direction: Memory to Peripheral
               DMA_CCR_TCIE |         // Transfer complete interrupt
               DMA_CCR_TEIE,          // Transfer error interrupt
        .cndtr = 0,                   // Set at runtime (TX bit count)
        .cpar = GPIOB_BSRR_ADDR,      // GPIOB BSRR address for set/reset
        .cmar = 0                     // Set at runtime (TX buffer)
    }

}
};

#define config_PB6 uarts[0]


// Public API functions
void sw_uart_init(sw_uart_config_t *config);
void sw_uart_deinit(void);
// Data transmission
void sw_uart_send_byte(uint8_t data);
// static void sw_uart_process_dma_samples(void);
static void sw_uart_decode_uart_frame(sw_uart_config_t *config, uint32_t *samples, uint16_t sample_count, uint8_t *decoded_byte);
static void sw_uart_setup_edge_detection(sw_uart_config_t *config);

static void sw_uart_start_dma_sampling(sw_uart_config_t *config);

static void sw_uart_prepare_tx_dma_buffer(sw_uart_config_t *config, uint8_t *data, uint8_t length);
static void sw_uart_start_tx_dma(sw_uart_config_t *config);
static void sw_uart_stop_dma_sampling(sw_uart_config_t *config);

// Fast DMA configuration functions
static inline void sw_uart_configure_dma_rx(sw_uart_config_t *config);
static inline void sw_uart_configure_dma_tx(sw_uart_config_t *config, uint16_t bit_count);
static inline void sw_uart_enable_rx(sw_uart_config_t *config);
static inline void sw_uart_disable_rx(sw_uart_config_t *config);
static inline void sw_uart_enable_tx(sw_uart_config_t *config);

// testing stuff
static volatile uint8_t indmahandler = 0;
// Static configuration and state
// lets initialize it to zero
// so we can test if baudrate == 0 we are not initialized and return from public functions
static sw_uart_config_t sw_uart_config = {0};

// Pin configuration

// Pre-configured DMA settings for RX and TX
// direct register manipulation for fast configuration and bring down interrupt overhead
// libopencm3 GPIO register addresses
#define GPIOB_IDR_ADDR (GPIOB + 0x10)  // GPIO Input Data Register
#define GPIOB_BSRR_ADDR (GPIOB + 0x18) // GPIO Bit Set/Reset Register

// we do double buffering for RX
// so our buffer needs to be twice the amount of samples for one byte
// we need interrupts on half and full transfer
// static const dma_fast_config_t dma_rx_config __attribute__((aligned(4))) = {
//     .ccr = DMA_CCR_PL_VERY_HIGH |       // Priority: Very High
//            DMA_CCR_MSIZE_32BIT |        // Memory: 32-bit
//            DMA_CCR_PSIZE_32BIT |        // Peripheral: 32-bit
//            DMA_CCR_MINC |               // Memory increment
//            DMA_CCR_CIRC |               // Circular mode enabled
//            DMA_CCR_TCIE |               // Transfer complete interrupt
//            DMA_CCR_HTIE |               // Half transfer interrupt enabled
//            DMA_CCR_TEIE,                // Transfer error interrupt
//     .cndtr = SW_UART_RX_BUFFER_SIZE,    // Number of data transfers
//     .cpar = GPIOB_IDR_ADDR,             // GPIOB IDR address
//     .cmar = (uint32_t)sw_uart_rx_buffer // RX buffer address
// };

// static const dma_fast_config_t dma_tx_config __attribute__((aligned(4))) = {
//     .ccr = DMA_CCR_PL_VERY_HIGH | // Priority: Very High
//            DMA_CCR_MSIZE_32BIT |  // Memory: 32-bit
//            DMA_CCR_PSIZE_32BIT |  // Peripheral: 32-bit
//            DMA_CCR_MINC |         // Memory increment
//            DMA_CCR_DIR |          // Direction: Memory to Peripheral
//            DMA_CCR_TCIE |         // Transfer complete interrupt
//            DMA_CCR_TEIE,          // Transfer error interrupt
//     .cndtr = 0,                   // Set at runtime (TX bit count)
//     .cpar = GPIOB_BSRR_ADDR,      // GPIOB BSRR address for set/reset
//     .cmar = 0                     // Set at runtime (TX buffer)
// };

// DMA register offsets (from libopencm3 dma_common_l1f013.h)
// Each DMA channel has 5 registers: CCR, CNDTR, CPAR, CMAR, reserved (0x14 = 20 bytes total)
// we cant use the defines as it uses MMMIO32 macro which dereferences the calculated address :(
#define DMA_CCR_OFFSET 0x08   // Channel Configuration Register offset from DMA base
#define DMA_CHANNEL_SIZE 0x14 // Size of each channel register block (20 bytes)

// Fast DMA configuration for RX - direct struct copy to registers
static inline void sw_uart_configure_dma_rx(sw_uart_config_t *config)
{
    uint32_t dma_base = config->dma;
    uint8_t channel = config->dma_channel;

    // Calculate CCR address: DMA_BASE + CCR_OFFSET + (CHANNEL_SIZE * (channel - 1))
    uint32_t ccr_addr = dma_base + DMA_CCR_OFFSET + (DMA_CHANNEL_SIZE * (channel - 1));

    // Disable channel first (clear EN bit)
    *(volatile uint32_t *)ccr_addr &= ~DMA_CCR_EN;

    // Fast block copy - struct is aligned to register layout
    *(volatile dma_fast_config_t *)ccr_addr = config->rx_config;
}

// Fast DMA configuration for TX - direct struct copy to registers
static inline void sw_uart_configure_dma_tx(sw_uart_config_t *config, uint16_t bit_count)
{
    // Calculate CCR address: DMA_BASE + CCR_OFFSET + (CHANNEL_SIZE * (channel - 1))
    uint32_t ccr_addr = config->dma + DMA_CCR_OFFSET + (DMA_CHANNEL_SIZE * (config->dma_channel - 1));

    // config->tx_config = dma_tx_config; // Copy template

    // Set runtime values
    config->tx_config.cndtr = bit_count;
    config->tx_config.cpar = GPIOB_BSRR_ADDR; // Use libopencm3 style address
    config->tx_config.cmar = (uint32_t)config->sw_uart_tx_dma_buffer;

    // Disable channel first (clear EN bit)
    *(volatile uint32_t *)ccr_addr &= ~DMA_CCR_EN;

    // Fast block copy - struct is aligned to register layout
    *(volatile dma_fast_config_t *)ccr_addr = config->tx_config;
}

// Initialize software UART with given configuration
void sw_uart_init(sw_uart_config_t *config)
{
    // if (!config) return;

    // Initialize state variables
    config->sw_uart_tx_bits_count = 0;
    config->uart_state = IDLE;

    sw_uart_config = *config; // Copy configuration first!

    // Enable required clocks using libopencm3
    rcc_periph_clock_enable(RCC_GPIOB);         // Enable GPIOB clock
    rcc_periph_clock_enable(RCC_DMA1);          // Enable DMA1 clock
    rcc_periph_clock_enable(config->timer_clk); // Enable timer clock

    // Configure timer for bit-rate sampling using libopencm3
    timer_set_mode(config->timer, TIM_CR1_CKD_CK_INT, TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);
    timer_enable_preload(config->timer);
    timer_continuous_mode(config->timer);

    nvic_set_priority(config->dma_irq, 4);
    nvic_enable_irq(config->dma_irq);

    libopencm3_atomic_exti_bit_modify(config->gpio_pin_num, 0); // disable the exti interrupt for now

    nvic_set_priority(config->exti_irq, 0); // high as we dont want to miss the start bit
    nvic_enable_irq(config->exti_irq);
    // Start in RX mode
    sw_uart_enable_rx(config);

    return;
}
void singlewire_uart_init()
{

    // Software UART configuration for Sport telemetry
    // PB6, 57600 baud, inverted logic, TIM15 + DMA5
    sw_uart_config_t uart_config;

    uart_config.gpio_port = SWUART_GPIO_PORT;
    uart_config.gpio_pin = SWUART_GPIO_PIN;         // libopencm3 GPIO pin constant
    uart_config.gpio_pin_num = SWUART_GPIO_PIN_NUM; // Pin number for PB6
    uart_config.baud_rate = SWUART_BAUD_RATE;

    // EXTI configuration for PB6 which is the Sport telemetry input pin
    uart_config.exti_line = SWUART_EXTI_LINE; // libopencm3 EXTI line
    uart_config.exti_irq = SWUART_EXTI_IRQ;   // libopencm3 EXTI IRQ for F421 (EXTI4-15 shared handler)

    // Timer configuration (using TIM15 for both TX and RX)
    uart_config.timer = SWUART_TIMER;         // libopencm3 timer
    uart_config.timer_clk = SWUART_TIMER_CLK; // libopencm3 timer clock

    // DMA configuration (using DMA1 Channel 5)
    uart_config.dma = SWUART_DMA;                 // libopencm3 DMA controller
    uart_config.dma_channel = SWUART_DMA_CHANNEL; // libopencm3 DMA channel
    uart_config.dma_irq = SWUART_DMA_IRQ;         // libopencm3 DMA IRQ

    sw_uart_init(&uart_config);
}

// Configure pin for reception with DMA sampling
static inline void sw_uart_enable_rx(sw_uart_config_t *config)
{
    // Configure GPIO pin for input with pull-down using libopencm3
    gpio_mode_setup(config->gpio_port, GPIO_MODE_INPUT, GPIO_PUPD_PULLDOWN, config->gpio_pin);

    // Configure EXTI for rising edge detection to trigger delayed DMA start
    sw_uart_setup_edge_detection(config);
    sw_uart_start_dma_sampling(config); // we can do this here as there will only be DMA request after the first overflow
}

// Disable reception
static inline void sw_uart_disable_rx(sw_uart_config_t *config)
{
    // Disable EXTI interrupt using libopencm3 bit-banding for thread safety
    // This prevents race conditions when called from interrupts and normal code
    libopencm3_atomic_exti_bit_modify(config->gpio_pin_num, 0);

    // Stop DMA sampling
    sw_uart_stop_dma_sampling(config);
}

// Set RX callback
void sw_uart_set_rx_callback(sw_uart_rx_callback_t callback, void *context)
{
    sw_uart_config_t *config = &config_PB6;
    config->sw_uart_rx_callback = callback;
    config->sw_uart_rx_context = context; // Store context for callback
}

// EXTI interrupt handler (call from MCU interrupt handler)
#define sw_uart_exti_handler exti4_15_isr
void exti4_15_isr(void)
{
    sw_uart_config_t *config = &config_PB6;
    if (config->baud_rate == 0)
    {
        return; // Not initialized
    }

    if (indmahandler)
    {
        // we are in the middle of a DMA handler, ignore this interrupt
        volatile int temp = 42;
    }
    // Check if line is actually high (start bit for inverted protocol) using libopencm3
    if (!gpio_get(config->gpio_port, config->gpio_pin))
    {
        // Line is low, this should not happen on rising edge - ignore interrupt
        exti_reset_request(config->exti_line);
        return;
    }

    // lets not do ADC when we are receiving, it interferes heavily with timer DMA requests
    disable_ADC();

    // Start timer using libopencm3
    timer_enable_counter(config->timer);

    // Clear EXTI flag using libopencm3
    exti_reset_request(config->exti_line);

    // Disable EXTI interrupt atomically using libopencm3 bit-banding
    libopencm3_atomic_exti_bit_modify(config->gpio_pin_num, 0);
    // just to be on the safe side, lets assume we start from 0

    config->uart_state = RECEIVING; // Set state to receiving
}

// Start DMA sampling triggered by timer
static inline void sw_uart_start_dma_sampling(sw_uart_config_t *config)
{

    // Fast DMA configuration for RX
    sw_uart_configure_dma_rx(config);
    // Enable Timer DMA request on overflow using libopencm3
    timer_enable_irq(config->timer, TIM_DIER_UDE);

    // Enable DMA channel and interrupts using libopencm3
    dma_enable_transfer_complete_interrupt(config->dma, config->dma_channel);
    dma_enable_half_transfer_interrupt(config->dma, config->dma_channel);
    dma_enable_transfer_error_interrupt(config->dma, config->dma_channel);
    nvic_enable_irq(config->dma_irq);
    nvic_set_priority(config->dma_irq, 4);
    dma_enable_channel(config->dma, config->dma_channel);
}

// Stop DMA sampling
inline static void sw_uart_stop_dma_sampling(sw_uart_config_t *config)
{
    // Stop timer using libopencm3
    timer_disable_counter(config->timer);
    timer_disable_irq(config->timer, TIM_DIER_UDE);

    // Disable DMA channel using libopencm3
    dma_disable_channel(config->dma, config->dma_channel);
}

// Decode UART frame from DMA samples (9 bits: 8 data + 1 stop)
inline static void sw_uart_decode_uart_frame(sw_uart_config_t *config, uint32_t *samples, uint16_t sample_count, uint8_t *decoded_byte)
{

    uint32_t pin_mask = (1 << config->gpio_pin_num);
    *decoded_byte = 0;
    // Samples are taken at bit centers: [bit0][bit1][bit2][bit3][bit4][bit5][bit6][bit7][stop]
    // Samples 0-7 = data bits (LSB first, inverted)
    // Sample 8 = stop bit (should be HIGH/0 for inverted protocol)

    // Verify stop bit is HIGH (0 for inverted protocol)
    uint8_t stop_bit = (samples[8] & pin_mask) ? 1 : 0;
    if (stop_bit != 0)
    {
        return; // Invalid stop bit for inverted protocol
    }

    // Decode 8 data bits (LSB first, inverted logic)
    for (uint8_t bit = 0; bit < 8; bit++)
    {
        // these instructions make complete call 563 ns
        ///  uint8_t data_bit = (samples[bit] & pin_mask) ? 0 : 1; // Inverted logic: HIGH=0, LOW=1
        //  if (data_bit)
        //  {
        //      decoded_byte |= (1 << bit); // Set bit in LSB-first order
        //  }

        // these instructions make complete call 500 ns , 437 ns for all 1's, 500 ns for all 0's
        // Use inverted logic: HIGH=0, LOW=1
        // This is more efficient than using a separate variable for data_bit
        // Directly set bit in decoded byte based on inverted logic
        if (!(samples[bit] & pin_mask))
        {
            *decoded_byte |= (1 << bit); // Set bit in LSB-first order
        }
    }
}

// DMA interrupt handler for channels 2-3 (also handles DMA2 channels 1-2 on STM32F0)
// Prototype provided by libopencm3/stm32/f0/nvic.h
void dma1_channel2_3_dma2_channel1_2_isr(void)
{
    sw_uart_dma_complete_handler();
}

inline void sw_uart_dma_complete_handler(void)
{
    sw_uart_config_t *config = &config_PB6;
    if (config->baud_rate == 0)
    {
        return; // Not initialized so we skip the whole thing
    }
    indmahandler = 1;

    // Check DMA flags using libopencm3
    if (dma_get_interrupt_flag(config->dma, config->dma_channel, DMA_TCIF) ||
        dma_get_interrupt_flag(config->dma, config->dma_channel, DMA_HTIF))
    {
        // lets stop the timer as soon as possible so we dont get any more DMA requests
        // but only if its our DMA channel that triggered this
        timer_disable_counter(config->timer);

        // Check if we're in TX or RX mode
        if (config->uart_state == SENDING)
        {
            // lets clear all flags as send only uses full transfer so half is silly.
            dma_clear_interrupt_flags(config->dma, config->dma_channel, DMA_TCIF | DMA_HTIF);
            // TX completion - inline handler
            // Stop timer and DMA
            timer_disable_counter(config->timer);
            timer_disable_irq(config->timer, TIM_DIER_UDE);
            dma_disable_channel(config->dma, config->dma_channel);

            config->sw_uart_tx_bits_count = 0;

            // Set line to idle state (LOW for inverted protocol) using libopencm3
            gpio_clear(config->gpio_port, config->gpio_pin);

            sw_uart_enable_rx(config); // Switch back to RX mode
            config->uart_state = IDLE; // Reset state to IDLE

            // Call completion callback if registered
            if (config->sw_uart_tx_complete_callback)
            {
                config->sw_uart_tx_complete_callback();
            }
            // lets do adc again
            enable_ADC();
        }
        else
        {
            bool is_full = dma_get_interrupt_flag(config->dma, config->dma_channel, DMA_TCIF);

            dma_clear_interrupt_flags(config->dma, config->dma_channel, DMA_TCIF | DMA_HTIF);
            config->uart_state = IDLE; // Reset state to IDLE

            sw_uart_setup_edge_detection(config); // Re-enable edge detection for next byte
            // we start a new reception asap, as we have double buffering we can do this
            // without losing data
            // we enable the detection here and we allow the edge interrupt to interfere with the decoding

            uint8_t data = 0;
            // RX completion
            // we always have 9 samples..
            // Decode UART frame from RX samples (9 bits: 8 data + 1 stop)
            sw_uart_decode_uart_frame(config, (uint32_t *)config->sw_uart_rx_buffer + is_full * SW_UART_MIN_FRAME_SAMPLES, SW_UART_MIN_FRAME_SAMPLES, &data);

            // Call RX callback with decoded byte
            if (config->sw_uart_rx_callback)
            {
                config->sw_uart_rx_callback(config->sw_uart_rx_context, data);
            }
            else
            {
                // No callback registered, store in FIFO
                // sw_uart_fifo_write(data);
            }
            enable_ADC(); // just for test to see what adc will do....

            // Call RX callb
        }
    }

    indmahandler = 0;
}

// Setup EXTI for rising edge detection to trigger delayed DMA start
inline static void sw_uart_setup_edge_detection(sw_uart_config_t *config)
{

    // Configure EXTI line mapping using libopencm3
    exti_select_source(config->exti_line, config->gpio_port);
    // Configure EXTI for rising edge detection using libopencm3
    exti_set_trigger(config->exti_line, EXTI_TRIGGER_RISING);

    // Disable DMA channel using libopencm3
    timer_disable_irq(config->timer, TIM_DIER_UDE);
    // lets setup the timer overhere and start it as soon as the interrupt is triggered
    // this way we can get a high baudrate
    timer_disable_counter(config->timer);
    timer_set_period(config->timer, ((bit_period * 3) / 2 - 1)); // 1.5  bit time will give first sample
    timer_generate_event(config->timer, TIM_EGR_UG);
    // after we get the first overflow the reload register will be loaded with 1 bit time, this saves interrupt overhad
    TIM_ARR(config->timer) = bit_period - 1;
    // enable DMA again using libopencm3
    timer_enable_irq(config->timer, TIM_DIER_UDE);

    // Clear any pending interrupt flags before starting using libopencm3
    exti_reset_request(config->exti_line); // Clear pending interrupt
    // Enable EXTI interrupt atomically using libopencm3 bit-banding
    libopencm3_atomic_exti_bit_modify(config->gpio_pin_num, 1);
    // Enable EXTI interrupt
    // nvic_enable_irq(config->exti_irq);
}

// Prepare TX DMA buffer with GPIO set/clear commands for UART data
static inline void sw_uart_prepare_tx_dma_buffer(sw_uart_config_t *config, uint8_t *data, uint8_t length)
{

    if (!data || !length)
        return;

    uint16_t buffer_index = 0;
    uint16_t pin_mask = (1 << config->gpio_pin_num);
    uint32_t set_command = pin_mask;       // Set bit (HIGH)
    uint32_t clr_command = pin_mask << 16; // Clear bit (LOW) - upper 16 bits of SCR

    // the following code takes ~16 usec,
    // Process each byte
    for (uint8_t byte_idx = 0; byte_idx < length && buffer_index < SW_UART_TX_DMA_BUFFER_SIZE - 11; byte_idx++)
    {
        uint8_t byte_data = data[byte_idx];

        // Prepare frame for inverted protocol (SPORT):
        // Start bit = HIGH, Data inverted, Stop bits = LOW

        // 1. Start bit (HIGH for inverted protocol)
        config->sw_uart_tx_dma_buffer[buffer_index++] = set_command; // start command
        // quicker than a loop
        config->sw_uart_tx_dma_buffer[buffer_index++] = (byte_data & (1 << 0)) ? clr_command : set_command; // Bit 0
        config->sw_uart_tx_dma_buffer[buffer_index++] = (byte_data & (1 << 1)) ? clr_command : set_command; // Bit 1
        config->sw_uart_tx_dma_buffer[buffer_index++] = (byte_data & (1 << 2)) ? clr_command : set_command; // Bit 2
        config->sw_uart_tx_dma_buffer[buffer_index++] = (byte_data & (1 << 3)) ? clr_command : set_command; // Bit 3
        config->sw_uart_tx_dma_buffer[buffer_index++] = (byte_data & (1 << 4)) ? clr_command : set_command; // Bit 4
        config->sw_uart_tx_dma_buffer[buffer_index++] = (byte_data & (1 << 5)) ? clr_command : set_command; // Bit 5
        config->sw_uart_tx_dma_buffer[buffer_index++] = (byte_data & (1 << 6)) ? clr_command : set_command; // Bit 6
        config->sw_uart_tx_dma_buffer[buffer_index++] = (byte_data & (1 << 7)) ? clr_command : set_command; // Bit 7
        config->sw_uart_tx_dma_buffer[buffer_index++] = clr_command;                                        // First stop bit
        config->sw_uart_tx_dma_buffer[buffer_index++] = clr_command;                                        // Second stop bit
    }
    config->sw_uart_tx_bits_count = buffer_index;
}

// Start TX DMA transmission
inline static void sw_uart_start_tx_dma(sw_uart_config_t *config)
{

    // we need to disable DMA as configuring timer can lead to a DMA request we dont want
    dma_disable_channel(config->dma, config->dma_channel);

    // Fast DMA configuration for TX
    sw_uart_configure_dma_tx(config, config->sw_uart_tx_bits_count);

    // as we are starting a transmission we assume we switched from RX to TX
    // to give the receive the time to switch from TX to RX we add a delay of 400 usec
    // this is what frsky does in their implementation

    timer_disable_counter(config->timer);
    // we will wait a while to give receiver time to switch from TX to RX
    // after that we need 1 bit period, therefore dis/enable preload

    timer_enable_preload(config->timer);
    timer_set_period(config->timer, TX_START_DELAY); // wait a while
    timer_generate_event(config->timer, TIM_EGR_UG);

    // timer_clear_flag(config->timer, TIM_SR_UIF);     // Clear any pending update flags
    //  Set the auto-reload register for subsequent bit periods
    TIM_ARR(config->timer) = config->bit_period - 1;
    timer_set_counter(config->timer, 0);

    // Enable Timer DMA request on overflow using libopencm3
    timer_enable_irq(config->timer, TIM_DIER_UDE);
    dma_clear_interrupt_flags(config->dma, config->dma_channel, DMA_TCIF | DMA_HTIF | DMA_TEIF); // lets clear the flag, just to be very certain
    // Enable DMA channel and interrupts using libopencm3
    dma_enable_transfer_complete_interrupt(config->dma, config->dma_channel);
    dma_enable_transfer_error_interrupt(config->dma, config->dma_channel);
    nvic_enable_irq(config->dma_irq);
    nvic_set_priority(config->dma_irq, 4);
    dma_enable_channel(config->dma, config->dma_channel);

    // Start timer using libopencm3
    timer_enable_counter(config->timer);
}

// Configure pin for transmission
void sw_uart_enable_tx(sw_uart_config_t *config)
{
    // Configure GPIO pin for output using libopencm3
    gpio_mode_setup(config->gpio_port, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, config->gpio_pin);
    gpio_set_output_options(config->gpio_port, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, config->gpio_pin);

    // Inverted logic: set initial idle state to LOW using libopencm3
    gpio_clear(config->gpio_port, config->gpio_pin);
}

// Send a single byte
void sw_uart_send_byte(uint8_t data)
{
    singlewire_uart_send_frame(&data, 1);
}

// Send a frame of bytes
uint8_t singlewire_uart_send_frame(uint8_t *buffer, uint8_t length)
{
    sw_uart_config_t *config = &config_PB6;
    if (!buffer || length == 0 || length > SW_UART_TX_MAX_FRAME_SIZE)
        return 0;

    // Atomic check and set of state
    cm_disable_interrupts();
    if (config->uart_state != IDLE)
    {
        cm_enable_interrupts();
        return 0; // Busy - cannot send
    }
    config->uart_state = SENDING; // Set state to sending
    // ensure ADC DMA won't hog the bus during the critical TX window
    // ADC DMA channel on this project is DMA1_CHANNEL1 — adjust if different
    disable_ADC();
    // Stop DMA sampling during transmission
    sw_uart_disable_rx(&config_PB6);

    // Configure pin for transmission
    sw_uart_enable_tx(&config_PB6);
    cm_enable_interrupts();
    // Prepare DMA buffer with frame data
    sw_uart_prepare_tx_dma_buffer(&config_PB6, buffer, length);

    // Start DMA transmission
    sw_uart_start_tx_dma(&config_PB6);
    return 1;
}
