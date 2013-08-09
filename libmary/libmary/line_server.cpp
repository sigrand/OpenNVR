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
#include <libmary/util_dev.h>

#include <libmary/line_server.h>


namespace M {

Receiver::Frontend const LineServer::receiver_frontend = {
    processInput,
    processEof,
    processError
};

Receiver::ProcessInputResult
LineServer::processInput (Memory   const &_mem,
                          Size   * const mt_nonnull ret_accepted,
                          void   * const  _self)
{
    LineServer * const self = static_cast <LineServer*> (_self);
    Memory mem = _mem;

    *ret_accepted = 0;

    for (;;) {
        assert (mem.len() >= self->recv_pos);
        if (mem.len() == self->recv_pos) {
          // No new data since the last input event => nothing changed.
            return Receiver::ProcessInputResult::Again;
        }

        Size cr_pos;
        {
            Byte const *cr_ptr = NULL;
            for (Size i = self->recv_pos, i_end = mem.len(); i < i_end; ++i) {
                if (mem.mem() [i] == 13 /* CR */ ||
                    mem.mem() [i] == 10 /* LF */)
                {
                    cr_ptr = mem.mem() + i;
                    break;
                }
            }

            if (!cr_ptr) {
                self->recv_pos = mem.len();
                if (self->recv_pos >= self->max_line_len)
                    return Receiver::ProcessInputResult::Error;

                return Receiver::ProcessInputResult::Again;
            }

            cr_pos = cr_ptr - mem.mem();
        }

        Size line_start = 0;
        while (mem.mem() [line_start] == 13 /* CR */ ||
               mem.mem() [line_start] == 10 /* LF */)
        {
            ++line_start;
        }

        if (line_start >= cr_pos) {
            self->recv_pos = 0;
            mem = mem.region (line_start);
            *ret_accepted += line_start;
            continue;
        }

        Size const next_pos = cr_pos + 1;

        if (self->frontend)
            self->frontend.call (self->frontend->line, mem.region (line_start, cr_pos));

        self->recv_pos = 0;
        mem = mem.region (next_pos);
        *ret_accepted += next_pos;
    }

    return Receiver::ProcessInputResult::Normal;
}

void
LineServer::processEof (void * const _self)
{
    LineServer * const self = static_cast <LineServer*> (_self);
    if (self->frontend)
        self->frontend.call (self->frontend->closed);
}

void
LineServer::processError (Exception * const /* exc_ */,
                          void      * const _self)
{
    LineServer * const self = static_cast <LineServer*> (_self);
    if (self->frontend)
        self->frontend.call (self->frontend->closed);
}

void
LineServer::init (Receiver         * const  receiver,
                  CbDesc<Frontend>   const &frontend,
                  Size               const  max_line_len)
{
    this->frontend = frontend;
    this->max_line_len = max_line_len;

    receiver->setFrontend (Cb<Receiver::Frontend> (&receiver_frontend, this, getCoderefContainer()));
}

LineServer::LineServer (Object * const coderef_container)
    : DependentCodeReferenced (coderef_container),
      max_line_len (4096),
      recv_pos (0)
{
}

LineServer::~LineServer ()
{
}

}

