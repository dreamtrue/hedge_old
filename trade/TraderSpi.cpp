//#include <iostream>
#include "stdafx.h"
#include "afxmt.h"
#include <stdlib.h>
#include <stdio.h>
#include "resource.h"
#include "client2Dlg.h"
#include "ThostFtdcTraderApi.h"
#include "TraderSpi.h"
#include "afxmt.h"
#include <map>
#include <vector>
using namespace std;
#pragma warning(disable : 4996)
extern CClient2Dlg * clientDlg;
TThostFtdcPosiDirectionType direcIf;
TThostFtdcVolumeType positionIf;
int iNextOrderRef;
//同步事件
extern CEvent ReqAccount;
extern CRITICAL_SECTION REQUEST_ID_IF;//if请求ID操作临界区
extern CRITICAL_SECTION ifDealRtn;//处理成交信息返回临界区
extern CRITICAL_SECTION ODREF;//if合约报单引用临界区
extern int rqIfID;//查询IF帐户时的校核ID
// USER_API参数
extern CThostFtdcTraderApi* pUserApi;
bool rtnTradeIf;
// 配置参数
extern char BROKER_ID[];		// 经纪公司代码
extern char INVESTOR_ID[];		// 投资者代码
extern char PASSWORD[];			// 用户密码
extern char INSTRUMENT_ID[];	// 合约代码
extern TThostFtdcPriceType	LIMIT_PRICE;	// 价格
extern TThostFtdcDirectionType	DIRECTION;	// 买卖方向
// 请求编号
extern int iRequestID;
double availIf;
int rtnAvailIfID;//查询帐户余额返回的ID，用来核对

// 会话参数
TThostFtdcFrontIDType	FRONT_ID;	//前置编号,只要不断线，这个值不发生变化 
TThostFtdcSessionIDType	SESSION_ID;	//会话编号,只要不断线，这个值不发生变化 
TThostFtdcOrderRefType	ORDER_REF;	//报单引用,这个变量在本程序里实际不用,都是把报单引用保护在局部变量里,以满足多线程的需要 

std::map<CString,std::vector<CThostFtdcOrderField>> ifOrderRtn;
std::map<CString,std::vector<CThostFtdcTradeField>> ifTradeRtn;

void CTraderSpi::OnFrontConnected()
{
	//cerr << "--->>> " << "OnFrontConnected" << endl;
	///用户登录请求
	ReqUserLogin();
}

void CTraderSpi::ReqUserLogin()
{
	//clientDlg->SetDlgItemTextA(IDC_EDIT18,"登陆");
	CThostFtdcReqUserLoginField req;
	memset(&req, 0, sizeof(req));
	strcpy(req.BrokerID, BROKER_ID);
	strcpy(req.UserID, INVESTOR_ID);
	strcpy(req.Password, PASSWORD);
	::EnterCriticalSection(&REQUEST_ID_IF);
	int iResult = pUserApi->ReqUserLogin(&req, ++iRequestID);
	::LeaveCriticalSection(&REQUEST_ID_IF);;
	//cerr << "--->>> 发送用户登录请求: " << ((iResult == 0) ? "成功" : "失败") << endl;
}

void CTraderSpi::OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin,
		CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	//clientDlg->SetDlgItemTextA(IDC_EDIT18,"登陆回应");
	//cerr << "--->>> " << "OnRspUserLogin" << endl;
	if (bIsLast && !IsErrorRspInfo(pRspInfo))
	{
		// 保存会话参数
		FRONT_ID = pRspUserLogin->FrontID;
		SESSION_ID = pRspUserLogin->SessionID;
		TRACE("前置ID=%d\r\n",FRONT_ID);
		TRACE("会话ID=%d\r\n",SESSION_ID);
		::EnterCriticalSection(&ODREF);
		iNextOrderRef = atoi(pRspUserLogin->MaxOrderRef);
		iNextOrderRef++;
		sprintf(ORDER_REF, "%d", iNextOrderRef);
		::LeaveCriticalSection(&ODREF);
		///获取当前交易日
		//cerr << "--->>> 获取当前交易日 = " << pUserApi->GetTradingDay() << endl;
		///投资者结算结果确认
		ReqSettlementInfoConfirm();//好像是必须的,否则无法交易
		//请求查询资金账户
		//ReqQryTradingAccount();//尽量不在开始时调用吧,因为出现过一次所谓的冲突(后来再没出现)
	}
}

void CTraderSpi::ReqSettlementInfoConfirm()
{
	CThostFtdcSettlementInfoConfirmField req;
	memset(&req, 0, sizeof(req));
	strcpy(req.BrokerID, BROKER_ID);
	strcpy(req.InvestorID, INVESTOR_ID);
	::EnterCriticalSection(&REQUEST_ID_IF);
	int iResult = pUserApi->ReqSettlementInfoConfirm(&req, ++iRequestID);
	::LeaveCriticalSection(&REQUEST_ID_IF);;
	//cerr << "--->>> 投资者结算结果确认: " << ((iResult == 0) ? "成功" : "失败") << endl;
}

void CTraderSpi::OnRspSettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField *pSettlementInfoConfirm, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	//clientDlg->SetDlgItemTextA(IDC_EDIT18,"结算回应");
	//cerr << "--->>> " << "OnRspSettlementInfoConfirm" << endl;
	if (bIsLast && !IsErrorRspInfo(pRspInfo))
	{
		///请求查询合约		
		//ReqQryInstrument();
		//ReqQryInvestorPosition();//查询持仓,作为交易判断
	}
}

void CTraderSpi::ReqQryInstrument()
{
	//clientDlg->SetDlgItemTextA(IDC_EDIT18,"QryInstrument请求");
	CThostFtdcQryInstrumentField req;
	memset(&req, 0, sizeof(req));
	strcpy(req.InstrumentID, INSTRUMENT_ID);
	::EnterCriticalSection(&REQUEST_ID_IF);
	int iResult = pUserApi->ReqQryInstrument(&req, ++iRequestID);
	::LeaveCriticalSection(&REQUEST_ID_IF);
	//cerr << "--->>> 请求查询合约: " << ((iResult == 0) ? "成功" : "失败") << endl;
}

void CTraderSpi::OnRspQryInstrument(CThostFtdcInstrumentField *pInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	//clientDlg->SetDlgItemTextA(IDC_EDIT18,"QryInstrument回应");
	//cerr << "--->>> " << "OnRspQryInstrument" << endl;
	if (bIsLast && !IsErrorRspInfo(pRspInfo))
	{
		//ReqQryTradingAccount();
		TRACE("Long Margin %.4f\r\n",pInstrument->LongMarginRatio);
		TRACE("Short Margin %.4f\r\n",pInstrument->ShortMarginRatio);
	}
}

void CTraderSpi::ReqQryTradingAccount()
{
	//clientDlg->SetDlgItemTextA(IDC_EDIT18,"请求查询TradingAccount");
	CThostFtdcQryTradingAccountField req;
	memset(&req, 0, sizeof(req));
	strcpy(req.BrokerID, BROKER_ID);
	strcpy(req.InvestorID, INVESTOR_ID);
	::EnterCriticalSection(&REQUEST_ID_IF);
	int iResult = pUserApi->ReqQryTradingAccount(&req, ++iRequestID);
	::LeaveCriticalSection(&REQUEST_ID_IF);
	//cerr << "--->>> 请求查询资金账户: " << ((iResult == 0) ? "成功" : "失败") << endl;
}

void CTraderSpi::OnRspQryTradingAccount(CThostFtdcTradingAccountField *pTradingAccount, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	//cerr << "--->>> " << "OnRspQryTradingAccount = " << pTradingAccount->Available << endl;
	//CString Ava;
	//Ava.Format("%.4f",pTradingAccount->Available);
	availIf = pTradingAccount->Available;
	rtnAvailIfID = nRequestID;
	//clientDlg->SetDlgItemTextA(IDC_EDIT18,Ava);
	if (bIsLast && !IsErrorRspInfo(pRspInfo))
	{
		///请求查询投资者持仓
		//ReqQryInvestorPosition();
		if(rqIfID == nRequestID){
			ReqAccount.SetEvent();//将事件置于触发状态
		}
	}
}

void CTraderSpi::ReqQryInvestorPosition()
{
	CThostFtdcQryInvestorPositionField req;
	memset(&req, 0, sizeof(req));
	strcpy(req.BrokerID, BROKER_ID);
	strcpy(req.InvestorID, INVESTOR_ID);
	strcpy(req.InstrumentID, INSTRUMENT_ID);
	::EnterCriticalSection(&REQUEST_ID_IF);
	int iResult = pUserApi->ReqQryInvestorPosition(&req, ++iRequestID);
	::LeaveCriticalSection(&REQUEST_ID_IF);
	//cerr << "--->>> 请求查询投资者持仓: " << ((iResult == 0) ? "成功" : "失败") << endl;
}

void CTraderSpi::OnRspQryInvestorPosition(CThostFtdcInvestorPositionField *pInvestorPosition, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	//cerr << "--->>> " << "OnRspQryInvestorPosition" << endl;
	if (bIsLast && !IsErrorRspInfo(pRspInfo))
	{
		if(pInvestorPosition != NULL){
			TThostFtdcPosiDirectionType direcIf = pInvestorPosition->PosiDirection;
			TThostFtdcVolumeType positionIf = pInvestorPosition->Position;
			TRACE("%c\r\n",direcIf);
			TRACE("持数量%d positionIf\r\n",positionIf);
		}
		///报单录入请求
		//ReqOrderInsert();
	}
}

void CTraderSpi::ReqOrderInsert()
{
	CThostFtdcInputOrderField req;

	memset(&req, 0, sizeof(req));
	///经纪公司代码
	strcpy(req.BrokerID, BROKER_ID);
	///投资者代码
	strcpy(req.InvestorID, INVESTOR_ID);
	///合约代码
	strcpy(req.InstrumentID, INSTRUMENT_ID);
	///报单引用
	strcpy(req.OrderRef, ORDER_REF);
	///用户代码
//	TThostFtdcUserIDType	UserID;
	///报单价格条件: 限价
	req.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
	///买卖方向: 
	req.Direction = DIRECTION;
	///组合开平标志: 开仓
	req.CombOffsetFlag[0] = THOST_FTDC_OF_Close;
	///组合投机套保标志
	req.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
	///价格
	req.LimitPrice = LIMIT_PRICE;
	///数量: 1
	req.VolumeTotalOriginal = 1;
	///有效期类型: 当日有效
	req.TimeCondition = THOST_FTDC_TC_GFD;
	///GTD日期
//	TThostFtdcDateType	GTDDate;
	///成交量类型: 任何数量
	req.VolumeCondition = THOST_FTDC_VC_AV;
	///最小成交量: 1
	req.MinVolume = 1;
	///触发条件: 立即
	req.ContingentCondition = THOST_FTDC_CC_Immediately;
	///止损价
//	TThostFtdcPriceType	StopPrice;
	///强平原因: 非强平
	req.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;
	///自动挂起标志: 否
	req.IsAutoSuspend = 0;
	///业务单元
//	TThostFtdcBusinessUnitType	BusinessUnit;
	///请求编号
//	TThostFtdcRequestIDType	RequestID;
	///用户强评标志: 否
	req.UserForceClose = 0;
	::EnterCriticalSection(&REQUEST_ID_IF);
	int iResult = pUserApi->ReqOrderInsert(&req, ++iRequestID);
	::LeaveCriticalSection(&REQUEST_ID_IF);
	//cerr << "--->>> 报单录入请求: " << ((iResult == 0) ? "成功" : "失败") << endl;
}

void CTraderSpi::OnRspOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	//cerr << "--->>> " << "OnRspOrderInsert" << endl;
	IsErrorRspInfo(pRspInfo);
	TRACE("InsertError!\r\n");
}

void CTraderSpi::ReqOrderAction(CThostFtdcOrderField *pOrder)
{
	//static bool ORDER_ACTION_SENT = false;		//是否发送了报单
	//if (ORDER_ACTION_SENT)
	//	return;

	CThostFtdcInputOrderActionField req;
	memset(&req, 0, sizeof(req));
	///经纪公司代码
	strcpy(req.BrokerID, pOrder->BrokerID);
	///投资者代码
	strcpy(req.InvestorID, pOrder->InvestorID);
	///报单操作引用
//	TThostFtdcOrderActionRefType	OrderActionRef;
	///报单引用
	strcpy(req.OrderRef, pOrder->OrderRef);
	///请求编号
//	TThostFtdcRequestIDType	RequestID;
	///前置编号
	req.FrontID = FRONT_ID;
	///会话编号
	req.SessionID = SESSION_ID;
	///交易所代码
//	TThostFtdcExchangeIDType	ExchangeID;
	///报单编号
//	TThostFtdcOrderSysIDType	OrderSysID;
	///操作标志
	req.ActionFlag = THOST_FTDC_AF_Delete;
	///价格
//	TThostFtdcPriceType	LimitPrice;
	///数量变化
//	TThostFtdcVolumeType	VolumeChange;
	///用户代码
//	TThostFtdcUserIDType	UserID;
	///合约代码
	strcpy(req.InstrumentID, pOrder->InstrumentID);
	::EnterCriticalSection(&REQUEST_ID_IF);
	int iResult = pUserApi->ReqOrderAction(&req, ++iRequestID);
	::LeaveCriticalSection(&REQUEST_ID_IF);
	//cerr << "--->>> 报单操作请求: " << ((iResult == 0) ? "成功" : "失败") << endl;
	//ORDER_ACTION_SENT = true;
}

void CTraderSpi::OnRspOrderAction(CThostFtdcInputOrderActionField *pInputOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	//cerr << "--->>> " << "OnRspOrderAction" << endl;
	IsErrorRspInfo(pRspInfo);
}

///报单通知
void CTraderSpi::OnRtnOrder(CThostFtdcOrderField *pOrder)
{
	TRACE("OnRtnOrder所有报单通知%s\r\n",pOrder->OrderRef);
	if (IsMyOrder(pOrder))
	{
		TRACE("OnRtnOrder本会话报单通知%s\r\n",pOrder->OrderRef);
		::EnterCriticalSection(&ifDealRtn);
		CString orf(pOrder->OrderRef);
		std::map<CString,std::vector<CThostFtdcOrderField>>::iterator iOrderRtn;
		iOrderRtn = ifOrderRtn.find(orf);
		if(iOrderRtn != ifOrderRtn.end()){
			TRACE("已经存在报单引用%s,无需增加条目\r\n",orf);
			iOrderRtn->second.push_back(*pOrder);
		}
		else{
			TRACE("新的报单引用%s,需要增加条目\r\n",orf);
			std::vector<CThostFtdcOrderField> orderRes;
			orderRes.push_back(*pOrder);
			ifOrderRtn.insert(std::pair<CString,std::vector<CThostFtdcOrderField>>(orf,orderRes));
		}
		::LeaveCriticalSection(&ifDealRtn);
		/*
		if (IsTradingOrder(pOrder))
		ReqOrderAction(pOrder);
		else if (pOrder->OrderStatus == THOST_FTDC_OST_Canceled)
		{
		//cout << "--->>> 撤单成功" << endl;
		}
		*/
	}
}
///成交通知
void CTraderSpi::OnRtnTrade(CThostFtdcTradeField *pTrade)
{
	::EnterCriticalSection(&ifDealRtn);
	CString SysID(pTrade->OrderSysID);//采用系统编号进行标记
	TRACE("成交返回,%s\r\n",pTrade->OrderSysID);
	std::map<CString,std::vector<CThostFtdcTradeField>>::iterator iTradeRtn;
	iTradeRtn = ifTradeRtn.find(SysID);
	if(iTradeRtn != ifTradeRtn.end()){
		iTradeRtn->second.push_back(*pTrade);
	}
	else{
		std::vector<CThostFtdcTradeField> tradeRes;
		tradeRes.push_back(*pTrade);
		ifTradeRtn.insert(std::pair<CString,std::vector<CThostFtdcTradeField>>(SysID,tradeRes));
	}
	::LeaveCriticalSection(&ifDealRtn);
}

void CTraderSpi:: OnFrontDisconnected(int nReason)
{
	TRACE("断线\r\n");
	//cerr << "--->>> " << "OnFrontDisconnected" << endl;
	//cerr << "--->>> Reason = " << nReason << endl;
}
		
void CTraderSpi::OnHeartBeatWarning(int nTimeLapse)
{
	//cerr << "--->>> " << "OnHeartBeatWarning" << endl;
	//cerr << "--->>> nTimerLapse = " << nTimeLapse << endl;
}

void CTraderSpi::OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	//cerr << "--->>> " << "OnRspError" << endl;
	IsErrorRspInfo(pRspInfo);
}

bool CTraderSpi::IsErrorRspInfo(CThostFtdcRspInfoField *pRspInfo)
{
	// 如果ErrorID != 0, 说明收到了错误的响应
	bool bResult = ((pRspInfo) && (pRspInfo->ErrorID != 0));
	//if (bResult)
		//cerr << "--->>> ErrorID=" << pRspInfo->ErrorID << ", ErrorMsg=" << pRspInfo->ErrorMsg << endl;
	return bResult;
}

bool CTraderSpi::IsMyOrder(CThostFtdcOrderField *pOrder)
{
	return ((pOrder->FrontID == FRONT_ID) &&
			(pOrder->SessionID == SESSION_ID) 
			/*&&(strcmp(pOrder->OrderRef, ORDER_REF) == 0)*/);
}

bool CTraderSpi::IsTradingOrder(CThostFtdcOrderField *pOrder)
{
	return ((pOrder->OrderStatus != THOST_FTDC_OST_PartTradedNotQueueing) &&
			(pOrder->OrderStatus != THOST_FTDC_OST_Canceled) &&
			(pOrder->OrderStatus != THOST_FTDC_OST_AllTraded));
}
