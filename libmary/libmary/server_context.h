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


#ifndef LIBMARY__SERVER_CONTEXT__H__
#define LIBMARY__SERVER_CONTEXT__H__


#include <libmary/timers.h>
#include <libmary/poll_group.h>
#include <libmary/deferred_processor.h>
#include <libmary/deferred_connection_sender.h>


namespace M {

class ServerThreadContext : public DependentCodeReferenced
{
private:
    mt_const DataDepRef<Timers>                        timers;
    mt_const DataDepRef<PollGroup>                     poll_group;
    mt_const DataDepRef<DeferredProcessor>             deferred_processor;
    mt_const DataDepRef<DeferredConnectionSenderQueue> dcs_queue;

public:
    Timers*            getTimers            () const { return timers; }
    PollGroup*         getPollGroup         () const { return poll_group; }
    DeferredProcessor* getDeferredProcessor () const { return deferred_processor; }

    DeferredConnectionSenderQueue* getDeferredConnectionSenderQueue () const
        { return dcs_queue; }

    mt_const void init (Timers                        * const timers,
			PollGroup                     * const poll_group,
			DeferredProcessor             * const deferred_processor,
			DeferredConnectionSenderQueue * const dcs_queue)
    {
	this->timers             = timers;
	this->poll_group         = poll_group;
	this->deferred_processor = deferred_processor;
	this->dcs_queue          = dcs_queue;
    }

    ServerThreadContext (Object * const coderef_container)
	: DependentCodeReferenced (coderef_container),
          timers                  (coderef_container),
	  poll_group              (coderef_container),
	  deferred_processor      (coderef_container),
	  dcs_queue               (coderef_container)
    {}
};

class ServerContext : public DependentCodeReferenced
{
public:
    virtual CodeDepRef<ServerThreadContext> selectThreadContext  () = 0;
    virtual CodeDepRef<ServerThreadContext> getMainThreadContext () = 0;

    ServerContext (Object * const coderef_container)
	: DependentCodeReferenced (coderef_container)
    {}

    virtual ~ServerContext () {}
};

}


#endif /* LIBMARY__SERVER_CONTEXT__H__ */

