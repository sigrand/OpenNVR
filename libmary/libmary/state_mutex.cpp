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


//#include <cstdio>

#include <libmary/types.h>
#include <libmary/libmary_thread_local.h>

#include <libmary/state_mutex.h>


namespace M {

#ifdef LIBMARY_MT_SAFE
void
StateMutex::lock ()
{
#ifdef LIBMARY_MT_SAFE
    mutex.lock ();
#endif

    {
	LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal ();

//	fprintf (stderr, "0x%lx %s: tlocal 0x%lx, BEFORE %lu\n", (unsigned long) this,
//		 __func__, (unsigned long) tlocal, (unsigned long) tlocal->state_mutex_counter);
	++ tlocal->state_mutex_counter;
//	fprintf (stderr, "0x%lx %s: tlocal 0x%lx, AFTER  %lu\n", (unsigned long) this,
//		 __func__, (unsigned long) tlocal, (unsigned long) tlocal->state_mutex_counter);
    }
}

void StateMutex::unlock ()
{
    {
	LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal ();

//	fprintf (stderr, "0x%lx %s: tlocal 0x%lx, %lu\n", (unsigned long) this,
//		 __func__, (unsigned long) tlocal, (unsigned long) tlocal->state_mutex_counter);
	assert (tlocal->state_mutex_counter > 0);
	-- tlocal->state_mutex_counter;

	if (tlocal->state_mutex_counter == 0) {
#ifdef LIBMARY_MT_SAFE
	    mutex.unlock ();
#endif

	    deletionQueue_process ();
	    return;
	}
    }

#ifdef LIBMARY_MT_SAFE
    mutex.unlock ();
#endif
}
#endif

}

