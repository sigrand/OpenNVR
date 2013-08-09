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


#ifndef LIBMARY__LINE_SERVER__H__
#define LIBMARY__LINE_SERVER__H__


#include <libmary/code_referenced.h>
#include <libmary/receiver.h>


namespace M {

class LineServer : public DependentCodeReferenced
{
public:
    struct Frontend
    {
        void (*line) (ConstMemory  line,
                      void        *cb_data);

        void (*closed) (void *cb_data);
    };

private:
    mt_const Size max_line_len;

    mt_const Cb<Frontend> frontend;

    mt_sync (processInput) Size recv_pos;

    mt_iface (Receiver::Frontend)
      static Receiver::Frontend const receiver_frontend;

      static Receiver::ProcessInputResult processInput (Memory   const &mem,
                                                        Size   * mt_nonnull ret_accepted,
                                                        void   *_self);

      static void processEof (void *_self);

      static void processError (Exception *exc_,
                                void      *_self);
    mt_iface_end

public:
    void init (Receiver               *receiver,
               CbDesc<Frontend> const &frontend,
               Size                    max_line_len = 4096);

    LineServer (Object *coderef_container);

    ~LineServer ();
};

}


#endif /* LIBMARY__LINE_SERVER__H__ */

