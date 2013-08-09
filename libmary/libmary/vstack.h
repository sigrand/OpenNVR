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


#ifndef LIBMARY__VSTACK__H__
#define LIBMARY__VSTACK__H__


#include <libmary/types.h>
#include <libmary/intrusive_list.h>


namespace M {

mt_unsafe class VStack
{
public:
    typedef Size Level;

private:
    // TODO Use one malloc for Block+buf,
    //      with buf allocated right after Block.
    //      Don't forget about alignment requirements
    //      for the first data block in buf.
    class Block : public IntrusiveListElement<>
    {
    public:
	Byte *buf;

	Size start_level;
	Size height;
    };

    typedef IntrusiveList<Block> BlockList;

    Size const block_size;
    bool const shrinking;

    Size level;

    BlockList block_list;
    Block *cur_block;

    Byte* addBlock (Size num_bytes);

public:
    Byte* push_unaligned (Size num_bytes);

    Byte* push_malign (Size num_bytes,
                       Size alignment);

    Level getLevel () const { return level; }

    void setLevel (Level new_level);

    void clear () { setLevel (0); }

    VStack (Size block_size /* > 0 */,
	    bool shrinking = false);

    ~VStack ();
};

}


#endif /* LIBMARY__VSTACK__H__ */

