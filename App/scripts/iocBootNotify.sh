#!/bin/bash
#
# This script should only be called by the ioc at boot time.
# The startup script should have the following line:
# rsh bootHost(),"cd",SLSBASE,";sls/bin/iocBootNotice.sh",bootInfo("TendFs"),vxWorksVersion,"'",epicsRelease1,"'"
# or
# bootNotice SLSBASE,"sls/bin/iocBootNotice.sh"

if [ $# -lt 9 ]
then
    echo "This script should only be called by an IOC at boot time!" >&2
    exit 1
fi

#. /etc/profile

SYSTEM=$1
IPADDR=$2
PROCNUM=$3
DEVICE=$4
BOOTFILE=$5
SCRIPT=$6
VXWORKSVER=$7
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

$SLSBASE/sls/bin/call_ioc_ins $SYSTEM $IPADDR $PROCNUM $DEVICE $BOOTPC $SLSBASE $BOOTFILE $SCRIPT $VXWORKS $EPICSVER $VXWORKSVER $ETHADDR

