#include <stdio.h>
#include <uart_stdio_stm32.h>

ssize_t uart_write(void *huart, const char *ptr, size_t len)
{
    HAL_StatusTypeDef status = HAL_UART_Transmit((UART_HandleTypeDef*)huart, (uint8_t*)ptr, len, HAL_MAX_DELAY);
    return status == HAL_OK ? len : -1;
}


ssize_t uart_read(void *huart, char* buff, size_t len)
{
    size_t bytes_read = 0;
    while (bytes_read < len && __HAL_UART_GET_FLAG((UART_HandleTypeDef*)huart, UART_FLAG_RXNE))
    {
        HAL_UART_Receive((UART_HandleTypeDef*)huart, (uint8_t*)buff, 1, HAL_MAX_DELAY);
        bytes_read++;
    }
    return bytes_read == 0 ? FILE_READ_NO_MORE_DATA : bytes_read;
}

FILE* uart_fopen(UART_HandleTypeDef *huart)
{
    cookie_io_functions_t cookie_funcs = {
        .read = uart_read,
        .write = uart_write,
        .seek = 0,
        .close = 0
    };
    FILE *f = fopencookie(huart, "a+", cookie_funcs);
    setbuf(f, NULL);
    setvbuf(f, NULL, _IONBF, 0);
    return f;
}
