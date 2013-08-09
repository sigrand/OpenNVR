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


#include <libmary/types.h>

#include <cctype>

#include <libmary/log.h>
#include <libmary/util_str.h>
#include <libmary/util_dev.h> // Debugging

#include <libmary/http_server.h>


namespace M {

static LogGroup libMary_logGroup_http ("http", LogLevel::I);

Receiver::Frontend const HttpServer::receiver_frontend = {
    processInput,
    processEof,
    processError
};

Sender::Frontend const HttpServer::sender_frontend = {
    sendStateChanged,
    senderClosed
};


char from_hex(char ch) {
  return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
}

void
url_decode (std::string & str, std::string & result) {
      const char *pstr = str.c_str();
      char *buf = (char *) malloc (str.length() + 1);
      char *pbuf = buf;
      while (*pstr) {
        if (*pstr == '%') {
          if (pstr[1] && pstr[2]) {
            *pbuf++ = from_hex (pstr[1]) << 4 | from_hex (pstr[2]);
            pstr += 2;
          }
        } else if (*pstr == '+') {
          *pbuf++ = ' ';
        } else {
          *pbuf++ = *pstr;
        }
        pstr++;
      }
      *pbuf = '\0';

     result.assign(buf);
      if(buf)
        delete[] buf;
}

void parseHttpParameters (ConstMemory                     const mem,
                          ParseHttpParameters_Callback    const param_cb,
                          void                          * const param_cb_data)
{
    Byte const *uri_end = mem.mem() + mem.len();
    Byte const *param_pos = mem.mem();

    while (param_pos < uri_end) {
        ConstMemory name;
        ConstMemory value;

        Byte const * const name_end = (Byte const *) memchr (param_pos, '&', uri_end - param_pos);
        Byte const *value_start     = (Byte const *) memchr (param_pos, '=', uri_end - param_pos);
        if (value_start && (!name_end || value_start < name_end)) {
            ++value_start; // Skipping '='
            name = ConstMemory (param_pos, value_start - 1 /*'='*/ - param_pos);

            Byte const *value_end = (Byte const *) memchr (value_start, '&', uri_end - value_start);
            if (value_end) {
                value = ConstMemory (value_start, value_end - value_start);
                param_pos = value_end + 1; // Skipping '&'
            } else {
                value = ConstMemory (value_start, uri_end - value_start);
                param_pos = uri_end;
            }
        } else {
            if (name_end) {
                name = ConstMemory (param_pos, name_end - param_pos);
                param_pos = name_end + 1; // Skipping '&'
            } else {
                name = ConstMemory (param_pos, uri_end - param_pos);
                param_pos = uri_end;
            }
        }

        param_cb (name, value, param_cb_data);
    }
}


void
HttpRequest::parseParameters_paramCallback (ConstMemory   const name,
                                            ConstMemory   const value,
                                            void        * const _self)
{
    HttpRequest * const self = static_cast <HttpRequest*> (_self);

    HttpRequest::Parameter * const param = new (std::nothrow) HttpRequest::Parameter;
    assert (param);
    param->name = name;
    param->value = value;

    self->parameter_hash.add (param);

    //** unescape parsed parameter
    std::string key ((const char*) name.mem(), name.len());
    if(value.len() == 0)
    {
        self->decoded_args[key] = std::string("true");
        return;
    }
    std::string undecoded_value((const char*)value.mem());

    size_t req_end = undecoded_value.find(" HTTP");
    size_t param_end = undecoded_value.find_first_of('&');

    size_t end;

    if (param_end > 0 && req_end == -1)
        end = param_end;
    else if (param_end == -1 && req_end > 0)
        end = req_end;
    else if (param_end > 0 && req_end > 0)
        end = std::min (param_end, req_end);
    else 
        end = undecoded_value.length();

    std::string decoded_value;
    undecoded_value = undecoded_value.substr(0, end);

    url_decode(undecoded_value, decoded_value);

    self->decoded_args[key] = decoded_value;
}

void
HttpRequest::parseParameters (Memory const mem)
{
    parseHttpParameters (mem, parseParameters_paramCallback, this);
}

static bool isAlpha (unsigned char c)
{
    if ((c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z'))
    {
        return true;
    }

    return false;
}

static bool isAlphaOrDash (unsigned char c)
{
    if (isAlpha (c) || c == '-')
        return true;

    return false;
}

// Linear white space
//
// Note that "Implied *LWS" rule in HTTP spec is just crazy.
// No sane person could come up with such a thing.
//
static void skipLWS (ConstMemory const mem,
                     Size * const mt_nonnull ret_pos)
{
    Size pos = *ret_pos;
    for (;;) {
        if (pos >= mem.len())
            return;

        if (mem.mem() [pos] == 13 /* CR */) {
            ++pos;
            if (pos == mem.len())
                return;

            if (mem.mem() [pos] == 10 /* LF */) {
                ++pos;
                if (pos == mem.len())
                    return;
            }
        }

        if (mem.mem() [pos] == ' ' ||
            mem.mem() [pos] == '\t')
        {
            ++pos;
            *ret_pos = pos;
            continue;
        }

        break;
    }
}

void
HttpRequest::parseAcceptLanguage (ConstMemory              const mem,
                                  List<AcceptedLanguage> * const mt_nonnull res_list)
{
    Size pos = 0;
    for (;;) {
        skipLWS (mem, &pos);
        if (pos >= mem.len())
            return;

        unsigned long const lang_begin = pos;
        for (; pos < mem.len(); ++pos) {
            if (!isAlphaOrDash (mem.mem() [pos]))
                break;
        }

        ConstMemory const lang = mem.region (lang_begin, pos - lang_begin);
        double weight = 1.0;

        // TODO Doesn't whitespace + ';' turn the rest of the string into a comment?
        skipLWS (mem, &pos);
        do {
            if (pos >= mem.len())
                break;

            if (mem.mem() [pos] == ';') {
                ++pos;
                skipLWS (mem, &pos);
                if (pos >= mem.len())
                    break;

                if (mem.mem() [pos] == 'q') {
                    ++pos;
                    skipLWS (mem, &pos);
                    if (pos >= mem.len())
                        break;

                    if (mem.mem() [pos] == '=') {
                        ++pos;
                        skipLWS (mem, &pos);
                        if (pos >= mem.len())
                            break;

                        if (mem.mem() [pos] == '0') {
                            weight = 0.0;

                            do {
                                ++pos;
                                skipLWS (mem, &pos);
                                if (pos >= mem.len())
                                    break;

                                if (mem.mem() [pos] != '.')
                                    break;

                                ++pos;
                                skipLWS (mem, &pos);
                                if (pos >= mem.len())
                                    break;

                                double const mul_arr [3] = { 0.1, 0.01, 0.001 };
                                unsigned mul_idx = 0;
                                for (; mul_idx < 3; ++mul_idx) {
                                    if (mem.mem() [pos] >= '0' && mem.mem() [pos] <= '9') {
                                        weight += mul_arr [mul_idx] * (mem.mem() [pos] - '0');

                                        ++pos;
                                        skipLWS (mem, &pos);
                                        if (pos >= mem.len())
                                            break;
                                    } else {
                                      // garbage
                                        break;
                                    }
                                }
                            } while (0);
                        } else {
                          // The 'qvalue' is either ("1" ["." 0*3 ("0")])
                          // or some garbage.
                            weight = 1.0;
                        }
                    }
                }
            }

            // Garbage skipping.
            for (; pos < mem.len(); ++pos) {
                if (mem.mem() [pos] == ',')
                    break;
            }
        } while (0);

        if (pos >= mem.len() || mem.mem() [pos] == ',') {
            if (lang.len() > 0) {
                res_list->appendEmpty ();
                AcceptedLanguage * const alang = &res_list->getLast();
                alang->lang = grab (new (std::nothrow) String (lang));
                alang->weight = weight;
            }

            if (pos < mem.len())
                ++pos;
        } else {
          // Unreachable because of garbage skipping.
            logW_ (_func, "parse error, Accept-Language: ", mem);
            break;
        }
    }
}

static Ref<String> parseQuotedString (ConstMemory   const mem,
                                      Size        * const mt_nonnull ret_pos)
{
    Size pos = *ret_pos;
    Ref<String> unescaped_str;

    skipLWS (mem, &pos);
    if (pos >= mem.len())
        goto _return;

    if (mem.mem() [pos] != '"')
        goto _return;

    ++pos;
    if (pos >= mem.len())
        goto _return;

    {
        Size const tag_begin = pos;
        for (unsigned i = 0; i < 2; ++i) {
            Size unescaped_len = 0;
            bool escaped = false;
            for (; pos < mem.len(); ++pos) {
                if (!escaped) {
                    if (mem.mem() [pos] == '"')
                        break;

                    if (mem.mem() [pos] == '\\') {
                        escaped = true;
                    } else {
                        if (i == 1)
                            unescaped_str->mem().mem() [unescaped_len] = mem.mem() [pos];

                        ++unescaped_len;
                    }
                } else {
                    escaped = false;
                    ++unescaped_len;
                }
            }

            if (i == 0) {
                unescaped_str = grab (new (std::nothrow) String (unescaped_len));
                pos = tag_begin;
            }
        }
    }

_return:
    *ret_pos = pos;
    return unescaped_str;
}

static Ref<String> parseEntityTag (ConstMemory   const mem,
                                   bool        * const ret_weak,
                                   Size        * const mt_nonnull ret_pos)
{
    Size pos = *ret_pos;
    Ref<String> etag_str;

    if (*ret_weak)
        *ret_weak = false;

    skipLWS (mem, &pos);
    if (pos >= mem.len())
        goto _return;

    if (mem.mem() [pos] == 'W') {
        if (*ret_weak)
            *ret_weak = true;

        for (; pos < mem.len(); ++pos) {
            if (mem.mem() [pos] == '"')
                break;
        }

        if (pos >= mem.len())
            goto _return;
    }

    etag_str = parseQuotedString (mem, &pos);

_return:
    *ret_pos = pos;
    return etag_str;
}

void HttpRequest::parseEntityTagList (ConstMemory       const mem,
                                      bool            * const ret_any,
                                      List<EntityTag> * const mt_nonnull ret_etags)
{
    if (ret_any)
        *ret_any = false;

    Size pos = 0;

    skipLWS (mem, &pos);

    if (pos >= mem.len())
        return;

    if (mem.mem() [pos] == '*') {
        if (ret_any)
            *ret_any = true;

        return;
    }

    for (;;) {
        bool weak;
        Ref<String> etag_str = parseEntityTag (mem, &weak, &pos);
        if (!etag_str)
            break;

        ret_etags->appendEmpty ();
        EntityTag * const etag = &ret_etags->getLast();
        etag->etag = etag_str;
        etag->weak = weak;

        skipLWS (mem, &pos);
    }
}

HttpRequest::~HttpRequest ()
{
    delete[] path;

    {
	ParameterHash::iter iter (parameter_hash);
	while (!parameter_hash.iter_done (iter)) {
	    Parameter * const param = parameter_hash.iter_next (iter);
	    delete param;
	}
    }
}

Result
HttpServer::processRequestLine (Memory const _mem)
{
    logD (http, _func, "mem: ", _mem);

    cur_req->request_line = grab (new (std::nothrow) String (_mem));
    ConstMemory const mem = cur_req->request_line->mem();

    Byte const *path_beg = (Byte const *) memchr (mem.mem(), 32 /* SP */, mem.len());
    if (!path_beg) {
	logE_ (_func, "Bad request (1) \"", mem, "\"");
        logLock ();
	hexdump (logs, mem);
        logUnlock ();
	return Result::Failure;
    }

    cur_req->method = ConstMemory (mem.mem(), path_beg - mem.mem());
    ++path_beg;

    Size path_offs = path_beg - mem.mem();
    if (mem.len() - path_offs >= 1 &&
	mem.mem() [path_offs] == '/')
    {
	++path_offs;
	++path_beg;
    }

    Byte const *uri_end = (Byte const *) memchr (path_beg, 32 /* SP */, mem.len() - path_offs);
    if (!uri_end) {
	logE_ (_func, "Bad request (2) \"", mem, "\"");
        logLock ();
	hexdump (logs, mem);
        logUnlock ();
	return Result::Failure;
    }

    Byte * const params_start = (Byte*) memchr (path_beg, '?', uri_end - path_beg);
    Byte const *path_end;
    if (params_start)
	path_end = params_start;
    else
	path_end = uri_end;

    cur_req->full_path = ConstMemory (path_beg, path_end - mem.mem() - path_offs);

    logD (http, _func, "path_offs: ", path_offs);

    {
      // Counting path elements.

	Size path_pos = path_offs;
	while (path_pos < (Size) (path_end - mem.mem())) {
	    Byte const *next_path_elem = (Byte const *) memchr (mem.mem() + path_pos, '/', mem.len() - path_pos);
	    if (next_path_elem > path_end)
		next_path_elem = NULL;

	    Size path_elem_end;
	    if (next_path_elem)
		path_elem_end = next_path_elem - mem.mem();
	    else
		path_elem_end = path_end - mem.mem();

	    logD (http, _func, "path_pos: ", path_pos, ", path_elem_end: ", path_elem_end);

	    ++cur_req->num_path_elems;

	    path_pos = path_elem_end + 1;
	}
    }

    logD (http, _func, "num_path_elems: ", cur_req->num_path_elems);
    if (cur_req->num_path_elems > 0) {
      // Filling path elements.

	cur_req->path = new (std::nothrow) ConstMemory [cur_req->num_path_elems];
        assert (cur_req->path);

	Size path_pos = path_offs;
	Count index = 0;
	while (path_pos < (Size) (path_end - mem.mem())) {
	    Byte const *next_path_elem = (Byte const *) memchr (mem.mem() + path_pos, '/', mem.len() - path_pos);
	    if (next_path_elem > path_end)
		next_path_elem = NULL;

	    Size path_elem_end;
	    if (next_path_elem)
		path_elem_end = next_path_elem - mem.mem();
	    else
		path_elem_end = path_end - mem.mem();

	    logD (http, _func, "path_pos: ", path_pos, ", path_elem_end: ", path_elem_end);

	    cur_req->path [index] = ConstMemory (mem.mem() + path_pos, path_elem_end - path_pos);
	    ++index;

	    path_pos = path_elem_end + 1;
	}
    }

    if (params_start) {
      // Parsing request parameters.
	Byte * const param_pos = params_start + 1; // Skipping '?'
	if (param_pos < uri_end)
	    cur_req->parseParameters (Memory (param_pos, uri_end - param_pos));
    }

    // TODO Distinguish between HTTP/1.0 and HTTP/1.1.

    if (logLevelOn (http, LogLevel::Debug)) {
	logD (http, _func, "request line: ", cur_req->getRequestLine ());
	logD (http, _func, "method: ", cur_req->getMethod ());
	logD (http, _func, "full path: ", cur_req->getFullPath ());
	logD (http, _func, "path elements (", cur_req->getNumPathElems(), "):");
	for (Count i = 0, i_end = cur_req->getNumPathElems(); i < i_end; ++i)
	    logD (http, _func, cur_req->getPath (i));
    }

    return Result::Success;
}

void
HttpServer::processHeaderField (ConstMemory const &mem)
{
    logD (http, _func, mem);

    Byte const * const header_name_end = (Byte const *) memchr (mem.mem(), ':', mem.len());
    if (!header_name_end) {
	logE_ (_func, "bad header line: ", mem);
        logLock ();
	hexdump (logs, mem);
        logUnlock ();
	return;
    }

    // TODO allow modifying data by receiver backend (Memory, not ConstMemory).

    Size const header_name_len = header_name_end - mem.mem();
    for (Size i = 0; i < header_name_len; ++i)
	// TODO Get rid of this const_cast
	((Byte*) mem.mem()) [i] = tolower (mem.mem() [i]);

    // TODO SP, HT - correct?
    Byte const *header_value_buf = header_name_end + 1;
    Size header_value_len = mem.len() - header_name_len - 1;
    while (header_value_len > 0
	   && (header_value_buf [0] == 32 /* SP */ ||
	       header_value_buf [0] ==  9 /* HT */))
    {
	++header_value_buf;
	--header_value_len;
    }

    ConstMemory const header_name (mem.mem(), header_name_len);
    ConstMemory const header_value (header_value_buf, header_value_len);

    if (!compare (header_name, "content-length")) {
	recv_content_length = strToUlong (header_value);
        recv_content_length_specified = true;

	cur_req->content_length = recv_content_length;
        cur_req->content_length_specified = true;

	logD (http, _func, "recv_content_length: ", recv_content_length);
    } else
    if (!compare (header_name, "expect")) {
	if (!compare (header_value, "100-continue")) {
            if (sender && page_pool) {
                logD (http, _func, "responding to 100-continue");
                sender->send (
                        page_pool,
                        true /* do_flush */,
                        "HTTP/1.1 100 Continue\r\n"
                        "Cache-Control: no-cache\r\n"
                        "Content-Type: application/x-fcs\r\n"
                        "Content-Length: 0\r\n"
                        "Connection: Keep-Alive\r\n"
                        "\r\n");
            } else {
                if (!sender)
                    logW_ (_this_func, "Sender is not set");
                if (!page_pool)
                    logW_ (_this_func, "PagePool is not set");
            }
	}
    } else
    if (!compare (header_name, "accept-language")) {
        cur_req->accept_language = grab (new (std::nothrow) String (header_value));
    } else
    if (!compare (header_name, "if-modified-since")) {
        cur_req->if_modified_since = grab (new (std::nothrow) String (header_value));
    } else
    if (!compare (header_name, "if-none-match")) {
        cur_req->if_none_match = grab (new (std::nothrow) String (header_value));
    }
}

Receiver::ProcessInputResult
HttpServer::receiveRequestLine (Memory const _mem,
				Size * const mt_nonnull ret_accepted,
				bool * const mt_nonnull ret_header_parsed)
{
    logD (http, _func, _mem.len(), " bytes");
    if (logLevelOn (http, LogLevel::Debug)) {
        logLock ();
	hexdump (logs, _mem);
        logUnlock ();
    }

    *ret_accepted = 0;
    *ret_header_parsed = false;

    Memory mem = _mem;

    Size field_offs = 0;
    for (;;) {
	logD (http, _func, "iteration, mem.len(): ", mem.len(), ", recv_pos: ", recv_pos);

	assert (mem.len() >= recv_pos);
	if (mem.len() == recv_pos) {
	  // No new data since the last input event => nothing changed.
	    return Receiver::ProcessInputResult::Again;
	}

	Size cr_pos;
	for (;;) {
	    Byte const * const cr_ptr = (Byte const *) memchr (mem.mem() + recv_pos, 13 /* CR */, mem.len() - recv_pos);
	    if (!cr_ptr) {
		recv_pos = mem.len();
		logD (http, _func, "returning Again #1, recv_pos: ", recv_pos);
		return Receiver::ProcessInputResult::Again;
	    }

	    cr_pos = cr_ptr - mem.mem();
	    // We need LF and one non-SP symbol to determine header field end.
	    // Also we want to look 2 symbols ahead to detect end of message headers.
	    // This means that we need 3 symbols of lookahead.
	    if (mem.len() - (cr_pos + 1) < 3) {
		// Leaving CR unaccepted for the next input event.
		recv_pos = cr_pos;
		logD (http, _func, "returning Again #2, recv_pos: ", recv_pos);
		return Receiver::ProcessInputResult::Again;
	    }

	    if (mem.mem() [cr_pos + 1] == 10 /* LF */ &&
		    // Request line cannot be split into multiple lines.
		    (req_state == RequestState::RequestLine ||
			    (mem.mem() [cr_pos + 2] != 32 /* SP */ &&
			     mem.mem() [cr_pos + 2] !=  9 /* HT */)))
	    {
	      // Got a complete header field.
		break;
	    }

	  // CR at cr_pos does not end header field.
	  // Searching for another one.
	    recv_pos = cr_pos + 1;
	    logD (http, _func, "new recv_pos: ", recv_pos);
	}

	if (req_state == RequestState::RequestLine) {
	    // TODO returns Result
	    logD (http, _func, "calling processRequestLine()");
	    processRequestLine (mem.region (0, cr_pos));
	    req_state = RequestState::HeaderField;
	} else
	    processHeaderField (mem.region (0, cr_pos));

	Size const next_pos = cr_pos + 2;
	*ret_accepted = field_offs + next_pos;

	recv_pos = 0;

	if (mem.mem() [cr_pos + 2] == 13 /* CR */ &&
	    mem.mem() [cr_pos + 3] == 10 /* LF */)
	{
	    if (frontend && frontend->request)
		frontend.call (frontend->request, /*(*/ cur_req /*)*/);

	    *ret_accepted += 2;
	    *ret_header_parsed = true;
	    return Receiver::ProcessInputResult::Again;
	}

	field_offs += next_pos;
	mem = mem.region (next_pos);
    }

    unreachable ();
}

void
HttpServer::resetRequestState ()
{
    recv_pos = 0;
    recv_content_length = 0;
    recv_content_length_specified = false;
    cur_req = NULL;
    req_state = RequestState::RequestLine;
}

Receiver::ProcessInputResult
HttpServer::processInput (Memory const &_mem,
			  Size * const mt_nonnull ret_accepted,
			  void * const _self)
{
    logD (http, _func, _mem.len(), " bytes");

    HttpServer * const self = static_cast <HttpServer*> (_self);

    *ret_accepted = 0;

    Memory mem = _mem;
    for (;;) {
        if (self->input_blocked.get() == 1) {
            logD (http, _func, "input_blocked");
            return Receiver::ProcessInputResult::InputBlocked;
        }

	switch (self->req_state) {
	    case RequestState::RequestLine:
		logD (http, _func, "RequestState::RequestLine");
	    case RequestState::HeaderField: {
		if (!self->cur_req) {
		    self->cur_req = st_grab (new (std::nothrow) HttpRequest (self->client_mode));
		    self->cur_req->client_addr = self->client_addr;
		}

		bool header_parsed;
		Size line_accepted;
		Receiver::ProcessInputResult const res = self->receiveRequestLine (mem, &line_accepted, &header_parsed);
		*ret_accepted += line_accepted;
		if (!header_parsed)
		    return res;

		mem = mem.region (line_accepted);

		if ((self->client_mode && !self->recv_content_length_specified)
                    || self->recv_content_length > 0)
                {
		    self->recv_pos = 0;
		    self->req_state = RequestState::MessageBody;
		} else
		    self->resetRequestState ();
	    } break;
	    case RequestState::MessageBody: {
		logD (http, _func, "MessageBody, mem.len(): ", mem.len());

		Size toprocess = mem.len();
		bool must_consume = false;
                if (self->recv_content_length_specified
                    && toprocess >= self->recv_content_length - self->recv_pos)
                {
                    toprocess = self->recv_content_length - self->recv_pos;
                    must_consume = true;
                }

		logD (http, _func, "recv_pos: ", self->recv_pos, ", toprocess: ", toprocess);
		Size accepted = toprocess;
		if (self->frontend && self->frontend->messageBody) {
		    self->frontend.call (self->frontend->messageBody, /*(*/
			    self->cur_req,
			    Memory (mem.mem(), toprocess),
			    must_consume /* end_of_request */,
			    &accepted /*)*/);
		    assert (accepted <= toprocess);
		}

		if (must_consume && accepted != toprocess) {
		    logW (http, _func, "HTTP data was not processed in full");
		    return Receiver::ProcessInputResult::Error;
		}

		*ret_accepted += accepted;

		self->recv_pos += accepted;
		if (!self->recv_content_length_specified || self->recv_pos < self->recv_content_length) {
		    logD (http, _func, "waiting for more content "
                          "(recv_content_length_specified: ", self->recv_content_length_specified, ")");
		    return Receiver::ProcessInputResult::Again;
		}

		logD (http, _func, "message body processed in full");

	      // Message body processed in full.

		assert (self->recv_pos == self->recv_content_length);
		mem = mem.region (toprocess);

                self->resetRequestState ();
	    } break;
	    default:
		unreachable ();
	}
    }
    unreachable ();
}

void
HttpServer::processEof (void * const _self)
{
    HttpServer * const self = static_cast <HttpServer*> (_self);

    logD (http, _func_);

    if (self->frontend && self->frontend->closed)
	self->frontend.call (self->frontend->closed,
                             /*(*/ self->cur_req, (Exception*) NULL /* exc_ */ /*)*/);
}

void
HttpServer::processError (Exception * const exc_,
			  void      * const _self)
{
    HttpServer * const self = static_cast <HttpServer*> (_self);

    logD (http, _func_);

    if (self->frontend && self->frontend->closed)
	self->frontend.call (self->frontend->closed, /*(*/ self->cur_req, exc_ /*)*/);
}

void
HttpServer::sendStateChanged (Sender::SendState   const send_state,
                              void              * const _self)
{
    HttpServer * const self = static_cast <HttpServer*> (_self);

    logD (http, _func_);

    if (send_state == Sender::SendState::ConnectionReady) {
        logD (http, _func, "unblocking input");
        self->input_blocked.set (0);
        self->receiver->unblockInput ();
    } else {
        logD (http, _func, "blocking input");
        self->input_blocked.set (1);
    }
}

void
HttpServer::senderClosed (Exception * const exc_,
                          void      * const _self)
{
    HttpServer * const self = static_cast <HttpServer*> (_self);

    logD (http, _func_);

#warning Verify that the connection is not closed prematurely when we've already sent the reply but have not processed input data yet.
    if (self->frontend && self->frontend->closed)
	self->frontend.call (self->frontend->closed, /*(*/ self->cur_req, exc_ /*)*/);
}

}

