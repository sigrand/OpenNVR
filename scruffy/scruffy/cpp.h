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


#ifndef SCRUFFY__CPP_H__
#define SCRUFFY__CPP_H__

#include <mycpp/mycpp.h>

namespace Scruffy {

using namespace MyCpp;

namespace Cpp {

class TemplateArgument;

class Name;

class TypeDesc;
class TypeDesc_BasicType;
class TypeDesc_Class;
class TypeDesc_Enum;
class TypeDesc_Function;

class Member;

class Initializer;

class Container;

class NamespaceEntry;
class Namespace;

class Class;
class Enum;

class MemInitializer;
class MemInitializer_Base;
class MemInitializer_Data;

class Function;

class Statement;
class Statement_Label;
class Statement_Expression;
class Statement_Compound;
class Statement_If;
class Statement_Switch;
class Statement_While;
class Statement_DoWhile;
class Statement_For;
class Statement_Break;
class Statement_Continue;
class Statement_Return;
class Statement_Goto;
class Statement_Declaration;
class Statement_Try;

class Value;
class Expression;
// TODO List of expressions


class TemplateArgument : public SimplyReferenced
{
public:
    enum Type {
	t_Type,
	t_Expression
    };

private:
    TemplateArgument& operator = (TemplateArgument const &);
    // TODO Copy constructor - ?

    Type const self_type;

protected:
    TemplateArgument (Type const self_type)
	: self_type (self_type)
    {
    }

public:
    Type getType ()
    {
	return self_type;
    }
};

class TemplateArgument_Type : public TemplateArgument
{
public:
    Ref<TypeDesc> type_desc;

    TemplateArgument_Type ()
	: TemplateArgument (TemplateArgument::t_Type)
    {
    }
};

class TemplateArgument_Expression : public TemplateArgument
{
public:
    Ref<Expression> expression;

    TemplateArgument_Expression ()
	: TemplateArgument (TemplateArgument::t_Expression)
    {
    }
};


// When we obtain a name, we do not yet know
// whether it refers to a type or to an object.
// Names are used to make queries to NameTracker.
// The form is mostly dictated by the grammar.
class Name : public Cloneable<Name>,
	     public SimplyReferenced
{
public:
  // Namespace-related fields

    class NameComparator
    {
    private:
	static ComparisonResult compare (Name const &left,
					 Name const &right);

    public:
	static bool greater (Name const &left,
			     Name const &right)
	{
	    return compare (left, right) == ComparisonGreater;
	}

	static bool equals (Name const &left,
			    Name const &right)
	{
	    return compare (left, right) == ComparisonEqual;
	}
    };

    typedef Map< Ref<Name>,
		 DereferenceExtractor<Name const>,
		 NameComparator >
	    NameMap;

    // The namespace that the name belongs to.
    Namespace *parent_namespace;
    // Utility link to the namespace's name map.
    NameMap::Entry parent_namespace_link;

  // (End of namespace-related fields)

#if 0
    // nested-name-specifier is represented as a list
    // of NestedNameSpecifier objects.
    class NestedNameSpecifier : public virtual SimplyReferenced
    {
    public:
	Ref<String> name;

	Bool is_template;
	Ref<TemplateArgument> template_arguments;
    };
#endif

    class OperatorType
    {
    public:
	enum OperatorType_
	{
	    Unknown,
	    New,
	    Delete,
	    NewArray,
	    DeleteArray,
	    Plus,
	    Minus,
	    Multiply,
	    Divide,
	    Remainder,
	    ExclusiveOr,
	    And,
	    InclusiveOr,
	    Inversion,
	    Not,
	    Set,
	    Less,
	    Greater,
	    PlusSet,
	    MinusSet,
	    MultiplySet,
	    DivideSet,
	    RemainderSet,
	    ExclusiveOrSet,
	    AndSet,
	    InclusiveOrSet,
	    LeftShift,
	    RightShift,
	    RightShiftSet,
	    LeftShiftSet,
	    Equal,
	    NotEqual,
	    LessOrEqual,
	    GreaterOrEqual,
	    LogicalAnd,
	    LogicalOr,
	    Increment,
	    Decrement,
	    Comma,
	    MemberDereference,
	    Dereference,
	    FunctionCall,
	    Subscripting
	};

	OperatorType_ value;

	operator OperatorType_ () const
	{
	    return value;
	}

	OperatorType& operator = (OperatorType_ value)
	{
	    this->value = value;
	    return *this;
	}

	OperatorType (OperatorType_ value)
	{
	    this->value = value;
	}

	OperatorType ()
	    : value (Unknown)
	{
	}

	Ref<String> toString () const;
    };

    // The name (identifier).
    // null if the name does not contain an identifier.
    Ref<String> name_str;

#if 0
// Deprecated

    // qualified-id
    Bool global_namespace;
    List< Ref<NestedNameSpecifier> > nested_name_specifiers;
#endif

    // unqualified-id/Destructor
    Bool is_destructor;

    // operator-function-id
    Bool is_operator;
    OperatorType operator_type;

    // conversion-function-id
    Bool is_conversion_operator;
    Ref<TypeDesc> conversion_type;

    // template-id
    Bool is_template;
    List< Ref<TemplateArgument> > template_arguments;

private:
    Name& operator = (Name const &);
    Name (Name const &);

public:
    Name* clone () const;

    void setName (Pointer<Name const> const &name);

    // Note: Name::toString() has turned out to be a bad idea because of the
    // need to dump template arguments, which contain arbitrary type names and
    // expressions.
    Ref<String> toString () const;

    bool validate () const;

    Name ()
    {
	parent_namespace = NULL;
    }
};

class TypeDesc : public Cloneable<TypeDesc>,
		 public SimplyReferenced
{
public:
    enum Type {
	t_BasicType,
	t_Class,
	t_Enum,
	t_Function,
	t_Dependent
    };

    // Used when comparing names (TODO?)
    class TypeDescComparator
    {
    public:
	static ComparisonResult compare (TypeDesc const & /* left */,
					 TypeDesc const & /* right */)
	{
	    // TODO
	    return ComparisonEqual;
	}
    };

    class TypeAtom : public Cloneable<TypeAtom>,
		     public SimplyReferenced
    {
    public:
	enum Type {
	    t_Pointer,
	    t_PointerToMember,
	    t_Array
	};

	Type const type_atom_type;

    private:
	// Non-copyable (there's no implementation for this method).
	TypeAtom& operator = (TypeAtom const &);

    protected:
	// Copy constructor for child classes' clone() methods.
	TypeAtom (TypeAtom const &type_atom)
	    : M::Referenced (),
	      SimplyReferenced (),
	      type_atom_type (type_atom.type_atom_type)
	{
	}

    public:
	TypeAtom (Type type_atom_type)
	    : type_atom_type (type_atom_type)
	{
	}
    };

    class TypeAtom_Pointer : public TypeAtom
    {
    public:
	Bool is_const;
	Bool is_volatile;

	TypeAtom* clone () const
	{
	    return new TypeAtom_Pointer (*this);
	}

	TypeAtom_Pointer ()
	    : TypeAtom (TypeAtom::t_Pointer)
	{
	}
    };

    class TypeAtom_PointerToMember : public TypeAtom
    {
    public:
	Bool is_const;
	Bool is_volatile;

	// TODO TypeDesc_Class* (to allow undefined classes)
//	ClassDefinition *containing_class;

	TypeAtom* clone () const
	{
	    return new TypeAtom_PointerToMember (*this);
	}

	TypeAtom_PointerToMember ()
	    : TypeAtom (TypeAtom::t_PointerToMember)
	{
	}
    };

    class TypeAtom_Array : public TypeAtom
    {
    private:
	TypeAtom_Array (TypeAtom_Array const &type_atom__array)
	    : M::Referenced (),
	      TypeAtom (type_atom__array)
	{
	    // TODO size_expression
	}

    public:
	Ref<Expression> size_expression;

	TypeAtom* clone () const
	{
	    return new TypeAtom_Array (*this);
	}

	TypeAtom_Array ()
	    : TypeAtom (TypeAtom::t_Array)
	{
	}
    };

private:
    Type const self_type;

public:
    Bool is_template;
    List< Ref<TemplateArgument> > template_arguments;

    // There can be only one reference atom at the very top,
    // hence there's no need in a TypeAtom to describe it.
    Bool is_reference;

    List< Ref<TypeAtom> > type_atoms;

#if 0
// TODO
// Unnecessary? (Belongs to Member)    Container *container;
    // Binding of the type to the namespace, if there is any.
    // For non-typedef basic types 'member' is always NULL.
    Member *member;
#endif

    // For functions, 'is_const' and 'is_volatile' refer
    // to implicit 'this' argument.
    Bool is_const,
	 is_volatile;

    Bool simple_type_specifier_used;

private:
    // Non-copyable (there's no implementation for this method).
    TypeDesc& operator = (TypeDesc const &);

protected:
    // Copy constructor for child classes' clone() methods.
    TypeDesc (TypeDesc const &type_desc)
	: M::Referenced (),
	  SimplyReferenced (),
	  self_type    (type_desc.self_type),
	  is_template  (type_desc.is_template),
	  is_reference (type_desc.is_reference),
// TODO
//	  container    (type_desc.container),
//	  memmber      (type_desc.member),
	  is_const     (type_desc.is_const),
	  is_volatile  (type_desc.is_volatile),
	  simple_type_specifier_used (type_desc.simple_type_specifier_used)
    {
	{
	    List< Ref<TemplateArgument> >::DataIterator iter (type_desc.template_arguments);
	    while (!iter.done ()) {
		Ref<TemplateArgument> &template_argument = iter.next ();
		abortIf (template_argument.isNull ());

		template_arguments.append (grab (new TemplateArgument (*template_argument)));
	    }
	}

	{
	    List< Ref<TypeAtom> >::DataIterator iter (type_desc.type_atoms);
	    while (!iter.done ()) {
		Ref<TypeAtom> &type_atom = iter.next ();
		abortIf (type_atom.isNull ());

		type_atoms.append (grab (type_atom->clone ()));
	    }
	}
    }

public:
    Type getType () const
    {
	return self_type;
    }

    bool isClass ()
    {
	return getType () == Cpp::TypeDesc::t_Class &&
	       !is_reference                        &&
	       type_atoms.isEmpty ();
    }

    bool isFunction ()
    {
	return getType () == Cpp::TypeDesc::t_Function &&
	       !is_reference                           &&
	       type_atoms.isEmpty ();
    }

    TypeDesc (Type self_type)
	: self_type (self_type)
    {
    }
};

class TypeDesc_BasicType : public TypeDesc
{
public:
    enum BasicType
    {
	Char,
	SignedChar,
	UnsignedChar,
	ShortInt,
	Int,
	LongInt,
	UnsignedShortInt,
	UnsignedInt,
	UnsignedLongInt,
	WcharT,
	Bool,
	Float,
	Double,
	LongDouble,
	Void
    };

    // 'basic_type' may be changed on-the-fly, hence non-const.
    BasicType basic_type;

    TypeDesc* clone () const
    {
	return new TypeDesc_BasicType (*this);
    }

    TypeDesc_BasicType ()
	: TypeDesc (TypeDesc::t_BasicType),
	  basic_type (Void)
    {
    }
};

class TypeDesc_Class : public TypeDesc
{
public:
    // Always non-zero.
    Ref<Class> class_;

    TypeDesc* clone () const
    {
	return new TypeDesc_Class (*this);
    }

    TypeDesc_Class ()
	: TypeDesc (TypeDesc::t_Class)
    {
    }
};

class TypeDesc_Enum : public TypeDesc
{
public:
    Ref<Enum> enum_;

    TypeDesc* clone () const
    {
	return new TypeDesc_Enum (*this);
    }

    TypeDesc_Enum ()
	: TypeDesc (TypeDesc::t_Enum)
    {
    }
};

class TypeDesc_Function : public TypeDesc
{
public:
    Ref<TypeDesc> return_type;
    List< Ref<Member> > parameters;

    // TODO
    // throw spec,

private:
    TypeDesc_Function (TypeDesc_Function const &type_desc__function);

public:
    TypeDesc* clone () const
    {
	return new TypeDesc_Function (*this);
    }

    TypeDesc_Function ()
	: TypeDesc (TypeDesc::t_Function)
    {
    }
};

class ContainerName : public SimplyReferenced
{
public:
    Ref<String> name_str;

    Bool is_template;
    List< Ref<Member> > template_arguments;
};

class TypeDesc_Dependent : public TypeDesc
{
public:
    List< Ref<ContainerName> > nested_name;
    Ref<Name> name;

private:
    TypeDesc_Dependent (TypeDesc_Dependent const &type_desc)
	: M::Referenced(),
	  TypeDesc (TypeDesc::t_Dependent)
    {
	{
	    List< Ref<ContainerName> >::DataIterator iter (type_desc.nested_name);
	    while (!iter.done ()) {
		Ref<ContainerName> &container_name = iter.next ();
		nested_name.append (container_name);
	    }
	}
    }

public:
    TypeDesc* clone () const
    {
	return new TypeDesc_Dependent (*this);
    }

    TypeDesc_Dependent ()
	: TypeDesc (TypeDesc::t_Dependent)
    {
    }
};

class Member : public Cloneable<Member>,
	       public SimplyReferenced
{
public:
    enum Type {
	t_Type,
	t_Object,
	t_Function,
	t_TypeTemplateParameter,
	t_DependentType,
	t_DependentObject
    };

    enum Category {
	Generic,
	FunctionParameter,
	TemplateParameter
    };

protected:
    Type const self_type;
    Category const category;

    Member (Member const &member)
	: M::Referenced (),
	  Cloneable<Member> (*this),
	  SimplyReferenced (*this),
	  self_type (member.self_type),
	  category  (member.category),
	  type_desc (member.type_desc->clone ()),
	  name      (member.name->clone ())
    {
    }

    Member (Type const self_type,
	    Category const category = Generic)
	: self_type (self_type),
	  category  (category),
	  parent_container (NULL)
    {
    }

public:
    // PROP Dependent members are allowed to have null 'type_desc'.
    // For type template parameters ("class T" or "typename T")
    // 'type_desc' is null.
    Ref<TypeDesc> type_desc;
    Ref<Name> name;

    Container *parent_container;

    typedef MultiMap< Ref<Member>,
		      MemberExtractor< Member,
				       Ref<Name>,
				       &Member::name,
				       Name&,
				       DereferenceExtractor<Name> >,
		      Name::NameComparator >
	    MemberMap;

    Type getType () const
    {
	return self_type;
    }

    Category getCategory () const
    {
	return category;
    }

    bool isObject () const;

    bool isClass () const;

    bool isTemplate () const;

  // Constructors are protected (see above)
};

class Member_Type : public Member
{
public:
    Bool is_typedef;

    Member* clone () const
    {
	return new Member_Type (*this);
    }

    Member_Type (Category const category = Generic)
	: Member (Member::t_Type, category)
    {
    }
};

class Member_Object : public Member
{
public:
    Bool is_auto,
	 is_register,
	 is_static,
	 is_extern,
	 is_mutable;

    Ref<Initializer> initializer;

    Member* clone () const
    {
	return new Member_Object (*this);
    }

    Member_Object (Category const category = Generic)
	: Member (Member::t_Object, category)
    {
    }
};

class Member_Function : public Member
{
public:
    Bool is_inline,
	 is_virtual,
	 is_explicit;

    Ref<Function> function;

    Member* clone () const
    {
	// TODO Clone 'function'
	return new Member_Function (*this);
    }

    Member_Function (Category const category = Generic)
	: Member (Member::t_Function, category)
    {
    }
};

class Member_TypeTemplateParameter : public Member
{
public:
    Bool is_template;
    List< Ref<Member> > template_parameters;

    Ref<TypeDesc> default_type;

private:
    Member_TypeTemplateParameter (Member_TypeTemplateParameter const &member)
	: M::Referenced (),
	  Member (*this)
    {
	{
	    List< Ref<Member> >::DataIterator iter (member.template_parameters);
	    while (!iter.done ()) {
		Ref<Member> &member = iter.next ();
		template_parameters.append (member);
	    }
	}
    }

public:
    Member* clone () const
    {
	return new Member_TypeTemplateParameter (*this);
    }

    Member_TypeTemplateParameter ()
	: Member (Member::t_TypeTemplateParameter, Member::Generic)
    {
    }
};

class Member_DependentType : public Member
{
public:
    List< Ref<ContainerName> > nested_name;
    Ref<Name> name;

private:
    Member_DependentType (Member_DependentType const &member)
	: M::Referenced (),
	  Member (*this),
	  name (member.name ? member.name->clone () : NULL)
    {
	{
	    List< Ref<ContainerName> >::DataIterator iter (member.nested_name);
	    while (!iter.done ()) {
		Ref<ContainerName> &container_name = iter.next ();
		nested_name.append (container_name);
	    }
	}
    }

public:
    Member* clone () const
    {
	return new Member_DependentType (*this);
    }

    Member_DependentType (Category const category = Generic)
	: Member (Member::t_DependentType, category)
    {
    }
};

class Member_DependentObject : public Member
{
public:
    List< Ref<ContainerName> > nested_name;
    Ref<Name> name;

    Ref<Initializer> initializer;

private:
    Member_DependentObject (Member_DependentObject const &member)
	: M::Referenced (),
	  Member (*this),
	  name (member.name ? member.name->clone () : NULL),
	  initializer (member.initializer)
    {
	{
	    List< Ref<ContainerName> >::DataIterator iter (member.nested_name);
	    while (!iter.done ()) {
		Ref<ContainerName> &container_name = iter.next ();
		nested_name.append (container_name);
	    }
	}
    }

public:
    Member* clone () const
    {
	return new Member_DependentObject (*this);
    }

    Member_DependentObject (Category const category = Generic)
	: Member (Member::t_DependentObject, category)
    {
    }
};

// NamespaceEntry is here to allow namespace aliases.
class NamespaceEntry : public SimplyReferenced
{
public:
    // May be null (root namespace, anonymous namespaces).
    Ref<String> name;

    typedef Map< Ref<NamespaceEntry>,
		 MemberExtractor< NamespaceEntry,
				  Ref<String>,
				  &NamespaceEntry::name,
				  MemoryDesc,
				  AccessorExtractor< String,
						     MemoryDesc,
						     &String::getMemoryDesc > >,
		 MemoryComparator<> >
	    NamespaceEntryMap;

    Ref<Namespace> namespace_;

// Unused    Namespace *containing_namespace;

    NamespaceEntry ()
// Unused	: containing_namespace (NULL)
    {
    }
};

class Container : public SimplyReferenced,
		  public UidProvider
{
public:
    enum Type {
	t_Namespace,
	t_Class
    };

private:
    Type self_type;

public:
    Container *parent_container;

    Container* getParentContainer () const
    {
	return parent_container;
    }

    // Dummy class for proper order of declarations.
    class NamespacePrimaryNameExtractor
    {
    public:
	static inline MemoryDesc getValue (Ref<Namespace> const &namespace_);
    };

    typedef Map< Ref<Namespace>,
		 NamespacePrimaryNameExtractor,
		 MemoryComparator<> >
	    NamespaceMap;

    // TODO
    // 'namespace_entries' for namespace entries,
    // 'namespaces' for namespaces by their primary name.
    // That's because namespace_entries may contain multiple entries
    // for the same namespace (namespace aliases).
    NamespaceMap namespaces;
    NamespaceEntry::NamespaceEntryMap namespace_entries;

    Member::MemberMap members;

    Type getType () const
    {
	return self_type;
    }

    Container (Type self_type)
	: self_type (self_type),
	  parent_container (NULL)
    {
    }
};

class Namespace : public Container
{
public:
//    Namespace *parent_namespace;

// TODO
//    List<Namespace*> using_namespaces;

    Ref<String> primary_name;

    ConstMemoryDesc getPrimaryName () const
    {
	if (!primary_name.isNull ())
	    return primary_name->getMemoryDesc ();

	return ConstMemoryDesc ();
    }

    Namespace ()
	: Container (Container::t_Namespace)
//	: parent_namespace (NULL)
    {
    }
};

// We need class Namespace to be defined to be able to extract its 'primary_name' member,
// hence this method is defined outside of class Container.
MemoryDesc
Container::NamespacePrimaryNameExtractor::getValue (Ref<Namespace> const &namespace_)
{
    return MemberExtractor< Namespace,
			    Ref<String>,
			    &Namespace::primary_name,
			    MemoryDesc,
			    AccessorExtractor< String,
					       MemoryDesc,
					       &String::getMemoryDesc > >
	   ::getValue (namespace_);
}

class TemplateInstance : public virtual SimplyReferenced
{
public:
    enum Type {
	t_Class,
	t_Function
    };

private:
    Type const self_type;

public:
    List< Ref<Cpp::TemplateArgument> > template_arguments;

protected:
    TemplateInstance (Type const self_type)
	: self_type (self_type)
    {
    }

public:
    Type getType () const
    {
	return self_type;
    }
};

class ClassTemplateInstance : public TemplateInstance,
			      public virtual SimplyReferenced
{
public:
    Class *template_class;

    // Result of template istantiation
    Ref<Class> class_;

    ClassTemplateInstance ()
	: TemplateInstance (TemplateInstance::t_Class)
    {
    }
};

class FunctionTemplateInstance : public TemplateInstance,
				 public virtual SimplyReferenced
{
public:
    Function *template_function;

    // Result of template instantiation.
    Ref<Function> function;

    FunctionTemplateInstance ()
	: TemplateInstance (TemplateInstance::t_Function)
    {
    }
};

class Class : public Container
{
public:
    // TODO Cpp::AccessRights::
    //      access rights for members.
    enum AccessRights
    {
	Public,
	Private,
	Protected
    };

    class ParentEntry : public virtual SimplyReferenced
    {
    public:
	Bool is_virtual;
	AccessRights access_rights;
	Ref<TypeDesc_Class> type_desc__class;
    };

    Bool is_template;
    List< Ref<Member> > template_parameters;
    List< Ref<ClassTemplateInstance> > template_instances;

    Bool is_union;

    // TODO Member*
    // May be null.
    Ref<Name> name;

    Bool got_definition;

    // We know the parents only after we've got a class definition.
    List< Ref<ParentEntry> > parents;

    Class ()
	: Container (Container::t_Class)
    {
    }
};

// TODO Rewrite similar to class Class
class Enum : public virtual SimplyReferenced
{
public:
    class EnumEntry : public virtual SimplyReferenced
    {
    public:
	Ref<String> name;
	Int64 value;
    };

    // May be null.
    Ref<Name> name;
    List< Ref<Name> > name_aliases;

    // TODO map
    List< Ref<EnumEntry> > enum_entries;
};

class Initializer : public SimplyReferenced
{
public:
    enum Type {
	t_Assignment,
	t_Constructor,
	t_InitializerList
    };

private:
    Type const self_type;

public:
    Type getType () const
    {
	return self_type;
    }

    Initializer (Type self_type)
	: self_type (self_type)
    {
    }
};

class Initializer_Assignment : public Initializer
{
public:
    Ref<Expression> expression;

    Initializer_Assignment ()
	: Initializer (Initializer::t_Assignment)
    {
    }
};

class Initializer_Constructor : public Initializer
{
public:
    List< Ref<Expression> > expressions;

    Initializer_Constructor ()
	: Initializer (Initializer::t_Constructor)
    {
    }
};

class Initializer_InitializerList : public Initializer
{
public:
    List< Ref<Expression> > expressions;

    Initializer_InitializerList ()
	: Initializer (Initializer::t_InitializerList)
    {
    }
};

class MemInitializer : public SimplyReferenced
{
public:
    enum Type {
	t_Base,
	t_Data
    };

private:
    Type const self_type;

public:
    Ref<Name> name;

//    Ref<TypeDesc> type_desc;

    List< Ref<Expression> > arguments;

    Type getType ()
    {
	return self_type;
    }

    MemInitializer (Type self_type)
	: self_type (self_type)
    {
    }
};

class MemInitializer_Base : public MemInitializer
{
public:
    MemInitializer_Base ()
	: MemInitializer (MemInitializer::t_Base)
    {
    }
};

class MemInitializer_Data : public MemInitializer
{
public:
//    Ref<Object> object;

    MemInitializer_Data ()
	: MemInitializer (MemInitializer::t_Data)
    {
    }
};

class Function : public virtual SimplyReferenced
{
public:
    // TODO Additional properties (definition TypeDesc)
    // TODO Member* (^^^)

    Bool is_template;
    List< Ref<Member> > template_parameters;
    List< Ref<FunctionTemplateInstance> > template_instances;

    List<Member*> parameters;

    Ref<Namespace> namespace_;

    Bool got_definition;

    List< Ref<MemInitializer> > mem_initializers;

    List< Ref<Statement> > statements;
};

class Statement : public SimplyReferenced
{
public:
    enum Type {
	t_Label,
	t_Expression,
	t_Compound,
	t_If,
	t_Switch,
	t_While,
	t_DoWhile,
	t_For,
	t_Break,
	t_Continue,
	t_Return,
	t_Goto,
	t_Declaration,
	t_Try
    };

private:
    Type const self_type;

public:
    Type getType () const
    {
	return self_type;
    }

    Statement (Type self_type)
	: self_type (self_type)
    {
    }
};

class Statement_Expression : public Statement
{
public:
    Ref<Expression> expression;

    Statement_Expression ()
	: Statement (Statement::t_Expression)
    {
    }
};

class Statement_Label : public Statement
{
public:
    Ref<String> label;

    Statement_Label ()
	: Statement (Statement::t_Label)
    {
    }
};

class Statement_Compound : public Statement
{
public:
    List< Ref<Statement> > statements;

    Statement_Compound ()
	: Statement (Statement::t_Compound)
    {
    }
};

class Statement_If : public Statement
{
public:
    Ref<Expression> condition;

    Ref<Statement> if_statement;
    Ref<Statement> else_statement;

    Statement_If ()
	: Statement (Statement::t_If)
    {
    }
};

class Statement_Switch : public Statement
{
public:
    // TODO

    Ref<Expression> condition;

    Statement_Switch ()
	: Statement (Statement::t_Switch)
    {
    }
};

class Statement_While : public Statement
{
public:
    Ref<Expression> condition_expression;
    Ref<Statement> statement;

    Statement_While ()
	: Statement (Statement::t_While)
    {
    }
};

class Statement_DoWhile : public Statement
{
public:
    Ref<Expression> condition_expression;
    Ref<Statement> statement;

    Statement_DoWhile ()
	: Statement (Statement::t_DoWhile)
    {
    }
};

class Statement_For : public Statement
{
public:
    Ref<Expression> condition_expression;
    Ref<Expression> iteration_expression;

    Ref<Statement> statement;

    Statement_For ()
	: Statement (Statement::t_For)
    {
    }
};

class Statement_Break : public Statement
{
public:
    Statement_Break ()
	: Statement (Statement::t_Break)
    {
    }
};

class Statement_Continue : public Statement
{
public:
    Statement_Continue ()
	: Statement (Statement::t_Continue)
    {
    }
};

class Statement_Return : public Statement
{
public:
    Ref<Expression> expression;

    Statement_Return ()
	: Statement (Statement::t_Return)
    {
    }
};

class Statement_Goto : public Statement
{
public:
    Ref<String> label;

    Statement_Goto ()
	: Statement (Statement::t_Goto)
    {
    }
};

class Statement_Declaration : public Statement
{
public:
    // What is being declared?
    // - Members (!)

//    Ref<DeclarationDesc> declaration_desc;

    // TODO This should be a plain pointer.
//    Member *member;
    Ref<Member> member;

    Statement_Declaration ()
	: Statement (Statement::t_Declaration)
    {
    }
};

class Statement_Try : public Statement
{
public:
    // TODO

    Statement_Try ()
	: Statement (Statement::t_Try)
    {
    }
};

class ConstantValue : public SimplyReferenced
{
public:
  // TODO

//    enum Type {
//    };
};

class Value : public SimplyReferenced
{
public:
    Bool is_lvalue;
    Ref<TypeDesc> type_desc;
    Ref<ConstantValue> constant_value;
};

class Expression : public SimplyReferenced
{
public:
    class Type
    {
    public:
	enum Type_ {
	    Literal,
	    This,
	    Id,
	    MemberAccess,
//	    Cast,
	    Generic,
	    ImplicitCast
	};

    private:
	Type_ value;

    public:
	operator Type_ () const
	{
	    return value;
	}

	Type (Type_ const value)
	    : value (value)
	{
	}

	Type ()
	{
	}
    };

    class Operation
    {
    public:
	enum Operation_ {
	    Literal,
	    This,
	    Id,

	    Subscripting,
	    FunctionCall,

	    TypeConversion,

	    DotMemberAccess,
	    ArrowMemberAccess,

	    PostfixIncrement,
	    PostfixDecrement,

	    DynamicCast,
	    StaticCast,
	    ReinterpretCast,
	    ConstCast,

	    TypeidExpression,
	    TypeidType,

	    PrefixIncrement,
	    PrefixDecrement,

	    SizeofExpression,
	    SizeofType,

	    Indirection,
	    PointerTo,
	    UnaryPlus,
	    UnaryMinus,
	    LogicalNegation,
	    OnesComplement,

	    Delete,

	    Cast,

	    DotPointerToMember,
	    ArrowPointerToMember,

	    Multiplication,
	    Division,
	    Remainder,

	    Addition,
	    Subtraction,

	    LeftShift,
	    RightShift,

	    Less,
	    Greater,
	    LessOrEqual,
	    GreaterOrEqual,

	    Equal,
	    NotEqual,

	    BitwiseAnd,
	    BitwiseExclusiveOr,
	    BitwiseInclusiveOr,
	    LogicalAnd,
	    LogicalOr,

	    Conditional,

	    Assignment,
	    Assignment_Multiplication,
	    Assignment_Division,
	    Assignment_Remainder,
	    Assignment_Addition,
	    Assignment_Subtraction,
	    Assignment_RightShift,
	    Assignment_LeftShift,
	    Assignment_BitwiseAnd,
	    Assignment_BitwiseExclusiveOr,
	    Assignment_BitwiseInclusiveOr,

	    Throw,

	    Comma,

	    ImplicitCast
	};

    private:
	Operation_ value;

    public:
	operator Operation_ () const
	{
	    return value;
	}

	Operation (Operation_ const value)
	    : value (value)
	{
	}

	Operation ()
	{
	}
    };

private:
    Type self_type;
    Operation operation;

public:
    Type getType () const
    {
	return self_type;
    }

    Operation getOperation () const
    {
	return operation;
    }

    List< Ref<Expression> > expressions;
    List< Ref<Expression> > cast_expressions;

    Ref<TypeDesc> type_argument;

    Ref<Value> value;

    Expression (Type      const self_type,
		Operation const operation)
	: self_type (self_type),
	  operation (operation)
    {
    }
};

class Expression_Literal : public Expression
{
public:
    // TODO

    Expression_Literal ()
	: Expression (Expression::Type::Literal, Expression::Operation::Literal)
    {
    }
};

// This is just a convenient way to construct c expressions.
class Expression_This : public Expression
{
public:
    Expression_This ()
	: Expression (Expression::Type::This, Expression::Operation::This)
    {
    }
};

class Expression_Id : public Expression
{
public:
    Bool global_scope;
    List<Cpp::Container*> nested_name;
    Ref<Name> name;
    Ref<Member> member;

    Expression_Id ()
	: Expression (Expression::Type::Id, Expression::Operation::Id)
    {
    }
};

class Expression_MemberAccess : public Expression
{
public:
    Ref<Name> member_name;

    Expression_MemberAccess (Expression::Operation const operation)
	: Expression (Expression::Type::MemberAccess, operation)
    {
    }
};

#if 0
class Expression_Cast : public Expression
{
public:
    Ref<TypeDesc> type_desc;

    Expression_Cast (Operation const operation)
	: Expression (Expression::Type::Cast, operation)
    {
    }
};
#endif

// This is just a convenient way to construct generic expressions.
class Expression_Generic : public Expression
{
public:
    Expression_Generic (Operation const operation)
	: Expression (Expression::Type::Generic, operation)
    {
    }
};

class Expression_ImplicitCast : public Expression
{
public:
  // TODO Describe an implicit cast somehow
};

#if 0
// Deprecated
class Expression : public SimplyReferenced
{
public:
    enum Type {
	_Literal,
	_This,
	_Id,

	_Subscripting,
	_FunctionCall,

	_TypeConversion,

	_DotMemberAccess,
	_ArrowMemberAccess,

	_PostfixIncrement,
	_PostfixDecrement,

	_DynamicCast,
	_StaticCast,
	_ReinterpretCast,
	_ConstCast,

	_TypeidExpression,
	_TypeidType,

	_PrefixIncrement,
	_PrefixDecrement,

	_SizeofExpression,
	_SizeofType,

	_Indirection,
	_PointerTo,
	_UnaryPlus,
	_UnaryMinus,
	_LogicalNegation,
	_OnesComplement,

	_Delete,

	_Cast,

	_DotPointerToMember,
	_ArrowPointerToMember,

	_Multiplication,
	_Division,
	_Remainder,

	_Addition,
	_Subtraction,

	_LeftShift,
	_RightShift,

	_Less,
	_Greater,
	_LessOrEqual,
	_GreaterOrEqual,

	_Equal,
	_NotEqual,

	_BitwiseAnd,
	_BitwiseExclusiveOr,
	_BitwiseInclusiveOr,
	_LogicalAnd,
	_LogicalOr,

	_Conditional,

	_Assignment,
	_Assignment_Multiplication,
	_Assignment_Division,
	_Assignment_Remainder,
	_Assignment_Addition,
	_Assignment_Subtraction,
	_Assignment_RightShift,
	_Assignment_LeftShift,
	_Assignment_BitwiseAnd,
	_Assignment_BitwiseExclusiveOr,
	_Assignment_BitwiseInclusiveOr,

	_Throw,

	_Comma
    };

private:
    Type const self_type;

public:
    // Type of the expression.
    Ref<TypeDesc> type_desc;

    Type getType () const
    {
	return self_type;
    }

    Expression (Type self_type)
	: self_type (self_type)
    {
    }
};

class Expression_Literal : public Expression
{
public:
    // TODO literal

    Expression_Literal ()
	: Expression (Expression::t_Literal)
    {
    }
};

class Expression_This : public Expression
{
public:
    Expression_This ()
	: Expression (Expression::t_This)
    {
    }
};

class Expression_Id : public Expression
{
public:
    Bool global_scope;
    List<Cpp::Container*> nested_name;

    Ref<Cpp::Name> name;

    Ref<Member> member;

    Expression_Id ()
	: Expression (Expression::t_Id)
    {
    }
};

class Expression_Subscripting : public Expression
{
public:
    Ref<Expression> expression;
    Ref<Expression> subscript;

    Expression_Subscripting ()
	: Expression (Expression::t_Subscripting)
    {
    }
};

class Expression_FunctionCall : public Expression
{
public:
    Ref<Expression> expression;
    List< Ref<Expression> > arguments;

    Expression_FunctionCall ()
	: Expression (Expression::t_FunctionCall)
    {
    }
};

class Expression_TypeConversion : public Expression
{
public:
    Ref<TypeDesc> type_desc;
    // May be null.
    Ref<Expression> expression;

    Expression_TypeConversion ()
	: Expression (Expression::t_TypeConversion)
    {
    }
};

class Expression_DotMemberAccess : public Expression
{
public:
    Ref<Expression> expression;
    Ref<Name> member_name;

    Expression_DotMemberAccess ()
	: Expression (Expression::t_DotMemberAccess)
    {
    }
};

class Expression_ArrowMemberAccess : public Expression
{
public:
    Ref<Expression> expression;
    Ref<Name> member_name;

    Expression_ArrowMemberAccess ()
	: Expression (Expression::t_ArrowMemberAccess)
    {
    }
};

class Expression_PostfixIncrement : public Expression
{
public:
    Ref<Expression> expression;

    Expression_PostfixIncrement ()
	: Expression (Expression::t_PostfixIncrement)
    {
    }
};

class Expression_PostfixDecrement : public Expression
{
public:
    Ref<Expression> expression;

    Expression_PostfixDecrement ()
	: Expression (Expression::t_PostfixDecrement)
    {
    }
};

class Expression_DynamicCast : public Expression
{
public:
    Ref<TypeDesc> type_desc;
    Ref<Expression> expression;

    Expression_DynamicCast ()
	: Expression (Expression::t_DynamicCast)
    {
    }
};

class Expression_StaticCast : public Expression
{
public:
    Ref<TypeDesc> type_desc;
    Ref<Expression> expression;

    Expression_StaticCast ()
	: Expression (Expression::t_StaticCast)
    {
    }
};

class Expression_ReinterpretCast : public Expression
{
public:
    Ref<TypeDesc> type_desc;
    Ref<Expression> expression;

    Expression_ReinterpretCast ()
	: Expression (Expression::t_ReinterpretCast)
    {
    }
};

class Expression_ConstCast : public Expression
{
public:
    Ref<TypeDesc> type_desc;
    Ref<Expression> expression;

    Expression_ConstCast ()
	: Expression (Expression::t_ConstCast)
    {
    }
};

class Expression_TypeidExpression : public Expression
{
public:
    Ref<Expression> expression;

    Expression_TypeidExpression ()
	: Expression (Expression::t_TypeidExpression)
    {
    }
};

class Expression_TypeidType : public Expression
{
public:
    Ref<TypeDesc> type_desc;

    Expression_TypeidType ()
	: Expression (Expression::t_TypeidType)
    {
    }
};

class Expression_PrefixIncrement : public Expression
{
public:
    Ref<Expression> expression;

    Expression_PrefixIncrement ()
	: Expression (Expression::t_PrefixIncrement)
    {
    }
};

class Expression_PrefixDecrement : public Expression
{
public:
    Ref<Expression> expression;

    Expression_PrefixDecrement ()
	: Expression (Expression::t_PrefixDecrement)
    {
    }
};

class Expression_SizeofExpression : public Expression
{
public:
    Ref<Expression> expression;

    Expression_SizeofExpression ()
	: Expression (Expression::t_SizeofExpression)
    {
    }
};

class Expression_SizeofType : public Expression
{
public:
    Ref<TypeDesc> type_desc;

    Expression_SizeofType ()
	: Expression (Expression::t_SizeofType)
    {
    }
};

class Expression_Indirection : public Expression
{
public:
    Ref<Expression> expression;

    Expression_Indirection ()
	: Expression (Expression::t_Indirection)
    {
    }
};

class Expression_PointerTo : public Expression
{
public:
    Ref<Expression> expression;

    Expression_PointerTo ()
	: Expression (Expression::t_PointerTo)
    {
    }
};

class Expression_UnaryPlus : public Expression
{
public:
    Ref<Expression> expression;

    Expression_UnaryPlus ()
	: Expression (Expression::t_UnaryPlus)
    {
    }
};

class Expression_UnaryMinus : public Expression
{
public:
    Ref<Expression> expression;

    Expression_UnaryMinus ()
	: Expression (Expression::t_UnaryMinus)
    {
    }
};

class Expression_LogicalNegation : public Expression
{
public:
    Ref<Expression> expression;

    Expression_LogicalNegation ()
	: Expression (Expression::t_LogicalNegation)
    {
    }
};

class Expression_OnesComplement : public Expression
{
public:
    Ref<Expression> expression;

    Expression_OnesComplement ()
	: Expression (Expression::t_OnesComplement)
    {
    }
};

class Expression_Delete : public Expression
{
public:
    // TODO

    Expression_Delete ()
	: Expression (Expression::t_Delete)
    {
    }
};

class Expression_Cast : public Expression
{
public:
    Ref<TypeDesc> type_desc;
    Ref<Expression> expression;

    Expression_Cast ()
	: Expression (Expression::t_Cast)
    {
    }
};

class Expression_DotPointerToMember : public Expression
{
public:
    // TODO

    Expression_DotPointerToMember ()
	: Expression (Expression::t_DotPointerToMember)
    {
    }
};

class Expression_ArrowPointerToMember : public Expression
{
public:
    // TODO

    Expression_ArrowPointerToMember ()
	: Expression (Expression::t_ArrowPointerToMember)
    {
    }
};

class Expression_Multiplication : public Expression
{
public:
    Ref<Expression> left;
    Ref<Expression> right;

    Expression_Multiplication ()
	: Expression (Expression::t_Multiplication)
    {
    }
};

class Expression_Division : public Expression
{
public:
    Ref<Expression> left;
    Ref<Expression> right;

    Expression_Division ()
	: Expression (Expression::t_Division)
    {
    }
};

class Expression_Remainder : public Expression
{
public:
    Ref<Expression> left;
    Ref<Expression> right;

    Expression_Remainder ()
	: Expression (Expression::t_Remainder)
    {
    }
};

class Expression_Addition : public Expression
{
public:
    Ref<Expression> left;
    Ref<Expression> right;

    Expression_Addition ()
	: Expression (Expression::t_Addition)
    {
    }
};

class Expression_Subtraction : public Expression
{
public:
    Ref<Expression> left;
    Ref<Expression> right;

    Expression_Subtraction ()
	: Expression (Expression::t_Subtraction)
    {
    }
};

class Expression_LeftShift : public Expression
{
public:
    Ref<Expression> left;
    Ref<Expression> right;

    Expression_LeftShift ()
	: Expression (Expression::t_LeftShift)
    {
    }
};

class Expression_RightShift : public Expression
{
public:
    Ref<Expression> left;
    Ref<Expression> right;

    Expression_RightShift ()
	: Expression (Expression::t_RightShift)
    {
    }
};

class Expression_Less : public Expression
{
public:
    Ref<Expression> left;
    Ref<Expression> right;

    Expression_Less ()
	: Expression (Expression::t_Less)
    {
    }
};

class Expression_Greater : public Expression
{
public:
    Ref<Expression> left;
    Ref<Expression> right;

    Expression_Greater ()
	: Expression (Expression::t_Greater)
    {
    }
};

class Expression_LessOrEqual : public Expression
{
public:
    Ref<Expression> left;
    Ref<Expression> right;

    Expression_LessOrEqual ()
	: Expression (Expression::t_LessOrEqual)
    {
    }
};

class Expression_GreaterOrEqual : public Expression
{
public:
    Ref<Expression> left;
    Ref<Expression> right;

    Expression_GreaterOrEqual ()
	: Expression (Expression::t_GreaterOrEqual)
    {
    }
};

class Expression_Equal : public Expression
{
public:
    Ref<Expression> left;
    Ref<Expression> right;

    Expression_Equal ()
	: Expression (Expression::t_Equal)
    {
    }
};

class Expression_NotEqual : public Expression
{
public:
    Ref<Expression> left;
    Ref<Expression> right;

    Expression_NotEqual ()
	: Expression (Expression::t_NotEqual)
    {
    }
};

class Expression_BitwiseAnd : public Expression
{
public:
    Ref<Expression> left;
    Ref<Expression> right;

    Expression_BitwiseAnd ()
	: Expression (Expression::t_BitwiseAnd)
    {
    }
};

class Expression_BitwiseExclusiveOr : public Expression
{
public:
    Ref<Expression> left;
    Ref<Expression> right;

    Expression_BitwiseExclusiveOr ()
	: Expression (Expression::t_BitwiseExclusiveOr)
    {
    }
};

class Expression_BitwiseInclusiveOr : public Expression
{
public:
    Ref<Expression> left;
    Ref<Expression> right;

    Expression_BitwiseInclusiveOr ()
	: Expression (Expression::t_BitwiseInclusiveOr)
    {
    }
};

class Expression_LogicalAnd : public Expression
{
public:
    Ref<Expression> left;
    Ref<Expression> right;

    Expression_LogicalAnd ()
	: Expression (Expression::t_LogicalAnd)
    {
    }
};

class Expression_LogicalOr : public Expression
{
public:
    Ref<Expression> left;
    Ref<Expression> right;

    Expression_LogicalOr ()
	: Expression (Expression::t_LogicalOr)
    {
    }
};

class Expression_Conditional : public Expression
{
public:
    // TODO

    Expression_Conditional ()
	: Expression (Expression::t_Conditional)
    {
    }
};

class Expression_Assignment : public Expression
{
public:
    Ref<Expression> left;
    Ref<Expression> right;

    Expression_Assignment ()
	: Expression (Expression::t_Assignment)
    {
    }
};

class Expression_Assignment_Multiplication : public Expression
{
public:
    Ref<Expression> left;
    Ref<Expression> right;

    Expression_Assignment_Multiplication ()
	: Expression (Expression::t_Assignment_Multiplication)
    {
    }
};

class Expression_Assignment_Division : public Expression
{
public:
    Ref<Expression> left;
    Ref<Expression> right;

    Expression_Assignment_Division ()
	: Expression (Expression::t_Assignment_Division)
    {
    }
};

class Expression_Assignment_Remainder : public Expression
{
public:
    Ref<Expression> left;
    Ref<Expression> right;

    Expression_Assignment_Remainder ()
	: Expression (Expression::t_Assignment_Remainder)
    {
    }
};

class Expression_Assignment_Addition : public Expression
{
public:
    Ref<Expression> left;
    Ref<Expression> right;

    Expression_Assignment_Addition ()
	: Expression (Expression::t_Assignment_Addition)
    {
    }
};

class Expression_Assignment_Subtraction : public Expression
{
public:
    Ref<Expression> left;
    Ref<Expression> right;

    Expression_Assignment_Subtraction ()
	: Expression (Expression::t_Assignment_Subtraction)
    {
    }
};

class Expression_Assignment_RightShift : public Expression
{
public:
    Ref<Expression> left;
    Ref<Expression> right;

    Expression_Assignment_RightShift ()
	: Expression (Expression::t_Assignment_RightShift)
    {
    }
};

class Expression_Assignment_LeftShift : public Expression
{
public:
    Ref<Expression> left;
    Ref<Expression> right;

    Expression_Assignment_LeftShift ()
	: Expression (Expression::t_Assignment_LeftShift)
    {
    }
};

class Expression_Assignment_BitwiseAnd : public Expression
{
public:
    Ref<Expression> left;
    Ref<Expression> right;

    Expression_Assignment_BitwiseAnd ()
	: Expression (Expression::t_Assignment_BitwiseAnd)
    {
    }
};

class Expression_Assignment_BitwiseExclusiveOr : public Expression
{
public:
    Ref<Expression> left;
    Ref<Expression> right;

    Expression_Assignment_BitwiseExclusiveOr ()
	: Expression (Expression::t_BitwiseExclusiveOr)
    {
    }
};

class Expression_Assignment_BitwiseInclusiveOr : public Expression
{
public:
    Ref<Expression> left;
    Ref<Expression> right;

    Expression_Assignment_BitwiseInclusiveOr ()
	: Expression (Expression::t_BitwiseInclusiveOr)
    {
    }
};

class Expression_Throw : public Expression
{
public:
    Ref<Expression> expression;

    Expression_Throw ()
	: Expression (Expression::t_Throw)
    {
    }
};

class Expression_Comma : public Expression
{
public:
    List< Ref<Expression> > expressions;

    Expression_Comma ()
	: Expression (Expression::t_Comma)
    {
    }
};
#endif

} // namespace Cpp

} // namespace Scruffy

#endif /* SCRUFFY__CPP_H__ */

