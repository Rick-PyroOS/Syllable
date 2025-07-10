# Syllable Main Repo

---

This is the main repo that I will be putting changes in.

At some point this will be  merged into Pyro, but the structure
has changed so much that I want to be able to just focus on changes for now.


How am I compiling quickly
---

I am using qemu to compile on a Mac M2 with 32gb of ram.
Qemu is only reporting it as a 1000mhz single thread, something we will
have to fix.  For now, I am just syncing code over using rsync: 

```shell

alias syllable-rsync='(){ echo $1; rsync -acvzh $2 $1 --recursive --cvs-exclude --rsync-path=/usr/rsync/bin/rsync --exclude .git ;}'

#Run with: syllable-rsync "root@172.16.224.104:/home/root/Code/" ../Syllable


```

UTM looks like:
![qemu.png](qemu.png)


How can I see Syllable logs in QEMU window?
---

You can change: ***/system/bin/dbterm 8 >> /var/log/kernel &*** to ***/system/bin/dbterm 1 >> /var/log/kernel &*** in /system/init.sh of your Syllable machine


Building appserver/libos
---
You will need: 
* gdk-pixelbuf-2.24.1 -> run `build gdk-pixelbuf-2.24.1 && build install gdk-pixelbuf-2.24.1`
* cairo-1.4.12 -> run `build cairo-1.4.12 && build install cairo-1.4.12`
* pango-1.20.2  -> run `build pango-1.20.2 && build install pango-1.20.2`
* librsvg - run `build librsvg-2.22.3 && build install librsvg-2.22.3`



