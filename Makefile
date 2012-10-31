include /ioc/tools/driver.makefile

# I need to find the Linux link.h before the EPICS link.h
USR_INCLUDES=-idirafter ${EPICS_BASE}/include 

HEADERS += require.h
BUILDCLASSES += Linux

SOURCES += require.c
DBDS    += require.dbd

SOURCES += listRecords.c
DBDS    += listRecords.dbd

SOURCES += updateMenuConvert.c
DBDS    += updateMenuConvert.dbd

SOURCES += addScan.c
DBDS    += addScan.dbd

SOURCES_3.14 += disctools.c
DBDS_3.14    += disctools.dbd

SOURCES_3.14 += exec.c
DBDS_3.14    += exec.dbd

SOURCES_3.14 += mlock.c
DBDS_3.14    += mlock.dbd

SOURCES_vxWorks += bootNotify.c
