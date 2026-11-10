# QEMU standard VGA / bochs-display / QXL (VGA mode) kernel driver (for the
# appserver's bochs video driver)
syllable_driver(gfx_bochs OUTPUT_NAME bochs SOURCES driver.c INSTALL system/drivers/dev/graphics)
