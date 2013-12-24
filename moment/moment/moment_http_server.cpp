#include "moment_request_handler.h"
#include "moment_http_server.h"

namespace Moment {

HTTPRequestHandler* HttpHandlerFactory::createRequestHandler(const HTTPServerRequest &)
{
    return new HttpReqHandler;
}

HTTPRequestHandler* AdminHttpHandlerFactory::createRequestHandler(const HTTPServerRequest &)
{
    return new AdminHttpReqHandler;
}

void MomentHTTPServer::SetPorts(int http_port, int admin_http_port)
{
    this->http_port = http_port;
    this->admin_http_port = admin_http_port;
}

int MomentHTTPServer::main(const std::vector<std::string> &)
{
    if(http_port == -1 || admin_http_port == -1)
        return Application::EXIT_NOHOST; // this error is most suitable

    HTTPServer * http_server = new HTTPServer(new HttpHandlerFactory, ServerSocket(http_port), new HTTPServerParams);
    HTTPServer * admin_http_server = new HTTPServer(new AdminHttpHandlerFactory, ServerSocket(admin_http_port), new HTTPServerParams);

    http_server->start();
    admin_http_server->start();

	waitForTerminationRequest();  // wait for CTRL-C or kill

    http_server->stop();
    admin_http_server->stop();

	return Application::EXIT_OK;
}

}
