#include "stdafx.h"
#include <iostream>
#include "WriteLog.h"
#include "DBAccess.h"
#include "Guard.h"

using namespace std;
using namespace oracle::occi;

const int nMaxInsertRow = 10000;
static bool s_bConn = false;

DBAccess::DBAccess()
: m_pEnvironment(NULL)
, m_pCon(NULL)
, m_pStmt(NULL)
, m_pRes(NULL)
, m_strLastError("")
, m_nErrorID(-1)
, m_strUserName("")
, m_strPwd("")
, m_strDBName("")
, m_pWriteLog(NULL)
{
	CGuard guard(&m_mutex);
	m_pWriteLog = new CWriteLog(WriteLogMode_LOCALFILE, "DBAccess.log");
}

DBAccess::~DBAccess(void)
{
	CGuard guard(&m_mutex);
	DisConnected();

	if ( m_pWriteLog != NULL )
	{
		delete m_pWriteLog;
		m_pWriteLog = NULL;
	}
}

void DBAccess::InitDB( const string& strUserName, const string& strPwd, const string& strDBName )
{
	CGuard guard(&m_mutex);
	m_strUserName = strUserName;
	m_strPwd = strPwd;
	m_strDBName = strDBName;
}

bool DBAccess::Conncect()
{
	char szBuffer[1024];
	memset(szBuffer, 0, sizeof(szBuffer));
	sprintf(szBuffer, "%s, %s, %s", m_strUserName.c_str(), m_strPwd.c_str(), m_strDBName.c_str());
	WriteLog("", szBuffer);
	if ( m_strUserName.empty() || m_strPwd.empty() || m_strDBName.empty() )
	{
		return false;
	}

	try{
		m_pEnvironment = Environment::createEnvironment(Environment::DEFAULT);
		if ( NULL == m_pEnvironment )
		{
			return false;
		}

		m_pCon = m_pEnvironment->createConnection(m_strUserName, m_strPwd, m_strDBName);
		cout << "test count" << endl;
		WriteLog("", "createConnection\n\n");
		if ( NULL == m_pCon )
		{
			return false;
		}
		else
		{
			WriteLog("", "NotifyFn\n\n");
			m_pCon->setTAFNotify(&NotifyFn, NULL);
		}
		WriteLog("", "end createConnection\n\n");

	}catch(SQLException &e){
		std::cout<<e.what()<<endl;
		m_strLastError = e.what();
		m_nErrorID = 0;
		WriteLog("", m_strLastError);
		return false;
	}
	
	s_bConn = true;
	return true;
}

bool DBAccess::IsConnected()
{
	char szBuffer[1024];
	memset(szBuffer, 0, sizeof(szBuffer));
	sprintf(szBuffer, "判断是不是数据库处于连接状态：%d\n", s_bConn);
	WriteLog("", szBuffer);

	return s_bConn;
}

void DBAccess::DisConnected()
{
	if ( !s_bConn )
	{
		return;
	}

	try
	{
		if ( NULL != m_pCon && NULL != m_pEnvironment )
		{
			m_pEnvironment->terminateConnection(m_pCon);
		}
	}
	catch (SQLException& e)
	{
		std::cout << e.what() << endl;
	}

	s_bConn = false;
}

std::string DBAccess::GetLastErrorString()
{
	CGuard guard(&m_mutex);
	return m_strLastError;
}

int DBAccess::GetLastErrorCode()
{
	CGuard guard(&m_mutex);
	return m_nErrorID;
}

int DBAccess::NotifyFn(oracle::occi::Environment *env, oracle::occi::Connection *conn, 
					   void *ctx, oracle::occi::Connection::FailOverType foType, 
					   oracle::occi::Connection::FailOverEventType foEvent)
{
	OutputDebugString("TAF callback FailOverEventType \n ");
	cout << "TAF callback FailOverEventType " << foEvent << endl;
	switch(foEvent)
	{
	case Connection::FO_BEGIN:
		cout << "TAF callback start reconnecting..." << endl;
		OutputDebugString("TAF callback start reconnecting...\n ");
		s_bConn = false;
		break;
	case Connection::FO_END:
	case Connection::FO_ABORT:
		cout << "TAF callback reconnected successful" << endl;
		OutputDebugString("TAF callback reconnected successful\n ");
		s_bConn = true;
		break;
	case Connection::FO_REAUTH:
		break;
	case Connection::FO_ERROR:
		cout << "Retrying" << endl;
		OutputDebugString("Retrying\n ");
		return FO_RETRY;
	default:
		break;
	}

	return 0; //continue failover

}

void DBAccess::RollBack()
{
	try
	{
		if ( NULL != m_pCon  )
		{
			m_pCon->rollback();
			if ( NULL != m_pStmt )
			{
				m_pCon->terminateStatement(m_pStmt);
			}
		}
	}
	catch (SQLException& e)
	{
		std::cout << e.what() << endl;
	}
}

void DBAccess::WriteLog(const string& strSql, const string& strError)
{
	if ( NULL != m_pWriteLog )
	{
		char szBuffer[MAX_SQL_LENGTH];
		memset(szBuffer, 0, sizeof(szBuffer));
		sprintf_s(szBuffer, MAX_USABLE_SQL_LENGTH, "%s\n%s", strError.c_str(), strSql.c_str());
		m_pWriteLog->WriteLog_Fmt("", WriteLogLevel_DEBUGINFO, szBuffer);
	}
}

bool DBAccess::ExcuteUpdate( const char* pSql, int& nNum )
{
	CGuard guard(&m_mutex);
	if ( NULL == pSql )
	{
		return false;
	}

	if ( !IsConnected() && !Conncect())
	{
		return false;
	}

	try
	{
		m_pStmt = m_pCon->createStatement();
		nNum = m_pStmt->executeUpdate( pSql );
		m_pCon->commit();

		m_pCon->terminateStatement(m_pStmt);
	}catch(oracle::occi::SQLException &e){
		RollBack();
		std::cout<<e.what()<<endl;
		std::cout<<pSql<<endl;
		m_strLastError = e.what();
		m_nErrorID = 0;
		WriteLog(pSql, m_strLastError);
		return false;
	}

	return true;
}

bool DBAccess::Excute( const char* pSql )
{
	CGuard guard(&m_mutex);
	if ( NULL == pSql )
	{
		return false;
	}

	if ( !IsConnected() && !Conncect())
	{
		return false;
	}

	try
	{
		m_pStmt = m_pCon->createStatement();
		m_pStmt->execute( pSql );
		m_pCon->commit();

		m_pCon->terminateStatement(m_pStmt);
	}catch(oracle::occi::SQLException &e){
		RollBack();
		std::cout<<e.what()<<endl;
		std::cout<<pSql<<endl;
		m_strLastError = e.what();
		m_nErrorID = 0;
		WriteLog(pSql, m_strLastError);
		return false;
	}

	return true;
}

bool DBAccess::ExcuteSelect( const char* pSql, int& nRecordNum )
{
	CGuard guard(&m_mutex);
	if ( NULL == pSql )
	{
		return false;
	}

	if ( !IsConnected() && !Conncect())
	{
		return false;
	}

	try
	{
		m_pStmt = m_pCon->createStatement( pSql );
		m_pRes = m_pStmt->executeQuery();
		if ( m_pRes->next())
		{
			nRecordNum = m_pRes->getInt(1);
		}

		m_pStmt->closeResultSet(m_pRes);
		m_pCon->terminateStatement(m_pStmt);
	}catch(oracle::occi::SQLException &e){
		RollBack();
		std::cout<<e.what()<<endl;
		std::cout<<pSql<<endl;
		m_strLastError = e.what();
		m_nErrorID = 0;
		WriteLog(pSql, m_strLastError);
		return false;
	}

	return true;
}

bool DBAccess::InsertAndReturnID( const char* pSql, const char* pIDSql, int& nPKID )
{
	CGuard guard(&m_mutex);
	if ( NULL == pSql || (NULL != pSql && strlen(pSql) == 0) )
	{
		return false;
	}

	if ( NULL == pIDSql || (NULL != pIDSql && strlen(pIDSql) == 0) )
	{
		return false;
	}

	if ( !IsConnected() && !Conncect())
	{
		return false;
	}

	try
	{
		m_pStmt = m_pCon->createStatement( );
		m_pStmt->executeUpdate( pSql );
		m_pRes = m_pStmt->executeQuery( pIDSql );
		if ( m_pRes->next())
		{
			nPKID = m_pRes->getInt(1);
		}

		m_pCon->commit();
		m_pStmt->closeResultSet(m_pRes);
		m_pCon->terminateStatement(m_pStmt);
	}catch(oracle::occi::SQLException &e){
		RollBack();
		std::cout<<e.what()<<endl;
		std::cout<<pSql<<endl;
		m_strLastError = e.what();
		m_nErrorID = 0;
		WriteLog(pSql, m_strLastError);
		return false;
	}

	return true;
}


bool DBAccess::QueryData( const char* pSql, std::vector<std::vector<_variant_t>>& vec/*, int& nErrorCode */)
{	
	CGuard guard(&m_mutex);
//	nErrorCode = CF_ERROR_SUCCESS;
	if ( NULL == pSql || (NULL != pSql && strlen(pSql) == 0) )
	{
		//nErrorCode = CF_ERROR_DATABASE_OTHER_ERROR;
		return false;
	}

	if ( !IsConnected() && !Conncect())
	{
		//nErrorCode = CF_ERROR_DATABASE_OTHER_ERROR;
		return false;
	}

	bool bRet = true;
	try
	{
		m_pStmt = m_pCon->createStatement( );
		m_pRes = m_pStmt->executeQuery( pSql );
		std::vector<MetaData> vMetaData = m_pRes->getColumnListMetaData();
		while ( m_pRes->next())
		{
			//OutputDebugString("new line\n");
			std::vector<_variant_t> vColumn;
			for(size_t i = 0; i < vMetaData.size(); i++)
			{
				/*char s[50] = {0};
				sprintf(s,"new colume %d\n",i+1);
				OutputDebugString(s);*/
				_variant_t var;
				MetaData data = vMetaData[i];
				int nType = data.getInt(MetaData::ATTR_DATA_TYPE);
				switch(nType)
				{

				case OCCIIBDOUBLE:
					{
						var.vt = VT_R8;
						var.dblVal = m_pRes->getBDouble(i+1).value;
						break;
					}


				case OCCI_SQLT_AFC: //ansi char
					{
						std::string strValue = m_pRes->getString(i+1);
						var.vt = VT_I1;
						var.cVal = strValue[0];
						break;
					}
				case OCCI_SQLT_CHR: //char string
					{						
						std::string strValue = m_pRes->getString(i+1);
						var.SetString(strValue.c_str());
					}
					break;
				case OCCI_SQLT_NUM:
					{
						int nScale = data.getInt(MetaData::ATTR_SCALE);
						if ( nScale == 0 )
						{
							//INT
							var.vt = VT_INT;
							var.intVal = m_pRes->getInt(i+1);
						}
						else if ( nScale == -127)
						{
							//DOUBLE 
							var.vt = VT_R8;
							var.dblVal = m_pRes->getDouble(i+1);
						}
						else
						{

						}
						break;
					}
				case OCCI_SQLT_DAT:
					{//日期
						var.vt = VT_DATE;						
						Date dd = m_pRes->getDate(i+1);

						int nYear;
						unsigned int nMonth, nDay, nHour, nMinute, nSecond;
						dd.getDate(nYear, nMonth, nDay, nHour, nMinute, nSecond);
						char szDate[256];
						memset(szDate, 0, sizeof(szDate));
						sprintf(szDate,"%4d-%02d-%02d %02d:%02d:%02d", nYear, nMonth, nDay, nHour, nMinute, nSecond);

						var.SetString(szDate);					
					}
					break;
				case OCCI_SQLT_DATE:
					//Date类型必须被格式化成字符串后返回
					var.SetString("");
					break;
				default:
					break;
				}

				vColumn.push_back(var);
			}

			vec.push_back(vColumn);
		}

		m_pCon->commit();
		m_pStmt->closeResultSet(m_pRes);
		m_pCon->terminateStatement(m_pStmt);
	}catch(oracle::occi::SQLException &e){
		RollBack();
		std::cout<<e.what()<<endl;
		std::cout<<pSql<<endl;
		m_strLastError = e.what();
		m_nErrorID = 0;
		WriteLog(pSql, m_strLastError);
		bRet = false;
	}

	return bRet;
}/*
bool DBAccess::SaveServerOrder(std::string strTable, std::vector<ServerOrder>& vecServerOrder)
{
	CGuard guard(&m_mutex);
	if ( !IsConnected() && !Conncect())
	{
		return false;
	}
	for(int i =0; i< (int)vecServerOrder.size(); i++)
	{
		char szBuffer[MAX_SQL_LENGTH];
		memset(szBuffer, 0, sizeof(szBuffer));
		sprintf(szBuffer, "insert into MONITOR20150127(Cmdid, Seq, Userdata1, userdata2, USERDATA3, USERDATA4, UTC, MILLISECOND,INSERTDBTIME)\
						  values(%d, %d, %d, %d, %d, %d, %d, %d, sysdate),  );
		if(true == Excute(szBuffer))
		 {
				 
		 }
		 else
		{

		}	
	}	

}*/