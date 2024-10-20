#include <stdio.h>
#include <usb_vcom_stdio_stm32.h>
#include <usbd_def.h>
#include <usbd_cdc_if.h>
#include <usbd_cdc_if_template.h>

ssize_t usb_vcom_write(void *husb, const char *ptr, size_t len)
{
    int status;
    do {
        status = CDC_Transmit_FS((uint8_t*)ptr, len);
    } while (status == USBD_BUSY);
    return status == USBD_OK ? len : -1;
}


ssize_t usb_vcom_read(void *husb, char* buff, size_t len)
{
    size_t bytes_read = usb_vcom_pop_data((uint8_t*)buff, len);
    /*while (bytes_read < len && __HAL_UART_GET_FLAG((UART_HandleTypeDef*)huart, UART_FLAG_RXNE))
    {
        HAL_UART_Receive((UART_HandleTypeDef*)huart, (uint8_t*)buff, 1, HAL_MAX_DELAY);
        bytes_read++;
    }*/
    return bytes_read == 0 ? FILE_READ_NO_MORE_DATA : bytes_read;
}

FILE* usb_vcom_fopen(UART_HandleTypeDef *husb)
{
    cookie_io_functions_t cookie_funcs = {
        .read = usb_vcom_read,
        .write = usb_vcom_write,
        .seek = 0,
        .close = 0
    };
    FILE *f = fopencookie(husb, "a+", cookie_funcs);
    setbuf(f, NULL);
    setvbuf(f, NULL, _IONBF, 0);
    return f;
}
