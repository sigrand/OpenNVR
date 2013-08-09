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


#include <libmary/log.h>

#include <libmary/file_connection.h>


namespace M {

#ifdef LIBMARY_WIN32_IOCP
bool
FileConnection::outputCompleteTask (void * const _self)
{
    FileConnection * const self = static_cast <FileConnection*> (_self);

    self->mutex.lock ();

    assert (self->output_completion_pending);
    self->output_completion_pending = false;

    Cb<Overlapped::IoCompleteCallback> const tmp_io_complete_cb = self->io_complete_cb;
    Overlapped * const tmp_overlapped = self->overlapped;
    Size const tmp_bytes_transferred = self->bytes_transferred;

    self->mutex.unlock ();

    tmp_io_complete_cb.call_ ((Exception*) NULL,
                              tmp_overlapped,
                              tmp_bytes_transferred);
    tmp_overlapped->unref ();

    return false /* do not reschedule */;
}
#endif

mt_throws AsyncIoResult
FileConnection::read (
#ifdef LIBMARY_WIN32_IOCP
                      OVERLAPPED  * const mt_nonnull /* overlapped */,
#endif
                      Memory        const mem,
		      Size        * const ret_nread)
{
#ifdef LIBMARY_WIN32_IOCP
    (void) mem;
    (void) ret_nread;
    logF_ (_func, "NOT IMPLEMENTED");
    return AsyncIoResult::Error;
#else
    IoResult const res = file->read (mem, ret_nread);
    switch (res) {
	case IoResult::Normal:
	    return AsyncIoResult::Normal;
	case IoResult::Eof:
	    return AsyncIoResult::Eof;
	case IoResult::Error:
	    return AsyncIoResult::Error;
    }

    unreachable ();
    return AsyncIoResult::Error;
#endif
}

mt_throws AsyncIoResult
FileConnection::write (
#ifdef LIBMARY_WIN32_IOCP
                       OVERLAPPED  * const mt_nonnull sys_overlapped,
#endif
                       ConstMemory   const mem,
		       Size        * const ret_nwritten)
{
    if (!file->writeFull (mem, ret_nwritten))
	return AsyncIoResult::Error;

    if (!file->flush ())
	return AsyncIoResult::Error;

  // TODO if (*ret_nwritten != mem.len()) then error.

#ifdef LIBMARY_WIN32_IOCP
    Overlapped * const tmp_overlapped = static_cast <Overlapped*> (sys_overlapped);
    if (tmp_overlapped->io_complete_cb) {
        mutex.lock ();
        output_completion_pending = true;
        overlapped = tmp_overlapped;
        bytes_transferred = mem.len();
        io_complete_cb = tmp_overlapped->io_complete_cb;
        mutex.unlock ();
        deferred_reg.scheduleTask (&output_complete_task, false /* permanent */);
    }
#endif

    return AsyncIoResult::Normal;
}

mt_throws AsyncIoResult
FileConnection::writev (
#ifdef LIBMARY_WIN32_IOCP
                        OVERLAPPED   * const mt_nonnull sys_overlapped,
                        WSABUF       * const mt_nonnull buffers,
#else
                        struct iovec * const iovs,
#endif
			Count          const num_iovs,
			Size         * const ret_nwritten)
{
#ifdef LIBMARY_WIN32_IOCP
    if (ret_nwritten)
        *ret_nwritten = 0;

    Size total_written = 0;
    for (Count i = 0; i < num_iovs; ++i) {
        Size nwritten;
        Result const res = file->writeFull (ConstMemory ((Byte const *) buffers [i].buf, buffers [i].len),
                                            &nwritten);

      // TODO if (*nwritten != buffer len) then error.

        total_written += buffers [i].len;
        if (!res) {
            if (ret_nwritten)
                *ret_nwritten = total_written;

            return AsyncIoResult::Error;
        }
    }

    if (ret_nwritten)
        *ret_nwritten = total_written;

    Overlapped * const tmp_overlapped = static_cast <Overlapped*> (sys_overlapped);
    if (tmp_overlapped->io_complete_cb) {
        mutex.lock ();
        output_completion_pending = true;
        overlapped = tmp_overlapped;
        bytes_transferred = total_written;
        io_complete_cb = tmp_overlapped->io_complete_cb;
        mutex.unlock ();
        deferred_reg.scheduleTask (&output_complete_task, false /* permanent */);
    }
#else
    Result const res = file->writev (iovs, num_iovs, ret_nwritten);
    if (!res)
	return AsyncIoResult::Error;
#endif

    if (!file->flush ())
	return AsyncIoResult::Error;

    return AsyncIoResult::Normal;
}

}

