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


#ifndef LIBMARY__UTIL_POSIX__H__
#define LIBMARY__UTIL_POSIX__H__


#include <libmary/types.h>

#include <sys/types.h>
#include <sys/stat.h>


namespace M {

#ifndef LIBMARY_PLATFORM_WIN32
mt_throws Result posix_createNonblockingPipe (int (*fd) [2]);

mt_throws Result commonTriggerPipeWrite (int fd);
mt_throws Result commonTriggerPipeRead  (int fd);
#endif

mt_throws Result posix_statToFileStat (struct stat * mt_nonnull stat_buf,
                                       FileStat    * mt_nonnull ret_stat);

}


#endif /* LIBMARY__UTIL_POSIX__H__ */

