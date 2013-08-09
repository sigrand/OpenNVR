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


#include <libmary/log.h>

#include <libmary/vstack.h>


namespace M {

Byte*
VStack::addBlock (Size const num_bytes)
{
    Byte *ret_buf = NULL;

    if (cur_block != NULL &&
        block_list.getNext (cur_block) != NULL)
    {
      // Reusing allocated block.

        Block * const block = block_list.getNext (cur_block);
        block->start_level = level;
        block->height = num_bytes;

        cur_block = block;
        ret_buf = block->buf;
    } else {
      // Allocating a new block.

        // TODO 'block.buf' could be allocated along with 'block'
        // in the same chunk of memory.
        Block * const block = new (std::nothrow) Block;
        assert (block);
        block_list.append (block);

        block->buf = new (std::nothrow) Byte [block_size];
        assert (block->buf);
        block->start_level = level;
        block->height = num_bytes;

        cur_block = block_list.getLast();
        ret_buf = block->buf;
    }

    level += num_bytes;
    return ret_buf;
}

Byte*
VStack::push_unaligned (Size const num_bytes)
{
    assert (num_bytes <= block_size);

    if (block_list.isEmpty () ||
        block_size - cur_block->height < num_bytes)
    {
        return addBlock (num_bytes);
    }

    Block * const block = cur_block;

    Size const prv_height = block->height;
    block->height += num_bytes;

    level += num_bytes;
    return block->buf + prv_height;
}

// Returned address meets alignment requirements for an object of class A
// if alignof(A) is @alignment.
// Returned address is always a multiple of @alignment bytes away from
// the start of the corresponding block.
Byte*
VStack::push_malign (Size const num_bytes,
                     Size const alignment)
{
    assert (num_bytes <= block_size);

    if (block_list.isEmpty () ||
        block_size - cur_block->height < num_bytes)
    {
        return addBlock (num_bytes);
    }

    Block * const block = cur_block;

    //
    //  block->height
    //   ____|_____     new aligned addr
    //  /          \         |
    // +============+--------+----------+------->
    // | used space |        |          |
    // +============+--------+----------+------->
    //              |                   |
    //      new unaligned addr    new unaligned height
    //               \_________________/
    //                        | 
    //                    num_bytes
    //
    //

    Byte *ptr = (Byte*) alignPtr (block->buf + block->height, alignment);
    Size const new_height = ptr + num_bytes - block->buf;
    if (new_height > block_size)
        return addBlock (num_bytes);

    Size const prv_height = block->height;
    block->height = new_height;

    level += new_height - prv_height;
    return ptr;
}

void
VStack::setLevel (Level const new_level)
{
    if (!block_list.isEmpty ()) {
        if (cur_block->start_level > new_level) {
            Block * const prv_block = block_list.getPrevious (cur_block);

            if (shrinking) {
              // Deleting block.
                delete[] cur_block->buf;
                block_list.remove (cur_block);
                delete cur_block;
              // 'cur_block' is not valid anymore
            }

            cur_block = prv_block;
        } else {
            cur_block->height = new_level - cur_block->start_level;
        }
    }

    level = new_level;
}

VStack::VStack (Size const block_size,
                bool const shrinking)
    : block_size (block_size),
      shrinking (shrinking),
      level (0),
      cur_block (NULL)
{
}

VStack::~VStack ()
{
    BlockList::iter iter (block_list);
    while (!block_list.iter_done (iter)) {
        Block * const block = block_list.iter_next (iter);
        delete[] block->buf;
        delete block;
    }
}

}

