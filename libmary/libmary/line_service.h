/*  LibMary - C++ library for high-performance network servers
    Copyright (C) 2012 Dmitry Shatrov

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


#ifndef __LIBMARY__LINE_SERVICE__H__
#define __LIBMARY__LINE_SERVICE__H__


#include <libmary/types.h>
#include <libmary/list.h>
#include <libmary/server_context.h>
#include <libmary/tcp_server.h>
#include <libmary/tcp_connection.h>
#include <libmary/connection_receiver.h>

#include <libmary/line_server.h>


namespace M {

class LineService : public DependentCodeReferenced
{
private:
    StateMutex mutex;

public:
    struct Frontend {
        void (*line) (ConstMemory  line,
                      void        *cb_data);
    };

private:
    class LineConnection : public Object
    {
    public:
        typedef List< Ref<LineConnection> > LineConnectionList;

	mt_const WeakCodeRef weak_line_service;
	mt_const LineService *unsafe_line_service;

        mt_mutex (LineService::mutex) LineConnectionList::Element *conn_list_el;

	mt_mutex (LineService::mutex) bool valid;
	mt_mutex (LineService::mutex) PollGroup::PollableKey pollable_key;

	TcpConnection tcp_conn;
	ConnectionReceiver conn_receiver;
        LineServer line_server;

	LineConnection ();

	~LineConnection ();
    };

    typedef LineConnection::LineConnectionList LineConnectionList;

    mt_const Size max_line_len;

    mt_const DataDepRef<ServerThreadContext> thread_ctx;

    mt_const Cb<Frontend> frontend;

    TcpServer tcp_server;
    mt_mutex (mutex) PollGroup::PollableKey server_pollable_key;

    mt_mutex (mutex) LineConnectionList conn_list;

    void releaseLineConnection (LineConnection * mt_nonnull line_conn);

    bool acceptOneConnection ();

  mt_iface (TcpServer::Frontend)
    static TcpServer::Frontend const tcp_server_frontend;

    static void accepted (void *_self);
  mt_iface_end

  mt_iface (LineServer::Frontend)
    static LineServer::Frontend const line_server_frontend;

    static void line (ConstMemory  line,
                      void        *_self);

    static void closed (void *_self);
  mt_iface_end

public:
    mt_throws Result init ();

    mt_throws Result bind (IpAddress addr);

    mt_throws Result start ();

    mt_throws Result init (ServerContext          * mt_nonnull server_ctx,
                           CbDesc<Frontend> const &frontend,
                           Size                    max_line_len = 4096);

    LineService (Object *coderef_container);

    ~LineService ();
};

}


#endif /* __LIBMARY__LINE_SERVICE__H__ */

