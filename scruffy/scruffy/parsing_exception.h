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


#ifndef SCRUFFY__PARSING_EXCEPTION__H__
#define SCRUFFY__PARSING_EXCEPTION__H__


#include <libmary/libmary.h>

#include <pargen/file_position.h>


namespace Scruffy {

using namespace M;

class ParsingException : public Exception
{
public:
    Pargen::FilePosition const fpos;
    StRef<String> message;

    Ref<String> toString ()
    {
        if (cause)
            return makeString ("ParsingException: ", message->mem(), ": ", cause->toString()->mem());
        else
            return makeString ("ParsingException: ", message->mem());
    }

    ParsingException (Pargen::FilePosition const &fpos,
		      String * const message)
        : fpos (fpos),
          message (message)
    {
        if (!this->message)
            this->message = st_grab (new (std::nothrow) String);
    }
};

}


#endif /* SCRUFFY__PARSING_EXCEPTION__H__ */

