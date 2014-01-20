/*
 * Copyright (C) 2000 Onlyer (onlyer@263.net)
 * Copyright (C) 2001 Ross Combs (ross@bnetd.org)
 * Copyright (C) 2002 Gianluigi Tiesi (sherpya@netfarm.it)
 * Copyright (C) 2004 CreepLord (creeplord@pvpgn.org)
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
#define VERSIONCHECK_INTERNAL_ACCESS
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
#include "compat/strtoul.h"
#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif
#ifdef HAVE_MKTIME
# ifdef TIME_WITH_SYS_TIME
#  include <sys/time.h>
#  include <time.h>
# else
#  ifdef HAVE_SYS_TIME_H
#   include <sys/time.h>
#  else
#   include <time.h>
#  endif
# endif
#endif
#ifdef HAVE_ASSERT_H
# include <assert.h>
#endif
#include "compat/strchr.h"ha
#include "compat/strdup.h"
#include "compat/strcasecmp.h"
#include <ctype.h>
#include <errno.h>
#include "compat/strerror.h"
#include "common/eventlog.h"
#include "common/list.h"
#include "common/util.h"
#include "common/proginfo.h"
#include "common/token.h"
#include "common/field_sizes.h"
#include "prefs.h"
#include "versioncheck.h"
#include "common/tag.h"
#include "common/xalloc.h"
#include "common/setup_after.h"


static t_list * versioninfo_head=NULL;
static t_versioncheck dummyvc={ "A=42 B=42 C=42 4 A=A^S B=B^B C=C^C A=A^S", "IX86ver1.mpq", "NoVC" };

static int versioncheck_compare_exeinfo(t_parsed_exeinfo * pattern, t_parsed_exeinfo * match);

void versioncheck_create_eqn(char* ccode){
	int ix;
	for(ix = 0; ix < 15; ix++){
		ccode[ix] = 65 + (rand() % 26);
	}
	ccode[15] = '\0';
}

void versioncheck_hash_eqn(char * hashrtn, char const *eqn){
	char const * hashkey = prefs_get_eqn_hashstr();
	int i, j = 0;
	int buf = 0;
	for(i = 0; i < (int)strlen(eqn) ; i ++){
		buf = (eqn[i] - 65) ^ (hashkey[i] - 65);
		if(buf > 25) buf = buf + 10;
		hashrtn[j] = buf + 65;
		j++;
	}
	hashrtn[j] = 0;
}

extern t_versioncheck * versioncheck_create(t_tag archtag, t_tag clienttag)
{
    t_elem const *   curr;
    t_versioninfo *  vi;
    t_versioncheck * vc;
    char             archtag_str[5];
    char             clienttag_str[5];
    
    LIST_TRAVERSE_CONST(versioninfo_head,curr)
    {
        if (!(vi = elem_get_data(curr))) /* should not happen */
        {
            eventlog(eventlog_level_error,__FUNCTION__,"version list contains NULL item");
            continue;
        }
	
	eventlog(eventlog_level_debug,__FUNCTION__,"version check entry archtag=%s, clienttag=%s",
	    tag_uint_to_str(archtag_str,vi->archtag),
	    tag_uint_to_str(clienttag_str,vi->clienttag));
	
	if (vi->archtag != archtag)
	    continue;
	if (vi->clienttag != clienttag)
	    continue;
	if (strcmp(archtag_str, "IX86") != 0)
		continue;

	/* FIXME: randomize the selection if more than one match */
	vc = xmalloc(sizeof(t_versioncheck));
	//if (strcmp(vi->mpqfile, "IX86ver1.mpq") == 0){
	//vc->eqn = xmalloc(16);
	//versioncheck_create_eqn(vc->eqn);
	//}else{
	vc->eqn = xstrdup(vi->eqn);
	//}
	vc->mpqfile = xstrdup(vi->mpqfile);
	vc->versiontag = xstrdup(tag_uint_to_str(clienttag_str,clienttag));

	return vc;
    }
    
    /*
     * No entries in the file that match, return the dummy because we have to send
     * some equation and auth mpq to the client.  The client is not going to pass the
     * validation later unless skip_versioncheck or allow_unknown_version is enabled.
     */
    return &dummyvc;
}


extern int versioncheck_destroy(t_versioncheck * vc)
{
    if (!vc)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL vc");
	return -1;
    }
    
    if (vc==&dummyvc)
	return 0;
    
    xfree((void *)vc->versiontag);
    xfree((void *)vc->mpqfile);
    xfree((void *)vc->eqn);
    xfree(vc);
    
    return 0;
}

extern int versioncheck_set_versiontag(t_versioncheck * vc, char const * versiontag)
{
    if (!vc) {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL vc");
	return -1;
    }
    if (!versiontag) {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL versiontag");
	return -1;
    }
    
    if (vc->versiontag!=NULL) xfree((void *)vc->versiontag);
    vc->versiontag = xstrdup(versiontag);
    return 0;
}


extern char const * versioncheck_get_versiontag(t_versioncheck const * vc)
{
    if (!vc) {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL vc");
	return NULL;
    }
    
    return vc->versiontag;
}


extern char const * versioncheck_get_mpqfile(t_versioncheck const * vc)
{
    if (!vc)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL vc");
	return NULL;
    }
    
    return vc->mpqfile;
}


extern char const * versioncheck_get_eqn(t_versioncheck const * vc)
{
    if (!vc)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL vc");
	return NULL;
    }
    
    return vc->eqn;
}

#define safe_toupper(X) (islower((int)X)?toupper((int)X):(X))

/* This implements some dumb kind of pattern matching. Any '?'
 * signs in the pattern are treated as "don't care" signs. This
 * means that it doesn't matter what's on this place in the match.
 */
//static int versioncheck_compare_exeinfo(char const * pattern, char const * match)

extern int versioncheck_validate(t_versioncheck * vc, t_tag archtag, t_tag clienttag, char const * exeinfo, unsigned long versionid, unsigned long gameversion, unsigned long checksum)
{
    t_elem const     * curr;
    t_versioninfo    * vi;
    int                badexe,badcs;
	char			hashrtn[16];
    
	return 1;

    if (!vc)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL vc");
	return -1;
    }

	//if(strcmp(vc->mpqfile, "ver-ix86-1.mpq") == 0){
		//eventlog(eventlog_level_fatal, __FUNCTION__, "non-windows entry");
	//	return 1;
	//}

	//if(strcmp(vc->mpqfile, "IX86ver1.mpq") != 0){
		//eventlog(eventlog_level_fatal, __FUNCTION__, "non-windows entry");
	//	return 0;
	//}

    LIST_TRAVERSE_CONST(versioninfo_head,curr)
    {
        if (!(vi = elem_get_data(curr))) 
        {
	    eventlog(eventlog_level_error,__FUNCTION__,"version list contains NULL item");
	    continue;
        }
	
	if (vi->archtag != archtag)
	    continue;
	if (vi->clienttag != clienttag)
	    continue;
	if (strcmp(vi->mpqfile,vc->mpqfile)!=0)
	    continue;
	
	if (vi->versionid && vi->versionid != versionid)
	    continue;
	
	if (vi->gameversion && vi->gameversion != gameversion)
	    continue;

	versioncheck_hash_eqn(hashrtn, vc->eqn);
	if(strcmp(exeinfo, hashrtn)==0){
		badexe = 0;
	}else{
		badexe = 1;
	}
	
	if (vi->checksum && vi->checksum != checksum)
	{
	    badcs = 1;
	}
	else
	    badcs = 0;
	
	if (vc->versiontag)
	    xfree((void *)vc->versiontag);
	vc->versiontag = xstrdup(vi->versiontag);
	
	if (badexe || badcs)
	    continue;
	
	eventlog(eventlog_level_info,__FUNCTION__,"got a matching entry: %s",vc->versiontag);
	return 1;
    }
    
    if (badcs) 
    {
	eventlog(eventlog_level_info,__FUNCTION__,"bad checksum, closest match is: %s",vc->versiontag);
	return -1;
    }
    if (badexe)
    {
		eventlog(eventlog_level_info,__FUNCTION__,"bad exeinfo, closest match is: %s, eqn: %s", vc->versiontag, hashrtn);
	return -1;
    }
    
    eventlog(eventlog_level_info,__FUNCTION__,"no match in list, setting to: %s",vc->versiontag);
    return 0;

}

extern int versioncheck_load(char const * filename)
{
    FILE *	    fp;
    unsigned int    line;
    unsigned int    pos;
    char *	    buff;
    char *	    temp;
    char const *    eqn;
    char const *    mpqfile;
    char const *    archtag;
    char const *    clienttag;
    char const *    exeinfo;
    char const *    versionid;
    char const *    gameversion;
    char const *    checksum;
    char const *    versiontag;
    t_versioninfo * vi;
    
    if (!filename)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL filename");
	return -1;
    }
    
    if (!(versioninfo_head = list_create()))
    {
	eventlog(eventlog_level_error,__FUNCTION__,"could create list");
	return -1;
    }
    if (!(fp = fopen(filename,"r")))
    {
	eventlog(eventlog_level_error,__FUNCTION__,"could not open file \"%s\" for reading (fopen: %s)",filename,pstrerror(errno));
	list_destroy(versioninfo_head);
	versioninfo_head = NULL;
	return -1;
    }

    line = 1;
    for (; (buff = file_get_line(fp)); line++)
    {
	for (pos=0; buff[pos]=='\t' || buff[pos]==' '; pos++);
	if (buff[pos]=='\0' || buff[pos]=='#')
	{
	    continue;
	}
	if ((temp = strrchr(buff,'#')))
	{
	    unsigned int len;
	    unsigned int endpos;
	    
	    *temp = '\0';
	    len = strlen(buff)+1;
	    for (endpos=len-1;  buff[endpos]=='\t' || buff[endpos]==' '; endpos--);
	    buff[endpos+1] = '\0';
	}

	if (!(eqn = next_token(buff,&pos)))
	{
	    eventlog(eventlog_level_error,__FUNCTION__,"missing eqn near line %u of file \"%s\"",line,filename);
	    continue;
	}
	line++;
	if (!(mpqfile = next_token(buff,&pos)))
	{
	    eventlog(eventlog_level_error,__FUNCTION__,"missing mpqfile near line %u of file \"%s\"",line,filename);
	    continue;
	}
	line++;
	if (!(archtag = next_token(buff,&pos)))
	{
	    eventlog(eventlog_level_error,__FUNCTION__,"missing archtag near line %u of file \"%s\"",line,filename);
	    continue;
	}
	line++;
	if (!(clienttag = next_token(buff,&pos)))
	{
	    eventlog(eventlog_level_error,__FUNCTION__,"missing clienttag near line %u of file \"%s\"",line,filename);
	    continue;
	}
	line++;
	if (!(exeinfo = next_token(buff,&pos)))
	{
	    eventlog(eventlog_level_error,__FUNCTION__,"missing exeinfo near line %u of file \"%s\"",line,filename);
	    continue;
	}
	line++;
	if (!(versionid = next_token(buff,&pos)))
	{
	    eventlog(eventlog_level_error,__FUNCTION__,"missing versionid near line %u of file \"%s\"",line,filename);
	    continue;
	}
	line++;
	if (!(gameversion = next_token(buff,&pos)))
	{
	    eventlog(eventlog_level_error,__FUNCTION__,"missing gameversion near line %u of file \"%s\"",line,filename);
	    continue;
	}
	line++;
	if (!(checksum = next_token(buff,&pos)))
	{
	    eventlog(eventlog_level_error,__FUNCTION__,"missing checksum near line %u of file \"%s\"",line,filename);
	    continue;
	}
	line++;
	if (!(versiontag = next_token(buff,&pos)))
	{
	    versiontag = NULL;
	}

	vi = xmalloc(sizeof(t_versioninfo));
	vi->eqn = xstrdup(eqn);
	vi->mpqfile = xstrdup(mpqfile);
	if (strlen(archtag)!=4)
	{
	    eventlog(eventlog_level_error,__FUNCTION__,"invalid arch tag on line %u of file \"%s\"",line,filename);
	    xfree((void *)vi->mpqfile); /* avoid warning */
	    xfree((void *)vi->eqn); /* avoid warning */
	    xfree(vi);
	    continue;
	}
	if (!tag_check_arch((vi->archtag = tag_str_to_uint(archtag))))
	{
	    eventlog(eventlog_level_error,__FUNCTION__,"got unknown archtag \"%s\"",archtag);
	    xfree((void *)vi->mpqfile); /* avoid warning */
	    xfree((void *)vi->eqn); /* avoid warning */
	    xfree(vi);
	    continue;
	}
	if (strlen(clienttag)!=4)
	{
	    eventlog(eventlog_level_error,__FUNCTION__,"invalid client tag on line %u of file \"%s\"",line,filename);
	    xfree((void *)vi->mpqfile); /* avoid warning */
	    xfree((void *)vi->eqn); /* avoid warning */
	    xfree(vi);
	    continue;
	}
	if (!tag_check_client((vi->clienttag = tag_str_to_uint(clienttag))))
	{
	    eventlog(eventlog_level_error,__FUNCTION__,"got unknown clienttag\"%s\"",clienttag);
	    xfree((void *)vi->mpqfile); /* avoid warning */
	    xfree((void *)vi->eqn); /* avoid warning */
	    xfree(vi);
	    continue;
	}

	vi->exeinfo = NULL;
	/*if (strcmp(exeinfo, "NULL") == 0)
	    vi->parsed_exeinfo = NULL;
	else
	{
	    if (!(vi->parsed_exeinfo = parse_exeinfo(exeinfo)))
	    {
		eventlog(eventlog_level_error,__FUNCTION__,"encountered an error while parsing exeinfo");
		xfree((void *)vi->mpqfile);
		xfree((void *)vi->eqn);
		xfree(vi);
		continue;
	    }
	}*/

	vi->versionid = strtoul(versionid,NULL,0);
	if (verstr_to_vernum(gameversion,&vi->gameversion)<0)
	{
	    eventlog(eventlog_level_error,__FUNCTION__,"malformed version on line %u of file \"%s\"",line,filename);
	    xfree((void *)vi->exeinfo); /* avoid warning */
	    xfree((void *)vi->mpqfile); /* avoid warning */
	    xfree((void *)vi->eqn); /* avoid warning */
	    xfree(vi);
	    continue;
        }

	vi->checksum = strtoul(checksum,NULL,0);
	if (versiontag)
	    vi->versiontag = xstrdup(versiontag);
	else
	    vi->versiontag = NULL;
	
	
	list_append_data(versioninfo_head,vi);
    }
    
    file_get_line(NULL); // clear file_get_line buffer
    if (fclose(fp)<0)
	eventlog(eventlog_level_error,__FUNCTION__,"could not close versioncheck file \"%s\" after reading (fclose: %s)",filename,pstrerror(errno));
    
	
	srand(time(NULL));
    return 0;
}


extern int versioncheck_unload(void)
{
    t_elem *	    curr;
    t_versioninfo * vi;
    
    if (versioninfo_head)
    {
	LIST_TRAVERSE(versioninfo_head,curr)
	{
	    if (!(vi = elem_get_data(curr))) /* should not happen */
	    {
		eventlog(eventlog_level_error,__FUNCTION__,"version list contains NULL item");
		continue;
	    }
	    
	    if (list_remove_elem(versioninfo_head,&curr)<0)
		eventlog(eventlog_level_error,__FUNCTION__,"could not remove item from list");

	    if (vi->exeinfo)
            {
		xfree((void *)vi->exeinfo); /* avoid warning */
            }
	    xfree((void *)vi->mpqfile); /* avoid warning */
	    xfree((void *)vi->eqn); /* avoid warning */
	    if (vi->versiontag)
		xfree((void *)vi->versiontag); /* avoid warning */
	    xfree(vi);
	}
	
	if (list_destroy(versioninfo_head)<0)
	    return -1;
	versioninfo_head = NULL;
    }
    
    return 0;
}
