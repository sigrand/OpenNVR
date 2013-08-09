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


#ifndef PARGEN__SOURCE_COMPILER__H__
#define PARGEN__SOURCE_COMPILER__H__


#include <libmary/libmary.h>

#include <pargen/declarations.h>
#include <pargen/pargen_task_parser.h>
#include <pargen/compile.h>


namespace Pargen {

using namespace M;

mt_throws Result compileSource (File                     *file,
                                PargenTask const         *pargen_task,
                                CompilationOptions const *opts);

}


#endif /* PARGEN__SOURCE_COMPILER__H__ */

