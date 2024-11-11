#pragma once

#include <stdio.h>
#include <hal_wrapper_stm32.h>
#include <intrinsics.h>

#ifdef __cplusplus
extern "C" {
#endif
FILE* uart_fopen(UART_HandleTypeDef *huart);

ssize_t uart_read(void *huart, char* buff, size_t len);

#ifdef __cplusplus
}
#endif
