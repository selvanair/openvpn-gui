/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Maximum number of auto connect config files supported. */
#define MAX_AUTO_CONNECT 50

/* Name of package */
#define PACKAGE "openvpn-gui"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "openvpn-devel@lists.sourceforge.net"

/* Define to the full name of this package. */
#define PACKAGE_NAME "OpenVPN GUI"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "OpenVPN GUI 11"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "openvpn-gui"

/* Define to the home page for this package. */
#define PACKAGE_URL "https://github.com/openvpn/openvpn-gui/"

/* Define to the version of this package. */
#define PACKAGE_VERSION "11"

/* Version in windows resource format */
#define PACKAGE_VERSION_RESOURCE 11,0,0,0

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Enable extensions on AIX 3, Interix.  */
#ifndef _ALL_SOURCE
# define _ALL_SOURCE 1
#endif
/* Enable GNU extensions on systems that have them.  */
#ifndef _GNU_SOURCE
# define _GNU_SOURCE 1
#endif
/* Enable threading extensions on Solaris.  */
#ifndef _POSIX_PTHREAD_SEMANTICS
# define _POSIX_PTHREAD_SEMANTICS 1
#endif
/* Enable extensions on HP NonStop.  */
#ifndef _TANDEM_SOURCE
# define _TANDEM_SOURCE 1
#endif
/* Enable general extensions on Solaris.  */
#ifndef __EXTENSIONS__
# define __EXTENSIONS__ 1
#endif


/* Version number of package */
#define VERSION "11"

/* Define to 1 if on MINIX. */
/* #undef _MINIX */

/* Define to 2 if the system does not provide POSIX.1 features except with
   this defined. */
/* #undef _POSIX_1_SOURCE */

/* Define to 1 if you need to in order for `stat' and other things to work. */
/* #undef _POSIX_SOURCE */

// Add in needed header files for MSVC (needed prior to windows.h)
#include <WinSock2.h>
#include <WS2tcpip.h>
