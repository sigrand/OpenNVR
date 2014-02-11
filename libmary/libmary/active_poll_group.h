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


#ifndef LIBMARY__ACTIVE_POLL_GROUP__H__
#define LIBMARY__ACTIVE_POLL_GROUP__H__


#include <libmary/poll_group.h>


namespace M {

class ActivePollGroup : public PollGroup
{
public:
    struct Frontend {
	// pollIterationBegin is not called when poll() returns (poll timeout/error).
	void (*pollIterationBegin) (void *cb_data);

	// If returns 'true', then there's more work to do in pollIterationEnd(),
	// and the next poll iteration will be performed with zero timeout.
	bool (*pollIterationEnd)   (void *cb_data);
    };

protected:
    mt_const Cb<Frontend> frontend;

public:
    virtual mt_throws Result poll (Uint64 timeout_millisec = (Uint64) -1) = 0;

    virtual mt_throws Result trigger () = 0;

    void setFrontend (Cb<Frontend> const &frontend)
        { this->frontend = frontend; }

    virtual ~ActivePollGroup () {}
};

}


#ifdef LIBMARY_PLATFORM_WIN32
  #ifdef LIBMARY_WIN32_IOCP
    #include <libmary/iocp_poll_group.h>
  #else
	#include <wsa_poll_group.h>
  #endif
#else
  #include <libmary/select_poll_group.h>
  #include <libmary/poll_poll_group.h>
#endif

#if !defined (LIBMARY_PLATFORM_WIN32) && defined (LIBMARY_ENABLE_EPOLL)
  #include <libmary/epoll_poll_group.h>
#endif


#endif /* LIBMARY__ACTIVE_POLL_GROUP__H__ */

