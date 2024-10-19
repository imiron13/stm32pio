#pragma once

#include <stdio.h>
#include <hal_wrapper_stm32.h>

#ifdef __cplusplus
extern "C" {
#endif
FILE* uart_fopen(UART_HandleTypeDef *huart);

// returning EOF for file read causes no further reads possible from the file,
// but for UART as file we want to return no data, but be able to continue
// reading later once data is available
#define FILE_READ_NO_MORE_DATA          (-10)

#define ENDL                            "\r\n"

ssize_t uart_read(void *huart, char* buff, size_t len);

#ifdef __cplusplus
}
#endif
