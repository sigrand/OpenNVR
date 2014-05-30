#ifndef MOMENT__PATH_MANAGER__H__
#define MOMENT__PATH_MANAGER__H__

#include <utility>
#include <map>

#include <sys/time.h>
#ifndef PLATFORM_WIN32
#include <sys/statvfs.h>
#else
#include <windows.h>
#endif

#include <moment/libmoment.h>

using namespace M;

namespace Moment {

enum QuotaResult
{
    QuotaSuccess = 0,
    QuotaErrNotFound,
    QuotaErrAlreadyExist,
    QuotaErrSpace,
    QuotaErrNotEmpty,
    QuotaErrOther
} ;

struct SpaceSizes
{
    SpaceSizes():occupSize(0),reservedFileSize(0),writing(false){}
    SpaceSizes(const SpaceSizes & ss)
    {
        occupSize = ss.occupSize;
        reservedFileSize = ss.reservedFileSize;
        writing = ss.writing;
    }

    Int64 occupSize;
    Int64 reservedFileSize;
    bool writing;
};

typedef std::map<std::string, SpaceSizes> SrcSizes;

class FullSrcSizes
{
public:
    FullSrcSizes():totalSize(0){}
    FullSrcSizes(const FullSrcSizes & fss)
    {
        totalSize = fss.totalSize;
        srcSizes = fss.srcSizes;
    }

    Int64 totalSize;
    SrcSizes srcSizes;

    Int64 GetTotalOccupSize();
    Int64 GetWritingSrcCount();
    bool FindSrc(const std::string & src);
};

typedef std::map<std::string, FullSrcSizes> DiskSrcSizes;

class Quota
{
public:
    Quota(){}
    Quota(const Quota & q)
    {
        diskSrcSizes = q.diskSrcSizes;
    }
    DiskSrcSizes diskSrcSizes;

    Int64 GetTotalSize();
    Int64 GetTotalOccupSize();
    Int64 GetFreeSize();
    bool FindSrc(const std::string & src);
    bool FindDisk(const std::string & diskname);
};

typedef std::map<Int64, Quota> IdQuota;

typedef Uint64 (*RemoveAllFilesCallback)(const std::string & path, const std::string & src);
typedef Int64 (*RemoveFilesCallback)(void * ffmodule, const std::string & src, Time start, Time end, std::map<std::string,Int64> & diskSizeDeletedOut);
typedef Int64 (*RemoveOldestFilesCallback)(void * ffmodule, std::vector<std::string> & srcList, std::string & pathOut, std::string & srcOut);

class PathManager: public Object
{
public:

    static PathManager& Instance();

    bool Init(const std::string & pathToConf, const std::string & pathToQuotas,
              RemoveAllFilesCallback    removeAllFilesCallback,
              RemoveFilesCallback       removeFilesCallback,
              RemoveOldestFilesCallback removeOldestFilesCallback,
              void * pFFmodule);

    bool IsInit();
    bool CreateOldSrc(const std::string & src); // create src from already existing sources in conf.d (when moment starts)
    QuotaResult CreateNewSrc(const std::string & src, Int64 id); // create new src by request
    QuotaResult DeleteSrc(const std::string & src); // assume that src is unique (only one on all quotas)
    std::string GetPath(const std::string & src, const std::string & prevPath);
    std::string GetPathForRecord(const std::string & src);
    std::string GetConfigJson();
    bool IsEmpty();

    bool GetDiskOccupation(const std::string & src, std::map<std::string, Int64> & diskOccupSize);

    QuotaResult AddQuota(Int64 id, Int64 size);
    QuotaResult UpdateQuota(Int64 id, Int64 size);
    QuotaResult RemoveQuota(Int64 id);

    Quota GetQuotaInfo(Int64 id);
    IdQuota GetQuotaList();

    bool GetDiskForSrc(const std::string & src, std::vector<std::string> & out);

    unsigned long GetPermission(const std::string & fileName, const Uint64 nDuration);
    bool Notify(const std::string & fileName, bool bDone, unsigned long size = 0);

    bool DumpAll();

    bool RemoveFiles(const std::string & src, Time timeStart, Time timeEnd);
    bool RemoveExpiredFiles(const std::string & filename, const std::string & path, const std::string src);

private:

    PathManager():m_isInit(false){}
    PathManager(const PathManager& root);
    PathManager& operator=(const PathManager&);

    RemoveAllFilesCallback      m_removeAllFilesCallback;
    RemoveFilesCallback         m_removeFilesCallback;
    RemoveOldestFilesCallback   m_removeOldestFileCallback;

    bool writeQuotaFile();

    bool m_isInit;

    std::string m_path_to_config;
    std::string m_path_to_quota;
    std::string m_configJson;

    static StateMutex g_mutexPathManager;

    IdQuota m_quotas; // [id, quota]
    std::vector<std::string> m_disknames;
    void * m_pFFmodule;
};

}

#endif /* MOMENT__PATH_MANAGER__H__ */
