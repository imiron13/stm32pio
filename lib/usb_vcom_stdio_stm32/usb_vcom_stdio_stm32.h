#pragma once

#include <stdio.h>
#include <hal_wrapper_stm32.h>

#ifdef __cplusplus
extern "C" {
#endif
FILE* usb_vcom_fopen(UART_HandleTypeDef *huart);

// returning EOF for file read causes no further reads possible from the file,
// but for UART as file we want to return no data, but be able to continue
// reading later once data is available
#ifndef FILE_READ_NO_MORE_DATA
#define FILE_READ_NO_MORE_DATA          (-10)
#endif

#ifndef ENDL
#define ENDL                            "\r\n"
#endif

#ifdef __cplusplus
}
#endif
