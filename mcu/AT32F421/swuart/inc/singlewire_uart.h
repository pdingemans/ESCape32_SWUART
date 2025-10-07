/**
 * Single-wire UART implementation
 *
 */

#ifndef SINGLEWIRE_UART_H
#define SINGLEWIRE_UART_H


#include <stdint.h>
#include <stddef.h>


// Interrupt handlers (to be called from MCU interrupt handlers)
// discussion neede wether interrupt handling shouldn't be done in the _it file but
// in the uart itself. for now lets keep it like this, as all interrupt handling is
// in the _it file.
void sw_uart_exti_handler(void);
void sw_uart_dma_complete_handler(void);

// the following functions are necessay for every different MCU
void singlewire_uart_init(void);
/*
* following function sends a frame with a delay of 200 usec.
*/
uint8_t singlewire_uart_send_frame(uint8_t* buffer, uint8_t length);

typedef void (*sw_uart_rx_callback_t)(void *context, uint8_t data);

// Callback registration
void sw_uart_set_rx_callback(sw_uart_rx_callback_t callback, void *context);
#endif // SINGLEWIRE_UART_H
