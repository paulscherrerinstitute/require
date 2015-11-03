TOP=..

include $(TOP)/configure/CONFIG

# library
LOADABLE_LIBRARY = require
LIB_SRCS += require_registerRecordDeviceDriver.cpp

LIB_SRCS += require.c
LIB_SRCS += runScript.c
LIB_SRCS_vxWorks = asprintf.c strdup.c
LIB_SRCS_WIN32 = asprintf.c

DBD = requireSup.dbd
requireSup_DBD += require.dbd
requireSup_DBD += runScript.dbd

LIB_LIBS += $(EPICS_BASE_IOC_LIBS)

# We need to find the Linux link.h before the EPICS link.h
USR_INCLUDES_Linux=-idirafter ${EPICS_BASE}/include 

# Pass T_A to the code
USR_CFLAGS += -DT_A=${T_A}

# This should really go into some global WIN32 config file
USR_CFLAGS_WIN32 += /D_WIN32_WINNT=0x501

include $(TOP)/configure/RULES
