# USB mass storage
syllable_driver(usb_disk OUTPUT_NAME usb SOURCES scsi_convert.c transport.c protocol.c jumpshot.c usb_disk.c
  INSTALL system/drivers/dev/disk)
