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


#ifndef PARGEN__PARGEN_TASK_PARSER__H__
#define PARGEN__PARGEN_TASK_PARSER__H__


#include <libmary/libmary.h>

#include <pargen/token_stream.h>

#include <pargen/declarations.h>
#include <pargen/parsing_exception.h>


namespace Pargen {

using namespace M;

class PargenTask : public StReferenced
{
public:
    List< StRef<Declaration> > decls;
};

mt_throws Result parsePargenTask (TokenStream       * mt_nonnull token_stream,
                                  StRef<PargenTask> * mt_nonnull ret_pargen_task);

void dumpDeclarations (PargenTask const * mt_nonnull pargen_task);

}


#endif /* PARGEN__PARGEN_TASK_PARSER__H__ */

