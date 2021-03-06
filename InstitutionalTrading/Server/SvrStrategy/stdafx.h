// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>
#ifndef DISALLOW_COPY_AND_ASSIGN
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
	TypeName(const TypeName&); \
	void operator=(const TypeName&)
#endif
#include <vector>
#include <map>
#include <set>
#include <queue>
#include "iostream"
#include "fstream"
#include "ostream"
#include "string"
#include "CommonStruct.h"

#include <Tools_Win32.h>


#include  "ReadWriteLock.h"
#include "..\SvrDBOpr\Interface_SvrDBOpr.h"
#include "..\SvrLogin\Interface_SvrLogin.h"
#include "..\SvrTradeData\Interface_SvrTradeData.h"
#include "..\SvrNotifyAndAsk\Interface_SvrNotifyAndAsk.h"
#include "..\SvrUserOrg\Interface_SvrUserOrg.h"
#include "..\SvrBrokerInfo\Interface_SvrBrokerInfo.h"
#include "..\SvrMsg\Interface_SvrMsg.h"

#define SAFE_GET_DATACENTER()  if(CDataCenter::Get())CDataCenter::Get()
#define safestrcpy(dest,destlen,src) strncpy_s(dest,destlen,src,(destlen)-1)
int GetTodayString(char* datestring=NULL);
void GetFolderFileName(std::string& strFileName, std::string& folder);

#include "FileOpr.h "
#include "tinyxml.h"
#if 0
#define Strategy_LOGINFO(fmt, ...)  ;
#else
#define Strategy_LOGINFO(fmt, ...)  CFileOpr::getObj().writelocallogDaily("log","Strategy", "[%s][%d]:"fmt, __FILE__, __LINE__, __VA_ARGS__);
#endif

/*
class OutPut
{
public:
	OutPut(std::string str)
	{
		m_str = str;
		lFfreq = CTools_Win32::MyGetCpuTickFreq();
		n1 =  CTools_Win32::MyGetCpuTickCounter();
		
	}
	~OutPut()
	{		
		LONGLONG n2 =  CTools_Win32::MyGetCpuTickCounter();
		double db1 = (n2-n1)*1000.0/lFfreq;
		if(db1 >0.0001)
			OUTPUT_LOG("%s %.4f " ,m_str.c_str(), db1);
	}
private:
	std::string m_str;
	LONGLONG lFfreq;
	LONGLONG n1;
};*/