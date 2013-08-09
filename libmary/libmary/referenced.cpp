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


#include <libmary/types.h>
#include <cstdio>

#include <libmary/util_base.h>

#include <libmary/referenced.h>


namespace M {

#ifdef LIBMARY_REF_TRACING
void
Referenced::traceRef ()
{
    char * const bt = rawCollectBacktrace ();
    fprintf (stderr, "reftrace:   ref 0x%lx, rc %u\n%s\n",
	     (unsigned long) this, (unsigned) refcount.get(), bt ? bt : "");
    delete[] bt;
}

void
Referenced::traceUnref ()
{
    char * const bt = rawCollectBacktrace ();
    fprintf (stderr, "reftrace: unref 0x%lx, rc %u\n%s\n",
	     (unsigned long) this, (unsigned) refcount.get(), bt ? bt : "");
    delete[] bt;
}
#endif // LIBMARY_REF_TRACING

}

