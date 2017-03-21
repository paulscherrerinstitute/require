include /ioc/tools/driver.makefile

BUILDCLASSES += Linux

SOURCES += require.c
DBDS    += require.dbd
SOURCES += runScript.c
DBDS    += runScript.dbd

#HEADERS += require.h

SOURCES_T2 += strdup.c
SOURCES_vxWorks   += asprintf.c
HEADERS += strdup.h asprintf.h
HEADERS += require.h

# We need to find the Linux link.h before the EPICS link.h
USR_INCLUDES_Linux=-idirafter ${EPICS_BASE}/include 

# Pass T_A to the code
USR_CFLAGS += -DT_A=${T_A}
