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
#include <libmary/util_dev.h>
#include <libmary/log.h>

#include <libmary/connection_sender_impl.h>


#warning "TODO Don't hold the mutex while in write*() syscall."


namespace M {

static LogGroup libMary_logGroup_send    ("send",    LogLevel::I);
static LogGroup libMary_logGroup_writev  ("writev",  LogLevel::I);
static LogGroup libMary_logGroup_close   ("close",   LogLevel::I);
static LogGroup libMary_logGroup_hexdump ("hexdump", LogLevel::I);
static LogGroup libMary_logGroup_mwritev ("sender_impl_mwritev", LogLevel::I);

#ifdef LIBMARY_WIN32_IOCP
ConnectionSenderImpl::SenderOverlapped::~SenderOverlapped ()
{
  // At this point, ConnectionSenderImpl object is no more.
  // We simply release all remaining data.

    // Synchronizing in the absense of *Sender::mutex.
    full_memory_barrier ();

    Sender::PendingMessageList::iterator msg_iter (pending_msg_list);
    while (!msg_iter.done()) {
        Sender::MessageEntry * const msg_entry = msg_iter.next ();
        Sender::MessageEntry_Pages * const msg_pages = static_cast <Sender::MessageEntry_Pages*> (msg_entry);
        msg_pages->page_pool->msgUnref (msg_pages->first_pending_page);
        msg_pages->setFirstPage (NULL);
        Sender::deleteMessageEntry (msg_entry);
    }
}
#endif

void
ConnectionSenderImpl::setSendState (Sender::SendState const new_state)
{
    if (new_state == send_state)
	return;

    logD (send, _func, "Send state: ", (unsigned) new_state);

    send_state = new_state;

    if (sender)
        sender->fireSendStateChanged_deferred (deferred_reg, new_state);

    if (frontend && (*frontend)) {
        frontend->call_deferred (deferred_reg,
                                 (*frontend)->sendStateChanged,
                                 NULL /* extra_ref_data */,
                                 new_state);
    }
}

void
ConnectionSenderImpl::resetSendingState ()
{
  // We get here every time a complete message has been send.

    if (msg_list.isEmpty ()) {
	sending_message = false;
	// Note that if there was 'closeAfterFlush()' for ConnectionSenderImpl,
	// then this would be the right place to signal "closed" if message
	// queue is empty.
	return;
    }

    sending_message = true;
    send_header_sent = 0;

    Sender::MessageEntry * const cur_msg = msg_list.getFirst();

    if (cur_msg->type == Sender::MessageEntry::Pages) {
	Sender::MessageEntry_Pages * const msg_pages = static_cast <Sender::MessageEntry_Pages*> (cur_msg);
	send_cur_offset = msg_pages->msg_offset;
	logD (send, _func, "msg_pages->msg_offset: ", msg_pages->msg_offset);
    } else {
	send_cur_offset = 0;
    }
    logD (send, _func, "send_cur_offset: ", send_cur_offset);
}

void
ConnectionSenderImpl::popPage (Sender::MessageEntry_Pages * const mt_nonnull msg_pages)
{
    PagePool::Page * const page = msg_pages->getFirstPage();
    PagePool::Page * const next_page = page->getNextMsgPage();

#ifndef LIBMARY_WIN32_IOCP
    msg_pages->page_pool->pageUnref (msg_pages->getFirstPage());
#endif

    msg_pages->msg_offset = 0;
    send_cur_offset = 0;

    msg_pages->setFirstPageNoPending (next_page);
}

#ifdef LIBMARY_WIN32_IOCP
#warning TODO Communicate the number of bytes transferred here and compare it with the expected value.
#warning      Can it be less during normal operation (without I/O errors)?
void
ConnectionSenderImpl::outputComplete ()
{
    overlapped_pending = false;

    {
        Sender::PendingMessageList::iterator msg_iter (sender_overlapped->pending_msg_list);
        while (!msg_iter.done()) {
            Sender::MessageEntry * const msg_entry = msg_iter.next ();
            if (msg_entry == msg_list.getFirst())
                break;

            Sender::MessageEntry_Pages * const msg_pages = static_cast <Sender::MessageEntry_Pages*> (msg_entry);
            msg_pages->page_pool->msgUnref (msg_pages->first_pending_page);
            msg_pages->setFirstPage (NULL);

            sender_overlapped->pending_msg_list.remove (msg_entry);
            Sender::deleteMessageEntry (msg_entry);
        }
    }

    if (send_state == Sender::ConnectionOverloaded)
        setSendState (Sender::ConnectionReady);

    overloaded = false;
}
#endif

AsyncIoResult
ConnectionSenderImpl::sendPendingMessages ()
    mt_throw ((IoException,
	       InternalException))
{
    logD (send, _func_);

#ifdef LIBMARY_WIN32_IOCP
    if (overlapped_pending)
        return AsyncIoResult::Again;
#endif

    processing_barrier_hit = false;

    if (!sending_message) {
	if (!msg_list.getFirst())
	    return AsyncIoResult::Normal;

	logD (send, _func, "calling resetSendingState()");
	resetSendingState ();
    }

    logD (send, _func, "calling sendPendingMessages_writev()");
    return sendPendingMessages_writev ();
}

#ifdef LIBMARY_ENABLE_MWRITEV
void
ConnectionSenderImpl::sendPendingMessages_fillIovs (Count        * const ret_num_iovs,
						    struct iovec * const ret_iovs,
						    Count          const max_iovs)
{
    processing_barrier_hit = false;

    if (!sending_message) {
	if (!msg_list.getFirst())
	    return;

	logD (send, _func, "calling resetSendingState()");
	resetSendingState ();
    }

    if (!gotDataToSend()) {
	logD (mwritev, _func, "no data to send");

	if (send_state == Sender::ConnectionOverloaded)
	    setSendState (Sender::ConnectionReady);

	overloaded = false;
	return;
    }

    if (processingBarrierHit()) {
	logD (mwritev, _func, "processing barrier hit");
	return;
    }

    sendPendingMessages_vector_fill (ret_num_iovs,
				     ret_iovs,
				     IOV_MAX <= max_iovs ? IOV_MAX : max_iovs /* num_iovs */);
}

void
ConnectionSenderImpl::sendPendingMessages_react (AsyncIoResult res,
						 Size          num_written)
{
    if (res != AsyncIoResult::Normal)
	processing_barrier_hit = false;

    if (res == AsyncIoResult::Again) {
	logD (mwritev, _func, "connection overloaded");

	if (send_state == Sender::ConnectionReady)
	    setSendState (Sender::ConnectionOverloaded);

	overloaded = true;
	return;
    }

    sendPendingMessages_vector_react (num_written);

    if (!gotDataToSend()) {
	logD (mwritev, _func, "connection ready");

	if (send_state == Sender::ConnectionOverloaded)
	    setSendState (Sender::ConnectionReady);

	overloaded = false;
	return;
    }
}
#endif // LIBMARY_ENABLE_MWRITEV

AsyncIoResult
ConnectionSenderImpl::sendPendingMessages_writev ()
    mt_throw ((IoException,
	       InternalException))
{
    logD (send, _func, "msg_list len: ", msg_list.countNumElements());

    for (;;) {
	if (!gotDataToSend()) {
	    logD (send, _func, "no data to send");

	    if (send_state == Sender::ConnectionOverloaded)
		setSendState (Sender::ConnectionReady);

	    overloaded = false;
	    return AsyncIoResult::Normal;
	}

	if (processingBarrierHit()) {
	    logD (send, _func, "processing barrier hit");
	    return AsyncIoResult::Normal;
	}

	Count num_iovs = 0;

#ifdef LIBMARY_WIN32_IOCP
        Size num_bytes = 0;
        WSABUF buffers [IOV_MAX];
#else
	// Note: Comments in boost headers suggest that posix platforms are not
	// required to define IOV_MAX.
	struct iovec iovs [IOV_MAX];
#endif

	sendPendingMessages_vector_fill (&num_iovs,
#ifdef LIBMARY_WIN32_IOCP
                                         &num_bytes,
                                         buffers,
#else
                                         iovs,
#endif
                                         IOV_MAX /* num_iovs */);
	if (num_iovs > IOV_MAX) {
	    logF_ (_func, "num_iovs: ", num_iovs, ", IOV_MAX: ", IOV_MAX);
	    assert (0);
	}

#if 0
	// Dump of all iovs.
        if (logLevelOn (hexdump, LogLevel::Debug)) {
            logLock ();
	    log_unlocked_ (libMary_logGroup_writev.getLogLevel(), _func, "iovs:");
	    for (Count i = 0; i < num_iovs; ++i) {
		logD_unlocked (writev, "    #", i, ": 0x", (UintPtr) iovs [i].iov_base, ": ", iovs [i].iov_len);
		hexdump (logs, ConstMemory ((Byte const *) iovs [i].iov_base, iovs [i].iov_len));
	    }
            logUnlock ();
	}
#endif

	if (num_iovs) {
            Size num_written = 0;

	    bool tmp_processing_barrier_hit = processing_barrier_hit;
	    processing_barrier_hit = false;

            logD (send, _func, "writev: num_iovs: ", num_iovs);
#ifdef LIBMARY_WIN32_IOCP
            sender_overlapped->ref ();
#endif
            AsyncIoResult const res = conn->writev (
#ifdef LIBMARY_WIN32_IOCP
                                                    sender_overlapped,
                                                    buffers,
#else
                                                    iovs,
#endif
                                                    num_iovs,
                                                    &num_written);
	    if (res == AsyncIoResult::Error) {
#ifdef LIBMARY_WIN32_IOCP
                sender_overlapped->unref ();
#endif
		return AsyncIoResult::Error;
            }

#ifdef LIBMARY_WIN32_IOCP
            num_written = num_bytes;
            overlapped_pending = true;
#endif

	    if (res == AsyncIoResult::Again) {
		if (send_state == Sender::ConnectionReady)
		    setSendState (Sender::ConnectionOverloaded);

		logD (send, _func, "connection overloaded");
		overloaded = true;

#ifdef LIBMARY_WIN32_IOCP
                sendPendingMessages_vector_react (num_written);
#endif
		return AsyncIoResult::Again;
	    }

	    if (res == AsyncIoResult::Eof) {
		logD (close, _func, "Eof, num_iovs: ", num_iovs);
		return AsyncIoResult::Eof;
	    }

	    processing_barrier_hit = tmp_processing_barrier_hit;

	    // Normal_Again is not handled specially here yet.

	    // Note that we can get "num_written == 0" here in two cases:
	    //   a) writev() syscall returned EINTR;
	    //   b) there was nothing to send.
	    // That's why we do not do a usual EINTR loop here.

            sendPendingMessages_vector_react (num_written);
	}

#ifdef LIBMARY_WIN32_IOCP
        // We can't reuse OVERLAPPED until we get completion notification.
        return AsyncIoResult::Again;
#endif
    } // for (;;)

    unreachable();
    return AsyncIoResult::Normal;
}

void
ConnectionSenderImpl::sendPendingMessages_vector_fill (Count        * const mt_nonnull ret_num_iovs,
#ifdef LIBMARY_WIN32_IOCP
                                                       Size         * const mt_nonnull ret_num_bytes,
                                                       WSABUF       * const mt_nonnull buffers,
#else
						       struct iovec * const mt_nonnull iovs,
#endif
						       Count          const num_iovs)
{
    logD (writev, _func_);

    *ret_num_iovs = 0;
#ifdef LIBMARY_WIN32_IOCP
    *ret_num_bytes = 0;
#endif

    Sender::MessageEntry *msg_entry = msg_list.getFirst ();
    if (mt_unlikely (!msg_entry)) {
	logD (writev, _func, "message queue is empty");
	return;
    }

    if (mt_unlikely (enable_processing_barrier
		     && !processing_barrier))
    {
	if (gotDataToSend())
	    processing_barrier_hit = true;

	logD (writev, _func, "processing barrier is NULL");
	return;
    }

    Count cur_num_iovs = 0;

    bool first_entry = true;
    Count i = 0;
    while (msg_entry) {
	Sender::MessageEntry * const next_msg_entry = msg_list.getNext (msg_entry);
	Sender::MessageEntry_Pages * const msg_pages = static_cast <Sender::MessageEntry_Pages*> (msg_entry);

	{
	    logD (writev, _func, "message entry");

	    // Разделение "if (first_entry) {} else {}" нужно, потому что переменные
	    // состояния send*, в том числе и send_header_sent, обновляются только
	    // в фазе "react".
	    if (first_entry) {
		if (send_header_sent < msg_pages->header_len) {
		    logD (writev, _func, "first entry, header");

		    ++cur_num_iovs;
		    if (mt_unlikely (cur_num_iovs > num_iovs))
			break;
		    ++*ret_num_iovs;

                    Byte * const buf = msg_pages->getHeaderData() + send_header_sent;
                    Size   const len = msg_pages->header_len - send_header_sent;
#ifdef LIBMARY_WIN32_IOCP
                    buffers [i].buf = (char*) buf;
                    buffers [i].len = len;
                    *ret_num_bytes += len;
#else
		    iovs [i].iov_base = buf;
		    iovs [i].iov_len  = len;
#endif
		    ++i;
		}
	    } else {
		if (msg_pages->header_len > 0) {
		    logD (writev, _func, "header");

		    ++cur_num_iovs;
		    if (cur_num_iovs > num_iovs)
			break;
		    ++*ret_num_iovs;

                    Byte * const buf = msg_pages->getHeaderData();
                    Size   const len = msg_pages->header_len;
#ifdef LIBMARY_WIN32_IOCP
                    buffers [i].buf = (char*) buf;
                    buffers [i].len = len;
                    *ret_num_bytes += len;
#else
		    iovs [i].iov_base = buf;
		    iovs [i].iov_len  = len;
#endif
		    ++i;
		}
	    } // if (first_entry)

	    PagePool::Page *page = msg_pages->getFirstPage();
	    bool first_page = true;
	    while (page) {
		logD (writev, _func, "page");

		PagePool::Page * const next_page = page->getNextMsgPage();

		if (page->data_len > 0) {
		    logD (writev, _func, "non-empty page");

		    // FIXME Limit on the number of iovs conflicts with send barrier logics.
		    //       There probably are bugs because of this.
		    //       Consider situations where we hit num_iovs limit and the message
		    //       has been sent completely.
		    ++cur_num_iovs;
		    if (cur_num_iovs > num_iovs)
			break;
		    ++*ret_num_iovs;

		    if (first_page) {
			if (first_entry) {
			    logD (writev, _func, "#", i, ": first page, first entry");

                            if (send_cur_offset < page->data_len) {
                                Byte * const buf = page->getData() + send_cur_offset;
                                Size   const len = page->data_len - send_cur_offset;
#ifdef LIBMARY_WIN32_IOCP
                                buffers [i].buf = (char*) buf;
                                buffers [i].len = len;
                                *ret_num_bytes += len;
#else
                                iovs [i].iov_base = buf;
                                iovs [i].iov_len  = len;
#endif
                                ++i;
                            } else
                            if (send_cur_offset == page->data_len) {
                                --cur_num_iovs;
                                --*ret_num_iovs;
                            } else {
                                unreachable ();
                            }
			} else {
			    logD (writev, _func, "#", i, ": first page");

			    if (msg_pages->msg_offset < page->data_len) {
                                Byte * const buf = page->getData() + msg_pages->msg_offset;
                                Size   const len = page->data_len - msg_pages->msg_offset;
#ifdef LIBMARY_WIN32_IOCP
                                buffers [i].buf = (char*) buf;
                                buffers [i].len = len;
                                *ret_num_bytes += len;
#else
                                iovs [i].iov_base = buf;
                                iovs [i].iov_len  = len;
#endif
                                ++i;
                            } else
                            if (msg_pages->msg_offset == page->data_len) {
                                --cur_num_iovs;
                                --*ret_num_iovs;
                            } else {
                                unreachable ();
                            }
			}
		    } else {
			logD (writev, _func, "#", i);

                        Byte * const buf = page->getData();
                        Size   const len = page->data_len;
#ifdef LIBMARY_WIN32_IOCP
                        buffers [i].buf = (char*) buf;
                        buffers [i].len = len;
                        *ret_num_bytes += len;
#else
			iovs [i].iov_base = buf;
			iovs [i].iov_len  = len;
#endif

                        ++i;
		    }
		} else { // if (page->data_len > 0)
		  // Empty page.
		}

		first_page = false;
		page = next_page;
	    } // while (page)
	}

	if (mt_unlikely (cur_num_iovs > num_iovs))
	    break;

	if (enable_processing_barrier
	    && msg_entry == processing_barrier)
	{
	    break;
	}

	first_entry = false;
	msg_entry = next_msg_entry;
    } // while (msg_entry)
}

void
ConnectionSenderImpl::sendPendingMessages_vector_react (Size num_written)
{
    logD (writev, _func_);

    Sender::MessageEntry *msg_entry = msg_list.getFirst ();
    if (mt_unlikely (!msg_entry)) {
	logD (writev, _func, "message queue is empty");
	return;
    }

    if (mt_unlikely (enable_processing_barrier
		     && !processing_barrier))
    {
	if (gotDataToSend())
	    processing_barrier_hit = true;

	logD (writev, _func, "processing barrier is NULL");
	return;
    }

    bool first_entry = true;
    while (msg_entry) {
	Sender::MessageEntry * const next_msg_entry = msg_list.getNext (msg_entry);
	Sender::MessageEntry_Pages * const msg_pages = static_cast <Sender::MessageEntry_Pages*> (msg_entry);

#ifdef LIBMARY_WIN32_IOCP
        if (sender_overlapped->pending_msg_list.getLast() != msg_entry)
            sender_overlapped->pending_msg_list.append (msg_entry);
#endif

	// If still set to 'true' after the switch, then msg_entry is removed
	// from the queue.
	bool msg_sent_completely = true;
	{
	    logD (writev, _func, "message entry");

	    // Разделение "if (first_entry) {} else {}" нужно, потому что переменные
	    // состояния send*, в том числе и send_header_sent, обновляются только
	    // при последнем вызове этого метода (в фазе "react").
	    if (first_entry) {
		if (send_header_sent < msg_pages->header_len) {
		    logD (writev, _func, "first entry, header");

		    if (mt_unlikely (num_written < msg_pages->header_len - send_header_sent)) {
			send_header_sent += num_written;
			num_written = 0;
			msg_sent_completely = false;
			break;
		    }

		    num_written -= msg_pages->header_len - send_header_sent;
		    send_header_sent = msg_pages->header_len;
		}
	    } else {
		if (msg_pages->header_len > 0) {
		    logD (writev, _func, "header");

		    if (mt_unlikely (num_written < msg_pages->header_len)) {
			send_header_sent = num_written;
			num_written = 0;
			msg_sent_completely = false;
			break;
		    }

		    send_header_sent = msg_pages->header_len;
		    num_written -= msg_pages->header_len;
		}
	    } // if (first_entry)

	    PagePool::Page *page = msg_pages->getFirstPage();
	    bool first_page = true;
	    while (page) {
		logD (writev, _func, "page");

		PagePool::Page * const next_page = page->getNextMsgPage();

		if (mt_likely (page->data_len > 0)) {
		    logD (writev, _func, "non-empty page");

		    if (first_page) {
			if (first_entry) {
			    assert (send_cur_offset <= page->data_len);
			    if (mt_unlikely (num_written < page->data_len - send_cur_offset)) {
				send_cur_offset += num_written;
				num_written = 0;
				msg_sent_completely = false;
				break;
			    }

			    num_written -= page->data_len - send_cur_offset;
			} else {
			    assert (msg_pages->msg_offset <= page->data_len);
			    if (mt_unlikely (num_written < page->data_len - msg_pages->msg_offset)) {
                                // Note: '+=' is correct here. '=' is wrong.
                                //       That's because it is pre-set to msg_pages->msg_offset
                                //       when switching to the next msg entry.
				send_cur_offset += num_written;
				num_written = 0;
				msg_sent_completely = false;
				break;
			    }

			    num_written -= page->data_len - msg_pages->msg_offset;
			}
		    } else {
			if (mt_unlikely (num_written < page->data_len)) {
			    send_cur_offset = num_written;
			    num_written = 0;
			    msg_sent_completely = false;
			    break;
			}

			num_written -= page->data_len;
		    }

		    popPage (msg_pages);
		} else { // if (page->data_len > 0)
		  // Empty page.
		    popPage (msg_pages);
		}

		first_page = false;
		page = next_page;
	    } // while (page)
	}

	if (msg_sent_completely) {
	  // This is the only place where messages are removed from the queue.

	    msg_list.remove (msg_entry);
#ifndef LIBMARY_WIN32_IOCP
	    Sender::deleteMessageEntry (msg_entry);
#endif

	    --num_msg_entries;
	    if (mt_unlikely (send_state == Sender::QueueSoftLimit ||
			     send_state == Sender::QueueHardLimit))
	    {
		if (num_msg_entries < soft_msg_limit) {
		    if (overloaded)
			setSendState (Sender::ConnectionOverloaded);
		    else
			setSendState (Sender::ConnectionReady);
		} else
		if (num_msg_entries < hard_msg_limit)
		    setSendState (Sender::QueueSoftLimit);
	    }

	    logD (send, _func, "calling resetSendingState()");
	    resetSendingState ();

	    if (mt_unlikely (enable_processing_barrier
			     && msg_entry == processing_barrier))
	    {
		processing_barrier = NULL;

		if (gotDataToSend())
		    processing_barrier_hit = true;

		break;
	    }
	} else {
	    assert (gotDataToSend());
#if 0
// Unnecessary
	    if (mt_unlikely (enable_processing_barrier
			     && msg_entry == processing_barrier))
	    {
		processing_barrier_hit = true;
	    }
#endif

	    break;
	}

	first_entry = false;
	msg_entry = next_msg_entry;
    } // while (msg_entry)

    assert (num_written == 0);
}

void
ConnectionSenderImpl::dumpMessage (Sender::MessageEntry * const mt_nonnull msg_entry)
{
    switch (msg_entry->type) {
        case Sender::MessageEntry::Pages: {
            Sender::MessageEntry_Pages * const msg_pages = static_cast <Sender::MessageEntry_Pages*> (msg_entry);

            // Counting message length.
            Size msg_len = 0;
            {
                msg_len += msg_pages->header_len;

                PagePool::Page *cur_page = msg_pages->getFirstPage();
                while (cur_page != NULL) {
                    if (cur_page == msg_pages->getFirstPage()) {
                        assert (cur_page->data_len >= msg_pages->msg_offset);
                        msg_len += cur_page->data_len - msg_pages->msg_offset;
                    } else {
                        msg_len += cur_page->data_len;
                    }

                    cur_page = cur_page->getNextMsgPage ();
                }
            }

            // Collecting message data into a single adrray.
            Byte * const tmp_data = new Byte [msg_len];
            {
                Size pos = 0;

                memcpy (tmp_data + pos, msg_pages->getHeaderData(), msg_pages->header_len);
                pos += msg_pages->header_len;

                PagePool::Page *cur_page = msg_pages->getFirstPage();
                while (cur_page != NULL) {
                    if (cur_page == msg_pages->getFirstPage()) {
                        assert (cur_page->data_len >= msg_pages->msg_offset);
                        memcpy (tmp_data + pos,
                                cur_page->getData() + msg_pages->msg_offset,
                                cur_page->data_len - msg_pages->msg_offset);
                        pos += cur_page->data_len - msg_pages->msg_offset;
                    } else {
                        memcpy (tmp_data + pos, cur_page->getData(), cur_page->data_len);
                        pos += cur_page->data_len;
                    }

                    cur_page = cur_page->getNextMsgPage ();
                }
            }

            logLock ();
            log_unlocked_ (libMary_logGroup_hexdump.getLogLevel(), _func, "Message data:");
            hexdump (logs, ConstMemory (tmp_data, msg_len));
            logUnlock ();

            delete tmp_data;
        } break;
        default:
            unreachable ();
    }
}

void
ConnectionSenderImpl::queueMessage (Sender::MessageEntry * const mt_nonnull msg_entry)
{
    if (logLevelOn (hexdump, LogLevel::Debug))
        dumpMessage (msg_entry);

    // Don't queue empty messages, so that gotDataToSend() is correct.
    switch (msg_entry->type) {
        case Sender::MessageEntry::Pages: {
            Sender::MessageEntry_Pages * const msg_pages = static_cast <Sender::MessageEntry_Pages*> (msg_entry);
            if (msg_pages->header_len == 0) {
                if (msg_pages->getFirstPage() == NULL)
                    return;

                if (msg_pages->getFirstPage()->data_len <= msg_pages->msg_offset) {
                    PagePool::Page *page = msg_pages->getFirstPage()->getNextMsgPage();
                    while (page) {
                        if (page->data_len > 0)
                            break;

                        page = page->getNextMsgPage();
                    }
                    if (!page)
                        return;
                }
            }
        } break;
        default:
            unreachable ();
    }

    ++num_msg_entries;
    if (num_msg_entries >= hard_msg_limit)
	setSendState (Sender::QueueHardLimit);
    else
    if (num_msg_entries >= soft_msg_limit)
	setSendState (Sender::QueueSoftLimit);

    msg_list.append (msg_entry);
}

ConnectionSenderImpl::ConnectionSenderImpl (
#ifdef LIBMARY_WIN32_IOCP
                                            CbDesc<Overlapped::IoCompleteCallback> const &io_complete_cb,
#endif
                                            bool const enable_processing_barrier)
#warning TODO blocking_mode: unused
    : blocking_mode      (false),
      conn               (NULL),
      soft_msg_limit     (1024),
      hard_msg_limit     (4096),
#ifdef LIBMARY_WIN32_IOCP
      overlapped_pending (false),
#endif
      send_state         (Sender::ConnectionReady),
      overloaded         (false),
      num_msg_entries    (0),
      enable_processing_barrier (enable_processing_barrier),
      processing_barrier (NULL),
      processing_barrier_hit (false),
      sending_message    (false),
      send_header_sent   (0),
      send_cur_offset    (0)
{
#ifdef LIBMARY_WIN32_IOCP
    sender_overlapped = grab (new (std::nothrow) SenderOverlapped);
    sender_overlapped->io_complete_cb = io_complete_cb;
#endif
}

void
ConnectionSenderImpl::release ()
{
    Sender::MessageList::iter iter (msg_list);
    while (!msg_list.iter_done (iter)) {
	Sender::MessageEntry * const msg_entry = msg_list.iter_next (iter);
#ifdef LIBMARY_WIN32_IOCP
        // msg_list and pending_msg_list cannot overlap for more than one MessageEntry,
        // the first for msg_list and the last for pending_msg_list.
        if (msg_entry != sender_overlapped->pending_msg_list.getLast())
#endif
            Sender::deleteMessageEntry (msg_entry);
    }
    msg_list.clear ();
}

}

