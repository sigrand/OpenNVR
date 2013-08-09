/*  Moment Video Server - High performance media server
    Copyright (C) 2011 Dmitry Shatrov
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


#include <moment/local_storage.h>


namespace Moment {

Ref<Storage::StorageFile>
LocalStorage::openFile (ConstMemory         const filename,
                        DeferredProcessor * const mt_nonnull deferred_processor)
{
    Ref<LocalStorageFile> const ls_file = grab (new (std::nothrow) LocalStorageFile);

    if (!ls_file->file.open (filename,
                             File::OpenFlags::Create | File::OpenFlags::Truncate,
                             File::AccessMode::WriteOnly))
    {
	logE_ (_func, "file.open() failed: ", exc->toString());
	return NULL;
    }

    ls_file->conn.init (deferred_processor, &ls_file->file);

    return ls_file;
}

}

