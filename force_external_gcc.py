Import("env")

TOOLCHAIN = "/usr/bin"

# Make sure system GCC is found first
env.PrependENVPath("PATH", TOOLCHAIN)

# Override ALL compiler, assembler, and linker commands
env.Replace(
    CC      = f"{TOOLCHAIN}/arm-none-eabi-gcc",
    CXX     = f"{TOOLCHAIN}/arm-none-eabi-g++",
    AR      = f"{TOOLCHAIN}/arm-none-eabi-ar",
    AS      = f"{TOOLCHAIN}/arm-none-eabi-gcc",
    LD      = f"{TOOLCHAIN}/arm-none-eabi-ld",
    LINK    = f"{TOOLCHAIN}/arm-none-eabi-g++",
    RANLIB  = f"{TOOLCHAIN}/arm-none-eabi-ranlib",
    OBJCOPY = f"{TOOLCHAIN}/arm-none-eabi-objcopy",
    OBJDUMP = f"{TOOLCHAIN}/arm-none-eabi-objdump",
    SIZE    = f"{TOOLCHAIN}/arm-none-eabi-size",
    GDB     = f"{TOOLCHAIN}/arm-none-eabi-gdb",
)

#env.Append(ASFLAGS=[
    #"-mcpu=cortex-m4",
    #"-mthumb",
    #"-mfpu=fpv4-sp-d16",
    #"-mfloat-abi=softfp"
#])
