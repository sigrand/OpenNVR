/*  Moment Video Server - High performance media server
    Copyright (C) 2013 Dmitry Shatrov
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


#include <moment/util_moment.h>


using namespace M;

namespace Moment {

Result
parseMomentRtmpUri (ConstMemory   const uri,
                    IpAddress   * const mt_nonnull ret_server_addr,
                    ConstMemory * const mt_nonnull ret_app_name,
                    ConstMemory * const mt_nonnull ret_stream_name,
                    bool        * const mt_nonnull ret_momentrtmp_proto)
{
    *ret_momentrtmp_proto = false;

  // URI forms:   rtmp://user:password@host:port/foo/bar
  //              rtmp://host:port/foo/bar
  //
  // Note that we do not extract user:password from the URI but rather
  // accept them as separate function parameters. This is inconsistent.
  // It might be convenient to parse user:password and use them
  // instead of explicit parameters when the latter are null.

    unsigned long pos = 0;

    while (pos < uri.len()) {
        if (uri.mem() [pos] == ':') {
            ++pos;
            break;
        }
        ++pos;
    }

    if (pos >= 1
        && equal ("momentrtmp", uri.region (0, pos - 1)))
    {
        *ret_momentrtmp_proto = true;
    }

    while (pos < uri.len()) {
        if (uri.mem() [pos] == '/') {
            ++pos;
            break;
        }
        ++pos;
    }

    while (pos < uri.len()) {
        if (uri.mem() [pos] == '/') {
            ++pos;
            break;
        }
        ++pos;
    }

    // user:password@host:port
    unsigned long const user_addr_begin = pos;

    while (pos < uri.len()) {
        if (uri.mem() [pos] == '/')
            break;
        ++pos;
    }

    ConstMemory const user_addr = uri.region (user_addr_begin, pos - user_addr_begin);
    logD_ (_func, "user_addr_begin: ", user_addr_begin, ", pos: ", pos, ", user_addr: ", user_addr);
    unsigned long at_pos = 0;
    bool got_at = false;
    while (at_pos < user_addr.len()) {
        if (user_addr.mem() [at_pos] == '@') {
            got_at = true;
            break;
        }
        ++at_pos;
    }

    ConstMemory addr_mem = user_addr;
    if (got_at)
        addr_mem = user_addr.region (at_pos + 1, user_addr.len() - (at_pos + 1));

    logD_ (_func, "addr_mem: ", addr_mem);
    if (!setIpAddress (addr_mem, ret_server_addr)) {
        logE_ (_func, "Could not extract address from URI: ", uri);
        return Result::Failure;
    }

    if (pos < uri.len())
        ++pos;

    unsigned long const app_name_begin = pos;
    while (pos < uri.len()) {
        if (uri.mem() [pos] == '/')
            break;
        ++pos;
    }

    *ret_app_name = uri.region (app_name_begin, pos - app_name_begin);

    if (pos < uri.len())
        ++pos;

    *ret_stream_name = uri.region (pos, uri.len() - pos);

    return Result::Success;
}

}

