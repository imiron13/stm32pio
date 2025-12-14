#pragma once

#include <stddef.h>
#include <stdint.h>

// returning EOF for file read causes no further reads possible from the file,
// but for UART as file we want to return no data, but be able to continue
// reading later once data is available
#ifndef FILE_READ_NO_MORE_DATA
#define FILE_READ_NO_MORE_DATA          (-10)
#endif

#ifndef ENDL
#define ENDL                            "\r\n"
#endif

#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)
#define force_inline   __attribute__((always_inline)) inline
#define no_inline      __attribute__((noinline))
#define no_return      __attribute__((noreturn))
//#define unused         __attribute__((unused))
#define optimize_size __attribute__((optimize("Os")))
#define optimize_speed __attribute__((optimize("-O3")))
#define attr_section(x)    __attribute__((section(x)))
#define fast_code_section  __attribute__((section(".ramfunc")))
#define sram2_section __attribute__((section(".sram2_data"), aligned(4)))
