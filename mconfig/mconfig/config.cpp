/*  MConfig - C++ library for working with configuration files
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


#include <mconfig/util.h>

#include <mconfig/config.h>


using namespace M;

namespace MConfig {

namespace {
LogGroup libMary_logGroup_mconfig ("mconfig", LogLevel::I);
}

Result
Value::getAsDouble (double * const ret_val)
{
    if (!value_str)
	return Result::Failure;

    if (cached_type == CachedType_Double) {
	if (bad_value)
	    return Result::Failure;

	if (ret_val)
	    *ret_val = cached_value.cached_double;

	return Result::Success;
    }

    cached_type = CachedType_Double;

    if (!strToDouble_safe (value_str->mem(), &cached_value.cached_double)) {
	logE_ (_func, exc->toString());
	bad_value = true;
	return Result::Failure;
    }

    if (ret_val)
	*ret_val = cached_value.cached_double;

    return Result::Success;
}

Result
Value::getAsInt64 (Int64 * const ret_val)
{
    if (!value_str)
	return Result::Failure;

    if (cached_type == CachedType_Int64) {
	if (bad_value)
	    return Result::Failure;

	if (ret_val)
	    *ret_val = cached_value.cached_int64;

	return Result::Success;
    }

    cached_type = CachedType_Int64;

    if (!strToInt64_safe (value_str->mem(), &cached_value.cached_int64)) {
	logE_ (_func, exc->toString());
	bad_value = true;
	return Result::Failure;
    }

    if (ret_val)
	*ret_val = cached_value.cached_int64;

    return Result::Success;
}

Result
Value::getAsUint64 (Uint64 * const ret_val)
{
    if (!value_str)
	return Result::Failure;

    if (cached_type == CachedType_Uint64) {
	if (bad_value)
	    return Result::Failure;

	if (ret_val)
	    *ret_val = cached_value.cached_uint64;

	return Result::Success;
    }

    cached_type = CachedType_Uint64;

    if (!strToUint64_safe (value_str->mem(), &cached_value.cached_uint64)) {
	logE_ (_func, exc->toString());
	bad_value = true;
	return Result::Failure;
    }

    if (ret_val)
	*ret_val = cached_value.cached_uint64;

    return Result::Success;
}

BooleanValue
Option::getBoolean ()
{
    ConstMemory value_mem;
    if (getValue())
        value_mem = getValue()->mem();

    return strToBoolean (value_mem);
}

SectionEntry*
Section::getSectionEntry (ConstMemory const path_,
			  bool        const create,
			  SectionEntry::Type const section_entry_type)
{
    ConstMemory path = path_;
    while (path.len() > 0 && path.mem() [0] == '/')
	path = path.region (1);

    Byte const *delim = (Byte const *) memchr (path.mem(), '/', path.len());
    if (!delim) {
	if (!create)
	    return getSectionEntry_nopath (path);

	switch (section_entry_type) {
	    case SectionEntry::Type_Option:
		return getOption_nopath (path, create);
	    case SectionEntry::Type_Section:
		return getSection_nopath (path, create);
	    default:
		unreachable ();
	}
    }

    Section *section = getSection_nopath (path.region (0, delim - path.mem()), create);
    if (!section)
	return NULL;

    return section->getSectionEntry (path.region (delim - path.mem() + 1), create, section_entry_type);
}

Attribute*
Section::getAttribute (ConstMemory const attr_name)
{
    return attribute_hash.lookup (attr_name);
}

Option*
Section::getOption (ConstMemory const path,
		    bool        const create)
{
    SectionEntry * const section_entry = getSectionEntry (path, create, SectionEntry::Type_Option);
    if (!section_entry ||
	section_entry->getType() != SectionEntry::Type_Option)
    {
	return NULL;
    }

    return static_cast <Option*> (section_entry);
}

Section*
Section::getSection (ConstMemory const path,
		     bool        const create)
{
    SectionEntry * const section_entry = getSectionEntry (path, create, SectionEntry::Type_Section);
    if (!section_entry ||
	section_entry->getType() != SectionEntry::Type_Section)
    {
	return NULL;
    }

    return static_cast <Section*> (section_entry);
}

SectionEntry*
Section::getSectionEntry_nopath (ConstMemory const section_entry_name)
{
    return section_entry_hash.lookup (section_entry_name);
}

Option*
Section::getOption_nopath (ConstMemory const option_name,
			   bool        const create)
{
    SectionEntry * const section_entry = section_entry_hash.lookup (option_name);
    if (!section_entry ||
	section_entry->getType() != SectionEntry::Type_Option)
    {
	if (section_entry)
	    removeSectionEntry (section_entry);

	if (create) {
	    Option * const option = new Option (option_name);
	    addOption (option);
	    return option;
	}

	return NULL;
    }

    return static_cast <Option*> (section_entry);
}

Section*
Section::getSection_nopath (ConstMemory const section_name,
			    bool        const create)
{
    SectionEntry * const section_entry = section_entry_hash.lookup (section_name);
    if (!section_entry ||
	section_entry->getType() != SectionEntry::Type_Section)
    {
	if (section_entry)
	    removeSectionEntry (section_entry);

	if (create) {
	    Section * const section = new Section (section_name);
	    addSection (section);
	    return section;
	}

	return NULL;
    }

    return static_cast <Section*> (section_entry);
}

void
Section::addAttribute (Attribute * const attr)
{
    attribute_hash.add (attr);
}

void
Section::addOption (Option * const option)
{
    section_entry_hash.add (option);
}

void
Section::addSection (Section * const section)
{
    section_entry_hash.add (section);
}

void
Section::removeSectionEntry (SectionEntry * const section_entry)
{
    section_entry_hash.remove (section_entry);
    delete section_entry;
}

Section::~Section ()
{
    SectionEntryHash::iter iter (section_entry_hash);
    while (!section_entry_hash.iter_done (iter)) {
	SectionEntry * const section_entry = section_entry_hash.iter_next (iter);
	delete section_entry;
    }
}

Option*
Config::setOption (ConstMemory const path,
		   ConstMemory const value)
{
    Option * const option = getOption (path, true /* create */);
    option->removeValues ();
    option->addValue (value);
    return option;
}

ConstMemory
Config::getString (ConstMemory   const path,
		   bool        * const ret_is_set)
{
    Option * const option = getOption (path, false /* create */);
    if (!option
	|| !option->getValue())
    {
	if (ret_is_set)
	    *ret_is_set = false;

	return ConstMemory ();
    }

    if (ret_is_set)
	*ret_is_set = true;

    return option->getValue()->mem();
}

ConstMemory
Config::getString_default (ConstMemory const path,
			   ConstMemory const default_value)
{
    bool is_set;
    ConstMemory const str = getString (path, &is_set);
    if (is_set)
	return str;

    return default_value;
}

GetResult
Config::getUint64 (ConstMemory   const path,
		   Uint64      * const ret_value)
{
    ConstMemory const value_mem = getString (path);
    if (value_mem.len() == 0)
	return GetResult::Default;

    Result const res = strToUint64_safe (value_mem, ret_value);
    if (!res) {
	logE_ (_func, "Bad value \"", value_mem, "\" for option \"", path, "\" (unsigned integer expected): ", exc->toString());
	return GetResult::Invalid;
    }

    return GetResult::Success;
}

BooleanValue
Config::getBoolean (ConstMemory const path)
{
    Option * const option = getOption (path, false /* create */);
    if (!option)
	return Boolean_Default;

    return option->getBoolean ();
}


// ________________________________ Dump methods _______________________________

static void
dumpTab (OutputStream * const outs,
	 unsigned       const nest_level)
{
    for (unsigned i = 0; i < nest_level; ++i)
	outs->print ("    ");
}

void
Option::dump (OutputStream * const outs,
	      unsigned       const nest_level)
{
    Option * const option = this;

    dumpTab (outs, nest_level);
    outs->print (option->getName (), " = ");

    Option::iter iter (*option);
    while (!option->iter_done (iter)) {
	Value * const value = option->iter_next (iter);
	outs->print ("\"", value->getAsString(), "\"");
	if (!option->iter_done (iter))
	    outs->print (", ");
    }

    outs->print (";");
}

void
Section::dump (OutputStream * const outs,
	       unsigned       const nest_level)
{
    Section * const section = this;

    dumpTab (outs, nest_level);
    if (section->getName().len())
        outs->print (section->getName(), " {\n");
    else
        outs->print ("{\n");

    dumpBody (outs, nest_level + 1);

    dumpTab (outs, nest_level);
    outs->print ("}\n");
}

void
Section::dumpBody (OutputStream * const outs,
		   unsigned       const nest_level)
{
    Section * const section = this;

    Section::iter iter (*section);
    bool first_entry = true;
    while (!section->iter_done (iter)) {
	SectionEntry * const section_entry = section->iter_next (iter);
	switch (section_entry->type) {
	    case SectionEntry::Type_Option: {
		Option * const option = static_cast <Option*> (section_entry);
		option->dump (outs, nest_level);
		outs->print ("\n");
	    } break;
	    case SectionEntry::Type_Section: {
		Section * const subsection = static_cast <Section*> (section_entry);

		if (!first_entry)
		    outs->print ("\n");

		subsection->dump (outs, nest_level);
	    } break;
	    default:
		unreachable ();
	}

	first_entry = false;
    }
}

void
Config::dump (OutputStream * const outs,
	      unsigned       const nest_level)
{
    root_section.dumpBody (outs, nest_level);
    outs->flush ();
}

}

