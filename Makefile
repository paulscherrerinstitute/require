TOP=..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

# build an ioc application (all IOC targets except vxWorks)

APP = require
LIBRARY_IOC_DEFAULT = require
DBD += require.dbd

USR_CFLAGS += /D_WIN32_WINNT=0x501
USR_CFLAGS += /D'T_A="$(T_A)"'




# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
require_SRCS += require_registerRecordDeviceDriver.cpp
require_LIBS += $(EPICS_BASE_IOC_LIBS)


# require
require_SRCS += require.c
require_DBD += require.dbd

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

