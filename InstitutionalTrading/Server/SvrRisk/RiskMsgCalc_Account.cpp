#include "stdafx.h"
#include "DataCenter.h"
#include "RiskMsgCalc_Account.h"
#include "RiskProcess.h"
#include "ThreadSharedData.h"
extern bool g_bSendToExcute;//通过网络发送给交易执行命令
extern  int g_hSocket;      //主服务器套接字
CRiskMsgCalc_Account::CRiskMsgCalc_Account(CThreadSharedData* pThreadSharedData)
	:m_pThreadShareData(pThreadSharedData)
{	
	m_RiskProcess = new CRiskProcess(pThreadSharedData);
}

CRiskMsgCalc_Account::~CRiskMsgCalc_Account(void)
{
	SAFE_DELETE(m_RiskProcess);
}
void CRiskMsgCalc_Account::ChangeRiskPlan()
{//风控方案改变，则需要算所有账号的所有风险指标
	std::vector<TradeAccount> vec;
	CInterface_SvrBrokerInfo::getObj().GetTradeAccounts(vec);	//得到所有委托交易账号
	if(vec.size() ==0)
		return;
	for(int i=0; i < (int)vec.size(); i++)
	{
		TradeAccount& tradeAccount = vec[i];
		BrokerInfo brokeInfo;
		CInterface_SvrBrokerInfo::getObj().GetBrokerInfoByID(tradeAccount.nBrokerID, brokeInfo);

		NewPosition(brokeInfo.szBrokerCode,  tradeAccount.szTradeAccount);		
	}
}
//交易数据管理数据准备好，持仓判断
void CRiskMsgCalc_Account::TradeDataInit()
{
	std::vector<TradeAccount> vec;
	CInterface_SvrBrokerInfo::getObj().GetTradeAccounts(vec);	//得到所有委托交易账号
	if(vec.size() ==0)
		return;
	for(int i=0; i < (int)vec.size(); i++)
	{
		TradeAccount& tradeAccount = vec[i];
		BrokerInfo brokeInfo;
		CInterface_SvrBrokerInfo::getObj().GetBrokerInfoByID(tradeAccount.nBrokerID, brokeInfo);

		NewPosition(brokeInfo.szBrokerCode,  tradeAccount.szTradeAccount);		
	}
}
//资金到来
void CRiskMsgCalc_Account::NewFundAccount(const std::string & BrokerID, const string& InvestorID )
{//ProcessForbid 必须要知道该账户所有的限制交易和限制开仓的状态，不然没法对账户准确设置
	NewPosition(BrokerID, InvestorID);
}
//持仓到来
void CRiskMsgCalc_Account::NewPosition(const std::string & BrokerID, const string& InvestorID, bool bDoResponse)
{//今日限亏、账户限亏和基金净值风险如果设置了市价强平的话，只要风险还在，有持仓就应该强平 ,bDoResponse的意义就在这里

	char szTime[6];
	SYSTEMTIME st;
	GetLocalTime(&st);
	memset(szTime, 0, sizeof(szTime));
	sprintf(szTime,"%.2d:%.2d",st.wHour,st.wMinute);
	std::string strTime = szTime;

	ActiveCalcTodayLossRisk(BrokerID, InvestorID, strTime, bDoResponse);
	ActiveCalcMarginRatioRisk(BrokerID, InvestorID, strTime);
	ActiveCalcFundNetValueRisk(BrokerID, InvestorID, strTime, bDoResponse);

	std::set<string> setMaxPositionID;
	std::set<string> setMarketShockID;
	if(m_RiskProcess)
	{
		m_RiskProcess->GetMaxPositionRisk(BrokerID, InvestorID, setMaxPositionID);
		m_RiskProcess->GetMarketShockRisk(BrokerID, InvestorID, setMarketShockID);
	}

	vector<PlatformStru_Position> vecPosition;
	if(CF_ERROR_SUCCESS == CInterface_SvrTradeData::getObj().GetAccountPositions(BrokerID, InvestorID, "", vecPosition))
	{		
		for(int i =0; i < (int)vecPosition.size(); i++)
		{
			PlatformStru_Position PosField = vecPosition[i];
			std::string InstrumentID = PosField.InstrumentID;
			ActiveCalcMaxPositionRisk(BrokerID,InvestorID,strTime, InstrumentID);
			ActiveCalcMarketShockRisk(BrokerID,InvestorID,strTime, InstrumentID);
			setMaxPositionID.erase(InstrumentID);
			setMarketShockID.erase(InstrumentID);
		}
	}
	std::set<string>::iterator itMaxPosition = setMaxPositionID.begin();
	for(; itMaxPosition != setMaxPositionID.end(); itMaxPosition++)
	{//持仓已经没有，但是风险事件还在的情况
		std::string strInstrument = *itMaxPosition;
		ActiveCalcMaxPositionRisk(BrokerID, InvestorID, strTime, strInstrument);
	}
	std::set<string>::iterator itMarketShock = setMarketShockID.begin();
	for(; itMarketShock != setMarketShockID.end(); itMarketShock++)
	{//持仓已经没有，但是风险事件还在的情况
		std::string strInstrument = *itMarketShock;
		ActiveCalcMarketShockRisk(BrokerID, InvestorID,strTime, strInstrument);
	}


//	ProcessForbid(InvestorID);
//	ProcessRisk(InvestorID);
}
//成交到来
void CRiskMsgCalc_Account::NewTrade(const std::string & BrokerID, const string& InvestorID, const string&  InstrumentID)
{
	NewPosition(BrokerID,InvestorID, true);
}
void CRiskMsgCalc_Account::NewOrder(const std::string & BrokerID, const string& InvestorID, const string&  InstrumentID )
{
	NewPosition(BrokerID,InvestorID);
}
//一个新的Timer到达时，用来判断持仓明细的持仓时间
void CRiskMsgCalc_Account::NewTimer_PosiDetailTime()
{
	char szTime[6];
	SYSTEMTIME st;
	GetLocalTime(&st);
	memset(szTime, 0, sizeof(szTime));
	sprintf(szTime,"%.2d:%.2d",st.wHour,st.wMinute);
	std::string strTime = szTime;
	std::string strTimeBegin;
	std::string strTimeEnd;

	std::map<string, std::map<PositionDetailKey, RiskEventLevelID>>::iterator itRisk;
	std::map<string, std::map<PositionDetailKey, RiskEventLevelID>>& mapRisk = 
			m_RiskProcess->GetEfficaciousRisk_MaxTimerPosiDetail_Keys();

	m_RiskProcess->m_mutex_MaxTimePosiDetailLevel.read_lock();

	// 处理风险消失的情况
	for(itRisk = mapRisk.begin(); itRisk != mapRisk.end(); itRisk++)
	{
		ActivePosiDetalMaxTimeRiskLose(itRisk->first, strTime, itRisk->second, strTimeBegin, strTimeEnd);
	}
	
	m_RiskProcess->m_mutex_MaxTimePosiDetailLevel.read_unlock();

#if 1
	
	std::vector<Organization> vecOrg;

	CInterface_SvrUserOrg::getObj().GetOrgs(vecOrg);
	for(int i=0; i<(int)vecOrg.size(); i++) 
	{
		SResponse retWarning;
		std::vector<std::string> vecAccount;
		std::map<int,RiskPlan>::iterator it;
		const time_t currTime = time(NULL);
		int nPrevValue = 0, nCurrValue = 0;
		BOOL bHasWarning = FALSE;

		std::vector<UserInfo> vecUser;

		CInterface_SvrUserOrg::getObj().GetUserInfosByOrgID(vecOrg[i].nAssetMgmtOrgID, vecUser);
		vecAccount.clear();
		for(int ii=0; ii<(int)vecUser.size(); ii++) {
			vecAccount.push_back(vecUser[ii].szAccount);
		}

		if(CDataCenter::Get()->GetPosiDetailWarning(vecOrg[i].nAssetMgmtOrgID, strTime, retWarning, strTimeBegin, strTimeEnd)) {
			
			if(retWarning.mapResponse.size()>0) {

				it = retWarning.mapResponse.begin();
				nPrevValue = currTime;
				
				while(it != retWarning.mapResponse.end()) {

					std::map<std::string, std::vector<PlatformStru_PositionDetail>> mapDetail;

					nCurrValue = (int)it->second.WaringLevel.fThresholdValue;
					if(CInterface_SvrTradeData::getObj().GetPositionDetail_MaxTime(vecAccount, 
							(int)(currTime-nPrevValue), (int)(currTime-nCurrValue), mapDetail)
							==CF_ERROR_SUCCESS && mapDetail.size()>0) {
						DoPosiDetailRisk(mapDetail, retWarning, it->first, currTime, strTimeBegin, strTimeEnd);
					}
					nPrevValue = nCurrValue;
					
					it++;

				} 
			
			}
		}
	}

#else

	std::map<std::string,std::vector<PlatformStru_PositionDetail>> outData;
	//获取所有的持仓明细
	if(CF_ERROR_SUCCESS != CInterface_SvrTradeData::getObj().GetAllPositionDetail(outData))
		return;
	
	std::map<std::string,std::vector<PlatformStru_PositionDetail>>::iterator it;
	for(it = outData.begin(); it != outData.end(); it++) 
	{
		for(int i=0; i < it->second.size(); i++) 
		{
			vector<SResponse> vecResponse;

			ActiveCalcPosiDetalMaxTimeRisk(it->first, it->second, vecResponse);

			//ProcessRisk(it->first, vecResponse);
		}
	}

#endif

}

void CRiskMsgCalc_Account::DoPosiDetailRisk(std::map<std::string, std::vector<PlatformStru_PositionDetail>>& mapDetail, 
			SResponse& warnings, int nLevel, time_t currTime, std::string& strTimeBegin, std::string& strTimeEnd)
{
	std::map<std::string, std::vector<PlatformStru_PositionDetail>>::iterator it;
	int nEventID = 0;
	double fVal = 0.0;
	std::map<int,RiskPlan>::iterator itVal = warnings.mapResponse.find(nLevel);
	if(itVal!=warnings.mapResponse.end()) {
		fVal = itVal->second.WaringLevel.fThresholdValue;
	}

	for(it = mapDetail.begin(); it != mapDetail.end(); it++)
	{
		//vector<SResponse> vecResponse;
		for(int i=0; i<(int)it->second.size(); i++) 
		{
			if(it->second[i].Volume==0)
				continue;

			SResponse retRsp = warnings;
			std::string strProduct;
			PlatformStru_InstrumentInfo info;	

			if(CDataCenter::Get()->GetInstrumentInfo_DataCenter(it->second[i].InstrumentID, info) != CF_ERROR_SUCCESS)
				continue;
			strProduct = info.ProductID;
			if(retRsp.setProducts.size()>0 && retRsp.setProducts.find(strProduct)==retRsp.setProducts.end())
				continue;	//设置了品种，但是没有找到；放弃判断风险

			PositionDetailKey key(it->second[i]);
			m_RiskProcess->AddMaxTime_PosiDetailRisk(it->first, strTimeBegin, strTimeEnd, key, nLevel, nEventID, fVal, (int)(currTime-it->second[i].OpenTimeUTC), retRsp);
			// 执行撤相关的报单后强行平仓
			
			//CDataCenter::Get()->AddResponse(it->first, RI_SingleHoldTime, it->second[i].InstrumentID, vecResponse);		
		}
	//	ProcessRisk(it->first);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
//
int CRiskMsgCalc_Account::GetMaxPositionRiskLevel(const std::string & BrokerID, string InvestorID, std::string strTime, RiskIndicatorType RiskType, std::string InstrumentID, double dblRisk, SResponse& vecResponse,int& nLevel, std::string& strTimeBegin, std::string& strTimeEnd)
{
	int nOrgID = 0;
	if(!CInterface_SvrUserOrg::getObj().GetOrgIDByAccount(InvestorID, nOrgID))
		return 0;

	RiskKey rKey;
	rKey.RiskType	= RiskType;
	rKey.nOrgID		= nOrgID;	
	while(1)
	{
		rKey.nOrgID = nOrgID;
		int nRet = CDataCenter::Get()->GetMaxPositionWarningMapLevel(rKey, strTime, InstrumentID, dblRisk, vecResponse, nLevel, strTimeBegin, strTimeEnd);
		if(nRet == 0)
		{
			Organization org;
			if(!CInterface_SvrUserOrg::getObj().GetOrgByID(nOrgID, org))
				return 0;//如果没有父级，则查找失败
			nOrgID = org.nUpperLevelOrgID;//从父级里面查找
		}
		else 		
			return nRet;		
	}
}

int CRiskMsgCalc_Account::GetRiskLevel(const std::string & BrokerID, string InvestorID,std::string strTime, RiskIndicatorType RiskType, double dblRisk, SResponse& vecResponse,int& nLevel, std::string& strTimeBegin, std::string& strTimeEnd)
{
	int nOrgID = 0;
	if(!CInterface_SvrUserOrg::getObj().GetOrgIDByAccount(InvestorID, nOrgID))
		return 0;
	
	RiskKey rKey;
	rKey.RiskType	= RiskType;
	rKey.nOrgID		= nOrgID;	
	while(1)
	{
		rKey.nOrgID = nOrgID;
		int nRet = CDataCenter::Get()->GetWarningMapLevel(rKey, strTime, dblRisk, vecResponse, nLevel, strTimeBegin, strTimeEnd);
		if(nRet == 0)
		{
			Organization org;
			if(!CInterface_SvrUserOrg::getObj().GetOrgByID(nOrgID, org))
				return 0;//如果没有父级，则查找失败
			nOrgID = org.nUpperLevelOrgID;//从父级里面查找
		}
		else 		
			return nRet;		
	}
}
int CRiskMsgCalc_Account::GetMaxPositionRisk_Level(const std::string & BrokerID, string InvestorID,std::string strTime, const std::string& InstrumentID, double&	dblRisk, int& nVolume, SResponse& vecThisRisk, int& nLevel, std::string& strTimeBegin, std::string& strTimeEnd)
{	
	std::vector<PlatformStru_Position> outData;
	if(CF_ERROR_SUCCESS == CInterface_SvrTradeData::getObj().GetAccountPositions(BrokerID, InvestorID, InstrumentID, outData))
	{
		for(int i =0; i<(int)outData.size(); i++)
		{
			PlatformStru_Position& pPosition = outData[i];
			nVolume = nVolume + pPosition.Position;
		}	
	}
	else
		nVolume = 0;
	
	dblRisk = nVolume;
	int nRet = GetMaxPositionRiskLevel(BrokerID,InvestorID, strTime, RI_MaxPosition, InstrumentID, dblRisk, vecThisRisk, nLevel, strTimeBegin, strTimeEnd);
	
	return nRet;	
}
//激活计算单合约最大持仓
void CRiskMsgCalc_Account::ActiveCalcMaxPositionRisk(const std::string & BrokerID, string InvestorID,std::string strTime, const std::string& InstrumentID)
{
	double	dblRisk = 0.0;
	int		nVolume = 0;
	SResponse vecThisRisk;
	int		nLevel = 0;
	std::string strTimeBegin, strTimeEnd;
	int nRet = GetMaxPositionRisk_Level(BrokerID,InvestorID, strTime, InstrumentID, dblRisk, nVolume, vecThisRisk, nLevel, strTimeBegin, strTimeEnd);

	int nEventID =-1;//风险事件ID
	if(nRet == -1 || nRet == 0)
	{//没有风险，则判断之前是不是有风险，有风险，则是风险消失
		if(m_RiskProcess->HasMaxPositionRisk(BrokerID,InvestorID, strTimeBegin, strTimeEnd,InstrumentID))
			m_RiskProcess->AddMaxPositionEvent(BrokerID,InvestorID, strTimeBegin, strTimeEnd,InstrumentID,dblRisk,nVolume,0, nEventID, vecThisRisk);	
		CDataCenter::Get()->AddAccountForbid(InvestorID, RI_MaxPosition, InstrumentID, NULL, vecThisRisk, false);	
	}
	else if(nRet == 1)
	{//有风险				
		m_RiskProcess->AddMaxPositionEvent(BrokerID,InvestorID, strTimeBegin, strTimeEnd,InstrumentID,dblRisk,nVolume,nLevel, nEventID, vecThisRisk);
		CDataCenter::Get()->AddAccountForbid(InvestorID, RI_MaxPosition, InstrumentID, NULL, vecThisRisk, true);	
	
	}		
}

int CRiskMsgCalc_Account::GetMarketShockRisk_Level(const std::string & BrokerID, const string& InvestorID,std::string strTime, const std::string& InstrumentID, double& dblRisk, int& nVolume,  double& total, SResponse& vecThisRisk, int& nLevel, std::string& strTimeBegin, std::string& strTimeEnd)
{
	PlatformStru_DepthMarketData outData;
	if(CF_ERROR_SUCCESS != CInterface_SvrTradeData::getObj().GetQuotInfo(InstrumentID, outData))
		return -1;
	total = outData.OpenInterest;//市场总持仓量
	if(total == 0)
	{
		RISK_LOGINFO("outData.OpenInterest;市场总持仓量为0,不触发风险 \n");
		return -1;
	}

	std::vector<PlatformStru_Position> outDataPosition;
	if(CF_ERROR_SUCCESS != CInterface_SvrTradeData::getObj().GetAccountPositions(BrokerID, InvestorID, InstrumentID, outDataPosition))
		return -1;

	for(int i =0; i< (int)outDataPosition.size(); i++)
	{
		PlatformStru_Position& pPosition = outDataPosition[i];
		nVolume = nVolume + pPosition.Position;
	}	

	dblRisk = nVolume*1.0/total;

	
	int nRet = GetMaxPositionRiskLevel(BrokerID,InvestorID, strTime, RI_MarketShock, InstrumentID, dblRisk, vecThisRisk, nLevel, strTimeBegin, strTimeEnd);
	

	return nRet;
}

void CRiskMsgCalc_Account::ActiveCalcMarketShockRisk(const std::string & BrokerID, const string& InvestorID, std::string strTime,const std::string& InstrumentID)
{
	double	dblRisk = 0.0;
	int		nVolume = 0;
	double total = 0;
	SResponse vecThisRisk;
	int nLevel = 0;
	std::string strTimeBegin, strTimeEnd;

	int nRet = GetMarketShockRisk_Level(BrokerID,InvestorID,strTime, InstrumentID, dblRisk, nVolume, total, vecThisRisk, nLevel, strTimeBegin, strTimeEnd);

	int nEventID =-1;//风险事件ID
	if(nRet == -1 || nRet == 0)
	{//没有风险，则判断之前是不是有风险，有风险，则是风险消失
		if(m_RiskProcess->HasMarketShockRisk(BrokerID,InvestorID, strTimeBegin, strTimeEnd,InstrumentID))
			m_RiskProcess->AddMarketShockEvent(BrokerID,InvestorID, strTimeBegin, strTimeEnd,InstrumentID,dblRisk,nVolume,0, nEventID, vecThisRisk);	
		CDataCenter::Get()->AddAccountForbid(InvestorID, RI_MarketShock, InstrumentID, NULL, vecThisRisk, false);		
	}
	else if(nRet == 1)
	{//有风险				
		m_RiskProcess->AddMarketShockEvent(BrokerID,InvestorID, strTimeBegin, strTimeEnd,InstrumentID,dblRisk,nVolume,nLevel, nEventID, vecThisRisk);	
		CDataCenter::Get()->AddAccountForbid(InvestorID, RI_MarketShock, InstrumentID, NULL, vecThisRisk, true);	
	}
	
}
int CRiskMsgCalc_Account::GetTodayLossRisk_Level(const std::string & BrokerID, const string& InvestorID,std::string strTime, double& dblRisk, SResponse& vecThisRisk, int& nLevel, std::string& strTimeBegin, std::string& strTimeEnd)
{//今日限亏	

	double  dbTodayInit   = 0.0;
	double  dbBlance = 0.0;

	PlatformStru_TradingAccountInfo outData;
	if(CF_ERROR_SUCCESS != CInterface_SvrTradeData::getObj().GetAccountFundInfo(BrokerID, InvestorID, outData))
		return -1;

	dbTodayInit	 = outData.StaticProfit - outData.Deposit + outData.Withdraw;  //今日初始总权益
	dbBlance	 = outData.DynamicProfit; //动态权益

	if(dbTodayInit>-0.00001 && dbTodayInit < 0.00001)
		return -1;//初始总权益为0，则不判断这个风险指标

	dblRisk = (dbBlance - outData.StaticProfit)/dbTodayInit;

	if(dblRisk > 0)
		return -1;//赚钱的没有风险
	else
	{
		dblRisk = -dblRisk;
	}
	
	int nRet = GetRiskLevel(BrokerID,InvestorID,strTime, RI_TodayLoss, dblRisk, vecThisRisk, nLevel, strTimeBegin, strTimeEnd);
	
	return nRet;//有风险
}
void CRiskMsgCalc_Account::ActiveCalcTodayLossRisk(const std::string & BrokerID, const string& InvestorID,std::string strTime, bool bDoResponse)
{//今日限亏
	double	dblRisk  = 0.0;
	SResponse vecThisRisk;
	int nLevel = 0;
	std::string strTimeBegin, strTimeEnd;
	int nRet = GetTodayLossRisk_Level(BrokerID,InvestorID,strTime, dblRisk, vecThisRisk, nLevel, strTimeBegin, strTimeEnd);

	int nEventID =-1;//风险事件ID
	if(nRet == -1 || nRet == 0)
	{//没有风险，则判断之前是不是有风险，有风险，则是风险消失
		if(m_RiskProcess->HasTodayLossRisk(BrokerID,InvestorID, strTimeBegin, strTimeEnd))
			m_RiskProcess->AddTodayLossEvent(BrokerID,InvestorID, strTimeBegin, strTimeEnd,dblRisk,0, nEventID, vecThisRisk, bDoResponse);
		CDataCenter::Get()->AddAccountForbid(InvestorID, RI_TodayLoss, "", NULL, vecThisRisk, false);
	}
	else if(nRet == 1)
	{//有风险				
		m_RiskProcess->AddTodayLossEvent(BrokerID,InvestorID, strTimeBegin, strTimeEnd,dblRisk,nLevel, nEventID, vecThisRisk, bDoResponse);	
		CDataCenter::Get()->AddAccountForbid(InvestorID, RI_TodayLoss, "", NULL, vecThisRisk, true);	

	}
	
}

int CRiskMsgCalc_Account::GetMarginRatioRisk_Level(const std::string & BrokerID, const string& InvestorID, std::string strTime,double& dblRisk, SResponse& vecThisRisk, int& nLevel, std::string& strTimeBegin, std::string& strTimeEnd)
{//保证金比例风险
	//OutPut output("ActiveCalcMarginRatioRisk");
	double  dbBlance = 0.0;
	double dblCurrMargin = 0.0;

	PlatformStru_TradingAccountInfo outData;
	if(CF_ERROR_SUCCESS != CInterface_SvrTradeData::getObj().GetAccountFundInfo(BrokerID, InvestorID, outData))
		return -1;
	dbBlance	  = outData.DynamicProfit; //动态权益
	dblCurrMargin = outData.CurrMargin;//当前保证金
	dblRisk=dblCurrMargin/dbBlance;

	
	int nRet = GetRiskLevel(BrokerID,InvestorID,strTime, RI_MarginRatio, dblRisk, vecThisRisk, nLevel, strTimeBegin, strTimeEnd);
	return nRet;
}
void CRiskMsgCalc_Account::ActiveCalcMarginRatioRisk(const std::string & BrokerID, const string& InvestorID, std::string strTime)
{//保证金比例风险
	//OutPut output("ActiveCalcMarginRatioRisk");
	double	dblRisk  = 0.0;
	SResponse vecThisRisk;
	int nLevel = 0;
	std::string strTimeBegin, strTimeEnd;
	int nRet = GetMarginRatioRisk_Level(BrokerID,InvestorID,strTime, dblRisk, vecThisRisk, nLevel, strTimeBegin, strTimeEnd);

	int nEventID =-1;//风险事件ID
	if(nRet == -1 || nRet == 0)
	{//没有风险，则判断之前是不是有风险，有风险，则是风险消失
		if(m_RiskProcess->HasMarginRatioRisk(BrokerID,InvestorID, strTimeBegin, strTimeEnd))
			m_RiskProcess->AddMarginRatioEvent(BrokerID,InvestorID, strTimeBegin, strTimeEnd,dblRisk,0, nEventID, vecThisRisk);		
		CDataCenter::Get()->AddAccountForbid(InvestorID, RI_MarginRatio, "", NULL, vecThisRisk, false);
	}
	else if(nRet == 1)
	{//有风险				
		m_RiskProcess->AddMarginRatioEvent(BrokerID, InvestorID, strTimeBegin, strTimeEnd,dblRisk,nLevel, nEventID, vecThisRisk);	
		CDataCenter::Get()->AddAccountForbid(InvestorID, RI_MarginRatio, "", NULL, vecThisRisk, true);
	}
	
}

/*
//持仓明细的持仓时间风险
void CRiskMsgCalc_Account::ActiveCalcPosiDetalMaxTimeRisk(const string& InvestorID, std::string strTime, 
												  std::vector<PlatformStru_PositionDetail>& vecPosiDetails, 
												  vector<SResponse>& vecResponse, std::string& strTimeBegin, std::string& strTimeEnd)
{	
	int nEventID =-1;

	int nRet = 0, nLevel = 0;
	double fVal = 0.0;

	for(int i=0; i < (int)vecPosiDetails.size(); i++) 
	{
		PlatformStru_PositionDetail& posiDetail = vecPosiDetails[i];
		if(posiDetail.Volume==0)
			continue;

		int nTimeSpan = CalcRemainTime(string(posiDetail.InstrumentID), string(posiDetail.OpenTime));

		SResponse retRsp;
		nRet = CalcOverTime_DealerPosiDetail(posiDetail.InvestorID, 
						posiDetail.InstrumentID, retRsp, nTimeSpan, nLevel);

		if(nRet == 0)				//没有设置风控方案
		  continue;
		PositionDetailKey key(posiDetail);
		if(nRet == 1) {				//有风险
			fVal = 0.0;
			std::map<int,RiskPlan>::iterator itVal = retRsp.mapResponse.find(nLevel);
			if(itVal != retRsp.mapResponse.end()) {
				fVal = itVal->second.WaringLevel.fThresholdValue;
			}
			//vecResponse.push_back(retRsp);
			m_RiskProcess->AddMaxTime_PosiDetailRisk(InvestorID, key, nLevel, nEventID, fVal, 0.0, retRsp);
			// 执行撤相关的报单后强行平仓
		}
		else if(nRet == -1)
		{//没有风险，则判断之前是不是有风险，有风险，则是风险消失
			if(m_RiskProcess->HasMaxTime_PosiDetailRisk(InvestorID, key))
				m_RiskProcess->AddMaxTime_PosiDetailRisk(InvestorID, key, 0, nEventID, 0.0, 0.0, retRsp);
		}
	}
}*/
void CRiskMsgCalc_Account::ActivePosiDetalMaxTimeRiskLose(const string& InvestorID, std::string strTime,  
					  std::map<PositionDetailKey, RiskEventLevelID>& mapDetailKey, std::string& strTimeBegin, std::string& strTimeEnd)
{
	std::map<PositionDetailKey, RiskEventLevelID>::iterator itkey;
	std::set<PositionDetailKey>::iterator itOff;
	std::set<PositionDetailKey> setOff;
	PlatformStru_PositionDetail posiDetail;

	for(itkey = mapDetailKey.begin(); itkey != mapDetailKey.end(); )
	{
		SResponse retWarning;
		std::map<int,RiskPlan>::iterator it;
		int nOrgID = 0;
		bool bRiskLose = true;
		if(CInterface_SvrUserOrg::getObj().GetOrgIDByAccount(InvestorID, nOrgID)) {
			if(CDataCenter::Get()->GetPosiDetailWarning(nOrgID, strTime, retWarning, strTimeBegin, strTimeEnd)) {
				for(it = retWarning.mapResponse.begin(); it != retWarning.mapResponse.end(); it++) {
					if(it->second.WaringLevel.nRiskIndicatorID==RI_SingleHoldTime) {
						bRiskLose = false;
						break;
					}
				}
			}
		}
		if(!bRiskLose && CInterface_SvrTradeData::getObj().GetUserPositionDetail(
				InvestorID, itkey->first, posiDetail)!=CF_ERROR_SUCCESS || posiDetail.Volume==0) {
			bRiskLose = true;
		}
		if(bRiskLose) {
			m_RiskProcess->AddMaxTime_PosiDetailRisk(InvestorID, strTimeBegin, strTimeEnd, itkey->first, 0, itkey->second.EventID, 0.0, 0.0, retWarning);
			itkey = mapDetailKey.erase(itkey);
		}
		else 
		{
			itkey++;
		}
	}
}

int CRiskMsgCalc_Account::GetFundNetValueRisk_Level(const std::string & BrokerID, const string& InvestorID, std::string strTime,double& dblRisk, SResponse& vecThisRisk, int& nLevel, std::string& strTimeBegin, std::string& strTimeEnd)
{//基金净值风险 
	//OutPut output("ActiveCalcFundNetValueRisk");	
	NetFundParam netFundParam;
	bool bFind = CalcGetFundParam(BrokerID, InvestorID, netFundParam);	
	if(!bFind)
		return -1;//基金净值配置参数取不到，无法进行风险判断
	//////////////////////////////////////////////////////////////////////////	
	double dblBalance= 0.0;
	PlatformStru_TradingAccountInfo outData;
	if(CF_ERROR_SUCCESS != CInterface_SvrTradeData::getObj().GetAccountFundInfo(BrokerID, InvestorID, outData))
		return -1;
	dblBalance = outData.DynamicProfit;//动态权益

	
	if(netFundParam.dOuterVolumn + netFundParam.dInnerVolumn <= 0)
		return -1;	
	dblRisk = (netFundParam.dOuterNetAsset + dblBalance)/(netFundParam.dOuterVolumn + netFundParam.dInnerVolumn);	
	//////////////////////////////////////////////////////////////////////////	
	int nRet = GetRiskLevel(BrokerID, InvestorID, strTime, RI_FundNetValue, dblRisk, vecThisRisk, nLevel, strTimeBegin, strTimeEnd);

	return nRet;
}
void CRiskMsgCalc_Account::ActiveCalcFundNetValueRisk(const std::string & BrokerID,const string& InvestorID,std::string strTime, bool bDoResponse)
{//基金净值风险 
	double dblRisk = 0.0;
	SResponse vecThisRisk;
	int nLevel = 0;
	std::string strTimeBegin, strTimeEnd;
	int nRet = GetFundNetValueRisk_Level(BrokerID, InvestorID,strTime, dblRisk, vecThisRisk, nLevel, strTimeBegin, strTimeEnd);
	

	int nEventID =-1;//风险事件ID
	if(nRet == -1 || nRet == 0)
	{//没有风险，则判断之前是不是有风险，有风险，则是风险消失
		if(m_RiskProcess->HasFundNetValueRisk(BrokerID,InvestorID, strTimeBegin, strTimeEnd))
			m_RiskProcess->AddFundNetValueEvent(BrokerID, InvestorID, strTimeBegin, strTimeEnd,dblRisk,0, nEventID, vecThisRisk, bDoResponse);		
		CDataCenter::Get()->AddAccountForbid(InvestorID, RI_FundNetValue, "", NULL, vecThisRisk, false);	
	}
	else if(nRet == 1)
	{//有风险				
		m_RiskProcess->AddFundNetValueEvent(BrokerID, InvestorID, strTimeBegin, strTimeEnd,dblRisk,nLevel, nEventID, vecThisRisk, bDoResponse);	
		CDataCenter::Get()->AddAccountForbid(InvestorID, RI_FundNetValue, "", NULL, vecThisRisk, true);
	}
	
}
bool CRiskMsgCalc_Account::CalcGetFundParam(const std::string & BrokerID, const string& InvestorID, NetFundParam& netFundParam)
{	
	std::string strTime = "";

	int nOrgID = 0;
	if(!CInterface_SvrUserOrg::getObj().GetOrgIDByAccount(InvestorID, nOrgID))
		return false;

	RiskKey rKey;
	rKey.RiskType	= RI_FundNetValue;
	rKey.nOrgID		= nOrgID;	
	while(1)
	{
		rKey.nOrgID = nOrgID;		
		bool bRet = CDataCenter::Get()->GetFundParam(rKey, strTime, netFundParam);
		if(bRet == false)
		{
			Organization org;
			if(!CInterface_SvrUserOrg::getObj().GetOrgByID(nOrgID, org))
				return 0;//如果没有父级，则查找失败
			nOrgID = org.nUpperLevelOrgID;//从父级里面查找
		}
		else 		
			return bRet;		
	}
	return false;	
}

int CRiskMsgCalc_Account::CalcOverTime_DealerPosiDetail(const string& InvestorID, const string& strProdID, 
												SResponse& retRsp, int nOpenTime, int& nLevel)
{
	//OutPut output("CalcOverTime_DealerPosiDetail");	
	int nOrgID = 0;
	if(!CInterface_SvrUserOrg::getObj().GetOrgIDByAccount(InvestorID, nOrgID))
		return 0;
	
	RiskKey rKey;
	rKey.RiskType	= RI_SingleHoldTime;
	rKey.nOrgID		= nOrgID;	
	while(1) {
		rKey.nOrgID = nOrgID;
		int nRet = CDataCenter::Get()->GetPosiDetailLevel(rKey, (double)nOpenTime, 
														  strProdID, retRsp, nLevel);
		if(nRet == 0) {
			Organization org;
			if(!CInterface_SvrUserOrg::getObj().GetOrgByID(nOrgID, org))
				return 0;//如果没有父级，则查找失败
			nOrgID = org.nUpperLevelOrgID;//从父级里面查找
		}
		else 		
			return nRet;		
	}
	return 0;
}

int CRiskMsgCalc_Account::CalcRemainTime(std::string& strInstID, std::string& strOpenTime)
{
	int nOpenTime = GetRemainTime(strOpenTime);

	PlatformStru_InstrumentInfo info;	
	if(CDataCenter::Get()->GetInstrumentInfo_DataCenter(strInstID, info) != CF_ERROR_SUCCESS)
		return nOpenTime;
	else
		return CalcRealHoldingTime(std::string(info.ExchangeID), nOpenTime);
}

int CRiskMsgCalc_Account::CalcRealHoldingTime(std::string& strExchangeID, int nTime)
{
	std::string strTextID(strExchangeID);
	std::string strExchID = _strlwr((char*)(strTextID.c_str()));
	map<string, vector<EXCHANGERESTTIME>> mapRestTime = CDataCenter::Get()->GetExchangeRestTime();
	map<string, vector<EXCHANGERESTTIME>>::iterator it = mapRestTime.find(strExchID);
	if(it != mapRestTime.end()) {
		int nSubTime = 0;
		for(int i=0; i < (int)it->second.size(); i++) {
			if(nTime < (int)it->second[i].nStart)
				break;
			if(nTime < (int)it->second[i].nEnd) {
				int nSub = nTime - it->second[i].nStart;
				nSubTime += nSub;
				break;
			}
			nSubTime += it->second[i].nSubNum;
		}
		nTime -= nSubTime;
	}
	return nTime;
}

int CRiskMsgCalc_Account::GetRemainTime(std::string& strOpenTime)
{
	int iHour=0, iMin=0, iSec=0;
	sscanf_s(strOpenTime.c_str(), "%02d:%02d:%02d", &iHour, &iMin, &iSec);
	int nOpenTime = iHour*3600+iMin*60+iSec;
	SYSTEMTIME currTime;
	::GetLocalTime(&currTime);
	int nCurrTime = currTime.wHour*3600+currTime.wMinute*60+currTime.wSecond;
	return nCurrTime-nOpenTime+1;
}
