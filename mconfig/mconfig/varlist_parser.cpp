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


#include <libmary/libmary.h>

#include <pargen/memory_token_stream.h>
#include <pargen/parser.h>

#include <mconfig/varlist_pargen.h>
#include <mconfig/varlist_parser.h>


using namespace M;

namespace MConfig {

bool
varlist_word_token_match_func (ConstMemory const &token_mem,
                               void * const /* token_user_ptr */,
                               void * const /* user_data */)
{
    char const * const token = (char const*) token_mem.mem();
    if (token [0] == '=')
        return false;

    return true;
}

bool
varlist_accept_var_decl (VarList_VarDecl        * const var_decl,
		         Pargen::ParserControl  * const /* parser_control */,
		         void                   * const _varlist)
{
    Varlist * const varlist = static_cast <Varlist*> (_varlist);

    ConstMemory name;
    ConstMemory value;
    bool with_value = false;

    bool enable_section  = false;
    bool disable_section = false;

    switch (var_decl->var_decl_type) {
        case VarList_VarDecl::t_NameValue: {
            VarList_VarDecl_NameValue * const var_decl__name_value =
                    static_cast <VarList_VarDecl_NameValue*> (var_decl);

            if (VarList_SectionSpecifier * const section_specifier =
                        var_decl__name_value->sectionSpecifier)
            {
                switch (section_specifier->section_specifier_type) {
                    case VarList_SectionSpecifier::t_Enable:
                        enable_section = true;
                        break;
                    case VarList_SectionSpecifier::t_Disable:
                        disable_section = true;
                        break;
                }
            }

            name  = var_decl__name_value->name->any_token->token;
            value = var_decl__name_value->value->any_token->token;
            with_value = true;
        } break;
        case VarList_VarDecl::t_Name: {
            VarList_VarDecl_Name * const var_decl__name =
                    static_cast <VarList_VarDecl_Name*> (var_decl);

            if (VarList_SectionSpecifier * const section_specifier =
                        var_decl__name->sectionSpecifier)
            {
                switch (section_specifier->section_specifier_type) {
                    case VarList_SectionSpecifier::t_Enable:
                        enable_section = true;
                        break;
                    case VarList_SectionSpecifier::t_Disable:
                        disable_section = true;
                        break;
                }
            }

            name  = var_decl__name->name->any_token->token;
        } break;
        default:
            unreachable ();
    }

    logD_ (_func,
           "name: ", name, ", "
           "value: ", value, ", "
           "with_value: ", with_value, ", "
           "enable_section: ", enable_section, ", "
           "disable_section: ", disable_section);

    varlist->addEntry (name, value, with_value, enable_section, disable_section);

    return true;
}

Result VarlistParser::parseVarlist (ConstMemory   const filename,
                                    Varlist     * const varlist)
{
    Byte *buf = NULL;

 try {
    NativeFile file;
    if (!file.open (filename, 0 /* open_flags */, FileAccessMode::ReadOnly)) {
        logD_ (_func, "Could not open ", filename, ": ", exc->toString());
        return Result::Failure;
    }

    ConstMemory mem;
    {
        FileStat fs;
        if (!file.stat (&fs)) {
            logE_ (_func, "NativeFile::stat() failed:\n", exc->backtrace());
            return Result::Failure;
        }

        Uint64 const limit = (1 << 22 /* 4 MB */);
        if (fs.size > limit) {
            logE_ (_func, "varlist file is too large: ", fs.size, " bytes (max ", limit, " bytes");
            return Result::Failure;
        }

        if (fs.size == 0) {
          // Nothing to read. This early return simplifies EOF handling.
            return Result::Success;
        }

        buf = new (std::nothrow) Byte [fs.size];
        assert (buf);

        Size nread;
        IoResult const res = file.readFull (Memory (buf, fs.size), &nread);
        if (res == IoResult::Error) {
            delete[] buf;
            logE_ (_func, "varlist file read error:\n", exc->backtrace());
            return Result::Failure;
        }

        if (nread != fs.size) {
            logW_ (_func, "WARNING: read size (", nread, ") differs from stat size (", fs.size, ")");
        }
        assert (nread <= fs.size);

        mem = ConstMemory (buf, nread);

        logD_ (_func, "varlist file data:\n", mem, "\n");
    }

    Pargen::MemoryTokenStream token_stream;
    token_stream.init (mem,
                       true  /* report_newlines */,
                       ";"   /* newline_replacement */,
                       false /* minus_is_alpha */,
                       4096  /* max_token_len */);

    StRef<StReferenced> varlist_elem_container;
    Pargen::ParserElement *varlist_elem = NULL;
    Pargen::parse (&token_stream,
                   NULL /* lookup_data */,
                   varlist /* user_data */,
                   grammar,
                   &varlist_elem,
                   &varlist_elem_container,
                   "default",
                   parser_config,
                   false /* debug_dump */);

    delete[] buf;
    buf = NULL;

    ConstMemory token;
    if (!token_stream.getNextToken (&token)) {
        logE_ (_func, "Read error: ", exc->toString());
        return Result::Failure;
    }

    if (varlist_elem == NULL ||
	token.len() > 0)
    {
	logE_ (_func, "Syntax error in configuration file ", filename);
	return Result::Failure;
    }
 } catch (...) {
     delete[] buf;
     logE_ (_func, "parsing exception");
     return Result::Failure;
 }

   delete[] buf;
   return Result::Success;
}

VarlistParser::VarlistParser ()
{
 try {
    grammar = create_varlist_grammar ();
    Pargen::optimizeGrammar (grammar);
    parser_config = Pargen::createParserConfig (true /* upwards_jumps */);
 } catch (...) {
    logE_ (_func, "exception");
 }
}

}

