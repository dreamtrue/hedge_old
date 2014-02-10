#include "MdSpi.h"
#include "StdAfx.h"
#include <iostream>
#include "afxmt.h"
using namespace std;
#pragma warning(disable : 4996)
extern CRITICAL_SECTION g_IfPrice;//�ٽ���
extern CRITICAL_SECTION REQUEST_ID_IF;//if����ID�����ٽ���
//IF��������
double ifBid1,ifAsk1;//��һ����һ��
// USER_API����
extern CThostFtdcMdApi* pUserApiMD;

// ���ò���	
extern TThostFtdcBrokerIDType	BROKER_ID;
extern TThostFtdcInvestorIDType INVESTOR_ID;
extern TThostFtdcPasswordType	PASSWORD;
extern char* ppInstrumentID[];	
extern int iInstrumentID;

// ������
extern int iRequestID;
extern CEvent UpdateTrade;//��������ͬ��,��ʼΪδ����,�Զ���λ
void CMdSpi::OnRspError(CThostFtdcRspInfoField *pRspInfo,
	int nRequestID, bool bIsLast)
{
	//cerr << "--->>> "<< __FUNCTION__ << endl;
	IsErrorRspInfo(pRspInfo);
}

void CMdSpi::OnFrontDisconnected(int nReason)
{
	//cerr << "--->>> " << __FUNCTION__ << endl;
	//cerr << "--->>> Reason = " << nReason << endl;
}

void CMdSpi::OnHeartBeatWarning(int nTimeLapse)
{
	//cerr << "--->>> " << __FUNCTION__ << endl;
	//cerr << "--->>> nTimerLapse = " << nTimeLapse << endl;
}

void CMdSpi::OnFrontConnected()
{
	//cerr << "--->>> " << __FUNCTION__ << endl;
	///�û���¼����
	ReqUserLogin();
}

void CMdSpi::ReqUserLogin()
{
	CThostFtdcReqUserLoginField req;
	memset(&req, 0, sizeof(req));
	strcpy(req.BrokerID, BROKER_ID);
	strcpy(req.UserID, INVESTOR_ID);
	strcpy(req.Password, PASSWORD);
	::EnterCriticalSection(&REQUEST_ID_IF);//if����ID�����ٽ���
	int iResult = pUserApiMD->ReqUserLogin(&req, ++iRequestID);
	::LeaveCriticalSection(&REQUEST_ID_IF);
	TRACE("���͵�½����\r\n");
	//cerr << "--->>> �����û���¼����: " << ((iResult == 0) ? "�ɹ�" : "ʧ��") << endl;
}

void CMdSpi::OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin,
	CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	//cerr << "--->>> " << __FUNCTION__ << endl;
	if (bIsLast && !IsErrorRspInfo(pRspInfo))
	{
		///��ȡ��ǰ������
		//cerr << "--->>> ��ȡ��ǰ������ = " << pUserApiMD->GetTradingDay() << endl;
		// ����������
		TRACE("�����½�ɹ�\r\n");
		SubscribeMarketData();	
	}
	else{
		TRACE("�����½ʧ��\r\n");
	}
}

void CMdSpi::SubscribeMarketData()
{
	int iResult = pUserApiMD->SubscribeMarketData(ppInstrumentID, iInstrumentID);
	//cerr << "--->>> �������鶩������: " << ((iResult == 0) ? "�ɹ�" : "ʧ��") << endl;
}

void CMdSpi::OnRspSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	TRACE("���鶩�ĳɹ�\r\n");
}

void CMdSpi::OnRspUnSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{

}

void CMdSpi::OnRtnDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData)
{
	::EnterCriticalSection(&g_IfPrice);
	ifBid1 = pDepthMarketData->BidPrice1;
	ifAsk1 = pDepthMarketData->AskPrice1;
	::LeaveCriticalSection(&g_IfPrice);
	UpdateTrade.SetEvent();//��������
}

bool CMdSpi::IsErrorRspInfo(CThostFtdcRspInfoField *pRspInfo)
{
	// ���ErrorID != 0, ˵���յ��˴������Ӧ
	bool bResult = ((pRspInfo) && (pRspInfo->ErrorID != 0));
	//if (bResult)
	//cerr << "--->>> ErrorID=" << pRspInfo->ErrorID << ", ErrorMsg=" << pRspInfo->ErrorMsg << endl;
	return bResult;
}