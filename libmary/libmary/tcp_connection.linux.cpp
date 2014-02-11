/*  LibMary - C++ library for high-performance network servers
    Copyright (C) 2011-2013 Dmitry Shatrov

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/


#include <libmary/types.h>

#include <cstdio>

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <libmary/log.h>
#include <libmary/util_net.h>

#include <libmary/tcp_connection.h>
#ifdef LIBMARY_PERFORMANCE_TESTING
#include <libmary/istat_measurer.h>
#endif

//#define LIBMARY_TEST_MWRITEV
//#define LIBMARY_TEST_MWRITEV_SINGLE

//#define LIBMARY__TCP_CONNECTION__DEBUG


#ifdef LIBMARY_TEST_MWRITEV
#include <libmary/mwritev.h>
#endif


namespace M {

static LogGroup libMary_logGroup_tcp_conn ("tcp_conn", LogLevel::I);

#ifdef LIBMARY_PERFORMANCE_TESTING
extern IStatMeasurer* measurer_;
extern ITimeChecker* checker_;
#endif

#ifdef LIBMARY_TCP_CONNECTION_NUM_INSTANCES
AtomicInt TcpConnection::num_instances;
#endif

PollGroup::Pollable const TcpConnection::pollable = {
    processEvents,
    setFeedback,
    getFd
};

void
TcpConnection::processEvents (Uint32   const event_flags,
			      void   * const _self)
{
    TcpConnection * const self = static_cast <TcpConnection*> (_self);

//    logD_ (_func, fmt_hex, event_flags);
//    logD_ (_func, "Connection::frontend: 0x", fmt_hex, (uintptr_t) Connection::frontend);

    if (event_flags & PollGroup::Hup) {
	logD_ (_func, "0x", fmt_hex, (UintPtr) self, " Hup");
	self->hup_received = true;
    }

    if (event_flags & PollGroup::Output) {
	if (!self->connected) {
	    self->connected = true;

	    int opt_val = 0;
	    socklen_t opt_len = sizeof (opt_val);
	    int const res = getsockopt (self->fd, SOL_SOCKET, SO_ERROR, &opt_val, &opt_len);
	    if (res == -1) {
		int const errnum = errno;

		logE_ (_func, "getsockopt() failed: ", errnoString (errno));

		PosixException posix_exc (errnum);

		InternalException internal_exc (InternalException::BackendError);
		internal_exc.cause = &posix_exc;

		if (self->frontend && self->frontend->connected)
		    self->frontend.call (self->frontend->connected, /*(*/ &internal_exc /*)*/);

		if (self->input_frontend && self->input_frontend->processError)
		    self->input_frontend.call (self->input_frontend->processError, /*(*/ &internal_exc /*)*/);

		return;
	    } else
	    if (res != 0) {
		logE_ (_func, "getsockopt(): unexpected return value: ", res);

		InternalException internal_exc (InternalException::BackendMalfunction);

		if (self->frontend && self->frontend->connected)
		    self->frontend.call (self->frontend->connected, /*(*/ &internal_exc /*)*/);

		if (self->input_frontend && self->input_frontend->processError)
		    self->input_frontend.call (self->input_frontend->processError, /*(*/ &internal_exc /*)*/);

		return;
	    }

	    if (opt_val == 0) {
		if (self->frontend && self->frontend->connected)
		    self->frontend.call (self->frontend->connected,  /*(*/ (Exception*) NULL /* exc */ /*)*/);
	    } else {
		if (opt_val != EINPROGRESS && opt_val != EINTR) {
		    logE_ (_func, "0x", fmt_hex, (UintPtr) self, " Connection error: ", errnoString (opt_val));

		    PosixException posix_exc (opt_val);
		    InternalException internal_exc (InternalException::BackendError);
		    internal_exc.cause = &posix_exc;

		    if (self->frontend && self->frontend->connected)
			self->frontend.call (self->frontend->connected, /*(*/ &internal_exc /*)*/);

		    if (self->input_frontend && self->input_frontend->processError)
			self->input_frontend.call (self->input_frontend->processError, /*(*/ &internal_exc /*)*/);

		    return;
		} else {
		    logW_ (_func, "0x", fmt_hex, (UintPtr) self,
			   " Got output event, but not connected yet. opt_val: ", opt_val);
                    self->connected = false;
		    return;
		}
	    }
	}

	if (self->output_frontend && self->output_frontend->processOutput) {
#ifdef LIBMARY__TCP_CONNECTION__DEBUG
            logD_ (_self_func, "calling frontend->processOutput()");
#endif

	    self->output_frontend.call (self->output_frontend->processOutput);

#ifdef LIBMARY__TCP_CONNECTION__DEBUG
            logD_ (_self_func, "frontend->processOutput() returned");
#endif
        }
    }

    if (event_flags & PollGroup::Input ||
	event_flags & PollGroup::Hup)
    {
#ifdef LIBMARY__TCP_CONNECTION__DEBUG
        logD_ (_self_func, "calling frontend->processInput()");
#endif

	if (self->input_frontend && self->input_frontend->processInput)
	    self->input_frontend.call (self->input_frontend->processInput);

#ifdef LIBMARY__TCP_CONNECTION__DEBUG
        logD_ (_self_func, "frontend->processInput() returned");
#endif
    }

    if (event_flags & PollGroup::Error) {
	logD_ (_func, "0x", fmt_hex, (UintPtr) self, " Error");
	if (self->input_frontend && self->input_frontend->processError) {
	    // TODO getsockopt SO_ERROR + fill PosixException
	    IoException io_exc;
	    self->input_frontend.call (self->input_frontend->processError, /*(*/ &io_exc /*)*/);
	}
    }

    if (!(event_flags & PollGroup::Input)  &&
	!(event_flags & PollGroup::Output) &&
	!(event_flags & PollGroup::Error)  &&
	!(event_flags & PollGroup::Hup))
    {
	logD_ (_func, "0x", fmt_hex, (UintPtr) self, " No events");
	return;
    }
}

int
TcpConnection::getFd (void *_self)
{
    TcpConnection * const self = static_cast <TcpConnection*> (_self);
    return self->fd;
}

void
TcpConnection::setFeedback (Cb<PollGroup::Feedback> const &feedback,
			    void * const _self)
{
    TcpConnection * const self = static_cast <TcpConnection*> (_self);
    self->feedback = feedback;
}

AsyncIoResult
TcpConnection::read (Memory   const mem,
		     Size   * const ret_nread)
    mt_throw ((IoException,
	       InternalException))
{
    if (ret_nread)
	*ret_nread = 0;

    Size len;
    if (mem.len() > SSIZE_MAX)
	len = SSIZE_MAX;
    else
	len = mem.len();

    ssize_t const res = recv (fd, mem.mem(), (ssize_t) len, 0 /* flags */);
    if (res == -1) {
	if (errno == EAGAIN || errno == EWOULDBLOCK) {
	    requestInput ();
	    return AsyncIoResult::Again;
	}

	if (errno == EINTR)
	    return AsyncIoResult::Normal;

	exc_throw (PosixException, errno);
	exc_push_ (IoException);
	return AsyncIoResult::Error;
    } else
    if (res < 0) {
	exc_throw (InternalException, InternalException::BackendMalfunction);
	return AsyncIoResult::Error;
    } else
    if (res == 0) {
	return AsyncIoResult::Eof;
    }

    if (ret_nread)
	*ret_nread = (Size) res;

    if ((Size) res < len) {
	if (hup_received) {
	    return AsyncIoResult::Normal_Eof;
	} else {
	    requestInput ();
	    return AsyncIoResult::Normal_Again;
	}
    }

    return AsyncIoResult::Normal;
}

AsyncIoResult
TcpConnection::write (ConstMemory   const mem,
		      Size        * const ret_nwritten)
    mt_throw ((IoException,
	       InternalException))
{
    if (ret_nwritten)
	*ret_nwritten = 0;

    Size len;
    if (mem.len() > SSIZE_MAX)
	len = SSIZE_MAX;
    else
	len = mem.len();

    ssize_t const res = send (fd, mem.mem(), (ssize_t) len,
#ifdef __MACH__
            0
#else
            MSG_NOSIGNAL
#endif
            );
    if (res == -1) {
	if (errno == EAGAIN || errno == EWOULDBLOCK) {
	    requestOutput ();
	    return AsyncIoResult::Again;
	}

	if (errno == EINTR)
	    return AsyncIoResult::Normal;

	if (errno == EPIPE)
	    return AsyncIoResult::Eof;

	exc_throw (PosixException, errno);
	exc_push_ (IoException);
	return AsyncIoResult::Error;
    } else
    if (res < 0) {
	exc_throw (InternalException, InternalException::BackendMalfunction);
	return AsyncIoResult::Error;
    }

    if (ret_nwritten)
	*ret_nwritten = (Size) res;

    return AsyncIoResult::Normal;
}

mt_throws AsyncIoResult
TcpConnection::writev (struct iovec * const iovs,
		       Count          const num_iovs,
		       Size         * const ret_nwritten)
{
    if (ret_nwritten)
	*ret_nwritten = 0;

#ifndef LIBMARY_TEST_MWRITEV
    ssize_t const res = ::writev (fd, iovs, num_iovs);
#else
    ssize_t res;
    if (libMary_mwritevAvailable()) {
#ifndef LIBMARY_TEST_MWRITEV_SINGLE
	int w_fd = fd;
	struct iovec *w_iovs = iovs;
	int w_num_iovs = num_iovs;
	int w_res;
	if (!libMary_mwritev (1, &w_fd, &w_iovs, &w_num_iovs, &w_res)) {
#else
	int w_res;
	if (!libMary_mwritevSingle (fd, iovs, num_iovs, &w_res)) {
#endif // LIBMARY_TEST_MWRITEV_SINGLE
	    res = -1;
	    errno = EINVAL;
	} else {
	    if (w_res >= 0) {
		res = w_res;
	    } else {
		res = -1;
		errno = -w_res;
	    }
	}
    } else {
	res = ::writev (fd, iovs, num_iovs);
    }
#endif // LIBMARY_TEST_MWRITEV
    if (res == -1) {
	if (errno == EAGAIN || errno == EWOULDBLOCK) {
	    requestOutput ();
	    return AsyncIoResult::Again;
	}

	if (errno == EINTR)
	    return AsyncIoResult::Normal;

	if (errno == EPIPE)
	    return AsyncIoResult::Eof;

	exc_throw (PosixException, errno);
	exc_push (InternalException, InternalException::BackendError);
	logE_ (_func, "writev() failed: ", errnoString (errno));

	logE_ (_func, "num_iovs: ", num_iovs);
#if 0
	// Dump of all iovs.
	for (Count i = 0; i < num_iovs; ++i) {
	    logE_ (_func, "iovs[", i, "]: ", fmt_hex, (UintPtr) iovs [i].iov_base, ", ", fmt_def, iovs [i].iov_len);
	}
#endif

	return AsyncIoResult::Error;
    } else
    if (res < 0) {
	exc_throw (InternalException, InternalException::BackendMalfunction);
	logE_ (_func, "writev(): unexpected return value: ", res);
	return AsyncIoResult::Error;
    }

    if (ret_nwritten)
		*ret_nwritten = (Size) res;

    logD (tcp_conn, _func_, "we have written ", res, " bytes");
#ifdef LIBMARY_PERFORMANCE_TESTING
    if (!measurer_ || !checker_) {
        logW_ (_func, "measurer_ or checker_ havent been set");
    } else {
        if( (res - checker_->GetPacketSize()) < 30) {
            logD (tcp_conn, _func_, "check restream ending time. sizes match[",checker_->GetPacketSize(),";",res,"]");
            Time t;
            checker_->Stop(&t);
            measurer_->AddTimeInOut(t);
        } else {
            logD (tcp_conn, _func_, "looks like sizes doesnt match origin:",checker_->GetPacketSize(), " written: ", res);
        }
    }
#endif
    return AsyncIoResult::Normal;
}

#if 0
mt_throws Result
TcpConnection::close ()
{
  // TODO Protect against duplicate calls to close() somehow.
    logE_ (_this_func_);
    abort ();

    if (fd == -1)
	return Result::Success;

    for (;;) {
	int const res = ::close (fd);
	if (res == -1) {
	    if (errno == EINTR)
		continue;

	    exc_throw (PosixException, errno);
	    logE_ (_func, "close() failed: ", errnoString (errno));
	    return Result::Failure;
	} else
	if (res != 0) {
	    exc_throw (InternalException, InternalException::BackendError);
	    logE_ (_func, "close(): unexpected return value: ", res);
	    return Result::Failure;
	}

	break;
    }

    return Result::Success;
}
#endif

mt_throws Result
TcpConnection::open ()
{
    fd = socket (AF_INET, SOCK_STREAM, 0 /* protocol */);
    if (fd == -1) {
	exc_throw (PosixException, errno);
	exc_push (InternalException, InternalException::BackendError);
	logE_ (_func, "socket() failed: ", errnoString (errno));
        return Result::Failure;
    }

    {
	int flags = fcntl (fd, F_GETFL, 0);
	if (flags == -1) {
	    exc_throw (PosixException, errno);
	    exc_push (InternalException, InternalException::BackendError);
	    logE_ (_func, "fcntl() failed (F_GETFL): ", errnoString (errno));
            return Result::Failure;
	}

	flags |= O_NONBLOCK;

	if (fcntl (fd, F_SETFL, flags) == -1) {
	    exc_throw (PosixException, errno);
	    exc_push (InternalException, InternalException::BackendError);
	    logE_ (_func, "fcntl() failed (F_SETFL): ", errnoString (errno));
            return Result::Failure;
	}
    }

    {
	int opt_val = 1;
	int const res = setsockopt (fd, IPPROTO_TCP, TCP_NODELAY, &opt_val, sizeof (opt_val));
	if (res == -1) {
	    exc_throw (PosixException, errno);
	    exc_push (InternalException, InternalException::BackendError);
	    logE_ (_func, "setsockopt() failed (TCP_NODELAY): ", errnoString (errno));
            return Result::Failure;
	} else
	if (res != 0) {
	    exc_throw (InternalException, InternalException::BackendMalfunction);
	    logE_ (_func, "setsockopt() (TCP_NODELAY): unexpected return value: ", res);
            return Result::Failure;
	}
    }

#ifdef __linux__
    // TODO Consider using TCP_THIN_LINEAR_TIMEOUTS and TCP_THIN_DUPACK.

    {
        int opt_val = 1;
        int const res = setsockopt (fd, IPPROTO_TCP, TCP_QUICKACK, &opt_val, sizeof (opt_val));
        if (res == -1) {
            exc_throw (PosixException, errno);
            exc_push (InternalException, InternalException::BackendError);
            logE_ (_func, "setsockopt() failed (TCP_QUICKACK): ", errnoString (errno));
            return Result::Failure;
        } else
        if (res != 0) {
            exc_throw (InternalException, InternalException::BackendMalfunction);
            logE_ (_func, "setsockopt() (TCP_QUICKACK): unexpected return value: ", res);
            return Result::Failure;
        }
    }
#endif /* __linux__ */

    return Result::Success;
}

mt_throws TcpConnection::ConnectResult
TcpConnection::connect (IpAddress const addr)
{
    struct sockaddr_in saddr;
    setIpAddress (addr, &saddr);
    for (;;) {
	int const res = ::connect (fd, (struct sockaddr*) &saddr, sizeof (saddr));
	if (res == 0) {
	    connected = true;
            return ConnectResult_Connected;
	} else
	if (res == -1) {
	    if (errno == EINTR)
		continue;

	    if (errno == EINPROGRESS) {
		logD (tcp_conn, _func, "EINPROGRESS");
		break;
	    }

	    exc_throw (PosixException, errno);
	    exc_push_ (IoException);
	    logE_ (_func, "connect() failed: ", errnoString (errno));
            return ConnectResult_Error;
	} else {
	    exc_throw (InternalException, InternalException::BackendMalfunction);
	    logE_ (_func, "connect(): unexpected return value ", res);
            return ConnectResult_Error;
	}

	break;
    }

    return ConnectResult_InProgress;
}

TcpConnection::TcpConnection (Object * const coderef_container)
    : DependentCodeReferenced (coderef_container),
      fd (-1),
      connected    (false),
      hup_received (false)
{
#ifdef LIBMARY_TCP_CONNECTION_NUM_INSTANCES
    fprintf (stderr, " TcpConnection(): 0x%lx, num_instances: %d\n",
             (unsigned long) this,
	     (int) (num_instances.fetchAdd (1) + 1));
#endif
}

TcpConnection::~TcpConnection ()
{
#ifdef LIBMARY_TCP_CONNECTION_NUM_INSTANCES
    fprintf (stderr, "~TcpConnection(): 0x%lx, num_instances: %d\n",
	     (unsigned long) this,
	     (int) (num_instances.fetchAdd (-1) - 1));
#endif

    if (fd != -1) {
	for (;;) {
	    int const res = ::close (fd);
	    if (res == -1) {
		if (errno == EINTR)
		    continue;

		logE_ (_func, "close() failed: ", errnoString (errno));
	    } else
	    if (res != 0) {
		logE_ (_func, "close(): unexpected return value: ", res);
	    }

	    break;
	}
    }
}

}

