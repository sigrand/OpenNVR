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


#ifndef LIBMARY__SENDER__H__
#define LIBMARY__SENDER__H__


#include <libmary/types.h>
#include <new>

#include <libmary/intrusive_list.h>
#include <libmary/code_referenced.h>
#include <libmary/cb.h>
#include <libmary/informer.h>
#include <libmary/exception.h>
#include <libmary/page_pool.h>
#include <libmary/log.h>


// M::VSlab is a grow-only data structure. Be warned.
#define LIBMARY_SENDER_VSLAB


#ifdef LIBMARY_SENDER_VSLAB
#include <libmary/vslab.h>
#endif


namespace M {

class Sender : public virtual CodeReferenced
{
protected:
    StateMutex mutex;

public:
    enum SendState {
			      // The comments below describe usual behavior of
			      // user's code when processing sendStateChanged()
			      // notifications.

	ConnectionReady,      // Fast client, no flow control restrictions.

	ConnectionOverloaded, // Slow client, dropping disposable messages.

	QueueSoftLimit,       // Send queue is full, blocking input from
			      // the client as an extra countermeasure.

	QueueHardLimit        // Send queue growth is out of control.
			      // Disconnecting the client.
    };

    struct Frontend {
	// This callback is called with Sender::mutex held, which means that
	// it is forbidden to call any methods of Sender from the callback.
	void (*sendStateChanged) (Sender::SendState  send_state,
				  void              *cb_data);

//#warning TODO pass "close_input" parameter to communicate user's' intentions to the server (HttpServer mostly).
	void (*closed) (Exception *exc_,
			void      *cb_data);
    };

    class MessageList_name;
    class PendingMessageList_name;

    class MessageEntry : public IntrusiveListElement<MessageList_name>,
                         public IntrusiveListElement<PendingMessageList_name>
    {
    public:
	enum Type {
	    Pages
	};

	Type const type;

	MessageEntry (Type const type) : type (type) {}
    };

    typedef IntrusiveList <MessageEntry, MessageList_name> MessageList;
    typedef IntrusiveList <MessageEntry, PendingMessageList_name> PendingMessageList;

    static void deleteMessageEntry (MessageEntry * mt_nonnull msg_entry);

    class MessageEntry_Pages : public MessageEntry
    {
	friend void Sender::deleteMessageEntry (MessageEntry * mt_nonnull msg_entry);
#ifdef LIBMARY_SENDER_VSLAB
	friend class VSlab<MessageEntry_Pages>;
#endif

    private:
        MessageEntry_Pages ()
            : MessageEntry (MessageEntry::Pages)
#ifdef LIBMARY_WIN32_IOCP
              , first_pending_page (NULL)
#endif
        {}

	~MessageEntry_Pages () {}

    public:
	Size header_len;
	CodeDepRef<PagePool> page_pool;

    private:
	PagePool::Page *first_page;
    public:
#ifdef LIBMARY_WIN32_IOCP
        PagePool::Page *first_pending_page;
#endif

        void setFirstPage (PagePool::Page * const page)
        {
            first_page = page;
#ifdef LIBMARY_WIN32_IOCP
            first_pending_page = page;
#endif
        }

        void setFirstPageNoPending (PagePool::Page * const page)
            { first_page = page; }

        PagePool::Page* getFirstPage () { return first_page; }

	Size msg_offset;

#ifdef LIBMARY_SENDER_VSLAB
	VSlab<MessageEntry_Pages>::AllocKey vslab_key;
#endif

	Byte* getHeaderData () const
	    { return (Byte*) this + sizeof (*this); }

	Size getTotalMsgLen() const
            { return header_len + getPagesDataLen(); }

	Size getPagesDataLen () const
            { return PagePool::countPageListDataLen (first_page, msg_offset); }

	static MessageEntry_Pages* createNew (Size max_header_len = 0);
    };

#ifdef LIBMARY_SENDER_VSLAB
    typedef VSlab<MessageEntry_Pages> MsgVSlab;
    static MsgVSlab msg_vslab;
  #ifdef LIBMARY_MT_SAFE
    // TODO thread-local msg_vslabs
    static Mutex msg_vslab_mutex;
  #endif
#endif

protected:
    mt_const Cb<Frontend> frontend;

    Informer_<Frontend> event_informer;

    static void informClosed (Frontend *events,
                              void     *cb_data,
                              void     *inform_data);

    static void informSendStateChanged (Frontend *events,
                                        void     *cb_data,
                                        void     *inform_data);

    void fireClosed (Exception *exc_);

    mt_mutex (mutex) void fireClosed_unlocked (Exception *exc_);

    static void fireClosed_static (Exception *exc_,
                                   void      *_self);

    void fireClosed_deferred (DeferredProcessor::Registration *def_reg,
                              ExceptionBuffer                 *exc_buf);

    void fireSendStateChanged (SendState send_state);

    static void fireSendStateChanged_static (SendState  send_state,
                                             void      *_self);

public:
    Informer_<Frontend>* getEventInformer ()
        { return &event_informer; }

    // public for ConnectionSenderImpl.
    void fireSendStateChanged_deferred (DeferredProcessor::Registration *def_reg,
                                        SendState send_state);

    // Takes ownership of msg_entry.
    virtual void sendMessage (MessageEntry * mt_nonnull msg_entry,
			      bool          do_flush) = 0;

    virtual mt_mutex (mutex) void sendMessage_unlocked (MessageEntry * mt_nonnull msg_entry,
                                                        bool          do_flush) = 0;

    virtual void flush () = 0;

    virtual mt_mutex (mutex) void flush_unlocked () = 0;

    // Frontend::closed() will be called after message queue becomes empty.
    virtual void closeAfterFlush () = 0;

    // Frontend::closed() will be called (deferred callback invocation).
    virtual void close () = 0;

    virtual mt_mutex (mutex) bool isClosed_unlocked () = 0;

    virtual mt_mutex (mutex) SendState getSendState_unlocked () = 0;

    virtual void lock   () = 0;
    virtual void unlock () = 0;

    void sendPages (PagePool       * const mt_nonnull page_pool,
                    PagePool::Page * const mt_nonnull first_page,
		    bool             const do_flush)
    {
      // Same as: sendPages (page_pool, page_list->first, 0 /* msg_offset */, do_flush);

	MessageEntry_Pages * const msg_pages = MessageEntry_Pages::createNew (0 /* max_header_len */);
	msg_pages->header_len = 0;
	msg_pages->page_pool = page_pool;
	msg_pages->setFirstPage (first_page);
	msg_pages->msg_offset = 0;

	sendMessage (msg_pages, do_flush);
    }

    void sendPages (PagePool       * const mt_nonnull page_pool,
                    PagePool::Page * const mt_nonnull first_page,
                    Size             const msg_offset,
                    bool             const do_flush)
    {
        MessageEntry_Pages * const msg_pages = MessageEntry_Pages::createNew (0 /* max_header_len */);
        msg_pages->header_len = 0;
        msg_pages->page_pool = page_pool;
        msg_pages->setFirstPage (first_page);
        msg_pages->msg_offset = msg_offset;

        sendMessage (msg_pages, do_flush);
    }

    template <class ...Args>
    void send (PagePool * const mt_nonnull page_pool,
               bool const do_flush,
               Args const &...args)
    {
        PagePool::PageListHead page_list;
        page_pool->printToPages (&page_list, args...);

        MessageEntry_Pages * const msg_pages = MessageEntry_Pages::createNew (0 /* max_header_len */);
        msg_pages->header_len = 0;
        msg_pages->page_pool = page_pool;
        msg_pages->setFirstPage (page_list.first);
        msg_pages->msg_offset = 0;

        sendMessage (msg_pages, do_flush);
    }

    mt_const void setFrontend (CbDesc<Frontend> const &frontend)
        { this->frontend = frontend; }

    Sender (Object * const coderef_container)
        : event_informer (coderef_container, &mutex)
    {}
};

}


#endif /* LIBMARY__SENDER__H__ */

