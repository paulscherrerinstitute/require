#!/bin/bash
#
# This script should only be called by the ioc at boot time.
# The startup script should have the following line:
# bootNotify SLSBASE,"sls/bin/iocBootNotify.sh"

if [ "$1" = "-v" ]
then
    echo '$Source: /cvs/G/DRV/misc/App/scripts/iocBootNotify.sh,v $'
    echo '$Author: maden $'
    echo '$Date: 2004/07/22 14:25:30 $'
    exit
fi

if [ "$1" = "-h" ] || [ "$1" = "-?" ]
then
    echo "Usage:" >&2
    echo "        iocBootNotify.sh       \\" >&2
    echo "                  <system>     \\" >&2
    echo "                  <ipaddr>     \\" >&2
    echo "                  <procnum>    \\" >&2
    echo "                  <device>     \\" >&2
    echo "                  <bootfile>   \\" >&2
    echo "                  <script>     \\" >&2
    echo "                  <vxworksver> \\" >&2
    echo "                  <epicsver>   \\" >&2
    echo "                  <ethaddr>"       >&2
    exit 0
fi

if [ $# -lt 9 ] || [ "$1" = "-h" ] || [ "$1" = "-?" ]
then
    echo "This script should only be called by an IOC at boot time!" >&2
    echo "It needs 9 arguments." >&2
    exit 1
fi

#. /etc/profile

SYSTEM=$1
IPADDR=$2
PROCNUM=$3
DEVICE=$4
BOOTFILE=$5
SCRIPT=$6
VXWORKSVER=${7#VxWorks}
EPICSVER=$8
ETHADDR=$9
if [ ! -L /ioc/$SYSTEM ]
then
  echo "ERROR: $SYSTEM is not an existing system name."
  echo "Rename 'target name' to your system name!"
  exit 1
fi
case $SYSTEM in
    ( *-VME-* ) ;;
    ( * ) echo "ERROR: $SYSTEM is not an acceptable system name."
          echo "Rename your system and 'target name' to match *-VME-*."
          exit 1 ;;
esac
link=$(readlink /ioc/$SYSTEM)
SLSBASE=${link%%/iocBoot*}
BOOTPC=$(hostname -s)
if [ -L $BOOTFILE ]
then
  link=$(readlink $BOOTFILE)
  VXWORKS=$SLSBASE/${link##*../}
else
  VXWORKS=$BOOTFILE
fi

echo "I will put the following values to the database:"
echo "SYSTEM=$SYSTEM"
echo "IPADDR=$IPADDR"
echo "PROCNUM=$PROCNUM"
echo "DEVICE=$DEVICE"
echo "BOOTPC=$BOOTPC"
echo "SLSBASE=$SLSBASE"
echo "BOOTFILE=$BOOTFILE"
echo "SCRIPT=$SCRIPT"
echo "VXWORKS=$VXWORKS"
echo "EPICSVER=$EPICSVER"
echo "VXWORKSVER=$VXWORKSVER"
echo "ETHADDR=$ETHADDR"

$SLSBASE/sls/bin/call_ioc_ins $SYSTEM $IPADDR $PROCNUM $DEVICE \
             $BOOTPC $SLSBASE $BOOTFILE $SCRIPT $VXWORKS $EPICSVER \
             $VXWORKSVER $ETHADDR
exit
#--------------------------------------------------#
# emacs setup - force text mode to prevent emacs   #
#               from helping with the indentation! #
# Local Variables:                                 #
# mode:text                                        #
# indent-tabs-mode:nil                             #
# End:                                             #
#--------------------------------------------------#
#
#---------------------------------------- End of $RCSfile: iocBootNotify.sh,v $
