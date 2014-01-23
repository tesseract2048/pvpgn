/*
 * Copyright (C) 2000  Ross Combs (rocombs@cs.nmsu.edu)
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
#define REALM_INTERNAL_ACCESS
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
#include <errno.h>
#include "compat/strerror.h"
#include "common/eventlog.h"
#include "common/list.h"
#include "common/util.h"
#include "common/addr.h"
#include "common/xalloc.h"
#include "connection.h"
#include "common/rcm.h"
#include "realm.h"
#ifdef HAVE_ASSERT_H
# include <assert.h>
#endif
#include "common/setup_after.h"


static t_list * realmlist_head=NULL;

static t_realm * realm_create(char const * name, char const * description, unsigned int ip, unsigned int port, unsigned int vip_realm, unsigned int realm_version);
static int realm_destroy(t_realm * realm);
static unsigned int realm_number = 0;

static t_realm * realm_create(char const * name, char const * description, unsigned int ip, unsigned int port, unsigned int vip_realm, unsigned int realm_version)
{
    t_realm * realm;
    
    if (!name)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL name");
	return NULL;
    }
    if (!description)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL description");
	return NULL;
    }

    realm = xmalloc(sizeof(t_realm));
    realm->name = NULL;
    realm->description = NULL;

    if (realm_set_name(realm ,name)<0)
    {
        eventlog(eventlog_level_error,__FUNCTION__,"failed to set name for realm");
	xfree(realm);
	return NULL;
    }
    if (realm->description != NULL) xfree((void *)realm->description);
    realm->description = xstrdup(description);
    realm->ip = ip;
    realm->port = port;
    realm->conn = NULL;
    realm->active = 0;
    realm->player_number = 0;
    realm->game_number = 0;
    realm->sessionnum = 0;
    realm->tcp_sock = 0;
    realm->vip_realm = vip_realm;
    realm->realm_version = realm_version;
    rcm_init(&realm->rcm);

	eventlog(eventlog_level_info,__FUNCTION__,"created realm \"%s\" (ifvip: %d, ver: %d)",name, vip_realm, realm_version);
    return realm;
}


static int realm_destroy(t_realm * realm)
{
    if (!realm)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL realm");
	return -1;
    }
    
    if (realm->active)
    	realm_deactive(realm);

    xfree((void *)realm->name); /* avoid warning */
    xfree((void *)realm->description); /* avoid warning */
    xfree((void *)realm); /* avoid warning */
    
    return 0;
}


extern char const * realm_get_name(t_realm const * realm)
{
    if (!realm)
    {
	return NULL;
    }
    return realm->name;
}


extern int realm_set_name(t_realm * realm, char const * name)
{
    char const      * temp;
    t_realm const * temprealm;

    if (!realm)
    {
        eventlog(eventlog_level_error,__FUNCTION__,"got NULL realm");
        return -1;
    }

    if (name && (temprealm=realmlist_find_realm(name)))
    {
         if (temprealm == realm)
              return 0;
         else
         {
              eventlog(eventlog_level_error,__FUNCTION__,"realm %s does already exist in list",name);
              return -1;
         }
    }

    if (name)
	temp=xstrdup(name);
    else
      temp = NULL;

    if (realm->name)
      xfree((void *)realm->name); /* avoid warning */
    realm->name = temp;

    return 0;
}


extern char const * realm_get_description(t_realm const * realm)
{
    if (!realm)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL realm");
	return NULL;
    }
    return realm->description;
}

extern unsigned int realm_get_vip_realm(t_realm const * realm)
{
    if (!realm)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL realm");
	return 0;
    }
    return realm->vip_realm;
}

extern unsigned int realm_get_version(t_realm const * realm)
{
    if (!realm)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL realm");
	return 0;
    }
    return realm->realm_version;
}



extern unsigned short realm_get_port(t_realm const * realm)
{
    if (!realm)
    {
        eventlog(eventlog_level_error,__FUNCTION__,"got NULL realm");
        return 0;
    }
    return realm->port;
}


extern unsigned int realm_get_ip(t_realm const * realm)
{
    if (!realm)
    {
        eventlog(eventlog_level_error,__FUNCTION__,"got NULL realm");
        return 0;
    }
    return realm->ip;
}


extern unsigned int realm_get_active(t_realm const * realm)
{
    if (!realm)
    {
        eventlog(eventlog_level_error,__FUNCTION__,"got NULL realm");
        return 0;
    }
    return realm->active;
}

extern int realm_set_active(t_realm * realm, unsigned int active)
{
    if (!realm)
    {
        eventlog(eventlog_level_error,__FUNCTION__,"got NULL realm");
        return -1;
    }

    realm->active=active;
    return 0;
}

extern unsigned int realm_get_player_number(t_realm const * realm)
{
    if (!realm)
    {
        eventlog(eventlog_level_error,__FUNCTION__,"got NULL realm");
        return 0;
    }
    return realm->player_number;
}

extern int realm_add_player_number(t_realm * realm, int number)
{
    if (!realm)
    {
        eventlog(eventlog_level_error,__FUNCTION__,"got NULL realm");
        return -1;
    }
    realm->player_number += number;
    return 0;
}

extern unsigned int realm_get_game_number(t_realm const * realm)
{
    if (!realm)
    {
        eventlog(eventlog_level_error,__FUNCTION__,"got NULL realm");
        return 0;
    }
    return realm->game_number;
}

extern int realm_add_game_number(t_realm * realm, int number)
{
    if (!realm)
    {
        eventlog(eventlog_level_error,__FUNCTION__,"got NULL realm");
        return -1;
    }
    realm->game_number += number;
    return 0;
}

extern int realm_active(t_realm * realm, t_connection * c)
{
    if (!realm)
    {
        eventlog(eventlog_level_error,__FUNCTION__,"got NULL realm");
        return -1;
    }
    if (!c)
    {
        eventlog(eventlog_level_error,__FUNCTION__,"got NULL connection");
        return -1;
    }
    if (realm->active)
    {
        eventlog(eventlog_level_debug,__FUNCTION__, "realm %s is already actived,destroy previous one",realm->name);
        realm_deactive(realm);
    }
    realm->active=1;
    realm->conn=c;
    conn_set_realm(c,realm);
    realm->sessionnum=conn_get_sessionnum(c);
    realm->tcp_sock=conn_get_socket(c);
	realm_number ++;
    eventlog(eventlog_level_info,__FUNCTION__, "realm %s actived",realm->name);
    return 0;
}

extern int realm_deactive(t_realm * realm)
{
    t_connection * c;

    if (!realm)
    {
        eventlog(eventlog_level_error,__FUNCTION__,"got NULL realm");
        return -1;
    }
    if (!realm->active)
    {
        eventlog(eventlog_level_error,__FUNCTION__,"realm %s is not actived",realm->name);
        return -1;
    }
    if ((c = realm_get_conn(realm)))
        conn_set_state(c,conn_state_destroy);

    realm->active=0;
    realm->sessionnum=0;
    realm->tcp_sock=0;
    /*
    realm->player_number=0;
    realm->game_number=0;
    */
	realm_number--;
    eventlog(eventlog_level_info,__FUNCTION__, "realm %s deactived",realm->name);
    return 0;
}


t_list * realmlist_load(char const * filename)
{
    FILE *          fp;
    unsigned int    line;
    unsigned int    pos;
    unsigned int    len;
    t_addr *        raddr;
    char *          temp, *temp2;
    char *          buff;
    char *          name;
    char *          desc;
    t_realm *       realm;
    t_list *        list_head = NULL;
	unsigned int	vip_realm;
	unsigned int	realm_version;
    
    if (!filename)
    {
        eventlog(eventlog_level_error,__FUNCTION__,"got NULL filename");
        return NULL;
    }
    
    if (!(fp = fopen(filename,"r")))
    {
        eventlog(eventlog_level_error,__FUNCTION__,"could not open realm file \"%s\" for reading (fopen: %s)",filename,pstrerror(errno));
        return NULL;
    }
    
    list_head = list_create();

    for (line=1; (buff = file_get_line(fp)); line++)
    {
        for (pos=0; buff[pos]=='\t' || buff[pos]==' '; pos++);
        if (buff[pos]=='\0' || buff[pos]=='#')
        {
            continue;
        }
        if ((temp = strrchr(buff,'#')))
        {
	    unsigned int endpos;
	    
            *temp = '\0';
	    len = strlen(buff)+1;
            for (endpos=len-1;  buff[endpos]=='\t' || buff[endpos]==' '; endpos--);
            buff[endpos+1] = '\0';
        }
        
	/* skip any separators */
	for (temp = buff; *temp && (*temp == ' ' || *temp == '\t');temp++);
	if (*temp != '"') {
	    eventlog(eventlog_level_error,__FUNCTION__,"malformed line %u in file \"%s\" (no realmname)",line,filename);
	    continue;
	}
	
	temp2 = temp + 1;
	/* find the next " */
	for (temp = temp2; *temp && *temp != '"';temp++);
	if (*temp != '"' || temp == temp2) {
	    eventlog(eventlog_level_error,__FUNCTION__,"malformed line %u in file \"%s\" (no realmname)",line,filename);
	    continue;
	}
	
	/* save the realmname */
	*temp = '\0';
        name = xstrdup(temp2);
	
	/* eventlog(eventlog_level_trace, __FUNCTION__,"found realmname: %s",name); */

	/* skip any separators */
	for(temp = temp + 1; *temp && (*temp == '\t' || *temp == ' ');temp++);
	
	if (*temp == '"') { /* we have realm description */
	    temp2 = temp + 1;
	    /* find the next " */
	    for(temp = temp2;*temp && *temp != '"';temp++);
	    if (*temp != '"' || temp == temp2) {
		eventlog(eventlog_level_error,__FUNCTION__,"malformed line %u in file \"%s\" (no valid description)",line,filename);
		xfree(name);
		continue;
	    }
	    
	    /* save the description */
	    *temp = '\0';
    	    desc = xstrdup(temp2);
	    
	    /* eventlog(eventlog_level_trace, __FUNCTION__,"found realm desc: %s",desc); */

	    /* skip any separators */
	    for(temp = temp + 1; *temp && (*temp == ' ' || *temp == '\t');temp++);
	} else desc = xstrdup("\0");

	temp2 = temp;
	/* find out where address ends */
	for(temp = temp2 + 1; *temp && *temp != ' ' && *temp != '\t';temp++);

	*temp++ = '\0';

	/* eventlog(eventlog_level_trace, __FUNCTION__,"found realm ip: %s",temp2); */

	if (!(raddr = addr_create_str(temp2,0,BNETD_REALM_PORT))) /* 0 means "this computer" */ {
	    eventlog(eventlog_level_error,__FUNCTION__,"invalid address value for field 3 on line %u in file \"%s\"",line,filename);
	    xfree(name);
	    xfree(desc);
	    continue;
	}

	temp2 = temp;
	for (temp = temp2; *temp && (*temp == ' ' || *temp == '\t');temp++);

	if(*temp == '1'){
		vip_realm = 1;
	}else{
		vip_realm = 0;
	}
	*temp++ = '\0';
	
	temp2 = temp;
	for (temp = temp2; *temp && (*temp == ' ' || *temp == '\t');temp++);
	if(*temp == 'd'){
		realm_version = 0x0d;
	}else{
		realm_version = 0x0b;
	}

	if (*temp) *temp++ = '\0'; /* if is not the end of the file, end addr and move forward */

	
	if (!(realm = realm_create(name,desc,addr_get_ip(raddr),addr_get_port(raddr),vip_realm,realm_version)))
	{
	    eventlog(eventlog_level_error,__FUNCTION__,"could not create realm");
	    addr_destroy(raddr);
	    xfree(name);
	    xfree(desc);
	    continue;
	}

	addr_destroy(raddr);
	xfree(name);
	xfree(desc);
	
	list_prepend_data(list_head,realm);
    }
    file_get_line(NULL); // clear file_get_line buffer
	realm_number = 0;
    if (fclose(fp)<0)
	eventlog(eventlog_level_error,__FUNCTION__,"could not close realm file \"%s\" after reading (fclose: %s)",filename,pstrerror(errno));
    return list_head;
}

extern int realmlist_reload(char const * filename)
{
    t_elem * new_curr;
    t_elem * old_curr;
    t_realm * new_realm;
    t_realm * old_realm;
    int match;
    t_list * newlist = NULL;
    t_list * oldlist = realmlist_head;

    realmlist_head = NULL;

    if (!(newlist = realmlist_load(filename)))
        return -1;

    LIST_TRAVERSE(oldlist,old_curr)
    {
    	if (!(old_realm = elem_get_data(old_curr)))
	{
	  eventlog(eventlog_level_error,__FUNCTION__,"found NULL elem in list");
	  continue;
	}

	match = 0;

	LIST_TRAVERSE(newlist,new_curr)
	{
    	    if (!(new_realm = elem_get_data(new_curr)))
	    {
	      eventlog(eventlog_level_error,__FUNCTION__,"found NULL elem in list");
	      continue;
	    }

	    if (!strcmp(old_realm->name,new_realm->name))
	    {
		match = 1;
		rcm_chref(&old_realm->rcm,new_realm);
		
		break;
	    }

	}
	if (!match)
	  rcm_chref(&old_realm->rcm,NULL);

	realm_destroy(old_realm);
        list_remove_elem(oldlist,&old_curr);
    }

    list_destroy(oldlist);

    realmlist_head = newlist;
	realm_number = 0;

    return 0;
}

extern int realmlist_create(char const * filename)
{
    if (!(realmlist_head = realmlist_load(filename)))
       return -1;

    return 0;
       
}

extern int realmlist_unload(t_list * list_head)
{
    t_elem *  curr;
    t_realm * realm;
    
    if (list_head)
    {
	LIST_TRAVERSE(list_head,curr)
	{
	    if (!(realm = elem_get_data(curr)))
		eventlog(eventlog_level_error,__FUNCTION__,"found NULL realm in list");
	    else
	        realm_destroy(realm);

	    list_remove_elem(list_head,&curr);
	}
	list_destroy(list_head);
    }
	realm_number = 0;
    
    return 0;
}

extern int realmlist_destroy()
{
	int res;
	
	res = realmlist_unload(realmlist_head);
	realmlist_head = NULL;

	return res;
}

extern t_list * realmlist(void)
{
    return realmlist_head;
}

extern t_realm * realmlist_find_realm(char const * realmname)
{
    t_elem const *  curr;
    t_realm * realm;
    
    if (!realmname)
    {
	eventlog(eventlog_level_error,__FUNCTION__,"got NULL realmname");
	return NULL;
    }

    LIST_TRAVERSE_CONST(realmlist_head,curr)
    {
	realm = elem_get_data(curr);
	if (strcasecmp(realm->name,realmname)==0)
	    return realm;
    }
    
    return NULL;
}

extern t_realm * realmlist_find_realm_by_ip(unsigned long ip)
{
    t_elem const *  curr;
    t_realm * realm;

    LIST_TRAVERSE_CONST(realmlist_head,curr)
    {
        realm = elem_get_data(curr);
        if (realm->ip==ip)
            return realm;
    }
    return NULL;
}

extern t_connection * realm_get_conn(t_realm * realm)
{
	assert(realm);
	
	return realm->conn;
}

extern t_realm * realm_get(t_realm * realm, t_rcm_regref * regref)
{
	rcm_get(&realm->rcm,regref);
	return realm;
}

extern void realm_put(t_realm * realm, t_rcm_regref * regref)
{
	rcm_put(&realm->rcm,regref);
}
