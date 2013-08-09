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


#ifndef MOMENT__CHANNEL_SET__H__
#define MOMENT__CHANNEL_SET__H__


#include <libmary/libmary.h>
#include <moment/channel.h>


namespace Moment {

using namespace M;

class ChannelSet
{
private:
    StateMutex mutex;

    class ChannelEntry : public HashEntry<>
    {
    public:
	Ref<Channel> channel;
	Ref<String> channel_name;
    };

    typedef Hash< ChannelEntry,
		  Memory,
		  MemberExtractor< ChannelEntry,
				   Ref<String>,
				   &ChannelEntry::channel_name,
				   Memory,
				   AccessorExtractor< String,
						      Memory,
						      &String::mem > >,
		  MemoryComparator<> >
	    ChannelEntryHash;

    mt_mutex (mutex) ChannelEntryHash channel_entry_hash;

public:
    typedef ChannelEntry* ChannelKey;

    ChannelKey addChannel (Channel     *channel,
			   ConstMemory  channel_name);

    void removeChannel (ChannelKey channel_key);

    Ref<Channel> lookupChannel (ConstMemory channel_name);

    ~ChannelSet ();
};

}


#endif /* MOMENT__CHANNEL_SET__H__ */

