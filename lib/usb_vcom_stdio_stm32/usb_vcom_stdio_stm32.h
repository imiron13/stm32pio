#pragma once

#include <stdio.h>
#include <hal_wrapper_stm32.h>
#include <intrinsics.h>

#ifdef __cplusplus
extern "C" {
#endif
FILE* usb_vcom_fopen();

#ifdef __cplusplus
}
#endif
