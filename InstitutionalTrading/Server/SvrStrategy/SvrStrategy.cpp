// testtcpsvr.cpp : Defines the exported functions for the DLL application.
//

#pragma warning(disable : 4996)
#pragma warning(disable : 4786)
#include "stdafx.h"
#include <windows.h>
#include "SvrStrategy.h"
#include "CommonPkg.h"
#include "CommonDef.h"
#include "EventParam.h"
#include "tmpstackbuf.h"
#include "Manage.h"

//-----------------------------------------------------------------------------------
//	import Tools4dll库
//-----------------------------------------------------------------------------------
#ifdef _DEBUG
#pragma comment(lib, "Tools4dllD.lib")											
#else 
#pragma comment(lib, "Tools4dll.lib")											
#endif

//-----------------------------------------------------------------------------------
//	下面import本模块需要引用的其它模块
//-----------------------------------------------------------------------------------
#pragma comment(lib, "SvrTcp.lib")
#pragma comment(lib, "SvrTradeData.lib")
#pragma comment(lib, "SvrTradeExcute.lib")
#pragma comment(lib, "SvrDBOpr.lib")
#pragma comment(lib, "SvrUserOrg.lib")
#pragma comment(lib, "SvrMsg.lib")
#pragma comment(lib, "SvrNotifyAndAsk.lib")
#pragma comment(lib, "SvrBrokerInfo.lib")

#include "..\\SvrTcp\\Interface_SvrTcp.h"

#define WRITELOGID_Strategy


//全局互斥锁
Ceasymutex			g_mutex;

//线程参数
HANDLE				g_hThread=NULL;
DWORD				g_idThread=0;

bool				g_disConnect = false;

DWORD ThreadWorker(void *arg);

//处理一个接收到的数据包
void ProcessOneUniPkg_InThread(int hSocket,const Stru_UniPkgHead& PkgHead,const void*pPkgData);

//处理内部通知的事件
bool ParseDataChangedEvent(const Stru_NotifyEvent& dataEvt);

bool GetTradeAccountByUserID(std::string strTraderID, std::string& strBrokerID, std::string& strInvestID);
CManage g_manage;

//模块初始化，创建工作线程并向SvrTcp订阅感兴趣的数据包
SVRSTRATEGY_API void InitFunc(void)
{		
	//创建工作线程
	g_hThread=CreateThread(NULL,10*1024*1024,(LPTHREAD_START_ROUTINE)ThreadWorker,0,0,&g_idThread);

	//下面订阅本线程感兴趣的数据包
	CInterface_SvrTcp::getObj().SubscribePkg(CMD_STRTEGY_UPLOAD_Base_Req,g_idThread);
	CInterface_SvrTcp::getObj().SubscribePkg(CMD_STRTEGY_UPLOAD_STRATEGY2INDEX_Base_Req,g_idThread);	
	CInterface_SvrTcp::getObj().SubscribePkg(CMD_STRTEGY_UPLOAD_File_Req,g_idThread);

	CInterface_SvrTcp::getObj().SubscribePkg(CMD_STRTEGY_DOWNLOAD_Base_Req,g_idThread);
	CInterface_SvrTcp::getObj().SubscribePkg(CMD_STRTEGY_DOWNLOAD_Req,g_idThread);	


	CInterface_SvrTcp::getObj().SubscribePkg(CMD_STRTEGY_UPLOADINDEX_Base_Req,g_idThread);
	CInterface_SvrTcp::getObj().SubscribePkg(CMD_STRTEGY_UPLOADINDEX_File_Req,g_idThread);

	CInterface_SvrTcp::getObj().SubscribePkg(CMD_STRTEGY_DOWNLOADINDEX_Base_Req,g_idThread);	
	CInterface_SvrTcp::getObj().SubscribePkg(CMD_STRTEGY_DOWNLOADINDEX_Req,g_idThread);

	CInterface_SvrTcp::getObj().SubscribePkg(CMD_STRTEGY_USE_Req,g_idThread);
	CInterface_SvrTcp::getObj().SubscribePkg(CMD_STRTEGY_DEL_Req,g_idThread);	


	CInterface_SvrTcp::getObj().SubscribePkg(CMD_INSTANCE_ADD_Req,g_idThread);
	CInterface_SvrTcp::getObj().SubscribePkg(CMD_INSTANCE_Modify_Req,g_idThread);
	CInterface_SvrTcp::getObj().SubscribePkg(CMD_INSTANCE_DEL_Req,g_idThread);	
	CInterface_SvrTcp::getObj().SubscribePkg(CMD_INSTANCE_USE_Req,g_idThread);

	CInterface_SvrTcp::getObj().SubscribePkg(CMD_STRTEGY_DOWNLOAD_ALLINDEX_Base_Req,g_idThread);	
	CInterface_SvrTcp::getObj().SubscribePkg(CMD_STRTEGY_DOWNLOAD_All_Base_Req,g_idThread);
	CInterface_SvrTcp::getObj().SubscribePkg(CMD_INSTANCE_AllDownlaod_Req,g_idThread);	
	CInterface_SvrTcp::getObj().SubscribePkg(CMD_UPLOAD_START_Req,g_idThread);	
	CInterface_SvrTcp::getObj().SubscribePkg(CMD_UPLOAD_END_Req,g_idThread);	
	CInterface_SvrTcp::getObj().SubscribePkg(CMDID_NotifyStartSettlementToSlave_Push,g_idThread);	
	CInterface_SvrTcp::getObj().SubscribePkg(CMDID_TcpClientDisconn,g_idThread);	
//	CInterface_SvrTcp::getObj().SubscribePkg(CMDID_TcpClientConnect,g_idThread);	

	CInterface_SvrTcp::getObj().SubscribePkg(CMD_QUERY_COMMISSION_Req,g_idThread);	
	CInterface_SvrTcp::getObj().SubscribePkg(CMD_QUERY_MARGINRATE_Req,g_idThread);	
	

	CInterface_SvrNotifyAndAsk::getObj().SubscribeNotifyEvent(EventTypeUserLogin, g_idThread);
	CInterface_SvrNotifyAndAsk::getObj().SubscribeNotifyEvent(EventTypeInitFinish, g_idThread);
	CInterface_SvrNotifyAndAsk::getObj().SubscribeNotifyEvent(EventTypeUserDiscon, g_idThread);	
	CInterface_SvrNotifyAndAsk::getObj().SubscribeNotifyEvent(EventTypeUserLogoff, g_idThread);


	
}

//模块结束，释放资源，关闭工作线程
SVRSTRATEGY_API void ReleaseFunc(void)
{
    g_manage.StopAllInstance();		
    Strategy_LOGINFO("服务正常退出，停止运行所有策略方案\n");

	if(g_hThread)
	{
		//退订数据包
		CInterface_SvrTcp::getObj().UnsubscribeAllPkg(g_idThread);

		//发送WM_QUIT通知线程结束
		PostThreadMessage(g_idThread,WM_QUIT,0,0);

		//等待线程退出
		DWORD ExitCode;
		WaitForSingleObject((HANDLE)g_hThread,8000);
		if(GetExitCodeThread((HANDLE)g_hThread,&ExitCode)!=0&&ExitCode==STILL_ACTIVE)
			TerminateThread((HANDLE)g_hThread,0);
		CloseHandle(g_hThread);
		g_hThread=NULL;
		g_idThread=0;
	}
}

void DealPakage(unsigned int EventParamID,int& PkgLen,int& hSocket)
{
	AllocTmpStackBuf(TmpPkg,PkgLen,10*1024);
	if(TmpPkg.m_pbuf&&
		CInterface_SvrTcp::getObj().getEventParamObj().GetEventParam(EventParamID,NULL,NULL,TmpPkg.m_pbuf,&PkgLen,PkgLen,&hSocket)&&
		Stru_UniPkgHead::IsValidPkg(TmpPkg.m_pbuf,PkgLen))
	{
		Stru_UniPkgHead& PkgHead=*((Stru_UniPkgHead*)TmpPkg.m_pbuf);
		void* pPkgData=(char*)TmpPkg.m_pbuf+sizeof(Stru_UniPkgHead);

		//调用数据包处理函数处理数据包
		ProcessOneUniPkg_InThread(hSocket,PkgHead,pPkgData);	
	}
}
//工作线程
DWORD ThreadWorker(void *arg)
{
	//In the thread to which the message will be posted, call PeekMessage as shown here to force the system to create the message queue.
	MSG MsgTemp;
	PeekMessage(&MsgTemp, NULL, WM_USER, WM_USER, PM_NOREMOVE);//调用这个函数用于建立消息队列

	MSG Msg;
	while(GetMessage(&Msg,NULL,0,0))
	{		
		if(Msg.message==WM_COMMAND&&Msg.wParam==WndCmd_YourPkgArrival2)
		{
			//数据包的传输ID
			unsigned int EventParamID=(unsigned int)Msg.lParam;
			//数据包长度
			int PkgLen;
			//Socket句柄
			int hSocket;
			if(CInterface_SvrTcp::getObj().getEventParamObj().GetEventParam(EventParamID,NULL,NULL,NULL,&PkgLen,0,NULL))
			{
				//申请临时空间并获取数据包
				//AllocTmpStackBuf(TmpPkg,PkgLen,10*1024);
				//if(TmpPkg.m_pbuf&&
				//	CInterface_SvrTcp::getObj().getEventParamObj().GetEventParam(EventParamID,NULL,NULL,TmpPkg.m_pbuf,&PkgLen,PkgLen,&hSocket)&&
				//	Stru_UniPkgHead::IsValidPkg(TmpPkg.m_pbuf,PkgLen))
				//{
				//	Stru_UniPkgHead& PkgHead=*((Stru_UniPkgHead*)TmpPkg.m_pbuf);
				//	void* pPkgData=(char*)TmpPkg.m_pbuf+sizeof(Stru_UniPkgHead);

				//	//调用数据包处理函数处理数据包
				//	ProcessOneUniPkg_InThread(hSocket,PkgHead,pPkgData);
				//}
				DealPakage(EventParamID,PkgLen,hSocket);

				//释放EventParam
				CInterface_SvrTcp::getObj().getEventParamObj().DeleteEventParam(EventParamID);
			}			

		}
		else if(Msg.message==WM_COMMAND&&Msg.wParam==WndCmd_NotifyEventArrival)		
		{
			//数据包的传输ID
			unsigned int EventParamID=(unsigned int)Msg.lParam;
			//数据包长度
			Stru_NotifyEvent ls;
			int nLen = sizeof(ls);
			if(CInterface_SvrTcp::getObj().getEventParamObj().GetEventParam(EventParamID,NULL,NULL,&ls,&nLen,nLen,NULL))
			{			
				//调用数据包处理函数处理数据包
				ParseDataChangedEvent(ls);			
				//释放EventParam
				CInterface_SvrTcp::getObj().getEventParamObj().DeleteEventParam(EventParamID);
			}
		}
	}
	return 0;
}

//处理一个SvrTcp推送过来的数据包
void ProcessOneUniPkg_InThread(int hSocket,const Stru_UniPkgHead& PkgHead,const void*pPkgData)
{
	//-----------------------------------------------------------------------------------
	//	下面根据数据包的命令字，处理数据包
	//-----------------------------------------------------------------------------------
	switch(PkgHead.cmdid)
	{
	case CMD_STRTEGY_UPLOAD_Base_Req:
		{		
			 SStrategy *pFP = (SStrategy*)pPkgData;
			 if(pFP == NULL)
				 return;
			int nErrorCode = CF_ERROR_SUCCESS;
		  	bool bSuccess = g_manage.UploadBaseStrategy(*pFP, nErrorCode);	
			if(bSuccess)
			{		
				char pData[54];
				memset(pData,0,sizeof(pData));
				int nIime = 1;
				memcpy((char*)pData,&nIime, sizeof(nIime));
				memcpy((char*)pData+4,pFP->strategyName,sizeof(pFP->strategyName));
				memcpy((char*)pData+25+4,pFP->strTraderName,sizeof(pFP->strTraderName));
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_STRTEGY_UPLOAD_Base_Rsp, 
					pData, sizeof(pData), PkgHead.seq,0, CF_ERROR_SUCCESS);	
			}
			else
			{
				const char * lErrorString = FormatErrorCode(nErrorCode);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_STRTEGY_UPLOAD_Base_Rsp, 
					(void*)lErrorString, 
					strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode);	
			}
		}

			break;
	case CMD_STRTEGY_UPLOAD_STRATEGY2INDEX_Base_Req:
		{
			bool bAdd = true;
			int nErrorCode = CF_ERROR_SUCCESS;
			vector<SStrategy2Index> vec;
			int pkgLen= PkgHead.len;
			int AccCnt=pkgLen/sizeof(SStrategy2Index);				
			for(int i =0; i< AccCnt; i++)
			{
				SStrategy2Index* pField = (SStrategy2Index*)((char*)(pPkgData) + i*sizeof(SStrategy2Index));
				vec.push_back(*pField);		
				bool bSuccess = g_manage.AddStrategyIndexRelation(pField->strategyName, pField->IndexName, nErrorCode);	
				if(!bSuccess)
				{
					bAdd = false;
					break;
				}
			}	
			if(vec.size() == 0)
			{
				const char * lErrorString = FormatErrorCode(CF_ERROR_COMMON_INPUT_PARAM);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_STRTEGY_UPLOAD_STRATEGY2INDEX_Base_Rsp, 
					(void*)lErrorString, strlen(lErrorString)+1, PkgHead.seq, 0, CF_ERROR_COMMON_INPUT_PARAM);
				return;
			}			
			
			if(bAdd)
			{				
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_STRTEGY_UPLOAD_STRATEGY2INDEX_Base_Rsp, 
					NULL, 0, PkgHead.seq,0, CF_ERROR_SUCCESS);	
			}
			else
			{
				const char * lErrorString = FormatErrorCode(nErrorCode);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_STRTEGY_UPLOAD_STRATEGY2INDEX_Base_Rsp, 
					(void*)lErrorString, 
					strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode);	
			}

		}
		break;
	case CMD_STRTEGY_UPLOAD_File_Req:
		{
			int nTime = *(int*)pPkgData;

			char Uploader[25];
			char Name[25];
			memcpy(Name, (char*)pPkgData +sizeof(int), 25 );
			memcpy(Uploader, (char*)pPkgData+25 +sizeof(int), 25);
			int nBeforeLength = 50 +sizeof(int);
			if(nTime == 2)
			{//第二次上传dll文件
				int nErrorCode = CF_ERROR_SUCCESS;
				bool bSuccess = g_manage.UploadDllFile(Name, Uploader, (char*)pPkgData+nBeforeLength, PkgHead.len -nBeforeLength,  nErrorCode);	
				if(bSuccess)
				{		
					char pData[54];
					memset(pData,0,sizeof(pData));				
					memcpy((char*)pData,&nTime, sizeof(nTime));
					memcpy((char*)pData+4,Name,sizeof(Name));
					memcpy((char*)pData+25+4,Uploader,sizeof(Uploader));

					CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_STRTEGY_UPLOAD_File_Rsp, 
						pData, sizeof(pData), PkgHead.seq,0, CF_ERROR_SUCCESS);	
				}
				else
				{
					const char * lErrorString = FormatErrorCode(nErrorCode);
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_STRTEGY_UPLOAD_File_Rsp, 
						(void*)lErrorString, 
						strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode);	
				}		
			}
			else if(nTime == 3)
			{//第三次上传zip源码文件
				int nErrorCode = CF_ERROR_SUCCESS;
				bool bSuccess = g_manage.UploadZIPFile(Name, Uploader, (char*)pPkgData+nBeforeLength, PkgHead.len -nBeforeLength,  nErrorCode);	
				if(bSuccess)
				{		
					char pData[54];
					memset(pData,0,sizeof(pData));				
					memcpy((char*)pData,&nTime, sizeof(nTime));
					memcpy((char*)pData+4,Name,sizeof(Name));
					memcpy((char*)pData+25+4,Uploader,sizeof(Uploader));
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_STRTEGY_UPLOAD_File_Rsp, 
						pData, sizeof(pData), PkgHead.seq,0, CF_ERROR_SUCCESS);	
				}
				else
				{
					const char * lErrorString = FormatErrorCode(nErrorCode);
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_STRTEGY_UPLOAD_File_Rsp, 
						(void*)lErrorString, 
						strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode);	
				}		

			}
		}
		break;
	case CMD_STRTEGY_DOWNLOAD_Base_Req:
		{
			
			char Name[25];
			memset(Name,0,sizeof(Name));
			memcpy(Name, (char*)pPkgData, 25 );			
			SStrategy strategy;
			int nErrorCode= CF_ERROR_SUCCESS;
			bool bSuccess = g_manage.DownloadBaseStrategy(Name, strategy,  nErrorCode);
			if(bSuccess)
			{			
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_STRTEGY_DOWNLOAD_Base_Rsp, 
					&strategy, sizeof(SStrategy), PkgHead.seq,0, CF_ERROR_SUCCESS);	
			}
			else
			{
				const char * lErrorString = FormatErrorCode(nErrorCode);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_STRTEGY_DOWNLOAD_Base_Rsp, 
					(void*)lErrorString, 
					strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode);	
			}		
		}
		break;
	case CMD_STRTEGY_DOWNLOAD_Req:
		{			
			int nTime = *(int*)pPkgData;
			char Uploader[25];
			char Name[25];
			memcpy(Name, (char*)pPkgData +sizeof(int), 25 );
			memcpy(Uploader, (char*)pPkgData+25 +sizeof(int), 25);
			int nBeforeLength = 50 +sizeof(int);
			if(nTime == 2)
			{
				void* pData=NULL;
				int nLength = 0;
				int nErrorCode= CF_ERROR_SUCCESS;
				bool bSuccess = g_manage.DownloadDllFile(Name, Uploader, pData, nLength,  nErrorCode);
				if(bSuccess)
				{
					memcpy((char*)pData,&nTime,sizeof(nTime));
					memcpy((char*)pData+4,&Name,sizeof(Name));
					memcpy((char*)pData+25+4,&Uploader,sizeof(Uploader));

					CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_STRTEGY_DOWNLOAD_Rsp, 
						pData, nLength, PkgHead.seq,0, CF_ERROR_SUCCESS);	
				}
				else
				{
					const char * lErrorString = FormatErrorCode(nErrorCode);
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_STRTEGY_DOWNLOAD_Rsp, 
						(void*)lErrorString, 
						strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode);	
				}		
			}
			else if(nTime == 3)
			{
				void* pData=NULL;
				int nLength = 0;
				int nErrorCode= CF_ERROR_SUCCESS;
				bool bSuccess = g_manage.DownloadZIPFile(Name, Uploader, pData, nLength,  nErrorCode);
				if(bSuccess)
				{
					memcpy((char*)pData,&nTime,sizeof(nTime));
					memcpy((char*)pData+4,&Name,sizeof(Name));
					memcpy((char*)pData+25+4,&Uploader,sizeof(Uploader));
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_STRTEGY_DOWNLOAD_Rsp, 
						pData, nLength, PkgHead.seq,0, CF_ERROR_SUCCESS);	
				}
				else
				{
					const char * lErrorString = FormatErrorCode(nErrorCode);
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_STRTEGY_DOWNLOAD_Rsp, 
						(void*)lErrorString, 
						strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode);	
				}		
			}
			break;
		}	
	case CMD_STRTEGY_UPLOADINDEX_Base_Req:
		{		
			SIndex *pFP = (SIndex*)pPkgData;
			if(pFP == NULL)
				return;
			int nErrorCode = CF_ERROR_SUCCESS;
			bool bSuccess = g_manage.UploadBaseIndex(*pFP, nErrorCode);	
			if(bSuccess)
			{	
				char pData[54];
				memset(pData,0,sizeof(pData));
				int nIime = 1;
				memcpy((char*)pData,&nIime, sizeof(nIime));
				memcpy((char*)pData+4,pFP->IndexName,sizeof(pFP->IndexName));
				memcpy((char*)pData+25+4,pFP->strTraderName,sizeof(pFP->strTraderName));

				CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_STRTEGY_UPLOADINDEX_Base_Rsp, 
					&pData, sizeof(pData), PkgHead.seq,0, CF_ERROR_SUCCESS);	
			}
			else
			{
				const char * lErrorString = FormatErrorCode(nErrorCode);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_STRTEGY_UPLOADINDEX_Base_Rsp, 
					(void*)lErrorString, 
					strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode);	
			}
			break;
		}
	case CMD_STRTEGY_UPLOADINDEX_File_Req:
		{//nTime :2 传 dll； 3传zip
			int nTime = *(int*)pPkgData;
			char Uploader[25];
			char Name[25];
			memcpy(Name, (char*)pPkgData +sizeof(int), 25 );
			memcpy(Uploader, (char*)pPkgData+25 +sizeof(int), 25);
			int nBeforeLength = 50 +sizeof(int);
		//	if(nTime == 2)
			{
				int nErrorCode= CF_ERROR_SUCCESS;
				bool bSuccess = g_manage.UploadIndexFile(Name, Uploader, nTime, (char*)pPkgData+nBeforeLength, PkgHead.len -nBeforeLength,  nErrorCode);
				if(bSuccess)
				{
					char pData[54];			
					memset(pData,0,sizeof(pData));
					memcpy((char*)pData,&nTime, sizeof(nTime));
					memcpy((char*)pData+4,Name,sizeof(Name));
					memcpy((char*)pData+25+4,Uploader,sizeof(Uploader));

					CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_STRTEGY_UPLOADINDEX_File_Rsp, 
						&pData, sizeof(pData), PkgHead.seq,0, CF_ERROR_SUCCESS);	
				}
				else
				{
					const char * lErrorString = FormatErrorCode(nErrorCode);
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_STRTEGY_UPLOADINDEX_File_Rsp, 
						(void*)lErrorString, 
						strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode);	
				}		
			}

		}
		break;
	case CMD_STRTEGY_DOWNLOADINDEX_Base_Req:
		{
			char Name[25];
			memset(Name,0,sizeof(Name));
			memcpy(Name, (char*)pPkgData, 25 );		

			SIndex Index;
			int nErrorCode= CF_ERROR_SUCCESS;
			bool bSuccess = g_manage.DownloadBaseIndex(Name, Index,  nErrorCode);
			if(bSuccess)
			{			
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_STRTEGY_DOWNLOADINDEX_Base_Rsp, 
					&Index, sizeof(SIndex), PkgHead.seq,0, CF_ERROR_SUCCESS);	
			}
			else
			{
				const char * lErrorString = FormatErrorCode(nErrorCode);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_STRTEGY_DOWNLOADINDEX_Base_Rsp, 
					(void*)lErrorString, 
					strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode);	
			}		
		}
		break;
	case CMD_STRTEGY_DOWNLOADINDEX_Req:
		{
			int nTime = *(int*)pPkgData;
			char Uploader[25];
			char Name[25];
			memcpy(Name, (char*)pPkgData +sizeof(int), 25 );
			memcpy(Uploader, (char*)pPkgData+25 +sizeof(int), 25);
			int nBeforeLength = 50 +sizeof(int);
		//	if(nTime == 2)
			{
				void* pData=NULL;
				int nLength = 0;
				int nErrorCode= CF_ERROR_SUCCESS;
				bool bSuccess = g_manage.DownloadIndexFile(Name, Uploader, nTime, pData, nLength,  nErrorCode);
				if(bSuccess)
				{
					memcpy((char*)pData,&nTime,sizeof(nTime));
					memcpy((char*)pData+4,&Name,sizeof(Name));
					memcpy((char*)pData+25+4,&Uploader,sizeof(Uploader));

					CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_STRTEGY_DOWNLOADINDEX_Rsp, 
						pData, nLength, PkgHead.seq,0, CF_ERROR_SUCCESS);	
				}
				else
				{
					const char * lErrorString = FormatErrorCode(nErrorCode);
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_STRTEGY_DOWNLOADINDEX_Rsp, 
						(void*)lErrorString, 
						strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode);	
				}		
			}

		}
		break;
	case CMD_STRTEGY_USE_Req:
		{//停用启用策略名
			SUseStrategy *pFP = (SUseStrategy*)pPkgData;
			if(pFP == NULL)
				return;

			int nErrorCode= CF_ERROR_SUCCESS;
			bool bSuccess = g_manage.UseStrategy(*pFP,  nErrorCode);
			if(bSuccess)
			{
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_STRTEGY_USE_Rsp, 
					NULL, 0, PkgHead.seq,0, CF_ERROR_SUCCESS);	
			}
			else
			{
				const char * lErrorString = FormatErrorCode(nErrorCode);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_STRTEGY_USE_Rsp, 
					(void*)lErrorString, 
					strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode);	
			}		

			break;
		}
	case CMD_STRTEGY_DEL_Req:
		{
			SDelStrategy *pFP = (SDelStrategy*)pPkgData;
			if(pFP == NULL)
				return;

			int nErrorCode= CF_ERROR_SUCCESS;
			bool bSuccess = g_manage.DelStrategy(*pFP,  nErrorCode);
			if(bSuccess)
			{
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_STRTEGY_DEL_Rsp, 
					NULL, 0, PkgHead.seq,0, CF_ERROR_SUCCESS);	
			}
			else
			{
				const char * lErrorString = FormatErrorCode(nErrorCode);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_STRTEGY_DEL_Rsp, 
					(void*)lErrorString, 
					strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode);	
			}		

		}
		break;
	case CMD_INSTANCE_ADD_Req:
		{
			SStrategyInstance *pFP = (SStrategyInstance*)pPkgData;
			if(pFP == NULL)
				return;

			int nErrorCode= CF_ERROR_SUCCESS;
			bool bSuccess = g_manage.AddInstance(*pFP, nErrorCode);
			if(bSuccess)
			{
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_INSTANCE_ADD_Rsp, 
					NULL, 0, PkgHead.seq,0, CF_ERROR_SUCCESS);	
			}
			else
			{
				const char * lErrorString = FormatErrorCode(nErrorCode);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_INSTANCE_ADD_Rsp, 
					(void*)lErrorString, 
					strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode);	
			}		

		}
		break;
	case CMD_INSTANCE_Modify_Req:
		{
			SStrategyInstance *pFP = (SStrategyInstance*)pPkgData;
			if(pFP == NULL)
				return;

			int nErrorCode= CF_ERROR_SUCCESS;
			bool bSuccess = g_manage.AddInstance(*pFP, nErrorCode);
			if(bSuccess)
			{
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_INSTANCE_Modify_Rsp, 
					NULL, 0, PkgHead.seq,0, CF_ERROR_SUCCESS);	
			}
			else
			{
				const char * lErrorString = FormatErrorCode(nErrorCode);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_INSTANCE_Modify_Rsp, 
					(void*)lErrorString, 
					strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode);	
			}		
		}
		break;
	case CMD_INSTANCE_DEL_Req:
		{
			SDelStrategy *pFP = (SDelStrategy*)pPkgData;
			if(pFP == NULL)
				return;

			int nErrorCode= CF_ERROR_SUCCESS;
			bool bSuccess = g_manage.DelInstance_ByTrader(*pFP, nErrorCode);
			if(bSuccess)
			{			
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_INSTANCE_DEL_Rsp, 
					NULL, 0, PkgHead.seq,0, CF_ERROR_SUCCESS);	
			}
			else
			{
				const char * lErrorString = FormatErrorCode(nErrorCode);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_INSTANCE_DEL_Rsp, 
					(void*)lErrorString, 
					strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode);	
			}	
		}
		break;
	case CMD_INSTANCE_USE_Req:
		{//可以理解成为加载策略方案
			SUseStrategy *pFP = (SUseStrategy*)pPkgData;
			if(pFP == NULL)
				return;

			int nErrorCode= CF_ERROR_SUCCESS;
			bool bSuccess = g_manage.UseInstance(*pFP,  nErrorCode);
			if(bSuccess)
			{
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_INSTANCE_USE_Rsp, 
					NULL, 0, PkgHead.seq,0, CF_ERROR_SUCCESS);	
			}
			else
			{
				const char * lErrorString = FormatErrorCode(nErrorCode);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_INSTANCE_USE_Rsp, 
					(void*)lErrorString, 
					strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode);	
			}		

			break;
		}
		break;
	case CMD_STRTEGY_DOWNLOAD_ALLINDEX_Base_Req:
		{				
			char Uploader[25];
			memset(Uploader,0,sizeof(Uploader));
			if(pPkgData)
				memcpy(Uploader, (char*)pPkgData, 25);

			std::vector<SIndex> vecIndex;
			int nErrorCode= CF_ERROR_SUCCESS;
			bool bSuccess = g_manage.DownloadAllBaseIndex(Uploader, vecIndex,  nErrorCode);
			if(bSuccess)
			{		
				if(vecIndex.size())
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_STRTEGY_DOWNLOAD_ALLINDEX_Base_Rsp, 
						&vecIndex[0], sizeof(SIndex)*vecIndex.size(), PkgHead.seq,0, CF_ERROR_SUCCESS);	
				else
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_STRTEGY_DOWNLOAD_ALLINDEX_Base_Rsp, 
					NULL, 0, PkgHead.seq,0, CF_ERROR_SUCCESS);	
			}
			else
			{
				const char * lErrorString = FormatErrorCode(nErrorCode);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_STRTEGY_DOWNLOAD_ALLINDEX_Base_Rsp, 
					(void*)lErrorString, 
					strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode);	
			}		

		}
		break;
	case CMD_STRTEGY_DOWNLOAD_All_Base_Req:
		{
			char Uploader[25];
			memset(Uploader,0,sizeof(Uploader));
			if(pPkgData)
				memcpy(Uploader, (char*)pPkgData, 25);

			std::vector<SStrategy> vecStrategy;
			int nErrorCode= CF_ERROR_SUCCESS;
			bool bSuccess = g_manage.DownloadAllBaseStategy(Uploader, vecStrategy,  nErrorCode);
			if(bSuccess)
			{		
				if(vecStrategy.size())
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_STRTEGY_DOWNLOAD_All_Base_Rsp, 
					&vecStrategy[0], sizeof(SStrategy)*vecStrategy.size(), PkgHead.seq,0, CF_ERROR_SUCCESS);	
				else
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_STRTEGY_DOWNLOAD_All_Base_Rsp, 
					NULL, 0, PkgHead.seq,0, CF_ERROR_SUCCESS);	
			}
			else
			{
				const char * lErrorString = FormatErrorCode(nErrorCode);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_STRTEGY_DOWNLOAD_All_Base_Rsp, 
					(void*)lErrorString, 
					strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode);	
			}		
		}
		break;
	case CMD_INSTANCE_AllDownlaod_Req:
		{
			char Uploader[25];
			memset(Uploader,0,sizeof(Uploader));
			memcpy(Uploader, (char*)pPkgData, 25);

			std::vector<SStrategyInstance> vecStrategy;
			int nErrorCode= CF_ERROR_SUCCESS;
			bool bSuccess = g_manage.DownloadAllInstance(Uploader, vecStrategy,  nErrorCode);
			if(bSuccess)
			{		
				if(vecStrategy.size())
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_INSTANCE_AllDownlaod_Rsp, 
					&vecStrategy[0], sizeof(SStrategyInstance)*vecStrategy.size(), PkgHead.seq,0, CF_ERROR_SUCCESS);	
				else
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_INSTANCE_AllDownlaod_Rsp, 
					NULL, 0, PkgHead.seq,0, CF_ERROR_SUCCESS);	
			}
			else
			{
				const char * lErrorString = FormatErrorCode(nErrorCode);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_INSTANCE_AllDownlaod_Rsp, 
					(void*)lErrorString, 
					strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode);	
			}		
		}
		break;
	case CMD_UPLOAD_START_Req:
		{
			UploadStart *p = (UploadStart*)pPkgData;
			if(p == NULL)
				return;
			
			int nErrorCode= CF_ERROR_SUCCESS;
			std::string strError = "";
			bool bSuccess = g_manage.UploadStartAll(*p,  nErrorCode, strError);
			if(bSuccess)
			{
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_UPLOAD_START_Rsp, 
					NULL, 0, PkgHead.seq,0, CF_ERROR_SUCCESS);	
			}
			else
			{
				if(strError.length()>1)
				{
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_UPLOAD_START_Rsp, 
						(void*)strError.c_str(), strlen(strError.c_str())+1, PkgHead.seq,0,nErrorCode);	
				}
				else
				{
					const char * lErrorString = FormatErrorCode(nErrorCode);					
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_UPLOAD_START_Rsp, 
						(void*)lErrorString, strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode);	
				}
			}		
			break;
		}
	case CMD_UPLOAD_END_Req:
		{
			UploadEnd *p = (UploadEnd*)pPkgData;
			if(p == NULL)
				return;

			int nErrorCode= CF_ERROR_SUCCESS;
			bool bSuccess = g_manage.UploadEndAll(*p,  nErrorCode);
			if(bSuccess)
			{
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_UPLOAD_END_Rsp, 
					NULL, 0, PkgHead.seq,0, CF_ERROR_SUCCESS);	
			}
			else
			{
				const char * lErrorString = FormatErrorCode(nErrorCode);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_UPLOAD_END_Rsp, 
					(void*)lErrorString, strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode);	
			}		
			break;
		}
	case CMDID_NotifyStartSettlementToSlave_Push:
		{//开始结算的时候停止所有策略实例
			g_manage.StopAllInstance();		
			Strategy_LOGINFO("开始结算，停止运行所有策略方案\n");
		}
		break;
	case  CMDID_TcpClientDisconn:
		{//socket disconnect,so discrible all orders		
			//g_manage.StopAllInstance();
            //不用停用（因为主从断开后就没有行情驱动去下单了），只是关闭启动的定时器
            //如果主从服务都在一台机器上，登陆主服务的交易客户端也会断开连接
            g_disConnect = true;
            g_manage.HandleAllInstanceTimer(true);		
			
			Strategy_LOGINFO("主从断开，停止运行所有策略方案计时器\n");
		}
		break;
// 	case CMDID_TcpClientConnect:
// 		{
// 			if(g_disConnect && g_bEventTypeInitFinish)//如果断开过，则重连后并且接收到服务器初始化完成的事件后需要重新启动所有的策略方案	
//                 g_manage.HandleAllInstanceTimer(false);	
//             else
//                 g_manage.Init();
// 			
// 			Strategy_LOGINFO("主从重新连上，开始运行所有策略方案计时器\n");
// 		}
// 		break;
/*	case CMD_QUERY_COMMISSION_Req:
		{
			QueryCommission *p = (QueryCommission*)pPkgData;
			if(p == NULL)
				return;
			std::string strBroker, strUser;
			bool bGet = GetTradeAccountByUserID(p->InvestorID, strBroker, strUser);
			if(!bGet)
			{
				const char * lErrorString = FormatErrorCode(CF_ERROR_NO_ACCOUNT_FAIL);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_QUERY_COMMISSION_Rsp, 
					(void*)lErrorString, strlen(lErrorString)+1, PkgHead.seq,0,CF_ERROR_NO_ACCOUNT_FAIL);	
				return;
			}
			PlatformStru_InstrumentCommissionRate outData;
			if(CF_ERROR_SUCCESS != CInterface_SvrTradeData::getObj().GetCommissionRate(strBroker, strUser, p->InstrumentID, outData))
			{
				const char * lErrorString = FormatErrorCode(CF_ERROR_NO_ACCOUNT_FAIL);
				char Buf[1024];
				memset(Buf,0,sizeof(Buf));
				sprintf(Buf, lErrorString, p->InstrumentID);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_QUERY_COMMISSION_Rsp, 
					(void*)Buf, strlen(Buf)+1, PkgHead.seq,0,CF_ERROR_GET_COMMISSION_FAIL);	
			}
			else
			{
				strcpy(outData.InvestorID, p->InvestorID);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_QUERY_COMMISSION_Rsp, 
					&outData, sizeof(PlatformStru_InstrumentCommissionRate), PkgHead.seq,0, CF_ERROR_SUCCESS);	
			}
		}
		break;
	case CMD_QUERY_MARGINRATE_Req:
		{
			QueryMarginRate *p = (QueryMarginRate*)pPkgData;
			if(p == NULL)
				return;
			std::string strBroker, strUser;
			bool bGet = GetTradeAccountByUserID(p->InvestorID, strBroker, strUser);
			if(!bGet)
			{
				const char * lErrorString = FormatErrorCode(CF_ERROR_NO_ACCOUNT_FAIL);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_QUERY_MARGINRATE_Rsp, 
					(void*)lErrorString, strlen(lErrorString)+1, PkgHead.seq,0,CF_ERROR_NO_ACCOUNT_FAIL);	
				return;
			}
			PlatformStru_InstrumentMarginRate outData;
			if(CF_ERROR_SUCCESS != CInterface_SvrTradeData::getObj().GetMarginRate(strBroker, strUser, p->InstrumentID, outData))
			{
				const char * lErrorString = FormatErrorCode(CF_ERROR_GET_MARGINATE_FAIL);							
				char Buf[1024];
				memset(Buf,0,sizeof(Buf));
				sprintf(Buf, lErrorString, p->InstrumentID);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_QUERY_MARGINRATE_Rsp, 
					(void*)Buf, strlen(Buf)+1, PkgHead.seq,0,CF_ERROR_GET_MARGINATE_FAIL);	
			}
			else
			{
				strcpy(outData.InvestorID, p->InvestorID);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, CMD_QUERY_MARGINRATE_Rsp, 
					&outData, sizeof(PlatformStru_InstrumentMarginRate), PkgHead.seq,0, CF_ERROR_SUCCESS);	
			}
		}
		break;*/
	}

}
// 解析一个数据通知事件
bool ParseDataChangedEvent(const Stru_NotifyEvent& dataEvt)
{
	switch(dataEvt.meEventType)
	{
		case EventTypeUserLogin:
			{
				int nUserID = dataEvt.mnDataID;
				UserInfo lUserInfo;
				bool bGet = CInterface_SvrUserOrg::getObj().GetUserByID(nUserID,lUserInfo);
				if(!bGet)
					return 0;
				//如果用户的登陆客户端>0
				sUserOnlineInfo lsInfo;
				CInterface_SvrLogin::getObj().GetOnlineUserStatus(nUserID,lsInfo);
				if(lsInfo.mUserOnlineCount ==1 && nUserID != SLAVE_USER_ID)
				{
					g_manage.OnOffLineProcessInstance(lUserInfo.szAccount, true);//如果离线不运行，上线后运行（运行状态）                   
					Strategy_LOGINFO("交易员上线，目前总共还有%d个%s交易员在线，运行相关策略", lsInfo.mUserOnlineCount, lUserInfo.szAccount );
				}
				else
				{
					Strategy_LOGINFO("交易员上线，目前总共还有%d个%s交易员在线，运行相关策略", lsInfo.mUserOnlineCount, lUserInfo.szAccount );
				}

				break;;
			}
 		case EventTypeInitFinish:
 			{
                //如果断开过，则重连后并且接收到服务器初始化完成的事件后需要重新启动所有的策略方案
                if(g_disConnect)
                {
                    std::vector<sUserOnlineInfo> nvecInfo;
                    CInterface_SvrLogin::getObj().GetAllOnlineUserStatus(nvecInfo);

                    bool bHaveTraderLogin = false;
                    for (std::vector<sUserOnlineInfo>::iterator it = nvecInfo.begin();it!=nvecInfo.end();it++)
                    {
                        Strategy_LOGINFO("主从重新连上前的在线用户 分别是mUserID = %d，mUserOnlineCount = %d\n",it->mUserID,it->mUserOnlineCount);

                        if(it->mUserID != SLAVE_USER_ID) 
                            bHaveTraderLogin = true;
                    }
                    
                    if(bHaveTraderLogin)
                    {
                        g_manage.HandleAllInstanceTimer(false);
                        Strategy_LOGINFO("主从重新连上，开始运行所有策略方案计时器\n");
                    }
                }
                else
                {
                    g_manage.Init();
                    Strategy_LOGINFO("收到服务器启动初始化完成的事件，开始运行所有策略方案 \n");
                }  
 			}
 			break;
		case EventTypeUserDiscon:
			{
				int nUserID = dataEvt.mnDataID;
				UserInfo lUserInfo;
				if(CInterface_SvrUserOrg::getObj().GetUserByID(nUserID,lUserInfo) == false)
					return 0;

				//如果用户的登陆客户端为，则推送通知离线
				sUserOnlineInfo lsInfo;
				CInterface_SvrLogin::getObj().GetOnlineUserStatus(nUserID,lsInfo);
				if(lsInfo.mUserOnlineCount == 0 && nUserID != SLAVE_USER_ID)
				{
					g_manage.OnOffLineProcessInstance(lUserInfo.szAccount, false);
					Strategy_LOGINFO("交易员下线，目前总共还有%d个%s交易员在线，停止相关策略\n", lsInfo.mUserOnlineCount, lUserInfo.szAccount );
				}
				else
				{
					Strategy_LOGINFO("交易员下线，目前总共还有%d个%s交易员在线，不能停止相关策略\n", lsInfo.mUserOnlineCount, lUserInfo.szAccount );
				}

				break;
			}
		case EventTypeUserLogoff:
			{
				int nUserID = dataEvt.mnDataID;
				UserInfo lUserInfo;
				if(CInterface_SvrUserOrg::getObj().GetUserByID(nUserID,lUserInfo) == false)
					return 0;

				//如果用户的登陆客户端为，则推送通知离线
				sUserOnlineInfo lsInfo;
				CInterface_SvrLogin::getObj().GetOnlineUserStatus(nUserID,lsInfo);
				if(lsInfo.mUserOnlineCount == 0 && nUserID != SLAVE_USER_ID)
				{
					g_manage.OnOffLineProcessInstance(lUserInfo.szAccount, false);
					Strategy_LOGINFO("交易员登出，目前总共还有%d个%s交易员在线，停止相关策略\n", lsInfo.mUserOnlineCount, lUserInfo.szAccount );
				}
				else
				{
					Strategy_LOGINFO("交易员登出，目前总共还有%d个%s交易员在线，不能停止相关策略\n", lsInfo.mUserOnlineCount, lUserInfo.szAccount );
				}

				break;
			}
	//	case CMDID_NotifyStartSettlementToSlave_Push:
	//		{//开始结算的时候停止所有策略实例
	//			g_manage.StopAllInstance();
	//			break;
	//		}
	}
	

	return true;
}
bool GetTradeAccountByUserID(std::string strTraderID, std::string& strBrokerID, std::string& strInvestID)
{//根据交易员账号得到委托交易账号

	UserInfo userInfo;
	std::string strAcc;
	bool bFind = CInterface_SvrUserOrg::getObj().GetUserByName(strTraderID, userInfo);    
	if(!bFind)
		return false;


	int nUserID = userInfo.nUserID;	
	std::vector<UserAndTradeAccountRelation> vec;
	CInterface_SvrUserOrg::getObj().GetUserAndTradeAccountRelationByUserID(nUserID, vec);
	if(vec.size() == 0)
		return false;

	TradeAccount account;
	int nAccountID = vec[0].nTradeAccountID;
	if(!CInterface_SvrBrokerInfo::getObj().GetTradeAccount(nAccountID, account ))
		return false;	

	strInvestID = account.szTradeAccount;

	BrokerInfo brokerInfo;
	if(!CInterface_SvrBrokerInfo::getObj().GetBrokerInfoByID(account.nBrokerID, brokerInfo))
		return false;
	strBrokerID = brokerInfo.szBrokerCode;
	return true;
}