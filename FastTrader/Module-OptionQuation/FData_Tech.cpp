#include "StdAfx.h"

#include "FData.h"


//技术指标相关
bool CFData::CalcKTechIndex(vector<SKLine>& vecKLine, string strTechIndexNameOrg, string strTechIndexName, STechCalcPara& sTCPara, CTechIndex& pTechIndex, vector<int>* vecConfigDefault)
{
//	CAutoLock l(&m_CritSecVector_TechIndex);	
	pTechIndex.ClearMemory();

	string strOrg = GetTechNamePhrase(strTechIndexNameOrg,  sTCPara.enumPhrase);
	map<string, CTechIndex>::iterator itOrg = m_mapName2TechIndex.find(strOrg);
	if(itOrg != m_mapName2TechIndex.end())
	{		
		pTechIndex = itOrg->second;
		pTechIndex.ClearMemory();
		m_mapName2TechIndex.erase(itOrg);
	}
	string strNowTech = GetTechNamePhrase(strTechIndexName,  sTCPara.enumPhrase);
	map<string, CTechIndex>::iterator it = m_mapName2TechIndex.find(strNowTech);
	if(it != m_mapName2TechIndex.end())
	{		
		pTechIndex = it->second;
		pTechIndex.ClearMemory();
	}
	
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

	Calc(strTechIndexName, vecKLine, sTCPara, vecCofig, pTechIndex);	
	m_mapName2TechIndex.insert(map<string, CTechIndex>::value_type(strNowTech, pTechIndex));
	return true;
}
bool CFData::GetConfigFrmFile(string strTechIndexName, EnumPhrase enumPhrase, vector<int>& vecCofig)
{
	//CAutoLock l(&m_CritSecVector_TechIndex);	
	CString strPath;
	char localPath[256];
	memset(localPath, 0, 256);
	GetModuleFileName( NULL, localPath, 256);
	string filename=localPath;
	size_t splitpos=filename.find_last_of('\\');
	strPath.Format(_T("%s"), filename.substr(0, splitpos+1).c_str());

	CString strDir;
	strDir.Format(_T("%shqData\\hqCfg.xml"), strPath);


	TiXmlDocument* pXmlDocment = new TiXmlDocument( strDir.GetBuffer(strDir.GetLength()));
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
	//CAutoLock l(&m_CritSecVector_TechIndex);	
	string strNowTech = GetTechNamePhrase(strTechIndex,  enumPhrase);
	map<string, vector<int>>::iterator itConfig = m_mapName2Config.find(strNowTech);
	if(itConfig != m_mapName2Config.end())
	{//
		vecConfig = itConfig->second;		
	}
}
void CFData::ClearMemory_TechIndex()
{
	map<string, CTechIndex>::iterator it = m_mapName2TechIndex.begin();
	while(it != m_mapName2TechIndex.end())
	{
		map<string, CTechIndex>::iterator itTemp = it;
		it++;
		CTechIndex& pTemp = itTemp->second;
		pTemp.ClearMemory();
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
void CFData::Calc(string strTechIndexName, vector<SKLine>& vecKLine, STechCalcPara& sTCPara, vector<int>& vecConfig, CTechIndex& pTechIndex)
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
		pTechIndex.Calc_MA(vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == VOL)
	{
		vecConfigText.push_back("MA1");
		vecConfigText.push_back("MA2");
		vecConfigText.push_back("MA3");		
		pTechIndex.Calc_VOL(vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == KDJ)
	{
		vecConfigText.push_back("K");
		vecConfigText.push_back("D");
		vecConfigText.push_back("J");		
		pTechIndex.Calc_KDJ(vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == MACD)
	{
		vecConfigText.push_back("DIFF");
		vecConfigText.push_back("DEA");
		vecConfigText.push_back(MACD);		
		pTechIndex.Calc_MACD(vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == BOLL)
	{
		vecConfigText.push_back("MID");
		vecConfigText.push_back("UPPER");
		vecConfigText.push_back("LOWER");
		vecConfigText.push_back(BOLL);		
		pTechIndex.Calc_BOLL(vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == UOS)
	{		
		vecConfigText.push_back("UOS");
		vecConfigText.push_back("MUOS");
		pTechIndex.Calc_UOS(vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == BIAS)
	{		
		vecConfigText.push_back("BIAS1");
		vecConfigText.push_back("BIAS2");
		vecConfigText.push_back("BIAS3");
		pTechIndex.Calc_BIAS(vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == DMI)
	{		
		vecConfigText.push_back("PDI");
		vecConfigText.push_back("MDI");
		vecConfigText.push_back("ADX");
		vecConfigText.push_back("ADXR");
		pTechIndex.Calc_DMI(vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == PSY)
	{		
		vecConfigText.push_back("PSY");
		pTechIndex.Calc_PSY(vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == ROC)
	{		
		vecConfigText.push_back("ROC");
		vecConfigText.push_back("ROCMA");
		pTechIndex.Calc_ROC(vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == BBI)
	{		
		pTechIndex.Calc_BBI(vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == EXPMA)
	{		
		vecConfigText.push_back("MA1");
		vecConfigText.push_back("MA2");
		vecConfigText.push_back("MA3");
		vecConfigText.push_back("MA4");
		pTechIndex.Calc_EXPMA(vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == DMA)
	{		
		vecConfigText.push_back("DDD");
		vecConfigText.push_back("AMA");
		pTechIndex.Calc_DMA(vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == SAR)
	{		
		pTechIndex.Calc_SAR(vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == TRIX)
	{		
		vecConfigText.push_back("TRIX");
		vecConfigText.push_back("TRMA");
		pTechIndex.Calc_TRIX(vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == MTM)
	{		
		vecConfigText.push_back("MTM");
		vecConfigText.push_back("MTMMA");
		pTechIndex.Calc_MTM(vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == CRTECH)
	{		
		vecConfigText.push_back("CR");
		vecConfigText.push_back("MA1");
		vecConfigText.push_back("MA2");
		vecConfigText.push_back("MA3");
		pTechIndex.Calc_CRTECH(vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == VR)
	{
		vecConfigText.push_back("VR");
		vecConfigText.push_back("MA1");
		pTechIndex.Calc_VR(vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == OBV)
	{		
		pTechIndex.Calc_OBV(vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == ASI)
	{		
		pTechIndex.Calc_ASI(vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == EMV)
	{		
		vecConfigText.push_back("EMV");
		vecConfigText.push_back("EMVA");
		pTechIndex.Calc_EMV(vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == CCI)
	{		
		pTechIndex.Calc_CCI(vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == RSI)
	{		
		vecConfigText.push_back("RSI1");
		vecConfigText.push_back("RSI2");
		vecConfigText.push_back("RSI3");		
		pTechIndex.Calc_RSI(vecKLine, sTCPara, vecConfig, vecConfigText);
	}
	else if(strTechIndexName == CDP)
	{		
		vecConfigText.push_back("CDP");
		vecConfigText.push_back("AH");
		vecConfigText.push_back("AL");
		vecConfigText.push_back("NH");
		vecConfigText.push_back("NL");
		pTechIndex.Calc_CDP(vecKLine, sTCPara, vecConfig, vecConfigText);
	}
}