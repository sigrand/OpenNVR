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


#ifndef PARGEN__PARSER__H__
#define PARGEN__PARSER__H__


#include <pargen/token_stream.h>
#include <pargen/grammar.h>
#include <pargen/parser_element.h>
#include <pargen/lookup_data.h>
//#include <pargen/parsing_exception.h>


/**
 * .root
 * .title Pargen
 */

namespace Pargen {

using namespace M;

/*c
 * Position marker
 */
class ParserPositionMarker : public StReferenced
{
};

/*c
 * Parser control object
 */
class ParserControl : public StReferenced
{
public:
    virtual void setCreateElements (bool create_elements) = 0;

    virtual StRef<ParserPositionMarker> getPosition () = 0;

    virtual mt_throws Result setPosition (ParserPositionMarker *pmark) = 0;

    virtual void setVariant (ConstMemory variant_name) = 0;
};

// External users should not modify contents of ParserConfig objects.
class ParserConfig : public StReferenced
{
public:
    bool upwards_jumps;
};

StRef<ParserConfig> createParserConfig (bool upwards_jumps);

StRef<ParserConfig> createDefaultParserConfig ();

/*m*/
void optimizeGrammar (Grammar * mt_nonnull grammar);

/*m*/
//#warning TODO explicit error report
//#warning TODO handle return value
mt_throws Result parse (TokenStream    * mt_nonnull token_stream,
                        LookupData     *lookup_data,
                        void           *user_data,
                        Grammar        * mt_nonnull grammar,
                        ParserElement **ret_element,
                        StRef<StReferenced> *ret_element_container,
                        ConstMemory     default_variant = ConstMemory ("default"),
                        ParserConfig   *parser_config = NULL,
                        bool            debug_dump = false);

}


#endif /* PARGEN__PARSER_H__ */

