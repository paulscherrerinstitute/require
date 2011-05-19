#!/bin/bash
#
# This script should only be called by the ioc at boot time.
# The startup script should have the following line:
# bootNotify SLSBASE,"sls/bin/iocBootNotify.sh"

PATH=$PATH:/bin:/usr/bin
. /etc/profile

if [ "$1" = "-v" ]
then
    echo '$Source: /cvs/G/DRV/misc/App/scripts/iocBootNotify.sh,v $'
    echo '$Author: lutz_h $'
    echo '$Date: 2011/05/19 12:31:26 $'
    exit
fi

if [ "$1" = "-h" ] || [ "$1" = "-?" ]
then
{
    echo "Usage:"
    echo "        iocBootNotify.sh       \\"
    echo "                  <system>     \\"
    echo "                  <ipaddr>     \\"
    echo "                  <procnum>    \\"
    echo "                  <device>     \\"
    echo "                  <bootfile>   \\"
    echo "                  <script>     \\"
    echo "                  <vxworksver> \\"
    echo "                  <epicsver>   \\"
    echo "                  <ethaddr>"
} >&2
    exit 0
fi

SYSTEM=$1
IPADDR=${2/-/$(hostname -i)}
PROCNUM=${3#*:}
DEVICE=${4/-/eth0}
BOOTFILE=$5
SCRIPT=$6
VXWORKSVER=${7#VxWorks}
EPICSVER=${8#R}
ETHADDR=${9/-/$(/sbin/ifconfig eth0 | awk '/HWaddr/ {print $5}')}

if [ $# -lt 9 ]
then
{
    echo "This script should only be called by an IOC at boot time!"
    echo "It needs 9 arguments."
    echo "got: SYSTEM=\"$SYSTEM\""
    echo "     IPADDR=\"$IPADDR\""
    echo "     PROCNUM=\"$PROCNUM\""
    echo "     DEVICE=\"$DEVICE\""
    echo "     BOOTFILE=\"$BOOTFILE\""
    echo "     SCRIPT=\"$SCRIPT\""
    echo "     VXWORKSVER=\"$VXWORKSVER\""
    echo "     EPICSVER=\"$EPICSVER\""
    echo "     ETHADDR=\"$ETHADDR\""
}  >&2
    exit 1
fi

BOOTPC=$(hostname -s)

if [ ! -L /ioc/$SYSTEM ]
then
  echo "ERROR: No symbolic link /ioc/$SYSTEM on $BOOTPC."
  echo "Rename 'target name' to your system name!"
  exit 1
fi
case $SYSTEM in
    ( *-VME* ) ;;
    ( *-CV* ) ;;
    ( *-PC* ) ;;
    ( *-CP* ) ;;
    ( * ) echo "ERROR: $SYSTEM is not an acceptable system name."
          echo "Rename your system and 'target name' to match *-VME* or *-CV*."
          exit 1 ;;
esac
link=$(readlink /ioc/$SYSTEM)
SLSBASE=${link%%/iocBoot*}
if [ -L $BOOTFILE ]
then
  link=$(readlink $BOOTFILE)
  tail=${link#../../}
  if [ $tail = $link ]
  then
    VXWORKS=$BOOTFILE
  else
    VXWORKS=$SLSBASE/iocBoot/$tail
  fi
else
  VXWORKS=$BOOTFILE
fi

if [ "$7" = "-" ]
then
VXWORKSVER=NULL
VXWORKS=NULL
else
VXWORKSVER="'$VXWORKSVER'"
VXWORKS="'$VXWORKS'"
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

if [ -z "$ORACLE_HOME" ] ; then
    echo "ORACLE_HOME not defined" >&2
    exit 1
fi

sqlplus -s gfa_public/pub01@GFAPRD << EOF &
INSERT INTO HOSTS.IOC_BOOTLOG
       (SYSTEM, IPADDR, PROCNUM, DEVICE, BOOTPC,
        SLSBASE, BOOTFILE, SCRIPT, VXWORKS, EPICSVER,
        VXWORKSVER, ETHADDR)
VALUES ('$SYSTEM', '$IPADDR', '$PROCNUM', '$DEVICE', '$BOOTPC',
        '$SLSBASE', '$BOOTFILE', '$SCRIPT', $VXWORKS, '$EPICSVER',
        $VXWORKSVER, '$ETHADDR');
EXIT
EOF
# $Name:  $
# $Id: iocBootNotify.sh,v 1.20 2011/05/19 12:31:26 lutz_h Exp $
# $Source: /cvs/G/DRV/misc/App/scripts/iocBootNotify.sh,v $
# $Revision: 1.20 $
