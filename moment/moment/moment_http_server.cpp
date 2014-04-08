#include "moment_request_handler.h"
#include "moment_http_server.h"

namespace Moment {

HTTPRequestHandler* HttpHandlerFactory::createRequestHandler(const HTTPServerRequest &)
{
    return new HttpReqHandler;
}

void MomentHTTPServer::SetPorts(int http_port)
{
    this->http_port = http_port;
}

int MomentHTTPServer::main(const std::vector<std::string> &)
{
    if(http_port == -1)
        return Application::EXIT_NOHOST; // this error is most suitable

    HTTPServer * http_server = new HTTPServer(new HttpHandlerFactory, ServerSocket(http_port), new HTTPServerParams);

    http_server->start();

	waitForTerminationRequest();  // wait for CTRL-C or kill

    http_server->stop();

	return Application::EXIT_OK;
}

}
