
#include <string>
#include <iostream>

#include <stdio.h>
#include "OnvifSDK.h"

void command(const std::string &s)
{
    int iRet;
    IOnvifClient * pClient = getOnvifClient();
    soap* pSoap = pClient->GetSoap();

    if (s == "cr") {
        RecCreateRecording r(pSoap);
        RecCreateRecordingResponse resp(pSoap);

        std::string address;
        printf("type address of the stream...\n");
        std::getline(std::cin, address);

        r.setSource(GenerateToken(), address);
        iRet = pClient->CreateRecording(r, resp);
        printf("Recording token = [%s]\n", resp.getToken().c_str());
    } else if (s == "cj") {
        RecCreateRecordingJob r(pSoap);

        std::string recording;
        printf("type token of the recording...\n");
        std::getline(std::cin, recording);
        r.setRecording(recording);
        r.setMode(true);
        RecCreateRecordingJobResponse resp(pSoap);

        iRet = pClient->CreateRecordingJob(r, resp);
        printf("RecordingJob token = [%s]\n", resp.getToken().c_str());
    } else if (s == "dr" || s == "dj") {

        std::string token;
        printf("type token for deleting...\n", iRet);
        std::getline(std::cin, token);

        if (s == "dr")
            iRet = pClient->DeleteRecording(token);
        else if (s == "dj")
            iRet = pClient->DeleteRecordingJob(token);

    }

    printf("command result %d\n", iRet);
}

int main(const int argc, const char* argv[])
{ 
    IOnvifClient * pClient = getOnvifClient();
    if(argc < 2)
    {
        printf("Input endpoint address. example:127.0.0.1:80\n");
        return -1;
    }

    pClient->Init(argv[1]);
    soap* pSoap = pClient->GetSoap();
    DevGetSystemDateAndTimeResponse r(pSoap);
    int iRet = pClient->GetDateAndTime(r);
    if(iRet != 0)
    {
        printf("GetDateAndTime failed\n");
    }
    else
    {
         int nYear, nMonth, nDay, nHour, nMin, nSec;
         r.GetUTCDateAndTime(nYear, nMonth, nDay, nHour, nMin, nSec);
         printf("DeviceService response is:\nDate is %d:%d:%d\nTime is %d:%d:%d\n", nYear, nMonth, nDay, nHour, nMin, nSec);
    }

    while(1)
    {
        printf("Choose operation:\n cr (createRecording),\n"
               "cj (createRecordingJob),\n"
               " dr (deleteRecording),\n"
               "dj (deleteRecordingJob)\n");
        std::string oper;
        std::getline(std::cin, oper);
        command(oper);
    };
	
    return 0; 
}

