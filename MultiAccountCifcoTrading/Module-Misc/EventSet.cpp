#include "StdAfx.h"
#include "EventSet.h"
DEFINE_EVENT_TYPE(wxEVT_WRITE_USER_LOG)
DEFINE_EVENT_TYPE(wxEVT_CONFIG_CHANGED)
DEFINE_EVENT_TYPE(wxEVT_UPDATE_LIST_TABLE)
DEFINE_EVENT_TYPE(wxEVT_SHOW_PANEL)
DEFINE_EVENT_TYPE(wxEVT_LIST_CHAR_HOOK)
DEFINE_EVENT_TYPE(wxEVT_STYLE_CHANGED)
DEFINE_EVENT_TYPE(wxEVT_GET_STYLE)
DEFINE_EVENT_TYPE(wxEVT_GROUP_CHANGE)
DEFINE_EVENT_TYPE(wxEVT_QUOT_VIEW_CHANGE)
DEFINE_EVENT_TYPE(wxEVT_SOAP_MSG)
DEFINE_EVENT_TYPE(wxEVT_CFG_CHANGE)
DEFINE_EVENT_TYPE(wxEVT_BUYSELL5_SHOW_CHANGE)
DEFINE_EVENT_TYPE(wxEVT_PANEL_FOCUS)
DEFINE_EVENT_TYPE(wxEVT_CHAR_EX)
DEFINE_EVENT_TYPE(wxEVT_SUBSCRIBE)
DEFINE_EVENT_TYPE(wxEVT_COMMAND_READY)
DEFINE_EVENT_TYPE(wxEVT_ALL_ORDER_LIST_UPDATE)
DEFINE_EVENT_TYPE(wxEVT_ALL_TRADE_LIST_UPDATE)
DEFINE_EVENT_TYPE(wxEVT_PANEL_CHAR_HOOK)
DEFINE_EVENT_TYPE(wxEVT_FRONT_DISCONNECTED)
DEFINE_EVENT_TYPE(wxEVT_TRADING_NOTICE)
DEFINE_EVENT_TYPE(wxEVT_INSTRUMENT_STATUS)
DEFINE_EVENT_TYPE(wxEVT_POSITION_LIST_UPDATE)
DEFINE_EVENT_TYPE(wxEVT_POSITION_LIST_TRADE_UPDATE)
DEFINE_EVENT_TYPE(wxEVT_POSITION_DETAIL_LIST_UPDATE)
DEFINE_EVENT_TYPE(wxEVT_POSITION_DETAIL_LIST_TRADE_UPDATE)
DEFINE_EVENT_TYPE(wxEVT_INSTRUMENT_LIST_COMMISSIONRATE_UPDATE)      //查到合约手续费率的事件，对合约列表
DEFINE_EVENT_TYPE(wxEVT_INSTRUMENT_LIST_MARGINRATE_UPDATE)          //查到合约保证金率的事件，对合约列表
DEFINE_EVENT_TYPE(wxEVT_STATIC_LEFT_UP)
DEFINE_EVENT_TYPE(wxEVT_QUOT_PANEL_NEW_QUOT)
DEFINE_EVENT_TYPE(wxEVT_FIREREQQRY)
DEFINE_EVENT_TYPE(wxEVT_RtnTradeRecord)
DEFINE_EVENT_TYPE(wxEVT_RtnTradeHistory)
DEFINE_EVENT_TYPE(wxEVT_ORDER_DELETE_RESULT)
DEFINE_EVENT_TYPE(wxEVT_ORDER_INSERT_PANEL_INSTRUMENT_ID)       //传递合约ID给OrderInsertPanel
DEFINE_EVENT_TYPE(wxEVT_ORDER_INSERT_PANEL_NEW_ORDER)
DEFINE_EVENT_TYPE(wxEVT_ORDER_DELETE)
DEFINE_EVENT_TYPE(wxEVT_CODECLICKED)
DEFINE_EVENT_TYPE(wxEVT_PRICECLICKED)
DEFINE_EVENT_TYPE(wxEVT_VOLUMECLICKED)
DEFINE_EVENT_TYPE(wxEVT_JSDPANEL_INSTRUMENTID_CHANGED)
DEFINE_EVENT_TYPE(wxEVT_JSDPANEL_AUTOOPENCLOSE_CHANGED)
DEFINE_EVENT_TYPE(wxEVT_JSDPANEL_AUTOTRACKPRICE_CHANGED)
DEFINE_EVENT_TYPE(wxEVT_KQPANEL_INSTRUMENTID_CHANGED)
DEFINE_EVENT_TYPE(wxEVT_KQPANEL_AUTOOPENCLOSE_CHANGED)
DEFINE_EVENT_TYPE(wxEVT_KQPANEL_AUTOTRACKPRICE_CHANGED)
DEFINE_EVENT_TYPE(wxEVT_STDPANEL_INSTRUMENTID_CHANGED)
DEFINE_EVENT_TYPE(wxEVT_STDPANEL_AUTOOPENCLOSE_CHANGED)
DEFINE_EVENT_TYPE(wxEVT_STDPANEL_AUTOTRACKPRICE_CHANGED)
DEFINE_EVENT_TYPE(wxEVT_OPEN_ORDER_RSP_ORDER)
DEFINE_EVENT_TYPE(wxEVT_OPEN_ORDER_RSP_TRADE)
DEFINE_EVENT_TYPE(wxEVT_TRANSFER_RSP)
DEFINE_EVENT_TYPE(wxEVT_HisCal)
DEFINE_EVENT_TYPE(wxEVT_TradePwdChange)
DEFINE_EVENT_TYPE(wxEVT_FundPwdChange)
DEFINE_EVENT_TYPE(wxEVT_ORDERSERVICE_THREAD_NEW_QUOT)
DEFINE_EVENT_TYPE(wxEVT_ORDERSERVICE_THREAD_NEW_ORDER)
DEFINE_EVENT_TYPE(wxEVT_ORDERSERVICE_THREAD_NEW_TRADE)
DEFINE_EVENT_TYPE(wxEVT_FIVEPRICE_PRICESELECTED)
DEFINE_EVENT_TYPE(wxEVT_TEXT_FOCUS)
DEFINE_EVENT_TYPE(wxEVT_MOUSEINPUT_CODE)
DEFINE_EVENT_TYPE(wxEVT_MOUSEINPUT_DIGIT)
DEFINE_EVENT_TYPE(wxEVT_MOUSEINPUT_VOLUME)
DEFINE_EVENT_TYPE(wxEVT_REMOVE)
DEFINE_EVENT_TYPE(wxEVT_REMOVEALL)
// 下单板有关的公共指令事件
DEFINE_EVENT_TYPE(wxEVT_GRID_INSTRUMENTID_CHANGED)
DEFINE_EVENT_TYPE(wxEVT_ORDERINSERT_VIEWMODE_CHANGED)
DEFINE_EVENT_TYPE(wxEVT_ORDERINSERT_KEYORDER)
DEFINE_EVENT_TYPE(wxEVT_ORDERINSERT_MOUSEORDER)
DEFINE_EVENT_TYPE(wxEVT_ORDERINSERT_MOUSECLOSE)
DEFINE_EVENT_TYPE(wxEVT_ORDERINSERT_ORDERCHANGE)
DEFINE_EVENT_TYPE(wxEVT_ORDERINSERT_ORDERREPLACE)
DEFINE_EVENT_TYPE(wxEVT_ORDERINSERT_INSTRUMENTID_CHANGED)
//报价表事件
//DEFINE_EVENT_TYPE(wxEVT_QUOT_GET_INSTRUMENTID)
DEFINE_EVENT_TYPE(wxEVT_QUOT_SET_GROUP)
//DEFINE_EVENT_TYPE(wxEVT_QUOT_GET_GROUP)
//主窗体响应
DEFINE_EVENT_TYPE(wxEVT_UPDATE_CONFIG_PANE)
DEFINE_EVENT_TYPE(wxEVT_LOAD_CONFIG_APP)
//DEFINE_EVENT_TYPE(wxEVT_GET_PANE_CAPTION)
//DEFINE_EVENT_TYPE(wxEVT_GET_ALL_COMMISSIONRATE)
DEFINE_EVENT_TYPE(wxEVT_SUCESS_TRANSFER)//银期模块转账成功后发送消息给主窗体，主窗体通知资金模块=======

DEFINE_EVENT_TYPE(wxEVT_INSTRUMENT_ASYNCGET)

DEFINE_EVENT_TYPE(wxEVT_KQPANEL_BUYSELLSEL_CHANGED)

DEFINE_EVENT_TYPE(wxEVT_QUERY_PARKED_ORDERINSERT)
DEFINE_EVENT_TYPE(wxEVT_QUERY_PARKED_ORDERACTION)
DEFINE_EVENT_TYPE(wxEVT_QUERY_PARKED_REMOVEPARKEDORDER)
DEFINE_EVENT_TYPE(wxEVT_QUERY_PARKED_REMOVEPARKEDORDERACTION)
DEFINE_EVENT_TYPE(wxEVT_QUERY_PARKED_QUERYPARKED)
DEFINE_EVENT_TYPE(wxEVT_QUERY_PARKED_QUERYPARKEDACTION)

DEFINE_EVENT_TYPE(wxEVT_PLATFORMSVR_STATUSCHANGE)

DEFINE_EVENT_TYPE(wxEVT_QUERY_BATCH_REQUERY)
DEFINE_EVENT_TYPE(wxEVT_QUERY_PARKED_REQUERY)
DEFINE_EVENT_TYPE(wxEVT_QUERY_CONDITION_REQUERY)
DEFINE_EVENT_TYPE(wxEVT_QUERY_STOP_REQUERY)

DEFINE_EVENT_TYPE(wxEVT_CONNECT_LOGIN)

DEFINE_EVENT_TYPE(wxEVT_MOVEPOSITION_TIMER)					// 移仓的Timer事件
DEFINE_EVENT_TYPE(wxEVT_MOVEPOSITION_ORDERLOG)				// 移仓的报单回报Log事件
DEFINE_EVENT_TYPE(wxEVT_MOVEPOSITION_TRADELOG)				// 移仓的成交Log事件
DEFINE_EVENT_TYPE(wxEVT_MOVEPOSITION_ACTION)					// 移仓的自动执行动作Log事件
DEFINE_EVENT_TYPE(wxEVT_SWAPPOSITION_TIMER)					// 仓位互换的Timer事件
DEFINE_EVENT_TYPE(wxEVT_SWAPPOSITION_ORDERLOG)				// 仓位互换的成交Log事件
DEFINE_EVENT_TYPE(wxEVT_SWAPPOSITION_TRADELOG)				// 仓位互换的成交Log事件
DEFINE_EVENT_TYPE(wxEVT_SWAPPOSITION_ACTION)					// 仓位互换的自动执行动作Log事件
DEFINE_EVENT_TYPE(wxEVT_ORDER_LOGON)
DEFINE_EVENT_TYPE(wxEVT_POS_MOVE_SWAP_UPDATE)
DEFINE_EVENT_TYPE(wxEVT_DIALOG_SHOW)
DEFINE_EVENT_TYPE(wxEVT_LANGUAGE_CHANGE)
DEFINE_EVENT_TYPE(wxEVT_CLOSE_PLUSIN)
DEFINE_EVENT_TYPE(wxEVT_OPEN_PLUSIN)
DEFINE_EVENT_TYPE(wxEVT_LOAD_PLUSIN)
DEFINE_EVENT_TYPE(wxEVT_COMMISSIONRATE)
DEFINE_EVENT_TYPE(wxEVT_CONNECTIVITY_STATUS)
//DEFINE_EVENT_TYPE(wxEVT_PLUGIN_NEWQUOTE)
DEFINE_EVENT_TYPE(wxEVT_PLUGIN_HIDE)
DEFINE_EVENT_TYPE(wxEVT_PATSPANEL_ORDERTYPE_CHANGED)
DEFINE_EVENT_TYPE(wxEVT_PLUGIN_SHOW)
DEFINE_EVENT_TYPE(wxEVT_STOP_PLATFORM)
DEFINE_EVENT_TYPE(wxEVT_CALC_FUNDACCOUNT)
DEFINE_EVENT_TYPE(wxEVT_EXCHANGERATE)

DEFINE_EVENT_TYPE(wxEVT_LOCALPARKED_STATUSCHANGED)
DEFINE_EVENT_TYPE(wxEVT_LOCALCONDITION_STATUSCHANGED)
DEFINE_EVENT_TYPE(wxEVT_LOCALPOSITION_STATUSCHANGED)
DEFINE_EVENT_TYPE(wxEVT_DULPLICATE_KEY_PROMPT)
DEFINE_EVENT_TYPE(wxEVT_RECV_MSG)

DEFINE_EVENT_TYPE(wxEVT_ALL_ORDER_LIST_UPDATE_ByRate)
DEFINE_EVENT_TYPE(wxEVT_USER_NOTICE)

DEFINE_EVENT_TYPE(wxEVT_RspQryTradeLast)
DEFINE_EVENT_TYPE(wxEVT_RspQryOrderLast)
DEFINE_EVENT_TYPE(wxEVT_RspQryPositionLast)
DEFINE_EVENT_TYPE(wxEVT_RspQryPositionDetailLast)
DEFINE_EVENT_TYPE(wxEVT_RspQryPositionCombLast)

DEFINE_EVENT_TYPE(wxEVT_RspQryCommissionRate)
DEFINE_EVENT_TYPE(wxEVT_RspQryMarginRate)

DEFINE_EVENT_TYPE(wxEVT_RtnOrder)
DEFINE_EVENT_TYPE(wxEVT_RtnTrade)
DEFINE_EVENT_TYPE(wxEVT_RtnDepthMarketData)

DEFINE_EVENT_TYPE(wxEVT_ALL_ORDER_RspOrderAction1)
DEFINE_EVENT_TYPE(wxEVT_ALL_ORDER_RspOrderAction2)

DEFINE_EVENT_TYPE(wxEVT_MAINFRAME_QUOT_SETFOCUS)
DEFINE_EVENT_TYPE(wxEVT_MAINFRAME_QUOT_SELGROUP)
DEFINE_EVENT_TYPE(wxEVT_SEND_IE_CONTENT)
DEFINE_EVENT_TYPE(wxEVT_RTN_RECONNECTED)
DEFINE_EVENT_TYPE(wxEVT_REQ_REQRY)
DEFINE_EVENT_TYPE(wxEVT_PLUSIN_MENU_CHANGE)
DEFINE_EVENT_TYPE(wxEVT_MA_SOCKET)
DEFINE_EVENT_TYPE(wxEVT_MA_PLAT_MODIFY)
DEFINE_EVENT_TYPE(wxEVT_MA_PLAT_ADD)
DEFINE_EVENT_TYPE(wxEVT_MA_PLAT_DELETE)
DEFINE_EVENT_TYPE(wxEVT_MA_FUND_UPDATE)
DEFINE_EVENT_TYPE(wxEVT_MA_UI_CALLBACK)
DEFINE_EVENT_TYPE(wxEVT_MA_LIST_UPDATE)
DEFINE_EVENT_TYPE(wxEVT_MA_USER_CHANGE)
DEFINE_EVENT_TYPE(wxEVT_MA_ORDERINSERT_ORDERCHANGE)
DEFINE_EVENT_TYPE(wxEVT_MA_ORDERINSERT_MOUSECLOSE)
DEFINE_EVENT_TYPE(wxEVT_QRY_ACCOUNT_DLG)