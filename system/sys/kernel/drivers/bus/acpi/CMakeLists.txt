# ACPI bus driver (ACPICA)
syllable_driver(acpi
  SOURCES
  acpi.c bus.c button.c ec.c fan.c memset.c
  osl.c power.c scan.c simple_pci.c thermal.c utils.c
  wakeup.c video.c dispatcher/dsfield.c dispatcher/dsinit.c dispatcher/dsmethod.c dispatcher/dsmthdat.c
  dispatcher/dsobject.c dispatcher/dsopcode.c dispatcher/dsutils.c dispatcher/dswexec.c dispatcher/dswload.c dispatcher/dswscope.c
  dispatcher/dswstate.c events/evevent.c events/evgpe.c events/evgpeblk.c events/evmisc.c events/evregion.c
  events/evrgnini.c events/evsci.c events/evxface.c events/evxfevnt.c events/evxfregn.c executer/exconfig.c
  executer/exconvrt.c executer/excreate.c executer/exdump.c executer/exfield.c executer/exfldio.c executer/exmisc.c
  executer/exmutex.c executer/exnames.c executer/exoparg1.c executer/exoparg2.c executer/exoparg3.c executer/exoparg6.c
  executer/exprep.c executer/exregion.c executer/exresnte.c executer/exresolv.c executer/exresop.c executer/exstore.c
  executer/exstoren.c executer/exstorob.c executer/exsystem.c executer/exutils.c hardware/hwacpi.c hardware/hwgpe.c
  hardware/hwregs.c hardware/hwsleep.c hardware/hwtimer.c namespace/nsaccess.c namespace/nsalloc.c namespace/nsdump.c
  namespace/nsdumpdv.c namespace/nseval.c namespace/nsinit.c namespace/nsload.c namespace/nsnames.c namespace/nsobject.c
  namespace/nsparse.c namespace/nssearch.c namespace/nsutils.c namespace/nswalk.c namespace/nsxfeval.c namespace/nsxfname.c
  namespace/nsxfobj.c parser/psargs.c parser/psloop.c parser/psopcode.c parser/psparse.c parser/psscope.c
  parser/pstree.c parser/psutils.c parser/pswalk.c parser/psxface.c resources/rsaddr.c resources/rscalc.c
  resources/rscreate.c resources/rsdump.c resources/rsio.c resources/rsirq.c resources/rslist.c resources/rsmemory.c
  resources/rsmisc.c resources/rsutils.c resources/rsxface.c resources/rsinfo.c tables/tbfadt.c tables/tbfind.c
  tables/tbinstal.c tables/tbutils.c tables/tbxface.c tables/tbxfroot.c utilities/utalloc.c utilities/utcache.c
  utilities/utcopy.c utilities/utdebug.c utilities/utdelete.c utilities/uteval.c utilities/utglobal.c utilities/utinit.c
  utilities/utmath.c utilities/utmisc.c utilities/utmutex.c utilities/utobject.c utilities/utstate.c utilities/utxface.c
  utilities/utresrc.c
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/include
  DEFINES __ENABLE_DEBUG__
  OPTIONS -ffreestanding -fno-strict-aliasing
  INSTALL system/drivers/dev/bus)
