#include "provider.h"
#include "../../fuzzy.h"
#include "atheos/filesystem.h"
#include "posix/errno.h"
#include "storage/nodemonitor.h"
#include "storage/registrar.h"
#include "util/variant.h"
#include <cerrno>

FileProvider::FileProvider() : Provider("FileProvider"), m_cLock("FileProvider"), m_pcManager(NULL),m_pcNodeMonitor(NULL), m_nFuzzyScore(5){  
}

FileProvider::~FileProvider()
{
}

status_t FileProvider::Initialize(){
	m_pcManager = os::RegistrarManager::Get();
	if( m_pcManager == NULL )
	{
		errno = EINVAL;
		perror( "Failed to get manager" );
		return EINVAL;
	}
    m_pcNodeMonitor = new os::NodeMonitor("/",NWATCH_ALL,this);

   if (m_pcNodeMonitor == NULL){
        errno = EINVAL;
        perror("Failed to generate a node monitor");
        return EINVAL;
   }
   return EOK;
}


std::vector<Result> FileProvider::Process(const os::Variant& cVariant)
{
    /*for( int i = 0; i < m_cList.GetCount(); i++ ){
        int score = 0;
        fts::fuzzy_match(cName.c_str(),m_cList.GetName(i).c_str(),score);

        if (score > 5){
            os::Image* pcItemIcon = NULL;
            if( m_pcManager->GetTypeAndIcon( m_cList.GetPath(i), os::Point( 64, 64 ), &zTemp, &pcItemIcon ) < 0 ){
                pcItemIcon = NULL;
            }
            Result result = Result("ApplicationProvider",m_cList.GetName(i),m_cList.GetCategory(i),(os::BitmapImage*)pcItemIcon,1.0f);

            result.addMetaData(std::pair<os::String,os::String>("path",m_cList.GetPath(i)));
            results.push_back(result);
        }
    }*/
    return std::vector<Result>();
}

status_t FileProvider::Launch(const Result&)
{
    return 0;
}


