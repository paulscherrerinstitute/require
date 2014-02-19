#ifndef INC_epicsEndian_H
#define INC_epicsEndian_H

#define EPICS_ENDIAN_LITTLE   1234
#define EPICS_ENDIAN_BIG      4321

#if defined (vxWorks)

#include <types/vxArch.h>

#if _BYTE_ORDER == _LITTLE_ENDIAN
#   define EPICS_BYTE_ORDER EPICS_ENDIAN_LITTLE
#elif _BYTE_ORDER == _BIG_ENDIAN
#   define EPICS_BYTE_ORDER EPICS_ENDIAN_BIG
#else
#   error EPICS hasnt been ported to _BYTE_ORDER specified by vxWorks <types/vxArch.h>
#endif

/* for now, assume that vxWorks doesnt run on weird arch like ARM NWFP */
#define EPICS_FLOAT_WORD_ORDER EPICS_BYTE_ORDER

#elif defined (_WIN32)

/* for now, assume that win32 runs only on generic little endian */
#define EPICS_BYTE_ORDER EPICS_ENDIAN_LITTLE
#define EPICS_FLOAT_WORD_ORDER EPICS_BYTE_ORDER

#else

/* if compilation fails because this wasnt found then you may need to define an OS
   specific osdWireConfig.h */

#include <sys/param.h>

#ifdef __BYTE_ORDER
#   if __BYTE_ORDER == __LITTLE_ENDIAN
#       define EPICS_BYTE_ORDER EPICS_ENDIAN_LITTLE
#   elif __BYTE_ORDER == __BIG_ENDIAN
#       define EPICS_BYTE_ORDER EPICS_ENDIAN_BIG
#   else
#       error EPICS hasnt been ported to run on the <sys/param.h> specified __BYTE_ORDER
#   endif
#else
#   ifdef BYTE_ORDER
#       if BYTE_ORDER == LITTLE_ENDIAN
#           define EPICS_BYTE_ORDER EPICS_ENDIAN_LITTLE
#       elif BYTE_ORDER == BIG_ENDIAN
#           define EPICS_BYTE_ORDER EPICS_ENDIAN_BIG
#       else
#           error EPICS hasnt been ported to run on the <sys/param.h> specified BYTE_ORDER
#       endif
#   else
#       error <sys/param.h> doesnt specify __BYTE_ORDER or BYTE_ORDER - is an OS specific osdWireConfig.h needed?
#   endif
#endif

#ifdef __FLOAT_WORD_ORDER
#   if __FLOAT_WORD_ORDER == __LITTLE_ENDIAN
#       define EPICS_FLOAT_WORD_ORDER EPICS_ENDIAN_LITTLE
#   elif __FLOAT_WORD_ORDER == __BIG_ENDIAN
#       define EPICS_FLOAT_WORD_ORDER EPICS_ENDIAN_BIG
#   else
#       error EPICS hasnt been ported to <sys/param.h> specified __FLOAT_WORD_ORDER
#   endif
#else
#    ifdef FLOAT_WORD_ORDER
#       if FLOAT_WORD_ORDER == LITTLE_ENDIAN
#           define EPICS_FLOAT_WORD_ORDER EPICS_ENDIAN_LITTLE
#       elif FLOAT_WORD_ORDER == BIG_ENDIAN
#           define EPICS_FLOAT_WORD_ORDER EPICS_ENDIAN_BIG
#       else
#           error EPICS hasnt been ported to <sys/param.h> specified FLOAT_WORD_ORDER
#       endif
#   else
        /* assume that if neither __FLOAT_WORD_ORDER nor FLOAT_WORD_ORDER are
           defined then weird fp ordered archs like arm nwfp aren't supported */
#       define EPICS_FLOAT_WORD_ORDER EPICS_BYTE_ORDER
#   endif
#endif

#endif

#endif /* INC_epicsEndian_H */
