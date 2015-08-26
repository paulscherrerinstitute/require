include /ioc/tools/driver.makefile

# I need to find the Linux link.h before the EPICS link.h
USR_INCLUDES_Linux=-idirafter ${EPICS_BASE}/include 
USR_INCLUDES+=${USR_INCLUDES_${OS_CLASS}}

USR_CFLAGS=-DEPICS_RELEASE='"${EPICSVERSION}"' -DT_A='"${T_A}"' -DOS_CLASS='"${OS_CLASS}"'

HEADERS += require.h
BUILDCLASSES += Linux

SOURCES += require.c
DBDS    += require.dbd

SOURCES_vxWorks += strdup.c asprintf.c
HEADERS += strdup.h asprintf.h
