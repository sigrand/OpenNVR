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

#include <gmodule.h>

#include <libmary/log.h>

#include <libmary/module.h>


namespace M {

mt_throws Result
loadModule (ConstMemory const &filename)
{
    Ref<String> filename_str = grab (new String (filename));
    GModule * const module = g_module_open ((gchar const*) filename_str->mem().mem(),
					    (GModuleFlags) (G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL));
    if (!module) {
	exc_throw (InternalException, InternalException::BackendError);
	logE_ (_func, "failed to open module ", filename, ": ",  g_module_error());
	return Result::Failure;
    }

    return Result::Success;
}

}

