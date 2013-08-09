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


#ifndef LIBMARY__COMPARATOR__H__
#define LIBMARY__COMPARATOR__H__


#include <libmary/types.h>
#include <libmary/extractor.h>


namespace M {

/* A comparator should provide the following static methods:
 *
       static bool greater (L const &left, R const &right);
       static bool equals  (L const &left, R const &right);
 */

template < class T,
	   class LeftExtractor = DirectExtractor<T const &>,
	   class RightExtractor = LeftExtractor >
class DirectComparator
{
public:
    static bool greater (T const &left, T const &right)
        { return LeftExtractor::getValue (left) > RightExtractor::getValue (right); }

    static bool equals (T const &left, T const &right)
        { return LeftExtractor::getValue (left) == RightExtractor::getValue (right); }
};

template < class T = ConstMemory const & >
class MemoryComparator
{
public:
    static bool greater (T left, T right)
        { return compare (left, right) == ComparisonResult::Greater; }

    static bool equals (T left, T right)
        { return compare (left, right) == ComparisonResult::Equal; }
};

}


#endif /* LIBMARY__COMPARATOR__H__ */

