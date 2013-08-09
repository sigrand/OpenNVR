
#include <moment-onvif/moment_onvif_module.h>

namespace MomentOnvif {

using namespace M;
using namespace Moment;

std::string MomentOnvifModule::currentRequest_ = "";

MomentOnvifModule::MomentOnvifModule ()
{
}

MomentOnvifModule::~MomentOnvifModule ()
{
}

Result MomentOnvifModule::init (MomentServer *moment, int port)
{
	moment_ = moment;
	onvif_ = getOnvifServer();
    onvif_->Init(DEV_S | RECORD_S, port, this);

    {
	Ref<Thread> const thread = grab (new (std::nothrow) Thread (
		CbDesc<Thread::ThreadFunc> (runOnvifServer, onvif_, NULL)));
	if (!thread->spawn (false /* joinable */))
	    logE_ (_func, "Failed to spawn glib main loop thread: ", exc->toString());
    }

	return Result::Success;
}

int MomentOnvifModule::GetDateAndTime(DevGetSystemDateAndTimeResponse & dt)
{
    time_t t = time(0);
    struct tm * now = localtime( & t );

    if(now == NULL)
    {
        return -1;
    }

    dt.SetUTCDateAndTime(now->tm_year + 1900, now->tm_mon + 1, now->tm_mday, now->tm_hour, now->tm_min, now->tm_sec);
    return 0;
}

int MomentOnvifModule::CreateRecording (RecCreateRecording & req, RecCreateRecordingResponse & resp)
{
    std::string sourceId;
    std::string address;
    req.getSource( sourceId, address );

    RecData entry;
    entry.address = address;
    entry.recToken = GenerateToken();
    data_.push_back( entry );
    resp.setToken( entry.recToken );
    return 0;
}

RecData* MomentOnvifModule::findData(const std::string & recordingToken, SearchType type)
{
    currentRequest_ = recordingToken;
    std::list<RecData>::iterator it = data_.end();
    if( type == SearchByRecording )
        it = std::find_if( data_.begin(), data_.end(), IsRequiredRec );
    else if( type == SearchByJob )
        it = std::find_if( data_.begin(), data_.end(), IsRequiredJob );

    if ( it == data_.end())
    {
        logD_( _func, "Not Found " );
        return NULL;
    }
    logD_( _func, "Found ");
    return &(*it);
}

bool MomentOnvifModule::IsRequiredRec(const RecData & data)
{
    return data.recToken == MomentOnvifModule::currentRequest_;
}

bool MomentOnvifModule::IsRequiredJob(const RecData & data)
{
    return data.recJobToken == MomentOnvifModule::currentRequest_;
}

void MomentOnvifModule::SendHttpRequest(const std::string & r)
{
    #warning method SendHttpRequest supposed to be rewritten or replaced with libCURL call
    int sd;
    struct sockaddr_in serv_addr;

    sd = socket(AF_INET, SOCK_STREAM, 0);

    if (sd < 0)
      return;

    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8082);

    if ( inet_aton("127.0.0.1", &serv_addr.sin_addr) <= 0 )
    {
        logD_ ( _func, "TIMECLNT: Invalid remote IP address.\n");
        return;
    }

    if (::connect(sd, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
    {
        logD_ ( _func, "connect failed");
        return;
    }

    int iRet = send(sd, r.c_str(), r.length(), 0);
    logD_ ( _func, "send", iRet);
}

int MomentOnvifModule::CreateRecordingJob (RecCreateRecordingJob & req, RecCreateRecordingJobResponse & resp)
{
    logD_ (_func, "hit");
    const std::string rec = req.getRecording();
    RecData * pEntry = findData( rec, SearchByRecording );
    if (pEntry == NULL)
        return -1;
    pEntry->recJobToken = GenerateToken();
    pEntry->bMode = req.getMode();
    resp.setToken( pEntry->recJobToken );

    if(!pEntry->bMode)
        return 0;
    std::stringstream ss;
    ss << "GET /admin/add_channel?conf_file=" << rec << "&title=" << rec
       << "&uri=" << pEntry->address
       << " HTTP/1.1\r\nHost: www.example.com\r\n\r\n";
    std::string request = ss.str();
    SendHttpRequest(request);
    return 0;
}

int MomentOnvifModule::DeleteRecording (const std::string & str)
{
    RecData * pEntry = findData( str, SearchByRecording );
    if (pEntry == NULL)
        return -1;
    data_.remove( *pEntry );


    # warning call here to delete data
    return 0;
}

int MomentOnvifModule::DeleteRecordingJob (const std::string & str)
{
    RecData * pEntry = findData( str, SearchByJob );
    if (pEntry == NULL)
        return -1;
    pEntry->bMode = false;
    pEntry->recJobToken = "";

    std::stringstream ss;
    ss << "GET /admin/remove_channel?conf_file=" << pEntry->recToken
       << " HTTP/1.1\r\nHost: www.example.com\r\n\r\n";
    std::string request = ss.str();
    SendHttpRequest(request);
    return 0;
}

}
