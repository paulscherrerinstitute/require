include /ioc/tools/driver.makefile

BUILDCLASSES += Linux

SOURCES += require.c
DBDS    += require.dbd

#HEADERS += require.h

SOURCES_vxWorks += strdup.c asprintf.c
HEADERS += strdup.h asprintf.h

# We need to find the Linux link.h before the EPICS link.h
USR_INCLUDES_Linux=-idirafter ${EPICS_BASE}/include 

# Pass T_A to the code
USR_CFLAGS += -DT_A=${T_A}
