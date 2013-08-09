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


#ifndef LIBMOMENT__MOMENT_TYPES__H__
#define LIBMOMENT__MOMENT_TYPES__H__


namespace Moment {

class RecordingMode
{
public:
    enum Value {
	NoRecording,
	Replace,
	Append
    };
    operator Value () const { return value; }
    RecordingMode (Value const value) : value (value) {}
    RecordingMode () {}
private:
    Value value;
};

}


#endif /* LIBMOMENT__MOMENT_TYPES__H__ */

