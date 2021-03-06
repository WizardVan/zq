// testtcpsvr.cpp : Defines the exported functions for the DLL application.
//

#pragma warning(disable : 4996)
#pragma warning(disable : 4786)
#include "stdafx.h"
#include <windows.h>
#include "SvrRisk.h"
#include "CommonPkg.h"
#include "CommonDef.h"
#include "EventParam.h"
#include "tmpstackbuf.h"
#include "..\\SvrTcp\\Interface_SvrTcp.h"
#include "Interface_SvrRisk.h"
#include "DataCenter.h"
#include "RiskMsgCalc.h"
#include "RiskMsgCalc_Account.h"
#include "ThreadSharedData.h"
#include "RiskMsgQueue.h"
#include "RiskMgr.h"

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



#define WRITELOGID_SvrRisk

//bool LoadXmlRiskConfig(std::string strConfig);

bool g_bSendToExcute;//通过网络发送给交易执行命令
int  g_hSocket;      //主服务器套接字
//////////////////////////////////////////////////////////////////////////
std::map<int, int> m_mapSeqID2Socket;
CReadWriteLock				m_Mutex_SeqRequestID;
int g_ExcuteID = 1;
int InsertmapSeqID2Socket(int hSocket)
{	
	g_ExcuteID++;
	m_Mutex_SeqRequestID.write_lock();
	m_mapSeqID2Socket.insert(make_pair(g_ExcuteID, hSocket));
	m_Mutex_SeqRequestID.write_unlock();

	return g_ExcuteID;
}
void DeletemapSeqID2Socket(int nQuotServerSeqID)
{
	m_Mutex_SeqRequestID.write_lock();
	m_mapSeqID2Socket.erase(nQuotServerSeqID);
	m_Mutex_SeqRequestID.write_unlock();
}
bool GetmapSeqID2Socket(int nQuotServerSeqID, int& hSocket)
{
	bool bGet= false;
	m_Mutex_SeqRequestID.write_lock();
	std::map<int, int>::iterator it = m_mapSeqID2Socket.find(nQuotServerSeqID);
	if(it != m_mapSeqID2Socket.end())
	{
		hSocket = it->second;
		bGet = true;
	}		
	m_Mutex_SeqRequestID.write_unlock();

	return bGet;
}
//////////////////////////////////////////////////////////////////////////

CRiskMsgCalc*		  m_RiskMsgCalc;//针对子账号风控计算
CRiskMsgCalc_Account* m_RiskMsgCalc_Account;//针对主账号风控计算
CThreadSharedData*    m_pThreadSharedData;

bool GetTradeAccountByUserID(std::string strTraderID, std::string& strBrokerID, std::string& strInvestID);
void DoAccountRisk(std::string strInvestor);
//全局互斥锁
Ceasymutex			g_mutex;

//线程参数
HANDLE				g_hThread=NULL;
DWORD				g_idThread=0;
DWORD ThreadWorker(void *arg);

//线程处理风控客户端请求
HANDLE				g_hThreadRequest  = NULL;
DWORD				g_idThreadRequest = 0;
DWORD				ThreadWorkerRequest(void *arg);

//存储线程
HANDLE				g_hThreadSave=NULL;
DWORD				g_idThreadSave=0;
DWORD ThreadSave(void *arg);
bool g_bThreadSaveExit = false;

//处理一个接收到的数据包
void ProcessOneUniPkg_InThread(int hSocket,const Stru_UniPkgHead& PkgHead,const void*pPkgData);
bool ParseDataChangedEvent(Stru_NotifyEvent& dataEvt);

void SendMessage( std::vector<MsgSendStatus> &vForeAccount, MessageInfo msgInfo );

void SubscribeAllCmdMsgID();
void ProcessConfig(int hSocket,const Stru_UniPkgHead& PkgHead,const void*pPkgData);
void ProcessRiskIndicatorRequest(int hSocket,const Stru_UniPkgHead& PkgHead,const void*pPkgData);
void ProcessRiskEventRequest(int hSocket,const Stru_UniPkgHead& PkgHead,const void*pPkgData);
//void ProcessMessageRequest(int hSocket,const Stru_UniPkgHead& PkgHead,const void*pPkgData);
void ProcessQuotRequest(int hSocket,const Stru_UniPkgHead& PkgHead,const void*pPkgData);
void ProcessTradeRequest(int hSocket,const Stru_UniPkgHead& PkgHead,const void*pPkgData);
void ProcessFundRequest(int hSocket,const Stru_UniPkgHead& PkgHead,const void*pPkgData);
void ProcessPositionRequest(int hSocket,const Stru_UniPkgHead& PkgHead,const void*pPkgData);
void ProcessOrderRequest(int hSocket,const Stru_UniPkgHead& PkgHead,const void*pPkgData);
void ProcessNetValueRequest(int hSocket,const Stru_UniPkgHead& PkgHead,const void*pPkgData);
void ProcessForceCloseRequest(int hSocket,const Stru_UniPkgHead& PkgHead,const void*pPkgData);
void ProcessRisk2ExcuteRequest(int hSocket,const Stru_UniPkgHead& PkgHead,const void*pPkgData);
//管理终端查询
void ProcessAdminQuery(int hSocket,const Stru_UniPkgHead& PkgHead,const void*pPkgData);

void PassVerifyOrder(int nUserID);
//模块初始化，创建工作线程并向SvrTcp订阅感兴趣的数据包
SVRRISK_API void InitFunc(void)
{
	g_bSendToExcute = true;//LoadXmlRiskConfig("RiskConfig.xml");
	InterlockedExchange((volatile long*)(&g_hSocket),0);

	CForceCloseMsgQueue::Init();
	CDataCenter::Init();
	CRiskMgr::Init();

	m_pThreadSharedData = new CThreadSharedData();
	m_RiskMsgCalc=new CRiskMsgCalc(m_pThreadSharedData);
	m_RiskMsgCalc_Account = new CRiskMsgCalc_Account(m_pThreadSharedData);
	//创建工作线程
	g_hThread=CreateThread(NULL,10*1024*1024,(LPTHREAD_START_ROUTINE)ThreadWorker,0,0,&g_idThread);
	
	//这个线程主要是针对风控客户端的请求的
	g_hThreadRequest=CreateThread(NULL,10*1024*1024,(LPTHREAD_START_ROUTINE)ThreadWorkerRequest,0,0,&g_idThreadRequest);

	//保存数据线程
	g_hThreadSave = CreateThread(NULL,10*1024*1024,(LPTHREAD_START_ROUTINE)ThreadSave,0,0,&g_idThreadSave);
	//下面订阅本线程感兴趣的数据包
	//CInterface_SvrTcp::getObj().SubscribePkg(CMDID_HEARTBEAT,g_idThread);

	//SubscribeAllCmdMsgID();
	/*
	CInterface_SvrNotifyAndAsk::getObj().SubscribeNotifyEvent(EventTypeInitFinish, g_idThread);
	CInterface_SvrNotifyAndAsk::getObj().SubscribeNotifyEvent(EventTypeStartInit, g_idThread);
	CInterface_SvrNotifyAndAsk::getObj().SubscribeNotifyEvent(EventTypeFund, g_idThread);
	CInterface_SvrNotifyAndAsk::getObj().SubscribeNotifyEvent(EventTypePosition, g_idThread);
	CInterface_SvrNotifyAndAsk::getObj().SubscribeNotifyEvent(EventTypeOrderRtn, g_idThread);
	CInterface_SvrNotifyAndAsk::getObj().SubscribeNotifyEvent(EventTypeTradeRtn, g_idThread);
	CInterface_SvrNotifyAndAsk::getObj().SubscribeNotifyEvent(EventTypeUserLimitAndVerifyUpdate, g_idThread);
	*/
}

//模块结束，释放资源，关闭工作线程
SVRRISK_API void ReleaseFunc(void)
{
	g_bThreadSaveExit = true;
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
	if(g_hThreadRequest)
	{
		//退订数据包
		CInterface_SvrTcp::getObj().UnsubscribeAllPkg(g_idThreadRequest);

		//发送WM_QUIT通知线程结束
		PostThreadMessage(g_idThreadRequest,WM_QUIT,0,0);

		//等待线程退出
		DWORD ExitCode;
		WaitForSingleObject((HANDLE)g_hThreadRequest,8000);
		if(GetExitCodeThread((HANDLE)g_hThreadRequest,&ExitCode)!=0&&ExitCode==STILL_ACTIVE)
			TerminateThread((HANDLE)g_hThreadRequest,0);
		CloseHandle(g_hThreadRequest);
		g_hThreadRequest	= NULL;
		g_idThreadRequest	= 0;
	}
	if(m_RiskMsgCalc)
	{
		delete m_RiskMsgCalc;
		m_RiskMsgCalc = NULL;
	}
	if(m_RiskMsgCalc_Account)
	{
		delete m_RiskMsgCalc_Account;
		m_RiskMsgCalc_Account = NULL;
	}
}
void DealPakage(unsigned int EventParamID,int& PkgLen,int& hSocket)
{
	AllocTmpStackBuf(TmpPkg,PkgLen);
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
const UINT_PTR g_lnTimerID = 100001;
UINT_PTR g_lnRealTimerID = 0;

//工作线程
DWORD ThreadWorkerRequest(void *arg)
{
	SubscribeAllCmdMsgID();

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
			if(CInterface_SvrTcp::getObj().getEventParamObj().GetEventParam(EventParamID,NULL,NULL,NULL,&PkgLen,0,&hSocket))
			{
				//申请临时空间并获取数据包
				//AllocTmpStackBuf(TmpPkg,PkgLen);
				//if(TmpPkg.m_pbuf&&
				//	CInterface_SvrTcp::getObj().getEventParamObj().GetEventParam(EventParamID,NULL,NULL,TmpPkg.m_pbuf,&PkgLen,PkgLen,&hSocket)&&
				//	Stru_UniPkgHead::IsValidPkg(TmpPkg.m_pbuf,PkgLen))
				//{
				//	Stru_UniPkgHead& PkgHead=*((Stru_UniPkgHead*)TmpPkg.m_pbuf);
				//	void* pPkgData=(char*)TmpPkg.m_pbuf+sizeof(Stru_UniPkgHead);


				//	int n=GetTickCount();
				//	//调用数据包处理函数处理数据包
				//	ProcessOneUniPkg_InThread(hSocket,PkgHead,pPkgData);
				//	int n3=GetTickCount();
				//	OUTPUT_LOG("ProcessOneUniPkg_InThread %d" ,n3-n);

				//}
				DealPakage(EventParamID,PkgLen,hSocket);
				//释放EventParam
				CInterface_SvrTcp::getObj().getEventParamObj().DeleteEventParam(EventParamID);
			}

		}	
	}

	return 0;
}
//工作线程
DWORD ThreadWorker(void *arg)
{
	CInterface_SvrNotifyAndAsk::getObj().SubscribeNotifyEvent(EventTypeInitFinish, g_idThread);
	CInterface_SvrNotifyAndAsk::getObj().SubscribeNotifyEvent(EventTypeStartInit, g_idThread);
	CInterface_SvrNotifyAndAsk::getObj().SubscribeNotifyEvent(EventTypeFund, g_idThread);
	CInterface_SvrNotifyAndAsk::getObj().SubscribeNotifyEvent(EventTypePosition, g_idThread);
	CInterface_SvrNotifyAndAsk::getObj().SubscribeNotifyEvent(EventTypeOrderRtn, g_idThread);
	CInterface_SvrNotifyAndAsk::getObj().SubscribeNotifyEvent(EventTypeTradeRtn, g_idThread);
	CInterface_SvrNotifyAndAsk::getObj().SubscribeNotifyEvent(EventTypeUserLimitAndVerifyUpdate, g_idThread);
	CInterface_SvrNotifyAndAsk::getObj().SubscribeNotifyEvent(EventTypeSlaverLoginedMaster, g_idThread);

	std::set<std::string> setPosition; //持仓
	std::set<std::string> setFund;     //资金
	bool lbTimeCome = false;
	g_lnRealTimerID = SetTimer(NULL, g_lnTimerID, 1000, (TIMERPROC)(NULL));
	MSG Msg;
	while(GetMessage(&Msg,NULL,0,0))
	{
	/*	LONGLONG w1 =  CTools_Win32::MyGetCpuTickCounter();
		static int gTotalCountFund = 0;
		static int gTotalCountPosition = 0;
		static int gTotalCount=0;*/
		lbTimeCome = false;
		//DWORD dwTickCount = GetTickCount();
		do
		{
			//gTotalCount++;
			if(Msg.message==WM_COMMAND&&Msg.wParam==WndCmd_NotifyEventArrival)		
			{
				/*LONGLONG lFfreq = CTools_Win32::MyGetCpuTickFreq();
				LONGLONG n1 =  CTools_Win32::MyGetCpuTickCounter();*/
				//数据包的传输ID
				unsigned int EventParamID=(unsigned int)Msg.lParam;
				//数据包长度
				Stru_NotifyEvent ls;				
				int nLen = sizeof(ls);
				if(CInterface_SvrTcp::getObj().getEventParamObj().GetEventParam(EventParamID,NULL,NULL,&ls,&nLen,nLen,NULL))
				{		
					if(ls.meEventType == EventTypeFund)
					{						
						//gTotalCountFund++;
						std::string strInvestor((char*)(ls.marrDataByte));	
						setFund.insert(strInvestor);
						CInterface_SvrTcp::getObj().getEventParamObj().DeleteEventParam(EventParamID);
					}
					else if(ls.meEventType == EventTypePosition)
					{
						
						//gTotalCountPosition++;
						std::string strInvestor((char*)(ls.marrDataByte));
						setPosition.insert(strInvestor);
						CInterface_SvrTcp::getObj().getEventParamObj().DeleteEventParam(EventParamID);
					}
					else
					{
						//LONGLONG n2 =  CTools_Win32::MyGetCpuTickCounter();
						//调用数据包处理函数处理数据包
						ParseDataChangedEvent(ls);	
						//LONGLONG n3 =  CTools_Win32::MyGetCpuTickCounter();
						//释放EventParam
						CInterface_SvrTcp::getObj().getEventParamObj().DeleteEventParam(EventParamID);

						//LONGLONG n4 =  CTools_Win32::MyGetCpuTickCounter();

						/*double db1 = (n2-n1)*1000.0/lFfreq;
						double db2 = (n3-n2)*1000.0/lFfreq;
						double db3 = (n4-n3)*1000.0/lFfreq;
						if(db1 >0.0001|| db2>0.0001|| db3>0.0001)
							OUTPUT_LOG("ParseDataChangedEvent %.2f %.2f %.2f " ,db1, db2, db3);*/
					}					
				}
			}
			if(Msg.message == WM_TIMER) 
			{
				lbTimeCome = true;

				LONGLONG f1 =  CTools_Win32::MyGetCpuTickCounter();
				if(m_RiskMsgCalc != NULL)
					m_RiskMsgCalc->NewTimer_PosiDetailTime();
				LONGLONG f2 =  CTools_Win32::MyGetCpuTickCounter();
				static int lnFCount = 0;
				lnFCount++;
				if(lnFCount % 1000 == 0)
				{
					LONGLONG lFfreq = CTools_Win32::MyGetCpuTickFreq();
					double lv = ((f2-f1)*1000.0)/(double)lFfreq;
					OUTPUT_LOG("NewTimer_PosiDetailTime Risk Deal  %.2f ", lv);
				}
				if(m_RiskMsgCalc != NULL)
					m_RiskMsgCalc->ActiveEventPassTime();
			}	
		}while(PeekMessage(&Msg,NULL,0,0,PM_REMOVE));

		if(lbTimeCome == false)
			continue;
		//DWORD dw =  GetTickCount() - dwTickCount;
		//OUTPUT_LOG("间隔多久%d\n", dw);

		//LONGLONG w2 =  CTools_Win32::MyGetCpuTickCounter();
		//{
		//	LONGLONG lFfreq = CTools_Win32::MyGetCpuTickFreq();
		//	double lv = ((w2-w1)*1000.0)/(double)lFfreq;
		//	OUTPUT_LOG("Position loop %.2f %d-%d-%d", lv,gTotalCount,gTotalCountFund,gTotalCountPosition);
		//}
		//LONGLONG f1 =  CTools_Win32::MyGetCpuTickCounter();
		//static int lnFCount = 0;
		//lnFCount += setPosition.size();
		std::set<std::string>::iterator itPosition = setPosition.begin();
		while(itPosition != setPosition.end())
		{//持仓消息推送处理
			std::string strInvestor = *itPosition;
			m_RiskMsgCalc->NewPosition(strInvestor);		
			m_RiskMsgCalc->SendPosition(strInvestor);	
		
			std::string strBroker, strUser;
			bool bGet = GetTradeAccountByUserID(strInvestor, strBroker, strUser);
			if(bGet)
				m_RiskMsgCalc_Account->NewPosition(strBroker, strUser);		

			itPosition++;
		}		
	/*	LONGLONG f2 =  CTools_Win32::MyGetCpuTickCounter();
		if(lnFCount % 1000 == 0)
		{
			LONGLONG lFfreq = CTools_Win32::MyGetCpuTickFreq();
			double lv = ((f2-f1)*1000.0)/(double)lFfreq;
			OUTPUT_LOG("Position Risk Deal %d %d %d %.2f ", gTotalCountPosition,lnFCount, setPosition.size(), lv);
		}*/
		setPosition.clear();

		/*LONGLONG p1 =  CTools_Win32::MyGetCpuTickCounter();
		static int lnPCount = 0;
		lnPCount += setFund.size();*/
		std::set<std::string>::iterator itFund = setFund.begin();
		while(itFund != setFund.end())
		{//资金数据推送处理
			std::string strInvestor = *itFund;
			m_RiskMsgCalc->NewFundAccount(strInvestor);		
			m_RiskMsgCalc->SendFund(strInvestor);

			std::string strBroker, strUser;
			bool bGet = GetTradeAccountByUserID(strInvestor, strBroker, strUser);
			if(bGet)
				m_RiskMsgCalc_Account->NewFundAccount(strBroker, strUser);		
			itFund++;
		}	
		/*LONGLONG p2 =  CTools_Win32::MyGetCpuTickCounter();
		if(lnPCount % 1000 == 0)
		{
			LONGLONG lFfreq = CTools_Win32::MyGetCpuTickFreq();
			double lv = ((p2-p1)*1000.0)/(double)lFfreq;
			OUTPUT_LOG("Fund Risk Deal %d %d %d %.2f " ,gTotalCountFund,lnPCount, setFund.size(), lv);
		}*/
		setFund.clear();
	}
	KillTimer(NULL, g_lnRealTimerID);
	return 0;
}
//工作线程存储
DWORD ThreadSave(void *arg)
{	
	while(!g_bThreadSaveExit)
	{
		SAFE_GET_DATACENTER()->IdleBusinessDataMain();
		Sleep(1000);
	}
	return 0;
}
//处理一个SvrTcp推送过来的数据包
void ProcessOneUniPkg_InThread(int hSocket,const Stru_UniPkgHead& PkgHead,const void*pPkgData)
{
	//-----------------------------------------------------------------------------------
	//	下面根据数据包的命令字，处理数据包
	//-----------------------------------------------------------------------------------
	if(PkgHead.cmdid == CMDID_SvrTcpDisconn )
	{//socket disconnect,so discrible all orders
		std::vector<UserInfo> vec;
		CInterface_SvrUserOrg::getObj().GetUserInfos(vec);//得到所有交易员
		for(int i=0; i<(int)vec.size(); i++)
		{
			UserInfo& userInfo = vec[i];
			if(USER_TYPE_TRADER != userInfo.nUserType)
				continue;

			int AccID= userInfo.nUserID;
			m_pThreadSharedData->UnSubscribe(hSocket,SubscribeID_RiskEvent,AccID);
			SAFE_GET_DATACENTER()->UnSubscribeRiskEvent(AccID);	
			SAFE_GET_DATACENTER()->UnSubscribeTrade(AccID);	
			SAFE_GET_DATACENTER()->UnSubscribeFund(AccID);	
			SAFE_GET_DATACENTER()->UnSubscribePosition(AccID);	
			SAFE_GET_DATACENTER()->UnSubscribeOrder(AccID);				
		}	
	}
	else if(PkgHead.cmdid == CMDID_TcpClientDisconn )
	{//socket disconnect,so discrible all orders		
		InterlockedExchange((volatile long*)(&g_hSocket),0);
		RISK_LOGINFO("主从服务断开\n");
	}
	else if(PkgHead.cmdid > Cmd_RM_Config_Min && PkgHead.cmdid < Cmd_RM_Config_Max)
	{
		LONGLONG lFfreq = CTools_Win32::MyGetCpuTickFreq();
		LONGLONG n1 =  CTools_Win32::MyGetCpuTickCounter();
		ProcessConfig(hSocket, PkgHead,pPkgData);
	
		LONGLONG n2 =  CTools_Win32::MyGetCpuTickCounter();
		double db1 = (n2-n1)*1000.0/lFfreq;
		if(db1 >0.0001)
			OUTPUT_LOG("ProcessConfig time=[%.4f] cmid =[%d]" ,db1, PkgHead.cmdid);
		
	}
	else if(PkgHead.cmdid > Cmd_RM_RiskIndicator_Min && PkgHead.cmdid < Cmd_RM_RiskIndicator_Max)
	{
		LONGLONG lFfreq = CTools_Win32::MyGetCpuTickFreq();
		LONGLONG n1 =  CTools_Win32::MyGetCpuTickCounter();

		ProcessRiskIndicatorRequest(hSocket, PkgHead,pPkgData);
		LONGLONG n2 =  CTools_Win32::MyGetCpuTickCounter();
		double db1 = (n2-n1)*1000.0/lFfreq;
		if(db1 >0.0001)
			OUTPUT_LOG("ProcessRiskIndicatorRequest time=[%.4f] cmid =[%d]" ,db1, PkgHead.cmdid);
	}
	else if(PkgHead.cmdid > Cmd_RM_RiskEvent_Min && PkgHead.cmdid < Cmd_RM_RiskEvent_Max)
	{
		LONGLONG lFfreq = CTools_Win32::MyGetCpuTickFreq();
		LONGLONG n1 =  CTools_Win32::MyGetCpuTickCounter();

		ProcessRiskEventRequest(hSocket, PkgHead,pPkgData);
		LONGLONG n2 =  CTools_Win32::MyGetCpuTickCounter();
		double db1 = (n2-n1)*1000.0/lFfreq;
		if(db1 >0.0001)
			OUTPUT_LOG("ProcessRiskEventRequest time=[%.4f] cmid =[%d]" ,db1, PkgHead.cmdid);
	}
	//else if(PkgHead.cmdid > Cmd_RM_Message_Min && PkgHead.cmdid < Cmd_RM_Message_Max)
	//{
		//ProcessMessageRequest(hSocket, PkgHead,pPkgData);
	//}
	else if(PkgHead.cmdid > Cmd_RM_SubscribeQuot_Min && PkgHead.cmdid < Cmd_RM_SubscribeQuot_Max)
	{//从行情服务器取行情
		//	ProcessQuotRequest(hSocket, PkgHead,pPkgData);
	}
	else if(PkgHead.cmdid > Cmd_RM_SubscribeTrade_Min && PkgHead.cmdid < Cmd_RM_SubscribeTrade_Max)
	{
		LONGLONG lFfreq = CTools_Win32::MyGetCpuTickFreq();
		LONGLONG n1 =  CTools_Win32::MyGetCpuTickCounter();

		ProcessTradeRequest(hSocket, PkgHead,pPkgData);
		LONGLONG n2 =  CTools_Win32::MyGetCpuTickCounter();
		double db1 = (n2-n1)*1000.0/lFfreq;
		if(db1 >0.0001)
			OUTPUT_LOG("ProcessTradeRequest time=[%.4f] cmid =[%d]" ,db1, PkgHead.cmdid);
	}
	else if(PkgHead.cmdid > Cmd_RM_SubscribeFund_Min && PkgHead.cmdid < Cmd_RM_SubscribeFund_Max)
	{
		LONGLONG lFfreq = CTools_Win32::MyGetCpuTickFreq();
		LONGLONG n1 =  CTools_Win32::MyGetCpuTickCounter();

		ProcessFundRequest(hSocket, PkgHead,pPkgData);
		LONGLONG n2 =  CTools_Win32::MyGetCpuTickCounter();
		double db1 = (n2-n1)*1000.0/lFfreq;
		if(db1 >0.0001)
			OUTPUT_LOG("ProcessFundRequest time=[%.4f] cmid =[%d]" ,db1, PkgHead.cmdid);
	}
	else if(PkgHead.cmdid > Cmd_RM_SubscribePosition_Min && PkgHead.cmdid < Cmd_RM_SubscribePosition_Max)
	{
		LONGLONG lFfreq = CTools_Win32::MyGetCpuTickFreq();
		LONGLONG n1 =  CTools_Win32::MyGetCpuTickCounter();

		ProcessPositionRequest(hSocket, PkgHead,pPkgData);
		LONGLONG n2 =  CTools_Win32::MyGetCpuTickCounter();
		double db1 = (n2-n1)*1000.0/lFfreq;
		if(db1 >0.0001)
			OUTPUT_LOG("ProcessPositionRequest time=[%.4f] cmid =[%d]" ,db1, PkgHead.cmdid);
	}
	else if(PkgHead.cmdid > Cmd_RM_SubscribeOrder_Min && PkgHead.cmdid < Cmd_RM_SubscribeOrder_Max)
	{
		LONGLONG lFfreq = CTools_Win32::MyGetCpuTickFreq();
		LONGLONG n1 =  CTools_Win32::MyGetCpuTickCounter();

		ProcessOrderRequest(hSocket, PkgHead,pPkgData);
		LONGLONG n2 =  CTools_Win32::MyGetCpuTickCounter();
		double db1 = (n2-n1)*1000.0/lFfreq;
		if(db1 >0.0001)
			OUTPUT_LOG("ProcessOrderRequest time=[%.4f] cmid =[%d]" ,db1, PkgHead.cmdid);
	}
	else if(PkgHead.cmdid > Cmd_RM_FundNet_Min && PkgHead.cmdid < Cmd_RM_FundNet_Max)
	{
		LONGLONG lFfreq = CTools_Win32::MyGetCpuTickFreq();
		LONGLONG n1 =  CTools_Win32::MyGetCpuTickCounter();

		ProcessNetValueRequest(hSocket, PkgHead,pPkgData);
		LONGLONG n2 =  CTools_Win32::MyGetCpuTickCounter();
		double db1 = (n2-n1)*1000.0/lFfreq;
		if(db1 >0.0001)
			OUTPUT_LOG("ProcessNetValueRequest time=[%.4f] cmid =[%d]" ,db1, PkgHead.cmdid);
	}
	else if(PkgHead.cmdid > Cmd_RM_ForceCloseDo_Min && PkgHead.cmdid < Cmd_RM_ForceCloseDo_Max)
	{
		LONGLONG lFfreq = CTools_Win32::MyGetCpuTickFreq();
		LONGLONG n1 =  CTools_Win32::MyGetCpuTickCounter();

		ProcessForceCloseRequest(hSocket, PkgHead,pPkgData);
		LONGLONG n2 =  CTools_Win32::MyGetCpuTickCounter();
		double db1 = (n2-n1)*1000.0/lFfreq;
		if(db1 >0.0001)
			OUTPUT_LOG("ProcessForceCloseRequest time=[%.4f] cmid =[%d]" ,db1, PkgHead.cmdid);
	}
	else if(PkgHead.cmdid > Cmd_RM_ToExecute_Min && PkgHead.cmdid < Cmd_RM_ToExecute_Max)
	{
		LONGLONG lFfreq = CTools_Win32::MyGetCpuTickFreq();
		LONGLONG n1 =  CTools_Win32::MyGetCpuTickCounter();

		ProcessRisk2ExcuteRequest(hSocket, PkgHead,pPkgData);
		LONGLONG n2 =  CTools_Win32::MyGetCpuTickCounter();
		double db1 = (n2-n1)*1000.0/lFfreq;
		if(db1 >0.0001)
			OUTPUT_LOG("ProcessRisk2ExcuteRequest time=[%.4f] cmid =[%d]" ,db1, PkgHead.cmdid);
	}
	else if(PkgHead.cmdid > Cmd_RM_QryAdmin_Min && PkgHead.cmdid < Cmd_RM_QryAdmin_Max)
	{
		LONGLONG lFfreq = CTools_Win32::MyGetCpuTickFreq();
		LONGLONG n1 =  CTools_Win32::MyGetCpuTickCounter();

		ProcessAdminQuery(hSocket, PkgHead,pPkgData);
		LONGLONG n2 =  CTools_Win32::MyGetCpuTickCounter();
		double db1 = (n2-n1)*1000.0/lFfreq;
		if(db1 >0.0001)
			OUTPUT_LOG("ProcessAdminQuery time=[%.4f] cmid =[%d]" ,db1, PkgHead.cmdid);
	}
	else
	{
		if(PkgHead.cmdid == Cmd_ModifyUserPassword_Req)
		{
			//向交易执行发送数据的模式
			int nSocket =0;
			InterlockedExchange((volatile long*)(&nSocket),g_hSocket);
			if(nSocket)
			{
				int nSeqID = InsertmapSeqID2Socket(hSocket);		
				CInterface_SvrTcp::getObj().SendPkgData(nSocket,Cmd_RM_Master_ModifyUserPassword_Req,
					(void*)pPkgData, PkgHead.len,0,0,CF_ERROR_SUCCESS,0,PkgHead.userdata1,nSeqID);	
			}
			else
			{//输出socket无效//输出日志
				RISK_LOGINFO("主从服务器断开，修改密码请求失败\n");
			}			
		}
	}

	switch(PkgHead.cmdid)
	{
		case CMDID_HEARTBEAT:
		{
			break;
		}
	}
}
void ProcessConfig(int hSocket,const Stru_UniPkgHead& PkgHead,const void*pPkgData)
{
	switch(PkgHead.cmdid)
	{
		case Cmd_RM_QryOrganization_Req:
			{
				if(PkgHead.len != 0)
				{
					const char * lErrorString = FormatErrorCode(CF_ERROR_COMMON_INPUT_PARAM);
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_QryOrganization_Rsp, 
						(void*)lErrorString, strlen(lErrorString)+1, PkgHead.seq, 0, CF_ERROR_COMMON_INPUT_PARAM);
					return;
				}

				std::vector<Organization> vec;
				CInterface_SvrUserOrg::getObj().GetOrgs(vec);
				
				if(vec.size() != 0)
					CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryOrganization_Rsp,
						&vec[0],vec.size()*sizeof(Organization),PkgHead.seq,0,CF_ERROR_SUCCESS);
				else
					CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryOrganization_Rsp,
					NULL,0,PkgHead.seq,0,CF_ERROR_SUCCESS);
			}
			break;
		case Cmd_RM_QryUser_Req:
			{
				if(PkgHead.len != 0)
				{
					const char * lErrorString = FormatErrorCode(CF_ERROR_COMMON_INPUT_PARAM);
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_QryUser_Rsp, 
						(void*)lErrorString, strlen(lErrorString)+1, PkgHead.seq, 0, CF_ERROR_COMMON_INPUT_PARAM);
					return;
				}

				std::vector<UserInfo> vec;
				std::vector<UserInfo> vec2;
				CInterface_SvrUserOrg::getObj().GetUserInfos(vec2);
				for(int i =0; i<(int)vec2.size(); i++)
				{
					UserInfo& userInfo = vec2[i];
				//	if(userInfo.nUserType == USER_TYPE_TRADER)
						vec.push_back(userInfo);
				}

				if(vec.size() != 0)
					CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryUser_Rsp,
						&vec[0],vec.size()*sizeof(UserInfo),PkgHead.seq,0,CF_ERROR_SUCCESS);	
				else
					CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryUser_Rsp,
					NULL, 0,PkgHead.seq,0,CF_ERROR_SUCCESS);	
			}
			break;
		case Cmd_RM_QryUserAndOrgRelation_Req:
			{
				if(PkgHead.len != 0)
				{
					const char * lErrorString = FormatErrorCode(CF_ERROR_COMMON_INPUT_PARAM);
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_QryUserAndOrgRelation_Rsp, 
						(void*)lErrorString, strlen(lErrorString)+1, PkgHead.seq, 0, CF_ERROR_COMMON_INPUT_PARAM);
					return;
				}

				std::vector<UserAndOrgRelation> vec;
				CInterface_SvrUserOrg::getObj().GetUserAndOrgRelation(vec);

				if(vec.size() != 0)					
					CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryUserAndOrgRelation_Rsp,
						&vec[0],vec.size()*sizeof(UserAndOrgRelation),PkgHead.seq,0,CF_ERROR_SUCCESS);	
				else
					CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryUserAndOrgRelation_Rsp,
					NULL,0,PkgHead.seq,0,CF_ERROR_SUCCESS);	
			}
			break;
		case Cmd_RM_QryTraderAndTradeAccountRelation_Req:
			{
				if(PkgHead.len != 0)
				{
					const char * lErrorString = FormatErrorCode(CF_ERROR_COMMON_INPUT_PARAM);
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_QryTraderAndTradeAccountRelation_Rsp, 
						(void*)lErrorString, strlen(lErrorString)+1, PkgHead.seq, 0, CF_ERROR_COMMON_INPUT_PARAM);
					return;
				}
				
				std::vector<UserAndTradeAccountRelation> vec;
				CInterface_SvrUserOrg::getObj().GetUserAndTradeAccountRelation(vec);
				if(vec.size() != 0)
					CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryTraderAndTradeAccountRelation_Rsp,
						&vec[0],vec.size()*sizeof(UserAndTradeAccountRelation),PkgHead.seq,0,CF_ERROR_SUCCESS);	
				else
					CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryTraderAndTradeAccountRelation_Rsp,
					NULL,0,PkgHead.seq,0,CF_ERROR_SUCCESS);	
			}
			break;
		case Cmd_RM_QueryRiskPlan_Req:
			{
				std::vector<RiskPlan> vecRiskPlan;
				int pkgLen= PkgHead.len;
				int AccCnt=pkgLen/sizeof(int);
				int AccRem=pkgLen%sizeof(int);
				if (pkgLen>0||AccRem==0)
				{
					std::vector<RiskEvent> vec;
					int* pOrgID = (int*)pPkgData;					
					for (int index=0;index<AccCnt;++index)
					{
						int nOrgID=pOrgID[index];					
						vector<RiskPlan> vec;
						SAFE_GET_DATACENTER()->GetRiskPlanByOrgID(nOrgID, vec);
						if(vec.size() >0)
						{
							vecRiskPlan.insert(vecRiskPlan.end(), vec.begin(), vec.end());
							
						}
					}
				}
				if(vecRiskPlan.size() >0)
					std::sort(vecRiskPlan.begin(),vecRiskPlan.end());
/*
				int nOrgID = *(int*)pPkgData;

				std::map<int, Organization> mapOrganization;
				CInterface_SvrUserOrg::getObj().GetLowerLevelOrgs( nOrgID, mapOrganization);
				
				Organization orgUpper;//这个值没用到
				memset(&orgUpper, 0, sizeof(Organization));
				mapOrganization.insert(make_pair(nOrgID, orgUpper));
				std::vector<RiskPlan> vecRiskPlan;
				std::map<int, Organization>::iterator it = mapOrganization.begin();
				for(; it != mapOrganization.end(); it++)
				{
					int nOrgID = it->first;
					vector<RiskPlan> vec;
					SAFE_GET_DATACENTER()->GetRiskPlanByOrgID(nOrgID, vec);
					if(vec.size() >0)
						vecRiskPlan.insert(vecRiskPlan.end(), vec.begin(), vec.end());
				}*/
			
				if(vecRiskPlan.size()!=0)
					CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QueryRiskPlan_Rsp,
						&vecRiskPlan[0],vecRiskPlan.size()*sizeof(RiskPlan),PkgHead.seq,0,CF_ERROR_SUCCESS);
				else
					CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QueryRiskPlan_Rsp,
					NULL,0,PkgHead.seq,0,CF_ERROR_SUCCESS);
			
			}
			break;
		case Cmd_RM_AddRiskPlan_Req:
			{
				if(!CInterface_SvrNotifyAndAsk::getObj().Ask_ValidateUserPrivilege(PkgHead.userdata1,RISK_ADD))
				{
					const char * lErrorString = FormatErrorCode(CF_ERROR_RISK_ADDPLAN_NOPRIVILEDGE);
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_AddRiskPlan_Rsp, 
						(void*)lErrorString, 
						strlen(lErrorString)+1, PkgHead.seq,0,CF_ERROR_COMMON_NO_PRIVILEGE);
					break;
				}

				vector<RiskPlan> vec;
				int pkgLen= PkgHead.len;
				int AccCnt=pkgLen/sizeof(RiskPlan);				
				for(int i =0; i< AccCnt; i++)
				{
					RiskPlan* pField = (RiskPlan*)((char*)(pPkgData) + i*sizeof(RiskPlan));
					vec.push_back(*pField);					
				}	
				if(vec.size() == 0)
				{
					const char * lErrorString = FormatErrorCode(CF_ERROR_COMMON_INPUT_PARAM);
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_AddRiskPlan_Rsp, 
						(void*)lErrorString, strlen(lErrorString)+1, PkgHead.seq, 0, CF_ERROR_COMMON_INPUT_PARAM);
					return;
				}
				RiskChange riskChange;
				riskChange.nOrgID			 = vec[0].OrgIDPlanRelation.nOrgID;
				riskChange.nRiskIndicatorID	 = vec[0].OrgIDPlanRelation.nRiskIndicatorID;
				riskChange.bUse				 = vec[0].OrgIDPlanRelation.bUse;
				int nErrorCode = CF_ERROR_SUCCESS;
				if ( !CInterface_SvrDBOpr::getObj().InsertRiskPlan(vec, nErrorCode))
				{
					//发送错误回传
					const char * lErrorString = FormatErrorCode(nErrorCode);
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_AddRiskPlan_Rsp, 
						(void*)lErrorString, strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode);					
				}
				else
				{					
					CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_AddRiskPlan_Rsp,
						&riskChange,sizeof(RiskChange),PkgHead.seq,0,CF_ERROR_SUCCESS);	

					SAFE_GET_DATACENTER()->ReadRiskWarning();//更新内存里面的风控方案设置
					m_RiskMsgCalc->ChangeRiskPlan();
					m_RiskMsgCalc_Account->ChangeRiskPlan();
				}				
				break;
			}
		case Cmd_RM_UseRiskPlanAdd_Req:
			{
				if(!CInterface_SvrNotifyAndAsk::getObj().Ask_ValidateUserPrivilege(PkgHead.userdata1,RISK_SWITCH))
				{
					const char * lErrorString = FormatErrorCode(CF_ERROR_RISK_USEPLAN_NOPRIVILEDGE);
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_UseRiskPlanAdd_Rsp, 
						(void*)lErrorString, 
						strlen(lErrorString)+1, PkgHead.seq,0,CF_ERROR_COMMON_NO_PRIVILEGE);
					break;
				}

				if ( PkgHead.len != sizeof(RiskChange))
				{
					const char * lErrorString = FormatErrorCode(CF_ERROR_COMMON_INPUT_PARAM);
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_UseRiskPlanAdd_Rsp, 
						(void*)lErrorString, strlen(lErrorString)+1, PkgHead.seq, 0, CF_ERROR_COMMON_INPUT_PARAM);
					return;
				}
				RiskChange key = *(RiskChange*)pPkgData;
			
				char szBuffer[MAX_SQL_LENGTH];
				memset(szBuffer, 0, sizeof(szBuffer));
				sprintf(szBuffer,  "update RISK_ORGIDPLANRELATION t set t.use = %d where t.ORGID = %d and RISKINDICATORID =%d", 
					key.bUse,	key.nOrgID, key.nRiskIndicatorID);
				int nErrorCode = CF_ERROR_DATABASE_OTHER_ERROR;
				CInterface_SvrDBOpr::getObj().Excute(szBuffer, nErrorCode);
				if(nErrorCode != CF_ERROR_SUCCESS)
				{
					const char * lErrorString = FormatErrorCode(nErrorCode);
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_UseRiskPlanAdd_Rsp, 
						(void*)lErrorString, strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode);		
				}
				else
				{
					CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_UseRiskPlanAdd_Rsp,
						&key,sizeof(RiskChange),PkgHead.seq,0,CF_ERROR_SUCCESS);	

					SAFE_GET_DATACENTER()->ReadRiskWarning();//更新内存里面的风控方案设置
					m_RiskMsgCalc->ChangeRiskPlan();
					m_RiskMsgCalc_Account->ChangeRiskPlan();
				}
				break;
			}
		case Cmd_RM_DeleteRiskPlan_Req:
			{
				if(!CInterface_SvrNotifyAndAsk::getObj().Ask_ValidateUserPrivilege(PkgHead.userdata1,RISK_DEL))
				{
					const char * lErrorString = FormatErrorCode(CF_ERROR_RISK_DELETEPLAN_NOPRIVILEDGE);
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_DeleteRiskPlan_Rsp, 
						(void*)lErrorString, 
						strlen(lErrorString)+1, PkgHead.seq,0,CF_ERROR_COMMON_NO_PRIVILEGE);
					break;
				}

				if ( PkgHead.len != sizeof(RiskChange))
				{
					const char * lErrorString = FormatErrorCode(CF_ERROR_COMMON_INPUT_PARAM);
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_DeleteRiskPlan_Rsp, 
						(void*)lErrorString, strlen(lErrorString)+1, PkgHead.seq, 0, CF_ERROR_COMMON_INPUT_PARAM);
					return;
				}
				RiskChange key = *(RiskChange*)pPkgData;

				char szBuffer[MAX_SQL_LENGTH];
				memset(szBuffer, 0, sizeof(szBuffer));
				sprintf(szBuffer,  "delete from RISK_ORGIDPLANRELATION t1  where t1.ORGID = %d and t1.RISKINDICATORID = %d ",
					key.nOrgID, key.nRiskIndicatorID);
				int nErrorCode = CF_ERROR_DATABASE_OTHER_ERROR;
				CInterface_SvrDBOpr::getObj().Excute(szBuffer, nErrorCode);				

				memset(szBuffer, 0, sizeof(szBuffer));
				sprintf(szBuffer,  "delete from RISK_RISKWARING t1 where t1.ORGID = %d and t1.RISKINDICATORID = %d ",
					key.nOrgID, key.nRiskIndicatorID);
				int nErrorCode2 = CF_ERROR_DATABASE_OTHER_ERROR;
				CInterface_SvrDBOpr::getObj().Excute(szBuffer, nErrorCode2);	

				int nErrorCode3 = CF_ERROR_DATABASE_OTHER_ERROR;
				if(key.nRiskIndicatorID == RI_FundNetValue)
				{
					memset(szBuffer, 0, sizeof(szBuffer));
					sprintf(szBuffer,  "delete from RISK_FUNDNETPARAM t1 where t1.ORGID = %d",
						key.nOrgID);					
					CInterface_SvrDBOpr::getObj().Excute(szBuffer, nErrorCode3);
				}

				if(nErrorCode != CF_ERROR_SUCCESS && nErrorCode2 != CF_ERROR_SUCCESS && nErrorCode3 != CF_ERROR_SUCCESS)
				{
					const char * lErrorString = FormatErrorCode(nErrorCode);
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_DeleteRiskPlan_Rsp, 
						(void*)lErrorString, strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode);		
				}
				else
				{
					CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_DeleteRiskPlan_Rsp,
						&key,sizeof(RiskChange),PkgHead.seq,0,CF_ERROR_SUCCESS);	

					SAFE_GET_DATACENTER()->ReadRiskWarning();//更新内存里面的风控方案设置
					m_RiskMsgCalc->ChangeRiskPlan();
					m_RiskMsgCalc_Account->ChangeRiskPlan();
				}
				break;
			}			
		case Cmd_RM_QryProducts_Req:
			{
				if ( PkgHead.len != 0)
				{
					const char * lErrorString = FormatErrorCode(CF_ERROR_COMMON_INPUT_PARAM);
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_QryProducts_Rsp, 
						(void*)lErrorString, strlen(lErrorString)+1, PkgHead.seq, 0, CF_ERROR_COMMON_INPUT_PARAM);
					return;
				}
				std::map<std::string, std::set<std::string> > outData;
				CInterface_SvrTradeData::getObj().GetProductID_InstrumentIDsByExchangeID(outData);
				
				if(outData.size() == 0)
				{
					CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryProducts_Rsp,
						NULL,0,PkgHead.seq,0,CF_ERROR_SUCCESS);
					return;
				}

				vector<SProduct> vecProduct;
				std::map<std::string, std::set<std::string> >::iterator it = outData.begin();
				for(; it != outData.end(); it++)
				{
					std::string strProduct = it->first;
					SProduct sProduct;
					strcpy(sProduct.ProductID, strProduct.c_str());
					vecProduct.push_back(sProduct);
				}

				if(vecProduct.size() != 0)
					CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryProducts_Rsp,
						&vecProduct[0],vecProduct.size()*sizeof(SProduct),PkgHead.seq,0,CF_ERROR_SUCCESS);
				else
					CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryProducts_Rsp,
					NULL,0,PkgHead.seq,0,CF_ERROR_SUCCESS);
			}
			break;
		case Cmd_RM_QryResponse_Req:
			{
				if ( PkgHead.len != 0)
				{
					const char * lErrorString = FormatErrorCode(CF_ERROR_COMMON_INPUT_PARAM);
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_QryResponse_Rsp, 
						(void*)lErrorString, strlen(lErrorString)+1, PkgHead.seq, 0, CF_ERROR_COMMON_INPUT_PARAM);
					return;
				}

				char szBuffer[MAX_SQL_LENGTH];
				memset(szBuffer, 0, sizeof(szBuffer));
				sprintf(szBuffer, "select * from RISK_RESPONSE");
				vector<RiskResponse> vec;
				int nErrorCode = CF_ERROR_SUCCESS;
				if ( !CInterface_SvrDBOpr::getObj().QueryData(szBuffer, vec, nErrorCode))
				{
					//发送错误回传
					const char * lErrorString = FormatErrorCode(nErrorCode);
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_QryResponse_Rsp, 
						(void*)lErrorString, 
						strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode);					
				}
				else
				{
					if(vec.size() != 0)
						CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryResponse_Rsp,
							&vec[0],sizeof(RiskResponse)*vec.size(),PkgHead.seq,0,CF_ERROR_SUCCESS);		
					else
						CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryResponse_Rsp,
						NULL,0,PkgHead.seq,0,CF_ERROR_SUCCESS);	
				}
			}
			break;
		case Cmd_RM_SetLimitTrade_Req:
			{
				if(!CInterface_SvrNotifyAndAsk::getObj().Ask_ValidateUserPrivilege(PkgHead.userdata1,RISK_LIMIT_TRADE))
				{
					const char * lErrorString = FormatErrorCode(CF_ERROR_RISK_LIMITTRADE_NOPRIVILEDGE);
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_SetLimitTrade_Rsp, 
						(void*)lErrorString, 
						strlen(lErrorString)+1, PkgHead.seq,0,CF_ERROR_COMMON_NO_PRIVILEGE);
					break;
				}

				if ( PkgHead.len != sizeof(SLimitTrade))
				{
					const char * lErrorString = FormatErrorCode(CF_ERROR_COMMON_INPUT_PARAM);
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_SetLimitTrade_Rsp, 
						(void*)lErrorString, strlen(lErrorString)+1, PkgHead.seq, 0, CF_ERROR_COMMON_INPUT_PARAM);
					return;
				}
				//向交易执行发送数据的模式
				int nSocket =0;
				InterlockedExchange((volatile long*)(&nSocket),g_hSocket);
				if(nSocket)
				{
					int nSeqID = InsertmapSeqID2Socket(hSocket);		
					CInterface_SvrTcp::getObj().SendPkgData(nSocket,Cmd_RM_Master_SetLimitTrade_Req,
						(void*)pPkgData, PkgHead.len,0,0,CF_ERROR_SUCCESS,0,PkgHead.userdata1,nSeqID);	
				}
				else
				{//输出socket无效//输出日志
					RISK_LOGINFO("主从服务器断开，限制交易请求失败\n");
				}				
/*				SLimitTrade key = *(SLimitTrade*)pPkgData;
				bool bSet = CInterface_SvrUserOrg::getObj().SetLimitTrade(key.nUserID, key.bLimitTrade,hSocket,PkgHead.seq);
				if(bSet)
					CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_SetLimitTrade_Rsp,
					(void*)pPkgData, PkgHead.len,PkgHead.seq,0,CF_ERROR_SUCCESS);	
				else
				{
					int nErrorCode = CF_ERROR_RISK_SETLIMITTRADE;
					const char * lErrorString = FormatErrorCode(nErrorCode);
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_SetLimitTrade_Rsp, 
						(void*)lErrorString, 
						strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode);						
				}*/
			}
			break;
		case Cmd_RM_SetManualVerify_Req:
			{
				if(!CInterface_SvrNotifyAndAsk::getObj().Ask_ValidateUserPrivilege(PkgHead.userdata1,RISK_MANUAL_VERIFY))
				{
					const char * lErrorString = FormatErrorCode(CF_ERROR_RISK_SETMEMUALVERIFY_NOPRIVILEDGE);
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_SetManualVerify_Rsp, 
						(void*)lErrorString, 
						strlen(lErrorString)+1, PkgHead.seq,0,CF_ERROR_COMMON_NO_PRIVILEGE);
					break;
				}

				if ( PkgHead.len != sizeof(SManualVerify))
				{
					const char * lErrorString = FormatErrorCode(CF_ERROR_COMMON_INPUT_PARAM);
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_SetManualVerify_Rsp, 
						(void*)lErrorString, strlen(lErrorString)+1, PkgHead.seq, 0, CF_ERROR_COMMON_INPUT_PARAM);
					return;
				}
				//向交易执行发送数据的模式
				int nSocket =0;
				InterlockedExchange((volatile long*)(&nSocket),g_hSocket);
				if(nSocket)
				{
					int nSeqID = InsertmapSeqID2Socket(hSocket);		
					CInterface_SvrTcp::getObj().SendPkgData(nSocket,Cmd_RM_Master_SetManualVerify_Req,
						(void*)pPkgData, PkgHead.len,0,0,CF_ERROR_SUCCESS,0,PkgHead.userdata1,nSeqID);	
				}
				else
				{//输出socket无效//输出日志
					RISK_LOGINFO("主从服务器断开，设置手动审核请求失败\n");
				}
			/*	SManualVerify key = *(SManualVerify*)pPkgData;

				bool bSet = CInterface_SvrUserOrg::getObj().SetManualVerify(key.nUserID, key.bManualVerify,hSocket,PkgHead.seq);
				if(bSet)
					CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_SetManualVerify_Rsp,
					(void*)pPkgData, PkgHead.len,PkgHead.seq,0,CF_ERROR_SUCCESS);	
				else
				{
					int nErrorCode = CF_ERROR_RISK_SETMEMUALVERIFY;
					const char * lErrorString = FormatErrorCode(nErrorCode);
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_SetManualVerify_Rsp, 
						(void*)lErrorString, 
						strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode);						
				}	*/		
			}
			break;
		case Cmd_RM_CONTRACTSET_Req:
			{
				if ( PkgHead.len != sizeof(STradeForbidRequest))
				{
					const char * lErrorString = FormatErrorCode(CF_ERROR_COMMON_INPUT_PARAM);
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_CONTRACTSET_Rsp, 
						(void*)lErrorString, strlen(lErrorString)+1, PkgHead.seq, 0, CF_ERROR_COMMON_INPUT_PARAM);
					return;
				}
				STradeForbidRequest key = *(STradeForbidRequest*)pPkgData;
				char szBuffer[MAX_SQL_LENGTH];
				memset(szBuffer, 0, sizeof(szBuffer));
				sprintf(szBuffer, 
					"Insert into RISK_CONTRACT_SET(riskindicatorid, account, riskername, INSERTDATE, productid, instruments, margindividdynamic, marginuse, lossamount, losspercent) \
					values(%d, '%s', '%s', sysdate, '%s', '%s', %f, %f, %f, %f )", 
					key.rType, key.Account, key.szRiskerName,  key.ProductID, key.cInstruments, key.dbMarginDividDynamic, key.dbMarginUse, key.dbLossAmount, key.dbLossPercent
					);				
				int nErrorCode = CF_ERROR_SUCCESS;
				if ( !CInterface_SvrDBOpr::getObj().Excute(szBuffer, nErrorCode))
				{
					//发送错误回传
					const char * lErrorString = FormatErrorCode(nErrorCode);
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_CONTRACTSET_Rsp, 
						(void*)lErrorString, 
						strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode);					
				}
				else
				{
					CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_CONTRACTSET_Rsp,
						NULL,0,PkgHead.seq,0,CF_ERROR_SUCCESS);	
				}

			}
			break;
		case Cmd_RM_CONTRACTSET_QUERY_Req:
			{
				if ( PkgHead.len != sizeof(SQueryTradeRistrict))
				{
					const char * lErrorString = FormatErrorCode(CF_ERROR_COMMON_INPUT_PARAM);
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_CONTRACTSET_QUERY_Rsp, 
						(void*)lErrorString, strlen(lErrorString)+1, PkgHead.seq, 0, CF_ERROR_COMMON_INPUT_PARAM);
					return;
				}
				SQueryTradeRistrict key = *(SQueryTradeRistrict*)pPkgData;
				char szBuffer[MAX_SQL_LENGTH];
				memset(szBuffer, 0, sizeof(szBuffer));
				sprintf(szBuffer, 
					"select  riskindicatorid, account, riskername, INSERTDATE, productid, instruments, margindividdynamic, marginuse, lossamount, losspercent from  RISK_CONTRACT_SET\
					where account = '%s' and INSERTDATE>= to_date('%s','YYYY-MM-DD') and INSERTDATE <=  to_date('%s','YYYY-MM-DD')", key.strAccount, key.szDateBegin,  key.szDateEnd);				

				std::vector<STradeForbidRequest> vecTradeForbid;
				std::vector<std::vector<_variant_t>> vNode;
				int nErrorCode = CF_ERROR_SUCCESS;
				if(!CInterface_SvrDBOpr::getObj().QueryData(szBuffer, vNode, nErrorCode)/* || vNode.size()==0*/)
				{
					const char * lErrorString = FormatErrorCode(nErrorCode);
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_CONTRACTSET_QUERY_Rsp, 
						(void*)lErrorString, 
						strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode);			
				}
				else
				{
					STradeForbidRequest tradeForbidRequest;
					for(UINT j = 0; j < vNode.size(); j++) 
					{
						memset(&tradeForbidRequest, 0, sizeof(tradeForbidRequest));

						std::vector<_variant_t> vValue = vNode[j];
						{
							int i = 0;							
							int nType = vValue[i++];
							tradeForbidRequest.rType = (RiskIndicatorType)nType;
							strncpy(tradeForbidRequest.Account, (_bstr_t)(vValue[i++]), sizeof(tradeForbidRequest.Account)-1);
							strncpy(tradeForbidRequest.szRiskerName, (_bstr_t)(vValue[i++]), sizeof(tradeForbidRequest.szRiskerName)-1);
							strncpy(tradeForbidRequest.szInsertDate, (_bstr_t)(vValue[i++]), sizeof(tradeForbidRequest.szInsertDate)-1);

							strncpy(tradeForbidRequest.ProductID, (_bstr_t)(vValue[i++]), sizeof(tradeForbidRequest.ProductID)-1);
							strncpy(tradeForbidRequest.cInstruments, (_bstr_t)(vValue[i++]), sizeof(tradeForbidRequest.cInstruments)-1);
							tradeForbidRequest.dbMarginDividDynamic  = vValue[i++];
							tradeForbidRequest.dbMarginUse  = vValue[i++];
							tradeForbidRequest.dbLossAmount  = vValue[i++];
							tradeForbidRequest.dbLossPercent  = vValue[i++];							

							vecTradeForbid.push_back(tradeForbidRequest);
						}
					}
					if(vecTradeForbid.size() != 0)
						CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_CONTRACTSET_QUERY_Rsp,
						&vecTradeForbid[0],sizeof(STradeForbidRequest)*vecTradeForbid.size(),PkgHead.seq,0,CF_ERROR_SUCCESS);		
					else
						CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_CONTRACTSET_QUERY_Rsp,
						NULL,0,PkgHead.seq,0,CF_ERROR_SUCCESS);	
				}

			}
			break;
		default:
			break;
	}
}
void ProcessRiskIndicatorRequest(int hSocket,const Stru_UniPkgHead& PkgHead,const void*pPkgData)
{
	switch(PkgHead.cmdid)
	{
	case Cmd_RM_QryRiskIndicator_Req:
		{
			if ( PkgHead.len != 0)
			{
				const char * lErrorString = FormatErrorCode(CF_ERROR_COMMON_INPUT_PARAM);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_QryRiskIndicator_Rsp, 
					(void*)lErrorString, strlen(lErrorString)+1, PkgHead.seq, 0, CF_ERROR_COMMON_INPUT_PARAM);
				return;
			}

			char szBuffer[MAX_SQL_LENGTH];
			memset(szBuffer, 0, sizeof(szBuffer));
			sprintf(szBuffer, "select * from RISK_RISKINDICATOR t order by t.riskindicatorid");
			vector<RiskIndicator> vec;
			int nErrorCode = CF_ERROR_SUCCESS;
			if ( !CInterface_SvrDBOpr::getObj().QueryData(szBuffer, vec, nErrorCode))
			{
				//发送错误回传
				const char * lErrorString = FormatErrorCode(nErrorCode);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_QryRiskIndicator_Rsp, 
					(void*)lErrorString, 
					strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode);					
			}
			else
			{
				if(vec.size() != 0)
					CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryRiskIndicator_Rsp,
						&vec[0],sizeof(RiskIndicator)*vec.size(),PkgHead.seq,0,CF_ERROR_SUCCESS);	
				else
					CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryRiskIndicator_Rsp,
					NULL,0,PkgHead.seq,0,CF_ERROR_SUCCESS);
			}
		}
		break;
	default:
		break;

	}
}

void ProcessRiskEventRequest(int hSocket,const Stru_UniPkgHead& PkgHead,const void*pPkgData)
{
	switch(PkgHead.cmdid)
	{		
	case Cmd_RM_SubscribeRiskEvent_Req://订阅风险事件请求
		{
			int pkgLen= PkgHead.len;
			int AccCnt=pkgLen/sizeof(int);
			int AccRem=pkgLen%sizeof(int);
			if (pkgLen>0||AccRem==0)
			{
				std::vector<RiskEvent> vec;
				int* pAccountID = (int*)pPkgData;
				for (int index=0;index<AccCnt;++index)
				{
					int AccID=pAccountID[index];
					m_pThreadSharedData->Subscribe(hSocket,SubscribeID_RiskEvent,AccID);	
					SAFE_GET_DATACENTER()->SubscribeRiskEvent(AccID);					
					SAFE_GET_DATACENTER()->GetRiskEventList(AccID,vec);				
				}
				if (!vec.empty())
					CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_SubscribeRiskEvent_Rsp,
					&vec[0],sizeof(RiskEvent)*vec.size(),PkgHead.seq,0,CF_ERROR_SUCCESS);
			}
			break;
		}
		break;
	case Cmd_RM_UnSubscribeRiskEvent_Req://退订风险事件请求
		{
			int pkgLen= PkgHead.len;
			int AccCnt=pkgLen/sizeof(int);
			int AccRem=pkgLen%sizeof(int);
			if (pkgLen>0||AccRem==0)
			{
				int* pAccountID = (int*)pPkgData;
				for (int index=0;index<AccCnt;++index)
				{
					int AccID=pAccountID[index];
					m_pThreadSharedData->UnSubscribe(hSocket,SubscribeID_RiskEvent,AccID);
					SAFE_GET_DATACENTER()->UnSubscribeRiskEvent(AccID);
				}
				CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_UnSubscribeRiskEvent_Rsp,
					(void*)pPkgData,PkgHead.len,PkgHead.seq,0,CF_ERROR_SUCCESS);				
			}
		}
		break;
	case Cmd_RM_QryRiskEvent_Req:
		{
			if(!CInterface_SvrNotifyAndAsk::getObj().Ask_ValidateUserPrivilege(PkgHead.userdata1,RISK_HISTORY_RISK_EVENT_QUERY))
			{
				const char * lErrorString = FormatErrorCode(CF_ERROR_HISTORY_RISK_EVENT_QUERY_NOPRIVILEDGE);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_QryRiskEvent_Rsp, 
					(void*)lErrorString, 
					strlen(lErrorString)+1, PkgHead.seq,0,CF_ERROR_COMMON_NO_PRIVILEGE);
				break;
			}


			if ( PkgHead.len != sizeof(RiskEventQueryKey))
			{
				const char * lErrorString = FormatErrorCode(CF_ERROR_COMMON_INPUT_PARAM);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_QryRiskEvent_Rsp, 
					(void*)lErrorString, strlen(lErrorString)+1, PkgHead.seq, 0, CF_ERROR_COMMON_INPUT_PARAM);
				return;
			}

			RiskEventQueryKey key = *(RiskEventQueryKey*)pPkgData;
			char szBuffer[MAX_SQL_LENGTH];
			memset(szBuffer, 0, sizeof(szBuffer));
	/*		sprintf(szBuffer, "select * from risk_riskevent t where t.TRADEACCOUNT = %d and \
							  to_char(to_date('1970-01-01','yyyy-MM-dd')+t.eventtime/86400,'YYYY-MM-DD') >= '%s'\
							  and to_char(to_date('1970-01-01','yyyy-MM-dd')+t.eventtime/86400,'YYYY-MM-DD') <= '%s'\
							  and (t.riskeventid,t.riskeventsubid) in \
							  (select t1.riskeventid,max(t1.riskeventsubid) from risk_riskevent t1 group by t1.riskeventid)", 
							  key.nTradeAccountID, key.szStartDate,key.szEndDate);
*/
			sprintf(szBuffer, "select * from risk_riskevent t where t.TRADEACCOUNT = %d and      \
				to_char(to_date('1970-01-01','yyyy-MM-dd')+t.eventtime/86400,'YYYY-MM-DD') >= '%s'     \
				and to_char(to_date('1970-01-01','yyyy-MM-dd')+t.eventtime/86400,'YYYY-MM-DD') <= '%s' 	order by t.riskeventid,t.riskeventsubid "  , 
				 key.nTradeAccountID, key.szStartDate,key.szEndDate);

			int nErrorCode = CF_ERROR_SUCCESS;
			vector<RiskEvent> vec;
			if ( !CInterface_SvrDBOpr::getObj().QueryData(szBuffer, vec, nErrorCode))
			{
				const char * lErrorString = FormatErrorCode(nErrorCode);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_QryRiskEvent_Rsp, 
					(void*)lErrorString, 
					strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode);				
			}
			else
			{
				if(vec.size() != 0)
				{
					//可能同一个风险有多条记录，取消失那条的信息以及消失之前那条的风险等级信息
				/*	vector<RiskEvent> vecSend;
					std::set<int> setRiskEventID;
					int nCount = 0;
					for(int k =0; k < (int)vec.size(); k++)
					{
						RiskEvent& riskEvent = vec[k];
						int nSize  = setRiskEventID.size();
						setRiskEventID.insert(riskEvent.nRiskEventID);
						int nSize2 = setRiskEventID.size();
						if(nSize != nSize2)
						{
							vecSend.push_back(riskEvent);
							if(riskEvent.nRiskLevelID == 0)
								nCount = 1;						
						}
						else
						{
							if(nCount == 1)
							{
								int nSendSize = vecSend.size();
								if(nSendSize >0)
								{
									RiskEvent& risk = vecSend[nSendSize-1];
									risk.nRiskLevelID = riskEvent.nRiskLevelID;
									risk.dblIndicatorValue = riskEvent.dblIndicatorValue;
									nCount = 2;
								}
							}
						}
					}*/
					CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryRiskEvent_Rsp,
						&vec[0],sizeof(RiskEvent)*vec.size(),PkgHead.seq,0,CF_ERROR_SUCCESS);	
					
				}
				else
					CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryRiskEvent_Rsp,
					NULL, 0,PkgHead.seq,0,CF_ERROR_SUCCESS);
			}
		}
		break;
	default:
		break;
	}
}/*
void SendMessage( std::vector<MsgSendStatus> &vForeAccount, MessageInfo msgInfo )
{
	//给在线用户发送消息
	if ( NULL == m_pThreadSharedData )
	{
		return;
	}

	std::vector<MsgSendStatus>::iterator it = vForeAccount.begin();
	for ( ; it != vForeAccount.end(); it++)
	{
		(*it).nSendStatus = 0;

		std::vector<SOCKET>  nVecSocket;
		if(CF_ERROR_SUCCESS != CInterface_SvrLogin::getObj().GetUserSockets((*it).nRiskMgmtUserID, nVecSocket))
			continue;

		if ( nVecSocket.empty())
		{
			continue;
		}
		std::vector<SOCKET>::iterator it_socket = nVecSocket.begin();
		for ( ; it_socket != nVecSocket.end(); it_socket++ )
		{
			CInterface_SvrTcp::getObj().SendPkgData( *it_socket,Cmd_RM_SendUnReadMessage_Push,
				&msgInfo, sizeof(msgInfo),0,0,CF_ERROR_SUCCESS);		
		}
		(*it).nSendStatus = 1;		
	}

	//未发送的信息保存，待下次用户登录时发送
	CInterface_SvrDBOpr::getObj().SaveMessageSendStatus(vForeAccount);
}*/
/*
void ProcessMessageRequest(int hSocket,const Stru_UniPkgHead& PkgHead,const void*pPkgData)
{
	switch(PkgHead.cmdid)
	{
	case Cmd_RM_AddMessage_Req:
		{
			if ( !(PkgHead.len > sizeof(MessageInfo) &&
				(PkgHead.len-sizeof(MessageInfo)) % sizeof(TargetAccount) == 0))
			{
				return;
			}

			MessageInfo msgInfo = *(MessageInfo*)pPkgData;
			vector<TargetAccount> vTargetAccountID;
			int nCount = (PkgHead.len - sizeof(MessageInfo)) / sizeof(TargetAccount);
			for ( int i = 0; i < nCount; i++ )
			{
				TargetAccount account = *(TargetAccount*)((char*)pPkgData + sizeof(MessageInfo) + i*sizeof(TargetAccount));
				vTargetAccountID.push_back(account);
			}

			int nErrorCode = CF_ERROR_SUCCESS;
			int nPKID = 0;
			if ( !CInterface_SvrDBOpr::getObj().AddMessage(msgInfo, vTargetAccountID, nPKID, nErrorCode))
			{
				const char * lErrorString = FormatErrorCode(nErrorCode);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_AddMessage_Rsp, 
					(void*)lErrorString, 
					strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode);	
			}
			else
			{
				CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_AddMessage_Rsp,
					&nPKID, sizeof(nPKID),PkgHead.seq,0,CF_ERROR_SUCCESS);	
				
				msgInfo.nMessageID = nPKID;
				std::vector<MsgSendStatus> vForeAccount;
				for ( UINT i = 0; i < vTargetAccountID.size(); i++ )
				{
					MsgSendStatus msgSendStatus;
					memset(&msgSendStatus, 0, sizeof(msgSendStatus));
					msgSendStatus.nMessageID = nPKID;
					msgSendStatus.nRiskMgmtUserID = vTargetAccountID[i].nRiskMgmtUserID;
					safestrcpy(msgSendStatus.szAccount, sizeof(msgSendStatus.szAccount), vTargetAccountID[i].szAccount);
					msgSendStatus.nSendStatus = 0;
					vForeAccount.push_back(msgSendStatus);
				}

				SendMessage(vForeAccount, msgInfo);
			}
		}
		break;
	case Cmd_RM_QrySendMessage_Req:
		{
			if ( PkgHead.len != sizeof(MessageQryCondition))
			{
				return;
			}

			MessageQryCondition key = *(MessageQryCondition*)pPkgData;
			char szBuffer[MAX_SQL_LENGTH];
			memset(szBuffer, 0, sizeof(szBuffer));
			sprintf(szBuffer, "select t.messageid,t.title,t.content,\
							  to_char(t.expireddate,'YYYY-MM-DD'),t.ownerid,\
							  to_char(t.createdate,'YYYY-MM-DD HH24:MI:SS'),t.ownername \
							  from RISK_MESSAGEINFO t where t.ownerid = %d and \
							  to_char(t.createdate,'YYYY-MM-DD') = '%s' order by t.messageid",
							  key.nRiskMgmtUserID, key.szCreateDate);

			int nErrorCode = CF_ERROR_SUCCESS;
			vector<MessageInfo> vec;
			if ( !CInterface_SvrDBOpr::getObj().QueryData(szBuffer, vec, nErrorCode) )
			{
				const char * lErrorString = FormatErrorCode(nErrorCode);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_QrySendMessage_Rsp, 
					(void*)lErrorString, 
					strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode);	
			}
			else
			{
				if(vec.size() == 0)
					return;
				CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QrySendMessage_Rsp,
					&vec[0], sizeof(MessageInfo)*vec.size(),PkgHead.seq,0,CF_ERROR_SUCCESS);				
			}
		}
		break;
	case Cmd_RM_QryRecvMessage_Req:
		{
			if ( PkgHead.len != sizeof(MessageQryCondition))
			{
				return;
			}

			MessageQryCondition key = *(MessageQryCondition*)pPkgData;
			char szBuffer[MAX_SQL_LENGTH];
			memset(szBuffer, 0, sizeof(szBuffer));
			sprintf(szBuffer, "select t2.messageid,t2.title,t2.content,\
							  to_char(t2.expireddate,'YYYY-MM-DD'),t2.ownerid,\
							  to_char(t2.createdate,'YYYY-MM-DD HH24:MI:SS'),t2.ownername from RISK_MESSAGETARGET t1 \
							  left join RISK_MESSAGEINFO t2 on t1.messageid = t2.messageid \
							  where t1.riskmgmtuserid = %d and \
							  to_char(t2.createdate,'YYYY-MM-DD') = '%s' \
							  order by t2.messageid",
							  key.nRiskMgmtUserID, key.szCreateDate);
			int nErrorCode = CF_ERROR_SUCCESS;
			vector<MessageInfo> vec;
			if ( !CInterface_SvrDBOpr::getObj().QueryData(szBuffer, vec, nErrorCode) )
			{
				const char * lErrorString = FormatErrorCode(nErrorCode);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_QryRecvMessage_Rsp, 
					(void*)lErrorString, 
					strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode);
			}
			else
			{
				if(vec.size() == 0)
					return;
				CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryRecvMessage_Rsp,
					&vec[0], sizeof(MessageInfo)*vec.size(),PkgHead.seq,0,CF_ERROR_SUCCESS);				
			}
		}
		break;
	case Cmd_RM_QryMsgSendStatus_Req:
		{
			if ( PkgHead.len != sizeof(MessageQryCondition))
			{
				return;
			}

			MessageQryCondition key = *(MessageQryCondition*)pPkgData;
			char szBuffer[MAX_SQL_LENGTH];
			memset(szBuffer, 0, sizeof(szBuffer));
			sprintf(szBuffer, "select t1.* from RISK_MESSAGETARGET t1 \
							  left join RISK_Messageinfo t2 on t1.messageid = t2.messageid \
							  where t2.ownerid = %d and \
							  to_char(t2.createdate,'YYYY-MM-DD') = '%s' order by t2.messageid",
							  key.nRiskMgmtUserID, key.szCreateDate);

			int nErrorCode = CF_ERROR_DATABASE_OTHER_ERROR;
			vector<MsgSendStatus> vec;
			if ( !CInterface_SvrDBOpr::getObj().QueryData(szBuffer, vec, nErrorCode) )
			{
				const char * lErrorString = FormatErrorCode(nErrorCode);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_QryMsgSendStatus_Rsp, 
					(void*)lErrorString, 
					strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode);				
			}
			else
			{
				if(vec.size() == 0)
					return;
				CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryMsgSendStatus_Rsp,
					&vec[0], sizeof(MsgSendStatus)*vec.size(),PkgHead.seq,0,CF_ERROR_SUCCESS);				
			}
		}
		break;
	case Cmd_RM_QryUnReadMessage_Req:
		{
			if ( PkgHead.len != sizeof(int))
			{
				return;
			}

			int nOwnerID = *(int*)pPkgData;
			char szBuffer[MAX_SQL_LENGTH];
			memset(szBuffer, 0, sizeof(szBuffer));
			sprintf(szBuffer, "select t2.messageid,t2.title,t2.content,t2.expireddate,\
							  t2.ownerid,t2.createdate,t2.ownername from risk_messagetarget t1 left join risk_messageinfo t2\
							  on t1.messageid = t2.messageid where t1.riskmgmtuserid = %d and t1.sendstatus=0 \
							  and t2.expireddate > (select trunc(sysdate) from dual)",
							  nOwnerID);
			int nErrorCode = CF_ERROR_DATABASE_OTHER_ERROR;
			vector<MessageInfo> vec;
			if ( !CInterface_SvrDBOpr::getObj().QueryData(szBuffer, vec, nErrorCode) )
			{
				const char * lErrorString = FormatErrorCode(nErrorCode);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_SendUnReadMessage_Rsp, 
					(void*)lErrorString, 
					strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode);	
			}
			else
			{
				if(vec.size() == 0)
					return;
				CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_SendUnReadMessage_Rsp,
					&vec[0], sizeof(MessageInfo)*vec.size(),PkgHead.seq,0,CF_ERROR_SUCCESS);		

			

				//发送成功后，删除已发送的数据
				memset(szBuffer, 0, sizeof(szBuffer));
				sprintf(szBuffer,  "update RISK_MESSAGETARGET t set t.sendstatus = 1 where t.riskmgmtuserid = %d",
					nOwnerID);
				int nErrorCode = CF_ERROR_DATABASE_OTHER_ERROR;
				CInterface_SvrDBOpr::getObj().Excute(szBuffer, nErrorCode);
			}

		}
		break;
	default:
		break;
	}
}/*
void ProcessQuotRequest(int hSocket,const Stru_UniPkgHead& PkgHead,const void*pPkgData)
{
	switch(PkgHead.cmdid)
	{		
	case Cmd_RM_SubscribeQuot_Req://订阅行情请求
		{
			int pkgLen=PkgHead.len;
			int AccCnt=pkgLen/sizeof(TInstrumentIDType);
			int AccRem=pkgLen%sizeof(TInstrumentIDType);
			if (pkgLen>0||AccRem==0)
			{
				std::vector<PlatformStru_DepthMarketData> vec;
				TInstrumentIDType* pInstrumentID = (TInstrumentIDType*)pPkgData;
				for (int index=0;index<AccCnt;++index)
				{
					std::string strInstr=pInstrumentID[index];
					m_pThreadSharedData->Subscribe(hSocket,SubscribeID_Quot,strInstr);
					SAFE_GET_DATACENTER()->SubscribeQuot(strInstr);
					
					PlatformStru_DepthMarketData outData;
					if(0 == CInterface_SvrTradeData::getObj().GetQuotInfo(strInstr, outData))
						vec.push_back(outData);
				
					//SAFE_GET_DATACENTER()->GetQuotList(strInstr,vec);
				}
				if(vec.size() == 0)
					return;
				CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_SubscribeQuot_Rsp,
					&vec[0], sizeof(PlatformStru_DepthMarketData)*vec.size(),PkgHead.seq,0,CF_ERROR_SUCCESS);			
			}
		}
		break;
	case Cmd_RM_UnSubscribeQuot_Req://退订行情请求
		{
			int pkgLen=PkgHead.len;
			int AccCnt=pkgLen/sizeof(TInstrumentIDType);
			int AccRem=pkgLen%sizeof(TInstrumentIDType);
			if (pkgLen>0||AccRem==0)
			{
				TInstrumentIDType* pInstrumentID = (TInstrumentIDType*)pPkgData;
				for (int index=0;index<AccCnt;++index)
				{
					std::string strInstr=pInstrumentID[index];
					m_pThreadSharedData->UnSubscribe(hSocket,SubscribeID_Quot,strInstr);
					SAFE_GET_DATACENTER()->UnSubscribeQuot(strInstr);
				}
				CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_UnSubscribeQuot_Rsp,
					(void*)pPkgData, PkgHead.len,PkgHead.seq,0,CF_ERROR_SUCCESS);			
				
			}
		}
		break;
	default:
		break;
	}
}*/

void ProcessTradeRequest(int hSocket,const Stru_UniPkgHead& PkgHead,const void*pPkgData)
{
	switch(PkgHead.cmdid)
	{		
	case Cmd_RM_SubscribeTrade_Req://订阅成交请求
		{
			int pkgLen=PkgHead.len;
			int AccCnt=pkgLen/sizeof(int);
			int AccRem=pkgLen%sizeof(int);
			if (pkgLen>0||AccRem==0)
			{
				std::vector<PlatformStru_TradeInfo> vec;
				int* pAccountID = (int*)pPkgData;
				for (int index=0;index<AccCnt;++index)
				{
					int AccID=pAccountID[index];
					m_pThreadSharedData->Subscribe(hSocket,SubscribeID_Trade,AccID);
					SAFE_GET_DATACENTER()->SubscribeTrade(AccID);				
					//SAFE_GET_DATACENTER()->GetTradeList(AccID,vec);
					UserInfo userInfo;
					std::string strAcc;
					bool bFind = CInterface_SvrUserOrg::getObj().GetUserByID(AccID, userInfo);    
					if(bFind)
					{
						std::string  strUserID = userInfo.szAccount;
						std::vector<PlatformStru_TradeInfo> vecTemp;
						CInterface_SvrTradeData::getObj().GetUserTradeInfo(strUserID,	vecTemp);
						if(vecTemp.size() >0)
							vec.insert(vec.end(), vecTemp.begin(), vecTemp.end());
					}
				}
				if(vec.size() != 0)
					CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_SubscribeTrade_Rsp,
						&vec[0], sizeof(PlatformStru_TradeInfo)*vec.size(),PkgHead.seq,0,CF_ERROR_SUCCESS);		
				else
					CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_SubscribeTrade_Rsp,
					NULL, 0,PkgHead.seq,0,CF_ERROR_SUCCESS);		
			}
		}
		break;
	case Cmd_RM_UnSubscribeTrade_Req://退订成交请求
		{
			int pkgLen= PkgHead.len;
			int AccCnt=pkgLen/sizeof(int);
			int AccRem=pkgLen%sizeof(int);
			if (pkgLen>0||AccRem==0)
			{
				int* pAccountID = (int*)pPkgData;
				for (int index=0;index<AccCnt;++index)
				{
					int AccID=pAccountID[index];
					m_pThreadSharedData->UnSubscribe(hSocket,SubscribeID_Trade,AccID);
					SAFE_GET_DATACENTER()->UnSubscribeTrade(AccID);
				}
				CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_UnSubscribeTrade_Rsp,
					(void*)pPkgData, PkgHead.len,PkgHead.seq,0,CF_ERROR_SUCCESS);		
			}
		}
		break;
	default:
		break;
	}
}
void ProcessFundRequest(int hSocket,const Stru_UniPkgHead& PkgHead,const void*pPkgData)
{
	switch(PkgHead.cmdid)
	{		
	case Cmd_RM_SubscribeFund_Req://订阅账户资金请求
		{
			int pkgLen=PkgHead.len;
			int AccCnt=pkgLen/sizeof(int);
			int AccRem=pkgLen%sizeof(int);
			if (pkgLen>0||AccRem==0)
			{
				std::vector<PlatformStru_TradingAccountInfo> vec;				
				int* pAccountID = (int*)pPkgData;
				for (int index=0;index<AccCnt;++index)
				{
					int AccID=pAccountID[index];
					m_pThreadSharedData->Subscribe(hSocket,SubscribeID_Fund,AccID);					
					SAFE_GET_DATACENTER()->SubscribeFund(AccID);

					UserInfo userInfo;
					std::string strAcc;
					bool bFind = CInterface_SvrUserOrg::getObj().GetUserByID(AccID, userInfo);    
					if(bFind)
					{
						std::string  strUserID = userInfo.szAccount;
						PlatformStru_TradingAccountInfo tradingAccountInfo;
						if(CF_ERROR_SUCCESS == CInterface_SvrTradeData::getObj().GetUserFundInfo(strUserID, tradingAccountInfo))
							vec.push_back(tradingAccountInfo);
						
					}					
				}
				if(vec.size() != 0)
					CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_SubscribeFund_Rsp,
						&vec[0], sizeof(PlatformStru_TradingAccountInfo)*vec.size(),PkgHead.seq,0,CF_ERROR_SUCCESS);	
				else
					CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_SubscribeFund_Rsp,
						NULL, 0,PkgHead.seq,0,CF_ERROR_SUCCESS);	
			}
		}
		break;
	case Cmd_RM_UnSubscribeFund_Req://退订账户资金请求
		{
			int pkgLen=PkgHead.len;
			int AccCnt=pkgLen/sizeof(int);
			int AccRem=pkgLen%sizeof(int);
			if (pkgLen>0||AccRem==0)
			{
				int* pAccountID = (int*)pPkgData;
				for (int index=0;index<AccCnt;++index)
				{
					int AccID=pAccountID[index];
					m_pThreadSharedData->UnSubscribe(hSocket,SubscribeID_Fund,AccID);					
					SAFE_GET_DATACENTER()->UnSubscribeFund(AccID);			
				}
				CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_UnSubscribeFund_Rsp,
					(void*)pPkgData, PkgHead.len,PkgHead.seq,0,CF_ERROR_SUCCESS);	
			}
		}
		break;
	}

}
void ProcessPositionRequest(int hSocket,const Stru_UniPkgHead& PkgHead,const void*pPkgData)
{
	switch(PkgHead.cmdid)
	{		
	case Cmd_RM_SubscribePosition_Req://订阅持仓请求
		{
			int pkgLen=PkgHead.len;
			int AccCnt=pkgLen/sizeof(int);
			int AccRem=pkgLen%sizeof(int);
			if (pkgLen>0||AccRem==0)
			{
				std::vector<PlatformStru_Position> vec;
				int* pAccountID = (int*)pPkgData;
				for (int index=0;index<AccCnt;++index)
				{
					int AccID=pAccountID[index];
					m_pThreadSharedData->Subscribe(hSocket,SubscribeID_Position,AccID);
					SAFE_GET_DATACENTER()->SubscribePosition(AccID);

					UserInfo userInfo;
					std::string strAcc;
					bool bFind = CInterface_SvrUserOrg::getObj().GetUserByID(AccID, userInfo);    
					if(bFind)
					{
						std::string  strUserID = userInfo.szAccount;	
						std::vector<PlatformStru_Position> vecTemp;
						CInterface_SvrTradeData::getObj().GetUserPositions(strUserID, "", vecTemp);		
						if(vecTemp.size()>0)
							vec.insert(vec.end(), vecTemp.begin(), vecTemp.end());
					}						
				}
				if(vec.size() != 0)
					CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_SubscribePosition_Rsp,
					&vec[0], sizeof(PlatformStru_Position)*vec.size(),PkgHead.seq,0,CF_ERROR_SUCCESS);	
				else
					CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_SubscribePosition_Rsp,
					NULL, 0,PkgHead.seq,0,CF_ERROR_SUCCESS);
			}
		}
		break;
	case Cmd_RM_UnSubscribePosition_Req://退订持仓请求
		{
			int pkgLen=PkgHead.len;
			int AccCnt=pkgLen/sizeof(int);
			int AccRem=pkgLen%sizeof(int);
			if (pkgLen>0||AccRem==0)
			{
				int* pAccountID = (int*)pPkgData;
				for (int index=0;index<AccCnt;++index)
				{
					int AccID=pAccountID[index];
					m_pThreadSharedData->UnSubscribe(hSocket,SubscribeID_Position,AccID);
					SAFE_GET_DATACENTER()->UnSubscribePosition(AccID);
				}
				CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_UnSubscribePosition_Rsp,
					(void*)pPkgData, PkgHead.len,PkgHead.seq,0,CF_ERROR_SUCCESS);	
			}
		}
		break;
//	case Cmd_RM_QryHistroyPosition_Req:
//		{
//		}
//		break;
	default:
		break;
	}
}
void ProcessOrderRequest(int hSocket,const Stru_UniPkgHead& PkgHead,const void*pPkgData)
{
	switch(PkgHead.cmdid)
	{		
	case Cmd_RM_SubscribeOrder_Req://订阅报单请求
		{
			int pkgLen=PkgHead.len;
			int AccCnt=pkgLen/sizeof(int);
			int AccRem=pkgLen%sizeof(int);
			if (pkgLen>0||AccRem==0)
			{
				std::vector<PlatformStru_OrderInfo> vec;
				int* pAccountID = (int*)pPkgData;
				for (int index=0;index<AccCnt;++index)
				{
					int AccID=pAccountID[index];
					m_pThreadSharedData->Subscribe(hSocket,SubscribeID_Order,AccID);
					SAFE_GET_DATACENTER()->SubscribeOrder(AccID);


					UserInfo userInfo;
					std::string strAcc;
					bool bFind = CInterface_SvrUserOrg::getObj().GetUserByID(AccID, userInfo);    
					if(bFind)
					{
						std::string  strUserID = userInfo.szAccount;	
						std::vector<PlatformStru_OrderInfo> vecTemp;
						CInterface_SvrTradeData::getObj().GetUserOrders(strUserID,  vecTemp);	
						if(vecTemp.size()>0)
							vec.insert(vec.end(), vecTemp.begin(), vecTemp.end());
					}	
				}
				if(vec.size() != 0)
					CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_SubscribeOrder_Rsp,
					&vec[0], sizeof(PlatformStru_OrderInfo)*vec.size(),PkgHead.seq,0,CF_ERROR_SUCCESS);		
				else
					CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_SubscribeOrder_Rsp,
					NULL, 0,PkgHead.seq,0,CF_ERROR_SUCCESS);
			}
		}
		break;
	case Cmd_RM_UnSubscribeOrder_Req://退订报单请求
		{
			int pkgLen=PkgHead.len;
			int AccCnt=pkgLen/sizeof(int);
			int AccRem=pkgLen%sizeof(int);
			if (pkgLen>0||AccRem==0)
			{
				int* pAccountID = (int*)pPkgData;
				for (int index=0;index<AccCnt;++index)
				{
					int AccID=pAccountID[index];
					m_pThreadSharedData->UnSubscribe(hSocket,SubscribeID_Order,AccID);
					SAFE_GET_DATACENTER()->UnSubscribeOrder(AccID);
				}
				CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_UnSubscribeOrder_Rsp,
					(void*)pPkgData, PkgHead.len,PkgHead.seq,0,CF_ERROR_SUCCESS);	
			}
		}
		break;
	case Cmd_RM_QryHistroyOrder_Req:
		{			
		}
		break;
	case Cmd_RM_VerifyOrder_Req:
		{
			if ( PkgHead.len != sizeof(SVerisyOrder))
			{
				const char * lErrorString = FormatErrorCode(CF_ERROR_COMMON_INPUT_PARAM);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_VerifyOrder_Rsp, 
					(void*)lErrorString, strlen(lErrorString)+1, PkgHead.seq, 0, CF_ERROR_COMMON_INPUT_PARAM);
				return;
			}
			SVerisyOrder verifyOrder = *(SVerisyOrder*)pPkgData;
			char szBuffer[MAX_SQL_LENGTH];
			memset(szBuffer, 0, sizeof(szBuffer));
			sprintf(szBuffer, "update  RISK_VERIFYORDER t set t.verifyuser = %d, t.result = %d \
				, t.verifytime = sysdate where  t.investorid = '%s' \
				and t.instrumentid = '%s' and t.frontid = %d and t.sessionid = %d and t.orderref = '%s'",
				verifyOrder.nVerifyUser,
				verifyOrder.nResult,
				verifyOrder.orderKey.Account,
				verifyOrder.orderKey.InstrumentID,
				verifyOrder.orderKey.FrontID,
				verifyOrder.orderKey.SessionID,
				verifyOrder.orderKey.OrderRef				
				);

			int nErrorCode = CF_ERROR_SUCCESS;
			CInterface_SvrDBOpr::getObj().Excute(szBuffer, nErrorCode);
			if(nErrorCode != CF_ERROR_SUCCESS)
			{
				CInterface_SvrDBOpr::getObj().SaveVertifyOrder2File(verifyOrder );
			}
			bool bPass = true;
			if(verifyOrder.nResult ==0)
				bPass = false;	
		/*	if(!g_bSendToExcute)
			{
				CF_ERROR bSet =  CInterface_SvrTradeExcute::getObj().SetVerifyOrder(verifyOrder.orderKey, bPass);			
				if(bSet == CF_ERROR_SUCCESS && nErrorCode == CF_ERROR_SUCCESS)
					CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_VerifyOrder_Rsp,
					(void*)pPkgData, PkgHead.len,PkgHead.seq,0,CF_ERROR_SUCCESS);	
				else
					CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_VerifyOrder_Rsp,
					(void*)pPkgData, PkgHead.len,PkgHead.seq,0,CF_ERROR_SUCCESS);	
			}*/
			if(g_bSendToExcute)
			{
				int nSocket =0;
				InterlockedExchange((volatile long*)(&nSocket),g_hSocket);
				if(nSocket)
				{					
					int nSeqID = InsertmapSeqID2Socket(hSocket);		
					CInterface_SvrTcp::getObj().SendPkgData(nSocket,Cmd_RM_ToExecute_VerifyOrder_Req,
						&verifyOrder, sizeof(SVerisyOrder),0,0,CF_ERROR_SUCCESS,0,0,nSeqID);	
				}
				else
				{//输出socket无效				
					RISK_LOGINFO("主从服务器断开，审核请求失败\n");
				}
			}
			
		}
		break;
	case Cmd_RM_QryVerifyOrder_Req:
		{	
			if(!CInterface_SvrNotifyAndAsk::getObj().Ask_ValidateUserPrivilege(PkgHead.userdata1,RISK_ORDER_VERIFY_RECORD_QUERY))
			{
				const char * lErrorString = FormatErrorCode(CF_ERROR_ORDER_VERIFY_RECORD_QUERY_NOPRIVILEDGE);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_QryVerifyOrder_Rsp, 
					(void*)lErrorString, 
					strlen(lErrorString)+1, PkgHead.seq,0,CF_ERROR_COMMON_NO_PRIVILEGE);
				break;
			}

			if ( PkgHead.len != sizeof(SQueryOrder))
			{
				const char * lErrorString = FormatErrorCode(CF_ERROR_COMMON_INPUT_PARAM);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_QryVerifyOrder_Rsp, 
					(void*)lErrorString, strlen(lErrorString)+1, PkgHead.seq, 0, CF_ERROR_COMMON_INPUT_PARAM);
				return;
			}
			SQueryOrder verifyOrder = *(SQueryOrder*)pPkgData;
			
			std::string strSql = "select t1.*, t2.*, t3.account from (TRADEDATA_ORDERS t1 left join  RISK_VERIFYORDER  t2\
				on  t1.investorid = t2.investorid and t1.instrumentid = t2.instrumentid\
				and t1.frontid = t2.frontid and t1.sessionid = t2.sessionid\
				and t1.orderref = t2.orderref)  left join BASE_USER t3 on(t2.verifyuser = t3.USERID) where ";
			
			
			std::string strInstrument="";
			std::set<std::string> setInstrument;
			if(verifyOrder.bIsProduct)
			{
				CInterface_SvrTradeData::getObj().GetInstrumentListByProductID(verifyOrder.InstrumentID, setInstrument);
				std::set<std::string>::iterator it = setInstrument.begin();
				for(;it != setInstrument.end(); it++)
				{
					std::string str = *it;
					if(it == setInstrument.begin())
					{
						std::string strName = "(t1.instrumentid = ''";
						strInstrument = strName;
						int nLength = strName.length() -1;
						strInstrument.insert(nLength,  str);
						continue;
					}					

					std::string strNameOr = " or t1.instrumentid = ''";
					int nLengthOr = strNameOr.length() -1;
					strInstrument.insert(strInstrument.length(),  strNameOr);		
					strInstrument.insert(strInstrument.length()-1,  str);					
				}	
				if(strInstrument.length())
					strInstrument.append(1, ')');			
			}
			else
			{
				if(strlen(verifyOrder.InstrumentID)>0)
				{
					std::string strName = "(t1.instrumentid = '')";
					strInstrument = strName;
					int nLength = strName.length() -1;
					strInstrument.insert(nLength-1,  verifyOrder.InstrumentID);
				}
			}
				
			bool bAnd = false;
			if(strInstrument.length() >0)
			{
				strSql.insert(strSql.length(), strInstrument);
				bAnd = true;
			}
			if(strcmp(verifyOrder.Account, "") != 0)
			{
				std::string strName = "t1.investorid = ''";
				if(bAnd)
					strName = "and t1.investorid = ''";
				strName.insert(strName.length()-1, verifyOrder.Account);
				strSql.insert(strSql.length() , strName);
				bAnd = true;
			}
			if(strcmp(verifyOrder.szVerifyDate, "") != 0)
			{
				std::string strName = "to_char(t2.VERIFYTIME,'YYYY-MM-DD') = ''";
				if(bAnd)
					strName = " and  to_char(t2.VERIFYTIME,'YYYY-MM-DD') = ''";
				strName.insert(strName.length()-1, verifyOrder.szVerifyDate);
				strSql.insert(strSql.length() , strName);
				bAnd = true;
			}	
			if(verifyOrder.nResult != -2)
			{
				std::string strName = " t2.Result = "; 
				if(bAnd)
					strName = " and t2.Result = ";

				char szResult[256];				
				sprintf(szResult, "%d", verifyOrder.nResult);
				std::string strResult =  szResult;
				
				strName.insert(strName.length()-1, strResult);
				strSql.insert(strSql.length() , strName);
				bAnd = true;
			}
			std::vector<SQueryOrderResult> vec;

			std::vector<std::vector<_variant_t>> vNode;
			int nErrorCode = CF_ERROR_SUCCESS;
			if ( !CInterface_SvrDBOpr::getObj().QueryData(strSql.c_str(), vNode, nErrorCode))
			{
				const char * lErrorString = FormatErrorCode(nErrorCode);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryVerifyOrder_Rsp,
					(void*)lErrorString, strlen(lErrorString)+1,PkgHead.seq,0,nErrorCode);	
				return ;
			}
			else
			{ 	
				std::set<OrderKey> setOrderKey; 
				for ( UINT j = 0; j < vNode.size(); j++ )
				{
					std::vector<_variant_t> vValue = vNode[j];
					SQueryOrderResult sResult;
					
					int i = 0;
					strcpy(sResult.order.BrokerID, vValue[i++].operator _bstr_t());
					strcpy(sResult.order.InvestorID, vValue[i++].operator _bstr_t());
					strcpy(sResult.order.InstrumentID, vValue[i++].operator _bstr_t());
					strcpy(sResult.order.OrderRef, vValue[i++].operator _bstr_t());
					strcpy(sResult.order.UserID, vValue[i++].operator _bstr_t());
					sResult.order.OrderPriceType = vValue[i++].cVal;
					sResult.order.Direction		 = vValue[i++].cVal;
					strcpy(sResult.order.CombOffsetFlag, vValue[i++].operator _bstr_t());
					strcpy(sResult.order.CombHedgeFlag, vValue[i++].operator _bstr_t());
					sResult.order.LimitPrice = vValue[i++].dblVal;
					sResult.order.VolumeTotalOriginal = vValue[i++].intVal;
					sResult.order.TimeCondition		 = vValue[i++].cVal;
					strcpy(sResult.order.GTDDate, vValue[i++].operator _bstr_t());
					sResult.order.VolumeCondition		 = vValue[i++].cVal;
					sResult.order.MinVolume = vValue[i++].intVal;
					sResult.order.ContingentCondition		 = vValue[i++].cVal;
					sResult.order.StopPrice = vValue[i++].dblVal;
					sResult.order.ForceCloseReason		 = vValue[i++].cVal;
					sResult.order.IsAutoSuspend = vValue[i++].intVal;
					strcpy(sResult.order.BusinessUnit, vValue[i++].operator _bstr_t());
					sResult.order.RequestID = vValue[i++].intVal;
					strcpy(sResult.order.OrderLocalID, vValue[i++].operator _bstr_t());
					strcpy(sResult.order.ExchangeID, vValue[i++].operator _bstr_t());
					strcpy(sResult.order.ParticipantID, vValue[i++].operator _bstr_t());
					strcpy(sResult.order.ClientID, vValue[i++].operator _bstr_t());	
					strcpy(sResult.order.ExchangeInstID, vValue[i++].operator _bstr_t());						
					strcpy(sResult.order.TraderID, vValue[i++].operator _bstr_t());	
					sResult.order.InstallID = vValue[i++].intVal;
					sResult.order.OrderSubmitStatus		 = vValue[i++].cVal;				
					sResult.order.NotifySequence = vValue[i++].intVal;
					strcpy(sResult.order.TradingDay, vValue[i++].operator _bstr_t());							
					sResult.order.SettlementID = vValue[i++].intVal;
					strcpy(sResult.order.OrderSysID, vValue[i++].operator _bstr_t());		
					sResult.order.OrderSource		 = vValue[i++].cVal;	
					sResult.order.OrderStatus		 = vValue[i++].cVal;
					sResult.order.OrderType		 = vValue[i++].cVal;						
					sResult.order.VolumeTraded = vValue[i++].intVal;
					sResult.order.VolumeTotal = vValue[i++].intVal;
					strcpy(sResult.order.InsertDate, vValue[i++].operator _bstr_t());	
					strcpy(sResult.order.InsertTime, vValue[i++].operator _bstr_t());					
					strcpy(sResult.order.ActiveTime, vValue[i++].operator _bstr_t());						
					strcpy(sResult.order.SuspendTime, vValue[i++].operator _bstr_t());
					strcpy(sResult.order.UpdateTime, vValue[i++].operator _bstr_t());
					strcpy(sResult.order.CancelTime, vValue[i++].operator _bstr_t());
					strcpy(sResult.order.ActiveTraderID, vValue[i++].operator _bstr_t());
					strcpy(sResult.order.ClearingPartID, vValue[i++].operator _bstr_t());
					sResult.order.SequenceNo = vValue[i++].intVal;
					sResult.order.FrontID = vValue[i++].intVal;
					sResult.order.SessionID = vValue[i++].intVal;
					strcpy(sResult.order.UserProductInfo, vValue[i++].operator _bstr_t());
					strcpy(sResult.order.StatusMsg, vValue[i++].operator _bstr_t());
					sResult.order.UserForceClose = vValue[i++].intVal;
					strcpy(sResult.order.ActiveUserID, vValue[i++].operator _bstr_t());
					sResult.order.BrokerOrderSeq = vValue[i++].intVal;
					strcpy(sResult.order.RelativeOrderSysID, vValue[i++].operator _bstr_t());
					sResult.order.AvgPrice = vValue[i++].dblVal;
					sResult.order.ExStatus =(PlatformStru_OrderInfo::EnumExStatus)vValue[i++].intVal;
					i++; //FTID
					i++; //UPDATESEQ
					i++; //VALIDATEDATE					
					i++; //INSERTDBTIME
					i++; //INVESTORID
					i++; //INSTRUMENTID
					i++; //FRONTID
					i++; //SESSIONID
					i++; //ORDERREF
					sResult.nVerifyUser = vValue[i++].intVal;
					strcpy(sResult.szVerifyDate, vValue[i++].operator _bstr_t());
					sResult.nResult = vValue[i++].intVal;
					strcpy(sResult.VerifyUserName, vValue[i++].operator _bstr_t());

					OrderKey orderkey;
					strcpy(orderkey.Account, sResult.order.InvestorID);
					strcpy(orderkey.InstrumentID, sResult.order.InstrumentID);
					orderkey.FrontID = sResult.order.FrontID;
					orderkey.SessionID = sResult.order.SessionID;
					strcpy(orderkey.OrderRef, sResult.order.OrderRef);	

					int nsize = setOrderKey.size();
					setOrderKey.insert(orderkey);//TRADEDATA_ORDERS 存了好多条，只取最后一条数据
					if(nsize != setOrderKey.size())	
						vec.push_back(sResult);			
				}
			}
			
			if(vec.size() != 0)
				CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryVerifyOrder_Rsp,
				&vec[0], sizeof(SQueryOrderResult)*vec.size(),PkgHead.seq,0,CF_ERROR_SUCCESS);	
			else
				CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryVerifyOrder_Rsp,
				NULL, 0,PkgHead.seq,0,CF_ERROR_SUCCESS);	

		}
		break;
	default:
		break;
	}
}
void ProcessNetValueRequest(int hSocket,const Stru_UniPkgHead& PkgHead,const void*pPkgData)
{
	switch(PkgHead.cmdid)
	{
	case Cmd_RM_QueryFundNetCalcResult_Req:
		{
			if(!CInterface_SvrNotifyAndAsk::getObj().Ask_ValidateUserPrivilege(PkgHead.userdata1,RISK_HISTROY_FUND_SUTTLE_QUERY))
			{
				const char * lErrorString = FormatErrorCode(CF_ERROR_HISTROY_FUND_SUTTLE_QUERY_NOPRIVILEDGE);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_QueryFundNetCalcResult_Rsp, 
					(void*)lErrorString, 
					strlen(lErrorString)+1, PkgHead.seq,0,CF_ERROR_COMMON_NO_PRIVILEGE);
				break;
			}

			if ( PkgHead.len != sizeof(RiskEventQueryKey))
			{		
				const char * lErrorString = FormatErrorCode(CF_ERROR_COMMON_INPUT_PARAM);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_QueryFundNetCalcResult_Rsp, 
					(void*)lErrorString, strlen(lErrorString)+1, PkgHead.seq, 0, CF_ERROR_COMMON_INPUT_PARAM);
				return;
			}

			RiskEventQueryKey key = *(RiskEventQueryKey*)pPkgData;

			vector<NetFundCalcResult> vec;
			char szBuffer[MAX_SQL_LENGTH];
			memset(szBuffer, 0, sizeof(szBuffer));
			sprintf(szBuffer, "select t.tradeaccountid,t.innervolumn,\
							  t.outervolumn,t.innernetasset,t.outernetasset,t.innerpernet,\
							  t.outernetasset,t.totalnetasset,to_char(t.updatedate,'YYYY-MM-DD') from RISK_FUNDNETCALCRESULT t\
							  where t.tradeaccountid = %d and to_char(t.updatedate, 'YYYY-MM-DD') >= '%s'\
							  and to_char(t.updatedate, 'YYYY-MM-DD') <= '%s'",
							  key.nTradeAccountID, key.szStartDate, key.szEndDate);
		
			int nErrorCode = CF_ERROR_SUCCESS;
			if ( !CInterface_SvrDBOpr::getObj().QueryData(szBuffer, vec, nErrorCode))
			{
				const char * lErrorString = FormatErrorCode(nErrorCode);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_QueryFundNetCalcResult_Rsp, 
					(void*)lErrorString, 
					strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode);		
			}
			else
			{
				if(vec.size() != 0)
					CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QueryFundNetCalcResult_Rsp,
						&vec[0], sizeof(NetFundCalcResult)*vec.size(),PkgHead.seq,0,CF_ERROR_SUCCESS);		
				else
					CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QueryFundNetCalcResult_Rsp,
					NULL, 0,PkgHead.seq,0,CF_ERROR_SUCCESS);	
			}	
		}
		break;
	}


}
void ProcessRisk2ExcuteRequest(int hSocket,const Stru_UniPkgHead& PkgHead,const void* pPkgData)
{
	switch(PkgHead.cmdid)
	{	
	case Cmd_RM_ToExecute_OrderAction_Rsp:
		{
			int hSocket = 0;
			bool bFind = GetmapSeqID2Socket(PkgHead.userdata4, hSocket);
			if(bFind)
			{
				if(PkgHead.userdata1 != CF_ERROR_SUCCESS)
				{			
					SRiskInputOrderAction sMClose = *(SRiskInputOrderAction*)pPkgData;
					SRiskInputOrderAction sMCloseSave = sMClose;
					std::string  nsForceReason = sMClose.nsActionReson;

					UserInfo userInfo;
					CInterface_SvrUserOrg::getObj().GetUserInfoByAccount(sMClose.nInputOrderAction.Thost.InvestorID, userInfo);	
					int nTradeInvestorID = userInfo.nUserID;

					CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_OrderAction_Rsp,
						(void*)pPkgData, PkgHead.len,PkgHead.seq,0,CF_ERROR_SUCCESS);		

					strcpy(sMClose.nInputOrderAction.Thost.InvestorID , sMCloseSave.nInputOrderAction.Thost.InvestorID);//改变回交易员
					int nErrorCode = CF_ERROR_SUCCESS;
					CInterface_SvrDBOpr::getObj().SaveForceCloseOrderAction(sMClose.nInputOrderAction, false, sMClose.szRiskerName, nErrorCode);
					if(nErrorCode != CF_ERROR_SUCCESS)
					{
						CInterface_SvrDBOpr::getObj().SaveForceCloseOrderAction2File(sMClose.nInputOrderAction, true, "");
					}
				}
				else
				{
					int nErrorCode = CF_ERROR_RISK_BASE;
					const char * lErrorString = "手动撤单失败";
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_OrderAction_Rsp, 
						(void*)lErrorString, strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode);		
				}					
			}
			else
			{//输出日志

			}
		}
		break;
	case Cmd_RM_ToExecute_ForceCloseOrderInsert_Rsp:
		{
			int hSocket = 0;
			bool bFind = GetmapSeqID2Socket(PkgHead.userdata4, hSocket);
			if(bFind)
			{
				SManualForceClose sMClose = *(SManualForceClose*)pPkgData;

				SManualForceClose sMCloseSave = sMClose;
				InputOrderKey lKey;
				lKey.nFrontID		= sMClose.nFrontID;//这个值刚好是风控员ID
				lKey.nSessionID		= sMClose.nSessionID;
				strcpy(lKey.szOrderRef, sMClose.szOrderRef);
				std::string  nsForceReason = sMClose.szReason;


				UserInfo userInfo;
				CInterface_SvrUserOrg::getObj().GetUserInfoByAccount(sMClose.nInputOrder.InvestorID, userInfo);	
				int nTradeInvestorID = userInfo.nUserID;

				//用UserForceClose = 1以及ForceCloseReason = THOST_FTDC_FCC_ForceClose代表强平单，不可撤销
				sMClose.nInputOrder.UserForceClose = 1;
				sMClose.nInputOrder.ForceCloseReason = THOST_FTDC_FCC_ForceClose;

				CF_ERROR nErrorCode = CF_ERROR_RISK_BASE;
				if(PkgHead.userdata1 == CF_ERROR_SUCCESS)
				{
						CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_ForceCloseOrderInsert_Rsp,
							(void*)pPkgData, PkgHead.len,PkgHead.seq,0,CF_ERROR_SUCCESS);		

						strcpy(sMClose.nInputOrder.InvestorID, sMCloseSave.nInputOrder.InvestorID);//改变回交易员
						int nErrorCode = CF_ERROR_SUCCESS;
						CInterface_SvrDBOpr::getObj().SaveForceCloseOrder(sMClose.nInputOrder, lKey, false, sMClose.szRiskerName, nErrorCode);
						if(nErrorCode != CF_ERROR_SUCCESS)
						{
							CInterface_SvrDBOpr::getObj().SaveForceCloseOrder2File(sMClose.nInputOrder, lKey, true, "");
						}

				}
				else
				{
						const char * lErrorString = "手动强平单下单失败";
						CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_ForceCloseOrderInsert_Rsp, 
							(void*)lErrorString, 
							strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode);					
				}	
							
			}
			else
			{//输出日志

			}
		}
		break;
	case Cmd_RM_ToExecute_VerifyOrder_Rsp:
		{
			int hSocket = 0;
			bool bFind = GetmapSeqID2Socket(PkgHead.userdata4, hSocket);
			if(bFind)
			{
				
					CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_VerifyOrder_Rsp,
						(void*)pPkgData, PkgHead.len,PkgHead.seq,0,PkgHead.userdata1);	
			
			}
			else
			{//输出日志
				CF_ERROR nErrorCode = CF_ERROR_RISK_BASE;
				const char * lErrorString = "审核失败";
				CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_VerifyOrder_Rsp,
					(void*)lErrorString, 
					strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode);			
			}
		
		}
		break;
	case Cmd_RM_Master_SetLimitTrade_Rsp:
		{
			int hSocket = 0;
			bool bFind = GetmapSeqID2Socket(PkgHead.userdata4, hSocket);
			if(bFind)
			{
				CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_SetLimitTrade_Rsp,
					(void*)pPkgData, PkgHead.len,PkgHead.seq,0,PkgHead.userdata1, 0, PkgHead.userdata1);	
			}
			else
			{//输出日志
				CF_ERROR nErrorCode = CF_ERROR_RISK_BASE;
				const char * lErrorString = "设置限制交易请求失败";
				CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_SetLimitTrade_Rsp,
					(void*)lErrorString, 
					strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode, 0, PkgHead.userdata1);			
			}

		}
		break;
	case Cmd_RM_Master_SetManualVerify_Rsp:
		{
			int hSocket = 0;
			bool bFind = GetmapSeqID2Socket(PkgHead.userdata4, hSocket);
			if(bFind)
			{
				CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_SetManualVerify_Rsp,
					(void*)pPkgData, PkgHead.len,PkgHead.seq,0,PkgHead.userdata1, 0,PkgHead.userdata1);	
			}
			else
			{//输出日志
				CF_ERROR nErrorCode = CF_ERROR_RISK_BASE;
				const char * lErrorString = "设置手动审核请求失败";
				CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_SetManualVerify_Rsp,
					(void*)lErrorString, 
					strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode, 0, PkgHead.userdata1);			
			}
		}
		break;
	case Cmd_RM_Master_ModifyUserPassword_Rsp:
		{
			int hSocket = 0;
			bool bFind = GetmapSeqID2Socket(PkgHead.userdata4, hSocket);
			if(bFind)
			{
				CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_ModifyUserPassword_Rsp,
					(void*)pPkgData, PkgHead.len,PkgHead.seq,0,PkgHead.userdata1, 0,PkgHead.userdata1);	
			}
			else
			{//输出日志
				CF_ERROR nErrorCode = CF_ERROR_RISK_BASE;
				const char * lErrorString = "修改密码请求失败";
				CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_ModifyUserPassword_Rsp,
					(void*)lErrorString, 
					strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode, 0, PkgHead.userdata1);			
			}
		}
		break;
	}
}
void ProcessForceCloseRequest(int hSocket,const Stru_UniPkgHead& PkgHead,const void* pPkgData)
{
	switch(PkgHead.cmdid)
	{	
	case Cmd_RM_OrderAction_Req:
		{
			if ( PkgHead.len != sizeof(SRiskInputOrderAction))
			{		
				const char * lErrorString = FormatErrorCode(CF_ERROR_COMMON_INPUT_PARAM);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_OrderAction_Rsp, 
					(void*)lErrorString, strlen(lErrorString)+1, PkgHead.seq, 0, CF_ERROR_COMMON_INPUT_PARAM);
				return;
			}
			SRiskInputOrderAction sMClose = *(SRiskInputOrderAction*)pPkgData;
			SRiskInputOrderAction sMCloseSave = sMClose;
			std::string  nsForceReason = sMClose.nsActionReson;

			UserInfo userInfo;
			CInterface_SvrUserOrg::getObj().GetUserInfoByAccount(sMClose.nInputOrderAction.Thost.InvestorID, userInfo);	
			int nTradeInvestorID = userInfo.nUserID;

		/*	if(!g_bSendToExcute)
			{
				CF_ERROR nErrorCode = CF_ERROR_SUCCESS;
				nErrorCode = CInterface_SvrTradeExcute::getObj().RiskForceActionOrder(sMClose.nInputOrderAction, nTradeInvestorID, sMClose.nRequestID, nsForceReason);
				if(nErrorCode == CF_ERROR_SUCCESS)
				{
					CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_OrderAction_Rsp,
						(void*)pPkgData, PkgHead.len,PkgHead.seq,0,CF_ERROR_SUCCESS);		
				
					strcpy(sMClose.nInputOrderAction.Thost.InvestorID , sMCloseSave.nInputOrderAction.Thost.InvestorID);//改变回交易员
					int nErrorCode = CF_ERROR_SUCCESS;
					CInterface_SvrDBOpr::getObj().SaveForceCloseOrderAction(sMClose.nInputOrderAction, false, sMClose.szRiskerName, nErrorCode);
				}
				else
				{
					const char * lErrorString = "手动撤单失败";
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_OrderAction_Rsp, 
						(void*)lErrorString, strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode);					
				}	
			}*/
			if(g_bSendToExcute)
			{//向交易执行发送数据的模式
				int nSocket =0;
				InterlockedExchange((volatile long*)(&nSocket),g_hSocket);
				if(nSocket)
				{									
					int nSeqID = InsertmapSeqID2Socket(hSocket);		
					CInterface_SvrTcp::getObj().SendPkgData(nSocket,Cmd_RM_ToExecute_OrderAction_Req,
						&sMClose, sizeof(SRiskInputOrderAction),0,0,CF_ERROR_SUCCESS,0,0,nSeqID);	
				}
				else
				{//输出socket无效//输出日志				
					RISK_LOGINFO("主从服务器断开，撤单请求失败\n");
				}
			}
		}
		break;
	case Cmd_RM_ForceCloseOrderInsert_Req://请求强平报单录入  
		{
			if(!CInterface_SvrNotifyAndAsk::getObj().Ask_ValidateUserPrivilege(PkgHead.userdata1,RISK_FORCE_CLOSE))
			{
				const char * lErrorString = FormatErrorCode(CF_ERROR_RISK_FORCECLOSE_NOPRIVILEDGE);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_ForceCloseOrderInsert_Rsp, 
					(void*)lErrorString, 
					strlen(lErrorString)+1, PkgHead.seq,0,CF_ERROR_COMMON_NO_PRIVILEGE);
				break;
			}

			if ( PkgHead.len != sizeof(SManualForceClose))
			{	
				const char * lErrorString = FormatErrorCode(CF_ERROR_COMMON_INPUT_PARAM);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_ForceCloseOrderInsert_Rsp, 
					(void*)lErrorString, strlen(lErrorString)+1, PkgHead.seq, 0, CF_ERROR_COMMON_INPUT_PARAM);
				return;
			}
			SManualForceClose sMClose = *(SManualForceClose*)pPkgData;
			
			SManualForceClose sMCloseSave = sMClose;
			InputOrderKey lKey;
			lKey.nFrontID		= sMClose.nFrontID;//这个值刚好是风控员ID
			lKey.nSessionID		= sMClose.nSessionID;
			strcpy(lKey.szOrderRef, sMClose.szOrderRef);
			std::string  nsForceReason = sMClose.szReason;


			UserInfo userInfo;
			CInterface_SvrUserOrg::getObj().GetUserInfoByAccount(sMClose.nInputOrder.InvestorID, userInfo);	
			int nTradeInvestorID = userInfo.nUserID;
			
			//用UserForceClose = 1以及ForceCloseReason = THOST_FTDC_FCC_ForceClose代表强平单，不可撤销
			sMClose.nInputOrder.UserForceClose = 1;
			sMClose.nInputOrder.ForceCloseReason = THOST_FTDC_FCC_ForceClose;

		/*	if(!g_bSendToExcute)
			{
				CF_ERROR nErrorCode = CF_ERROR_SUCCESS;
				nErrorCode = CInterface_SvrTradeExcute::getObj().RiskForceCloseOrder(sMClose.nInputOrder, lKey, nTradeInvestorID, nsForceReason);
				if(nErrorCode == CF_ERROR_SUCCESS)
				{
					CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_ForceCloseOrderInsert_Rsp,
						(void*)pPkgData, PkgHead.len,PkgHead.seq,0,CF_ERROR_SUCCESS);		
				
					strcpy(sMClose.nInputOrder.InvestorID, sMCloseSave.nInputOrder.InvestorID);//改变回交易员
					int nErrorCode = CF_ERROR_SUCCESS;
					CInterface_SvrDBOpr::getObj().SaveForceCloseOrder(sMClose.nInputOrder, lKey, false, sMClose.szRiskerName, nErrorCode);

				}
				else
				{
					const char * lErrorString = "手动强平单下单失败";
					CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_ForceCloseOrderInsert_Rsp, 
						(void*)lErrorString, 
						strlen(lErrorString)+1, PkgHead.seq,0,nErrorCode);					
				}		
			}*/
			if(g_bSendToExcute)
			{
				//向交易执行发送数据的模式
				int nSocket =0;
				InterlockedExchange((volatile long*)(&nSocket),g_hSocket);
				if(nSocket)
				{
					int nSeqID = InsertmapSeqID2Socket(hSocket);		
					CInterface_SvrTcp::getObj().SendPkgData(nSocket,Cmd_RM_ToExecute_ForceCloseOrderInsert_Req,
						&sMClose, sizeof(SManualForceClose),0,0,CF_ERROR_SUCCESS,0,0,nSeqID);	
				}
				else
				{//输出socket无效//输出日志				
					RISK_LOGINFO("主从服务器断开，强平请求失败\n");
				}
			}
		}
		break;
		/*查询强平单查询语句
		select t2.*, t1.FORCECLOSETYPE, t1.RISKERNAME, t1.INSERTDATE from RISK_FORCECLOSE_ORDER t1, TRADEDATA_ORDERS t2 where 
		(t1.NFRONTID = t2.FRONTID and t1.NSESSIONID = t2.SESSIONID and t1.SZORDERREF = t2.ORDERREF and  t1.INVESTORID = t2.INVESTORID  ) 
		and t1.investorid = '0001' and  to_char(t1.insertdate,'YYYY-MM-DD') = '2013-11-18'
		and  t2.TRADINGDAY  = '20131118'  order by t2.inserttime desc
		*/
	case Cmd_RM_QryForceCloseOrder_Req:
		{
			if(!CInterface_SvrNotifyAndAsk::getObj().Ask_ValidateUserPrivilege(PkgHead.userdata1,RISK_FORCE_CLOSE_RECORD_QUERY))
			{
				const char * lErrorString = FormatErrorCode(CF_ERROR_FORCE_CLOSE_RECORD_QUERY_NOPRIVILEDGE);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_QryForceCloseOrder_Rsp, 
					(void*)lErrorString, 
					strlen(lErrorString)+1, PkgHead.seq,0,CF_ERROR_COMMON_NO_PRIVILEGE);
				break;
			}

			if ( PkgHead.len != sizeof(ForceCloseCondition))
			{
				const char * lErrorString = FormatErrorCode(CF_ERROR_COMMON_INPUT_PARAM);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_QryForceCloseOrder_Rsp, 
					(void*)lErrorString, strlen(lErrorString)+1, PkgHead.seq, 0, CF_ERROR_COMMON_INPUT_PARAM);
				return;
			}
			ForceCloseCondition verifyOrder = *(ForceCloseCondition*)pPkgData;
			std::string strdate = verifyOrder.szInsertDate;
			std::string strSql = "select t2.*, t1.FORCECLOSETYPE, t1.RISKERNAME, t1.INSERTDATE from RISK_FORCECLOSE_ORDER t1, TRADEDATA_ORDERS t2 where (t1.NFRONTID = t2.FRONTID and t1.NSESSIONID = t2.SESSIONID and t1.SZORDERREF = t2.ORDERREF and  t1.INVESTORID = t2.INVESTORID  ) ";
			bool bAnd = true;

			std::string strInstrument="";
			std::set<std::string> setInstrument;
			if(verifyOrder.bIsProduct)
			{
				CInterface_SvrTradeData::getObj().GetInstrumentListByProductID(verifyOrder.InstrumentID, setInstrument);
				std::set<std::string>::iterator it = setInstrument.begin();
				for(;it != setInstrument.end(); it++)
				{
					std::string str = *it;
					if(it == setInstrument.begin())
					{
						std::string strName = "(t1.instrumentid = ''";
						if(bAnd)
							strName = "and (t1.instrumentid = ''";

						strInstrument = strName;
						int nLength = strName.length() -1;
						strInstrument.insert(nLength,  str);
						continue;
					}					

					std::string strNameOr = " or t1.instrumentid = ''";
					int nLengthOr = strNameOr.length() -1;
					strInstrument.insert(strInstrument.length(),  strNameOr);		
					strInstrument.insert(strInstrument.length()-1,  str);					
				}	
				if(strInstrument.length())
					strInstrument.append(1, ')');				
			}
			else
			{
				if(strlen(verifyOrder.InstrumentID)>0)
				{
					std::string strName = "(t1.instrumentid = '')";
					strInstrument = strName;
					int nLength = strName.length() -1;
					strInstrument.insert(nLength-1,  verifyOrder.InstrumentID);
				}
			}

		
			if(strInstrument.length() >0)
			{
				strSql.insert(strSql.length(), strInstrument);
				bAnd = true;
			}
			if(strcmp(verifyOrder.Account, "") != 0)
			{
				std::string strName = "t1.investorid = ''";
				if(bAnd)
					strName = "and t1.investorid = ''";
				strName.insert(strName.length()-1, verifyOrder.Account);
				strSql.insert(strSql.length() , strName);
				bAnd = true;
			}
			if(strcmp(verifyOrder.szInsertDate, "") != 0)
			{
				std::string strName = "to_char(t1.insertdate,'YYYY-MM-DD') = ''";
				if(bAnd)
					strName = " and  to_char(t1.insertdate,'YYYY-MM-DD') = ''";
				strName.insert(strName.length()-1, verifyOrder.szInsertDate);
				strSql.insert(strSql.length() , strName);
				bAnd = true;

				std::string str = verifyOrder.szInsertDate;
				if(str.length() == 10)
				{
					str.erase(7,1);
					str.erase(4,1);
					if(bAnd)
						strName = " and  t2.TRADINGDAY  = ''";
					strName.insert(strName.length()-1, str.c_str());
					strSql.insert(strSql.length() , strName);
					bAnd = true;
				}
			}	
			if(verifyOrder.nForceCloseType != -2)
			{
				std::string strName = " t1.FORCECLOSETYPE = "; 
				if(bAnd)
					strName = " and t1.FORCECLOSETYPE = ";

				char szResult[256];				
				sprintf(szResult, "%d", verifyOrder.nForceCloseType);
				std::string strResult =  szResult;

				strName.insert(strName.length()-1, strResult);
				strSql.insert(strSql.length() , strName);
				bAnd = true;
			}
			if(strcmp(verifyOrder.TradeInvestorName, "") != 0)
			{
				std::string strName = "t1.riskername = ''";
				if(bAnd)
					strName = "and t1.riskername = ''";
				strName.insert(strName.length()-1, verifyOrder.TradeInvestorName);
				strSql.insert(strSql.length() , strName);
				bAnd = true;
			}

			strSql.insert(strSql.length(), "  order by t2.inserttime desc");
			std::vector<ForceCloseResult> vec;

			std::vector<std::vector<_variant_t>> vNode;
			int nErrorCode = CF_ERROR_SUCCESS;
			if ( !CInterface_SvrDBOpr::getObj().QueryData(strSql.c_str(), vNode, nErrorCode))
			{
				const char * lErrorString = FormatErrorCode(nErrorCode);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryForceCloseOrder_Rsp,
					(void*)lErrorString, strlen(lErrorString)+1,PkgHead.seq,0,nErrorCode);	
				return ;
			}
			else if(vNode.size() == 0)
			{
				int i =0;
			}
			else
			{
				std::set<OrderKey> setOrderKey; 
				for ( UINT j = 0; j < vNode.size(); j++ )
				{
					std::vector<_variant_t> vValue = vNode[j];
					ForceCloseResult sResult;

					
					int i = 0;
					strcpy(sResult.order.BrokerID, vValue[i++].operator _bstr_t());
					strcpy(sResult.order.InvestorID, vValue[i++].operator _bstr_t());					
					strcpy(sResult.order.InstrumentID, vValue[i++].operator _bstr_t());
					strcpy(sResult.order.OrderRef, vValue[i++].operator _bstr_t());				
					strcpy(sResult.order.UserID, vValue[i++].operator _bstr_t());
					sResult.order.OrderPriceType = vValue[i++].cVal;
					sResult.order.Direction		 = vValue[i++].cVal;
					strcpy(sResult.order.CombOffsetFlag, vValue[i++].operator _bstr_t());
					strcpy(sResult.order.CombHedgeFlag, vValue[i++].operator _bstr_t());
					sResult.order.LimitPrice = vValue[i++].dblVal;
					sResult.order.VolumeTotalOriginal = vValue[i++].intVal;
					sResult.order.TimeCondition		 = vValue[i++].cVal;
					strcpy(sResult.order.GTDDate, vValue[i++].operator _bstr_t());
					sResult.order.VolumeCondition		 = vValue[i++].cVal;
					sResult.order.MinVolume = vValue[i++].intVal;
					sResult.order.ContingentCondition		 = vValue[i++].cVal;
					sResult.order.StopPrice = vValue[i++].dblVal;
					sResult.order.ForceCloseReason		 = vValue[i++].cVal;
					sResult.order.IsAutoSuspend = vValue[i++].intVal;
					strcpy(sResult.order.BusinessUnit, vValue[i++].operator _bstr_t());
					sResult.order.RequestID = vValue[i++].intVal;
					strcpy(sResult.order.OrderLocalID, vValue[i++].operator _bstr_t());
					strcpy(sResult.order.ExchangeID, vValue[i++].operator _bstr_t());
					strcpy(sResult.order.ParticipantID, vValue[i++].operator _bstr_t());
					strcpy(sResult.order.ClientID, vValue[i++].operator _bstr_t());
					strcpy(sResult.order.ExchangeInstID, vValue[i++].operator _bstr_t());
					strcpy(sResult.order.TraderID, vValue[i++].operator _bstr_t());
					sResult.order.InstallID = vValue[i++].intVal;
					sResult.order.OrderSubmitStatus	 = vValue[i++].cVal;
					sResult.order.NotifySequence = vValue[i++].intVal;	
					strcpy(sResult.order.TradingDay, vValue[i++].operator _bstr_t());
					sResult.order.SettlementID = vValue[i++].intVal;	
					strcpy(sResult.order.OrderSysID, vValue[i++].operator _bstr_t());
					sResult.order.OrderSource	 = vValue[i++].cVal;
					sResult.order.OrderStatus	 = vValue[i++].cVal;
					sResult.order.OrderType	 = vValue[i++].cVal;
					sResult.order.VolumeTraded = vValue[i++].intVal;
					sResult.order.VolumeTotal = vValue[i++].intVal;
					strcpy(sResult.order.InsertDate, vValue[i++].operator _bstr_t());
					strcpy(sResult.order.InsertTime, vValue[i++].operator _bstr_t());
					strcpy(sResult.order.ActiveTime, vValue[i++].operator _bstr_t());
					strcpy(sResult.order.SuspendTime, vValue[i++].operator _bstr_t());
					strcpy(sResult.order.UpdateTime, vValue[i++].operator _bstr_t());
					strcpy(sResult.order.CancelTime, vValue[i++].operator _bstr_t());
					strcpy(sResult.order.ActiveTraderID, vValue[i++].operator _bstr_t());
					strcpy(sResult.order.ClearingPartID, vValue[i++].operator _bstr_t());
					sResult.order.SequenceNo = vValue[i++].intVal;
					sResult.order.FrontID = vValue[i++].intVal;				
					sResult.order.SessionID = vValue[i++].intVal;					
					strcpy(sResult.order.UserProductInfo, vValue[i++].operator _bstr_t());
					strcpy(sResult.order.StatusMsg, vValue[i++].operator _bstr_t());
					sResult.order.UserForceClose = vValue[i++].intVal;
					strcpy(sResult.order.ActiveUserID, vValue[i++].operator _bstr_t());
					sResult.order.BrokerOrderSeq = vValue[i++].intVal;
						strcpy(sResult.order.RelativeOrderSysID, vValue[i++].operator _bstr_t());
					sResult.order.AvgPrice = vValue[i++].dblVal;					
					sResult.order.ExStatus = (PlatformStru_OrderInfo::EnumExStatus)vValue[i++].intVal;
					i++;//FTID++
					i++;//UPDATESEQ

					i++;//VALIDATEDATE
					i++;//INSERTDBTIME

					sResult.bForceCloseType = (bool)vValue[i++].intVal;
					strcpy(sResult.RiskerName, vValue[i++].operator _bstr_t());

					//std::string strDate = vValue[i].operator _bstr_t();			
				//	if(strDate.length() >=10)
						strcpy(sResult.szInsertDate, vValue[i++].operator _bstr_t());	
				
					OrderKey orderkey;
					strcpy(orderkey.Account, sResult.order.InvestorID);
					strcpy(orderkey.InstrumentID, sResult.order.InstrumentID);
					orderkey.FrontID = sResult.order.FrontID;
					orderkey.SessionID = sResult.order.SessionID;
					strcpy(orderkey.OrderRef, sResult.order.OrderRef);	

					int nsize = setOrderKey.size();
					setOrderKey.insert(orderkey);//TRADEDATA_ORDERS 存了好多条，只取最后一条数据
					if(nsize != setOrderKey.size())	
					vec.push_back(sResult);		
				
				}
				setOrderKey.clear();
			}
			if(vec.size() != 0)
				CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryForceCloseOrder_Rsp,
				&vec[0], sizeof(ForceCloseResult)*vec.size(),PkgHead.seq,0,CF_ERROR_SUCCESS);	
			else
				CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryForceCloseOrder_Rsp,
				NULL, 0,PkgHead.seq,0,CF_ERROR_SUCCESS);	
		}
		break;

	}

}
void GenerateSQL(SAdminQuery* pAdminQuery, std::string& strSql)
{
	std::string strInstrument="";			
	for(int i = 0; i< (int)pAdminQuery->nCount; i++)
	{
		std::string str = pAdminQuery->strAccount[i];
		if(i == 0)
		{
			std::string strName = " (investorid = ''";
			strInstrument = strName;
			int nLength = strName.length() -1;
			strInstrument.insert(nLength,  str);
			continue;
		}					

		std::string strNameOr = " or investorid =''";
		int nLengthOr = strNameOr.length() -1;
		strInstrument.insert(strInstrument.length(),  strNameOr);		
		strInstrument.insert(strInstrument.length()-1,  str);					
	}	
	if(strInstrument.length())
		strInstrument.append(1, ')');				

	bool bAnd = false;
	if(strInstrument.length() >0)
	{
		strSql.insert(strSql.length(), strInstrument);
		bAnd = true;
	}

	if(strcmp(pAdminQuery->szDateBegin, "") != 0 && strcmp(pAdminQuery->szDateEnd, "") != 0)
	{
		std::string strName = " to_date(t.TRADINGDAY,'YYYYMMDD') >= to_date(''";
		if(bAnd)
			strName = " and  to_date(t.TRADINGDAY,'YYYYMMDD') >= to_date(''";
		strName.insert(strName.length()-1, pAdminQuery->szDateBegin);
		strSql.insert(strSql.length() , strName);

		std::string strName2 = ",'YYYY-MM-DD')	and to_date(t.TRADINGDAY,'YYYYMMDD') <= to_date(''";
		strName2.insert(strName2.length()-1, pAdminQuery->szDateEnd);
		strSql.insert(strSql.length() , strName2);

		std::string strName3 = ",'YYYY-MM-DD') order by t.TRADINGDAY";			
		strSql.insert(strSql.length() , strName3);
		bAnd = true;
	}	
}
void ProcessAdminQuery(int hSocket,const Stru_UniPkgHead& PkgHead,const void*pPkgData)
{
	switch(PkgHead.cmdid)
	{	
	case Cmd_RM_QryHistoryFundInfo_Req:
		{
			SAdminQuery* pAdminQuery = (SAdminQuery*)pPkgData;
			if(pAdminQuery == NULL)
			{	
				const char * lErrorString = FormatErrorCode(CF_ERROR_COMMON_INPUT_PARAM);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_QryHistoryFundInfo_Rsp, 
					(void*)lErrorString, strlen(lErrorString)+1, PkgHead.seq, 0, CF_ERROR_COMMON_INPUT_PARAM);
				return;
			}
			if(!CInterface_SvrNotifyAndAsk::getObj().Ask_ValidateUserPrivilege(PkgHead.userdata1,QUERY_HISTORY_FUND))
			{
				const char * lErrorString = FormatErrorCode(CF_ERROR_QUERY_HISTORY_FUND_NOPRIVILEDGE);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_QryHistoryFundInfo_Rsp, 
					(void*)lErrorString, 
					strlen(lErrorString)+1, PkgHead.seq,0,CF_ERROR_COMMON_NO_PRIVILEGE);
				break;
			}
			std::string strSql = "select * from SETTLEMENT_FUNDINFO t where ";

			std::string strInstrument="";			
			for(int i = 0; i< (int)pAdminQuery->nCount; i++)
			{
				std::string str = pAdminQuery->strAccount[i];
				if(i == 0)
				{
					std::string strName = " (accountid = ''";
					strInstrument = strName;
					int nLength = strName.length() -1;
					strInstrument.insert(nLength,  str);
					continue;
				}					

				std::string strNameOr = " or accountid =''";
				int nLengthOr = strNameOr.length() -1;
				strInstrument.insert(strInstrument.length(),  strNameOr);		
				strInstrument.insert(strInstrument.length()-1,  str);					
			}	
			if(strInstrument.length())
				strInstrument.append(1, ')');				

			bool bAnd = false;
			if(strInstrument.length() >0)
			{
				strSql.insert(strSql.length(), strInstrument);
				bAnd = true;
			}

			if(strcmp(pAdminQuery->szDateBegin, "") != 0 && strcmp(pAdminQuery->szDateEnd, "") != 0)
			{
				std::string strName = " to_date(t.TRADINGDAY,'YYYYMMDD') >= to_date(''";
				if(bAnd)
					strName = " and  to_date(t.TRADINGDAY,'YYYYMMDD') >= to_date(''";
				strName.insert(strName.length()-1, pAdminQuery->szDateBegin);
				strSql.insert(strSql.length() , strName);

				std::string strName2 = ",'YYYY-MM-DD')	and to_date(t.TRADINGDAY,'YYYYMMDD') <= to_date(''";
				strName2.insert(strName2.length()-1, pAdminQuery->szDateEnd);
				strSql.insert(strSql.length() , strName2);

				std::string strName3 = ",'YYYY-MM-DD') order by t.TRADINGDAY";			
				strSql.insert(strSql.length() , strName3);
				bAnd = true;
			}	
			
			std::vector<PlatformStru_TradingAccountInfo> vec;

			std::vector<std::vector<_variant_t>> vNode;
			int nErrorCode = CF_ERROR_SUCCESS;
			if ( !CInterface_SvrDBOpr::getObj().QueryData(strSql.c_str(), vNode, nErrorCode))
			{
				const char * lErrorString = FormatErrorCode(nErrorCode);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryHistoryFundInfo_Rsp,
					(void*)lErrorString, strlen(lErrorString)+1,PkgHead.seq,0,nErrorCode);	
				return ;
			}
			else if(vNode.size() == 0)
			{
			//	CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryHistoryFundInfo_Rsp,
			//		NULL, 0,PkgHead.seq,0,CF_ERROR_SUCCESS);	
			}
			else
			{
				for ( UINT j = 0; j < vNode.size(); j++ )
				{
					std::vector<_variant_t> vValue = vNode[j];
					PlatformStru_TradingAccountInfo sResult;

					int i = 0;
					strcpy(sResult.BrokerID, vValue[i++].operator _bstr_t());
					strcpy(sResult.AccountID, vValue[i++].operator _bstr_t());					
					sResult.PreMortgage = vValue[i++].dblVal;
					sResult.PreCredit = vValue[i++].dblVal;
					sResult.PreDeposit = vValue[i++].dblVal;
					sResult.PreBalance = vValue[i++].dblVal;
					sResult.PreMargin = vValue[i++].dblVal;
					sResult.InterestBase = vValue[i++].dblVal;
					sResult.Interest = vValue[i++].dblVal;
					sResult.Deposit = vValue[i++].dblVal;
					sResult.Withdraw = vValue[i++].dblVal;
					sResult.FrozenMargin = vValue[i++].dblVal;
					sResult.FrozenCash = vValue[i++].dblVal;
					sResult.FrozenCommission = vValue[i++].dblVal;
					sResult.CurrMargin = vValue[i++].dblVal;
					sResult.CashIn = vValue[i++].dblVal;
					sResult.Commission = vValue[i++].dblVal;
					sResult.CloseProfit = vValue[i++].dblVal;
					sResult.PositionProfit = vValue[i++].dblVal;
					sResult.Balance = vValue[i++].dblVal;
					sResult.Available = vValue[i++].dblVal;
					sResult.WithdrawQuota = vValue[i++].dblVal;
					sResult.Reserve = vValue[i++].dblVal;
					strcpy(sResult.TradingDay, vValue[i++].operator _bstr_t());					
					sResult.SettlementID = vValue[i++].intVal;
					sResult.Credit = vValue[i++].dblVal;
					sResult.Mortgage = vValue[i++].dblVal;
					sResult.ExchangeMargin = vValue[i++].dblVal;
					sResult.DeliveryMargin = vValue[i++].dblVal;
					sResult.ExchangeDeliveryMargin = vValue[i++].dblVal;
					sResult.StaticProfit = vValue[i++].dblVal;
					sResult.DynamicProfit = vValue[i++].dblVal;
					sResult.RiskDegree = vValue[i++].dblVal;

					vec.push_back(sResult);			
				}
			}
			if(vec.size() != 0)
				CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryHistoryFundInfo_Rsp,
				&vec[0], sizeof(PlatformStru_TradingAccountInfo)*vec.size(),PkgHead.seq,0,CF_ERROR_SUCCESS);	
			else
				CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryHistoryFundInfo_Rsp,
				NULL, 0,PkgHead.seq,0,CF_ERROR_SUCCESS);
			break;		
		}
		
	case Cmd_RM_QryHistoryPosition_Req:
		{
			SAdminQuery* pAdminQuery = (SAdminQuery*)pPkgData;
			if(pAdminQuery == NULL)
			{
				const char * lErrorString = FormatErrorCode(CF_ERROR_COMMON_INPUT_PARAM);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_QryHistoryPosition_Rsp, 
					(void*)lErrorString, strlen(lErrorString)+1, PkgHead.seq, 0, CF_ERROR_COMMON_INPUT_PARAM);
				return;
			}
			if(!CInterface_SvrNotifyAndAsk::getObj().Ask_ValidateUserPrivilege(PkgHead.userdata1,QUERY_HISTORY_POSITION))
			{
				const char * lErrorString = FormatErrorCode(CF_ERROR_QUERY_HISTORY_POSITION_NOPRIVILEDGE);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_QryHistoryPosition_Rsp, 
					(void*)lErrorString, 
					strlen(lErrorString)+1, PkgHead.seq,0,CF_ERROR_COMMON_NO_PRIVILEGE);
				break;
			}
			
			std::string strSql = "select * from SETTLEMENT_POSITION t where ";		
			GenerateSQL(pAdminQuery, strSql);

			std::vector<PlatformStru_Position> vec;

			std::vector<std::vector<_variant_t>> vNode;
			int nErrorCode = CF_ERROR_SUCCESS;
			if ( !CInterface_SvrDBOpr::getObj().QueryData(strSql.c_str(), vNode, nErrorCode))
			{
				const char * lErrorString = FormatErrorCode(nErrorCode);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryHistoryPosition_Rsp,
					(void*)lErrorString, 
					strlen(lErrorString)+1,PkgHead.seq,0,nErrorCode);	
				return ;
			}
			else if(vNode.size() == 0)
			{
			//	CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryHistoryPosition_Rsp,
			//		NULL, 0,PkgHead.seq,0,CF_ERROR_SUCCESS);	
			}
			else
			{
				for ( UINT j = 0; j < vNode.size(); j++ )
				{
					std::vector<_variant_t> vValue = vNode[j];
					PlatformStru_Position sResult;

					int i = 0;
					strcpy(sResult.InstrumentID, vValue[i++].operator _bstr_t());
					strcpy(sResult.BrokerID, vValue[i++].operator _bstr_t());
					strcpy(sResult.InvestorID, vValue[i++].operator _bstr_t());
					sResult.PosiDirection			= vValue[i++].cVal;
					sResult.HedgeFlag				= vValue[i++].cVal;
					strcpy(sResult.TradingDay, vValue[i++].operator _bstr_t());
					sResult.PositionDate			= vValue[i++].cVal;
					sResult.SettlementID            = vValue[i++].intVal;
					sResult.Position                = vValue[i++].intVal;
					sResult.TodayPosition           = vValue[i++].intVal;
					sResult.YdPosition              = vValue[i++].intVal;
					sResult.OpenVolume              = vValue[i++].intVal;
					sResult.CloseVolume             = vValue[i++].intVal;
					sResult.OpenAmount				= vValue[i++].dblVal;
					sResult.CloseAmount				= vValue[i++].dblVal;
					sResult.PositionCost			= vValue[i++].dblVal;
					sResult.OpenCost			    = vValue[i++].dblVal;
					sResult.LongFrozen              = vValue[i++].intVal;
					sResult.ShortFrozen             = vValue[i++].intVal;
					sResult.LongFrozenAmount	    = vValue[i++].dblVal;
					sResult.ShortFrozenAmount	    = vValue[i++].dblVal;
					sResult.FrozenMargin			= vValue[i++].dblVal;
					sResult.FrozenCommission		= vValue[i++].dblVal;
					sResult.FrozenCash				= vValue[i++].dblVal;
					sResult.Commission				= vValue[i++].dblVal;
					sResult.PreMargin				= vValue[i++].dblVal;
					sResult.UseMargin				= vValue[i++].dblVal;
					sResult.ExchangeMargin			= vValue[i++].dblVal;
					sResult.MarginRateByMoney		= vValue[i++].dblVal;
					sResult.MarginRateByVolume		= vValue[i++].dblVal;
					sResult.CashIn					= vValue[i++].dblVal;
					sResult.PositionProfit			= vValue[i++].dblVal;
					sResult.CloseProfit				= vValue[i++].dblVal;
					sResult.CloseProfitByDate		= vValue[i++].dblVal;
					sResult.CloseProfitByTrade		= vValue[i++].dblVal;
					sResult.PreSettlementPrice		= vValue[i++].dblVal;
					sResult.SettlementPrice			= vValue[i++].dblVal;
					sResult.CombPosition            = vValue[i++].intVal;
					sResult.CombLongFrozen          = vValue[i++].intVal;
					sResult.CombShortFrozen         = vValue[i++].intVal;
					sResult.PositionProfitByTrade   = vValue[i++].dblVal;
					sResult.TotalPositionProfitByDate   = vValue[i++].dblVal;

					vec.push_back(sResult);			
				}
			}
			if(vec.size() != 0)
				CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryHistoryPosition_Rsp,
				&vec[0], sizeof(PlatformStru_Position)*vec.size(),PkgHead.seq,0,CF_ERROR_SUCCESS);	
			else
				CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryHistoryPosition_Rsp,
				NULL, 0,PkgHead.seq,0,CF_ERROR_SUCCESS);
			break;		
		}
	case Cmd_RM_QryHistoryPositionDetail_Req:
		{
			SAdminQuery* pAdminQuery = (SAdminQuery*)pPkgData;
			if(pAdminQuery == NULL)
			{
				const char * lErrorString = FormatErrorCode(CF_ERROR_COMMON_INPUT_PARAM);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_QryHistoryPositionDetail_Rsp, 
					(void*)lErrorString, strlen(lErrorString)+1, PkgHead.seq, 0, CF_ERROR_COMMON_INPUT_PARAM);
				return;
			}
			if(!CInterface_SvrNotifyAndAsk::getObj().Ask_ValidateUserPrivilege(PkgHead.userdata1,QUERY_HISTORY_POSITION_DETAIL))
			{
				const char * lErrorString = FormatErrorCode(CF_ERROR_QUERY_HISTORY_POSITION_DETAIL_NOPRIVILEDGE);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_QryHistoryPositionDetail_Rsp, 
					(void*)lErrorString, 
					strlen(lErrorString)+1, PkgHead.seq,0,CF_ERROR_COMMON_NO_PRIVILEGE);
				break;
			}

			std::string strSql = "select * from SETTLEMENT_POSITIONDETAIL t where ";		
			GenerateSQL(pAdminQuery, strSql);

			std::vector<PlatformStru_PositionDetail> vec;

			std::vector<std::vector<_variant_t>> vNode;
			int nErrorCode = CF_ERROR_SUCCESS;
			if ( !CInterface_SvrDBOpr::getObj().QueryData(strSql.c_str(), vNode, nErrorCode))
			{
				const char * lErrorString = FormatErrorCode(nErrorCode);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryHistoryPositionDetail_Rsp,
					(void*)lErrorString, 
					strlen(lErrorString)+1,PkgHead.seq,0,nErrorCode);	
				return ;
			}
			else if(vNode.size() == 0)
			{
			//	CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryHistoryPositionDetail_Rsp,
			//		NULL, 0,PkgHead.seq,0,CF_ERROR_SUCCESS);	
			}
			else
			{
				for ( UINT j = 0; j < vNode.size(); j++ )
				{
					std::vector<_variant_t> vValue = vNode[j];
					PlatformStru_PositionDetail sResult;

					int i = 0;
					strcpy(sResult.InstrumentID, vValue[i++].operator _bstr_t());
					strcpy(sResult.BrokerID, vValue[i++].operator _bstr_t());
					strcpy(sResult.InvestorID, vValue[i++].operator _bstr_t());
					sResult.HedgeFlag				= vValue[i++].cVal;
					sResult.Direction			    = vValue[i++].cVal;
					strcpy(sResult.OpenDate, vValue[i++].operator _bstr_t());
					strcpy(sResult.TradeID, vValue[i++].operator _bstr_t());
					sResult.Volume					= vValue[i++].intVal;
					sResult.OpenPrice				= vValue[i++].dblVal;
					strcpy(sResult.TradingDay, vValue[i++].operator _bstr_t());
					sResult.SettlementID			= vValue[i++].intVal;
					sResult.TradeType				= vValue[i++].cVal;
					strcpy(sResult.CombInstrumentID, vValue[i++].operator _bstr_t());
					strcpy(sResult.ExchangeID, vValue[i++].operator _bstr_t());
					sResult.CloseProfitByDate	    = vValue[i++].dblVal;
					sResult.CloseProfitByTrade	    = vValue[i++].dblVal;
					sResult.PositionProfitByDate    = vValue[i++].dblVal;
					sResult.PositionProfitByTrade   = vValue[i++].dblVal;
					sResult.Margin					= vValue[i++].dblVal;
					sResult.ExchMargin				= vValue[i++].dblVal;
					sResult.MarginRateByMoney		= vValue[i++].dblVal;
					sResult.MarginRateByVolume		= vValue[i++].dblVal;
					sResult.LastSettlementPrice		= vValue[i++].dblVal;
					sResult.SettlementPrice			= vValue[i++].dblVal;
					sResult.CloseVolume				= vValue[i++].intVal;
					sResult.CloseAmount				= vValue[i++].dblVal;
				

					vec.push_back(sResult);			
				}
			}
			if(vec.size() != 0)
				CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryHistoryPositionDetail_Rsp,
				&vec[0], sizeof(PlatformStru_PositionDetail)*vec.size(),PkgHead.seq,0,CF_ERROR_SUCCESS);	
			else
				CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryHistoryPositionDetail_Rsp,
				NULL, 0,PkgHead.seq,0,CF_ERROR_SUCCESS);
			break;		
		}
		break;
	case Cmd_RM_QryHistoryTrade_Req:
		{
			SAdminQuery* pAdminQuery = (SAdminQuery*)pPkgData;
			if(pAdminQuery == NULL)
			{
				const char * lErrorString = FormatErrorCode(CF_ERROR_COMMON_INPUT_PARAM);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_QryHistoryTrade_Rsp, 
					(void*)lErrorString, strlen(lErrorString)+1, PkgHead.seq, 0, CF_ERROR_COMMON_INPUT_PARAM);
				return;
			}
			if(!CInterface_SvrNotifyAndAsk::getObj().Ask_ValidateUserPrivilege(PkgHead.userdata1,QUERY_HISTORY_TRADE))
			{
				const char * lErrorString = FormatErrorCode(CF_ERROR_QUERY_HISTORY_TRADE_NOPRIVILEDGE);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_QryHistoryTrade_Rsp, 
					(void*)lErrorString, 
					strlen(lErrorString)+1, PkgHead.seq,0,CF_ERROR_COMMON_NO_PRIVILEGE);
				break;
			}
			std::string strSql = "select * from  SETTLEMENT_TRADE t where ";		
			GenerateSQL(pAdminQuery, strSql);

			std::vector<PlatformStru_TradeInfo> vec;

			std::vector<std::vector<_variant_t>> vNode;
			int nErrorCode = CF_ERROR_SUCCESS;
			if ( !CInterface_SvrDBOpr::getObj().QueryData(strSql.c_str(), vNode, nErrorCode))
			{
				const char * lErrorString = FormatErrorCode(nErrorCode);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryHistoryTrade_Rsp,
					(void*)lErrorString, 
					strlen(lErrorString)+1,PkgHead.seq,0,nErrorCode);	
				return ;
			}
			else if(vNode.size() == 0)
			{
			//	CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryHistoryTrade_Rsp,
			//		NULL, 0,PkgHead.seq,0,CF_ERROR_SUCCESS);	
			}
			else
			{
				for ( UINT j = 0; j < vNode.size(); j++ )
				{
					std::vector<_variant_t> vValue = vNode[j];
					PlatformStru_TradeInfo sResult;

					int i = 0;					
					strcpy(sResult.BrokerID, vValue[i++].operator _bstr_t());
					strcpy(sResult.InvestorID, vValue[i++].operator _bstr_t());
					strcpy(sResult.InstrumentID, vValue[i++].operator _bstr_t());
					strcpy(sResult.OrderRef, vValue[i++].operator _bstr_t());
					strcpy(sResult.UserID, vValue[i++].operator _bstr_t());
					strcpy(sResult.ExchangeID, vValue[i++].operator _bstr_t());
					strcpy(sResult.TradeID, vValue[i++].operator _bstr_t());
					sResult.Direction			    = vValue[i++].cVal;
					strcpy(sResult.OrderSysID, vValue[i++].operator _bstr_t());
					strcpy(sResult.ParticipantID, vValue[i++].operator _bstr_t());
					strcpy(sResult.ClientID, vValue[i++].operator _bstr_t());
					sResult.TradingRole			    = vValue[i++].cVal;
					strcpy(sResult.ExchangeInstID, vValue[i++].operator _bstr_t());
					sResult.OffsetFlag			    = vValue[i++].cVal;
					sResult.HedgeFlag				= vValue[i++].cVal;
					sResult.Price					= vValue[i++].dblVal;
					sResult.Volume					= vValue[i++].intVal;
					strcpy(sResult.TradeDate, vValue[i++].operator _bstr_t());
					strcpy(sResult.TradeTime, vValue[i++].operator _bstr_t());
					sResult.TradeType			    = vValue[i++].cVal;
					sResult.PriceSource				= vValue[i++].cVal;
					strcpy(sResult.TraderID, vValue[i++].operator _bstr_t());
					strcpy(sResult.OrderLocalID, vValue[i++].operator _bstr_t());
					strcpy(sResult.ClearingPartID, vValue[i++].operator _bstr_t());
					strcpy(sResult.BusinessUnit, vValue[i++].operator _bstr_t());
					sResult.SequenceNo					= vValue[i++].intVal;
					strcpy(sResult.TradingDay, vValue[i++].operator _bstr_t());
					sResult.SettlementID					= vValue[i++].intVal;
					sResult.BrokerOrderSeq					= vValue[i++].intVal;
					sResult.TradeSource						= vValue[i++].cVal;
					sResult.CloseProfitByDate				= vValue[i++].dblVal;	
					sResult.CloseProfitByTrade				= vValue[i++].dblVal;	
					sResult.TradeCommission					= vValue[i++].dblVal;	
				
					vec.push_back(sResult);			
				}
			}
			if(vec.size() != 0)
				CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryHistoryTrade_Rsp,
				&vec[0], sizeof(PlatformStru_TradeInfo)*vec.size(),PkgHead.seq,0,CF_ERROR_SUCCESS);	
			else
				CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryHistoryTrade_Rsp,
				NULL, 0,PkgHead.seq,0,CF_ERROR_SUCCESS);
			break;		
		}
	case Cmd_RM_QryHistoryOrders_Req:
		{
			SAdminQuery* pAdminQuery = (SAdminQuery*)pPkgData;
			if(pAdminQuery == NULL)
			{
				const char * lErrorString = FormatErrorCode(CF_ERROR_COMMON_INPUT_PARAM);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_QryHistoryOrders_Rsp, 
					(void*)lErrorString, strlen(lErrorString)+1, PkgHead.seq, 0, CF_ERROR_COMMON_INPUT_PARAM);
				return;
			}
			if(!CInterface_SvrNotifyAndAsk::getObj().Ask_ValidateUserPrivilege(PkgHead.userdata1,QUERY_HISTORY_ORDER))
			{
				const char * lErrorString = FormatErrorCode(CF_ERROR_QUERY_HISTORY_ORDER_NOPRIVILEDGE);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_QryHistoryOrders_Rsp, 
					(void*)lErrorString, 
					strlen(lErrorString)+1, PkgHead.seq,0,CF_ERROR_COMMON_NO_PRIVILEGE);
				break;
			}
			std::string strSql = "select * from  TRADEDATA_ORDERS t, SETTLEMENTDAY t1 where (to_date(t.TRADINGDAY,'YYYYMMDD')= to_date(t1.settlementday,'YYYY-MM-DD')) ";		
		//	GenerateSQL(pAdminQuery, strSql);

//////////////////////////////////////////////////////////////////////////
			bool bAnd = true;

			std::string strInstrument="";			
			for(int i = 0; i< (int)pAdminQuery->nCount; i++)
			{
				std::string str = pAdminQuery->strAccount[i];
				if(i == 0)
				{
					std::string strName = " (t.investorid = ''";
					if(bAnd)
						strName = " and (t.investorid = ''";
					strInstrument = strName;
					int nLength = strName.length() -1;
					strInstrument.insert(nLength,  str);
					continue;
				}					

				std::string strNameOr = " or t.investorid =''";
				int nLengthOr = strNameOr.length() -1;
				strInstrument.insert(strInstrument.length(),  strNameOr);		
				strInstrument.insert(strInstrument.length()-1,  str);					
			}	
			if(strInstrument.length())
				strInstrument.append(1, ')');				

			
			if(strInstrument.length() >0)
			{
				strSql.insert(strSql.length(), strInstrument);
				bAnd = true;
			}

			if(strcmp(pAdminQuery->szDateBegin, "") != 0 && strcmp(pAdminQuery->szDateEnd, "") != 0)
			{
				std::string strName = " to_date(t.TRADINGDAY,'YYYYMMDD') >= to_date(''";
				if(bAnd)
					strName = " and  to_date(t.TRADINGDAY,'YYYYMMDD') >= to_date(''";
				strName.insert(strName.length()-1, pAdminQuery->szDateBegin);
				strSql.insert(strSql.length() , strName);

				std::string strName2 = ",'YYYY-MM-DD')	and to_date(t.TRADINGDAY,'YYYYMMDD') <= to_date(''";
				strName2.insert(strName2.length()-1, pAdminQuery->szDateEnd);
				strSql.insert(strSql.length() , strName2);

				std::string strName3 = ",'YYYY-MM-DD') order by t.TRADINGDAY";			
				strSql.insert(strSql.length() , strName3);
				bAnd = true;
			}	




//////////////////////////////////////////////////////////////////////////
			std::vector<PlatformStru_OrderInfo> vec;

			std::vector<std::vector<_variant_t>> vNode;
			int nErrorCode = CF_ERROR_SUCCESS;
			if ( !CInterface_SvrDBOpr::getObj().QueryData(strSql.c_str(), vNode, nErrorCode))
			{
				const char * lErrorString = FormatErrorCode(nErrorCode);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryHistoryOrders_Rsp,
					(void*)lErrorString, strlen(lErrorString)+1,PkgHead.seq,0,nErrorCode);	
				return ;
			}
			else if(vNode.size() == 0)
			{
			//	CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryHistoryOrders_Rsp,
			//		NULL, 0,PkgHead.seq,0,CF_ERROR_SUCCESS);	
			}
			else
			{
				for ( UINT j = 0; j < vNode.size(); j++ )
				{
					std::vector<_variant_t> vValue = vNode[j];
					PlatformStru_OrderInfo sResult;

					int i = 0;					
					strcpy(sResult.BrokerID, vValue[i++].operator _bstr_t());
					strcpy(sResult.InvestorID, vValue[i++].operator _bstr_t());
					strcpy(sResult.InstrumentID, vValue[i++].operator _bstr_t());
					strcpy(sResult.OrderRef, vValue[i++].operator _bstr_t());
					strcpy(sResult.UserID, vValue[i++].operator _bstr_t());
					sResult.OrderPriceType			    = vValue[i++].cVal;
					sResult.Direction			    = vValue[i++].cVal;
					strcpy(sResult.CombOffsetFlag, vValue[i++].operator _bstr_t());
					strcpy(sResult.CombHedgeFlag, vValue[i++].operator _bstr_t());						
					sResult.LimitPrice				= vValue[i++].dblVal;
					sResult.VolumeTotalOriginal		= vValue[i++].intVal;
					sResult.TimeCondition			= vValue[i++].cVal;
					strcpy(sResult.GTDDate, vValue[i++].operator _bstr_t());
					sResult.VolumeCondition			= vValue[i++].cVal;
					sResult.MinVolume				= vValue[i++].intVal;
					sResult.ContingentCondition		= vValue[i++].cVal;
					sResult.StopPrice				= vValue[i++].dblVal;
					sResult.ForceCloseReason		= vValue[i++].cVal;
					sResult.IsAutoSuspend			= vValue[i++].intVal;
					strcpy(sResult.BusinessUnit, vValue[i++].operator _bstr_t());
					sResult.RequestID				= vValue[i++].intVal;
					strcpy(sResult.OrderLocalID, vValue[i++].operator _bstr_t());
					strcpy(sResult.ExchangeID, vValue[i++].operator _bstr_t());
					strcpy(sResult.ParticipantID, vValue[i++].operator _bstr_t());
					strcpy(sResult.ClientID, vValue[i++].operator _bstr_t());
					strcpy(sResult.ExchangeInstID, vValue[i++].operator _bstr_t());
					strcpy(sResult.TraderID, vValue[i++].operator _bstr_t());
					sResult.InstallID				= vValue[i++].intVal;
					sResult.OrderSubmitStatus		= vValue[i++].cVal;
					sResult.NotifySequence			= vValue[i++].intVal;
					strcpy(sResult.TradingDay, vValue[i++].operator _bstr_t());
					sResult.SettlementID				= vValue[i++].intVal;
					strcpy(sResult.OrderSysID, vValue[i++].operator _bstr_t());
					sResult.OrderSource		= vValue[i++].cVal;
					sResult.OrderStatus		= vValue[i++].cVal;
					sResult.OrderType		= vValue[i++].cVal;
					sResult.VolumeTraded	= vValue[i++].intVal;
					sResult.VolumeTotal		= vValue[i++].intVal;
					strcpy(sResult.InsertDate, vValue[i++].operator _bstr_t());
					strcpy(sResult.InsertTime, vValue[i++].operator _bstr_t());
					strcpy(sResult.ActiveTime, vValue[i++].operator _bstr_t());
					strcpy(sResult.SuspendTime, vValue[i++].operator _bstr_t());
					strcpy(sResult.UpdateTime, vValue[i++].operator _bstr_t());
					strcpy(sResult.CancelTime, vValue[i++].operator _bstr_t());
					strcpy(sResult.ActiveTraderID, vValue[i++].operator _bstr_t());
					strcpy(sResult.ClearingPartID, vValue[i++].operator _bstr_t());
					sResult.SequenceNo			= vValue[i++].intVal;
					sResult.FrontID				= vValue[i++].intVal;
					sResult.SessionID			= vValue[i++].intVal;
					strcpy(sResult.UserProductInfo, vValue[i++].operator _bstr_t());
					strcpy(sResult.StatusMsg, vValue[i++].operator _bstr_t());
					sResult.UserForceClose			= vValue[i++].intVal;
					strcpy(sResult.ActiveUserID, vValue[i++].operator _bstr_t());
					sResult.BrokerOrderSeq			= vValue[i++].intVal;
					strcpy(sResult.RelativeOrderSysID, vValue[i++].operator _bstr_t());
					sResult.AvgPrice				= vValue[i++].dblVal;
					sResult.ExStatus			= (PlatformStru_OrderInfo::EnumExStatus)vValue[i++].intVal;

					vec.push_back(sResult);			
				}
			}
			if(vec.size() != 0)
				CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryHistoryOrders_Rsp,
				&vec[0], sizeof(PlatformStru_OrderInfo)*vec.size(),PkgHead.seq,0,CF_ERROR_SUCCESS);	
			else
				CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryHistoryOrders_Rsp,
				NULL, 0,PkgHead.seq,0,CF_ERROR_SUCCESS);
			break;		
		}
	case Cmd_RM_QryHistoryFundInOut_Req:
		{			
			SAdminQuery* pAdminQuery = (SAdminQuery*)pPkgData;
			if(pAdminQuery == NULL)
			{
				const char * lErrorString = FormatErrorCode(CF_ERROR_COMMON_INPUT_PARAM);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket, Cmd_RM_QryHistoryFundInOut_Rsp, 
					(void*)lErrorString, strlen(lErrorString)+1, PkgHead.seq, 0, CF_ERROR_COMMON_INPUT_PARAM);
				return;
			}

			std::string strSql = "select * from  TRADEDATA_FUNDINOUT t where ";		
			std::string strInstrument="";			
			for(int i = 0; i< (int)pAdminQuery->nCount; i++)
			{
				std::string str = pAdminQuery->strAccount[i];
				if(i == 0)
				{
					std::string strName = " (USERID = ''";
					strInstrument = strName;
					int nLength = strName.length() -1;
					strInstrument.insert(nLength,  str);
					continue;
				}					

				std::string strNameOr = " or USERID =''";
				int nLengthOr = strNameOr.length() -1;
				strInstrument.insert(strInstrument.length(),  strNameOr);		
				strInstrument.insert(strInstrument.length()-1,  str);					
			}	
			if(strInstrument.length())
				strInstrument.append(1, ')');				

			bool bAnd = false;
			if(strInstrument.length() >0)
			{
				strSql.insert(strSql.length(), strInstrument);
				bAnd = true;
			}

			if(strcmp(pAdminQuery->szDateBegin, "") != 0 && strcmp(pAdminQuery->szDateEnd, "") != 0)
			{
				std::string strName = " to_date(t.OPDATE,'YYYY-MM-DD') >= to_date(''";
				if(bAnd)
					strName = " and  to_date(t.OPDATE,'YYYY-MM-DD') >= to_date(''";
				strName.insert(strName.length()-1, pAdminQuery->szDateBegin);
				strSql.insert(strSql.length() , strName);

				std::string strName2 = ",'YYYY-MM-DD')	and to_date(t.OPDATE,'YYYY-MM-DD') <= to_date(''";
				strName2.insert(strName2.length()-1, pAdminQuery->szDateEnd);
				strSql.insert(strSql.length() , strName2);

				std::string strName3 = ",'YYYY-MM-DD') order by t.OPDATE";			
				strSql.insert(strSql.length() , strName3);
				bAnd = true;
			}	

			std::vector<sFundInOut> vec;

			std::vector<std::vector<_variant_t>> vNode;
			int nErrorCode = CF_ERROR_SUCCESS;
			if ( !CInterface_SvrDBOpr::getObj().QueryData(strSql.c_str(), vNode, nErrorCode))
			{
				const char * lErrorString = FormatErrorCode(nErrorCode);
				CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryHistoryFundInOut_Rsp,
					(void*)lErrorString, strlen(lErrorString)+1,PkgHead.seq,0,nErrorCode);	
				return ;
			}
			else if(vNode.size() == 0)
			{
			//	CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryHistoryFundInOut_Rsp,
			//		NULL, 0,PkgHead.seq,0,CF_ERROR_SUCCESS);	
			}
			else
			{
				for ( UINT j = 0; j < vNode.size(); j++ )
				{
					std::vector<_variant_t> vValue = vNode[j];
					sFundInOut sResult;

					int i = 0;					
					strcpy(sResult.mUserID, vValue[i++].operator _bstr_t());
					sResult.meInOut				= (eInOut)vValue[i++].intVal;
					sResult.mdbVolume			= vValue[i++].dblVal;
					strcpy(sResult.mOpAdminID, vValue[i++].operator _bstr_t());
					strcpy(sResult.msDesc, vValue[i++].operator _bstr_t());
					strcpy(sResult.msDay, vValue[i++].operator _bstr_t());
					strcpy(sResult.msTime, vValue[i++].operator _bstr_t());

					vec.push_back(sResult);			
				}
			}
			if(vec.size() != 0)
				CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryHistoryFundInOut_Rsp,
				&vec[0], sizeof(sFundInOut)*vec.size(),PkgHead.seq,0,CF_ERROR_SUCCESS);	
			else
				CInterface_SvrTcp::getObj().SendPkgData(hSocket,Cmd_RM_QryHistoryFundInOut_Rsp,
				NULL, 0,PkgHead.seq,0,CF_ERROR_SUCCESS);
			break;	
		}
	}
	
}
double dbFund = 0;
double dbPosition = 0;
double dbOrderRtn = 0;
double dbTradeRtn = 0;
// 解析一个数据通知事件
bool ParseDataChangedEvent(Stru_NotifyEvent& dataEvt)
{
	switch(dataEvt.meEventType)
	{
	case EventTypeStartInit:
		{
			LONGLONG lFfreq = CTools_Win32::MyGetCpuTickFreq();
			LONGLONG n1 =  CTools_Win32::MyGetCpuTickCounter();

			SAFE_GET_DATACENTER()->ClearInstrumentInfo();//清除已有合约详情，因为过夜后数据变化
			
			LONGLONG n2 =  CTools_Win32::MyGetCpuTickCounter();
			double db1 = (n2-n1)*1000.0/lFfreq;
			if(db1 >0.0001)
				OUTPUT_LOG("EventTypeStartInit %.4f " ,db1);
		}
		break;
	case EventTypeUserDataReady:
		{//这个事件
			break;
		}
	case EventTypeInitFinish:
		{
			m_RiskMsgCalc->TradeDataInit();	
			m_RiskMsgCalc_Account->TradeDataInit();
		}
		break;

/*	case EventTypeQuto://行情到来，需要改关键字
		{
			if(GetTickCount() - g_dwQutoTickCount < 1000)
				return 0;//少于一秒钟不进行风险事件判断
			g_dwQutoTickCount = GetTickCount();

			PlatformStru_DepthMarketData* pData = (PlatformStru_DepthMarketData*)dataEvt.marrDataByte;
			if(pData == NULL)
				return 0;
			m_RiskMsgCalc->NewDepthMarketData(pData->InstrumentID);		
		}
		break;*/
		/*
	case EventTypeFund:
		{
			LONGLONG lFfreq = CTools_Win32::MyGetCpuTickFreq();
			LONGLONG n1 =  CTools_Win32::MyGetCpuTickCounter();

			char* strInvestor = (char*)dataEvt.marrDataByte;		
			m_RiskMsgCalc->NewFundAccount(strInvestor);		
			m_RiskMsgCalc->SendFund(strInvestor);

			LONGLONG n2 =  CTools_Win32::MyGetCpuTickCounter();
			double db1 = (n2-n1)*1000.0/lFfreq;
			dbFund = dbFund+db1;
			static LONGLONG count_subscrib = 1;
			if((count_subscrib++) % 10000 == 0)
			{
				OUTPUT_LOG("EventTypeFund %.4f " ,dbFund);
			}			
			break;
		}
	case EventTypePosition:
		{//持仓由风控客户端定时查询	
			LONGLONG lFfreq = CTools_Win32::MyGetCpuTickFreq();
			LONGLONG n1 =  CTools_Win32::MyGetCpuTickCounter();

			char* strInvestor = (char*)dataEvt.marrDataByte;
			m_RiskMsgCalc->NewPosition(strInvestor);		
			m_RiskMsgCalc->SendPosition(strInvestor);	

			LONGLONG n2 =  CTools_Win32::MyGetCpuTickCounter();
			double db1 = (n2-n1)*1000.0/lFfreq;

			dbPosition = dbPosition+db1;
			static LONGLONG count_subscribPosition = 1;
			if((count_subscribPosition++) % 10000 == 0)
			{
				OUTPUT_LOG("EventTypePosition %.4f " ,dbPosition);
			}
			
			break;
		}*/
	case EventTypeOrderRtn:
		{//负责推送报单
			LONGLONG lFfreq = CTools_Win32::MyGetCpuTickFreq();
			LONGLONG n1 =  CTools_Win32::MyGetCpuTickCounter();

			PlatformStru_OrderInfo * lpBuf = (PlatformStru_OrderInfo*)dataEvt.marrDataByte;		
			if(lpBuf)
			{//报单不做风险计算，不会对盈亏产生影响
			    m_RiskMsgCalc->NewOrder(lpBuf->InvestorID, lpBuf->InstrumentID);		
				m_RiskMsgCalc->SendOrder(lpBuf);
			
				std::string strBroker, strUser;
				bool bGet = GetTradeAccountByUserID(lpBuf->InvestorID, strBroker, strUser);
				if(bGet)
					m_RiskMsgCalc_Account->NewOrder(strBroker, strUser, lpBuf->InstrumentID);
			}

			LONGLONG n2 =  CTools_Win32::MyGetCpuTickCounter();
			double db1 = (n2-n1)*1000.0/lFfreq;
			dbOrderRtn = dbOrderRtn + db1;
			static LONGLONG count_dbOrderRtn = 1;
			if((count_dbOrderRtn++) % 10000 == 0)
			{
				OUTPUT_LOG("EventTypeOrderRtn %.4f " ,dbOrderRtn);
			}
			
			if(lpBuf->OrderStatus == THOST_FTDC_OST_Unknown && lpBuf->ExStatus == PlatformStru_OrderInfo::EnumExStatus::ExSta_approving)
			{//对未审核的报单存数据库
				SVerisyOrder verifyOrder;
				strcpy(verifyOrder.orderKey.Account,		lpBuf->InvestorID);
				strcpy(verifyOrder.orderKey.InstrumentID,	lpBuf->InstrumentID);
				verifyOrder.orderKey.FrontID				 = lpBuf->FrontID;
				verifyOrder.orderKey.SessionID				 = lpBuf->SessionID;
				strcpy(verifyOrder.orderKey.OrderRef,	lpBuf->OrderRef);
				verifyOrder.nResult							 = -1;
			
				char szBuffer[MAX_SQL_LENGTH];
				memset(szBuffer, 0, sizeof(szBuffer));
				sprintf(szBuffer, "insert into RISK_VERIFYORDER values('%s','%s',%d, %d,'%s',%d, sysdate, %d)",
					verifyOrder.orderKey.Account,
					verifyOrder.orderKey.InstrumentID,
					verifyOrder.orderKey.FrontID,
					verifyOrder.orderKey.SessionID,
					verifyOrder.orderKey.OrderRef,
					verifyOrder.nVerifyUser,
					verifyOrder.nResult
					);

				int nErrorCode = CF_ERROR_SUCCESS;
				CInterface_SvrDBOpr::getObj().Excute(szBuffer, nErrorCode);
				if(nErrorCode != CF_ERROR_SUCCESS)
				{
					CInterface_SvrDBOpr::getObj().SaveVertifyOrder2File(verifyOrder );
				}
				
			}
			break;
		}
	case EventTypeTradeRtn:
		{//负责推送成交
			LONGLONG lFfreq = CTools_Win32::MyGetCpuTickFreq();
			LONGLONG n1 =  CTools_Win32::MyGetCpuTickCounter();

			PlatformStru_TradeInfo * lpBuf = (PlatformStru_TradeInfo*)dataEvt.marrDataByte;		
			if(lpBuf)
			{
				m_RiskMsgCalc->NewTrade(lpBuf->InvestorID, lpBuf->InstrumentID);
				m_RiskMsgCalc->SendTrader(lpBuf);

				std::string strBroker, strUser;
				bool bGet = GetTradeAccountByUserID(lpBuf->InvestorID, strBroker, strUser);
				if(bGet)
					m_RiskMsgCalc_Account->NewTrade(strBroker, strUser, lpBuf->InstrumentID);
			}

			LONGLONG n2 =  CTools_Win32::MyGetCpuTickCounter();
			double db1 = (n2-n1)*1000.0/lFfreq;

			dbTradeRtn = dbTradeRtn + db1;
			static LONGLONG count_dbTradeRtn = 1;
			if((count_dbTradeRtn++) % 10000 == 0)
			{
				OUTPUT_LOG("EventTypeTradeRtn %.4f " ,dbOrderRtn);
			}
			break;
		}
	case EventTypeUserLimitAndVerifyUpdate:
		{
			int nUserID = dataEvt.mnDataID;	
			std::vector<UserAndTradeAccountRelation> vec;
			CInterface_SvrUserOrg::getObj().GetUserAndTradeAccountRelationByUserID(nUserID, vec);
			if(vec.size()>0)
			{
				UserAndTradeAccountRelation& pRelation = vec[0];
				char AccIDOrInst[256];
				sprintf_s(AccIDOrInst,256,"%d",pRelation.nUserID);

				int RspCmdID= Cmd_RM_SetLimitAndVerify_Rsp;
				std::set<SOCKET>::const_iterator it;
				std::set<SOCKET> sset;
				//注意：这个订阅了资金信息以后如果没有订阅资金了，风控客户端下单限制和报单审核权限就收到不到更新了
				m_pThreadSharedData->GetSubscribeData(AccIDOrInst, SubscribeID_Fund, sset);
				for (it=sset.begin();it!=sset.end();++it)
				{//发送风险事件
					CInterface_SvrTcp::getObj().SendPkgData(*it, RspCmdID,
						(void*)&pRelation,sizeof(UserAndTradeAccountRelation), 
						0, 0, CF_ERROR_SUCCESS, 0, 0);
				}
				if(pRelation.nNeedVerify == true)
					return true;//需要人工审核，则不需要让待审核单变成审核通过的报单
				PassVerifyOrder(nUserID);
			}
		}
		break;
	case EventTypeSlaverLoginedMaster:
		{			
			InterlockedExchange((volatile long*)(&g_hSocket),dataEvt.mnDataID);		
			RISK_LOGINFO("主从服务器连接成功，socket=%d\n", dataEvt.mnDataID);
			//PUSH_LOG_ERROR(FirstLevelError,true,true,"--------------风控收到主从服务器连接成功消息-------------------------，socket=%d\n", dataEvt.mnDataID);
		}
		break;
	
	}
	return true;
}

void SubscribeAllCmdMsgID()
{


	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_QryOrganization_Req, g_idThreadRequest);
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_QryUser_Req, g_idThreadRequest);
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_QryUserAndOrgRelation_Req, g_idThreadRequest);
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_QryTraderAndTradeAccountRelation_Req, g_idThreadRequest);
	

	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_AddRiskPlan_Req, g_idThreadRequest);	
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_UseRiskPlanAdd_Req, g_idThreadRequest);
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_DeleteRiskPlan_Req, g_idThreadRequest);	
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_QueryRiskPlan_Req, g_idThreadRequest);
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_QryProducts_Req, g_idThreadRequest);
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_QryResponse_Req, g_idThreadRequest);

	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_ModifyPassword_Req, g_idThreadRequest);	
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_UnLockWindow_Req, g_idThreadRequest);

	//风险指标相关接口
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_AddRiskIndicator_Req, g_idThreadRequest);
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_DelRiskIndicator_Req, g_idThreadRequest);
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_ModifyRiskIndicator_Req, g_idThreadRequest);
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_QryRiskIndicator_Req, g_idThreadRequest);

	//风险事件
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_SubscribeRiskEvent_Req, g_idThreadRequest);
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_UnSubscribeRiskEvent_Req, g_idThreadRequest);
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_QryRiskEvent_Req, g_idThreadRequest);
	
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_SetLimitTrade_Req, g_idThreadRequest);
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_SetManualVerify_Req, g_idThreadRequest);

	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_CONTRACTSET_Req, g_idThreadRequest);
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_CONTRACTSET_QUERY_Req, g_idThreadRequest);

	

	//消息相关接口
/*
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_AddMessage_Req, g_idThreadRequest);
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_QrySendMessage_Req, g_idThreadRequest);
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_QryMsgSendStatus_Req, g_idThreadRequest);
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_QryUnReadMessage_Req, g_idThreadRequest);
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_QryRecvMessage_Req, g_idThreadRequest);
*/
	//行情	
//	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_SubscribeQuot_Req, g_idThreadRequest);
//	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_UnSubscribeQuot_Req, g_idThreadRequest);

	//出入金
//	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_SubscribeDeposit_Req, g_idThreadRequest);
//	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_UnSubscribeDeposit_Req, g_idThreadRequest);
//	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_QryTraderDeposit_Req, g_idThreadRequest);

	//成交	
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_SubscribeTrade_Req, g_idThreadRequest);
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_UnSubscribeTrade_Req, g_idThreadRequest);
//	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_QryHistroyTrade_Req, g_idThreadRequest);

	//资金		
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_SubscribeFund_Req, g_idThreadRequest);
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_UnSubscribeFund_Req, g_idThreadRequest);
	//CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_QryHistroyRiskFund_Req, g_idThreadRequest);
	
	//持仓	
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_SubscribePosition_Req, g_idThreadRequest);
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_UnSubscribePosition_Req, g_idThreadRequest);
	//CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_QryHistroyPosition_Req, g_idThreadRequest);
	
	//报单
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_SubscribeOrder_Req, g_idThreadRequest);
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_UnSubscribeOrder_Req, g_idThreadRequest);
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_QryHistroyOrder_Req, g_idThreadRequest);
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_VerifyOrder_Req, g_idThreadRequest);
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_QryVerifyOrder_Req, g_idThreadRequest);

	//基金净值
	//CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_AddFundNetParam_Req, g_idThreadRequest);
	//CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_QueryFundNetParam_Req, g_idThreadRequest);
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_QueryFundNetCalcResult_Req, g_idThreadRequest);
	
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_ForceCloseOrderInsert_Req, g_idThreadRequest);
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_QryForceCloseOrder_Req, g_idThreadRequest);
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_OrderAction_Req, g_idThreadRequest);

	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_QryHistoryFundInfo_Req, g_idThreadRequest);
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_QryHistoryPosition_Req, g_idThreadRequest);
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_QryHistoryPositionDetail_Req, g_idThreadRequest);
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_QryHistoryTrade_Req, g_idThreadRequest);
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_QryHistoryOrders_Req, g_idThreadRequest);
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_QryHistoryFundInOut_Req, g_idThreadRequest);

	CInterface_SvrTcp::getObj().SubscribePkg(CMDID_SvrTcpDisconn,g_idThreadRequest);
	CInterface_SvrTcp::getObj().SubscribePkg(CMDID_TcpClientDisconn,g_idThreadRequest);


	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_ToExecute_OrderAction_Rsp,g_idThreadRequest);
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_ToExecute_VerifyOrder_Rsp,g_idThreadRequest);
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_ToExecute_ForceCloseOrderInsert_Rsp,g_idThreadRequest);
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_ToExecute_RiskForce_Rsp,g_idThreadRequest);


	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_Master_SetLimitTrade_Rsp,g_idThreadRequest);
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_Master_SetManualVerify_Rsp,g_idThreadRequest);
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_RM_Master_ModifyUserPassword_Rsp,g_idThreadRequest);
	CInterface_SvrTcp::getObj().SubscribePkg(Cmd_ModifyUserPassword_Req,g_idThreadRequest);

}
void PassVerifyOrder(int nUserID)
{
	//如果不需要审核，则所有的待审核单都置为直接通过审核
	UserInfo userInfo;	
	bool bFind = CInterface_SvrUserOrg::getObj().GetUserByID(nUserID, userInfo);  
	if(bFind)
	{
		std::string strAcc = userInfo.szAccount;
		std::vector<PlatformStru_OrderInfo> outData;
		CInterface_SvrTradeData::getObj().GetUserUnkownOrders(strAcc, outData);		
		for(int i =0; i< (int)outData.size(); i++)
		{
			PlatformStru_OrderInfo orderInfo = outData[i];
			if(orderInfo.ExStatus != PlatformStru_OrderInfo::EnumExStatus::ExSta_approving)
				continue;
			SVerisyOrder verifyOrder;
			verifyOrder.nVerifyUser				= -1;//管理终端或者风控客户端对交易员 权限变更（需要审核-》不需要审核） 导致的 所有待审核单 通过审核
			verifyOrder.nResult					= 1;
			strcpy(verifyOrder.orderKey.Account	, orderInfo.InvestorID);
			strcpy(verifyOrder.orderKey.InstrumentID, orderInfo.InstrumentID);
			verifyOrder.orderKey.FrontID		= orderInfo.FrontID;
			verifyOrder.orderKey.SessionID		= orderInfo.SessionID;
			strcpy(verifyOrder.orderKey.OrderRef, orderInfo.OrderRef);
	/*		if(!g_bSendToExcute)
			{
				CF_ERROR bSet =  CInterface_SvrTradeExcute::getObj().SetVerifyOrder(verifyOrder.orderKey, true);					
			}*/
			if(g_bSendToExcute)
			{
				int nSocket =0;
				InterlockedExchange((volatile long*)(&nSocket),g_hSocket);
				if(nSocket)
				{					
					CInterface_SvrTcp::getObj().SendPkgData(nSocket,Cmd_RM_ToExecute_VerifyOrder_Req,
						&verifyOrder, sizeof(SVerisyOrder),0,0,CF_ERROR_SUCCESS,0,0,0);	
				}
				else
				{//输出socket无效
					RISK_LOGINFO("主从服务断开，审核单通过审核失败\n");
				}
			}
			char szBuffer[MAX_SQL_LENGTH];
			memset(szBuffer, 0, sizeof(szBuffer));
			sprintf(szBuffer, "update  RISK_VERIFYORDER t set t.verifyuser = %d, t.result = %d \
							  , t.verifytime = sysdate where  t.investorid = '%s' \
							  and t.instrumentid = '%s' and t.frontid = %d and t.sessionid = %d and t.orderref = '%s'",
							  verifyOrder.nVerifyUser,
							  verifyOrder.nResult,
							  verifyOrder.orderKey.Account,
							  verifyOrder.orderKey.InstrumentID,
							  verifyOrder.orderKey.FrontID,
							  verifyOrder.orderKey.SessionID,
							  verifyOrder.orderKey.OrderRef				
							  );

			int nErrorCode = CF_ERROR_SUCCESS;
			CInterface_SvrDBOpr::getObj().Excute(szBuffer, nErrorCode);
			if(nErrorCode != CF_ERROR_SUCCESS)
			{
				CInterface_SvrDBOpr::getObj().SaveVertifyOrder2File(verifyOrder );
			}

		}
	}
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
