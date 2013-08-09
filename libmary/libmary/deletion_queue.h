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


#ifndef __LIBMARY__DELETION_QUEUE__H__
#define __LIBMARY__DELETION_QUEUE__H__


#include <libmary/object.h>


namespace M {

void deletionQueue_append (Object *obj);

void deletionQueue_process ();

// This is only used for a debug warning when going to wait for a condition
// with a state mutex held. Perhaps something could be done about this.
bool deletionQueue_isEmpty ();

void deletionQueue_init ();

}


#endif /* __LIBMARY__DELETION_QUEUE__H__ */

