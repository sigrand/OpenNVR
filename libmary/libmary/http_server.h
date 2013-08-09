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


#ifndef LIBMARY__HTTP_SERVER__H__
#define LIBMARY__HTTP_SERVER__H__

#include <map>
#include <string>
#include <algorithm>

#include <libmary/types.h>
#include <libmary/list.h>
#include <libmary/hash.h>
#include <libmary/exception.h>
#include <libmary/st_referenced.h>
#include <libmary/sender.h>
#include <libmary/receiver.h>
#include <libmary/code_referenced.h>
#include <libmary/util_net.h>


namespace M {

typedef void (*ParseHttpParameters_Callback) (ConstMemory  name,
                                              ConstMemory  value,
                                              void        *cb_data);

void parseHttpParameters (ConstMemory                   mem,
                          ParseHttpParameters_Callback  param_cb,
                          void                         *param_cb_data);

class HttpServer;

// Connection -> InputFrontend, OutputFrontend;
// Receiver - только принимает данные (InputFrontend);
// Sender - только отправляет данные (OutputFrontend);
// Server - и принимает, и отправляет данные (Input+Output);
// Service - принимает соединения и сам строит вокруг них цепочки обработки.

class HttpRequest : public StReferenced
{
    friend class HttpServer;

private:
    class Parameter : public HashEntry<>
    {
    public:
	ConstMemory name;
	ConstMemory value;
    };

    typedef Hash< Parameter,
		  ConstMemory,
		  MemberExtractor< Parameter,
				   ConstMemory,
				   &Parameter::name >,
		  MemoryComparator<> >
	    ParameterHash;

    bool client_mode;

    Ref<String> request_line;
    ConstMemory method;
    ConstMemory full_path;
    ConstMemory *path;
    Count num_path_elems;

    // TODO Support X-Real-IP and alikes.
    IpAddress   client_addr;
    Uint64      content_length;
    bool        content_length_specified;
    Ref<String> accept_language;
    Ref<String> if_modified_since;
    Ref<String> if_none_match;

    ParameterHash parameter_hash;

    bool keepalive;

    std::map<std::string, std::string> decoded_args;

public:
    ConstMemory getRequestLine     () const { return request_line ? request_line->mem() : ConstMemory(); }
    ConstMemory getMethod          () const { return method; }
    ConstMemory getFullPath        () const { return full_path; }
    Count       getNumPathElems    () const { return num_path_elems; }
    IpAddress   getClientAddress   () const { return client_addr; }
    Uint64      getContentLength   () const { return content_length; }
    bool        getContentLengthSpecified () const { return content_length_specified; }
    ConstMemory getAcceptLanguage  () const { return accept_language ? accept_language->mem()   : ConstMemory(); }
    ConstMemory getIfModifiedSince () const { return if_modified_since ? if_modified_since->mem() : ConstMemory(); }
    ConstMemory getIfNoneMatch     () const { return if_none_match   ? if_none_match->mem()     : ConstMemory(); }

    void setKeepalive (bool const keepalive) { this->keepalive = keepalive; }
    bool getKeepalive () const { return keepalive; }

    bool hasBody () const { return (client_mode && !getContentLengthSpecified()) || getContentLength() > 0; }

    ConstMemory getPath (Count const index) const
    {
	if (index >= num_path_elems)
	    return ConstMemory();

	return path [index];
    }

    // If ret.mem() == NULL, then the parameter is not set.
    // If ret.len() == 0, then the parameter has empty value.
    ConstMemory getParameter (ConstMemory const name)
    {
    std::string key ((const char*) name.mem(), name.len());
    std::string decoded_value = decoded_args[key];
    if(decoded_value.empty())
        return ConstMemory();

    ConstMemory const mem (decoded_value.c_str(), decoded_value.length());
    return mem;
    }

    bool hasParameter (ConstMemory const name) { return parameter_hash.lookup (name); }

private:
    static void parseParameters_paramCallback (ConstMemory  name,
                                               ConstMemory  value,
                                               void        *_self);

public:
    void parseParameters (Memory mem);

    struct AcceptedLanguage
    {
        // Always non-null after parsing.
        Ref<String> lang;
        double weight;
    };

    static void parseAcceptLanguage (ConstMemory             mem,
                                     List<AcceptedLanguage> * mt_nonnull res_list);

    struct EntityTag
    {
        Ref<String> etag;
        bool weak;
    };

    static void parseEntityTagList (ConstMemory      mem,
                                    bool            *ret_any,
                                    List<EntityTag> * mt_nonnull ret_etags);

    HttpRequest (bool const client_mode)
	: client_mode    (client_mode),
          path           (NULL),
	  num_path_elems (0),
	  content_length (0),
          content_length_specified (false),
	  keepalive      (true)
    {
    }

    ~HttpRequest ();
};

class HttpServer : public DependentCodeReferenced
{
public:
    struct Frontend {
	void (*request) (HttpRequest * mt_nonnull req,
			 // TODO ret_parse_query - для разбора данных от форм 
			 void        *cb_data);

        // Called only when req->hasBody() is true.
	void (*messageBody) (HttpRequest * mt_nonnull req,
			     Memory       mem,
			     bool         end_of_request,
			     Size        * mt_nonnull ret_accepted,
			     void        *cb_data);

	void (*closed) (HttpRequest *req,
                        Exception   *exc_,
			void        *cb_data);
    };

private:
    class RequestState
    {
    public:
	enum Value {
	    RequestLine,
	    HeaderField,
	    MessageBody
	};
	operator Value () const { return value; }
	RequestState (Value const value) : value (value) {}
	RequestState () {}
    private:
	Value value;
    };

    mt_const Cb<Frontend> frontend;

    mt_const DataDepRef<Receiver> receiver;
    mt_const DataDepRef<Sender>   sender;
    mt_const DataDepRef<PagePool> page_pool;

    mt_const bool client_mode;

    mt_const IpAddress client_addr;

    AtomicInt input_blocked;

    StRef<HttpRequest> cur_req;

    RequestState req_state;
    // Number of bytes received for message body / request line / header field.
    Size recv_pos;
    // What the "Content-Length" HTTP header said for the current request.
    Size recv_content_length;
    bool recv_content_length_specified;

    Result processRequestLine (Memory mem);

    void processHeaderField (ConstMemory const &mem);

    Receiver::ProcessInputResult receiveRequestLine (Memory _mem,
						     Size * mt_nonnull ret_accepted,
						     bool * mt_nonnull ret_header_parsed);

    void resetRequestState ();

  mt_iface (Receiver::Frontend)
    static Receiver::Frontend const receiver_frontend;

    static Receiver::ProcessInputResult processInput (Memory const &mem,
						      Size * mt_nonnull ret_accepted,
						      void *_self);

    static void processEof (void *_self);

    static void processError (Exception *exc_,
			      void      *_self);
  mt_iface_end

  mt_iface (Sender::Frontend)
    static Sender::Frontend const sender_frontend;

    static void sendStateChanged (Sender::SendState   send_state,
                                  void              *_self);

    static void senderClosed (Exception *exc_,
                              void      *_self);
  mt_iface_end

public:
    void init (CbDesc<Frontend> const &frontend,
               Receiver               * const mt_nonnull receiver,
               Sender                 * const sender    /* may be NULL for client mode */,
               PagePool               * const page_pool /* may be NULL for client mode */,
               IpAddress const        &client_addr,
               bool                     const client_mode = false)
    {
        this->frontend = frontend;
        this->receiver = receiver;
        this->sender = sender;
        this->page_pool = page_pool;
	this->client_addr = client_addr;
        this->client_mode = client_mode;

        receiver->setFrontend (
                CbDesc<Receiver::Frontend> (&receiver_frontend, this, getCoderefContainer()));

        if (sender) {
            sender->setFrontend (
                    CbDesc<Sender::Frontend> (&sender_frontend, this, getCoderefContainer()));
        }
    }

    HttpServer (Object * const coderef_container)
	: DependentCodeReferenced (coderef_container),
          receiver  (coderef_container),
	  sender    (coderef_container),
          page_pool (coderef_container),
          client_mode (false),
	  req_state (RequestState::RequestLine),
	  recv_pos (0),
	  recv_content_length (0),
          recv_content_length_specified (false)
    {}
};

}


#endif /* LIBMARY__HTTP_SERVER__H__ */

