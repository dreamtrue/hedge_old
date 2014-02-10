#include "MdSpi.h"
#include "StdAfx.h"
#include <iostream>
#include "afxmt.h"
using namespace std;
#pragma warning(disable : 4996)
extern CRITICAL_SECTION g_IfPrice;//临界区
extern CRITICAL_SECTION REQUEST_ID_IF;//if请求ID操作临界区
//IF行情数据
double ifBid1,ifAsk1;//买一，卖一价
// USER_API参数
extern CThostFtdcMdApi* pUserApiMD;

// 配置参数	
extern TThostFtdcBrokerIDType	BROKER_ID;
extern TThostFtdcInvestorIDType INVESTOR_ID;
extern TThostFtdcPasswordType	PASSWORD;
extern char* ppInstrumentID[];	
extern int iInstrumentID;

// 请求编号
extern int iRequestID;
extern CEvent UpdateTrade;//交易提醒同步,初始为未触发,自动复位
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
	///用户登录请求
	ReqUserLogin();
}

void CMdSpi::ReqUserLogin()
{
	CThostFtdcReqUserLoginField req;
	memset(&req, 0, sizeof(req));
	strcpy(req.BrokerID, BROKER_ID);
	strcpy(req.UserID, INVESTOR_ID);
	strcpy(req.Password, PASSWORD);
	::EnterCriticalSection(&REQUEST_ID_IF);//if请求ID操作临界区
	int iResult = pUserApiMD->ReqUserLogin(&req, ++iRequestID);
	::LeaveCriticalSection(&REQUEST_ID_IF);
	TRACE("发送登陆请求\r\n");
	//cerr << "--->>> 发送用户登录请求: " << ((iResult == 0) ? "成功" : "失败") << endl;
}

void CMdSpi::OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin,
	CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	//cerr << "--->>> " << __FUNCTION__ << endl;
	if (bIsLast && !IsErrorRspInfo(pRspInfo))
	{
		///获取当前交易日
		//cerr << "--->>> 获取当前交易日 = " << pUserApiMD->GetTradingDay() << endl;
		// 请求订阅行情
		TRACE("行情登陆成功\r\n");
		SubscribeMarketData();	
	}
	else{
		TRACE("行情登陆失败\r\n");
	}
}

void CMdSpi::SubscribeMarketData()
{
	int iResult = pUserApiMD->SubscribeMarketData(ppInstrumentID, iInstrumentID);
	//cerr << "--->>> 发送行情订阅请求: " << ((iResult == 0) ? "成功" : "失败") << endl;
}

void CMdSpi::OnRspSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	TRACE("行情订阅成功\r\n");
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
	UpdateTrade.SetEvent();//交易提醒
}

bool CMdSpi::IsErrorRspInfo(CThostFtdcRspInfoField *pRspInfo)
{
	// 如果ErrorID != 0, 说明收到了错误的响应
	bool bResult = ((pRspInfo) && (pRspInfo->ErrorID != 0));
	//if (bResult)
	//cerr << "--->>> ErrorID=" << pRspInfo->ErrorID << ", ErrorMsg=" << pRspInfo->ErrorMsg << endl;
	return bResult;
}