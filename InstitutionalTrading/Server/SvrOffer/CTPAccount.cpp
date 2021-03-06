#include "stdafx.h"

#include "CTPAccount.h"
#include "OfferMainInterface.h"
#include "ToolThread.h"
extern COfferMainInterface* g_pOfferMain;

CCTPAccount::CCTPAccount(void)
{	
	//memset(&m_login, 0, sizeof(SLogin));

	m_strBrokerID				= "";
	// 交易用户代码		
	m_strUserID					= "";
	// 交易用户代码
	m_strPassword				= "";	
	//交易API
	m_pTraderApi				= NULL;     
	//交易SPI
	m_pTraderSpi				= NULL;  
	//登录的状态
	m_enumAccountStatus			= ACCOUNT_STATUS_UnInit;	
	//当前可用的OrderRef
	m_nCurrentOrderref			= 0;
	//CTP返回的FrontID
	m_nCTPFrontID				= 0;  
	//CTP返回的SessionID
	m_nCTPSessionID				= 0;
	//登录次数
	m_nLoginInCount = 0;

	m_pTradeDataMsgQueue = new CTradeDataMsgQueue;

	m_bOnlyQueryRatio		= false;
	m_nQueryMargin			= 0;
	m_nQueryCommission		= 0;

	 m_LastQryTime			= 0;
	 m_bInQry				= false;

	 m_QueryMgrThread       = NULL;
	 m_QueryRatioMgrThread  = NULL;
	 m_bQueryCommission		= false;
	 m_bQueryMargin			= false;
	 m_LastAccount			= NULL;
}

CCTPAccount::~CCTPAccount(void)
{
	DWORD		ExitCode;
	if(m_QueryMgrThread)
	{
		m_QueryMgrThread->SetExit(true);		
		WaitForSingleObject((HANDLE)m_QueryMgrThread,8000);
		if(GetExitCodeThread((HANDLE)m_QueryMgrThread,&ExitCode)!=0&&ExitCode==STILL_ACTIVE)
			TerminateThread((HANDLE)m_QueryMgrThread,0);
		SAFE_DELETE(m_QueryMgrThread);
	}

	if(m_QueryRatioMgrThread)
	{
		m_QueryRatioMgrThread->SetExit(true);
		WaitForSingleObject((HANDLE)m_QueryRatioMgrThread,8000);
		if(GetExitCodeThread((HANDLE)m_QueryRatioMgrThread,&ExitCode)!=0&&ExitCode==STILL_ACTIVE)
			TerminateThread((HANDLE)m_QueryRatioMgrThread,0);
		SAFE_DELETE(m_QueryRatioMgrThread);
	}
	SAFE_DELETE(m_pTradeDataMsgQueue);

	Release();
}
void CCTPAccount::GetLogin(SLogin& login)
{
	login = m_login;
}
void CCTPAccount::Init(SLogin& login, int nAddrType, THOST_TE_RESUME_TYPE restartType)
{//nAddrType: 0 普通地址1 结算地址
	m_login = login;

	// 产生一个CThostFtdcTraderApi实例
	CThostFtdcTraderApi *pUserApi =	CThostFtdcTraderApi::CreateFtdcTraderApi();
	// 产生一个事件处理的实例
	CUserTraderSpi* sh = new CUserTraderSpi(pUserApi,login);
	// 注册一事件处理的实例
	pUserApi->RegisterSpi(sh);
	// 订阅私有流
	pUserApi->SubscribePrivateTopic(restartType);
	// 订阅公共流
	pUserApi->SubscribePublicTopic(restartType);
	// 设置交易托管系统服务的地址，可以注册多个地址备用
	for(int nServer = 0; nServer < (int)login.vecServerAddress.size(); nServer++)
	{
	//	if(login.vecServerAddress[nServer].nAddrType != nAddrType)	
	//		continue;		
		std::string strServer = login.vecServerAddress[nServer].szIP;
		char szServer[255];
		sprintf(szServer, "tcp://%s:%d", strServer.c_str(), login.vecServerAddress[nServer].nPort);

		strServer = szServer;
		pUserApi->RegisterFront(const_cast<char*>(strServer.c_str()));
		
	}
	// 使客户端开始与后台服务建立连接
	pUserApi->Init();


	SetAccountBaseInfo(login.strBrokerID, login.strUserID, login.strPassword);
	SetTrader(pUserApi, sh);		

	sh->SetAccount(this);	
	
}
void CCTPAccount::InitQueryThread()
{
	m_QueryMgrThread = new CToolThread(QueryThread,this);
}
void CCTPAccount::InitQueryRatioThread()
{
	m_QueryRatioMgrThread = new CToolThread(QueryRatioThread,this);
}
void CCTPAccount::GetAccountBaseInfo(std::string&	strBrokerID, std::string& strUserID, std::string& strPassword)
{
	m_Mutex_BrokerID.write_lock();
	strBrokerID		= m_strBrokerID;
	strUserID		= m_strUserID;
	strPassword		= m_strPassword;
	m_Mutex_BrokerID.write_unlock();
}
void CCTPAccount::SetAccountBaseInfo(std::string	strBrokerID, std::string strUserID, std::string	 strPassword)
{
	m_Mutex_BrokerID.write_lock();
	m_strBrokerID			= strBrokerID;
	m_strUserID				= strUserID;
	m_strPassword			= strPassword;
	m_Mutex_BrokerID.write_unlock();
}
void CCTPAccount::SetAccountStatus(EnumAccountStatus	enumAccountStatus)
{
	m_Mutex_AccountStatus.write_lock();
	m_enumAccountStatus = enumAccountStatus;
	m_Mutex_AccountStatus.write_unlock();
}
EnumAccountStatus CCTPAccount::GetAccountStatus()
{
	EnumAccountStatus	enumAccountStatus;
	m_Mutex_AccountStatus.write_lock();
	enumAccountStatus = m_enumAccountStatus;
	m_Mutex_AccountStatus.write_unlock();

	return enumAccountStatus;
}
void CCTPAccount::SetLoginCount(int nLoginCount)
{
	m_Mutex_loginInCount.write_lock();
	m_nLoginInCount = nLoginCount;
	m_Mutex_loginInCount.write_unlock();
}
int  CCTPAccount::GetLoginCount()
{
	int nLoginCount = 0;
	m_Mutex_loginInCount.write_lock();
	nLoginCount = m_nLoginInCount;
	m_Mutex_loginInCount.write_unlock();

	return nLoginCount;
}

CThostFtdcTraderApi* CCTPAccount::GetTraderApi()
{ 
	CThostFtdcTraderApi* pTraderApi= NULL;
	m_Mutex_TraderApi.write_lock();
	pTraderApi = m_pTraderApi;
	m_Mutex_TraderApi.write_unlock();
	return pTraderApi;
};
CUserTraderSpi*	CCTPAccount::GetTraderSpi()
{
	CUserTraderSpi* pTraderSpi = NULL;
	m_Mutex_TraderSpi.write_lock();	
	pTraderSpi= m_pTraderSpi;
	m_Mutex_TraderSpi.write_unlock();
	return	pTraderSpi;
};

void CCTPAccount::SetTrader(CThostFtdcTraderApi* pTraderApi, CUserTraderSpi* pTraderSpi)
{
	m_Mutex_TraderApi.write_lock();
	m_pTraderApi			= pTraderApi;
	m_Mutex_TraderApi.write_unlock();

	m_Mutex_TraderSpi.write_lock();	
	m_pTraderSpi			= pTraderSpi;
	m_Mutex_TraderSpi.write_unlock();
}
void CCTPAccount::Release()
{
	m_Mutex_TraderApi.write_lock();
	if(m_pTraderApi)
	{		
		//OFFER_INFO("Release begin： %s, %s\n", m_strBrokerID.c_str(), m_strUserID.c_str());
		m_pTraderApi->RegisterSpi(NULL);
		m_pTraderApi->Release();		
		m_pTraderApi = NULL;
	//	OFFER_INFO("Release end： %s, %s\n", m_strBrokerID.c_str(), m_strUserID.c_str());
	}
	m_Mutex_TraderApi.write_unlock();

	m_Mutex_TraderSpi.write_lock();	
	if(m_pTraderSpi)
	{
		delete m_pTraderSpi;
		m_pTraderSpi = NULL;
	}	
	m_Mutex_TraderSpi.write_unlock();
}
void CCTPAccount::SetCurrentOrderref(int nOrderRef)
{
	m_Mutex_nCurrentOrderref.write_lock();
	m_nCurrentOrderref = nOrderRef;
	m_Mutex_nCurrentOrderref.write_unlock();
}
void CCTPAccount::SetFrontID(int nFrontID)
{
	m_Mutex_nCTPFrontID.write_lock();
	m_nCTPFrontID = nFrontID;
	m_Mutex_nCTPFrontID.write_unlock();
}
void CCTPAccount::SetSessionID(int nSessionID)
{
	m_Mutex_nCTPSessionID.write_lock();
	m_nCTPSessionID = nSessionID;
	m_Mutex_nCTPSessionID.write_unlock();
}
int CCTPAccount::GetCurrentOrderref()
{
	int nOrderRef = 0;
	m_Mutex_nCurrentOrderref.write_lock();
	nOrderRef = m_nCurrentOrderref;
	m_Mutex_nCurrentOrderref.write_unlock();
	
	return nOrderRef;
}
int CCTPAccount::GetFrontID()
{
	int nFrontID = 0;
	m_Mutex_nCTPFrontID.write_lock();
	nFrontID = m_nCTPFrontID;
	m_Mutex_nCTPFrontID.write_unlock();

	return nFrontID;
}
int CCTPAccount::GetSessionID()
{
	int nCTPSessionID = 0;
	m_Mutex_nCTPSessionID.write_lock();
	nCTPSessionID = m_nCTPSessionID;
	m_Mutex_nCTPSessionID.write_unlock();

	return nCTPSessionID;
}
void CCTPAccount::SetMarginQuery(std::map<std::string, int>& mapInstrument)
{
	m_Mutex_mapInstrumentMargin.write_lock();
	std::map<std::string, int>::iterator it = mapInstrument.begin();
	for(; it != mapInstrument.end(); it++)
	{
		SMarginKey key;
		key.strInstrument = it->first;
		key.strHedgeFlag  = THOST_FTDC_HF_Speculation;//投机
		m_mapInstrumentMargin.insert(std::make_pair(key, 0));
	//目前只考虑投机
	//	key.strHedgeFlag  = THOST_FTDC_HF_Arbitrage;//套利
	//	m_mapInstrumentMargin.insert(std::make_pair(key, 0));

	//	key.strHedgeFlag  = THOST_FTDC_HF_Hedge;//套保
	//	m_mapInstrumentMargin.insert(std::make_pair(key, 0));
	}	
	m_Mutex_mapInstrumentMargin.write_unlock();
}
void CCTPAccount::SetCommissionQuery(std::map<std::string, int>& mapInstrument)
{
	m_Mutex_mapInstrumentCommission.write_lock();
	m_mapInstrumentCommission = mapInstrument;	
	m_Mutex_mapInstrumentCommission.write_unlock();
}
bool CCTPAccount::GetNextInstrumentOfMargin(std::string& strInstrument, std::string& strHedgeFlag)
{
	bool bFind = false;
	m_Mutex_mapInstrumentMargin.write_lock();
	std::map<SMarginKey, int>::iterator it = m_mapInstrumentMargin.begin();
	for(;it!= m_mapInstrumentMargin.end(); it++)
	{
		if(it->second == 1)
			continue;
		strInstrument = it->first.strInstrument;
		strHedgeFlag  = it->first.strHedgeFlag;
		m_mapInstrumentMargin[it->first] = 1;
	//	m_mapInstrumentMargin.erase(it);//只查询一次，如果以后查询多次，则可对map的值进行设置
		bFind = true;
		break;
	}
	m_Mutex_mapInstrumentMargin.write_unlock();

	return bFind;
}
bool CCTPAccount::FinishedQueryInstrumentMargin(std::string strInstrument, std::string strHedgeFlag)
{
	m_Mutex_mapInstrumentMargin.write_lock();
	SMarginKey key;
	key.strInstrument = strInstrument;
	key.strHedgeFlag  = strHedgeFlag;
	m_mapInstrumentMargin.erase(key);
	m_Mutex_mapInstrumentMargin.write_unlock();
	
//	if(m_mapInstrumentMargin.size() == 0)
//		CInterface_SvrTradeData::getObj().EndUserQryMargin(m_strBrokerID, m_strUserID);
	return true;
}
bool CCTPAccount::GetNextInstrumentOfCommission(std::string& strInstrument)
{
	bool bFind = false;
	m_Mutex_mapInstrumentCommission.write_lock();
	std::map<std::string, int>::iterator it = m_mapInstrumentCommission.begin();
	for(; it!= m_mapInstrumentCommission.end(); it++)
	{
		if(it->second == 1)
			continue;
		strInstrument = it->first;
		m_mapInstrumentCommission[strInstrument] = 1;
	//	m_mapInstrumentCommission.erase(it);//只查询一次，如果以后查询多次，则可对map的值进行设置
		bFind = true;
		break;
	}
	m_Mutex_mapInstrumentCommission.write_unlock();

	return bFind;
}
void CCTPAccount::DeleteInstrumrntOfComminssion(std::string& strInstrument)
{
	m_Mutex_mapInstrumentCommission.write_lock();
	std::map<std::string, int>::iterator it = m_mapInstrumentCommission.find(strInstrument);
	if(it!= m_mapInstrumentCommission.end())
	{		
		m_mapInstrumentCommission.erase(it);

		std::string str = strInstrument;
		char buf[256];
		sprintf(buf,"不查询---手续费率： %s, %s\n", m_strUserID.c_str(), strInstrument.c_str());
		OutputDebugString(buf);	
	}	
	m_Mutex_mapInstrumentCommission.write_unlock();
}
bool CCTPAccount::FinishedQueryInstrumentCommission(std::string strInstrument)
{
	m_Mutex_mapInstrumentCommission.write_lock();
	m_mapInstrumentCommission.erase(strInstrument);
	m_Mutex_mapInstrumentCommission.write_unlock();

	return true;
}
void CCTPAccount::QueryInstruments()
{
	STradeQueryData data;
	data.enumQueryContent = QUERYCOTENT_INSTRUMENT;
	data.dwBeginTickCount = GetTickCount();
	data.enumStatus		  = QUERYSTATUS_INIT;
	data.nRepeatCount	  = 0;
	data.strInstrument	  = "";
	if(m_pTradeDataMsgQueue)
		m_pTradeDataMsgQueue->AddMsg(data);	
}
void CCTPAccount::QueryCommission(std::string strInstrument)
{
	m_Mutex_setCommissionProductID.write_lock();
	m_setCommissionProductID.clear();
	m_Mutex_setCommissionProductID.write_unlock();

	STradeQueryData data;
	data.enumQueryContent = QUERYCOTENT_COMMISSION;
	data.dwBeginTickCount = GetTickCount();
	data.enumStatus		  = QUERYSTATUS_INIT;
	data.nRepeatCount	  = 0;
	data.strInstrument	  = strInstrument;
	if(m_pTradeDataMsgQueue)
		m_pTradeDataMsgQueue->AddMsg(data);		
}
void CCTPAccount::QueryMargin(std::string strInstrument)
{
	STradeQueryData data;
	data.enumQueryContent = QUERYCOTENT_MARGIN;
	data.dwBeginTickCount = GetTickCount();
	data.enumStatus		  = QUERYSTATUS_INIT;
	data.nRepeatCount	  = 0;
	data.strInstrument	  = strInstrument;
	data.strHedgeFlag     = THOST_FTDC_HF_Speculation;//目前只考虑投机

	if(m_pTradeDataMsgQueue)
		m_pTradeDataMsgQueue->AddMsg(data);		
}
void CCTPAccount::QueryOrder()
{
	STradeQueryData data;
	data.enumQueryContent = QUERYCOTENT_ORDER;
	data.dwBeginTickCount = GetTickCount();
	data.enumStatus		  = QUERYSTATUS_INIT;
	data.nRepeatCount	  = 0;
	data.strInstrument	  = "";

	if(m_pTradeDataMsgQueue)
		m_pTradeDataMsgQueue->AddMsg(data);		
}
void CCTPAccount::QueryTrade()
{
	STradeQueryData data;
	data.enumQueryContent = QUERYCOTENT_TRADE;
	data.dwBeginTickCount = GetTickCount();
	data.enumStatus		  = QUERYSTATUS_INIT;
	data.nRepeatCount	  = 0;
	data.strInstrument	  = "";

	if(m_pTradeDataMsgQueue)
		m_pTradeDataMsgQueue->AddMsg(data);		
}
void CCTPAccount::QueryFund()
{
	STradeQueryData data;
	data.enumQueryContent = QUERYCOTENT_FUND;
	data.dwBeginTickCount = GetTickCount();
	data.enumStatus		  = QUERYSTATUS_INIT;
	data.nRepeatCount	  = 0;
	data.strInstrument	  = "";

	if(m_pTradeDataMsgQueue)
		m_pTradeDataMsgQueue->AddMsg(data);		
}
void CCTPAccount::QueryPosition()
{
	STradeQueryData data;
	data.enumQueryContent = QUERYCOTENT_POSITION;
	data.dwBeginTickCount = GetTickCount();
	data.enumStatus		  = QUERYSTATUS_INIT;
	data.nRepeatCount	  = 0;
	data.strInstrument	  = "";

	if(m_pTradeDataMsgQueue)
		m_pTradeDataMsgQueue->AddMsg(data);		
}
void CCTPAccount::QueryPositionDetail()
{
	STradeQueryData data;
	data.enumQueryContent = QUERYCOTENT_POSITIONDETAIL;
	data.dwBeginTickCount = GetTickCount();
	data.enumStatus		  = QUERYSTATUS_INIT;
	data.nRepeatCount	  = 0;
	data.strInstrument	  = "";

	if(m_pTradeDataMsgQueue)
		m_pTradeDataMsgQueue->AddMsg(data);		
}
void CCTPAccount::SetCurrentQueryData(STradeQueryData	dataCurrent, CCTPAccount* pCtpAccount)
{
	m_Mutex_dataCurrent.write_lock();
	m_dataCurrent = dataCurrent;
	if(m_dataCurrent.enumQueryContent == QUERYCOTENT_COMMISSION)
		m_bQueryCommission = true;//查询费率的命令
	if(m_dataCurrent.enumQueryContent == QUERYCOTENT_MARGIN)
		m_bQueryMargin = true;//查询保证金的命令
	m_LastAccount = pCtpAccount;
	m_Mutex_dataCurrent.write_unlock();
}
void CCTPAccount::GetCurrentQueryData(STradeQueryData&	dataCurrent, CCTPAccount*& pCtpAccount, bool&	bQueryCommission, bool& bQueryMargin)
{
	m_Mutex_dataCurrent.write_lock();
	dataCurrent			= m_dataCurrent;
	bQueryCommission	= m_bQueryCommission;
	bQueryMargin		= m_bQueryMargin;
	pCtpAccount			= m_LastAccount;
	m_Mutex_dataCurrent.write_unlock();
}
void CCTPAccount::ClearbInQry() 
{
	STradeQueryData	dataCurrent;
	bool			bQueryCommission;
	bool			bQueryMargin;    
	CCTPAccount*	LastAccount;    
	GetCurrentQueryData(dataCurrent, LastAccount, bQueryCommission, bQueryMargin);
	dataCurrent.enumStatus = QUERYSTATUS_SUCCESS;

	SetCurrentQueryData(dataCurrent, NULL);

	m_bInQry = false;
	
}
void CCTPAccount::SetQueryConnectFail()
{
	STradeQueryData	dataCurrent;
	bool			bQueryCommission;
	bool			bQueryMargin;    
	CCTPAccount*	LastAccount;    
	GetCurrentQueryData(dataCurrent, LastAccount, bQueryCommission, bQueryMargin);
	if(dataCurrent.enumQueryContent == QUERYCOTENT_NULL)
		return;
	if(dataCurrent.enumStatus == QUERYSTATUS_SUCCESS)
		return;

	BrokerAccountKey BAKey;
	strcpy(BAKey.BrokerID, m_strBrokerID.c_str());
	strcpy(BAKey.AccountID, m_strUserID.c_str());
	eEventType eType;
	switch(dataCurrent.enumQueryContent)
	{
		case QUERYCOTENT_INSTRUMENT:
			{
				eType = EventTypeNeedQryInstrument;
				OFFER_INFO("断开：查询合约失败： %s, %s\n", m_strBrokerID.c_str(), m_strUserID.c_str());

				PUSH_LOG_ERROR(FirstLevelError,true,true,"断开：查询合约失败： %s, %s\n", m_strBrokerID.c_str(), m_strUserID.c_str());
			}
			break;
		case QUERYCOTENT_ORDER:
			{
				eType = EventTypeNeedQryOrder;
				OFFER_INFO("断开：查询报单失败： %s, %s\n", m_strBrokerID.c_str(), m_strUserID.c_str());
				PUSH_LOG_ERROR(FirstLevelError,true,true,"断开：查询报单失败： %s, %s\n", m_strBrokerID.c_str(), m_strUserID.c_str());

			}
			break;
		case QUERYCOTENT_TRADE:
			{
				eType = EventTypeNeedQryTrader;
				OFFER_INFO("断开：查询成交失败： %s, %s\n", m_strBrokerID.c_str(), m_strUserID.c_str());
				PUSH_LOG_ERROR(FirstLevelError,true,true,"断开：查询成交失败： %s, %s\n", m_strBrokerID.c_str(), m_strUserID.c_str());
			}
			break;
		case QUERYCOTENT_COMMISSION:
			{
				eType = EventTypeNeedQryCommission;
			}
			break;
		case QUERYCOTENT_MARGIN:
			{
				eType = EventTypeNeedQryMargin;
			}
			break;
		case QUERYCOTENT_FUND:
			{
				eType = EventTypeNeedQryFund;
				OFFER_INFO("断开：查询资金失败： %s, %s\n", m_strBrokerID.c_str(), m_strUserID.c_str());
				PUSH_LOG_ERROR(FirstLevelError,true,true,"断开：查询资金失败： %s, %s\n", m_strBrokerID.c_str(), m_strUserID.c_str());
			}
			break;
		case QUERYCOTENT_POSITION:
			{
				eType = EventTypeNeedQryPosition;
				OFFER_INFO("断开：查询持仓失败： %s, %s\n", m_strBrokerID.c_str(), m_strUserID.c_str());
				PUSH_LOG_ERROR(FirstLevelError,true,true,"断开：查询持仓失败： %s, %s\n", m_strBrokerID.c_str(), m_strUserID.c_str());
			}
			break;
		case QUERYCOTENT_POSITIONDETAIL:
			{
				eType = EventTypeNeedQryPositionDetail;
				OFFER_INFO("断开：查询持仓明细失败： %s, %s\n", m_strBrokerID.c_str(), m_strUserID.c_str());
				PUSH_LOG_ERROR(FirstLevelError,true,true,"断开：查询持仓明细失败： %s, %s\n", m_strBrokerID.c_str(), m_strUserID.c_str());
			}
			break;
	}
	if(eType == EventTypeNeedQryCommission || eType == EventTypeNeedQryMargin)
	{
		CInterface_SvrTradeData::getObj().QryFailed(BAKey, EventTypeNeedQryCommission);
		CInterface_SvrTradeData::getObj().QryFailed(BAKey, EventTypeNeedQryMargin);
		OFFER_INFO("断开：查询费率失败： %s, %s\n", m_strBrokerID.c_str(), m_strUserID.c_str());
		PUSH_LOG_ERROR(FirstLevelError,true,true,"断开：查询费率失败： %s, %s\n", m_strBrokerID.c_str(), m_strUserID.c_str());
	}
	else
		CInterface_SvrTradeData::getObj().QryFailed(BAKey, eType);

	m_bInQry = false;
	OFFER_INFO("SetQueryConnectFail：m_bInQry = false \n");
	//PUSH_LOG_ERROR(Logger,false,true,"SetQueryConnectFail：m_bInQry = false \n");
}
DWORD WINAPI CCTPAccount::QueryThread(PVOID pParam)
{
	CCTPAccount* pCtpAccount = (CCTPAccount*)pParam;
	if(pCtpAccount == NULL)
		return 0;
	
	
	
	int ReqRlt;
	while(pCtpAccount && pCtpAccount->m_QueryMgrThread && !pCtpAccount->m_QueryMgrThread->IsNeedExit())
	{		
		DWORD dwTickCount = GetTickCount();
		while(pCtpAccount->GetAccountStatus() != ACCOUNT_STATUS_Login
			&& !pCtpAccount->m_QueryMgrThread->IsNeedExit())
		{//如果没有登录成功
			Sleep(1000);
			if(GetTickCount() - dwTickCount > 60000*7)//7分钟都登不上去，就放弃了
			{
				OFFER_INFO("Release 主账号线程，登不上去： %s, %s\n", pCtpAccount->m_strBrokerID.c_str(), pCtpAccount->m_strUserID.c_str());
				//PUSH_LOG_ERROR(FirstLevelError,true,true,"释放主账号线程，登不上去： %s, %s\n", pCtpAccount->m_strBrokerID.c_str(), pCtpAccount->m_strUserID.c_str());
				pCtpAccount->Release();
				return 0;
			}
		}
		if( !pCtpAccount->IsQryTime())
		{
			Sleep(50);		
			continue;
		}	
		STradeQueryData data;
		SLogin login;
		pCtpAccount->GetLogin(login);
		if(pCtpAccount->m_pTradeDataMsgQueue->GetMsg(data))
		{	
			data.enumStatus	= QUERYSTATUS_BEGIN;		
		//	OFFER_INFO("GetMsg: %s, %s, %d", login.strBrokerID.c_str(), login.strUserID.c_str(), (int)data.enumQueryContent);
			
			pCtpAccount->SetCurrentQueryData(data, pCtpAccount);
			if(data.nRepeatCount >= 3)
			{//查询3次失败以后，则不再查询
				pCtpAccount->ProcessResultFail(data, login.strBrokerID, login.strUserID);
				continue;
			}
			
			pCtpAccount->SendQryCmd(data, ReqRlt, pCtpAccount);

			if(ReqRlt == 0)
				pCtpAccount->UpdateQryTime();	
			else
			{
				data.nRepeatCount++;
				if(data.enumQueryContent!= QUERYCOTENT_COMMISSION &&//保证金率和费率只查一次
					data.enumQueryContent!= QUERYCOTENT_MARGIN
					&& pCtpAccount->GetAccountStatus() == ACCOUNT_STATUS_Login)//如果不是登录状态，断开了，不再增加任务
				{
						pCtpAccount->m_pTradeDataMsgQueue->AddMsg(data);
				}				
			}					
		}
		else
		{//任务队列里面没有任务了
			STradeQueryData dataCurrent;
			bool			bQueryCommission;
			bool			bQueryMargin;    
			CCTPAccount*	LastAccount;     
			pCtpAccount->GetCurrentQueryData(dataCurrent, LastAccount, bQueryCommission, bQueryMargin);			
			if(LastAccount)
			{
				while( !LastAccount->IsQryTime())
				{
					Sleep(50);				
				}	
			}
			pCtpAccount->ProcessResult(dataCurrent, login.strBrokerID, login.strUserID, bQueryCommission, bQueryMargin);
		}

		Sleep(1000);
	}
	return 0;
}
bool CCTPAccount::ProcessResultFail(STradeQueryData& data, std::string strBrokerID, std::string strAccountID)
{
	switch(data.enumQueryContent)
	{
	case QUERYCOTENT_INSTRUMENT:
		{
			BrokerAccountKey BAKey;
			strcpy(BAKey.BrokerID, strBrokerID.c_str());
			strcpy(BAKey.AccountID, strAccountID.c_str());
			CInterface_SvrTradeData::getObj().QryFailed(BAKey, EventTypeNeedQryInstrument);//查询合约失败
			OFFER_INFO("查询合约失败： %s, %s\n", m_strBrokerID.c_str(), m_strUserID.c_str());
			PUSH_LOG_ERROR(FirstLevelError,true,true,"查询合约失败： %s, %s\n", m_strBrokerID.c_str(), m_strUserID.c_str());
		}
		break;
	case QUERYCOTENT_COMMISSION://费率:
	case QUERYCOTENT_MARGIN://费率:
		{				
		}
		break;
	case QUERYCOTENT_ORDER://报单
		{
			CInterface_SvrTradeData::getObj().EndUserQryOrder(strBrokerID, strAccountID);
			OFFER_INFO("查询报单失败： %s, %s\n", m_strBrokerID.c_str(), m_strUserID.c_str());
			PUSH_LOG_ERROR(FirstLevelError,true,true,"查询报单失败： %s, %s\n", m_strBrokerID.c_str(), m_strUserID.c_str());
		}
		break;
	case QUERYCOTENT_TRADE://成交
		{
			CInterface_SvrTradeData::getObj().EndUserQryTrade(strBrokerID, strAccountID);
			OFFER_INFO("查询成交失败： %s, %s\n", m_strBrokerID.c_str(), m_strUserID.c_str());
			PUSH_LOG_ERROR(FirstLevelError,true,true,"查询成交失败： %s, %s\n", m_strBrokerID.c_str(), m_strUserID.c_str());
		}
		break;		
	case QUERYCOTENT_FUND://资金
		{
			//CInterface_SvrTradeData::getObj().EndUserQryTrade(strBrokerID, strAccountID);
			OFFER_INFO("查询资金失败： %s, %s\n", m_strBrokerID.c_str(), m_strUserID.c_str());
			PUSH_LOG_ERROR(FirstLevelError,true,true,"查询资金失败： %s, %s\n", m_strBrokerID.c_str(), m_strUserID.c_str());
		}
		break;	
	case QUERYCOTENT_POSITION://持仓
		{
			CInterface_SvrTradeData::getObj().EndUserQryPosition(strBrokerID, strAccountID);
			OFFER_INFO("查询持仓失败： %s, %s\n", m_strBrokerID.c_str(), m_strUserID.c_str());
			PUSH_LOG_ERROR(FirstLevelError,true,true,"查询持仓失败： %s, %s\n", m_strBrokerID.c_str(), m_strUserID.c_str());
		}
		break;	
	case QUERYCOTENT_POSITIONDETAIL://持仓明细
		{
			CInterface_SvrTradeData::getObj().EndUserQryPositionDetail(strBrokerID, strAccountID);
			OFFER_INFO("查询持仓明细失败： %s, %s\n", m_strBrokerID.c_str(), m_strUserID.c_str());
			PUSH_LOG_ERROR(FirstLevelError,true,true,"查询持仓明细失败： %s, %s\n", m_strBrokerID.c_str(), m_strUserID.c_str());
		}
		break;	
	}
	m_bInQry = false;
	OFFER_INFO("ProcessResultFail：m_bInQry = false \n");
	//PUSH_LOG_ERROR(Logger,false,true,"ProcessResultFail：m_bInQry = false \n");
	return true;
}
bool CCTPAccount::ProcessResult(STradeQueryData& data, std::string strBrokerID, std::string strAccountID,bool bQueryCommission, bool bQueryMargin)
{
	bool bExist = false;//是否退出线程
	switch(data.enumQueryContent)
	{
	case QUERYCOTENT_COMMISSION://费率:
	case QUERYCOTENT_MARGIN://费率:
		{			
			if(bQueryCommission)
				CInterface_SvrTradeData::getObj().EndUserQryCommission(strBrokerID, strAccountID);
			if(bQueryMargin)
				CInterface_SvrTradeData::getObj().EndUserQryMargin(strBrokerID, strAccountID);
			OFFER_INFO("查询费率完毕： %s, %s", m_strBrokerID.c_str(), m_strUserID.c_str());
			//PUSH_LOG_ERROR(Logger,false,true,"查询费率完毕： %s, %s\n", m_strBrokerID.c_str(), m_strUserID.c_str());
		}
		break;
	case QUERYCOTENT_ORDER://报单
		{			
		//	bExist = true;
		}
		break;
	case QUERYCOTENT_TRADE://成交
		{			
		}
		break;	
	case QUERYCOTENT_POSITIONDETAIL:
		{//最后查询持仓明细
			bExist = true;
		}
	}
	
	return bExist;
}
bool CCTPAccount::SendQryCmd(STradeQueryData& data, int& ReqRlt, CCTPAccount* pCtpMainAccount)
{
	switch(data.enumQueryContent)
	{
	case QUERYCOTENT_INSTRUMENT://合约
		{		
			CThostFtdcQryInstrumentField QryInstrument;
			memset(&QryInstrument, 0, sizeof(QryInstrument));	
			ReqRlt = GetTraderApi()->ReqQryInstrument(&QryInstrument, GetTraderSpi()->GetRequestID());	
			
			if(ReqRlt == 0)
			{
				m_bInQry = true;
				OFFER_INFO("%x,%d,发送查询合约成功： %s, %s\n", this,GetCurrentThreadId(),m_strBrokerID.c_str(), m_strUserID.c_str());
				//PUSH_LOG_ERROR(Logger,false,true,"发送查询合约成功： %s, %s\n", m_strBrokerID.c_str(), m_strUserID.c_str());
			}
			else
			{
				OFFER_INFO("%x,%d,发送查询合约失败： %s, %s, 错误返回值[%d]\n",this, GetCurrentThreadId(),m_strBrokerID.c_str(), m_strUserID.c_str(), ReqRlt);
				PUSH_LOG_ERROR(FirstLevelError,true,true,"发送查询合约失败： %s, %s, 错误返回值[%d]\n",m_strBrokerID.c_str(), m_strUserID.c_str(), ReqRlt);
			}
		
		}
		break;
	case QUERYCOTENT_COMMISSION://费率:
		{	
			//////////////////////////////////////////////////////////////////////////
			std::string strProductID;
			bool bGetProductID = g_pOfferMain->GetProductIDByInstruments(data.strInstrument, strProductID);
			if(bGetProductID)
			{
				SLogin login;
				pCtpMainAccount->GetLogin(login);
				BrokerAccountKey BAKey;
				strcpy(BAKey.BrokerID, login.strBrokerID.c_str());
				strcpy(BAKey.AccountID,login.strUserID.c_str());

				bool bFinishedQuery = pCtpMainAccount->IsCommissionProductIDQuery_Main(BAKey, strProductID);
				if(bFinishedQuery)
					return true;
			}
			//////////////////////////////////////////////////////////////////////////

			//查询合约手续费率
			CThostFtdcQryInstrumentCommissionRateField QryInstrumentCommissionRate;
			memset(&QryInstrumentCommissionRate, 0, sizeof(QryInstrumentCommissionRate));
			strcpy(QryInstrumentCommissionRate.BrokerID, m_strBrokerID.c_str());		///经纪公司代码    
			strcpy(QryInstrumentCommissionRate.InvestorID, m_strUserID.c_str());		///投资者代码
			strcpy(QryInstrumentCommissionRate.InstrumentID,data.strInstrument.c_str()); 
			
			if(m_pTraderApi)
				ReqRlt = m_pTraderApi->ReqQryInstrumentCommissionRate(&QryInstrumentCommissionRate, GetTraderSpi()->GetRequestID());
			if(ReqRlt == 0)
			{
				m_bInQry = true;
				OFFER_INFO("查询  手续费率：%x,%d %s, %s\n", this,GetCurrentThreadId(),m_strUserID.c_str(), data.strInstrument.c_str());
				//PUSH_LOG_ERROR(Logger,false,true,"查询  手续费率：%x,%d %s, %s\n", this,GetCurrentThreadId(),m_strUserID.c_str(), data.strInstrument.c_str());
			}
			else
			{
				OFFER_INFO("查询  手续费率失败:%x %d %s, %s, 错误返回值[%d]\n",this,GetCurrentThreadId(), m_strUserID.c_str(), data.strInstrument.c_str(), ReqRlt);
				//PUSH_LOG_ERROR(FirstLevelError,true,true,"发送查询手续费率失败:%s, %s, 错误返回值[%d]\n", m_strUserID.c_str(), data.strInstrument.c_str(), ReqRlt);			
			}
		}
		break;
	case QUERYCOTENT_MARGIN://费率:
		{				
			CThostFtdcQryInstrumentMarginRateField QryInstrumentMarginRate;
			memset(&QryInstrumentMarginRate, 0, sizeof(QryInstrumentMarginRate));
			strcpy(QryInstrumentMarginRate.BrokerID, m_strBrokerID.c_str());		///经纪公司代码    
			strcpy(QryInstrumentMarginRate.InvestorID, m_strUserID.c_str());		///投资者代码
			strcpy(QryInstrumentMarginRate.InstrumentID,data.strInstrument.c_str()); 
			if(data.strHedgeFlag.length() >0)
				QryInstrumentMarginRate.HedgeFlag = data.strHedgeFlag[0]; //
		
			if(m_pTraderApi)
				ReqRlt = m_pTraderApi->ReqQryInstrumentMarginRate(&QryInstrumentMarginRate, GetTraderSpi()->GetRequestID());

			if(ReqRlt == 0)
			{	
				m_bInQry = true;
				OFFER_INFO("查询  保证金率：%x %d %s, %s\n", this,GetCurrentThreadId(),m_strUserID.c_str(), data.strInstrument.c_str());
				//PUSH_LOG_ERROR(Logger,false,true,"查询  保证金率：%x %d %s, %s\n", this,GetCurrentThreadId(),m_strUserID.c_str(), data.strInstrument.c_str());

			}
			else
			{
				OFFER_INFO("查询  保证金失败：%x %d %s, %s, 错误返回值[%d]\n",this,GetCurrentThreadId(), m_strUserID.c_str(), data.strInstrument.c_str(), ReqRlt);
				//PUSH_LOG_ERROR(FirstLevelError,true,true,"查询  保证金失败：%x %d %s, %s, 错误返回值[%d]\n",this,GetCurrentThreadId(), m_strUserID.c_str(), data.strInstrument.c_str(), ReqRlt);
			}
		}
		break;
	case QUERYCOTENT_ORDER://报单
		{
			CThostFtdcQryOrderField QryOrder;
			memset(&QryOrder, 0, sizeof(QryOrder));
			if(m_pTraderApi)
				ReqRlt = m_pTraderApi->ReqQryOrder(&QryOrder, GetTraderSpi()->GetRequestID());	
			if(ReqRlt == 0)
			{
				m_bInQry = true;
				OFFER_INFO("发送报单查询成功：%x,%d %s, %s\n",this,GetCurrentThreadId(), m_strBrokerID.c_str(), m_strUserID.c_str());		
				PUSH_LOG_ERROR(Logger,false,true,"发送报单查询成功：%s, %s\n", m_strBrokerID.c_str(), m_strUserID.c_str());	

			}
			else				
			{
				OFFER_INFO("发送报单查询失败：%x,%d %s, %s, 错误返回值[%d]\n", this,GetCurrentThreadId(),m_strBrokerID.c_str(), m_strUserID.c_str(), ReqRlt);
				PUSH_LOG_ERROR(FirstLevelError,true,true,"发送报单查询失败： %s, %s, 错误返回值[%d]\n", m_strBrokerID.c_str(), m_strUserID.c_str(), ReqRlt);
			}
		}
		break;
	case QUERYCOTENT_TRADE://成交
		{
			CThostFtdcQryTradeField QryTrade;
			memset(&QryTrade, 0, sizeof(QryTrade));
			
			if(m_pTraderApi)
				ReqRlt = m_pTraderApi->ReqQryTrade(&QryTrade, GetTraderSpi()->GetRequestID());
			if(ReqRlt == 0)
			{
				m_bInQry = true;
				OFFER_INFO("发送成交查询成功：%d %s, %s\n", GetCurrentThreadId(),m_strBrokerID.c_str(), m_strUserID.c_str());		
				//PUSH_LOG_ERROR(Logger,false,true,"发送成交查询成功： %s, %s\n", m_strBrokerID.c_str(), m_strUserID.c_str());		
			}
			else
			{
				OFFER_INFO("发送成交查询失败：%d %s, %s, 错误返回值[%d]\n", GetCurrentThreadId(),m_strBrokerID.c_str(), m_strUserID.c_str(), ReqRlt);
				//PUSH_LOG_ERROR(FirstLevelError,true,true,"发送成交查询失败： %s, %s, 错误返回值[%d]\n",m_strBrokerID.c_str(), m_strUserID.c_str(), ReqRlt);
			}
		}
		break;	
	case QUERYCOTENT_FUND:	//资金
		{
			CThostFtdcQryTradingAccountField QryTradingAccount;
			memset(&QryTradingAccount, 0, sizeof(CThostFtdcQryTradingAccountField));
			strncpy(QryTradingAccount.BrokerID, m_strBrokerID.c_str(),sizeof(QryTradingAccount.BrokerID)-1);
			strncpy(QryTradingAccount.InvestorID,m_strUserID.c_str(),sizeof(QryTradingAccount.InvestorID)-1);

		
			if(m_pTraderApi)
				ReqRlt = m_pTraderApi->ReqQryTradingAccount(&QryTradingAccount, GetTraderSpi()->GetRequestID());
			if(ReqRlt == 0)
			{
				m_bInQry = true;
				OFFER_INFO("发送资金查询成功：%d %s, %s\n", GetCurrentThreadId(),m_strBrokerID.c_str(), m_strUserID.c_str());		
				//PUSH_LOG_ERROR(Logger,false,true,"发送成交查询成功： %s, %s\n", m_strBrokerID.c_str(), m_strUserID.c_str());		
			}
			else
			{
				OFFER_INFO("发送资金查询失败：%d %s, %s, 错误返回值[%d]\n", GetCurrentThreadId(),m_strBrokerID.c_str(), m_strUserID.c_str(), ReqRlt);
				//PUSH_LOG_ERROR(FirstLevelError,true,true,"发送成交查询失败： %s, %s, 错误返回值[%d]\n",m_strBrokerID.c_str(), m_strUserID.c_str(), ReqRlt);
			}
		} 
		break;
	case QUERYCOTENT_POSITION:   //持仓
		{
			//持仓查询
			CThostFtdcQryInvestorPositionField QryInvestorPosition;
			memset(&QryInvestorPosition, 0, sizeof(QryInvestorPosition));	
			strncpy(QryInvestorPosition.BrokerID, m_strBrokerID.c_str(),sizeof(QryInvestorPosition.BrokerID)-1);
			strncpy(QryInvestorPosition.InvestorID,m_strUserID.c_str(),sizeof(QryInvestorPosition.InvestorID)-1);

			if(m_pTraderApi)
				ReqRlt = m_pTraderApi->ReqQryInvestorPosition(&QryInvestorPosition, GetTraderSpi()->GetRequestID());
			if(ReqRlt == 0)
			{
				m_bInQry = true;
				OFFER_INFO("发送持仓查询成功：%d %s, %s\n", GetCurrentThreadId(),m_strBrokerID.c_str(), m_strUserID.c_str());		
				//PUSH_LOG_ERROR(Logger,false,true,"发送成交查询成功： %s, %s\n", m_strBrokerID.c_str(), m_strUserID.c_str());		
			}
			else
			{
				OFFER_INFO("发送持仓查询失败：%d %s, %s, 错误返回值[%d]\n", GetCurrentThreadId(),m_strBrokerID.c_str(), m_strUserID.c_str(), ReqRlt);
				//PUSH_LOG_ERROR(FirstLevelError,true,true,"发送成交查询失败： %s, %s, 错误返回值[%d]\n",m_strBrokerID.c_str(), m_strUserID.c_str(), ReqRlt);
			}

		}
		break;
	case QUERYCOTENT_POSITIONDETAIL://持仓明细
		{
			//持仓明细查询
			CThostFtdcQryInvestorPositionDetailField QryInvestorPositionDetail;
			memset(&QryInvestorPositionDetail, 0, sizeof(QryInvestorPositionDetail));	
			strncpy(QryInvestorPositionDetail.BrokerID, m_strBrokerID.c_str(),sizeof(QryInvestorPositionDetail.BrokerID)-1);
			strncpy(QryInvestorPositionDetail.InvestorID,m_strUserID.c_str(),sizeof(QryInvestorPositionDetail.InvestorID)-1);

			if(m_pTraderApi)
				ReqRlt = m_pTraderApi->ReqQryInvestorPositionDetail(&QryInvestorPositionDetail, GetTraderSpi()->GetRequestID());
			if(ReqRlt == 0)
			{
				m_bInQry = true;
				OFFER_INFO("发送持仓明细查询成功：%d %s, %s\n", GetCurrentThreadId(),m_strBrokerID.c_str(), m_strUserID.c_str());		
				//PUSH_LOG_ERROR(Logger,false,true,"发送成交查询成功： %s, %s\n", m_strBrokerID.c_str(), m_strUserID.c_str());		
			}
			else
			{
				OFFER_INFO("发送持仓明细查询失败：%d %s, %s, 错误返回值[%d]\n", GetCurrentThreadId(),m_strBrokerID.c_str(), m_strUserID.c_str(), ReqRlt);
				//PUSH_LOG_ERROR(FirstLevelError,true,true,"发送成交查询失败： %s, %s, 错误返回值[%d]\n",m_strBrokerID.c_str(), m_strUserID.c_str(), ReqRlt);
			}
	
		}
		break;
	}
	if(ReqRlt == 0)
		return true;
	
	return false;
}
void CCTPAccount::InsertCommissionProductID_Main(BrokerAccountKey& BAKey, std::string strProductID)
{
	CCTPAccount* pCtpMainAccount = NULL;
	g_pOfferMain->GetCTPAccount(BAKey, pCtpMainAccount);
	if(pCtpMainAccount)
	{
		pCtpMainAccount->InsertCommissionProductID(strProductID);
	}
}
void CCTPAccount::InsertCommissionProductID(std::string strProductID)
{
	m_Mutex_setCommissionProductID.write_lock();
	m_setCommissionProductID.insert(strProductID);
	m_Mutex_setCommissionProductID.write_unlock();
}
bool CCTPAccount::IsCommissionProductIDQuery_Main(BrokerAccountKey& BAKey, std::string strProductID)
{
	bool bFind = false;
	CCTPAccount* pCtpMainAccount = NULL;
	g_pOfferMain->GetCTPAccount(BAKey, pCtpMainAccount);
	if(pCtpMainAccount)
	{
		bFind = pCtpMainAccount->IsCommissionProductIDQuery(strProductID);
	}
	return bFind;}
bool CCTPAccount::IsCommissionProductIDQuery(std::string strProductID)
{
	bool bFind = false;
	m_Mutex_setCommissionProductID.write_lock();
	std::set<std::string>::iterator it = m_setCommissionProductID.find(strProductID);
	if(it != m_setCommissionProductID.end())
		bFind = true;
	m_Mutex_setCommissionProductID.write_unlock();

	return bFind;
}
bool CCTPAccount::GetIsQueryRatio()
{
	return m_bOnlyQueryRatio;
}
void CCTPAccount::SetIsQueryRatio(bool bOnlyQueryRatio)
{
	m_bOnlyQueryRatio = bOnlyQueryRatio;
}
DWORD WINAPI CCTPAccount::QueryRatioThread(PVOID pParam)
{
	CCTPAccount* pCtpAccount = (CCTPAccount*)pParam;//
	if(pCtpAccount == NULL)
		return 0;

	int ReqRlt;
	while(pCtpAccount && pCtpAccount->m_QueryRatioMgrThread && !pCtpAccount->m_QueryRatioMgrThread->IsNeedExit())
	{		
		DWORD dwTickCount = GetTickCount();
		while(pCtpAccount->GetAccountStatus() != ACCOUNT_STATUS_Login
			&& !pCtpAccount->m_QueryRatioMgrThread->IsNeedExit())
		{//如果没有登录成功
			Sleep(1000);
			if(GetTickCount() - dwTickCount > 60000*7)//7分钟都登不上去，就放弃了
			{
				OFFER_INFO("Release 查费率线程，登不上去： %s, %s\n", pCtpAccount->m_strBrokerID.c_str(), pCtpAccount->m_strUserID.c_str());
				//PUSH_LOG_ERROR(FirstLevelError,true,true,"释放查费率线程，登不上去： %s, %s\n", pCtpAccount->m_strBrokerID.c_str(), pCtpAccount->m_strUserID.c_str());
				pCtpAccount->Release();
				return 0;
			}
		}
		if( !pCtpAccount->IsQryTime())
		{
			Sleep(50);		
			continue;
		}	

		STradeQueryData data;

		SLogin login;
		pCtpAccount->GetLogin(login);
		BrokerAccountKey BAKey;
		strcpy(BAKey.BrokerID, login.strBrokerID.c_str());
		strcpy(BAKey.AccountID,login.strUserID.c_str());
		CCTPAccount* pCtpMainAccount = NULL;
		g_pOfferMain->GetCTPAccount(BAKey, pCtpMainAccount);//从主账户的队列里获取任务信息
		if(pCtpMainAccount && pCtpMainAccount->m_pTradeDataMsgQueue->GetMsg(data))
		{	
			data.enumStatus	= QUERYSTATUS_BEGIN;		
		//	OFFER_INFO("GetMsg: %s, %s, %d", login.strBrokerID.c_str(), login.strUserID.c_str(), (int)data.enumQueryContent);			
			pCtpMainAccount->SetCurrentQueryData(data, pCtpAccount);
			if(data.nRepeatCount == 3)
			{					
				continue;//查询3次失败以后，则不再查询
			}
			if(data.enumQueryContent!= QUERYCOTENT_COMMISSION &&
				data.enumQueryContent!= QUERYCOTENT_MARGIN)
			{
				OFFER_INFO("查费率线程 执行了 %d任务", (int)data.enumQueryContent);
				pCtpMainAccount->m_pTradeDataMsgQueue->AddMsg(data);//任务重新加到主账户队列中

				OFFER_INFO("Release 查费率线程，任务队列里面没有任务了： %s, %s\n", pCtpAccount->m_strBrokerID.c_str(), pCtpAccount->m_strUserID.c_str());
				pCtpAccount->Release();
				return 0;
			}			
			
			pCtpAccount->SendQryCmd(data, ReqRlt, pCtpMainAccount);

			if(ReqRlt == 0)
				pCtpAccount->UpdateQryTime();	
			else
			{					
			/*	if(data.enumQueryContent!= QUERYCOTENT_COMMISSION &&
					data.enumQueryContent!= QUERYCOTENT_MARGIN
					&& pCtpAccount->GetAccountStatus() == ACCOUNT_STATUS_Login)//保证金率和费率只查一次
				{
					OFFER_INFO("查费率线程 执行了 %d任务", (int)data.enumQueryContent);
					pCtpMainAccount->m_pTradeDataMsgQueue->AddMsg(data);//任务重新加到主账户队列中
				}*/
			}					
		}
		else
		{//任务队列里面没有任务了
			OFFER_INFO("Release 查费率线程，任务队列里面没有任务了： %s, %s\n", pCtpAccount->m_strBrokerID.c_str(), pCtpAccount->m_strUserID.c_str());
			pCtpAccount->Release();
			return 0;
		}
		Sleep(1000);
	}
	

	return 0;
}
bool CCTPAccount::GetTradeAccountByUserID(int nTraderID, std::string& strBrokerID, std::string& strInvestID)
{//根据交易员账号得到委托交易账号
	int nUserID = nTraderID;
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
int CCTPAccount::ReqOrderInsert_Account(PlatformStru_InputOrder *pInputOrder, int nTraderID, InputOrderKey key, int nRequestID)
{
	if(GetAccountStatus() != ACCOUNT_STATUS_Login)
	{
		OFFER_INFO("下单失败,账号断开： 账号%s, frontid:%d, sessionid:%d, orderref:%s \n", m_login.strUserID.c_str(), key.nFrontID, key.nSessionID, key.szOrderRef);
		//PUSH_LOG_ERROR(FirstLevelError,true,true,"下单失败,账号断开： 账号%s, frontid:%d, sessionid:%d, orderref:%s \n", m_login.strUserID.c_str(), key.nFrontID, key.nSessionID, key.szOrderRef);
		return CF_ERROR_RISK_ACCOUNTCONNECTFAIL;
	}

	int nRet = CF_ERROR_RISK_ACCOUNTCONNECTFAIL;

	std::string strBrokerID;
	std::string strInvestor   = pInputOrder->InvestorID;	
	std::string strAccountID;//需要转化strInvestor=>accoutid;	
	GetTradeAccountByUserID(nTraderID, strBrokerID, strAccountID);
	strcpy(pInputOrder->InvestorID, strAccountID.c_str());


	int nOrderRef = ++m_nCurrentOrderref;		
	sprintf(pInputOrder->OrderRef,"%d",nOrderRef);	
	
	CThostFtdcInputOrderField tField = {0};
	strcpy(tField.BrokerID,strBrokerID.c_str());					///经纪公司代码
	strcpy(tField.InvestorID, pInputOrder->InvestorID);				///投资者代码
	strcpy(tField.InstrumentID, pInputOrder->InstrumentID);			///合约代码
	strcpy(tField.OrderRef, pInputOrder->OrderRef);					///报单引用
	strcpy(tField.UserID, pInputOrder->UserID);						///用户代码
	tField.OrderPriceType = pInputOrder->OrderPriceType;			///报单价格条件
	tField.Direction = pInputOrder->Direction;						///买卖方向
	strcpy(tField.CombOffsetFlag, pInputOrder->CombOffsetFlag);		///组合开平标志
	strcpy(tField.CombHedgeFlag, pInputOrder->CombHedgeFlag);		///组合投机套保标志
	tField.LimitPrice = pInputOrder->LimitPrice;					///价格
	tField.VolumeTotalOriginal = pInputOrder->VolumeTotalOriginal;	///数量
	tField.TimeCondition = pInputOrder->TimeCondition;				///有效期类型
	strcpy(tField.GTDDate, pInputOrder->GTDDate);					///GTD日期
	tField.VolumeCondition = pInputOrder->VolumeCondition;			///成交量类型
	tField.MinVolume = pInputOrder->MinVolume;						///最小成交量
	tField.ContingentCondition = pInputOrder->ContingentCondition;	///触发条件
	tField.StopPrice = pInputOrder->StopPrice;						///止损价
	tField.ForceCloseReason = pInputOrder->ForceCloseReason;		///强平原因
	tField.IsAutoSuspend = pInputOrder->IsAutoSuspend;				///自动挂起标志
	strcpy(tField.BusinessUnit, pInputOrder->BusinessUnit);			///业务单元
	tField.RequestID = pInputOrder->RequestID;						///请求编号
	tField.UserForceClose = pInputOrder->UserForceClose;			///用户强评标志


	SetCurrentOrderref(nOrderRef);
	STransfer keyClient;
	keyClient.key.nFrontID = key.nFrontID;
	keyClient.key.nSessionID = key.nSessionID;
	strcpy(keyClient.key.szOrderRef, key.szOrderRef);
	keyClient.strBrokerID.empty();//客户端的经纪公司ID无效
	keyClient.strID = strInvestor;	
	keyClient.ForceCloseReason = tField.ForceCloseReason;
	keyClient.UserForceClose   = tField.UserForceClose;

	STransfer keyCtp;
	keyCtp.key.nFrontID		= GetFrontID();
	keyCtp.key.nSessionID	= GetSessionID();
	sprintf_s(keyCtp.key.szOrderRef,"%d",m_nCurrentOrderref);
	keyCtp.strBrokerID		= strBrokerID;
	keyCtp.strID			= strAccountID;
	keyCtp.ForceCloseReason = '0';
	keyCtp.UserForceClose   = 0;

	InsertCTP2CleintKeyTransfer(keyCtp, keyClient);

	keyClient.ForceCloseReason = '0';
	keyClient.UserForceClose   =  0;//撤单的时候，最好强平单的标识别取消;
	InsertCleint2CTPKeyTransfer(keyClient, keyCtp);
	


	STransfer ClientOrderRef;
	ClientOrderRef.strBrokerID.empty();
	ClientOrderRef.strID = strInvestor;
	strcpy(ClientOrderRef.key.szOrderRef,key.szOrderRef);
	ClientOrderRef.key.nFrontID		= key.nFrontID;
	ClientOrderRef.key.nSessionID	= key.nSessionID;
	ClientOrderRef.ForceCloseReason = tField.ForceCloseReason;
	ClientOrderRef.UserForceClose   = tField.UserForceClose;

	SOrderRef CtpOrderRef; 
	CtpOrderRef.strBrokerID = strBrokerID;
	CtpOrderRef.strID = strAccountID;
	CtpOrderRef.nOrderRef = m_nCurrentOrderref;
	InsertCtp2ClientOrderRef(CtpOrderRef, ClientOrderRef);		

	if(tField.UserForceClose == 1 && tField.ForceCloseReason == THOST_FTDC_FCC_ForceClose)
	{//如果是风控强平单，则替换回ctp能接受的格式
		tField.UserForceClose	= 0;
		tField.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;
	}

	nRet = GetTraderApi()->ReqOrderInsert(&tField, GetTraderSpi()->GetRequestID());		
//	if(nRet == 0)
	{
		//std::vector<SOrderTransfer> vecOrderTransfer;

		SOrderTransfer sData;
		strcpy(sData.szInvestorID, strInvestor.c_str());
		sData.nFrontID		= key.nFrontID;
		sData.nSessionID	= key.nSessionID;
		sData.nOrderRef     = atoi(key.szOrderRef);

		strcpy(sData.szBrokerID, strBrokerID.c_str());
		strcpy(sData.szAccountID, strAccountID.c_str());
		sData.nCTPFrontID   = GetFrontID();
		sData.nCTPSessionID = GetSessionID();
		sData.nCTPOrderRef  = nOrderRef;
		if(pInputOrder->UserForceClose == 1 && pInputOrder->ForceCloseReason ==THOST_FTDC_FCC_ForceClose)
		{//如果是风控强平单，则替换回ctp能接受的格式
			sData.UserForceClose	= 1;
			sData.ForceCloseReason = THOST_FTDC_FCC_ForceClose;
		}
		else
		{
			sData.UserForceClose	= 0;
			sData.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;
		}

		CSTransferMsgQueue::Get()->AddTransferMsg(sData, 1);
		//vecOrderTransfer.push_back(sData);
		//int nError = 0;
		//CInterface_SvrDBOpr::getObj().SaveOrderTransfer(vecOrderTransfer, nError);
	//	OFFER_INFO("下单成功： frontid:%d, sessionid:%d, orderref:%s ",key.nFrontID, key.nSessionID, key.szOrderRef);
	}
	if(nRet != 0)
	{
		OFFER_INFO("下单失败： accountid:%s, frontid:%d, sessionid:%d, orderref:%s \n",strInvestor.c_str(), key.nFrontID, key.nSessionID, key.szOrderRef);
		//PUSH_LOG_ERROR(FirstLevelError,true,true,"下单失败： accountid:%s, frontid:%d, sessionid:%d, orderref:%s \n",strInvestor.c_str(), key.nFrontID, key.nSessionID, key.szOrderRef);

	}

	return nRet;
}
int CCTPAccount::ReqOrderAction_Account(CThostFtdcInputOrderActionField *pInputOrderAction, int nTraderID, InputOrderKey key, int nRequestID)
{
	if(GetAccountStatus() != ACCOUNT_STATUS_Login)
	{
		OFFER_INFO("撤单失败,账号断开： 账号%s, frontid:%d, sessionid:%d, orderref:%s \n", m_login.strUserID.c_str(), key.nFrontID, key.nSessionID, key.szOrderRef);
		//PUSH_LOG_ERROR(FirstLevelError,true,true,"撤单失败,账号断开： 账号%s, frontid:%d, sessionid:%d, orderref:%s \n", m_login.strUserID.c_str(), key.nFrontID, key.nSessionID, key.szOrderRef);

		return CF_ERROR_RISK_ACCOUNTCONNECTFAIL;
	}

	std::string strInvestorID = pInputOrderAction->InvestorID;
	std::string strBrokerID;
	std::string strAccountID;
	GetTradeAccountByUserID(nTraderID, strBrokerID, strAccountID);

	strcpy(pInputOrderAction->BrokerID, strBrokerID.c_str());		
	strcpy(pInputOrderAction->InvestorID, strAccountID.c_str());				///投资者代码

	pInputOrderAction->ActionFlag = THOST_FTDC_AF_Delete;//撤单
	pInputOrderAction->FrontID		= GetFrontID();
	pInputOrderAction->SessionID	= GetSessionID();
	//OrderRefID 怎么处理是个问题
	
	STransfer tfClient;
	tfClient.key.nFrontID	= key.nFrontID;
	tfClient.key.nSessionID = key.nSessionID;
	strcpy(tfClient.key.szOrderRef, key.szOrderRef);
	tfClient.strBrokerID.empty();
	tfClient.strID = strInvestorID;
	tfClient.ForceCloseReason = '0';
	tfClient.UserForceClose   = 0;

	STransfer keyCTP;
	bool bRet = GetCleint2CTPKeyTransfer(tfClient, keyCTP);
	if(!bRet)
	{//找不到撤单的orderref了
		return CF_ERROR_RISK_NOTVALIDACTION;
	}
	strcpy(pInputOrderAction->OrderRef, keyCTP.key.szOrderRef);	
	int nRet = GetTraderApi()->ReqOrderAction((CThostFtdcInputOrderActionField *)pInputOrderAction, GetTraderSpi()->GetRequestID());	
	return nRet;
}
void CCTPAccount::InsertCleint2CTPKeyTransfer(STransfer keyClient, STransfer keyCTP)
{
	m_Mutex_Cleint2CTPKeyTransfer.write_lock();
	m_Cleint2CTPKeyTransfer[keyClient] = keyCTP;
	m_Mutex_Cleint2CTPKeyTransfer.write_unlock();
}
bool CCTPAccount::GetCleint2CTPKeyTransfer(STransfer keyClient, STransfer& keyCTP)
{	
	bool bFind =false;
	m_Mutex_Cleint2CTPKeyTransfer.write_lock();
	std::map<STransfer, STransfer>::iterator it = m_Cleint2CTPKeyTransfer.find(keyClient);
	if(it != m_Cleint2CTPKeyTransfer.end())
	{
		keyCTP = it->second;
		bFind = true;
	}	
	m_Mutex_Cleint2CTPKeyTransfer.write_unlock();

	return bFind;
}
//增加一条ctp（FrontID,SessionID,OrderRef）到客户端（FrontID,SessionID,OrderRef）的转换关系
void CCTPAccount::InsertCTP2CleintKeyTransfer(STransfer keyCTP, STransfer keyClient)
{
	m_Mutex_CTP2ClientKeyTransfer.write_lock();
	m_CTP2ClientKeyTransfer[keyCTP] = keyClient;
	m_Mutex_CTP2ClientKeyTransfer.write_unlock();
}
//根据 CTP（FrontID,SessionID,OrderRef）找到对应的 CTP客户端的ID相关的（FrontID,SessionID,OrderRef）
bool CCTPAccount::GetCTP2CleintKeyTransfer(STransfer keyCTP, STransfer& keyClient)
{
	bool bFind =false;
	m_Mutex_CTP2ClientKeyTransfer.write_lock();
	std::map<STransfer, STransfer>::iterator it = m_CTP2ClientKeyTransfer.find(keyCTP);
	if(it != m_CTP2ClientKeyTransfer.end())
	{
		keyClient = it->second;
		bFind = true;
	}	
	m_Mutex_CTP2ClientKeyTransfer.write_unlock();

	return bFind;
}
void CCTPAccount::InsertCtp2ClientOrderRef(SOrderRef CtpOrderRef, STransfer ClientOrderRef)
{
	m_Mutex_CtpOrderref2Client.write_lock();
	m_CtpOrderref2Client[CtpOrderRef]  = ClientOrderRef;
	m_Mutex_CtpOrderref2Client.write_unlock();
}
bool CCTPAccount::GetClientOrderRef(SOrderRef CtpOrderRef, STransfer& ClientOrderRef)
{
	bool bFind =false;
	m_Mutex_CtpOrderref2Client.write_lock();
	std::map<SOrderRef, STransfer>::iterator it = m_CtpOrderref2Client.find(CtpOrderRef);
	if(it != m_CtpOrderref2Client.end())
	{
		ClientOrderRef = it->second;
		bFind = true;
	}	
	m_Mutex_CtpOrderref2Client.write_unlock();

	return bFind;
}
void CCTPAccount::Insertordersysid2ClientOrderRef(SOrderSysIDRef orderSysIDRef, STransfer ClientOrderRef)
{
	m_Mutex_CtpOrderSysID2Client.write_lock();
	m_CtpOrderSysID2Client[orderSysIDRef] = ClientOrderRef;
	m_Mutex_CtpOrderSysID2Client.write_unlock();
}

bool CCTPAccount::GetRefByOrderSysID(SOrderSysIDRef orderSysIDRef, STransfer& ClientOrderRef)
{
	bool bFind =false;
	m_Mutex_CtpOrderSysID2Client.write_lock();
	std::map<SOrderSysIDRef, STransfer>::iterator it = m_CtpOrderSysID2Client.find(orderSysIDRef);
	if(it != m_CtpOrderSysID2Client.end())
	{
		ClientOrderRef = it->second;
		bFind = true;
	}	
	m_Mutex_CtpOrderSysID2Client.write_unlock();
	return bFind;
}
int CCTPAccount::GetQueryMargin()
{
	int nQueryMargin = 0;
	m_Mutex_QueryMargin.write_lock();
	nQueryMargin = m_nQueryMargin;
	m_Mutex_QueryMargin.write_unlock();

	return nQueryMargin;
}

void CCTPAccount::SetQueryMargin()
{
	m_Mutex_QueryMargin.write_lock();
	m_nQueryMargin = 1;
	m_Mutex_QueryMargin.write_unlock();
}

int CCTPAccount::GetQueryCommission()
{
	int nQueryCommission = 0;
	m_Mutex_QueryCommission.write_lock();
	nQueryCommission = m_nQueryCommission;
	m_Mutex_QueryCommission.write_unlock();

	return nQueryCommission;
}
void CCTPAccount:: SetQueryCommission()
{
	m_Mutex_QueryCommission.write_lock();
	m_nQueryCommission = 1;
	m_Mutex_QueryCommission.write_unlock();
}
//是否到达查询时刻。ctp规定每秒只能有一次查询 
bool CCTPAccount::IsQryTime(void)
{
	DWORD CurTickCount=GetTickCount();
	if(!m_bInQry)
	{                                                       //没有在途查询
		if(CurTickCount>=m_LastQryTime && CurTickCount-m_LastQryTime > 1000 ||
			CurTickCount<m_LastQryTime && CurTickCount+((DWORD)0xffffffff-m_LastQryTime) > 1000)
			return true;
	}
	else
	{                                                       //有在途查询，超时为8s
		if(CurTickCount>=m_LastQryTime && CurTickCount-m_LastQryTime > 8000 ||
			CurTickCount<m_LastQryTime && CurTickCount+((DWORD)0xffffffff-m_LastQryTime) > 8000)
			return true;
	}
	return false;
}

//更新查询时刻
void CCTPAccount::UpdateQryTime(void)
{
	m_LastQryTime = GetTickCount();
}
#ifdef  SIMU //模拟交易相关的都在这个宏里面

void CCTPAccount::Init_simu(SLogin& login)
{
	m_login = login;
	// 产生一个CThostFtdcTraderApi实例
	CThostFtdcTraderApi *pUserApi =	NULL;
	// 产生一个事件处理的实例
	CUserTraderSpi* sh = new CUserTraderSpi(pUserApi,login);

	SetAccountBaseInfo(login.strBrokerID, login.strUserID, login.strPassword);
	SetTrader(pUserApi, sh);		

	sh->SetAccount(this);	

}
int CCTPAccount::ReqOrderInsert_SimuAccount(PlatformStru_InputOrder *pInputOrder, int nTraderID, InputOrderKey key, int nRequestID)
{
	if(GetAccountStatus() != ACCOUNT_STATUS_Login)
	{
		OFFER_INFO("下单失败,账号断开： 账号%s, frontid:%d, sessionid:%d, orderref:%s \n", m_login.strUserID.c_str(), key.nFrontID, key.nSessionID, key.szOrderRef);
		//PUSH_LOG_ERROR(FirstLevelError,true,true,"下单失败,账号断开： 账号%s, frontid:%d, sessionid:%d, orderref:%s \n", m_login.strUserID.c_str(), key.nFrontID, key.nSessionID, key.szOrderRef);
		return CF_ERROR_RISK_ACCOUNTCONNECTFAIL;
	}

	int nRet = CF_ERROR_RISK_ACCOUNTCONNECTFAIL;

	std::string strBrokerID;
	std::string strInvestor   = pInputOrder->InvestorID;	
	std::string strAccountID;//需要转化strInvestor=>accoutid;	
	GetTradeAccountByUserID(nTraderID, strBrokerID, strAccountID);
	strcpy(pInputOrder->InvestorID, strAccountID.c_str());


	int nOrderRef = ++m_nCurrentOrderref;		
	sprintf(pInputOrder->OrderRef,"%d",nOrderRef);	

	CThostFtdcInputOrderField tField = {0};
	strcpy(tField.BrokerID,strBrokerID.c_str());					///经纪公司代码
	strcpy(tField.InvestorID, pInputOrder->InvestorID);				///投资者代码
	strcpy(tField.InstrumentID, pInputOrder->InstrumentID);			///合约代码
	strcpy(tField.OrderRef, pInputOrder->OrderRef);					///报单引用
	strcpy(tField.UserID, pInputOrder->UserID);						///用户代码
	tField.OrderPriceType = pInputOrder->OrderPriceType;			///报单价格条件
	tField.Direction = pInputOrder->Direction;						///买卖方向
	strcpy(tField.CombOffsetFlag, pInputOrder->CombOffsetFlag);		///组合开平标志
	strcpy(tField.CombHedgeFlag, pInputOrder->CombHedgeFlag);		///组合投机套保标志
	tField.LimitPrice = pInputOrder->LimitPrice;					///价格
	tField.VolumeTotalOriginal = pInputOrder->VolumeTotalOriginal;	///数量
	tField.TimeCondition = pInputOrder->TimeCondition;				///有效期类型
	strcpy(tField.GTDDate, pInputOrder->GTDDate);					///GTD日期
	tField.VolumeCondition = pInputOrder->VolumeCondition;			///成交量类型
	tField.MinVolume = pInputOrder->MinVolume;						///最小成交量
	tField.ContingentCondition = pInputOrder->ContingentCondition;	///触发条件
	tField.StopPrice = pInputOrder->StopPrice;						///止损价
	tField.ForceCloseReason = pInputOrder->ForceCloseReason;		///强平原因
	tField.IsAutoSuspend = pInputOrder->IsAutoSuspend;				///自动挂起标志
	strcpy(tField.BusinessUnit, pInputOrder->BusinessUnit);			///业务单元
	tField.RequestID = pInputOrder->RequestID;						///请求编号
	tField.UserForceClose = pInputOrder->UserForceClose;			///用户强评标志


	SetCurrentOrderref(nOrderRef);
	STransfer keyClient;
	keyClient.key.nFrontID = key.nFrontID;
	keyClient.key.nSessionID = key.nSessionID;
	strcpy(keyClient.key.szOrderRef, key.szOrderRef);
	keyClient.strBrokerID.empty();//客户端的经纪公司ID无效
	keyClient.strID = strInvestor;	
	keyClient.ForceCloseReason = tField.ForceCloseReason;
	keyClient.UserForceClose   = tField.UserForceClose;

	STransfer keyCtp;
	keyCtp.key.nFrontID		= GetFrontID();
	keyCtp.key.nSessionID	= GetSessionID();
	sprintf_s(keyCtp.key.szOrderRef,"%d",m_nCurrentOrderref);
	keyCtp.strBrokerID		= strBrokerID;
	keyCtp.strID			= strAccountID;
	keyCtp.ForceCloseReason = '0';
	keyCtp.UserForceClose   = 0;

	InsertCTP2CleintKeyTransfer(keyCtp, keyClient);

	keyClient.ForceCloseReason = '0';
	keyClient.UserForceClose   =  0;//撤单的时候，最好强平单的标识别取消
	InsertCleint2CTPKeyTransfer(keyClient, keyCtp);
	


	STransfer ClientOrderRef;
	ClientOrderRef.strBrokerID.empty();
	ClientOrderRef.strID = strInvestor;
	strcpy(ClientOrderRef.key.szOrderRef,key.szOrderRef);
	ClientOrderRef.key.nFrontID		= key.nFrontID;
	ClientOrderRef.key.nSessionID	= key.nSessionID;
	ClientOrderRef.ForceCloseReason = tField.ForceCloseReason;
	ClientOrderRef.UserForceClose   = tField.UserForceClose;

	SOrderRef CtpOrderRef; 
	CtpOrderRef.strBrokerID = strBrokerID;
	CtpOrderRef.strID = strAccountID;
	CtpOrderRef.nOrderRef = m_nCurrentOrderref;
	InsertCtp2ClientOrderRef(CtpOrderRef, ClientOrderRef);		
	
	if(tField.UserForceClose == 1 && tField.ForceCloseReason == THOST_FTDC_FCC_ForceClose)
	{//如果是风控强平单，则替换回ctp能接受的格式
		tField.UserForceClose	= 0;
		tField.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;
	}

	PlatformStru_InstrumentInfo info;		
	CInterface_SvrTradeData::getObj().GetInstrumentInfo(tField.InstrumentID,info);
	std::string strExchangeID = info.ExchangeID;	

	std::vector<CThostFtdcOrderField> vecOrder; 
	std::vector<CThostFtdcTradeField> vecTrade;
	nRet = CInterface_SvrSimulateTrade::getObj().InsertOrder(tField, m_pTraderSpi->GetRequestID(), strExchangeID, vecOrder, vecTrade);
	//if(nRet == 0)
	{
	//	std::vector<SOrderTransfer> vecOrderTransfer;

		SOrderTransfer sData;
		strcpy(sData.szInvestorID, strInvestor.c_str());
		sData.nFrontID		= key.nFrontID;
		sData.nSessionID	= key.nSessionID;
		sData.nOrderRef     = atoi(key.szOrderRef);

		strcpy(sData.szBrokerID, strBrokerID.c_str());
		strcpy(sData.szAccountID, strAccountID.c_str());
		sData.nCTPFrontID   = GetFrontID();
		sData.nCTPSessionID = GetSessionID();
		sData.nCTPOrderRef  = nOrderRef;
		
		if(pInputOrder->UserForceClose == 1 && pInputOrder->ForceCloseReason == THOST_FTDC_FCC_ForceClose)
		{//如果是风控强平单，则替换回ctp能接受的格式
			sData.UserForceClose	= 1;
			sData.ForceCloseReason = THOST_FTDC_FCC_ForceClose;
		}
		else
		{
			sData.UserForceClose	= 0;
			sData.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;
		}

	//	vecOrderTransfer.push_back(sData);
		CSTransferMsgQueue::Get()->AddTransferMsg(sData, 1);
	//	int nError = 0;
	//	CInterface_SvrDBOpr::getObj().SaveOrderTransfer(vecOrderTransfer, nError);
	//	OFFER_INFO("下单成功： accountid:%s, frontid:%d, sessionid:%d, orderref:%s ",strInvestor.c_str(), key.nFrontID, key.nSessionID, key.szOrderRef);
	}
	if(nRet != 0)
	{
		OFFER_INFO("下单失败： accountid:%s, frontid:%d, sessionid:%d, orderref:%s \n",strInvestor.c_str(),key.nFrontID, key.nSessionID, key.szOrderRef);
		//PUSH_LOG_ERROR(FirstLevelError,true,true,"下单失败： accountid:%s, frontid:%d, sessionid:%d, orderref:%s \n",strInvestor.c_str(),key.nFrontID, key.nSessionID, key.szOrderRef);
	}

	for(int p = 0; p < (int)vecOrder.size(); p++)
	{
		CThostFtdcOrderField& order = vecOrder[p];
		ProcessOrder(order);
	}

	for(int p = 0; p < (int)vecTrade.size(); p++)
	{
		CThostFtdcTradeField& trade = vecTrade[p];
		ProcessTrade(trade);
	}

	return nRet;
}
void CCTPAccount::ProcessOrder(CThostFtdcOrderField& order)
{
	PlatformStru_OrderInfo data(false);
	CopyOrderField(data, order);	
	//原始数据交由交易数据管理存储
	CInterface_SvrTradeData::getObj().PushCTPOrder(data);

	STransfer keyCtp,keyClient;
	keyCtp.key.nFrontID		= order.FrontID;
	keyCtp.key.nSessionID	= order.SessionID;
	sprintf(keyCtp.key.szOrderRef, "%d", atoi(order.OrderRef));
	keyCtp.strBrokerID = order.BrokerID;;
	keyCtp.strID = order.InvestorID;
	keyCtp.ForceCloseReason = '0';
	keyCtp.UserForceClose   = 0;

	BrokerAccountKey BAKey(order.BrokerID, order.InvestorID);
	if(!GetCTP2CleintKeyTransfer(keyCtp, keyClient))	
	{//没有得到 orderref相关的客户端key信息		
		OFFER_INFO("报单回报转换失败： accountid:%s, nFrontID:%d, nSessionID:%d, orderRef:%s\n",order.InvestorID, order.FrontID, order.SessionID, order.OrderRef);
		//PUSH_LOG_ERROR(FirstLevelError,true,true,"报单回报转换失败： accountid:%s, nFrontID:%d, nSessionID:%d, orderRef:%s\n",strAccountID.c_str(), order.FrontID, order.SessionID, order.OrderRef);
		return;
	}

//////////////////////////////////////////////////////////////////////////
	//保存报单到数据库库
	std::string strAccount = data.InvestorID;
	std::vector<PlatformStru_OrderInfo> vecOrder;
//	strcpy(data.UserID, keyClient.strID.c_str());
	vecOrder.push_back(data);		
	std::string strTradingDay;
	CInterface_SvrTradeData::getObj().GetCurrentTradingDay(strTradingDay);
	CInterface_SvrTradeData::getObj().GetInterfaceDBOpr()->WriteOrderInfos("SIMU_CTP_ORDERS", strAccount, strTradingDay, vecOrder, false);
//////////////////////////////////////////////////////////////////////////
	data.SessionID = keyClient.key.nSessionID;
	data.FrontID   = keyClient.key.nFrontID;
	strcpy(data.OrderRef, keyClient.key.szOrderRef); 
	strcpy(data.InvestorID, keyClient.strID.c_str());
	strcpy(data.szAccount, keyClient.strID.c_str());

	if(strcmp(order.OrderSysID, "")!=0)
	{
		SOrderTransfer sOrderTransfer;
		strcpy(sOrderTransfer.szAccountID, order.InvestorID);
		sOrderTransfer.nFrontID = keyClient.key.nFrontID;
		sOrderTransfer.nSessionID = keyClient.key.nSessionID;
		sOrderTransfer.nOrderRef = atoi(keyClient.key.szOrderRef);

		strcpy(sOrderTransfer.orderSysID, order.OrderSysID);
		strcpy(sOrderTransfer.ExchangeID, order.ExchangeID);
		//CInterface_SvrDBOpr::getObj().UpdateOrderTransfer(sOrderTransfer);
		CSTransferMsgQueue::Get()->AddTransferMsg(sOrderTransfer, 0);

		SOrderSysIDRef orderSysIDRef;
		orderSysIDRef.strExchanggeID = order.ExchangeID;
		orderSysIDRef.strOrderSysID  = sOrderTransfer.orderSysID;
		Insertordersysid2ClientOrderRef(orderSysIDRef, keyClient);
	}

	CInterface_SvrTradeData::getObj().PushOrder(data);



}
void CCTPAccount::ProcessTrade(CThostFtdcTradeField& trade)
{
	PlatformStru_TradeInfo data;
	CopyTradeRecordField(data, trade);
	//原始数据交由交易数据管理存储
	CInterface_SvrTradeData::getObj().PushCTPTrader(data);

	//std::string strAccountID = login.strUserID;
	int nOrderRef = atoi(trade.OrderRef);

	SOrderSysIDRef orderSysIDRef;
	orderSysIDRef.strExchanggeID = trade.ExchangeID;
	orderSysIDRef.strOrderSysID  = trade.OrderSysID;

	STransfer ClientRef;
	if(!GetRefByOrderSysID(orderSysIDRef, ClientRef))
	{
		OFFER_INFO("成交通知转换失败：AccountID:%s, ExchanggeID:%s, OrderSysID:%s ",trade.InvestorID,orderSysIDRef.strExchanggeID.c_str(), orderSysIDRef.strOrderSysID.c_str());
		//PUSH_LOG_ERROR(FirstLevelError,true,true,"成交通知转换失败：AccountID:%s, ExchanggeID:%s, OrderSysID:%s \n",login.strUserID.c_str(),orderSysIDRef.strExchanggeID.c_str(), orderSysIDRef.strOrderSysID.c_str());
		return;//不是机构版下单的信息
	}
//////////////////////////////////////////////////////////////////////////
	//保存报单到数据库库
	std::string strAccount = data.InvestorID;
	std::vector<PlatformStru_TradeInfo> vecTrade;
	//strcpy(data.UserID, ClientRef.strID.c_str());
	vecTrade.push_back(data);		
	std::string strTradingDay;
	CInterface_SvrTradeData::getObj().GetCurrentTradingDay(strTradingDay);	
	CInterface_SvrTradeData::getObj().GetInterfaceDBOpr()->WriteTradeInfos("SIMU_CTP_TRADES", strAccount, strTradingDay, vecTrade, false);
//////////////////////////////////////////////////////////////////////////

	strcpy(data.OrderRef, ClientRef.key.szOrderRef);	
	strcpy(data.InvestorID, ClientRef.strID.c_str());
	strcpy(data.szAccount, ClientRef.strID.c_str());

	CInterface_SvrTradeData::getObj().PushTrade(data);		



}
int CCTPAccount::ReqOrderAction_SimuAccount(CThostFtdcInputOrderActionField *pInputOrderAction, int nTraderID, InputOrderKey key, int nRequestID)
{
	if(GetAccountStatus() != ACCOUNT_STATUS_Login)
	{
		OFFER_INFO("撤单失败,账号断开： 账号%s, frontid:%d, sessionid:%d, orderref:%s ", m_login.strUserID.c_str(), key.nFrontID, key.nSessionID, key.szOrderRef);
		//PUSH_LOG_ERROR(FirstLevelError,true,true,"撤单失败,账号断开： 账号%s, frontid:%d, sessionid:%d, orderref:%s \n", m_login.strUserID.c_str(), key.nFrontID, key.nSessionID, key.szOrderRef);
		return CF_ERROR_RISK_ACCOUNTCONNECTFAIL;
	}

	std::string strInvestorID = pInputOrderAction->InvestorID;
	std::string strBrokerID;
	std::string strAccountID;
	GetTradeAccountByUserID(nTraderID, strBrokerID, strAccountID);

	strcpy(pInputOrderAction->BrokerID, strBrokerID.c_str());		
	strcpy(pInputOrderAction->InvestorID, strAccountID.c_str());				///投资者代码

	pInputOrderAction->ActionFlag = THOST_FTDC_AF_Delete;//撤单
	pInputOrderAction->FrontID		= GetFrontID();
	pInputOrderAction->SessionID	= GetSessionID();
	//OrderRefID 怎么处理是个问题

	STransfer tfClient;
	tfClient.key.nFrontID	= key.nFrontID;
	tfClient.key.nSessionID = key.nSessionID;
	strcpy(tfClient.key.szOrderRef, key.szOrderRef);
	tfClient.strBrokerID.empty();
	tfClient.strID = strInvestorID;
	tfClient.ForceCloseReason = '0';
	tfClient.UserForceClose   = 0;

	STransfer keyCTP;
	bool bRet = GetCleint2CTPKeyTransfer(tfClient, keyCTP);
	if(!bRet)
	{//找不到撤单的orderref了
		return -5;
	}
	strcpy(pInputOrderAction->OrderRef, keyCTP.key.szOrderRef);	
	std::vector<CThostFtdcOrderField> vecOrder;
	int nRet = CInterface_SvrSimulateTrade::getObj().CancelOrder(*(CThostFtdcInputOrderActionField*)pInputOrderAction,  GetTraderSpi()->GetRequestID(), vecOrder);
	for(int p = 0; p < (int)vecOrder.size(); p++)
	{
		CThostFtdcOrderField& order = vecOrder[p];
		ProcessOrder(order);
	}	
	
	return nRet;
}
void CCTPAccount::OnRspQryOrder(CThostFtdcOrderField *pOrder)
{
	if(m_pTraderSpi)
		m_pTraderSpi->OnRspQryOrder(pOrder, NULL, 0, false);
}
void CCTPAccount::OnRspQryTrade(CThostFtdcTradeField *pTrade)
{
	if(m_pTraderSpi)
		m_pTraderSpi->OnRspQryTrade(pTrade, NULL, 0, false);
}
#endif