/*  Scruffy - C/C++ parser and source code analyzer
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


#ifndef SCRUFFY__CPP_PARSER_H__
#define SCRUFFY__CPP_PARSER_H__

#include <pargen/lookup_data.h>

#include <scruffy/cpp.h>

namespace Scruffy {

using namespace MyCpp;

class CppParser_Impl;

class CppParser : public virtual SimplyReferenced
{
private:
    Ref<CppParser_Impl> impl;

public:
    Ref<Pargen::LookupData> getLookupData ();

    Ref<Cpp::Namespace> getRootNamespace ();

    CppParser (ConstMemoryDesc const &default_variant = ConstMemoryDesc::forString ("default"));

    // This prevents clang from trying to instantiate Ref<CppParser_Impl>::~Ref too early.
    ~CppParser ();
};

}

#endif /* SCRUFFY__CPP_PARSER_H__ */

