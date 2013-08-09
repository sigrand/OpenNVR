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


#include <scruffy/util.h>


using namespace M;

namespace Scruffy {
   
void
validateStringNumberPairs (const StringNumberPair * /* pairs */,
			   unsigned long            /* num_pairs */)
    throw (InternalException)
{
// Always valid
#if 0
    unsigned long i;
    for (i = 0; i < num_pairs; i++) {
	if (!utf8_validate_sz (pairs [i].string, NULL))
	    throw InternalException (InternalException::BadInput);
    }
#endif
}

bool
matchStringNumberPairs (const char             *str,
			unsigned long          *ret_number,
			const StringNumberPair *pairs,
			unsigned long           num_pairs)
{
    unsigned long i;
    for (i = 0; i < num_pairs; i++) {
	if (equal (ConstMemory (str, strlen (str)),
                   ConstMemory (pairs [i].string, strlen (pairs [i].string))))
        {
	    break;
        }
    }

    if (i >= num_pairs)
	return false;

    if (ret_number != NULL)
	*ret_number = pairs [i].number;

    return true;
}

void
validateStrings (const char   ** /* strs */,
		 unsigned long   /* num_strs */)
    throw (InternalException)
{
// Always valid
#if 0
    unsigned long i;
    for (i = 0; i < num_strs; i++) {
	if (!utf8_validate_sz (strs [i], NULL))
	    throw InternalException ();
    }
#endif
}

unsigned long
matchStrings (const char     *str,
	      const char    **strs,
	      unsigned long   num_strs)
{
    unsigned long i;
    for (i = 0; i < num_strs; i++) {
	if (equal (ConstMemory (str, strlen (str)),
                   ConstMemory (strs [i], strlen (strs [i]))))
	    break;
    }

    if (i >= num_strs)
	return 0;

    return 1;
}

bool
numbersAreZero (const unsigned long *number_set,
		unsigned long        nnumbers)
{
    unsigned long i;
    for (i = 0; i < nnumbers; i++) {
	if (number_set [i] != 0)
	    return false;
    }

    return true;
}

bool
numberIsMaxOf (unsigned long        number,
	       const unsigned long *number_set,
	       unsigned long        nnumbers)
{
    unsigned long i;
    for (i = 0; i < nnumbers; i++) {
	if (number < number_set [i])
	    return false;
    }

    return true;
}

unsigned long
maxNumberOf (const unsigned long *number_set,
	     unsigned long        nnumbers)
{
    assert (nnumbers != 0);

    unsigned long max = number_set [0];

    unsigned long i;
    for (i = 1; i < nnumbers; i++) {
	if (number_set [i] > max)
	    max = number_set [i];
    }

    return max;
}

}

