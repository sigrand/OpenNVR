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


#ifndef LIBMARY__UTIL_NET__H__
#define LIBMARY__UTIL_NET__H__


#include <libmary/types.h>

#ifndef LIBMARY_PLATFORM_WIN32
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include <libmary/log.h>


namespace M {

Result splitHostPort (ConstMemory  hostname,
		      ConstMemory *ret_host,
		      ConstMemory *ret_port);

Result hostToIp (ConstMemory  host,
		 Uint32      *ret_addr);

Result serviceToPort (ConstMemory  service,
		      Uint16      *ret_port);


// _______________________ Arch-independent setIpAddress _______________________

    class IpAddress
    {
    public:
	Uint32 ip_addr;
	Uint16 port;

        bool operator == (IpAddress const &addr) const
        {
            return (ip_addr == addr.ip_addr) && (port == addr.port);
        }

        bool operator != (IpAddress const &addr) const
        {
            return !(*this == addr);
        }

	void reset ()
	{
	    ip_addr = 0;
	    port = 0;
	}

	Size toString_ (Memory const &mem,
		        Format const &fmt) const;

        IpAddress ()
            : ip_addr (0),
              port (0)
        {}
    };

    class IpAddress_NoPort
    {
    public:
        Uint32 ip_addr;

        Size toString_ (Memory const &mem,
                        Format const &fmt) const;

        IpAddress_NoPort (IpAddress const addr)
            : ip_addr (addr.ip_addr)
        {}
    };

    static inline void setIpAddress (Uint32 const ip_addr,
				     Uint16 const port,
				     IpAddress * const ret_addr)
    {
	if (ret_addr) {
	    ret_addr->ip_addr = ip_addr;
	    ret_addr->port = port;
	}
    }

    static inline Result setIpAddress (ConstMemory const &host,
				       ConstMemory const &service,
				       IpAddress *ret_addr);

    static inline Result setIpAddress (ConstMemory const &host,
				       Uint16 port,
				       IpAddress *ret_addr);


    static inline Result setIpAddress (ConstMemory const &hostname,
				       IpAddress * const ret_addr)
    {
	ConstMemory host;
	ConstMemory port;
	if (!splitHostPort (hostname, &host, &port)) {
	    logE_ (_func, "no colon found in hostname: ", hostname);
	    return Result::Failure;
	}

	return setIpAddress (host, port, ret_addr);
    }

    // @allow_any - If 'true', then set host to INADDR_ANY when @hostname is empty.
    Result setIpAddress_default (ConstMemory const &hostname,
				 ConstMemory const &default_host,
				 Uint16      const &default_port,
				 bool               allow_any_host,
				 IpAddress         *ret_addr);

    static inline Result setIpAddress (ConstMemory const &host,
				       ConstMemory const &service,
				       IpAddress * const ret_addr)
    {
	Uint16 port;
	if (!serviceToPort (service, &port))
	    return Result::Failure;

	return setIpAddress (host, port, ret_addr);
    }

    static inline Result setIpAddress (ConstMemory const &host,
				       Uint16 const port,
				       IpAddress * const ret_addr)
    {
	Uint32 ip_addr;
	if (!hostToIp (host, &ip_addr))
	    return Result::Failure;

	setIpAddress (ip_addr, port, ret_addr);
	return Result::Success;
    }


// _________________________ Arch-specific setIpAddress ________________________

    void setIpAddress (Uint32 ip_addr,
		       Uint16 port,
		       struct sockaddr_in *ret_addr);

    void setIpAddress (struct sockaddr_in * mt_nonnull addr,
		       IpAddress          * mt_nonnull ret_addr);

    static inline void setIpAddress (IpAddress const &addr,
				     struct sockaddr_in * const ret_addr)
    {
	return setIpAddress (addr.ip_addr, addr.port, ret_addr);
    }

    static inline Result setIpAddress (ConstMemory const &host,
				       ConstMemory const &service,
				       struct sockaddr_in *ret_addr);

    static inline Result setIpAddress (ConstMemory const &host,
				       Uint16 port,
				       struct sockaddr_in *ret_addr);

    static inline Result setIpAddress (ConstMemory const &hostname,
				       struct sockaddr_in * const ret_addr)
    {
	ConstMemory host;
	ConstMemory port;
	if (!splitHostPort (hostname, &host, &port)) {
	    logE_ (_func, "no colon found in hostname: ", hostname);
	    return Result::Failure;
	}

	return setIpAddress (host, port, ret_addr);
    }

    static inline Result setIpAddress (ConstMemory const &host,
				       ConstMemory const &service,
				       struct sockaddr_in * const ret_addr)
    {
	Uint16 port;
	if (!serviceToPort (service, &port))
	    return Result::Failure;

	return setIpAddress (host, port, ret_addr);
    }

    static inline Result setIpAddress (ConstMemory const &host,
				       Uint16 const port,
				       struct sockaddr_in * const ret_addr)
    {
	Uint32 ip_addr;
	if (!hostToIp (host, &ip_addr))
	    return Result::Failure;

	setIpAddress (ip_addr, port, ret_addr);
	return Result::Success;
    }

// _____________________________________________________________________________


}


#endif /* LIBMARY__UTIL_NET__H__ */

