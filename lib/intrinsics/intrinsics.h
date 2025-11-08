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
