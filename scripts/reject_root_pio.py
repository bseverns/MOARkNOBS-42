Import("env")

print(
    "\nERROR: PlatformIO must be run from ./firmware.\n"
    "Use:\n"
    "  pio -d firmware run -e teensy40_main\n"
    "  pio -d firmware test -e teensy40_unity -vvv\n"
)
env.Exit(1)
