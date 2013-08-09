/*  MConfig - C++ library for working with configuration files
    Copyright (C) 2012 Dmitry Shatrov

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
#include <cctype>

#include <mconfig/util.h>


using namespace M;

namespace MConfig {

BooleanValue strToBoolean (ConstMemory const value_mem)
{
    if (value_mem.len() == 0)
	return Boolean_Default;

    // TODO malloc here is not necessary
    Ref<String> value_str = grab (new String (value_mem));
    Memory const mem = value_str->mem();
    for (Size i = 0; i < mem.len(); ++i)
	mem.mem() [i] = (Byte) tolower (mem.mem() [i]);

    if (equal (mem, "y")    ||
	equal (mem, "yes")  ||
	equal (mem, "on")   ||
	equal (mem, "true") ||
	equal (mem, "1"))
    {
	return Boolean_True;
    }

    if (equal (mem, "n")     ||
	equal (mem, "no")    ||
	equal (mem, "off")   ||
	equal (mem, "false") ||
	equal (mem, "0"))
    {
	return Boolean_False;
    }

    return Boolean_Invalid;
}

}

