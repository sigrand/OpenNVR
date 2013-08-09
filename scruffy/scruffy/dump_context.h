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


#ifndef SCRUFFY__DUMP_CONTEXT_H__
#define SCRUFFY__DUMP_CONTEXT_H__

#include <mycpp/mycpp.h>

#include <scruffy/cpp.h>

namespace Scruffy {

using namespace MyCpp;

namespace Cpp {

using namespace MyCpp;

// All I/O methods throw (InternalException, IOException).
class DumpContext : public virtual SimplyReferenced
{
private:
    Ref<File> file;
    Size tab_size;

    class DumpDeclaration_Data;

    void dumpClassNestedNameSpecifier (Cpp::Container *dst_container);

    void do_dumpDeclaration (TypeDesc             *type_desc,
			     Cpp::Container       *nested_name_container,
			     Name                 *name,
			     Size                  nest_level,
//			     TypeDesc       const *inner_type_desc = NULL,
			     Bool                  after_ret_type,
			     Bool                  first_type_atom,
			     List< Ref<TypeDesc::TypeAtom> >::Element const *cur_type_atom,
			     Bool                  expand_definitions,
			     DumpDeclaration_Data *data);

public:
    void outTab (Size nest_level);

    void outNewline ();

    void outNewlineTab (Size nest_level);

    void dumpTypeSpecifier (TypeDesc *type_desc);

    void dumpClass (Class *class_,
		    Size   nest_level = 0);

    void dumpName (Cpp::Name *name);

    void dumpMember (Cpp::Member const &member,
		     Size               nest_level = 0,
		     Cpp::Container    *nested_name_container = NULL);

    void dumpType (Cpp::Member *type_entry,
		   Size         nest_level = 0);

    void dumpInitializer (Initializer *intializer);

    void dumpDeclaration (TypeDesc *type_desc,
			  Name     *name,
			  Size      nest_level = 0);

    void dumpObject (Member *member,
		     Size    nest_level = 0);

    void dumpExpression (Expression *expression,
			 Size        nest_level = 0);

    void dumpStatement (Statement *statement,
			Size       nest_level = 0);

    void dumpMemInitializer (MemInitializer *mem_initializer,
			     Size            nest_level = 0);

    void dumpFunction (Function *function_definition,
		       Size      nest_level = 0);

    void dumpNamespace (Namespace *namespace_,
			Size       nest_level = 0);

    void dumpTranslationUnit (Namespace *namespace_);

    DumpContext (File *file,
		 Size  tab_size = 4)
	: file (file),
	  tab_size (tab_size)
    {
	abortIf (file == NULL);
    }
};

} // namespace Cpp
} // namespace Scruffy

#endif /* SCRUFFY__DUMP_CONTEXT_H__ */

