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


#ifndef PARGEN__FILE_POSITION__H__
#define PARGEN__FILE_POSITION__H__


#include <libmary/libmary.h>


namespace Pargen {

using namespace M;

class FilePosition
{
public:
	   // Current line number
    Uint64 line,
	   // Absolute position of the beginning of the current line
	   line_pos,
	   // Absoulute position of the current character
	   char_pos;

    FilePosition (Uint64 line,
		  Uint64 line_pos,
		  Uint64 char_pos)
	: line (line),
	  line_pos (line_pos),
	  char_pos (char_pos)
    {
    }

    FilePosition ()
	: line (0),
	  line_pos (0),
	  char_pos (0)
    {
    }
};

}


#endif /* PARGEN__FILE_POSITION__H__ */

