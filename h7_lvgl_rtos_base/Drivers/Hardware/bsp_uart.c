#include "bsp_uart.h"
#include "bsp_GPS.h"
#include <stdio.h>
#include <stdarg.h>

//extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;

uint16_t uart1_delay_count = 0;
uint16_t uart2_delay_count = 0;

uint8_t uart1_rx_mode_temp;
uint8_t uart2_rx_mode_temp;
uint8_t uart1_rx_data[UART_RX_BUFFER_SIZE];
uint8_t uart2_rx_data[UART_RX_BUFFER_SIZE];
//static uint8_t uart1_print_buf[UART_TX_BUFFER_SIZE];
static uint8_t uart2_print_buf[UART_TX_BUFFER_SIZE];

void uart_init(UART_HandleTypeDef *huart, uint8_t uart_rx_mode)
{
//	if (huart == &huart1)
//	{
//		uart1_rx_mode_temp = uart_rx_mode;

//		if (uart_rx_mode == UART_DMA_RX)
//		{
//			HAL_UART_Receive_DMA(&huart1, uart1_rx_data, UART_RX_BUFFER_SIZE);
//		}
//		else if (uart_rx_mode == UART_DMA_ToIdle_RX)
//		{
//			HAL_UARTEx_ReceiveToIdle_DMA(&huart1, uart1_rx_data, UART_RX_BUFFER_SIZE);
//		}
//		else if (uart_rx_mode == UART_IT_RX)
//		{
//			HAL_UART_Receive_IT(&huart1, uart1_rx_data, UART_RX_BUFFER_SIZE);
//		}
//		else if (uart_rx_mode == UART_IT_ToIdle_RX)
//		{
//			HAL_UARTEx_ReceiveToIdle_IT(&huart1, uart1_rx_data, UART_RX_BUFFER_SIZE);
//		}
//		else if (uart_rx_mode == UART_Block_RX)
//		{
//			HAL_UART_Receive(&huart1, uart1_rx_data, UART_RX_BUFFER_SIZE, BLOCK_WAITING_TIME);
//		}
//	}
	if (huart == &huart2)
	{
		uart2_rx_mode_temp = uart_rx_mode;

		if (uart_rx_mode == UART_DMA_RX)
		{
			HAL_UART_Receive_DMA(&huart2, uart2_rx_data, UART_RX_BUFFER_SIZE);
		}
		else if (uart_rx_mode == UART_DMA_ToIdle_RX)
		{
			HAL_UARTEx_ReceiveToIdle_DMA(&huart2, uart2_rx_data, UART_RX_BUFFER_SIZE);
		}
		else if (uart_rx_mode == UART_IT_RX)
		{
			HAL_UART_Receive_IT(&huart2, uart2_rx_data, UART_RX_BUFFER_SIZE);
		}
		else if (uart_rx_mode == UART_IT_ToIdle_RX)
		{
			HAL_UARTEx_ReceiveToIdle_IT(&huart2, uart2_rx_data, UART_RX_BUFFER_SIZE);
		}
		else if (uart_rx_mode == UART_Block_RX)
		{
			HAL_UART_Receive(&huart2, uart2_rx_data, UART_RX_BUFFER_SIZE, BLOCK_WAITING_TIME);
		}
	}
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
//	if (huart == &huart1)
//	{
//		if (uart1_rx_mode_temp == UART_IT_RX)
//		{
//			HAL_UART_Receive_IT(&huart1, uart1_rx_data, UART_RX_BUFFER_SIZE);
//		}
//		else if (uart1_rx_mode_temp == UART_DMA_RX)
//		{
//			HAL_UART_Receive_DMA(&huart1, uart1_rx_data, UART_RX_BUFFER_SIZE);
//		}

//		BT_ProcessRxData(uart1_rx_data, UART_RX_BUFFER_SIZE);

//		if (uart1_rx_data[0] == '1')
//		{
//			my_uart_printf(&huart1, UART_DMA_TX, "%d\r\n", 1);
//			uart1_rx_data[0] = 0;
//		}
//	}
	if (huart == &huart2)
	{
		if (uart2_rx_mode_temp == UART_IT_RX)
		{
			HAL_UART_Receive_IT(&huart2, uart2_rx_data, UART_RX_BUFFER_SIZE);
		}
		else if (uart2_rx_mode_temp == UART_DMA_RX)
		{
			HAL_UART_Receive_DMA(&huart2, uart2_rx_data, UART_RX_BUFFER_SIZE);
		}

		GPS_RxPro_HAL(uart2_rx_data, UART_RX_BUFFER_SIZE);
	}
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
//	if (huart == &huart1)
//	{
//		if (uart1_rx_mode_temp == UART_DMA_ToIdle_RX)
//		{
//			SCB_InvalidateDCache_by_Addr((uint32_t *)uart1_rx_data, UART_RX_BUFFER_SIZE);
//		}

//		BT_ProcessRxData(uart1_rx_data, Size);

//		if (uart1_rx_mode_temp == UART_IT_ToIdle_RX)
//		{
//			HAL_UARTEx_ReceiveToIdle_IT(&huart1, uart1_rx_data, UART_RX_BUFFER_SIZE);
//		}
//		else if (uart1_rx_mode_temp == UART_DMA_ToIdle_RX)
//		{
//			HAL_UARTEx_ReceiveToIdle_DMA(&huart1, uart1_rx_data, UART_RX_BUFFER_SIZE);
//		}

//		if (uart1_rx_data[0] == '1')
//		{
//			my_uart_printf(&huart1, UART_DMA_TX, "%d\r\n", 1);
//			uart1_rx_data[0] = 0;
//		}
//	}
	if (huart == &huart2)
	{
		// Clear DCache before parsing DMA RX data.
		SCB_InvalidateDCache_by_Addr((uint32_t *)uart2_rx_data, UART_RX_BUFFER_SIZE);
		GPS_RxPro_HAL(uart2_rx_data, Size);

		if (uart2_rx_mode_temp == UART_IT_ToIdle_RX)
		{
			HAL_UARTEx_ReceiveToIdle_IT(&huart2, uart2_rx_data, UART_RX_BUFFER_SIZE);
		}
		else if (uart2_rx_mode_temp == UART_DMA_ToIdle_RX)
		{
			HAL_UARTEx_ReceiveToIdle_DMA(&huart2, uart2_rx_data, UART_RX_BUFFER_SIZE);
		}
	}
}

int my_uart_printf(UART_HandleTypeDef *huart, uint8_t send_mode, const char *format, ...)
{
	va_list args;
	int16_t len;
	uint8_t *print_buf = NULL;

//	if (huart == &huart1)
//	{
//		print_buf = uart1_print_buf;
//	}
	if (huart == &huart2)
	{
		print_buf = uart2_print_buf;
	}

	if (print_buf == NULL || format == NULL)
	{
		return -1;
	}

	va_start(args, format);
	len = vsnprintf((char *)print_buf, UART_TX_BUFFER_SIZE, format, args);
	va_end(args);

	if (len < 0 || len >= UART_TX_BUFFER_SIZE)
	{
		return -1;
	}

	if (send_mode == UART_DMA_TX)
	{
		HAL_UART_Transmit_DMA(huart, print_buf, len);
	}
	else if (send_mode == UART_IT_TX)
	{
		HAL_UART_Transmit_IT(huart, print_buf, len);
	}
	else if (send_mode == UART_Block_TX)
	{
		HAL_UART_Transmit(huart, print_buf, len, BLOCK_WAITING_TIME);
	}
	else
	{
		return -1;
	}

	return len;
}
