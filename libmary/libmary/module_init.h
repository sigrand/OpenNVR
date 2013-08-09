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


#ifndef __LIBMARY__MODULE_INIT__H__
#define __LIBMARY__MODULE_INIT__H__


#include <libmary/types.h>
#include <gmodule.h>


// This file must be included just once by module code.
// It provides LibMary-specific static initialization functions.
// Glib's implementation of modules is used currently.


namespace M {

extern void libMary_moduleInit ();

extern void libMary_moduleUnload ();

}


extern "C" {

G_MODULE_EXPORT const gchar* g_module_check_init (GModule * const /* module */)
{
    M::libMary_moduleInit ();
    return NULL;
}

G_MODULE_EXPORT void g_module_unload (GModule * const /* module */)
{
    M::libMary_moduleUnload ();
}

}


#endif /* __LIBMARY__MODULE_INIT__H__ */

