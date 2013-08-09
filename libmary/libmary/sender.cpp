/*  LibMary - C++ library for high-performance network servers
    Copyright (C) 2011 Dmitry Shatrov

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


#include <libmary/log.h>

#include <libmary/sender.h>


namespace M {

#ifdef LIBMARY_SENDER_VSLAB
Sender::MsgVSlab Sender::msg_vslab (1 << 16 /* prealloc, 64К messages */ /* TODO Preallocate less */);
#ifdef LIBMARY_MT_SAFE
Mutex Sender::msg_vslab_mutex;
#endif
#endif

Sender::MessageEntry_Pages*
Sender::MessageEntry_Pages::createNew (Size const max_header_len)
{
#ifdef LIBMARY_SENDER_VSLAB
    unsigned const vslab_header_len = 33 /* RtmpConnection::MaxHeaderLen */;
    if (max_header_len <= vslab_header_len /* TODO Artificial limit (matches Moment::RtmpConnection's needs) */) {
        VSlab<MessageEntry_Pages>::AllocKey vslab_key;
        MessageEntry_Pages *msg_pages;
        {
  #ifdef LIBMARY_MT_SAFE
          MutexLock msg_vslab_l (&msg_vslab_mutex);
  #endif
            msg_pages = msg_vslab.alloc (sizeof (MessageEntry_Pages) + vslab_header_len, &vslab_key);
            new (msg_pages) MessageEntry_Pages;
        }
        msg_pages->vslab_key = vslab_key;
        return msg_pages;
    } else {
        Byte * const buf = new (std::nothrow) Byte [sizeof (MessageEntry_Pages) + max_header_len];
        assert (buf);
        MessageEntry_Pages * const msg_pages = new (buf) MessageEntry_Pages;
        msg_pages->vslab_key = NULL;
        return msg_pages;
    }
#else
    Byte * const buf = new (std::nothrow) Byte [sizeof (MessageEntry_Pages) + max_header_len];
    assert (buf);
    return new (buf) MessageEntry_Pages;
#endif
}

#ifdef LIBMARY_SENDER_VSLAB
void
Sender::deleteMessageEntry (MessageEntry * const mt_nonnull msg_entry)
{
    MessageEntry_Pages * const msg_pages = static_cast <MessageEntry_Pages*> (msg_entry);

    msg_pages->page_pool->msgUnref (msg_pages->first_page);
    if (msg_pages->vslab_key) {
  #ifdef LIBMARY_MT_SAFE
      MutexLock msg_vslab_l (&msg_vslab_mutex);
  #endif
        msg_vslab.free (msg_pages->vslab_key);
    } else {
        delete[] (Byte*) msg_pages;
    }
}
#else
void
Sender::deleteMessageEntry (MessageEntry * const mt_nonnull msg_entry)
{
//    logD_ (_func, "0x", fmt_hex, (UintPtr) msg_entry);

    switch (msg_entry->type) {
	case MessageEntry::Pages: {
	    MessageEntry_Pages * const msg_pages = static_cast <MessageEntry_Pages*> (msg_entry);
	    msg_pages->page_pool->msgUnref (msg_pages->first_page);
	    delete[] (Byte*) msg_pages;
	} break;
	default:
	    unreachable ();
    }
}
#endif

namespace {
    struct InformClosed_Data
    {
        Exception *exc_;
    };
}

void
Sender::informClosed (Frontend * const events,
                      void     * const cb_data,
                      void     * const _inform_data)
{
    if (events->closed) {
        InformClosed_Data * const inform_data =
                static_cast <InformClosed_Data*> (_inform_data);
        events->closed (inform_data->exc_, cb_data);
    }
}

namespace {
    struct InformSendStateChanged_Data
    {
        Sender::SendState send_state;
    };
}

void
Sender::informSendStateChanged (Frontend * const events,
                                void     * const cb_data,
                                void     * const _inform_data)
{
    if (events->sendStateChanged) {
        InformSendStateChanged_Data * const inform_data =
                static_cast <InformSendStateChanged_Data*> (_inform_data);
        events->sendStateChanged (inform_data->send_state, cb_data);
    }
}

void
Sender::fireClosed (Exception * const exc_)
{
    InformClosed_Data inform_data = { exc_ };
    event_informer.informAll (informClosed, &inform_data);
}

mt_mutex (mutex) void
Sender::fireClosed_unlocked (Exception * const exc_)
{
    InformClosed_Data inform_data = { exc_ };
    mt_unlocks_locks (mutex) event_informer.informAll_unlocked (informClosed, &inform_data);
}

void
Sender::fireClosed_static (Exception * const exc_,
                           void      * const _self)
{
    Sender * const self = static_cast <Sender*> (_self);
    self->fireClosed (exc_);
}

void
Sender::fireClosed_deferred (DeferredProcessor::Registration * const def_reg,
                             ExceptionBuffer                 * const exc_buf)
{
    Cb <void (Exception*, void*)> cb (fireClosed_static,
                                      this,
                                      getCoderefContainer());
    cb.call_deferred (def_reg,
                      fireClosed_static,
                      exc_buf /* extra_ref_data */,
                      exc_buf ? exc_buf->getException() : NULL);
}

void
Sender::fireSendStateChanged (SendState const send_state)
{
    InformSendStateChanged_Data inform_data = { send_state };
    event_informer.informAll (informSendStateChanged, &inform_data);
}

void
Sender::fireSendStateChanged_static (SendState   const send_state,
                                     void      * const _self)
{
    Sender * const self = static_cast <Sender*> (_self);
    self->fireSendStateChanged (send_state);
}

void
Sender::fireSendStateChanged_deferred (DeferredProcessor::Registration * const def_reg,
                                       SendState const send_state)
{
    Cb <void (SendState, void*)> cb (fireSendStateChanged_static,
                                     this,
                                     getCoderefContainer());
    cb.call_deferred (def_reg,
                      fireSendStateChanged_static,
                      NULL /* extra_ref_data */,
                      send_state);
}

}

