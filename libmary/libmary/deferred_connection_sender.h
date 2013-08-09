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


#ifndef LIBMARY__DEFERRED_CONNECTION_SENDER__H__
#define LIBMARY__DEFERRED_CONNECTION_SENDER__H__


#include <libmary/intrusive_list.h>
#include <libmary/connection.h>
#include <libmary/sender.h>
#include <libmary/connection_sender_impl.h>
#include <libmary/code_referenced.h>
#include <libmary/deferred_processor.h>


namespace M {

class DeferredConnectionSenderQueue;

class DeferredConnectionSender_OutputQueue_name;
class DeferredConnectionSender_ProcessingQueue_name;

class DeferredConnectionSender : public Sender,
				 public IntrusiveListElement<DeferredConnectionSender_OutputQueue_name>,
				 public IntrusiveListElement<DeferredConnectionSender_ProcessingQueue_name>,
				 public DependentCodeReferenced
{
    friend class DeferredConnectionSenderQueue;

private:
    mt_const DataDepRef<DeferredConnectionSenderQueue> dcs_queue;

  mt_mutex (mutex)
  mt_begin
    ConnectionSenderImpl conn_sender_impl;

    bool closed;
    bool close_after_flush;
    bool ready_for_output;
    bool in_output_queue;
  mt_end

    mt_unlocks (mutex) void toGlobOutputQueue (bool add_ref,
                                               bool unlock);

    mt_unlocks (mutex) void closeIfNeeded (bool deferred_event);

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

    mt_unlocks (mutex) void doFlush (bool unlock);

public:
  mt_iface (Sender)
    void sendMessage (MessageEntry * mt_nonnull msg_entry,
		      bool do_flush);

    mt_mutex (mutex) void sendMessage_unlocked (MessageEntry * mt_nonnull msg_entry,
                                                bool          do_flush);

    void flush ();

    mt_mutex (mutex) void flush_unlocked ();

    void closeAfterFlush ();

    void close ();

    mt_mutex (mutex) bool isClosed_unlocked ();

    mt_mutex (mutex) SendState getSendState_unlocked ();

    void lock   ();
    void unlock ();
  mt_iface_end

    mt_const void setConnection (Connection * const mt_nonnull conn)
    {
	conn_sender_impl.setConnection (conn);
#ifndef LIBMARY_WIN32_IOCP
	conn->setOutputFrontend (
                CbDesc<Connection::OutputFrontend> (&conn_output_frontend, this, getCoderefContainer()));
#endif
    }

    mt_const void setQueue (DeferredConnectionSenderQueue * mt_nonnull dcs_queue);

     DeferredConnectionSender (Object *coderef_container);
    ~DeferredConnectionSender ();
};

class DeferredConnectionSenderQueue : public DependentCodeReferenced
{
    friend class DeferredConnectionSender;

private:
    Mutex queue_mutex;

    typedef IntrusiveList<DeferredConnectionSender, DeferredConnectionSender_OutputQueue_name> OutputQueue;
    typedef IntrusiveList<DeferredConnectionSender, DeferredConnectionSender_ProcessingQueue_name> ProcessingQueue;

    mt_const DataDepRef<DeferredProcessor> deferred_processor;

    DeferredProcessor::Task send_task;
    DeferredProcessor::Registration send_reg;

    mt_mutex (queue_mutex) OutputQueue output_queue;
    mt_mutex (queue_mutex) bool processing;

    mt_mutex (queue_mutex) bool released;

    static bool process (void *_self);

#ifdef LIBMARY_ENABLE_MWRITEV
    static bool process_mwritev (void *_self);
#endif

public:
    void setDeferredProcessor (DeferredProcessor * const deferred_processor);

    void release ();

     DeferredConnectionSenderQueue (Object *coderef_container);
    ~DeferredConnectionSenderQueue ();
};

}


#endif /* LIBMARY__DEFERRED_CONNECTION_SENDER__H__ */

