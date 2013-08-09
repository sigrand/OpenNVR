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


#ifndef LIBMARY__LINE_PIPE__H__
#define LIBMARY__LINE_PIPE__H__


#include <libmary/code_referenced.h>
#include <libmary/timers.h>
#include <libmary/poll_group.h>
#include <libmary/native_async_file.h>
#include <libmary/connection_receiver.h>
#include <libmary/line_server.h>


namespace M {

class LinePipe : public DependentCodeReferenced
{
private:
    StateMutex mutex;

public:
    struct Frontend {
        void (*line) (ConstMemory  line,
                      void        *cb_data);
    };

private:
    mt_const Ref<String> filename;
    mt_const Time reopen_timeout_millisec;
    mt_const Size max_line_len;

    mt_const Cb<Frontend> frontend;

    mt_const DataDepRef<PollGroup> poll_group;
    mt_const DataDepRef<DeferredProcessor> deferred_processor;
    mt_const DataDepRef<Timers> timers;

    class PipeSession : public Object
    {
    public:
        mt_const WeakDepRef<LinePipe> weak_line_pipe;

        mt_const PollGroup::PollableKey pollable_key;

        NativeAsyncFile     line_file;
        ConnectionReceiver  line_receiver;
        LineServer          line_server;

        PipeSession ();
    };

    mt_mutex (mutex) Ref<PipeSession> pipe_session;

    mt_mutex (mutex) Timers::TimerKey reopen_timer;
    mt_mutex (mutex) StRef<String> prv_error_str;

  mt_iface (LineServer::Frontend)
    static LineServer::Frontend const line_frontend;

    static void line (ConstMemory  line,
                      void        *_pipe_session);

    static void closed (void *_pipe_session);
  mt_iface_end

    static void reopenTimerTick (void *_self);

    mt_mutex (mutex) void releasePipeSession ();

    mt_mutex (mutex) mt_throws Result openPipeSession ();

public:
    mt_throws Result init (ConstMemory             filename,
                           CbDesc<Frontend> const &frontend,
                           PollGroup              * mt_nonnull poll_group,
                           DeferredProcessor      * mt_nonnull deferred_processor,
                           Timers                 *timers,
                           Time                    reopen_timeout_millisec,
                           Size                    max_line_len = 4096);

     LinePipe (Object *coderef_container);
    ~LinePipe ();
};

}


#endif /* LIBMARY__LINE_PIPE__H__ */

