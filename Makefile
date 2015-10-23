TOP=..

include $(TOP)/configure/CONFIG

# library
LOADABLE_LIBRARY = require
LIB_SRCS += require_registerRecordDeviceDriver.cpp

#require_DBD += base.dbd

LIB_SRCS += require.c
LIB_SRCS_vxWorks = asprintf.c strdup.c

require_DBD += require.dbd

LIB_LIBS += $(EPICS_BASE_IOC_LIBS)

# We need to find the Linux link.h before the EPICS link.h
USR_INCLUDES_Linux=-idirafter ${EPICS_BASE}/include 

# Pass T_A to the code
USR_CFLAGS += -DT_A=${T_A}
USR_CFLAGS_WIN32 += -D_WIN32_WINNT=0x501

include $(TOP)/configure/RULES
