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


#include <libmary/types.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <libmary/util_dev.h>
#include <libmary/log.h>

#include <libmary/mwritev.h>


namespace M {

namespace {
mt_const int libMary_mwritevFd = -1;
}

bool libMary_mwritevAvailable ()
{
    return libMary_mwritevFd != -1;
}

namespace {
typedef struct {
    int  num_fds;
    int *fds;
    struct iovec **iovs;
    int *num_iovs;
    int *ret_res;
} __attribute__ ((packed)) LibMary_MwritevArgs;
}

mt_throws Result libMary_mwritev (int   const num_fds,
				  int * const fds,
				  struct iovec ** const iovs,
				  int * const num_iovs,
				  int * const ret_res)
{
#if 0
    logD_ (_func, "num_fds: ", num_fds);
    if (defaultLogLevelOn (LogLevel::Debug)) {
	Size total_len = 0;
	for (int i = 0; i < num_fds; ++i) {
	    for (int j = 0; j < num_iovs [i]; ++j) {
		logD_ ("    #", j, ": "
		       "0x", fmt_hex, (UintPtr) iovs [i][j].iov_base, ": "
		       "", fmt_def, iovs [i][j].iov_len);
                logLock ();
		hexdump (logs, ConstMemory ((Byte const *) iovs [i][j].iov_base, iovs [i][j].iov_len));
                logUnlock ();
		total_len += iovs [i][j].iov_len;
	    }
	}
	logD_ (_func, "total_len: ", total_len);
    }
#endif

    LibMary_MwritevArgs args;
    args.num_fds = num_fds;
    args.fds = fds;
    args.iovs = iovs;
    args.num_iovs = num_iovs;
    args.ret_res = ret_res;

//    int const res = ioctl (libMary_mwritevFd, 0, (void*) &args);
    int const res = write (libMary_mwritevFd, &args, sizeof (args));
    if (res == -1) {
	exc_throw <PosixException> (errno);
	logE_ (_func, "write() failed: ", errnoString (errno));
	return Result::Failure;
    }

#if 0
    logD_ (_func, "ret_res: ");
    if (defaultLogLevelOn (LogLevel::Debug)) {
	for (int i = 0; i < num_fds; ++i)
	    logD_ ("    #", i, ": ", ret_res [i]);
    }
#endif

    return Result::Success;
}

namespace {
typedef struct {
    int  fd;
    struct iovec *iovs;
    int  num_iovs;
    int *ret_res;
} __attribute__ ((packed)) LibMary_MwritevSingleArgs;
}

mt_throws Result libMary_mwritevSingle (int   const fd,
					struct iovec * const iovs,
					int   const num_iovs,
					int * const ret_res)
{
    LibMary_MwritevSingleArgs args;
    args.fd = fd;
    args.iovs = iovs;
    args.num_iovs = num_iovs;
    args.ret_res = ret_res;

    int const res = write (libMary_mwritevFd, &args, sizeof (args));
    *ret_res = res;
    if (res < 0) {
	exc_throw <PosixException> (errno);
	logE_ (_func, "write() failed: ", errnoString (errno));
	return Result::Failure;
    }

    return Result::Success;
}

mt_const void libMary_mwritevInit ()
{
    for (;;) {
	libMary_mwritevFd = open ("/dev/mwritev", O_WRONLY);
	if (libMary_mwritevFd == -1) {
	    if (errno == EINTR)
		continue;

	    logE_ (_func, "open() failed: ", errnoString (errno));
	    return;
	}

	break;
    }
}

}

