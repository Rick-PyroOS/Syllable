# System tools, the login screen and the terminal
add_subdirectory(systools)        # init, ps, top, shutdown...
add_subdirectory(dlogin)
add_subdirectory(aterm)

# What start.sh and network-start.sh run
add_subdirectory(debug)           # dbterm
add_subdirectory(nettools)        # ifconfig, route, ping
add_subdirectory(dhcpc)

# Other applications
foreach(app SimpleBurn aedit Albert archiver AttribEdit aview bzoom catalog charmap cpumon
            crypt DiskManager fsutils keymapconv Launch Man memmon mixer PrintQueue xpdf)
  add_subdirectory(${app})
endforeach()

add_subdirectory(Preferences)
