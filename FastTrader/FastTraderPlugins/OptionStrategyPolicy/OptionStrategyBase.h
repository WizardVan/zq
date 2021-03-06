#pragma once

#include <vector>
using namespace std;


/*

SteepUp     急涨
SteepDown   急跌

SlowUp      缓涨
SlowDown    缓跌

PopularityUp    人气上升  
PopularityDown  人气下降 

Buy         买进
Sell        卖出

Bearish     看跌
Bullish     看涨

Arbitrage   套利  

Bear        熊市

Strong      强势
General     一般

Break       突破

Strangle    宽跨式期权
Straddle    跨式期权

Wide        宽
Narrow      窄
Shake       震荡

SteepUp_PopularityUp_BuyBearish             	急涨+人气上升：买进看涨期权
SlowUp_PopularityUp_ArbitrageBullish        	缓涨+人气上升：看涨期权牛市套利（买方策略）
SlowUp_PopularityDown_ArbitrageBearish      	缓涨+人气下降：看跌期权牛市套利（卖方策略）
NoDown_PopularityDown_SellBearish           	不跌+人气下降：卖出看跌期权（冒险）
SteepDown_PopularityUp_SellBearish          	急跌+人气上升：卖出看跌期权
SlowDown_PopularityUp_BearishBear           	缓跌+人气上升：看跌期权熊市套利
SlowDown_PopularityDown_BullishBear         	缓跌+人气下降：看涨期权熊市套利
NoUp_PopularityDown_SellBullish             	不涨+人气下降：卖出看涨期权（冒险）
StrongBreak_PopularityUp_BuyStrangle        	强势突破+人气上升：买入宽跨式期权
GeneralBreak_PopularityUp_BuyStraddle       	一般突破+人气上升：买入跨式期权
WideShake_PopularityDown_SellStrangle       	宽幅震荡+人气下降：卖出宽跨式期权（冒险）
NarrowShake_PopularityDown_SellStraddle     	窄幅震荡+人气下降：卖出跨式期权（冒险）
*/

struct sOrder
{
    
};

enum eStrategyStyle
{
    None                                   ,
    SteepUp_PopularityUp_BuyBearish        ,     //	急涨+人气上升：买进看涨期权
    SlowUp_PopularityUp_ArbitrageBullish   ,     //	缓涨+人气上升：看涨期权牛市套利（买方策略）
    SlowUp_PopularityDown_ArbitrageBearish ,     //	缓涨+人气下降：看跌期权牛市套利（卖方策略）
    NoDown_PopularityDown_SellBearish      ,     //	不跌+人气下降：卖出看跌期权（冒险）
    SteepDown_PopularityUp_SellBearish     ,     //	急跌+人气上升：卖出看跌期权
    SlowDown_PopularityUp_BearishBear      ,     //	缓跌+人气上升：看跌期权熊市套利
    SlowDown_PopularityDown_BullishBear    ,     //	缓跌+人气下降：看涨期权熊市套利
    NoUp_PopularityDown_SellBullish        ,     //	不涨+人气下降：卖出看涨期权（冒险）
    StrongBreak_PopularityUp_BuyStrangle   ,     //	强势突破+人气上升：买入宽跨式期权
    GeneralBreak_PopularityUp_BuyStraddle  ,     //	一般突破+人气上升：买入跨式期权
    WideShake_PopularityDown_SellStrangle  ,     //	宽幅震荡+人气下降：卖出宽跨式期权（冒险）
    NarrowShake_PopularityDown_SellStraddle      //	窄幅震荡+人气下降：卖出跨式期权（冒险）
};

class __declspec(dllexport) COptionStrategyBase
{
public:
    
    /**
	 * @功能描述: 获取策略实例对象的指针
	 * @返 回 值: 策略实例对象指针
	**/
    static COptionStrategyBase* GetOptionStrategy(eStrategyStyle nStrategyStyle);

    /**
	 * @功能描述: 销毁策略实例对象
	 * @返 回 值: 无
	**/
    static void DestroyOptionStrategy(eStrategyStyle nStrategyStyle);


    virtual bool GetOptionStrategyOrders(vector<sOrder>& outOrders) = 0;
};
