/**
 * Single-wire UART implementation
 *
 */

#ifndef SINGLEWIRE_UART_H
#define SINGLEWIRE_UART_H


#include <stdint.h>
#include <stddef.h>




// the following functions are necessay for every different MCU
void singlewire_uart_init(uint8_t uart_id);
/*
* following function sends a frame with a delay of 200 usec.
*/
uint8_t singlewire_uart_send_frame(uint8_t uart_id, uint8_t* buffer, uint8_t length);

typedef void (*sw_uart_rx_callback_t)(void *context, uint8_t data);
typedef void (*sw_uart_tx_complete_callback_t)(void *context);

// Callback registration
void sw_uart_set_rx_callback(uint8_t uart_id, sw_uart_rx_callback_t callback, void *context);
void sw_uart_set_tx_callback(uint8_t uart_id, sw_uart_tx_complete_callback_t callback, void *context);
// interrupt handlers (to be called from mcu interrupt handlers)
void sw_uart_exti_handler(void);
#endif // SINGLEWIRE_UART_H
