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


#ifndef LIBMARY__ANNOTATIONS__H__
#define LIBMARY__ANNOTATIONS__H__


// It is preferred not to include this header directly.
// Include types.h instead.

// Note that this source file may be included in plain C source files.


#define mt_begin
#define mt_end

// Function parameters marked with 'mt_nonnull' cannot be null. If null is
// passed for such a parameter, then function's behaviour is undefined.
#define mt_nonnull

// TODO Abolish mt_throw in favor of mt_throws
#define mt_throw(a)
// Throws LibMary exceptions.
#define mt_throws
// Throws C++ exceptions.
#define mt_throws_cpp

// Functions marked with 'mt_locked' lock state mutexes of their objects when
// called. They are called "locked functions".
#define mt_locked

#define mt_const
#define mt_mutex(a)
#define mt_locks(a)
#define mt_unlocks(a)
#define mt_unlocks_locks(a)
#define mt_sync(a)

#define mt_caller_exc_none
#define mt_exc_kind(a)

#define mt_iface(a)
#define mt_iface_end

#define mt_async
#define mt_unsafe

#define mt_one_of(a)
#define mt_sync_domain(a)


#endif /* LIBMARY__ANNOTATIONS__H__ */

