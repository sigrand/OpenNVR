#include <mconfig/varlist.h>


using namespace M;

namespace MConfig {

void
Varlist::addEntry (ConstMemory const name,
                   ConstMemory const value,
                   bool        const with_value,
                   bool        const enable_section,
                   bool        const disable_section)
{
    if (with_value
        || (!enable_section && !disable_section))
    {
        Var * const var = new (std::nothrow) Var;
        assert (var);
        if (!with_value) {
            var->value_buf = NULL;
            var->value_len = 0;
        } else {
            if (value.len()) {
                var->value_buf = new (std::nothrow) Byte [value.len()];
                assert (var->value_buf);
                memcpy (var->value_buf, value.mem(), value.len());
            } else {
                var->value_buf = NULL;
            }
            var->value_len = value.len();
        }

        if (name.len()) {
            var->name_buf = new (std::nothrow) Byte [name.len()];
            assert (var->name_buf);
            memcpy (var->name_buf, name.mem(), name.len());
        } else {
            var->name_buf = NULL;
        }
        var->name_len = name.len();

        var_list.append (var);
    }

    if (enable_section || disable_section) {
        Section * const section = new (std::nothrow) Section;
        assert (section);

        if (name.len()) {
            section->name_buf = new (std::nothrow) Byte [name.len()];
            assert (section->name_buf);
            memcpy (section->name_buf, name.mem(), name.len());
        } else {
            section->name_buf = NULL;
        }
        section->name_len = name.len();

        if (disable_section)
            section->enabled = false;
        else
            section->enabled = true;

        section_list.append (section);
    }
}

Varlist::~Varlist ()
{
    {
        VarList::iterator iter (var_list);
        while (!iter.done()) {
            Var * const var = iter.next ();
            delete var;
        }
    }

    {
        SectionList::iterator iter (section_list);
        while (!iter.done()) {
            Section * const section = iter.next ();
            delete section;
        }
    }
}

void parseVarlistSection (MConfig::Section * const mt_nonnull section,
                          MConfig::Varlist * const mt_nonnull varlist)
{
    logD_ (_func_);

    MConfig::Section::iterator iter (*section);
    while (!iter.done()) {
        MConfig::SectionEntry * const sect_entry = iter.next ();
        if (sect_entry->getType() == MConfig::SectionEntry::Type_Option) {
            MConfig::Option * const option = static_cast <MConfig::Option*> (sect_entry);

            ConstMemory var_name = option->getName();
            ConstMemory var_value;

            bool enable_section = false;
            {
                ConstMemory const enable_mem = "enable ";
                if (var_name.len() > enable_mem.len() &&
                    equal (var_name.region (0, enable_mem.len()), enable_mem))
                {
                    enable_section = true;
                    var_name = var_name.region (enable_mem.len());
                }
            }

            bool disable_section = false;
            if (!enable_section) {
                ConstMemory const disable_mem = "disable ";
                if (var_name.len() > disable_mem.len() &&
                    equal (var_name.region (0, disable_mem.len()), disable_mem))
                {
                    disable_section = true;
                    var_name = var_name.region (disable_mem.len());
                }
            }

            MConfig::Value * const value = option->getValue();
            bool const with_value = value;
            if (value)
                var_value = value->mem();

            logD_ (_func,
                   "var_name: ", var_name, ", ",
                   "var_value: ", var_value, ", ",
                   "with_value: ", with_value, ", ",
                   "enable_section: ", enable_section, ", ",
                   "disable_section: ", disable_section);

            varlist->addEntry (var_name,
                               var_value,
                               with_value,
                               enable_section,
                               disable_section);
        }
    }
}

}

