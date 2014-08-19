# driver.makefile
#
# $Header: /cvs/G/DRV/misc/App/tools/driver.makefile,v 1.99 2014/08/19 09:19:13 zimoch Exp $
#
# This generic makefile compiles EPICS code (drivers, records, snl, ...)
# for all installed EPICS versions in parallel.
# Read this documentation and the inline comments carefully before
# changing anything in this file.
#
# Usage: Create a Makefile containig the line:
#          include /ioc/tool/driver.makefile
#        Optionally add variable definitions below that line.
#
# This makefile automatically finds the source file (unless overwritten with
# the SOURCES variable in your Makefile) and generates a
# library for each EPICS version and each target architecture.
# Therefore, it calls Makefile (i.e. itself) recursively.
#
# - First run: (see comment ## RUN 1)
#   Find out what to build
#   Iterate over all installed EPICS versions
#
# - Second run: (see comment ## RUN 2)
#   Find the sources etc.
#   Include EPICS configuration files for this ${EPICSVERSION}
#   Iterate over all target architectures (${T_A}) defined for this version
#   Create O.${EPICSVERSION}_${T_A} subdirectories if necessary.
#
# - Third run: (see comment ## RUN 3)
#   Compile (or install, uninstall, etc) everything
#
# Library names are derived from the directory name (unless overwritten
# with the PROJECT variable in your Makefile).
# A version number is appended to the name which is derived from
# the latest CVS tag on the files in the source directory.
# If any file is not up-to-date in CVS, not tagged, or tagged differently from the
# other files, the version is a test version and labelled with the user name.
# The library is installed to ${INSTALL_ROOT}/R${EPICSVERSION}/${T_A}.
# Symbolic links are set to the latest version, the highest minor release
# if each major release, and the highest patch level of each minor release.
# Test versions are never linked, however.
# The library can be loaded with  require "<libname>" [,"<version>"]
#
# This makefile can also be used to build local libraries for IOC projects
# instead of drivers. Here however, no versioning is done (i.e. no
# version string is appended) and the libraries cannot be installed with
# this makefile. Use slsinstall to install the IOC project instead.
#
# User variables (add them to your Makefile, none is required):
# PROJECT
#    Basename of the built library.
#    If not defined, it is derived from the directory name.
# SOURCES
#    All source files to compile. 
#    If not defined, default is all *.c *.cc *.cpp *.st *.stt in
#    the source directory (where your Makefile is).
#    If you define this, you must list ALL sources.
# DBDS
#    All dbd files of the project.
#    If not defined, default is all *.dbd files in the source directory.
# HEADERS
#    Header files to install (e.g. to be included by other drivers)
#    If not defined, all headers are for local use only.
# EXCLUDE_VERSIONS
#    EPICS versions to skip. Usually 3.13 or 3.14
# EXCLUDE_ARCHS
#    Skip architectures that start or end with the pattern, e.g. T2 or ppc604 

# get the location of this file 
MAKEHOME:=$(dir $(lastword ${MAKEFILE_LIST}))
# get the name of the Makefile that included this file
USERMAKEFILE:=$(lastword $(filter-out $(lastword ${MAKEFILE_LIST}), ${MAKEFILE_LIST}))

# Some configuration
DEFAULT_EPICS_VERSIONS = 3.13.9 3.13.10 3.14.8 3.14.12
BUILDCLASSES = vxWorks
INSTBASE ?= /work
INSTALL_ROOT=${INSTBASE}/iocBoot
EPICS_LOCATION = /usr/local/epics
SNCSEQ=${EPICS_BASE}/../seq

DOCUEXT = txt html htm doc pdf ps tex dvi gif jpg png
DOCUEXT += TXT HTML HTM DOC PDF PS TEX DVI GIF JPG PNG
DOCUEXT += template db dbt subs subst substitutions script

GLOBALPROJECTS = /PROJECTS/drivers/ /DRV/ /IOCMON/ /CRLOGIC/

#override config here
-include ${MAKEHOME}/config

# Are we in an IOC project directory?
# YES:
#   - Don't use versions.
#   - Install to IOC directory with slsinstall.
#   - Use for local code (snl, subroutine records, etc.)
#   - Autodetected: SLS beamline and machine, PROSCAN, FEL
# NO:
#   - Get version number from CVS tag.
#   - Install to driver pool with make install.
#   - Use for drivers and other modules of global interest.
#   - This is the default.
# User can overwrite USE_LIBVERSION in the Makefile.

# Where are we in CVS (or in PWD if no CVS is around)?
THISDIR := $(if $(wildcard CVS/Repository),$(shell cat CVS/Repository),${PWD})
USE_LIBVERSION = $(if $(strip $(foreach d,${GLOBALPROJECTS},$(findstring $d,${THISDIR}))),YES,NO)

# Some shortcuts
MAKEVERSION = ${MAKE} -f ${USERMAKEFILE} LIBVERSION=${LIBVERSION}

# Some shell commands
LN = ln -s
EXISTS = test -e
NM = nm
RMDIR = rm -rf
RM = rm -f

# some generated file names
VERSIONFILE = ${PRJ}_Version${LIBVERSION}.c
REGISTRYFILE = ${PRJ}_registerRecordDeviceDriver.cpp
EXPORTFILE = ${PRJ}_exportAddress.c
SUBFUNCFILE = ${PRJ}_subRecordFunctions.dbd

ifndef EPICSVERSION
## RUN 1
# in source directory, first run

# Find out which EPICS versions to build
INSTALLED_EPICS_VERSIONS := $(patsubst ${EPICS_LOCATION}/base-%,%,$(wildcard ${EPICS_LOCATION}/base-*[0-9]))
EPICS_VERSIONS = $(filter-out ${EXCLUDE_VERSIONS:=%},${DEFAULT_EPICS_VERSIONS})
MISSING_EPICS_VERSIONS = $(filter-out ${BUILD_EPICS_VERSIONS},${EPICS_VERSIONS})
BUILD_EPICS_VERSIONS = $(filter ${INSTALLED_EPICS_VERSIONS},${EPICS_VERSIONS})
EPICS_VERSIONS_3.13 = $(filter 3.13.%,${BUILD_EPICS_VERSIONS})
EPICS_VERSIONS_3.14 = $(filter 3.14.%,${BUILD_EPICS_VERSIONS})
EPICS_VERSIONS_3.15 = $(filter 3.15.%,${BUILD_EPICS_VERSIONS})

VERSIONCHECKFILES = ${SOURCES} ${DBDS} $(foreach v,3.13 3.14 3.15, ${SOURCES_$v} ${DBDS_$v})
VERSIONCHECKCMD = ${MAKEHOME}/getVersion.tcl ${VERSIONCHECKFILES}
LIBVERSION_YES = $(or $(filter-out test,$(shell ${VERSIONCHECKCMD} 2>/dev/null)),${USER},test)
LIBVERSION_Yes = $(LIBVERSION_YES)
LIBVERSION_yes = $(LIBVERSION_YES)
LIBVERSION = ${LIBVERSION_${USE_LIBVERSION}}

# Default project name is name of current directory.
# But don't use "src" or "snl", go up directory tree instead.
PRJDIR:=$(notdir $(patsubst %Lib,%,$(patsubst %/snl,%,$(patsubst %/src,%,${PWD}))))
PRJ = $(if ${PROJECT},${PROJECT},${PRJDIR})
export PRJ

OS_CLASS_LIST = $(BUILDCLASSES)
export OS_CLASS_LIST

export ARCH_FILTER
export EXCLUDE_ARCHS

# Default target is "build" for all versions.
# Don't install anything (different from default EPICS make rules)
build::

clean::
	$(RMDIR) O.*
#	find . -name "*~" -exec $(RM) {} \;

clean.3.%::
	$(RMDIR) O.${@:clean.%=%}*

help:
	@echo "usage:"
	@for target in '' build '<EPICS version>' \
	install 'install.<EPICS version>' \
	uninstall 'uninstall.<EPICS version>' \
	install-headers 'install-headers.<EPICS version>' \
	install-doc install-templates clean help version; \
	do echo "  make $$target"; \
	done
	@echo "Makefile variables: (defaults)"
	@echo "  EPICS_VERSIONS   (${DEFAULT_EPICS_VERSIONS})"
	@echo "  PROJECT          (${PRJDIR}) [from current directory name]"
	@echo "  SOURCES          (*.c *.cc *.cpp *.st *.stt *.gt)"
	@echo "  HEADERS          () [only those to install]"
	@echo "  TEMPLATES        ()"
	@echo "  DBDS             (*.dbd)"
	@echo "  EXCLUDE_VERSIONS () [versions not to build, e.g. 3.14]"
	@echo "  EXCLUDE_ARCHS    () [target architectures not to build, e.g. embeddedlinux]"
	@echo "  BUILDCLASSES     (vxWorks) [other choices: Linux]"

# "make version" shows the version and why it is how it is.       
version:
	@${VERSIONCHECKCMD}

debug::
	@echo "INSTALLED_EPICS_VERSIONS = ${INSTALLED_EPICS_VERSIONS}"
	@echo "BUILD_EPICS_VERSIONS = ${BUILD_EPICS_VERSIONS}"
	@echo "MISSING_EPICS_VERSIONS = ${MISSING_EPICS_VERSIONS}"
	@echo "EPICS_VERSIONS_3.13 = ${EPICS_VERSIONS_3.13}"
	@echo "EPICS_VERSIONS_3.14 = ${EPICS_VERSIONS_3.14}"
	@echo "BUILDCLASSES = ${BUILDCLASSES}"
	@echo "USE_LIBVERSION = ${USE_LIBVERSION}"
	@echo "LIBVERSION = ${LIBVERSION}"
	@echo "VERSIONCHECKFILES = ${VERSIONCHECKFILES}"
	@echo "ARCH_FILTER = ${ARCH_FILTER}"

# Loop over all EPICS versions for second run.
build install uninstall install-headers install-doc install-templates debug::
	for VERSION in ${BUILD_EPICS_VERSIONS}; do \
	${MAKEVERSION} EPICSVERSION=$$VERSION $@ || exit; done

# Handle cases where user requests 3.13 or 3.14 
# make <action>.3.13 or make <action>.3.14 instead of make <action> or
# make 3.13 or make 3.14 instead of make

define VERSIONRULES
$(1):
	for VERSION in $${EPICS_VERSIONS_$(1)}; do \
	$${MAKEVERSION} EPICSVERSION=$$$$VERSION build || exit; done

%.$(1):
	for VERSION in $${EPICS_VERSIONS_$(1)}; do \
	$${MAKEVERSION} EPICSVERSION=$$$$VERSION $${@:%.$(1)=%} || exit; done
endef

$(foreach v,3.13 3.14 3.15,$(eval $(call VERSIONRULES,$v)))

# Handle cases where user requests one specific version
# make <action>.<version> instead of make <action> or
# make <version> instead of make
# EPICS version must be installed but need not be in EPICS_VERSIONS
${INSTALLED_EPICS_VERSIONS}:
	${MAKEVERSION} EPICSVERSION=$@ build

${INSTALLED_EPICS_VERSIONS:%=build.%}:
	${MAKEVERSION} EPICSVERSION=${@:build.%=%} build

${INSTALLED_EPICS_VERSIONS:%=install.%}:
	${MAKEVERSION} EPICSVERSION=${@:install.%=%} install

${INSTALLED_EPICS_VERSIONS:%=uninstall.%}:
	${MAKEVERSION} EPICSVERSION=${@:uninstall.%=%} uninstall

${INSTALLED_EPICS_VERSIONS:%=install-headers.%}:
	${MAKEVERSION} EPICSVERSION=${@:install-headers.%=%} install-headers

${INSTALLED_EPICS_VERSIONS:%=debug.%}:
	${MAKEVERSION} EPICSVERSION=${@:debug.%=%} debug

else # EPICSVERSION
# EPICSVERSION defined 
# second or third turn (see T_A branch below)

EPICS_BASE=${EPICS_LOCATION}/base-${EPICSVERSION}

ifneq ($(filter 3.14.% 3.15.% ,$(EPICSVERSION)),)
EPICS_BASETYPE=3.14

# There is no 64 bit support before 3.14.12 
ifneq ($(filter %_64,$(EPICS_HOST_ARCH)),)
ifeq ($(wildcard $(EPICS_BASE)/lib/$(EPICS_HOST_ARCH)),)
EPICS_HOST_ARCH:=$(patsubst %_64,%,$(EPICS_HOST_ARCH))
export USR_CFLAGS_$(EPICS_HOST_ARCH) += -m32
export USR_CXXFLAGS_$(EPICS_HOST_ARCH) += -m32
export USR_LDFLAGS_$(EPICS_HOST_ARCH) += -m32
endif
endif

${EPICS_BASE}/configure/CONFIG:
	@echo "ERROR: EPICS release ${EPICSVERSION} not installed on this host."

# Some TOP and EPICS_BASE tweeking necessary to work around release check in 3.14.10+
CONFIG=${EPICS_BASE}/configure
EB=${EPICS_BASE}
TOP:=${EPICS_BASE}
-include ${EPICS_BASE}/configure/CONFIG
EPICS_BASE:=${EB}
SHRLIB_VERSION=
COMMON_DIR = O.${EPICSVERSION}_Common
# do not link *everything* with readline (and curses)
COMMANDLINE_LIBRARY =
endif # 3.14

ifneq ($(filter 3.13.%,$(EPICSVERSION)),)

EPICS_BASETYPE=3.13
${EPICS_BASE}/config/CONFIG:
	@echo "ERROR: EPICS release ${EPICSVERSION} not installed on this host."

-include ${EPICS_BASE}/config/CONFIG
OBJ=.o
export BUILD_TYPE=Vx
endif # 3.13

INSTALL_LOCATION = ${INSTALL_ROOT}/R${EPICSVERSION}

ifndef T_A
### RUN 2
# target achitecture not yet defined
# but EPICSVERSION is already known
# still in source directory, second run

# Look for sources etc.
# Export everything for third run

AUTOSRCS := $(filter-out ~%,$(wildcard *.c) $(wildcard *.cc) $(wildcard *.cpp) $(wildcard *.st) $(wildcard *.stt) $(wildcard *.gt))
SRCS = $(if ${SOURCES},$(filter-out -none-,${SOURCES}),${AUTOSRCS})
SRCS += ${SOURCES_${EPICS_BASETYPE}}
SRCS += ${SOURCES_${EPICSVERSION}}
export SRCS

DBDFILES = $(if ${DBDS},$(filter-out -none-,${DBDS}),$(wildcard *Record.dbd) $(strip $(filter-out %Include.dbd dbCommon.dbd %Record.dbd,$(wildcard *.dbd)) ${BPTS}))
DBDFILES += ${DBDS_${EPICS_BASETYPE}}
DBDFILES += ${DBDS_${EPICSVERSION}}
DBDFILES += $(patsubst %.gt,%.dbd,$(notdir $(filter %.gt,${SRCS})))
ifeq (${EPICS_BASETYPE},3.14)
DBDFILES += $(patsubst %.st,%_snl.dbd,$(notdir $(filter %.st,${SRCS})))
DBDFILES += $(patsubst %.stt,%_snl.dbd,$(notdir $(filter %.stt,${SRCS})))
endif # 3.14
export DBDFILES

RECORDS1 = $(patsubst %Record.dbd,%,$(notdir $(filter %Record.dbd, ${DBDFILES})))
RECORDS2 = $(shell ${MAKEHOME}/expandDBD.tcl -r $(addprefix -I, $(sort $(dir ${DBDFILES}))) $(realpath ${DBDS}))
RECORDS = $(sort ${RECORDS1} ${RECORDS2})
export RECORDS

MENUS = $(patsubst %.dbd,%.h,$(wildcard menu*.dbd))
export MENUS

BPTS = $(patsubst %.data,%.dbd,$(wildcard bpt*.data))
export BPTS

HDRS = ${HEADERS} $(addprefix ${COMMON_DIR}/,$(addsuffix Record.h,${RECORDS}))
HDRS += ${HEADERS_${EPICS_BASETYPE}}
HDRS += ${HEADERS_${EPICSVERSION}}
export HDRS

TEMPLS = ${TEMPLATES}
TEMPLS += ${TEMPLATES_${EPICS_BASETYPE}}
TEMPLS += ${TEMPLATES_${EPICSVERSION}}
export TEMPLS

DOCUDIR = .
#DOCU = $(foreach DIR,${DOCUDIR},$(wildcard ${DIR}/*README*) $(foreach EXT,${DOCUEXT}, $(wildcard ${DIR}/*.${EXT})))
export DOCU

# Loop over all target architectures for third run
# Go to O.${T_A} subdirectory because Rules.Vx only work there:

ifeq (${EPICS_BASETYPE},3.14)
CROSS_COMPILER_TARGET_ARCHS += ${EPICS_HOST_ARCH}
endif # 3.14
CROSS_COMPILER_TARGET_ARCHS := $(filter-out $(addprefix %,${EXCLUDE_ARCHS}),$(filter-out $(addsuffix %,${EXCLUDE_ARCHS}),$(if ${ARCH_FILTER},$(filter ${ARCH_FILTER},${CROSS_COMPILER_TARGET_ARCHS}),${CROSS_COMPILER_TARGET_ARCHS})))

SRCS_Linux = ${SOURCES_Linux}
SRCS_Linux += ${SOURCES_${EPICS_BASETYPE}_Linux}
SRCS_Linux += ${SOURCES_Linux_${EPICS_BASETYPE}}
export SRCS_Linux
SRCS_vxWorks = ${SOURCES_vxWorks}
SRCS_vxWorks += ${SOURCES_${EPICS_BASETYPE}_vxWorks}
SRCS_vxWorks += ${SOURCES_vxWorks_${EPICS_BASETYPE}}
export SRCS_vxWorks
DBDFILES_Linux = ${DBDS_Linux}
DBDFILES_Linux += ${DBDS_${EPICS_BASETYPE}_Linux}
DBDFILES_Linux += ${DBDS_Linux_${EPICS_BASETYPE}}
export DBDFILES_Linux
DBDFILES_vxWorks = ${DBDS_vxWorks}
DBDFILES_vxWorks += ${DBDS_${EPICS_BASETYPE}_vxWorks}
DBDFILES_vxWorks += ${DBDS_vxWorks_${EPICS_BASETYPE}}
export DBDFILES_vxWorks

# Do not install without version
ifndef LIBVERSION
install uninstall install-headers::
	@echo "ERROR: Can't $@ without LIBVERSION defined"
	@exit 1
endif # !LIBVERSION

install build install-headers debug::
	@echo "MAKING EPICS VERSION R${EPICSVERSION}"

debug::
	@echo "EPICS_BASE = ${EPICS_BASE}"
	@echo "EPICSVERSION = ${EPICSVERSION}" 
	@echo "EPICS_BASETYPE = ${EPICS_BASETYPE}" 
	@echo "CROSS_COMPILER_TARGET_ARCHS = ${CROSS_COMPILER_TARGET_ARCHS}"
	@echo "EXCLUDE_ARCHS = ${EXCLUDE_ARCHS}"
	@echo "INSTALL_LOCATION = ${INSTALL_LOCATION}"
	@echo "LIBVERSION = ${LIBVERSION}"
	@echo "RELEASE_TOPS = ${RELEASE_TOPS}"


# Create build dirs (and links) if necessary
LINK_eldk52-e500v2 = eldk52-rt-e500v2 eldk52-xenomai-e500v2

BUILDDIRS = $(addprefix O.${EPICSVERSION}_, ${CROSS_COMPILER_TARGET_ARCHS})
ifeq (${EPICS_BASETYPE},3.14)
    BUILDDIRS += O.${EPICSVERSION}_Common
endif

${BUILDDIRS}:
	mkdir $@

define MAKELINKDIRS
LINKDIRS+=O.${EPICSVERSION}_$1
O.${EPICSVERSION}_$1:
	$(LN) O.${EPICSVERSION}_$2 O.${EPICSVERSION}_$1
endef 

$(foreach a,${CROSS_COMPILER_TARGET_ARCHS},$(foreach l,$(LINK_$a),$(eval $(call MAKELINKDIRS,$l,$a))))

install build install-headers debug:: .cvsignore ${BUILDDIRS} ${LINKDIRS}
# Delete old build if INSTBASE has changed.
	@for ARCH in ${CROSS_COMPILER_TARGET_ARCHS}; do \
            echo ${INSTBASE} | cmp -s O.${EPICSVERSION}_$$ARCH/INSTBASE - || $(RM) O.${EPICSVERSION}_$$ARCH/*; \
	    ${MAKE} -C O.${EPICSVERSION}_$$ARCH -f ../${USERMAKEFILE} T_A=$$ARCH $@; \
	done

.cvsignore:
	echo "O.* .cvsignore" > .cvsignore

# No need to create O.${T_A} subdirectory here:
uninstall install-doc install-templates::
	@echo "MAKING EPICS VERSION R${EPICSVERSION}"
	for ARCH in ${CROSS_COMPILER_TARGET_ARCHS}; do \
	${MAKEVERSION} T_A=$$ARCH $@; done

else # T_A
## RUN 3
# target architecture defined 
# third run, in O.* directory for build, install, install-headers
# still in source directory for uninstall, install-doc, install-templates

ifeq ($(filter ${OS_CLASS},${OS_CLASS_LIST}),)

install%: build
install: build
build%: build
build:
	@echo Skipping ${T_A} because $(if ${OS_CLASS},${OS_CLASS} is not in BUILDCLASSES = ${BUILDCLASSES},it is not available for R$(EPICSVERSION).)
%:
	@true

else ifeq ($(wildcard $(firstword ${CC})),)

install%: build
install: build
build%: build
build:
	@echo Warning: Skipping ${T_A} because cross compiler $(firstword ${CC}) is not installed.
%:
	@true

else

CFLAGS += ${EXTRA_CFLAGS}

LIBVERSIONSTR = $(if ${LIBVERSION},-${LIBVERSION})
TESTVERSION := $(shell echo "${LIBVERSION}" | grep -v -E "^[0-9]+\.[0-9]+\.[0-9]+\$$")
PROJECTDBD=${if ${DBDFILES},${PRJ}${LIBVERSIONSTR}.dbd}
DEPFILE = ${PRJ}${LIBVERSIONSTR}.dep

INSTALL_BIN = ${INSTALL_LOCATION}/${T_A}
INSTALL_DOC = $(dir ${INSTALL_LOCATION})driverdoc
INSTALL_TEMPL = $(dir ${INSTALL_LOCATION})templates
INSTALL_LIBRARY = $(addprefix ${INSTALL_BIN}/,${PROJECTLIB})
INSTALL_PROJECTDBD = $(addprefix ${INSTALL_DBD}/,${PROJECTDBD})
INSTALL_HDRS = $(patsubst %.h,${INSTALL_INCLUDE}/%${LIBVERSIONSTR}.h, $(notdir ${HDRS}))
INSTALL_DOCUS = $(addprefix ${INSTALL_DOC}/${PRJ}/,$(notdir ${DOCU}))
INSTALL_TEMPLATES = $(addprefix ${INSTALL_TEMPL}/,$(subst .,${LIBVERSIONSTR}.,$(notdir ${TEMPLS})))
INSTALL_DEP = ${INSTALL_BIN}/${DEPFILE}

INSTALLDIRS = ${INSTALL_LOCATION} ${INSTALL_INCLUDE} ${INSTALL_BIN} 
INSTALLDIRS += ${INSTALL_DBD} ${INSTALL_DOC} ${INSTALL_DOC}/${PRJ}
INSTALLDIRS += ${INSTALL_TEMPL} 

COMMON_DIR_3.14 = ../O.${EPICSVERSION}_Common
COMMON_DIR_3.13 = .
COMMON_DIR = ${COMMON_DIR_${EPICS_BASETYPE}}

debug::
	@echo "BUILDCLASSES = ${BUILDCLASSES}"
	@echo "OS_CLASS = ${OS_CLASS}"
	@echo "T_A = ${T_A}"
	@echo "ARCH_PARTS = ${ARCH_PARTS}"
	@echo "PROJECTDBD = ${PROJECTDBD}"
	@echo "RECORDS = ${RECORDS}"
	@echo "MENUS = ${MENUS}"
	@echo "BPTS = ${BPTS}"
	@echo "HDRS = ${HDRS}"
	@echo "SOURCES = ${SOURCES}" 
	@echo "SOURCES_${EPICS_BASETYPE} = ${SOURCES_${EPICS_BASETYPE}}" 
	@echo "SOURCES_${OS_CLASS} = ${SOURCES_${OS_CLASS}}" 
	@echo "SRCS = ${SRCS}" 
	@echo "LIBOBJS = ${LIBOBJS}"
	@echo "DBDS = ${DBDS}"
	@echo "DBDS_${EPICS_BASETYPE} = ${DBDS_${EPICS_BASETYPE}}"
	@echo "DBDS_${OS_CLASS} = ${DBDS_${OS_CLASS}}"
	@echo "DBDFILES = ${DBDFILES}"
	@echo "LIBVERSION = ${LIBVERSION}"
	@echo "TESTVERSION = ${TESTVERSION}"

ifeq (${EPICS_BASETYPE},3.13)
INSTALLRULE=install::
BUILDRULE=build::
BASERULES=${EPICS_BASE}/config/RULES.Vx
else # 3.14
INSTALLRULE=install:
BUILDRULE=build:
BASERULES=${EPICS_BASE}/configure/RULES
endif # 3.14

$(INSTALLRULE) build ${INSTALLDIRS} ${INSTALL_HDRS} ${INSTALL_TEMPLATES}

install-headers:: ${INSTALL_LOCATION} ${INSTALL_INCLUDE}
install-headers:: ${INSTALL_HDRS}
install-templates:: ${INSTALL_TEMPLATES}
install-doc:: ${INSTALL_LOCATION} ${INSTALL_DOC} ${INSTALL_DOC}/${PRJ} ${INSTALL_DOCUS}

#link only non-test versions
SETLINKS=$(if ${TESTVERSION},@\#,${MAKEHOME}setLinks.tcl)

${INSTALLDIRS}:
	mkdir -m 775 $@

${INSTALL_DOC}/${PRJ}/%: %
	@echo "Installing documentation $@"
	$(RM) $@
	cp $^ $@
	chmod 444 $@

${INSTALL_TEMPL}/%${LIBVERSIONSTR}.template: %.template
	@echo "Installing template file $@"
	$(RM) $@
	echo "#${PRJ}Lib ${LIBVERSION}" > $@
	cat $^ >> $@
	chmod 444 $@
	$(SETLINKS) ${INSTALL_TEMPL} .template $(basename $(notdir $^))

${INSTALL_TEMPL}/%${LIBVERSIONSTR}.db: %.db
	@echo "Installing template file $@"
	$(RM) $@
	echo "#${PRJ}Lib ${LIBVERSION}" > $@
	cat $^ >> $@
	chmod 444 $@
	$(SETLINKS) ${INSTALL_TEMPL} .db $(basename $(notdir $^))

ifeq ($(filter O.%,$(notdir ${CURDIR})),)
# still in source directory, third run
# EPICSVERSION and T_A defined 

RMFILES += ${INSTALL_BIN}/${PRJ}Lib${LIBVERSIONSTR}
RMFILES += ${INSTALL_BIN}/${PRJ}Lib${LIBVERSIONSTR}.munch
RMFILES += ${INSTALL_BIN}/lib${PRJ}${LIBVERSIONSTR}.so
RMFILES += ${INSTALL_DEP}
RMFILES += ${INSTALL_PROJECTDBD}
RMFILES += ${INSTALL_HDRS}
RMFILES += ${INSTALL_TEMPLATES}

uninstall::
	@for i in ${RMFILES}; \
	    do ${EXISTS} $$i && echo "Uninstalling $$i" && $(RM) $$i; \
	done; true
	$(SETLINKS) ${INSTALL_BIN} "" ${PRJ}Lib
	$(SETLINKS) ${INSTALL_BIN} .munch ${PRJ}Lib
	$(SETLINKS) ${INSTALL_BIN} .so lib${PRJ}
	$(SETLINKS) ${INSTALL_BIN} .dep ${PRJ}
	$(SETLINKS) ${INSTALL_DBD} .dbd $(notdir ${INSTALL_PROJECTDBD:%${LIBVERSIONSTR}.dbd=%})
	$(SETLINKS) ${INSTALL_INCLUDE} .h $(notdir ${HDRS:%.h=%})
	$(SETLINKS) ${INSTALL_TEMPL} .template $(notdir ${TEMPLS:%.template=%})
	$(SETLINKS) ${INSTALL_TEMPL} .db $(notdir ${TEMPLS:%.db=%})

${INSTALL_INCLUDE}/%${LIBVERSIONSTR}.h: %.h
	@echo "Installing header file $@"
	$(RM) $@
	echo "#define __${PRJ}Lib__ ${MAJOR}.${MINOR}" > $@
	cat $^ >> $@
	chmod 444 $@
	$(SETLINKS) ${INSTALL_INCLUDE} .h $(basename $(notdir $^))

vpath %.db $(sort $(dir ${TEMPLS}))
vpath %.template $(sort $(dir ${TEMPLS}))
vpath % $(sort $(dir ${DOCU}))

else # in O.* directory, third run

# add sources for specific epics types (3.13 or 3.14) or architectures
ARCH_PARTS = ${T_A} $(subst -, ,${T_A}) ${OS_CLASS}
SRCS += $(foreach PART, ${ARCH_PARTS}, ${SRCS_${PART}})
SRCS += $(foreach PART, ${ARCH_PARTS}, ${SRCS_${EPICS_BASETYPE}_${PART}})
DBDFILES += $(foreach PART, ${ARCH_PARTS}, ${DBDFILES_${PART}})
DBDFILES += $(foreach PART, ${ARCH_PARTS}, ${DBDFILES_${EPICS_BASETYPE}_${PART}})

# Different settings required to build library in 3.13. and 3.14

ifeq (${EPICS_BASETYPE},3.13) # only 3.13 from here

PROJECTLIB = $(if ${LIBOBJS},${PRJ}Lib${LIBVERSIONSTR},)
# Convert sources to object code, skip .a and .o here
LIBOBJS += $(patsubst %,%.o,$(notdir $(basename $(filter-out %.o %.a,${SRCS}))))
# add all .a and .o with absolute path
LIBOBJS += $(filter /%.o /%.a,${SRCS})
# add all .a and .o with relative path, but prefix with ../
LIBOBJS += $(patsubst %,../%,$(filter-out /%,$(filter %.o %.a,${SRCS})))
LIBOBJS += ${LIBRARIES:%=${INSTALL_BIN}/%Lib}
LIBNAME = ${PROJECTLIB}

#add munched library for C++ code (does not work for Tornado 1)
ifneq ($(filter %.cc %.cpp %.C,${SRCS}),)
ifeq ($(filter T1-%,${T_A}),)
PROD = ${PROJECTLIB}.munch
endif # T1- T_A
endif # .cc or .cpp found

else # only 3.14 from here

ifeq (${OS_CLASS},vxWorks)
# only install the munched lib
INSTALL_PROD=
PROJECTLIB = $(if ${LIBOBJS},${PRJ}Lib${LIBVERSIONSTR}.munch,)
else
PROJECTLIB = $(if ${LIBOBJS},${LIB_PREFIX}${PRJ}${LIBVERSIONSTR}${SHRLIB_SUFFIX},)
endif

# vxWorks
PROD_vxWorks=${PROJECTLIB}
LIBOBJS += $(addsuffix $(OBJ),$(notdir $(basename $(filter-out %.o %.a,$(sort ${SRCS})))))
LIBOBJS += ${LIBRARIES:%=${INSTALL_BIN}/%Lib}
LIBS = -L ${EPICS_BASE_LIB} ${BASELIBS:%=-l%}
LINK.cpp += ${LIBS}
PRODUCT_OBJS = ${LIBOBJS}

# Linux
LOADABLE_LIBRARY=$(if ${LIBOBJS},${PRJ}${LIBVERSIONSTR},)
LIBRARY_OBJS = ${LIBOBJS}

# Handle registry stuff automagically if we have a dbd file.
# See ${REGISTRYFILE} and ${EXPORTFILE} rules below.
ifdef PROJECTDBD
LIBOBJS += $(addsuffix $(OBJ),$(basename ${REGISTRYFILE} ${EXPORTFILE}))
endif # PROJECTDBD

endif # both, 3.13 and 3.14 from here

# If we build a library and use versions, provide a version variable.
ifdef PROJECTLIB
ifdef LIBVERSION
LIBOBJS += $(addsuffix $(OBJ),$(basename ${VERSIONFILE}))
endif # LIBVERSION
endif # PROJECTLIB

ifdef LIBVERSION
ifndef TESTVERSION
# Provide a global symbol for every version with the same
# major and equal or smaller minor version number.
# OUTDATED: Other code using this will look for one of those symbols.
# NOT ANY MORE: Add an undefined symbol for the version of every used driver.
# This is done with the #define in the used headers (see below).
MAJOR_MINOR_PATCH=$(subst ., ,${LIBVERSION})
MAJOR=$(word 1,${MAJOR_MINOR_PATCH})
MINOR=$(word 2,${MAJOR_MINOR_PATCH})
PATCH=$(word 3,${MAJOR_MINOR_PATCH})
ALLMINORS := $(shell for ((i=0;i<=${MINOR};i++));do echo $$i;done)
PREREQUISITES = $(shell ${MAKEHOME}/getPrerequisites.tcl ${INSTALL_INCLUDE} | grep -vw ${PRJ})
ifeq (${OS_CLASS}, vxWorks)
PROVIDES = ${ALLMINORS:%=--defsym __${PRJ}Lib_${MAJOR}.%=0}
endif # vxWorks
ifeq (${OS_CLASS}, Linux)
PROVIDES = ${ALLMINORS:%=-Wl,--defsym,${PRJ}Lib_${MAJOR}.%=0}
endif # Linux
endif # TESTVERSION
endif # LIBVERSION defined

LDFLAGS += ${PROVIDES} ${USR_LDFLAGS_${T_A}}

# Create and include dependency files
CPPFLAGS += -MD
# 3.14.12 already defines -MDD here (what we don't want):
HDEPENDSCFLAGS =
-include *.d

# Setup searchpaths from all used files
vpath % ..
vpath % $(sort $(dir ${SRCS:%=../%}))
vpath %.h $(addprefix ../,$(sort $(dir $(filter-out /%,${HDRS})))) $(dir $(filter /%,${HDRS}))
vpath %.template $(sort $(dir ${TEMPLS:%=../%}))
vpath %.db $(sort $(dir ${TEMPLS:%=../%}))
vpath %.dbd $(sort $(dir ${DBDFILES:%=../%}))
#VPATH += $(sort $(dir ${DOCU:%=../%}))

DBDDIRS = $(sort $(dir ${DBDFILES:%=../%}))
DBDDIRS += ${INSTALL_DBD} ${EPICS_BASE}/dbd
DBDEXPANDPATH = $(addprefix -I , ${DBDDIRS})
USR_DBDFLAGS += $(DBDEXPANDPATH)

ifeq (${EPICS_BASETYPE},3.13)
USR_INCLUDES += $(addprefix -I, $(sort $(dir ${SRCS:%=../%} ${HDRS:%=../%})))

else # 3.14

# different macros for 3.14.12 and earlier versions
SRC_INCLUDES = $(addprefix -I, $(sort $(dir ${SRCS:%=../%} ${HDRS:%=../%})))
GENERIC_SRC_INCLUDES = $(SRC_INCLUDES)

EXPANDARG = -3.14
ifneq ($(words $(filter %.c %.cc %.C %.cpp, $(SRCS))),0)
DBDFILES+=${SUBFUNCFILE}
endif

# snc location in 3.14
#-include ${SNCSEQ}/configure/RULES_BUILD # incompatible to 3.15
SNC=${SNCSEQ}/bin/$(EPICS_HOST_ARCH)/snc
SNC_CFLAGS=-I ${SNCSEQ}/include

endif # 3.14

${BUILDRULE} PROJECTINFOS $(addprefix ${COMMON_DIR}/,$(addsuffix Record.h,${RECORDS})) ${PROJECTDBD} ${DEPFILE}

PROJECTINFOS:
	@echo ${PRJ} > PROJECTNAME
	@echo ${INSTBASE} > INSTBASE
	@echo ${PROJECTLIB} ${PROJECTDBD} ${DEPFILE} > PRODUCTS

# Build one dbd file by expanding all source dbd files.
# We can't use dbExpand (from the default EPICS make rules)
# because it has too strict checks for a loadable module.
${PROJECTDBD}: ${DBDFILES}
	@echo "Expanding $@"
	${MAKEHOME}/expandDBD.tcl ${EXPANDARG} ${DBDEXPANDPATH} $^ > $@

# Install everything and set up symbolic links
ifeq (${EPICS_BASETYPE},3.14)
${INSTALL_BIN}/${PROJECTLIB}: ${PROJECTLIB}
	@echo "Installing library $@"
	$(RM) $@
	cp $^ $@
	chmod 444 $@
ifeq (${OS_CLASS},vxWorks)
	$(SETLINKS) ${INSTALL_BIN} .munch ${PRJ}Lib
else
	$(SETLINKS) ${INSTALL_BIN} ${SHRLIB_SUFFIX} ${LIB_PREFIX}${PRJ}
endif
else
${INSTALL_BIN}/${PROJECTLIB}.munch: ${PROJECTLIB}.munch
	@echo "Installing munched library $@"
	$(RM) $@
	cp $^ $@
	chmod 444 $@
	$(SETLINKS) ${INSTALL_BIN} .munch ${PRJ}Lib

${INSTALL_BIN}/${PROJECTLIB}: ${PROJECTLIB}
	@echo "Installing library $@"
	$(RM) $@
	cp $^ $@
	chmod 444 $@
	$(SETLINKS) ${INSTALL_BIN} "" ${PRJ}Lib
endif

${INSTALL_BIN}/${DEPFILE}: ${DEPFILE}
	@echo "Installing dependency file $@"
	$(RM) $@
	cp $^ $@
	chmod 444 $@
	$(SETLINKS) ${INSTALL_BIN} .dep ${PRJ}

${INSTALL_DBD}/%.dbd: %.dbd
	@echo "Installing dbd file $@"
	$(RM) $@
	cp $^ $@
	chmod 444 $@
	$(SETLINKS) ${INSTALL_DBD} .dbd ${PRJ}

# Add a #define so that users of the header know the version.
${INSTALL_INCLUDE}/%${LIBVERSIONSTR}.h: %.h
	@echo "Installing header file $@"
	$(RM) $@
	echo "#define __${PRJ}Lib__ ${MAJOR}.${MINOR}" > $@
	cat $^ >> $@
	chmod 444 $@
	$(SETLINKS) ${INSTALL_INCLUDE} .h $(basename $(notdir $^))


# Include default EPICS Makefiles (version dependent)
# avoid library installation when doing 'make build'
INSTALL_LOADABLE_SHRLIBS=
include ${BASERULES}

#Fix release rules
RELEASE_DBDFLAGS = -I ${EPICS_BASE}/dbd
RELEASE_INCLUDES = -I${EPICS_BASE}/include 
RELEASE_INCLUDES += -I${EPICS_BASE}/include/compiler/${CMPLR_CLASS}
RELEASE_INCLUDES += -I${EPICS_BASE}/include/os/${OS_CLASS}

#relax 3.13 cross compilers (default is STRICT)
CMPLR=ANSI
CXXCMPLR=ANSI
G++_ANSI = $(G++) -ansi

${INSTALLRULE} ${INSTALL_DOCUS} ${INSTALL_PROJECTDBD} ${INSTALL_LIBRARY} ${INSTALL_DEP}

# Create SNL code from st/stt file
# (RULES.Vx only allows ../%.st, 3.14 has no .st rules at all)
# Important to have %.o: %.st and %.o: %.stt rule before %.o: %.c rule!
# Preprocess in any case because docu and EPICS makefiles mismatch here

CPPSNCFLAGS1  = $(filter -D%, ${OP_SYS_CFLAGS})
CPPSNCFLAGS1 += $(filter-out ${OP_SYS_INCLUDE_CPPFLAGS} ,${CPPFLAGS}) ${CPPSNCFLAGS}
SNCFLAGS += -r

%$(OBJ) %_snl.dbd: %.st
	@echo "Preprocessing $*.st"
	$(RM) $(*F).i
	$(CPP) ${CPPSNCFLAGS1} $< > $(*F).i
	@echo "Converting $(*F).i"
	$(RM) $@
	$(SNC) $(TARGET_SNCFLAGS) $(SNCFLAGS) $(*F).i
	@echo "Compiling $(*F).c"
	$(RM) $@
	$(COMPILE.c) ${SNC_CFLAGS} $(*F).c
	@echo "Building $(*F)_snl.dbd"
	awk -F '[ ;]' '/extern struct seqProgram/ { print "registrar (" $$4 "Registrar)"}' $(*F).c > $(*F)_snl.dbd

%$(OBJ) %_snl.dbd: %.stt
	@echo "Preprocessing $*.stt"
	$(RM) $(*F).i
	$(CPP) ${CPPSNCFLAGS1} $< > $(*F).i
	@echo "Converting $(*F).i"
	$(RM) $@
	$(SNC) $(TARGET_SNCFLAGS) $(SNCFLAGS) $(*F).i
	@echo "Compiling $(*F).c"
	$(RM) $@
	$(COMPILE.c) ${SNC_CFLAGS} $(*F).c
	@echo "Building $(*F)_snl.dbd"
	awk -F '[ ;]' '/extern struct seqProgram/ { print "registrar (" $$4 "Registrar)"}' $(*F).c > $(*F)_snl.dbd

# Create GPIB code from gt file
%.c %.dbd %.list: %.gt
	@echo "Converting $*.gt"
	${LN} $< $(*F).gt
	gdc $(*F).gt

# Create dbd file with references to all subRecord functions
${SUBFUNCFILE}: $(filter %.c %.cc %.C %.cpp, $(SRCS))
	awk '/^[\t ]*static/ {next} /\([\t ]*(struct)?[\t ]*(genSub|sub|aSub)Record[\t ]*\*[\t ]*\w+[\t ]*\)/ {\
		match ($$0,/(\w+)[\t ]*\([\t ]*(struct)?[\t ]*\w+Record[\t ]*\*[\t ]*\w+[\t ]*\)/, a);\
		n=a[1];if(!f[n]){f[n]=1;print "function (" n ")"}\
	}' $^ > $@

# The original 3.13 munching rule does not really work well
ifeq (${EPICS_BASETYPE},3.13)
MUNCH=tclsh $(VX_DIR)/host/src/hutils/munch.tcl
%.munch: CMPLR=TRAD
%.munch: %
	@echo Munching $<
	@ $(RM) ctct.o ctdt.c
	$(NM) $< | $(MUNCH) > ctdt.c
	$(COMPILE.c) ctdt.c
	$(LINK.c) $@ $< ctdt.o
endif

${VERSIONFILE}:
ifndef TESTVERSION
	echo "double _${PRJ}LibVersion = ${MAJOR}.${MINOR};" > $@
endif
	echo "char _${PRJ}LibRelease[] = \"${LIBVERSION}\";" >> $@

# EPICS R3.14.*:
# Create file to fill registry from dbd file.
${REGISTRYFILE}: ${PROJECTDBD}
	$(RM) $@ temp.cpp
	$(PERL) $(EPICS_BASE_HOST_BIN)/registerRecordDeviceDriver.pl $< $(basename $@) > temp.cpp
	$(MV) temp.cpp $@

# 3.14.12 complains if this rule is not overwritten
./%Include.dbd:

# For 3.13 code used with 3.14:
# Add missing epicsExportAddress() calls for registry.

define makexportfile
BEGIN { print "/* This is a generated file. Do not modify! */"; \
	print "#include <drvSup.h>"; \
	print "#include <devSup.h>"; \
	print "#include <recSup.h>"; \
	print "#include <registryFunction.h>"; \
	print "#include <epicsExport.h>"; \
	print "/* These are the RegisterFunction and ExportAddress calls missing for 3.14 compatible code. */"; \
      } \
/ U pvar_func_register_func_/ {name=substr($$2,25); func_missing[name]=1; next;} \
/ [A-Z] pvar_func_register_func_/ {name=substr($$3,25); func_found[name]=1; next;} \
/ U pvar_func_/ {name=substr($$2,11); reg_missing[name]=1; next;} \
/ [A-Z] pvar_func_/ {name=substr($$3,11); reg_found[name]=1; next;} \
/ U pvar_/ {i=index(substr($$2,6),"_"); type=substr($$2,6,i-1); name=substr($$2,i+6); var_missing[name]=type; next;} \
/ [A-Z] pvar_/ {i=index(substr($$3,6),"_"); name=substr($$3,i+6); var_found[name]=1; next;} \
END {for (name in func_missing) if (!func_found[name]) { \
	print "void " name "();"; \
	print "epicsRegisterFunction(" name ");"} \
     for (name in reg_missing) if (!reg_found[name]) { \
	print "extern REGISTRYFUNCTION " name ";"; \
	print "epicsExportRegistrar(" name ");"} \
     for (name in var_missing) if (!var_found[name]) { \
        type = var_missing[name]; \
	print "extern " type " " name ";"; \
	print "epicsExportAddress(" type ", " name ");"} \
    }
endef
 
CORELIB = ${CORELIB_${OS_CLASS}}
CORELIB_vxWorks = ${EPICS_BASE}/bin/${T_A}/$(if $(filter 3.15.% ,$(EPICSVERSION)),softIoc.munch,iocCoreLibrary.munch)
 
ifeq (${OS_CLASS},vxWorks)
SHARED_LIBRARIES=NO
endif
LSUFFIX_YES=$(SHRLIB_SUFFIX)
LSUFFIX_NO=$(LIB_SUFFIX)
LSUFFIX=$(LSUFFIX_$(SHARED_LIBRARIES))
 
${EXPORTFILE}: $(filter-out $(basename ${EXPORTFILE})$(OBJ),${LIBOBJS})
	$(RM) $@
	$(NM) $^ ${BASELIBS:%=${EPICS_BASE}/lib/${T_A}/${LIB_PREFIX}%$(LSUFFIX)} ${CORELIB} | awk '$(makexportfile)' > $@

# Create dependency file for recursive requires
${DEPFILE}: ${LIBOBJS}
	@echo "Collecting dependencies"
	$(RM) $@
	@echo "# Generated file. Do not edit." > $@
	${MAKEHOME}/getPrerequisites.tcl -dep ${INSTALL_INCLUDE} | grep -vw ${PRJ} >> $@; true

$(BUILDRULE)
	$(RM) MakefileInclude

endif # in O.* directory
endif # T_A defined
endif # OS_CLASS in BUILDCLASSES
endif # EPICSVERSION defined
