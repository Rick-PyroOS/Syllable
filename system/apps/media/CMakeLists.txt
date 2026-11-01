# Media applications.  Installed where the Makefiles put them
# (applications/media), which is where ColdFish looks for its plugins
# (/applications/media/plug-ins); the release renamed them into
# "Audio & Video" with a newer ColdFish.
add_subdirectory(mediaplayer)
add_subdirectory(mediaconverter)
add_subdirectory(coldfish)
add_subdirectory(coldfish_plugins)
