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


#ifndef MCONFIG__VARLIST__H__
#define MCONFIG__VARLIST__H__


#include <libmary/libmary.h>

#include <mconfig/config.h>


namespace MConfig {

using namespace M;

class Varlist : public Object
{
public:
    class Var : public IntrusiveListElement<>
    {
        friend class Varlist;

    private:
        Byte *name_buf;
        Size  name_len;

        Byte *value_buf;
        Size  value_len;

    public:
        ConstMemory getName () const
        {
            return ConstMemory (name_buf, name_len);
        }

        ConstMemory getValue () const
        {
            return ConstMemory (value_buf, value_len);
        }

        ~Var ()
        {
            delete[] name_buf;
            delete[] value_buf;
        }
    };

    typedef IntrusiveList<Var> VarList;

    class Section : public IntrusiveListElement<>
    {
        friend class Varlist;

    private:
        Byte *name_buf;
        Size  name_len;

        bool enabled;

    public:
        ConstMemory getName () const
        {
            return ConstMemory (name_buf, name_len);
        }

        bool getEnabled () const
        {
            return enabled;
        }

        ~Section ()
        {
            delete[] name_buf;
        }
    };

    typedef IntrusiveList<Section> SectionList;

    VarList var_list;
    SectionList section_list;

    void addEntry (ConstMemory name,
                   ConstMemory value,
                   bool        with_value,
                   bool        enable_section,
                   bool        disable_section);

    ~Varlist ();
};

void parseVarlistSection (MConfig::Section * mt_nonnull section,
                          MConfig::Varlist * mt_nonnull varlist);

}


#endif /* MCONFIG__VARLIST__H__ */

