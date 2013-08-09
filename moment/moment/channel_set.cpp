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


#include <moment/channel_set.h>


using namespace M;

namespace Moment {

ChannelSet::ChannelKey
ChannelSet::addChannel (Channel     * const channel,
			ConstMemory   const channel_name)
{
    ChannelEntry * const channel_entry = new (std::nothrow) ChannelEntry;
    assert (channel_entry);
    channel_entry->channel = channel;
    channel_entry->channel_name = grab (new (std::nothrow) String (channel_name));

    mutex.lock ();
    channel_entry_hash.add (channel_entry);
    mutex.unlock ();

    return channel_entry;
}

void
ChannelSet::removeChannel (ChannelKey const channel_key)
{
    mutex.lock ();
    channel_entry_hash.remove (channel_key);
    mutex.unlock ();
}

Ref<Channel>
ChannelSet::lookupChannel (ConstMemory const channel_name)
{
    mutex.lock ();

    ChannelEntry * const channel_entry = channel_entry_hash.lookup (channel_name);
    if (!channel_entry) {
	mutex.unlock ();
	return NULL;
    }

    Ref<Channel> const channel = channel_entry->channel;

    mutex.unlock ();

    return channel;
}

ChannelSet::~ChannelSet ()
{
    mutex.lock ();

    ChannelEntryHash::iter iter (channel_entry_hash);
    while (!channel_entry_hash.iter_done (iter)) {
	ChannelEntry * const channel_entry = channel_entry_hash.iter_next (iter);
	delete channel_entry;
    }

    mutex.unlock ();
}

}

