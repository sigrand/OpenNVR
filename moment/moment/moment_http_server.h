#ifndef MOMENT__HTTP_SERVER__H__
#define MOMENT__HTTP_SERVER__H__

#include <moment/libmoment.h>
#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Util/ServerApplication.h>
#include <vector>
#include <string>
#include <iostream>

using namespace Poco::Net;
using namespace Poco::Util;

namespace Moment {

using namespace M;

class HttpHandlerFactory : public HTTPRequestHandlerFactory
{
public:
	virtual HTTPRequestHandler* createRequestHandler(const HTTPServerRequest &);
};

class AdminHttpHandlerFactory : public HTTPRequestHandlerFactory
{
public:
    virtual HTTPRequestHandler* createRequestHandler(const HTTPServerRequest &);
};

class MomentHTTPServer : public ServerApplication
{
public:

    void SetPorts(int http_port, int admin_http_port);

    MomentHTTPServer():http_port(-1),admin_http_port(-1)
    {

    }

protected:

	int main(const std::vector<std::string> &);

private:

    int http_port;
    int admin_http_port;
};
}
#endif /* MOMENT__HTTP_SERVER__H__ */
