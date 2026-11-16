# TUN virtual network device
syllable_driver(tun SOURCES tun_if.c tun_dev.c DEFINES __ENABLE_DEBUG__ INSTALL system/drivers/dev/net)
