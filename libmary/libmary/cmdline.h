/*  LibMary - C++ library for high-performance network servers
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


#ifndef __LIBMARY__CMDLINE__H__
#define __LIBMARY__CMDLINE__H__


#include <libmary/types.h>
#include <libmary/iterator.h>


namespace M {

typedef bool (*ParseCmdlineCallback) (const char *short_name,
				      const char *long_name,
				      const char *value,
				      void       *opt_data,
				      void       *callback_data);

class CmdlineOption
{
public:
    const char *short_name,
	       *long_name;

    bool  with_value;
    void *opt_data;

    ParseCmdlineCallback opt_callback;

    CmdlineOption ()
	: short_name   (NULL),
	  long_name    (NULL),
	  with_value   (false),
	  opt_data     (NULL),
	  opt_callback (NULL)
    {
    }
};

void parseCmdline (int                        *argc,
		   char                     ***argv,
		   Iterator<CmdlineOption&>   &opt_iter,
		   ParseCmdlineCallback        callback,
		   void                       *callbackData);

}


#endif /* __LIBMARY__CMDLINE__H__ */

