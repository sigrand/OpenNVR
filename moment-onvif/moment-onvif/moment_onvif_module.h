
#ifndef MOMENT_ONVIF__MOMENT_ONVIF_MODULE__H__
#define MOMENT_ONVIF__MOMENT_ONVIF_MODULE__H__

#include <string>
#include <algorithm>
#include <list>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <moment/libmoment.h>

#include "OnvifSDK.h"

namespace MomentOnvif {

using namespace M;
using namespace Moment;

class RecData
{
public:
    std::string address;
    std::string recToken;
    std::string recJobToken;
    bool bMode;

    bool operator == ( const RecData& rhs )
    {
        return this->address == rhs.address &&
                 this->recToken == rhs.recToken &&
                 this->recJobToken == rhs.recJobToken &&
                 this->bMode == rhs.bMode;
    }
};

class MomentOnvifModule : public Object, private IOnvif
{
public:
    MomentOnvifModule ();
    ~MomentOnvifModule ();
    Result init (MomentServer *moment, int port);

private:
    //===DEV==============================
    virtual int GetDateAndTime( /*out*/ DevGetSystemDateAndTimeResponse & );
    virtual int SetDateAndTime( DevSetSystemDateAndTime & ){ return -1; }
    virtual int GetUsers( /*out*/ DevGetUsersResponse & ){ return -1; }
    //===DEVIO============================
    virtual int GetVideoOutputs( /*out*/ DevIOGetVideoOutputsResponse & ){ return -1; }
    //===DISP=============================
    virtual int GetLayout( std::string &, /*out*/ DispGetLayoutResponse & ){ return -1; }
    virtual int GetDisplayOptions( const std::string &, DispGetDisplayOptionsResponse & ){ return -1; }
    virtual int SetLayout( /*out*/ DispSetLayout &){ return -1; }
    virtual int CreatePaneConfiguration( DispCreatePaneConfiguration &, /*out*/ DispCreatePaneConfigurationResponse & ){ return -1; }
    //===RECV=============================
    virtual int GetReceivers( RecvGetReceiversResponse & ){ return -1; }
    virtual int CreateReceiver( const std::string & uri, /*out*/ std::string & recvToken ){ return -1; }
    virtual int SetReceiverMode( const std::string & recvToken, bool bMode ){ return -1; }
    //===RECORDING=========================
    virtual int CreateRecording (RecCreateRecording &, RecCreateRecordingResponse &);
    virtual int CreateRecordingJob (RecCreateRecordingJob &, RecCreateRecordingJobResponse &);
    virtual int DeleteRecording (const std::string &);
    virtual int DeleteRecordingJob (const std::string &);

    static void runOnvifServer(void* context)
    {
        ((IOnvifServer*)context)->Run();
    }

    enum SearchType
    {
        SearchByRecording,
        SearchByJob
    };

    void SendHttpRequest(const std::string &);

    RecData* findData(const std::string & recordingToken, SearchType type );
    static std::string currentRequest_;
    static bool IsRequiredJob(const RecData & data);
    static bool IsRequiredRec(const RecData & data);

    StateMutex mutex;
    mt_const MomentServer *moment_;
    IOnvifServer *onvif_;
    std::list<RecData> data_;
};

}


#endif // MOMENT_ONVIF__MOMENT_ONVIF_MODULE__H__

