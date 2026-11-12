# PlatformIO post-build check: fails the build when the RAM left for the heap (between the end of
# static data and the stack) drops below HEAP_RESERVE; the reserve's measurement is in
# docs/development.md.
import subprocess

Import("env")

HEAP_RESERVE = 4096


def check(target, source, env):
    nm = env.subst("$CC").replace("gcc", "nm")
    symbols = {}
    for line in subprocess.run([nm, str(target[0])], capture_output=True, text=True, check=True).stdout.splitlines():
        parts = line.split()
        if len(parts) == 3:
            symbols[parts[2]] = int(parts[0], 16)
    free = symbols["__StackLimit"] - symbols["__bss_end__"]
    print("Heap: %d bytes free for a reserve of %d" % (free, HEAP_RESERVE))
    if free < HEAP_RESERVE:
        print("Error: too little RAM left for the heap, reduce static data")
        env.Exit(1)


env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", check)
