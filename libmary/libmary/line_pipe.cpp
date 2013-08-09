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


#include <libmary/log.h>

#include <libmary/line_pipe.h>


namespace M {

LinePipe::PipeSession::PipeSession ()
    : line_file     (this /* coderef_container */),
      line_receiver (this /* coderef_container */),
      line_server   (this /* coderef_container */)
{
}

LineServer::Frontend const LinePipe::line_frontend = {
    line,
    closed
};

void
LinePipe::line (ConstMemory   const line,
                void        * const _pipe_session)
{
    PipeSession * const pipe_session = static_cast <PipeSession*> (_pipe_session);
    CodeDepRef<LinePipe> const self = pipe_session->weak_line_pipe;
    if (!self)
        return;

    self->mutex.lock ();

    if (self->pipe_session != pipe_session) {
        logF_ (_func, "WARNING: spurious line: ", line);
        self->mutex.unlock ();
        return;
    }

    if (self->frontend)
        self->frontend.call (self->frontend->line, line);

    self->mutex.unlock ();
}

void
LinePipe::closed (void * const _pipe_session)
{
    PipeSession * const pipe_session = static_cast <PipeSession*> (_pipe_session);
    CodeDepRef<LinePipe> const self = pipe_session->weak_line_pipe;
    if (!self)
        return;
    
    self->mutex.lock ();

    if (self->pipe_session != pipe_session) {
        self->mutex.unlock ();
        return;
    }

    self->releasePipeSession ();
    self->openPipeSession ();

    self->mutex.unlock ();
}

void
LinePipe::reopenTimerTick (void * const _self)
{
    LinePipe * const self = static_cast <LinePipe*> (_self);

    assert (self->timers);

    self->mutex.lock ();
    self->timers->deleteTimer (self->reopen_timer);
    self->reopen_timer = NULL;

    if (self->pipe_session) {
        logF_ (_func, "WARNING: pipe_session is not NULL");
        self->mutex.unlock ();
        return;
    }

    self->openPipeSession ();

    self->mutex.unlock ();
}

mt_mutex (mutex) void
LinePipe::releasePipeSession ()
{
    if (!pipe_session)
        return;

    assert (pipe_session->pollable_key);
    poll_group->removePollable (pipe_session->pollable_key);
    pipe_session->pollable_key = NULL;

    if (!pipe_session->line_file.close (false /* flush_data */))
        logE_ (_func, "could not close pipe file: ", exc->toString());

    pipe_session = NULL;
}

mt_mutex (mutex) mt_throws Result
LinePipe::openPipeSession ()
{
    assert (!pipe_session);
    pipe_session = grab (new (std::nothrow) PipeSession);
    pipe_session->weak_line_pipe = this;

    bool opened = false;
    if (!pipe_session->line_file.open (filename->mem(), 0 /* open_flags */, FileAccessMode::ReadOnly)) {
        goto _close;
    }
    opened = true;

    pipe_session->pollable_key = poll_group->addPollable (pipe_session->line_file.getPollable(),
                                                          false /* activate */);
    if (!pipe_session->pollable_key)
        goto _close;

    pipe_session->line_receiver.init (&pipe_session->line_file,
                                      deferred_processor);
    pipe_session->line_server.init (&pipe_session->line_receiver,
                                    CbDesc<LineServer::Frontend> (&line_frontend, pipe_session, pipe_session),
                                    max_line_len);

    if (!poll_group->activatePollable (pipe_session->pollable_key)) {
        poll_group->removePollable (pipe_session->pollable_key);
        pipe_session->pollable_key = NULL;
        goto _close;
    }

    return Result::Success;

_close:
    {
        Ref<String> const error_str = exc->toString();
        if (!prv_error_str || !equal (prv_error_str->mem(), error_str ? error_str->mem() : ConstMemory())) {
            prv_error_str = st_grab (new (std::nothrow) String (error_str ? error_str->mem() : ConstMemory()));
            logW_ (_func, "could not open pipe\"", filename, "\": ", error_str);
        }
    }

    ExceptionBuffer * const exc_buf = exc_swap_nounref ();

    if (opened) {
        if (!pipe_session->line_file.close (false /* flush_data */))
            logE_ (_func, "file.close() failed: ", exc->toString());
    }

    pipe_session = NULL;

    if (timers) {
        reopen_timer = timers->addTimer_microseconds (CbDesc<Timers::TimerCallback> (reopenTimerTick,
                                                                                     this,
                                                                                     getCoderefContainer()),
                                                      reopen_timeout_millisec * 1000,
                                                      false /* periodical */);
    }

    exc_set_noref (exc_buf);
    return Result::Failure;
}

mt_throws Result
LinePipe::init (ConstMemory         const filename,
                CbDesc<Frontend>    const &frontend,
                PollGroup         * const mt_nonnull poll_group,
                DeferredProcessor * const mt_nonnull deferred_processor,
                Timers            * const timers,
                Time                const reopen_timeout_millisec,
                Size                const max_line_len)
{
    this->filename = grab (new (std::nothrow) String (filename));

    this->poll_group = poll_group;
    this->deferred_processor = deferred_processor;
    this->timers = timers;
    this->reopen_timeout_millisec = reopen_timeout_millisec;
    this->max_line_len = max_line_len;

    this->frontend = frontend;

    mutex.lock ();
    Result const res = openPipeSession ();
    mutex.unlock ();
    if (!timers && !res)
        return Result::Failure;

    return Result::Success;
}

LinePipe::LinePipe (Object * const coderef_container)
    : DependentCodeReferenced (coderef_container),
      reopen_timeout_millisec (0),
      max_line_len       (4096),
      poll_group         (coderef_container),
      deferred_processor (coderef_container),
      timers             (coderef_container)
{
}

LinePipe::~LinePipe ()
{
    mutex.lock ();
    releasePipeSession ();
    mutex.unlock ();
}

}

