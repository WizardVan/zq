#include "StdAfx.h"

#include <stdlib.h>
#include <string.h>
#include "AllOrdersPanel.h"
#include "Module-Misc2/GlobalFunc.h"
#include "../inc/Module-Misc/FieldValueTool.h"
#include "../inc/Module-Misc/zqControl.h"
#include "../ConfigApp/Const.h"
#include "../inc/Module-Misc/orderCommonFunc.h"
#include "../Module-Misc/LogDefine.h"
#include "../inc/Module-Misc/ZqMessageBox.h"
#include "../inc/Order-Common/CLocalOrderService.h"
#include "FileOpr.h"

//class IPlatformSvr;
extern PlatformSvrMgr* g_pPlatformMgr;

BEGIN_EVENT_TABLE(CAllOrdersPanel, wxPanel)
    EVT_SIZE( CAllOrdersPanel::OnSize )
    EVT_RADIOBUTTON(wxID_ANY,CAllOrdersPanel::OnRadioButton)
    EVT_COMMAND(wxID_ANY, wxEVT_ALL_ORDER_RspOrderAction1,  CAllOrdersPanel::OnRspOrderAction1)
    EVT_COMMAND(wxID_ANY, wxEVT_ALL_ORDER_RspOrderAction2,  CAllOrdersPanel::OnRspOrderAction2)
    EVT_COMMAND(wxID_ANY, wxEVT_RtnTrade,  CAllOrdersPanel::OnRcvTrade)
    EVT_COMMAND(wxID_ANY, wxEVT_RspQryOrderLast,  CAllOrdersPanel::OnRspQryOrder)
    EVT_COMMAND(wxID_ANY, wxEVT_RspQryCommissionRate,  CAllOrdersPanel::OnGetCommissionRate)
    EVT_COMMAND(wxID_ANY, wxEVT_RspQryMarginRate,  CAllOrdersPanel::OnGetMarginRate)
    EVT_COMMAND(wxID_ANY, wxEVT_RspQryDepthMarketData,  CAllOrdersPanel::OnRspQryDepthMarketData)
    EVT_COMMAND(wxID_ANY, wxEVT_RtnOrder,  CAllOrdersPanel::OnRtnOrder)
    EVT_COMMAND(wxID_ANY, wxEVT_ALL_ORDER_RspForQuoteInsert,  CAllOrdersPanel::OnRspForQuoteInsert)
    EVT_COMMAND(wxID_ANY, wxEVT_ALL_ORDER_ErrRtnForQuoteInsert,  CAllOrdersPanel::OnErrRtnForQuoteInsert)
    EVT_COMMAND(wxID_ANY, wxEVT_ALL_ORDER_RtnExecOrder,  CAllOrdersPanel::OnRtnExecOrder)
    EVT_COMMAND(wxID_ANY, wxEVT_ALL_ORDER_RspExecOrderInsert,  CAllOrdersPanel::OnRspExecOrderInsert)
    EVT_COMMAND(wxID_ANY, wxEVT_ALL_ORDER_ErrRtnExecOrderInsert,  CAllOrdersPanel::OnErrRtnExecOrderInsert)
    EVT_COMMAND(wxID_ANY, wxEVT_ALL_ORDER_RspExecOrderAction,  CAllOrdersPanel::OnRspExecOrderAction)
    EVT_COMMAND(wxID_ANY, wxEVT_ALL_ORDER_ErrRtnExecOrderAction,  CAllOrdersPanel::OnErrRtnExecOrderAction)

    EVT_PANEL_CHAR_HOOK(OnPanelCharHook)
    EVT_BUTTON(ID_BUTTON_REMOVE,CAllOrdersPanel::OnRemove)
    EVT_BUTTON(ID_BUTTON_REMOVEALL,CAllOrdersPanel::OnRemoveAll)
    EVT_BUTTON(ID_BUTTON_REQRY,CAllOrdersPanel::OnReQry)
    EVT_COMMAND(wxID_ANY,wxEVT_REQ_REQRY,CAllOrdersPanel::OnReConnectQry)
    EVT_LIST_EXT_ITEM_ACTIVATED(wxID_ANY, CAllOrdersPanel::OnActivated)
    EVT_CFG_CHANGE(OnCfgChanged)
    EVT_LIST_EXT_CONTEXT_MENU(CAllOrdersPanel::OnContext)
    EVT_COMMAND(wxID_ANY, wxEVT_REMOVE,  CAllOrdersPanel::OnRemove)
    EVT_COMMAND(wxID_ANY, wxEVT_REMOVEALL,  CAllOrdersPanel::OnRemoveAll)
    EVT_COMMAND(wxID_ANY, wxEVT_ORDER_INSERT_PANEL_NEW_ORDER,  CAllOrdersPanel::OnOrderInsert)
    EVT_PANEL_FOCUS(OnPanelFocus)
    EVT_LIST_EXT_COL_END_DRAG(wxID_ANY, OnColumnResize)
    EVT_SUBSCRIBE(OnSubscrible)
#ifdef _USE_MULTI_LANGUAGE
	EVT_COMMAND(wxID_ANY, wxEVT_LANGUAGE_CHANGE, OnLanguageChanged)
#endif
	EVT_COMMAND(wxID_ANY, wxEVT_MA_PLAT_ADD,OnMAPlatformAdd)
	EVT_COMMAND(wxID_ANY, wxEVT_MA_PLAT_DELETE, OnMAPlatformDelete)
END_EVENT_TABLE()

CAllOrdersPanel *s_pAllOrdersPanel = NULL;
CAllOrdersPanel *s_pOpenOrdersPanel = NULL;



//----------�����Ƕ����¼�֪ͨ----------

void CAllOrdersPanel::OnSubscrible(wxCommandEvent& evt)
{
    vector<IPlatformSingleSvr*> Svrs=g_pPlatformMgr->GetLogonPlatformSvr();

    for(int i=0;i<(int)Svrs.size();i++)
    {
	    if(evt.GetInt())
	    {
            if(m_nViewStyle==conAllOrderStyle)
            {
		        Svrs[i]->SubscribeBusinessData(BID_RspOrderInsert, m_nGID, OrderInsertBackFunc1);	//���ı���¼���ִ
		        Svrs[i]->SubscribeBusinessData(BID_ErrRtnOrderInsert, m_nGID, OrderInsertBackFunc2);	//���ı���¼���ִ
		        Svrs[i]->SubscribeBusinessData(BID_RspOrderAction, m_nGID, RspOrderAction1CallBackFunc);
		        Svrs[i]->SubscribeBusinessData(BID_ErrRtnOrderAction, m_nGID, RspOrderAction2CallBackFunc);
		        Svrs[i]->SubscribeBusinessData(BID_RtnTrade, m_nGID, TradeCallBackFunc);	//���ĳɽ��ر�
		        Svrs[i]->SubscribeBusinessData(BID_RspForQuoteInsert, m_nGID, RspForQuoteInsertCallBackFunc);	//���ĳɽ��ر�
		        Svrs[i]->SubscribeBusinessData(BID_ErrRtnForQuoteInsert, m_nGID, ErrRtnForQuoteInsertCallBackFunc);	//���ĳɽ��ر�
		        Svrs[i]->SubscribeBusinessData(BID_RtnExecOrder, m_nGID, RtnExecOrderCallBackFunc);	//���ĳɽ��ر�
		        Svrs[i]->SubscribeBusinessData(BID_RspExecOrderInsert, m_nGID, RspExecOrderInsertCallBackFunc);	//���ĳɽ��ر�
		        Svrs[i]->SubscribeBusinessData(BID_ErrRtnExecOrderInsert, m_nGID, ErrRtnExecOrderInsertCallBackFunc);	//���ĳɽ��ر�
				//Svrs[i]->SubscribeBusinessData(BID_RspExecOrderAction, m_nGID, RspExecOrderCallBackFunc);
				//Svrs[i]->SubscribeBusinessData(BID_ErrRtnExecOrderAction, m_nGID, RspExecOrderCallBackFunc);
            }
            if(m_nViewStyle==conAllOrderStyle||m_nViewStyle==conOpenOrderStyle)
            {
                Svrs[i]->SubscribeBusinessData(BID_RspQryOrder, m_nGID, RspQryOrderCallBackFunc);
		        Svrs[i]->SubscribeBusinessData(BID_RspQryInstrumentCommissionRate, m_nGID, GetCommissionRateCallBackFunc);
		        Svrs[i]->SubscribeBusinessData(BID_RspQryInstrumentMarginRate, m_nGID, GetMarginRateCallBackFunc);
		        Svrs[i]->SubscribeBusinessData(BID_RspQryDepthMarketData, m_nGID, QryDepthMarketDataCallBackFunc);
		        Svrs[i]->SubscribeBusinessData(BID_RtnOrder, m_nGID, RtnOrderCallBackFunc);	//���ı����ر�
            }
	    }
	    else
	    {
            if(m_nViewStyle==conAllOrderStyle)
            {
			    Svrs[i]->UnSubscribeBusinessData(BID_RspOrderInsert, m_nGID);	//���ı���¼���ִ
			    Svrs[i]->UnSubscribeBusinessData(BID_ErrRtnOrderInsert, m_nGID);	//���ı���¼���ִ
			    Svrs[i]->UnSubscribeBusinessData(BID_RspOrderAction, m_nGID);
			    Svrs[i]->UnSubscribeBusinessData(BID_ErrRtnOrderAction, m_nGID);
			    Svrs[i]->UnSubscribeBusinessData(BID_RtnTrade, m_nGID);	//���ĳɽ��ر�
 			    Svrs[i]->UnSubscribeBusinessData(BID_RspForQuoteInsert, m_nGID);	//���ĳɽ��ر�
			    Svrs[i]->UnSubscribeBusinessData(BID_ErrRtnForQuoteInsert, m_nGID);	//���ĳɽ��ر�
 			    Svrs[i]->UnSubscribeBusinessData(BID_RtnExecOrder, m_nGID);	//���ĳɽ��ر�
			    Svrs[i]->UnSubscribeBusinessData(BID_RspExecOrderInsert, m_nGID);	//���ĳɽ��ر�
			    Svrs[i]->UnSubscribeBusinessData(BID_ErrRtnExecOrderInsert, m_nGID);	//���ĳɽ��ر�
			    //Svrs[i]->UnSubscribeBusinessData(BID_RspExecOrderAction, m_nGID);	//���ĳɽ��ر�
			    //Svrs[i]->UnSubscribeBusinessData(BID_ErrRtnExecOrderAction, m_nGID);	//���ĳɽ��ر�
           }
            if(m_nViewStyle==conAllOrderStyle||m_nViewStyle==conOpenOrderStyle)
            {
			    Svrs[i]->UnSubscribeBusinessData(BID_RspQryOrder, m_nGID);
			    Svrs[i]->UnSubscribeBusinessData(BID_RspQryInstrumentCommissionRate, m_nGID);
			    Svrs[i]->UnSubscribeBusinessData(BID_RspQryInstrumentMarginRate, m_nGID);
			    Svrs[i]->SubscribeBusinessData(BID_RspQryDepthMarketData, m_nGID, QryDepthMarketDataCallBackFunc);
			    Svrs[i]->UnSubscribeBusinessData(BID_RtnOrder, m_nGID);	//�����ر�(����¼���뱨��������������ûر�)	
            }
	    }
    }
}

//----------�������¼��ص�����----------

//������ѯ�ر�
int CAllOrdersPanel::RspQryOrderCallBackFunc(const GUIModuleID GID,const AbstractBusinessData& data)
{
	if(data.BID!=BID_RspQryOrder || data.Size!=sizeof(DataRspQryOrder))
		return 0;	

	DataRspQryOrder& refData = *(DataRspQryOrder*)(const_cast<AbstractBusinessData*>(&data));	
	if(refData.bIsLast)
	{
        wxCommandEvent evt(wxEVT_RspQryOrderLast, wxID_ANY);
        //s_pAllOrdersPanel->AddPendingEvent(evt);
	    switch(GID) 
	    { 
		    case GID_ALL_ORDER:
                if(s_pAllOrdersPanel) s_pAllOrdersPanel->AddPendingEvent(evt);
                break;
		    case GID_OPEN_ORDER: 
                if(s_pOpenOrdersPanel) s_pOpenOrdersPanel->AddPendingEvent(evt);
			    break;
	    };
	}
	return 0;
}

//�������ʻر�
int CAllOrdersPanel::GetCommissionRateCallBackFunc(const GUIModuleID GID,const AbstractBusinessData &data)	  
{ 
	if(data.BID!=BID_RspQryInstrumentCommissionRate || data.Size!=sizeof(DataRspQryInstrumentCommissionRate))
		return 0;	

    DWORD EvtParamID;
    CFTEventParam::CreateEventParam(EvtParamID,NULL,NULL,&data,sizeof(DataRspQryInstrumentCommissionRate),0);

    wxCommandEvent evt(wxEVT_RspQryCommissionRate, wxID_ANY);
    evt.SetInt((int)EvtParamID);
    //s_pAllOrdersPanel->AddPendingEvent(evt);
    switch(GID) 
    { 
	    case GID_ALL_ORDER:
            if(s_pAllOrdersPanel) s_pAllOrdersPanel->AddPendingEvent(evt);
            break;
	    case GID_OPEN_ORDER: 
            if(s_pOpenOrdersPanel) s_pOpenOrdersPanel->AddPendingEvent(evt);
		    break;
    };
	return 0;
}



//��֤���ʲ�ѯ�ر�
int CAllOrdersPanel::GetMarginRateCallBackFunc(const GUIModuleID GID,const AbstractBusinessData &data)	  
{ 
	if(data.BID!=BID_RspQryInstrumentMarginRate || data.Size!=sizeof(DataRspQryInstrumentMarginRate))
		return 0;	

    DWORD EvtParamID;
    CFTEventParam::CreateEventParam(EvtParamID,NULL,NULL,&data,sizeof(DataRspQryInstrumentMarginRate),0);

    wxCommandEvent evt(wxEVT_RspQryMarginRate, wxID_ANY);
    evt.SetInt((int)EvtParamID);
    //s_pAllOrdersPanel->AddPendingEvent(evt);
    switch(GID) 
    { 
	    case GID_ALL_ORDER:
            if(s_pAllOrdersPanel) s_pAllOrdersPanel->AddPendingEvent(evt);
            break;
	    case GID_OPEN_ORDER: 
            if(s_pOpenOrdersPanel) s_pOpenOrdersPanel->AddPendingEvent(evt);
		    break;
    };
	return 0;
}
//��ϱ�����ѯ����ر�
int CAllOrdersPanel::QryDepthMarketDataCallBackFunc(const GUIModuleID GID,const AbstractBusinessData &data)	  
{ 
	if(data.BID!=BID_RspQryDepthMarketData || data.Size!=sizeof(DataRspQryDepthMarketData))
		return 0;	

    DWORD EvtParamID;
    CFTEventParam::CreateEventParam(EvtParamID,NULL,NULL,&data,sizeof(DataRspQryDepthMarketData),0);

    wxCommandEvent evt(wxEVT_RspQryDepthMarketData, wxID_ANY);
    evt.SetInt((int)EvtParamID);
    switch(GID) 
    { 
	    case GID_ALL_ORDER:
            if(s_pAllOrdersPanel) s_pAllOrdersPanel->AddPendingEvent(evt);
            break;
	    case GID_OPEN_ORDER: 
            if(s_pOpenOrdersPanel) s_pOpenOrdersPanel->AddPendingEvent(evt);
		    break;
    };
	return 0;
}

//�����ر�
int CAllOrdersPanel::RtnOrderCallBackFunc(const GUIModuleID GID,const AbstractBusinessData &data)	  
{ 
	if(data.BID!=BID_RtnOrder || data.Size!=sizeof(DataRtnOrder))
		return 0;	

    DWORD EvtParamID;
    CFTEventParam::CreateEventParam(EvtParamID,NULL,NULL,&data,sizeof(DataRtnOrder),0);

    wxCommandEvent evt(wxEVT_RtnOrder, wxID_ANY);
    evt.SetInt((int)EvtParamID);
    //s_pAllOrdersPanel->AddPendingEvent(evt);
    switch(GID) 
    { 
	    case GID_ALL_ORDER:
            if(s_pAllOrdersPanel) s_pAllOrdersPanel->AddPendingEvent(evt);
            break;
	    case GID_OPEN_ORDER: 
            if(s_pOpenOrdersPanel) s_pOpenOrdersPanel->AddPendingEvent(evt);
		    break;
    };
    return 0;
}

//�ɽ��ر�
int CAllOrdersPanel::TradeCallBackFunc(const GUIModuleID GID,const AbstractBusinessData &data)	  
{
	if(data.BID!=BID_RtnTrade || data.Size!=sizeof(DataRtnTrade))
		return 0;	

    DWORD EvtParamID;
    CFTEventParam::CreateEventParam(EvtParamID,NULL,NULL,&data,sizeof(DataRtnTrade),0);

    wxCommandEvent evt(wxEVT_RtnTrade, wxID_ANY);
    evt.SetInt((int)EvtParamID);
    //s_pAllOrdersPanel->AddPendingEvent(evt);
    switch(GID) 
    { 
	    case GID_ALL_ORDER:
             if(s_pAllOrdersPanel) s_pAllOrdersPanel->AddPendingEvent(evt);
            break;
	    case GID_OPEN_ORDER: 
            if(s_pOpenOrdersPanel) s_pOpenOrdersPanel->AddPendingEvent(evt);
		    break;
    };
    return 0;
}

//����¼��ر�
int CAllOrdersPanel::OrderInsertBackFunc1(const GUIModuleID GID,const AbstractBusinessData &data)	  
{
	if(data.BID!=BID_RspOrderInsert || data.Size!=sizeof(DataRspOrderInsert))
		return 0;	

    if(((DataRspOrderInsert*)&data)->nRequestID<0)
        return 0;

    DWORD EvtParamID;
    CFTEventParam::CreateEventParam(EvtParamID,NULL,NULL,&data,sizeof(DataRspOrderInsert),0);

    wxCommandEvent evt(wxEVT_ORDER_INSERT_PANEL_NEW_ORDER, wxID_ANY);
    evt.SetInt((int)EvtParamID);
    //s_pAllOrdersPanel->AddPendingEvent(evt);
    switch(GID) 
    { 
	    case GID_ALL_ORDER:
            if(s_pAllOrdersPanel) s_pAllOrdersPanel->AddPendingEvent(evt);
            break;
	    case GID_OPEN_ORDER: 
            if(s_pOpenOrdersPanel) s_pOpenOrdersPanel->AddPendingEvent(evt);
		    break;
    };
    return 0;
}
//����¼��ر�
int CAllOrdersPanel::OrderInsertBackFunc2(const GUIModuleID GID,const AbstractBusinessData &data)	  
{
	if(data.BID!=BID_ErrRtnOrderInsert || data.Size!=sizeof(DataErrRtnOrderInsert))
		return 0;	

	if(((DataErrRtnOrderInsert*)&data)->nRequestID<0)
		return 0;

	DWORD EvtParamID;
	CFTEventParam::CreateEventParam(EvtParamID,NULL,NULL,&data,sizeof(DataErrRtnOrderInsert),0);

	wxCommandEvent evt(wxEVT_ORDER_INSERT_PANEL_NEW_ORDER, wxID_ANY);
	evt.SetInt((int)EvtParamID);
	//s_pAllOrdersPanel->AddPendingEvent(evt);
	switch(GID) 
	{ 
	case GID_ALL_ORDER:
		if(s_pAllOrdersPanel) s_pAllOrdersPanel->AddPendingEvent(evt);
		break;
	case GID_OPEN_ORDER: 
		if(s_pOpenOrdersPanel) s_pOpenOrdersPanel->AddPendingEvent(evt);
		break;
	};
	return 0;
}
//������ִ1(�����ִ1���ۺϽ���ƽ̨��Ϊ������Ч)
int CAllOrdersPanel::RspOrderAction1CallBackFunc(const GUIModuleID GID,const AbstractBusinessData &data)	  
{ 
	if(data.BID!=BID_RspOrderAction || data.Size!=sizeof(DataRspOrderAction))
		return 0;	

    wxCommandEvent evt(wxEVT_ALL_ORDER_RspOrderAction1, wxID_ANY);
    DWORD EvtParamID;
    CFTEventParam::CreateEventParam(EvtParamID,NULL,NULL,&data,sizeof(DataRspOrderAction),0);
    evt.SetInt((int)EvtParamID);
    //s_pAllOrdersPanel->AddPendingEvent(evt);
    switch(GID) 
    { 
	    case GID_ALL_ORDER:
            if(s_pAllOrdersPanel) s_pAllOrdersPanel->AddPendingEvent(evt);
            break;
	    case GID_OPEN_ORDER: 
            if(s_pOpenOrdersPanel) s_pOpenOrdersPanel->AddPendingEvent(evt);
		    break;
    };
    return 0;
}


//������ִ2(�����ִ2����������Ϊ������Ч)
int CAllOrdersPanel::RspOrderAction2CallBackFunc(const GUIModuleID GID,const AbstractBusinessData &data)	  
{ 
	if(data.BID!=BID_ErrRtnOrderAction || data.Size!=sizeof(DataErrRtnOrderAction))
		return 0;	

    wxCommandEvent evt(wxEVT_ALL_ORDER_RspOrderAction2, wxID_ANY);
    DWORD EvtParamID;
    CFTEventParam::CreateEventParam(EvtParamID,NULL,NULL,&data,sizeof(DataErrRtnOrderAction),0);
    evt.SetInt((int)EvtParamID);
    //s_pAllOrdersPanel->AddPendingEvent(evt);
    switch(GID) 
    { 
	    case GID_ALL_ORDER:
            if(s_pAllOrdersPanel) s_pAllOrdersPanel->AddPendingEvent(evt);
            break;
	    case GID_OPEN_ORDER: 
            if(s_pOpenOrdersPanel) s_pOpenOrdersPanel->AddPendingEvent(evt);
		    break;
    };
    return 0;
}

// ��Ȩѯ�ۻر�
int CAllOrdersPanel::RspForQuoteInsertCallBackFunc(const GUIModuleID GID,const AbstractBusinessData& data)
{
	if(data.BID!=BID_RspForQuoteInsert || data.Size!=sizeof(DataRspForQuoteInsert))
		return 0;	

    wxCommandEvent evt(wxEVT_ALL_ORDER_RspForQuoteInsert, wxID_ANY);
    DWORD EvtParamID;
    CFTEventParam::CreateEventParam(EvtParamID,NULL,NULL,&data,sizeof(DataRspForQuoteInsert),0);
    evt.SetInt((int)EvtParamID);
    //s_pAllOrdersPanel->AddPendingEvent(evt);
    switch(GID) 
    { 
	    case GID_ALL_ORDER:
            if(s_pAllOrdersPanel) s_pAllOrdersPanel->AddPendingEvent(evt);
            break;
    };
    return 0;
}

// ��Ȩѯ�۴���ر�
int CAllOrdersPanel::ErrRtnForQuoteInsertCallBackFunc(const GUIModuleID GID,const AbstractBusinessData& data)
{
	if(data.BID!=BID_ErrRtnForQuoteInsert || data.Size!=sizeof(DataErrRtnForQuoteInsert))
		return 0;	

    wxCommandEvent evt(wxEVT_ALL_ORDER_ErrRtnForQuoteInsert, wxID_ANY);
    DWORD EvtParamID;
    CFTEventParam::CreateEventParam(EvtParamID,NULL,NULL,&data,sizeof(DataErrRtnForQuoteInsert),0);
    evt.SetInt((int)EvtParamID);
    //s_pAllOrdersPanel->AddPendingEvent(evt);
    switch(GID) 
    { 
	    case GID_ALL_ORDER:
            if(s_pAllOrdersPanel) s_pAllOrdersPanel->AddPendingEvent(evt);
            break;
    };
    return 0;
}

// ִ������֪ͨ
int CAllOrdersPanel::RtnExecOrderCallBackFunc(const GUIModuleID GID,const AbstractBusinessData& data)
{
	if(data.BID!=BID_RtnExecOrder || data.Size!=sizeof(DataRtnExecOrder))
		return 0;	

    wxCommandEvent evt(wxEVT_ALL_ORDER_RtnExecOrder, wxID_ANY);
    DWORD EvtParamID;
    CFTEventParam::CreateEventParam(EvtParamID,NULL,NULL,&data,sizeof(DataRtnExecOrder),0);
    evt.SetInt((int)EvtParamID);
    //s_pAllOrdersPanel->AddPendingEvent(evt);
    switch(GID) 
    { 
	    case GID_ALL_ORDER:
            if(s_pAllOrdersPanel) s_pAllOrdersPanel->AddPendingEvent(evt);
            break;
    };
    return 0;
}

// ִ��������Ӧ
int CAllOrdersPanel::RspExecOrderInsertCallBackFunc(const GUIModuleID GID,const AbstractBusinessData& data)
{
	if(data.BID!=BID_RspExecOrderInsert || data.Size!=sizeof(DataRspExecOrderInsert))
		return 0;	

    wxCommandEvent evt(wxEVT_ALL_ORDER_RspExecOrderInsert, wxID_ANY);
    DWORD EvtParamID;
    CFTEventParam::CreateEventParam(EvtParamID,NULL,NULL,&data,sizeof(DataRspExecOrderInsert),0);
    evt.SetInt((int)EvtParamID);
    //s_pAllOrdersPanel->AddPendingEvent(evt);
    switch(GID) 
    { 
	    case GID_ALL_ORDER:
            if(s_pAllOrdersPanel) s_pAllOrdersPanel->AddPendingEvent(evt);
            break;
    };
    return 0;
}

// ִ���������ر�
int CAllOrdersPanel::ErrRtnExecOrderInsertCallBackFunc(const GUIModuleID GID,const AbstractBusinessData& data)
{
	if(data.BID!=BID_ErrRtnExecOrderInsert || data.Size!=sizeof(DataErrRtnExecOrderInsert))
		return 0;	

    wxCommandEvent evt(wxEVT_ALL_ORDER_ErrRtnExecOrderInsert, wxID_ANY);
    DWORD EvtParamID;
    CFTEventParam::CreateEventParam(EvtParamID,NULL,NULL,&data,sizeof(DataErrRtnExecOrderInsert),0);
    evt.SetInt((int)EvtParamID);
    //s_pAllOrdersPanel->AddPendingEvent(evt);
    switch(GID) 
    { 
	    case GID_ALL_ORDER:
            if(s_pAllOrdersPanel) s_pAllOrdersPanel->AddPendingEvent(evt);
            break;
    };
    return 0;
}

// ִ��������Ӧ
int CAllOrdersPanel::RspExecOrderActionCallBackFunc(const GUIModuleID GID,const AbstractBusinessData& data)
{
	if(data.BID!=BID_RspExecOrderAction || data.Size!=sizeof(DataRspExecOrderAction))
		return 0;	

    wxCommandEvent evt(wxEVT_ALL_ORDER_RspExecOrderAction, wxID_ANY);
    DWORD EvtParamID;
    CFTEventParam::CreateEventParam(EvtParamID,NULL,NULL,&data,sizeof(DataRspExecOrderAction),0);
    evt.SetInt((int)EvtParamID);
    //s_pAllOrdersPanel->AddPendingEvent(evt);
    switch(GID) 
    { 
	    case GID_ALL_ORDER:
            if(s_pAllOrdersPanel) s_pAllOrdersPanel->AddPendingEvent(evt);
            break;
    };
    return 0;
}

// ִ���������ر�
int CAllOrdersPanel::ErrRtnExecOrderActionCallBackFunc(const GUIModuleID GID,const AbstractBusinessData& data)
{
	if(data.BID!=BID_ErrRtnExecOrderAction || data.Size!=sizeof(DataErrRtnExecOrderAction))
		return 0;	

    wxCommandEvent evt(wxEVT_ALL_ORDER_ErrRtnExecOrderAction, wxID_ANY);
    DWORD EvtParamID;
    CFTEventParam::CreateEventParam(EvtParamID,NULL,NULL,&data,sizeof(DataErrRtnExecOrderAction),0);
    evt.SetInt((int)EvtParamID);
    //s_pAllOrdersPanel->AddPendingEvent(evt);
    switch(GID) 
    { 
	    case GID_ALL_ORDER:
            if(s_pAllOrdersPanel) s_pAllOrdersPanel->AddPendingEvent(evt);
            break;
    };
    return 0;
}


//----------�������ڲ��¼���Ӧ���������������߳���ִ��----------

//������ѯ��Ӧ
void CAllOrdersPanel::OnRspQryOrder(wxCommandEvent& evt)
{
    ShowAll();
}

//���ĳЩ��Լ�ı�֤���ʣ���Ҫ������Ӧ�ı����Ķ�����ֵ
void CAllOrdersPanel::OnGetCommissionRate(wxCommandEvent& evt)
{
    DataRspQryInstrumentCommissionRate CommissionRate;
    DWORD EvtParamID=(DWORD)evt.GetInt();
    if(CFTEventParam::GetEventParam(EvtParamID,NULL,NULL,&CommissionRate,NULL,sizeof(CommissionRate),NULL))
        CFTEventParam::DeleteEventParam(EvtParamID);
    else return;
    if(CommissionRate.RspInfoField.ErrorID!=0)
        return;

    IPlatformSingleSvr* pSvr=g_pPlatformMgr->GetPlatformSvr(CommissionRate.Head.PlatformID);
    if(!pSvr) return;

    std::set<std::string> setInstruments;
    std::set<std::string>::iterator it;
    std::vector<PlatformStru_OrderInfo> vecOrders;
	int iret;

    //��ʱ���Ǻ�ԼID,��ʱ����Ʒ�ִ���
    std::string strInstru(CommissionRate.InstrumentCommissionRateField.InstrumentID);
    if(strInstru.empty())
        return;

	iret=pSvr->GetInstrumentListByProductID(strInstru,setInstruments);

	if(iret>0)
	{//��Ʒ�ִ���
        for(it=setInstruments.begin();it!=setInstruments.end();it++)
        {
            vecOrders.clear();
            if(m_ShowType==ALLORDER) pSvr->GetTriggerOrders2(*it,vecOrders);
            else if(m_ShowType==OPENORDER) pSvr->GetWaitOrders2(*it,vecOrders);
            else if(m_ShowType==TRADEDORDER) pSvr->GetTradedOrders2(*it,vecOrders);
            else if(m_ShowType==CANCELORDER) pSvr->GetCanceledOrders2(*it,vecOrders);

            if(!vecOrders.empty())
                m_pwxExtListCtrl->UpdateInstrumentItems3(vecOrders);
        }
	}
	else
	{//�Ǻ�ԼID
       vecOrders.clear();
        if(m_ShowType==ALLORDER) pSvr->GetTriggerOrders2(strInstru,vecOrders);
        else if(m_ShowType==OPENORDER) pSvr->GetWaitOrders2(strInstru,vecOrders);
        else if(m_ShowType==TRADEDORDER) pSvr->GetTradedOrders2(strInstru,vecOrders);
        else if(m_ShowType==CANCELORDER) pSvr->GetCanceledOrders2(strInstru,vecOrders);

        if(!vecOrders.empty())
            m_pwxExtListCtrl->UpdateInstrumentItems3(vecOrders);
	}
}

//���ĳЩ��Լ�ı�֤���ʣ���Ҫ������Ӧ�ı����Ķ�����ֵ
void CAllOrdersPanel::OnGetMarginRate(wxCommandEvent& evt)
{
    DataRspQryInstrumentMarginRate MarginRate;
    DWORD EvtParamID=(DWORD)evt.GetInt();
    if(CFTEventParam::GetEventParam(EvtParamID,NULL,NULL,&MarginRate,NULL,sizeof(MarginRate),NULL))
        CFTEventParam::DeleteEventParam(EvtParamID);
    else return;
    if(MarginRate.RspInfoField.ErrorID!=0)
        return;

    IPlatformSingleSvr* pSvr=g_pPlatformMgr->GetPlatformSvr(MarginRate.Head.PlatformID);
    if(!pSvr) return;

    std::vector<PlatformStru_OrderInfo> vecOrders;
    std::string strInstru(MarginRate.InstrumentMarginRateField.InstrumentID);
    if(strInstru.empty())
        return;

    if(m_ShowType==ALLORDER) 
        pSvr->GetTriggerOrders2(strInstru,vecOrders);
    else if(m_ShowType==OPENORDER) 
        pSvr->GetWaitOrders2(strInstru,vecOrders);
    else if(m_ShowType==TRADEDORDER) 
        pSvr->GetTradedOrders2(strInstru,vecOrders);
    else if(m_ShowType==CANCELORDER) 
        pSvr->GetCanceledOrders2(strInstru,vecOrders);

    if(!vecOrders.empty())
        m_pwxExtListCtrl->UpdateInstrumentItems3(vecOrders);
}
//��ϱ������㶳��ʱ��Ҫ����������ۣ���Ҫ��ѯ���˴����������ѯ���
void CAllOrdersPanel::OnRspQryDepthMarketData(wxCommandEvent& evt)
{
    DataRspQryDepthMarketData QuotData;
    DWORD EvtParamID=(DWORD)evt.GetInt();
    if(CFTEventParam::GetEventParam(EvtParamID,NULL,NULL,&QuotData,NULL,sizeof(QuotData),NULL))
        CFTEventParam::DeleteEventParam(EvtParamID);
    else return;
    if(QuotData.RspInfoField.ErrorID!=0)
        return;

    IPlatformSingleSvr* pSvr=g_pPlatformMgr->GetPlatformSvr(QuotData.Head.PlatformID);
    if(!pSvr) return;

    std::vector<PlatformStru_OrderInfo> vecOrders;
    std::string strInstru(QuotData.DepthMarketDataField.InstrumentID);
    if(strInstru.empty())
        return;

    if(m_ShowType==ALLORDER) 
        pSvr->GetTriggerOrders2(strInstru,vecOrders);
    else if(m_ShowType==OPENORDER) 
        pSvr->GetWaitOrders2(strInstru,vecOrders);
    else if(m_ShowType==TRADEDORDER) 
        pSvr->GetTradedOrders2(strInstru,vecOrders);
    else if(m_ShowType==CANCELORDER) 
        pSvr->GetCanceledOrders2(strInstru,vecOrders);

    if(!vecOrders.empty())
        m_pwxExtListCtrl->UpdateInstrumentItems3(vecOrders);

    char buf[1024];
    sprintf(buf,"OnRspQryDepthMarketData: ");
    for(int i=0;i<(int)vecOrders.size()&&(int)strlen(buf)<(int)sizeof(buf)-100;i++)
        sprintf(buf+strlen(buf),"%s/%s/%g/%g  ",vecOrders[i].InstrumentID,vecOrders[i].OrderSysID,vecOrders[i].freezeMargin,vecOrders[i].troubleMoney);
    CFileOpr::getObj().writelocallog("��������UI",buf);
}

//�����ر�
void CAllOrdersPanel::OnRtnOrder(wxCommandEvent& evt)
{
    DataRtnOrder RtnOrder;
    DWORD EvtParamID=(DWORD)evt.GetInt();
    if(CFTEventParam::GetEventParam(EvtParamID,NULL,NULL,&RtnOrder,NULL,sizeof(RtnOrder),NULL))
        CFTEventParam::DeleteEventParam(EvtParamID);
    else return;

    PlatformStru_OrderInfo& rawData= RtnOrder.OrderField;
    PlatformStru_OrderInfo OrderInfo;

    IPlatformSingleSvr* pSvr=g_pPlatformMgr->GetPlatformSvr(RtnOrder.Head.PlatformID);
    if(!pSvr) return;

    bool bvalue=false;
    OrderKey tmpOrderKey(rawData);

    if(m_nViewStyle==conOpenOrderStyle) 
        bvalue=pSvr->GetWaitOrder(tmpOrderKey,OrderInfo);
    else if(m_nViewStyle==conAllOrderStyle)
    {
        if(m_ShowType==ALLORDER)
            bvalue=pSvr->GetTriggerOrder(tmpOrderKey,OrderInfo);
        else if(m_ShowType==OPENORDER)
            bvalue=pSvr->GetWaitOrder(tmpOrderKey,OrderInfo);
        else if(m_ShowType==TRADEDORDER)
            bvalue=pSvr->GetTradedOrder(tmpOrderKey,OrderInfo);
        else if(m_ShowType==CANCELORDER)
            bvalue=pSvr->GetCanceledOrder(tmpOrderKey,OrderInfo);
    }

    if(bvalue)
        m_pwxExtListCtrl->UpdateOneItem(OrderInfo);
    else
        m_pwxExtListCtrl->DeleteOneItem(tmpOrderKey);

    if(m_nViewStyle==conAllOrderStyle)
        PopupCancel_InsertDlg(pSvr,rawData);

    char buf[1024];
    sprintf(buf,"OnRtnOrder: bvalue=%d InstrumentID=%s OrderSysID=%s freezeMargin=%g freezeCommission=%g",bvalue,OrderInfo.InstrumentID,OrderInfo.OrderSysID,OrderInfo.freezeMargin,OrderInfo.troubleMoney);
    CFileOpr::getObj().writelocallog("��������UI",buf);
}

void CAllOrdersPanel::OnRcvTrade(wxCommandEvent& evt)
{
    DataRtnTrade RtnTrade;
    DWORD EvtParamID=(DWORD)evt.GetInt();
    if(CFTEventParam::GetEventParam(EvtParamID,NULL,NULL,&RtnTrade,NULL,sizeof(RtnTrade),NULL))
        CFTEventParam::DeleteEventParam(EvtParamID);
    else return;

    IPlatformSingleSvr* pSvr=g_pPlatformMgr->GetPlatformSvr(RtnTrade.Head.PlatformID);
    if(!pSvr) return;

    if(m_nViewStyle==conAllOrderStyle) 
    {
        if(NeedOrderTradeConfirm())
            PopupTradeDlg( pSvr,RtnTrade.TradeField );
		if(NeedOrderTradeSound())
			NoticeSound(conNoticeTrader);
    }
}

void CAllOrdersPanel::OnOrderInsert(wxCommandEvent& evt)
{
    DataRspOrderInsert RspOrderInsert;
    DWORD EvtParamID=(DWORD)evt.GetInt();
    if(CFTEventParam::GetEventParam(EvtParamID,NULL,NULL,&RspOrderInsert,NULL,sizeof(RspOrderInsert),NULL))
        CFTEventParam::DeleteEventParam(EvtParamID);
    else return;

	if(m_nViewStyle==conAllOrderStyle) {
		if(!(RspOrderInsert.RspInfoField.ErrorID==53 
				&& RspOrderInsert.InputOrderField.ContingentCondition>=
				THOST_FTDC_CC_LastPriceGreaterThanStopPrice))
		{
			ShowInsertDlg("OIP_ERROR", RspOrderInsert, NeedOrderFailConfirm());
			if(NeedOrderFailSound())
				NoticeSound(conNoticeNewOrderFail);
		}
	}
}

void CAllOrdersPanel::OnRspOrderAction1(wxCommandEvent& evt)
{
    DataRspOrderAction RspOrderAction1;
    DWORD EvtParamID=(DWORD)evt.GetInt();
    if(CFTEventParam::GetEventParam(EvtParamID,NULL,NULL,&RspOrderAction1,NULL,sizeof(RspOrderAction1),NULL))
        CFTEventParam::DeleteEventParam(EvtParamID);
    else return;

    IPlatformSingleSvr* pSvr=g_pPlatformMgr->GetPlatformSvr(RspOrderAction1.Head.PlatformID);
    if(!pSvr) return;

    if(m_nViewStyle==conAllOrderStyle) 
    {
	    m_pTradeInfoDlg=TRADEINFODLG(this);
	    if(m_pTradeInfoDlg)
	    {
	        wxString info;
	        info.reserve(512);
	        PlatformStru_InstrumentInfo SimpleInstrumentInfo;
	        memset(&SimpleInstrumentInfo,0,sizeof(SimpleInstrumentInfo));
	        pSvr->GetInstrumentInfo(RspOrderAction1.InputOrderActionField.InstrumentID,SimpleInstrumentInfo);

			OrderKey orderKey(RspOrderAction1.InputOrderActionField.InvestorID, 
						RspOrderAction1.InputOrderActionField.InstrumentID, 
						RspOrderAction1.InputOrderActionField.FrontID, 
						RspOrderAction1.InputOrderActionField.SessionID, 
						wxString(RspOrderAction1.InputOrderActionField.OrderRef).Trim(false).c_str());
			PlatformStru_OrderInfo rawData;
			pSvr->GetOrder(orderKey, rawData);
			
			std::string strStatusMsg=RspOrderAction1.RspInfoField.ErrorMsg;
			int valStatusMsg=CFieldValueTool::String2OrderStatusMsg(strStatusMsg);
			if(valStatusMsg>0)  
				strStatusMsg=CFieldValueTool::OrderStatusMsg2String(valStatusMsg,SVR_LANGUAGE);
			info.Printf(LOADFORMATSTRING(CANCEL_ORDER_FORMAT,"%s%s%s%s%s%s%s"),//�ı��������%s,%s,%s,%s,%s,%s,%s
				RspOrderAction1.InputOrderActionField.InstrumentID,
				CFieldValueTool::Direction2String(rawData.Direction,SVR_LANGUAGE).c_str(),
				CFieldValueTool::OffsetFlag2String(rawData.CombOffsetFlag[0],SVR_LANGUAGE).c_str(),
				Number2String(rawData.VolumeTotalOriginal).c_str(),
				CFieldValueTool::OrderPriceType2String(rawData.OrderPriceType,SVR_LANGUAGE).c_str(),
				Price2String(rawData.LimitPrice,SimpleInstrumentInfo.PriceTick).c_str(),
				strStatusMsg.c_str());
	        m_pTradeInfoDlg->ShowTradeMsg(info, -1, LOADSTRING(OLS_CANCEL_ORDER_FAIL), -1, NeedCancelFailConfirm());

			LOG_DATA* pData = new LOG_DATA;
	        pData->strTitle = "OLS_CANCEL_ORDER_FAIL";
	        pData->strFormat = "CVOLPRC_FORMAT";
			pData->vParam.push_back(LOG_PARAM(RspOrderAction1.InputOrderActionField.InstrumentID));
			pData->vParam.push_back(LOG_PARAM(rawData.Direction, DIRECTION_TYPE));
			pData->vParam.push_back(LOG_PARAM(rawData.CombOffsetFlag[0],  OFFSETFLAG_TYPE));
			pData->vParam.push_back(LOG_PARAM(Number2String(rawData.VolumeTotalOriginal)));
			pData->vParam.push_back(LOG_PARAM(rawData.OrderPriceType, PRICETYPE_MSG_TYPE));
			pData->vParam.push_back(LOG_PARAM(Price2String(rawData.LimitPrice,SimpleInstrumentInfo.PriceTick)));
			pData->vParam.push_back(LOG_PARAM(RspOrderAction1.RspInfoField.ErrorMsg, ORDER_STATUS_MSG_TYPE));
			m_pTradeInfoDlg->WriteLog( pData );

			if(NeedCancelFailSound())
				NoticeSound(conNoticeCancelFail);
		}
    }
}


void CAllOrdersPanel::OnRspOrderAction2(wxCommandEvent& evt)
{
    DataErrRtnOrderAction RspOrderAction2;
    DWORD EvtParamID=(DWORD)evt.GetInt();
    if(CFTEventParam::GetEventParam(EvtParamID,NULL,NULL,&RspOrderAction2,NULL,sizeof(RspOrderAction2),NULL))
        CFTEventParam::DeleteEventParam(EvtParamID);
    else return;

    IPlatformSingleSvr* pSvr=g_pPlatformMgr->GetPlatformSvr(RspOrderAction2.Head.PlatformID);
    if(!pSvr) return;

    if(m_nViewStyle==conAllOrderStyle) 
    {
	    m_pTradeInfoDlg=TRADEINFODLG(this);
	    if(m_pTradeInfoDlg)
        {
	        wxString info;
	        PlatformStru_InstrumentInfo SimpleInstrumentInfo;
	        memset(&SimpleInstrumentInfo,0,sizeof(SimpleInstrumentInfo));
	        pSvr->GetInstrumentInfo(RspOrderAction2.OrderActionField.InstrumentID,SimpleInstrumentInfo);
	        info.reserve(512);
			
			OrderKey orderKey(RspOrderAction2.OrderActionField.InvestorID, 
						RspOrderAction2.OrderActionField.InstrumentID, 
						RspOrderAction2.OrderActionField.FrontID, 
						RspOrderAction2.OrderActionField.SessionID, 
						wxString(RspOrderAction2.OrderActionField.OrderRef).Trim(false).c_str());
			PlatformStru_OrderInfo rawData;
			pSvr->GetOrder(orderKey, rawData);

			if(strlen(RspOrderAction2.RspInfoField.ErrorMsg)==0)
				strncpy(RspOrderAction2.RspInfoField.ErrorMsg, 
						RspOrderAction2.OrderActionField.StatusMsg, 
						sizeof(RspOrderAction2.RspInfoField.ErrorMsg));
			
			std::string strStatusMsg=RspOrderAction2.RspInfoField.ErrorMsg;
			int valStatusMsg=CFieldValueTool::String2OrderStatusMsg(strStatusMsg);
			if(valStatusMsg>0)  
				strStatusMsg=CFieldValueTool::OrderStatusMsg2String(valStatusMsg,SVR_LANGUAGE);
			info.Printf(LOADFORMATSTRING(CANCEL_ORDER_FORMAT,"%s%s%s%s%s%s%s"),//�ı��������%s,%s,%s,%s,%s,%s,%s
				RspOrderAction2.OrderActionField.InstrumentID,
				CFieldValueTool::Direction2String(rawData.Direction,SVR_LANGUAGE).c_str(),
				CFieldValueTool::OffsetFlag2String(rawData.CombOffsetFlag[0],SVR_LANGUAGE).c_str(),
				Number2String(rawData.VolumeTotalOriginal).c_str(),
				CFieldValueTool::OrderPriceType2String(rawData.OrderPriceType,SVR_LANGUAGE).c_str(),
				Price2String(rawData.LimitPrice,SimpleInstrumentInfo.PriceTick).c_str(),
				strStatusMsg.c_str());
	        m_pTradeInfoDlg->ShowTradeMsg(info, -1, LOADSTRING(OLS_CANCEL_ORDER_FAIL), -1, NeedCancelFailConfirm());

			LOG_DATA* pData = new LOG_DATA;
	        pData->strTitle = "OLS_CANCEL_ORDER_FAIL";
	        pData->strFormat = "CVOLPRC_FORMAT";
			pData->vParam.push_back(LOG_PARAM(RspOrderAction2.OrderActionField.InstrumentID));
			pData->vParam.push_back(LOG_PARAM(rawData.Direction, DIRECTION_TYPE));
			pData->vParam.push_back(LOG_PARAM(rawData.CombOffsetFlag[0],  OFFSETFLAG_TYPE));
			pData->vParam.push_back(LOG_PARAM(Number2String(rawData.VolumeTotalOriginal)));
			pData->vParam.push_back(LOG_PARAM(rawData.OrderPriceType, PRICETYPE_MSG_TYPE));
			pData->vParam.push_back(LOG_PARAM(Price2String(rawData.LimitPrice,SimpleInstrumentInfo.PriceTick)));
			pData->vParam.push_back(LOG_PARAM(RspOrderAction2.RspInfoField.ErrorMsg, ORDER_STATUS_MSG_TYPE));
			m_pTradeInfoDlg->WriteLog( pData );

			if(NeedCancelFailSound())
				NoticeSound(conNoticeCancelFail);
	    }
    }
}

void CAllOrdersPanel::OnRspForQuoteInsert(wxCommandEvent& evt)
{
    DataRspForQuoteInsert rspForQuoteInsert;
    DWORD EvtParamID=(DWORD)evt.GetInt();
    if(CFTEventParam::GetEventParam(EvtParamID,NULL,NULL,&rspForQuoteInsert,NULL,sizeof(DataRspForQuoteInsert),NULL))
        CFTEventParam::DeleteEventParam(EvtParamID);
    else return;

    IPlatformSingleSvr* pSvr=g_pPlatformMgr->GetPlatformSvr(rspForQuoteInsert.Head.PlatformID);
    if(!pSvr) return;

    if(m_nViewStyle==conAllOrderStyle) 
    {
	    m_pTradeInfoDlg=TRADEINFODLG(this);
	    if(m_pTradeInfoDlg)
        {
	        wxString info;
	        info.reserve(512);
			
			std::string strStatusMsg=rspForQuoteInsert.RspInfoField.ErrorMsg;
			info.Printf(LOADFORMATSTRING(CONMENO_FORMAT,"%s\n%s"),//�ı��������%s,%s,%s,%s,%s,%s,%s
				rspForQuoteInsert.InputForQuoteField.InstrumentID,
				strStatusMsg.c_str());
	        m_pTradeInfoDlg->ShowTradeMsg(info, -1, LOADSTRING(ѯ�ۻر�), -1, NeedOrderSuccessConfirm());

			LOG_DATA* pData = new LOG_DATA;
	        pData->strTitle = "ѯ�ۻر�";
	        pData->strFormat = "CONMENO_FORMAT";
			pData->vParam.push_back(LOG_PARAM(rspForQuoteInsert.InputForQuoteField.InstrumentID));
			pData->vParam.push_back(LOG_PARAM(rspForQuoteInsert.RspInfoField.ErrorMsg, ORDER_STATUS_MSG_TYPE));
			m_pTradeInfoDlg->WriteLog( pData );

			if(NeedOrderSuccessSound())
				NoticeSound(conNoticeNewOrderSuccess);
	    }
    }
}

void CAllOrdersPanel::OnErrRtnForQuoteInsert(wxCommandEvent& evt)
{
    DataErrRtnForQuoteInsert errRtnForQuoteInsert;
    DWORD EvtParamID=(DWORD)evt.GetInt();
    if(CFTEventParam::GetEventParam(EvtParamID,NULL,NULL,&errRtnForQuoteInsert,NULL,sizeof(DataErrRtnForQuoteInsert),NULL))
        CFTEventParam::DeleteEventParam(EvtParamID);
    else return;

    IPlatformSingleSvr* pSvr=g_pPlatformMgr->GetPlatformSvr(errRtnForQuoteInsert.Head.PlatformID);
    if(!pSvr) return;

    if(m_nViewStyle==conAllOrderStyle) 
    {
	    m_pTradeInfoDlg=TRADEINFODLG(this);
	    if(m_pTradeInfoDlg)
        {
	        wxString info;
	        info.reserve(512);
			
			std::string strStatusMsg=errRtnForQuoteInsert.RspInfoField.ErrorMsg;
			info.Printf(LOADFORMATSTRING(CANCEL_ORDER_FORMAT,"��Լ: %s\n������Ϣ: \n%s"),//�ı��������%s,%s,%s,%s,%s,%s,%s
				errRtnForQuoteInsert.InputForQuoteField.InstrumentID,
				strStatusMsg.c_str());
	        m_pTradeInfoDlg->ShowTradeMsg(info, -1, LOADSTRING(ѯ��ʧ��), -1, NeedOrderFailConfirm());

			LOG_DATA* pData = new LOG_DATA;
	        pData->strTitle = "ѯ��ʧ��";
			pData->strFormat = "��Լ: %s, ������Ϣ: %s";
			pData->vParam.push_back(LOG_PARAM(errRtnForQuoteInsert.InputForQuoteField.InstrumentID));
			pData->vParam.push_back(LOG_PARAM(errRtnForQuoteInsert.RspInfoField.ErrorMsg, ORDER_STATUS_MSG_TYPE));
			m_pTradeInfoDlg->WriteLog( pData );

			if(NeedOrderFailSound())
				NoticeSound(conNoticeNewOrderFail);
	    }
    }
}

void CAllOrdersPanel::OnRtnExecOrder(wxCommandEvent& evt)
{
    DataRtnExecOrder rtnExecOrder;
    DWORD EvtParamID=(DWORD)evt.GetInt();
    if(CFTEventParam::GetEventParam(EvtParamID,NULL,NULL,&rtnExecOrder,NULL,sizeof(DataRtnExecOrder),NULL))
        CFTEventParam::DeleteEventParam(EvtParamID);
    else return;

    IPlatformSingleSvr* pSvr=g_pPlatformMgr->GetPlatformSvr(rtnExecOrder.Head.PlatformID);
    if(!pSvr) return;

    if(m_nViewStyle==conAllOrderStyle) 
    {
	    m_pTradeInfoDlg=TRADEINFODLG(this);
	    if(m_pTradeInfoDlg)
        {
	        wxString info;
	        info.reserve(512);
			
			std::string strStatusMsg=rtnExecOrder.ExecOrderField.StatusMsg;
			info.Printf(LOADFORMATSTRING(CANCEL_ORDER_FORMAT,"��Լ: %s\n��Ȩ: %s\n����: %d\n״̬��Ϣ: \n%s"),
				rtnExecOrder.ExecOrderField.InstrumentID,
				rtnExecOrder.ExecOrderField.ActionType==THOST_FTDC_ACTP_Exec ? "ִ��" : "����",
				rtnExecOrder.ExecOrderField.Volume,
				strStatusMsg.c_str());
	        m_pTradeInfoDlg->ShowTradeMsg(info, -1, LOADSTRING(��Ȩִ�лر�), -1, NeedOrderSuccessConfirm());

			LOG_DATA* pData = new LOG_DATA;
	        pData->strTitle = "��Ȩִ�лر�";
			pData->strFormat = "��Լ: %s\n��Ȩ: %s\n����: %d\n״̬��Ϣ: \n%s";
			pData->vParam.push_back(LOG_PARAM(rtnExecOrder.ExecOrderField.InstrumentID));
			pData->vParam.push_back(LOG_PARAM(rtnExecOrder.ExecOrderField.ActionType==THOST_FTDC_ACTP_Exec ? "ִ��" : "����"));
			pData->vParam.push_back(LOG_PARAM(rtnExecOrder.ExecOrderField.Volume));
			pData->vParam.push_back(LOG_PARAM(rtnExecOrder.ExecOrderField.StatusMsg, ORDER_STATUS_MSG_TYPE));
			m_pTradeInfoDlg->WriteLog( pData );

			if(NeedOrderSuccessSound())
				NoticeSound(conNoticeNewOrderSuccess);
	    }
    }
}

void CAllOrdersPanel::OnRspExecOrderInsert(wxCommandEvent& evt)
{
    DataRspExecOrderInsert rspExecOrderInsert;
    DWORD EvtParamID=(DWORD)evt.GetInt();
    if(CFTEventParam::GetEventParam(EvtParamID,NULL,NULL,&rspExecOrderInsert,NULL,sizeof(DataRspExecOrderInsert),NULL))
        CFTEventParam::DeleteEventParam(EvtParamID);
    else return;

    IPlatformSingleSvr* pSvr=g_pPlatformMgr->GetPlatformSvr(rspExecOrderInsert.Head.PlatformID);
    if(!pSvr) return;

    if(m_nViewStyle==conAllOrderStyle) 
    {
	    m_pTradeInfoDlg=TRADEINFODLG(this);
	    if(m_pTradeInfoDlg)
        {
	        wxString info;
	        info.reserve(512);
			
			std::string strStatusMsg=rspExecOrderInsert.RspInfoField.ErrorMsg;
			info.Printf(LOADFORMATSTRING(CANCEL_ORDER_FORMAT,"��Լ: %s\n��Ȩ: %s\n����: %d\n״̬��Ϣ: \n%s"),
				rspExecOrderInsert.InputExecOrderField.InstrumentID,
				rspExecOrderInsert.InputExecOrderField.ActionType==THOST_FTDC_ACTP_Exec ? "ִ��" : "����",
				rspExecOrderInsert.InputExecOrderField.Volume, 
				strStatusMsg.c_str());
	        m_pTradeInfoDlg->ShowTradeMsg(info, -1, LOADSTRING(��Ȩִ�д���), -1, NeedOrderFailConfirm());

			LOG_DATA* pData = new LOG_DATA;
	        pData->strTitle = "��Ȩִ�д���";
			pData->strFormat = "��Լ: %s\n��Ȩ: %s\n����: %d\n״̬��Ϣ: \n%s";
			pData->vParam.push_back(LOG_PARAM(rspExecOrderInsert.InputExecOrderField.InstrumentID));
			pData->vParam.push_back(LOG_PARAM(rspExecOrderInsert.InputExecOrderField.ActionType==THOST_FTDC_ACTP_Exec ? "ִ��" : "����"));
			pData->vParam.push_back(LOG_PARAM(rspExecOrderInsert.InputExecOrderField.Volume));
			pData->vParam.push_back(LOG_PARAM(rspExecOrderInsert.RspInfoField.ErrorMsg, ORDER_STATUS_MSG_TYPE));
			m_pTradeInfoDlg->WriteLog( pData );

			if(NeedOrderFailSound())
				NoticeSound(conNoticeNewOrderFail);
	    }
    }
}

void CAllOrdersPanel::OnErrRtnExecOrderInsert(wxCommandEvent& evt)
{
    DataErrRtnExecOrderInsert errRtnExecOrderInsert;
    DWORD EvtParamID=(DWORD)evt.GetInt();
    if(CFTEventParam::GetEventParam(EvtParamID,NULL,NULL,&errRtnExecOrderInsert,NULL,sizeof(DataErrRtnExecOrderInsert),NULL))
        CFTEventParam::DeleteEventParam(EvtParamID);
    else return;

    IPlatformSingleSvr* pSvr=g_pPlatformMgr->GetPlatformSvr(errRtnExecOrderInsert.Head.PlatformID);
    if(!pSvr) return;

    if(m_nViewStyle==conAllOrderStyle) 
    {
	    m_pTradeInfoDlg=TRADEINFODLG(this);
	    if(m_pTradeInfoDlg)
        {
	        wxString info;
	        info.reserve(512);
			
			std::string strStatusMsg=errRtnExecOrderInsert.RspInfoField.ErrorMsg;
			info.Printf(LOADFORMATSTRING(CANCEL_ORDER_FORMAT,"��Լ: %s\n��Ȩ: %s\n����: %d\n״̬��Ϣ: \n%s"),
				errRtnExecOrderInsert.InputExecOrderField.InstrumentID,
				errRtnExecOrderInsert.InputExecOrderField.ActionType==THOST_FTDC_ACTP_Exec ? "ִ��" : "����",
				errRtnExecOrderInsert.InputExecOrderField.Volume, 
				strStatusMsg.c_str());
	        m_pTradeInfoDlg->ShowTradeMsg(info, -1, LOADSTRING(��Ȩִ�д���), -1, NeedOrderFailConfirm());

			LOG_DATA* pData = new LOG_DATA;
	        pData->strTitle = "��Ȩִ�д���";
			pData->strFormat = "��Լ: %s\n��Ȩ: %s\n����: %d\n״̬��Ϣ: \n%s";
			pData->vParam.push_back(LOG_PARAM(errRtnExecOrderInsert.InputExecOrderField.InstrumentID));
			pData->vParam.push_back(LOG_PARAM(errRtnExecOrderInsert.InputExecOrderField.ActionType==THOST_FTDC_ACTP_Exec ? "ִ��" : "����"));
			pData->vParam.push_back(LOG_PARAM(errRtnExecOrderInsert.InputExecOrderField.Volume));
			pData->vParam.push_back(LOG_PARAM(errRtnExecOrderInsert.RspInfoField.ErrorMsg, ORDER_STATUS_MSG_TYPE));
			m_pTradeInfoDlg->WriteLog( pData );

			if(NeedOrderFailSound())
				NoticeSound(conNoticeNewOrderFail);
	    }
    }
}

void CAllOrdersPanel::OnRspExecOrderAction(wxCommandEvent& evt)
{
    DataRspExecOrderAction rspExecOrderAction;
    DWORD EvtParamID=(DWORD)evt.GetInt();
    if(CFTEventParam::GetEventParam(EvtParamID,NULL,NULL,&rspExecOrderAction,NULL,sizeof(DataRspExecOrderAction),NULL))
        CFTEventParam::DeleteEventParam(EvtParamID);
    else return;

    IPlatformSingleSvr* pSvr=g_pPlatformMgr->GetPlatformSvr(rspExecOrderAction.Head.PlatformID);
    if(!pSvr) return;

    if(m_nViewStyle==conAllOrderStyle) 
    {
	    m_pTradeInfoDlg=TRADEINFODLG(this);
	    if(m_pTradeInfoDlg)
        {
	        wxString info;
	        info.reserve(512);
			
			std::string strStatusMsg=rspExecOrderAction.RspInfoField.ErrorMsg;
			info.Printf(LOADFORMATSTRING(CANCEL_ORDER_FORMAT,"��Լ: %s\n������: %s, ��Ȩ���: %s\n������Ϣ: \n%s"),
				rspExecOrderAction.InputExecOrderActionField.InstrumentID,
				rspExecOrderAction.InputExecOrderActionField.ExchangeID,
				rspExecOrderAction.InputExecOrderActionField.ExecOrderSysID,
				strStatusMsg.c_str());
	        m_pTradeInfoDlg->ShowTradeMsg(info, -1, LOADSTRING(��Ȩ��Ȩ��������), -1, NeedOrderFailConfirm());

			LOG_DATA* pData = new LOG_DATA;
	        pData->strTitle = "��Ȩ��Ȩ��������";
			pData->strFormat = "��Լ: %s\n������: %s, ��Ȩ���: %s\n������Ϣ: \n%s";
			pData->vParam.push_back(LOG_PARAM(rspExecOrderAction.InputExecOrderActionField.InstrumentID));
			pData->vParam.push_back(LOG_PARAM(rspExecOrderAction.InputExecOrderActionField.ExchangeID));
			pData->vParam.push_back(LOG_PARAM(rspExecOrderAction.InputExecOrderActionField.ExecOrderSysID));
			pData->vParam.push_back(LOG_PARAM(rspExecOrderAction.RspInfoField.ErrorMsg, ORDER_STATUS_MSG_TYPE));
			m_pTradeInfoDlg->WriteLog( pData );

			if(NeedOrderFailSound())
				NoticeSound(conNoticeNewOrderFail);
	    }
    }
}

void CAllOrdersPanel::OnErrRtnExecOrderAction(wxCommandEvent& evt)
{
    DataErrRtnExecOrderAction errRtnExecOrderAction;
    DWORD EvtParamID=(DWORD)evt.GetInt();
    if(CFTEventParam::GetEventParam(EvtParamID,NULL,NULL,&errRtnExecOrderAction,NULL,sizeof(DataErrRtnExecOrderAction),NULL))
        CFTEventParam::DeleteEventParam(EvtParamID);
    else return;

    IPlatformSingleSvr* pSvr=g_pPlatformMgr->GetPlatformSvr(errRtnExecOrderAction.Head.PlatformID);
    if(!pSvr) return;

    if(m_nViewStyle==conAllOrderStyle) 
    {
	    m_pTradeInfoDlg=TRADEINFODLG(this);
	    if(m_pTradeInfoDlg)
        {
	        wxString info;
	        info.reserve(512);
			
			std::string strStatusMsg=errRtnExecOrderAction.RspInfoField.ErrorMsg;
			info.Printf(LOADFORMATSTRING(CANCEL_ORDER_FORMAT,"��Լ: %s\n������: %s, ��Ȩ���: %s\n������Ϣ: \n%s"),
				errRtnExecOrderAction.ExecOrderActionField.InstrumentID,
				errRtnExecOrderAction.ExecOrderActionField.ExchangeID,
				errRtnExecOrderAction.ExecOrderActionField.ExecOrderSysID,
				strStatusMsg.c_str());
	        m_pTradeInfoDlg->ShowTradeMsg(info, -1, LOADSTRING(��Ȩ��Ȩ��������), -1, NeedOrderFailConfirm());

			LOG_DATA* pData = new LOG_DATA;
	        pData->strTitle = "��Ȩ��Ȩ��������";
			pData->strFormat = "��Լ: %s\n������: %s, ��Ȩ���: %s\n������Ϣ: \n%s";
			pData->vParam.push_back(LOG_PARAM(errRtnExecOrderAction.ExecOrderActionField.InstrumentID));
			pData->vParam.push_back(LOG_PARAM(errRtnExecOrderAction.ExecOrderActionField.ExchangeID));
			pData->vParam.push_back(LOG_PARAM(errRtnExecOrderAction.ExecOrderActionField.ExecOrderSysID));
			pData->vParam.push_back(LOG_PARAM(errRtnExecOrderAction.RspInfoField.ErrorMsg, ORDER_STATUS_MSG_TYPE));
			m_pTradeInfoDlg->WriteLog( pData );

			if(NeedOrderFailSound())
				NoticeSound(conNoticeNewOrderFail);
	    }
    }
}

CAllOrdersPanel::CAllOrdersPanel(int nViewStyle, 
								 wxWindow *parent,
                                 wxWindowID winid,
                                 const wxPoint& pos,
                                 const wxSize& size,
                                 long style,
                                 const wxString& name)
:COrderListPanel(parent,winid, pos,size, style, name),m_pListCfg(NULL),
m_WxRadioButton1(NULL),
m_WxRadioButton2(NULL),
m_WxRadioButton3(NULL),
m_WxRadioButton4(NULL),
m_pButtonRemoveAll(NULL),
m_pButtonRemove(NULL),
m_pButtonReQry(NULL)
{

	SetBackgroundColour(DEFAULT_COLOUR);

    switch(nViewStyle) 
	{ 
		case conAllOrderStyle:
			m_nGID = GID_ALL_ORDER;
			m_ShowType = ALLORDER;
			m_nViewStyle = conAllOrderStyle;
			break;
		case conOpenOrderStyle:
			m_nGID = GID_OPEN_ORDER;
			m_ShowType = OPENORDER;
			m_nViewStyle = conOpenOrderStyle;
			break;
	};

	switch(nViewStyle) 
	{ 
		case conAllOrderStyle: 
		{
			m_WxRadioButton1 = new wxRadioButton(this, ID_WXRADIOBUTTON1, LOADSTRING(ALL_ORDER), wxPoint(), wxSize(60, CommonBtnHeight));//ȫ����
			m_WxRadioButton1->SetValue(true);
			m_WxRadioButton2 = new wxRadioButton(this, ID_WXRADIOBUTTON2, LOADSTRING(OPEN_ORDER), wxPoint(), wxSize(60, CommonBtnHeight));//�ҵ�
			m_WxRadioButton3 = new wxRadioButton(this, ID_WXRADIOBUTTON3, LOADSTRING(OIP_FILLED), wxPoint(), wxSize(60, CommonBtnHeight));//�ѳɽ�
			m_WxRadioButton4 = new wxRadioButton(this, ID_WXRADIOBUTTON4, LOADSTRING(CANCEL_FAIL_ORDER), wxPoint(), wxSize(90, CommonBtnHeight));//�ѳ���/����
			break;
		}
		case conOpenOrderStyle:
            break;
	};

    m_pButtonReQry = new wxButton(this, ID_BUTTON_REQRY, LOADSTRING(BUTTON_REQRY_CAPTION), wxPoint(), wxSize(80,CommonBtnHeight));

	m_pButtonRemove = new wxButton(this, ID_BUTTON_REMOVE, LOADSTRING(CONTEXTMENU_REMOVE), wxPoint(), wxSize(60,CommonBtnHeight));             //����
	m_pButtonRemoveAll = new wxButton(this, ID_BUTTON_REMOVEALL, LOADSTRING(CONTEXTMENU_REMOVEALL), wxPoint(), wxSize(60,CommonBtnHeight));    //ȫ��
    
	switch(nViewStyle) 
	{ 
		case conAllOrderStyle:
			m_pwxExtListCtrl= new CDataListCtrl<OrderKey,PlatformStru_OrderInfo>(UpdateListItemCallBack, 
                                                                                 this, 
                                                                                 wxID_ANY,
                                                                                 wxDefaultPosition,
                                                                                 wxDefaultSize,
                                                                                 wxLCEXT_TOGGLE_COLOUR|wxLCEXT_REPORT|wxLCEXT_VRULES|wxLCEXT_MASK_SORT,
                                                                                 wxDefaultValidator,
                                                                                 wxEmptyString);
			break;
		case conOpenOrderStyle: 
			m_pwxExtListCtrl= new CDataListCtrl<OrderKey,PlatformStru_OrderInfo>(UpdateOpenListItemCallBack, 
                                                                                 this, 
                                                                                 wxID_ANY,
                                                                                 wxDefaultPosition,
                                                                                 wxDefaultSize,
                                                                                 wxLCEXT_TOGGLE_COLOUR|wxLCEXT_REPORT|wxLCEXT_VRULES|wxLCEXT_MASK_SORT,
                                                                                 wxDefaultValidator,
                                                                                 wxEmptyString);
			break;
	};

	switch(nViewStyle) 
	{ 
		case conAllOrderStyle:
            s_pAllOrdersPanel = this;
            break;
		case conOpenOrderStyle: 
            s_pOpenOrdersPanel = this;
			break;
	};

    Init();

}

CAllOrdersPanel::~CAllOrdersPanel(void)
{
    if(m_WxRadioButton1) { delete m_WxRadioButton1;  m_WxRadioButton1=NULL; }
    if(m_WxRadioButton2) { delete m_WxRadioButton2;  m_WxRadioButton2=NULL; }
    if(m_WxRadioButton3) { delete m_WxRadioButton3;  m_WxRadioButton3=NULL; }
    if(m_WxRadioButton4) { delete m_WxRadioButton4;  m_WxRadioButton4=NULL; }
    if(m_pButtonRemoveAll) { delete m_pButtonRemoveAll;  m_pButtonRemoveAll=NULL; }
    if(m_pButtonRemove) { delete m_pButtonRemove;  m_pButtonRemove=NULL; }
    if(m_pButtonReQry) { delete m_pButtonReQry;  m_pButtonReQry=NULL; }
}


void CAllOrdersPanel::ShowAll()
{
    vector<IPlatformSingleSvr*> Svrs=g_pPlatformMgr->GetLogonPlatformSvr();
	vector<PlatformStru_OrderInfo> vecAll,vecOrders;
    for (int i=0;i<(int)Svrs.size();i++)
	{
        if(i==0)
        {
		    if(m_ShowType==ALLORDER) Svrs[i]->GetTriggerOrders(vecAll);
		    else if(m_ShowType==OPENORDER) Svrs[i]->GetWaitOrders(vecAll);
		    else if(m_ShowType==TRADEDORDER) Svrs[i]->GetTradedOrders(vecAll);
		    else if(m_ShowType==CANCELORDER) Svrs[i]->GetCanceledOrders(vecAll);
        }
        else
        {
		    if(m_ShowType==ALLORDER) Svrs[i]->GetTriggerOrders(vecOrders);
		    else if(m_ShowType==OPENORDER) Svrs[i]->GetWaitOrders(vecOrders);
		    else if(m_ShowType==TRADEDORDER) Svrs[i]->GetTradedOrders(vecOrders);
		    else if(m_ShowType==CANCELORDER) Svrs[i]->GetCanceledOrders(vecOrders);

		    vecAll.insert(vecAll.end(),vecOrders.begin(),vecOrders.end());
        }
	}
    m_pwxExtListCtrl->UpdateAllItems(vecAll);
}


void CAllOrdersPanel::OnReQry(wxCommandEvent& evt)
{
    PlatformStru_QryOrder tmp;
    vector<IPlatformSingleSvr*> Svrs=g_pPlatformMgr->GetLogonPlatformSvr();
    for(int i=0;i<(int)Svrs.size();i++)
        Svrs[i]->ReqQryOrder(tmp);
}
void CAllOrdersPanel::OnReConnectQry(wxCommandEvent& evt)
{
	if(evt.GetInt()==1)//��ȡ��Ҫˢ�µ�����
	{
        vector<IPlatformSingleSvr*> Svrs=g_pPlatformMgr->GetLogonPlatformSvr();
		vector<PlatformStru_OrderInfo> vecOrders;
        int count=0;
        for(int i=0;i<(int)Svrs.size();i++)
        {
            Svrs[i]->GetTriggerOrders(vecOrders);
            count+=vecOrders.size();
        }
		evt.SetExtraLong(vecOrders.size());
	}
	else
	{
		OnReQry(evt);
	}
}
// �µ��ɹ� �Ƿ� ����
BOOL CAllOrdersPanel::NeedOrderSuccessConfirm()
{
    // �����xml�ж��Ƿ�Ҫȷ�ϵ�����
    CfgMgr * pMgr =  CFG_MGR_DEFAULT();  
    if( pMgr == NULL) return TRUE;
    LPORDER_CFG p = pMgr->GetOrderCfg();
    return p->bOrderSuccessDlg;
}

// �µ��ɹ� �Ƿ� ����
BOOL CAllOrdersPanel::NeedOrderSuccessSound()
{
    // �����xml�ж��Ƿ�Ҫȷ�ϵ�����
    CfgMgr * pMgr =  CFG_MGR_DEFAULT();  
    if( pMgr == NULL) return TRUE;
    LPORDER_CFG p = pMgr->GetOrderCfg();
    return p->bOrderSuccessSound;
}

// �µ��ɹ� �Ƿ� ����
BOOL CAllOrdersPanel::NeedOrderFailConfirm()
{
    // �����xml�ж��Ƿ�Ҫȷ�ϵ�����
    CfgMgr * pMgr =  CFG_MGR_DEFAULT();  
    if( pMgr == NULL) return TRUE;
    LPORDER_CFG p = pMgr->GetOrderCfg();
    return p->bOrderFailDlg;
}

// �µ��ɹ� �Ƿ� ����
BOOL CAllOrdersPanel::NeedOrderFailSound()
{
    // �����xml�ж��Ƿ�Ҫȷ�ϵ�����
    CfgMgr * pMgr =  CFG_MGR_DEFAULT();  
    if( pMgr == NULL) return TRUE;
    LPORDER_CFG p = pMgr->GetOrderCfg();
    return p->bOrderFailSound;
}

// �ҵ��ɽ� �Ƿ� ����
BOOL CAllOrdersPanel::NeedOrderTradeConfirm()
{
    // �����xml�ж��Ƿ�Ҫȷ�ϵ�����
    CfgMgr * pMgr =  CFG_MGR_DEFAULT();  
    if( pMgr == NULL) return TRUE;
    LPORDER_CFG p = pMgr->GetOrderCfg();
    return p->bTradeDlg;
}

// �ҵ��ɽ� �Ƿ� ����
BOOL CAllOrdersPanel::NeedOrderTradeSound()
{
    // �����xml�ж��Ƿ�Ҫȷ�ϵ�����
    CfgMgr * pMgr =  CFG_MGR_DEFAULT();  
    if( pMgr == NULL) return TRUE;
    LPORDER_CFG p = pMgr->GetOrderCfg();
    return p->bTradeSound;
}

// �����ɽ� �Ƿ� ����
BOOL CAllOrdersPanel::NeedCancelSuccessConfirm()
{
	CANCEL_ORDER_CFG cancelOrderCfg = CFG_MGR_DEFAULT()->GetCancelOrderCfg();
	return cancelOrderCfg.bCancelSuccessDlg;
}

// �����ɹ� �Ƿ� ����
BOOL CAllOrdersPanel::NeedCancelSuccessSound()
{
    // �����xml�ж��Ƿ�Ҫȷ�ϵ�����
    CfgMgr * pMgr =  CFG_MGR_DEFAULT();  
    if( pMgr == NULL) return TRUE;
    CANCEL_ORDER_CFG p = pMgr->GetCancelOrderCfg();
    return p.bCancelSuccessSound;
}

// ����ʧ�� �Ƿ� ����
BOOL CAllOrdersPanel::NeedCancelFailConfirm()
{
    CANCEL_ORDER_CFG cancelOrderCfg = CFG_MGR_DEFAULT()->GetCancelOrderCfg();
    return cancelOrderCfg.bCancelFailDlg;
}

// �����ɹ� �Ƿ� ����
BOOL CAllOrdersPanel::NeedCancelFailSound()
{
    // �����xml�ж��Ƿ�Ҫȷ�ϵ�����
    CfgMgr * pMgr =  CFG_MGR_DEFAULT();  
    if( pMgr == NULL) return TRUE;
    CANCEL_ORDER_CFG p = pMgr->GetCancelOrderCfg();
    return p.bCancelFailSound;
}

bool CAllOrdersPanel::NeedHideCancelButton()
{
    CfgMgr *pCfgMgr = CFG_MGR_DEFAULT();
    if(pCfgMgr) 
	{
		CANCEL_ORDER_CFG tCfg = pCfgMgr->GetCancelOrderCfg();
		return tCfg.bHideCancelButton;
	}
	return false;
}

//���ݱ�����ִ��ʾ ������Ϣ
void CAllOrdersPanel::ShowInsertDlg(const string& Title, const DataRspOrderInsert& OrderInfo, BOOL bIsShow)
{
    if(TRADEINFODLG(this)==NULL) return;
	wxString info;
    std::string stdstrprice=GlobalFunc::ConvertToString(OrderInfo.InputOrderField.LimitPrice,4);
	info.reserve(512);
	info.Printf(LOADFORMATSTRING(CDOCQPM_FORMAT,"%s%s%s%s%s%s"),//�ı��������%s,%s,%s,%s,%s,%s
					OrderInfo.InputOrderField.InstrumentID,
					CFieldValueTool::Direction2String(OrderInfo.InputOrderField.Direction,SVR_LANGUAGE).c_str(),
					CFieldValueTool::OffsetFlag2String(OrderInfo.InputOrderField.CombOffsetFlag[0],SVR_LANGUAGE).c_str(),
					Number2String(OrderInfo.InputOrderField.VolumeTotalOriginal).c_str(),
					stdstrprice.c_str(),
					OrderInfo.RspInfoField.ErrorMsg);
	TRADEINFODLG(this)->ShowTradeMsg(info, -1, LOADSTRING_TEXT(Title), -1, bIsShow);

	LOG_DATA* pData = new LOG_DATA;
	pData->strTitle = Title;
	pData->strFormat = "CDOCQPM_FORMAT";
	pData->vParam.push_back(LOG_PARAM(OrderInfo.InputOrderField.InstrumentID));
	pData->vParam.push_back(LOG_PARAM(OrderInfo.InputOrderField.Direction, DIRECTION_TYPE));
	pData->vParam.push_back(LOG_PARAM(OrderInfo.InputOrderField.CombOffsetFlag[0], OFFSETFLAG_TYPE));
	pData->vParam.push_back(LOG_PARAM(Number2String(OrderInfo.InputOrderField.VolumeTotalOriginal)));
	pData->vParam.push_back(LOG_PARAM(stdstrprice));
	pData->vParam.push_back(LOG_PARAM(OrderInfo.RspInfoField.ErrorMsg));
	TRADEINFODLG(this)->WriteLog( pData );
}


void  CAllOrdersPanel::PopupCancel_InsertDlg(IPlatformSingleSvr* pSvr,const PlatformStru_OrderInfo& rawData)
{
    if(!pSvr) return;

    //int localid=atoi(rawData.OrderSysID);//atoi(rawData.OrderLocalID);
	if(rawData.OrderStatus == THOST_FTDC_OST_Canceled || rawData.OrderStatus>THOST_FTDC_OST_Touched || 
			rawData.OrderSubmitStatus==THOST_FTDC_OSS_InsertRejected) {
		unsigned int sysid=strlen(rawData.OrderSysID);//atoi(rawData.OrderSysID);//����пո�
		BOOL bIsFail = FALSE;
            m_pTradeInfoDlg=TRADEINFODLG(this);
			if(m_pTradeInfoDlg){
            string title;
			wxString info;
			BOOL bIsShow = (NeedCancelSuccessConfirm() && sysid>0) || (NeedOrderFailConfirm() && sysid==0);
			if(sysid>0 && rawData.OrderSubmitStatus!=THOST_FTDC_OSS_InsertRejected) 
			{
				title = "CANCEL_ORDER_OK";
				bIsFail = FALSE;
			}
			else
			{
				title = "OLS_SEND_ORDER_FAIL";
				bIsFail = TRUE;
			}
            info.reserve(512);
			PlatformStru_InstrumentInfo SimpleInstrumentInfo;
		    pSvr->GetInstrumentInfo(rawData.InstrumentID,SimpleInstrumentInfo);
			std::string strStatusMsg=rawData.StatusMsg;
			int valStatusMsg=CFieldValueTool::String2OrderStatusMsg(strStatusMsg);
			if(valStatusMsg>0)  
				strStatusMsg=CFieldValueTool::OrderStatusMsg2String(valStatusMsg,SVR_LANGUAGE);
			info.Printf(LOADFORMATSTRING(CANCEL_ORDER_FORMAT,"%s%s%s%s%s%s%s"),//�ı��������%s,%s,%s,%s,%s,%s,%s
				rawData.InstrumentID,
				CFieldValueTool::Direction2String(rawData.Direction,SVR_LANGUAGE).c_str(),
				CFieldValueTool::OffsetFlag2String(rawData.CombOffsetFlag[0],SVR_LANGUAGE).c_str(),
				Number2String(rawData.VolumeTotalOriginal).c_str(),
				CFieldValueTool::OrderPriceType2String(rawData.OrderPriceType,SVR_LANGUAGE).c_str(),
				Price2String(rawData.LimitPrice,SimpleInstrumentInfo.PriceTick).c_str(),
				strStatusMsg.c_str());
			m_pTradeInfoDlg->ShowTradeMsg(info, -1, LOADSTRING_TEXT(title), -1, bIsShow);

			LOG_DATA* pData = new LOG_DATA;
			pData->strTitle = "ORDERSTATUS";//title;
			pData->strFormat = "CANCEL_ORDER_FORMAT";
			pData->vParam.push_back(LOG_PARAM(rawData.InstrumentID));
			pData->vParam.push_back(LOG_PARAM(rawData.Direction, DIRECTION_TYPE));
			pData->vParam.push_back(LOG_PARAM(rawData.CombOffsetFlag[0],  OFFSETFLAG_TYPE));
			pData->vParam.push_back(LOG_PARAM(Number2String(rawData.VolumeTotalOriginal)));
			pData->vParam.push_back(LOG_PARAM(rawData.OrderPriceType, PRICETYPE_MSG_TYPE));
			pData->vParam.push_back(LOG_PARAM(Price2String(rawData.LimitPrice,SimpleInstrumentInfo.PriceTick)));
			pData->vParam.push_back(LOG_PARAM(rawData.StatusMsg, ORDER_STATUS_MSG_TYPE));
			m_pTradeInfoDlg->WriteLog( pData );

			if(!bIsFail && NeedCancelSuccessSound()) 
				NoticeSound(conNoticeCancelSuccess);
			if(bIsFail && NeedOrderFailSound()) 
				NoticeSound(conNoticeNewOrderFail);
		}

    }
	if(rawData.OrderStatus == THOST_FTDC_OST_AllTraded 
			|| rawData.OrderStatus == THOST_FTDC_OST_PartTradedQueueing	//���ֳɽ����ڶ����� '1'
			|| rawData.OrderStatus == THOST_FTDC_OST_NoTradeQueueing) {
			BOOL bIsShow = TRUE;
			string strKey;
			DWORD dwCurrTime = ::GetTickCount();
			map<string, DWORD>::iterator it;
			strKey = rawData.ExchangeID;
			strKey += ",";
			strKey += rawData.OrderSysID;
			it = m_mapCancelWait.find(strKey);
			if(it != m_mapCancelWait.end()) {
				// �ж��Ƿ�ʱ��δ��ʱ����Բ���ʾ
				if(it->second+2000 > dwCurrTime) {
					bIsShow = FALSE;
				}
				m_mapCancelWait.erase(it);
			}
			if(bIsShow) {
				m_pTradeInfoDlg=TRADEINFODLG(this);
				if(m_pTradeInfoDlg){
				wxString info;
				info.reserve(512);
				PlatformStru_InstrumentInfo SimpleInstrumentInfo;
				pSvr->GetInstrumentInfo(rawData.InstrumentID,SimpleInstrumentInfo);
				std::string strStatusMsg=rawData.StatusMsg;
				int valStatusMsg=CFieldValueTool::String2OrderStatusMsg(strStatusMsg);
				if(valStatusMsg>0)  strStatusMsg=CFieldValueTool::OrderStatusMsg2String(valStatusMsg,SVR_LANGUAGE);
				info.Printf(LOADFORMATSTRING(CANCEL_ORDER_FORMAT,"%s%s%s%s%s%s%s"),//�ı��������%s,%s,%s,%s,%s,%s,%s
					rawData.InstrumentID,
					CFieldValueTool::Direction2String(rawData.Direction,SVR_LANGUAGE).c_str(),
					CFieldValueTool::OffsetFlag2String(rawData.CombOffsetFlag[0],SVR_LANGUAGE).c_str(),
					Number2String(rawData.VolumeTotalOriginal).c_str(),
					CFieldValueTool::OrderPriceType2String(rawData.OrderPriceType,SVR_LANGUAGE).c_str(),
					Price2String(rawData.LimitPrice,SimpleInstrumentInfo.PriceTick).c_str(),
					strStatusMsg.c_str());
				m_pTradeInfoDlg->ShowTradeMsg2(info, LOADSTRING(SEND_ORDER_OK), NeedOrderSuccessConfirm(), wxString(rawData.OrderSysID), false);

				LOG_DATA* pData = new LOG_DATA;
				pData->strTitle = "ORDERSTATUS";//"SEND_ORDER_OK";
				pData->strFormat = "CANCEL_ORDER_FORMAT";
				pData->vParam.push_back(LOG_PARAM(rawData.InstrumentID));
				pData->vParam.push_back(LOG_PARAM(rawData.Direction, DIRECTION_TYPE));
				pData->vParam.push_back(LOG_PARAM(rawData.CombOffsetFlag[0], OFFSETFLAG_TYPE));
				pData->vParam.push_back(LOG_PARAM(Number2String(rawData.VolumeTotalOriginal)));
				pData->vParam.push_back(LOG_PARAM(rawData.OrderPriceType, PRICETYPE_MSG_TYPE));
				pData->vParam.push_back(LOG_PARAM(Price2String(rawData.LimitPrice,SimpleInstrumentInfo.PriceTick)));
				pData->vParam.push_back(LOG_PARAM(rawData.StatusMsg, ORDER_STATUS_MSG_TYPE));
				m_pTradeInfoDlg->WriteLog( pData );
				
				if(NeedOrderSuccessSound()) 
					NoticeSound(conNoticeNewOrderSuccess);
			}
		}
    }
}

void  CAllOrdersPanel::PopupTradeDlg(IPlatformSingleSvr* pSvr,const PlatformStru_TradeInfo& rawData)
{
    if(!pSvr) return;

    m_pTradeInfoDlg=TRADEINFODLG(this);
	if(m_pTradeInfoDlg!=NULL)
    {
        wxString info;

	    PlatformStru_InstrumentInfo SimpleInstrumentInfo;
        pSvr->GetInstrumentInfo(rawData.InstrumentID,SimpleInstrumentInfo);

	    info.reserve(512);
	    info.Printf(LOADFORMATSTRING(CBSOCQP_FORMAT,"%s%s%s%s%s"),//�ı��������%s  %s  %s %s %s
		    rawData.InstrumentID,
		    CFieldValueTool::Direction2String(rawData.Direction,SVR_LANGUAGE).c_str(),
		    CFieldValueTool::OffsetFlag2String(rawData.OffsetFlag,SVR_LANGUAGE).c_str(),
		    Number2String(rawData.Volume).c_str(),
		    Price2String(rawData.Price,SimpleInstrumentInfo.PriceTick).c_str());
	    info = REPLACEFMTSTRING(info.c_str());
	    m_pTradeInfoDlg->ShowTradeMsg(info, -1, LOADSTRING(FILL_MESSAGE), -1, TRUE);

	    LOG_DATA* pData = new LOG_DATA;
	    pData->strTitle = "FILL_MESSAGE";
	    pData->strFormat = "CBSOCQP_FORMAT";
	    pData->vParam.push_back(LOG_PARAM(rawData.InstrumentID));
	    pData->vParam.push_back(LOG_PARAM(rawData.Direction, DIRECTION_TYPE));
	    pData->vParam.push_back(LOG_PARAM(rawData.OffsetFlag, OFFSETFLAG_TYPE));
	    pData->vParam.push_back(LOG_PARAM(Number2String(rawData.Volume)));
	    pData->vParam.push_back(LOG_PARAM(Price2String(rawData.Price,SimpleInstrumentInfo.PriceTick).c_str()));
	    m_pTradeInfoDlg->WriteLog( pData );
	}
}




//CommonBtnHeight

void CAllOrdersPanel::OnSize( wxSizeEvent& event )
{
	wxRect rt=GetRect();
    int Pox,Poy;

	if(m_pwxExtListCtrl)
		m_pwxExtListCtrl->SetSize(0,0,rt.width,rt.height-CommonBtnHeight-2);

    Pox=8;
    Poy=rt.height-2-CommonBtnHeight;

    if(m_WxRadioButton1)
    {
        m_WxRadioButton1->SetPosition(wxPoint(Pox,Poy));
        Pox+=m_WxRadioButton1->GetRect().GetWidth()+8;
    }
    if(m_WxRadioButton2)
    {
        m_WxRadioButton2->SetPosition(wxPoint(Pox,Poy));
        Pox+=m_WxRadioButton2->GetRect().GetWidth()+8;
    }
    if(m_WxRadioButton3)
    {
        m_WxRadioButton3->SetPosition(wxPoint(Pox,Poy));
        Pox+=m_WxRadioButton3->GetRect().GetWidth()+8;
    }
    if(m_WxRadioButton4)
    {
        m_WxRadioButton4->SetPosition(wxPoint(Pox,Poy));
        Pox+=m_WxRadioButton4->GetRect().GetWidth()+8;
    }

    Pox+=8;

    if(m_pButtonReQry)
    {
        m_pButtonReQry->SetPosition(wxPoint(Pox,Poy));
        Pox+=m_pButtonReQry->GetRect().GetWidth()+8;
    }

    Pox+=8;

    if(m_pButtonRemove)
    {
        m_pButtonRemove->SetPosition(wxPoint(Pox,Poy));
        Pox+=m_pButtonRemove->GetRect().GetWidth()+8;
    }
    if(m_pButtonRemoveAll)
    {
        m_pButtonRemoveAll->SetPosition(wxPoint(Pox,Poy));
        Pox+=m_pButtonRemoveAll->GetRect().GetWidth()+8;
    }
}

/*������ť�¼���Ӧ*/
void CAllOrdersPanel::OnRemove(wxCommandEvent& evt)
{
    int SelectItemId = m_pwxExtListCtrl->GetNextItem(-1, wxLISTEXT_NEXT_ALL, wxLISTEXT_STATE_SELECTED);
    if(SelectItemId<0)
    {
        wxMessageBox(LOADSTRING(SELECT_NOTHING),LOADSTRING(SELECT_TITLE),wxOK|wxICON_ERROR);
        return;
    }

    wxExtListItem  item;
    wxString strMsg,strInstrumentID;
    item.SetId(SelectItemId);
    item.SetMask(wxLISTEXT_MASK_DATA);
    m_pwxExtListCtrl ->GetItem( item );

    PlatformStru_OrderInfo OrderInfo;

    if(!m_pwxExtListCtrl->GetValueByRow(SelectItemId,OrderInfo))
        return;

    IPlatformSingleSvr* pSvr = IS_MULTIACCOUNT_VERSION ? g_pPlatformMgr->GetPlatformSvr(string(OrderInfo.Account)) : DEFAULT_SVR();
    if(!pSvr) return;

    switch(OrderInfo.OrderStatus)
    {
		case THOST_FTDC_OST_Unknown:
        case THOST_FTDC_OST_PartTradedQueueing:
        case THOST_FTDC_OST_NoTradeQueueing:
        //case THOST_FTDC_OST_NotTouched:
	    {
			CLocalOrderService* poService = CLocalOrderService::GetInstance();
			if(poService!=NULL)
			{
				poService->LockObject();
				CLocalOrderService::MAPPLORDER plMap = poService->GetOrderPLMap();
				poService->UnlockObject();
				CLocalOrderService::MAPPLORDERITEM it = plMap.begin();
				for(; it!=plMap.end(); it++)
				{
					if(strncmp(it->second.ref.OrderSysID, OrderInfo.OrderSysID, sizeof(OrderInfo.OrderSysID))==0)
					{
						if(wxMessageBox(LOADSTRING(MSG_CANCELPLORDER_CONTENT), 
								LOADSTRING(MSG_CANCELPLORDER_TITLE), wxYES_NO|wxICON_QUESTION)!=wxYES)
							return;
					}
				}
			}


            wxString strmsg,strcaption;
		    PlatformStru_InstrumentInfo SimpleInstrumentInfo;
            pSvr->GetInstrumentInfo(OrderInfo.InstrumentID,SimpleInstrumentInfo);
		    std::string stdstrprice=Price2String(OrderInfo.LimitPrice,SimpleInstrumentInfo.PriceTick);
		    strmsg.Printf(LOADFORMATSTRING(CANCELQTYPRC_FORMAT,"%s%s%s%s%s%s%s"),//�ı��������%s,%s,%s,%s,%s,%s,%s
			    wxString(OrderInfo.OrderSysID).Trim(true).Trim(false).c_str(),
			    CFieldValueTool::OffsetFlag2String(OrderInfo.CombOffsetFlag[0],SVR_LANGUAGE).c_str(),
			    CFieldValueTool::Direction2String(OrderInfo.Direction,SVR_LANGUAGE).c_str(),
			    OrderInfo.InstrumentID,
			    Number2String(OrderInfo.VolumeTotalOriginal).c_str(),
			    stdstrprice.c_str(),
			    CFieldValueTool::HedgeFlag2String(OrderInfo.CombHedgeFlag[0],SVR_LANGUAGE).c_str()
			    );
		    strmsg = REPLACEFMTSTRING(strmsg.c_str());
		    strcaption=LOADSTRING(TITLE_CANCELORDER_CONFIRM);


		    CfgMgr *pCfgMgr = CFG_MGR_DEFAULT();
            CANCEL_ORDER_CFG cancelOrderCfg;
            if( pCfgMgr) 
            {
                cancelOrderCfg = pCfgMgr->GetCancelOrderCfg(); 
            }
			int ret = wxID_YES;
            if( cancelOrderCfg.bCancelConfirm )
            {
				CZqMessageBox msgbox(this);
				msgbox.ResetMessage(strcaption, strmsg, 
						CZqMessageBox::btnYes|CZqMessageBox::btnNo, CZqMessageBox::cancelOrder);
				ret = msgbox.ShowModal();
				USERLOG_INFO("�û�����%s��ť[����]����ѡ��[%s]ȷ��\n\"\n%s\n%s\n\"", 
						(m_nViewStyle==conOpenOrderStyle?"ί��ģ��":"�ҵ�ģ��"), 
						(ret==wxID_YES?"��":"��"), strcaption, strmsg);
			}
			else
				USERLOG_INFO("�û�����%s��ť[����]��������ȷ��\n\"\n%s\n%s\n\"", 
						(m_nViewStyle==conOpenOrderStyle?"ί��ģ��":"�ҵ�ģ��"), 
						strcaption, strmsg);

			if(ret==wxID_YES)
			{
				PlatformStru_InputOrderAction ReqData;
                ReqData.Thost.FrontID=OrderInfo.FrontID;
                ReqData.Thost.SessionID=OrderInfo.SessionID;
                memcpy(ReqData.Thost.OrderRef, OrderInfo.OrderRef, sizeof(OrderInfo.OrderRef));
                memcpy(ReqData.Thost.OrderSysID, OrderInfo.OrderSysID, sizeof(OrderInfo.OrderSysID));
                memcpy(ReqData.Thost.ExchangeID, OrderInfo.ExchangeID, sizeof(OrderInfo.ExchangeID));
				memcpy(ReqData.Thost.InstrumentID, OrderInfo.InstrumentID, sizeof(OrderInfo.InstrumentID));

			    // ��Ҫ�����ı��浽Map��
			    string strKey;
			    strKey = ReqData.Thost.ExchangeID;
			    strKey += ",";
			    strKey += ReqData.Thost.OrderSysID;
			    m_mapCancelWait[strKey] = GetTickCount();

                int ret=pSvr->ReqOrderAction(ReqData);
            }
            break;
        }
        default:
	    {
		    ShowTradeInfoDlg("OIP_ERROR", "CANCELORDER_ERROE_SINGLE", NeedCancelFailConfirm());
    	    break;
	    }
    }
}


/*ȫ����ť�¼���Ӧ*/
void CAllOrdersPanel::OnRemoveAll(wxCommandEvent& evt)
{
    int SelectItemId = 0, iItemCount = 0;
    wxExtListItem  item;
    wxString strMsg,strInstrumentID;

    iItemCount = m_pwxExtListCtrl->GetItemCount();
    if (iItemCount <= 0) 
    {
        wxMessageBox(LOADSTRING(SELECT_NOTHING),LOADSTRING(SELECT_TITLE),wxOK|wxICON_ERROR);
        return;
    }

    wxString strmsg,strmsgall,strcaption;
	strcaption=LOADSTRING(TITLE_CANCELORDER_CONFIRM);
    vector<PlatformStru_OrderInfo> selectedOrder;//ѡ�е��б��������ȫ��

    for ( SelectItemId = 0; SelectItemId < iItemCount; SelectItemId++ )	
    {
        item.SetId(SelectItemId);
        item.SetMask(wxLISTEXT_MASK_DATA);
        m_pwxExtListCtrl ->GetItem( item );

        PlatformStru_OrderInfo OrderInfo;
        if(!m_pwxExtListCtrl->GetValueByRow(SelectItemId,OrderInfo))
            continue;

        IPlatformSingleSvr* pSvr = IS_MULTIACCOUNT_VERSION ? g_pPlatformMgr->GetPlatformSvr(string(OrderInfo.Account)) : DEFAULT_SVR();
        if(!pSvr) continue;

        switch(OrderInfo.OrderStatus)
        {
			case THOST_FTDC_OST_Unknown:
            case THOST_FTDC_OST_PartTradedQueueing:
            case THOST_FTDC_OST_NoTradeQueueing:		
            {
				PlatformStru_InstrumentInfo SimpleInstrumentInfo;
	            pSvr->GetInstrumentInfo(OrderInfo.InstrumentID,SimpleInstrumentInfo);
				std::string stdstrprice=Price2String(OrderInfo.LimitPrice,SimpleInstrumentInfo.PriceTick);
				strmsg.Printf(LOADFORMATSTRING(CANCELQTYPRC_FORMAT,"%s%s%s%s%s%s%s"),//�ı��������%s,%s,%s,%s,%s,%s,%s
						wxString(OrderInfo.OrderSysID).Trim(true).Trim(false).c_str(),
						CFieldValueTool::OffsetFlag2String(OrderInfo.CombOffsetFlag[0],SVR_LANGUAGE).c_str(),
						CFieldValueTool::Direction2String(OrderInfo.Direction,SVR_LANGUAGE).c_str(),
						OrderInfo.InstrumentID,
						Number2String(OrderInfo.VolumeTotalOriginal).c_str(),
						stdstrprice.c_str(),
						CFieldValueTool::HedgeFlag2String(OrderInfo.CombHedgeFlag[0],SVR_LANGUAGE).c_str()
						); 
				strmsg = REPLACEFMTSTRING(strmsg.c_str());
                strmsgall+=strmsg;
                selectedOrder.push_back(OrderInfo);
                break;
            }
            default:
                break;
        }
    }
    if(selectedOrder.size()==0) 
    {
        wxMessageBox(LOADSTRING(SELECT_NOTHING),LOADSTRING(SELECT_TITLE),wxOK|wxICON_ERROR);
        return;
    }
	CfgMgr *pCfgMgr = CFG_MGR_DEFAULT();
	CANCEL_ORDER_CFG cancelOrderCfg;
	if( pCfgMgr) 
	{
		cancelOrderCfg = pCfgMgr->GetCancelOrderCfg(); 
	}
	int ret = wxYES;
	if( cancelOrderCfg.bCancelConfirm )
	{ 
		COrderMessageDlg dlg(NULL);
		dlg.SetTitle(strcaption);
		strmsgall.Replace("\n", "\r\n");
		dlg.SetOrderInfo(strmsgall);
		ret=dlg.ShowModal();
		USERLOG_INFO("�û�����%s��ť[ȫ��]����ѡ��[%s]ȷ��\n\"\n%s\n%s\n\"", 
				(m_nViewStyle==conOpenOrderStyle?"ί��ģ��":"�ҵ�ģ��"), 
				(ret==wxYES?"��":"��"), strcaption, strmsgall);
	}
	else
		USERLOG_INFO("�û�����%s��ť[ȫ��]���Ҳ���Ҫȷ��\n\"\n%s\n%s\n\"", 
				(m_nViewStyle==conOpenOrderStyle?"ί��ģ��":"�ҵ�ģ��"), 
				(ret==wxYES?"��":"��"), strcaption, strmsgall);

    if(ret==wxYES)
    {
        PlatformStru_InputOrderAction ReqData;
        vector<PlatformStru_OrderInfo>::iterator it=selectedOrder.begin();
		if(it!=selectedOrder.end()) 
        {
			for(; it!=selectedOrder.end(); ++it)
			{
                IPlatformSingleSvr* pSvr = IS_MULTIACCOUNT_VERSION ? g_pPlatformMgr->GetPlatformSvr(string(it->Account)) : DEFAULT_SVR();
                if(!pSvr) continue;

				ReqData.Thost.FrontID=it->FrontID;
				ReqData.Thost.SessionID=it->SessionID;
				memcpy(ReqData.Thost.OrderRef, it->OrderRef, sizeof(it->OrderRef));
				memcpy(ReqData.Thost.OrderSysID, it->OrderSysID, sizeof(it->OrderSysID));
				memcpy(ReqData.Thost.ExchangeID, it->ExchangeID, sizeof(it->ExchangeID));
				memcpy(ReqData.Thost.InstrumentID, it->InstrumentID, sizeof(it->InstrumentID));

				// ��Ҫ�����ı��浽Map��
				string strKey;
				strKey = ReqData.Thost.ExchangeID;
				strKey += ",";
				strKey += ReqData.Thost.OrderSysID;
				m_mapCancelWait[strKey] = GetTickCount();

				pSvr->ReqOrderAction(ReqData);
			}
			selectedOrder.clear();
		}
		else 
        {
			ShowTradeInfoDlg("OIP_ERROR", "CANCELORDER_ERROE_MULTI", NeedCancelFailConfirm());
		}
    }
}

//�����ļ������仯
void CAllOrdersPanel::OnCfgChanged(wxCommandEvent& evt)
{
	WXLOG_INFO("CAllOrdersPanel::OnCfgChanged");

    Init();

	if(m_pButtonRemove)
		m_pButtonRemove->Show(!NeedHideCancelButton());
	if(m_pButtonRemoveAll)
		m_pButtonRemoveAll->Show(!NeedHideCancelButton());
}

void CAllOrdersPanel::Init()
{
	m_pwxExtListCtrl->Clear();
	if(m_pwxExtListCtrl->GetColumnCount()>0) m_pwxExtListCtrl->DeleteAllColumns();
    m_FieldID2ColumnID.clear();

    CfgMgr *pCfgMgr = CFG_MGR_DEFAULT();
    if(!pCfgMgr) return;
    m_pListCfg = pCfgMgr->GetListCfg(m_nGID);
    if(!m_pListCfg) return;

    //����LIST��������
    m_HeadBackgroundColor = wxColor(m_pListCfg->HeadBackgroundColor);
    m_HeadColor = wxColor(m_pListCfg->HeadColor);
    m_BackgroundColor = wxColor(m_pListCfg->BackgroundColor);
    m_TextColor = wxColor(m_pListCfg->TextColor);
    m_TextColor2=m_pListCfg->TextColor;
    m_Font.SetNativeFontInfoUserDesc(m_pListCfg->szFont); 
    m_EvenLineBgColor=wxColor(m_pListCfg->EvenLineBackgroundColor);
    m_OddLineBgColor= wxColor(m_pListCfg->OddLineBackgroundColor);

    m_pwxExtListCtrl->SetHeaderBackgroundColour(m_HeadBackgroundColor);
    m_pwxExtListCtrl->SetHeaderForegroundColour(m_HeadColor);
    m_pwxExtListCtrl->SetBackgroundColour(m_BackgroundColor);
    m_pwxExtListCtrl->SetForegroundColour(m_TextColor);
    m_pwxExtListCtrl->SetFont(m_Font);
    m_pwxExtListCtrl->SetOddRowColour(m_OddLineBgColor);
    m_pwxExtListCtrl->SetEvenRowColour(m_EvenLineBgColor);

    wxExtListItem col;	
    int nColCount = pCfgMgr->GetListVisibleColCount(m_nGID);
    for(int i = 0; i < nColCount; i++)
    {
        LPLIST_COLUMN_CFG pColCfg = pCfgMgr->GetListVisibleColCfg(m_nGID, i);
        if(!pColCfg) continue;

        //"��Լ" "�������"���������
        if(pColCfg->id == OPENORDER_OrderLocalID || pColCfg->id == OPENORDER_InstrumentID || 
				(pColCfg->id == ALLORDER_InstrumentID) || (pColCfg->id == ALLORDER_OrderLocalID) )
            col.SetAlign(wxLISTEXT_FORMAT_LEFT);
        else//����ģ��Ҷ���
            col.SetAlign(wxLISTEXT_FORMAT_RIGHT);
	
        col.SetText(pCfgMgr->GetColCfgCaption(m_nGID,pColCfg->id));
        col.SetWidth(pColCfg->Width);
        col.SetTextColour(pColCfg->TextColor);
		col.SetColData(pColCfg->id);

        m_pwxExtListCtrl->InsertColumn(i, col); 

        m_FieldID2ColumnID[pColCfg->id] = i;
    }

    m_pwxExtListCtrl->SetCfg(m_TextColor2,m_FieldID2ColumnID);


    ShowAll();

    //ֱ�Ӷ����¼��������ʼ����ͬ����������
	wxCommandEvent myEvent(wxEVT_SUBSCRIBE);
	myEvent.SetInt(1);
    OnSubscrible(myEvent);

    return ;
}




//˫������
void CAllOrdersPanel::OnActivated(wxExtListEvent& event)
{
    CfgMgr *pCfgMgr = CFG_MGR_DEFAULT(); 
    if( pCfgMgr == NULL) return;
    CANCEL_ORDER_CFG cancelOrderCfg = pCfgMgr->GetCancelOrderCfg();
    if(cancelOrderCfg.bDoubleClickMouseCancel)
    {
        wxCommandEvent evt(wxEVT_COMMAND_BUTTON_CLICKED,ID_BUTTON_REMOVE);
        ProcessEvent(evt);
    }
    event.Skip();
}

void CAllOrdersPanel::OnPanelFocus(wxCommandEvent& evt)
{
	if(m_pwxExtListCtrl&&m_pwxExtListCtrl->GetItemCount())
	{
		m_pwxExtListCtrl->SetFocus();
		long   pos   =   m_pwxExtListCtrl->GetFirstSelected(); 
		if(pos<0)
		{
			m_pwxExtListCtrl->Select(0);
			m_pwxExtListCtrl->Focus(0);
		}
	}
}

void CAllOrdersPanel::OnContext(wxContextMenuEvent& evt)
{
    wxMenu menu;
	BOOL bIsRowCLick = evt.GetId();
    int colcount=m_pwxExtListCtrl->GetColumnCount();
    int rowcount=m_pwxExtListCtrl->GetItemCount();
    if(bIsRowCLick)
    {
		int SelectItemId = m_pwxExtListCtrl->GetNextItem(-1, wxLISTEXT_NEXT_ALL, wxLISTEXT_STATE_SELECTED);
	    if(SelectItemId>=0)
		{
			wxExtListItem  item;
			item.SetId(SelectItemId);
			item.SetMask(wxLISTEXT_MASK_DATA);
			m_pwxExtListCtrl ->GetItem( item );

			PlatformStru_OrderInfo OrderInfo;
			if(m_pwxExtListCtrl->GetValueByRow(SelectItemId,OrderInfo))
			{
				switch(OrderInfo.OrderStatus)
				{
					case THOST_FTDC_OST_PartTradedQueueing:
					case THOST_FTDC_OST_NoTradeQueueing:
						{    
							menu.Append(ID_MENU_REMOVE,LOADSTRING(CONTEXTMENU_REMOVE));
							menu.Append(ID_MENU_REMOVEALL,LOADSTRING(CONTEXTMENU_REMOVEALL));
							menu.Append(ID_MENU_CHANGEORDER,LOADSTRING(CONTEXTMENU_CHANGEORDER));
							menu.AppendSeparator();
						}
						break;
					default:
						break;
				}
			}
        }
    }

	menu.Append(ID_MENU_AUTO_ADJUST_COL_WIDTH, LOADSTRING(CONTEXTMENU_AUTO_ADJUST_COL_WIDTH));
	menu.Append(ID_MENU_EXPORT,LOADSTRING(CONTEXTMENU_EXPORT));
	menu.Append(ID_MENU_COLUMNCONFIG,LOADSTRING(CONTEXTMENU_COLUMNCONFIG));
	if ( bIsRowCLick )
	{
		menu.AppendSeparator();
		INSERT_ADDTOMYSELF_ITEM(menu);
	}

    if(menu.GetMenuItemCount()>0&&DEFAULT_SVR())
    {
        POINT pt;
        ::GetCursorPos(&pt);
        BOOL retcmd=zqPopupMenu(&menu,pt.x,pt.y,this);

		//������ѡ
		CfgMgr *pCfgMgr = CFG_MGR_DEFAULT();
		if( retcmd - ID_MENU_ADDTOMYSELF >= 0 )
		{
			bool bAdded = false;
			for ( int i = 0; i < m_pwxExtListCtrl->GetItemCount(); i++)
			{
				if ( m_pwxExtListCtrl->IsSelected( i ))
				{			
                    PlatformStru_OrderInfo OrderInfo;
                    if(!m_pwxExtListCtrl->GetValueByRow(i,OrderInfo))
                        continue;

					string InstrumentId = OrderInfo.InstrumentID;
					if(pCfgMgr->GetInstrumentGroupMemberCount(retcmd - ID_MENU_ADDTOMYSELF)>=DEFAULT_SVR()->GetGroupMaxContractNum())
					{
						wxMessageBox(LOADSTRING(OVER_LIMIT_OF_GROUP),LOADSTRING(USERLOGINDLG_ERROR),wxOK|wxICON_QUESTION);
						break;
					}
					else
					{
						pCfgMgr->AddInstrument( retcmd - ID_MENU_ADDTOMYSELF, InstrumentId.c_str()); 
						bAdded = true;					
					}
				}
			}

			if ( bAdded ){SEND_CONFIG_CHANGED_EVENT(MSG_INSTRUMENT_GROUP_CHANGE);}
			return;
		}

        switch(retcmd)
        {
		case ID_MENU_AUTO_ADJUST_COL_WIDTH:
			m_pwxExtListCtrl->AutoAdjustColumnWidth();
			SaveColWidth();
			break;
        case ID_MENU_REMOVE://����
            {
                wxCommandEvent evt(wxEVT_REMOVE);	
                this->AddPendingEvent(evt);
            }
            break;
        case ID_MENU_REMOVEALL://ȫ��
            {
                wxCommandEvent evt(wxEVT_REMOVEALL);	
                this->AddPendingEvent(evt);
            }
            break;
        case ID_MENU_CHANGEORDER://���ٸĵ�
			{
				int SelectItemId = m_pwxExtListCtrl->GetNextItem(-1, wxLISTEXT_NEXT_ALL, wxLISTEXT_STATE_SELECTED);
				if(SelectItemId<0)
					return;

				PlatformStru_OrderInfo OrderInfo;

				if(m_pwxExtListCtrl->GetValueByRow(SelectItemId,OrderInfo)) 
				{
					wxCommandEvent request_event;
					if(DEFAULT_SVR()->HaveOrderType(UIOT_CMD_REPLACEORDER))
						request_event.SetEventType(wxEVT_ORDERINSERT_ORDERREPLACE);
					else
						request_event.SetEventType(wxEVT_ORDERINSERT_ORDERCHANGE);
					wxString strCmsStream;
					strCmsStream.Printf("%s,%s,%s",OrderInfo.InvestorID, OrderInfo.ExchangeID, wxString(OrderInfo.OrderSysID));
					if(1)
					{
						DWORD EvtParamID;
						std::string EvtParamStr(strCmsStream.c_str());
						if(CFTEventParam::CreateEventParam(EvtParamID,NULL,&EvtParamStr,NULL,0,2))
						{
							request_event.SetInt((int)EvtParamID);
							GETTOPWINDOW()->AddPendingEvent(request_event);
						}
					}
				}
            }
            break;  
        case ID_MENU_EXPORT://�����б�
            {
                wxString cap;
                switch(m_ShowType)
                {
                    case ALLORDER:
                        cap=LOADSTRING(ALL_ORDER);
                        break;
                    case OPENORDER:
                        cap=LOADSTRING(OPEN_ORDER);
                        break;
                    case TRADEDORDER:
                        cap=LOADSTRING(OIP_FILLED);
                        break;
                    case CANCELORDER:
                        cap=LOADSTRING(CANCEL_FAIL_ORDER);
                        break;
                }
   				LIST_EXPORT_CSV_FILE(cap, m_pwxExtListCtrl);
            }
            break;
        case ID_MENU_COLUMNCONFIG://����������
            switch(m_nViewStyle) 
            { 
            case conAllOrderStyle:
                SEND_LOAD_CONFIG_APP_EVENT(ID_ITEM_ALLORDER);
                break;
            case conOpenOrderStyle:
                SEND_LOAD_CONFIG_APP_EVENT(ID_ITEM_WAITTRADE);
                break;
            }
            break;
        default:
            break;
        }
    }
    evt.Skip();
}

void CAllOrdersPanel::OnPanelCharHook(wxCommandEvent& evt)
{
    wxKeyEvent* pEvent=(wxKeyEvent*)evt.GetClientData();

	if(!pEvent) return;
    wxWindow *win = FindFocus();
    if(win == NULL) {
		evt.Skip();
        return;
    }
	pEvent->SetId(win->GetId());

	BOOL bFocusInPanel = IsFocusInPanel(this);

	// �����xml�ж��Ƿ�Ҫȷ�ϵ�����
	CfgMgr* pMgr = CFG_MGR_DEFAULT();  
	if(pMgr == NULL) return;
	
    int keycode=pEvent->GetKeyCode();
	map<int,string>::iterator itKey;
	//��ü������ַ�����ӳ���
	map<int,string> addr = pMgr->GetShortCutKeyNameMap();
	itKey = addr.find(keycode);
	if(itKey != addr.end()) {
		std::string strKeyName = itKey->second;
		std::map<std::string, int>::iterator itOrder;
		// �����xml�ж��Ƿ�Ҫȷ�ϵ�����
		CANCEL_ORDER_CFG p = pMgr->GetCancelOrderCfg();
		// ���ӳ�����ť��ݼ�
		if(strKeyName.compare(p.szCancelOrderBtnHotKey)==0 && bFocusInPanel 
				&& m_pButtonRemove && m_pButtonRemove->IsShown()) {
			wxCommandEvent newEvent(wxEVT_COMMAND_BUTTON_CLICKED, ID_BUTTON_REMOVE);
			this->AddPendingEvent(newEvent);
			return;
		}
		if(m_nViewStyle==conOpenOrderStyle)
		{
			if(strKeyName.compare(p.szCancelAllOrderBtnHotKey)==0) {
				wxCommandEvent newEvent(wxEVT_COMMAND_BUTTON_CLICKED, ID_BUTTON_REMOVEALL);
				this->AddPendingEvent(newEvent);
				return;
			}
		}
	}

	evt.Skip();
}
#ifdef _USE_MULTI_LANGUAGE
void CAllOrdersPanel::OnLanguageChanged(wxCommandEvent& evt)
{

	if(m_nViewStyle==conAllOrderStyle && m_pwxExtListCtrl) 
	{
		TRANSFORM_COLUMN_TEXT(GID_ALL_ORDER,m_pwxExtListCtrl);
	}
	else if(m_nViewStyle==conOpenOrderStyle && m_pwxExtListCtrl) 
	{
		TRANSFORM_COLUMN_TEXT(GID_OPEN_ORDER,m_pwxExtListCtrl);
	}
	else
		return;
    if(m_WxRadioButton1) m_WxRadioButton1->SetLabel(LOADSTRING(ALL_ORDER));
	if(m_WxRadioButton2) m_WxRadioButton2->SetLabel(LOADSTRING(OPEN_ORDER));
	if(m_WxRadioButton3) m_WxRadioButton3->SetLabel(LOADSTRING(OIP_FILLED));
	if(m_WxRadioButton4) m_WxRadioButton4->SetLabel(LOADSTRING(CANCEL_FAIL_ORDER));
	m_pButtonRemove->SetLabel(LOADSTRING(CONTEXTMENU_REMOVE));
	m_pButtonRemoveAll->SetLabel(LOADSTRING(CONTEXTMENU_REMOVEALL));
	m_pButtonReQry->SetLabel(LOADSTRING(BUTTON_REQRY_CAPTION));
	int TotalRow=m_pwxExtListCtrl->GetItemCount();
	if(TotalRow==0) return;
	CfgMgr *pCfgMgr = CFG_MGR_DEFAULT();  
	wxASSERT(pCfgMgr);
	switch(m_nViewStyle) 
	{ 
		case conOpenOrderStyle:
			for(int row=0;row<TotalRow;row++ )
			{
				std::string strValue;
				for(int i = 0; i < pCfgMgr->GetListVisibleColCount(GID_OPEN_ORDER); i++)
				{
					bool bReplace=true;
					LPLIST_COLUMN_CFG pColCfg =pCfgMgr->GetListVisibleColCfg(GID_OPEN_ORDER, i);
					strValue=m_pwxExtListCtrl->GetSubItemText( row, i);
					if ( pColCfg->id == OPENORDER_Direction)
					{
						strValue=CFieldValueTool::Direction2String(CFieldValueTool::String2Direction(strValue),evt.GetInt());
					}
					else if(pColCfg->id == OPENORDER_StatusMsg)
					{
						int val=CFieldValueTool::String2OrderStatusMsg(strValue);
						if(val>0)
							strValue=CFieldValueTool::OrderStatusMsg2String(val,evt.GetInt());
						else
							bReplace=false;
					}
					else if(pColCfg->id == OPENORDER_OrderPriceType)
					{
						strValue = CFieldValueTool::OrderPriceType2String(CFieldValueTool::String2OrderPriceType(strValue),evt.GetInt());    //�����۸�����
					}
					else if(pColCfg->id == OPENORDER_CombOffsetFlag)
					{
						strValue = CFieldValueTool::OffsetFlag2String(CFieldValueTool::String2OffsetFlag(strValue),evt.GetInt()); 
					}
					else if(pColCfg->id == OPENORDER_OrderStatus)
					{
						strValue =CFieldValueTool::OrderStatus2String(CFieldValueTool::String2OrderStatus(strValue),evt.GetInt());
					}
					else if(pColCfg->id == OPENORDER_CombHedgeFlag)
					{
						strValue =  CFieldValueTool::HedgeFlag2String(CFieldValueTool::String2HedgeFlag(strValue),evt.GetInt());  
					}
					else if(pColCfg->id == OPENORDER_TimeCondition)
					{
						strValue = CFieldValueTool::TimeCondition2String(CFieldValueTool::String2TimeCondition(strValue),evt.GetInt());    //��Ч������
					}
					else if(pColCfg->id == OPENORDER_OrderType)
					{
						strValue = CFieldValueTool::OrderType2String(CFieldValueTool::String2OrderType(strValue),evt.GetInt());    //��������
					}
					else if(pColCfg->id == OPENORDER_UserForceClose)
					{
						strValue = CFieldValueTool::UserForceClose2String(CFieldValueTool::String2UserForceClose(strValue),evt.GetInt());
					}
					else if(pColCfg->id == OPENORDER_OrderSubmitStatus)
					{
						strValue = CFieldValueTool::OrderSubmitStatus2String(CFieldValueTool::String2OrderSubmitStatus(strValue),evt.GetInt()); 
					}
					else
					{
						bReplace=false;
					}
					if(bReplace) m_pwxExtListCtrl->SetItem( row, i, strValue );
				}
			}
			break;
		case conAllOrderStyle:
		default:
			for(int row=0;row<TotalRow;row++ )
			{
				string strValue;
				for(int i = 0; i < pCfgMgr->GetListVisibleColCount(m_nGID); i++)
				{
					bool bReplace=true;
					LPLIST_COLUMN_CFG pColCfg =pCfgMgr->GetListVisibleColCfg(m_nGID, i);
					strValue=m_pwxExtListCtrl->GetSubItemText( row, i);
					//����
					if ( pColCfg->id == ALLORDER_Direction)
					{
						strValue=CFieldValueTool::Direction2String(CFieldValueTool::String2Direction(strValue),evt.GetInt());
					}
					else if(pColCfg->id == ALLORDER_StatusMsg)
					{
						int val=CFieldValueTool::String2OrderStatusMsg(strValue);
						if(val>0)
							strValue=CFieldValueTool::OrderStatusMsg2String(val,evt.GetInt());
						else
							bReplace=false;
					}
					else if(pColCfg->id == ALLORDER_OrderPriceType)
					{
						strValue = CFieldValueTool::OrderPriceType2String(CFieldValueTool::String2OrderPriceType(strValue),evt.GetInt());
					}
					else if(pColCfg->id == ALLORDER_CombOffsetFlag)
					{
						strValue = CFieldValueTool::OffsetFlag2String(CFieldValueTool::String2OffsetFlag(strValue),evt.GetInt()); 
					}
					else if(pColCfg->id == ALLORDER_OrderStatus)
					{
						strValue =CFieldValueTool::OrderStatus2String(CFieldValueTool::String2OrderStatus(strValue),evt.GetInt());
					}
					else if(pColCfg->id == ALLORDER_CombHedgeFlag)
					{
						strValue =  CFieldValueTool::HedgeFlag2String(CFieldValueTool::String2HedgeFlag(strValue),evt.GetInt());  
					}
					else if(pColCfg->id == ALLORDER_TimeCondition)
					{
						strValue = CFieldValueTool::TimeCondition2String(CFieldValueTool::String2TimeCondition(strValue),evt.GetInt());   
					}
					else if(pColCfg->id == ALLORDER_OrderType)
					{
						strValue = CFieldValueTool::OrderType2String(CFieldValueTool::String2OrderType(strValue),evt.GetInt());
					}
					else if(pColCfg->id == ALLORDER_UserForceClose)
					{
						strValue = CFieldValueTool::UserForceClose2String(CFieldValueTool::String2UserForceClose(strValue),evt.GetInt());
					}
					else if(pColCfg->id == ALLORDER_ForceCloseReason)
					{
						strValue = CFieldValueTool::ForceCloseReason2String(CFieldValueTool::String2ForceCloseReason(strValue),evt.GetInt());
					}
					else if(pColCfg->id == ALLORDER_OrderSubmitStatus)
					{	
						strValue = CFieldValueTool::OrderSubmitStatus2String(CFieldValueTool::String2OrderSubmitStatus(strValue),evt.GetInt()); 
					}
					else
					{
						bReplace=false;
					}
					if(bReplace) m_pwxExtListCtrl->SetItem( row, i, strValue );

				}
			}	
			break;
	};
}
#endif
void CAllOrdersPanel::OnColumnResize( wxExtListEvent& event )
{
	CfgMgr* pCfgMgr = CFG_MGR_DEFAULT();
	if ( NULL == pCfgMgr )
	{
		return;
	}

	int nCol = event.GetColumn();
	if ( nCol < 0 || nCol > pCfgMgr->GetListVisibleColCount( m_nGID ))
	{
		return;
	}

	LPLIST_COLUMN_CFG pColCfg = pCfgMgr->GetListVisibleColCfg(m_nGID, nCol);
	pColCfg->Width = m_pwxExtListCtrl->GetColumnWidth( nCol );
}


void CAllOrdersPanel::OnRadioButton(wxCommandEvent& event)
{
	int id=event.GetId();
    char newShowType=m_ShowType;

	switch(id)
	{
	    case ID_WXRADIOBUTTON1:
            newShowType=ALLORDER;
		    break;
	    case ID_WXRADIOBUTTON2:
            newShowType=OPENORDER;
		    break;
	    case ID_WXRADIOBUTTON3:
            newShowType=TRADEDORDER;
		    break;
	    case ID_WXRADIOBUTTON4:
            newShowType=CANCELORDER;
		    break;
	    default:
		    break;
	}
    if(newShowType!=m_ShowType)
    {
        m_ShowType=newShowType;
        if(m_ShowType==ALLORDER||m_ShowType==OPENORDER)
        {
            if(m_pButtonRemove) m_pButtonRemove->Show(true && !NeedHideCancelButton());
            if(m_pButtonRemoveAll) m_pButtonRemoveAll->Show(true && !NeedHideCancelButton());
        }
        else
        {
            if(m_pButtonRemove) m_pButtonRemove->Show(false);
            if(m_pButtonRemoveAll) m_pButtonRemoveAll->Show(false);
        }
        ShowAll();
    }
}



#define UpdateCol_String(FieldName)  \
            if(!bnewline&&memcmp(pOld->FieldName,pNew->FieldName,sizeof(pOld->FieldName))==0) \
                return false;\
            outItem.SetText(wxString(pNew->FieldName));
#define UpdateCol_String3(FieldName,DispName)  \
            if(!bnewline&&memcmp(pOld->FieldName,pNew->FieldName,sizeof(pOld->FieldName))==0) \
                return false;\
            outItem.SetText(wxString(DispName));
#define UpdateCol_Number(FieldName,DispName)  \
            if(!bnewline&&pOld->FieldName==pNew->FieldName)\
                return false;\
            outItem.SetText(wxString(DispName));\
            outItem.SetiColData((int)(pNew->FieldName));
#define UpdateCol_Price(FieldName)  \
            if(!bnewline&&pOld->FieldName==pNew->FieldName) \
                return false;\
            if(pNew->FieldName==util::GetIntInvalidValue()) outItem.SetText("-");\
            else outItem.SetText(Price2String(pNew->FieldName,pNew->PriceTick));\
            outItem.SetdColData(pNew->FieldName);
#define UpdateCol_Int(FieldName)  \
            if(!bnewline&&pOld->FieldName==pNew->FieldName) \
                return false;\
            if(pNew->FieldName==util::GetIntInvalidValue()) tmpstr.Printf("-");\
            else tmpstr.Printf("%d",pNew->FieldName);\
            outItem.SetText(tmpstr);\
            outItem.SetiColData(pNew->FieldName);
#define UpdateCol_Money(FieldName)  \
            if(!bnewline&&pOld->FieldName==pNew->FieldName) \
                return false;\
            if(pNew->FieldName==util::GetDoubleInvalidValue()) tmpstr.Printf("-");\
            else tmpstr.Printf("%.02lf", pNew->FieldName ); \
            outItem.SetText(tmpstr);\
            outItem.SetdColData(pNew->FieldName);
#define UpdateCol_Fund(FieldName)  \
			if(!bnewline&&pOld->FieldName==pNew->FieldName) \
				return false;\
			if(pNew->FieldName==util::GetDoubleInvalidValue()) tmpstr.Printf("-");\
			else tmpstr = GlobalFunc::GetAccountFormatString(pNew->FieldName, 2); \
			outItem.SetText(tmpstr);\
            outItem.SetdColData(pNew->FieldName);


bool CAllOrdersPanel::UpdateListItemCallBack(const void* pNewData,void* pOldData,unsigned long textcol,int FieldID,int ColID,int RowID,bool bnewline,wxExtListItem& outItem)
{
    const PlatformStru_OrderInfo* pNew=(const PlatformStru_OrderInfo*)pNewData;
    PlatformStru_OrderInfo* pOld=(PlatformStru_OrderInfo*)pOldData;
    bool brlt=false;
    wxString tmpstr;

    outItem.SetColumn(ColID);
    outItem.SetId(RowID);
    outItem.SetTextColour(wxColor(textcol));

    switch(FieldID)
    {
        case ALLORDER_OrderLocalID:
            UpdateCol_String3(OrderSysID,wxString(pNew->OrderSysID).Trim(true).Trim(false));
            outItem.SetbtrynumericAsComparing(true);
            return true;
        case ALLORDER_InstrumentID:
            if(!bnewline) return false;
            UpdateCol_String(InstrumentID);
            return true;
        case ALLORDER_Direction:
            UpdateCol_Number(Direction,CFieldValueTool::Direction2String(pNew->Direction,SVR_LANGUAGE));
            if ( pNew->Direction == THOST_FTDC_D_Buy )
                outItem.SetTextColour(wxColor(RGB(255,0,0)));//��ɫ
            else if ( pNew->Direction == THOST_FTDC_D_Sell )
                outItem.SetTextColour(wxColor(RGB(0,128,0)));//��ɫ
            return true;
        case ALLORDER_CombOffsetFlag:
            UpdateCol_Number(CombOffsetFlag[0],CFieldValueTool::OffsetFlag2String(pNew->CombOffsetFlag[0],SVR_LANGUAGE));
            return true;
        case ALLORDER_OrderStatus:
            UpdateCol_Number(OrderStatus,CFieldValueTool::OrderStatus2String(pNew->OrderStatus,SVR_LANGUAGE));
            return true;
        case ALLORDER_LimitPrice:
            UpdateCol_Price(LimitPrice);
            return true;
        case ALLORDER_VolumeTotalOriginal:
            UpdateCol_Int(VolumeTotalOriginal);
            return true;
        case ALLORDER_VolumeTotal:
            UpdateCol_Int(VolumeTotal);
            return true;
        case ALLORDER_VolumeTraded:
            UpdateCol_Int(VolumeTraded);
            return true;
        case ALLORDER_StatusMsg:
        {
            if(!bnewline&&memcmp(pOld->StatusMsg,pNew->StatusMsg,sizeof(pOld->StatusMsg))==0) 
                return false;
            int valStatusMsg=CFieldValueTool::String2OrderStatusMsg(pNew->StatusMsg);
            if(valStatusMsg>0)  tmpstr=CFieldValueTool::OrderStatusMsg2String(valStatusMsg,SVR_LANGUAGE);
            else tmpstr=wxString(pNew->StatusMsg);
            outItem.SetText(tmpstr);
            return true;
        }
        case ALLORDER_InsertTime:
			// InsertTime���ܻ�ı��
            //if(!bnewline) return false;
            UpdateCol_String(InsertTime);
            return true;
        case ALLORDER_d_freezeMargin:
            UpdateCol_Fund(freezeMargin);
            return true;
        case ALLORDER_d_troubleMoney:
            UpdateCol_Fund(troubleMoney);
            return true;
        case ALLORDER_CombHedgeFlag:
            UpdateCol_Number(CombHedgeFlag[0],CFieldValueTool::HedgeFlag2String(pNew->CombHedgeFlag[0],SVR_LANGUAGE));
            return true;
        case ALLORDER_ExchangeID:
            if(!bnewline) return false;
            UpdateCol_String3(ExchangeID,CFieldValueTool::ExchangeID2Name((char*)pNew->ExchangeID));
            return true;
        case ALLORDER_BrokerOrderSeq:
            UpdateCol_Int(BrokerOrderSeq);
            return true;
        case ALLORDER_OrderPriceType:
            UpdateCol_Number(OrderPriceType,CFieldValueTool::OrderPriceType2String(pNew->OrderPriceType,SVR_LANGUAGE));
            return true;
        case ALLORDER_TimeCondition:
            UpdateCol_Number(TimeCondition,CFieldValueTool::TimeCondition2String(pNew->TimeCondition,SVR_LANGUAGE));
            return true;
        case ALLORDER_OrderType:
            UpdateCol_Number(OrderType,CFieldValueTool::OrderType2String(pNew->OrderType,SVR_LANGUAGE));
            return true;
        case ALLORDER_UserForceClose:
            UpdateCol_Number(UserForceClose,CFieldValueTool::UserForceClose2String(pNew->UserForceClose,SVR_LANGUAGE));
            return true;
        case ALLORDER_ForceCloseReason:
            UpdateCol_Number(ForceCloseReason,CFieldValueTool::ForceCloseReason2String(pNew->ForceCloseReason,SVR_LANGUAGE));
            return true;
        case ALLORDER_OrderSubmitStatus:
            UpdateCol_Number(OrderSubmitStatus,CFieldValueTool::OrderSubmitStatus2String(pNew->OrderSubmitStatus,SVR_LANGUAGE));
            return true;
        case ALLORDER_UserProductInfo:
            if(!bnewline) return false;
            UpdateCol_String(UserProductInfo);
            return true;
        case ALLORDER_UpdateTime:
            UpdateCol_String(UpdateTime);
            return true;
        case ALLORDER_d_avgPrice:
            UpdateCol_Price(AvgPrice);
            return true;
        case ALLORDER_Account:
            if(!bnewline) return false;
            UpdateCol_String(Account);
            return true;
	};
	return false;
}

bool CAllOrdersPanel::UpdateOpenListItemCallBack(const void* pNewData,void* pOldData,unsigned long textcol,int FieldID,int ColID,int RowID,bool bnewline,wxExtListItem& outItem)
{
    const PlatformStru_OrderInfo* pNew=(const PlatformStru_OrderInfo*)pNewData;
    PlatformStru_OrderInfo* pOld=(PlatformStru_OrderInfo*)pOldData;
    bool brlt=false;
    wxString tmpstr;

    outItem.SetColumn(ColID);
    outItem.SetId(RowID);
    outItem.SetTextColour(wxColor(textcol));

	// ���￪ʼΪδ�ɽ���
	switch(FieldID)
	{
		case OPENORDER_OrderLocalID:
            if(!bnewline) return false;
            UpdateCol_String(OrderSysID);
            outItem.SetbtrynumericAsComparing(true);
			return true;
		case OPENORDER_InstrumentID:
            if(!bnewline) return false;
            UpdateCol_String(InstrumentID);
			return true;
		case OPENORDER_Direction:
            UpdateCol_Number(Direction,CFieldValueTool::Direction2String(pNew->Direction,SVR_LANGUAGE));
			if(pNew->Direction==THOST_FTDC_D_Buy)
				outItem.SetTextColour(wxColor(RGB(255,0,0)));
			else
				outItem.SetTextColour(wxColor(RGB(0,128,0)));
			return true;
		case OPENORDER_CombOffsetFlag:
            UpdateCol_Number(CombOffsetFlag[0],CFieldValueTool::OffsetFlag2String(pNew->CombOffsetFlag[0],SVR_LANGUAGE));
			return true;
		case OPENORDER_VolumeTotalOriginal:
            UpdateCol_Int(VolumeTotalOriginal);
			return true;
		case OPENORDER_VolumeTotal:
            UpdateCol_Int(VolumeTotal);
			return true;
		case OPENORDER_LimitPrice:
            UpdateCol_Price(LimitPrice);
			return true;
		case OPENORDER_InsertTime:
			// InsertTime���ܻ�ı��
            //if(!bnewline) return false;
            UpdateCol_String(InsertTime);
			return true;
		case OPENORDER_d_freezeMargin:																		//���ᱣ֤��
            UpdateCol_Fund(freezeMargin);
			return true;
		case OPENORDER_d_troubleMoney:																		//����������
            UpdateCol_Fund(troubleMoney);
			return true;
		case OPENORDER_BrokerOrderSeq:																		//���к�
            UpdateCol_Int(BrokerOrderSeq);
			return true;
		case OPENORDER_ExchangeID:
            if(!bnewline) return false;
            UpdateCol_String3(ExchangeID,CFieldValueTool::ExchangeID2Name((char*)pNew->ExchangeID));
			return true;
		case OPENORDER_CombHedgeFlag:																		//Ͷ��
            UpdateCol_Number(CombHedgeFlag[0],CFieldValueTool::HedgeFlag2String(pNew->CombHedgeFlag[0],SVR_LANGUAGE));
			return true;
		case OPENORDER_VolumeTraded:
            UpdateCol_Int(VolumeTraded);
			return true;
		case OPENORDER_OrderPriceType:
            UpdateCol_Number(OrderPriceType,CFieldValueTool::OrderPriceType2String(pNew->OrderPriceType,SVR_LANGUAGE));
			return true;
		case OPENORDER_TimeCondition:
            UpdateCol_Number(TimeCondition,CFieldValueTool::TimeCondition2String(pNew->TimeCondition,SVR_LANGUAGE));
			return true;
		case OPENORDER_OrderType:
            UpdateCol_Number(OrderType,CFieldValueTool::OrderType2String(pNew->OrderType,SVR_LANGUAGE));
			return true;
		case OPENORDER_UpdateTime:
            UpdateCol_String(UpdateTime);
			return true;
		case OPENORDER_d_avgPrice:
			outItem.SetText("");
			return true;
		case OPENORDER_UserForceClose:
            UpdateCol_Number(UserForceClose,CFieldValueTool::UserForceClose2String(pNew->UserForceClose,SVR_LANGUAGE));
			return true;
		case OPENORDER_ForceCloseReason:
            UpdateCol_Number(ForceCloseReason,CFieldValueTool::ForceCloseReason2String(pNew->ForceCloseReason,SVR_LANGUAGE));
			return true;
		case OPENORDER_OrderSubmitStatus:
            UpdateCol_Number(OrderSubmitStatus,CFieldValueTool::OrderSubmitStatus2String(pNew->OrderSubmitStatus,SVR_LANGUAGE));
			return true;
		case OPENORDER_OrderStatus:
            UpdateCol_Number(OrderStatus,CFieldValueTool::OrderStatus2String(pNew->OrderStatus,SVR_LANGUAGE));
			return true;
		case OPENORDER_StatusMsg:																			//��ϸ״̬
		{					
            if(!bnewline&&memcmp(pOld->StatusMsg,pNew->StatusMsg,sizeof(pOld->StatusMsg))==0) 
                return false;
            int valStatusMsg=CFieldValueTool::String2OrderStatusMsg(pNew->StatusMsg);
            if(valStatusMsg>0)  tmpstr=CFieldValueTool::OrderStatusMsg2String(valStatusMsg,SVR_LANGUAGE);
            else tmpstr=wxString(pNew->StatusMsg);
            outItem.SetText(tmpstr);
            return true;
		}
		case OPENORDER_UserProductInfo:
            if(!bnewline) return false;
            UpdateCol_String(UserProductInfo);
			return true;
		case OPENORDER_Price2:
            UpdateCol_String(Price2);
			return true;
		case OPENORDER_ACCOUNT:
            if(!bnewline) return false;
            UpdateCol_String(Account);
			return true;
	};
	return false;
}

void CAllOrdersPanel::SaveColWidth()
{
	CfgMgr* pCfgMgr = CFG_MGR_DEFAULT();
	if ( NULL == pCfgMgr || NULL == m_pwxExtListCtrl )
	{
		return;
	}

	int nColCount = m_pwxExtListCtrl->GetColumnCount();
	if ( nColCount != pCfgMgr->GetListVisibleColCount( m_nGID ))
	{
		return;
	}

	for ( int i = 0; i < nColCount; i++)
	{
		LPLIST_COLUMN_CFG pColCfg = pCfgMgr->GetListVisibleColCfg(m_nGID, i);
		pColCfg->Width = m_pwxExtListCtrl->GetColumnWidth( i );
	}
}

void CAllOrdersPanel::OnMAPlatformAdd(wxCommandEvent& event)
{
	wxCommandEvent myEvent(wxEVT_SUBSCRIBE);
	myEvent.SetInt(1);
	ProcessEvent(myEvent);
	m_pwxExtListCtrl->Clear();
	ShowAll();

}
void CAllOrdersPanel::OnMAPlatformDelete(wxCommandEvent& evt)
{
	wxCommandEvent myEvent(wxEVT_SUBSCRIBE);
	myEvent.SetInt(0);
	ProcessEvent(myEvent);
	myEvent.SetInt(1);
	ProcessEvent(myEvent);
	m_pwxExtListCtrl->Clear();
	ShowAll();
}