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


#ifndef LIBMARY__CONNECTION_RECEIVER__GENERIC__H__
#define LIBMARY__CONNECTION_RECEIVER__GENERIC__H__


#include <libmary/receiver.h>
#include <libmary/async_input_stream.h>
#include <libmary/code_referenced.h>


namespace M {


// TODO Rename to AsyncReceiver. It now depends on AsyncInputStream, not on Connection.

// Synchronized externally by the associated AsyncInputStream object.
class ConnectionReceiver : public Receiver,
			   public DependentCodeReferenced
{
private:
    DeferredProcessor::Task unblock_input_task;
    DeferredProcessor::Registration deferred_reg;

    mt_const AsyncInputStream *conn;

    mt_const Byte *recv_buf;
    mt_const Size const recv_buf_len;

    mt_sync_domain (conn_input_frontend) Size recv_buf_pos;
    mt_sync_domain (conn_input_frontend) Size recv_accepted_pos;

    mt_sync_domain (conn_input_frontend) bool block_input;
    mt_sync_domain (conn_input_frontend) bool error_received;
    mt_sync_domain (conn_input_frontend) bool error_reported;

    mt_sync_domain (conn_input_frontend) void doProcessInput ();

  mt_iface (AsyncInputStream::InputFrontend)
    static AsyncInputStream::InputFrontend const conn_input_frontend;

    static void processInput (void *_self);

    static void processError (Exception *exc_,
			      void      *_self);
  mt_iface_end

    static bool unblockInputTask (void *_self);

public:
  mt_iface (Receiver)
    void unblockInput ();
  mt_iface_end

    void start ();

    mt_const void init (AsyncInputStream  * mt_nonnull conn,
                        DeferredProcessor * mt_nonnull deferred_processor,
                        bool               block_input = false);

     ConnectionReceiver (Object * const coderef_container);
    ~ConnectionReceiver ();
};

}


#endif /* LIBMARY__CONNECTION_RECEIVER__GENERIC__H__ */

