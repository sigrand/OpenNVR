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


#ifndef LIBMARY__STRING__H__
#define LIBMARY__STRING__H__


#include <libmary/types_base.h>
#include <cstring>

#include <libmary/st_referenced.h>
#include <libmary/basic_referenced.h>
#include <libmary/memory.h>


namespace M {

// TODO Complete BasicReferenced -> StReferenced transition for class String.
class String : public BasicReferenced, public StReferenced
{
private:
    Byte *data_buf;
    // String length not including the zero byte at the end.
    Size  data_len;

private:
    static Byte no_data [1];

    String& operator = (String const &);
    String (String const &);

public:
    Memory mem () const
    {
	return Memory (data_buf, data_len);
    }

    // mem() plus space for terminating null byte for  makeStringInto().
    Memory cstrMem () const
    {
        return Memory (data_buf, data_len + 1);
    }

    Size len () const
    {
        return data_len;
    }

    char* cstr () const
    {
	return (char*) data_buf;
    }

    void set (ConstMemory const &mem)
    {
	if (data_buf != no_data)
	    delete[] data_buf;

	if (mem.len() != 0) {
	    data_buf = new Byte [mem.len() + 1];
	    memcpy (data_buf, mem.mem(), mem.len());
	    data_buf [mem.len()] = 0;
	    data_len = mem.len ();
	} else {
	    data_buf = no_data;
	    data_len = 0;
	}
    }

    // Use this carefully.
    void setLength (Size const len)
    {
        data_len = len;
    }

    // Deprecated in favor of isNullString()
    bool isNull () const
    {
	return data_buf == no_data;
    }

    bool isNullString () const
    {
        return data_buf == no_data;
    }

    // Allocates an additional byte for the trailing zero and sets it to 0.
    void allocate (Size const nbytes)
    {
	if (data_buf != no_data)
	    delete[] data_buf;

	if (nbytes > 0) {
	    data_buf = new Byte [nbytes + 1];
	    data_buf [nbytes] = 0;
	    data_len = nbytes;
	} else {
	    data_buf = no_data;
	    data_len = 0;
	}
    }

    template <Size N>
    String (char const (&str) [N])
    {
	if (N > 1) {
	    data_buf = new Byte [N];
	    memcpy (data_buf, str, N);
	    data_len = N - 1;
	} else {
	    data_buf = no_data;
	    data_len = 0;
	    return;
	}
    }

    String (char const * const str)
    {
	if (str != NULL) {
	    Size const len = strlen (str);
	    data_buf = new Byte [len + 1];
	    memcpy (data_buf, str, len + 1);
	    data_len = len;
	} else {
	    data_buf = no_data;
	    data_len = 0;
	    return;
	}
    }

    String (ConstMemory const &mem)
    {
	if (mem.len() != 0) {
	    data_buf = new Byte [mem.len() + 1];
	    memcpy (data_buf, mem.mem(), mem.len());
	    data_buf [mem.len()] = 0;
	    data_len = mem.len ();
	} else {
	    data_buf = no_data;
	    data_len = 0;
	}
    }

    // Preallocates (nbytes + 1) bytes and sets the last byte to 0.
    String (Size const nbytes)
    {
	if (nbytes > 0) {
	    data_buf = new Byte [nbytes + 1];
	    data_buf [nbytes] = 0;
	    data_len = nbytes;
	} else {
	    data_buf = no_data;
	    data_len = 0;
	}
    }

    String ()
	: data_buf (no_data),
	  data_len (0)
    {
    }

    ~String ()
    {
	if (data_buf != no_data)
	    delete[] data_buf;
    }
};

}


#endif /* LIBMARY__STRING__H__ */

