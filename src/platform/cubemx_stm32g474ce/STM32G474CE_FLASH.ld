/* Linker script for STM32G474CEU6 */

/* Entry Point */
ENTRY(Reset_Handler)

/* Highest address of the user mode stack */
_estack = 0x20020000;    /* end of RAM (128KB from 0x20000000) */

/* Generate a link error if heap and stack don't fit into RAM */
_Min_Heap_Size = 0x200;      /* required amount of heap  */
_Min_Stack_Size = 0x400;     /* required amount of stack */

/* Specify the memory areas */
MEMORY
{
  /* Flash: 512KB at 0x08000000 */
  FLASH (rx)      : ORIGIN = 0x08000000, LENGTH = 512K
  
  /* SRAM1: 80KB (Main RAM for stack, heap, video) */
  RAM   (xrw)     : ORIGIN = 0x20000000, LENGTH = 80K
  
  /* SRAM2: 16KB (Dedicated for Audio DMA to avoid contention) */
  RAM2  (xrw)     : ORIGIN = 0x20014000, LENGTH = 16K

  /* CCRAM: 32KB (Dedicated for critical code) */
  CCMRAM  (xrw)     : ORIGIN = 0x1000000, LENGTH = 32K
}

/* Define output sections */
SECTIONS
{
  /* The startup code goes first into FLASH */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector)) /* Startup code */
    . = ALIGN(4);
  } >FLASH

  /* The program code and other data goes into FLASH */
  .text :
  {
    . = ALIGN(4);
    *(.text)           /* .text sections (code) */
    *(.text*)          /* .text* sections (code) */
    *(.glue_7)         /* glue arm to thumb code */
    *(.glue_7t)        /* glue thumb to arm code */
    *(.eh_frame)

    KEEP (*(.init))
    KEEP (*(.fini))

    . = ALIGN(4);
    _etext = .;        /* define a global symbols at end of code */
  } >FLASH

  /* Constant data goes into FLASH */
  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)         /* .rodata sections (constants, strings, etc.) */
    *(.rodata*)        /* .rodata* sections (constants, strings, etc.) */
    . = ALIGN(4);
  } >FLASH

  /* Initialized data sections goes into RAM, load LMA copy after code */
  _sidata = LOADADDR(.data);

/* --- START OF ADDED SECTIONS --- */
  
  /* C++ Exception handling / Unwind info (Required for some STL/math functions) */
  .ARM.extab   : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >FLASH
  .ARM : {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } >FLASH

  /* Pre-initialization array */
  .preinit_array     :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } >FLASH

  /* Initialization array (C++ Constructors) - CRITICAL FOR FIXING CRASH */
  .init_array :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
  } >FLASH

  /* Finalization array (Destructors) */
  .fini_array :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } >FLASH

  /* --- END OF ADDED SECTIONS --- */

  .data :
  {
    . = ALIGN(4);
    _sdata = .;        /* create a global symbol at data start */
    *(.data)           /* .data sections */
    *(.data*)          /* .data* sections */
    . = ALIGN(4);
    _edata = .;        /* define a global symbol at data end */
  } >RAM AT> FLASH

  /* Critical Code/Data in CCM RAM */
  _siccmram = LOADADDR(.ccmram); /* Load address (in Flash) */
  
  .ccmram :
  {
    . = ALIGN(4);
    _sccmram = .;       /* Start address (in CCMRAM) */
    
    *(.ccmram)
    *(.ccmram*)
    
    . = ALIGN(4);
    _eccmram = .;       /* End address (in CCMRAM) */
  } >CCMRAM AT> FLASH

  /* Uninitialized data section */
  .bss :
  {
    . = ALIGN(4);
    _sbss = .;         /* define a global symbol at bss start */
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;         /* define a global symbol at bss end */
    __bss_end__ = _ebss;
  } >RAM

/* Custom section to force Audio Buffer into SRAM2 */
  .sram2_data :
  {
    . = ALIGN(4);
    *(.sram2_data)
    *(.sram2_data*)
    . = ALIGN(4);
  } >RAM2

  /* User_heap_stack section, used to check that there is enough RAM left */
  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM

  /* Remove information from the standard libraries */
  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
  }
}
