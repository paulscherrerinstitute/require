# Accept SLSBASE or INSTBASE
PBASE=0
symFindByName(sysSymTbl,"INSTBASE",&PBASE,0)
symFindByName(sysSymTbl,"SLSBASE",&PBASE,0)
SLSBASE=*PBASE
INSTBASE=*PBASE

# Router for SLS machine net (fails otherwise but that's ok)
routeAdd ("0.0.0.0","172.20.10.1")

# setup mount points (try both possible locations for /ioc)
hostAdd ("slsfs","129.129.145.52")
nfsMount (&sysBootHost, "/home/ioc", "/home/ioc")
nfsMount (&sysBootHost, "/ioc", "/ioc")
nfsMount ("slsfs", "/export/csa/releases/ioc", "/ioc")
nfsMount ("slsfs","/export/csa/releases/work","/work")
nfsMount ("slsfs","/export/csa/releases/prod","/prod")
nfsMount ("slsfs","/export/csa/releases/devl","/devl")
nfsMount ("slsfs","/export/exchange","/exchange")

# overwrite driver module location
EPICS_MODULES = "/work/iocBoot/modules"
