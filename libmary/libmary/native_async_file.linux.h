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


#ifndef LIBMARY__NATIVE_ASYNC_FILE__H__
#define LIBMARY__NATIVE_ASYNC_FILE__H__


#include <libmary/types.h>
#include <libmary/poll_group.h>
#include <libmary/async_file.h>


namespace M {

class NativeAsyncFile : public AsyncFile,
                        public virtual DependentCodeReferenced
{
private:
    int fd;

    Cb<PollGroup::Feedback> feedback;

    void requestInput ()
    {
	if (feedback && feedback->requestInput)
	    feedback.call (feedback->requestInput);
    }

    void requestOutput ()
    {
	if (feedback && feedback->requestOutput)
	    feedback.call (feedback->requestOutput);
    }

  mt_iface (PollGroup::Pollable)
    static PollGroup::Pollable const pollable;

    static void processEvents (Uint32  event_flags,
			       void   *_self);

    static int getFd (void *_self);

    static void setFeedback (Cb<PollGroup::Feedback> const &feedback,
			     void *_self);
  mt_iface_end

public:
  mt_iface (AsyncFile)
    mt_iface (AsyncInputStream)
      mt_throws AsyncIoResult read (Memory  mem,
                                    Size   *ret_nread);
    mt_iface_end

    mt_iface (AsyncOutputStream)
      mt_throws AsyncIoResult write (ConstMemory  mem,
                                     Size        *ret_nwritten);
    mt_iface_end

    mt_throws Result seek (FileOffset offset,
                             SeekOrigin origin);

    mt_throws Result tell (FileSize *ret_pos);

    mt_throws Result sync ();

    mt_throws Result close (bool flush_data = true);
  mt_iface_end

    void setFd (int fd);

    void resetFd ();

    mt_throws Result open (ConstMemory    filename,
                           Uint32         open_flags,
                           FileAccessMode access_mode);

    CbDesc<PollGroup::Pollable> getPollable ()
        { return CbDesc<PollGroup::Pollable> (&pollable, this, getCoderefContainer()); }

    NativeAsyncFile (Object *coderef_container,
                     int     fd = -1);

    ~NativeAsyncFile ();
};

}


#endif /* LIBMARY__NATIVE_ASYNC_FILE__H__ */

