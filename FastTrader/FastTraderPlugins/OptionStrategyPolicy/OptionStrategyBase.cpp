#include "StdAfx.h"
#include "OptionStrategyBase.h"
#include "Strategy_GeneralBreak_PopularityUp_BuyStraddle.h"
#include "Strategy_NarrowShake_PopularityDown_SellStraddle.h"
#include "Strategy_NoDown_PopularityDown_SellBearish.h"
#include "Strategy_NoUp_PopularityDown_SellBullish.h"
#include "Strategy_SlowDown_PopularityDown_BullishBear.h"
#include "Strategy_SlowDown_PopularityUp_BearishBear.h"
#include "Strategy_SlowUp_PopularityDown_ArbitrageBearish.h"
#include "Strategy_SlowUp_PopularityUp_ArbitrageBullish.h"
#include "Strategy_SteepDown_PopularityUp_SellBearish.h"
#include "Strategy_SteepUp_PopularityUp_BuyBearish.h"
#include "Strategy_StrongBreak_PopularityUp_BuyStrangle.h"
#include "Strategy_WideShake_PopularityDown_SellStrangle.h"


COptionStrategyBase* COptionStrategyBase::GetOptionStrategy(eStrategyStyle nStrategyStyle)
{
    switch(nStrategyStyle)
    {
        case None:
            return NULL;
        case SteepUp_PopularityUp_BuyBearish:           //	急涨+人气上升：买进看涨期权
            return (COptionStrategyBase*)CStrategy_SteepUp_PopularityUp_BuyBearish::GetInstance();
        case SlowUp_PopularityUp_ArbitrageBullish:      //	缓涨+人气上升：看涨期权牛市套利（买方策略）
            return (COptionStrategyBase*)CStrategy_SlowUp_PopularityUp_ArbitrageBullish::GetInstance();
        case SlowUp_PopularityDown_ArbitrageBearish:    //	缓涨+人气下降：看跌期权牛市套利（卖方策略）
            return (COptionStrategyBase*)CStrategy_SlowUp_PopularityDown_ArbitrageBearish::GetInstance();
        case NoDown_PopularityDown_SellBearish:         //	不跌+人气下降：卖出看跌期权（冒险）
            return (COptionStrategyBase*)CStrategy_NoDown_PopularityDown_SellBearish::GetInstance();
        case SteepDown_PopularityUp_SellBearish:        //	急跌+人气上升：卖出看跌期权
            return (COptionStrategyBase*)CStrategy_SteepDown_PopularityUp_SellBearish::GetInstance();
        case SlowDown_PopularityUp_BearishBear:         //	缓跌+人气上升：看跌期权熊市套利
            return (COptionStrategyBase*)CStrategy_SlowDown_PopularityUp_BearishBear::GetInstance();
        case SlowDown_PopularityDown_BullishBear:       //	缓跌+人气下降：看涨期权熊市套利
            return (COptionStrategyBase*)CStrategy_SlowDown_PopularityDown_BullishBear::GetInstance();
        case NoUp_PopularityDown_SellBullish:           //	不涨+人气下降：卖出看涨期权（冒险）
            return (COptionStrategyBase*)CStrategy_NoUp_PopularityDown_SellBullish::GetInstance();
        case StrongBreak_PopularityUp_BuyStrangle:      //	强势突破+人气上升：买入宽跨式期权
            return (COptionStrategyBase*)CStrategy_StrongBreak_PopularityUp_BuyStrangle::GetInstance();
        case GeneralBreak_PopularityUp_BuyStraddle:     //	一般突破+人气上升：买入跨式期权
            return (COptionStrategyBase*)CStrategy_GeneralBreak_PopularityUp_BuyStraddle::GetInstance();
        case WideShake_PopularityDown_SellStrangle:     //	宽幅震荡+人气下降：卖出宽跨式期权（冒险）
            return (COptionStrategyBase*)CStrategy_WideShake_PopularityDown_SellStrangle::GetInstance();
        case NarrowShake_PopularityDown_SellStraddle:   //	窄幅震荡+人气下降：卖出跨式期权（冒险）
            return (COptionStrategyBase*)CStrategy_NarrowShake_PopularityDown_SellStraddle::GetInstance();
        default:
            return NULL;
    }   

    return NULL;
}

void COptionStrategyBase::DestroyOptionStrategy(eStrategyStyle nStrategyStyle)
{
    switch(nStrategyStyle)
    {
    case SteepUp_PopularityUp_BuyBearish:           //	急涨+人气上升：买进看涨期权
        CStrategy_SteepUp_PopularityUp_BuyBearish::DeleteInstance();
    case SlowUp_PopularityUp_ArbitrageBullish:      //	缓涨+人气上升：看涨期权牛市套利（买方策略）
        CStrategy_SlowUp_PopularityUp_ArbitrageBullish::DeleteInstance();
    case SlowUp_PopularityDown_ArbitrageBearish:    //	缓涨+人气下降：看跌期权牛市套利（卖方策略）
        CStrategy_SlowUp_PopularityDown_ArbitrageBearish::DeleteInstance();
    case NoDown_PopularityDown_SellBearish:         //	不跌+人气下降：卖出看跌期权（冒险）
        CStrategy_NoDown_PopularityDown_SellBearish::DeleteInstance();
    case SteepDown_PopularityUp_SellBearish:        //	急跌+人气上升：卖出看跌期权
        CStrategy_SteepDown_PopularityUp_SellBearish::DeleteInstance();
    case SlowDown_PopularityUp_BearishBear:         //	缓跌+人气上升：看跌期权熊市套利
        CStrategy_SlowDown_PopularityUp_BearishBear::DeleteInstance();
    case SlowDown_PopularityDown_BullishBear:       //	缓跌+人气下降：看涨期权熊市套利
        CStrategy_SlowDown_PopularityDown_BullishBear::DeleteInstance();
    case NoUp_PopularityDown_SellBullish:           //	不涨+人气下降：卖出看涨期权（冒险）
        CStrategy_NoUp_PopularityDown_SellBullish::DeleteInstance();
    case StrongBreak_PopularityUp_BuyStrangle:      //	强势突破+人气上升：买入宽跨式期权
        CStrategy_StrongBreak_PopularityUp_BuyStrangle::DeleteInstance();
    case GeneralBreak_PopularityUp_BuyStraddle:     //	一般突破+人气上升：买入跨式期权
        CStrategy_GeneralBreak_PopularityUp_BuyStraddle::DeleteInstance();
    case WideShake_PopularityDown_SellStrangle:     //	宽幅震荡+人气下降：卖出宽跨式期权（冒险）
        CStrategy_WideShake_PopularityDown_SellStrangle::DeleteInstance();
    case NarrowShake_PopularityDown_SellStraddle:   //	窄幅震荡+人气下降：卖出跨式期权（冒险）
        CStrategy_NarrowShake_PopularityDown_SellStraddle::DeleteInstance();
    } 
}

//bool COptionStrategyBase::GetOptionStrategyOrders(eStrategyStyle nStrategyStyle, vector<sOrder>* outOrders)
//{
//    switch(nStrategyStyle)
//    {
//    case SteepUp_PopularityUp_BuyBearish:           //	急涨+人气上升：买进看涨期权
//    case SlowUp_PopularityUp_ArbitrageBullish:      //	缓涨+人气上升：看涨期权牛市套利（买方策略）
//    case SlowUp_PopularityDown_ArbitrageBearish:    //	缓涨+人气下降：看跌期权牛市套利（卖方策略）
//    case NoDown_PopularityDown_SellBearish:         //	不跌+人气下降：卖出看跌期权（冒险）
//    case SteepDown_PopularityUp_SellBearish:        //	急跌+人气上升：卖出看跌期权
//    case SlowDown_PopularityUp_BearishBear:         //	缓跌+人气上升：看跌期权熊市套利
//    case SlowDown_PopularityDown_BullishBear:       //	缓跌+人气下降：看涨期权熊市套利
//    case NoUp_PopularityDown_SellBullish:           //	不涨+人气下降：卖出看涨期权（冒险）
//    case StrongBreak_PopularityUp_BuyStrangle:      //	强势突破+人气上升：买入宽跨式期权
//    case GeneralBreak_PopularityUp_BuyStraddle:     //	一般突破+人气上升：买入跨式期权
//    case WideShake_PopularityDown_SellStrangle:     //	宽幅震荡+人气下降：卖出宽跨式期权（冒险）
//    case NarrowShake_PopularityDown_SellStraddle:   //	窄幅震荡+人气下降：卖出跨式期权（冒险）
//        return true;
//    } 
//
//    return false;
//};