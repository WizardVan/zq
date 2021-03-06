#include "StdAfx.h"
#include "CDlgRealTimeFundInfoData.h"

#include "TcpLayer.h"
#include "CDataInfo.h"
#include "RiskManageCmd.h"
#include "UserApiStruct.h"
#include "CommonPkg.h"

bool Client::CDlgRealTimeFundInfoData::NewQuery()
{
#if 1
	int nReqID = 0;
	int nIndex = 0;

	std::set<int>::iterator it;
	int* pnNewIDs = NULL;

	LockObject();

	pnNewIDs = new int[setQueryAccountID.size()+1];
	memset(pnNewIDs, 0, (setQueryAccountID.size()+1)*sizeof(int));

	// 执行按帐号ID订阅
	for(it = setQueryAccountID.begin(); 
			it != setQueryAccountID.end(); it++) {
		pnNewIDs[nIndex] = *it;
		nIndex++;
		// 这里实际需要的是按帐号进行订阅退订
	}
	CTcpLayer::SendData(Cmd_RM_SubscribeFund_Req, (void*)pnNewIDs, nIndex*sizeof(int), 0);
	delete pnNewIDs;
	pnNewIDs = NULL;
	UnlockObject();
	return true;

#else

	int nReqID = 0;
	int nAccountID = 0;

	std::set<int>::iterator it;

	LockObject();

	// 执行按帐号ID订阅
	for(it = setQueryAccountID.begin(); 
			it != setQueryAccountID.end(); it++) {
		nAccountID = *it;
		// 这里实际需要的是按帐号进行订阅退订
		CTcpLayer::SendData(Cmd_RM_SubscribeFund_Req, (void*)&nAccountID, sizeof(nAccountID), 0);
	}
	UnlockObject();
	return true;
#endif
}

void Client::CDlgRealTimeFundInfoData::ResetQuery()
{
	 LockObject();
	 mapKey2Index.clear();
	 mapIndex2Key.clear();
	 mapPreFundInfo.clear();
	 mapLastFundInfo.clear();
	 UnlockObject();

	 std::set<int>::iterator it;
	 int nAccountID = 0;
	 
	 LockObject();
	 // 先对原来的订阅进行退订
	 for(it = setQueryAccountID.begin(); 
			it != setQueryAccountID.end(); it++) {
		 nAccountID = *it;
		 // 这里实际需要的是按帐号进行订阅退订
		 CTcpLayer::SendData(Cmd_RM_UnSubscribeFund_Req, (void*)&nAccountID, sizeof(nAccountID), 0);
	 }
	 setQueryAccountID.clear();
	 UnlockObject();
}

void Client::CDlgRealTimeFundInfoData::CheckNewData(CDataInfo* pdataInfo, 
								std::queue<RiskAllAccountField>& queueRet)
{
	if(pdataInfo==NULL)
		return;

	int nIndex = -1;
	char strTemp[1024];
	std::string strKey;
	strKey.reserve(1024);

	LockObject();
	while(!queueAllFundInfo.empty()) {
		RiskAllAccountField field = queueAllFundInfo.front();
		queueAllFundInfo.pop();
		if(bIsGroupQuery) {
			if(setFilterFinanProductID.size()>0) {
				TrustTradeAccount traderAccount;
				memset(&traderAccount, 0, sizeof(traderAccount));
				if(pdataInfo->GetTrustTradeAccount(std::string(field.cur.InvestorID), traderAccount)) {
					if(setFilterFinanProductID.find(traderAccount.nFinancialProductID)==setFilterFinanProductID.end())
						continue;
				}
			}
		}

		memset(strTemp, 0, sizeof(strTemp));
		sprintf(strTemp, "%s", field.cur.InvestorID);
		strKey = strTemp;
		mapPreFundInfo[strKey] = field.pre;
		mapLastFundInfo[strKey] = field.cur;

		queueRet.push(field);

	}
		
	UnlockObject();
}

void Client::CDlgRealTimeFundInfoData::CheckNewData(CDataInfo* pdataInfo, 
								std::queue<RiskSyncAccountField>& queueRet)
{
	if(pdataInfo==NULL)
		return;

	std::map<string, int>::iterator it;
	int nIndex = -1;
	char strTemp[1024];
	std::string strKey;
	strKey.reserve(1024);

	LockObject();
	while(!queueLastFundInfo.empty()) {
		RiskSyncAccountField field = queueLastFundInfo.front();
		queueLastFundInfo.pop();
		if(bIsGroupQuery) {
			if(setFilterFinanProductID.size()>0) {
				TrustTradeAccount traderAccount;
				memset(&traderAccount, 0, sizeof(traderAccount));
				if(pdataInfo->GetTrustTradeAccount(std::string(field.InvestorID), traderAccount)) {
					if(setFilterFinanProductID.find(traderAccount.nFinancialProductID)==setFilterFinanProductID.end())
						continue;
				}
			}
		}
		memset(strTemp, 0, sizeof(strTemp));
		sprintf(strTemp, "%s", field.InvestorID);
		strKey = strTemp;
		mapLastFundInfo[strKey] = field;

		queueRet.push(field);
	}
	UnlockObject();

}