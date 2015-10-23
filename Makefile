TOP=..

include $(TOP)/configure/CONFIG

# library
LIBRARY = require
LIB_SRCS += require_registerRecordDeviceDriver.cpp

#require_DBD += base.dbd

LIB_SRCS += require.c
LIB_SRCS_vxWorks = asprintf.c strdup.c

require_DBD += $(LIBRARY).dbd

LIB_LIBS += $(EPICS_BASE_IOC_LIBS)

# We need to find the Linux link.h before the EPICS link.h
USR_INCLUDES_Linux=-idirafter ${EPICS_BASE}/include 

# Pass T_A to the code
USR_CFLAGS += -DT_A=${T_A}

include $(TOP)/configure/RULES
