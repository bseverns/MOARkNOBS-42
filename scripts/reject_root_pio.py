Import("env")

print(
    "\nERROR: PlatformIO must be run from ./firmware.\n"
    "Use:\n"
    "  pio run -d firmware -e teensy40_main\n"
    "  pio test -d firmware -e teensy40_unity -vvv\n"
)
env.Exit(1)
