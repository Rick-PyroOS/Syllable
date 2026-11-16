# system/image: files installed onto the boot volume as they are

Everything here is laid out the way it appears on the installed system
(`etc/`, `home/`, `system/config/`, ...) and is installed as-is by
`CMakeLists.txt`: configuration files, the boot scripts init runs, the home
directory template and root's home, and wallpapers.

Most of it came from `apps/utils/Builder/resources`, which Builder's profiles
copied into the image (`syllable`: configuration, scripts, home directories;
`base`: s3conf, vsftpd, rsyncd; `gui`: appserver and net configuration).
`home/` is `binaries/home-directories.zip` unpacked (its symlinks kept).

Generated or built files do not belong here; components install their own.

system/config/net.cfg describes QEMU's network card (rtl8139, MAC
52:54:00:12:34:56, DHCP), so `sb run` boots without the first-boot "network
configuration changed" dialog, which blocks start.sh (and with it gdbserver)
until someone clicks OK.  On other hardware the MAC differs and the dialog
appears as in the release.
