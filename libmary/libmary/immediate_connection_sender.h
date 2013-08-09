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


#ifndef LIBMARY__IMMEDIATE_CONNECTION_SENDER__H__
#define LIBMARY__IMMEDIATE_CONNECTION_SENDER__H__


#include <libmary/connection.h>
#include <libmary/sender.h>
#include <libmary/connection_sender_impl.h>
#include <libmary/code_referenced.h>


namespace M {

class ImmediateConnectionSender : public Sender,
				  public DependentCodeReferenced
{
private:
    DeferredProcessor::Registration deferred_reg;

    mt_mutex (mutex) ConnectionSenderImpl conn_sender_impl;

    mt_mutex (mutex) bool closed;
    mt_mutex (mutex) bool close_after_flush;
    mt_mutex (mutex) bool ready_for_output;

    mt_unlocks (mutex) void closeIfNeeded (bool deferred_event,
                                           bool unlock);

#ifdef LIBMARY_WIN32_IOCP
    static void outputIoComplete (Exception  *exc_,
                                  Overlapped *overlapped,
                                  Size        bytes_transferred,
                                  void       *cb_data);
#else
  mt_iface (Connection::OutputFrontend)
    static Connection::OutputFrontend const conn_output_frontend;
    static void processOutput (void *_self);
  mt_iface_end
#endif

    mt_mutex (mutex) mt_unlocks (mutex) void doFlush (bool unlock);

public:
  mt_iface (Sender)
    void sendMessage (MessageEntry * mt_nonnull msg_entry,
		      bool          do_flush);

    mt_mutex (mutex) void sendMessage_unlocked (MessageEntry * mt_nonnull msg_entry,
                                                bool          do_flush);

    void flush ();

    mt_mutex (mutex) void flush_unlocked ();

    void closeAfterFlush ();

    void close ();

    mt_mutex (mutex) bool isClosed_unlocked ();

    mt_mutex (mutex) SendState getSendState_unlocked ();

    void lock ();

    void unlock ();
  mt_iface_end

    void setConnection (Connection * const mt_nonnull conn)
    {
	conn_sender_impl.setConnection (conn);
#ifndef LIBMARY_WIN32_IOCP
	conn->setOutputFrontend (
                CbDesc<Connection::OutputFrontend> (&conn_output_frontend, this, getCoderefContainer()));
#endif
    }

    void init (DeferredProcessor * const mt_nonnull deferred_processor)
    {
        deferred_reg.setDeferredProcessor (deferred_processor);
    }

     ImmediateConnectionSender (Object *coderef_container);
    ~ImmediateConnectionSender ();
};

}


#endif /* LIBMARY__IMMEDIATE_CONNECTION_SENDER__H__ */

