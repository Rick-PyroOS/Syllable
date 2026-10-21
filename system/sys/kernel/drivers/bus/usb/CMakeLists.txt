# USB bus driver
syllable_driver(usb_bus OUTPUT_NAME usb SOURCES usb_hub.c usb.c libusb.c
  DEFINES __ENABLE_DEBUG__ INSTALL system/drivers/dev/bus)
