#include <moment/libmoment.h>
#include "moment_request_handler.h"

using namespace M;
using namespace Moment;

namespace Moment {


//======== common handler


static void commonHandleRequest(HTTPServerRequest &req, HTTPServerResponse &resp, HandlerMap &handlerMap)
{
    URI uri(req.getURI());
    std::vector < std::string > segments;
    uri.getPathSegments(segments);

    if(segments.size() != 0)
    {
        std::string handlerName = segments[0];
        std::pair<HandlerMap::const_iterator, HandlerMap::const_iterator> _pair = handlerMap.equal_range(handlerName);
        bool bRes = false;
        for(_pair.first; _pair.first != _pair.second; _pair.first++)
        {
            HandlerFunc func = _pair.first->second.first;
            bRes = func(req, resp, _pair.first->second.second);
            if(bRes) break;
        }
        if(!bRes)
        {
            logE_(_func_, "there is no such handler: [", handlerName.c_str(), "]");
            resp.setStatus(HTTPResponse::HTTP_NOT_FOUND);
            std::ostream& out = resp.send();
            out << "404 method not found";
            out.flush();
        }
//        if(handlerMap.find(handlerName) != handlerMap.end())
//        {
//            HandlerFunc func = handlerMap[handlerName].first;
//            func(req, resp, handlerMap[handlerName].second);
//        }
//        else
//        {
//            logE_(_func_, "there is no such handler: [", handlerName.c_str(), "]");
//            resp.setStatus(HTTPResponse::HTTP_NOT_FOUND);
//            std::ostream& out = resp.send();
//            out << "404 method not found";
//            out.flush();
//        }
    }
    else
    {
        logE_(_func_, "there is no any handler");
        resp.setStatus(HTTPResponse::HTTP_NOT_FOUND);
        std::ostream& out = resp.send();
        out << "404 method not found";
        out.flush();
    }
}


//======== http handler


void HttpReqHandler::addHandler(std::string ss, HandlerFunc handler, void * data)
{
    //_handlerMap[ss] = std::make_pair(handler, data);
    _handlerMap.insert(std::make_pair(ss, std::make_pair(handler, data)));
}

void HttpReqHandler::handleRequest(HTTPServerRequest &req, HTTPServerResponse &resp)
{
    commonHandleRequest(req, resp, _handlerMap);
}

HandlerMap HttpReqHandler::_handlerMap;

}
