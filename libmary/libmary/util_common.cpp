
    
/*  LibMary - C++ library for high-performance network servers
    Copyright (C) 2011, 2012 Dmitry Shatrov

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


#include <libmary/types.h>
#include <glib.h>

#include <libmary/util_base.h>

#include <libmary/util_common.h>


namespace M {

void
randomSetSeed (Uint32 const seed)
{
    g_random_set_seed (seed);
}

// TODO Switch to using cryptographically secure random number generator
//      where necessary (seed PRNG from /dev/random?).
Uint32
randomUint32 ()
{
    /* From Glib docs on g_random_int():
     * "Return a random guint32 equally distributed over the range [0..2^32-1]." */
    Uint32 const res = (Uint32) g_random_int ();
    return res;
}

}

