/*-
 * Copyright (c) 1982, 1986, 1988, 1993
 *	The Regents of the University of California.
 * Copyright (c) 2006-2007 Robert N. M. Watson
 * Copyright (c) 2010-2011 Juniper Networks, Inc.
 * All rights reserved.
 *
 * Portions of this software were developed by Robert N. M. Watson under
 * contract to Juniper Networks, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	From: @(#)tcp_usrreq.c	8.2 (Berkeley) 1/3/94
 */

#include <sys/cdefs.h>

#include <osv/initialize.hh>
#include <bsd/porting/netport.h>
#include <bsd/porting/sync_stub.h>

#include <bsd/sys/sys/libkern.h>

#include <bsd/sys/sys/param.h>
#include <bsd/sys/sys/limits.h>
#include <bsd/sys/sys/mbuf.h>
#ifdef INET6
#include <bsd/sys/sys/domain.h>
#endif /* INET6 */
#include <bsd/sys/sys/socket.h>
#include <bsd/sys/sys/socketvar.h>
#include <bsd/sys/sys/protosw.h>

#ifdef DDB
#include <ddb/ddb.h>
#endif

#include <bsd/sys/net/if.h>
#include <bsd/sys/net/route.h>
#include <bsd/sys/net/vnet.h>

#include <bsd/sys/netinet/cc.h>
#include <bsd/sys/netinet/in.h>
#include <bsd/sys/netinet/in_pcb.h>
#include <bsd/sys/netinet/in_systm.h>
#include <bsd/sys/netinet/in_var.h>
#include <bsd/sys/netinet/ip_var.h>
#ifdef INET6
#include <bsd/sys/netinet/ip6.h>
#include <bsd/sys/netinet6/in6_pcb.h>
#include <bsd/sys/netinet6/ip6_var.h>
#include <bsd/sys/netinet6/scope6_var.h>
#endif
#include <bsd/sys/netinet/tcp_fsm.h>
#include <bsd/sys/netinet/tcp_seq.h>
#include <bsd/sys/netinet/tcp_timer.h>
#include <bsd/sys/netinet/tcp_var.h>
#include <bsd/sys/netinet/tcpip.h>
#ifdef TCPDEBUG
#include <bsd/sys/netinet/tcp_debug.h>
#endif

#include <osv/poll.h>

/*
 * TCP protocol interface to socket abstraction.
 */
static int	tcp_attach(struct socket *);
#ifdef INET
static int	tcp_connect(struct tcpcb *, struct bsd_sockaddr *,
		    struct thread *td);
#endif /* INET */
#ifdef INET6
static int	tcp6_connect(struct tcpcb *, struct bsd_sockaddr *,
		    struct thread *td);
#endif /* INET6 */
static void	tcp_disconnect(struct tcpcb *);
static void	tcp_usrclosed(struct tcpcb *);
static void	tcp_fill_info(struct tcpcb *, struct tcp_info *);

#ifdef TCPDEBUG
#define	TCPDEBUG0	int ostate = 0
#define	TCPDEBUG1()	ostate = tp ? tp->get_state() : 0
#define	TCPDEBUG2(req)	if (tp && (so->so_options & SO_DEBUG)) \
				tcp_trace(TA_USER, ostate, tp, 0, 0, req)
#else
#define	TCPDEBUG0
#define	TCPDEBUG1()
#define	TCPDEBUG2(req)
#endif

/*
 * TCP attaches to socket via pru_attach(), reserving space,
 * and an internet control block.
 */
static int
tcp_usr_attach(struct socket *so, int proto, struct thread *td)
{
	struct inpcb *inp;
	struct tcpcb *tp = NULL;
	int error;
	TCPDEBUG0;

	inp = sotoinpcb(so);
	KASSERT(inp == NULL, ("tcp_usr_attach: inp != NULL"));
	TCPDEBUG1();

	error = tcp_attach(so);
	if (error)
		goto out;

	if ((so->so_options & SO_LINGER) && so->so_linger == 0)
		so->so_linger = TCP_LINGERTIME;

	inp = sotoinpcb(so);
	tp = intotcpcb(inp);
	if (tp->nc) {
		so->so_nc = tp->nc;
		for (auto&& pl : so->fp->f_poll_list) {
			so->so_nc->add_poller(*pl._req);
		}
		if (so->fp->f_epolls) {
		    for (auto&& ep : *so->fp->f_epolls) {
		        so->so_nc->add_epoll(ep);
		    }
		}
	}
out:
	TCPDEBUG2(PRU_ATTACH);
	return error;
}

/*
 * tcp_detach is called when the socket layer loses its final reference
 * to the socket, be it a file descriptor reference, a reference from TCP,
 * etc.  At this point, there is only one case in which we will keep around
 * inpcb state: time wait.
 *
 * This function can probably be re-absorbed back into tcp_usr_detach() now
 * that there is a single detach path.
 */
static void
tcp_detach(struct socket *so, struct inpcb *inp)
{
	struct tcpcb *tp;

	INP_INFO_WLOCK_ASSERT(&V_tcbinfo);
	INP_LOCK_ASSERT(inp);

	KASSERT(so->so_pcb == inp, ("tcp_detach: so_pcb != inp"));
	KASSERT(inp->inp_socket == so, ("tcp_detach: inp_socket != so"));

	tp = intotcpcb(inp);

	if (inp->inp_flags & INP_TIMEWAIT) {
		/*
		 * There are two cases to handle: one in which the time wait
		 * state is being discarded (INP_DROPPED), and one in which
		 * this connection will remain in timewait.  In the former,
		 * it is time to discard all state (except tcptw, which has
		 * already been discarded by the timewait close code, which
		 * should be further up the call stack somewhere).  In the
		 * latter case, we detach from the socket, but leave the pcb
		 * present until timewait ends.
		 *
		 * XXXRW: Would it be cleaner to free the tcptw here?
		 */
		if (inp->inp_flags & INP_DROPPED) {
			KASSERT(tp == NULL, ("tcp_detach: INP_TIMEWAIT && "
			    "INP_DROPPED && tp != NULL"));
			in_pcbdetach(inp);
			in_pcbfree(inp);
		} else {
			in_pcbdetach(inp);
			INP_UNLOCK(inp);
		}
	} else {
		/*
		 * If the connection is not in timewait, we consider two
		 * two conditions: one in which no further processing is
		 * necessary (dropped || embryonic), and one in which TCP is
		 * not yet done, but no longer requires the socket, so the
		 * pcb will persist for the time being.
		 *
		 * XXXRW: Does the second case still occur?
		 */
		if (inp->inp_flags & INP_DROPPED ||
		    tp->get_state() < TCPS_SYN_SENT) {
			tcp_discardcb(tp);
			in_pcbdetach(inp);
			in_pcbfree(inp);
		} else {
			in_pcbdetach(inp);
			INP_UNLOCK(inp);
		}
	}
}

/*
 * pru_detach() detaches the TCP protocol from the socket.
 * If the protocol state is non-embryonic, then can't
 * do this directly: have to initiate a pru_disconnect(),
 * which may finish later; embryonic TCB's can just
 * be discarded here.
 */
static void
tcp_usr_detach(struct socket *so)
{
	struct inpcb *inp;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("tcp_usr_detach: inp == NULL"));
	INP_INFO_WLOCK(&V_tcbinfo);
	INP_LOCK(inp);
	KASSERT(inp->inp_socket != NULL,
	    ("tcp_usr_detach: inp_socket == NULL"));
	tcp_detach(so, inp);
	INP_INFO_WUNLOCK(&V_tcbinfo);
}

#ifdef INET
/*
 * Give the socket an address.
 */
static int
tcp_usr_bind(struct socket *so, struct bsd_sockaddr *nam, struct thread *td)
{
	int error = 0;
	struct inpcb *inp;
	struct tcpcb *tp = NULL;
	struct bsd_sockaddr_in *sinp;

	sinp = (struct bsd_sockaddr_in *)nam;
	if (nam->sa_len != sizeof (*sinp))
		return (EINVAL);
	/*
	 * Must check for multicast addresses and disallow binding
	 * to them.
	 */
	if (sinp->sin_family == AF_INET &&
	    IN_MULTICAST(ntohl(sinp->sin_addr.s_addr)))
		return (EAFNOSUPPORT);

	TCPDEBUG0;
	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("tcp_usr_bind: inp == NULL"));
	INP_LOCK(inp);
	if (inp->inp_flags & (INP_TIMEWAIT | INP_DROPPED)) {
		error = EINVAL;
		goto out;
	}
	tp = intotcpcb(inp);
	TCPDEBUG1();
	INP_HASH_WLOCK(&V_tcbinfo);
	error = in_pcbbind(inp, nam, 0);
	INP_HASH_WUNLOCK(&V_tcbinfo);
out:
	TCPDEBUG2(PRU_BIND);
	INP_UNLOCK(inp);

	return (error);
}
#endif /* INET */

#ifdef INET6
static int
tcp6_usr_bind(struct socket *so, struct bsd_sockaddr *nam, struct thread *td)
{
	int error = 0;
	struct inpcb *inp;
	struct tcpcb *tp = NULL;
	struct bsd_sockaddr_in6 *sin6p;

	sin6p = (struct bsd_sockaddr_in6 *)nam;
	if (nam->sa_len != sizeof (*sin6p))
		return (EINVAL);
	/*
	 * Must check for multicast addresses and disallow binding
	 * to them.
	 */
	if (sin6p->sin6_family == AF_INET6 &&
	    IN6_IS_ADDR_MULTICAST(&sin6p->sin6_addr))
		return (EAFNOSUPPORT);

	TCPDEBUG0;
	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("tcp6_usr_bind: inp == NULL"));
	INP_LOCK(inp);
	if (inp->inp_flags & (INP_TIMEWAIT | INP_DROPPED)) {
		error = EINVAL;
		goto out;
	}
	tp = intotcpcb(inp);
	TCPDEBUG1();
	INP_HASH_WLOCK(&V_tcbinfo);
	inp->inp_vflag &= ~INP_IPV4;
	inp->inp_vflag |= INP_IPV6;
#ifdef INET
	if ((inp->inp_flags & IN6P_IPV6_V6ONLY) == 0) {
		if (IN6_IS_ADDR_UNSPECIFIED(&sin6p->sin6_addr))
			inp->inp_vflag |= INP_IPV4;
		else if (IN6_IS_ADDR_V4MAPPED(&sin6p->sin6_addr)) {
			struct bsd_sockaddr_in sin;

			in6_sin6_2_sin(&sin, sin6p);
			inp->inp_vflag |= INP_IPV4;
			inp->inp_vflag &= ~INP_IPV6;
			error = in_pcbbind(inp, (struct bsd_sockaddr *)&sin,
			    td->td_ucred);
			INP_HASH_WUNLOCK(&V_tcbinfo);
			goto out;
		}
	}
#endif
	error = in6_pcbbind(inp, nam, td->td_ucred);
	INP_HASH_WUNLOCK(&V_tcbinfo);
out:
	TCPDEBUG2(PRU_BIND);
	INP_UNLOCK(inp);
	return (error);
}
#endif /* INET6 */

#ifdef INET
/*
 * Prepare to accept connections.
 */
static int
tcp_usr_listen(struct socket *so, int backlog, struct thread *td)
{
	int error = 0;
	struct inpcb *inp;
	struct tcpcb *tp = NULL;

	TCPDEBUG0;
	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("tcp_usr_listen: inp == NULL"));
	INP_LOCK(inp);
	if (inp->inp_flags & (INP_TIMEWAIT | INP_DROPPED)) {
		error = EINVAL;
		goto out;
	}
	tp = intotcpcb(inp);
	TCPDEBUG1();
	SOCK_LOCK(so);
	error = solisten_proto_check(so);
	INP_HASH_WLOCK(&V_tcbinfo);
	if (error == 0 && inp->inp_lport == 0)
		error = in_pcbbind(inp, (struct bsd_sockaddr *)0, 0);
	INP_HASH_WUNLOCK(&V_tcbinfo);
	if (error == 0) {
		tp->set_state(TCPS_LISTEN);
		solisten_proto(so, backlog);
	}
	SOCK_UNLOCK(so);

out:
	TCPDEBUG2(PRU_LISTEN);
	INP_UNLOCK(inp);
	return (error);
}
#endif /* INET */

#ifdef INET6
static int
tcp6_usr_listen(struct socket *so, int backlog, struct thread *td)
{
	int error = 0;
	struct inpcb *inp;
	struct tcpcb *tp = NULL;

	TCPDEBUG0;
	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("tcp6_usr_listen: inp == NULL"));
	INP_LOCK(inp);
	if (inp->inp_flags & (INP_TIMEWAIT | INP_DROPPED)) {
		error = EINVAL;
		goto out;
	}
	tp = intotcpcb(inp);
	TCPDEBUG1();
	SOCK_LOCK(so);
	error = solisten_proto_check(so);
	INP_HASH_WLOCK(&V_tcbinfo);
	if (error == 0 && inp->inp_lport == 0) {
		inp->inp_vflag &= ~INP_IPV4;
		if ((inp->inp_flags & IN6P_IPV6_V6ONLY) == 0)
			inp->inp_vflag |= INP_IPV4;
		error = in6_pcbbind(inp, (struct bsd_sockaddr *)0, td->td_ucred);
	}
	INP_HASH_WUNLOCK(&V_tcbinfo);
	if (error == 0) {
		tp->set_state(TCPS_LISTEN);
		solisten_proto(so, backlog);
	}
	SOCK_UNLOCK(so);

out:
	TCPDEBUG2(PRU_LISTEN);
	INP_UNLOCK(inp);
	return (error);
}
#endif /* INET6 */

#ifdef INET
/*
 * Initiate connection to peer.
 * Create a template for use in transmissions on this connection.
 * Enter SYN_SENT state, and mark socket as connecting.
 * Start keep-alive timer, and seed output sequence space.
 * Send initial segment on connection.
 */
static int
tcp_usr_connect(struct socket *so, struct bsd_sockaddr *nam, struct thread *td)
{
	int error = 0;
	struct inpcb *inp;
	struct tcpcb *tp = NULL;
	struct bsd_sockaddr_in *sinp;

	sinp = (struct bsd_sockaddr_in *)nam;
	if (nam->sa_len != sizeof (*sinp))
		return (EINVAL);
	/*
	 * Must disallow TCP ``connections'' to multicast addresses.
	 */
	if (sinp->sin_family == AF_INET
	    && IN_MULTICAST(ntohl(sinp->sin_addr.s_addr)))
		return (EAFNOSUPPORT);

	TCPDEBUG0;
	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("tcp_usr_connect: inp == NULL"));
	INP_LOCK(inp);
	if (inp->inp_flags & (INP_TIMEWAIT | INP_DROPPED)) {
		error = EINVAL;
		goto out;
	}
	tp = intotcpcb(inp);
	TCPDEBUG1();
	if ((error = tcp_connect(tp, nam, td)) != 0)
		goto out;
	error = tcp_output(tp);
out:
	TCPDEBUG2(PRU_CONNECT);
	INP_UNLOCK(inp);
	return (error);
}
#endif /* INET */

#ifdef INET6
static int
tcp6_usr_connect(struct socket *so, struct bsd_sockaddr *nam, struct thread *td)
{
	int error = 0;
	struct inpcb *inp;
	struct tcpcb *tp = NULL;
	struct bsd_sockaddr_in6 *sin6p;

	TCPDEBUG0;

	sin6p = (struct bsd_sockaddr_in6 *)nam;
	if (nam->sa_len != sizeof (*sin6p))
		return (EINVAL);
	/*
	 * Must disallow TCP ``connections'' to multicast addresses.
	 */
	if (sin6p->sin6_family == AF_INET6
	    && IN6_IS_ADDR_MULTICAST(&sin6p->sin6_addr))
		return (EAFNOSUPPORT);

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("tcp6_usr_connect: inp == NULL"));
	INP_LOCK(inp);
	if (inp->inp_flags & (INP_TIMEWAIT | INP_DROPPED)) {
		error = EINVAL;
		goto out;
	}
	tp = intotcpcb(inp);
	TCPDEBUG1();
#ifdef INET
	/*
	 * XXXRW: Some confusion: V4/V6 flags relate to binding, and
	 * therefore probably require the hash lock, which isn't held here.
	 * Is this a significant problem?
	 */
	if (IN6_IS_ADDR_V4MAPPED(&sin6p->sin6_addr)) {
		struct bsd_sockaddr_in sin;

		if ((inp->inp_flags & IN6P_IPV6_V6ONLY) != 0) {
			error = EINVAL;
			goto out;
		}

		in6_sin6_2_sin(&sin, sin6p);
		inp->inp_vflag |= INP_IPV4;
		inp->inp_vflag &= ~INP_IPV6;
		if ((error = prison_remote_ip4(td->td_ucred,
		    &sin.sin_addr)) != 0)
			goto out;
		if ((error = tcp_connect(tp, (struct bsd_sockaddr *)&sin, td)) != 0)
			goto out;
		error = tcp_output_connect(so, nam);
		goto out;
	}
#endif
	inp->inp_vflag &= ~INP_IPV4;
	inp->inp_vflag |= INP_IPV6;
	inp->inp_inc.inc_flags |= INC_ISIPV6;
	if ((error = prison_remote_ip6(td->td_ucred, &sin6p->sin6_addr)) != 0)
		goto out;
	if ((error = tcp6_connect(tp, nam, td)) != 0)
		goto out;
	error = tcp_output_connect(so, nam);

out:
	TCPDEBUG2(PRU_CONNECT);
	INP_UNLOCK(inp);
	return (error);
}
#endif /* INET6 */

/*
 * Initiate disconnect from peer.
 * If connection never passed embryonic stage, just drop;
 * else if don't need to let data drain, then can just drop anyways,
 * else have to begin TCP shutdown process: mark socket disconnecting,
 * drain unread data, state switch to reflect user close, and
 * send segment (e.g. FIN) to peer.  Socket will be really disconnected
 * when peer sends FIN and acks ours.
 *
 * SHOULD IMPLEMENT LATER PRU_CONNECT VIA REALLOC TCPCB.
 */
static int
tcp_usr_disconnect(struct socket *so)
{
	struct inpcb *inp;
	struct tcpcb *tp = NULL;
	int error = 0;

	TCPDEBUG0;
	INP_INFO_WLOCK(&V_tcbinfo);
	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("tcp_usr_disconnect: inp == NULL"));
	INP_LOCK(inp);
	if (inp->inp_flags & (INP_TIMEWAIT | INP_DROPPED)) {
		error = ECONNRESET;
		goto out;
	}
	tp = intotcpcb(inp);
	TCPDEBUG1();
	tcp_disconnect(tp);
out:
	TCPDEBUG2(PRU_DISCONNECT);
	INP_UNLOCK(inp);
	INP_INFO_WUNLOCK(&V_tcbinfo);
	return (error);
}

#ifdef INET
/*
 * Accept a connection.  Essentially all the work is done at higher levels;
 * just return the address of the peer, storing through addr.
 */
static int
tcp_usr_accept(struct socket *so, struct bsd_sockaddr **nam)
{
	int error = 0;
	struct inpcb *inp = NULL;
	struct tcpcb *tp = NULL;
	struct in_addr addr;
	in_port_t port = 0;
	TCPDEBUG0;

	if (so->so_state & SS_ISDISCONNECTED)
		return (ECONNABORTED);

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("tcp_usr_accept: inp == NULL"));
	INP_LOCK(inp);
	if (inp->inp_flags & (INP_TIMEWAIT | INP_DROPPED)) {
		error = ECONNABORTED;
		goto out;
	}
	tp = intotcpcb(inp);
	TCPDEBUG1();

	/*
	 * We inline in_getpeeraddr and COMMON_END here, so that we can
	 * copy the data of interest and defer the malloc until after we
	 * release the lock.
	 */
	port = inp->inp_fport;
	addr = inp->inp_faddr;

out:
	TCPDEBUG2(PRU_ACCEPT);
	INP_UNLOCK(inp);
	if (error == 0)
		*nam = in_sockaddr(port, &addr);
	return error;
}
#endif /* INET */

#ifdef INET6
static int
tcp6_usr_accept(struct socket *so, struct bsd_sockaddr **nam)
{
	struct inpcb *inp = NULL;
	int error = 0;
	struct tcpcb *tp = NULL;
	struct in_addr addr;
	struct in6_addr addr6;
	in_port_t port = 0;
	int v4 = 0;
	TCPDEBUG0;

	if (so->so_state & SS_ISDISCONNECTED)
		return (ECONNABORTED);

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("tcp6_usr_accept: inp == NULL"));
	INP_INFO_RLOCK(&V_tcbinfo);
	INP_LOCK(inp);
	if (inp->inp_flags & (INP_TIMEWAIT | INP_DROPPED)) {
		error = ECONNABORTED;
		goto out;
	}
	tp = intotcpcb(inp);
	TCPDEBUG1();

	/*
	 * We inline in6_mapped_peeraddr and COMMON_END here, so that we can
	 * copy the data of interest and defer the malloc until after we
	 * release the lock.
	 */
	if (inp->inp_vflag & INP_IPV4) {
		v4 = 1;
		port = inp->inp_fport;
		addr = inp->inp_faddr;
	} else {
		port = inp->inp_fport;
		addr6 = inp->in6p_faddr;
	}

out:
	TCPDEBUG2(PRU_ACCEPT);
	INP_UNLOCK(inp);
	INP_INFO_RUNLOCK(&V_tcbinfo);
	if (error == 0) {
		if (v4)
			*nam = in6_v4mapsin6_sockaddr(port, &addr);
		else
			*nam = in6_sockaddr(port, &addr6);
	}
	return error;
}
#endif /* INET6 */

/*
 * Mark the connection as being incapable of further output.
 */
static int
tcp_usr_shutdown(struct socket *so)
{
	int error = 0;
	struct inpcb *inp;
	struct tcpcb *tp = NULL;

	TCPDEBUG0;
	INP_INFO_WLOCK(&V_tcbinfo);
	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("inp == NULL"));
	INP_LOCK(inp);
	if (inp->inp_flags & (INP_TIMEWAIT | INP_DROPPED)) {
		error = ECONNRESET;
		goto out;
	}
	tp = intotcpcb(inp);
	TCPDEBUG1();
	socantsendmore_locked(so);
	tcp_usrclosed(tp);
	if (!(inp->inp_flags & INP_DROPPED))
		error = tcp_output(tp);

out:
	TCPDEBUG2(PRU_SHUTDOWN);
	INP_UNLOCK(inp);
	INP_INFO_WUNLOCK(&V_tcbinfo);

	return (error);
}

/*
 * After a receive, possibly send window update to peer.
 */
static int
tcp_usr_rcvd(struct socket *so, int flags)
{
	struct inpcb *inp;
	struct tcpcb *tp = NULL;
	int error = 0;

	TCPDEBUG0;
	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("tcp_usr_rcvd: inp == NULL"));
	INP_LOCK(inp);
	if (inp->inp_flags & (INP_TIMEWAIT | INP_DROPPED)) {
		error = ECONNRESET;
		goto out;
	}
	tp = intotcpcb(inp);
	TCPDEBUG1();
	tcp_output(tp);

out:
	TCPDEBUG2(PRU_RCVD);
	INP_UNLOCK(inp);
	return (error);
}

/*
 * Do a send by putting data in output queue and updating urgent
 * marker if URG set.  Possibly send more data.  Unlike the other
 * pru_*() routines, the mbuf chains are our responsibility.  We
 * must either enqueue them or free them.  The other pru_* routines
 * generally are caller-frees.
 */
static int
tcp_usr_send(struct socket *so, int flags, struct mbuf *m,
    struct bsd_sockaddr *nam, struct mbuf *control, struct thread *td)
{
	int error = 0;
	struct inpcb *inp;
	struct tcpcb *tp = NULL;
#ifdef INET6
	int isipv6;
#endif
	TCPDEBUG0;
    debug( "%03d|---> tcp_usr_send: mbuf length=%d\n",
		   sched::thread::current()->id(), m->m_hdr.mh_len);

	/*
	 * We require the pcbinfo lock if we will close the socket as part of
	 * this call.
	 */
	if (flags & PRUS_EOF)
		INP_INFO_WLOCK(&V_tcbinfo);
	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("tcp_usr_send: inp == NULL"));
	INP_LOCK(inp);
	if (inp->inp_flags & (INP_TIMEWAIT | INP_DROPPED)) {
		if (control)
			m_freem(control);
		if (m)
			m_freem(m);
		error = ECONNRESET;
		goto out;
	}
#ifdef INET6
	isipv6 = nam && nam->sa_family == AF_INET6;
#endif /* INET6 */
	tp = intotcpcb(inp);
	TCPDEBUG1();
	if (control) {
		/* TCP doesn't do control messages (rights, creds, etc) */
		if (control->m_hdr.mh_len) {
			m_freem(control);
			if (m)
				m_freem(m);
			error = EINVAL;
			goto out;
		}
		m_freem(control);	/* empty control, just free it */
	}
	if (!(flags & PRUS_OOB)) {
		sbappendstream(so, &so->so_snd, m);
		if (nam && tp->get_state() < TCPS_SYN_SENT) {
			/*
			 * Do implied connect if not yet connected,
			 * initialize window to default value, and
			 * initialize maxseg/maxopd using peer's cached
			 * MSS.
			 */
#ifdef INET6
			if (isipv6)
				error = tcp6_connect(tp, nam, td);
#endif /* INET6 */
#if defined(INET6) && defined(INET)
			else
#endif
#ifdef INET
				error = tcp_connect(tp, nam, td);
#endif
			if (error)
				goto out;
			tp->snd_wnd = TTCP_CLIENT_SND_WND;
			tcp_mss(tp, -1);
		}
		if (flags & PRUS_EOF) {
			/*
			 * Close the send side of the connection after
			 * the data is sent.
			 */
			INP_INFO_WLOCK_ASSERT(&V_tcbinfo);
			socantsendmore_locked(so);
			tcp_usrclosed(tp);
		}
		if (!(inp->inp_flags & INP_DROPPED)) {
			if (flags & PRUS_MORETOCOME)
				tp->t_flags |= TF_MORETOCOME;
			error = tcp_output(tp);
			if (flags & PRUS_MORETOCOME)
				tp->t_flags &= ~TF_MORETOCOME;
		}
	} else {
		/*
		 * XXXRW: PRUS_EOF not implemented with PRUS_OOB?
		 */
		if (sbspace(&so->so_snd) < -512) {
			SOCK_UNLOCK(so);
			m_freem(m);
			error = ENOBUFS;
			goto out;
		}
		/*
		 * According to RFC961 (Assigned Protocols),
		 * the urgent pointer points to the last octet
		 * of urgent data.  We continue, however,
		 * to consider it to indicate the first octet
		 * of data past the urgent section.
		 * Otherwise, snd_up should be one lower.
		 */
		sbappendstream_locked(so, &so->so_snd, m);
		if (nam && tp->get_state() < TCPS_SYN_SENT) {
			/*
			 * Do implied connect if not yet connected,
			 * initialize window to default value, and
			 * initialize maxseg/maxopd using peer's cached
			 * MSS.
			 */
#ifdef INET6
			if (isipv6)
				error = tcp6_connect(tp, nam, td);
#endif /* INET6 */
#if defined(INET6) && defined(INET)
			else
#endif
#ifdef INET
				error = tcp_connect(tp, nam, td);
#endif
			if (error)
				goto out;
			tp->snd_wnd = TTCP_CLIENT_SND_WND;
			tcp_mss(tp, -1);
		}
		tp->snd_up = tp->snd_una + so->so_snd.sb_cc;
		tp->t_flags |= TF_FORCEDATA;
		error = tcp_output(tp);
		tp->t_flags &= ~TF_FORCEDATA;
	}
out:
	TCPDEBUG2((flags & PRUS_OOB) ? PRU_SENDOOB :
		  ((flags & PRUS_EOF) ? PRU_SEND_EOF : PRU_SEND));
	INP_UNLOCK(inp);
	if (flags & PRUS_EOF)
		INP_INFO_WUNLOCK(&V_tcbinfo);
	return (error);
}

/*
 * Abort the TCP.  Drop the connection abruptly.
 */
static void
tcp_usr_abort(struct socket *so)
{
	struct inpcb *inp;
	struct tcpcb *tp = NULL;
	TCPDEBUG0;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("tcp_usr_abort: inp == NULL"));

	INP_INFO_WLOCK(&V_tcbinfo);
	INP_LOCK(inp);
	KASSERT(inp->inp_socket != NULL,
	    ("tcp_usr_abort: inp_socket == NULL"));

	/*
	 * If we still have full TCP state, and we're not dropped, drop.
	 */
	if (!(inp->inp_flags & INP_TIMEWAIT) &&
	    !(inp->inp_flags & INP_DROPPED)) {
		tp = intotcpcb(inp);
		TCPDEBUG1();
		tcp_drop(tp, ECONNABORTED);
		TCPDEBUG2(PRU_ABORT);
	}
	if (!(inp->inp_flags & INP_DROPPED)) {
		so->so_state |= SS_PROTOREF;
		inp->inp_flags |= INP_SOCKREF;
	}
	INP_UNLOCK(inp);
	INP_INFO_WUNLOCK(&V_tcbinfo);
}

/*
 * TCP socket is closed.  Start friendly disconnect.
 */
static void
tcp_usr_close(struct socket *so)
{
	struct inpcb *inp;
	struct tcpcb *tp = NULL;
	TCPDEBUG0;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("tcp_usr_close: inp == NULL"));

	INP_INFO_WLOCK(&V_tcbinfo);
	INP_LOCK(inp);
	KASSERT(inp->inp_socket != NULL,
	    ("tcp_usr_close: inp_socket == NULL"));

	/*
	 * If we still have full TCP state, and we're not dropped, initiate
	 * a disconnect.
	 */
	if (!(inp->inp_flags & INP_TIMEWAIT) &&
	    !(inp->inp_flags & INP_DROPPED)) {
		tp = intotcpcb(inp);
		tcp_teardown_net_channel(tp);
		TCPDEBUG1();
		tcp_disconnect(tp);
		TCPDEBUG2(PRU_CLOSE);
	}
	if (!(inp->inp_flags & INP_DROPPED)) {
		SOCK_LOCK(so);
		so->so_state |= SS_PROTOREF;
		SOCK_UNLOCK(so);
		inp->inp_flags |= INP_SOCKREF;
	}
	INP_UNLOCK(inp);
	INP_INFO_WUNLOCK(&V_tcbinfo);
}

/*
 * Receive out-of-band data.
 */
static int
tcp_usr_rcvoob(struct socket *so, struct mbuf *m, int flags)
{
	int error = 0;
	struct inpcb *inp;
	struct tcpcb *tp = NULL;

	TCPDEBUG0;
	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("tcp_usr_rcvoob: inp == NULL"));
	INP_LOCK(inp);
	if (inp->inp_flags & (INP_TIMEWAIT | INP_DROPPED)) {
		error = ECONNRESET;
		goto out;
	}
	tp = intotcpcb(inp);
	TCPDEBUG1();
	if ((so->so_oobmark == 0 &&
	     (so->so_rcv.sb_state & SBS_RCVATMARK) == 0) ||
	    so->so_options & SO_OOBINLINE ||
	    tp->t_oobflags & TCPOOB_HADDATA) {
		error = EINVAL;
		goto out;
	}
	if ((tp->t_oobflags & TCPOOB_HAVEDATA) == 0) {
		error = EWOULDBLOCK;
		goto out;
	}
	m->m_hdr.mh_len = 1;
	*mtod(m, caddr_t) = tp->t_iobc;
	if ((flags & MSG_PEEK) == 0)
		tp->t_oobflags ^= (TCPOOB_HAVEDATA | TCPOOB_HADDATA);

out:
	TCPDEBUG2(PRU_RCVOOB);
	INP_UNLOCK(inp);
	return (error);
}

#ifdef INET
struct pr_usrreqs tcp_usrreqs = initialize_with([] (pr_usrreqs& x) {
	x.pru_abort =		tcp_usr_abort;
	x.pru_accept =		tcp_usr_accept;
	x.pru_attach =		tcp_usr_attach;
	x.pru_bind =		tcp_usr_bind;
	x.pru_connect =		tcp_usr_connect;
	x.pru_control =		in_control;
	x.pru_detach =		tcp_usr_detach;
	x.pru_disconnect =	tcp_usr_disconnect;
	x.pru_listen =		tcp_usr_listen;
	x.pru_peeraddr =		in_getpeeraddr;
	x.pru_rcvd =		tcp_usr_rcvd;
	x.pru_rcvoob =		tcp_usr_rcvoob;
	x.pru_send =		tcp_usr_send;
	x.pru_shutdown =		tcp_usr_shutdown;
	x.pru_sockaddr =		in_getsockaddr;
	x.pru_sosetlabel =	in_pcbsosetlabel;
	x.pru_close =		tcp_usr_close;
});
#endif /* INET */

#ifdef INET6
struct pr_usrreqs tcp6_usrreqs = {
	.pru_abort =		tcp_usr_abort,
	.pru_accept =		tcp6_usr_accept,
	.pru_attach =		tcp_usr_attach,
	.pru_bind =		tcp6_usr_bind,
	.pru_connect =		tcp6_usr_connect,
	.pru_control =		in6_control,
	.pru_detach =		tcp_usr_detach,
	.pru_disconnect =	tcp_usr_disconnect,
	.pru_listen =		tcp6_usr_listen,
	.pru_peeraddr =		in6_mapped_peeraddr,
	.pru_rcvd =		tcp_usr_rcvd,
	.pru_rcvoob =		tcp_usr_rcvoob,
	.pru_send =		tcp_usr_send,
	.pru_shutdown =		tcp_usr_shutdown,
	.pru_sockaddr =		in6_mapped_sockaddr,
	.pru_sosetlabel =	in_pcbsosetlabel,
	.pru_close =		tcp_usr_close,
};
#endif /* INET6 */

#ifdef INET
/*
 * Common subroutine to open a TCP connection to remote host specified
 * by struct bsd_sockaddr_in in mbuf *nam.  Call in_pcbbind to assign a local
 * port number if needed.  Call in_pcbconnect_setup to do the routing and
 * to choose a local host address (interface).  If there is an existing
 * incarnation of the same connection in TIME-WAIT state and if the remote
 * host was sending CC options and if the connection duration was < MSL, then
 * truncate the previous TIME-WAIT state and proceed.
 * Initialize connection parameters and enter SYN-SENT state.
 */
static int
tcp_connect(struct tcpcb *tp, struct bsd_sockaddr *nam, struct thread *td)
{
	struct inpcb *inp = tp->t_inpcb, *oinp;
	struct socket *so = inp->inp_socket;
	struct in_addr laddr;
	u_short lport;
	int error;

	INP_LOCK_ASSERT(inp);
	INP_HASH_WLOCK(&V_tcbinfo);

	if (inp->inp_lport == 0) {
		error = in_pcbbind(inp, (struct bsd_sockaddr *)0, 0);
		if (error)
			goto out;
	}

	/*
	 * Cannot simply call in_pcbconnect, because there might be an
	 * earlier incarnation of this same connection still in
	 * TIME_WAIT state, creating an ADDRINUSE error.
	 */
	laddr = inp->inp_laddr;
	lport = inp->inp_lport;
	error = in_pcbconnect_setup(inp, nam, &laddr.s_addr, &lport,
	    &inp->inp_faddr.s_addr, &inp->inp_fport, &oinp, 0);
	if (error && oinp == NULL)
		goto out;
	if (oinp) {
		error = EADDRINUSE;
		goto out;
	}
	inp->inp_laddr = laddr;
	in_pcbrehash(inp);
	INP_HASH_WUNLOCK(&V_tcbinfo);

	/*
	 * Compute window scaling to request:
	 * Scale to fit into sweet spot.  See tcp_syncache.c.
	 * XXX: This should move to tcp_output().
	 */
	while (tp->request_r_scale < TCP_MAX_WINSHIFT &&
	    (TCP_MAXWIN << tp->request_r_scale) < sb_max)
		tp->request_r_scale++;

	soisconnecting(so);
	TCPSTAT_INC(tcps_connattempt);
	tp->set_state(TCPS_SYN_SENT);
	tcp_timer_activate(tp, TT_KEEP, TP_KEEPINIT(tp));
	tp->iss = tcp_new_isn(tp);
	tcp_sendseqinit(tp);

	return 0;

out:
	INP_HASH_WUNLOCK(&V_tcbinfo);
	return (error);
}
#endif /* INET */

#ifdef INET6
static int
tcp6_connect(struct tcpcb *tp, struct bsd_sockaddr *nam, struct thread *td)
{
	struct inpcb *inp = tp->t_inpcb, *oinp;
	struct socket *so = inp->inp_socket;
	struct bsd_sockaddr_in6 *sin6 = (struct bsd_sockaddr_in6 *)nam;
	struct in6_addr addr6;
	int error;

	INP_LOCK_ASSERT(inp);
	INP_HASH_WLOCK(&V_tcbinfo);

	if (inp->inp_lport == 0) {
		error = in6_pcbbind(inp, (struct bsd_sockaddr *)0, td->td_ucred);
		if (error)
			goto out;
	}

	/*
	 * Cannot simply call in_pcbconnect, because there might be an
	 * earlier incarnation of this same connection still in
	 * TIME_WAIT state, creating an ADDRINUSE error.
	 * in6_pcbladdr() also handles scope zone IDs.
	 *
	 * XXXRW: We wouldn't need to expose in6_pcblookup_hash_locked()
	 * outside of in6_pcb.c if there were an in6_pcbconnect_setup().
	 */
	error = in6_pcbladdr(inp, nam, &addr6);
	if (error)
		goto out;
	oinp = in6_pcblookup_hash_locked(inp->inp_pcbinfo,
				  &sin6->sin6_addr, sin6->sin6_port,
				  IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_laddr)
				  ? &addr6
				  : &inp->in6p_laddr,
				  inp->inp_lport,  0, NULL);
	if (oinp) {
		error = EADDRINUSE;
		goto out;
	}
	if (IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_laddr))
		inp->in6p_laddr = addr6;
	inp->in6p_faddr = sin6->sin6_addr;
	inp->inp_fport = sin6->sin6_port;
	/* update flowinfo - draft-itojun-ipv6-flowlabel-api-00 */
	inp->inp_flow &= ~IPV6_FLOWLABEL_MASK;
	if (inp->inp_flags & IN6P_AUTOFLOWLABEL)
		inp->inp_flow |=
		    (htonl(ip6_randomflowlabel()) & IPV6_FLOWLABEL_MASK);
	in_pcbrehash(inp);
	INP_HASH_WUNLOCK(&V_tcbinfo);

	/* Compute window scaling to request.  */
	while (tp->request_r_scale < TCP_MAX_WINSHIFT &&
	    (TCP_MAXWIN << tp->request_r_scale) < sb_max)
		tp->request_r_scale++;

	soisconnecting(so);
	TCPSTAT_INC(tcps_connattempt);
	tp->set_state(TCPS_SYN_SENT);
	tcp_timer_activate(tp, TT_KEEP, TP_KEEPINIT(tp));
	tp->iss = tcp_new_isn(tp);
	tcp_sendseqinit(tp);

	return 0;

out:
	INP_HASH_WUNLOCK(&V_tcbinfo);
	return error;
}
#endif /* INET6 */

/*
 * Export TCP internal state information via a struct tcp_info, based on the
 * Linux 2.6 API.  Not ABI compatible as our constants are mapped differently
 * (TCP state machine, etc).  We export all information using FreeBSD-native
 * constants -- for example, the numeric values for tcpi_state will differ
 * from Linux.
 */
static void
tcp_fill_info(struct tcpcb *tp, struct tcp_info *ti)
{

	INP_LOCK_ASSERT(tp->t_inpcb);
	bzero(ti, sizeof(*ti));

	ti->tcpi_state = tp->get_state();
	if ((tp->t_flags & TF_REQ_TSTMP) && (tp->t_flags & TF_RCVD_TSTMP))
		ti->tcpi_options |= TCPI_OPT_TIMESTAMPS;
	if (tp->t_flags & TF_SACK_PERMIT)
		ti->tcpi_options |= TCPI_OPT_SACK;
	if ((tp->t_flags & TF_REQ_SCALE) && (tp->t_flags & TF_RCVD_SCALE)) {
		ti->tcpi_options |= TCPI_OPT_WSCALE;
		ti->tcpi_snd_wscale = tp->snd_scale;
		ti->tcpi_rcv_wscale = tp->rcv_scale;
	}

	ti->tcpi_rto = tp->t_rxtcur * tick;
	ti->tcpi_last_data_recv = (long)(bsd_ticks - (int)tp->t_rcvtime) * tick;
	ti->tcpi_rtt = ((u_int64_t)tp->t_srtt * tick) >> TCP_RTT_SHIFT;
	ti->tcpi_rttvar = ((u_int64_t)tp->t_rttvar * tick) >> TCP_RTTVAR_SHIFT;

	ti->tcpi_snd_ssthresh = tp->snd_ssthresh;
	ti->tcpi_snd_cwnd = tp->snd_cwnd;

	/*
	 * FreeBSD-specific extension fields for tcp_info.
	 */
	ti->tcpi_rcv_space = tp->rcv_wnd;
	ti->tcpi_rcv_nxt = tp->rcv_nxt.raw();
	ti->tcpi_snd_wnd = tp->snd_wnd;
	ti->tcpi_snd_bwnd = 0;		/* Unused, kept for compat. */
	ti->tcpi_snd_nxt = tp->snd_nxt.raw();
	ti->tcpi_snd_mss = tp->t_maxseg;
	ti->tcpi_rcv_mss = tp->t_maxseg;
	if (tp->t_flags & TF_TOE)
		ti->tcpi_options |= TCPI_OPT_TOE;
	ti->tcpi_snd_rexmitpack = tp->t_sndrexmitpack;
	ti->tcpi_rcv_ooopack = tp->t_rcvoopack;
	ti->tcpi_snd_zerowin = tp->t_sndzerowin;
}

/*
 * tcp_ctloutput() must drop the inpcb lock before performing copyin on
 * socket option arguments.  When it re-acquires the lock after the copy, it
 * has to revalidate that the connection is still valid for the socket
 * option.
 */
#define INP_LOCK_RECHECK(inp) do {					\
	INP_LOCK(inp);							\
	if (inp->inp_flags & (INP_TIMEWAIT | INP_DROPPED)) {		\
		INP_UNLOCK(inp);					\
		return (ECONNRESET);					\
	}								\
	tp = intotcpcb(inp);						\
} while(0)

int
tcp_ctloutput(struct socket *so, struct sockopt *sopt)
{
	int	error, opt, optval;
	u_int	ui;
	struct	inpcb *inp;
	struct	tcpcb *tp;
	struct	tcp_info ti;
	char buf[TCP_CA_NAME_MAX];
	struct cc_algo *algo;

	error = 0;
	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("tcp_ctloutput: inp == NULL"));
	INP_LOCK(inp);
	if (sopt->sopt_level != IPPROTO_TCP) {
#ifdef INET6
		if (inp->inp_vflag & INP_IPV6PROTO) {
			INP_UNLOCK(inp);
			error = ip6_ctloutput(so, sopt);
		}
#endif /* INET6 */
#if defined(INET6) && defined(INET)
		else
#endif
#ifdef INET
		{
			INP_UNLOCK(inp);
			error = ip_ctloutput(so, sopt);
		}
#endif
		return (error);
	}
	if (inp->inp_flags & (INP_TIMEWAIT | INP_DROPPED)) {
		INP_UNLOCK(inp);
		return (ECONNRESET);
	}

	switch (sopt->sopt_dir) {
	case SOPT_SET:
		switch (sopt->sopt_name) {
#ifdef TCP_SIGNATURE
		case TCP_MD5SIG:
			INP_UNLOCK(inp);
			error = sooptcopyin(sopt, &optval, sizeof optval,
			    sizeof optval);
			if (error)
				return (error);

			INP_LOCK_RECHECK(inp);
			if (optval > 0)
				tp->t_flags |= TF_SIGNATURE;
			else
				tp->t_flags &= ~TF_SIGNATURE;
			INP_UNLOCK(inp);
			break;
#endif /* TCP_SIGNATURE */
		case TCP_NODELAY:
		case TCP_NOOPT:
			INP_UNLOCK(inp);
			error = sooptcopyin(sopt, &optval, sizeof optval,
			    sizeof optval);
			if (error)
				return (error);

			INP_LOCK_RECHECK(inp);
			switch (sopt->sopt_name) {
			case TCP_NODELAY:
				opt = TF_NODELAY;
				break;
			case TCP_NOOPT:
				opt = TF_NOOPT;
				break;
			default:
				opt = 0; /* dead code to fool gcc */
				break;
			}

			if (optval)
				tp->t_flags |= opt;
			else
				tp->t_flags &= ~opt;
			INP_UNLOCK(inp);
			break;

		case TCP_NOPUSH:
			INP_UNLOCK(inp);
			error = sooptcopyin(sopt, &optval, sizeof optval,
			    sizeof optval);
			if (error)
				return (error);

			INP_LOCK_RECHECK(inp);
			if (optval)
				tp->t_flags |= TF_NOPUSH;
			else if (tp->t_flags & TF_NOPUSH) {
				tp->t_flags &= ~TF_NOPUSH;
				if (TCPS_HAVEESTABLISHED(tp->get_state()))
					error = tcp_output(tp);
			}
			INP_UNLOCK(inp);
			break;

		case TCP_MAXSEG:
			INP_UNLOCK(inp);
			error = sooptcopyin(sopt, &optval, sizeof optval,
			    sizeof optval);
			if (error)
				return (error);

			INP_LOCK_RECHECK(inp);
			if (optval > 0 && optval <= tp->t_maxseg &&
			    optval + 40 >= V_tcp_minmss)
				tp->t_maxseg = optval;
			else
				error = EINVAL;
			INP_UNLOCK(inp);
			break;

		case TCP_INFO:
			INP_UNLOCK(inp);
			error = EINVAL;
			break;

		case TCP_CONGESTION:
			INP_UNLOCK(inp);
			bzero(buf, sizeof(buf));
			error = sooptcopyin(sopt, &buf, sizeof(buf), 1);
			if (error)
				break;
			INP_LOCK_RECHECK(inp);
			/*
			 * Return EINVAL if we can't find the requested cc algo.
			 */
			error = EINVAL;
			CC_LIST_RLOCK();
			STAILQ_FOREACH(algo, &cc_list, entries) {
				if (strncmp(buf, algo->name, TCP_CA_NAME_MAX)
				    == 0) {
					/* We've found the requested algo. */
					error = 0;
					/*
					 * We hold a write lock over the tcb
					 * so it's safe to do these things
					 * without ordering concerns.
					 */
					if (CC_ALGO(tp)->cb_destroy != NULL)
						CC_ALGO(tp)->cb_destroy(tp->ccv);
					CC_ALGO(tp) = algo;
					/*
					 * If something goes pear shaped
					 * initialising the new algo,
					 * fall back to newreno (which
					 * does not require initialisation).
					 */
					if (algo->cb_init != NULL)
						if (algo->cb_init(tp->ccv) > 0) {
							CC_ALGO(tp) = &newreno_cc_algo;
							/*
							 * The only reason init
							 * should fail is
							 * because of malloc.
							 */
							error = ENOMEM;
						}
					break; /* Break the STAILQ_FOREACH. */
				}
			}
			CC_LIST_RUNLOCK();
			INP_UNLOCK(inp);
			break;

		case TCP_KEEPIDLE:
		case TCP_KEEPINTVL:
		case TCP_KEEPINIT:
			INP_UNLOCK(inp);
			error = sooptcopyin(sopt, &ui, sizeof(ui), sizeof(ui));
			if (error)
				return (error);

			if (ui > (UINT_MAX / hz)) {
				error = EINVAL;
				break;
			}
			ui *= hz;

			INP_LOCK_RECHECK(inp);
			switch (sopt->sopt_name) {
			case TCP_KEEPIDLE:
				tp->t_keepidle = ui;
				/*
				 * XXX: better check current remaining
				 * timeout and "merge" it with new value.
				 */
				if ((tp->get_state() > TCPS_LISTEN) &&
				    (tp->get_state() <= TCPS_CLOSING))
					tcp_timer_activate(tp, TT_KEEP,
					    TP_KEEPIDLE(tp));
				break;
			case TCP_KEEPINTVL:
				tp->t_keepintvl = ui;
				if ((tp->get_state() == TCPS_FIN_WAIT_2) &&
				    (TP_MAXIDLE(tp) > 0))
					tcp_timer_activate(tp, TT_2MSL,
					    TP_MAXIDLE(tp));
				break;
			case TCP_KEEPINIT:
				tp->t_keepinit = ui;
				if (tp->get_state() == TCPS_SYN_RECEIVED ||
				    tp->get_state() == TCPS_SYN_SENT)
					tcp_timer_activate(tp, TT_KEEP,
					    TP_KEEPINIT(tp));
				break;
			}
			INP_UNLOCK(inp);
			break;

		case TCP_KEEPCNT:
			INP_UNLOCK(inp);
			error = sooptcopyin(sopt, &ui, sizeof(ui), sizeof(ui));
			if (error)
				return (error);

			INP_LOCK_RECHECK(inp);
			tp->t_keepcnt = ui;
			if ((tp->get_state() == TCPS_FIN_WAIT_2) &&
			    (TP_MAXIDLE(tp) > 0))
				tcp_timer_activate(tp, TT_2MSL,
				    TP_MAXIDLE(tp));
			INP_UNLOCK(inp);
			break;

		default:
			INP_UNLOCK(inp);
			error = ENOPROTOOPT;
			break;
		}
		break;

	case SOPT_GET:
		tp = intotcpcb(inp);
		switch (sopt->sopt_name) {
#ifdef TCP_SIGNATURE
		case TCP_MD5SIG:
			optval = (tp->t_flags & TF_SIGNATURE) ? 1 : 0;
			INP_UNLOCK(inp);
			error = sooptcopyout(sopt, &optval, sizeof optval);
			break;
#endif

		case TCP_NODELAY:
			optval = tp->t_flags & TF_NODELAY;
			INP_UNLOCK(inp);
			error = sooptcopyout(sopt, &optval, sizeof optval);
			break;
		case TCP_MAXSEG:
			optval = tp->t_maxseg;
			INP_UNLOCK(inp);
			error = sooptcopyout(sopt, &optval, sizeof optval);
			break;
		case TCP_NOOPT:
			optval = tp->t_flags & TF_NOOPT;
			INP_UNLOCK(inp);
			error = sooptcopyout(sopt, &optval, sizeof optval);
			break;
		case TCP_NOPUSH:
			optval = tp->t_flags & TF_NOPUSH;
			INP_UNLOCK(inp);
			error = sooptcopyout(sopt, &optval, sizeof optval);
			break;
		case TCP_INFO:
			tcp_fill_info(tp, &ti);
			INP_UNLOCK(inp);
			error = sooptcopyout(sopt, &ti, sizeof ti);
			break;
		case TCP_CONGESTION:
			bzero(buf, sizeof(buf));
			strlcpy(buf, CC_ALGO(tp)->name, TCP_CA_NAME_MAX);
			INP_UNLOCK(inp);
			error = sooptcopyout(sopt, buf, TCP_CA_NAME_MAX);
			break;
		case TCP_KEEPINTVL:
			optval = tp->t_keepintvl;
			INP_UNLOCK(inp);
			error = sooptcopyout(sopt, &optval, sizeof optval);
			break;
		case TCP_KEEPIDLE:
			optval = tp->t_keepidle;
			INP_UNLOCK(inp);
			error = sooptcopyout(sopt, &optval, sizeof optval);
			break;
		case TCP_KEEPINIT:
			optval = tp->t_keepinit;
			INP_UNLOCK(inp);
			error = sooptcopyout(sopt, &optval, sizeof optval);
			break;
		default:
			INP_UNLOCK(inp);
			error = ENOPROTOOPT;
			break;
		}
		break;
	}
	return (error);
}
#undef INP_LOCK_RECHECK

/*
 * tcp_sendspace and tcp_recvspace are the default send and receive window
 * sizes, respectively.  These are obsolescent (this information should
 * be set by the route).
 */
u_long	tcp_sendspace = 1024*32;
SYSCTL_ULONG(_net_inet_tcp, TCPCTL_SENDSPACE, sendspace, CTLFLAG_RW,
    &tcp_sendspace , 0, "Maximum outgoing TCP datagram size");
u_long	tcp_recvspace = 1024*64;
SYSCTL_ULONG(_net_inet_tcp, TCPCTL_RECVSPACE, recvspace, CTLFLAG_RW,
    &tcp_recvspace , 0, "Maximum incoming TCP datagram size");

/*
 * Attach TCP protocol to socket, allocating
 * internet protocol control block, tcp control block,
 * bufer space, and entering LISTEN state if to accept connections.
 */
static int
tcp_attach(struct socket *so)
{
	struct tcpcb *tp;
	struct inpcb *inp;
	int error;

	if (so->so_snd.sb_hiwat == 0 || so->so_rcv.sb_hiwat == 0) {
		error = soreserve_internal(so, tcp_sendspace, tcp_recvspace);
		if (error)
			return (error);
	}
	so->so_rcv.sb_flags |= SB_AUTOSIZE;
	so->so_snd.sb_flags |= SB_AUTOSIZE;
	INP_INFO_WLOCK(&V_tcbinfo);
	inp = new inpcb(so, &V_tcbinfo);
#ifdef INET6
	if (inp->inp_vflag & INP_IPV6PROTO) {
		inp->inp_vflag |= INP_IPV6;
		inp->in6p_hops = -1;	/* use kernel default */
	}
	else
#endif
	inp->inp_vflag |= INP_IPV4;
	tp = tcp_newtcpcb(inp);
	if (tp == NULL) {
		in_pcbdetach(inp);
		in_pcbfree(inp);
		INP_INFO_WUNLOCK(&V_tcbinfo);
		return (ENOBUFS);
	}
	tp->set_state(TCPS_CLOSED);
	INP_UNLOCK(inp);
	INP_INFO_WUNLOCK(&V_tcbinfo);
	return (0);
}

/*
 * Initiate (or continue) disconnect.
 * If embryonic state, just send reset (once).
 * If in ``let data drain'' option and linger null, just drop.
 * Otherwise (hard), mark socket disconnecting and drop
 * current input data; switch states based on user close, and
 * send segment to peer (with FIN).
 */
static void
tcp_disconnect(struct tcpcb *tp)
{
	struct inpcb *inp = tp->t_inpcb;
	struct socket *so = inp->inp_socket;

	INP_INFO_WLOCK_ASSERT(&V_tcbinfo);
	INP_LOCK_ASSERT(inp);

	/*
	 * Neither tcp_close() nor tcp_drop() should return NULL, as the
	 * socket is still open.
	 */
	if (tp->get_state() < TCPS_ESTABLISHED) {
		tp = tcp_close(tp);
		KASSERT(tp != NULL,
		    ("tcp_disconnect: tcp_close() returned NULL"));
	} else if ((so->so_options & SO_LINGER) && so->so_linger == 0) {
		tp = tcp_drop(tp, 0);
		KASSERT(tp != NULL,
		    ("tcp_disconnect: tcp_drop() returned NULL"));
	} else {
		soisdisconnecting(so);
		sbflush(so, &so->so_rcv);
		tcp_usrclosed(tp);
		if (!(inp->inp_flags & INP_DROPPED))
			tcp_output(tp);
	}
}

/*
 * User issued close, and wish to trail through shutdown states:
 * if never received SYN, just forget it.  If got a SYN from peer,
 * but haven't sent FIN, then go to FIN_WAIT_1 state to send peer a FIN.
 * If already got a FIN from peer, then almost done; go to LAST_ACK
 * state.  In all other cases, have already sent FIN to peer (e.g.
 * after PRU_SHUTDOWN), and just have to play tedious game waiting
 * for peer to send FIN or not respond to keep-alives, etc.
 * We can let the user exit from the close as soon as the FIN is acked.
 */
static void
tcp_usrclosed(struct tcpcb *tp)
{

	INP_INFO_WLOCK_ASSERT(&V_tcbinfo);
	INP_LOCK_ASSERT(tp->t_inpcb);

	tcp_teardown_net_channel(tp);

	switch (tp->get_state()) {
	case TCPS_LISTEN:
		/* FALLTHROUGH */
	case TCPS_CLOSED:
		tp->set_state(TCPS_CLOSED);
		tp = tcp_close(tp);
		/*
		 * tcp_close() should never return NULL here as the socket is
		 * still open.
		 */
		KASSERT(tp != NULL,
		    ("tcp_usrclosed: tcp_close() returned NULL"));
		break;

	case TCPS_SYN_SENT:
	case TCPS_SYN_RECEIVED:
		tp->t_flags |= TF_NEEDFIN;
		break;

	case TCPS_ESTABLISHED:
		tp->set_state(TCPS_FIN_WAIT_1);
		break;

	case TCPS_CLOSE_WAIT:
		tp->set_state(TCPS_LAST_ACK);
		break;
	}
	if (tp->get_state() >= TCPS_FIN_WAIT_2) {
		soisdisconnected(tp->t_inpcb->inp_socket);
		/* Prevent the connection hanging in FIN_WAIT_2 forever. */
		if (tp->get_state() == TCPS_FIN_WAIT_2) {
			int timeout;

			timeout = (tcp_fast_finwait2_recycle) ? 
			    tcp_finwait2_timeout : TP_MAXIDLE(tp);
			tcp_timer_activate(tp, TT_2MSL, timeout);
		}
	}
}

//#ifdef DDB
static void
db_print_indent(int indent)
{
	int i;

	for (i = 0; i < indent; i++)
		debugf(" ");
}

static void
db_print_tstate(int t_state)
{

	switch (t_state) {
	case TCPS_CLOSED:
		debugf("TCPS_CLOSED");
		return;

	case TCPS_LISTEN:
		debugf("TCPS_LISTEN");
		return;

	case TCPS_SYN_SENT:
		debugf("TCPS_SYN_SENT");
		return;

	case TCPS_SYN_RECEIVED:
		debugf("TCPS_SYN_RECEIVED");
		return;

	case TCPS_ESTABLISHED:
		debugf("TCPS_ESTABLISHED");
		return;

	case TCPS_CLOSE_WAIT:
		debugf("TCPS_CLOSE_WAIT");
		return;

	case TCPS_FIN_WAIT_1:
		debugf("TCPS_FIN_WAIT_1");
		return;

	case TCPS_CLOSING:
		debugf("TCPS_CLOSING");
		return;

	case TCPS_LAST_ACK:
		debugf("TCPS_LAST_ACK");
		return;

	case TCPS_FIN_WAIT_2:
		debugf("TCPS_FIN_WAIT_2");
		return;

	case TCPS_TIME_WAIT:
		debugf("TCPS_TIME_WAIT");
		return;

	default:
		debugf("unknown");
		return;
	}
}

static void
db_print_tflags(u_int t_flags)
{
	int comma;

	comma = 0;
	if (t_flags & TF_ACKNOW) {
		debugf("%sTF_ACKNOW", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_DELACK) {
		debugf("%sTF_DELACK", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_NODELAY) {
		debugf("%sTF_NODELAY", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_NOOPT) {
		debugf("%sTF_NOOPT", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_SENTFIN) {
		debugf("%sTF_SENTFIN", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_REQ_SCALE) {
		debugf("%sTF_REQ_SCALE", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_RCVD_SCALE) {
		debugf("%sTF_RECVD_SCALE", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_REQ_TSTMP) {
		debugf("%sTF_REQ_TSTMP", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_RCVD_TSTMP) {
		debugf("%sTF_RCVD_TSTMP", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_SACK_PERMIT) {
		debugf("%sTF_SACK_PERMIT", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_NEEDSYN) {
		debugf("%sTF_NEEDSYN", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_NEEDFIN) {
		debugf("%sTF_NEEDFIN", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_NOPUSH) {
		debugf("%sTF_NOPUSH", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_MORETOCOME) {
		debugf("%sTF_MORETOCOME", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_LQ_OVERFLOW) {
		debugf("%sTF_LQ_OVERFLOW", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_LASTIDLE) {
		debugf("%sTF_LASTIDLE", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_RXWIN0SENT) {
		debugf("%sTF_RXWIN0SENT", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_FASTRECOVERY) {
		debugf("%sTF_FASTRECOVERY", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_CONGRECOVERY) {
		debugf("%sTF_CONGRECOVERY", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_WASFRECOVERY) {
		debugf("%sTF_WASFRECOVERY", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_SIGNATURE) {
		debugf("%sTF_SIGNATURE", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_FORCEDATA) {
		debugf("%sTF_FORCEDATA", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_TSO) {
		debugf("%sTF_TSO", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_ECN_PERMIT) {
		debugf("%sTF_ECN_PERMIT", comma ? ", " : "");
		comma = 1;
	}
}

static void
db_print_toobflags(char t_oobflags)
{
	int comma;

	comma = 0;
	if (t_oobflags & TCPOOB_HAVEDATA) {
		debugf("%sTCPOOB_HAVEDATA", comma ? ", " : "");
		comma = 1;
	}
	if (t_oobflags & TCPOOB_HADDATA) {
		debugf("%sTCPOOB_HADDATA", comma ? ", " : "");
		comma = 1;
	}
}

void
db_print_tcpcb(struct tcpcb *tp, const char *name, int indent)
{

	db_print_indent(indent);
	debugf("%03d|%s at %p\n", sched::thread::current()->id(), name, tp);

	indent += 2;

	db_print_indent(indent);
	debugf("t_segq first: %p   t_segqlen: %d   t_dupacks: %d\n",
	   LIST_FIRST(&tp->t_segq), tp->t_segqlen, tp->t_dupacks);

	//db_print_indent(indent);
	//debugf("tt_rexmt: %p   tt_persist: %p   tt_keep: %p\n",
	//    &tp->t_timers->tt_rexmt, &tp->t_timers->tt_persist, &tp->t_timers->tt_keep);

	//db_print_indent(indent);
	//debugf("tt_2msl: %p   tt_delack: %p   t_inpcb: %p\n", &tp->t_timers->tt_2msl,
	//    &tp->t_timers->tt_delack, tp->t_inpcb);

	db_print_indent(indent);
	debugf("t_state: %d (", tp->get_state());
	db_print_tstate(tp->get_state());
	debugf(")\n");

	db_print_indent(indent);
	debugf("t_flags: 0x%x (", tp->t_flags);
	db_print_tflags(tp->t_flags);
	debugf(")\n");

	db_print_indent(indent);
	debugf("snd_una: 0x%08x   snd_max: 0x%08x   snd_nxt: x0%08x\n",
	    tp->snd_una, tp->snd_max, tp->snd_nxt);

	db_print_indent(indent);
	debugf("snd_up: 0x%08x   snd_wl1: 0x%08x   snd_wl2: 0x%08x\n",
	   tp->snd_up, tp->snd_wl1, tp->snd_wl2);

	db_print_indent(indent);
	debugf("iss: 0x%08x   irs: 0x%08x   rcv_nxt: 0x%08x\n",
	    tp->iss, tp->irs, tp->rcv_nxt);

	db_print_indent(indent);
	debugf("rcv_adv: 0x%08x   rcv_wnd: %lu   rcv_up: 0x%08x\n",
	    tp->rcv_adv, tp->rcv_wnd, tp->rcv_up);

	db_print_indent(indent);
	debugf("snd_wnd: %lu   snd_cwnd: %lu\n",
	   tp->snd_wnd, tp->snd_cwnd);

	db_print_indent(indent);
	debugf("snd_ssthresh: %lu   snd_recover: "
	    "0x%08x\n", tp->snd_ssthresh, tp->snd_recover);

	db_print_indent(indent);
	debugf("t_maxopd: %u   t_rcvtime: %u   t_startime: %u\n",
	    tp->t_maxopd, tp->t_rcvtime, tp->t_starttime);

	db_print_indent(indent);
	debugf("t_rttime: %u   t_rtsq: 0x%08x\n",
	    tp->t_rtttime, tp->t_rtseq);

	db_print_indent(indent);
	debugf("t_rxtcur: %d   t_maxseg: %u   t_srtt: %d\n",
	    tp->t_rxtcur, tp->t_maxseg, tp->t_srtt);

	db_print_indent(indent);
	debugf("t_rttvar: %d   t_rxtshift: %d   t_rttmin: %u   "
	    "t_rttbest: %u\n", tp->t_rttvar, tp->t_rxtshift, tp->t_rttmin,
	    tp->t_rttbest);

	db_print_indent(indent);
	debugf("t_rttupdated: %lu   max_sndwnd: %lu   t_softerror: %d\n",
	    tp->t_rttupdated, tp->max_sndwnd, tp->t_softerror);

	db_print_indent(indent);
	debugf("t_oobflags: 0x%x (", tp->t_oobflags);
	db_print_toobflags(tp->t_oobflags);
	debugf(")   t_iobc: 0x%02x\n", tp->t_iobc);

	db_print_indent(indent);
	debugf("snd_scale: %u   rcv_scale: %u   request_r_scale: %u\n",
	    tp->snd_scale, tp->rcv_scale, tp->request_r_scale);

	db_print_indent(indent);
	debugf("ts_recent: %u   ts_recent_age: %u\n",
	    tp->ts_recent, tp->ts_recent_age);

	db_print_indent(indent);
	debugf("ts_offset: %u   last_ack_sent: 0x%08x   snd_cwnd_prev: "
	    "%lu\n", tp->ts_offset, tp->last_ack_sent, tp->snd_cwnd_prev);

	db_print_indent(indent);
	debugf("snd_ssthresh_prev: %lu   snd_recover_prev: 0x%08x   "
	    "t_badrxtwin: %u\n", tp->snd_ssthresh_prev,
	    tp->snd_recover_prev, tp->t_badrxtwin);

	db_print_indent(indent);
	debugf("snd_numholes: %d  snd_holes first: %p\n",
	    tp->snd_numholes, TAILQ_FIRST(&tp->snd_holes));

	db_print_indent(indent);
	debugf("snd_fack: 0x%08x   rcv_numsacks: %d   sack_newdata: "
	    "0x%08x\n", tp->snd_fack, tp->rcv_numsacks, tp->sack_newdata);

	/* Skip sackblks, sackhint. */

	db_print_indent(indent);
	debugf("t_rttlow: %d   rfbuf_ts: %u   rfbuf_cnt: %d\n",
	    tp->t_rttlow, tp->rfbuf_ts, tp->rfbuf_cnt);
}
/*
DB_SHOW_COMMAND(tcpcb, db_show_tcpcb)
{
	struct tcpcb *tp;

	if (!have_addr) {
		debugf("usage: show tcpcb <addr>\n");
		return;
	}
	tp = (struct tcpcb *)addr;

	db_print_tcpcb(tp, "tcpcb", 0);
}*/
//#endif
