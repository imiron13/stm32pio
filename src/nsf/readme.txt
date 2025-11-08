arm-none-eabi-objcopy -I binary -O elf32-littlearm -B arm --rename-section .data=.rodata,alloc,load,readonly,data,contents goal3.nsf goal3_nsf.o
arm-none-eabi-readelf -S goal3_nsf.o
