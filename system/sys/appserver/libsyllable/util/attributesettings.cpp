/*  Attribute Settings - Manager for storing settings in files attributes.
 *  Copyright (C)2009 Dee Sharpe <demetrioussharpe@netscape.net>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU Library
 *  General Public License as published by the Free Software
 *  Foundation.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 *  MA 02111-1307, USA
 */


#include <os/kernel.h>
#include <os/types.h>
#include <os/fs_attribs.h>
#include <os/string.h>

#include <storage/fsnode.h>

#include <util/string.h>
#include <util/attributesettings.h>
#include <util/locker.h>

#include <map>

using namespace os;

typedef struct AttrBundle_s {
	fsattr_type	nType;
	String*		pcName;
	AttrData_s	uValue;
	bool		bDirty;
} AttrBundle_s;

class AttrContainer
{
public:
	AttrContainer();
	~AttrContainer();
	
	status_t	Insert( AttrBundle_s* psBundle );
	status_t	Remove( const String& cName );
	
	bool		HasAttrib( const String& cName ) const;
	AttrData_s*	GetAttrib( const String& cName );
	AttrBundle_s* GetBundle( const String& cName );
	
	void		MakeEmpty( void )			{	CALLED();	m_tSet.erase(m_tSet.begin(), m_tSet.end());	}
	bool		IsEmpty( void ) const		{	CALLED();	return( m_tSet.empty() );	}
	
	size_t		GetSize( void ) const		{	CALLED();	return( m_tSet.size() );	}
	
	AttrBundle_s*	GetFirstObj( void );
	AttrBundle_s*	Begin( void );
	AttrBundle_s*	GetLastObj( void );
	AttrBundle_s*	End( void );
	AttrBundle_s*	GetNextObj( AttrBundle_s* psBundle );
	
private:
	std::set<AttrBundle_s*>	m_tSet;
	Locker					*m_pcSetLock;
};

class AttributeSettings::Private
{
public:
	Private( const String& cPath );
	~Private();
	
	Private&	operator=( const Private& cSource );
	
	String GetPath() const;
	FSNode GetNode() const;
	
	status_t		AddSetting( AttrBundle_s *psSetting );
	status_t		RemoveSetting( const String& cName );
	status_t		ChangeSetting( const String& cName, AttrBundle_s& sBundle );
	AttrData_s&		GetSetting( const String& cName );
	size_t			CountSettings() const;
	
	bool			IsEmpty() const;
	
	status_t		LoadSettings();
	status_t		StoreSettings();
	status_t		EraseSettings();
	
private:
	AttrContainer	*m_pcSettings;
	FSNode			*m_pcNode;
	String			*m_pcPath;
};

AttrContainer::AttrContainer()
{
	CALLED();
	
	m_pcSetLock = new Locker("attribute_settings_container");
	
	MakeEmpty();
}

AttrContainer::~AttrContainer()
{
	CALLED();
	
	MakeEmpty();
	
	delete m_pcSetLock;
}

status_t AttrContainer::Insert( AttrBundle_s* psSettings )
{
	CALLED();
	
	AutoLocker _lock_(m_pcSetLock);

	if(!psSettings)
		return -EFAULT;
	else {
		if(m_tSet.find(psSettings) != m_tSet.end())
			return -EPERM;
		else {
			m_tSet.insert(psSettings);
			return EOK;
		}
	}
}

status_t AttrContainer::Remove( const String& cName )
{
	CALLED();
	
	AutoLocker _lock_(m_pcSetLock);
	std::set<AttrBundle_s*>::iterator it;

	it = m_tSet.begin();
	
	while(it != m_tSet.end()) {
		if((*it)->pcName->c_str() == cName.c_str()) {
			m_tSet.erase(it);
			return EOK;
		} else
			it++;
	}
	
	return -EFAULT;
}

bool AttrContainer::HasAttrib( const String& cName ) const
{
	CALLED();
	
	AutoLocker _lock_(m_pcSetLock);
	std::set<AttrBundle_s*>::iterator it;
	
	it = m_tSet.begin();
	
	while(it != m_tSet.end()) {
		if((*it)->pcName->c_str() == cName.c_str())
			return true;
		else
			it++;
	}
	
	return false;
}

AttrData_s* AttrContainer::GetAttrib( const String& cName )
{
	CALLED();
	
	AutoLocker _lock_(m_pcSetLock);
	std::set<AttrBundle_s*>::iterator it;
	
	it = m_tSet.begin();
	
	while(it != m_tSet.end()) {
		if((*it)->pcName->c_str() == cName.c_str())
			break;
		else
			it++;
	}
	
	return (&(*it)->uValue);
}

AttrBundle_s* AttrContainer::GetBundle( const String& cName )
{
	CALLED();
	
	AutoLocker _lock_(m_pcSetLock);
	std::set<AttrBundle_s*>::iterator it;
	
	it = m_tSet.begin();
	
	while(it != m_tSet.end()) {
		if((*it)->pcName->c_str() == cName.c_str())
			break;
		else
			it++;
	}
	
	return ((AttrBundle_s*)&(*it));
}

AttrBundle_s* AttrContainer::GetFirstObj( void )
{
	CALLED();
	
	return Begin();
}

AttrBundle_s* AttrContainer::Begin( void )
{
	CALLED();
	
	AutoLocker _lock_(m_pcSetLock);
	
	std::set<AttrBundle_s*>::iterator it;
	
	it = m_tSet.begin();
	
	return (*it);
}

AttrBundle_s* AttrContainer::GetLastObj( void )
{
	CALLED();
	
	AutoLocker _lock_(m_pcSetLock);

	std::set<AttrBundle_s*>::iterator it;
	
	it = --m_tSet.end();
	
	return (*it);
}

AttrBundle_s* AttrContainer::End( void )
{
	CALLED();
	
	AutoLocker _lock_(m_pcSetLock);

	std::set<AttrBundle_s*>::iterator it;
	
	it = m_tSet.end();
	
	return (*it);
}

AttrBundle_s* AttrContainer::GetNextObj( AttrBundle_s* psSettings )
{
	CALLED();
	
	AutoLocker _lock_(m_pcSetLock);
	
	std::set<AttrBundle_s*>::iterator it;
	
	if((it = m_tSet.find(psSettings)) == m_tSet.end())
		return NULL;
	
	it++;
	
	return (*it);
}

AttributeSettings::Private::Private( const String& cPath )
{
	CALLED();
	
	m_pcNode = new FSNode(cPath);
	m_pcPath = new String(cPath);
	m_pcSettings = new AttrContainer();
	
	LoadSettings();
}

AttributeSettings::Private::~Private()
{
	CALLED();
	
	StoreSettings();
	
	delete m_pcSettings;
	delete m_pcPath;
	delete m_pcNode;
}

AttributeSettings::Private& AttributeSettings::Private::operator=( const AttributeSettings::Private& cSource )
{
	CALLED();
	
	m_pcNode = new FSNode(cSource.GetPath());
	m_pcPath = new String(cSource.GetPath());
	m_pcSettings = new AttrContainer();
	
}

String AttributeSettings::Private::GetPath() const
{
	CALLED();
	
	String cPath = *m_pcPath;
	return cPath;
}

FSNode AttributeSettings::Private::GetNode() const
{
	CALLED();
	
	FSNode cNode = *m_pcNode;
	return cNode;
}

status_t AttributeSettings::Private::AddSetting( AttrBundle_s *psSetting )
{
	CALLED();
	
	psSetting->bDirty = true;
	
	return m_pcSettings->Insert(psSetting);
}

status_t AttributeSettings::Private::RemoveSetting( const String& cName )
{
	CALLED();
	
	return m_pcSettings->Remove(cName);
}

status_t AttributeSettings::Private::ChangeSetting( const String& cName, AttrBundle_s& sSetting )
{
	CALLED();
	
	AttrBundle_s *psSetting;
	
	psSetting = m_pcSettings->GetBundle(cName);
	
	psSetting->nType = sSetting.nType;
	psSetting->uValue = sSetting.uValue;
	psSetting->bDirty = true;
	
	return EOK;
}

AttrData_s& AttributeSettings::Private::GetSetting( const String& cName )
{
	CALLED();
	
	return (*m_pcSettings->GetAttrib(cName));
}

size_t AttributeSettings::Private::CountSettings() const
{
	CALLED();
	
	return m_pcSettings->GetSize();
}

bool AttributeSettings::Private::IsEmpty() const
{
	CALLED();
	
	return m_pcSettings->IsEmpty();
}

status_t AttributeSettings::Private::LoadSettings()
{
	CALLED();
	
	AttrBundle_s *psSettings;
	attr_info_s sBuffer;
	register ssize_t nLen;
	
	int32 nError;
	
	if((nError = m_pcNode->RewindAttrdir()) < 0) {
		dbprintf("%s: No attributes stored\n", __PRETTY_FUNCTION__);
		dbprintf("%s: error value is: %d\n", __PRETTY_FUNCTION__, nError);
		return -ENOENT;
	}
	
	while(true) {
		if((psSettings = (AttrBundle_s*)malloc(sizeof(AttrBundle_s))) == NULL) {
			dbprintf("%s: No memory for attribute bundle allocation\n", __PRETTY_FUNCTION__);
			return -ENOMEM;
		}
		
		if((psSettings->pcName = new String()) == NULL) {
			dbprintf("%s: No memory for string allocation\n", __PRETTY_FUNCTION__);
			free(psSettings);
			return -ENOMEM;
		}
		
		if(m_pcNode->GetNextAttrName(psSettings->pcName) <= 0) {
			dbprintf("%s: No more attributes for file\n", __PRETTY_FUNCTION__);
			break;
		}
		
		m_pcNode->StatAttr(*psSettings->pcName, &sBuffer);
		psSettings->nType = (fsattr_type)sBuffer.ai_type;
		
		switch(sBuffer.ai_type) {
			case ATTR_TYPE_NONE: {
	    			dbprintf("%s: [ATTR_TYPE_NONE (Raw binary data)] - this is not handled yet!!!", __PRETTY_FUNCTION__);
			} break;
			case ATTR_TYPE_INT32: {
				if(!(sBuffer.ai_size == sizeof(int32)) ||
					((nLen = m_pcNode->ReadAttr(*psSettings->pcName, ATTR_TYPE_INT32,
					&(psSettings->uValue.i32), 0, sBuffer.ai_size))!= sBuffer.ai_size))
					
					dbprintf("%s: [ATTR_TYPE_INT32] - ****** ERROR ****** \n", __PRETTY_FUNCTION__);
			} break;
			case ATTR_TYPE_INT64: {
				if(!(sBuffer.ai_size == sizeof(int64)) ||
					((nLen = m_pcNode->ReadAttr(*psSettings->pcName, ATTR_TYPE_INT64,
					&(psSettings->uValue.i64), 0, sBuffer.ai_size))!= sBuffer.ai_size))
					
					dbprintf("%s: [ATTR_TYPE_INT64] - ****** ERROR ****** \n", __PRETTY_FUNCTION__);
			} break;
			case ATTR_TYPE_FLOAT: {
				if(!(sBuffer.ai_size == sizeof(float)) ||
					((nLen = m_pcNode->ReadAttr(*psSettings->pcName, ATTR_TYPE_FLOAT,
					&(psSettings->uValue.f), 0, sBuffer.ai_size))!= sBuffer.ai_size))
					
					dbprintf("%s: [ATTR_TYPE_FLOAT] - ****** ERROR ****** \n", __PRETTY_FUNCTION__);
			} break;
			case ATTR_TYPE_DOUBLE: {
				if(!(sBuffer.ai_size == sizeof(double)) ||
					((nLen = m_pcNode->ReadAttr(*psSettings->pcName, ATTR_TYPE_DOUBLE,
					&(psSettings->uValue.d), 0, sBuffer.ai_size))!= sBuffer.ai_size))
					
					dbprintf("%s: [ATTR_TYPE_DOUBLE] - ****** ERROR ****** \n", __PRETTY_FUNCTION__);
			} break;
			case ATTR_TYPE_STRING: {
				if((nLen = m_pcNode->ReadAttr(*psSettings->pcName, ATTR_TYPE_STRING,
					&(psSettings->uValue.z), 0, sizeof(psSettings->uValue.z) - 1))!= sBuffer.ai_size)
					
					dbprintf("%s: [ATTR_TYPE_STRING] - ****** ERROR ****** \n", __PRETTY_FUNCTION__);
				else
					psSettings->uValue.z[nLen] = '\0';
			} break;
			default: {
				return -EFAULT;
			}
		}
		
		m_pcSettings->Insert(psSettings);
	}
	
	return EOK;
}

status_t AttributeSettings::Private::StoreSettings()
{
	CALLED();
	
	AttrBundle_s* psSettings;
	
	if(m_pcSettings->IsEmpty())
		return -ENODATA;
	
	while((psSettings = m_pcSettings->GetFirstObj()) != m_pcSettings->End()) {
		switch(psSettings->nType) {
			case ATTR_TYPE_NONE: {
				dbprintf("%s: [ATTR_TYPE_NONE (Raw binary data)] - this is not handled yet!!!", __PRETTY_FUNCTION__);
			} break;
			case ATTR_TYPE_INT32: {
				if(!psSettings->bDirty)
					break;
				m_pcNode->WriteAttr(*psSettings->pcName, O_TRUNC, ATTR_TYPE_INT32, &(psSettings->uValue.i32), 0, sizeof(int32));
				
				psSettings->bDirty = false;
			} break;
			case ATTR_TYPE_INT64: {
				if(!psSettings->bDirty)
					break;
				m_pcNode->WriteAttr(*psSettings->pcName, O_TRUNC, ATTR_TYPE_INT64, &(psSettings->uValue.i64), 0, sizeof(int64));
				
				psSettings->bDirty = false;
			} break;
			case ATTR_TYPE_FLOAT: {
				if(!psSettings->bDirty)
					break;
				m_pcNode->WriteAttr(*psSettings->pcName, O_TRUNC, ATTR_TYPE_FLOAT, &(psSettings->uValue.f), 0, sizeof(float));
				
				psSettings->bDirty = false;
			} break;
			case ATTR_TYPE_DOUBLE: {
				if(!psSettings->bDirty)
					break;
				m_pcNode->WriteAttr(*psSettings->pcName, O_TRUNC, ATTR_TYPE_DOUBLE, &(psSettings->uValue.d), 0, sizeof(double));
				
				psSettings->bDirty = false;
			} break;
			case ATTR_TYPE_STRING: {
				if(!psSettings->bDirty)
					break;
				m_pcNode->WriteAttr(*psSettings->pcName, O_TRUNC, ATTR_TYPE_STRING, &(psSettings->uValue.z),
									0, sizeof(psSettings->uValue.z) - 1);
				
				psSettings->bDirty = false;
			} break;
			default: {
				return -EFAULT;
			}
		}
		
		m_pcSettings->Remove(*psSettings->pcName);
	}
	
	return EOK;
}

status_t AttributeSettings::Private::EraseSettings()
{
	CALLED();
	
	String *pcName = new String();
	
	if((m_pcNode->RewindAttrdir()) < 0) {
		dbprintf("%s: No attributes stored\n", __PRETTY_FUNCTION__);
		
		delete pcName;
		return EOK;
	}
	
	while(m_pcNode->GetNextAttrName(pcName) < 0) {
		m_pcNode->RemoveAttr(*pcName);
	}	
	delete pcName;
	
	delete m_pcSettings;
	m_pcSettings = new AttrContainer();
	
	return EOK;
}

AttributeSettings::AttributeSettings( const String& cPath )
{
	CALLED();
	
	m = new Private(cPath);
}

AttributeSettings::~AttributeSettings()
{
	CALLED();
	
	delete m;
}

AttributeSettings& AttributeSettings::operator=( const AttributeSettings& cSource )
{
	CALLED();
	
	m = cSource.m;	
}

String AttributeSettings::GetPath() const
{
	CALLED();
	
	return (m->GetPath());
}

FSNode AttributeSettings::GetNode() const
{
	CALLED();
	
	return (m->GetNode());
}

status_t AttributeSettings::AddSetting( const char* pzName, AttrData_s& sData, fsattr_type nType )
{
	CALLED();
	
	AttrBundle_s *psSetting = new AttrBundle_s;
	
	psSetting->pcName = new String(pzName);
	psSetting->nType = nType;
	psSetting->uValue = sData;
	
	return (m->AddSetting(psSetting));
}

status_t AttributeSettings::RemoveSetting( const String& cName )
{
	CALLED();
	
	return (m->RemoveSetting(cName));
}

status_t AttributeSettings::ChangeSetting( const char* pzName, AttrData_s& sData, fsattr_type nType )
{
	CALLED();
	
	AttrBundle_s sSetting;
	
	sSetting.nType = nType;
	sSetting.uValue = sData;
	
	return (m->ChangeSetting(String(pzName), sSetting));
}

AttrData_s& AttributeSettings::GetSetting( const String& cName )
{
	CALLED();
	
	return (m->GetSetting(cName));
}

size_t AttributeSettings::CountSettings() const
{
	CALLED();
	
	return (m->CountSettings());
}

bool AttributeSettings::IsEmpty() const
{
	CALLED();
	
	return m->IsEmpty();
}

status_t AttributeSettings::LoadSettings()
{
	CALLED();
		
	return (m->LoadSettings());
}

status_t AttributeSettings::StoreSettings()
{
	CALLED();
	
	return (m->StoreSettings());
}

status_t AttributeSettings::EraseSettings()
{
	CALLED();
	
	return (m->EraseSettings());
}


