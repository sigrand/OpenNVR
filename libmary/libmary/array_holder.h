/*  LibMary - C++ library for high-performance network servers
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


#ifndef __LIBMARY__ARRAY_HOLDER__H__
#define __LIBMARY__ARRAY_HOLDER__H__


#include <libmary/types_base.h>
#include <libmary/util_base.h>


namespace M {

template <class T>
class ArrayHolder
{
private:
    T *array;

    ArrayHolder& operator = (const ArrayHolder &);
    ArrayHolder (const ArrayHolder &);

public:
/* This is not necessary because of the presense of 'operator T*'.
    T& operator [] (const size_t index) const
    {
	return array [index];
    }
*/
    bool isNull () const
    {
		return array == NULL;
    }

    operator T* ()
    {
	return array;
    }

    void releasePointer ()
    {
	array = NULL;
    }

    void setPointer (T *new_array)
    {
	if (array != NULL)
	    delete[] array;

	array = new_array;
    }

    void allocate (unsigned long size)
    {
	if (array != NULL)
	    delete[] array;

	if (size > 0) {
	    array = new (std::nothrow) T [size];
	    assert (array);
	} else
	    array = NULL;
    }

    void deallocate ()
    {
	if (array != NULL)
	    delete[] array;

	array = NULL;
    }

    ArrayHolder ()
    {
	array = NULL;
    }

    ArrayHolder (T *array)
    {
	this->array = array;
    }

    ArrayHolder (unsigned long size)
    {
	if (size > 0) {
	    array = new (std::nothrow) T [size];
	    assert (array);
	} else {
	    array = NULL;
	}
    }

    ~ArrayHolder ()
    {
	if (array != NULL)
	    delete[] array;
    }
};

}


#endif /* __LIBMARY__ARRAY_HOLDER__H__ */

