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


#ifndef LIBMARY__CONNECTION__H__
#define LIBMARY__CONNECTION__H__


#include <libmary/types.h>
#include <libmary/async_input_stream.h>
#include <libmary/async_output_stream.h>


namespace M {

class Connection : public AsyncInputStream,
		   public AsyncOutputStream
{
public:
    // TODO Варианты:
    //      fin ();   // (частичный shutdown - если понадобится)
    //      close (); // <- Не асинхронный. Асинхронным занимаается Sender.
    //                // НО нужно помнить о SO_LINGER, хотя он здесь и не нужен.
//    virtual mt_throws Result close () = 0;

#ifdef LIBMARY_ENABLE_MWRITEV
    virtual int getFd () = 0;
#endif

    Connection ()
        : DependentCodeReferenced (NULL /* coderef_container */)
    {}

    virtual ~Connection () {}
};

}


#endif /* LIBMARY__CONNECTION__H__ */

