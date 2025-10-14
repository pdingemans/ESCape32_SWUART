/*
** Copyright (C) Arseny Vakhrushev <arseny.vakhrushev@me.com>
**
** This firmware is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** This firmware is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this firmware. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#define CLK 120000000
#ifndef IO_PB4
#define IO_PA2
#endif

#define IFTIM TIM3
#define IFTIM_XRES 0
#define IFTIM_ICFL 128
#define IFTIM_ICMR TIM3_CCMR1
#define IFTIM_ICM1 (TIM_CCMR1_CC1S_IN_TI1 | TIM_CCMR1_IC1F_DTF_DIV_16_N_8)
#define IFTIM_ICM2 (TIM_CCMR1_CC1S_IN_TI1 | TIM_CCMR1_IC1F_DTF_DIV_8_N_8)
#define IFTIM_ICM3 (TIM_CCMR1_CC1S_IN_TI1 | TIM_CCMR1_IC1F_DTF_DIV_4_N_8)
#define IFTIM_ICIE TIM_DIER_CC1IE
#define IFTIM_ICR TIM3_CCR1
#define IFTIM_OCR TIM3_CCR3
#define iftim_isr tim3_isr


#define IOTIM TIM15

#define iotim_isr tim15_isr
#define IOTIM_DMA 5
#ifdef IO_PA2

#define iodma_isr dma1_channel4_7_dma2_channel3_5_isr

#define IOTIM_IDR (GPIOA_IDR & 0x4) // A2
#elif defined(IO_PA6)
#define IOTIM_IDR (GPIOA_IDR & 0x40) // A6
#else
#define IOTIM_IDR (GPIOB_IDR & 1<<4) // B4
#endif



#define USART1_RX_DMA 3
#define USART1_TX_DMA 2
#define usart1_tx_dma_isr dma1_channel2_3_dma2_channel1_2_isr



// sw uart stuff has been moved to swuart itself.
// tim 16 is used for sport telemetry
// tim15 is used for swuart on PB4 and pwm input on PB4. we only support pwm and sbus input on PB4 

#define USART2_RX_DMA 5
#define USART2_TX_DMA 4

#define tim1_com_isr tim1_brk_up_trg_com_isr
