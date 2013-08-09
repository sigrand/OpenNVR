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


#ifndef LIBMARY__RECEIVER_H__
#define LIBMARY__RECEIVER_H__


#include <libmary/types.h>
#include <libmary/code_referenced.h>
#include <libmary/cb.h>
#include <libmary/exception.h>


namespace M {

class Receiver : public virtual CodeReferenced
{
public:
    class ProcessInputResult
    {
    public:
	enum Value {
	    // TODO "Normal" is equal to "Again" + ret_accepted = mem.len().
	    //      Rename "Again" to "Normal.
	    Normal,
	    Error,
	    // The buffer does not contain a complete message. Buffer contents
	    // must be shifted to the beginning of the buffer and more data
	    // should be received before processing.
	    Again,
	    // The buffer contains data which has not been analyzed yet.
	    // Does not require read buffer contents to be moved
	    // to the beginning of the buffer.
            InputBlocked
	};
	operator Value () const { return value; }
	ProcessInputResult (Value const value) : value (value) {}
	ProcessInputResult () {}
	Size toString_ (Memory const &mem, Format const &fmt) const;
    private:
	Value value;
    };

    struct Frontend {
	ProcessInputResult (*processInput) (Memory const &mem,
					    Size *ret_accepted,
					    void *cb_data);

        // TODO There may be unprocessed data in the input buffer.
        //      Pass it to processEof().
	void (*processEof) (void *cb_data);

	void (*processError) (Exception *exc_,
			      void      *cb_data);
    };

protected:
    mt_const Cb<Frontend> frontend;

public:
    virtual void unblockInput () = 0;

    mt_const void setFrontend (Cb<Frontend> const frontend)
        { this->frontend = frontend; }

    virtual ~Receiver () {}
};

}


#endif /* LIBMARY__RECEIVER_H__ */

