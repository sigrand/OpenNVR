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


#ifndef LIBMARY__ASYNC_INPUT_STREAM__H__
#define LIBMARY__ASYNC_INPUT_STREAM__H__


#include <libmary/code_referenced.h>
#include <libmary/exception.h>
#include <libmary/cb.h>


namespace M {

class AsyncInputStream : public virtual DependentCodeReferenced
{
#ifdef LIBMARY_WIN32_IOCP
public:
    virtual mt_throws AsyncIoResult read (OVERLAPPED * mt_nonnull overlapped,
                                          Memory      mem,
                                          Size       *ret_nread) = 0;
#else
public:
    struct InputFrontend {
	void (*processInput) (void *cb_data);

	void (*processError) (Exception *exc_,
			      void      *cb_data);
    };

protected:
    mt_const Cb<InputFrontend> input_frontend;

public:
    virtual mt_throws AsyncIoResult read (Memory  mem,
					  Size   *ret_nread) = 0;

    mt_const void setInputFrontend (CbDesc<InputFrontend> const &input_frontend)
        { this->input_frontend = input_frontend; }
#endif

    AsyncInputStream ()
        : DependentCodeReferenced (NULL /* coderef_container */)
    {}

    virtual ~AsyncInputStream () {}
};

}


#endif /* LIBMARY__ASYNC_INPUT_STREAM__H__ */

