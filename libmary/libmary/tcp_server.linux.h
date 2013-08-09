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


#ifndef LIBMARY__TCP_SERVER__LINUX__H__
#define LIBMARY__TCP_SERVER__LINUX__H__


#include <libmary/code_referenced.h>
#include <libmary/exception.h>
#include <libmary/poll_group.h>
#include <libmary/timers.h>
#include <libmary/tcp_connection.h>


namespace M {

class TcpServer : public DependentCodeReferenced
{
public:
    struct Frontend
    {
	void (*accepted) (void *cb_data);
    };

private:
    mt_const Time accept_retry_timeout_millisec;

    mt_const DataDepRef<Timers> timers;

    int fd;

    Cb<Frontend> frontend;
    Cb<PollGroup::Feedback> feedback;

    void requestInput ()
    {
	if (feedback && feedback->requestInput)
	    feedback.call (feedback->requestInput);
    }

    void requestOutput ()
    {
	if (feedback && feedback->requestOutput)
	    feedback.call (feedback->requestOutput);
    }

  mt_iface (PollGroup::Pollable)
    static PollGroup::Pollable const pollable;

    static void processEvents (Uint32  event_flags,
			       void   *_self);

    static int getFd (void *_self);

    static void setFeedback (Cb<PollGroup::Feedback> const &feedback,
			     void *_self);
  mt_iface_end

    StateMutex accept_retry_mutex;
    mt_mutex (mutex) bool accept_retry_timer_set;

    static void acceptRetryTimerTick (void *_data);

public:
    mt_throws Result open ();

    class AcceptResult
    {
    public:
	enum Value {
	    Accepted,
	    NotAccepted,
	    Error
	};
	operator Value () const { return value; }
	AcceptResult (Value const value) : value (value) {}
	AcceptResult () {}
    private:
	Value value;
    };

    mt_throws AcceptResult accept (TcpConnection * mt_nonnull tcp_connection,
				   IpAddress     *ret_addr = NULL);

    // Should be called before listen().
    mt_throws Result bind (IpAddress ip_addr);

    // Should only be called once.
    mt_throws Result listen ();

    mt_throws Result start ();

//    // TODO Questionable: there's no synchronization for "fd = -1" assignment.
//    mt_throws Result close ();

    CbDesc<PollGroup::Pollable> getPollable ()
        { return CbDesc<PollGroup::Pollable> (&pollable, this, getCoderefContainer()); }

    void init (CbDesc<Frontend> const &frontend,
               DeferredProcessor * mt_nonnull deferred_processor,
               Timers            * mt_nonnull timers,
               Time               accept_retry_timeout_millisec = 1000);

     TcpServer (Object *coderef_container);
    ~TcpServer ();
};

}


#endif /* LIBMARY__TCP_SERVER__LINUX__H__ */

