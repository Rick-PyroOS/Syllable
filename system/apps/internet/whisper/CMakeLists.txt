# Whisper: e-mail client (Applications/Whisper), with its POP3 and SMTP
# libraries (static, as the Makefile builds them) and import plugins
add_library(whisper_pop3 STATIC libpop3/libpop3.c libpop3/pop3_timer.c)
add_library(whisper_smtp STATIC libsmtp/libsmtp.c libsmtp/smtp_timer.c libsmtp/smtp_util.c
                                libsmtp/smtp_auth.c libsmtp/base64.c libsmtp/MD5.c)
foreach(lib whisper_pop3 whisper_smtp)
  syllable_userland_headers(${lib})
  target_compile_options(${lib} PRIVATE -O2 -Wall)
endforeach()
target_include_directories(whisper_pop3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/libpop3)
target_include_directories(whisper_smtp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/libsmtp)

set(_main main appwindow appwindow_messages icon inputview messagenode composer_window dialogs
    message_view message_window mail mailbox syllable_mailbox qp_codec base64_codec identity
    transport pop3_transport smtp_transport filter rule_filter ixport strfuncs rfctime)
set(_settings settings_window identity_view signature_view outbound_view inbound_view filters_view)
list(TRANSFORM _main APPEND .cpp)
list(TRANSFORM _settings PREPEND settings_window/)
list(TRANSFORM _settings APPEND .cpp)
syllable_program(whisper OUTPUT_NAME Whisper SOURCES ${_main} ${_settings}
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/include
  OPTIONS -Wall LIBS whisper_pop3 whisper_smtp libsyllable STRIP INSTALL applications/Whisper)
file(GLOB _res RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/icons/*.png)
syllable_resources(whisper FILES ${_res})
syllable_catalogs(whisper NAME Whisper DIR resources LANGS da es hu nl sv EMBED)
syllable_attribute(applications/Whisper/Whisper os::Category Internet)

# Import plugins (Applications/Whisper/plugins/ixport)
syllable_plugin(whisper_mbox OUTPUT_NAME mbox SOURCES ixport/mbox/mbox.cpp
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/ixport/mbox ${CMAKE_CURRENT_SOURCE_DIR}/include
  OPTIONS -Wall LIBS libsyllable INSTALL applications/Whisper/plugins/ixport)
syllable_plugin(whisper_pst OUTPUT_NAME pst
  SOURCES ixport/pst/libpst.c ixport/pst/strfunc.c ixport/pst/lzfu.c ixport/pst/timeconv.c
          ixport/pst/debug.c ixport/pst/pst.cpp
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/ixport/pst ${CMAKE_CURRENT_SOURCE_DIR}/include
  OPTIONS -Wall LIBS libsyllable INSTALL applications/Whisper/plugins/ixport)
syllable_plugin(whisper_dbx OUTPUT_NAME dbx SOURCES ixport/dbx/dbx.cpp ixport/dbx/libdbx.c
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/ixport/dbx ${CMAKE_CURRENT_SOURCE_DIR}/include
  OPTIONS -Wall LIBS libsyllable INSTALL applications/Whisper/plugins/ixport)
