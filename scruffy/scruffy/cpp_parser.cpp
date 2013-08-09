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


#include <mycpp/io.h>

#include <pargen/parser.h>

#include "cpp_pargen.h"

#include <scruffy/cpp_util.h>

#include <scruffy/preprocessor_types.h>
#include <scruffy/checkpoint_tracker.h>

#include <scruffy/cpp_parser.h>
#include <scruffy/dump_context.h>


#define DEBUG(a) a
#define DEBUG_FLOW(a) ;
#define DEBUG_OLD(a) ;

#define FUNC_NAME(a) a


using namespace MyCpp;
using namespace Pargen;

namespace Scruffy {

namespace {

    template <class T>
    class Acceptor : public SimplyReferenced
    {
    public:
	virtual void accept (T *obj) = 0;

	// TODO This method should go away. (shouldn't it?)
	virtual void cancel ()
	{
	    abortIfReached ();
	}
    };

    template <class T>
    class Acceptor_Bound_Ref : public Acceptor<T>
    {
    private:
	// TODO Use Cancellable_Ref from above
	class Cancellable_Ref : public CheckpointTracker::Cancellable
	{
	private:
	    Ref<T> * const ref;

	public:
	    void cancel ()
	    {
		if (ref != NULL)
		    *ref = NULL;
	    }

	    Cancellable_Ref (Ref<T> * const ref)
		: ref (ref)
	    {
	    }
	};

	CheckpointTracker * const checkpoint_tracker;
	Ref<T> * const ref;

	CheckpointTracker::CheckpointKey checkpoint_key;

    public:
	void accept (T * const obj)
	{
	    if (ref != NULL) {
		*ref = obj;

		if (checkpoint_tracker != NULL) {
		    Ref<Cancellable_Ref> cancellable = grab (new Cancellable_Ref (ref));
		    checkpoint_tracker->addBoundCancellable (cancellable, checkpoint_key);
		}
	    }
	}

	void cancel ()
	{
	    abortIfReached ();
	}

	Acceptor_Bound_Ref (CheckpointTracker * const checkpoint_tracker,
			    Ref<T> * const ref)
	    : checkpoint_tracker (checkpoint_tracker),
	      ref (ref)
	{
	    checkpoint_key = checkpoint_tracker->getCurrentCheckpoint ();
	}
    };

    template <class T>
    class Acceptor_Bound_List : public Acceptor<T>
    {
    private:
	CheckpointTracker * const checkpoint_tracker;
	List< Ref<T> > * const list;

	CheckpointTracker::CheckpointKey checkpoint_key;

    public:
	void accept (T * const obj)
	{
	    if (list != NULL) {
		list->append (obj);

		if (checkpoint_tracker != NULL) {
		    Ref< Cancellable_ListElement< Ref<T> > > cancellable =
			    grab (new Cancellable_ListElement< Ref<T> > (list, list->last));

		    checkpoint_tracker->addBoundCancellable (cancellable, checkpoint_key);
		}
	    }
	}

	void cancel ()
	{
	    abortIfReached ();
	}

	Acceptor_Bound_List (CheckpointTracker * const checkpoint_tracker,
			     List< Ref<T> > * const list)
	    : checkpoint_tracker (checkpoint_tracker),
	      list (list)
	{
	    checkpoint_key = checkpoint_tracker->getCurrentCheckpoint ();
	}
    };

    template <class T>
    class Acceptor_Ref : public Acceptor<T>
    {
    private:
	Ref<T> * const ref;

    public:
	void accept (T * const obj)
	{
	    if (ref != NULL)
		*ref = obj;
	}

	void cancel ()
	{
	    if (ref != NULL)
		*ref = NULL;
	}

	Acceptor_Ref (Ref<T> * const ref)
	    : ref (ref)
	{
	}
    };

    template <class T>
    class Acceptor_List : public Acceptor<T>
    {
    private:
	List< Ref<T> > * const list;

    public:
	void accept (T * const obj)
	{
	    if (list != NULL)
		list->append (obj);
	}

	void cancel ()
	{
	    if (list != NULL)
		list->remove (list->last);
	}

	Acceptor_List (List< Ref<T> > * const list)
	    : list (list)
	{
	}
    };

    class DeclarationState;

    class TypeSpecifierParser
    {
    private:
	enum TypeCategory
	{
	    Unknown,
	    BasicType,
	    Class,
	    Template,
	    Enum,
	    Typedef
	};

	TypeCategory type_category;

	Bool is_const;
	Bool is_volatile;

	// For TypeCategory::BasicType
	Cpp::TypeDesc_BasicType::BasicType basic_type;
	// For TypeCategory::class_
	Ref<Cpp::Class> class_;
	// For TypeCategory::Enum
	Ref<Cpp::Enum> enum_;
	// For TypeCategory::Typedef
	Ref<Cpp::TypeDesc> typedef__type_desc;

	Bool has_int;
	Bool has_signed;
	Bool has_unsigned;

	Bool simple_type_specifier_used;

	// TODO These data fields belong to DeclSpecifierParser.
	// {
	    Bool is_auto,
		 is_register,
		 is_static,
		 is_extern,
		 is_mutable;

	    Bool is_inline,
		 is_virtual,
		 is_explicit;

	    Bool is_friend,
		 is_typedef;
	// }

    public:
	// May only be used when parsing declarations (with a valid DeclarationState).
	void parseTypeSpecifier (Cpp_TypeSpecifier const * const type_specifier,
				 DeclarationState               &declaration_state);

	// May be used independently (no DeclarationState required).
	void parseSimpleTypeSpecifier (Cpp_SimpleTypeSpecifier const *simple_type_specifier);

	bool gotType ()
	{
	    return type_category != Unknown;
	}

	void setBasicType_Void ()
	{
	    static char const * const _func_name = "Scruffy.CppParser.TypeSpecifierParser.setBasicType_Void";

	    if (type_category != Unknown) {
		errf->print (_func_name).print (": more than one data type").pendl ();
		abortIfReached ();
	    }

	    type_category = BasicType;
	    basic_type = Cpp::TypeDesc_BasicType::Void;
	}

	void setBasicType_Bool ()
	{
	    static char const * const _func_name = "Scruffy.CppParser.TypeSpecifierParser.setBasicType_Bool";

	    if (type_category != Unknown) {
		errf->print (_func_name).print (": more than one data type").pendl ();
		abortIfReached ();
	    }

	    type_category = BasicType;
	    basic_type = Cpp::TypeDesc_BasicType::Bool;
	}

	void setBasicType_Int ()
	{
	    static char const * const _func_name = "Scruffy.CppParser.TypeSpecifierParser.setBasicType_Int";

	    if (type_category != Unknown &&
		type_category != BasicType)
	    {
		errf->print (_func_name).print (": more than one data type").pendl ();
		abortIfReached ();
	    }

	    if (has_int) {
		errf->print (_func_name).print (": more than one data type").pendl ();
		abortIfReached ();
	    }

	    if (type_category == BasicType) {
		switch (basic_type) {
		    case Cpp::TypeDesc_BasicType::Int:
		    case Cpp::TypeDesc_BasicType::UnsignedInt:
		    case Cpp::TypeDesc_BasicType::ShortInt:
		    case Cpp::TypeDesc_BasicType::LongInt:
		    case Cpp::TypeDesc_BasicType::UnsignedShortInt:
		    case Cpp::TypeDesc_BasicType::UnsignedLongInt:
			break;
		    default:
			errf->print (_func_name).print (": more than one data type").pendl ();
			abortIfReached ();
		}
	    } else {
		abortIf (type_category != Unknown);
		type_category = BasicType;
		basic_type = Cpp::TypeDesc_BasicType::Int;
	    }

	    has_int = true;
	}

	void setBasicType_Char ()
	{
	    static char const * const _func_name = "Scruffy.CppParser.TypeSpecifierParser.setBasicType_Char";

	    if (type_category != Unknown &&
		type_category != BasicType)
	    {
		errf->print (_func_name).print (": more than one data type").pendl ();
		abortIfReached ();
	    }

	    if (has_int) {
		errf->print (_func_name).print (": more than one data type").pendl ();
		abortIfReached ();
	    }

	    if (type_category == BasicType) {
		switch (basic_type) {
		    case Cpp::TypeDesc_BasicType::Int: {
			basic_type = Cpp::TypeDesc_BasicType::SignedChar;
		    } break;
		    case Cpp::TypeDesc_BasicType::UnsignedInt: {
			basic_type = Cpp::TypeDesc_BasicType::UnsignedChar;
		    } break;
		    default:
			errf->print (_func_name).print (": more than one data type").pendl ();
			abortIfReached ();
		}
	    } else {
		abortIf (type_category != Unknown);
		type_category = BasicType;
		basic_type = Cpp::TypeDesc_BasicType::Char;
	    }
	}

	void setBasicType_Unsigned ()
	{
	    static char const * const _func_name = "Scruffy.CppParser.TypeSpecifierParser.setBasicType_Unsigned";

	    if (type_category != Unknown &&
		type_category != BasicType)
	    {
		errf->print (_func_name).print (": more than one data type").pendl ();
		abortIfReached ();
	    }

	    if (has_signed || has_unsigned) {
		errf->print (_func_name).print (": more than one data type").pendl ();
		abortIfReached ();
	    }

	    if (type_category == BasicType) {
		switch (basic_type) {
		    case Cpp::TypeDesc_BasicType::Char: {
			basic_type = Cpp::TypeDesc_BasicType::UnsignedChar;
		    } break;
		    case Cpp::TypeDesc_BasicType::Int: {
			basic_type = Cpp::TypeDesc_BasicType::UnsignedInt;
		    } break;
		    case Cpp::TypeDesc_BasicType::ShortInt: {
			basic_type = Cpp::TypeDesc_BasicType::UnsignedShortInt;
		    } break;
		    case Cpp::TypeDesc_BasicType::LongInt: {
			basic_type = Cpp::TypeDesc_BasicType::UnsignedLongInt;
		    } break;
		    default:
			errf->print (_func_name).print (": more than one data type").pendl ();
			abortIfReached ();
		}
	    } else {
		abortIf (type_category != Unknown);
		type_category = BasicType;
		basic_type = Cpp::TypeDesc_BasicType::UnsignedInt;
	    }

	    has_unsigned = true;
	}

	void setBasicType_Long ()
	{
	    static char const * const _func_name = "Scruffy.CppParser.TypeSpecifierParser.setBasicType_Long";

	    if (type_category != Unknown &&
		type_category != BasicType)
	    {
		errf->print (_func_name).print (": more than one data type").pendl ();
		abortIfReached ();
	    }

	    if (type_category == BasicType) {
		switch (basic_type) {
		  // TODO LongLong
		    case Cpp::TypeDesc_BasicType::Int: {
			basic_type = Cpp::TypeDesc_BasicType::LongInt;
		    } break;
		    case Cpp::TypeDesc_BasicType::UnsignedInt: {
			basic_type = Cpp::TypeDesc_BasicType::UnsignedLongInt;
		    } break;
		    case Cpp::TypeDesc_BasicType::Double: {
			basic_type = Cpp::TypeDesc_BasicType::LongDouble;
		    } break;
		    default:
			errf->print (_func_name).print (": more than one data type").pendl ();
			abortIfReached ();
		}
	    } else {
		abortIf (type_category != Unknown);
		type_category = BasicType;
		basic_type = Cpp::TypeDesc_BasicType::LongInt;
	    }
	}

	void setBasicType_Float ()
	{
	    static char const * const _func_name = "Scruffy.CppParser.TypeSpecifierParser.setBasicType_Float";

	    if (type_category != Unknown) {
		errf->print (_func_name).print (": more than one data type").pendl ();
		abortIfReached ();
	    }

	    type_category = BasicType;
	    basic_type = Cpp::TypeDesc_BasicType::Float;
	}

	void setBasicType_Double ()
	{
	    static char const * const _func_name = "Scruffy.CppParser.TypeSpecifierParser.setBasicType_Double";

	    if (type_category != Unknown &&
		type_category != BasicType)
	    {
		errf->print (_func_name).print (": more than one data type").pendl ();
		abortIfReached ();
	    }

	    if (has_int) {
		errf->print (_func_name).print (": more than one data type").pendl ();
		abortIfReached ();
	    }

	    if (type_category == BasicType) {
		switch (basic_type) {
		    case Cpp::TypeDesc_BasicType::LongInt: {
			basic_type = Cpp::TypeDesc_BasicType::LongDouble;
		    } break;
		    default:
			errf->print (_func_name).print (": more than one data type").pendl ();
			abortIfReached ();
		}
	    } else {
		abortIf (type_category != Unknown);
		type_category = BasicType;
		basic_type = Cpp::TypeDesc_BasicType::Double;
	    }
	}

	void setBasicType_Signed ()
	{
	    static char const * const _func_name = "Scruffy.CppParser.TypeSpecifierParser.setBasicType_Signed";

	    if (type_category != Unknown &&
		type_category != BasicType)
	    {
		errf->print (_func_name).print (": more than one data type").pendl ();
		abortIfReached ();
	    }

	    if (has_signed || has_unsigned) {
		errf->print (_func_name).print (": more than one data type").pendl ();
		abortIfReached ();
	    }

	    if (type_category == BasicType) {
		switch (basic_type) {
		    case Cpp::TypeDesc_BasicType::Char: {
			basic_type = Cpp::TypeDesc_BasicType::SignedChar;
		    } break;
		    case Cpp::TypeDesc_BasicType::Int: {
			basic_type = Cpp::TypeDesc_BasicType::Int;
		    } break;
		    case Cpp::TypeDesc_BasicType::ShortInt: {
			basic_type = Cpp::TypeDesc_BasicType::ShortInt;
		    } break;
		    case Cpp::TypeDesc_BasicType::LongInt: {
			basic_type = Cpp::TypeDesc_BasicType::LongInt;
		    } break;
		    default:
			errf->print (_func_name).print (": more than one data type").pendl ();
			abortIfReached ();
		}
	    } else {
		abortIf (type_category != Unknown);
		type_category = BasicType;
		basic_type = Cpp::TypeDesc_BasicType::Int;
	    }

	    has_signed = true;
	}

	void setClass (Cpp::Class * const new_class,
		       bool         const new_simple_type_specifier_used = false)
	{
	    static char const * const _func_name = "Scruffy.CppParser.TypeSpecifierParser.setClass";

	    if (gotType ()) {
		errf->print (_func_name).print (": more than one data type").pendl ();
		abortIfReached ();
	    }

	    class_ = new_class;
	    type_category = Class;

	    simple_type_specifier_used = new_simple_type_specifier_used;
	}

	void setEnum (Cpp::Enum * const new_enum)
	{
	    static char const * const _func_name = "Scruffy.CppParser.TypeSpecifierParser.setEnum";

	    if (gotType ()) {
		errf->print (_func_name).print (": more than one data type").pendl ();
		abortIfReached ();
	    }

	    enum_ = new_enum;
	    type_category = Enum;
	}

	void setTypedefName (Cpp::TypeDesc * const type_desc)
	{
	    type_category = Typedef;
	    typedef__type_desc = type_desc;
	}

	void setConst ()
	{
	    static char const * const _func_name = "Scruffy.CppParser.TypeSpecifierParser.setConst";

	    if (is_const) {
		errf->print (_func_name).print (": duplicate \"const\"").pendl ();
		abortIfReached ();
	    }

	    is_const = true;
	}

	void setVolatile ()
	{
	    static char const * const _func_name = "Scruffy.CppParser.TypeSpecifierParser.setVolatile";

	    if (is_volatile) {
		errf->print (_func_name).print (": duplicate \"volatile\"").pendl ();
		abortIfReached ();
	    }

	    is_volatile = true;
	}

	void setAuto ()
	{
	    static char const * const _func_name = "Scruffy.CppParser.TypeSpecifierParser.setAuto";

	    if (is_auto) {
		errf->print (_func_name).print (": duplicate \"auto\"").pendl ();
		abortIfReached ();
	    }

	    is_auto = true;
	}

	void setRegister ()
	{
	    static char const * const _func_name = "Scruffy.CppParser.TypeSpecifierParser.setRegister";

	    if (is_register) {
		errf->print (_func_name).print (": duplicate \"register\"").pendl ();
		abortIfReached ();
	    }

	    is_register = true;
	}

	void setStatic ()
	{
	    static char const * const _func_name = "Scruffy.CppParser.TypeSpecifierParser.setStatic";

	    if (is_static) {
		errf->print (_func_name).print (": duplicate \"static\"").pendl ();
		abortIfReached ();
	    }

	    is_static = true;
	}

	void setExtern ()
	{
	    static char const * const _func_name = "Scruffy.CppParser.TypeSpecifierParser.setExtern";

	    if (is_extern) {
		errf->print (_func_name).print (": duplicate \"extern\"").pendl ();
		abortIfReached ();
	    }

	    is_extern = true;
	}

	void setMutable ()
	{
	    static char const * const _func_name = "Scruffy.CppParser.TypeSpecifierParser.setMutable";

	    if (is_mutable) {
		errf->print (_func_name).print (": duplicate \"mutable\"").pendl ();
		abortIfReached ();
	    }

	    is_mutable = true;
	}

	void setInline ()
	{
	    static char const * const _func_name = "Scruffy.CppParser.TypeSpecifierParser.setInline";

	    if (is_inline) {
		errf->print (_func_name).print (": duplicate \"inline\"").pendl ();
		abortIfReached ();
	    }

	    is_inline = true;
	}

	void setVirtual ()
	{
	    static char const * const _func_name = "Scruffy.CppParser.TypeSpecifierParser.setVirtual";

	    if (is_virtual) {
		errf->print (_func_name).print (": duplicate \"virtual\"").pendl ();
		abortIfReached ();
	    }

	    is_virtual = true;
	}

	void setExplicit ()
	{
	    static char const * const _func_name = "Scruffy.CppParser.TypeSpecifierParser.setExplicit";

	    if (is_explicit) {
		errf->print (_func_name).print (": duplicate \"explicit\"").pendl ();
		abortIfReached ();
	    }

	    is_explicit = true;
	}

	void setFriend ()
	{
	    static char const * const _func_name = "Scruffy.CppParser.TypeSpecifierParser.setFriend";

	    if (is_friend) {
		errf->print (_func_name).print (": duplicate \"friend\"").pendl ();
		abortIfReached ();
	    }

	    is_friend = true;
	}

	void setTypedef ()
	{
	    static char const * const _func_name = "Scruffy.CppParser.TypeSpecifierParser.setTypedef";

	    if (is_typedef) {
		errf->print (_func_name).print (": duplicate \"typedef\"").pendl ();
		abortIfReached ();
	    }

	    is_typedef = true;
	}

	Ref<Cpp::TypeDesc> createTypeDesc ();

	void fillDeclarationState (DeclarationState *declaration_state /* non-null */);

	TypeSpecifierParser ()
	    : type_category (Unknown),
	      basic_type (Cpp::TypeDesc_BasicType::Void)
	{
	}
    };

    /* Declaration description.
     *
     * A declaration description contains information necessary to declare
     * a member: nested-name-specifier and member's identity.
     */
    class DeclarationDesc : public SimplyReferenced
    {
    public:
	Bool global_scope;
	List<Cpp::Container*> nested_name_atoms;

	Ref<Cpp::Member> member;

	bool isSane () const
	{
	    return !member.isNull () &&
		       (!member->type_desc.isNull () ||
			member->getType () == Cpp::Member::t_TypeTemplateParameter);
	}

	bool hasNestedNameSpecifier () const
	{
	    return global_scope || !nested_name_atoms.isEmpty ();
	}
    };

    class DeclaratorState : public SimplyReferenced
    {
    public:
	Ref<Cpp::TypeDesc> type_desc;
	Ref<Cpp::TypeDesc> *cur_type;

	Ref<Cpp::Name> name;

	List< Ref<Cpp::TypeDesc::TypeAtom> > type_atoms;
	Bool is_reference;

	DeclaratorState ()
	    : cur_type (&type_desc)
	{
	}
    };

    // TODO This belongs to DeclarationState. There's no real reason for
    // FunctionDeclaratorState to be a separate unit of the state.
    // Note that its lifetime is bound to the lifetime of DeclarationState
    // with a bound cancellable, which is a good indicator.
    class FunctionDeclaratorState
    {
    public:
	List< Ref<Cpp::Member> > function_parameters;
	Ref<DeclaratorState> declarator_state;
    };

    class DeclarationState
    {
    public:
	class Cancellable_DeclaratorId : public CheckpointTracker::Cancellable
	{
	private:
	    DeclarationState &declaration_state;

	public:
	    void cancel ()
	    {
		declaration_state.declarator_has_declarator_id = false;
		declaration_state.global_scope = false;
		declaration_state.nested_name_atoms.clear ();
	    }

	    Cancellable_DeclaratorId (DeclarationState &declaration_state)
		: declaration_state (declaration_state)
	    {
	    }
	};

	TypeSpecifierParser type_specifier_parser;

	List<FunctionDeclaratorState> function_declarator_states;

	Bool got_declarator;

#if 0
	Bool is_const,
	     is_volatile;
#endif

	Bool is_auto,
	     is_register,
	     is_static,
	     is_extern,
	     is_mutable;

	Bool is_inline,
	     is_virtual,
	     is_explicit;

	Bool is_friend,
	     is_typedef;

	Ref<Cpp::TypeDesc> type_desc;

	List< Ref<Cpp::MemInitializer> > mem_initializers;

	List< Ref<Cpp::Class::ParentEntry> > class_parents;
	// TODO We could safeuly keep Class in TypeDesc_Data->DataType_Class
	//      This would also make cpp_accept_class_definition() unnecessary.
	// Should not be reset
	Ref<Cpp::Class> class_;
	// Should not be reset
	Ref<Cpp::Enum> enum_;

	Ref<Cpp::Member> member;

	Ref< Acceptor<DeclarationDesc> > declaration_acceptor;

	// TODO FIXME This field is unused (!)
	Bool is_parameter;

	CheckpointTracker::CheckpointKey checkpoint_key;

	Size num_declarators;

      // Declarator state

	Bool declarator_has_declarator_id;

	Bool global_scope;
	List<Cpp::Container*> nested_name_atoms;

	DeclarationState ()
	    : num_declarators (0)
	{
	}

#if 0
	// TODO ?
	void reset ()
	{
	    is_const = false;
	    is_volatile = false;

	    is_auto = false;
	    is_register = false;
	    is_static = false;
	    is_extern = false;
	    is_mutable = false;

	    is_inline = false;
	    is_virtual = false;
	    is_explicit = false;

	    is_friend = false;
	    is_typedef = false;

	    type_desc = NULL;

	    is_parameter = false;
	}
#endif
    };

}

class CppParser_Impl : public Pargen::LookupData,
		       public virtual SimplyReferenced
{
//private:
public:
    enum ClassParsingPhase
    {
	ClassParsingPhase_None = 0,
	ClassParsingPhase_Prematch,
	ClassParsingPhase_Rematch
    };

    class InitializerState
    {
    public:
	Ref<Cpp::Initializer> initializer;
	CheckpointTracker::CheckpointKey checkpoint_key;
    };

    // TODO Rename this back to IdExpressionState
    class IdState
    {
    public:
	Bool global_scope;
	List<Cpp::Container*> nested_name_atoms;

	// TODO template-id should not be in IdState anymore.
	// We create a map of valid template-ids instead and
	// put pointers to the entries in that map in template-id's user_ptr.
	Bool is_template_id;

	ConstMemoryDesc template_name;
	List< Ref<Cpp::TemplateArgument> > template_arguments;
	CheckpointTracker::CheckpointKey checkpoint_key;

	class Cancellable_NestedName : public CheckpointTracker::Cancellable
	{
	private:
	    IdState &id_state;

	public:
	    void cancel ()
	    {
		id_state.global_scope = false;
		id_state.nested_name_atoms.clear ();
	    }

	    Cancellable_NestedName (IdState &id_state)
		: id_state (id_state)
	    {
	    }
	};
    };

    class TemplateIdState
    {
    public:
	List< Ref<Cpp::TemplateArgument> > template_arguments;
	CheckpointTracker::CheckpointKey checkpoint_key;
    };

    class NestedNameSpecifierState
    {
    public:
	Bool global_scope;
	List<Cpp::Container*> atoms;
    };

    class TemplateDeclarationState
    {
    public:
	List< Ref<Cpp::Member> > template_parameters;
	CheckpointTracker::CancellableKey acceptor_cancellable_key;

//	CheckpointTracker::CheckpointKey checkpoint_key;
    };

    class ConstructorAmbiguityState
    {
    public:
	Bool is_dummy;

	Bool constructor_attempt;
	Ref<Pargen::ParserPositionMarker> parser_position;

	CheckpointTracker::CancellableKey cancellable_key;
    };

    class TypeidState
    {
    public:
	Ref<Cpp::TypeDesc> type_desc;
    };

    class ExpressionEntry
    {
    public:
	Ref<Cpp::Expression> expression;
	CheckpointTracker::CancellableKey cancellable_key;
    };

    class ExpressionState
    {
    public:
	List<ExpressionEntry> expressions;
	CheckpointTracker::CheckpointKey checkpoint_key;
    };

    class StatementState
    {
    public:
	List< Ref<Cpp::Statement> > statements;
	CheckpointTracker::CheckpointKey checkpoint_key;
    };

    class ConditionDesc : public SimplyReferenced
    {
    public:
    };

    class ConditionState
    {
    public:
	// Declared member, if any.
	Ref<Cpp::Member> member;
	// Condition's expression (initialization expression, if a member is declared).
	Ref<Cpp::Expression> expression;

	CheckpointTracker::CheckpointKey checkpoint_key;
    };

    class ForInitStatementState
    {
    public:
	Ref<Cpp::Member> member;
	Ref<Cpp::Expression> expression;

	CheckpointTracker::CheckpointKey checkpoint_key;
    };

    class DeclarationStatementState
    {
    public:
	Ref<DeclarationDesc> declaration_desc;
    };

    class TemplateInstanceEntry
    {
    public:
	Cpp::TemplateInstance *template_instance;
    };

    Ref<String> default_variant;

    CheckpointTracker checkpoint_tracker;
    Size checkpoint_counter;

    Ref<Cpp::NamespaceEntry> root_namespace_entry;
    List<Cpp::Container*> containers;

    List< Ref<Cpp::Namespace> > temporal_namespaces;

    List< Ref< Acceptor<DeclarationDesc> > > declaration_acceptors;

  // Class body pre-matching

    ClassParsingPhase class_phase;
//    Size class_phase_depth;
    Size class_checkpoint;
    Ref<Pargen::ParserPositionMarker> class_pmark;

  // (End class body pre-matching)

  // Parsing simple declarations

// Deprecated    Ref<TemplateDeclarationDesc> template_declaration_desc;
// Deprecated    CheckpointTracker::CancellableKey template_declaration_cancellable_key;

    List<InitializerState>          initializer_states;
    List<ExpressionState>           expression_states;
//    List<IdExpressionState>         id_expression_states;
    List<IdState>                   id_states;
    List<TemplateIdState>           template_id_states;
    List<NestedNameSpecifierState>  nested_name_specifier_states;
    List<ConstructorAmbiguityState> constructor_ambiguity_states;
    List<DeclarationState>          declaration_states;
    List<StatementState>            statement_states;
    List<ConditionState>            condition_states;
    List<ForInitStatementState>     for_init_statement_states;
    List<DeclarationStatementState> declaration_statement_states;
    List<TypeidState>               typeid_states;
    List<TemplateDeclarationState>  template_declaration_states;

  // (End parsing simple declarations)

    Cpp::Container* getContainerForNestedName (bool *ret_fixed_namespace);

    void setDeclarationTypeDesc (DeclarationState &declaration_state,
				 Cpp::TypeDesc *type_desc);

    Ref<Cpp::Member> addClass (DeclarationState &declaration_state,
			       Cpp::Container * const container,
			       ConstMemoryDesc const &identifier_str);

    Cpp::Namespace* lookupNamespace (ConstMemoryDesc const &identifier_str,
				     bool global_scope);

    Cpp::Member* lookupMember (bool                         global_scope,
			       List<Cpp::Container*> const &nested_name_atoms,
			       Cpp::Name             const &name);

    Cpp::Member* lookupType (ConstMemoryDesc const &identifier_str);

    Cpp::Container* getFixedNameContainer (DeclarationDesc const &declaration_desc);

    Cpp::Member* getPreviousObjectDeclaration (DeclarationDesc const &declaration_desc__object);

    Ref<Cpp::TypeDesc> getPreviousTypeDeclaration (DeclarationDesc const &declaration_desc__type);

    void grabTemplateDeclaration ();

    void beginNamespace (ConstMemoryDesc const &namespace_name);

    void beginTemporalNamespace ();

// TODO    void beginDeclarations ();

    void beginDeclSpecifierSeq ();

    void beginInitializer ();

    void acceptInitializer (Cpp::Initializer *initializer);

    void acceptDeclaration (Cpp::Container                         &container,
			    DeclarationDesc                  const &declaration_desc,
			    CheckpointTracker::CheckpointKey const &checkpoint_key);

    void acceptStatement (StatementState &target_state,
			  Cpp::Statement *statement);

public:
  // interface LookupData

    void newCheckpoint ();

    void commitCheckpoint ();

    void cancelCheckpoint ();

  // (end interface LookupData)

    CppParser_Impl (ConstMemoryDesc const &default_variant);
};

namespace {

    template <class T>
    class Cancellable_Accept : public CheckpointTracker::Cancellable
    {
    private:
	Ref< Scruffy::Acceptor<T> > const acceptor;

    public:
	void cancel ()
	{
	    DEBUG (
		errf->print ("Scruffy.CppParser_Impl.Cancellable_Acceptor.cancel").pendl ();
	    )

	    if (!acceptor.isNull ())
		acceptor->cancel ();
	}

	Cancellable_Accept (Scruffy::Acceptor<T> * const acceptor)
	    : acceptor (acceptor)
	{
	}
    };

    class Cancellable_ClassParsingPhase : public CheckpointTracker::Cancellable
    {
    private:
	CppParser_Impl * const cpp_parser;
	// TODO This is a hack. Allow to store pointers to ParserControl objects.
	Pargen::ParserControl * const parser_control;

    public:
	void cancel ()
	{
	    parser_control->setVariant (cpp_parser->default_variant->getMemoryDesc ());

	    cpp_parser->class_phase = CppParser_Impl::ClassParsingPhase_None;
	    cpp_parser->class_pmark = NULL;
	}

	Cancellable_ClassParsingPhase (CppParser_Impl * const cpp_parser,
				       ParserControl  * const parser_control)
	    : cpp_parser (cpp_parser),
	      parser_control (parser_control)
	{
	}
    };

}

void
CppParser_Impl::newCheckpoint ()
{
    checkpoint_counter ++;

    checkpoint_tracker.newCheckpoint ();
}

void
CppParser_Impl::commitCheckpoint ()
{
    abortIf (checkpoint_counter <= 0);

    checkpoint_tracker.commitCheckpoint ();

    if (class_phase != ClassParsingPhase_None) {
	if (class_checkpoint == checkpoint_counter) {
	  // There was no match for class-body.

	    DEBUG (
		errf->print ("Scruffy.CppParser_Impl.commitCheckpoint: resetting phase").pendl ();
	    )
	    class_phase = ClassParsingPhase_None;
	    class_pmark = NULL;
	}
    }

    checkpoint_counter --;
}

void
CppParser_Impl::cancelCheckpoint ()
{
    abortIf (checkpoint_counter <= 0);

    checkpoint_tracker.cancelCheckpoint ();

    if (class_phase != ClassParsingPhase_None) {
	if (class_checkpoint == checkpoint_counter) {
	  // There was no match for class-body.

	    DEBUG (
		errf->print ("Scruffy.CppParser_Impl.cancelCheckpoint: resetting phase").pendl ();
	    )
	    class_phase = ClassParsingPhase_None;
	    class_pmark = NULL;
	}
    }

    checkpoint_counter --;
}

Cpp::Container*
CppParser_Impl::getContainerForNestedName (bool * const ret_fixed_namespace)
{
  FUNC_NAME (
    static char const * const _func_name = "Scruffy.CppParser_Impl.getContainerForNestedName";
  )

    if (ret_fixed_namespace != NULL)
	*ret_fixed_namespace = false;

    abortIf (containers.isEmpty ());
    Cpp::Container *container = containers.last->data;

    bool fixed_namespace = false;
    if (!nested_name_specifier_states.isEmpty ()) {
	DEBUG (
	    errf->print (_func_name).print (": nested-name-specifier present").pendl ();
	)

	NestedNameSpecifierState &nested_name_specifier_state =
		nested_name_specifier_states.last->data;
	if (!nested_name_specifier_state.atoms.isEmpty ()) {
	    fixed_namespace = true;
	    DEBUG (
		errf->print (_func_name).print (": last atom").pendl ();
	    )
	    container = nested_name_specifier_state.atoms.last->data;
	} else {
	    if (nested_name_specifier_state.global_scope) {
		fixed_namespace = true;
		DEBUG (
		    errf->print (_func_name).print (": global scope").pendl ();
		)
		container = containers.first->data;
	    }
	}
    }

    if (ret_fixed_namespace != NULL)
	*ret_fixed_namespace = fixed_namespace;

    return container;
}

void
CppParser_Impl::setDeclarationTypeDesc (DeclarationState &declaration_state,
					Cpp::TypeDesc * const type_desc)
{
    static char const * const _func_name = "Scruffy.CppParser_Impl.setDeclarationTypeDesc";

// TODO 3.cpp fails here
    if (!declaration_state.type_desc.isNull ()) {
      // Only one type at a time for a declaration is allowed (obviously).

	errf->print (Pr << _func_name << ": extra type").pendl ();
	abortIfReached ();
    }

    // The class is now the declaration's type.
    declaration_state.type_desc = type_desc;
    {
      // Setting up a cancellable.
	Ref< Cancellable_Ref<Cpp::TypeDesc> > cancellable =
		grab (new Cancellable_Ref<Cpp::TypeDesc> (declaration_state.type_desc));
	checkpoint_tracker.addBoundCancellable (cancellable, declaration_state.checkpoint_key);
    }
}

Ref<Cpp::Member>
CppParser_Impl::addClass (DeclarationState         &declaration_state,
			  Cpp::Container   * const  container,
			  ConstMemoryDesc    const &identifier_str)
{
  FUNC_NAME (
    static char const * const _func_name = "Scruffy.CppParser.addClass";
  )

    DEBUG (
	errf->print (_func_name).print (": class ").print (identifier_str).pendl ();
    )

    abortIf (container == NULL);

    // We create the Class object early to obtain a place to store template parameters.
    Ref<Cpp::Class> class_ = grab (new Cpp::Class);

    class_->parent_container = container;
    DEBUG (
	errf->print (_func_name).print (": parent container: ");
    )
    if (container != NULL) {
	DEBUG (
	    Cpp::dumpContainerInfo (*container);
	)
    } else {
	DEBUG (
	    errf->print ("(null)").pendl ();
	)
    }

    Ref<Cpp::TypeDesc_Class> type_desc__class = grab (new Cpp::TypeDesc_Class);
    type_desc__class->class_ = class_;

    if (!temporal_namespaces.isEmpty ()) {
	// TODO temporal namespaces should be represented as containers.
	abortIfReached ();
    }

    Ref<Cpp::Member> member = grab (new Cpp::Member_Type);
    member->type_desc = type_desc__class;
    member->name = grab (new Cpp::Name);
    member->name->name_str = grab (new String (identifier_str));

    class_->name = member->name;

    {
      // Calling declaration acceptor.

	Ref<DeclarationDesc> declaration_desc = grab (new DeclarationDesc);
	declaration_desc->member = member;

	abortIf (declaration_state.declaration_acceptor.isNull ());
	declaration_state.declaration_acceptor->accept (declaration_desc);

    }

    {
      // Setting 'declaration_state.member'
	abortIf (!declaration_state.member.isNull ());
	declaration_state.member = member;
	checkpoint_tracker.addBoundCancellable (
		grab (new Cancellable_Ref<Cpp::Member> (declaration_state.member)),
		declaration_state.checkpoint_key);
    }

    return member;
}

Cpp::Namespace*
CppParser_Impl::lookupNamespace (ConstMemoryDesc const &identifier_str,
				 // TODO This parameter is excessive:
				 // nested_name_specifier_state.global_scope could be used instead.
				 bool global_scope)
{
  FUNC_NAME (
    static const char * const _func_name = "Scruffy.CppParser_Impl.lookupNamespace";
  )

    DEBUG (
	errf->print (_func_name).print (": identifier: ").print (identifier_str).pendl ();
    )

    abortIf (containers.isEmpty ());
    Cpp::Container *cur_container = containers.last->data;
    for (;;) {
	if (cur_container->getType () != Cpp::Container::t_Namespace) {
	    DEBUG (
		errf->print (_func_name).print (": not a namespace").pendl ();
	    )
	    // TEST

	    cur_container = cur_container->parent_container;
	    if (cur_container == NULL)
		return NULL;

    //	abortIfReached ();
	} else
	    break;
    }

    Cpp::Namespace *cur_namespace =
	    static_cast <Cpp::Namespace*> (cur_container);

    Cpp::Namespace *namespace_ = NULL;

    abortIf (nested_name_specifier_states.isEmpty ());

    bool fixed_namespace = false;
    CppParser_Impl::NestedNameSpecifierState &nested_name_specifier_state =
	    nested_name_specifier_states.last->data;
    if (!nested_name_specifier_state.atoms.isEmpty ()) {
	fixed_namespace = true;

	Cpp::Container *atom = nested_name_specifier_state.atoms.last->data;
	switch (atom->getType ()) {
	    case Cpp::Container::t_Namespace: {
		Cpp::Namespace * const atom_namespace =
			static_cast <Cpp::Namespace*> (atom);

		namespace_ = atom_namespace;

//		errf->print ("--- looking in namespace ").print (namespace_->name).pendl ();
	    } break;
	    case Cpp::Container::t_Class: {
		DEBUG (
		    errf->print ("--- WARNING: looking up a namespace in a class").pendl ();
		)
		return NULL;
	    } break;
	    default:
		abortIfReached ();
	}
    } else {
	DEBUG (
	    errf->print ("--- no atoms").pendl ();
	)

	if (global_scope) {
	    // TODO Anonymous namespaces?
	    namespace_ = root_namespace_entry->namespace_;
	} else
	    namespace_ = cur_namespace;
    }

    while (namespace_ != NULL) {
	Cpp::NamespaceEntry::NamespaceEntryMap::Entry entry = namespace_->namespace_entries.lookup (identifier_str);
	if (!entry.isNull ()) {
	    DEBUG (
		errf->print ("Scruffy.CppParser_Impl.lookupNamespace: namespace found").pendl ();
	    )
	    return entry.getData ()->namespace_;
	}

	if (fixed_namespace) {
	    DEBUG (
		errf->print ("Scruffy.CppParser_Impl.lookupNamespace: fixed, namespace not found").pendl ();
	    )
	    break;
	}

	if (namespace_->parent_container != NULL) {
	    abortIf (namespace_->parent_container->getType () != Cpp::Container::t_Namespace);
	    namespace_ = static_cast <Cpp::Namespace*> (namespace_->parent_container);
	} else
	    namespace_ = NULL;
    }

    DEBUG (
	errf->print ("Scruffy.CppParser_Impl.lookupNamespace: namespace not found").pendl ();
    )
    return NULL;
}

void
CppParser_Impl::beginInitializer ()
{
    CppParser_Impl::InitializerState &initializer_state = initializer_states.appendEmpty ()->data;
    initializer_state.checkpoint_key = checkpoint_tracker.getCurrentCheckpoint ();
}

void
CppParser_Impl::acceptInitializer (Cpp::Initializer * const initializer)
{
    abortIf (initializer_states.isEmpty ());
    CppParser_Impl::InitializerState &initializer_state = initializer_states.last->data;

    abortIf (!initializer_state.initializer.isNull ());
    initializer_state.initializer = initializer;
    checkpoint_tracker.addBoundCancellable (
	    grab (new Cancellable_Ref<Cpp::Initializer> (initializer_state.initializer)),
	    initializer_state.checkpoint_key);
}

Cpp::Member*
CppParser_Impl::lookupMember (bool                  const  global_scope,
			      List<Cpp::Container*> const &nested_name_atoms,
			      Cpp::Name             const &name)
{
  FUNC_NAME (
    static char const * const _func_name = "Scruffy.CppParser.lookupMember";
  )

    Cpp::Container *cur_container = containers.last->data;

    bool fixed_namespace = false;
    if (!nested_name_atoms.isEmpty ()) {
	cur_container = nested_name_atoms.last->data;
	fixed_namespace = true;
    } else {
	if (global_scope) {
	    cur_container = containers.first->data;
	    fixed_namespace = true;
	}
    }

    if (!fixed_namespace) {
	List< Ref<Cpp::Namespace> >::Element *cur_el = temporal_namespaces.last;
	while (cur_el != NULL) {
	    Ref<Cpp::Namespace> &namespace_ = cur_el->data;

	    Cpp::Member::MemberMap::Entry entry = namespace_->members.lookup (name);
	    if (!entry.isNull ())
		return entry.getData ();

	    cur_el = cur_el->previous;
	}
    }

    while (cur_container != NULL) {
	DEBUG (
	    errf->print (_func_name).print (": looking up in ");
	    Cpp::dumpContainerInfo (*cur_container);
	)

	switch (cur_container->getType ()) {
	    case Cpp::Container::t_Namespace: {
		Cpp::Namespace const * const namespace_ =
			static_cast <Cpp::Namespace const *> (cur_container);

		Cpp::Member::MemberMap::Entry map_entry = namespace_->members.lookup (name);
		if (!map_entry.isNull ()) {
		    DEBUG (
			errf->print (_func_name).print (": FOUND, returning 0x").printHex ((Uint64) map_entry.getData ().ptr ()).pendl ();
		    )
		    return map_entry.getData ();
		}
	    } break;
	    case Cpp::Container::t_Class: {
		Cpp::Class const * const class_ =
			static_cast <Cpp::Class const *> (cur_container);

		Cpp::Member::MemberMap::Entry map_entry = class_->members.lookup (name);
		if (!map_entry.isNull ()) {
		    DEBUG (
			errf->print (_func_name).print (": FOUND, returning 0x").printHex ((Uint64) map_entry.getData ().ptr ()).pendl ();
		    )
		    return map_entry.getData ();
		}

		// TODO Search in base classes
	    } break;
	    default:
		abortIfReached ();
	}

	if (fixed_namespace) {
	    DEBUG (
		errf->print (_func_name).print (": fixed namespace, breaking").pendl ();
	    )
	    break;
	}

	cur_container = cur_container->parent_container;
    }

    return NULL;
}

Cpp::Member*
CppParser_Impl::lookupType (ConstMemoryDesc const &identifier_str)
{
  // TODO Lookup according to real lookup rules.

  FUNC_NAME (
    static const char * const _func_name = "Scruffy.CppParser_Impl.lookupType";
  )

    DEBUG (
	errf->print (_func_name).print (": ").print (identifier_str).pendl ();
    )

    bool fixed_namespace = false;

    if (!fixed_namespace) {
	DEBUG (
	    errf->print (_func_name).print (": no fixed namespace, searching for temporals").pendl ();
	)

	List< Ref<Cpp::Namespace> >::Element *cur_el = temporal_namespaces.last;
	while (cur_el != NULL) {
	    List< Ref<Cpp::Namespace> >::Element *prv_el = cur_el->previous;

	    Ref<Cpp::Namespace> &namespace_ = cur_el->data;

	    // TODO Make this in a more efficient way (additional method in a comparator?)
	    Cpp::Name tmp_name;
	    tmp_name.name_str = grab (new String (identifier_str));

	    Cpp::Member::MemberMap::Entry type_map_entry = namespace_->members.lookup (tmp_name);
	    if (!type_map_entry.isNull ()) {
		DEBUG (
		    errf->print (_func_name).print (": FOUND in a temporal namespace").pendl ();
		)
		return type_map_entry.getData ();
	    }

	    cur_el = prv_el;
	}
    }

    Cpp::Container *cur_container = getContainerForNestedName (&fixed_namespace);
    while (cur_container != NULL) {
	DEBUG (
	    errf->print (_func_name).print (": looking up in ");
	    Cpp::dumpContainerInfo (*cur_container);
	)

	switch (cur_container->getType ()) {
	    case Cpp::Container::t_Namespace: {
		Cpp::Namespace const * const namespace_ =
			static_cast <Cpp::Namespace const *> (cur_container);

		// TODO Make this in a more efficient way
		Cpp::Name tmp_name;
		tmp_name.name_str = grab (new String (identifier_str));

		Cpp::Member::MemberMap::Entry type_map_entry = namespace_->members.lookup (tmp_name);

		if (!type_map_entry.isNull ()) {
		    DEBUG (
			errf->print (_func_name).print (": FOUND, returning 0x").printHex ((Uint64) type_map_entry.getData ().ptr ()).pendl ();
		    )
		    return type_map_entry.getData ();
		}
	    } break;
	    case Cpp::Container::t_Class: {
		Cpp::Class const * const class_ =
			static_cast <Cpp::Class const *> (cur_container);

		// TODO Make this in a more efficient way
		Cpp::Name tmp_name;
		tmp_name.name_str = grab (new String (identifier_str));

		Cpp::Member::MemberMap::Entry type_map_entry = class_->members.lookup (tmp_name);

		if (!type_map_entry.isNull ()) {
		    DEBUG (
			errf->print (_func_name).print (": FOUND, returning 0x").printHex ((Uint64) type_map_entry.getData ().ptr ()).pendl ();
		    )
		    return type_map_entry.getData ();
		}

		// TODO Search in base classes
	    } break;
	    default:
		abortIfReached ();
	}

	if (fixed_namespace) {
	    DEBUG (
		errf->print (_func_name).print (": fixed namespace, breaking").pendl ();
	    )
	    break;
	}

	cur_container = cur_container->parent_container;
    }

    DEBUG (
	errf->print (_func_name).print (": not found").pendl ();
    )

    return NULL;
}

// TODO Iterator<Cpp::Member*> (pointers!)
static Cpp::Member*
getMatchingObjectDeclaration (StatefulIterator_anybase<Cpp::Member&> &obj_iter,
			      Cpp::TypeDesc const & /* type_desc */)
{
    DEBUG (
	errf->print ("--- GET MATCHING OBJECT ---").pendl ();
    )

    while (!obj_iter.done ()) {
	Cpp::Member &object_entry = obj_iter.next ();

	// TODO Compare types
	DEBUG (
	    errf->print ("--- DECL MATCH ---").pendl ();
	)
	return &object_entry;
    }

    return NULL;
}

static Cpp::Member*
lookupObjectInContainer (Cpp::Member    const &member,
			 Cpp::Container const &container)
{
    DEBUG (
	errf->print ("--- CONTAINER OBJECT LOOKUP --- ").print (*member.name->toString ()).pendl ();
    )

    StatefulExtractorIterator< Cpp::Member&,
			       DereferenceExtractor<Cpp::Member>,
			       Cpp::Member::MemberMap::SameKeyDataIterator >
	    iter = Cpp::Member::MemberMap::SameKeyDataIterator (
			   container.members.lookup (*member.name));

    // TODO Fix this (there should always be a type)
    if (member.type_desc.isNull ())
	return NULL;

    return getMatchingObjectDeclaration (iter, *member.type_desc);
}

static Ref<Cpp::TypeDesc>
lookupTypeInContainer (Cpp::Member    const & /* member */,
		       Cpp::Container const & /* container */)
{
    // TODO

    return NULL;
}

Cpp::Container*
CppParser_Impl::getFixedNameContainer (DeclarationDesc const &declaration_desc)
{
    if (declaration_desc.nested_name_atoms.isEmpty ()) {
	if (declaration_desc.global_scope)
	    return root_namespace_entry->namespace_;

	return NULL;
    }

    return declaration_desc.nested_name_atoms.last->data;
}

Cpp::Member*
CppParser_Impl::getPreviousObjectDeclaration (DeclarationDesc const &declaration_desc__object)
{
    {
	Cpp::Container* name_container = getFixedNameContainer (declaration_desc__object);

	if (name_container != NULL) {
	  // DEBUG

	    if (name_container->getType () == Cpp::Container::t_Namespace) {
		DEBUG (
		    errf->print ("--- NAMESPACE").pendl ();
		)
	    } else {
		DEBUG (
		    errf->print ("--- CLASS").pendl ();
		)
	    }

	  // (DEBUG)

	    DEBUG (
		errf->print ("--- FIXED CONTAINER ---").pendl ();
	    )
	    return lookupObjectInContainer (*declaration_desc__object.member,
					    *name_container);
	} else {
	    DEBUG (
		errf->print ("--- NO FIXED CONTAINER ---").pendl ();
	    )
	}
    }

#if 0
// Deprecated: There's no need to iterate! Only current container matters.

    {
	List<Cpp::Container*>::DataIterator iter (containers);
	while (!iter.done ()) {
	    Cpp::Container* &name_container = iter.next ();
	    abortIf (name_container == NULL);

	    // TODO For classes, stop searching after hitting a namespace.

	    Cpp::Member *obj = lookupObjectInContainer (*declaration_desc__object.member,
							*name_container);
	    if (obj != NULL)
		return obj;
	}
    }
#endif

    abortIf (containers.isEmpty ());
    return lookupObjectInContainer (*declaration_desc__object.member, *containers.last->data);

// Deprecated:    return NULL;
}

Ref<Cpp::TypeDesc>
CppParser_Impl::getPreviousTypeDeclaration (DeclarationDesc const &declaration_desc__type)
{
    {
	Cpp::Container *name_container = getFixedNameContainer (declaration_desc__type);
	if (name_container != NULL)
	    return lookupTypeInContainer (*declaration_desc__type.member,
					  *name_container);
    }

    {
	List<Cpp::Container*>::DataIterator iter (containers);
	while (!iter.done ()) {
	    Cpp::Container* &name_container = iter.next ();
	    abortIf (name_container == NULL);

	    Ref<Cpp::TypeDesc> type = lookupTypeInContainer (*declaration_desc__type.member,
							     *name_container);
	    if (!type.isNull ())
		return type;
	}
    }

    return NULL;
}

#if 0
// FIXME Transition (fix last)
static Ref<Cpp::Member>
createObjectForDeclaration (Cpp::Member const &member)
{
    abortIf (declaration_entry.type_desc.isNull () ||
	     declaration_entry.name.isNull ());

    Ref<Cpp::Object> object;
    switch (member.type_desc->getType ()) {
	case Cpp::TypeDesc::t_BasicType:
	case Cpp::TypeDesc::t_Class:
	case Cpp::TypeDesc::t_Enum: {
	    Ref<Cpp::Object_Data> object__data =
		    grab (new Cpp::Object_Data);
	    object__data->type_desc = declaration_entry.type_desc;
	    object__data->name = declaration_entry.name;

	    object = object__data;
	} break;
	case Cpp::TypeDesc::t_Function: {
	    Ref<Cpp::Object_Function> object__function =
		    grab (new Cpp::Object_Function);
	    object__function->type_desc = declaration_entry.type_desc;
	    object__function->name = declaration_entry.name;

	  // Note: FunctionDefinition will be created later on,
	  // in case we're parsing a function-definition.

	    object = object__function;
	} break;
	default:
	    abortIfReached ();
    }
    abortIf (object.isNull ());

    return object;
}
#endif

void
CppParser_Impl::acceptDeclaration (Cpp::Container                         &container,
				   DeclarationDesc                  const &declaration_desc,
				   CheckpointTracker::CheckpointKey const &checkpoint_key)
{
    abortIf (!declaration_desc.isSane ());

    abortIf (declaration_desc.member->name.isNull ());

    Ref<Cpp::Member> member;

  // TODO Handle symbol redefinitions
  //      ISO C++ 3.3.4

    if (declaration_desc.member->getType () == Cpp::Member::t_Type) {
	// TODO getPreviousTypeDeclaration ()
	//      Types may be redefined only if declarations are equivalent.

	Cpp::Member::MemberMap::Entry member_map_entry =
		container.members.add (declaration_desc.member);
	checkpoint_tracker.addBoundCancellable (
		MyCpp::grab (new Cancellable_MapEntry< Cpp::Member::MemberMap > (
				     container.members,
				     member_map_entry)),
		checkpoint_key);
    } else {
      // Not a type

	member = getPreviousObjectDeclaration (declaration_desc);
	if (member.isNull ()) {
	    if (declaration_desc.global_scope ||
		!declaration_desc.nested_name_atoms.isEmpty ())
	    {
		errf->print ("not a member of the container").pendl ();
		abortIfReached ();
	    }

	    member = declaration_desc.member;

	    Cpp::Member::MemberMap::Entry member_map_entry =
		    container.members.add (member);
	    checkpoint_tracker.addBoundCancellable (
		    MyCpp::grab (new Cancellable_MapEntry< Cpp::Member::MemberMap > (
					 container.members,
					 member_map_entry)),
		    checkpoint_key);
	}
    }

#if 0
// Deprecated

  // Setting declaration_state.member

    abortIf (declaration_states.isEmpty ());
    CppParser_Impl::DeclarationState &declaration_state =
	    declaration_states.last->data;

    abortIf (!declaration_state.member.isNull ());
    declaration_state.member = member;
    checkpoint_tracker.addBoundCancellable (
	    grab (new Cancellable_Ref<Cpp::Member> (declaration_state.member)),
	    declaration_state.checkpoint_key);
#endif
}

namespace {

    class Acceptor_ContainerMember : public Scruffy::Acceptor<DeclarationDesc>
    {
    private:
	CppParser_Impl &self;
	Cpp::Container &container;
	CheckpointTracker::CheckpointKey checkpoint_key;

    public:
	void accept (DeclarationDesc * const declaration_desc)
	{
	    abortIf (declaration_desc == NULL);

	    self.acceptDeclaration (container, *declaration_desc, checkpoint_key);
	}

	void cancel ()
	{
	    abortIfReached ();
	}

	Acceptor_ContainerMember (CppParser_Impl &self,
				  Cpp::Container &container,
				  CheckpointTracker::CheckpointKey checkpoint_key)
	    : self (self),
	      container (container),
	      checkpoint_key (checkpoint_key)
	{
	}
    };

}

#if 0
// Deprecated
namespace {
class Cancellable_TemplateDeclarationGrab : public CheckpointTracker::Cancellable
{
private:
    CppParser_Impl *self;

    Ref<CppParser_Impl::TemplateDeclarationDesc> template_declaration_desc;

public:
    void cancel ()
    {
      FUNC_NAME (
	static char const * const _func_name = "Scruffy.CppParser.Cancellable_TemplateDeclarationGrab.cancel";
      )

	DEBUG (
	    errf->print (_func_name).pendl ();
	)

	abortIf (!self->template_declaration_desc.isNull ());

	self->template_declaration_desc = template_declaration_desc;
    }

    Cancellable_TemplateDeclarationGrab (CppParser_Impl * const self,
					 CppParser_Impl::TemplateDeclarationDesc * const template_declaration_desc)
	: self (self),
	  template_declaration_desc (template_declaration_desc)
    {
    }
};
}
#endif

#if 0
void
CppParser_Impl::grabTemplateDeclarationDesc ()
{
    abortIf (template_declaration_desc.isNull ());

    {
	Ref<Cancellable_TemplateDeclarationGrab> cancellable =
		grab (new Cancellable_TemplateDeclarationGrab (
			      this,
			      template_declaration_desc));
	checkpoint_tracker.addBoundCancellable (cancellable, template_declaration_desc->checkpoint_key);
    }

    this->template_declaration_desc = NULL;
}
#endif

void
CppParser_Impl::grabTemplateDeclaration ()
{
    abortIf (template_declaration_states.isEmpty ());

#if 0
    {
	Ref<Cancellable_TemplateDeclarationGrab> cancellable =
		grab (new Cancellable_TemplateDeclarationGrab (
			      this,
			      template_declaration_states.last->data));
    }
#endif
    // DONE Implement conditional jumps in pargen.
    //      Cancelling template declaration state grabs will become
    //      unnecessary then.

    // Note: This function is completely unnecessary.

//    template_declaration_states.remove (template_declaration_states.last);
}

void
CppParser_Impl::beginNamespace (ConstMemoryDesc const &namespace_name)
{
    static const char * const _func_name = "Scruffy.CppParser_Impl.beginNamespace";

    DEBUG (
	errf->print (_func_name).pendl ();
    )

    if (!temporal_namespaces.isEmpty ()) {
	errf->print (_func_name).print(": in temporal namespace").pendl ();
	abortIfReached ();
    }

    abortIf (containers.isEmpty ());
    Cpp::Container *cur_container = containers.last->data;
    if (cur_container->getType () != Cpp::Container::t_Namespace) {
	errf->print (_func_name).print (": not a namespace").pendl ();
	abortIfReached ();
    }

    Ref<Cpp::Namespace> namespace_;

    bool got_namespace = false;
    {
	Cpp::NamespaceEntry::NamespaceEntryMap::Entry entry =
		cur_container->namespace_entries.lookup (namespace_name);
	if (!entry.isNull ()) {
	    namespace_ = entry.getData ()->namespace_;
	    got_namespace = true;
	}
    }

    if (!got_namespace) {
	DEBUG (
	    errf->print (Pr << _func_name << ": creating namespace \"" << namespace_name << "\"").pendl ();
	)

	Ref<Cpp::Namespace> new_namespace = grab (new Cpp::Namespace);
	new_namespace->parent_container = cur_container;
	new_namespace->primary_name = grab (new String (namespace_name));

	Ref<Cpp::NamespaceEntry> namespace_entry = grab (new Cpp::NamespaceEntry);
	namespace_entry->name = grab (new String (namespace_name));
	namespace_entry->namespace_ = new_namespace;
// Unused	namespace_entry->containing_namespace = namespace_;

	{
	    Cpp::NamespaceEntry::NamespaceEntryMap::Entry namespace_entries_map_entry =
		    cur_container->namespace_entries.add (namespace_entry);

	    checkpoint_tracker.addStickyCancellable (
		    MyCpp::grab (new Cancellable_MapEntry<Cpp::NamespaceEntry::NamespaceEntryMap> (
					 cur_container->namespace_entries,
					 namespace_entries_map_entry)));
	}

	{
	  // TODO Namespace aliases?

	    Cpp::Container::NamespaceMap::Entry namespaces_map_entry =
		    cur_container->namespaces.add (new_namespace);

	    checkpoint_tracker.addStickyCancellable (
		    MyCpp::grab (new Cancellable_MapEntry<Cpp::Container::NamespaceMap> (
					 cur_container->namespaces,
					 namespaces_map_entry)));
	}

	namespace_ = new_namespace;
    }

    // TODO Is this necessary?
    //      Note: This is for nested-names.
    {
	containers.append (namespace_);

	Ref< Cancellable_ListElement< Cpp::Container* > > cancellable =
		grab (new Cancellable_ListElement< Cpp::Container* > (
			      containers,
			      containers.last));
	checkpoint_tracker.addUnconditionalCancellable (cancellable);
    }

    abortIf (namespace_.isNull ());
    declaration_acceptors.append (
	    grab (new Acceptor_ContainerMember (*this,
						*namespace_,
						checkpoint_tracker.getCurrentCheckpoint ())));
    checkpoint_tracker.addUnconditionalCancellable (
	    grab (new Cancellable_ListElement< Ref< Scruffy::Acceptor<DeclarationDesc> > > (
			  declaration_acceptors,
			  declaration_acceptors.last)));
}

void
CppParser_Impl::beginTemporalNamespace ()
{
    DEBUG (
	errf->print ("Scruffy.CppParser_Impl.beginTemporalNamespace").pendl ();
    )

    Ref<Cpp::Namespace> temporal_namespace = grab (new Cpp::Namespace);
//    List< Ref<Cpp::Namespace> >::Element *temporal_namespace_el =
	    temporal_namespaces.append (temporal_namespace);
    {
	Ref< Cancellable_ListElement< Ref<Cpp::Namespace> > > cancellable =
		grab (new Cancellable_ListElement< Ref<Cpp::Namespace> > (
			      temporal_namespaces,
			      temporal_namespaces.last));

	checkpoint_tracker.addUnconditionalCancellable (cancellable);
    }

    // TODO Declaration acceptor
}

CppParser_Impl::CppParser_Impl (ConstMemoryDesc const &default_variant)
    : default_variant (grab (new String (default_variant)))
{
    checkpoint_counter = 0;

    {
      // TODO Unify this with beginNamespace() somehow.

	root_namespace_entry = grab (new Cpp::NamespaceEntry);
	root_namespace_entry->namespace_ = grab (new Cpp::Namespace);

	containers.append (root_namespace_entry->namespace_);

	declaration_acceptors.append (
		grab (new Acceptor_ContainerMember (*this,
						    *root_namespace_entry->namespace_,
						    checkpoint_tracker.getCurrentCheckpoint ())));
	checkpoint_tracker.addUnconditionalCancellable (
		grab (new Cancellable_ListElement< Ref< Scruffy::Acceptor<DeclarationDesc> > > (
			      declaration_acceptors,
			      declaration_acceptors.last)));
    }

    class_phase = ClassParsingPhase_None;
//    class_phase_depth = 0;
    class_checkpoint = 0;

    {
	ExpressionState &expression_state = expression_states.appendEmpty ()->data;
	expression_state.checkpoint_key = checkpoint_tracker.getCurrentCheckpoint ();
    }
}

static Ref<Cpp::TypeDesc_Class>
getTypeForClassName (Cpp_ClassName const * const class_name)
{
    switch (class_name->class_name_type) {
	case Cpp_ClassName::t_ClassNameIdentifier: {
	    Cpp_ClassName_ClassNameIdentifier const * const class_name__class_name_identifier =
		    static_cast <Cpp_ClassName_ClassNameIdentifier const *> (class_name);
	    Cpp_ClassNameIdentifier const * const class_name_identifier =
		    class_name__class_name_identifier->classNameIdentifier;

	    Cpp::TypeDesc_Class * const type_desc__class =
		    static_cast <Cpp::TypeDesc_Class*> (class_name_identifier->user_obj);
	    abortIf (type_desc__class == NULL);

	    return type_desc__class;
	} break;
	case Cpp_ClassName::t_TemplateId: {
	  // TODO
	    abortIfReached ();
	} break;
	default:
	    abortIfReached ();
    }

    abortIfReached ();
    return NULL;
}

bool
cpp_begin_id (CppElement            * const /* cpp_element */,
	      Pargen::ParserControl * const /* parser_control */,
	      void                  * const _self)
{
  FUNC_NAME (
    static char const * const _func_name = "cpp_begin_id";
  )

    DEBUG (
	errf->print (_func_name).pendl ();
    )

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    CppParser_Impl::IdState &id_state = self->id_states.appendEmpty ()->data;
    self->checkpoint_tracker.addUnconditionalCancellable (
	    grab (new Cancellable_ListElement<CppParser_Impl::IdState> (
			  self->id_states,
			  self->id_states.last)));

    id_state.checkpoint_key = self->checkpoint_tracker.getCurrentCheckpoint ();

    return true;
}

bool
cpp_accept_qualified_id_nested_name (Cpp_QualifiedId       * const /* qualified_id */,
				     Pargen::ParserControl * const /* parser_control */,
				     void                  * const _self)
{
    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    CppParser_Impl::IdState &id_expression_state = self->id_states.getLast ();

    abortIf (self->nested_name_specifier_states.isEmpty ());
    CppParser_Impl::NestedNameSpecifierState &nested_name_specifier_state =
	    self->nested_name_specifier_states.last->data;

    abortIf (id_expression_state.global_scope ||
	     !id_expression_state.nested_name_atoms.isEmpty ());

    id_expression_state.global_scope = nested_name_specifier_state.global_scope;
    id_expression_state.nested_name_atoms.steal (&nested_name_specifier_state.atoms,
						 nested_name_specifier_state.atoms.first,
						 nested_name_specifier_state.atoms.last,
						 id_expression_state.nested_name_atoms.first,
						 GenericList::StealAppend);
    {
	// FIXME addBoundCancellable()
	Ref<CppParser_Impl::IdState::Cancellable_NestedName> cancellable =
		grab (new CppParser_Impl::IdState::Cancellable_NestedName (
			      id_expression_state));
	self->checkpoint_tracker.addCancellable (cancellable);
    }

    return true;
}

bool
cpp_begin_nested_name (CppElement            * const /* cpp_element */,
		       Pargen::ParserControl * const /* parser_control */,
		       void                  * const _self)
{
    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    self->nested_name_specifier_states.appendEmpty ();
    {
	Ref< Cancellable_ListElement<CppParser_Impl::NestedNameSpecifierState> > cancellable =
		grab (new Cancellable_ListElement<CppParser_Impl::NestedNameSpecifierState> (
			      self->nested_name_specifier_states,
			      self->nested_name_specifier_states.last));

	self->checkpoint_tracker.addUnconditionalCancellable (cancellable);
    }

    return true;
}

void
cpp_Scruffy_GlobalScope_accept_func (Cpp_Scruffy_GlobalScope * const scruffy_global_scope,
				     ParserControl           * const /* parser_control */,
				     void                    * const _parser_state)
{
    if (scruffy_global_scope == NULL)
	return;

    CppParser_Impl *parser_state = static_cast <CppParser_Impl*> (_parser_state);

    abortIf (parser_state->nested_name_specifier_states.isEmpty ());
    CppParser_Impl::NestedNameSpecifierState &nested_name_specifier_state =
	    parser_state->nested_name_specifier_states.last->data;

    DEBUG (
	errf->print ("--- global scope").pendl ();
    )
    nested_name_specifier_state.global_scope = true;
}

bool
cpp_NestedNameSpecifier_ClassOrNamespaceName_accept_func (Cpp_NestedNameSpecifier * const nested_name_specifier,
							  ParserControl           * const /* parser_control */,
							  void                    * const parser_)
{
  FUNC_NAME (
    static char const * const _func_name =
	    "Scruffy.(CppParser_Impl).cpp_NestedNameSpecifier_ClassOrNamespaceName_accept_func";
  )

    if (nested_name_specifier == NULL)
	return true;

    DEBUG (
	errf->print (_func_name).pendl ();
    )

    CppParser_Impl *parser = static_cast <CppParser_Impl*> (parser_);

    abortIf (parser->nested_name_specifier_states.isEmpty ());
    CppParser_Impl::NestedNameSpecifierState &nested_name_specifier_state =
	    parser->nested_name_specifier_states.last->data;

    Cpp_ClassOrNamespaceName const *class_or_namespace_name = NULL;
    switch (nested_name_specifier->nested_name_specifier_type) {
	case Cpp_NestedNameSpecifier::t_Template: {
	    Cpp_NestedNameSpecifier_Template const * const nested_name_specifier__template =
		    static_cast <Cpp_NestedNameSpecifier_Template const *> (nested_name_specifier);

	    class_or_namespace_name = nested_name_specifier__template->classOrNamespaceName;
	} break;
	case Cpp_NestedNameSpecifier::t_Generic: {
	    Cpp_NestedNameSpecifier_Generic const * const nested_name_specifier__generic =
		    static_cast <Cpp_NestedNameSpecifier_Generic const *> (nested_name_specifier);

	    class_or_namespace_name = nested_name_specifier__generic->classOrNamespaceName;
	} break;
	default:
	    abortIfReached ();
    }

    List<Cpp::Container*>::Element *atom_el = NULL;
    switch (class_or_namespace_name->class_or_namespace_name_type) {
	case Cpp_ClassOrNamespaceName::t_ClassName: {
	    Cpp_ClassOrNamespaceName_ClassName const * const class_or_namespace_name__class_name =
		    static_cast <Cpp_ClassOrNamespaceName_ClassName const *> (class_or_namespace_name);
	    Cpp_ClassName const * const class_name =
		    class_or_namespace_name__class_name->className;

	    Ref<Cpp::TypeDesc_Class> type_desc__class = getTypeForClassName (class_name);
	    if (type_desc__class.isNull ())
		abortIfReached ();

	    if (type_desc__class->class_.isNull ()) {
		errf->print ("No class definition").pendl ();
		abortIfReached ();
	    }

	    atom_el = nested_name_specifier_state.atoms.append (type_desc__class->class_);

#if 0
// getClassForClassName (done)
	    switch (class_name->class_name_type) {
		case Cpp_ClassName::t_ClassNameIdentifier: {
		    Cpp_ClassName_ClassNameIdentifier const * const class_name__class_name_identifier =
			    static_cast <Cpp_ClassName_ClassNameIdentifier const *> (class_name);
		    Cpp_ClassNameIdentifier const * const class_name_identifier =
			    class_name__class_name_identifier->classNameIdentifier;

		    Cpp::TypeDesc_Class * const type_desc__class =
			    static_cast <Cpp::TypeDesc_Class*> (class_name_identifier->user_obj);
		    abortIf (type_desc__class == NULL);

		    {
			if (type_desc__class->class_.isNull ()) {
			    DEBUG (
				errf->print ("No class definition").pendl ();
			    )
			    abortIfReached ();
			}

			// TODO A cancellable is required here (?)
			atom_el = nested_name_specifier_state.atoms.append (type_desc__class->class_);
		    }
		} break;
		case Cpp_ClassName::t_TemplateId: {
		  // TODO
		    abortIfReached ();
		} break;
		default:
		    abortIfReached ();
	    }
#endif
	} break;
	case Cpp_ClassOrNamespaceName::t_NamespaceName: {
	    Cpp_ClassOrNamespaceName_NamespaceName const * const class_or_namespace_name__namespace_name =
		    static_cast <Cpp_ClassOrNamespaceName_NamespaceName const *> (class_or_namespace_name);
	    Cpp_NamespaceName const * const namespace_name =
		    class_or_namespace_name__namespace_name->namespaceName;

	    Cpp::Namespace * const namespace_ =
		    static_cast <Cpp::Namespace*> (namespace_name->user_obj);
	    abortIf (namespace_ == NULL);

	    {
		// TODO A cancellable is required here (?)
		atom_el = nested_name_specifier_state.atoms.append (namespace_);
	    }
	} break;
	default:
	    abortIfReached ();
    }
    abortIf (atom_el == NULL);

    {
	Ref< Cancellable_ListElement<Cpp::Container*> > cancellable =
		grab (new Cancellable_ListElement<Cpp::Container*> (
			      nested_name_specifier_state.atoms,
			      atom_el));

	parser->checkpoint_tracker.addCancellable (cancellable);
    }

    return true;
}

#if 0
// Deprecated
bool
cpp_Literal_match_func (Cpp_Literal *literal,
			void * /* _data */)
{
    Token *token = static_cast <Token*> (literal->any_token->getUserPtr ());
    abortIf (token == NULL);

    if (token->token_type == TokenLiteral)
	return true;

    return false;
}
#endif

bool
cpp_literal_token_match_func (ConstMemoryDesc const & /* token_mem */,
			      void                  * const token_user_ptr,
			      void                  * const /* user_data */)
{
    Token * const token = static_cast <Token*> (token_user_ptr);
    abortIf (token == NULL);

    if (token->token_type == TokenLiteral)
	return true;

    return false;
}

#if 0
// Deprecated
bool
cpp_Identifier_match_func (Cpp_Identifier *identifier,
			   void * /* _data */)
{
    Token *token = static_cast <Token*> (identifier->any_token->getUserPtr ());

    if (token->token_type == TokenIdentifier)
	return true;

  /* DEBUG */
//    if (token->token_type == TokenKeyword)
//	errf->print ("Scruffy.(CppParser_Impl).cpp_Identifier_match_func: keyword").pendl ();

//    errf->print ("Scruffy.(CppParser_Impl).cpp_Identifier_match_func: no match: \"").print (token->str).print ("\"").pendl ();
  /* (DEBUG) */

    return false;

#if 0
    ConstMemoryDesc mem = identifier->any_token->getToken ()->getMemoryDesc ();
    for (Size i = 0; i < mem.getLength (); i++) {
	if ((mem [i] >= 0x41 /* 'A' */ && mem [i] <= 0x5a /* 'Z' */) ||
	    (mem [i] >= 0x61 /* 'a' */ && mem [i] <= 0x7a /* 'z' */) ||
	    (mem [i] == 0x5f /* '_' */))
	{
	    continue;
	}

	if (i > 0 &&
	    (mem [i] >= 0x30 /* '0' */ && mem [i] <= 0x39 /* '9' */))
	{
	    continue;
	} 

	return false;
    }

    return true;
#endif
}
#endif

bool
cpp_identifier_token_match_func (ConstMemoryDesc const & /* token_mem */,
				 void                  * const token_user_ptr,
				 void                  * const /* user_data */)
{
    Token * const token = static_cast <Token*> (token_user_ptr);
    abortIf (token == NULL);

    if (token->token_type == TokenIdentifier) {
//	errf->print ("i").pendl ();
	return true;
    }

//    errf->print ("!i").pendl ();
    return false;
}

bool
cpp_Scruffy_NotABrace_match_func (Cpp_Scruffy_NotABrace * const el,
				  ParserControl         * const /* parser_control */,
				  void                  * const /* _data */)
{
    if (compareByteArrays (el->any_token->token, ConstMemoryDesc::forString ("{")) == ComparisonEqual ||
	compareByteArrays (el->any_token->token, ConstMemoryDesc::forString ("}")) == ComparisonEqual)
    {
	return false;
    }

    return true;
}

bool
cpp_Scruffy_NotAParenthesis_match_func (Cpp_Scruffy_NotAParenthesis * const el,
					ParserControl               * const /* parser_control */,
					void                        * const /* _data */)
{
    if (compareByteArrays (el->any_token->token, ConstMemoryDesc::forString ("(")) == ComparisonEqual ||
	compareByteArrays (el->any_token->token, ConstMemoryDesc::forString (")")) == ComparisonEqual)
    {
	return false;
    }

    return true;
}

void
cpp_Scruffy_SkipCompoundStatement_accept_func (Cpp_Scruffy_SkipCompoundStatement * const /* el */,
					       Pargen::ParserControl             * const /* parser_control */,
					       void                              * const /* _data */)
{
    DEBUG (
	errf->print ("--- skip-compound-statement").pendl ();
    )
}

void
cpp_Scruffy_SkipExpressionList_accept_func (Cpp_Scruffy_SkipExpressionList * const /* el */,
					    Pargen::ParserControl          * const /* parser_control */,
					    void                           * const /* _data */)
{
    DEBUG (
	errf->print ("--- skip-expression-list").pendl ();
    )
}

void
cpp_Scruffy_SkipExceptionDeclaration_accept_func (Cpp_Scruffy_SkipExceptionDeclaration * const /* el */,
						  Pargen::ParserControl                * const /* parser_control */,
						  void                                 * const /* _data */)
{
    DEBUG (
	errf->print ("--- skip-exception-declaration").pendl ();
    )
}

bool
cpp_begin_constructor_ambiguity (CppElement            * const /* cpp_element */,
				 Pargen::ParserControl * const parser_control,
				 void                  * const _self)
{
    DEBUG (
	errf->print ("--- BEGIN_CONSTRUCTOR_AMBIGUITY ---").pendl ();
    )

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    CppParser_Impl::ConstructorAmbiguityState &constructor_ambiguity_state =
	    self->constructor_ambiguity_states.appendEmpty ()->data;
    constructor_ambiguity_state.cancellable_key =
	    self->checkpoint_tracker.addUnconditionalCancellable (
		    createLastElementCancellable (self->constructor_ambiguity_states));

    constructor_ambiguity_state.parser_position = parser_control->getPosition ();

    return true;
}

bool
cpp_begin_dummy_constructor_ambiguity (CppElement            * const /* cpp_element */,
				       Pargen::ParserControl * const /*  parser_control */,
				       void                  * const _self)
{
    DEBUG (
	errf->print ("--- BEGIN_DUMMY_CONSTRUCTOR_AMBIGUITY ---").pendl ();
    )

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    CppParser_Impl::ConstructorAmbiguityState &constructor_ambiguity_state =
	    self->constructor_ambiguity_states.appendEmpty ()->data;
    constructor_ambiguity_state.cancellable_key =
	    self->checkpoint_tracker.addUnconditionalCancellable (
		    createLastElementCancellable (self->constructor_ambiguity_states));

    constructor_ambiguity_state.is_dummy = true;
// Unnecessary    constructor_ambiguity_state.parser_position = parser_control->getPosition ();

    return true;
}

bool
cpp_function_definition_match_func (Cpp_FunctionDefinition * const /* function_definition */,
				    ParserControl          * const /* const parser_control */,
				    void                   * const _self)
{
  FUNC_NAME (
    static char const * const _func_name = "cpp_function_definition_match_func";
  )

//    DEBUG (
//	errf->print (_func_name).print (": function_definition: 0x").printHex ((Uint64) function_definition).pendl ();
//    )

  // Temporal solution (inline match funcs needed).

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

#if 0
// Deprecated

    switch (function_definition->function_definition_type) {
      // Note: 'declarator' is fictively optional for constructor name disambiguation.

	case Cpp_FunctionDefinition::t_FunctionBody: {
	    Cpp_FunctionDefinition_FunctionBody const * const function_definition__function_body =
		    static_cast <Cpp_FunctionDefinition_FunctionBody const *> (function_definition);

	    if (function_definition__function_body->declarator.isNull ())
		return false;
	} break;
	case Cpp_FunctionDefinition::t_FunctionTryBlock: {
	    Cpp_FunctionDefinition_FunctionTryBlock const * const function_definition__function_try_block =
		    static_cast <Cpp_FunctionDefinition_FunctionTryBlock const *> (function_definition);

	    if (function_definition__function_try_block->declarator.isNull ())
		return false;
	} break;
	default:
	    abortIfReached ();
    }
#endif

    abortIf (self->statement_states.isEmpty ());
    CppParser_Impl::StatementState &statement_state = self->statement_states.last->data;

    DeclarationState &declaration_state = self->declaration_states.getLast ();

    // Note: 'declarator' is fictively optional for constructor name disambiguation.
    if (!declaration_state.got_declarator) {
	DEBUG (
	    errf->print (_func_name).print (": no declarator").pendl ();
	)
	return false;
    }

    {
      // We should be in the process of parsing a function definition.

	abortIf (declaration_state.member.isNull () ||
		 declaration_state.member->type_desc.isNull ());

	if (declaration_state.member->getType () != Cpp::Member::t_Function) {
	    errf->print ("Not a function").pendl ();
	    abortIfReached ();
	}
    }

    Cpp::Member_Function * const member__function =
	    static_cast <Cpp::Member_Function*> (declaration_state.member.ptr ());

    DEBUG (
	errf->print (_func_name).print (": member__function: 0x").printHex ((Uint64) member__function).pendl ();
	errf->print (_func_name).print (": ").print (statement_state.statements.getNumElements ()).print (" statements").pendl ();
    )

    member__function->function->statements.steal (&statement_state.statements,
						  statement_state.statements.first,
						  statement_state.statements.last,
						  member__function->function->statements.last,
						  GenericList::StealAppend);

    return true;
}

bool
cpp_accept_type_specifier__cv_qualifier (Cpp_TypeSpecifier * const type_specifier,
					 ParserControl     * const /* parser_control */,
					 void              * const _self)
{
    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    DeclarationState &declaration_state = self->declaration_states.getLast ();

    abortIf (type_specifier->type_specifier_type != Cpp_TypeSpecifier::t_CvQualifier);
    Cpp_TypeSpecifier_CvQualifier * const type_specifier__cv_qualifier =
	    static_cast <Cpp_TypeSpecifier_CvQualifier*> (type_specifier);

    switch (type_specifier__cv_qualifier->cvQualifier->cv_qualifier_type) {
	case Cpp_CvQualifier::t_Const: {
	    declaration_state.type_specifier_parser.setConst ();
	} break;
	case Cpp_CvQualifier::t_Volatile: {
	    declaration_state.type_specifier_parser.setVolatile ();
	} break;
	default:
	    abortIfReached ();
    }

    return true;
}

bool
cpp_accept_elaborated_type_specifier (Cpp_ElaboratedTypeSpecifier * const elaborated_type_specifier,
				      ParserControl               * const /* parser_control */,
				      void                        * const _self)
{
    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    DeclarationState &declaration_state = self->declaration_states.getLast ();

    switch (elaborated_type_specifier->elaborated_type_specifier_type) {
	case Cpp_ElaboratedTypeSpecifier::t_Class: {
	    Cpp_ElaboratedTypeSpecifier_Class const * const elaborated_type_specifier__class =
		    static_cast <Cpp_ElaboratedTypeSpecifier_Class const *> (elaborated_type_specifier);

	    ConstMemoryDesc identifier_mem = elaborated_type_specifier__class->identifier->any_token->token;
	    Cpp::Member * const member = self->lookupType (identifier_mem);
	    if (member != NULL) {
		abortIf (member->getType () != Cpp::Member::t_Type);

		Cpp::Member_Type const * const member__type =
			static_cast <Cpp::Member_Type const *> (member);

		if (member__type->is_typedef) {
		    errf->print ("Typedef in elaborated-type-specifier").pendl ();
		    abortIfReached ();
		}

		abortIf (member->type_desc.isNull ());
		switch (member->type_desc->getType ()) {
		    case Cpp::TypeDesc::t_Class: {
			Cpp::TypeDesc_Class const * const type_desc__class =
				static_cast <Cpp::TypeDesc_Class const *> (member->type_desc.ptr ());

			abortIf (type_desc__class->class_.isNull ());

			declaration_state.type_specifier_parser.setClass (type_desc__class->class_);
		    } break;
		    case Cpp::TypeDesc::t_Dependent: {
			// TODO
			abortIfReached ();
		    } break;
		    default: {
			errf->print ("Bad member in elaborated-type-specifier").pendl ();
			abortIfReached ();
		    }
		}
	    } else {
	      // Forward declaration

		Cpp::Container * const container = self->getContainerForNestedName (NULL /* ret_fixed_namespace */);

		Ref<Cpp::Member> member = self->addClass (declaration_state, container, identifier_mem);
		self->setDeclarationTypeDesc (declaration_state, member->type_desc);

		abortIf (member->type_desc.isNull () ||
			 member->type_desc->getType () != Cpp::TypeDesc::t_Class);
		Cpp::TypeDesc_Class const * const type_desc__class =
			static_cast <Cpp::TypeDesc_Class*> (member->type_desc.ptr ());

		declaration_state.type_specifier_parser.setClass (type_desc__class->class_);
	    }
	} break;
	case Cpp_ElaboratedTypeSpecifier::t_Enum: {
//	    Cpp_ElaboratedTypeSpecifier_Enum const * const elaborated_type_specifier__enum =
//		    static_cast <Cpp_ElaboratedTypeSpecifier_Enum const *> (elaborated_type_specifier);

	    // TODO
	    abortIfReached ();
	} break;
	case Cpp_ElaboratedTypeSpecifier::t_Typename: {
//	    Cpp_ElaboratedTypeSpecifier_Typename const * const elaborated_type_specifier__typename =
//		    static_cast <Cpp_ElaboratedTypeSpecifier_Typename const *> (elaborated_type_specifier);

	    // TODO
	    abortIfReached ();
	} break;
	case Cpp_ElaboratedTypeSpecifier::t_TypenameTemplate: {
//	    Cpp_ElaboratedTypeSpecifier_TypenameTemplate const * const elaborated_type_specifier__typename_template =
//		    static_cast <Cpp_ElaboratedTypeSpecifier_TypenameTemplate const *> (elaborated_type_specifier);

	    // TODO
	    abortIfReached ();
	} break;
	default:
	    abortIfReached ();
    }

    return true;
}

bool
cpp_accept_enum_specifier (Cpp_EnumSpecifier * const enum_specifier,
			   ParserControl     * const /* parser_control */,
			   void              * const _self)
{
    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    DeclarationState &declaration_state = self->declaration_states.getLast ();

    declaration_state.type_specifier_parser.setEnum (declaration_state.enum_);

    return true;
}

bool
cpp_simple_type_specifier__template_id_jump (ParserElement * const _simple_type_specifier,
					     void          * const /* _self */)
{
    Cpp_SimpleTypeSpecifier * const simple_type_specifier =
	    static_cast <Cpp_SimpleTypeSpecifier*> (_simple_type_specifier);

    abortIf (simple_type_specifier->simple_type_specifier_type != Cpp_SimpleTypeSpecifier::t_TypeName);
    Cpp_SimpleTypeSpecifier_TypeName * const simple_type_specifier__type_name =
	    static_cast <Cpp_SimpleTypeSpecifier_TypeName*> (simple_type_specifier);

    // nested-name-specifier is non-optional in simple-type-specifier:TemplateId.
    if (simple_type_specifier__type_name->nestedNameSpecifier != NULL)
	return true;

    return false;
}

#if 0
// Unnecessary
bool
cpp_SimpleTypeSpecifier_match_func (Cpp_SimpleTypeSpecifier * const simple_type_specifier,
				    ParserControl           * const /* parser_control */,
				    void                    * const _self)
{
  // Here we are resolving constructor declaration ambiguity.

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    switch (simple_type_specifier->simple_type_specifier_type) {
	case Cpp_SimpleTypeSpecifier::t_TypeName: {
	    Cpp_SimpleTypeSpecifier_TypeName const * const simple_type_specifier__nested_type =
		    static_cast <Cpp_SimpleTypeSpecifier_TypeName const *> (simple_type_specifier);

	    Cpp_TypeName const * const type_name =
		    simple_type_specifier__nested_type->typeName;

	    switch (type_name->type_name_type) {
		case Cpp_TypeName::t_ClassName: {
		    Cpp_TypeName_ClassName const * const type_name__class_name =
			    static_cast <Cpp_TypeName_ClassName const *> (type_name);

		    Cpp_ClassName const * const class_name =
			    type_name__class_name->className;

		    switch (class_name->class_name_type) {
			case Cpp_ClassName::t_ClassNameIdentifier: {
#if 0
			    Cpp_ClassName_ClassNameIdentifier const * const class_name__class_name_identifier =
				    static_cast <Cpp_ClassName_ClassNameIdentifier const *> (class_name);

			    Cpp_ClassNameIdentifier const * const class_name_identifier =
				    class_name__class_name_identifier->classNameIdentifier;

			    abortIf (class_name_identifier->user_obj.isNull ());
			    Cpp::TypeDesc_Class * const type_desc__class =
				    static_cast <Cpp::TypeDesc_Class*> (class_name_identifier->user_obj.ptr ());
#endif

			    Ref<Cpp::TypeDesc_Class> type_desc__class = getTypeForClassName (class_name);

			    DEBUG (
				errf->print ("--- SIMPLE TYPE SPECIFIER USED ---").pendl ();
			    )
			    type_desc__class->simple_type_specifier_used = true;

			    if (!self->constructor_ambiguity_states.isEmpty ()) {
				CppParser_Impl::ConstructorAmbiguityState &constructor_ambiguity_state =
					self->constructor_ambiguity_states.last->data;

				if (constructor_ambiguity_state.constructor_attempt) {
				    DEBUG (
					errf->print ("--- SIMPLE TYPE SPECIFIER DISAMBIGUATED ---").pendl ();
				    )
				    return false;
				}
			    }
			} break;
			default:
			  // No-op
			    ;
		    }
		} break;
		default:
		  // No-op
		    ;
	    }
	} break;
	default:
	 // No-op
	    ;
    }

    return true;
}
#endif

static bool
cpp_accept_template_type_specifier (CppParser_Impl        * const self,
				    Cpp::TemplateInstance * const template_instance)
{
    static char const * const _func_name = "cpp_accept_template_type_specifier";

    DeclarationState &declaration_state = self->declaration_states.getLast ();

#if 0
	    // TODO Store template instantiations used instead of creating
	    // a new TypeDesc each time.

	    Ref<TypeDesc_Class> type_desc__class = grab (new TypeDesc_Class);

	    type_desc__class->is_template = true;
	    type_desc__class->template_arguments.steal (&id_state->template_arguments,
							id_state->template_arguments.first,
							id_state->template_arguments.last,
							type_desc__class->template_arguments.last,
							GenericList::StealAppend);
#endif

    if (template_instance->getType () != Cpp::TemplateInstance::t_Class) {
	errf->print (_func_name).print (": not a class template").pendl ();
	return false;
    }

    Cpp::ClassTemplateInstance * const class_template_instance =
	    static_cast <Cpp::ClassTemplateInstance*> (template_instance);

#if 0
// Deprecated

    // TODO Find the right Member among possible alternatives (explicit specializations).
    Cpp::Member const * const member = self->lookupType (id_state.template_name);
    abortIf (member == NULL);
    abortIf (!member->isTemplate ());

    if (member->type_desc->getType () != Cpp::TypeDesc::t_Class) {
	errf->print (_func_name).print (": not a class template");
	abortIfReached ();
    }
#endif

#if 0
    Cpp::TypeDesc_Class * const type_desc__class =
	    static_cast <Cpp::TypeDesc_Class*> (member->type_desc.ptr ());

    declaration_state.type_specifier_parser.setClass (type_desc__class->class_);
#endif

    declaration_state.type_specifier_parser.setClass (class_template_instance->class_);

//#error TODO implement stealTemplateArguments()
//    declaration_state.type_specifier_parser.stealTemplateArguments (&id_state->template_arguments);

    return true;
}

bool
cpp_accept_simple_type_specifier (Cpp_SimpleTypeSpecifier * const simple_type_specifier,
				  ParserControl           * const /* parser_control */,
				  void                    * const _self)
{
    static char const * const _func_name = "cpp_accept_simple_type_specifier";

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    DeclarationState &declaration_state = self->declaration_states.getLast ();

    switch (simple_type_specifier->simple_type_specifier_type) {
	case Cpp_SimpleTypeSpecifier::t_Void: {
	    declaration_state.type_specifier_parser.setBasicType_Void ();
	} break;
	case Cpp_SimpleTypeSpecifier::t_Bool: {
	    declaration_state.type_specifier_parser.setBasicType_Bool ();
	} break;
	case Cpp_SimpleTypeSpecifier::t_Int: {
	    declaration_state.type_specifier_parser.setBasicType_Int ();
	} break;
	case Cpp_SimpleTypeSpecifier::t_Char: {
	    declaration_state.type_specifier_parser.setBasicType_Char ();
	} break;
	case Cpp_SimpleTypeSpecifier::t_Unsigned: {
	    declaration_state.type_specifier_parser.setBasicType_Unsigned ();
	} break;
	case Cpp_SimpleTypeSpecifier::t_Long: {
	    declaration_state.type_specifier_parser.setBasicType_Long ();
	} break;
	case Cpp_SimpleTypeSpecifier::t_Float: {
	    declaration_state.type_specifier_parser.setBasicType_Float ();
	} break;
	case Cpp_SimpleTypeSpecifier::t_Double: {
	    declaration_state.type_specifier_parser.setBasicType_Double ();
	} break;
	case Cpp_SimpleTypeSpecifier::t_Short: {
	    // TODO
	    abortIfReached ();
	} break;
	case Cpp_SimpleTypeSpecifier::t_WcharT: {
	    // TODO
	    abortIfReached ();
	} break;
	case Cpp_SimpleTypeSpecifier::t_Signed: {
	    declaration_state.type_specifier_parser.setBasicType_Signed ();
	} break;
	case Cpp_SimpleTypeSpecifier::t_TypeName: {
#if 0
	    if (type_category != Unknown) {
		errf->print (_func_name).print (": more than one data type").pendl ();
		abortIfReached ();
	    }
#endif

	    Cpp_SimpleTypeSpecifier_TypeName * const simple_type_specifier__nested_type =
		    static_cast <Cpp_SimpleTypeSpecifier_TypeName*> (simple_type_specifier);

	    Cpp_TypeName * const type_name =
		    simple_type_specifier__nested_type->typeName;
	    switch (type_name->type_name_type) {
		case Cpp_TypeName::t_ClassName: {
		    Cpp_TypeName_ClassName * const type_name__class_name =
			    static_cast <Cpp_TypeName_ClassName*> (type_name);

		    Cpp_ClassName * const class_name =
			    type_name__class_name->className;

		    switch (class_name->class_name_type) {
			case Cpp_ClassName::t_ClassNameIdentifier: {
			    Cpp_ClassName_ClassNameIdentifier * const class_name__class_name_identifier =
				    static_cast <Cpp_ClassName_ClassNameIdentifier*> (class_name);

			    if (!self->constructor_ambiguity_states.isEmpty ()) {
				CppParser_Impl::ConstructorAmbiguityState &constructor_ambiguity_state =
					self->constructor_ambiguity_states.last->data;

				if (constructor_ambiguity_state.constructor_attempt) {
				    DEBUG (
					errf->print ("--- SIMPLE TYPE SPECIFIER DISAMBIGUATED ---").pendl ();
				    )
				    return false;
				}
			    }

			    Cpp_ClassNameIdentifier * const class_name_identifier =
				    class_name__class_name_identifier->classNameIdentifier;

			    abortIf (class_name_identifier->user_obj == NULL);
			    Cpp::TypeDesc_Class * const type_desc__class =
				    static_cast <Cpp::TypeDesc_Class*> (class_name_identifier->user_obj);

			    declaration_state.type_specifier_parser.setClass (type_desc__class->class_,
									      true /* simple_type_specifier_used */);
			} break;
			case Cpp_ClassName::t_TemplateId: {
			    Cpp_ClassName_TemplateId * const class_name__template_id =
				    static_cast <Cpp_ClassName_TemplateId*> (class_name);

			    Cpp::TemplateInstance * const template_instance =
				    static_cast <Cpp::TemplateInstance*> (class_name__template_id->templateId->user_obj);

			    bool const res = cpp_accept_template_type_specifier (self, template_instance);
			    if (!res)
				return false;
			} break;
			default:
			    abortIfReached ();
		    }
		} break;
		case Cpp_TypeName::t_EnumName: {
		    errf->print ("--- ENUM-NAME").pendl ();
		    // TODO
		    abortIfReached ();
		} break;
		case Cpp_TypeName::t_TypedefName: {
		    Cpp_TypeName_TypedefName * const type_name__typedef_name =
			    static_cast <Cpp_TypeName_TypedefName*> (type_name);

		    Cpp_TypedefName * const typedef_name =
			    type_name__typedef_name->typedefName;

		    abortIf (typedef_name->user_obj == NULL);
		    Ref<Cpp::TypeDesc> type_desc =
			    static_cast <Cpp::TypeDesc*> (typedef_name->user_obj);

		    declaration_state.type_specifier_parser.setTypedefName (type_desc);
		} break;
		default:
		    abortIfReached ();
	    }
	} break;
	case Cpp_SimpleTypeSpecifier::t_TemplateId: {
	    Cpp_SimpleTypeSpecifier_TemplateId * const simple_type_specifier__template_id =
		    static_cast <Cpp_SimpleTypeSpecifier_TemplateId*> (simple_type_specifier);

	    Cpp::TemplateInstance * const template_instance =
		    static_cast <Cpp::TemplateInstance*> (
			    simple_type_specifier__template_id->templateId->user_obj);

	    bool const res = cpp_accept_template_type_specifier (self, template_instance);
	    if (!res)
		return false;
	} break;
	default:
	    abortIfReached ();
    }

    return true;
}

namespace {

    void
    TypeSpecifierParser::parseTypeSpecifier (Cpp_TypeSpecifier const * const type_specifier,
					     DeclarationState               &declaration_state)
    {
	static char const * const _func_name = "Scruffy.CppParser.TypeSpecifierParser.parseTypeSpecifier";

	switch (type_specifier->type_specifier_type) {
	    case Cpp_TypeSpecifier::t_SimpleTypeSpecifier: {
		Cpp_TypeSpecifier_SimpleTypeSpecifier const * const type_specifier__simple_type_specifier =
			static_cast <Cpp_TypeSpecifier_SimpleTypeSpecifier const *> (type_specifier);
		Cpp_SimpleTypeSpecifier const * const simple_type_specifier =
			type_specifier__simple_type_specifier->simpleTypeSpecifier;

		parseSimpleTypeSpecifier (simple_type_specifier);
	    } break;
	    case Cpp_TypeSpecifier::t_ClassSpecifier: {
		if (gotType ()) {
		    errf->print (_func_name).print (": more than one data type").pendl ();
		    abortIfReached ();
		}

		// The class itself is parsed by subcallbacks.
		// It has already been parsed at this point.
		// We've stored the result of parsing the last class-specifier
		// in 'declaration_state.class_'.

		// Note that anonymous classes are possible.

		// TODO Actually, we should assign the same DataType_Class
		// as the one we've created after parsing the class.
		class_ = declaration_state.class_;
		type_category = Class;
	    } break;
	    case Cpp_TypeSpecifier::t_EnumSpecifier: {
		if (gotType ()) {
		    errf->print (_func_name).print (": more than one data type").pendl ();
		    abortIfReached ();
		}

		// The enum itself is parsed by subcallbacks.
		// It has already been parsed at this point.
		// We've stored the result of parsing the last enum-specifier
		// in 'declaration_state.enum_'.

		// Note that anonymous enums are possible.

		enum_ = declaration_state.enum_;
		type_category = Enum;
	    } break;
	    case Cpp_TypeSpecifier::t_ElaboratedTypeSpecifier: {
		if (gotType ()) {
		    errf->print (_func_name).print (": more than one data type").pendl ();
		    abortIfReached ();
		}

		Cpp_TypeSpecifier_ElaboratedTypeSpecifier const * const type_specifier__elaborated_type_specifier =
			static_cast <Cpp_TypeSpecifier_ElaboratedTypeSpecifier const *> (type_specifier);

		Cpp_ElaboratedTypeSpecifier const * const elaborated_type_specifier =
			type_specifier__elaborated_type_specifier->elaboratedTypeSpecifier;
		abortIf (elaborated_type_specifier == NULL);

		switch (elaborated_type_specifier->elaborated_type_specifier_type) {
		    case Cpp_ElaboratedTypeSpecifier::t_Class: {
//#error TODO cpp_parser
#if 0
			Cpp_ElaboratedTypeSpecifier_Class const * const elaborated_type_specifier__class =
				static_cast <Cpp_ElaboratedTypeSpecifier_Class const *> (elaborated_type_specifier);

			ConstMemoryDesc identifier_mem = elaborated_type_specifier__class->identifier->any_token->token;
			Cpp::Member * const member = cpp_parser.lookupType (identifier_mem);
			if (member != NULL) {
			    abortIf (member->getType () != Cpp::Member::t_Type);

			    Cpp::Member_Type const * const member__type =
				    static_cast <Cpp::Member_Type const *> (member);

			    if (member__type->is_typedef) {
				errf->print ("Typedef in elaborated-type-specifier").pendl ();
				abortIfReached ();
			    }

			    abortIf (member->type_desc.isNull ());
			    switch (member->type_desc->getType ()) {
				case Cpp::TypeDesc::t_Class: {
				    Cpp::TypeDesc_Class const * const type_desc__class =
					    static_cast <Cpp::TypeDesc_Class const *> (member->type_desc.ptr ());

				    abortIf (type_desc__class->class_.isNull ());

				    class_ = type_desc__class->class_;
				    type_category = Class;
				} break;
				case Cpp::TypeDesc::t_Dependent: {
				    // TODO
				    abortIfReached ();
				} break;
				default: {
				    errf->print ("Bad member in elaborated-type-specifier").pendl ();
				    abortIfReached ();
				}
			    }
			} else {
			  // Forward declaration

			    Cpp::Container * const container = cpp_parser.getContainerForNestedName (NULL /* ret_fixed_namespace */);

			    Ref<Cpp::Member> member = cpp_parser.addClass (declaration_state, container, identifier_mem);
			    cpp_parser.setDeclarationTypeDesc (declaration_state, member->type_desc);

			    abortIf (member->type_desc.isNull () ||
				     member->type_desc->getType () != Cpp::TypeDesc::t_Class);
			    Cpp::TypeDesc_Class const * const type_desc__class =
				    static_cast <Cpp::TypeDesc_Class*> (member->type_desc.ptr ());

			    class_ = type_desc__class->class_;
			    type_category = Class;
			}
#endif
		    } break;
		    case Cpp_ElaboratedTypeSpecifier::t_Enum: {
//			Cpp_ElaboratedTypeSpecifier_Enum const * const elaborated_type_specifier__enum =
//				static_cast <Cpp_ElaboratedTypeSpecifier_Enum const *> (elaborated_type_specifier);

			// TODO
			abortIfReached ();
		    } break;
		    case Cpp_ElaboratedTypeSpecifier::t_Typename: {
//			Cpp_ElaboratedTypeSpecifier_Typename const * const elaborated_type_specifier__typename =
//				static_cast <Cpp_ElaboratedTypeSpecifier_Typename const *> (elaborated_type_specifier);

			// TODO
			abortIfReached ();
		    } break;
		    case Cpp_ElaboratedTypeSpecifier::t_TypenameTemplate: {
//			Cpp_ElaboratedTypeSpecifier_TypenameTemplate const * const elaborated_type_specifier__typename_template =
//				static_cast <Cpp_ElaboratedTypeSpecifier_TypenameTemplate const *> (elaborated_type_specifier);

			// TODO
			abortIfReached ();
		    } break;
		    default:
			break;
		}
	    } break;
	    case Cpp_TypeSpecifier::t_CvQualifier: {
		Cpp_TypeSpecifier_CvQualifier const * const type_specifier__cv_qualifier =
			static_cast <Cpp_TypeSpecifier_CvQualifier const *> (type_specifier);
		Cpp_CvQualifier const * const cv_qualifier =
			type_specifier__cv_qualifier->cvQualifier;

		switch (cv_qualifier->cv_qualifier_type) {
		    case Cpp_CvQualifier::t_Const: {
			if (is_const) {
			    errf->print (_func_name).print (": duplicate \"const\"").pendl ();
			    abortIfReached ();
			}

			is_const = true;
		    } break;
		    case Cpp_CvQualifier::t_Volatile: {
			if (is_volatile) {
			    errf->print (_func_name).print (": duplicate \"volatile\"").pendl ();
			    abortIfReached ();
			}

			is_volatile = true;
		    } break;
		    default:
			abortIfReached ();
		}
	    } break;
	    default:
		abortIfReached ();
	}
    }

    void
    TypeSpecifierParser::parseSimpleTypeSpecifier (Cpp_SimpleTypeSpecifier const * const simple_type_specifier)
    {
	static char const * const _func_name = "Scruffy.CppParser.TypeSpecifierParser.parseSimpleTypeSpecifier";

	switch (simple_type_specifier->simple_type_specifier_type) {
	    case Cpp_SimpleTypeSpecifier::t_Void: {
		if (type_category != Unknown) {
		    errf->print (_func_name).print (": more than one data type").pendl ();
		    abortIfReached ();
		}

		type_category = BasicType;
		basic_type = Cpp::TypeDesc_BasicType::Void;
	    } break;
	    case Cpp_SimpleTypeSpecifier::t_Bool: {
		if (type_category != Unknown) {
		    errf->print (_func_name).print (": more than one data type").pendl ();
		    abortIfReached ();
		}

		type_category = BasicType;
		basic_type = Cpp::TypeDesc_BasicType::Bool;
	    } break;
	    case Cpp_SimpleTypeSpecifier::t_Int: {
		if (type_category != Unknown &&
		    type_category != BasicType)
		{
		    errf->print (_func_name).print (": more than one data type").pendl ();
		    abortIfReached ();
		}

		if (has_int) {
		    errf->print (_func_name).print (": more than one data type").pendl ();
		    abortIfReached ();
		}

		if (type_category == BasicType) {
		    switch (basic_type) {
			case Cpp::TypeDesc_BasicType::Int:
			case Cpp::TypeDesc_BasicType::UnsignedInt:
			case Cpp::TypeDesc_BasicType::ShortInt:
			case Cpp::TypeDesc_BasicType::LongInt:
			case Cpp::TypeDesc_BasicType::UnsignedShortInt:
			case Cpp::TypeDesc_BasicType::UnsignedLongInt:
			    break;
			default:
			    errf->print (_func_name).print (": more than one data type").pendl ();
			    abortIfReached ();
		    }
		} else {
		    abortIf (type_category != Unknown);
		    type_category = BasicType;
		    basic_type = Cpp::TypeDesc_BasicType::Int;
		}

		has_int = true;
	    } break;
	    case Cpp_SimpleTypeSpecifier::t_Char: {
		if (type_category != Unknown &&
		    type_category != BasicType)
		{
		    errf->print (_func_name).print (": more than one data type").pendl ();
		    abortIfReached ();
		}

		if (has_int) {
		    errf->print (_func_name).print (": more than one data type").pendl ();
		    abortIfReached ();
		}

		if (type_category == BasicType) {
		    switch (basic_type) {
			case Cpp::TypeDesc_BasicType::Int: {
			    basic_type = Cpp::TypeDesc_BasicType::SignedChar;
			} break;
			case Cpp::TypeDesc_BasicType::UnsignedInt: {
			    basic_type = Cpp::TypeDesc_BasicType::UnsignedChar;
			} break;
			default:
			    errf->print (_func_name).print (": more than one data type").pendl ();
			    abortIfReached ();
		    }
		} else {
		    abortIf (type_category != Unknown);
		    type_category = BasicType;
		    basic_type = Cpp::TypeDesc_BasicType::Char;
		}
	    } break;
	    case Cpp_SimpleTypeSpecifier::t_Unsigned: {
		if (type_category != Unknown &&
		    type_category != BasicType)
		{
		    errf->print (_func_name).print (": more than one data type").pendl ();
		    abortIfReached ();
		}

		if (has_signed || has_unsigned) {
		    errf->print (_func_name).print (": more than one data type").pendl ();
		    abortIfReached ();
		}

		if (type_category == BasicType) {
		    switch (basic_type) {
			case Cpp::TypeDesc_BasicType::Char: {
			    basic_type = Cpp::TypeDesc_BasicType::UnsignedChar;
			} break;
			case Cpp::TypeDesc_BasicType::Int: {
			    basic_type = Cpp::TypeDesc_BasicType::UnsignedInt;
			} break;
			case Cpp::TypeDesc_BasicType::ShortInt: {
			    basic_type = Cpp::TypeDesc_BasicType::UnsignedShortInt;
			} break;
			case Cpp::TypeDesc_BasicType::LongInt: {
			    basic_type = Cpp::TypeDesc_BasicType::UnsignedLongInt;
			} break;
			default:
			    errf->print (_func_name).print (": more than one data type").pendl ();
			    abortIfReached ();
		    }
		} else {
		    abortIf (type_category != Unknown);
		    type_category = BasicType;
		    basic_type = Cpp::TypeDesc_BasicType::UnsignedInt;
		}

		has_unsigned = true;
	    } break;
	    case Cpp_SimpleTypeSpecifier::t_Long: {
		if (type_category != Unknown &&
		    type_category != BasicType)
		{
		    errf->print (_func_name).print (": more than one data type").pendl ();
		    abortIfReached ();
		}

		if (type_category == BasicType) {
		    switch (basic_type) {
		      // TODO LongLong
			case Cpp::TypeDesc_BasicType::Int: {
			    basic_type = Cpp::TypeDesc_BasicType::LongInt;
			} break;
			case Cpp::TypeDesc_BasicType::UnsignedInt: {
			    basic_type = Cpp::TypeDesc_BasicType::UnsignedLongInt;
			} break;
			case Cpp::TypeDesc_BasicType::Double: {
			    basic_type = Cpp::TypeDesc_BasicType::LongDouble;
			} break;
			default:
			    errf->print (_func_name).print (": more than one data type").pendl ();
			    abortIfReached ();
		    }
		} else {
		    abortIf (type_category != Unknown);
		    type_category = BasicType;
		    basic_type = Cpp::TypeDesc_BasicType::LongInt;
		}
	    } break;
	    case Cpp_SimpleTypeSpecifier::t_Float: {
		if (type_category != Unknown) {
		    errf->print (_func_name).print (": more than one data type").pendl ();
		    abortIfReached ();
		}

		type_category = BasicType;
		basic_type = Cpp::TypeDesc_BasicType::Float;
	    } break;
	    case Cpp_SimpleTypeSpecifier::t_Double: {
		if (type_category != Unknown &&
		    type_category != BasicType)
		{
		    errf->print (_func_name).print (": more than one data type").pendl ();
		    abortIfReached ();
		}

		if (has_int) {
		    errf->print (_func_name).print (": more than one data type").pendl ();
		    abortIfReached ();
		}

		if (type_category == BasicType) {
		    switch (basic_type) {
			case Cpp::TypeDesc_BasicType::LongInt: {
			    basic_type = Cpp::TypeDesc_BasicType::LongDouble;
			} break;
			default:
			    errf->print (_func_name).print (": more than one data type").pendl ();
			    abortIfReached ();
		    }
		} else {
		    abortIf (type_category != Unknown);
		    type_category = BasicType;
		    basic_type = Cpp::TypeDesc_BasicType::Double;
		}
	    } break;
	    case Cpp_SimpleTypeSpecifier::t_Short: {
		// TODO
		abortIfReached ();
	    } break;
	    case Cpp_SimpleTypeSpecifier::t_WcharT: {
		// TODO
		abortIfReached ();
	    } break;
	    case Cpp_SimpleTypeSpecifier::t_Signed: {
		if (type_category != Unknown &&
		    type_category != BasicType)
		{
		    errf->print (_func_name).print (": more than one data type").pendl ();
		    abortIfReached ();
		}

		if (has_signed || has_unsigned) {
		    errf->print (_func_name).print (": more than one data type").pendl ();
		    abortIfReached ();
		}

		if (type_category == BasicType) {
		    switch (basic_type) {
			case Cpp::TypeDesc_BasicType::Char: {
			    basic_type = Cpp::TypeDesc_BasicType::SignedChar;
			} break;
			case Cpp::TypeDesc_BasicType::Int: {
			    basic_type = Cpp::TypeDesc_BasicType::Int;
			} break;
			case Cpp::TypeDesc_BasicType::ShortInt: {
			    basic_type = Cpp::TypeDesc_BasicType::ShortInt;
			} break;
			case Cpp::TypeDesc_BasicType::LongInt: {
			    basic_type = Cpp::TypeDesc_BasicType::LongInt;
			} break;
			default:
			    errf->print (_func_name).print (": more than one data type").pendl ();
			    abortIfReached ();
		    }
		} else {
		    abortIf (type_category != Unknown);
		    type_category = BasicType;
		    basic_type = Cpp::TypeDesc_BasicType::Int;
		}

		has_signed = true;
	    } break;
	    case Cpp_SimpleTypeSpecifier::t_TypeName: {
		if (type_category != Unknown) {
		    errf->print (_func_name).print (": more than one data type").pendl ();
		    abortIfReached ();
		}

		Cpp_SimpleTypeSpecifier_TypeName const * const simple_type_specifier__nested_type =
			static_cast <Cpp_SimpleTypeSpecifier_TypeName const *> (simple_type_specifier);

		Cpp_TypeName const * const type_name =
			simple_type_specifier__nested_type->typeName;
		switch (type_name->type_name_type) {
		    case Cpp_TypeName::t_ClassName: {
			Cpp_TypeName_ClassName const * const type_name__class_name =
				static_cast <Cpp_TypeName_ClassName const *> (type_name);

			Cpp_ClassName const * const class_name =
				type_name__class_name->className;

			switch (class_name->class_name_type) {
			    case Cpp_ClassName::t_ClassNameIdentifier: {
				Cpp_ClassName_ClassNameIdentifier const * const class_name__class_name_identifier =
					static_cast <Cpp_ClassName_ClassNameIdentifier const *> (class_name);

				Cpp_ClassNameIdentifier const *class_name_identifier =
					class_name__class_name_identifier->classNameIdentifier;

				abortIf (class_name_identifier->user_obj == NULL);
				Cpp::TypeDesc_Class * const type_desc__class =
					static_cast <Cpp::TypeDesc_Class*> (class_name_identifier->user_obj);

				type_category = Class;
				class_ = type_desc__class->class_;

				simple_type_specifier_used = type_desc__class->simple_type_specifier_used;
			    } break;
			    case Cpp_ClassName::t_TemplateId: {
				// TODO
				abortIfReached ();
			    } break;
			    default:
				abortIfReached ();
			}
		    } break;
		    case Cpp_TypeName::t_EnumName: {
			errf->print ("--- ENUM-NAME").pendl ();
			// TODO
			abortIfReached ();
		    } break;
		    case Cpp_TypeName::t_TypedefName: {
			Cpp_TypeName_TypedefName const * const type_name__typedef_name =
				static_cast <Cpp_TypeName_TypedefName const *> (type_name);

			Cpp_TypedefName const * const typedef_name =
				type_name__typedef_name->typedefName;

			abortIf (typedef_name->user_obj == NULL);
			Ref<Cpp::TypeDesc> type_desc =
				static_cast <Cpp::TypeDesc*> (typedef_name->user_obj);

			type_category = Typedef;
			typedef__type_desc = type_desc;
		    } break;
		    default:
			abortIfReached ();
		}
	    } break;
	    case Cpp_SimpleTypeSpecifier::t_TemplateId: {
		if (type_category != Unknown) {
		    errf->print (_func_name).print (": more than one data type").pendl ();
		    abortIfReached ();
		}

		// TODO
		abortIfReached ();
	    } break;
	    default:
		abortIfReached ();
	}
    }

    Ref<Cpp::TypeDesc>
    TypeSpecifierParser::createTypeDesc ()
    {
	Ref<Cpp::TypeDesc> type_desc;

	switch (type_category) {
	    case BasicType: {
		Ref<Cpp::TypeDesc_BasicType> type_desc__basic_type =
			grab (new Cpp::TypeDesc_BasicType);
		type_desc__basic_type->basic_type = basic_type;

		type_desc = type_desc__basic_type;
	    } break;
	    case Class: {
		Ref<Cpp::TypeDesc_Class> type_desc__class =
			grab (new Cpp::TypeDesc_Class);
		type_desc__class->class_ = class_;

		type_desc = type_desc__class;

		type_desc->simple_type_specifier_used = simple_type_specifier_used;
	    } break;
	    case Template: {
		// TODO
		abortIfReached ();
	    } break;
	    case Enum: {
		Ref<Cpp::TypeDesc_Enum> type_desc__enum =
			grab (new Cpp::TypeDesc_Enum);
		type_desc__enum->enum_ = enum_;

		type_desc = type_desc__enum;
	    } break;
	    case Typedef: {
		type_desc = typedef__type_desc;
	    } break;
	    default:
		abortIfReached ();
	}

	abortIf (type_desc.isNull ());

	type_desc->is_const = is_const;
	type_desc->is_volatile = is_volatile;

	return type_desc;
    }

    void
    TypeSpecifierParser::fillDeclarationState (DeclarationState * const declaration_state /* non-null */)
    {
      // TODO: is it necessary to remember these?
      // (Not all of these are necessary. Some refer to types/return types.)
#if 0
	declaration_state->is_const = is_const;
	declaration_state->is_volatile = is_volatile;
#endif

	declaration_state->is_auto = is_auto;
	declaration_state->is_register = is_register;
	declaration_state->is_static = is_static;
	declaration_state->is_extern = is_extern;
	declaration_state->is_mutable = is_mutable;

	declaration_state->is_inline = is_inline;
	declaration_state->is_virtual = is_virtual;
	declaration_state->is_explicit = is_explicit;

	declaration_state->is_friend = is_friend;
	declaration_state->is_typedef = is_typedef;

      // Creating initial TypeDesc for the declaration.

	declaration_state->type_desc = createTypeDesc ();
	abortIf (declaration_state->type_desc.isNull ());
    }

} // namespace {}

#if 0
// Deprecated
static void
cpp_parser_parse_type_specifier_seq (CppParser_Impl             * const self,
				     Cpp_TypeSpecifierSeq const * const type_specifier_seq)
{
//    static char const * const _func_name = "cpp_parser_parse_type_specifier_seq";

    if (type_specifier_seq == NULL)
	return;

    DEBUG (
	dump_cpp_TypeSpecifierSeq (type_specifier_seq);
    )

    abortIf (self->declaration_states.isEmpty ());
    CppParser_Impl::DeclarationState &declaration_state = self->declaration_states.last->data;

    TypeSpecifierParser type_specifier_parser (*self);

    Cpp_TypeSpecifierSeq const *cur_type_specifier_seq = type_specifier_seq;
    while (cur_type_specifier_seq != NULL) {
	Cpp_TypeSpecifier const * const type_specifier = cur_type_specifier_seq->typeSpecifier;

	type_specifier_parser.parseTypeSpecifier (type_specifier, declaration_state);

	cur_type_specifier_seq = cur_type_specifier_seq->typeSpecifierSeq;
    }

    declaration_state.type_desc = type_specifier_parser.createTypeDesc ();
    abortIf (declaration_state.type_desc.isNull ());
}
#endif

// DONE Implement complete functionality of cpp_parser_parse_decl_specifier_seq()
#if 0
// Deprecated
static void
cpp_parser_parse_decl_specifier_seq (CppParser_Impl             * const self,
				     Cpp_DeclSpecifierSeq const * const decl_specifier_seq)
{
    static char const * const _func_name = "cpp_parser_parse_decl_specifier_seq";

    if (decl_specifier_seq == NULL)
	return;

    DEBUG (
	dump_cpp_DeclSpecifierSeq (decl_specifier_seq);
    )

    abortIf (self->declaration_states.isEmpty ());
    CppParser_Impl::DeclarationState &declaration_state = self->declaration_states.last->data;

    TypeSpecifierParser type_specifier_parser (*self);

    Bool is_auto,
	 is_register,
	 is_static,
	 is_extern,
	 is_mutable;

    Bool is_inline,
	 is_virtual,
	 is_explicit;

    Bool is_friend,
	 is_typedef;

    Cpp_DeclSpecifierSeq const *cur_decl_specifier_seq = decl_specifier_seq;
    while (cur_decl_specifier_seq != NULL) {
	Cpp_DeclSpecifier const * const decl_specifier = cur_decl_specifier_seq->declSpecifier;

	DEBUG (
	    errf->print ("--- decl-specifier").pendl ();
	)

	switch (decl_specifier->decl_specifier_type) {
	    case Cpp_DeclSpecifier::t_StorageClassSpecifier: {
		Cpp_DeclSpecifier_StorageClassSpecifier const * const decl_specifier__storage_class_specifier =
			static_cast <Cpp_DeclSpecifier_StorageClassSpecifier const *> (decl_specifier);
		Cpp_StorageClassSpecifier const * const storage_class_specifier =
			decl_specifier__storage_class_specifier->storageClassSpecifier;

		switch (storage_class_specifier->storage_class_specifier_type) {
		    case Cpp_StorageClassSpecifier::t_Auto: {
			if (is_auto) {
			    errf->print (_func_name).print (": duplicate \"auto\"").pendl ();
			    abortIfReached ();
			}

			is_auto = true;
		    } break;
		    case Cpp_StorageClassSpecifier::t_Register: {
			if (is_register) {
			    errf->print (_func_name).print (": duplicate \"register\"").pendl ();
			    abortIfReached ();
			}

			is_register = true;
		    } break;
		    case Cpp_StorageClassSpecifier::t_Static: {
			if (is_static) {
			    errf->print (_func_name).print (": duplicate \"static\"").pendl ();
			    abortIfReached ();
			}

			is_static = true;
		    } break;
		    case Cpp_StorageClassSpecifier::t_Extern: {
			if (is_extern) {
			    errf->print (_func_name).print (": duplicate \"extern\"").pendl ();
			    abortIfReached ();
			}

			is_extern = true;
		    } break;
		    case Cpp_StorageClassSpecifier::t_Mutable: {
			if (is_mutable) {
			    errf->print (_func_name).print (": duplicate \"mutable\"").pendl ();
			    abortIfReached ();
			}

			is_mutable = true;
		    } break;
		    default:
			abortIfReached ();
		}
	    } break;
	    case Cpp_DeclSpecifier::t_TypeSpecifier: {
		Cpp_DeclSpecifier_TypeSpecifier const * const decl_specifier__type_specifier =
			static_cast <Cpp_DeclSpecifier_TypeSpecifier const *> (decl_specifier);
		Cpp_TypeSpecifier const * const type_specifier =
			decl_specifier__type_specifier->typeSpecifier;

		type_specifier_parser.parseTypeSpecifier (type_specifier, declaration_state);
	    } break;
	    case Cpp_DeclSpecifier::t_FunctionSpecifier: {
		Cpp_DeclSpecifier_FunctionSpecifier const * const decl_specifier__function_specifier =
			static_cast <Cpp_DeclSpecifier_FunctionSpecifier const *> (decl_specifier);
		Cpp_FunctionSpecifier const * const function_specifier =
			decl_specifier__function_specifier->functionSpecifier;

		switch (function_specifier->function_specifier_type) {
		    case Cpp_FunctionSpecifier::t_Inline: {
			if (is_inline) {
			    errf->print (_func_name).print (": duplicate \"inline\"").pendl ();
			    abortIfReached ();
			}

			is_inline = true;
		    } break;
		    case Cpp_FunctionSpecifier::t_Virtual: {
			if (is_virtual) {
			    errf->print (_func_name).print (": duplicate \"virtual\"").pendl ();
			    abortIfReached ();
			}

			is_virtual = true;
		    } break;
		    case Cpp_FunctionSpecifier::t_Explicit: {
			if (is_explicit) {
			    errf->print (_func_name).print (": duplicate \"explicit\"").pendl ();
			    abortIfReached ();
			}

			is_explicit = true;
		    } break;
		    default:
			abortIfReached ();
		}
	    } break;
	    case Cpp_DeclSpecifier::t_Friend: {
		if (is_friend) {
		    errf->print (_func_name).print (": duplicate \"friend\"").pendl ();
		    abortIfReached ();
		}

		is_friend = true;
	    } break;
	    case Cpp_DeclSpecifier::t_Typedef: {
		if (is_typedef) {
		    errf->print (_func_name).print (": duplicate \"typedef\"").pendl ();
		    abortIfReached ();
		}

		is_typedef = true;
	    } break;
	    default:
		abortIfReached ();
	}

	cur_decl_specifier_seq = cur_decl_specifier_seq->declSpecifierSeq;
    }

  // TODO: is it necessary to remember these?
  // (Not all of these are necessary. Some refer to types/return types.)
#if 0
    declaration_state.is_const = is_const;
    declaration_state.is_volatile = is_volatile;
#endif

    declaration_state.is_auto = is_auto;
    declaration_state.is_register = is_register;
    declaration_state.is_static = is_static;
    declaration_state.is_extern = is_extern;
    declaration_state.is_mutable = is_mutable;

    declaration_state.is_inline = is_inline;
    declaration_state.is_virtual = is_virtual;
    declaration_state.is_explicit = is_explicit;

    declaration_state.is_friend = is_friend;
    declaration_state.is_typedef = is_typedef;

  // Creating initial TypeDesc for the declaration.

    declaration_state.type_desc = type_specifier_parser.createTypeDesc ();
    abortIf (declaration_state.type_desc.isNull ());
}
#endif

static void 
cpp_do_begin_declaration (void * const _self,
			  bool   const is_parameter)
{
  FUNC_NAME (
    static char const * const _func_name = "Scruffy.(CppParser_Impl).cpp_do_begin_declaration";
  )

    DEBUG (
	errf->print (_func_name).pendl ();
    )

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    self->declaration_states.appendEmpty ();
    DeclarationState &declaration_state = self->declaration_states.getLast ();

    declaration_state.is_parameter = is_parameter;
    declaration_state.checkpoint_key = self->checkpoint_tracker.getCurrentCheckpoint ();

    abortIf (self->declaration_acceptors.isEmpty ());
    declaration_state.declaration_acceptor = self->declaration_acceptors.last->data;

    self->checkpoint_tracker.addUnconditionalCancellable (
	    grab (new Cancellable_ListElement<DeclarationState> (
			  self->declaration_states,
			  self->declaration_states.last)));
}

bool
cpp_begin_declaration (CppElement    * const /* cnst cpp_el */,
		       ParserControl * const /* const parser_control */,
		       void          * const _self)
{
    cpp_do_begin_declaration (_self, false /* is_parameter */);
    return true;
}

// TODO Same as cpp_begin_declaration
bool
cpp_begin_parameter_declaration (CppElement    * const /* cnst cpp_el */,
				 ParserControl * const /* const parser_control */,
				 void          * const _self)
{
    cpp_do_begin_declaration (_self, true /* is_parameter */);
    return true;
}

bool
cpp_accept_decl_specifier__storage_class_specifier (Cpp_DeclSpecifier * const decl_specifier,
						    ParserControl     * const /* parser_control */,
						    void              * const _self)
{
    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    DeclarationState &declaration_state = self->declaration_states.getLast ();

    abortIf (decl_specifier->decl_specifier_type != Cpp_DeclSpecifier::t_StorageClassSpecifier);
    Cpp_DeclSpecifier_StorageClassSpecifier * const decl_specifier__storage_class_specifier =
	    static_cast <Cpp_DeclSpecifier_StorageClassSpecifier*> (decl_specifier);

    switch (decl_specifier__storage_class_specifier->storageClassSpecifier->storage_class_specifier_type) {
	case Cpp_StorageClassSpecifier::t_Auto: {
	    declaration_state.type_specifier_parser.setAuto ();
	} break;
	case Cpp_StorageClassSpecifier::t_Register: {
	    declaration_state.type_specifier_parser.setRegister ();
	} break;
	case Cpp_StorageClassSpecifier::t_Static: {
	    declaration_state.type_specifier_parser.setStatic ();
	} break;
	case Cpp_StorageClassSpecifier::t_Extern: {
	    declaration_state.type_specifier_parser.setExtern ();
	} break;
	case Cpp_StorageClassSpecifier::t_Mutable: {
	    declaration_state.type_specifier_parser.setMutable ();
	} break;
	default:
	    abortIfReached ();
    }

    return true;
}

bool
cpp_accept_decl_specifier__function_specifier (Cpp_DeclSpecifier * const decl_specifier,
					       ParserControl     * const /* parser_control */,
					       void              * const _self)
{
    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    DeclarationState &declaration_state = self->declaration_states.getLast ();

    abortIf (decl_specifier->decl_specifier_type != Cpp_DeclSpecifier::t_FunctionSpecifier);
    Cpp_DeclSpecifier_FunctionSpecifier * const decl_specifier__function_specifier =
	    static_cast <Cpp_DeclSpecifier_FunctionSpecifier*> (decl_specifier);

    switch (decl_specifier__function_specifier->functionSpecifier->function_specifier_type) {
	case Cpp_FunctionSpecifier::t_Inline: {
	    declaration_state.type_specifier_parser.setInline ();
	} break;
	case Cpp_FunctionSpecifier::t_Virtual: {
	    declaration_state.type_specifier_parser.setVirtual ();
	} break;
	case Cpp_FunctionSpecifier::t_Explicit: {
	    declaration_state.type_specifier_parser.setExplicit ();
	} break;
	default:
	    abortIfReached ();
    }

    return true;
}

bool
cpp_accept_decl_specifier__friend (Cpp_DeclSpecifier * const /* decl_specifier */,
				   ParserControl     * const /* parser_control */,
				   void              * const _self)
{
    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    DeclarationState &declaration_state = self->declaration_states.getLast ();

    declaration_state.type_specifier_parser.setFriend ();

    return true;
}

bool
cpp_accept_decl_specifier__typedef (Cpp_DeclSpecifier * const /* decl_specifier */,
				    ParserControl     * const /* parser_control */,
				    void              * const _self)
{
    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    DeclarationState &declaration_state = self->declaration_states.getLast ();

    declaration_state.type_specifier_parser.setTypedef ();

    return true;
}

bool
cpp_DeclSpecifier_match_func (Cpp_DeclSpecifier * const /* decl_specifier_ */,
			      ParserControl     * const /* parser_control */,
			      void              * const /* _self */)
{
#if 0
// This is wrong: works for constructors, but doesn't work for "int (*a) ()".

    switch (decl_specifier_->decl_specifier_type) {
	case Cpp_DeclSpecifier::t_StorageClassSpecifier: {
	    Cpp_DeclSpecifier_StorageClassSpecifier const * const decl_specifier =
		    static_cast <Cpp_DeclSpecifier_StorageClassSpecifier const *> (decl_specifier_);

	    if (!decl_specifier->scruffy_Test_GlobalScope.isNull () ||
		!decl_specifier->scruffy_Test_OpeningParenthesis.isNull ())
	    {
		return false;
	    }
	} break;
	case Cpp_DeclSpecifier::t_TypeSpecifier: {
	    Cpp_DeclSpecifier_TypeSpecifier const * const decl_specifier =
		    static_cast <Cpp_DeclSpecifier_TypeSpecifier const *> (decl_specifier_);

	    if (!decl_specifier->scruffy_Test_GlobalScope.isNull () ||
		!decl_specifier->scruffy_Test_OpeningParenthesis.isNull ())
	    {
		return false;
	    }
	} break;
	case Cpp_DeclSpecifier::t_FunctionSpecifier: {
	    Cpp_DeclSpecifier_FunctionSpecifier const * const decl_specifier =
		    static_cast <Cpp_DeclSpecifier_FunctionSpecifier const *> (decl_specifier_);

	    if (!decl_specifier->scruffy_Test_GlobalScope.isNull () ||
		!decl_specifier->scruffy_Test_OpeningParenthesis.isNull ())
	    {
		return false;
	    }
	} break;
	case Cpp_DeclSpecifier::t_Friend: {
	    Cpp_DeclSpecifier_Friend const * const decl_specifier =
		    static_cast <Cpp_DeclSpecifier_Friend const *> (decl_specifier_);

	    if (!decl_specifier->scruffy_Test_GlobalScope.isNull () ||
		!decl_specifier->scruffy_Test_OpeningParenthesis.isNull ())
	    {
		return false;
	    }
	} break;
	case Cpp_DeclSpecifier::t_Typedef: {
	    Cpp_DeclSpecifier_Typedef const * const decl_specifier =
		    static_cast <Cpp_DeclSpecifier_Typedef const *> (decl_specifier_);

	    if (!decl_specifier->scruffy_Test_GlobalScope.isNull () ||
		!decl_specifier->scruffy_Test_OpeningParenthesis.isNull ())
	    {
		return false;
	    }
	} break;
	default:
	    abortIfReached ();
    }
#endif

    return true;
}

void
CppParser_Impl::beginDeclSpecifierSeq ()
{
//    decl_specifier
//
//    Unnecessary?
}

void
cpp_Scruffy_DeclSpecifierSeq_begin_func (void * const _self)
{
    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    self->beginDeclSpecifierSeq ();
}

void
cpp_Scruffy_DeclSpecifierSeq_accept_func (Cpp_DeclSpecifierSeq * const decl_specifier_seq,
					  ParserControl        * const /* parser_control */,
					  void                 * const _self)
{
    if (decl_specifier_seq == NULL)
	return;

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    DeclarationState &declaration_state = self->declaration_states.getLast ();

    // Finalizing decl-specifier-seq
// Deprecated    declaration_state.type_desc = declaration_state.type_specifier_parser.createTypeDesc ();
    declaration_state.type_specifier_parser.fillDeclarationState (&declaration_state);
}

void
cpp_Scruffy_TypeSpecifierSeq_begin_func (void * const _self)
{
    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    self->beginDeclSpecifierSeq ();
}

void
cpp_Scruffy_TypeSpecifierSeq_accept_func (Cpp_TypeSpecifierSeq * const type_specifier_seq,
					  ParserControl        * const /* parser_control */,
					  void                 * const _self)
{
    if (type_specifier_seq == NULL)
	return;

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    DeclarationState &declaration_state = self->declaration_states.getLast ();

    // Finalizing type-specifier-seq
    declaration_state.type_desc = declaration_state.type_specifier_parser.createTypeDesc ();
}

static Ref<Cpp::Name>
cpp_parser_name_for_identifier (Cpp_Identifier const * const identifier)
{
    abortIf (identifier == NULL ||
	     identifier->any_token == NULL);

    Ref<Cpp::Name> name = grab (new Cpp::Name);
    // FIXME Optimize this (no new): class Name should not require the memory
    // for the name string to be allocated on the heap.
    name->name_str = grab (new String (identifier->any_token->token));
    return name;
}

static Ref<Cpp::Name>
cpp_parser_name_for_operator_function_id (Cpp_OperatorFunctionId const * const operator_function_id)
{
    abortIf (operator_function_id == NULL);

    Ref<Cpp::Name> name = grab (new Cpp::Name);

    name->is_operator = true;

    switch (operator_function_id->_operator->operator_type) {
	case Cpp_Operator::t_New: {
	    name->operator_type = Cpp::Name::OperatorType::New;
	} break;
	case Cpp_Operator::t_Delete: {
	    name->operator_type = Cpp::Name::OperatorType::Delete;
	} break;
	case Cpp_Operator::t_NewArray: {
	    name->operator_type = Cpp::Name::OperatorType::NewArray;
	} break;
	case Cpp_Operator::t_DeleteArray: {
	    name->operator_type = Cpp::Name::OperatorType::DeleteArray;
	} break;
	case Cpp_Operator::t_Plus: {
	    name->operator_type = Cpp::Name::OperatorType::Plus;
	} break;
	case Cpp_Operator::t_Minus: {
	    name->operator_type = Cpp::Name::OperatorType::Minus;
	} break;
	case Cpp_Operator::t_Multiply: {
	    name->operator_type = Cpp::Name::OperatorType::Multiply;
	} break;
	case Cpp_Operator::t_Divide: {
	    name->operator_type = Cpp::Name::OperatorType::Divide;
	} break;
	case Cpp_Operator::t_Remainder: {
	    name->operator_type = Cpp::Name::OperatorType::Remainder;
	} break;
	case Cpp_Operator::t_ExclusiveOr: {
	    name->operator_type = Cpp::Name::OperatorType::ExclusiveOr;
	} break;
	case Cpp_Operator::t_And: {
	    name->operator_type = Cpp::Name::OperatorType::And;
	} break;
	case Cpp_Operator::t_InclusiveOr: {
	    name->operator_type = Cpp::Name::OperatorType::InclusiveOr;
	} break;
	case Cpp_Operator::t_Inversion: {
	    name->operator_type = Cpp::Name::OperatorType::Inversion;
	} break;
	case Cpp_Operator::t_Not: {
	    name->operator_type = Cpp::Name::OperatorType::Not;
	} break;
	case Cpp_Operator::t_Set: {
	    name->operator_type = Cpp::Name::OperatorType::Set;
	} break;
	case Cpp_Operator::t_Less: {
	    name->operator_type = Cpp::Name::OperatorType::Less;
	} break;
	case Cpp_Operator::t_Greater: {
	    name->operator_type = Cpp::Name::OperatorType::Greater;
	} break;
	case Cpp_Operator::t_PlusSet: {
	    name->operator_type = Cpp::Name::OperatorType::PlusSet;
	} break;
	case Cpp_Operator::t_MinusSet: {
	    name->operator_type = Cpp::Name::OperatorType::MinusSet;
	} break;
	case Cpp_Operator::t_MultiplySet: {
	    name->operator_type = Cpp::Name::OperatorType::MultiplySet;
	} break;
	case Cpp_Operator::t_DivideSet: {
	    name->operator_type = Cpp::Name::OperatorType::DivideSet;
	} break;
	case Cpp_Operator::t_RemainderSet: {
	    name->operator_type = Cpp::Name::OperatorType::RemainderSet;
	} break;
	case Cpp_Operator::t_ExclusiveOrSet: {
	    name->operator_type = Cpp::Name::OperatorType::ExclusiveOrSet;
	} break;
	case Cpp_Operator::t_AndSet: {
	    name->operator_type = Cpp::Name::OperatorType::AndSet;
	} break;
	case Cpp_Operator::t_InclusiveOrSet: {
	    name->operator_type = Cpp::Name::OperatorType::InclusiveOrSet;
	} break;
	case Cpp_Operator::t_LeftShift: {
	    name->operator_type = Cpp::Name::OperatorType::LeftShift;
	} break;
	case Cpp_Operator::t_RightShift: {
	    name->operator_type = Cpp::Name::OperatorType::RightShift;
	} break;
	case Cpp_Operator::t_RightShiftSet: {
	    name->operator_type = Cpp::Name::OperatorType::RightShiftSet;
	} break;
	case Cpp_Operator::t_LeftShiftSet: {
	    name->operator_type = Cpp::Name::OperatorType::LeftShiftSet;
	} break;
	case Cpp_Operator::t_Equal: {
	    name->operator_type = Cpp::Name::OperatorType::Equal;
	} break;
	case Cpp_Operator::t_NotEqual: {
	    name->operator_type = Cpp::Name::OperatorType::NotEqual;
	} break;
	case Cpp_Operator::t_LessOrEqual: {
	    name->operator_type = Cpp::Name::OperatorType::LessOrEqual;
	} break;
	case Cpp_Operator::t_GreaterOrEqual: {
	    name->operator_type = Cpp::Name::OperatorType::GreaterOrEqual;
	} break;
	case Cpp_Operator::t_LogicalAnd: {
	    name->operator_type = Cpp::Name::OperatorType::LogicalAnd;
	} break;
	case Cpp_Operator::t_LogicalOr: {
	    name->operator_type = Cpp::Name::OperatorType::LogicalOr;
	} break;
	case Cpp_Operator::t_Increment: {
	    name->operator_type = Cpp::Name::OperatorType::Increment;
	} break;
	case Cpp_Operator::t_Decrement: {
	    name->operator_type = Cpp::Name::OperatorType::Decrement;
	} break;
	case Cpp_Operator::t_Comma: {
	    name->operator_type = Cpp::Name::OperatorType::Comma;
	} break;
	case Cpp_Operator::t_MemberDereference: {
	    name->operator_type = Cpp::Name::OperatorType::MemberDereference;
	} break;
	case Cpp_Operator::t_Dereference: {
	    name->operator_type = Cpp::Name::OperatorType::Dereference;
	} break;
	case Cpp_Operator::t_FunctionCall: {
	    name->operator_type = Cpp::Name::OperatorType::FunctionCall;
	} break;
	case Cpp_Operator::t_Subscripting: {
	    name->operator_type = Cpp::Name::OperatorType::Subscripting;
	} break;
	default:
	    abortIfReached ();
    }

    return name;
}

static Ref<Cpp::Name>
cpp_parser_name_for_template_id (Cpp_TemplateId const * const template_id /* non-null */)
{
    Cpp::TemplateInstance * const template_instance =
	    static_cast <Cpp::TemplateInstance*> (template_id->user_obj);

    Ref<Cpp::Name> name = grab (new Cpp::Name);
    name->name_str = grab (new String (template_id->templateName->identifier->any_token->token));

    // TODO Сохранять в Name как реальный (после раскрытия константных выражений),
    // так и фактический (до раскрытия) список аргументов шаблона.
    name->is_template = true;
    {
	List< Ref<Cpp::TemplateArgument> >::DataIterator arg_iter (template_instance->template_arguments);
	while (!arg_iter.done ()) {
	    Ref<Cpp::TemplateArgument> &template_argument = arg_iter.next ();
	    name->template_arguments.append (template_argument);
	}
    }

    return name;
}

static Ref<Cpp::Name>
cpp_parser_parse_unqualified_id (Cpp_UnqualifiedId const * const unqualified_id)
{
    switch (unqualified_id->unqualified_id_type) {
	case Cpp_UnqualifiedId::t_Identifier: {
	    Cpp_UnqualifiedId_Identifier const * const unqualified_id__identifier =
		    static_cast <Cpp_UnqualifiedId_Identifier const *> (unqualified_id);

	    return cpp_parser_name_for_identifier (unqualified_id__identifier->identifier);
	} break;
	case Cpp_UnqualifiedId::t_OperatorFunction: {
	    Cpp_UnqualifiedId_OperatorFunction const * const unqualified_id__operator_function =
		    static_cast <Cpp_UnqualifiedId_OperatorFunction const *> (unqualified_id);
	    Cpp_OperatorFunctionId const * const operator_function_id =
		    unqualified_id__operator_function->operatorFunctionId;

	    return cpp_parser_name_for_operator_function_id (operator_function_id);
	} break;
	case Cpp_UnqualifiedId::t_ConversionFunction: {
//			    Cpp_UnqualifiedId_ConversionFunction const * const unqualified_id__conversion_function =
//				    static_cast <Cpp_UnqualifiedId_ConversionFunction const *> (unqualified_id);
//			    Cpp_ConversionFunctionId const * const conversion_function_id =
//				    unqualified_id__conversion_function->conversionFunctionId;

//			    Cpp_ConversionTypeId const * const conversion_type_id =
//				    conversion_function_id->conversionTypeId;

	    // TODO Parse type-specifier-seq; parse ptr-operators; form a TypeDesc.
	    abortIfReached ();

//	    name->is_operator = true;
//	    name->is_conversion_operator = true;
	} break;
	case Cpp_UnqualifiedId::t_Destructor: {
//			    Cpp_UnqualifiedId_Destructor const * const unqualified_id__destructor =
//				    static_cast <Cpp_UnqualifiedId_Destructor const *> (unqualified_id);

//			    Cpp_ClassName const * const class_name =
//				    unqualified_id__destructor->className;

	    // TODO Parse class-name
	    abortIfReached ();

//	    name->is_destructor = true;
	} break;
	case Cpp_UnqualifiedId::t_Template: {
	    Cpp_UnqualifiedId_Template const * const unqualified_id__template =
		    static_cast <Cpp_UnqualifiedId_Template const *> (unqualified_id);

	    Cpp_TemplateId const * const template_id =
		    unqualified_id__template->templateId;

	    return cpp_parser_name_for_template_id (template_id);
	} break;
	default:
	    abortIfReached ();
    }

    abortIfReached ();
    return NULL;
}

static Ref<Cpp::Name>
cpp_parser_parse_qualified_id (Cpp_QualifiedId const * const qualified_id)
{
    switch (qualified_id->qualified_id_type) {
	case Cpp_QualifiedId::t_NestedNameSpecifier: {
	    Cpp_QualifiedId_NestedNameSpecifier const * const qualified_id__nested_name_specifier =
		    static_cast <Cpp_QualifiedId_NestedNameSpecifier const *> (qualified_id);

	    // TODO [template]_opt

	    return cpp_parser_parse_unqualified_id (qualified_id__nested_name_specifier->unqualifiedId);
	} break;
	case Cpp_QualifiedId::t_Identifier: {
	    Cpp_QualifiedId_Identifier const * const qualified_id__identifier =
		    static_cast <Cpp_QualifiedId_Identifier const *> (qualified_id);

	    return cpp_parser_name_for_identifier (qualified_id__identifier->identifier);
	} break;
	case Cpp_QualifiedId::t_OperatorFunction: {
	    Cpp_QualifiedId_OperatorFunction const * const qualified_id__operator_function =
		    static_cast <Cpp_QualifiedId_OperatorFunction const *> (qualified_id);

	    return cpp_parser_name_for_operator_function_id (qualified_id__operator_function->operatorFunctionId);
	} break;
	case Cpp_QualifiedId::t_Template: {
	    Cpp_QualifiedId_Template const * const qualified_id__template =
		    static_cast <Cpp_QualifiedId_Template const *> (qualified_id);

	    return cpp_parser_name_for_template_id (qualified_id__template->templateId);
	} break;
	default:
	    abortIfReached ();
    }

    abortIfReached ();
    return NULL;
}

static Ref<Cpp::Name>
cpp_parser_parse_id_expression (Cpp_IdExpression const * const id_expression)
{
    switch (id_expression->id_expression_type) {
	case Cpp_IdExpression::t_UnqualifiedId: {
	    Cpp_IdExpression_UnqualifiedId const * const id_expression__unqualified_id =
		    static_cast <Cpp_IdExpression_UnqualifiedId const *> (id_expression);
	    Cpp_UnqualifiedId const * const unqualified_id =
		    id_expression__unqualified_id->unqualifiedId;

	    return cpp_parser_parse_unqualified_id (unqualified_id);
	} break;
	case Cpp_IdExpression::t_QualifiedId: {
	    Cpp_IdExpression_QualifiedId const * const id_expression__qualified_id =
		    static_cast <Cpp_IdExpression_QualifiedId const *> (id_expression);
	    Cpp_QualifiedId const * const qualified_id =
		    id_expression__qualified_id->qualifiedId;

	    return cpp_parser_parse_qualified_id (qualified_id);
	} break;
	default:
	    abortIfReached ();
    } // switch (id_expression->id_expression_type)

    abortIfReached ();
    return NULL;
}

namespace {

class CppParser_DoParseDirectDeclarator_Data
{
public:
    List<FunctionDeclaratorState>::Element *cur_function_declarator_state_el;

    CppParser_DoParseDirectDeclarator_Data ()
	: cur_function_declarator_state_el (NULL)
    {
    }
};

}

static void cpp_parser_do_parse_declarator (CppParser_Impl  * const self,
					    DeclaratorState * const declarator_state,
					    Cpp_Declarator  * const declarator,
					    CppParser_DoParseDirectDeclarator_Data * const direct_data = NULL);

static void
cpp_parser_do_parse_direct_declarator (CppParser_Impl       * const self,
				       DeclaratorState      * const declarator_state,
				       Cpp_DirectDeclarator * const direct_declarator,
				       CppParser_DoParseDirectDeclarator_Data * const data /* non-null */)
{
    static char const * const _func_name =
	    "Scruffy.(CppParser_Impl).cpp_parser_do_parse_direct_declarator";

    abortIf (self == NULL             ||
	     declarator_state == NULL ||
	     direct_declarator == NULL);

    DeclarationState &declaration_state = self->declaration_states.getLast ();

    switch (direct_declarator->direct_declarator_type) {
	case Cpp_DirectDeclarator::t_Identifier: {
	    DEBUG (
		errf->print ("--- IDENTIFIER").pendl ();
	    )

	    Cpp_DirectDeclarator_Identifier const * const direct_declarator__identifier =
		    static_cast <Cpp_DirectDeclarator_Identifier const *> (direct_declarator);
	    abortIf (direct_declarator__identifier->declaratorId == NULL);
	    Cpp_DeclaratorId const * const declarator_id = direct_declarator__identifier->declaratorId;

	    abortIf (declarator_id->idExpression == NULL);
	    Cpp_IdExpression const * const id_expression = declarator_id->idExpression;

	    abortIf (!declarator_state->name.isNull ());
	    declarator_state->name = cpp_parser_parse_id_expression (id_expression);

#if 0
// TODO This should happen elsewhere
	    if (declaration_state.is_typedef) {
		errf->print ("--- TYPEDEF").pendl ();
		abortIf (name->name_str.isNull ());
		abortIf (name->is_destructor ||
			 name->is_operator   ||
			 name->is_conversion_operator ||
			 name->is_template);

		abortIf (declaration_state.type_desc.isNull ());
		errf->print ("--- TypeDesc: ").print (declaration_state.type_desc->toString ()).pendl ();

		self->addType (declaration_state.type_desc, name->name_str);
	    } else {
		errf->print ("--- NAME").pendl ();
// TODO
//		self->name_tracker->addName (&name);
	    }
#endif
	} break;
	case Cpp_DirectDeclarator::t_Function: {
	    DEBUG (
		errf->print ("--- FUNCTION").pendl ();
	    )

	    DEBUG (
		errf->print (_func_name).print (": direct_declarator: 0x").printHex ((Uint64) direct_declarator).print (", ")
			     .print ("user_obj: 0x").print ((Uint64) direct_declarator->user_obj).pendl ();
	    )

	    DEBUG (
		errf->print (_func_name).print (": _state_el: 0x").printHex ((Uint64) data->cur_function_declarator_state_el).pendl ();
	    )

	    if (data->cur_function_declarator_state_el == NULL)
		data->cur_function_declarator_state_el = declaration_state.function_declarator_states.last;
	    else
		data->cur_function_declarator_state_el = data->cur_function_declarator_state_el->previous;

	    abortIf (data->cur_function_declarator_state_el == NULL);

	    FunctionDeclaratorState &function_declarator_state = data->cur_function_declarator_state_el->data;

	    DEBUG (
		errf->print (_func_name).print (": function_declarator_state: 0x").printHex ((Uint64) &function_declarator_state).print (", declarator_state: 0x").printHex ((Uint64) function_declarator_state.declarator_state.ptr ()).pendl ();
	    )
	    abortIf (function_declarator_state.declarator_state.isNull ());
	    DeclaratorState * const saved_state = function_declarator_state.declarator_state;

#if 0
	    abortIf (direct_declarator->user_obj.isNull ());
	    CppParser_Impl::DeclaratorState *saved_state =
		    static_cast <CppParser_Impl::DeclaratorState*> (
			    direct_declarator->user_obj.ptr ());
#endif

	    declarator_state->type_desc = saved_state->type_desc;
	    declarator_state->cur_type = saved_state->cur_type;

	    declarator_state->name = saved_state->name;

	    DEBUG (
		errf->print (_func_name).print (": type_atoms: ").print (saved_state->type_atoms.getNumElements ()).pendl ();
	    )
	    // TODO Сомнительно
	    declarator_state->type_atoms.steal (&saved_state->type_atoms,
						saved_state->type_atoms.first,
						saved_state->type_atoms.last,
						declarator_state->type_atoms.first,
						GenericList::StealPrepend);
	    declarator_state->is_reference = saved_state->is_reference;

#if 0
// Wrong	    direct_declarator->user_obj = NULL;
	  // 'saved_state' is not valid anymore.
#endif
	} break;
	case Cpp_DirectDeclarator::t_Array: {
	    DEBUG (
		errf->print ("--- ARRAY").pendl ();
	    )

	    Cpp_DirectDeclarator_Array const * const direct_declarator__array =
		    static_cast <Cpp_DirectDeclarator_Array const *> (direct_declarator);

	    if (declarator_state->is_reference) {
		errf->print (_func_name)
		     .print (": array of references").pendl ();
		// TODO Throw an exception.
		abortIfReached ();
	    }

	    Ref<Cpp::TypeDesc::TypeAtom_Array> type_atom__array =
		    grab (new Cpp::TypeDesc::TypeAtom_Array);
	    declarator_state->type_atoms.prepend (type_atom__array);
	    // TODO Array size expression (subscript)

	    if (direct_declarator__array->directDeclarator != NULL) {
		cpp_parser_do_parse_direct_declarator (self,
						       declarator_state,
						       direct_declarator__array->directDeclarator,
						       data);
	    }
	} break;
	case Cpp_DirectDeclarator::t_Declarator: {
	    Cpp_DirectDeclarator_Declarator const * const direct_declarator__declarator =
		    static_cast <Cpp_DirectDeclarator_Declarator const *> (direct_declarator);

	    abortIf (direct_declarator__declarator->declarator == NULL);
	    cpp_parser_do_parse_declarator (self,
					    declarator_state,
					    direct_declarator__declarator->declarator,
					    data);
	} break;
	default:
	    abortIfReached ();
    }
}

#if 0
// Unused
static Ref<CppParser_Impl::DeclaratorState>
cpp_parser_parse_direct_declarator (CppParser_Impl       * const self,
				    Cpp_DirectDeclarator * const direct_declarator)
{
    Ref<CppParser_Impl::DeclaratorState> declarator_state =
	    grab (new CppParser_Impl::DeclaratorState);

    if (direct_declarator != NULL) {
	CppParser_DoParseDirectDeclarator_Data data;
	cpp_parser_do_parse_direct_declarator (self, declarator_state, direct_declarator, &data);
    }

    return declarator_state;
}
#endif

static void
cpp_parser_do_parse_declarator (CppParser_Impl  * const self,
				DeclaratorState * const declarator_state,
				Cpp_Declarator  * const declarator,
			        CppParser_DoParseDirectDeclarator_Data * const direct_data)
{
    static char const * const _func_name =
	    "Scruffy.(CppParser_Impl).cpp_parser_do_parse_declarator";

    DEBUG (
	errf->print (_func_name).pendl ();
    )

    abortIf (self == NULL             ||
	     declarator_state == NULL ||
	     declarator == NULL);

    Cpp_DirectDeclarator *direct_declarator = NULL;
    {
	Cpp_Declarator const *cur_declarator = declarator;
	for (;;) {
	    Bool done = false;
	    switch (cur_declarator->declarator_type) {
		case Cpp_Declarator::t_DirectDeclarator: {
		    DEBUG (
			errf->print (_func_name)
			     .print (": declarator: DirectDeclarator").pendl ();
		    )

		    Cpp_Declarator_DirectDeclarator const *declarator__direct_declarator =
			    static_cast <Cpp_Declarator_DirectDeclarator const *> (cur_declarator);

		    direct_declarator = declarator__direct_declarator->directDeclarator;
		    done = true;
		} break;
		case Cpp_Declarator::t_PtrOperator: {
		    DEBUG (
			errf->print (_func_name)
			     .print (": declarator: PtrOperator").pendl ();
		    )

		    Cpp_Declarator_PtrOperator const *declarator__ptr_operator =
			    static_cast <Cpp_Declarator_PtrOperator const *> (cur_declarator);

		    if (declarator_state->is_reference) {
			errf->print (_func_name)
			     .print (": reference is not the last ptr-operator").pendl ();
			// TODO Throw an exception.
			abortIfReached ();
		    }

		    Cpp_PtrOperator const *ptr_operator = declarator__ptr_operator->ptrOperator;
		    switch (ptr_operator->ptr_operator_type) {
			case Cpp_PtrOperator::t_Pointer: {
			    DEBUG (
				errf->print (_func_name)
				     .print (": ptr-operator: Pointer").pendl ();
			    )

			    Cpp_PtrOperator_Pointer const *ptr_operator__pointer =
				    static_cast <Cpp_PtrOperator_Pointer const *> (ptr_operator);

			    Ref<Cpp::TypeDesc::TypeAtom_Pointer> type_atom__pointer =
				    grab (new Cpp::TypeDesc::TypeAtom_Pointer);
			    declarator_state->type_atoms.prepend (type_atom__pointer);

			    Cpp_CvQualifierSeq const *cur_cv_qualifier_seq = ptr_operator__pointer->cvQualifierSeq;
			    while (cur_cv_qualifier_seq != NULL) {
				Cpp_CvQualifier const *cv_qualifier = cur_cv_qualifier_seq->cvQualifier;
				abortIf (cv_qualifier == NULL);
				switch (cv_qualifier->cv_qualifier_type) {
				    case Cpp_CvQualifier::t_Const: {
					if (type_atom__pointer->is_const) {
					    errf->print (_func_name)
						 .print (": duplicate \"const\"").pendl ();
					    abortIfReached ();
					}

					type_atom__pointer->is_const = true;
				    } break;
				    case Cpp_CvQualifier::t_Volatile: {
					if (type_atom__pointer->is_volatile) {
					    errf->print (_func_name)
						 .print (": duplicate \"volatile\"").pendl ();
					    // TODO abort?
//					    abortIfReached ();
					}

					type_atom__pointer->is_volatile = true;
				    } break;
				    default:
					abortIfReached ();
				}

				cur_cv_qualifier_seq = cur_cv_qualifier_seq->cvQualifierSeq;
			    }
			} break;
			case Cpp_PtrOperator::t_Reference: {
			    DEBUG (
				errf->print (_func_name)
				     .print (": ptr-operator: Reference").pendl ();
			    )

			    declarator_state->is_reference = true;
			} break;
			case Cpp_PtrOperator::t_PointerToMember: {
			    DEBUG (
				errf->print (_func_name)
				     .print (": ptr-operator: PointerToMember").pendl ();
			    )

			    Cpp_PtrOperator_PointerToMember const *ptr_operator__pointer_to_member =
				    static_cast <Cpp_PtrOperator_PointerToMember const *> (ptr_operator);

			    Ref<Cpp::TypeDesc::TypeAtom_PointerToMember> type_atom__pointer_to_member =
				    grab (new Cpp::TypeDesc::TypeAtom_PointerToMember);
			    declarator_state->type_atoms.prepend (type_atom__pointer_to_member);

			    Cpp_CvQualifierSeq const *cur_cv_qualifier_seq = ptr_operator__pointer_to_member->cvQualifierSeq;
			    while (cur_cv_qualifier_seq != NULL) {
				Cpp_CvQualifier const *cv_qualifier = cur_cv_qualifier_seq->cvQualifier;
				abortIf (cv_qualifier == NULL);
				switch (cv_qualifier->cv_qualifier_type) {
				    case Cpp_CvQualifier::t_Const: {
					if (type_atom__pointer_to_member->is_const) {
					    errf->print (_func_name)
						 .print (": duplicate \"const\"").pendl ();
					    abortIfReached ();
					}

					type_atom__pointer_to_member->is_const = true;
				    } break;
				    case Cpp_CvQualifier::t_Volatile: {
					if (type_atom__pointer_to_member->is_volatile) {
					    errf->print (_func_name)
						 .print (": duplicate \"volatile\"").pendl ();
					    // TODO abort?
//					    abortIfReached ();
					}

					type_atom__pointer_to_member->is_volatile = true;
				    } break;
				    default:
					abortIfReached ();
				}

				cur_cv_qualifier_seq = cur_cv_qualifier_seq->cvQualifierSeq;
			    }

			    // TODO Get containing class from NameTracker
			    // by type_name_specifier.
			} break;
			default:
			    abortIfReached ();
		    }

		    cur_declarator = declarator__ptr_operator->declarator;
		    if (cur_declarator == NULL)
			done = true;
		} break;
		default:
		    abortIfReached ();
	    } // switch (cur_declarator->declarator_type)

	    if (done)
		break;
	} // for (;;) for direct_declarator
    } // direct_declarator block

    if (direct_declarator != NULL) {
	CppParser_DoParseDirectDeclarator_Data tmp_data;
	cpp_parser_do_parse_direct_declarator (self,
					       declarator_state,
					       direct_declarator,
					       direct_data != NULL ? direct_data : &tmp_data);
    }
}

static Ref<DeclaratorState>
cpp_parser_parse_declarator (CppParser_Impl * const self,
			     Cpp_Declarator * const declarator)
{
  FUNC_NAME (
    static char const * const _func_name =
	    "Scruffy.(CppParser_Impl).cpp_parser_parse_declarator";
  )

    DEBUG (
	errf->print (_func_name).pendl ();
    )

    Ref<DeclaratorState> declarator_state = grab (new DeclaratorState);

    if (declarator != NULL)
	cpp_parser_do_parse_declarator (self, declarator_state, declarator);

    return declarator_state;
}

static bool cpp_do_declarator_match (Cpp_Declarator * const declarator,
				     void           * const _self);

#if 0
bool
cpp_scruffy_declarator_match_func (Cpp_Declarator * const declarator,
				   ParserControl  * const parser_control,
				   void           * const _self)
{
    static char const * const _func_name = "cpp_scruffy_declarator_match_func";
}
#endif

bool
cpp_Scruffy_Declarator_match_func (Cpp_Declarator * const declarator,
				   ParserControl  * const parser_control,
				   void           * const _self)
{
  FUNC_NAME (
    static char const * const _func_name = "cpp_Scruffy_Declarator_match_func";
  )

    DEBUG_FLOW (
	errf->print (_func_name).pendl ();
    )

    DEBUG (
	dump_cpp_Declarator (declarator);
    )

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    DeclarationState &declaration_state = self->declaration_states.getLast ();

    {
      // Dealing with constructor declaration ambiguity.

	CppParser_Impl::ConstructorAmbiguityState &constructor_ambiguity_state =
		self->constructor_ambiguity_states.last->data;

	DEBUG (
	    errf->print (_func_name).print (": "
			 "is_dummy: ").print (constructor_ambiguity_state.is_dummy ? "true" : "false").print (", "
			 "constructor_attempt: ").print (constructor_ambiguity_state.constructor_attempt ? "true" : "false").pendl ();
	)

	if (!constructor_ambiguity_state.is_dummy &&
	    !constructor_ambiguity_state.constructor_attempt)
	{
	  DEBUG (
	    errf->print (_func_name).print (":\n"
			 "\ttype_desc.isNull (): ").print (declaration_state.type_desc.isNull () ? "true" : "false");
	    if (!declaration_state.type_desc.isNull ()) {
		errf->print (
			"\n"
			"\tTypeDesc::t_Class:     ").print (
				declaration_state.type_desc->getType () == Cpp::TypeDesc::t_Class ? "true" : "false").print ("\n"
			"\tsimple_type_specifier_used: ").print (
				declaration_state.type_desc->simple_type_specifier_used ? "true" : "false").print ("\n"
			"\tis_template:          ").print (declaration_state.type_desc->is_template ? "true" : "false").print ("\n"
			"\tis_reference:         ").print (declaration_state.type_desc->is_reference ? "true" : "false").print ("\n"
			"\ttype_atoms.isEmpty(): ").print (declaration_state.type_desc->type_atoms.isEmpty () ? "true" : "false").print ("\n"
			"\tis_const:             ").print (declaration_state.type_desc->is_const ? "true" : "false").print ("\n"
			"\tis_volatile:          ").print (declaration_state.type_desc->is_volatile ? "true" : "false").print ("\n"
			).pendl ();
	    } else {
		errf->pendl ();
	    }
	  )

	    if (!declaration_state.type_desc.isNull ()                            &&
		 declaration_state.type_desc->getType () == Cpp::TypeDesc::t_Class &&
		 declaration_state.type_desc->simple_type_specifier_used          &&
		!declaration_state.type_desc->is_template                         &&
		!declaration_state.type_desc->is_reference                        &&
		 declaration_state.type_desc->type_atoms.isEmpty ()               &&
		!declaration_state.type_desc->is_const                            &&
		!declaration_state.type_desc->is_volatile)
	    {
		Ref<DeclaratorState> declarator_state = cpp_parser_parse_declarator (self, declarator);

		if (declarator_state->name.isNull ()) {
		    DEBUG (
			errf->print (_func_name).print (": declarator_state->name.isNull ()").pendl ();

			errf->print (_func_name).print (": type_desc->getType(): ").print (declarator_state->type_desc->getType ()).print (", ")
				     .print ("type_atoms.isEmpty(): ").print (declarator_state->type_atoms.isEmpty () ? "true" : "false").print (", ")
				     .print ("is_reference: ").print (declarator_state->type_desc->is_reference ? "true" : "false").pendl ();
		    )

		    if ( declarator_state->type_desc->getType () == Cpp::TypeDesc::t_Function &&
			 declarator_state->type_desc->type_atoms.isEmpty () &&
			!declarator_state->type_desc->is_reference)
		    {
			DEBUG (
			    errf->print (_func_name).print (": beginning constructor attempt").pendl ();
			)

			constructor_ambiguity_state.constructor_attempt = true;
			DEBUG (
			    errf->print (_func_name).print (": calling parser_control->setPosition()").pendl ();
			)
			parser_control->setPosition (constructor_ambiguity_state.parser_position);
			return true;
		    }
		}
	    }
	}
    }

    bool match = declaration_state.declarator_has_declarator_id;

    DEBUG (
	errf->print (_func_name).print (": ").print (match ? "true" : "false").pendl ();
    )

    if (match)
	return cpp_do_declarator_match (declarator, self);

    DEBUG (
	errf->print (_func_name).print (": no match").pendl ();
    )

    return false;
}

bool
cpp_Scruffy_AbstractDeclarator_match_func (Cpp_Declarator * const declarator,
					   ParserControl  * const /* parser_control */,
					   void           * const _self)
{
  FUNC_NAME (
    static char const * const _func_name = "cpp_Scruffy_AbstractDeclarator_match_func";
  )

    DEBUG_FLOW (
	errf->print (_func_name).pendl ();
    )

    DEBUG (
	dump_cpp_Declarator (declarator);
    )

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    DeclarationState &declaration_state = self->declaration_states.getLast ();

    bool match = !declaration_state.declarator_has_declarator_id;

    DEBUG (
	errf->print (_func_name).print (": ").print (match ? "true" : "false").pendl ();
    )

    if (match)
	return cpp_do_declarator_match (declarator, self);

    return false;
}

bool
cpp_Scruffy_AnyDeclarator_match_func (Cpp_Declarator * const declarator,
				      ParserControl  * const /* parser_control */,
				      void           * const _self)
{
    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);
    return cpp_do_declarator_match (declarator, self);
}

void
cpp_Scruffy_Declarator_Common_begin_func (void * const _self)
{
  FUNC_NAME (
    static char const * const _func_name = "Scruffy.(CppParser_Impl).cpp_Scruffy_Declarator_Common_begin_func";
  )

    DEBUG (
	errf->print (_func_name).pendl ();
    )

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    DeclarationState &declaration_state = self->declaration_states.getLast ();

  // Resetting declarator state

    declaration_state.declarator_has_declarator_id = false;

    declaration_state.global_scope = false;
    declaration_state.nested_name_atoms.clear ();

    if (!declaration_state.type_desc.isNull ())
	declaration_state.type_desc->type_atoms.clear ();

    declaration_state.member = NULL;
}

void
cpp_DeclaratorId_accept_func (Cpp_DeclaratorId * const declarator_id,
			      ParserControl    * const /* parser_control */,
			      void             * const _self)
{
    if (declarator_id == NULL)
	return;

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    DeclarationState &declaration_state = self->declaration_states.getLast ();

  // Updating declarator state

    // This is the distinction between abstract and non-abstract declarators.
    abortIf (declaration_state.declarator_has_declarator_id);
    DEBUG (
	errf->print ("--- DECLARATOR_HAS_DECLARATOR_ID = true ---").pendl ();
    )
    declaration_state.declarator_has_declarator_id = true;
    self->checkpoint_tracker.addBoundCancellable (
	    createValueCancellable (declaration_state.declarator_has_declarator_id, false),
	    declaration_state.checkpoint_key);

    CppParser_Impl::IdState &id_expression_state = self->id_states.getLast ();

    abortIf (declaration_state.global_scope);
    declaration_state.global_scope = id_expression_state.global_scope;

    abortIf (!declaration_state.nested_name_atoms.isEmpty ());
    declaration_state.nested_name_atoms.steal (&id_expression_state.nested_name_atoms,
					       id_expression_state.nested_name_atoms.first,
					       id_expression_state.nested_name_atoms.last,
					       declaration_state.nested_name_atoms.last,
					       GenericList::StealAppend);

    self->checkpoint_tracker.addBoundCancellable (
	    grab (new DeclarationState::Cancellable_DeclaratorId (declaration_state)),
	    declaration_state.checkpoint_key);
}

bool
cpp_reset_declarator (CppElement    * const /* cpp_element */,
		      ParserControl * const /* parser_control */,
		      void          * const _self)
{
    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    DeclarationState &declaration_state = self->declaration_states.getLast ();

    // TODO Declarator_Common_begin does the same

    declaration_state.mem_initializers.clear ();
    declaration_state.member = NULL;

    return true;
}

#if 0
bool
cpp_Scruffy_Declarator_Common_match_func (Cpp_Declarator * const declarator,
//					   ParserControl  * const /* parser_control */,
					  void           * const _self)
#endif
static bool
cpp_do_declarator_match (Cpp_Declarator * const declarator,
			 void           * const _self)
{
  // Note: 'declarator' may be NULL for abstract declarators.

  FUNC_NAME (
    static char const * const _func_name =
	    "Scruffy.(CppParser_Impl).Scruffy_Declarator_Common_match_func";
  )

    DEBUG (
	errf->print (_func_name).print (": declarator: 0x").printHex ((UintPtr) declarator).pendl ();
    )

    DEBUG (
	dump_cpp_Declarator (declarator);
    )

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    DeclarationState &declaration_state = self->declaration_states.getLast ();

    Ref<DeclaratorState> declarator_state = cpp_parser_parse_declarator (self, declarator);

    DEBUG (
	errf->print (_func_name).print (": declarator type atoms: ").print (declarator_state->type_atoms.isEmpty () ? "empty" : "non-empty").pendl ();
    )

    Ref<Cpp::TypeDesc> type_desc;
    if (declaration_state.type_desc.isNull ()) {
      // We have no type information in the list of declaration specifiers.
      // This is possible for constructors/destructors.

	// We shoulnd't have any type atoms in this case.
//	abortIf (!declarator_state->type_atoms.isEmpty ());
	if (!declarator_state->type_atoms.isEmpty ())
	    return false;
    } else {
	type_desc = grab (declaration_state.type_desc->clone ());

	type_desc->is_reference = declarator_state->is_reference;

	type_desc->type_atoms.steal (&declarator_state->type_atoms,
				     declarator_state->type_atoms.first,
				     declarator_state->type_atoms.last,
				     declaration_state.type_desc->type_atoms.last,
				     GenericList::StealAppend);
    }

    // Putting the resulting TypeDesc to where it belongs.
    abortIf (declarator_state->cur_type == NULL);
    *declarator_state->cur_type = type_desc;

  // At this point, we've completed parsing the declarator.
  // 'declarator_state' contains a valid final description of the object declared.

    if (declarator_state->type_desc.isNull ()) {
	DEBUG (
	    errf->print ("No type for declaration").pendl ();
	)
	return false;
    }

  // Registering the declared object.

    DEBUG (
	errf->print ("--- ACCEPTING DECLARATOR ---").pendl ();
    )

    Ref<Cpp::Member> member;
    if ((self->class_phase == CppParser_Impl::ClassParsingPhase_Rematch &&
	 self->temporal_namespaces.isEmpty ()) ||
	// For external member definitions
	!declaration_state.nested_name_atoms.isEmpty ())
    {
	DEBUG (
	    errf->print (_func_name).print (": rematch phase or nested name").pendl ();
	)

	// TODO Check that the type of the member matches that in the original declaration.

	// TODO getPreviousObjectDeclaration()?
	member = self->lookupMember (declaration_state.global_scope,
				     declaration_state.nested_name_atoms,
				     *declarator_state->name);
	if (member.isNull ()) {
	    errf->print ("--- Rematch: member not found ---").pendl ();
	    abortIfReached ();
	}
    } else {
	if (declaration_state.is_typedef) {
	    Ref<Cpp::Member_Type> member__type = grab (new Cpp::Member_Type);
#if 0
// This is done below
	    member__type->type_desc = declarator_state->type_desc;
	    member__type->name = declarator_state->name;
#endif
	    member__type->is_typedef = true;

	    member = member__type;
	} else
#if 0
	if (declarator_state->type_desc->getType () != Cpp::TypeDesc::t_Function ||
	    declarator_state->type_desc->is_reference                           ||
	    !declarator_state->type_desc->type_atoms.isEmpty ())
	{
#endif
	if (!declarator_state->type_desc->isFunction ()) {
	  // Object member

	    DEBUG (
		errf->print (_func_name).print (": object member").pendl ();
	    )

	    member = grab (new Cpp::Member_Object);
	} else {
	  // Function member

	    DEBUG (
		errf->print (_func_name).print (": function member").pendl ();
	    )

	    {
	      // Functions must always have return types, except for constructors
	      // and destructors.

	      // TODO Add checks for destructors (is that necessary?)

		Cpp::TypeDesc_Function const * const type_desc__function =
			static_cast <Cpp::TypeDesc_Function const *> (declarator_state->type_desc.ptr ());

		if (type_desc__function->return_type.isNull ()) {
		    if (!self->temporal_namespaces.isEmpty ()) {
			DEBUG (
			    errf->print ("--- CONSTRUCTOR: temporal namespace ---").pendl ();
			)
			return false;
		    }

		    if (self->constructor_ambiguity_states.isEmpty ()) {
			DEBUG (
			    errf->print ("--- CONSTRUCTOR: no amiguity states ---").pendl ();
			)
			return false;
		    }

		    CppParser_Impl::ConstructorAmbiguityState &constructor_ambiguity_state =
			    self->constructor_ambiguity_states.last->data;

		    if (!constructor_ambiguity_state.constructor_attempt &&
			// In "rematch" and "none" phases, constructor's name doesn't look
			// like a class-name-identifier, and we do not have to make a "constructor attempt".
			self->class_phase != CppParser_Impl::ClassParsingPhase_Rematch &&
			self->class_phase != CppParser_Impl::ClassParsingPhase_None)
		    {
			DEBUG (
			    errf->print ("--- CONSTRUCTOR: not a constructor attempt ---").pendl ();
			)
			return false;
		    }
		}
	    }

	    Ref<Cpp::Member_Function> member__function = grab (new Cpp::Member_Function);

	    member__function->function = grab (new Cpp::Function);

	    if (!self->template_declaration_states.isEmpty ()) {
	      // function template

//		// TODO Make use of template_declaration_desc.

		CppParser_Impl::TemplateDeclarationState &template_declaration_state =
			self->template_declaration_states.last->data;

		member__function->function->is_template = true;
#if 0
// TODO Use steal() in non-AST mode.
		member__function->function->template_parameters.steal (&template_declaration_state.template_parameters,
								       template_declaration_state.template_parameters.first,
								       template_declaration_state.template_parameters.last,
								       member__function->function->template_parameters.first,
								       GenericList::StealAppend);
#endif
		{
		    List< Ref<Cpp::Member> >::DataIterator iter (template_declaration_state.template_parameters);
		    while (!iter.done ()) {
			Ref<Cpp::Member> &member = iter.next ();
			member__function->function->template_parameters.append (member);
		    }
		}

		self->grabTemplateDeclaration ();
	    }

#if 0
// Unnecessary
	    self->checkpoint_tracker.addBoundCancellable (
		    grab (new Cancellable_Ref<Cpp::Function> (member__function->function)),
		    declaration_state.checkpoint_key);
#endif


	    DEBUG (
		errf->print (_func_name).print (": member__function: 0x").printHex ((Uint64) member__function.ptr ()).pendl ();
	    )

	    member = member__function;
	}
	abortIf (member.isNull ());

	member->type_desc = declarator_state->type_desc;
	member->name = declarator_state->name;

	// TODO if (is_typedef)
	Ref<DeclarationDesc> declaration_desc = grab (new DeclarationDesc);
	declaration_desc->member = member;

	{
	  // Setting nested name state.

	    abortIf (declaration_desc->global_scope);
	    declaration_desc->global_scope = declaration_state.global_scope;

	    abortIf (!declaration_desc->nested_name_atoms.isEmpty ());
	    declaration_desc->nested_name_atoms.steal (&declaration_state.nested_name_atoms,
						       declaration_state.nested_name_atoms.first,
						       declaration_state.nested_name_atoms.last,
						       declaration_desc->nested_name_atoms.first,
						       GenericList::StealAppend);
	}

	abortIf (declaration_state.declaration_acceptor.isNull ());
	declaration_state.declaration_acceptor->accept (declaration_desc);
    }

    {
      // Setting 'declaration_state.member'
	abortIf (!declaration_state.member.isNull ());
	declaration_state.member = member;
	self->checkpoint_tracker.addBoundCancellable (
		grab (new Cancellable_Ref<Cpp::Member> (declaration_state.member)),
		declaration_state.checkpoint_key);
    }

    if (!declaration_state.got_declarator) {
	self->checkpoint_tracker.addBoundCancellable (
		createValueCancellable (declaration_state.got_declarator, false),
		declaration_state.checkpoint_key);
    }
    DEBUG (
	errf->print ("--- GOT_DECLARTOR = TRUE ---").pendl ();
    )
    declaration_state.got_declarator = true;

    self->checkpoint_tracker.addBoundCancellable (
		createValueCancellable (declaration_state.num_declarators,
					declaration_state.num_declarators),
		declaration_state.checkpoint_key);
    declaration_state.num_declarators ++;

#if 0
    if (!declaration_state.type_desc.isNull ()) {
	errf->print ("type_desc: 0x").printHex ((UintPtr) declaration_state.type_desc.ptr ()).print ("\n"
		     "getType(): ").print ((UintPtr) declaration_state.type_desc->getType ()).print ("\n"
		     "simple_type_specifier_used: ").print (declaration_state.type_desc->simple_type_specifier_used ? "true" : "false").print ("\n"
		     "is_template: ").print (declaration_state.type_desc->is_template ? "true" : "false").print ("\n"
		     "is_reference: ").print (declaration_state.type_desc->is_reference ? "true" : "false").print ("\n"
		     "type_atoms.isEmpty(): ").print (declaration_state.type_desc->type_atoms.isEmpty () ? "true" : "false").print ("\n"
		     "is_const: ").print (declaration_state.type_desc->is_const ? "true" : "false").print ("\n"
		     "is_volatile: ").print (declaration_state.type_desc->is_volatile ? "true" : "false").pendl ();
    } else {
	errf->print ("type_desc: NULL").pendl ();
    }
#endif

    return true;
}

// abstract-declarator_opt + no declarator
bool
cpp_accept_declaration (CppElement    * const /* cpp_element */,
			ParserControl * const /* parser_control */,
			void          * const _self)
{
    DEBUG (
	errf->print ("--- CPP_ACCEPT_DECLARATION ---").pendl ();
    )

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    DeclarationState &declaration_state = self->declaration_states.getLast ();

    if (declaration_state.got_declarator) {
	DEBUG (
	    errf->print ("--- GOT DECLARATOR ---").pendl ();
	)
	return true;
    }

    DEBUG (
	errf->print ("--- CPP_ACCEPT_DECLARATION: NO DECLARATOR ---").pendl ();
    )

    Ref<Cpp::Member> member = grab (new Cpp::Member_Object);

    abortIf (declaration_state.type_desc.isNull ());
    member->type_desc = declaration_state.type_desc;

    Ref<DeclarationDesc> declaration_desc = grab (new DeclarationDesc);
    declaration_desc->member = member;

    abortIf (declaration_state.declaration_acceptor.isNull ());
    declaration_state.declaration_acceptor->accept (declaration_desc);
    {
      // Setting 'declaration_state.member'
	abortIf (!declaration_state.member.isNull ());
	declaration_state.member = declaration_desc->member;
	self->checkpoint_tracker.addBoundCancellable (
		grab (new Cancellable_Ref<Cpp::Member> (declaration_state.member)),
		declaration_state.checkpoint_key);
    }

    return true;
}

namespace {
class Acceptor_Typeid : public Scruffy::Acceptor<DeclarationDesc>
{
private:
    CppParser_Impl &self;
    CppParser_Impl::TypeidState &typeid_state;

    CheckpointTracker::CheckpointKey checkpoint_key;

public:
    void accept (DeclarationDesc * const declaration_desc)
    {
      FUNC_NAME (
	static char const * const _func_name = "Scruffy.(CppParser).Acceptor_Typeid.accept";
      )

	DEBUG (
	    errf->print (_func_name).pendl ();
	)

	abortIf (declaration_desc == NULL ||
		 !declaration_desc->isSane ());

	abortIf (declaration_desc->hasNestedNameSpecifier ());

	// TODO "is_function"
#if 0
// Wrong, but there should be a check.
	if (declaration_desc->member->type_desc->getType () == Cpp::TypeDesc::t_Function) {
	    errf->print ("Function type-id").pendl ();
	    abortIfReached ();
	}
#endif

	abortIf (!typeid_state.type_desc.isNull ());
	typeid_state.type_desc = declaration_desc->member->type_desc;
	{
	    Ref< Cancellable_Ref<Cpp::TypeDesc> > cancellable =
		    grab (new Cancellable_Ref<Cpp::TypeDesc> (typeid_state.type_desc));
	    self.checkpoint_tracker.addBoundCancellable (cancellable, checkpoint_key);
	}
    }

    void cancel ()
    {
	abortIfReached ();
    }

    Acceptor_Typeid (CppParser_Impl              &self,
		     CppParser_Impl::TypeidState &typeid_state)
	: self (self),
	  typeid_state (typeid_state)
    {
	checkpoint_key = self.checkpoint_tracker.getCurrentCheckpoint ();
    }
};
} // namespace {}

bool
cpp_begin_typeid (CppElement    * const /* cpp_element */,
		  ParserControl * const /* parser_control */,
		  void          * const _self)
{
    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    self->typeid_states.appendEmpty ();
    {
	Ref< Cancellable_ListElement<CppParser_Impl::TypeidState> > cancellable =
		grab (new Cancellable_ListElement<CppParser_Impl::TypeidState> (
			      self->typeid_states,
			      self->typeid_states.last));
	self->checkpoint_tracker.addUnconditionalCancellable (cancellable);
    }

    CppParser_Impl::TypeidState &typeid_state = self->typeid_states.last->data;

    self->declaration_acceptors.append (
	    grab (new Acceptor_Typeid (*self,
				       typeid_state)));
    {
	Ref< Cancellable_ListElement< Ref< Scruffy::Acceptor<DeclarationDesc> > > > cancellable =
		grab (new Cancellable_ListElement< Ref< Scruffy::Acceptor<DeclarationDesc> > > (
			      self->declaration_acceptors,
			      self->declaration_acceptors.last));
	self->checkpoint_tracker.addUnconditionalCancellable (cancellable);
    }

    return true;
}

bool
cpp_accept_typeid (Cpp_TypeId    * const type_id,
		   ParserControl * const /* parser_control */,
		   void          * const _self)
{
    if (type_id == NULL)
	return true;

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

  // Sanity check.

    abortIf (self->typeid_states.isEmpty ());
    CppParser_Impl::TypeidState &typeid_state = self->typeid_states.last->data;

    abortIf (typeid_state.type_desc.isNull ());

    return true;
}

bool
cpp_accept_class_head (Cpp_ClassSpecifier * const class_specifier,
		       ParserControl      * const parser_control,
		       void               * const _self)
{
  FUNC_NAME (
    static char const * const _func_name = "cpp_accept_class_head";
  )

    if (class_specifier == NULL)
	return true;

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    DEBUG (
	errf->print (_func_name).pendl ();
    )

    DeclarationState &declaration_state = self->declaration_states.getLast ();

    if (self->class_phase == CppParser_Impl::ClassParsingPhase_None) {
	DEBUG (
	    errf->print (_func_name).print (": phase: none").pendl ();
	)

	char const * const variant_str = "class-prematch";
	parser_control->setVariant (ConstMemoryDesc (variant_str, countStrLength (variant_str)));
	self->class_checkpoint = self->checkpoint_counter;
	self->class_pmark = parser_control->getPosition ();
	self->class_phase = CppParser_Impl::ClassParsingPhase_Prematch;

	self->checkpoint_tracker.addBoundCancellable (
		grab (new Cancellable_ClassParsingPhase (self, parser_control)),
		declaration_state.checkpoint_key);
    }

    return true;
}

// Class definition parsing starts here.
bool
cpp_accept_class_identifier (Cpp_ClassHead * const class_head,
			     ParserControl * const /* parser_control */,
			     void          * const _self)
{
  // We register the class early, because
  // the class being declared should be already known when we're parsing its
  // bcase-clause. Consider the following code:
  //
  // template <class T>
  // class A {
  //     T *ptr;
  // }
  //
  // class B : public A<B>
  // {
  // };

    if (class_head == NULL)
	return true;

    static char const * const _func_name =
	    "Scruffy.(CppParser_Impl).cpp_accept_class_identifier";

    DEBUG (
	errf->print (_func_name).pendl ();
    )

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    DeclarationState &declaration_state = self->declaration_states.getLast ();

    if (!self->template_declaration_states.isEmpty ()) {
      // class template

	// TODO Make use of template_declaration_state.

	self->grabTemplateDeclaration ();
    }

  // Retreiving the identifier

    ConstMemoryDesc identifier;
    switch (class_head->class_head_type) {
	case Cpp_ClassHead::t_Simple: {
	    Cpp_ClassHead_Simple const * const class_head__simple =
		    static_cast <Cpp_ClassHead_Simple const *> (class_head);

	    if (class_head__simple->identifier != NULL)
		identifier = class_head__simple->identifier->any_token->token;
	} break;
	case Cpp_ClassHead::t_NestedNameSpecifier: {
	    Cpp_ClassHead_NestedNameSpecifier const * const class_head__nested_name_specifier =
		    static_cast <Cpp_ClassHead_NestedNameSpecifier const *> (class_head);

	    abortIf (class_head__nested_name_specifier->identifier == NULL);
	    identifier = class_head__nested_name_specifier->identifier->any_token->token;
	} break;
	case Cpp_ClassHead::t_TemplateId: {
	    // TODO templates
	    abortIfReached ();
	} break;
	default:
	    abortIfReached ();
    }

    Ref<Cpp::Member> member;

    {
      // Dealing with a nested-name-specifier, if any.
      //
      // If there's a nested-name-specifier, then we're dealing
      // with a definition for a previously declared class, like this:
      //     namespace A { class B; }
      //     class A::B {};

	abortIf (self->nested_name_specifier_states.isEmpty ());
	CppParser_Impl::NestedNameSpecifierState &nested_name_specifier_state =
		self->nested_name_specifier_states.last->data;

	if (!nested_name_specifier_state.atoms.isEmpty ()) {
	    if (identifier.getLength () == 0) {
		errf->print (Pr << _func_name << ": nested-name-specifier without identifier").pendl ();
		abortIfReached ();
	    }

	    // TODO Lookup an existing (empty) class definition (!),
	    // set 'member' accordingly.
	    abortIfReached ();
	} else {
	    if (nested_name_specifier_state.global_scope) {
	      // class ::A {}; is ill-formed.

		errf->print (Pr << _func_name << ": global scope is not allowed").pendl ();
		abortIfReached ();
	    }

	    {
	      // DEBUG
		DEBUG (
		    errf->print (_func_name).print (": looking up class ").print (identifier).pendl ();
		)

#if 0
// Debug
		Cpp::DumpContext dump_ctx (outf, 4 /* tab_size */);
		dump_ctx.dumpTranslationUnit (self->root_namespace_entry->namespace_);
		outf->oflush ();
#endif
	    }

	    member = self->lookupType (identifier);
	    DEBUG (
		if (!member.isNull ())
		    errf->print (_func_name).print (": class ").print (identifier).print (" FOUND").pendl ();
		else
		    errf->print (_func_name).print (": class ").print (identifier).print (" NOT FOUND").pendl ();
	    )

#if 0
// Checked below

	    if (!member.isNull ()) {
		if (member->getType () != Cpp::Member::t_Type) {
		    errf->print ("Redeclaring as a different kind of symbol").pendl ();
		    abortIfReached ();
		}

		Cpp::Member_Type const * const member__type =
			static_cast <Cpp::Member_Type const *> (member);

		if (member__type->is_typedef) {
		    errf->print ("Redeclaring as a different kind of symbol (typedef)").pendl ();
		    abortIfReached ();
		}

		abortIf (member->type_desc.isNull ());
		if (member->type_desc->getType () != Cpp::TypeDesc_Class) {
		    errf->print ("Redeclaring as a different kind of symbol (type)").pendl ();
		    abortIfReached ();
		}
	    }
#endif
	}
    }

    Ref<Cpp::TypeDesc_Class> type_desc__class;
    if (!member.isNull ()) {
	// The member has to refer to a class definition.
	if (member->getType () != Cpp::Member::t_Type                           ||
	    static_cast <Cpp::Member_Type const *> (member.ptr ())->is_typedef ||
	    member->type_desc.isNull ()                                        ||
	    member->type_desc->getType () != Cpp::TypeDesc::t_Class             ||
	    !member->type_desc->type_atoms.isEmpty ()                          ||
	    member->type_desc->is_reference)
	{
	    errf->print ("Not a class").pendl ();
	    abortIfReached ();
	}

	type_desc__class = static_cast <Cpp::TypeDesc_Class*> (member->type_desc.ptr ());
	abortIf (type_desc__class == NULL ||
		 type_desc__class->class_.isNull ());
#if 0
// Misplaced
	if (type_desc__class->class_->got_definition &&
	    self->class_phase != ClassParsingPhase_Rematch)
	{
	    errf->print ("ERROR: Class redefinition").pendl ();
	    errf->print ("    class: ").print (type_desc__class->class_->name->toString ()).pendl ();
	    abortIfReached ();
	}
#endif
    }

    if (self->class_phase != CppParser_Impl::ClassParsingPhase_Rematch) {
	// FIXME If there's no identifier, then we should create an unnamed class
	//       (set type_desc__class, etc.)
	if (member.isNull () &&
	    identifier.getLength () > 0)
	{
	    // FIXME Take container from nested-name-specifier
	    abortIf (self->containers.isEmpty ());
	    Cpp::Container *container = self->containers.last->data;

	    DEBUG (
		errf->print (_func_name).print (": adding class ").print (identifier).pendl ();
	    )
	    member = self->addClass (declaration_state, container, identifier);
	    type_desc__class = static_cast <Cpp::TypeDesc_Class*> (member->type_desc.ptr ());
	}
    }
    DEBUG (
	errf->print (_func_name).print (": class_phase: ").print (self->class_phase).pendl ();
    )
    abortIf (member.isNull ());

    self->setDeclarationTypeDesc (declaration_state, member->type_desc);

    // A cancellable is not required, since there's no alternatives to this path.
    declaration_state.class_ = type_desc__class->class_;

    return true;
}

bool
cpp_begin_class_definition (Cpp_ClassSpecifier * const /* class_specifier */,
			    ParserControl      * const /* parser_control */,
			    void               * const _self)
{
  FUNC_NAME (
    static char const * const _func_name = "cpp_begin_class_definition";
  )

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    DEBUG (
	errf->print (_func_name).pendl ();
    )

    DeclarationState &declaration_state = self->declaration_states.getLast ();

    bool rematch = false;
    switch (self->class_phase) {
	case CppParser_Impl::ClassParsingPhase_None: {
	  // We should be in either "class-prematch" or "class-rematch" phase.
	    abortIfReached ();
	} break;
	case CppParser_Impl::ClassParsingPhase_Prematch: {
	  // Parsing a new class definition (moving on).
	} break;
	case CppParser_Impl::ClassParsingPhase_Rematch: {
	  // Class definition has already been created during "class-prematch"
	  // phase.
	    abortIf (declaration_state.class_.isNull ());
	    DEBUG (
		errf->print (_func_name).print (": rematch state").pendl ();
	    )
	    rematch = true;
	} break;
	default:
	    abortIfReached ();
    }

    abortIf (declaration_state.class_.isNull ());

    if (!rematch) {
	if (declaration_state.class_->got_definition) {
	    DEBUG (
		errf->print ("ERROR: Class redefinition").pendl ();
		errf->print ("    class: ").print (declaration_state.class_->name->toString ()).pendl ();
	    )
	    abortIfReached ();
	}

	// No cancellable required.
	declaration_state.class_->got_definition = true;
	DEBUG (
	    errf->print (_func_name).print (": got definition for class ").print (declaration_state.class_->name->toString ()).pendl ();
	)

	DEBUG (
	    errf->print ("--- class parents: ").print (declaration_state.class_parents.getNumElements ()).pendl ();
	)
	declaration_state.class_->parents.steal (&declaration_state.class_parents,
						 declaration_state.class_parents.first,
						 declaration_state.class_parents.last,
						 declaration_state.class_->parents.last,
						 GenericList::StealAppend);
    }

  // Adding acceptor for member declarations.

    self->declaration_acceptors.append (
	    grab (new Acceptor_ContainerMember (*self,
						*declaration_state.class_,
						self->checkpoint_tracker.getCurrentCheckpoint ())));
    self->checkpoint_tracker.addUnconditionalCancellable (
	    grab (new Cancellable_ListElement< Ref< Scruffy::Acceptor<DeclarationDesc> > > (
			  self->declaration_acceptors,
			  self->declaration_acceptors.last)));

  // Entering the new container

    self->containers.append (declaration_state.class_);
    self->checkpoint_tracker.addUnconditionalCancellable (
	    grab (new Cancellable_ListElement< Cpp::Container* > (
			  self->containers,
			  self->containers.last)));

    return true;
}

bool
cpp_accept_class_definition (Cpp_ClassSpecifier * const class_specifier,
			     ParserControl      * const parser_control,
			     void               * const _self)
{
  FUNC_NAME (
    static char const * const _func_name = "cpp_accept_class_definition";
  )

    if (class_specifier == NULL)
	return true;

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    DEBUG (
	errf->print (_func_name).pendl ();
    )

    abortIf (self->class_phase == CppParser_Impl::ClassParsingPhase_None);
    if (self->class_checkpoint == self->checkpoint_counter) {
	if (self->class_phase == CppParser_Impl::ClassParsingPhase_Prematch) {
	    DEBUG (
		errf->print (_func_name).print (": leaving prematch phase").pendl ();
	    )

	    parser_control->setVariant (self->default_variant->getMemoryDesc ());
	    parser_control->setPosition (self->class_pmark);
	    self->class_phase = CppParser_Impl::ClassParsingPhase_Rematch;
	} else {
	    DEBUG (
		errf->print (_func_name).print (": leaving rematch phase").pendl ();
	    )

	    abortIf (self->class_phase != CppParser_Impl::ClassParsingPhase_Rematch);

	    self->class_phase = CppParser_Impl::ClassParsingPhase_None;
	    self->class_pmark = NULL;
	}
    }

  // Adding class-specifier to current type specifiers.

    DeclarationState &declaration_state = self->declaration_states.getLast ();

    // For nested classes, we should call type_specifier_parser.setClass() on each pass.
    if (!declaration_state.type_specifier_parser.gotType ()) {
	declaration_state.type_specifier_parser.setClass (declaration_state.class_);
    }

    return true;
}

namespace {
class Acceptor_TemplateParameter : public Scruffy::Acceptor<DeclarationDesc>
{
private:
    CppParser_Impl::TemplateDeclarationState &template_declaration_state;
    CheckpointTracker &checkpoint_tracker;

    CheckpointTracker::CheckpointKey checkpoint_key;

public:
    void accept (DeclarationDesc * const declaration_desc)
    {
	abortIf (declaration_desc == NULL ||
		 !declaration_desc->isSane ());

	abortIf (declaration_desc->hasNestedNameSpecifier ());

	template_declaration_state.template_parameters.append (declaration_desc->member);
	checkpoint_tracker.addBoundCancellable (
		grab (new Cancellable_ListElement< Ref<Cpp::Member> > (
			      template_declaration_state.template_parameters,
			      template_declaration_state.template_parameters.last)),
		checkpoint_key);
    }

    Acceptor_TemplateParameter (CppParser_Impl::TemplateDeclarationState &template_declaration_state,
				CheckpointTracker &checkpoint_tracker)
	: template_declaration_state (template_declaration_state),
	  checkpoint_tracker (checkpoint_tracker)
    {
	checkpoint_key = checkpoint_tracker.getCurrentCheckpoint ();
    }
};
}

bool
cpp_begin_template_declaration (CppElement              * const /* cpp_element */,
				Pargen::ParserControl   * const /* parser_control */,
				void                    * const _self)
{
    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

// Wrong    abortIf (self->template_declaration_states.isEmpty ());
    CppParser_Impl::TemplateDeclarationState &template_declaration_state =
	    self->template_declaration_states.appendEmpty ()->data;
    self->checkpoint_tracker.addUnconditionalCancellable (
	    grab (new Cancellable_ListElement<CppParser_Impl::TemplateDeclarationState> (
			  self->template_declaration_states,
			  self->template_declaration_states.last)));

#if 0
// Wrong
    if (!self->template_declaration_desc.isNull ()) {
	errf->print ("Too many template parameter lists").pendl ();
	abortIfReached ();
    }
#endif

#if 0
    self->template_declaration_desc = grab (new CppParser_Impl::TemplateDeclarationDesc);
    self->template_declaration_desc->checkpoint_key = self->checkpoint_tracker.getCurrentCheckpoint ();
    {
	Ref< Cancellable_Ref<CppParser_Impl::TemplateDeclarationDesc> > cancellable =
		grab (new Cancellable_Ref<CppParser_Impl::TemplateDeclarationDesc> (self->template_declaration_desc));
	self->checkpoint_tracker.addUnconditionalCancellable (cancellable);
    }
#endif

    self->declaration_acceptors.append (
	    grab (new Acceptor_TemplateParameter (
			  template_declaration_state,
			  self->checkpoint_tracker)));
    {
	Ref< Cancellable_ListElement< Ref< Scruffy::Acceptor<DeclarationDesc> > > > cancellable =
		grab (new Cancellable_ListElement< Ref< Scruffy::Acceptor<DeclarationDesc> > > (
			      self->declaration_acceptors,
			      self->declaration_acceptors.last));
	template_declaration_state.acceptor_cancellable_key =
		self->checkpoint_tracker.addUnconditionalCancellable (cancellable);

	errf->print ("--- TEMPLATE PARAMETER ACCEPTOR 0x").printHex ((Uint64) self->declaration_acceptors.last->data.ptr ()).print (", template_declaration_state 0x").printHex ((Uint64) &template_declaration_state).pendl ();
    }

    return true;
}

bool
cpp_accept_template_parameter_list (CppElement              * const /* cpp_element */,
				    Pargen::ParserControl   * const /* parser_control */,
				    void                    * const _self)
{
    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

  // Removing acceptor for template parameters, so that the following declaration
  // will land where it is meant to be.
  // TODO This sounds strange.

    abortIf (self->template_declaration_states.isEmpty ());
    CppParser_Impl::TemplateDeclarationState &template_declaration_state =
	    self->template_declaration_states.last->data;

    errf->print ("--- REMOVING TEMPLATE PARAMETER ACCEPTOR 0x").printHex ((Uint64) self->declaration_acceptors.last->data.ptr ()).print (", template_declaration_state 0x").printHex ((Uint64) &template_declaration_state).pendl ();

    abortIf (self->declaration_acceptors.isEmpty ());
    self->declaration_acceptors.remove (self->declaration_acceptors.last);

//    abortIf (self->template_declaration_desc.isNull ());
//    self->checkpoint_tracker.removeCancellable (self->template_declaration_cancellable_key);
//    self->template_declaration_cancellable_key = CheckpointTracker::CancellableKey ();

    self->checkpoint_tracker.removeCancellable (template_declaration_state.acceptor_cancellable_key);

    return true;
}

bool
cpp_accept_template_declaration (Cpp_TemplateDeclaration * const template_declaration,
				 Pargen::ParserControl   * const /* parser_control */,
				 void                    * const /* _self */)
{
    if (template_declaration == NULL)
	return true;

//    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    // TODO A better sanity check is required.
    //      When we accept a declarator, we must check if there's
    //      a template-parameter-list and that we're ready to consume it.
#if 0
// Wrong
    if (!self->template_declaration_desc.isNull ()) {
      // self->template_declaration_desc should have been consumed.

	errf->print ("Invalid template declaration").pendl ();
	abortIfReached ();
    }
#endif

    return true;
}

bool
cpp_accept_type_template_parameter (Cpp_TypeParameter * const _type_parameter,
				    ParserControl     * const /* parser_control */,
				    void              * const _self)
{
    if (_type_parameter == NULL)
	return true;

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    Ref<Cpp::Member_TypeTemplateParameter> member__type_template_parameter =
	    grab (new Cpp::Member_TypeTemplateParameter);

    ConstMemoryDesc identifier;
    switch (_type_parameter->type_parameter_type) {
	case Cpp_TypeParameter::t_Class_Default: {
	    Cpp_TypeParameter_Class_Default const * const type_parameter =
		    static_cast <Cpp_TypeParameter_Class_Default const *> (_type_parameter);

	    if (type_parameter->identifier != NULL)
		identifier = type_parameter->identifier->any_token->token;

	    // TODO Default parameter!
	} break;
	case Cpp_TypeParameter::t_Class: {
	    Cpp_TypeParameter_Class const * const type_parameter =
		    static_cast <Cpp_TypeParameter_Class const *> (_type_parameter);

	    if (type_parameter->identifier != NULL)
		identifier = type_parameter->identifier->any_token->token;
	} break;
	case Cpp_TypeParameter::t_Typename_Default: {
	    Cpp_TypeParameter_Typename_Default const * const type_parameter =
		    static_cast <Cpp_TypeParameter_Typename_Default const *> (_type_parameter);

	    if (type_parameter->identifier != NULL)
		identifier = type_parameter->identifier->any_token->token;

	    // TODO Default parameter!
	} break;
	case Cpp_TypeParameter::t_Typename: {
	    Cpp_TypeParameter_Typename const * const type_parameter =
		    static_cast <Cpp_TypeParameter_Typename const *> (_type_parameter);

	    if (type_parameter->identifier != NULL)
		identifier = type_parameter->identifier->any_token->token;
	} break;
	case Cpp_TypeParameter::t_Template_Default: {
	    Cpp_TypeParameter_Template_Default const * const type_parameter =
		    static_cast <Cpp_TypeParameter_Template_Default const *> (_type_parameter);

	    // TODO Template parameters

	    if (type_parameter->identifier != NULL)
		identifier = type_parameter->identifier->any_token->token;

	    member__type_template_parameter->is_template = true;

	    abortIf (self->template_declaration_states.isEmpty ());
	    CppParser_Impl::TemplateDeclarationState &template_declaration_state =
		    self->template_declaration_states.last->data;

	    // TODO Steal parameters in forwards-only mode.
	    {
		List< Ref<Cpp::Member> >::DataIterator param_iter (template_declaration_state.template_parameters);
		while (!param_iter.done ()) {
		    Ref<Cpp::Member> &param_member = param_iter.next ();
		    member__type_template_parameter->template_parameters.append (param_member);
		}
	    }

	    // TODO Default parameter!
	} break;
	case Cpp_TypeParameter::t_Template: {
	    Cpp_TypeParameter_Template const * const type_parameter =
		    static_cast <Cpp_TypeParameter_Template const *> (_type_parameter);

	    // TODO Template parameters

	    if (type_parameter->identifier != NULL)
		identifier = type_parameter->identifier->any_token->token;

	    member__type_template_parameter->is_template = true;

	    abortIf (self->template_declaration_states.isEmpty ());
	    CppParser_Impl::TemplateDeclarationState &template_declaration_state =
		    self->template_declaration_states.last->data;

	    // TODO Steal parameters in forwards-only mode.
	    {
		List< Ref<Cpp::Member> >::DataIterator param_iter (template_declaration_state.template_parameters);
		while (!param_iter.done ()) {
		    Ref<Cpp::Member> &param_member = param_iter.next ();
		    member__type_template_parameter->template_parameters.append (param_member);
		}
	    }
	} break;
	default:
	    abortIfReached ();
    }

    if (identifier.getLength () > 0) {
	member__type_template_parameter->name = grab (new Cpp::Name);
	// TODO No new
	member__type_template_parameter->name->name_str = grab (new String (identifier));
    }

  // TODO Make this the right way

    if (!member__type_template_parameter->name.isNull ()) {
	Ref<DeclarationDesc> declaration_desc =
		grab (new DeclarationDesc);
	declaration_desc->member = member__type_template_parameter;

	abortIf (self->declaration_acceptors.isEmpty ());
	Ref< Scruffy::Acceptor<DeclarationDesc> > &declaration_acceptor =
		self->declaration_acceptors.last->data;
	declaration_acceptor->accept (declaration_desc);
    }

    return true;
}

#if 0
namespace {
class Acceptor_TemplateArgument : public Scruffy::Acceptor<DeclarationDesc>
{
private:
    CppParser_Impl::TemplateIdState &template_id_state;
    CheckpointTracker &checkpoint_tracker;

    CheckpointTracker::CheckpointKey checkpoint_key;

public:
    void accept (DeclarationDesc * const declaration_desc)
    {
	abortIf (declaration_desc == NULL ||
		 !declaration_desc->isSane ());

	abortIf (declaration_desc->hasNestedNameSpecifier ());

#error

	sd

	template_id_state.template_arguments.append (declaration_desc->member);
	checkpoint_tracker.addBoundCancellable (
		createLastElementCancellable (template_id_state.template_arguments),
#if 0
		grab (new Cancellable_ListElement< Ref<Cpp::TemplateArgument> > (
			      template_id_state.template_arguments,
			      template_id_state.template_arguments.last)),
#endif
		checkpoint_key);
    }

    Acceptor_TemplateArgument (CppParser_Impl::TemplateIdState &template_id_state,
			       CheckpointTracker &checkpoint_tracker)
	: template_id_state (template_id_state),
	  checkpoint_tracker (checkpoint_tracker)
    {
	checkpoint_key = checkpoint_tracker.getCurrentCheckpoint ();
    }
};
}
#endif

#if 0
// Deprecated in favor of IdExpressionState
bool
cpp_begin_id (CppElement    * const /* cpp_element */,
	      ParserControl * const /* parser_control */,
	      void          * const _self)
{
    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    CppParser_Impl::IdState &id_state = self->id_states.appendEmpty ()->data;
    self->checkpoint_tracker.addUnconditionalCancellable (
	    grab (new Cancellable_ListElement<CppParser_Impl::IdState> (
			  self->id_states,
			  self->id_states.last)));

    id_state.checkpoint_key = self->checkpoint_tracker.getCurrentCheckpoint ();

    return true;
}
#endif

bool
cpp_begin_template_id (Cpp_TemplateId * const /* template_id */,
		       ParserControl  * const /* parser_control */,
		       void           * const _self)
{
    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    CppParser_Impl::TemplateIdState &template_id_state = self->template_id_states.appendEmpty ()->data;
    self->checkpoint_tracker.addUnconditionalCancellable (
	    grab (new Cancellable_ListElement<CppParser_Impl::TemplateIdState> (
			  self->template_id_states,
			  self->template_id_states.last)));

    template_id_state.checkpoint_key = self->checkpoint_tracker.getCurrentCheckpoint ();

#if 0
    self->declaration_acceptors.append (
	    grab (new Acceptor_TemplateArgument (
			  template_id_state,
			  self->checkpoint_tracker)));
    self->checkpoint_tracker.addUnconditionalCancellable (
	    createLastElementCancellable (self->declaration_acceptors));
#if 0
	    grab (new Cancellable_ListElement< Ref< CppParser_Impl::Acceptor<CppParser_Impl::DeclarationDesc> > > (
			  self->declaration_acceptors,
			  self->declaration_acceptors.last)));
#endif
#endif

    return true;
}

bool
cpp_accept_template_id (Cpp_TemplateId * const template_id,
			ParserControl  * const /* parser_control */,
			void           * const _self)
{
    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    CppParser_Impl::TemplateIdState &template_id_state = self->template_id_states.getLast ();

    // TODO Lookup existing template ids in a sane way (consider all templates with this name, not just one).
    //      What does the standard say on this?
    Cpp::Member * const member = self->lookupType (template_id->templateName->identifier->any_token->token);
    // The member should have already been accepted as a template-name.
    abortIf (member == NULL);
    abortIf (!member->isTemplate ());

    switch (member->getType ()) {
	case Cpp::Member::t_Type: {
	    if (!member->type_desc->isClass ())
		abortIfReached ();

	    Cpp::TypeDesc_Class * const type_desc__class =
		    static_cast <Cpp::TypeDesc_Class*> (member->type_desc.ptr ());

	    Ref<Cpp::ClassTemplateInstance> class_template_instance = grab (new Cpp::ClassTemplateInstance);
	    class_template_instance->template_class = type_desc__class->class_;
	    {
		List< Ref<Cpp::TemplateArgument> >::DataIterator arg_iter (template_id_state.template_arguments);
		while (!arg_iter.done ()) {
		    Ref<Cpp::TemplateArgument> &arg = arg_iter.next ();
		    class_template_instance->template_arguments.append (arg);
		}
	    }

	    class_template_instance->class_ = grab (new Cpp::Class);
	    class_template_instance->class_->parent_container = type_desc__class->class_->parent_container;
	    class_template_instance->class_->name = type_desc__class->class_->name;

	    type_desc__class->class_->template_instances.append (class_template_instance);
	    self->checkpoint_tracker.addBoundCancellable (
		    createLastElementCancellable (type_desc__class->class_->template_instances),
		    template_id_state.checkpoint_key);

// TODO If the brach containing the temlate-id is discarded, then we'll have
// a dangling template instance. Implement reference counting on template
// instances to avoid that. Once reference count (use count) becomes zero,
// we remove the instance from the list of instances.

	    template_id->user_obj = static_cast <Cpp::TemplateInstance*> (class_template_instance);
	} break;
	case Cpp::Member::t_Function: {
	    Cpp::Member_Function * const member__function =
		    static_cast <Cpp::Member_Function*> (member);

	    Ref<Cpp::FunctionTemplateInstance> function_template_instance = grab (new Cpp::FunctionTemplateInstance);
	    function_template_instance->template_function = member__function->function;
	    {
		List< Ref<Cpp::TemplateArgument> >::DataIterator arg_iter (template_id_state.template_arguments);
		while (!arg_iter.done ()) {
		    Ref<Cpp::TemplateArgument> &arg = arg_iter.next ();
		    function_template_instance->template_arguments.append (arg);
		}
	    }

	    member__function->function->template_instances.append (function_template_instance);
	    self->checkpoint_tracker.addBoundCancellable (
		    createLastElementCancellable (member__function->function->template_instances),
		    template_id_state.checkpoint_key);

// TODO Implement reference counting on template instances. See the comment above.

	    template_id->user_obj = static_cast <Cpp::TemplateInstance*> (function_template_instance);
	} break;
	default:
	    abortIfReached ();
    }

#if 0
// Deprecated

// TODO The following is robably deprecated.

  // Here we set a flag indicating that the id being parsed is a template-id.

    CppParser_Impl::IdState &id_state = self->id_states.getLast ();

    abortIf (id_state.is_template_id);
    id_state.is_template_id = true;
    id_state.template_name = template_id->templateName->identifier->any_token->token;
    self->checkpoint_tracker.addBoundCancellable (
	    createValueCancellable (id_state.is_template_id, false),
	    id_state.checkpoint_key);
#endif

    return true;
}

bool
cpp_accept_template_argument (Cpp_TemplateArgument * const template_argument,
			      ParserControl        * const /* parser_control */,
			      void                 * const _self)
{
    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    CppParser_Impl::TemplateIdState &template_id_state = self->template_id_states.getLast ();

    switch (template_argument->template_argument_type) {
	case Cpp_TemplateArgument::t_AssignmentExpression: {
	    abortIf (self->expression_states.isEmpty ());
	    CppParser_Impl::ExpressionState &expression_state =
		    self->expression_states.getLast ();

	    Ref<Cpp::TemplateArgument_Expression> template_argument__expression =
		    grab (new Cpp::TemplateArgument_Expression);

	    abortIf (expression_state.expressions.isEmpty ());
	    template_argument__expression->expression = expression_state.expressions.getLast ().expression;

	    template_id_state.template_arguments.append (template_argument__expression);
	    self->checkpoint_tracker.addBoundCancellable (
		    createLastElementCancellable (template_id_state.template_arguments),
		    template_id_state.checkpoint_key);
	} break;
	case Cpp_TemplateArgument::t_TypeId: {
	    abortIf (self->typeid_states.isEmpty ());
	    CppParser_Impl::TypeidState &typeid_state =
		    self->typeid_states.getLast ();

	    Ref<Cpp::TemplateArgument_Type> template_argument__type =
		    grab (new Cpp::TemplateArgument_Type);

	    abortIf (typeid_state.type_desc.isNull ());
	    template_argument__type->type_desc = typeid_state.type_desc;

	    template_id_state.template_arguments.append (template_argument__type);
	    self->checkpoint_tracker.addBoundCancellable (
		    createLastElementCancellable (template_id_state.template_arguments),
		    template_id_state.checkpoint_key);
	} break;
	case Cpp_TemplateArgument::t_IdExpression: {
	    CppParser_Impl::IdState &id_expression_state = self->id_states.getLast ();

	    Ref<Cpp::TemplateArgument> template_argument__type =
		    grab (new Cpp::TemplateArgument_Type);

	    // TODO

	    abortIfReached ();
	} break;
	default:
	    abortIfReached ();
    }

    return true;
}

bool
cpp_begin_function_definition (CppElement    * const /* cpp_element */,
			       ParserControl * const /* parser_control */,
			       void          * const _self)
{
  FUNC_NAME (
    static char const * const _func_name = "cpp_begin_function_definition";
  )

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    DeclarationState &declaration_state = self->declaration_states.getLast ();

    {
      // We should be in the process of parsing a function definition.

	abortIf (declaration_state.member.isNull () ||
		 declaration_state.member->type_desc.isNull ());

	if (declaration_state.member->getType () != Cpp::Member::t_Function) {
	    errf->print ("Not a function").pendl ();
	    abortIfReached ();
	}
    }

    Cpp::Member_Function * const member__function =
	    static_cast <Cpp::Member_Function*> (declaration_state.member.ptr ());

    if (member__function->function->got_definition) {
      // We've already created a FunctionDefinition object, which means that
      // this is not a toplevel block of the function's body.

	DEBUG (
	    errf->print (_func_name).print (": already got definition: member__function: 0x").printHex ((Uint64) member__function).pendl ();
	)

	return true;

#if 0
	errf->print ("Function redefinition").pendl ();
	abortIfReached ();
#endif
    }

    // No cancellable required.
    abortIf (member__function->function.isNull () ||
	     member__function->function->got_definition);
    member__function->function->got_definition = true;

    DEBUG (
	errf->print (_func_name).print (": member__function: 0x").printHex ((Uint64) member__function).pendl ();
    )

    member__function->function->mem_initializers.steal (&declaration_state.mem_initializers,
							declaration_state.mem_initializers.first,
							declaration_state.mem_initializers.last,
							member__function->function->mem_initializers.last,
							GenericList::StealAppend);

    return true;
}

bool
cpp_simple_declaration__function_definition_jump (ParserElement * const /* parser_element */,
						  void          * const _self)
{
//    static char const * const _func_name = "cpp_simple_declaration__function_definition_jump";

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    DeclarationState &declaration_state = self->declaration_states.getLast ();

    if (declaration_state.num_declarators != 1)
	return false;

    abortIf (declaration_state.member.isNull () ||
	     declaration_state.member->type_desc.isNull ());
    if (!declaration_state.member->type_desc->isFunction ())
	return false;

    errf->print ("--- FUNCTION DEFINITION JUMP ---").pendl ();

    return true;
}

namespace {
class Acceptor_FunctionParameter : public Scruffy::Acceptor<DeclarationDesc>
{
private:
    CppParser_Impl &self;
    FunctionDeclaratorState &function_declarator_state;

    CheckpointTracker::CheckpointKey checkpoint_key;

public:
    void accept (DeclarationDesc * const declaration_desc)
    {
	DEBUG (
	    errf->print ("--- ACCEPTING FUNCTION PARAMETER [0x").printHex ((UintPtr) declaration_desc->member.ptr ()).print ("] ---").pendl ();
	)

	abortIf (declaration_desc == NULL ||
		 !declaration_desc->isSane ());

	abortIf (declaration_desc->hasNestedNameSpecifier ());

	function_declarator_state.function_parameters.append (declaration_desc->member);
	{
	    Ref< Cancellable_ListElement< Ref<Cpp::Member> > > cancellable =
		    grab (new Cancellable_ListElement< Ref<Cpp::Member> > (
				  function_declarator_state.function_parameters,
				  function_declarator_state.function_parameters.last));
	    self.checkpoint_tracker.addBoundCancellable (cancellable, checkpoint_key);
	}

#if 0
// Deprecated

      // Setting declaration_state.member

	abortIf (self.declaration_states.isEmpty ());
	CppParser_Impl::DeclarationState &declaration_state =
		self.declaration_states.last->data;

	abortIf (!declaration_state.member.isNull ());
	declaration_state.member = declaration_desc->member;
	self.checkpoint_tracker.addBoundCancellable (
		grab (new Cancellable_Ref<Cpp::Member> (declaration_state.member)),
		declaration_state.checkpoint_key);
#endif
    }

    Acceptor_FunctionParameter (CppParser_Impl &self,
				FunctionDeclaratorState &function_declarator_state)
	: self (self),
	  function_declarator_state (function_declarator_state)
    {
	checkpoint_key = self.checkpoint_tracker.getCurrentCheckpoint ();
    }
};
}

bool
cpp_begin_function_parameters (CppElement    * const /* cpp_element */,
			       ParserControl * const /* parser_control */,
			       void          * const _self)
{
  FUNC_NAME (
    static char const * const _func_name = "CppParser.cpp_begin_function_parameters";
  )

    DEBUG (
	errf->print ("--- CPP_BEGIN_FUNCTION_PARAMETERS ---").pendl ();
    )

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    DeclarationState &declaration_state = self->declaration_states.getLast ();

    declaration_state.function_declarator_states.appendEmpty ();
    FunctionDeclaratorState &function_declarator_state =
	    declaration_state.function_declarator_states.getLast ();
    DEBUG (
	errf->print (_func_name).print (": new function_declarator_state 0x").printHex ((Uint64) &function_declarator_state).print (", element 0x").printHex ((Uint64) declaration_state.function_declarator_states.last).pendl ();
    )
    self->checkpoint_tracker.addBoundCancellable (
	    grab (new Cancellable_ListElement<FunctionDeclaratorState> (
			  declaration_state.function_declarator_states,
			  declaration_state.function_declarator_states.last)),
	    declaration_state.checkpoint_key);

    self->declaration_acceptors.append (
	    grab (new Acceptor_FunctionParameter (
			  *self,
			  function_declarator_state)));
    self->checkpoint_tracker.addUnconditionalCancellable (
	    grab (new Cancellable_ListElement< Ref< Scruffy::Acceptor<DeclarationDesc> > > (
			  self->declaration_acceptors,
			  self->declaration_acceptors.last)));

    return true;
}

bool
cpp_accept_function_declarator (Cpp_DirectDeclarator    * const direct_declarator,
				ParserControl           * const /* parser_control */,
				void                    * const _self)
{
  FUNC_NAME (
    static char const * const _func_name = "Scruffy.(CppParser_Impl).cpp_accept_function_declarator";
  )

    DEBUG (
	errf->print (_func_name).pendl ();
    )

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    abortIf (direct_declarator->direct_declarator_type != Cpp_DirectDeclarator::t_Function);
    Cpp_DirectDeclarator_Function * const direct_declarator__function =
	    static_cast <Cpp_DirectDeclarator_Function*> (direct_declarator);

    DeclarationState &declaration_state = self->declaration_states.getLast ();

    abortIf (declaration_state.function_declarator_states.isEmpty ());
    FunctionDeclaratorState &function_declarator_state =
	    declaration_state.function_declarator_states.getLast ();

    DEBUG (
	errf->print (_func_name).print (": function_declarator_state 0x").printHex ((Uint64) &function_declarator_state).pendl ();
    )

    Ref<DeclaratorState> declarator_state = grab (new DeclaratorState);
    if (direct_declarator__function->directDeclarator != NULL) {
      // Skipping the very last function declarator state.
	CppParser_DoParseDirectDeclarator_Data data;
	data.cur_function_declarator_state_el = declaration_state.function_declarator_states.last;
	DEBUG (
	    errf->print (_func_name).print (": _state_el: 0x").printHex ((Uint64) data.cur_function_declarator_state_el).pendl ();
	    if (data.cur_function_declarator_state_el != NULL)
		errf->print (_func_name).print (": previous el: 0x").printHex ((Uint64) data.cur_function_declarator_state_el->previous).pendl ();
	)
	cpp_parser_do_parse_direct_declarator (self, declarator_state, direct_declarator__function->directDeclarator, &data);
    }
//    Ref<CppParser_Impl::DeclaratorState> declarator_state =
//	    cpp_parser_parse_direct_declarator (self, direct_declarator__function->directDeclarator);
    abortIf (declarator_state.isNull ());

    Ref<Cpp::TypeDesc_Function> type_desc__function = grab (new Cpp::TypeDesc_Function);

    type_desc__function->type_atoms.steal (&declarator_state->type_atoms,
					   declarator_state->type_atoms.first,
					   declarator_state->type_atoms.last,
					   type_desc__function->type_atoms.last,
					   GenericList::StealAppend);

    {
	List< Ref<Cpp::Member> >::DataIterator iter (function_declarator_state.function_parameters);
	while (!iter.done ()) {
	    Ref<Cpp::Member> &parameter_member = iter.next ();
	    type_desc__function->parameters.append (parameter_member);
	}
    }

    *declarator_state->cur_type = type_desc__function;
    declarator_state->cur_type = &type_desc__function->return_type;

    abortIf (!function_declarator_state.declarator_state.isNull ());
    function_declarator_state.declarator_state = declarator_state;

    DEBUG (
	errf->print (_func_name).print (": direct_declarator: 0x").print ((Uint64) direct_declarator).print (", ")
		     .print ("declarator_state: ").print ((Uint64) declarator_state.ptr ()).pendl ();
    )

    return true;
}

bool
cpp_MemInitializer_accept_func (Cpp_MemInitializer * const _mem_initializer,
				ParserControl      * const /* parser_control */,
				void               * const _self)
{
    if (_mem_initializer == NULL)
	return true;

    if (_mem_initializer->mem_initializer_type != Cpp_MemInitializer::t_Default) {
      // "class-prematch" mode
	abortIf (_mem_initializer->mem_initializer_type != Cpp_MemInitializer::t_ClassPrematch);
	return true;
    }

    Cpp_MemInitializer_Default const * const mem_initializer =
	    static_cast <Cpp_MemInitializer_Default const *> (_mem_initializer);

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    Ref<Cpp::MemInitializer> cpp_mem_initializer;
    {
	Cpp_MemInitializerId const * const mem_initializer_id =
		mem_initializer->memInitializerId;

	// TODO Name

	switch (mem_initializer_id->mem_initializer_id_type) {
	    case Cpp_MemInitializerId::t_ClassName: {
		cpp_mem_initializer = grab (new Cpp::MemInitializer_Base);
	    } break;
	    case Cpp_MemInitializerId::t_Identifier: {
		cpp_mem_initializer = grab (new Cpp::MemInitializer_Data);
	    } break;
	    default:
		abortIfReached ();
	}
    }

    abortIf (self->expression_states.isEmpty ());
    CppParser_Impl::ExpressionState &expression_state = self->expression_states.last->data;

    {
	List<CppParser_Impl::ExpressionEntry>::DataIterator iter (expression_state.expressions);
	while (!iter.done ()) {
	    CppParser_Impl::ExpressionEntry &expression_entry = iter.next ();
	    cpp_mem_initializer->arguments.append (expression_entry.expression);
	}
    }

  // Storing mem-initializer for future appending to FunctionDefinition.

    DeclarationState &declaration_state = self->declaration_states.getLast ();

    declaration_state.mem_initializers.append (cpp_mem_initializer);
    {
	Ref< Cancellable_ListElement< Ref<Cpp::MemInitializer> > > cancellable =
		grab (new Cancellable_ListElement< Ref<Cpp::MemInitializer> > (declaration_state.mem_initializers,
									       declaration_state.mem_initializers.last));
	self->checkpoint_tracker.addBoundCancellable (cancellable, declaration_state.checkpoint_key);
    }

    return true;
}

void
cpp_BaseSpecifier_accept_func (Cpp_BaseSpecifier * const base_specifier,
			       ParserControl     * const /* parser_control */,
			       void              * const _self)
{
    if (base_specifier == NULL)
	return;

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    DeclarationState &declaration_state = self->declaration_states.getLast ();

    Ref<Cpp::Class::ParentEntry> parent_entry = grab (new Cpp::Class::ParentEntry);
    parent_entry->access_rights = Cpp::Class::Private;

    // TODO 'virtual', access rights

    switch (base_specifier->base_specifier_type) {
	case Cpp_BaseSpecifier::t_Base: {
	    Cpp_BaseSpecifier_Base const * const base_specifier__base =
		    static_cast <Cpp_BaseSpecifier_Base const *> (base_specifier);

	    parent_entry->type_desc__class = getTypeForClassName (base_specifier__base->className);
	} break;
	case Cpp_BaseSpecifier::t_Virtual: {
	    Cpp_BaseSpecifier_Virtual const * const base_specifier__virtual =
		    static_cast <Cpp_BaseSpecifier_Virtual const *> (base_specifier);

	    parent_entry->type_desc__class = getTypeForClassName (base_specifier__virtual->className);
	} break;
	case Cpp_BaseSpecifier::t_AccessSpecifier: {
	    Cpp_BaseSpecifier_AccessSpecifier const * const base_specifier__access_specifier =
		    static_cast <Cpp_BaseSpecifier_AccessSpecifier const *> (base_specifier);

	    parent_entry->type_desc__class = getTypeForClassName (base_specifier__access_specifier->className);
	} break;
	default:
	    abortIfReached ();
    }

    if (parent_entry->type_desc__class.isNull ()) {
	errf->print ("parent class not declared").pendl ();
	abortIfReached ();
    }

    abortIf (parent_entry->type_desc__class->class_.isNull ());
    if (!parent_entry->type_desc__class->class_->got_definition) {
	errf->print ("no definition for parent class").pendl ();
	abortIfReached ();
    }

    DEBUG (
	errf->print ("--- APPENDING PARENT ---").pendl ();
    )
    declaration_state.class_parents.append (parent_entry);
    {
	Ref< Cancellable_ListElement< Ref<Cpp::Class::ParentEntry> > > cancellable =
		grab (new Cancellable_ListElement< Ref<Cpp::Class::ParentEntry> > (
			      declaration_state.class_parents,
			      declaration_state.class_parents.last));
	self->checkpoint_tracker.addBoundCancellable (cancellable, declaration_state.checkpoint_key);
    }
}

bool
cpp_LabeledStatement_accept_func (Cpp_LabeledStatement * const labeled_statement,
				  ParserControl        * const /* parser_control */,
				  void                 * const _self)
{
    if (labeled_statement == NULL)
	return true;

  FUNC_NAME (
    static char const * const _func_name = "Scruffy.(CppParser_Impl).cpp_LabeledStatement_accept_func";
  )

    DEBUG (
	errf->print (_func_name).pendl ();
    )

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    abortIf (self->statement_states.last == NULL ||
	     self->statement_states.last->previous == NULL);

//    CppParser_Impl::StatementState &statement_state = self->statement_states.last->data;
    CppParser_Impl::StatementState &target_statement_state = self->statement_states.last->previous->data;

    switch (labeled_statement->labeled_statement_type) {
	case Cpp_LabeledStatement::t_Label: {
	    Cpp_LabeledStatement_Label const * const labeled_statement__label =
		    static_cast <Cpp_LabeledStatement_Label const *> (labeled_statement);

	    Ref<Cpp::Statement_Label> statement__label = grab (new Cpp::Statement_Label);
	    abortIf (labeled_statement__label->identifier == NULL);
	    statement__label->label = grab (new String (labeled_statement__label->identifier->any_token->token));

	    // TODO substatement

	    self->acceptStatement (target_statement_state, statement__label);
	} break;
	case Cpp_LabeledStatement::t_Case: {
//	    Cpp_LabeledStatement_Case const * const labeled_statement__label =
//		    static_cast <Cpp_LabeledStatement_Case const *> (labeled_statement);

	    // TODO
	    abortIfReached ();
	} break;
	case Cpp_LabeledStatement::t_Default: {
//	    Cpp_LabeledStatement_Default const * const labeled_statement__default =
//		    static_cast <Cpp_LabeledStatement_Default const *> (labeled_statement);

	    // TODO
	    abortIfReached ();
	} break;
	default:
	    abortIfReached ();
    }

    return true;
}

void
cpp_ExpressionStatement_accept_func (Cpp_ExpressionStatement * const expression_statement,
				     ParserControl           * const /* parser_control */,
				     void                    * const _self)
{
    if (expression_statement == NULL)
	return;

  FUNC_NAME (
    static char const * const _func_name = "Scruffy.(CppParser_Impl).cpp_ExpressionStatement_accept_func";
  )

    DEBUG (
	errf->print (_func_name).pendl ();
    )

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    abortIf (self->statement_states.isEmpty ());
    CppParser_Impl::StatementState &target_statement_state = self->statement_states.last->data;

    abortIf (self->expression_states.isEmpty ());
    CppParser_Impl::ExpressionState &expression_state = self->expression_states.last->data;

    Ref<Cpp::Statement_Expression> statement__expression = grab (new Cpp::Statement_Expression);

    // There may be no expressions (empty operator ";").
    if (!expression_state.expressions.isEmpty ()) {
	// Note: Due to left-recursive grammars, 'expression_state.expressions' may contain
	// more than one element, with the target expression being at the end of the list. - ? FIXME: Is this true?
	statement__expression->expression = expression_state.expressions.last->data.expression;
    }

    self->acceptStatement (target_statement_state, statement__expression);
}

bool
cpp_accept_compound_statement (Cpp_CompoundStatement * const /* compound_statement */,
			       ParserControl         * const /* parser_control */,
			       void                  * const _self)
{
    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    Ref<Cpp::Statement_Compound> statement__compound = grab (new Cpp::Statement_Compound);

    abortIf (self->statement_states.last == NULL ||
	     self->statement_states.last->previous == NULL);

    CppParser_Impl::StatementState &statement_state = self->statement_states.last->data;
    CppParser_Impl::StatementState &target_statement_state = self->statement_states.last->previous->data;

    statement__compound->statements.steal (&statement_state.statements,
					   statement_state.statements.first,
					   statement_state.statements.last,
					   statement__compound->statements.last,
					   GenericList::StealAppend);

    self->acceptStatement (target_statement_state, statement__compound);

    return true;
}

bool
cpp_begin_temporal_namespace (CppElement    * const /* cpp_element */,
			      ParserControl * const /* parser_control */,
			      void          * const _self)
{
    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    self->beginTemporalNamespace ();

    return true;
}

bool
cpp_begin_statement (CppElement    * const /* cpp_element */,
		     ParserControl * const /* parser_control */,
		     void          * const _self)
{
    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    CppParser_Impl::StatementState &statement_state = self->statement_states.appendEmpty ()->data;
    statement_state.checkpoint_key = self->checkpoint_tracker.getCurrentCheckpoint ();
    {
	Ref< Cancellable_ListElement<CppParser_Impl::StatementState> > cancellable =
		grab (new Cancellable_ListElement<CppParser_Impl::StatementState> (
			      self->statement_states,
			      self->statement_states.last));
	self->checkpoint_tracker.addUnconditionalCancellable (cancellable);
    }

    return true;
}

void
CppParser_Impl::acceptStatement (StatementState &target_state,
				 Cpp::Statement * const statement)
{
    target_state.statements.append (statement);
    checkpoint_tracker.addBoundCancellable (
	    grab (new Cancellable_ListElement< Ref<Cpp::Statement> > (target_state.statements)),
	    target_state.checkpoint_key);
}

namespace {

    class Acceptor_ConditionMember : public Scruffy::Acceptor<DeclarationDesc>
    {
    private:
	CppParser_Impl &self;
	CppParser_Impl::ConditionState &condition_state;
	CheckpointTracker::CheckpointKey checkpoint_key;

    public:
	void accept (DeclarationDesc * const declaration_desc)
	{
	    abortIf (declaration_desc == NULL ||
		     !declaration_desc->isSane ());

	    // TODO Check that a suitable member has been declared (guaranteed by the syntax?).

	    abortIf (self.temporal_namespaces.isEmpty () ||
		     self.temporal_namespaces.last->data.isNull ());
	    self.acceptDeclaration (*self.temporal_namespaces.last->data, *declaration_desc, checkpoint_key);

	    if (!declaration_desc->member->isObject ())
		return;

	    condition_state.member = declaration_desc->member;
	    self.checkpoint_tracker.addBoundCancellable (
		    grab (new Cancellable_Ref<Cpp::Member> (condition_state.member)),
		    checkpoint_key);
	}

	void cancel ()
	{
	    abortIfReached ();
	}

	Acceptor_ConditionMember (CppParser_Impl                   &self,
				  CppParser_Impl::ConditionState   &condition_state,
				  CheckpointTracker::CheckpointKey  checkpoint_key)
	    : self (self),
	      condition_state (condition_state),
	      checkpoint_key (checkpoint_key)
	{
	}
    };

}

bool
cpp_begin_condition (CppElement    * const /* cpp_element */,
		     ParserControl * const /* parser_control */,
		     void          * const _self)
{
    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    CppParser_Impl::ConditionState &condition_state = self->condition_states.appendEmpty ()->data;
    condition_state.checkpoint_key = self->checkpoint_tracker.getCurrentCheckpoint ();
    self->checkpoint_tracker.addUnconditionalCancellable (
	    grab (new Cancellable_ListElement<CppParser_Impl::ConditionState> (self->condition_states)));

    self->declaration_acceptors.append (
	    grab (new Acceptor_ConditionMember (*self,
						condition_state,
						self->checkpoint_tracker.getCurrentCheckpoint ())));
    self->checkpoint_tracker.addUnconditionalCancellable (
	    createLastElementCancellable (self->declaration_acceptors));

    return true;
}

namespace {

    class Acceptor_ForInitStatementMember : public Scruffy::Acceptor<DeclarationDesc>
    {
    private:
	CppParser_Impl &self;
	CppParser_Impl::ForInitStatementState &for_init_statement_state;
	CheckpointTracker::CheckpointKey checkpoint_key;

    public:
	void accept (DeclarationDesc * const declaration_desc)
	{
	    abortIf (declaration_desc == NULL ||
		     !declaration_desc->isSane ());

	    abortIf (self.temporal_namespaces.isEmpty () ||
		     self.temporal_namespaces.last->data.isNull ());
	    self.acceptDeclaration (*self.temporal_namespaces.last->data, *declaration_desc, checkpoint_key);

	    if (!declaration_desc->member->isObject ()) {
	      // "for (class A {} a; ; );"
		return;
	    }

	    for_init_statement_state.member = declaration_desc->member;
	    self.checkpoint_tracker.addBoundCancellable (
		    grab (new Cancellable_Ref<Cpp::Member> (for_init_statement_state.member)),
		    checkpoint_key);
	}

	void cancel ()
	{
	    abortIfReached ();
	}

	Acceptor_ForInitStatementMember (CppParser_Impl                        &self,
					 CppParser_Impl::ForInitStatementState &for_init_statement_state,
					 CheckpointTracker::CheckpointKey       checkpoint_key)
	    : self (self),
	      for_init_statement_state (for_init_statement_state),
	      checkpoint_key (checkpoint_key)
	{
	}
    };

}

bool
cpp_begin_for_init_statement (CppElement    * const /* cpp_element */,
			      ParserControl * const /* parser_control */,
			      void          * const _self)
{
    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    CppParser_Impl::ForInitStatementState &for_init_statement_state = self->for_init_statement_states.appendEmpty ()->data;
    for_init_statement_state.checkpoint_key = self->checkpoint_tracker.getCurrentCheckpoint ();
    self->checkpoint_tracker.addUnconditionalCancellable (
	    createLastElementCancellable (self->for_init_statement_states));

    self->declaration_acceptors.append (
	    grab (new Acceptor_ForInitStatementMember (*self,
						       for_init_statement_state,
						       self->checkpoint_tracker.getCurrentCheckpoint ())));
    self->checkpoint_tracker.addUnconditionalCancellable (
	    createLastElementCancellable (self->declaration_acceptors));

    return true;
}

bool
cpp_accept_for_init_statement (Cpp_ForInitStatement * const for_init_statement,
			       ParserControl        * const /* parser_control */,
			       void                 * const _self)
{
    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    abortIf (self->for_init_statement_states.isEmpty ());
    CppParser_Impl::ForInitStatementState &for_init_statement_state = self->for_init_statement_states.last->data;

    switch (for_init_statement->for_init_statement_type) {
	case Cpp_ForInitStatement::t_Declaration: {
	    DeclarationState &declaration_state = self->declaration_states.getLast ();

	    if (declaration_state.member.isNull ())
		break;

	    for_init_statement_state.member = declaration_state.member;
	    self->checkpoint_tracker.addBoundCancellable (
		    grab (new Cancellable_Ref<Cpp::Member> (for_init_statement_state.member)),
		    for_init_statement_state.checkpoint_key);
	} break;
	case Cpp_ForInitStatement::t_Expression: {
	    abortIf (self->statement_states.isEmpty ());
	    CppParser_Impl::StatementState &statement_state = self->statement_states.last->data;

	    if (!statement_state.statements.isEmpty ()) {
		Cpp::Statement &statement = *statement_state.statements.last->data;
		abortIf (statement.getType () != Cpp::Statement::t_Expression);

		Cpp::Statement_Expression &statement__expression =
			static_cast <Cpp::Statement_Expression&> (statement);

		for_init_statement_state.expression = statement__expression.expression;
		self->checkpoint_tracker.addBoundCancellable (
			grab (new Cancellable_Ref<Cpp::Expression> (for_init_statement_state.expression)),
			for_init_statement_state.checkpoint_key);
	    }
	} break;
	default:
	    abortIfReached ();
    }

    return true;
}

bool
cpp_begin_initializer (CppElement    * const /* cpp_element */,
		       ParserControl * const /* parser_control */,
		       void          * const _self)
{
    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    self->beginInitializer ();

    return true;
}

bool
cpp_accept_initializer (Cpp_Initializer * const cpp_initializer,
			ParserControl   * const /* parser_control */,
			void            * const _self)
{
    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    abortIf (self->initializer_states.isEmpty ());
//    CppParser_Impl::InitializerState &initializer_state = self->initializer_states.last->data;

    abortIf (self->expression_states.isEmpty ());
    CppParser_Impl::ExpressionState &expression_state = self->expression_states.last->data;

    Ref<Cpp::Initializer> _initializer;
    switch (cpp_initializer->initializer_type) {
	case Cpp_Initializer::t_InitializerClause: {
	    Cpp_Initializer_InitializerClause const * const initializer__initializer_clause =
		    static_cast <Cpp_Initializer_InitializerClause const *> (cpp_initializer);

	    Cpp_InitializerClause const * const initializer_clause =
		    initializer__initializer_clause->initializerClause;
	    abortIf (initializer_clause == NULL);

	    switch (initializer_clause->initializer_clause_type) {
		case Cpp_InitializerClause::t_Expression: {
		    Ref<Cpp::Initializer_Assignment> initializer = grab (new Cpp::Initializer_Assignment);
		    _initializer = initializer;

		    abortIf (expression_state.expressions.isEmpty () ||
			     expression_state.expressions.first != expression_state.expressions.last);

		    initializer->expression = expression_state.expressions.last->data.expression;
		} break;
		case Cpp_InitializerClause::t_InitializerList: {
		    Ref<Cpp::Initializer_InitializerList> initializer = grab (new Cpp::Initializer_InitializerList);
		    _initializer = initializer;

		    {
			List<CppParser_Impl::ExpressionEntry>::DataIterator iter (expression_state.expressions);
			while (!iter.done ()) {
			    CppParser_Impl::ExpressionEntry &entry = iter.next ();
			    initializer->expressions.append (entry.expression);
			}
		    }

#if 0
		    initializer->expressions.steal (&expression_state.expressions,
						    expression_state.expressions.first,
						    expression_state.expressions.last,
						    initializer->expressions.last,
						    GenericList::StealAppend);
#endif
		} break;
		default:
		    abortIfReached ();
	    }
	} break;
	case Cpp_Initializer::t_ExpressionList: {
	    Ref<Cpp::Initializer_Constructor> initializer = grab (new Cpp::Initializer_Constructor);
	    _initializer = initializer;

	    {
		List<CppParser_Impl::ExpressionEntry>::DataIterator iter (expression_state.expressions);
		while (!iter.done ()) {
		    CppParser_Impl::ExpressionEntry &entry = iter.next ();
		    initializer->expressions.append (entry.expression);
		}
	    }

#if 0
	    initializer->expressions.steal (&expression_state.expressions,
					    expression_state.expressions.first,
					    expression_state.expressions.last,
					    initializer->expressions.last,
					    GenericList::StealAppend);
#endif
	} break;
	default:
	    abortIfReached ();
    }
    abortIf (_initializer.isNull ());

    self->acceptInitializer (_initializer);

    return true;
}

bool
cpp_accept_init_declarator_initializer (Cpp_InitDeclarator * const /* init_declarator */,
					ParserControl      * const /* parser_control */,
					void               *_self)
{
    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    abortIf (self->initializer_states.isEmpty ());
    CppParser_Impl::InitializerState &initializer_state = self->initializer_states.last->data;

    if (initializer_state.initializer.isNull ())
	return true;

    DeclarationState &declaration_state = self->declaration_states.getLast ();

    if (declaration_state.member.isNull ()) {
	errf->print ("Misplaced initializer").pendl ();
	abortIfReached ();
    }

    switch (declaration_state.member->getType ()) {
	case Cpp::Member::t_Object: {
	    Cpp::Member_Object * const member__object =
		    static_cast <Cpp::Member_Object*> (declaration_state.member.ptr ());

	    member__object->initializer = initializer_state.initializer;
	    self->checkpoint_tracker.addBoundCancellable (
		    grab (new Cancellable_Ref<Cpp::Initializer> (member__object->initializer)),
		    declaration_state.checkpoint_key);
	} break;
	case Cpp::Member::t_DependentObject: {
	    Cpp::Member_DependentObject * const member__dependent_object =
		    static_cast <Cpp::Member_DependentObject*> (declaration_state.member.ptr ());

	    member__dependent_object->initializer = initializer_state.initializer;
	    self->checkpoint_tracker.addBoundCancellable (
		    grab (new Cancellable_Ref<Cpp::Initializer> (member__dependent_object->initializer)),
		    declaration_state.checkpoint_key);
	} break;
	default:
	    errf->print ("Misplaced initializer").pendl ();
	    abortIfReached ();
    }

    return true;
}

bool
cpp_accept_parameter_declaration_declarator_assignment (Cpp_ParameterDeclaration * const /* parameter_declaration */,
							ParserControl            * const /* parser_control */,
							void                     * const _self)
{
    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

//    DEBUG (
//	dump_cpp_ParameterDeclaration (parameter_declaration);
//    )

    DeclarationState &declaration_state = self->declaration_states.getLast ();

    DEBUG (
	errf->print ("--- DECLARATION STATE: 0x").printHex ((UintPtr) &declaration_state).pendl ();
    )
    if (declaration_state.member.isNull ()) {
	errf->print ("Misplaced function parameter initializer").pendl ();
	abortIfReached ();
    }

    abortIf (self->expression_states.isEmpty ());
    CppParser_Impl::ExpressionState &expression_state = self->expression_states.last->data;

    Ref<Cpp::Initializer_Assignment> initializer = grab (new Cpp::Initializer_Assignment);
    abortIf (expression_state.expressions.isEmpty ());
    initializer->expression = expression_state.expressions.last->data.expression;

    switch (declaration_state.member->getType ()) {
	case Cpp::Member::t_Object: {
	    Cpp::Member_Object * const member__object =
		    static_cast <Cpp::Member_Object*> (declaration_state.member.ptr ());

	    member__object->initializer = initializer;
	    self->checkpoint_tracker.addBoundCancellable (
		    grab (new Cancellable_Ref<Cpp::Initializer> (member__object->initializer)),
		    declaration_state.checkpoint_key);
	} break;
	case Cpp::Member::t_DependentObject: {
	    Cpp::Member_DependentObject * const member__dependent_object =
		    static_cast <Cpp::Member_DependentObject*> (declaration_state.member.ptr ());

	    member__dependent_object->initializer = initializer;
	    self->checkpoint_tracker.addBoundCancellable (
		    grab (new Cancellable_Ref<Cpp::Initializer> (member__dependent_object->initializer)),
		    declaration_state.checkpoint_key);
	} break;
	default:
	    errf->print ("Misplaced function parameter initializer").pendl ();
	    abortIfReached ();
    }

    return true;
}

bool
cpp_accept_condition (Cpp_Condition * const condition,
		      ParserControl * const /* parser_control */,
		      void          * _self)
{
    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    abortIf (self->condition_states.isEmpty ());
    CppParser_Impl::ConditionState &condition_state = self->condition_states.last->data;

    abortIf (self->expression_states.isEmpty ());
    CppParser_Impl::ExpressionState &expression_state = self->expression_states.last->data;

    abortIf (expression_state.expressions.isEmpty ());
    Ref<Cpp::Expression> expression = expression_state.expressions.last->data.expression;
    abortIf (expression.isNull ());

    switch (condition->condition_type) {
	case Cpp_Condition::t_Expression: {
	    condition_state.expression = expression;
	} break;
	case Cpp_Condition::t_Declarator: {
	    abortIf (condition_state.member.isNull ());
	    condition_state.expression = expression;
	} break;
	default:
	    abortIfReached ();
    }

    return true;
}

bool
cpp_accept_selection_statement (Cpp_SelectionStatement * const selection_statement,
				ParserControl          * const /* parser_control */,
				void                   * const _self)
{
    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    abortIf (self->condition_states.isEmpty ());
    CppParser_Impl::ConditionState &condition_state = self->condition_states.last->data;

    abortIf (self->statement_states.last == NULL ||
	     self->statement_states.last->previous == NULL);

    CppParser_Impl::StatementState &statement_state = self->statement_states.last->data;
    CppParser_Impl::StatementState &target_statement_state = self->statement_states.last->previous->data;

    Ref<Cpp::Statement_Compound> outer_statement;
    Ref<Cpp::Expression> condition;

    abortIf (condition_state.expression.isNull ());
    if (!condition_state.member.isNull ()) {
      // Faking "if (int a) {}" with "{ int a; if (a) {} }"

	outer_statement = grab (new Cpp::Statement_Compound);

	Ref<Cpp::Initializer_Assignment> initializer = grab (new Cpp::Initializer_Assignment);
	initializer->expression = condition_state.expression;

	switch (condition_state.member->getType ()) {
	    case Cpp::Member::t_Object: {
		Cpp::Member_Object * const member__object =
			static_cast <Cpp::Member_Object*> (condition_state.member.ptr ());

		member__object->initializer = initializer;
	    } break;
	    case Cpp::Member::t_DependentObject: {
		Cpp::Member_DependentObject * const member__dependent_object =
			static_cast <Cpp::Member_DependentObject*> (condition_state.member.ptr ());

		member__dependent_object->initializer = initializer;
	    } break;
	    default:
		abortIfReached ();
	}

	Ref<Cpp::Statement_Declaration> decl_statement = grab (new Cpp::Statement_Declaration);
	decl_statement->member = condition_state.member;

	outer_statement->statements.append (decl_statement);

	Ref<Cpp::Expression_Id> expression__id = grab (new Cpp::Expression_Id);
	expression__id->name = condition_state.member->name;
	expression__id->member = condition_state.member;

	condition = expression__id;
    } else {
	condition = condition_state.expression;
    }

    Ref<Cpp::Statement> new_statement;
    switch (selection_statement->selection_statement_type) {
	case Cpp_SelectionStatement::t_IfElse:
	case Cpp_SelectionStatement::t_If: {
	    Ref<Cpp::Statement_If> statement = grab (new Cpp::Statement_If);

	    statement->condition = condition;

	    abortIf (statement_state.statements.isEmpty ());
	    if (statement_state.statements.last->previous != NULL) {
		statement->if_statement = statement_state.statements.last->previous->data;
		statement->else_statement = statement_state.statements.last->data;
	    } else {
		statement->if_statement = statement_state.statements.last->data;
	    }

	    new_statement = statement;
	} break;
	case Cpp_SelectionStatement::t_Switch: {
	    // TODO
	    abortIfReached ();
	} break;
	default:
	    abortIfReached ();
    }
    abortIf (new_statement.isNull ());

    if (!outer_statement.isNull ()) {
	outer_statement->statements.append (new_statement);
	self->acceptStatement (target_statement_state, outer_statement);
    } else {
	self->acceptStatement (target_statement_state, new_statement);
    }

    return true;
}

bool
cpp_accept_iteration_statement (Cpp_IterationStatement * const iteration_statement,
				ParserControl          * const /* parser_control */,
				void                   * const _self)
{
    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    abortIf (self->statement_states.last == NULL ||
	     self->statement_states.last->previous == NULL);

    CppParser_Impl::StatementState &statement_state = self->statement_states.last->data;
    CppParser_Impl::StatementState &target_statement_state = self->statement_states.last->previous->data;

    Ref<Cpp::Statement> statement;
    switch (iteration_statement->iteration_statement_type) {
	case Cpp_IterationStatement::t_While: {
	    abortIf (self->condition_states.isEmpty ());
	    CppParser_Impl::ConditionState &condition_state = self->condition_states.last->data;

	    Ref<Cpp::Statement_Compound> outer_statement;
	    Ref<Cpp::Expression> condition_expression;

	    abortIf (condition_state.expression.isNull ());
	    if (!condition_state.member.isNull ()) {
	      // Faking "if (int a) {}" with "{ int a; if (a) {} }"

		outer_statement = grab (new Cpp::Statement_Compound);

		Ref<Cpp::Initializer_Assignment> initializer = grab (new Cpp::Initializer_Assignment);
		initializer->expression = condition_state.expression;

		switch (condition_state.member->getType ()) {
		    case Cpp::Member::t_Object: {
			Cpp::Member_Object * const member__object =
				static_cast <Cpp::Member_Object*> (condition_state.member.ptr ());

			member__object->initializer = initializer;
		    } break;
		    case Cpp::Member::t_DependentObject: {
			Cpp::Member_DependentObject * const member__dependent_object =
				static_cast <Cpp::Member_DependentObject*> (condition_state.member.ptr ());

			member__dependent_object->initializer = initializer;
		    } break;
		    default:
			abortIfReached ();
		}

		Ref<Cpp::Statement_Declaration> decl_statement = grab (new Cpp::Statement_Declaration);
		decl_statement->member = condition_state.member;

		outer_statement->statements.append (decl_statement);

		Ref<Cpp::Expression_Id> expression__id = grab (new Cpp::Expression_Id);
		expression__id->name = condition_state.member->name;
		expression__id->member = condition_state.member;

		condition_expression = expression__id;
	    } else {
		condition_expression = condition_state.expression;
	    }

	    Ref<Cpp::Statement_While> statement__while = grab (new Cpp::Statement_While);
	    statement__while->condition_expression = condition_expression;

	    if (!statement_state.statements.isEmpty ())
		statement__while->statement = statement_state.statements.last->data;

	    if (!outer_statement.isNull ()) {
		outer_statement->statements.append (statement__while);
		statement = outer_statement;
	    } else {
		statement = statement__while;
	    }
	} break;
	case Cpp_IterationStatement::t_DoWhile: {
	    abortIf (self->expression_states.isEmpty ());
	    CppParser_Impl::ExpressionState &expression_state = self->expression_states.last->data;

	    Ref<Cpp::Statement_DoWhile> statement__do_while = grab (new Cpp::Statement_DoWhile);

	    abortIf (expression_state.expressions.isEmpty ());
	    statement__do_while->condition_expression = expression_state.expressions.last->data.expression;

	    if (!statement_state.statements.isEmpty ())
		statement__do_while->statement = statement_state.statements.last->data;

	    statement = statement__do_while;
	} break;
	case Cpp_IterationStatement::t_For: {
	    abortIf (self->for_init_statement_states.isEmpty ());
	    CppParser_Impl::ForInitStatementState &for_init_statement_state = self->for_init_statement_states.last->data;

	    abortIf (self->condition_states.isEmpty ());
	    CppParser_Impl::ConditionState &condition_state = self->condition_states.last->data;

	    abortIf (self->expression_states.isEmpty ());
	    CppParser_Impl::ExpressionState &expression_state = self->expression_states.last->data;

	    Ref<Cpp::Statement_For> statement__for = grab (new Cpp::Statement_For);
	    Ref<Cpp::Statement_Compound> outer_statement;
	    Ref<Cpp::Statement_Compound> inner_statement;

	    Ref<Cpp::Expression> condition_expression;

	    if (!for_init_statement_state.member.isNull ()) {
	      // Faking "for (int a;;) {}" with "{ int a; for (;;) {} }"

		Ref<Cpp::Statement_Compound> statement__compound = grab (new Cpp::Statement_Compound);

		Ref<Cpp::Statement_Declaration> statement__declaration = grab (new Cpp::Statement_Declaration);
		statement__declaration->member = for_init_statement_state.member;

		statement__compound->statements.append (statement__declaration);

		outer_statement = statement__compound;
		inner_statement = statement__compound;
	    } else
	    if (!for_init_statement_state.expression.isNull ()) {
	      // Faking "for (a;;) {}" with "{ a; for (;;) {} }"

		Ref<Cpp::Statement_Compound> statement__compound = grab (new Cpp::Statement_Compound);

		Ref<Cpp::Statement_Expression> statement__expression = grab (new Cpp::Statement_Expression);
		statement__expression->expression = for_init_statement_state.expression;

		statement__compound->statements.append (statement__expression);

		outer_statement = statement__compound;
		inner_statement = statement__compound;
	    }

	    if (!condition_state.member.isNull ()) {
	      // Faking "for (; int a;) {}" with "{ int a; for (; a;) {} }"

	      // FIXME This should be wrong: isn't "int a=" computed on _every_ loop iteration?
	      //
	      // Could be substituted with:
	      // for (;;) {
	      //     int a = 0;
	      //     if ((bool) a == false)
	      //         break;
	      // }

		Ref<Cpp::Statement_Compound> statement__compound = grab (new Cpp::Statement_Compound);

		Ref<Cpp::Initializer_Assignment> initializer = grab (new Cpp::Initializer_Assignment);
		initializer->expression = condition_state.expression;

		switch (condition_state.member->getType ()) {
		    case Cpp::Member::t_Object: {
			Cpp::Member_Object * const member__object =
				static_cast <Cpp::Member_Object*> (condition_state.member.ptr ());

			member__object->initializer = initializer;
		    } break;
		    case Cpp::Member::t_DependentObject: {
			Cpp::Member_DependentObject * const member__dependent_object =
				static_cast <Cpp::Member_DependentObject*> (condition_state.member.ptr ());

			member__dependent_object->initializer = initializer;
		    } break;
		    default:
			abortIfReached ();
		}

		Ref<Cpp::Statement_Declaration> decl_statement = grab (new Cpp::Statement_Declaration);
		decl_statement->member = condition_state.member;

		statement__compound->statements.append (decl_statement);

		Ref<Cpp::Expression_Id> expression__id = grab (new Cpp::Expression_Id);
		expression__id->name = condition_state.member->name;
		expression__id->member = condition_state.member;

		condition_expression = expression__id;

		if (!outer_statement.isNull ())
		    outer_statement->statements.append (statement__compound);
		else
		    outer_statement = statement__compound;

		inner_statement = statement__compound;
	    } else
	    if (!condition_state.expression.isNull ()) {
		condition_expression = condition_state.expression;
	    }

	    statement__for->condition_expression = condition_expression;

	    if (!expression_state.expressions.isEmpty ())
		statement__for->iteration_expression = expression_state.expressions.last->data.expression;

	    if (!statement_state.statements.isEmpty ())
		statement__for->statement = statement_state.statements.last->data;

	    if (!inner_statement.isNull ()) {
		inner_statement->statements.append (statement__for);
		abortIf (outer_statement.isNull ());
		statement = outer_statement;
	    } else {
		abortIf (!outer_statement.isNull ());
		statement = statement__for;
	    }
	} break;
	default:
	    abortIfReached ();
    }
    abortIf (statement.isNull ());

    self->acceptStatement (target_statement_state, statement);

    return true;
}

#if 0
// Deprecated
void
cpp_SelectionStatement_accept_func (Cpp_SelectionStatement * const selection_statement,
				    ParserControl          * const /* parser_control */,
				    void                   * const _parser_state)
{
    if (selection_statement == NULL)
	return;

    static char const * const _func_name = "Scruffy.(CppParser_Impl).cpp_SelectionStatement_accept_func";

    errf->print (_func_name).pendl ();

    CppParser_Impl *parser_state = static_cast <CppParser_Impl*> (_parser_state);

    abortIf (parser_state->statement_acceptors.isEmpty ());
    Ref< CppParser_Impl::Acceptor<Cpp::Statement> > &statement_acceptor =
	    parser_state->statement_acceptors.last->data;

    Ref<Cpp::Statement> statement;
    switch (selection_statement->selection_statement_type) {
	case Cpp_SelectionStatement::t_IfElse: {
	    statement = grab (new Cpp::Statement_If);
	    // TODO
	} break;
	case Cpp_SelectionStatement::t_If: {
	    statement = grab (new Cpp::Statement_If);
	    // TODO
	} break;
	case Cpp_SelectionStatement::t_Switch: {
	    statement = grab (new Cpp::Statement_Switch);
	    // TODO
	} break;
	default:
	    abortIfReached ();
    }
    abortIf (statement.isNull ());

    statement_acceptor->accept (statement);
    {
	Ref< Cancellable_Accept<Cpp::Statement> > cancellable =
		grab (new Cancellable_Accept<Cpp::Statement> (statement_acceptor));
	parser_state->checkpoint_tracker.addCancellable (cancellable);
    }
}
#endif

#if 0
// Deprecated
void
cpp_IterationStatement_accept_func (Cpp_IterationStatement * const iteration_statement,
				    ParserControl          * const /* parser_control */,
				    void                   * const _parser_state)
{
    if (iteration_statement == NULL)
	return;

    static char const * const _func_name = "Scruffy.(CppParser_Impl).cpp_IterationStatement_accept_func";

    errf->print (_func_name).pendl ();

    CppParser_Impl *parser_state = static_cast <CppParser_Impl*> (_parser_state);

    abortIf (parser_state->statement_acceptors.isEmpty ());
    Ref< CppParser_Impl::Acceptor<Cpp::Statement> > &statement_acceptor =
	    parser_state->statement_acceptors.last->data;

    Ref<Cpp::Statement> statement;
    switch (iteration_statement->iteration_statement_type) {
	case Cpp_IterationStatement::t_While: {
	    statement = grab (new Cpp::Statement_While);
	    // TODO
	} break;
	case Cpp_IterationStatement::t_DoWhile: {
	    statement = grab (new Cpp::Statement_DoWhile);
	    // TODO
	} break;
	case Cpp_IterationStatement::t_For: {
	    statement = grab (new Cpp::Statement_For);
	    // TODO
	} break;
	default:
	    abortIfReached ();
    }
    abortIf (statement.isNull ());

    statement_acceptor->accept (statement);
    {
	Ref< Cancellable_Accept<Cpp::Statement> > cancellable =
		grab (new Cancellable_Accept<Cpp::Statement> (statement_acceptor));
	parser_state->checkpoint_tracker.addCancellable (cancellable);
    }
}
#endif

void
cpp_JumpStatement_accept_func (Cpp_JumpStatement * const jump_statement,
			       ParserControl     * const /* parser_control */,
			       void              * const _self)
{
    if (jump_statement == NULL)
	return;

  FUNC_NAME (
    static char const * const _func_name = "Scruffy.(CppParser_Impl).cpp_JumpStatement_accept_func";
  )

    DEBUG (
	errf->print (_func_name).pendl ();
    )

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    abortIf (self->statement_states.isEmpty ());
    CppParser_Impl::StatementState &target_statement_state = self->statement_states.last->data;

    Ref<Cpp::Statement> statement;
    switch (jump_statement->jump_statement_type) {
	case Cpp_JumpStatement::t_Break: {
	    statement = grab (new Cpp::Statement_Break);
	} break;
	case Cpp_JumpStatement::t_Continue: {
	    statement = grab (new Cpp::Statement_Continue);
	} break;
	case Cpp_JumpStatement::t_Return: {
	    statement = grab (new Cpp::Statement_Return);
	    // TODO expression
	} break;
	case Cpp_JumpStatement::t_Goto: {
	    statement = grab (new Cpp::Statement_Goto);
	    // TODO label
	} break;
	default:
	    abortIfReached ();
    }
    abortIf (statement.isNull ());

    self->acceptStatement (target_statement_state, statement);
}

namespace {

    class Acceptor_DeclarationStatementDeclaration : public Scruffy::Acceptor<DeclarationDesc>
    {
    private:
	CppParser_Impl &self;
	CppParser_Impl::DeclarationStatementState &declaration_statement_state;
	CheckpointTracker::CheckpointKey checkpoint_key;

    public:
	void accept (DeclarationDesc * const declaration_desc)
	{
	    abortIf (declaration_desc == NULL ||
		     !declaration_desc->isSane ());

	    abortIf (self.temporal_namespaces.isEmpty () ||
		     self.temporal_namespaces.last->data.isNull ());
	    self.acceptDeclaration (*self.temporal_namespaces.last->data, *declaration_desc, checkpoint_key);

	    declaration_statement_state.declaration_desc = declaration_desc;
	    self.checkpoint_tracker.addBoundCancellable (
		    grab (new Cancellable_Ref<DeclarationDesc> (declaration_statement_state.declaration_desc)),
		    checkpoint_key);
	}

	void cancel ()
	{
	    abortIfReached ();
	}

	Acceptor_DeclarationStatementDeclaration (CppParser_Impl                            &self,
						  CppParser_Impl::DeclarationStatementState &declaration_statement_state,
						  CheckpointTracker::CheckpointKey           checkpoint_key)
	    : self (self),
	      declaration_statement_state (declaration_statement_state),
	      checkpoint_key (checkpoint_key)
	{
	}
    };

}

void
cpp_DeclarationStatement_begin_func (void *_self)
{
    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    // Sanity check.
    abortIf (self->declaration_states.isEmpty ());

    self->declaration_statement_states.appendEmpty ();
    CppParser_Impl::DeclarationStatementState &declaration_statement_state =
	    self->declaration_statement_states.last->data;
    {
	Ref< Cancellable_ListElement<CppParser_Impl::DeclarationStatementState> > cancellable =
		grab (new Cancellable_ListElement<CppParser_Impl::DeclarationStatementState> (
			      self->declaration_statement_states,
			      self->declaration_statement_states.last));

	self->checkpoint_tracker.addUnconditionalCancellable (cancellable);
    }

#if 0
// Deprecated

    // TODO Dedicated acceptor for declaration statements.
    //      Must add elements to the current temporal namespaces.
    //      There _must_ be a temporal namespace.
    self->declaration_acceptors.append (
	    grab (new CppParser_Impl::Acceptor_Bound_Ref<CppParser_Impl::DeclarationDesc> (
			  &self->checkpoint_tracker,
			  &declaration_statement_state.declaration_desc)));
#endif
    self->declaration_acceptors.append (
	grab (new Acceptor_DeclarationStatementDeclaration (*self,
							    declaration_statement_state,
							    self->checkpoint_tracker.getCurrentCheckpoint ())));
    self->checkpoint_tracker.addUnconditionalCancellable (
	    createLastElementCancellable (self->declaration_acceptors));
}

void
cpp_DeclarationStatement_accept_func (Cpp_DeclarationStatement * const declaration_statement,
				      ParserControl            * const /* parser_control */,
				      void                     * const _self)
{
    if (declaration_statement == NULL)
	return;

  FUNC_NAME (
    static char const * const _func_name = "Scruffy.(CppParser_Impl).cpp_DeclarationStatement_accept_func";
  )

    DEBUG (
	errf->print (_func_name).pendl ();
    )

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    abortIf (self->statement_states.isEmpty ());
    CppParser_Impl::StatementState &target_statement_state = self->statement_states.last->data;

    abortIf (self->declaration_statement_states.isEmpty ());
    CppParser_Impl::DeclarationStatementState &declaration_statement_state = self->declaration_statement_states.last->data;

    if (declaration_statement_state.declaration_desc.isNull ())
	return;

    Ref<Cpp::Statement_Declaration> statement__declaration = grab (new Cpp::Statement_Declaration ());
    statement__declaration->member = declaration_statement_state.declaration_desc->member;

    self->acceptStatement (target_statement_state, statement__declaration);
}

bool
cpp_begin_expression (CppElement    * const /* cpp_element */,
		      ParserControl * const /* parser_control */,
		      void          * const _self)
{
  FUNC_NAME (
    static char const * const _func_name = "Scruffy.(CppParser_Impl).cpp_begin_expression";
  )

    DEBUG (
	errf->print (_func_name).pendl ();
    )

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    CppParser_Impl::ExpressionState &expression_state = self->expression_states.appendEmpty ()->data;
    expression_state.checkpoint_key = self->checkpoint_tracker.getCurrentCheckpoint ();
    self->checkpoint_tracker.addUnconditionalCancellable (
	    createLastElementCancellable (self->expression_states));

    return true;
}

static void
cpp_accept_expression (CppParser_Impl                  &self,
		       CppParser_Impl::ExpressionState &target_expression_state,
		       Cpp::Expression                 * const expression)
{
  FUNC_NAME (
    static char const * const _func_name = "Scruffy.(CppParser_Impl).cpp_accept_expression";
  )

    DEBUG (
	errf->print (_func_name).pendl ();
    )

    target_expression_state.expressions.appendEmpty ();
    CppParser_Impl::ExpressionEntry &expression_entry = target_expression_state.expressions.last->data;
    expression_entry.expression = expression;
    expression_entry.cancellable_key =
	    self.checkpoint_tracker.addBoundCancellable (
		    createLastElementCancellable (target_expression_state.expressions),
		    target_expression_state.checkpoint_key);
}

bool
cpp_accept_primary_expression (Cpp_PrimaryExpression * const primary_expression,
			       ParserControl         * const /* parser_control */,
			       void                  * const _self)
{
    CppParser_Impl *self = static_cast <CppParser_Impl*> (_self);

    abortIf (self->expression_states.isEmpty ());
    CppParser_Impl::ExpressionState &expression_state = self->expression_states.last->data;

    switch (primary_expression->primary_expression_type) {
	case Cpp_PrimaryExpression::t_Literal: {
	    Ref<Cpp::Expression_Literal> expression =
		    grab (new Cpp::Expression_Literal);

	    // TODO

	    cpp_accept_expression (*self, expression_state, expression);
	} break;
	case Cpp_PrimaryExpression::t_This: {
	    Ref<Cpp::Expression_This> expression =
		    grab (new Cpp::Expression_This);

	    cpp_accept_expression (*self, expression_state, expression);
	} break;
	case Cpp_PrimaryExpression::t_Braces: {
	  // Nothing to do
	} break;
	case Cpp_PrimaryExpression::t_IdExpression: {
	    Ref<Cpp::Expression_Id> expression =
		    grab (new Cpp::Expression_Id);

	    Cpp_PrimaryExpression_IdExpression const * const primary_expression__id_expression =
		    static_cast <Cpp_PrimaryExpression_IdExpression*> (primary_expression);

	    Cpp_IdExpression const * const id_expression =
		    primary_expression__id_expression->idExpression;

	    Ref<Cpp::Name> name = cpp_parser_parse_id_expression (id_expression);
	    abortIf (name.isNull ());

	    CppParser_Impl::IdState &id_expression_state = self->id_states.getLast ();

	    // TODO For _Template and _Destructor we can provide the member immediately.

	    expression->global_scope = id_expression_state.global_scope;
	    expression->nested_name.steal (&id_expression_state.nested_name_atoms,
					   id_expression_state.nested_name_atoms.first,
					   id_expression_state.nested_name_atoms.last,
					   expression->nested_name.last,
					   GenericList::StealPrepend);
	    expression->name = name;

#if 0
// Wrong way
	    Ref<Cpp::Member> member = self->lookupMember (id_expression_state.global_scope,
							  id_expression_state.nested_name_atoms,
							  *name);
	    if (member.isNull ()) {
		errf->print ("Unknown name").pendl ();
		abortIfReached ();
	    }

	    expression->member = member;
#endif

	    cpp_accept_expression (*self, expression_state, expression);
	} break;
	default:
	    abortIfReached ();
    }

    return true;
}

template <class T, Cpp::Expression::Operation::Operation_ operation>
T&
cpp_accept_lr_postfix_expression_one_arg (CppParser_Impl &self,
					  Ref<Cpp::Expression> * const ret_subexpression,
					  Ref<Cpp::Expression> * const ret_arg)
{
    Ref<T> expression = grab (new T (operation));

    abortIf (self.expression_states.last == NULL ||
	     self.expression_states.last->previous == NULL);

    CppParser_Impl::ExpressionState &expression_state = self.expression_states.last->data;
    CppParser_Impl::ExpressionState &target_expression_state = self.expression_states.last->previous->data;

    abortIf (expression_state.expressions.isEmpty ());
    *ret_arg = expression_state.expressions.last->data.expression;

    abortIf (target_expression_state.expressions.isEmpty ());
    *ret_subexpression = target_expression_state.expressions.last->data.expression;
    self.checkpoint_tracker.removeCancellable (target_expression_state.expressions.last->data.cancellable_key);
    target_expression_state.expressions.remove (target_expression_state.expressions.last);

    expression->expressions.append (*ret_subexpression);
    expression->expressions.append (*ret_arg);

    cpp_accept_expression (self, target_expression_state, expression);

    return *expression;
}

template <class T, Cpp::Expression::Operation::Operation_ operation>
void
cpp_accept_lr_postfix_expression (CppParser_Impl &self)
{
    Ref<T> expression = grab (new T (operation));

    abortIf (self.expression_states.isEmpty ());

    CppParser_Impl::ExpressionState &target_expression_state = self.expression_states.last->data;

    abortIf (target_expression_state.expressions.isEmpty ());
    expression->expressions.append (target_expression_state.expressions.last->data.expression);
    self.checkpoint_tracker.removeCancellable (target_expression_state.expressions.last->data.cancellable_key);
    target_expression_state.expressions.remove (target_expression_state.expressions.last);

    cpp_accept_expression (self, target_expression_state, expression);
}

// C++-style casts.
template <class T, Cpp::Expression::Operation::Operation_ operation>
void
cpp_accept_cast_expression (CppParser_Impl &self)
{
    Ref<T> expression = grab (new T (operation));

    abortIf (// Arg expression state
	     self.expression_states.last == NULL ||
	     // Target expression state
	     self.expression_states.last->previous == NULL);

    CppParser_Impl::ExpressionState &expression_state = self.expression_states.last->data;
    CppParser_Impl::ExpressionState &target_expression_state = self.expression_states.last->previous->data;

    {
	CppParser_Impl::TypeidState &typeid_state = self.typeid_states.getLast ();
	abortIf (typeid_state.type_desc.isNull ());
	expression->type_argument = typeid_state.type_desc;
    }

    abortIf (expression_state.expressions.isEmpty ());
    expression->expressions.append (expression_state.expressions.last->data.expression);

    cpp_accept_expression (self, target_expression_state, expression);
}

template <class T, Cpp::Expression::Operation::Operation_ operation>
void
cpp_accept_unary_expression (CppParser_Impl &self)
{
    Ref<T> expression = grab (new T (operation));

    abortIf (// Arg expression state
	     self.expression_states.last == NULL ||
	     // Target expression state
	     self.expression_states.last->previous == NULL);

    CppParser_Impl::ExpressionState &expression_state = self.expression_states.last->data;
    CppParser_Impl::ExpressionState &target_expression_state = self.expression_states.last->previous->data;

    expression->expressions.append (expression_state.expressions.last->data.expression);

    cpp_accept_expression (self, target_expression_state, expression);
}

template <class T, Cpp::Expression::Operation::Operation_ operation>
void
cpp_accept_unary_typeid_expression (CppParser_Impl &self)
{
    Ref<T> expression = grab (new T (operation));

    abortIf (// Target expression state
	     self.expression_states.isEmpty ());

    CppParser_Impl::ExpressionState &target_expression_state = self.expression_states.last->data;

    {
	abortIf (self.typeid_states.isEmpty ());
	CppParser_Impl::TypeidState &typeid_state = self.typeid_states.last->data;
	abortIf (typeid_state.type_desc.isNull ());
	expression->type_argument = typeid_state.type_desc;
    }

    cpp_accept_expression (self, target_expression_state, expression);
}

template <class T, Cpp::Expression::Operation::Operation_ operation>
void
cpp_accept_binary_expression (CppParser_Impl * const self)
{
    Ref<T> expression = grab (new T (operation));

    abortIf (// Right argument expression state
	     self->expression_states.last == NULL ||
	     // Left argument expression state
	     self->expression_states.last->previous == NULL ||
	     // Target expression state
	     self->expression_states.last->previous->previous == NULL);

    CppParser_Impl::ExpressionState &right_expression_state  = self->expression_states.last->data;
    CppParser_Impl::ExpressionState &left_expression_state   = self->expression_states.last->previous->data;
    CppParser_Impl::ExpressionState &target_expression_state = self->expression_states.last->previous->previous->data;

    abortIf (left_expression_state.expressions.isEmpty () ||
	     right_expression_state.expressions.isEmpty ());
    expression->expressions.append (left_expression_state.expressions.last->data.expression);
    expression->expressions.append (right_expression_state.expressions.last->data.expression);

    cpp_accept_expression (*self, target_expression_state, expression);
}

template <class T, Cpp::Expression::Operation::Operation_ operation>
void
cpp_accept_binary_lr_expression (CppParser_Impl * const self)
{
    Ref<T> expression = grab (new T (operation));

  // The expression is left-recursive, hence we've got the first argument
  // at the previous expression_state.

    abortIf (// Right argument expression state
	     self->expression_states.last == NULL ||
	     // Target expression state
	     self->expression_states.last->previous == NULL);

    CppParser_Impl::ExpressionState &right_expression_state  = self->expression_states.last->data;
    CppParser_Impl::ExpressionState &target_expression_state = self->expression_states.last->previous->data;

    abortIf (target_expression_state.expressions.isEmpty ());
    expression->expressions.append (target_expression_state.expressions.last->data.expression);
    self->checkpoint_tracker.removeCancellable (target_expression_state.expressions.last->data.cancellable_key);
    target_expression_state.expressions.remove (target_expression_state.expressions.last);

    abortIf (right_expression_state.expressions.isEmpty ());
    expression->expressions.append (right_expression_state.expressions.last->data.expression);

    cpp_accept_expression (*self, target_expression_state, expression);
}

static void
cpp_get_lr_binary_expression_args (CppParser_Impl * const self,
				   Ref<Cpp::Expression> &ret_left,
				   Ref<Cpp::Expression> &ret_right)
{
  // The expression is left-recursive, hence we've got the first argument
  // at the previous expression_state.

    abortIf (// Right argument expression state
	     self->expression_states.last == NULL ||
	     // Target expression state
	     self->expression_states.last->previous == NULL);

    CppParser_Impl::ExpressionState &right_expression_state  = self->expression_states.last->data;
    CppParser_Impl::ExpressionState &target_expression_state = self->expression_states.last->previous->data;

    abortIf (target_expression_state.expressions.isEmpty ());
    ret_left = target_expression_state.expressions.last->data.expression;
    self->checkpoint_tracker.removeCancellable (target_expression_state.expressions.last->data.cancellable_key);
    target_expression_state.expressions.remove (target_expression_state.expressions.last);

    abortIf (right_expression_state.expressions.isEmpty ());
    ret_right = right_expression_state.expressions.last->data.expression;
}

static CppParser_Impl::ExpressionState&
cpp_get_lr_binary_expression_target_state (CppParser_Impl * const self)
{
    abortIf (self->expression_states.last == NULL ||
	     self->expression_states.last->previous == NULL);
    CppParser_Impl::ExpressionState &target_expression_state =
	    self->expression_states.last->previous->data;

    return target_expression_state;
}

bool
cpp_accept_postfix_expression (Cpp_PostfixExpression * const postfix_expression,
			       ParserControl         * const /* parser_control */,
			       void                  * const _self)
{
    FUNC_NAME (
	static char const * const _func_name = "Scruffy.(CppParser).cpp_accept_postfix_expression";
    )

    DEBUG (
	errf->print (_func_name).pendl ();
    )

    if (postfix_expression == NULL) {
	DEBUG (
	    errf->print (_func_name).print (": null").pendl ();
	)
	return true;
    }

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    switch (postfix_expression->postfix_expression_type) {
	case Cpp_PostfixExpression::t_Subscripting: {
	    Ref<Cpp::Expression> subexpression;
	    Ref<Cpp::Expression> subscript;
//	    Cpp::Expression_Subscripting &expression =
		    cpp_accept_lr_postfix_expression_one_arg <Cpp::Expression_Generic,
							      Cpp::Expression::Operation::Subscripting> (
			    *self, &subexpression, &subscript);

#if 0
	    expression.expression = subexpression;
	    expression.subscript = subscript;
#endif
	} break;
	case Cpp_PostfixExpression::t_FunctionCall: {
	    Ref<Cpp::Expression> expression =
		    grab (new Cpp::Expression_Generic (Cpp::Expression::Operation::FunctionCall));

	  // PostfixExpression_FunctionCall is left-recursive, hence we've got the first argument
	  // at the previous expression_state.

	    abortIf (// Args expression state
		     self->expression_states.last == NULL ||
		     // Target expression state
		     self->expression_states.last->previous == NULL);

	    CppParser_Impl::ExpressionState &expression_state = self->expression_states.last->data;
	    CppParser_Impl::ExpressionState &target_expression_state = self->expression_states.last->previous->data;

	    abortIf (target_expression_state.expressions.isEmpty ());
	    expression->expressions.append (target_expression_state.expressions.last->data.expression);
	    self->checkpoint_tracker.removeCancellable (target_expression_state.expressions.last->data.cancellable_key);
	    target_expression_state.expressions.remove (target_expression_state.expressions.last);

	    {
		List<CppParser_Impl::ExpressionEntry>::DataIterator iter (expression_state.expressions);
		while (!iter.done ()) {
		    CppParser_Impl::ExpressionEntry &expression_entry = iter.next ();
		    expression->expressions.append (expression_entry.expression);
		}
	    }

	    cpp_accept_expression (*self, target_expression_state, expression);
	} break;
	case Cpp_PostfixExpression::t_TypeConversion: {
	    Cpp_PostfixExpression_TypeConversion const * const postfix_expression__type_conversion =
		    static_cast <Cpp_PostfixExpression_TypeConversion const *> (postfix_expression);

	    Ref<Cpp::Expression> expression =
		    grab (new Cpp::Expression_Generic (Cpp::Expression::Operation::TypeConversion));

	    abortIf (// Args expression state
		     self->expression_states.last == NULL ||
		     // Target expression state
		     self->expression_states.last->previous == NULL);

	    CppParser_Impl::ExpressionState &expression_state = self->expression_states.last->data;
	    CppParser_Impl::ExpressionState &target_expression_state = self->expression_states.last->previous->data;

	    DeclarationState &declaration_state = self->declaration_states.getLast ();
	    expression->type_argument = declaration_state.type_specifier_parser.createTypeDesc ();

	    // If expression-list contains more than one expression,
	    // then we represent it as Expression_Comma.
	    if (!expression_state.expressions.isEmpty ()) {
		if (expression_state.expressions.first == expression_state.expressions.last) {
		    expression->expressions.append (expression_state.expressions.first->data.expression);
		} else {
		    Ref<Cpp::Expression> expression__comma =
			    grab (new Cpp::Expression_Generic (Cpp::Expression::Operation::Comma));

		    {
			List<CppParser_Impl::ExpressionEntry>::DataIterator iter (expression_state.expressions);
			while (!iter.done ()) {
			    CppParser_Impl::ExpressionEntry &expression_entry = iter.next ();
			    expression__comma->expressions.append (expression_entry.expression);
			}
		    }
		}
	    }

	    cpp_accept_expression (*self, target_expression_state, expression);
	} break;
	case Cpp_PostfixExpression::t_TypenameTypeConversion: {
	    // TODO
	    DEBUG (
		errf->print ("--- POSTFIX TYPENAME TYPE CONVERSION").pendl ();
	    )
	} break;
	case Cpp_PostfixExpression::t_TemplateTypeConversion: {
	    // TODO
	    DEBUG (
		errf->print ("--- POSTFIX TEMPLATE TYPE CONVERSION").pendl ();
	    )
	} break;
	case Cpp_PostfixExpression::t_DotMemberAccess: {
	    Cpp_PostfixExpression_DotMemberAccess const * const postfix_expression__dot_member_access =
		    static_cast <Cpp_PostfixExpression_DotMemberAccess const *> (postfix_expression);

	    Ref<Cpp::Expression_MemberAccess> expression =
		    grab (new Cpp::Expression_MemberAccess (Cpp::Expression::Operation::DotMemberAccess));

	  // PostfixExpression_DotMemberAccess is left-recursive, hence we've got the first argument
	  // at the previous expression_state.

	    abortIf (// Target expression state
		     self->expression_states.isEmpty ());

	    CppParser_Impl::ExpressionState &target_expression_state = self->expression_states.last->data;

	    abortIf (target_expression_state.expressions.isEmpty ());
	    expression->expressions.append (target_expression_state.expressions.last->data.expression);
	    self->checkpoint_tracker.removeCancellable (target_expression_state.expressions.last->data.cancellable_key);
	    target_expression_state.expressions.remove (target_expression_state.expressions.last);

	    CppParser_Impl::IdState &id_expression_state = self->id_states.getLast ();

	    Ref<Cpp::Name> name = cpp_parser_parse_id_expression (postfix_expression__dot_member_access->idExpression);

	    if (id_expression_state.global_scope ||
		!id_expression_state.nested_name_atoms.isEmpty ())
	    {
		errf->print ("Qualified name in an id-expression").pendl ();
		abortIfReached ();
	    }

	    expression->member_name = name;

	    cpp_accept_expression (*self, target_expression_state, expression);
	} break;
	case Cpp_PostfixExpression::t_ArrowMemberAccess: {
	    Cpp_PostfixExpression_ArrowMemberAccess const * const postfix_expression__arrow_member_access =
		    static_cast <Cpp_PostfixExpression_ArrowMemberAccess const *> (postfix_expression);

	    Ref<Cpp::Expression_MemberAccess> expression =
		    grab (new Cpp::Expression_MemberAccess (Cpp::Expression::Operation::ArrowMemberAccess));

	  // PostfixExpression_DotMemberAccess is left-recursive, hence we've got the first argument
	  // at the previous expression_state.

	    abortIf (// Target expression state
		     self->expression_states.isEmpty ());

	    CppParser_Impl::ExpressionState &target_expression_state = self->expression_states.last->data;

	    abortIf (target_expression_state.expressions.isEmpty ());
	    expression->expressions.append (target_expression_state.expressions.last->data.expression);
	    self->checkpoint_tracker.removeCancellable (target_expression_state.expressions.last->data.cancellable_key);
	    target_expression_state.expressions.remove (target_expression_state.expressions.last);

	    CppParser_Impl::IdState &id_expression_state = self->id_states.getLast ();

	    Ref<Cpp::Name> name = cpp_parser_parse_id_expression (postfix_expression__arrow_member_access->idExpression);

	    if (id_expression_state.global_scope ||
		!id_expression_state.nested_name_atoms.isEmpty ())
	    {
		errf->print ("Qualified name in an id-expression").pendl ();
		abortIfReached ();
	    }

	    expression->member_name = name;

	    cpp_accept_expression (*self, target_expression_state, expression);
	} break;
	case Cpp_PostfixExpression::t_DotPseudoDestructorCall: {
	    // TODO
	    DEBUG (
		errf->print ("--- POSTFIX DOT PSEUDO DESTRUCTOR CALL").pendl ();
	    )
	} break;
	case Cpp_PostfixExpression::t_ArrowPseudoDestructorCall: {
	    // TODO
	    DEBUG (
		errf->print ("--- POSTFIX ARROW PSEUDO DESTRUCTOR CALL").pendl ();
	    )
	} break;
	case Cpp_PostfixExpression::t_PostfixIncrement: {
	    cpp_accept_lr_postfix_expression<Cpp::Expression_Generic, Cpp::Expression::Operation::PostfixIncrement> (*self);
	} break;
	case Cpp_PostfixExpression::t_PostfixDecrement: {
	    cpp_accept_lr_postfix_expression<Cpp::Expression_Generic, Cpp::Expression::Operation::PostfixDecrement> (*self);
	} break;
	case Cpp_PostfixExpression::t_DynamicCast: {
	    cpp_accept_cast_expression<Cpp::Expression_Generic, Cpp::Expression::Operation::DynamicCast> (*self);
	} break;
	case Cpp_PostfixExpression::t_StaticCast: {
	    cpp_accept_cast_expression<Cpp::Expression_Generic, Cpp::Expression::Operation::StaticCast> (*self);
	} break;
	case Cpp_PostfixExpression::t_ReinterpretCast: {
	    cpp_accept_cast_expression<Cpp::Expression_Generic, Cpp::Expression::Operation::ReinterpretCast> (*self);
	} break;
	case Cpp_PostfixExpression::t_ConstCast: {
	    cpp_accept_cast_expression<Cpp::Expression_Generic, Cpp::Expression::Operation::ConstCast> (*self);
	} break;
	case Cpp_PostfixExpression::t_TypeidExpression: {
	    cpp_accept_unary_expression<Cpp::Expression_Generic, Cpp::Expression::Operation::TypeidExpression> (*self);
	} break;
	case Cpp_PostfixExpression::t_TypeidTypeid: {
	    cpp_accept_unary_typeid_expression<Cpp::Expression_Generic, Cpp::Expression::Operation::TypeidType> (*self);
	} break;
	case Cpp_PostfixExpression::t_Expression: {
	    // Nothing to do
	    return true;
	} break;
	default:
	    abortIfReached ();
    }

    return true;
}

bool
cpp_accept_unary_expression (Cpp_UnaryExpression * const unary_expression,
			     ParserControl       * const /* parser_control */,
			     void                * const _self)
{
    FUNC_NAME (
	static char const * const _func_name = "Scruffy.(CppParser).cpp_accept_unary_expression";
    )

    DEBUG (
	errf->print (_func_name).pendl ();
    )

    if (unary_expression == NULL) {
	DEBUG (
	    errf->print (_func_name).print (": null").pendl ();
	)
	return true;
    }

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    switch (unary_expression->unary_expression_type) {
	case Cpp_UnaryExpression::t_PrefixIncrement: {
	    cpp_accept_unary_expression<Cpp::Expression_Generic, Cpp::Expression::Operation::PrefixIncrement> (*self);
	} break;
	case Cpp_UnaryExpression::t_PrefixDecrement: {
	    cpp_accept_unary_expression<Cpp::Expression_Generic, Cpp::Expression::Operation::PrefixDecrement> (*self);
	} break;
	case Cpp_UnaryExpression::t_UnaryOperator: {
	    Cpp_UnaryExpression_UnaryOperator const * const unary_expression__unary_operator =
		    static_cast <Cpp_UnaryExpression_UnaryOperator const *> (unary_expression);

	    Cpp_UnaryOperator const * const unary_operator =
		    unary_expression__unary_operator->unaryOperator;

	    switch (unary_operator->unary_operator_type) {
		case Cpp_UnaryOperator::t_Indirection: {
		    cpp_accept_unary_expression<Cpp::Expression_Generic, Cpp::Expression::Operation::Indirection> (*self);
		} break;
		case Cpp_UnaryOperator::t_PointerTo: {
		    cpp_accept_unary_expression<Cpp::Expression_Generic, Cpp::Expression::Operation::PointerTo> (*self);
		} break;
		case Cpp_UnaryOperator::t_UnaryPlus: {
		    cpp_accept_unary_expression<Cpp::Expression_Generic, Cpp::Expression::Operation::UnaryPlus> (*self);
		} break;
		case Cpp_UnaryOperator::t_UnaryMinus: {
		    cpp_accept_unary_expression<Cpp::Expression_Generic, Cpp::Expression::Operation::UnaryMinus> (*self);
		} break;
		case Cpp_UnaryOperator::t_LogicalNegation: {
		    cpp_accept_unary_expression<Cpp::Expression_Generic, Cpp::Expression::Operation::LogicalNegation> (*self);
		} break;
		case Cpp_UnaryOperator::t_OnesComplement: {
		    cpp_accept_unary_expression<Cpp::Expression_Generic, Cpp::Expression::Operation::OnesComplement> (*self);
		} break;
		default:
		    abortIfReached ();
	    }
	} break;
	case Cpp_UnaryExpression::t_SizeofExpression: {
	    cpp_accept_unary_expression<Cpp::Expression_Generic, Cpp::Expression::Operation::SizeofExpression> (*self);
	} break;
	case Cpp_UnaryExpression::t_SizeofTypeid: {
	    cpp_accept_unary_typeid_expression<Cpp::Expression_Generic, Cpp::Expression::Operation::SizeofType> (*self);
	} break;
	default:
	    abortIfReached ();
    }

    return true;
}

bool
cpp_accept_cast_expression (Cpp_CastExpression * const cast_expression,
			    ParserControl      * const /* parser_control */,
			    void               * const _self)
{
//    static char const * const _func_name = "Scruffy.CppParser.cpp_accept_cast_expression";

    if (cast_expression == NULL)
	return true;

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    switch (cast_expression->cast_expression_type) {
	case Cpp_CastExpression::t_Cast: {
	    cpp_accept_cast_expression<Cpp::Expression_Generic, Cpp::Expression::Operation::Cast> (*self);
	} break;
	default:
	    abortIfReached ();
    }

    return true;
}

bool
cpp_accept_multiplicative_expression (Cpp_MultiplicativeExpression * const multiplicative_expression,
				      ParserControl                * const /* parser_control */,
				      void                         * const _self)
{
//    static char const * const _func_name = "Scruffy.CppParser.cpp_accept_multiplicative_expression";

    if (multiplicative_expression == NULL)
	return true;

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    switch (multiplicative_expression->multiplicative_expression_type) {
	case Cpp_MultiplicativeExpression::t_Multiplication: {
	    cpp_accept_binary_lr_expression <Cpp::Expression_Generic, Cpp::Expression::Operation::Multiplication> (self);
	} break;
	case Cpp_MultiplicativeExpression::t_Division: {
	    cpp_accept_binary_lr_expression <Cpp::Expression_Generic, Cpp::Expression::Operation::Division> (self);
	} break;
	case Cpp_MultiplicativeExpression::t_Remainder: {
	    cpp_accept_binary_lr_expression <Cpp::Expression_Generic, Cpp::Expression::Operation::Remainder> (self);
	} break;
	default:
	    abortIfReached ();
    }

    return true;
}

bool
cpp_accept_additive_expression (Cpp_AdditiveExpression * const additive_expression,
				ParserControl          * const /* parser_control */,
				void                   * const _self)
{
    if (additive_expression == NULL)
	return true;

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    switch (additive_expression->additive_expression_type) {
	case Cpp_AdditiveExpression::t_Addition: {
	    cpp_accept_binary_lr_expression <Cpp::Expression_Generic, Cpp::Expression::Operation::Addition> (self);
	} break;
	case Cpp_AdditiveExpression::t_Subtraction: {
	    cpp_accept_binary_lr_expression <Cpp::Expression_Generic, Cpp::Expression::Operation::Subtraction> (self);
	} break;
	default:
	    abortIfReached ();
    }

    return true;
}

bool
cpp_accept_shift_expression (Cpp_ShiftExpression * const shift_expression,
			     ParserControl       * const /* parser_control */,
			     void                * const _self)
{
    if (shift_expression == NULL)
	return true;

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    switch (shift_expression->shift_expression_type) {
	case Cpp_ShiftExpression::t_LeftShift: {
	    cpp_accept_binary_lr_expression <Cpp::Expression_Generic, Cpp::Expression::Operation::LeftShift> (self);
	} break;
	case Cpp_ShiftExpression::t_RightShift: {
	    cpp_accept_binary_lr_expression <Cpp::Expression_Generic, Cpp::Expression::Operation::RightShift> (self);
	} break;
	default:
	    abortIfReached ();
    }

    return true;
}

bool
cpp_accept_relational_expression (Cpp_RelationalExpression * const relational_expression,
				  ParserControl            * const /* parser_control */,
				  void                     * const _self)
{
    if (relational_expression == NULL)
	return true;

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    switch (relational_expression->relational_expression_type) {
	case Cpp_RelationalExpression::t_Less: {
	    cpp_accept_binary_lr_expression <Cpp::Expression_Generic, Cpp::Expression::Operation::Less> (self);
	} break;
	case Cpp_RelationalExpression::t_Greater: {
	    cpp_accept_binary_lr_expression <Cpp::Expression_Generic, Cpp::Expression::Operation::Greater> (self);
	} break;
	case Cpp_RelationalExpression::t_LessOrEqual: {
	    cpp_accept_binary_lr_expression <Cpp::Expression_Generic, Cpp::Expression::Operation::LessOrEqual> (self);
	} break;
	case Cpp_RelationalExpression::t_GreaterOrEqual: {
	    cpp_accept_binary_lr_expression <Cpp::Expression_Generic, Cpp::Expression::Operation::GreaterOrEqual> (self);
	} break;
	default:
	    abortIfReached ();
    }

    return true;
}

bool
cpp_accept_equality_expression (Cpp_EqualityExpression * const equality_expression,
				ParserControl          * const /* parser_control */,
				void                   * const _self)
{
    if (equality_expression == NULL)
	return true;

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    switch (equality_expression->equality_expression_type) {
	case Cpp_EqualityExpression::t_Equal: {
	    cpp_accept_binary_lr_expression <Cpp::Expression_Generic, Cpp::Expression::Operation::Equal> (self);
	} break;
	case Cpp_EqualityExpression::t_NotEqual: {
	    cpp_accept_binary_lr_expression <Cpp::Expression_Generic, Cpp::Expression::Operation::NotEqual> (self);
	} break;
	default:
	    abortIfReached ();
    }

    return true;
}

bool
cpp_accept_and_expression (Cpp_AndExpression * const and_expression,
			   ParserControl     * const /* parser_control */,
			   void              * const _self)
{
    if (and_expression == NULL)
	return true;

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    switch (and_expression->and_expression_type) {
	case Cpp_AndExpression::t_BitwiseAnd: {
	    cpp_accept_binary_lr_expression <Cpp::Expression_Generic, Cpp::Expression::Operation::BitwiseAnd> (self);
	} break;
	default:
	    abortIfReached ();
    }

    return true;
}

bool
cpp_accept_exclusive_or_expression (Cpp_ExclusiveOrExpression * const exclusive_or_expression,
				    ParserControl             * const /* parser_control */,
				    void                      * const _self)
{
    if (exclusive_or_expression == NULL)
	return true;

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    switch (exclusive_or_expression->exclusive_or_expression_type) {
	case Cpp_ExclusiveOrExpression::t_BitwiseExclusiveOr: {
	    cpp_accept_binary_lr_expression <Cpp::Expression_Generic, Cpp::Expression::Operation::BitwiseExclusiveOr> (self);
	} break;
	default:
	    abortIfReached ();
    }

    return true;
}

bool
cpp_accept_inclusive_or_expression (Cpp_InclusiveOrExpression * const inclusive_or_expression,
				    ParserControl             * const /* parser_control */,
				    void                      * const _self)
{
    if (inclusive_or_expression == NULL)
	return true;

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    switch (inclusive_or_expression->inclusive_or_expression_type) {
	case Cpp_InclusiveOrExpression::t_BitwiseInclusiveOr: {
	    cpp_accept_binary_lr_expression <Cpp::Expression_Generic, Cpp::Expression::Operation::BitwiseInclusiveOr> (self);
	} break;
	default:
	    abortIfReached ();
    }

    return true;
}

bool
cpp_accept_logical_and_expression (Cpp_LogicalAndExpression * const logical_and_expression,
				   ParserControl            * const /* parser_control */,
				   void                     * const _self)
{
    if (logical_and_expression == NULL)
	return true;

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    switch (logical_and_expression->logical_and_expression_type) {
	case Cpp_LogicalAndExpression::t_LogicalAnd: {
	    cpp_accept_binary_lr_expression <Cpp::Expression_Generic, Cpp::Expression::Operation::LogicalAnd> (self);
	} break;
	default:
	    abortIfReached ();
    }

    return true;
}

bool
cpp_accept_logical_or_expression (Cpp_LogicalOrExpression * const logical_or_expression,
				  ParserControl           * const /* parser_control */,
				  void                    * const _self)
{
    if (logical_or_expression == NULL)
	return true;

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    switch (logical_or_expression->logical_or_expression_type) {
	case Cpp_LogicalOrExpression::t_LogicalOr: {
	    cpp_accept_binary_lr_expression <Cpp::Expression_Generic, Cpp::Expression::Operation::LogicalOr> (self);
	} break;
	default:
	    abortIfReached ();
    }

    return true;
}

bool
cpp_accept_assignment_expression (Cpp_AssignmentExpression * const assignment_expression,
				  ParserControl            * const /* parser_control */,
				  void                     * const _self)
{
    if (assignment_expression == NULL)
	return true;

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    switch (assignment_expression->assignment_expression_type) {
	case Cpp_AssignmentExpression::t_Assignment: {
	    Cpp_AssignmentExpression_Assignment const * const assignment_expression__assignment =
		    static_cast <Cpp_AssignmentExpression_Assignment const *> (assignment_expression);

	    Cpp_AssignmentOperator const * const assignment_operator =
		    assignment_expression__assignment->assignmentOperator;

	    switch (assignment_operator->assignment_operator_type) {
		case Cpp_AssignmentOperator::t_Generic: {
		    cpp_accept_binary_expression <Cpp::Expression_Generic, Cpp::Expression::Operation::Assignment> (self);
		} break;
		case Cpp_AssignmentOperator::t_Multiplication: {
		    cpp_accept_binary_expression <Cpp::Expression_Generic, Cpp::Expression::Operation::Assignment_Multiplication> (self);
		} break;
		case Cpp_AssignmentOperator::t_Division: {
		    cpp_accept_binary_expression <Cpp::Expression_Generic, Cpp::Expression::Operation::Assignment_Division> (self);
		} break;
		case Cpp_AssignmentOperator::t_Remainder: {
		    cpp_accept_binary_expression <Cpp::Expression_Generic, Cpp::Expression::Operation::Assignment_Remainder> (self);
		} break;
		case Cpp_AssignmentOperator::t_Addition: {
		    cpp_accept_binary_expression <Cpp::Expression_Generic, Cpp::Expression::Operation::Assignment_Addition> (self);
		} break;
		case Cpp_AssignmentOperator::t_Subtraction: {
		    cpp_accept_binary_expression <Cpp::Expression_Generic, Cpp::Expression::Operation::Assignment_Subtraction> (self);
		} break;
		case Cpp_AssignmentOperator::t_LeftShift: {
		    cpp_accept_binary_expression <Cpp::Expression_Generic, Cpp::Expression::Operation::Assignment_LeftShift> (self);
		} break;
		case Cpp_AssignmentOperator::t_RightShift: {
		    cpp_accept_binary_expression <Cpp::Expression_Generic, Cpp::Expression::Operation::Assignment_RightShift> (self);
		} break;
		case Cpp_AssignmentOperator::t_BitwiseAnd: {
		    cpp_accept_binary_expression <Cpp::Expression_Generic, Cpp::Expression::Operation::Assignment_BitwiseAnd> (self);
		} break;
		case Cpp_AssignmentOperator::t_BitwiseExclusiveOr: {
		    cpp_accept_binary_expression <Cpp::Expression_Generic, Cpp::Expression::Operation::Assignment_BitwiseExclusiveOr> (self);
		} break;
		case Cpp_AssignmentOperator::t_BitwiseInclusiveOr: {
		    cpp_accept_binary_expression <Cpp::Expression_Generic, Cpp::Expression::Operation::Assignment_BitwiseInclusiveOr> (self);
		} break;
		default:
		    abortIfReached ();
	    }
	} break;
	default:
	    abortIfReached ();
    }

    return true;
}

bool
cpp_accept_throw_expression (Cpp_ThrowExpression * const throw_expression,
			     ParserControl       * const /* parser_control */,
			     void                * const _self)
{
    if (throw_expression == NULL)
	return true;

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    cpp_accept_unary_expression<Cpp::Expression_Generic, Cpp::Expression::Operation::Throw> (*self);

    return true;
}

bool
cpp_accept_comma_expression (Cpp_Expression * const expression,
			     ParserControl  * const /* parser_control */,
			     void           * const _self)
{
    if (expression == NULL)
	return true;

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    switch (expression->expression_type) {
	case Cpp_Expression::t_Comma: {
	    Ref<Cpp::Expression> expression__comma =
		    grab (new Cpp::Expression_Generic (Cpp::Expression::Operation::Comma));

	    Ref<Cpp::Expression> left,
				 right;

	    cpp_get_lr_binary_expression_args (self, left, right);

	    expression__comma->expressions.append (left);
	    expression__comma->expressions.append (right);

	    CppParser_Impl::ExpressionState &target_expression_state =
		    cpp_get_lr_binary_expression_target_state (self);

	    cpp_accept_expression (*self, target_expression_state, expression__comma);
	} break;
	default:
	    abortIfReached ();
    }

    return true;
}

bool
cpp_NamedNamespaceDefinition_Identifier_accept_func (Cpp_NamedNamespaceDefinition * const named_namespace_definition,
						     ParserControl                * const /* parser_control */,
						     void                         * const _self)
{
    if (named_namespace_definition == NULL)
	return true;

    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    DEBUG (
	errf->print ("cpp_NamedNamespaceDefinition_Identifier_accept_func").pendl ();
    )

    abortIf (named_namespace_definition->identifier == NULL);

    self->beginNamespace (named_namespace_definition->identifier->any_token->token);

    return true;
}

void
cpp_NamedNamespaceDefinition_accept_func (Cpp_NamedNamespaceDefinition * const /* named_namespace_definition */,
					  ParserControl                * const /* parser_control */,
					  void                         * const /* _self */)
{
//    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    DEBUG (
	errf->print ("cpp_NamedNamespaceDefinition_accept_func").pendl ();
    )

// TODO
//    self->name_tracker->endNamespace ();
}

bool
cpp_NamespaceName_match_func (Cpp_NamespaceName * const namespace_name,
			      ParserControl     * const /* parser_control */,
			      void              * const _self)
{
    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    abortIf (self->nested_name_specifier_states.isEmpty ());
    CppParser_Impl::NestedNameSpecifierState &nested_name_specifier_state =
	    self->nested_name_specifier_states.last->data;

    bool global_scope = false;
    if (nested_name_specifier_state.atoms.isEmpty ())
	global_scope = nested_name_specifier_state.global_scope;

    Cpp::Namespace *namespace_ =
	    self->lookupNamespace (namespace_name->identifier->any_token->token,
				   global_scope);
    if (namespace_ == NULL)
	return false;

    namespace_name->user_obj = namespace_;

    return true;
}

bool
cpp_TypedefName_match_func (Cpp_TypedefName * const typedef_name,
			    ParserControl   * const /* parser_control */,
			    void            * const _parser_state)
{
  FUNC_NAME (
    static char const * const _func_name = "Scruffy.CppParser_Impl.cpp_TypedefName_match_func";
  )

    DEBUG (
	errf->print (_func_name).pendl ();
    )

    CppParser_Impl * const parser_state = static_cast <CppParser_Impl*> (_parser_state);

  // TODO if (is_typedef) ...

    Ref<Cpp::Member> const member =
	    parser_state->lookupType (typedef_name->identifier->any_token->token);
    if (member.isNull ()) {
	DEBUG (
	    errf->print (_func_name).print (": false").pendl ();
	)
	return false;
    }

    if (member->getType () != Cpp::Member::t_Type ||
	!static_cast <Cpp::Member_Type*> (member.ptr ())->is_typedef)
    {
	return false;
    }

    typedef_name->user_obj = member->type_desc;

    DEBUG (
	errf->print (_func_name).print (": true").pendl ();
    )
    return true;
}

bool
cpp_EnumName_match_func (Cpp_EnumName  * const /* enum_name */,
			 ParserControl * const /* parser_control */,
			 void          * const /* _data */)
{
/*
    DEBUG (
	errf->print ("Scruffy.CppParser_Impl.cpp_EnumName_match_func").pendl ();
    )
*/
    return false;
}

bool
cpp_ClassNameIdentifier_match_func (Cpp_ClassNameIdentifier * const class_name_identifier,
				    ParserControl           * const /* parser_control */,
				    void                    * const _parser_state)
{
/*
    DEBUG (
	errf->print ("Scruffy.CppParser_Impl.cpp_ClassNameIdentifier_match_func").pendl ();
    )
*/

    CppParser_Impl *parser_state = static_cast <CppParser_Impl*> (_parser_state);

    Cpp::Member const * const member =
	    parser_state->lookupType (class_name_identifier->identifier->any_token->token);

    if (member == NULL)
	return false;

    abortIf (member->type_desc.isNull ());
    if (member->type_desc->getType () == Cpp::TypeDesc::t_Class) {
	Cpp::TypeDesc_Class * const type_desc__class =
		static_cast <Cpp::TypeDesc_Class*> (member->type_desc.ptr ());

	class_name_identifier->user_obj = type_desc__class;
	return true;
    }

    return false;
}

bool
cpp_TemplateName_match_func (Cpp_TemplateName * const template_name,
			     ParserControl    * const /* parser_control */,
			     void             * const _self)
{
    CppParser_Impl * const self = static_cast <CppParser_Impl*> (_self);

    abortIf (self->nested_name_specifier_states.isEmpty ());
    CppParser_Impl::NestedNameSpecifierState &nested_name_specifier_state =
	    self->nested_name_specifier_states.last->data;

    bool global_scope = false;
    if (nested_name_specifier_state.atoms.isEmpty ())
	global_scope = nested_name_specifier_state.global_scope;

    {
	// TODO Creation of a temporal 'Name' object looks excessive.
	Cpp::Name tmp_name;
	tmp_name.name_str = grab (new String (template_name->identifier->any_token->token));

	Cpp::Member * const member = self->lookupMember (nested_name_specifier_state.global_scope,
							 nested_name_specifier_state.atoms,
							 tmp_name);
	if (member == NULL)
	    return false;

	if (member->isTemplate ())
	    return true;
    }

    return true;
}

Ref<Pargen::LookupData>
CppParser::getLookupData ()
{
    return impl;
}

Ref<Cpp::Namespace>
CppParser::getRootNamespace ()
{
    return impl->root_namespace_entry->namespace_;
}

CppParser::CppParser (ConstMemoryDesc const &default_variant)
{
    impl = grab (new CppParser_Impl (default_variant));
}

CppParser::~CppParser ()
{
}

}

