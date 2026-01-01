#include <stdio.h>
#include <usb_vcom_stdio_stm32.h>
#include <usbd_def.h>
#include <usbd_cdc.h>
#include <usbd_cdc_if.h>

extern volatile uint8_t g_USB_Port_Open;

ssize_t usb_vcom_write(void *husb, const char *ptr, size_t len)
{
    UNUSED(husb);

    // 1. Check if the terminal is actually open (DTR High)
    if (g_USB_Port_Open == 0)
    {
        // Drop the data. Return OK so the rest of the application continues.
        return USBD_OK; 
    }

    int status;
    do {
        status = CDC_Transmit_FS((uint8_t*)ptr, len);
    } while (status == USBD_BUSY);
    return status == USBD_OK ? len : -1;
}

int g_vcom_read_cnt = 0;
ssize_t usb_vcom_read(void *husb, char* buff, size_t len)
{
    UNUSED(husb);
    size_t bytes_read = usb_vcom_pop_data((uint8_t*)buff, len);
    g_vcom_read_cnt++;
    return bytes_read == 0 ? FILE_READ_NO_MORE_DATA : bytes_read;
}

FILE* usb_vcom_fopen()
{
    cookie_io_functions_t cookie_funcs = {
        .read = usb_vcom_read,
        .write = usb_vcom_write,
        .seek = 0,
        .close = 0
    };
    FILE *f = fopencookie(NULL, "a+", cookie_funcs);
    setbuf(f, NULL);
    setvbuf(f, NULL, _IONBF, 0);
    return f;
}
