include /ioc/tools/driver.makefile

BUILDCLASSES += Linux

# I need to find the Linux link.h before the EPICS link.h
USR_INCLUDES_Linux=-idirafter ${EPICS_BASE}/include 

USR_CFLAGS=-DEPICS_RELEASE='"${EPICSVERSION}"' -DT_A='"${T_A}"' -DOS_CLASS='"${OS_CLASS}"'

SOURCES += require.c
DBDS    += require.dbd

#HEADERS += require.h

SOURCES_vxWorks += strdup.c asprintf.c
HEADERS += strdup.h asprintf.h
