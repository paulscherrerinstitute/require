#!/bin/bash
#
# This script should only be called by the ioc at boot time.
# The startup script should have the following line:
# bootNotify SLSBASE,"sls/bin/iocBootNotify.sh"

if [ "$1" = "-v" ]
then
    echo '$Source: /cvs/G/DRV/misc/App/scripts/iocBootNotify.sh,v $'
    echo '$Author: zimoch $'
    echo '$Date: 2004/08/02 11:36:43 $'
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
IPADDR=$2
PROCNUM=$3
DEVICE=$4
BOOTFILE=$5
SCRIPT=$6
VXWORKSVER=${7#VxWorks}
EPICSVER=$8
ETHADDR=$9

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
    ( * ) echo "ERROR: $SYSTEM is not an acceptable system name."
          echo "Rename your system and 'target name' to match *-VME*."
          exit 1 ;;
esac
link=$(readlink /ioc/$SYSTEM)
SLSBASE=${link%%/iocBoot*}
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

if [ -d /usr/oracle-9.2 ] ; then
        export ORACLE_HOME=/usr/oracle-9.2
else
        export ORACLE_HOME=/usr/oracle-8.1.7
	export LD_LIBRARY_PATH=$ORACLE_HOME/lib:$LD_LIBRARY_PATH
fi

$ORACLE_HOME/bin/sqlplus -s ssrm_public/pub01@psip0 << EOF
INSERT INTO SSRM.IOC_BOOTLOG
       (SYSTEM, IPADDR, PROCNUM, DEVICE, BOOTPC,
        SLSBASE, BOOTFILE, SCRIPT, VXWORKS, EPICSVER,
        VXWORKSVER, ETHADDR)
VALUES ('$SYSTEM', '$IPADDR', '$PROCNUM', '$DEVICE', '$BOOTPC',
        '$SLSBASE', '$BOOTFILE', '$SCRIPT', '$VXWORKS', '$EPICSVER',
        '$VXWORKSVER', '$ETHADDR')
EXIT
EOF
# $Name:  $
# $Id: iocBootNotify.sh,v 1.6 2004/08/02 11:36:43 zimoch Exp $
# $Source: /cvs/G/DRV/misc/App/scripts/iocBootNotify.sh,v $
# $Revision: 1.6 $
