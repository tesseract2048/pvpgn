/*
 * Copyright (C) 2001  Dizzy 
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#ifndef INCLUDED_PDIR_TYPES
#define INCLUDED_PDIR_TYPES

#ifdef PDIR_INTERNAL_ACCESS

#ifdef JUST_NEED_TYPES
# ifdef HAVE_DIRENT_H
#  include <dirent.h>
# else
#  ifdef HAVE_SYS_NDIR_H
#   include <sys/ndir.h>
#  endif
#  if HAVE_SYS_DIR_H
#   include <sys/dir.h>
#  endif
#  if HAVE_NDIR_H
#   include <ndir.h>
#  endif
#  define dirent direct
# endif
# ifdef WIN32
#  include <io.h> /* for _findfirst(), _findnext(), etc */
# endif
#else
# define JUST_NEED_TYPES
# ifdef HAVE_DIRENT_H
#  include <dirent.h>
# else
#  ifdef HAVE_SYS_NDIR_H
#   include <sys/ndir.h>
#  endif
#  if HAVE_SYS_DIR_H
#   include <sys/dir.h>
#  endif
#  if HAVE_NDIR_H
#   include <ndir.h>
#  endif
#  define dirent direct
# endif
# ifdef WIN32
#  include <io.h> /* for _findfirst(), _findnext(), etc */
# endif
# undef JUST_NEED_TYPES
#endif

#endif

typedef struct pdir_struct
#ifdef PDIR_INTERNAL_ACCESS
{
   char const *       path;
   char const *       lastret;
#ifdef WIN32
   long               lFindHandle;
   struct _finddata_t fileinfo;
   int                status; /* -1 == failure, 0 == freshly opened, 1 == opened and read, 2 == eof */
#else /* POSIX */
   DIR *              dir;
#endif
}
#endif
t_pdir;

#endif

#ifndef JUST_NEED_TYPES
#ifndef INCLUDED_PDIR_PROTOS
#define INCLUDED_PDIR_PROTOS

extern t_pdir * p_opendir(const char * dirname);
extern int p_rewinddir(t_pdir * pdir);
extern char const * p_readdir(t_pdir * pdir);
extern int p_closedir(t_pdir * pdir);

#endif

#endif
