/*
  * Abstraction API/layer for the various ways PvPGN can inspect sockets state
  * 2003 (C) 
  *
  * Code is based on the ideas found in thttpd project.
  *
  * Linux iocp(4) based backend
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
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <windows.h>
#include "common/setup_before.h"

#ifdef HAVE_IOCP

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
#include "fdwatch.h"
#include "common/addr.h"
#include "common/eventlog.h"
#include "common/xalloc.h"
#include "common/setup_after.h"

struct iocp_accept_data;
struct iocp_event;

struct iocp_accept_data
{
	int accept_sock;
	int listen_sock;
	int accept_hdr_len;
	struct sockaddr_in accept_addr;
	char accept_hdr[sizeof(SOCKADDR_IN)*2+16*2];
};

struct iocp_event
{
	WSAOVERLAPPED overlapped;
	int idx;
	char events;
	struct iocp_accept_data* data;
};


static HANDLE iocp_port;
static char* iocp_state;
static struct iocp_event *revents  = NULL;
static struct iocp_event *wevents  = NULL;
static int fdw_iocp_init(int nfds);
static int fdw_iocp_close(void);
static int fdw_iocp_add_fd(int idx, t_fdwatch_type rw);
static int fdw_iocp_del_fd(int idx);
static int fdw_iocp_watch(long timeout_msecs);
static void fdw_iocp_handle(void);
static int last_accept_idx;

t_fdw_backend fdw_iocp = {
    fdw_iocp_init,
    fdw_iocp_close,
    fdw_iocp_add_fd,
    fdw_iocp_del_fd,
    fdw_iocp_watch,
    fdw_iocp_handle
};

LPFN_ACCEPTEX lpAcceptEx = NULL;
GUID guidAcceptEx = WSAID_ACCEPTEX;
WSABUF wsaBuf; 
DWORD dummy1 = 0, dummy2 = 0;

static int fdw_iocp_init(int nfds)
{
	WSADATA data;
	wsaBuf.buf = NULL; //(char*)malloc(4096); 
	wsaBuf.len = 0; 
	WSAStartup(MAKEWORD(2,2), &data);
	if ((iocp_port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0)) < 0)
		return -1;
    revents = (struct iocp_event *) xmalloc(sizeof(struct iocp_event) * nfds);
    wevents = (struct iocp_event *) xmalloc(sizeof(struct iocp_event) * nfds);
    iocp_state = (char*) xmalloc(sizeof(char) * nfds);
	
    memset(iocp_state, 0, sizeof(char) * nfds);
    memset(revents, 0, sizeof(struct iocp_event) * nfds);
    memset(wevents, 0, sizeof(struct iocp_event) * nfds);

    eventlog(eventlog_level_info, __FUNCTION__, "fdwatch iocp() based layer initialized (max %d sockets)", nfds);
    return 0;
}

static int fdw_iocp_close(void)
{
    if (revents != NULL)
		xfree((void *) revents);
    if (wevents != NULL)
		xfree((void *) wevents);

    return 0;
}

static int fdw_iocp_add_fd(int idx, t_fdwatch_type rw)
{
	struct iocp_event *tmpev = &revents[idx];
	struct iocp_event *tmpev2 = &wevents[idx];
	char orig_state = iocp_state[idx];
	int ret;
	int err;
    tmpev->events = fdwatch_type_read;
	tmpev->idx = idx;
    tmpev2->events = fdwatch_type_write;
	tmpev2->idx = idx;
	if (iocp_state[idx] == 0)
	{
		if (CreateIoCompletionPort((HANDLE)fdw_fd(fdw_fds + idx), iocp_port, (ULONG_PTR)tmpev, 0) == NULL) {
			eventlog(eventlog_level_fatal, __FUNCTION__, "got error from CreateIoCompletionPort(): %d", GetLastError());
			return -1;
		}
	}
	if ((rw & fdwatch_type_read) && !(rw & fdwatch_type_accept) && !(orig_state & fdwatch_type_read))
	{
		memset(tmpev, 0, sizeof(WSAOVERLAPPED));
		ret = WSARecv(fdw_fd(fdw_fds + idx), &wsaBuf, 1, &dummy1, &dummy2, (LPWSAOVERLAPPED)tmpev, NULL);
		if ((ret == -1) && (err = GetLastError()) != 997)
		{
			eventlog(eventlog_level_fatal, __FUNCTION__, "cannot update iocp sock %d with read state: %d", fdw_fd(fdw_fds + idx), err);
			//printf("Error %d on WSARecv\n", err);
		}
	}
	if (rw & fdwatch_type_write && !(orig_state & fdwatch_type_write))
	{
		memset(tmpev2, 0, sizeof(WSAOVERLAPPED));
		WSASend(fdw_fd(fdw_fds + idx), &wsaBuf, 1, &dummy1, 0, (LPWSAOVERLAPPED)tmpev2, NULL);
		if ((ret == -1) && (err = GetLastError()) != 997)
		{
			eventlog(eventlog_level_fatal, __FUNCTION__, "cannot update iocp sock %d with write state: %d", fdw_fd(fdw_fds + idx), err);
			//printf("Error %d on WSASend\n", err);
		}
	}
	if (rw & fdwatch_type_accept)
	{
		int dummy;
		memset(tmpev, 0, sizeof(WSAOVERLAPPED));
		tmpev->events = fdwatch_type_accept;
		tmpev->idx = idx;
		tmpev->data = (struct iocp_accept_data*)malloc(sizeof(struct iocp_accept_data));
		tmpev->data->listen_sock = fdw_fd(fdw_fds + idx);
		tmpev->data->accept_sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED);
		WSAIoctl(tmpev->data->listen_sock, SIO_GET_EXTENSION_FUNCTION_POINTER, &guidAcceptEx, sizeof(guidAcceptEx), &lpAcceptEx, sizeof(lpAcceptEx), (LPDWORD)&dummy, NULL, NULL);
		lpAcceptEx(tmpev->data->listen_sock, tmpev->data->accept_sock, tmpev->data->accept_hdr, 0, sizeof(SOCKADDR_IN)+16, sizeof(SOCKADDR_IN)+16, (LPDWORD)&tmpev->data->accept_hdr_len, (LPOVERLAPPED)tmpev);
	}
	iocp_state[idx] = rw;

    return 0;
}

static int fdw_iocp_del_fd(int idx)
{
	//CloseHandle((HANDLE)fdw_fd(fdw_fds + idx));
	iocp_state[idx] = 0;
    return 0;
}

struct iocp_event* pending_ev;
struct iocp_event* pending_key;
int pending_recvlen;
int pending_err;

static int fdw_iocp_watch(long timeout_msec)
{
	int ret;
	pending_ev = NULL;
	ret = GetQueuedCompletionStatus(iocp_port, (LPDWORD)&pending_recvlen, (PULONG_PTR)&pending_key, (LPOVERLAPPED*)&pending_ev, timeout_msec);
	pending_err = GetLastError();
	if (ret == 0 && pending_err != 64)
	{
		pending_ev = NULL;
		return 0;
	}
	return 1;
}

static void fdw_iocp_handle(void)
{
	int idx;
	int ret, err;
	t_fdwatch_fd *cfd;
	struct iocp_event *tmpev;
	struct iocp_event *tmpev2;
	fdwatch_handler hnd;
	if (pending_ev == NULL) return;
	idx = pending_ev->idx;
	cfd = fdw_fds + idx;
	tmpev = &revents[idx];
	tmpev2 = &wevents[idx];
	hnd = fdw_hnd(cfd);
	if (hnd == 0) return;
	if (pending_err == 64)
	{
		eventlog(eventlog_level_fatal, __FUNCTION__, "failed to handle iocp on sock %d with error: %d", fdw_fd(fdw_fds + idx), pending_err);
		//printf("errno: %d\n", pending_err);
		hnd(fdw_data(cfd), fdwatch_type_read);
		return;
	}
	if (pending_ev->events == fdwatch_type_accept)
	{
		memcpy(&pending_ev->data->accept_addr, &pending_ev->data->accept_hdr[38], sizeof(struct sockaddr_in));
		last_accept_idx = idx;
		hnd(fdw_data(cfd), fdwatch_type_read);
		return;
	}
	if (fdw_rw(cfd) & fdwatch_type_read && pending_ev->events == fdwatch_type_read)
	{
		if (hnd(fdw_data(cfd), fdwatch_type_read) == -2)
		{
			return;
		}
		memset(tmpev, 0, sizeof(WSAOVERLAPPED));
		WSARecv(fdw_fd(fdw_fds + idx), &wsaBuf, 1, &dummy1, &dummy2, (LPWSAOVERLAPPED)tmpev, NULL);
		if ((ret == -1) && (err = GetLastError()) != 997)
		{
			eventlog(eventlog_level_fatal, __FUNCTION__, "cannot update iocp sock %d with read state: %d", fdw_fd(fdw_fds + idx), err);
			//printf("Error %d on WSARecv\n", err);
		}
	}
    if (fdw_rw(cfd) & fdwatch_type_write && pending_ev->events == fdwatch_type_write)
	{
		hnd(fdw_data(cfd), fdwatch_type_write);
		memset(tmpev2, 0, sizeof(WSAOVERLAPPED));
		WSASend(fdw_fd(fdw_fds + idx), &wsaBuf, 1, &dummy1, 0, (LPWSAOVERLAPPED)tmpev2, NULL);
		if ((ret == -1) && (err = GetLastError()) != 997)
		{
			eventlog(eventlog_level_fatal, __FUNCTION__, "cannot update iocp sock %d with write state: %d", fdw_fd(fdw_fds + idx), err);
			//printf("Error %d on WSASend\n", err);
		}
	}
}

extern int iocp_accept(struct sockaddr* addr, int* addrlen)
{
	LINGER linger = {1,0};
	struct iocp_event* accept_ev = &revents[last_accept_idx];
	int sock = accept_ev->data->accept_sock;
	memcpy(addr, &accept_ev->data->accept_addr, sizeof(struct sockaddr_in));
	*addrlen = sizeof(struct sockaddr_in);
	
	setsockopt(accept_ev->data->accept_sock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)&accept_ev->data->listen_sock, sizeof(accept_ev->data->listen_sock));
	setsockopt(accept_ev->data->accept_sock, SOL_SOCKET, SO_LINGER, (char *)&linger, sizeof(linger));
	memset(accept_ev, 0, sizeof(WSAOVERLAPPED));
	accept_ev->data->accept_sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED);
	lpAcceptEx(accept_ev->data->listen_sock, accept_ev->data->accept_sock, accept_ev->data->accept_hdr, 0, sizeof(SOCKADDR_IN)+16, sizeof(SOCKADDR_IN)+16, (LPDWORD)&accept_ev->data->accept_hdr_len, (LPOVERLAPPED)accept_ev);

	return sock;
}

#endif