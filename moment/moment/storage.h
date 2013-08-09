/*  Moment Video Server - High performance media server
    Copyright (C) 2011-2013 Dmitry Shatrov
    e-mail: shatrov@gmail.com

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef MOMENT__STORAGE__H__
#define MOMENT__STORAGE__H__


#include <libmary/types.h>
#include <cstring>

#include <libmary/libmary.h>


namespace Moment {

using namespace M;

class Storage : public virtual CodeReferenced
{
public:
    class StorageFile : public Object
    {
    public:
        virtual Connection* getConnection () = 0;
    };

    virtual mt_throws Ref<StorageFile> openFile (ConstMemory        filename,
                                                 DeferredProcessor * mt_nonnull deferred_processor) = 0;
};

}


#endif /* MOMENT__STORAGE__H__ */

