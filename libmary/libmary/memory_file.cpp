/*  LibMary - C++ library for high-performance network servers
    Copyright (C) 2013 Dmitry Shatrov

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


#include <libmary/memory_file.h>

//#include <libmary/io.h>


namespace M {

mt_throws IoResult
MemoryFile::read (Memory   const mem,
                  Size   * const ret_nread)
{
    if (ret_nread)
        *ret_nread = 0;

    if (array_mem.mem() == NULL) {
//        errs->println ("--- MemoryFile: array_mem.mem() == NULL");
        exc_throw (InternalException, InternalException::IncorrectUsage);
        return IoResult::Error;
    }

    if (pos >= array_mem.len()) {
//        errs->println ("--- MemoryFile: pos is large");
        return IoResult::Eof;
    }

    Size lim;

    if (array_mem.len() - pos < mem.len())
        lim = array_mem.len() - pos;
    else
        lim = mem.len();

    memcpy (mem.mem(), array_mem.mem() + pos, lim);
    pos += lim;

    if (ret_nread)
        *ret_nread = lim;

//    errs->println ("--- MemoryFile: ok, lim: ", lim);
    return IoResult::Normal;
}

mt_throws Result
MemoryFile::write (ConstMemory   const mem,
                   Size        * const ret_nwritten)
{
    if (ret_nwritten)
        *ret_nwritten = 0;

    if (array_mem.mem() == NULL) {
        exc_throw (InternalException, InternalException::IncorrectUsage);
        return Result::Failure;
    }

    if (pos >= array_mem.len()) {
        // TODO Is this correct? This mimcs NativeFile's behavior,
        //      which may be wrong, too.
        return Result::Success;
    }

    Size lim;
    lim = array_mem.len() - pos;
    if (lim > mem.len())
        lim = mem.len();

    memcpy (array_mem.mem() + pos, mem.mem(), lim);

    pos += lim;

    if (ret_nwritten)
        *ret_nwritten = lim;

    return Result::Success;
}

mt_throws Result
MemoryFile::flush ()
{
  // No-op
    return Result::Success;
}

mt_throws Result
MemoryFile::seek (FileOffset const offset,
                  SeekOrigin const origin)
{
//    errs->println ("--- MemoryFile::seek: offset: ", offset, ", origin: ", (Uint32) origin);

    if (array_mem.mem() == NULL) {
//        errs->println ("--- MemoryFile::seek: array_mem.mem() == NULL");
        exc_throw (InternalException, InternalException::IncorrectUsage);
        return Result::Failure;
    }

    if (origin == SeekOrigin::Beg) {
        if (offset < 0 ||
            (Size) offset > array_mem.len())
        {
//            errs->println ("--- MemoryFile::seek: Beg overflow");
            exc_throw (InternalException, InternalException::OutOfBounds);
            return Result::Failure;
        }

        pos = (Size) offset;
    } else
    if (origin == SeekOrigin::Cur) {
        if (offset > 0) {
            if ((Size) offset > array_mem.len() - pos) {
                exc_throw (InternalException, InternalException::OutOfBounds);
                return Result::Failure;
            }

            pos += (Size) offset;
        } else
        if (offset < 0) {
            if ((Size) -offset > pos) {
                exc_throw (InternalException, InternalException::OutOfBounds);
                return Result::Failure;
            }

            pos -= (Size) offset;
        }
    } else
    if (origin == SeekOrigin::End) {
        if (offset > 0 ||
            (Size) -offset > array_mem.len())
        {
            exc_throw (InternalException, InternalException::OutOfBounds);
            return Result::Failure;
        }

        pos = array_mem.len() - (Size) -offset;
    } else {
        unreachable ();
    }

    return Result::Success;
}

mt_throws Result
MemoryFile::tell (FileSize * const ret_pos)
{
    if (ret_pos)
        *ret_pos = pos;

    return Result::Success;
}

mt_throws Result
MemoryFile::sync ()
{
  // No-op
    return Result::Success;
}

mt_throws Result
MemoryFile::close (bool const /* flush_data */)
{
  // No-op
    return Result::Success;
}

MemoryFile::MemoryFile (Memory const mem)
    : array_mem (mem),
      pos (0)
{
}

}

