#include "common/setup_before.h"
#include <stdio.h>
#ifdef HAVE_STDDEF_H
# include <stddef.h>
#else
# ifndef NULL
#  define NULL ((void *)0)
# endif
#endif
#ifdef STDC_HEADERS
# include <stdlib.h>
#else
# ifdef HAVE_MALLOC_H
#  include <malloc.h>
# endif
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif
#include "compat/strrchr.h"
#include "compat/strdup.h"
#include "compat/strcasecmp.h"
#include "compat/strncasecmp.h"
#include "compat/pdir.h"
#include <errno.h>
#include "compat/strerror.h"
#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#include "common/eventlog.h"
#include "common/packet.h"
#include "common/bnet_protocol.h"
#include "common/tag.h"
#include "common/util.h"
#include "common/bnettime.h"
#include "common/eventlog.h"
#include "common/list.h"
#include "common/proginfo.h"
#include "common/bn_type.h"
#include "common/xalloc.h"
#include "connection.h"
#include "prefs.h"
#include "storage.h"
#include "server.h"

int bnetd_log(const char* acctname, const char* ipaddr, const char* opt, const char* type, const char* addi, unsigned int ifsucc, unsigned int versionid){
	return storage->bnetd_log(acctname, ipaddr, opt, type, addi, ifsucc, versionid);
}