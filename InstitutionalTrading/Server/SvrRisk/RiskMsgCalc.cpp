#include "stdafx.h"
#include "DataCenter.h"
#include "RiskMsgCalc.h"
#include "RiskProcess.h"
#include "ThreadSharedData.h"
extern bool g_bSendToExcute;//通过网络发送给交易执行命令
extern  int g_hSocket;      //主服务器套接字
CRiskMsgCalc::CRiskMsgCalc(CThreadSharedData* pThreadSharedData)
	:m_pThreadShareData(pThreadSharedData)
{	
	m_RiskProcess = new CRiskProcess(pThreadSharedData);
}

CRiskMsgCalc::~CRiskMsgCalc(void)
{
	SAFE_DELETE(m_RiskProcess);
}
void CRiskMsgCalc::ChangeRiskPlan()
{//风控方案改变，则需要算所有账号的所有风险指标
	std::vector<UserInfo> vec;
	CInterface_SvrUserOrg::getObj().GetUserInfos(vec);//得到所有交易员
	for(int i=0; i<(int)vec.size(); i++)
	{
		UserInfo& userInfo = vec[i];
		if(USER_TYPE_TRADER != userInfo.nUserType)
			continue;
		if(userInfo.nStatus == 0)
			continue;//禁用状态账户不做风险分析

		NewPosition(userInfo.szAccount);		
	}	
}
//交易数据管理数据准备好，持仓判断
void CRiskMsgCalc::TradeDataInit()
{
	std::vector<UserInfo> vec;
	CInterface_SvrUserOrg::getObj().GetUserInfos(vec);//得到所有交易员
	for(int i=0; i<(int)vec.size(); i++)
	{
		UserInfo& userInfo = vec[i];
		if(USER_TYPE_TRADER != userInfo.nUserType)
			continue;
		if(userInfo.nStatus == 0)
			continue;//禁用状态账户不做风险分析
		NewPosition(userInfo.szAccount);	
	}	
}/*
//行情到来
void CRiskMsgCalc::NewDepthMarketData(const  std::string& InstrumentID )
{
	
}*/
//资金到来
void CRiskMsgCalc::NewFundAccount(const string& InvestorID )
{//ProcessForbid 必须要知道该账户所有的限制交易和限制开仓的状态，不然没法对账户准确设置
	NewPosition(InvestorID);
}
//持仓到来
void CRiskMsgCalc::NewPosition(const string& InvestorID, bool bDoResponse)
{//今日限亏、账户限亏和基金净值风险如果设置了市价强平的话，只要风险还在，有持仓就应该强平 ,bDoResponse的意义就在这里

	char szTime[6];
	SYSTEMTIME st;
	GetLocalTime(&st);
	memset(szTime, 0, sizeof(szTime));
	sprintf(szTime,"%.2d:%.2d",st.wHour,st.wMinute);
	std::string strTime = szTime;

	ActiveCalcAccountLossRisk(InvestorID, strTime, bDoResponse);
	ActiveCalcTodayLossRisk(InvestorID, strTime, bDoResponse);
	ActiveCalcGappedMarketRisk(InvestorID, strTime);
	ActiveCalcMarginRatioRisk(InvestorID, strTime);
	ActiveCalcFundNetValueRisk(InvestorID, strTime, bDoResponse);

	//保证金限制
	ActiveCalcMarginForbid(InvestorID, strTime);
	ActiveCalcLossForbid(InvestorID, strTime);
	ActiveCalcOnedayLargetsLossForbid(InvestorID, strTime);
	ActiveCalcLossMax(InvestorID, strTime);
	ActiveCalcContractsValuesForbid(InvestorID, strTime);
	ActiveBullBearValuesForbid(InvestorID, strTime);
	ActiveLossContinueDays(InvestorID, strTime);
	ActiveLossPercent(InvestorID, strTime);

	std::set<string> setMaxPositionID;
	std::set<string> setMarketShockID;
	if(m_RiskProcess)
	{
		m_RiskProcess->GetMaxPositionRisk("",InvestorID, setMaxPositionID);
		m_RiskProcess->GetMarketShockRisk("",InvestorID, setMarketShockID);
	}

	vector<PlatformStru_Position> vecPosition;
	if(CF_ERROR_SUCCESS == CInterface_SvrTradeData::getObj().GetUserPositions(InvestorID, "", vecPosition))
	{		
		for(int i =0; i < (int)vecPosition.size(); i++)
		{
			PlatformStru_Position PosField = vecPosition[i];
			std::string InstrumentID = PosField.InstrumentID;
			ActiveCalcMaxPositionRisk(InvestorID, strTime, InstrumentID);
			ActiveCalcMarketShockRisk(InvestorID, strTime, InstrumentID);
			setMaxPositionID.erase(InstrumentID);
			setMarketShockID.erase(InstrumentID);

			//交易合约限制
			ActiveCalcTradeForbid(InvestorID, strTime, InstrumentID);
		}
	}
	std::set<string>::iterator itMaxPosition = setMaxPositionID.begin();
	for(; itMaxPosition != setMaxPositionID.end(); itMaxPosition++)
	{//持仓已经没有，但是风险事件还在的情况
		std::string strInstrument = *itMaxPosition;
		ActiveCalcMaxPositionRisk(InvestorID, strTime, strInstrument);
	}
	std::set<string>::iterator itMarketShock = setMarketShockID.begin();
	for(; itMarketShock != setMarketShockID.end(); itMarketShock++)
	{//持仓已经没有，但是风险事件还在的情况
		std::string strInstrument = *itMarketShock;
		ActiveCalcMarketShockRisk(InvestorID, strTime, strInstrument);
	}

	ActiveCalcMaxPosZero(InvestorID, strTime);//对设置为0的最大持仓方案需要特殊处理
	ProcessForbid(InvestorID);
	ProcessRisk(InvestorID);
}
//成交到来
void CRiskMsgCalc::NewTrade(const string& InvestorID, const string&  InstrumentID)
{
	NewPosition(InvestorID, true);
}
void CRiskMsgCalc::NewOrder(const string& InvestorID, const string&  InstrumentID )
{
	NewPosition(InvestorID);
}
//一个新的Timer到达时，用来判断持仓明细的持仓时间
void CRiskMsgCalc::NewTimer_PosiDetailTime()
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
							==CF_ERROR_SUCCESS && mapDetail.size()>0)
					{
						SResponse ret;
						ret.nRiskEventID = retWarning.nRiskEventID;
						ret.setProducts  = retWarning.setProducts;
						ret.mapResponse[it->first] = it->second;
						strTimeBegin = it->second.OrgIDPlanRelation.cTimeBegin;
						strTimeEnd   = it->second.OrgIDPlanRelation.cTimeEnd;

						DoPosiDetailRisk(mapDetail, ret, it->first, currTime, strTimeBegin, strTimeEnd);
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

void CRiskMsgCalc::DoPosiDetailRisk(std::map<std::string, std::vector<PlatformStru_PositionDetail>>& mapDetail, 
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
		ProcessRisk(it->first);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
//
int CRiskMsgCalc::GetMaxPositionRiskLevel(string InvestorID, std::string strTime, RiskIndicatorType RiskType, std::string InstrumentID, double dblRisk, SResponse& vecResponse,int& nLevel, std::string& strTimeBegin, std::string& strTimeEnd)
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

int CRiskMsgCalc::GetRiskLevel(string InvestorID, std::string strTime, RiskIndicatorType RiskType, double dblRisk, SResponse& vecResponse,int& nLevel, std::string& strTimeBegin, std::string& strTimeEnd)
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
int CRiskMsgCalc::GetMaxPositionRisk_Level( string InvestorID, std::string strTime, const std::string& InstrumentID, double&	dblRisk, int& nVolume, SResponse& vecThisRisk, int& nLevel, std::string& strTimeBegin, std::string& strTimeEnd)
{	
	std::vector<PlatformStru_Position> outData;
	if(CF_ERROR_SUCCESS == CInterface_SvrTradeData::getObj().GetUserPositions(InvestorID, InstrumentID, outData))
	{
		for(int i =0; i<(int)outData.size(); i++)
		{
			PlatformStru_Position& pPosition = outData[i];
			nVolume = nVolume + pPosition.Position;
		}	
	}
	else
		nVolume = 0;//

	
	dblRisk = nVolume;
	int nRet = GetMaxPositionRiskLevel(InvestorID, strTime, RI_MaxPosition, InstrumentID, dblRisk, vecThisRisk, nLevel, strTimeBegin, strTimeEnd);
	
	return nRet;	
}
//激活计算单合约最大持仓
void CRiskMsgCalc::ActiveCalcMaxPositionRisk( string InvestorID, std::string strTime, const std::string& InstrumentID)
{
	double	dblRisk = 0.0;
	int		nVolume = 0;
	SResponse vecThisRisk;
	int		nLevel = 0;
	std::string strTimeBegin, strTimeEnd;

	int nRet = GetMaxPositionRisk_Level(InvestorID, strTime, InstrumentID, dblRisk, nVolume, vecThisRisk, nLevel, strTimeBegin, strTimeEnd);

	int nEventID =-1;//风险事件ID
	if(nRet == -1 || nRet == 0)
	{//没有风险，则判断之前是不是有风险，有风险，则是风险消失
		if(m_RiskProcess->HasMaxPositionRisk("",InvestorID, strTimeBegin, strTimeEnd,InstrumentID))
			m_RiskProcess->AddMaxPositionEvent("",InvestorID, strTimeBegin, strTimeEnd,InstrumentID,dblRisk,nVolume,0, nEventID, vecThisRisk);	
		CDataCenter::Get()->AddAccountForbid(InvestorID, RI_MaxPosition, InstrumentID, NULL, vecThisRisk, false);	
	}
	else if(nRet == 1)
	{//有风险				
		m_RiskProcess->AddMaxPositionEvent("",InvestorID, strTimeBegin, strTimeEnd,InstrumentID,dblRisk,nVolume,nLevel, nEventID, vecThisRisk);
		CDataCenter::Get()->AddAccountForbid(InvestorID, RI_MaxPosition, InstrumentID, NULL, vecThisRisk, true);	
	}		
}

int CRiskMsgCalc::GetMarketShockRisk_Level(const string& InvestorID, std::string strTime, const std::string& InstrumentID, double& dblRisk, int& nVolume,  double& total, SResponse& vecThisRisk, int& nLevel, std::string& strTimeBegin, std::string& strTimeEnd)
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
	if(CF_ERROR_SUCCESS == CInterface_SvrTradeData::getObj().GetUserPositions(InvestorID, InstrumentID, outDataPosition))
	{
		for(int i =0; i< (int)outDataPosition.size(); i++)
		{
			PlatformStru_Position& pPosition = outDataPosition[i];
			nVolume = nVolume + pPosition.Position;
		}	
	}
	else
		nVolume = 0;	

	dblRisk = nVolume*1.0/total;
	
	int nRet = GetMaxPositionRiskLevel(InvestorID, strTime, RI_MarketShock, InstrumentID, dblRisk, vecThisRisk, nLevel, strTimeBegin, strTimeEnd);
	
	return nRet;
}
void CRiskMsgCalc::ActiveCalcMaxPosZero( string InvestorID, std::string strTime)
{
	std::string strTimeBegin;
	std::string strTimeEnd;
	std::set<std::string> setAllProducts;
	SResponse vecResponse;
	if(HasZeroRiskPlan(InvestorID, strTime, vecResponse, strTimeBegin, strTimeEnd))
	{//判断是不是设置了0风险值的最大风险风控方案
		std::map<int,RiskPlan>::const_iterator coreit;
		for(coreit = vecResponse.mapResponse.begin();coreit != vecResponse.mapResponse.end();++coreit) 
		{
			const RiskPlan& risk=coreit->second;
			if(risk.WaringLevel.fThresholdValue > -0.00001 && risk.WaringLevel.fThresholdValue < 0.00001) 
			{//有0值的最大风险事件方案设置
				
				if(vecResponse.setProducts.size() >0)
					setAllProducts = vecResponse.setProducts;
				break;
			}
		}
		
		for(std::set<std::string>::iterator it  = setAllProducts.begin(); it!= setAllProducts.end(); it++)
		{
			std::set<std::string> outData;
			std::string strProduct = *it;
			CDataCenter::Get()->GetInstrumentInfos_ByProduct(strProduct, outData);
			for(std::set<std::string>::iterator itInstrument  = outData.begin(); itInstrument!= outData.end(); itInstrument++)
				ActiveCalcMaxPositionRisk( InvestorID, strTime,  *itInstrument);
		}
		
	}	
}
bool CRiskMsgCalc::HasZeroRiskPlan(string InvestorID, std::string strTime, SResponse& vecResponse, std::string& strTimeBegin, std::string& strTimeEnd)
{
	int nOrgID = 0;
	if(!CInterface_SvrUserOrg::getObj().GetOrgIDByAccount(InvestorID, nOrgID))
		return 0;

	RiskKey rKey;
	rKey.RiskType	= RI_MaxPosition;
	rKey.nOrgID		= nOrgID;	
	while(1)
	{
		rKey.nOrgID = nOrgID;
		int nRet = CDataCenter::Get()->GetAllMaxPositonZeroWarningMapLevel(rKey, strTime, vecResponse, strTimeBegin, strTimeEnd);
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
void CRiskMsgCalc::ActiveCalcMarketShockRisk(const string& InvestorID, std::string strTime, const std::string& InstrumentID)
{
	double	dblRisk = 0.0;
	int		nVolume = 0;
	double total = 0;
	SResponse vecThisRisk;
	int nLevel = 0;
	std::string strTimeBegin, strTimeEnd;

	int nRet = GetMarketShockRisk_Level(InvestorID, strTime, InstrumentID, dblRisk, nVolume, total, vecThisRisk, nLevel, strTimeBegin, strTimeEnd);

	int nEventID =-1;//风险事件ID
	if(nRet == -1 || nRet == 0)
	{//没有风险，则判断之前是不是有风险，有风险，则是风险消失
		if(m_RiskProcess->HasMarketShockRisk("",InvestorID, strTimeBegin, strTimeEnd,InstrumentID))
			m_RiskProcess->AddMarketShockEvent("",InvestorID, strTimeBegin, strTimeEnd,InstrumentID,dblRisk,nVolume,0, nEventID, vecThisRisk);	
		CDataCenter::Get()->AddAccountForbid(InvestorID, RI_MarketShock, InstrumentID, NULL, vecThisRisk, false);		
	}
	else if(nRet == 1)
	{//有风险				
		m_RiskProcess->AddMarketShockEvent("",InvestorID, strTimeBegin, strTimeEnd,InstrumentID,dblRisk,nVolume,nLevel, nEventID, vecThisRisk);	
		CDataCenter::Get()->AddAccountForbid(InvestorID, RI_MarketShock, InstrumentID, NULL, vecThisRisk, true);	
	}
	
}
int CRiskMsgCalc::GetAccountLossRisk_Level(const string& InvestorID, std::string strTime, double& dblRisk, SResponse& vecThisRisk, int& nLevel, std::string& strTimeBegin, std::string& strTimeEnd)
{//账户限亏
	//OutPut output("ActiveCalcAccountLossRisk");
	double  dbInit   = 0.0;
	double  dbBlance = 0.0;

	if(CF_ERROR_SUCCESS != CInterface_SvrTradeData::getObj().GetUserInitFund(InvestorID, dbInit))
		return -1;//初始化资金

	PlatformStru_TradingAccountInfo outData;
	if(CF_ERROR_SUCCESS != CInterface_SvrTradeData::getObj().GetUserFundInfo(InvestorID, outData))
		return -1;
	dbBlance = outData.DynamicProfit;//动态权益

	if(dbInit>-0.00001 && dbInit < 0.00001)
		return -1;//初始总权益为0，则不判断这个风险指标

	dblRisk = (dbBlance - dbInit)/dbInit;
	if(dblRisk > 0)
		return -1;//赚钱的没有风险
	else
	{
		dblRisk = -dblRisk;
	}
	int nRet = GetRiskLevel(InvestorID, strTime, RI_AccountLoss, dblRisk, vecThisRisk, nLevel, strTimeBegin, strTimeEnd);
	return nRet;
}
void CRiskMsgCalc::ActiveCalcAccountLossRisk(const string& InvestorID, std::string strTime, bool bDoResponse)
{//账户限亏
	double	dblRisk  = 0.0;
	SResponse vecThisRisk;
	int nLevel = 0;
	std::string strTimeBegin, strTimeEnd;

	int nRet = GetAccountLossRisk_Level(InvestorID, strTime, dblRisk, vecThisRisk, nLevel, strTimeBegin, strTimeEnd);

	int nEventID =-1;//风险事件ID
	if(nRet == -1 || nRet == 0)
	{//没有风险，则判断之前是不是有风险，有风险，则是风险消失
		if(m_RiskProcess->HasAccountLossRisk("",InvestorID, strTimeBegin, strTimeEnd))
			m_RiskProcess->AddAccountLossEvent("",InvestorID, strTimeBegin, strTimeEnd,dblRisk,0, nEventID, vecThisRisk, bDoResponse);	
		CDataCenter::Get()->AddAccountForbid(InvestorID, RI_AccountLoss, "", NULL, vecThisRisk, false);
	}
	else if(nRet == 1)
	{//有风险				
		m_RiskProcess->AddAccountLossEvent("",InvestorID, strTimeBegin, strTimeEnd,dblRisk,nLevel, nEventID,  vecThisRisk, bDoResponse);	
		CDataCenter::Get()->AddAccountForbid(InvestorID, RI_AccountLoss, "", NULL, vecThisRisk, true);
	}
	
}
int CRiskMsgCalc::GetTodayLossRisk_Level(const string& InvestorID, std::string strTime, double& dblRisk, SResponse& vecThisRisk, int& nLevel, std::string& strTimeBegin, std::string& strTimeEnd)
{//今日限亏	

	double  dbTodayInit   = 0.0;
	double  dbBlance = 0.0;

	PlatformStru_TradingAccountInfo outData;
	if(CF_ERROR_SUCCESS != CInterface_SvrTradeData::getObj().GetUserFundInfo(InvestorID, outData))
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
	
	int nRet = GetRiskLevel(InvestorID, strTime, RI_TodayLoss, dblRisk, vecThisRisk, nLevel, strTimeBegin, strTimeEnd);
	
	return nRet;//有风险
}
void CRiskMsgCalc::ActiveCalcTodayLossRisk(const string& InvestorID, std::string strTime, bool bDoResponse)
{//今日限亏
	double	dblRisk  = 0.0;
	SResponse vecThisRisk;
	int nLevel = 0;
	std::string strTimeBegin, strTimeEnd;

	int nRet = GetTodayLossRisk_Level(InvestorID, strTime, dblRisk, vecThisRisk, nLevel, strTimeBegin, strTimeEnd);

	int nEventID =-1;//风险事件ID
	if(nRet == -1 || nRet == 0)
	{//没有风险，则判断之前是不是有风险，有风险，则是风险消失
		if(m_RiskProcess->HasTodayLossRisk("",InvestorID, strTimeBegin, strTimeEnd))
			m_RiskProcess->AddTodayLossEvent("",InvestorID, strTimeBegin, strTimeEnd,dblRisk,0, nEventID, vecThisRisk, bDoResponse);
		CDataCenter::Get()->AddAccountForbid(InvestorID, RI_TodayLoss, "", NULL, vecThisRisk, false);
	}
	else if(nRet == 1)
	{//有风险				
		m_RiskProcess->AddTodayLossEvent("",InvestorID, strTimeBegin, strTimeEnd,dblRisk,nLevel, nEventID, vecThisRisk, bDoResponse);	
		CDataCenter::Get()->AddAccountForbid(InvestorID, RI_TodayLoss, "", NULL, vecThisRisk, true);	

	}
	
}
int CRiskMsgCalc::GetGappedMarketRisk_Level(const string& InvestorID, std::string strTime, double& dblRisk, SResponse& vecThisRisk, int& nLevel, std::string& strTimeBegin, std::string& strTimeEnd)
{//隔夜跳空
	vector<PlatformStru_Position> vecPosition;
	if(CF_ERROR_SUCCESS != CInterface_SvrTradeData::getObj().GetUserPositions(InvestorID, "", vecPosition))
		return -1;
	if(vecPosition.size() == 0)//没有持仓自然没有风险
		return -1;

	double  dbInit   = 0.0;
	double  dbBlance = 0.0;

	
	if(CF_ERROR_SUCCESS != CInterface_SvrTradeData::getObj().GetUserInitFund(InvestorID, dbInit))
		return -1;//账户初始权益

	if(dbInit>-0.00001 && dbInit < 0.00001)
		return -1;//初始总权益为0，则不判断这个风险指标

	PlatformStru_TradingAccountInfo outData;
	if(CF_ERROR_SUCCESS != CInterface_SvrTradeData::getObj().GetUserFundInfo(InvestorID, outData))
		return -1;	

	dbBlance			 = outData.DynamicProfit; //动态权益
	std::map<std::string, PlatformStru_Position> mapInstrument2Posotion;//一种合约对应的手数，反正就空头和多头，所以这个map里面有值，就
	double dblSimulateGappedLoss=0.0;
	for(int i =0; i < (int)vecPosition.size(); i++)
	{
		PlatformStru_Position PosField = vecPosition[i];
		std::map<std::string, PlatformStru_Position>::iterator itP = mapInstrument2Posotion.find(PosField.InstrumentID);
		if(itP == mapInstrument2Posotion.end())
			mapInstrument2Posotion.insert(std::make_pair(PosField.InstrumentID, PosField));
		else
		{
			int nPos = itP->second.Position;
			if(nPos > PosField.Position)
			{
				itP->second.PositionCost = itP->second.PositionCost * (itP->second.Position - PosField.Position)/itP->second.Position ;
				itP->second.Position = PosField.Position;		
			}
			else
			{
				mapInstrument2Posotion[PosField.InstrumentID] = PosField;
			}
		}
	}
	for(std::map<std::string, PlatformStru_Position>::iterator it = mapInstrument2Posotion.begin(); it!= mapInstrument2Posotion.end(); it++)
	{
		PlatformStru_Position PosField = it->second;
		PlatformStru_DepthMarketData Quot;
		PlatformStru_InstrumentInfo info;
		if(CInterface_SvrTradeData::getObj().GetQuotInfo(PosField.InstrumentID,Quot)==CF_ERROR_SUCCESS
			&&CDataCenter::Get()->GetInstrumentInfo_DataCenter(PosField.InstrumentID,info) == CF_ERROR_SUCCESS)
		{
			if(PosField.PosiDirection==T_PD_Long)
			{
				double dbPercent = 1 - Quot.LowerLimitPrice/Quot.PreSettlementPrice;//跌幅百分比
				double db = 0.0;
				if(Quot.SettlementPrice  == util::GetDoubleInvalidValue())//结算价无效则用今天的跌停价				
					db = Quot.LowerLimitPrice - dbPercent * Quot.LowerLimitPrice;
				else
					db = Quot.SettlementPrice - dbPercent * Quot.SettlementPrice;

				dblSimulateGappedLoss+=PosField.PositionCost - db*PosField.Position*info.VolumeMultiple;

				
			}
			else if(PosField.PosiDirection==T_PD_Short)
			{
				double dbPercent = Quot.UpperLimitPrice/Quot.PreSettlementPrice -1;//涨幅百分比
				double db = 0.0;
				if(Quot.SettlementPrice  == util::GetDoubleInvalidValue())//结算价无效则用今天的涨停价					
					db = Quot.UpperLimitPrice + dbPercent * Quot.UpperLimitPrice;
				else
					db = Quot.SettlementPrice + dbPercent * Quot.SettlementPrice;

				dblSimulateGappedLoss+= db*PosField.Position*info.VolumeMultiple - PosField.PositionCost;				

			}
		}
	}
	mapInstrument2Posotion.clear();
	dblRisk= 1-(dbBlance - dblSimulateGappedLoss)/dbInit;

	//dblRisk=(dblMaxGappedLoss-dbBlance+dblSimulateGappedLoss)/dblMaxGappedLoss;
	
	int nRet = GetRiskLevel(InvestorID, strTime, RI_GappedMarket, dblRisk, vecThisRisk, nLevel, strTimeBegin, strTimeEnd);	
	return nRet;
}
void CRiskMsgCalc::ActiveCalcGappedMarketRisk(const string& InvestorID, std::string strTime)
{//隔夜跳空
	double	dblRisk  = 0.0;	
	SResponse vecThisRisk;
	int nLevel = 0;
	std::string strTimeBegin, strTimeEnd;

	int nRet = GetGappedMarketRisk_Level(InvestorID, strTime, dblRisk, vecThisRisk, nLevel, strTimeBegin, strTimeEnd);

	int nEventID =-1;//风险事件ID
	if(nRet == -1 || nRet == 0)
	{//没有风险，则判断之前是不是有风险，有风险，则是风险消失
		if(m_RiskProcess->HasGappedMarketRisk("",InvestorID, strTimeBegin, strTimeEnd))
			m_RiskProcess->AddGappedMarketEvent("",InvestorID, strTimeBegin, strTimeEnd,dblRisk,0, nEventID, vecThisRisk);	
		CDataCenter::Get()->AddAccountForbid(InvestorID, RI_GappedMarket, "", NULL, vecThisRisk, false);	
	}
	else if(nRet == 1)
	{//有风险				
		m_RiskProcess->AddGappedMarketEvent("",InvestorID, strTimeBegin, strTimeEnd,dblRisk,nLevel, nEventID, vecThisRisk);	
		CDataCenter::Get()->AddAccountForbid(InvestorID, RI_GappedMarket, "", NULL, vecThisRisk, true);	
	}
	
}
int CRiskMsgCalc::GetMarginRatioRisk_Level(const string& InvestorID, std::string strTime, double& dblRisk, SResponse& vecThisRisk, int& nLevel, std::string& strTimeBegin, std::string& strTimeEnd)
{//保证金比例风险
	//OutPut output("ActiveCalcMarginRatioRisk");
	double  dbBlance = 0.0;
	double dblCurrMargin = 0.0;

	PlatformStru_TradingAccountInfo outData;
	if(CF_ERROR_SUCCESS != CInterface_SvrTradeData::getObj().GetUserFundInfo(InvestorID, outData))
		return -1;
	dbBlance	  = outData.DynamicProfit; //动态权益
	dblCurrMargin = outData.CurrMargin;//当前保证金
	dblRisk=dblCurrMargin/dbBlance;

	
	int nRet = GetRiskLevel(InvestorID, strTime, RI_MarginRatio, dblRisk, vecThisRisk, nLevel, strTimeBegin, strTimeEnd);
	return nRet;
}
void CRiskMsgCalc::ActiveCalcMarginRatioRisk(const string& InvestorID, std::string strTime)
{//保证金比例风险
	//OutPut output("ActiveCalcMarginRatioRisk");
	double	dblRisk  = 0.0;
	SResponse vecThisRisk;
	int nLevel = 0;
	std::string strTimeBegin, strTimeEnd;

	int nRet = GetMarginRatioRisk_Level(InvestorID,strTime, dblRisk, vecThisRisk, nLevel, strTimeBegin, strTimeEnd);

	int nEventID =-1;//风险事件ID
	if(nRet == -1 || nRet == 0)
	{//没有风险，则判断之前是不是有风险，有风险，则是风险消失
		if(m_RiskProcess->HasMarginRatioRisk("",InvestorID, strTimeBegin, strTimeEnd))
			m_RiskProcess->AddMarginRatioEvent("",InvestorID, strTimeBegin, strTimeEnd,dblRisk,0, nEventID, vecThisRisk);		
		CDataCenter::Get()->AddAccountForbid(InvestorID, RI_MarginRatio, "", NULL, vecThisRisk, false);
	}
	else if(nRet == 1)
	{//有风险				
		m_RiskProcess->AddMarginRatioEvent("",InvestorID, strTimeBegin, strTimeEnd,dblRisk,nLevel, nEventID, vecThisRisk);	
		CDataCenter::Get()->AddAccountForbid(InvestorID, RI_MarginRatio, "", NULL, vecThisRisk, true);
	}
	
}

void CRiskMsgCalc::ActivePosiDetalMaxTimeRiskLose(const string& InvestorID, std::string strTime,
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

int CRiskMsgCalc::GetFundNetValueRisk_Level(const string& InvestorID, std::string strTime, double& dblRisk, SResponse& vecThisRisk, int& nLevel, std::string& strTimeBegin, std::string& strTimeEnd)
{//基金净值风险 
	//OutPut output("ActiveCalcFundNetValueRisk");	
	NetFundParam netFundParam;
	bool bFind = CalcGetFundParam(InvestorID, strTime, netFundParam);	
	if(!bFind)
		return -1;//基金净值配置参数取不到，无法进行风险判断
	//////////////////////////////////////////////////////////////////////////	
	double dblBalance= 0.0;
	PlatformStru_TradingAccountInfo outData;
	if(CF_ERROR_SUCCESS != CInterface_SvrTradeData::getObj().GetUserFundInfo(InvestorID, outData))
		return -1;
	dblBalance = outData.DynamicProfit;//动态权益

	
	if(netFundParam.dOuterVolumn + netFundParam.dInnerVolumn <= 0)
		return -1;	
	dblRisk = (netFundParam.dOuterNetAsset + dblBalance)/(netFundParam.dOuterVolumn + netFundParam.dInnerVolumn);	
	//////////////////////////////////////////////////////////////////////////	
	int nRet = GetRiskLevel(InvestorID, strTime, RI_FundNetValue, dblRisk, vecThisRisk, nLevel, strTimeBegin, strTimeEnd);

	return nRet;
}
void CRiskMsgCalc::ActiveCalcFundNetValueRisk(const string& InvestorID, std::string strTime, bool bDoResponse)
{//基金净值风险 
	double dblRisk = 0.0;
	SResponse vecThisRisk;
	int nLevel = 0;
	std::string strTimeBegin, strTimeEnd;
	int nRet = GetFundNetValueRisk_Level(InvestorID, strTime, dblRisk, vecThisRisk, nLevel, strTimeBegin, strTimeEnd);
	

	int nEventID =-1;//风险事件ID
	if(nRet == -1 || nRet == 0)
	{//没有风险，则判断之前是不是有风险，有风险，则是风险消失
		if(m_RiskProcess->HasFundNetValueRisk("",InvestorID, strTimeBegin, strTimeEnd))
			m_RiskProcess->AddFundNetValueEvent("",InvestorID, strTimeBegin, strTimeEnd,dblRisk,0, nEventID, vecThisRisk, bDoResponse);		
		CDataCenter::Get()->AddAccountForbid(InvestorID, RI_FundNetValue, "", NULL, vecThisRisk, false);	
	}
	else if(nRet == 1)
	{//有风险				
		m_RiskProcess->AddFundNetValueEvent("",InvestorID, strTimeBegin, strTimeEnd,dblRisk,nLevel, nEventID, vecThisRisk, bDoResponse);	
		CDataCenter::Get()->AddAccountForbid(InvestorID, RI_FundNetValue, "", NULL, vecThisRisk, true);
	}
	
}
bool CRiskMsgCalc::CalcGetFundParam( const string& InvestorID, std::string strTime, NetFundParam& netFundParam)
{	
	//OutPut output("CalcGetFundParam");	
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

int CRiskMsgCalc::CalcOverTime_DealerPosiDetail(const string& InvestorID, const string& strProdID, 
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

int CRiskMsgCalc::CalcRemainTime(std::string& strInstID, std::string& strOpenTime)
{
	int nOpenTime = GetRemainTime(strOpenTime);

	PlatformStru_InstrumentInfo info;	
	if(CDataCenter::Get()->GetInstrumentInfo_DataCenter(strInstID, info) != CF_ERROR_SUCCESS)
		return nOpenTime;
	else
		return CalcRealHoldingTime(std::string(info.ExchangeID), nOpenTime);
}

int CRiskMsgCalc::CalcRealHoldingTime(std::string& strExchangeID, int nTime)
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

int CRiskMsgCalc::GetRemainTime(std::string& strOpenTime)
{
	int iHour=0, iMin=0, iSec=0;
	sscanf_s(strOpenTime.c_str(), "%02d:%02d:%02d", &iHour, &iMin, &iSec);
	int nOpenTime = iHour*3600+iMin*60+iSec;
	SYSTEMTIME currTime;
	::GetLocalTime(&currTime);
	int nCurrTime = currTime.wHour*3600+currTime.wMinute*60+currTime.wSecond;
	return nCurrTime-nOpenTime+1;
}
void CRiskMsgCalc::ProcessForbid(const string& InvestorID)
{
	vector<SRiskControl> vecRiskControl, vecAuto;

	std::map<string, std::map<SRiskKey, SResponse>>  mapAccountRisk;
	CDataCenter::Get()->GetAccountForbid(mapAccountRisk);
	std::map<string, std::map<SRiskKey, SResponse>>::iterator  it = mapAccountRisk.find(InvestorID);
	if(it == mapAccountRisk.end())
	{//如果风控动作里面没有该交易员，则所有的状态归0，恢复所有权限
		std::map<RiskWarningType, SRiskControl> mapRiskControl;//空的map就可以恢复所有权限
		//if(!g_bSendToExcute)
		//	CInterface_SvrTradeExcute::getObj().RiskControl(InvestorID, mapRiskControl);	
		if(g_bSendToExcute)
		{
			int nSocket =0;
			InterlockedExchange((volatile long*)(&nSocket),g_hSocket);
			if(nSocket)
			{		
				std::vector<SRiskControl2Execute> vecExecute;
				SRiskControl2Execute temp;
				strcpy(temp.Investor, InvestorID.c_str());
				temp.nResponseType = WarningType_None;
				vecExecute.push_back(temp);

				CInterface_SvrTcp::getObj().SendPkgData(nSocket,Cmd_RM_ToExecute_RiskForce_Req,
					&vecExecute[0], vecExecute.size() * sizeof(SRiskControl2Execute),0,0,CF_ERROR_SUCCESS);	
			}
			else
			{//输出socket无效
				RISK_LOGINFO("主从服务断开，设置账户风控动作失败\(例如：单合约限制开仓等\)\n");
			}
		}
		return;
	}

	std::map<SRiskKey, SResponse>& mapKey = it->second;
	std::map<SRiskKey, SResponse>::iterator itKey = mapKey.begin();
	while(itKey != mapKey.end())
	{
		SResponse& sResponse = itKey->second;
		std::map<int,RiskPlan>::iterator it = sResponse.mapResponse.begin();
		if(it != sResponse.mapResponse.end())
		{
			RiskPlan& plan = it->second;
			if((plan.WaringLevel.nResponseType & WarningType_ForbidOpen_Single) ==  WarningType_ForbidOpen_Single)//限制开仓(单合约);
			{
				SRiskControl rControl;
				rControl.nResponseType = WarningType_ForbidOpen_Single;
				rControl.setInstrument = sResponse.setProducts;		//vecProducts这个里面是具体合约了，不是品种名称了	
				//自动强平需要风险类型和等级，提醒交易客户端
				rControl.nEventID         = sResponse.nRiskEventID;
				rControl.nRiskIndicatorID = plan.WaringLevel.nRiskIndicatorID;
				rControl.nRiskLevelID	  = plan.WaringLevel.nRiskLevelID;
				vecRiskControl.push_back(rControl);
			}
			if((plan.WaringLevel.nResponseType & WarningType_ForbidOpen) ==  WarningType_ForbidOpen)//限制开仓
			{
				SRiskControl rControl;
				rControl.nResponseType = WarningType_ForbidOpen;
				rControl.setInstrument = sResponse.setProducts;		//vecProducts这个里面是具体合约了，不是品种名称了

				//自动强平需要风险类型和等级，提醒交易客户端
				rControl.nEventID         = sResponse.nRiskEventID;
				rControl.nRiskIndicatorID = plan.WaringLevel.nRiskIndicatorID;
				rControl.nRiskLevelID	  = plan.WaringLevel.nRiskLevelID;
				vecRiskControl.push_back(rControl);
			}
			if((plan.WaringLevel.nResponseType & WarningType_ForbidOrder_Single) ==  WarningType_ForbidOrder_Single)//限制下单(单合约)
			{
				SRiskControl rControl;
				rControl.nResponseType = WarningType_ForbidOrder_Single;
				rControl.setInstrument = sResponse.setProducts;		//vecProducts这个里面是具体合约了，不是品种名称了	

				//自动强平需要风险类型和等级，提醒交易客户端
				rControl.nEventID         = sResponse.nRiskEventID;
				rControl.nRiskIndicatorID = plan.WaringLevel.nRiskIndicatorID;
				rControl.nRiskLevelID	  = plan.WaringLevel.nRiskLevelID;

				vecRiskControl.push_back(rControl);
			}
			if((plan.WaringLevel.nResponseType & WarningType_ForbidOrder) ==  WarningType_ForbidOrder)//限制下单
			{
				SRiskControl rControl;
				rControl.nResponseType = WarningType_ForbidOrder;
				rControl.setInstrument = sResponse.setProducts;		//vecProducts这个里面是具体合约了，不是品种名称了	

				//自动强平需要风险类型和等级，提醒交易客户端
				rControl.nEventID         = sResponse.nRiskEventID;
				rControl.nRiskIndicatorID = plan.WaringLevel.nRiskIndicatorID;
				rControl.nRiskLevelID	  = plan.WaringLevel.nRiskLevelID;

				vecRiskControl.push_back(rControl);
			}			
		}
		itKey++;
	}
	//交易执行处理风控动作
	std::map<RiskWarningType, SRiskControl> mapRiskControl;	
	for(int  k =0; k< (int)vecRiskControl.size(); k++)
	{
		SRiskControl& rControl = vecRiskControl[k];
		std::map<RiskWarningType, SRiskControl>::iterator itFind = mapRiskControl.find(rControl.nResponseType);
		if(itFind != mapRiskControl.end())
		{
			SRiskControl& r = itFind->second;
			r.setInstrument.insert(rControl.setInstrument.begin(), rControl.setInstrument.end());
		}
		else
			mapRiskControl.insert(make_pair(rControl.nResponseType, rControl));
	}	
	//if(!g_bSendToExcute)
	//	CInterface_SvrTradeExcute::getObj().RiskControl(InvestorID, mapRiskControl);	
	if(g_bSendToExcute)
	{
		int nSocket =0;
		InterlockedExchange((volatile long*)(&nSocket),g_hSocket);
		if(nSocket)
		{		
			std::vector<SRiskControl2Execute> vecExecute;
			std::map<RiskWarningType, SRiskControl>::iterator itBegin = mapRiskControl.begin();
			while (itBegin != mapRiskControl.end())
			{
				SRiskControl sControl = itBegin->second;
				SRiskControl2Execute sTemp;
				memset(&sTemp, 0, sizeof(SRiskControl2Execute));

				strcpy(sTemp.Investor, InvestorID.c_str());
				sTemp.nResponseType = sControl.nResponseType;
				sTemp.PostionDetail = sControl.PostionDetail;
				sTemp.nEventID	    = sControl.nEventID;
				sTemp.nRiskIndicatorID	= sControl.nRiskIndicatorID;
				sTemp.nRiskLevelID      = sControl.nRiskLevelID;
				if(sControl.setInstrument.size() >0)
				{
					for(std::set<string>::iterator it = sControl.setInstrument.begin(); it != sControl.setInstrument.end(); it++)
					{
						std::string strInstrument = *it;
						strcpy(sTemp.Instrument, strInstrument.c_str());
						vecExecute.push_back(sTemp);
					}
				}
				else
					vecExecute.push_back(sTemp);
			
			

				itBegin++;
			}
			if(vecExecute.size() >0)
			CInterface_SvrTcp::getObj().SendPkgData(nSocket,Cmd_RM_ToExecute_RiskForce_Req,
				&vecExecute[0], sizeof(SRiskControl2Execute)*vecExecute.size(),0,0,CF_ERROR_SUCCESS);	
			else
			{
				SRiskControl2Execute temp;
				strcpy(temp.Investor, InvestorID.c_str());
				temp.nResponseType = WarningType_None;
				vecExecute.push_back(temp);

				CInterface_SvrTcp::getObj().SendPkgData(nSocket,Cmd_RM_ToExecute_RiskForce_Req,
					&vecExecute[0], vecExecute.size() * sizeof(SRiskControl2Execute),0,0,CF_ERROR_SUCCESS);					
			}
		}
		else
		{//输出socket无效
			RISK_LOGINFO("主从服务断开，设置账户风控动作失败\（例如：单合约限制开仓等\）\n");

		}
	}
	CDataCenter::Get()->EraseAccountForbid(InvestorID);//风险动作执行后去掉
}
void CRiskMsgCalc::ProcessRisk(const string& InvestorID, bool bSetTradeStaus)
{
	vector<SRiskControl> vecRiskControl, vecAuto;

	std::map<string, std::map<SRiskKey, SResponse>>  mapAccountRisk;
	CDataCenter::Get()->GetAccountRisk(mapAccountRisk);
	std::map<string, std::map<SRiskKey, SResponse>>::iterator  it = mapAccountRisk.find(InvestorID);
	if(it == mapAccountRisk.end())
	{
		return;
	}

	std::map<SRiskKey, SResponse>& mapKey = it->second;
		
	std::map<SRiskKey, SResponse>::iterator itKey = mapKey.begin();
	while(itKey != mapKey.end())
	{
		SResponse& sResponse = itKey->second;
		std::map<int,RiskPlan>::iterator it = sResponse.mapResponse.begin();
		if(it != sResponse.mapResponse.end())
		{
			RiskPlan& plan = it->second;
			if((plan.WaringLevel.nResponseType & WarningType_ForceClose_Single) == WarningType_ForceClose_Single)  //市价强平（单合约）						 
			{
				SRiskControl rControl;
				rControl.nResponseType = WarningType_ForceClose_Single;
				rControl.setInstrument.insert(itKey->first.strInstrument);

				//自动强平需要风险类型和等级，提醒交易客户端
				rControl.nEventID         = sResponse.nRiskEventID;
				rControl.nRiskIndicatorID = plan.WaringLevel.nRiskIndicatorID;
				rControl.nRiskLevelID	  = plan.WaringLevel.nRiskLevelID;
				vecAuto.push_back(rControl);
			}
			if((plan.WaringLevel.nResponseType & WarningType_ForceClose) == WarningType_ForceClose)		  //市价强平
			{
				SRiskControl rControl;
				rControl.nResponseType = WarningType_ForceClose;
				rControl.setInstrument = sResponse.setProducts;		

				//自动强平需要风险类型和等级，提醒交易客户端
				rControl.nEventID         = sResponse.nRiskEventID;
				rControl.nRiskIndicatorID = plan.WaringLevel.nRiskIndicatorID;
				rControl.nRiskLevelID	  = plan.WaringLevel.nRiskLevelID;
				vecAuto.push_back(rControl);
			}
			if((plan.WaringLevel.nResponseType & WarningType_ForceClose_SingleOpen) == WarningType_ForceClose_SingleOpen)		  //市价强平单笔持仓时间风险
			{
				SRiskControl rControl;
				rControl.nResponseType = WarningType_ForceClose_SingleOpen;
				rControl.setInstrument = sResponse.setProducts;		
				rControl.PostionDetail = itKey->first.PostionDetail;

				//自动强平需要风险类型和等级，提醒交易客户端
				rControl.nEventID         = sResponse.nRiskEventID;
				rControl.nRiskIndicatorID = plan.WaringLevel.nRiskIndicatorID;
				rControl.nRiskLevelID	  = plan.WaringLevel.nRiskLevelID;
				vecAuto.push_back(rControl);
			}
		}
		itKey++;
	}

	for(int k = 0; k< (int)vecAuto.size(); k++)
	{//自动强平
		SRiskControl& sRiskControl = vecAuto[k];
		if(sRiskControl.nResponseType == WarningType_ForceClose)
		{						
			CForceCloseMsgQueue::Get()->AddForceCloseMsg(sRiskControl.nEventID, InvestorID, "", NULL, sRiskControl.nRiskIndicatorID, sRiskControl.nRiskLevelID);
		}
		else if(sRiskControl.nResponseType == WarningType_ForceClose_Single)
		{
			std::set<std::string>::iterator it = sRiskControl.setInstrument.begin();
			for(; it != sRiskControl.setInstrument.end(); it++)
			{
				std::string str = *it;
				CForceCloseMsgQueue::Get()->AddForceCloseMsg(sRiskControl.nEventID, InvestorID, str, NULL, sRiskControl.nRiskIndicatorID, sRiskControl.nRiskLevelID);
			}			
		}
		else if(sRiskControl.nResponseType == WarningType_ForceClose_SingleOpen)
		{			
			CForceCloseMsgQueue::Get()->AddForceCloseMsg(sRiskControl.nEventID, InvestorID, "", &sRiskControl.PostionDetail, sRiskControl.nRiskIndicatorID, sRiskControl.nRiskLevelID);
		}
	}
	
	CDataCenter::Get()->EraseAccountRisk(InvestorID);//风险动作执行后去掉
}
void CRiskMsgCalc::SendQuto(PlatformStru_DepthMarketData& DepthMarketData)
{
    if(!CDataCenter::Get()->IsQutoSubscribed(DepthMarketData))
		return;

	std::string AccIDOrInst=DepthMarketData.InstrumentID;
	int RspCmdID=Cmd_RM_Quot_Push;	

	std::set<SOCKET>::const_iterator it;
	std::set<SOCKET> sset;
	m_pThreadShareData->GetSubscribeData(AccIDOrInst,SubscribeID_Quot,sset);
	for (it=sset.begin();it!=sset.end();++it)
	{
		CInterface_SvrTcp::getObj().SendPkgData(*it, RspCmdID,
			(void*)&DepthMarketData,sizeof(DepthMarketData), 
			0, 0, CF_ERROR_SUCCESS, 0,	0);			
	}
}
void CRiskMsgCalc::SendFund(std::string strInvestor)
{
	PlatformStru_TradingAccountInfo tradingAccountInfo;
	if(CF_ERROR_SUCCESS != CInterface_SvrTradeData::getObj().GetUserFundInfo(strInvestor, tradingAccountInfo))
		return;

	UserInfo userInfo;	
	bool bFind = CInterface_SvrUserOrg::getObj().GetUserInfoByAccount(strInvestor, userInfo);    
	if(bFind)
	{
		char szAccount[256];
		int nUserID = userInfo.nUserID;
		sprintf(szAccount,"%d",nUserID);

		std::string AccIDOrInst = szAccount;
		int RspCmdID = Cmd_RM_Fund_Push;	
		std::set<SOCKET>::const_iterator it;
		std::set<SOCKET> sset;
		m_pThreadShareData->GetSubscribeData(AccIDOrInst,SubscribeID_Fund,sset);
		for (it=sset.begin();it!=sset.end();++it)
		{
			CInterface_SvrTcp::getObj().SendPkgData(*it, RspCmdID,
				(void*)&tradingAccountInfo,sizeof(PlatformStru_TradingAccountInfo), 
				0, 0, CF_ERROR_SUCCESS, 0,	0);			
		}
	}	
}
void CRiskMsgCalc::SendOrder(PlatformStru_OrderInfo* lpBuf)
{
	std::string strInvestor = lpBuf->InvestorID;
	UserInfo userInfo;	
	bool bFind = CInterface_SvrUserOrg::getObj().GetUserInfoByAccount(strInvestor, userInfo);    
	if(bFind)
	{
		char szAccount[256];
		int nUserID = userInfo.nUserID;
		sprintf(szAccount,"%d",nUserID);

		std::string AccIDOrInst = szAccount;
		int RspCmdID = Cmd_RM_Order_Push;	
		std::set<SOCKET>::const_iterator it;
		std::set<SOCKET> sset;
		m_pThreadShareData->GetSubscribeData(AccIDOrInst,SubscribeID_Order,sset);
		for (it=sset.begin();it!=sset.end();++it)
		{
			CInterface_SvrTcp::getObj().SendPkgData(*it, RspCmdID,
				(void*)lpBuf,sizeof(PlatformStru_OrderInfo), 
				0, 0, CF_ERROR_SUCCESS, 0,	0);			
		}
	}	
}
void CRiskMsgCalc::SendTrader(PlatformStru_TradeInfo* lpBuf)
{
	std::string strInvestor = lpBuf->InvestorID;
	UserInfo userInfo;	
	bool bFind = CInterface_SvrUserOrg::getObj().GetUserInfoByAccount(strInvestor, userInfo);    
	if(bFind)
	{
		char szAccount[256];
		int nUserID = userInfo.nUserID;
		sprintf(szAccount,"%d",nUserID);

		std::string AccIDOrInst = szAccount;
		int RspCmdID = Cmd_RM_Trade_Push;	
		std::set<SOCKET>::const_iterator it;
		std::set<SOCKET> sset;
		m_pThreadShareData->GetSubscribeData(AccIDOrInst,SubscribeID_Trade,sset);
		for (it=sset.begin();it!=sset.end();++it)
		{
			CInterface_SvrTcp::getObj().SendPkgData(*it, RspCmdID,
				(void*)lpBuf,sizeof(PlatformStru_TradeInfo), 
				0, 0, CF_ERROR_SUCCESS, 0,	0);			
		}
	}	
}
void CRiskMsgCalc::SendPosition(std::string strInvestor)
{
	std::vector<PlatformStru_Position> vec;					
	CInterface_SvrTradeData::getObj().GetUserPositions(strInvestor, "", vec);	

	UserInfo userInfo;	
	bool bFind = CInterface_SvrUserOrg::getObj().GetUserInfoByAccount(strInvestor, userInfo);    	
	if(bFind)
	{
		char szAccount[256];
		int nUserID = userInfo.nUserID;
		sprintf(szAccount,"%d",nUserID);

		std::string AccIDOrInst = szAccount;
		int RspCmdID = Cmd_RM_Position_Push;	
		std::set<SOCKET>::const_iterator it;
		std::set<SOCKET> sset;
		m_pThreadShareData->GetSubscribeData(AccIDOrInst,SubscribeID_Position,sset);
		for (it=sset.begin();it!=sset.end();++it)
		{
			if(vec.size() == 0)
			{
				CInterface_SvrTcp::getObj().SendPkgData(*it, Cmd_RM_Position_Push,
					0,0, 0, 0, CF_ERROR_SUCCESS, userInfo.nUserID,	0);
			}
			else
			{
			
				CInterface_SvrTcp::getObj().SendPkgData(*it, RspCmdID,
					&vec[0],sizeof(PlatformStru_Position)*vec.size(), 
					0, 0, CF_ERROR_SUCCESS, 0,	0);		
			}
		}
	}	

}
/////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

//交易限制风险
void CRiskMsgCalc::ActiveCalcTradeForbid(const string& InvestorID, std::string strTime, const std::string& InstrumentID)
{		
	double	dblRisk = 0.0;
	int		nVolume = 0;
	SResponse vecThisRisk;
	int		nLevel = 0;
	std::string strTimeBegin, strTimeEnd;
	int nRet = GetTradeForbid_Level(InvestorID, strTime, InstrumentID, dblRisk, vecThisRisk, nLevel, strTimeBegin, strTimeEnd);

	int nEventID =-1;//风险事件ID
	if(nRet == -1 || nRet == 0)
	{//没有风险，则判断之前是不是有风险，有风险，则是风险消失
		if(m_RiskProcess->HasTradeForbidRisk("",InvestorID, strTimeBegin, strTimeEnd,InstrumentID))
			m_RiskProcess->AddTradeForbidEvent("",InvestorID, strTimeBegin, strTimeEnd,InstrumentID,dblRisk, 0, nEventID, vecThisRisk);	
		CDataCenter::Get()->AddAccountForbid(InvestorID, RI_TRADE_FORBID, InstrumentID, NULL, vecThisRisk, false);	
	}
	else if(nRet == 1)
	{//有风险				
		m_RiskProcess->AddTradeForbidEvent("",InvestorID, strTimeBegin, strTimeEnd,InstrumentID,dblRisk,nLevel, nEventID, vecThisRisk);
		CDataCenter::Get()->AddAccountForbid(InvestorID, RI_TRADE_FORBID, InstrumentID, NULL, vecThisRisk, true);	
	}		
}
int  CRiskMsgCalc::GetTradeForbid_Level(const string& InvestorID, std::string strTime, const std::string& InstrumentID, double& dblRisk, SResponse& vecThisRisk, int& nLevel, std::string& strTimeBegin, std::string& strTimeEnd)
{
	dblRisk = 0;
	int nRet = GetTradeForbidRiskLevel(InvestorID, strTime, RI_TRADE_FORBID, InstrumentID, dblRisk, vecThisRisk, nLevel, strTimeBegin, strTimeEnd);

	return nRet;	
}
int CRiskMsgCalc::GetTradeForbidRiskLevel(string InvestorID, std::string strTime, RiskIndicatorType RiskType, std::string InstrumentID, double dblRisk, SResponse& vecResponse,int& nLevel, std::string& strTimeBegin, std::string& strTimeEnd)
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
		int nRet = CDataCenter::Get()->GetTradeForbidMapLevel(rKey, strTime, InstrumentID, dblRisk, vecResponse, nLevel, strTimeBegin, strTimeEnd);
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
void CRiskMsgCalc::ActiveCalcMarginForbid(const string& InvestorID, std::string strTime)
{
	double	dblRisk = 0.0;
	double  dbRiskMargin = 0.0;
	int		nVolume = 0;
	SResponse vecThisRisk;
	int		nLevel = 0;
	std::string strTimeBegin, strTimeEnd;
	int nRet = GetMarginForbid_Level(InvestorID, strTime, dblRisk, dbRiskMargin, vecThisRisk, nLevel, strTimeBegin, strTimeEnd);

	int nEventID =-1;//风险事件ID
	if(nRet == -1 || nRet == 0)
	{//没有风险，则判断之前是不是有风险，有风险，则是风险消失
		if(m_RiskProcess->HasMarginForbidRisk("",InvestorID, strTimeBegin, strTimeEnd))
			m_RiskProcess->AddMarginForbidEvent("",InvestorID, strTimeBegin, strTimeEnd, dblRisk, dbRiskMargin, 0, nEventID, vecThisRisk);	
		CDataCenter::Get()->AddAccountForbid(InvestorID, RI_MARGIN_FORBID, "", NULL, vecThisRisk, false);	
	}
	else if(nRet == 1)
	{//有风险				
		m_RiskProcess->AddMarginForbidEvent("",InvestorID, strTimeBegin, strTimeEnd,dblRisk,dbRiskMargin, nLevel, nEventID, vecThisRisk);
		CDataCenter::Get()->AddAccountForbid(InvestorID, RI_MARGIN_FORBID, "", NULL, vecThisRisk, true);	
	}	

}
int  CRiskMsgCalc::GetMarginForbid_Level(const string& InvestorID, std::string strTime,  double& dblRisk, double& dbRiskMargin, SResponse& vecThisRisk, int& nLevel, std::string& strTimeBegin, std::string& strTimeEnd)
{
	double  dbBlance = 0.0;
	double dblCurrMargin = 0.0;

	PlatformStru_TradingAccountInfo outData;
	if(CF_ERROR_SUCCESS != CInterface_SvrTradeData::getObj().GetUserFundInfo(InvestorID, outData))
		return -1;
	dbBlance	  = outData.DynamicProfit; //动态权益
	dblCurrMargin = outData.CurrMargin;//当前保证金
	dblRisk=dblCurrMargin/dbBlance;
	dbRiskMargin = dblCurrMargin;
	int nRet = GetMarginForbidRiskLevel(InvestorID, strTime, RI_MARGIN_FORBID, dblRisk, dbRiskMargin, vecThisRisk, nLevel, strTimeBegin, strTimeEnd);
	return nRet;	
}
int  CRiskMsgCalc::GetMarginForbidRiskLevel(string InvestorID, std::string strTime, RiskIndicatorType RiskType, double dblRisk, double& dbRiskMargin, SResponse& vecResponse,int& nLevel, std::string& strTimeBegin, std::string& strTimeEnd)
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
		int nRet = CDataCenter::Get()->GetMarginForbidMapLevel(rKey, strTime, dblRisk, dbRiskMargin, vecResponse, nLevel, strTimeBegin, strTimeEnd);
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
void CRiskMsgCalc::ActiveCalcLossForbid(const string& InvestorID, std::string strTime)
{
	double	dblRisk = 0.0;
	double  dbLoss = 0.0;
	int		nVolume = 0;
	SResponse vecThisRisk;
	int		nLevel = 0;
	std::string strTimeBegin, strTimeEnd;
	int nRet = GetLossForbid_Level(InvestorID, strTime, dblRisk, dbLoss, vecThisRisk, nLevel, strTimeBegin, strTimeEnd);

	int nEventID =-1;//风险事件ID
	if(nRet == -1 || nRet == 0)
	{//没有风险，则判断之前是不是有风险，有风险，则是风险消失
		if(m_RiskProcess->HasLossForbidRisk("",InvestorID, strTimeBegin, strTimeEnd))
			m_RiskProcess->AddLossForbidEvent("",InvestorID, strTimeBegin, strTimeEnd, dblRisk, dbLoss, 0, nEventID, vecThisRisk);	
		CDataCenter::Get()->AddAccountForbid(InvestorID, RI_LOSS_FORBID, "", NULL, vecThisRisk, false);	
	}
	else if(nRet == 1)
	{//有风险				
		m_RiskProcess->AddLossForbidEvent("",InvestorID, strTimeBegin, strTimeEnd,dblRisk, dbLoss, nLevel, nEventID, vecThisRisk);
		CDataCenter::Get()->AddAccountForbid(InvestorID, RI_LOSS_FORBID, "", NULL, vecThisRisk, true);	
	}	
}
int CRiskMsgCalc::GetLossForbid_Level(const string& InvestorID, std::string strTime,  double& dblRisk, double& dbRiskLoss, SResponse& vecThisRisk, int& nLevel, std::string& strTimeBegin, std::string& strTimeEnd)
{
	double  dbInit   = 0.0;
	double  dbBlance = 0.0;	
	if(CF_ERROR_SUCCESS != CInterface_SvrTradeData::getObj().GetUserInitFund(InvestorID, dbInit))
		return -1;//账户初始权益
	if(dbInit>-0.00001 && dbInit < 0.00001)
		return -1;//初始总权益为0，则不判断这个风险指标

	PlatformStru_TradingAccountInfo outData;
	if(CF_ERROR_SUCCESS != CInterface_SvrTradeData::getObj().GetUserFundInfo(InvestorID, outData))
		return -1;	
	dbBlance			 = outData.DynamicProfit; //动态权益
	
	dblRisk    = (dbInit - dbBlance)/dbInit;
	dbRiskLoss = dbInit - dbBlance;

	int nRet = GetLossForbidRiskLevel(InvestorID, strTime, RI_LOSS_FORBID, dblRisk, dbRiskLoss, vecThisRisk, nLevel, strTimeBegin, strTimeEnd);
	return nRet;
}
int CRiskMsgCalc::GetLossForbidRiskLevel(string InvestorID, std::string strTime, RiskIndicatorType RiskType, double dblRisk, double& dbRiskLoss, SResponse& vecResponse,int& nLevel, std::string& strTimeBegin, std::string& strTimeEnd)
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
		int nRet = CDataCenter::Get()->GetLossForbidMapLevel(rKey, strTime, dblRisk, dbRiskLoss, vecResponse, nLevel, strTimeBegin, strTimeEnd);
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
void CRiskMsgCalc::ActiveCalcOnedayLargetsLossForbid(const string& InvestorID, std::string strTime)
{
	double	dblRisk = 0.0;
	SResponse vecThisRisk;
	int		nLevel = 0;
	std::string strTimeBegin, strTimeEnd;
	int nRet = GetOnedayLargetsLoss_Level(InvestorID, strTime, dblRisk,  vecThisRisk, nLevel, strTimeBegin, strTimeEnd);

	int nEventID =-1;//风险事件ID
	if(nRet == -1 || nRet == 0)
	{//没有风险，则判断之前是不是有风险，有风险，则是风险消失
		if(m_RiskProcess->HasOnedayLargetsLossRisk("",InvestorID, strTimeBegin, strTimeEnd))
			m_RiskProcess->AddOnedayLargetsLossEvent("",InvestorID, strTimeBegin, strTimeEnd, dblRisk, 0, nEventID, vecThisRisk);	
		CDataCenter::Get()->AddAccountForbid(InvestorID, RI_ONEDAY_LARGESTLOSS, "", NULL, vecThisRisk, false);	
	}
	else if(nRet == 1)
	{//有风险				
		m_RiskProcess->AddOnedayLargetsLossEvent("",InvestorID, strTimeBegin, strTimeEnd,dblRisk, nLevel, nEventID, vecThisRisk);
		CDataCenter::Get()->AddAccountForbid(InvestorID, RI_ONEDAY_LARGESTLOSS, "", NULL, vecThisRisk, true);	
	}	

}
int CRiskMsgCalc::GetOnedayLargetsLoss_Level(const string& InvestorID, std::string strTime,  double& dblRisk, SResponse& vecThisRisk, int& nLevel, std::string& strTimeBegin, std::string& strTimeEnd)
{
	double  dbInit   = 0.0;
	
	if(CF_ERROR_SUCCESS != CInterface_SvrTradeData::getObj().GetUserInitFund(InvestorID, dbInit))
		return -1;//账户初始权益 （不知道这个是不是账户累计初始资金）
	if(dbInit>-0.00001 && dbInit < 0.00001)
		return -1;//初始总权益为0，则不判断这个风险指标

	PlatformStru_TradingAccountInfo outData;
	if(CF_ERROR_SUCCESS != CInterface_SvrTradeData::getObj().GetUserFundInfo(InvestorID, outData))
		return -1;	
	
    //计算公式: （平仓盈亏+持仓盈亏+手续费）/账户累计初始资金： 平仓盈亏+持仓盈亏 应该为负值 
	dblRisk    = (outData.Commission - outData.CloseProfit - outData.PositionProfit)/dbInit;

	int nRet = GetRiskLevel(InvestorID, strTime, RI_ONEDAY_LARGESTLOSS, dblRisk, vecThisRisk, nLevel, strTimeBegin, strTimeEnd);
	return nRet;

}
void CRiskMsgCalc::ActiveCalcLossMax(const string& InvestorID, std::string strTime)
{
	double	dblRisk = 0.0;
	SResponse vecThisRisk;
	int		nLevel = 0;
	std::string strTimeBegin, strTimeEnd;
	int nRet = GetLossMax_Level(InvestorID, strTime, dblRisk,  vecThisRisk, nLevel, strTimeBegin, strTimeEnd);

	int nEventID =-1;//风险事件ID
	if(nRet == -1 || nRet == 0)
	{//没有风险，则判断之前是不是有风险，有风险，则是风险消失
		if(m_RiskProcess->HasLossMaxRisk("",InvestorID, strTimeBegin, strTimeEnd))
			m_RiskProcess->AddLossMaxEvent("",InvestorID, strTimeBegin, strTimeEnd, dblRisk, 0, nEventID, vecThisRisk);	
		CDataCenter::Get()->AddAccountForbid(InvestorID, RI_LOSSMAXVALUE, "", NULL, vecThisRisk, false);	
	}
	else if(nRet == 1)
	{//有风险				
		m_RiskProcess->AddLossMaxEvent("",InvestorID, strTimeBegin, strTimeEnd,dblRisk, nLevel, nEventID, vecThisRisk);
		CDataCenter::Get()->AddAccountForbid(InvestorID, RI_LOSSMAXVALUE, "", NULL, vecThisRisk, true);	
	}	
}
int CRiskMsgCalc::GetLossMax_Level(const string& InvestorID, std::string strTime,  double& dblRisk, SResponse& vecThisRisk, int& nLevel, std::string& strTimeBegin, std::string& strTimeEnd)
{
	double  dbInit   = 0.0;

	if(CF_ERROR_SUCCESS != CInterface_SvrTradeData::getObj().GetUserInitFund(InvestorID, dbInit))
		return -1;//账户初始权益 （不知道这个是不是账户累计初始资金）
	if(dbInit>-0.00001 && dbInit < 0.00001)
		return -1;//初始总权益为0，则不判断这个风险指标

	PlatformStru_TradingAccountInfo outData;
	if(CF_ERROR_SUCCESS != CInterface_SvrTradeData::getObj().GetUserFundInfo(InvestorID, outData))
		return -1;	

	//计算公式：（动态权益--账户初始权益）/账户初始权益 
	dblRisk    = (outData.DynamicProfit - dbInit)/dbInit;
	int nRet = GetRiskLevel(InvestorID, strTime, RI_LOSSMAXVALUE, dblRisk, vecThisRisk, nLevel, strTimeBegin, strTimeEnd);
	return nRet;
}
void CRiskMsgCalc::ActiveCalcContractsValuesForbid(const string& InvestorID, std::string strTime)
{
	double	dblRisk = 0.0;
	SResponse vecThisRisk;
	int		nLevel = 0;
	std::string strTimeBegin, strTimeEnd;
	int nRet = GetContractsValues_Level(InvestorID, strTime, dblRisk,  vecThisRisk, nLevel, strTimeBegin, strTimeEnd);

	int nEventID =-1;//风险事件ID
	if(nRet == -1 || nRet == 0)
	{//没有风险，则判断之前是不是有风险，有风险，则是风险消失
		if(m_RiskProcess->HasContractsValuesRisk("",InvestorID, strTimeBegin, strTimeEnd))
			m_RiskProcess->AddContractsValuesEvent("",InvestorID, strTimeBegin, strTimeEnd, dblRisk, 0, nEventID, vecThisRisk);	
		CDataCenter::Get()->AddAccountForbid(InvestorID, RI_CONTRACTS_VALUES, "", NULL, vecThisRisk, false);	
	}
	else if(nRet == 1)
	{//有风险				
		m_RiskProcess->AddContractsValuesEvent("",InvestorID, strTimeBegin, strTimeEnd,dblRisk, nLevel, nEventID, vecThisRisk);
		CDataCenter::Get()->AddAccountForbid(InvestorID, RI_CONTRACTS_VALUES, "", NULL, vecThisRisk, true);	
	}	

}
void CRiskMsgCalc::ActiveCalcMaxRetrace(const string& InvestorID, std::string strTime)
{
	double	dblRisk = 0.0;
	SResponse vecThisRisk;
	int		nLevel = 0;
	std::string strTimeBegin, strTimeEnd;
	int nRet = GetMaxRetrace_Level(InvestorID, strTime, dblRisk,  vecThisRisk, nLevel, strTimeBegin, strTimeEnd);

	int nEventID =-1;//风险事件ID
	if(nRet == -1 || nRet == 0)
	{//没有风险，则判断之前是不是有风险，有风险，则是风险消失
		if(m_RiskProcess->HasMaxRetraceRisk("",InvestorID, strTimeBegin, strTimeEnd))
			m_RiskProcess->AddMaxRetraceEvent("",InvestorID, strTimeBegin, strTimeEnd, dblRisk, 0, nEventID, vecThisRisk);	
		CDataCenter::Get()->AddAccountForbid(InvestorID, RI_MAXRETRACE, "", NULL, vecThisRisk, false);	
	}
	else if(nRet == 1)
	{//有风险				
		m_RiskProcess->AddMaxRetraceEvent("",InvestorID, strTimeBegin, strTimeEnd,dblRisk, nLevel, nEventID, vecThisRisk);
		CDataCenter::Get()->AddAccountForbid(InvestorID, RI_MAXRETRACE, "", NULL, vecThisRisk, true);	
	}	
}
int CRiskMsgCalc::GetMaxRetrace_Level(const string& InvestorID, std::string strTime,  double& dblRisk, SResponse& vecThisRisk, int& nLevel, std::string& strTimeBegin, std::string& strTimeEnd)
{
	PlatformStru_TradingAccountInfo outData;
	if(CF_ERROR_SUCCESS != CInterface_SvrTradeData::getObj().GetUserFundInfo(InvestorID, outData))
		return -1;	
	
	double dbDynamicProfit = 0;
	
	AccountLossDay accountLossDay;
	accountLossDay.maxdynamicProfit = 0;
	BrokerAccountKey key;
	strcpy(key.BrokerID, "");
	strcpy(key.AccountID, InvestorID.c_str());
	bool bGet = CDataCenter::Get()->GetAccountLossDays(key, accountLossDay);
	
	if(outData.DynamicProfit > accountLossDay.maxdynamicProfit)
		dbDynamicProfit = accountLossDay.maxdynamicProfit;
	else 
		dbDynamicProfit = outData.DynamicProfit;
	
	accountLossDay.maxdynamicProfit = dbDynamicProfit;
	strcpy(accountLossDay.BrokerID, "");
	strcpy(accountLossDay.AccountID, InvestorID.c_str());
	accountLossDay.dynamicProfit = outData.DynamicProfit;
	CDataCenter::Get()->SetAccountLossDays(key, accountLossDay);
	//计算公式：最高点权益 = 动态权益最大值 当动态权益超过最高点权益时，赋值给最高点权益。 
	//最大回撤=（动态权益 – 最高点权益）/ 最高点权益。是一个负数值。
	dblRisk    = (outData.DynamicProfit - dbDynamicProfit)/dbDynamicProfit;
	int nRet = GetRiskLevel(InvestorID, strTime, RI_MAXRETRACE, dblRisk, vecThisRisk, nLevel, strTimeBegin, strTimeEnd);
	return nRet;
}

int CRiskMsgCalc::GetContractsValues_Level(const string& InvestorID, std::string strTime,  double& dblRisk, SResponse& vecThisRisk, int& nLevel, std::string& strTimeBegin, std::string& strTimeEnd)
{
	vector<PlatformStru_Position> vecPosition;
	if(CF_ERROR_SUCCESS != CInterface_SvrTradeData::getObj().GetUserPositions(InvestorID, "", vecPosition))
		return -1;
	if(vecPosition.size() == 0)//没有持仓自然没有风险
		return -1;

	
	double dbCount=0.0;
	for(int i =0; i < (int)vecPosition.size(); i++)
	{
		PlatformStru_Position PosField = vecPosition[i];
		PlatformStru_DepthMarketData Quot;
		PlatformStru_InstrumentInfo info;
		if(CInterface_SvrTradeData::getObj().GetQuotInfo(PosField.InstrumentID,Quot)==CF_ERROR_SUCCESS
			&&CDataCenter::Get()->GetInstrumentInfo_DataCenter(PosField.InstrumentID,info) == CF_ERROR_SUCCESS)
		{
			if(PosField.PosiDirection==T_PD_Long)
				dbCount += Quot.LastPrice*PosField.Position*info.VolumeMultiple;
			else if(PosField.PosiDirection==T_PD_Short)
				dbCount += Quot.LastPrice*PosField.Position*info.VolumeMultiple;

		}
	}

	dblRisk= dbCount;
	int nRet = GetRiskLevel(InvestorID, strTime, RI_CONTRACTS_VALUES, dblRisk, vecThisRisk, nLevel, strTimeBegin, strTimeEnd);	
	return nRet;
}

void CRiskMsgCalc::ActiveBullBearValuesForbid(const string& InvestorID, std::string strTime)
{
	double	dblRisk = 0.0;
	SResponse vecThisRisk;
	int		nLevel = 0;
	std::string strTimeBegin, strTimeEnd;
	int nRet = GetBullBearValues_Level(InvestorID, strTime, dblRisk,  vecThisRisk, nLevel, strTimeBegin, strTimeEnd);

	int nEventID =-1;//风险事件ID
	if(nRet == -1 || nRet == 0)
	{//没有风险，则判断之前是不是有风险，有风险，则是风险消失
		if(m_RiskProcess->HasBullBearValuesRisk("",InvestorID, strTimeBegin, strTimeEnd))
			m_RiskProcess->AddBullBearValuesEvent("",InvestorID, strTimeBegin, strTimeEnd, dblRisk, 0, nEventID, vecThisRisk);	
		CDataCenter::Get()->AddAccountForbid(InvestorID, RI_BULLBEAR_VALUES, "", NULL, vecThisRisk, false);	
	}
	else if(nRet == 1)
	{//有风险				
		m_RiskProcess->AddBullBearValuesEvent("",InvestorID, strTimeBegin, strTimeEnd,dblRisk, nLevel, nEventID, vecThisRisk);
		CDataCenter::Get()->AddAccountForbid(InvestorID, RI_BULLBEAR_VALUES, "", NULL, vecThisRisk, true);	
	}	
}
int  CRiskMsgCalc::GetBullBearValues_Level(const string& InvestorID, std::string strTime,  double& dblRisk, SResponse& vecThisRisk, int& nLevel, std::string& strTimeBegin, std::string& strTimeEnd)
{
	vector<PlatformStru_Position> vecPosition;
	if(CF_ERROR_SUCCESS != CInterface_SvrTradeData::getObj().GetUserPositions(InvestorID, "", vecPosition))
		return -1;
	if(vecPosition.size() == 0)//没有持仓自然没有风险
		return -1;


	double dbLongCount = 0.0, dbShortCount = 0.0;
	for(int i =0; i < (int)vecPosition.size(); i++)
	{
		PlatformStru_Position PosField = vecPosition[i];
		PlatformStru_DepthMarketData Quot;
		PlatformStru_InstrumentInfo info;
		if(CInterface_SvrTradeData::getObj().GetQuotInfo(PosField.InstrumentID,Quot)==CF_ERROR_SUCCESS
			&&CDataCenter::Get()->GetInstrumentInfo_DataCenter(PosField.InstrumentID,info) == CF_ERROR_SUCCESS)
		{
			if(PosField.PosiDirection==T_PD_Long)
				dbLongCount = dbLongCount + Quot.LastPrice*PosField.Position*info.VolumeMultiple;
			else if(PosField.PosiDirection==T_PD_Short)
				dbShortCount = dbShortCount +Quot.LastPrice*PosField.Position*info.VolumeMultiple;
		}		
	}

	dblRisk= dbLongCount - dbShortCount;
	int nRet = GetRiskLevel(InvestorID, strTime, RI_BULLBEAR_VALUES, dblRisk, vecThisRisk, nLevel, strTimeBegin, strTimeEnd);	
	return nRet;
}
void CRiskMsgCalc::ActiveLossContinueDays(const string& InvestorID, std::string strTime)
{
	double	dblRisk = 0.0;
	SResponse vecThisRisk;
	int		nLevel = 0;
	std::string strTimeBegin, strTimeEnd;
	int nRet = GetLossContinueDays_Level(InvestorID, strTime, dblRisk,  vecThisRisk, nLevel, strTimeBegin, strTimeEnd);

	int nEventID =-1;//风险事件ID
	if(nRet == -1 || nRet == 0)
	{//没有风险，则判断之前是不是有风险，有风险，则是风险消失
		if(m_RiskProcess->HasLossContinueDaysRisk("",InvestorID, strTimeBegin, strTimeEnd))
			m_RiskProcess->AddLossContinueDaysEvent("",InvestorID, strTimeBegin, strTimeEnd, dblRisk, 0, nEventID, vecThisRisk);	
		CDataCenter::Get()->AddAccountForbid(InvestorID, RI_LOSS_CONTINUEDAYS, "", NULL, vecThisRisk, false);	
	}
	else if(nRet == 1)
	{//有风险				
		m_RiskProcess->AddLossContinueDaysEvent("",InvestorID, strTimeBegin, strTimeEnd,dblRisk, nLevel, nEventID, vecThisRisk);
		CDataCenter::Get()->AddAccountForbid(InvestorID, RI_LOSS_CONTINUEDAYS, "", NULL, vecThisRisk, true);	
	}	
}
int CRiskMsgCalc::GetLossContinueDays_Level(const string& InvestorID, std::string strTime,  double& dblRisk, SResponse& vecThisRisk, int& nLevel, std::string& strTimeBegin, std::string& strTimeEnd)
{
	AccountLossDay accountLossDay;
	BrokerAccountKey bakey;
	strcpy(bakey.BrokerID, "");
	strcpy(bakey.AccountID, InvestorID.c_str());
	bool bGet = CDataCenter::Get()->GetAccountLossDays(bakey, accountLossDay);
	if(bGet == false)
		return 0;

	dblRisk = accountLossDay.nlossdaycount;
	int nRet = GetRiskLevel(InvestorID, strTime, RI_LOSS_CONTINUEDAYS, dblRisk, vecThisRisk, nLevel, strTimeBegin, strTimeEnd);
	return nRet;
}
void CRiskMsgCalc::ActiveLossPercent(const string& InvestorID, std::string strTime)
{
	double	dblRisk = 0.0;
	SResponse vecThisRisk;
	int		nLevel = 0;
	std::string strTimeBegin, strTimeEnd;
	int nRet = GetLossPercent_Level(InvestorID, strTime, dblRisk,  vecThisRisk, nLevel, strTimeBegin, strTimeEnd);

	int nEventID =-1;//风险事件ID
	if(nRet == -1 || nRet == 0)
	{//没有风险，则判断之前是不是有风险，有风险，则是风险消失
		if(m_RiskProcess->HasLossPercentRisk("",InvestorID, strTimeBegin, strTimeEnd))
			m_RiskProcess->AddLossPercentEvent("",InvestorID, strTimeBegin, strTimeEnd, dblRisk, 0, nEventID, vecThisRisk);	
		CDataCenter::Get()->AddAccountForbid(InvestorID, RI_LOSS_PERCENT, "", NULL, vecThisRisk, false);	
	}
	else if(nRet == 1)
	{//有风险				
		m_RiskProcess->AddLossPercentEvent("",InvestorID, strTimeBegin, strTimeEnd,dblRisk, nLevel, nEventID, vecThisRisk);
		CDataCenter::Get()->AddAccountForbid(InvestorID, RI_LOSS_PERCENT, "", NULL, vecThisRisk, true);	
	}	
}
int CRiskMsgCalc::GetLossPercent_Level(const string& InvestorID, std::string strTime,  double& dblRisk, SResponse& vecThisRisk, int& nLevel, std::string& strTimeBegin, std::string& strTimeEnd)
{
	double  dbInit   = 0.0;

	if(CF_ERROR_SUCCESS != CInterface_SvrTradeData::getObj().GetUserInitFund(InvestorID, dbInit))
		return -1;//账户初始权益 （不知道这个是不是账户累计初始资金）
	if(dbInit>-0.00001 && dbInit < 0.00001)
		return -1;//初始总权益为0，则不判断这个风险指标

	PlatformStru_TradingAccountInfo outData;
	if(CF_ERROR_SUCCESS != CInterface_SvrTradeData::getObj().GetUserFundInfo(InvestorID, outData))
		return -1;	

	int nRet = GetLossPercentRiskLevel(InvestorID, strTime, RI_LOSS_PERCENT, outData.DynamicProfit, dbInit, vecThisRisk, nLevel, strTimeBegin, strTimeEnd);
	return nRet;
}
int CRiskMsgCalc::GetLossPercentRiskLevel(string InvestorID, std::string strTime, RiskIndicatorType RiskType, double dbdynamic, double& dbInit, SResponse& vecResponse,int& nLevel, std::string& strTimeBegin, std::string& strTimeEnd)
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
		int nRet = CDataCenter::Get()->GetLossPercentMapLevel(rKey, strTime, dbdynamic, dbInit, vecResponse, nLevel, strTimeBegin, strTimeEnd);
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
void CRiskMsgCalc::ActiveEventPassTime()
{//处理过时的风险事件，主要是风险现在是按照时间段来的
	std::map<RiskEventIDType,std::vector<RiskEvent>> ActiveEvent;
	CDataCenter::Get()->GetAllActiveEvent(ActiveEvent);

	char szTime[6];
	SYSTEMTIME st;
	GetLocalTime(&st);
	memset(szTime, 0, sizeof(szTime));
	sprintf(szTime,"%.2d:%.2d",st.wHour,st.wMinute);
	std::string strTime = szTime;
	std::map<RiskEventIDType,std::vector<RiskEvent>>::iterator it =  ActiveEvent.begin();
	for(; it != ActiveEvent.end(); it++)
	{
		std::vector<RiskEvent>& ss=it->second;
		if(ss.size() >0)
		{
			std::sort(ss.begin(),ss.end());
			RiskEvent& rE = ss[ss.size()-1];
			if(rE.cTimeBegin > szTime
				|| rE.cTimeEnd < szTime	)
			{	//在有效时间之外了				
				rE.nRiskLevelID = 0;
				m_RiskProcess->SendRiskEvent(rE);
			}
		}
	}

}