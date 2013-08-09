/*  Pargen - Flexible parser generator
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


#ifndef PARGEN__UTIL__H__
#define PARGEN__UTIL__H__


#include <libmary/libmary.h>


namespace Pargen {

using namespace M;

StRef<String> capitalizeName (ConstMemory name,
                              bool        keep_underscore = true);

StRef<String> capitalizeNameAllCaps (ConstMemory name);

StRef<String> lowercaseName (ConstMemory name);

}


#endif /* PARGEN__UTIL__H__ */

