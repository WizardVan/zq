#include "StdAfx.h"

#include "../../../inc/Quotation/FData.h"
#include "../../../inc/Quotation/KLineView.h"

//技术指标相关
bool CFData::CalcKTechIndex(vector<SKLine *>& m_vecKLine, string strTechIndexNameOrg, string strTechIndexName, STechCalcPara& sTCPara, CTechIndex*& pTechIndex, vector<int>* vecConfigDefault)
{
	CAutoLock l(&m_CritSecVector_TechIndex);	
	string strOrg = GetTechNamePhrase(strTechIndexNameOrg,  sTCPara.enumPhrase);
	map<string, CTechIndex*>::iterator itOrg = m_mapName2TechIndex.find(strOrg);
	if(itOrg != m_mapName2TechIndex.end())
	{
		pTechIndex = itOrg->second;
		pTechIndex->ClearMemory();
		delete pTechIndex;
		m_mapName2TechIndex.erase(itOrg);
	}
	string strNowTech = GetTechNamePhrase(strTechIndexName,  sTCPara.enumPhrase);
	map<string, CTechIndex*>::iterator it = m_mapName2TechIndex.find(strNowTech);
	if(it != m_mapName2TechIndex.end())
	{
		pTechIndex = it->second;
		pTechIndex->ClearMemory();
	}
	else
		pTechIndex = new CTechIndex;
	
	vector<int> vecCofig;
	if(vecConfigDefault != NULL)
	{//有外部传入的配置天数
		vecCofig = *vecConfigDefault;
		m_mapName2Config[strNowTech] = vecCofig;
	}
	else
	{
		map<string, vector<int>>::iterator itConfig = m_mapName2Config.find(strNowTech);
		if(itConfig != m_mapName2Config.end())
		{//
			vecCofig = itConfig->second;		
		}
		if(vecCofig.size() == 0)
		{//读取配置文件，看看有没有被更改过的值
			GetConfigFrmFile(strTechIndexName, sTCPara.enumPhrase, vecCofig);
		}
		if(vecCofig.size() == 0)
		{
			GetDefaultConfigPara(strTechIndexName, vecCofig);			
		}		
		m_mapName2Config.insert(map<string, vector<int>>::value_type(strNowTech, vecCofig));
	}	

	Calc(strTechIndexName, m_vecKLine, sTCPara, vecCofig, pTechIndex);	
	m_mapName2TechIndex.insert(map<string, CTechIndex*>::value_type(strNowTech, pTechIndex));
	return true;
}
bool CFData::GetConfigFrmFile(string strTechIndexName, EnumPhrase enumPhrase, vector<int>& vecCofig)
{
	CAutoLock l(&m_CritSecVector_TechIndex);	
	wxString strPath;
	char localPath[256];
	memset(localPath, 0, 256);
	GetModuleFileName( NULL, localPath, 256);
	string filename=localPath;
	size_t splitpos=filename.find_last_of('\\');
	strPath = filename.substr(0, splitpos+1);

	wxString strDir;
	strDir.Printf(_T("%shqData\\hqCfg.xml"), strPath);


	TiXmlDocument* pXmlDocment = new TiXmlDocument( strDir.c_str());
	if( NULL == pXmlDocment)
	{
		return false;
	}
	std::auto_ptr<TiXmlDocument> ptr( pXmlDocment );
	if( !pXmlDocment->LoadFile() )
	{
		return false;
	}
	TiXmlElement *root = pXmlDocment->RootElement();
	if ( NULL == root )
	{
		return false;
	}
	if( std::string(root->Value()) != "hq")
	{
		return false;
	}
	TiXmlElement *tList = root->FirstChildElement("techindex");
	if(tList == NULL)
		return false;

	TiXmlNode *ListItem = tList->FirstChild(strTechIndexName);
	if(ListItem == NULL)
		return false;
	
	string strNowTech = GetTechNamePhrase(strTechIndexName,  enumPhrase);
	TiXmlNode *ListItemNowTech = ListItem->FirstChild(strNowTech);
	if(ListItemNowTech == NULL)
		return false;
	
	const char *strValue = ListItemNowTech->ToElement()->GetText();
	if(strValue == NULL)
		return false;

	vector<string> v;
	split(strValue, ',', v);
	for(int i =0; i< (int)v.size(); i++)
	{
		int nConfig = atoi(v[i].c_str());
		vecCofig.push_back(nConfig);
	}
	
	return true;
}
void CFData::split(const string& s, char c, vector<string>& v) 
{
	string::size_type i = 0;
	string::size_type j = s.find(c);

	while (j != string::npos) 
	{
		v.push_back(s.substr(i, j-i));
		i = ++j;
		j = s.find(c, j);

		if (j == string::npos)
			v.push_back(s.substr(i, s.length( )));
	}

}
void CFData::GetTechIndexConfig(string strTechIndex, EnumPhrase enumPhrase, vector<int>& vecConfig)
{
	CAutoLock l(&m_CritSecVector_TechIndex);	
	string strNowTech = GetTechNamePhrase(strTechIndex,  enumPhrase);
	map<string, vector<int>>::iterator itConfig = m_mapName2Config.find(strNowTech);
	if(itConfig != m_mapName2Config.end())
	{//
		vecConfig = itConfig->second;		
	}
}
void CFData::SetTechIndexConfig(string strTechIndex, EnumPhrase enumPhrase, vector<int>& vecConfig)
{
	CAutoLock l(&m_CritSecVector_TechIndex);	
	string strNowTech = GetTechNamePhrase(strTechIndex,  enumPhrase);
	map<string, vector<int>>::iterator itConfig = m_mapName2Config.find(strNowTech);
	if(itConfig != m_mapName2Config.end())
	{//
		m_mapName2Config[strNowTech] = vecConfig;
	}
	else
		m_mapName2Config.insert(map<string, vector<int>>::value_type(strNowTech, vecConfig));


	CTechIndex* pTechIndex = NULL;
	map<string, CTechIndex*>::iterator it = m_mapName2TechIndex.find(strNowTech);
	if(it != m_mapName2TechIndex.end())
	{
		pTechIndex = it->second;
		pTechIndex->ClearMemory();
	}
	if(pTechIndex == NULL)
		return;

	STechCalcPara sTCPara;
	sTCPara.enumPhrase = enumPhrase;
	sTCPara.m_VolumeMultiple = m_VolumeMultiple;
	if(strTechIndex == MA)
		pTechIndex->Calc_MA(*m_vecKLineCommon, sTCPara, vecConfig, pTechIndex->m_vecConfigText);
	else if(strTechIndex == VOL)
		pTechIndex->Calc_VOL(*m_vecKLineCommon, sTCPara, vecConfig, pTechIndex->m_vecConfigText);
	else if(strTechIndex == KDJ)
		pTechIndex->Calc_KDJ(*m_vecKLineCommon, sTCPara, vecConfig, pTechIndex->m_vecConfigText);
	else if(strTechIndex == MACD)
		pTechIndex->Calc_MACD(*m_vecKLineCommon, sTCPara, vecConfig, pTechIndex->m_vecConfigText);
	else if(strTechIndex == BOLL)
		pTechIndex->Calc_BOLL(*m_vecKLineCommon, sTCPara, vecConfig, pTechIndex->m_vecConfigText);
	else if(strTechIndex == UOS)
		pTechIndex->Calc_UOS(*m_vecKLineCommon, sTCPara, vecConfig, pTechIndex->m_vecConfigText);
	else if(strTechIndex == BIAS)
		pTechIndex->Calc_BIAS(*m_vecKLineCommon, sTCPara, vecConfig, pTechIndex->m_vecConfigText);
	else if(strTechIndex == DMI)
		pTechIndex->Calc_DMI(*m_vecKLineCommon, sTCPara, vecConfig, pTechIndex->m_vecConfigText);
	else if(strTechIndex == PSY)
		pTechIndex->Calc_PSY(*m_vecKLineCommon, sTCPara, vecConfig, pTechIndex->m_vecConfigText);
	else if(strTechIndex == ROC)
		pTechIndex->Calc_ROC(*m_vecKLineCommon, sTCPara, vecConfig, pTechIndex->m_vecConfigText);
	else if(strTechIndex == BBI)
		pTechIndex->Calc_BBI(*m_vecKLineCommon, sTCPara, vecConfig, pTechIndex->m_vecConfigText);
	else if(strTechIndex == EXPMA)
		pTechIndex->Calc_EXPMA(*m_vecKLineCommon, sTCPara, vecConfig, pTechIndex->m_vecConfigText);
	else if(strTechIndex == DMA)
		pTechIndex->Calc_DMA(*m_vecKLineCommon, sTCPara, vecConfig, pTechIndex->m_vecConfigText);
	else if(strTechIndex == SAR)
		pTechIndex->Calc_SAR(*m_vecKLineCommon, sTCPara, vecConfig, pTechIndex->m_vecConfigText);
	else if(strTechIndex == TRIX)
		pTechIndex->Calc_TRIX(*m_vecKLineCommon, sTCPara, vecConfig, pTechIndex->m_vecConfigText);
	else if(strTechIndex == MTM)
		pTechIndex->Calc_MTM(*m_vecKLineCommon, sTCPara, vecConfig, pTechIndex->m_vecConfigText);
	else if(strTechIndex == CRTECH)
		pTechIndex->Calc_CRTECH(*m_vecKLineCommon, sTCPara, vecConfig, pTechIndex->m_vecConfigText);
	else if(strTechIndex == VR)
		pTechIndex->Calc_VR(*m_vecKLineCommon, sTCPara, vecConfig, pTechIndex->m_vecConfigText);
	else if(strTechIndex == OBV)
		pTechIndex->Calc_OBV(*m_vecKLineCommon, sTCPara, vecConfig, pTechIndex->m_vecConfigText);
	else if(strTechIndex == ASI)
		pTechIndex->Calc_ASI(*m_vecKLineCommon, sTCPara, vecConfig, pTechIndex->m_vecConfigText);
	else if(strTechIndex == EMV)
		pTechIndex->Calc_EMV(*m_vecKLineCommon, sTCPara, vecConfig, pTechIndex->m_vecConfigText);
	else if(strTechIndex == CCI)
		pTechIndex->Calc_CCI(*m_vecKLineCommon, sTCPara, vecConfig, pTechIndex->m_vecConfigText);
	else if(strTechIndex == RSI)
		pTechIndex->Calc_RSI(*m_vecKLineCommon, sTCPara, vecConfig, pTechIndex->m_vecConfigText);
	else if(strTechIndex == CDP)
		pTechIndex->Calc_CDP(*m_vecKLineCommon, sTCPara, vecConfig, pTechIndex->m_vecConfigText);
}
void CFData::ClearMemory_TechIndex()
{
	map<string, CTechIndex*>::iterator it = m_mapName2TechIndex.begin();
	while(it != m_mapName2TechIndex.end())
	{
		map<string, CTechIndex*>::iterator itTemp = it;
		it++;
		CTechIndex *pTemp = itTemp->second;
		if(pTemp)
			pTemp->ClearMemory();
		m_mapName2TechIndex.erase(itTemp);
	}	
}

void CFData::GetDefaultConfigPara(string strTechIndexName, vector<int>& vecCofig)
{
	if(strTechIndexName == MA)
	{
		vecCofig.push_back(5);
		vecCofig.push_back(10);
		vecCofig.push_back(20);
		vecCofig.push_back(30);
		vecCofig.push_back(60);
		vecCofig.push_back(120);
	}
	else if(strTechIndexName == VOL)
	{
		vecCofig.push_back(5);
		vecCofig.push_back(10);
		vecCofig.push_back(20);
	}
	else if(strTechIndexName == KDJ)
	{
		vecCofig.push_back(9);
		vecCofig.push_back(3);
		vecCofig.push_back(3);
	}
	else if(strTechIndexName == MACD)
	{
		vecCofig.push_back(26);
		vecCofig.push_back(12);
		vecCofig.push_back(9);
	}
	else if(strTechIndexName == BOLL)
	{
		vecCofig.push_back(26);
		vecCofig.push_back(2);				
	}
	else if(strTechIndexName == UOS)
	{
		vecCofig.push_back(7);
		vecCofig.push_back(14);	
		vecCofig.push_back(28);	
		vecCofig.push_back(6);	
	}
	else if(strTechIndexName == BIAS)
	{
		vecCofig.push_back(6);
		vecCofig.push_back(12);	
		vecCofig.push_back(24);	
	}
	else if(strTechIndexName == DMI)
	{
		vecCofig.push_back(14);
		vecCofig.push_back(6);	
	}
	else if(strTechIndexName == PSY)
	{
		vecCofig.push_back(12);
	}
	else if(strTechIndexName == ROC)
	{
		vecCofig.push_back(12);
		vecCofig.push_back(6);
	}
	else if(strTechIndexName == BBI)
	{
		vecCofig.push_back(3);
		vecCofig.push_back(6);
		vecCofig.push_back(12);
		vecCofig.push_back(24);
	}
	else if(strTechIndexName == EXPMA)
	{
		vecCofig.push_back(5);
		vecCofig.push_back(10);
		vecCofig.push_back(20);
		vecCofig.push_back(60);
	}
	else if(strTechIndexName == DMA)
	{
		vecCofig.push_back(10);
		vecCofig.push_back(50);
		vecCofig.push_back(10);
	}
	else if(strTechIndexName == SAR)
	{
		vecCofig.push_back(10);
		vecCofig.push_back(2);
		vecCofig.push_back(20);
	}
	else if(strTechIndexName == TRIX)
	{
		vecCofig.push_back(12);		
		vecCofig.push_back(20);
	}
	else if(strTechIndexName == MTM)
	{
		vecCofig.push_back(6);		
		vecCofig.push_back(6);
	}
	else if(strTechIndexName == CRTECH)
	{
		vecCofig.push_back(26);		
		vecCofig.push_back(5);
		vecCofig.push_back(10);		
		vecCofig.push_back(20);
	}
	else if(strTechIndexName == VR)
	{
		vecCofig.push_back(26);		
		vecCofig.push_back(6);		
	}
	else if(strTechIndexName == EMV)
	{
		vecCofig.push_back(14);		
		vecCofig.push_back(9);		
	}
	else if(strTechIndexName == CCI)
	{
		vecCofig.push_back(14);		
	}
	else if(strTechIndexName == RSI)
	{
		vecCofig.push_back(6);
		vecCofig.push_back(12);
		vecCofig.push_back(24);
	}
}
void CFData::Calc(string strTechIndexName, vector<SKLine *>& m_vecKLine, STechCalcPara& sTCPara, vector<int>& vecConfig, CTechIndex*& pTechIndex)
{
	vector<string> vecConfigText;
	if(strTechIndexName == MA)
	{
		vecConfigText.push_back("MA1");
		vecConfigText.push_back("MA2");
		vecConfigText.push_back("MA3");
		vecConfigText.push_back("MA4");
		vecConfigText.push_back("MA5");
		vecConfigText.push_back("MA6");
		pTechIndex->Calc_MA(m_vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == VOL)
	{
		vecConfigText.push_back("MA1");
		vecConfigText.push_back("MA2");
		vecConfigText.push_back("MA3");		
		pTechIndex->Calc_VOL(m_vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == KDJ)
	{
		vecConfigText.push_back("K");
		vecConfigText.push_back("D");
		vecConfigText.push_back("J");		
		pTechIndex->Calc_KDJ(m_vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == MACD)
	{
		vecConfigText.push_back("DIFF");
		vecConfigText.push_back("DEA");
		vecConfigText.push_back(MACD);		
		pTechIndex->Calc_MACD(m_vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == BOLL)
	{
		vecConfigText.push_back("MID");
		vecConfigText.push_back("UPPER");
		vecConfigText.push_back("LOWER");
		vecConfigText.push_back(BOLL);		
		pTechIndex->Calc_BOLL(m_vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == UOS)
	{		
		vecConfigText.push_back("UOS");
		vecConfigText.push_back("MUOS");
		pTechIndex->Calc_UOS(m_vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == BIAS)
	{		
		vecConfigText.push_back("BIAS1");
		vecConfigText.push_back("BIAS2");
		vecConfigText.push_back("BIAS3");
		pTechIndex->Calc_BIAS(m_vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == DMI)
	{		
		vecConfigText.push_back("PDI");
		vecConfigText.push_back("MDI");
		vecConfigText.push_back("ADX");
		vecConfigText.push_back("ADXR");
		pTechIndex->Calc_DMI(m_vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == PSY)
	{		
		vecConfigText.push_back("PSY");
		pTechIndex->Calc_PSY(m_vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == ROC)
	{		
		vecConfigText.push_back("ROC");
		vecConfigText.push_back("ROCMA");
		pTechIndex->Calc_ROC(m_vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == BBI)
	{		
		pTechIndex->Calc_BBI(m_vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == EXPMA)
	{		
		vecConfigText.push_back("MA1");
		vecConfigText.push_back("MA2");
		vecConfigText.push_back("MA3");
		vecConfigText.push_back("MA4");
		pTechIndex->Calc_EXPMA(m_vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == DMA)
	{		
		vecConfigText.push_back("DDD");
		vecConfigText.push_back("AMA");
		pTechIndex->Calc_DMA(m_vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == SAR)
	{		
		pTechIndex->Calc_SAR(m_vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == TRIX)
	{		
		vecConfigText.push_back("TRIX");
		vecConfigText.push_back("TRMA");
		pTechIndex->Calc_TRIX(m_vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == MTM)
	{		
		vecConfigText.push_back("MTM");
		vecConfigText.push_back("MTMMA");
		pTechIndex->Calc_MTM(m_vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == CRTECH)
	{		
		vecConfigText.push_back("CR");
		vecConfigText.push_back("MA1");
		vecConfigText.push_back("MA2");
		vecConfigText.push_back("MA3");
		pTechIndex->Calc_CRTECH(m_vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == VR)
	{
		vecConfigText.push_back("VR");
		vecConfigText.push_back("MA1");
		pTechIndex->Calc_VR(m_vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == OBV)
	{		
		pTechIndex->Calc_OBV(m_vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == ASI)
	{		
		pTechIndex->Calc_ASI(m_vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == EMV)
	{		
		vecConfigText.push_back("EMV");
		vecConfigText.push_back("EMVA");
		pTechIndex->Calc_EMV(m_vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == CCI)
	{		
		pTechIndex->Calc_CCI(m_vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == RSI)
	{		
		vecConfigText.push_back("RSI1");
		vecConfigText.push_back("RSI2");
		vecConfigText.push_back("RSI3");		
		pTechIndex->Calc_RSI(m_vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == CDP)
	{		
		vecConfigText.push_back("CDP");
		vecConfigText.push_back("AH");
		vecConfigText.push_back("AL");
		vecConfigText.push_back("NH");
		vecConfigText.push_back("NL");
		pTechIndex->Calc_CDP(m_vecKLine, sTCPara, vecConfig, vecConfigText);
	}
}