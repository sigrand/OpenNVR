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


#include <libmary/types.h>

#include <limits.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <libmary/log.h>
#include <libmary/util_base.h>
#include <libmary/util_dev.h>

#include <libmary/native_async_file.h>


namespace M {

PollGroup::Pollable const NativeAsyncFile::pollable = {
    processEvents,
    setFeedback,
    getFd
};

void
NativeAsyncFile::processEvents (Uint32   const event_flags,
                                void   * const _self)
{
    NativeAsyncFile * const self = static_cast <NativeAsyncFile*> (_self);

    if (event_flags & PollGroup::Hup) {
	logD_ (_self_func, "Hup");
    }

    if (event_flags & PollGroup::Output) {
	if (self->output_frontend && self->output_frontend->processOutput)
	    self->output_frontend.call (self->output_frontend->processOutput);
    }

    if (event_flags & PollGroup::Input ||
	event_flags & PollGroup::Hup)
    {
	if (self->input_frontend && self->input_frontend->processInput)
	    self->input_frontend.call (self->input_frontend->processInput);
    }

    if (event_flags & PollGroup::Error) {
	logD_ (_func, "0x", fmt_hex, (UintPtr) self, " Error");
	if (self->input_frontend && self->input_frontend->processError) {
	    // TODO getsockopt SO_ERROR + fill PosixException
	    IoException io_exc;
	    self->input_frontend.call (self->input_frontend->processError, /*(*/ &io_exc /*)*/);
	}
    }

    if (!(event_flags & PollGroup::Input)  &&
	!(event_flags & PollGroup::Output) &&
	!(event_flags & PollGroup::Error)  &&
	!(event_flags & PollGroup::Hup))
    {
	logD_ (_func, "0x", fmt_hex, (UintPtr) self, " No events");
	return;
    }
}

int
NativeAsyncFile::getFd (void *_self)
{
    NativeAsyncFile * const self = static_cast <NativeAsyncFile*> (_self);
    return self->fd;
}

void
NativeAsyncFile::setFeedback (Cb<PollGroup::Feedback> const &feedback,
                             void * const _self)
{
    NativeAsyncFile * const self = static_cast <NativeAsyncFile*> (_self);
    self->feedback = feedback;
}

mt_throws AsyncIoResult
NativeAsyncFile::read (Memory   const mem,
                       Size   * const ret_nread)
{
    if (ret_nread)
        *ret_nread = 0;

    // According to POSIX, if we pass a value larger than SSIZE_MAX to read,
    // then the result is implementation-defined.
    Size len;
    if (mem.len() > SSIZE_MAX)
	len = SSIZE_MAX;
    else
	len = mem.len();

    ssize_t const res = ::read (fd, mem.mem(), len);
    if (res == -1) {
	if (errno == EAGAIN || errno == EWOULDBLOCK) {
	    requestInput ();
	    return AsyncIoResult::Again;
	}

	if (errno == EINTR)
	    return AsyncIoResult::Normal;

	exc_throw (PosixException, errno);
	exc_push_ (IoException);
	return AsyncIoResult::Error;
    } else
    if (res < 0) {
	exc_throw (InternalException, InternalException::BackendMalfunction);
	return AsyncIoResult::Error;
    } else
    if (res == 0) {
	return AsyncIoResult::Eof;
    }

    if ((Size) res > len) {
	exc_throw (InternalException, InternalException::BackendMalfunction);
	return AsyncIoResult::Error;
    }

    if (ret_nread)
	*ret_nread = res;

    return AsyncIoResult::Normal;
}

mt_throws AsyncIoResult
NativeAsyncFile::write (ConstMemory   const mem,
                        Size        * const ret_nwritten)
{
    if (ret_nwritten)
	*ret_nwritten = 0;

    // According to POSIX, if we pass a value larger than SSIZE_MAX to read,
    // then the result is implementation-defined.
    Size len;
    if (mem.len() > SSIZE_MAX)
	len = SSIZE_MAX;
    else
	len = mem.len();

    ssize_t const res = ::write (fd, mem.mem(), len);

    if (res == -1) {
	if (errno == EAGAIN || errno == EWOULDBLOCK) {
	    requestOutput ();
	    return AsyncIoResult::Again;
	}

	if (errno == EINTR)
	    return AsyncIoResult::Normal;

	if (errno == EPIPE) {
	  // If there'll be a need to distinguish Eof from Error, then this
	  // is the place to intervene.
	    return AsyncIoResult::Error;
	}

	exc_throw (PosixException, errno);
	exc_push_ (IoException);
	return AsyncIoResult::Error;
    } else
    if (res < 0) {
	exc_throw (InternalException, InternalException::BackendMalfunction);
	return AsyncIoResult::Error;
    }

    if ((Size) res > len) {
	exc_throw (InternalException, InternalException::BackendMalfunction);
	return AsyncIoResult::Error;
    }

    if (ret_nwritten)
	*ret_nwritten = res;

    return AsyncIoResult::Normal;
}

mt_throws Result
NativeAsyncFile::seek (FileOffset const offset,
                       SeekOrigin const origin)
{
  // TODO
    return Result::Success;
}

mt_throws Result
NativeAsyncFile::tell (FileSize * const ret_pos)
{
  // TODO
    return Result::Success;
}

mt_throws Result
NativeAsyncFile::sync ()
{
  // TODO
    return Result::Success;
}

mt_throws Result
NativeAsyncFile::close (bool const flush_data)
{
  // TODO
    return Result::Success;
}

void
NativeAsyncFile::setFd (int const fd)
{
    this->fd = fd;
}

void
NativeAsyncFile::resetFd ()
{
    fd = -1;
}

mt_throws Result
NativeAsyncFile::open (ConstMemory    const filename,
                       Uint32         const open_flags,
                       FileAccessMode const access_mode)
{
    int flags = 0;
    switch (access_mode) {
	case FileAccessMode::ReadOnly:
	    flags = O_RDONLY;
	    break;
	case FileAccessMode::WriteOnly:
	    flags = O_WRONLY;
	    break;
	case FileAccessMode::ReadWrite:
	    flags = O_RDWR;
	    break;
	default:
	    unreachable ();
    }

    if (open_flags && FileOpenFlags::Create)
	flags |= O_CREAT;

    // TODO Specify behavior for Truncate & O_RDONLY combination.
    if (open_flags & FileOpenFlags::Truncate)
	flags |= O_TRUNC;

    // TODO Seek to the end of file instead. O_APPEND semantics is too complicated.
    if (open_flags & FileOpenFlags::Append)
	flags |= O_APPEND;

    Ref<String> const filename_str = grab (new String (filename));

    for (;;) {
	/* NOTE: man 2 open does not mention EINTR as a possible return
	 * value, while man 3 open _does_. This means that EINTR should
	 * be handled for all invocations of open() in MyCpp (and all
	 * over MyNC). */
	fd = ::open (filename_str->cstr(),
		     // Note that O_DIRECT affects kernel-level caching/buffering
		     // and should not be set here.
		     flags | O_NONBLOCK,
		     S_IRUSR | S_IWUSR);
	if (fd == -1) {
	    if (errno == EINTR)
		continue;

            if (errno == EAGAIN) {
                logD_ (_func, "EAGAIN");
                break;
            }

            if (errno == EWOULDBLOCK) {
                logD_ (_func, "EWOULDBLOCK");
                break;
            }

	    exc_throw (PosixException, errno);
	    exc_push_ (IoException);
	    return Result::Failure;
	}

	break;
    }

    return Result::Success;
}

NativeAsyncFile::NativeAsyncFile (Object * const coderef_container,
                                  int      const fd)
    : DependentCodeReferenced (coderef_container),
      fd (fd)
{
}

NativeAsyncFile::~NativeAsyncFile ()
{
    if (fd != -1) {
	for (;;) {
	    int res = ::close (fd);
	    if (res == -1) {
		if (errno == EINTR)
		    continue;

		logE_ (_func, "close() failed: ", errnoString (errno));
	    } else
	    if (res != 0) {
		logE_ (_func, "close(): unexpected return value: ", res);
	    }

	    break;
	}
    }
}

}

