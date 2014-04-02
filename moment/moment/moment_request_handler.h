#ifndef MOMENT__REQUEST_HANDLER__H__
#define MOMENT__REQUEST_HANDLER__H__

#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include "Poco/Net/HTTPClientSession.h"
#include <Poco/Net/HTMLForm.h>
#include <Poco/StreamCopier.h>
#include <Poco/Util/ServerApplication.h>
#include <Poco/URI.h>
#include <iostream>
#include <string>

using namespace Poco::Net;
using namespace Poco::Util;
using Poco::Net::HTTPClientSession;
using Poco::Net::HTMLForm;
using Poco::StreamCopier;
using Poco::URI;

namespace Moment {

typedef bool (*HandlerFunc)(HTTPServerRequest &req, HTTPServerResponse &resp, void * data);
typedef std::multimap<std::string, std::pair<HandlerFunc, void*> > HandlerMap;

class HttpReqHandler : public HTTPRequestHandler
{
  static HandlerMap _handlerMap;

public:

  static void addHandler(std::string ss, HandlerFunc handler, void * data);

  virtual void handleRequest(HTTPServerRequest &req, HTTPServerResponse &resp);
};

class AdminHttpReqHandler : public HTTPRequestHandler
{
  static HandlerMap _handlerMap;

public:

  static void addHandler(std::string ss, HandlerFunc handler, void * data);

  virtual void handleRequest(HTTPServerRequest &req, HTTPServerResponse &resp);
};

}
#endif /* MOMENT__REQUEST_HANDLER__H__ */
