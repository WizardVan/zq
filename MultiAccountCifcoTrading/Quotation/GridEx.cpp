#include "StdAfx.h"

#include "../inc/Quotation/GridEx.h"





IMPLEMENT_DYNAMIC_CLASS(wxGridEx, wxGrid)
DEFINE_EVENT_TYPE(wxEVT_GridEx_KeyDown)
wxGridEx::wxGridEx(void)
{
	m_Leftpanelzhang		= wxColour(192, 0, 0);	
	m_TextColor				= wxColour(192, 192, 192);
	m_nHeadFontSize         = 8;
}
wxGridEx::~wxGridEx(void)
{
	
}
wxGridEx::wxGridEx( wxWindow *parent,
			   wxWindowID id,
			   const wxPoint& pos,
			   const wxSize& size,
			   long style,
			   const wxString& name ):
wxGrid(parent,id,pos,size,style)			 
{	
	m_Leftpanelzhang		= wxColour(192, 0, 0);	
	m_TextColor				= wxColour(192, 192, 192);
	bool created = m_created;
}

BEGIN_EVENT_TABLE(wxGridEx, wxGrid)
EVT_KEY_DOWN( wxGridEx::OnKeyDown )
END_EVENT_TABLE()

void wxGridEx::DrawColLabel( wxDC& dc, int col )
{
	if ( GetColWidth(col) <= 0 || m_colLabelHeight <= 0 )
		return;

	int colLeft = GetColLeft(col);

	wxRect rect;

#if 0
	def __WXGTK20__
		rect.SetX( colLeft + 1 );
	rect.SetY( 1 );
	rect.SetWidth( GetColWidth(col) - 2 );
	rect.SetHeight( m_colLabelHeight - 2 );

	wxWindowDC *win_dc = (wxWindowDC*) &dc;

	wxRendererNative::Get().DrawHeaderButton( win_dc->m_owner, dc, rect, 0 );
#else
	int colRight = GetColRight(col) - 1;
	
	if(col == m_numCols-1)
		colRight = colRight+26;//股东条占16个像素
		//nColRight = m_extraWidth;

	dc.SetPen( wxPen(m_Leftpanelzhang, 1, wxSOLID) );
	if(col == m_numCols-1)
		dc.DrawLine( colRight, 0, colRight, m_colLabelHeight - 1 );
	
	dc.DrawLine( colLeft, 0, colRight+1, 0 );
	dc.DrawLine( colLeft, m_colLabelHeight - 1,
		colRight + 1, m_colLabelHeight - 1 );

	dc.SetPen( m_Leftpanelzhang );
	dc.DrawLine( colLeft, 1, colLeft, m_colLabelHeight - 1 );
	//dc.DrawLine( colLeft, 1, colRight, 1 );
#endif


	dc.SetBackgroundMode( wxTRANSPARENT );
	dc.SetTextForeground( GetLabelTextColour() );

	wxFont wxfont(m_nHeadFontSize, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_LIGHT, false); 	
	dc.SetFont(wxfont);


	int hAlign, vAlign, orient;
	GetColLabelAlignment( &hAlign, &vAlign );
	orient = GetColLabelTextOrientation();

	rect.SetX( colLeft + 2 );
	rect.SetY( 2 );
	rect.SetWidth( GetColWidth(col) - 4 );
	rect.SetHeight( m_colLabelHeight - 4 );
	DrawTextRectangle( dc, GetColLabelValue( col ), rect, hAlign, vAlign, orient );
}
void wxGridEx::OnKeyDown( wxKeyEvent& event )
{
	int nkeyCode = event.GetKeyCode();
	switch ( nkeyCode )
	{
	case WXK_PRIOR:
	case WXK_NEXT:
		{
			wxWindow* window= GetParent();
			wxCommandEvent myEvent(wxEVT_GridEx_KeyDown);
			myEvent.SetInt(nkeyCode);
			window->ProcessEvent(myEvent);
		}
		break;
	default:
		event.Skip();
		break;
	}
}
















//以下BigGridTable类的实现
BigGridTable::BigGridTable(long lRows, long lCols) 
{
	m_rowcount = lRows;		
	m_colcount = lCols;
	//m_pTextData = NULL;
	m_dbLastPrice = 0.0;
	m_clText				= wxColour(192,192,192);
	m_Leftpanelzhang		= wxColour(255, 82, 82);
	m_Leftpaneldie			= wxColour(82, 255, 82);
	m_Leftpanelhengpan		= wxColour(255, 255, 255);
	m_clVolume				= wxColour(255, 255, 82);
	m_pFData = NULL;
	m_nBodyfontSize = 10;
	//m_vecQIntime = NULL;
}

int BigGridTable::GetNumberRows() { return m_rowcount; }
int BigGridTable::GetNumberCols() { return m_colcount; }

wxString BigGridTable::GetValue( int row, int col )
{
	if(m_pFData == NULL)
		return wxEmptyString;

	if(row <0 || row >= (int)(m_pFData->m_vecQIntime).size()
		|| col<0|| col>=6)
		return wxEmptyString;
	wxString strText ="";
	SQIntime * pQIntime = (m_pFData->m_vecQIntime)[row];
	switch(col)
	{
	case 0://分笔
		{
			SQIntime *pItemLast2 = NULL;
			if(row -1>= 0)
				pItemLast2 = (m_pFData->m_vecQIntime)[row-1];
			if(pItemLast2 == NULL)
				strText.Printf(wxT("%.2d:%.2d"), pQIntime->dwTime.tm_hour, pQIntime->dwTime.tm_min);
			else
			{
				if(pQIntime->dwTime.tm_year == pItemLast2->dwTime.tm_year
					&& pQIntime->dwTime.tm_mon == pItemLast2->dwTime.tm_mon 
					&& pQIntime->dwTime.tm_mday == pItemLast2->dwTime.tm_mday
					&& pQIntime->dwTime.tm_hour == pItemLast2->dwTime.tm_hour
					&& pQIntime->dwTime.tm_min == pItemLast2->dwTime.tm_min)
					strText.Printf(wxT("   :%.2d"),  pQIntime->dwTime.tm_sec);			
				else
					strText.Printf(wxT("%.2d:%.2d"), pQIntime->dwTime.tm_hour, pQIntime->dwTime.tm_min);
			}
			break;
		}
	case 1://价格
		{
			wxString strFormat;
			strFormat.Printf(wxT("%%.%df"), m_nNumDigits);
			strText.Printf(strFormat, pQIntime->fLastPrice);					
			break;
		}
	case 2://量
		{
			strText.Printf(wxT("%d"),  pQIntime->dwVolume );
			break;
		}
	case 3://开
		{
			strText.Printf(wxT("%d"),  pQIntime->dwOpenVolume );
			break;
		}
	case 4://平
		{
			strText.Printf(wxT("%d"),  pQIntime->dwCloseVolume );
			break;
		}
	case 5://性质
		{
			switch(pQIntime->nDesc)
			{
			case 0:
				strText = KONGKAI;
				break;
			case 1:
				strText = DUOKAI;
				break;
			case 2:
				strText = DUOPING;
				break;
			case 3:
				strText = KONGPING;
				break;
			case 4:
				strText = DUOHUAN;
				break;
			case 5:
				strText = KONGHUAN;
				break;
			case 6:
				strText = SHUANGPING;
				break;
			case 7:
				strText = SHUANGKAI;
				break;
			}
			//	strText =  pQIntime->strDesc.c_str() ;
			break;
		}
	}
	return strText;
}
/*
性质的区分：
开仓数大于平仓数，则属于开仓。  成交价格属于买一价，则为空开，成交价格为卖一价，则为多开。
开仓数小于平仓数，则属于平仓。	成交价格属于买一价，则为多平，成交价格为卖一价，则为空平。
开仓数等于平仓数，则属于换仓。	成交价格属于买一价，则为多换，成交价格为卖一价，则为空换。
开仓数等于0，则为双平。
平仓数等于0，则为双开。

仓差：开仓量减去平仓量    即对持仓量的即时性增减。

颜色说明：成交价为买一价时，显示为绿色  代表价格下降。
成交价为卖一价时，显示为红色，代表价格上升。//这两点简单处理就是针对上一笔价格的上涨还是下跌判定颜色，上涨为红色，下跌为绿色
成交价为最新价时:
颜色首先根据价格判断，影响量和性质。如果价格不变，则根据性质判断。
多换（红色）       空换（绿色）
双开（红色）       双平（绿色）
空平（红色）       多平（绿色）
多开（红色）       空开（绿色）

*/
wxGridCellAttr* BigGridTable::GetAttr( int row, int col,wxGridCellAttr::wxAttrKind  kind )
{
	wxGridCellAttr *pCellAttr = new wxGridCellAttr(); 
	if(m_pFData == NULL)
		return pCellAttr;
	switch(col)
	{
	case 0://分笔
		{
			bool bBold = false;
			if(row >= 0 && row < (int)(m_pFData->m_vecQIntime).size())
			{
				SQIntime *pItemLast2 = NULL;
				if(row -1>= 0)
					pItemLast2 = (m_pFData->m_vecQIntime)[row-1];
				if(pItemLast2 == NULL)
				{
					wxFont font(m_nBodyfontSize, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false); 	
					pCellAttr->SetFont(font);	
				}
				else
				{
					SQIntime * pQIntime = (m_pFData->m_vecQIntime)[row];
					if(pQIntime->dwTime.tm_year == pItemLast2->dwTime.tm_year
						&& pQIntime->dwTime.tm_mon == pItemLast2->dwTime.tm_mon 
						&& pQIntime->dwTime.tm_mday == pItemLast2->dwTime.tm_mday
						&& pQIntime->dwTime.tm_hour == pItemLast2->dwTime.tm_hour
						&& pQIntime->dwTime.tm_min == pItemLast2->dwTime.tm_min)
					{

					}	
					else
					{
						bBold = true;
						wxFont font(m_nBodyfontSize, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD, false); 	
						pCellAttr->SetFont(font);	
					}
				}
				
			}
			if(!bBold)	
			{
				wxFont font(m_nBodyfontSize-1, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_LIGHT, false); 	
				pCellAttr->SetFont(font);	
			}
			pCellAttr->SetTextColour(m_clText);
			pCellAttr->SetAlignment(wxALIGN_RIGHT,-1);	
			break;
		}
	case 1://价格
		{
			if(row >=0 && row < (int)(m_pFData->m_vecQIntime).size())
			{
				double dbPrice = m_dbLastPrice;
				SQIntime *pItemLast = (m_pFData->m_vecQIntime)[row];
				if(pItemLast->fLastPrice > dbPrice)
					pCellAttr->SetTextColour(m_Leftpanelzhang);					
				else if(pItemLast->fLastPrice < dbPrice)
					pCellAttr->SetTextColour(m_Leftpaneldie);					
				else 
					pCellAttr->SetTextColour(m_Leftpanelhengpan);		

				wxFont font(m_nBodyfontSize, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_LIGHT, false); 	
				pCellAttr->SetFont(font);	
				pCellAttr->SetAlignment(wxALIGN_RIGHT,-1);	

			}				
			break;
		}
	case 2://量
		{
			wxFont font(m_nBodyfontSize-1, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_LIGHT, false); 	
			pCellAttr->SetFont(font);	
			SetVolumeDescColor(row, pCellAttr);
			pCellAttr->SetAlignment(wxALIGN_RIGHT,-1);	
			break;
		}
	case 3://开
		{
			wxFont font(m_nBodyfontSize-1, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_LIGHT, false); 	
			pCellAttr->SetFont(font);	
			pCellAttr->SetTextColour(m_clVolume);
			pCellAttr->SetAlignment(wxALIGN_RIGHT,-1);	
			break;
		}
	case 4://平
		{
			wxFont font(m_nBodyfontSize-1, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_LIGHT, false); 	
			pCellAttr->SetFont(font);	
			pCellAttr->SetTextColour(m_clVolume);
			pCellAttr->SetAlignment(wxALIGN_RIGHT,-1);	
			break;
		}
	case 5://性质
		{
			wxFont font(m_nBodyfontSize-1, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_LIGHT, false); 	
			pCellAttr->SetFont(font);	
			SetVolumeDescColor(row, pCellAttr);
			pCellAttr->SetAlignment(wxALIGN_RIGHT,-1);	
			break;
		}
	}
	pCellAttr->SetReadOnly();
	return pCellAttr;
}
void BigGridTable::SetVolumeDescColor(int row, wxGridCellAttr *pCellAttr)
{
	if(m_pFData == NULL)
		return;
	if(row >=0 && row < (int)m_pFData->m_vecQIntime.size())
	{
		double dbPrice = m_dbLastPrice;
		if(row -1 >=0 && row < (int)(m_pFData->m_vecQIntime).size())
		{
			SQIntime *pitem = m_pFData->m_vecQIntime[row-1];
			dbPrice = pitem->fLastPrice;
		}
		SQIntime *pItemLast = m_pFData->m_vecQIntime[row];
		if(pItemLast->fLastPrice > dbPrice)
			pCellAttr->SetTextColour(m_Leftpanelzhang);					
		else if(pItemLast->fLastPrice < dbPrice)
			pCellAttr->SetTextColour(m_Leftpaneldie);					
		else 
		{
			switch(pItemLast->nDesc)
			{
			case 0://空开（绿色）0
				pCellAttr->SetTextColour(m_Leftpaneldie);	
				break;
			case 1://多开（红色） 1
				pCellAttr->SetTextColour(m_Leftpanelzhang);	
				break;
			case 2:// 多平（绿色）2
				pCellAttr->SetTextColour(m_Leftpaneldie);	
				break;
			case 3://空平（红色）3
				pCellAttr->SetTextColour(m_Leftpanelzhang);	
				break;
			case 4://多换（红色）4
				pCellAttr->SetTextColour(m_Leftpanelzhang);	
				break;
			case 5://空换（绿色）5
				pCellAttr->SetTextColour(m_Leftpaneldie);	
				break;
			case 6://双平（绿色）6
				pCellAttr->SetTextColour(m_Leftpaneldie);	
				break;
			case 7://双开（红色）7
				pCellAttr->SetTextColour(m_Leftpanelzhang);	
				break;
			}
		}
	}			
}
bool BigGridTable::DeleteRows( size_t pos, size_t numRows )
{
	m_rowcount = m_rowcount - numRows;		
	if ( GetView() )
	{
		wxGridTableMessage msg( this,
			wxGRIDTABLE_NOTIFY_ROWS_DELETED,
			pos,
			numRows );

		GetView()->ProcessTableMessage( msg );
	}

	return true;		
}
void BigGridTable::SetValue( int , int , const wxString&  ) { /* ignore */ }
bool BigGridTable::IsEmptyCell( int , int  ) { return false; }
void BigGridTable::SetNumberRows(int count){ m_rowcount=count; }
void BigGridTable::SetFData(CFData*		pFData)
{
	m_pFData = pFData;
}
void BigGridTable::SetColLabelValue( int col, const wxString& str)
{
	m_mapLabels[col] = str;
}


wxString BigGridTable::GetColLabelValue( int col )
{
	if(col <0 || col>= (int)m_mapLabels.size())
		return "";
	return m_mapLabels[col];
}
bool BigGridTable::AppendRows( size_t numRows /*= 1*/ )
{
	m_rowcount = m_rowcount + numRows;		
	if ( GetView() )
	{
		wxGridTableMessage msg( this,
			wxGRIDTABLE_NOTIFY_ROWS_APPENDED,
			numRows );
		GetView()->ProcessTableMessage( msg );
	}
	return true;
}
void BigGridTable::SetLastPrice(double dbPrice)
{
	m_dbLastPrice = dbPrice;
}
bool BigGridTable::InitCfg(TiXmlElement *root)
{
	TiXmlElement *tList = root->FirstChildElement("biggridtable");
	if(tList == NULL)
		return false;

	TiXmlNode *pColor = tList->FirstChild("color");
	if(pColor == NULL)
		return false;

	TiXmlNode *ListItem = pColor->FirstChild("Text");
	if(ListItem == NULL)
		return false;
	const char *strValue = ListItem->ToElement()->GetText();
	unsigned long value = ColorStr2Long(strValue);
	SetclText(value);

	ListItem = pColor->FirstChild("Leftpanelzhang");
	if(ListItem == NULL)
		return false;
	strValue = ListItem->ToElement()->GetText();	
	value = ColorStr2Long(strValue);
	SetLeftpanelzhang(value);

	ListItem = pColor->FirstChild("Leftpaneldie");
	if(ListItem == NULL)
		return false;
	strValue = ListItem->ToElement()->GetText();	
	value = ColorStr2Long(strValue);
	SetLeftpaneldie(value);

	ListItem = pColor->FirstChild("Leftpanelhengpan");
	if(ListItem == NULL)
		return false;
	strValue = ListItem->ToElement()->GetText();	
	value = ColorStr2Long(strValue);
	SetLeftpanelhengpan(value);

	ListItem = pColor->FirstChild("Volume");
	if(ListItem == NULL)
		return false;
	strValue = ListItem->ToElement()->GetText();	
	value = ColorStr2Long(strValue);
	SetclVolume(value);

	ListItem = pColor->FirstChild("bodyfontsize");
	if(ListItem == NULL)
		return false;
	strValue = ListItem->ToElement()->GetText();	
	value = ColorStr2Long(strValue);
	SetBodyfontsize(value);
	return true;
}
void BigGridTable::SetBodyfontsize(unsigned long lValue)
{
	m_nBodyfontSize = lValue;
}
void BigGridTable::SetclText(unsigned long lValue)
{
	m_clText = lValue;
}
void BigGridTable::SetLeftpanelzhang(unsigned long lValue)
{
	m_Leftpanelzhang = lValue;
}
void BigGridTable::SetLeftpaneldie(unsigned long lValue)
{
	m_Leftpaneldie = lValue;
}
void BigGridTable::SetLeftpanelhengpan(unsigned long lValue)
{
	m_Leftpanelhengpan = lValue;
}	
void BigGridTable::SetclVolume(unsigned long lValue)
{
	m_clVolume = lValue;
}
void BigGridTable::SetNumDigits(int nNumDigits)
{
	m_nNumDigits = nNumDigits;
}