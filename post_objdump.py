Import("env")
from os.path import join



# Define the function you want to run
def run_objdump(source, target, env):
    # Path to ELF file
    elf = env.subst("$BUILD_DIR/${PROGNAME}.elf")

    # Path to toolchain objdump
    toolchain_dir = env.PioPlatform().get_package_dir("toolchain-gccarmnoneeabi")
    objdump = join(toolchain_dir, "bin", "arm-none-eabi-objdump")

    # Output file (place next to ELF)
    out = join(env.subst("$BUILD_DIR"), "output.s")

    cmd = f"{objdump} -d -C -S --disassemble-zeroes {elf} > {out}"

    print("Running objdump:", cmd)
    env.Execute(cmd)

# Attach this function to the 'buildprog' (the elf file)
env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", run_objdump)


