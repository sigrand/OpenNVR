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


#ifndef PARGEN__COMPILE__H__
#define PARGEN__COMPILE__H__


#include <libmary/libmary.h>


namespace Pargen {

using namespace M;

class CompilationOptions : public StReferenced
{
public:
    StRef<String> module_name;
    StRef<String> capital_module_name;
    StRef<String> all_caps_module_name;

    StRef<String> capital_namespace_name;

    StRef<String> header_name;
    StRef<String> capital_header_name;
    StRef<String> all_caps_header_name;
};

}


#endif /* PARGEN__COMPILE__H__ */

