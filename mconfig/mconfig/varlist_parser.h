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


#ifndef MCONFIG__VARLIST_PARSER__H__
#define MCONFIG__VARLIST_PARSER__H__


#include <libmary/types.h>
#include <pargen/parser.h>

#include <mconfig/varlist.h>


namespace MConfig {

using namespace M;

class VarlistParser
{
private:
    mt_const StRef<Pargen::Grammar> grammar;
    mt_const StRef<Pargen::ParserConfig> parser_config;

public:
    Result parseVarlist (ConstMemory  filename,
                         Varlist     *varlist);

    VarlistParser ();
};

}


#endif /* MCONFIG__VARLIST_PARSER__H__ */

