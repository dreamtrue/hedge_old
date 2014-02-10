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
//ͬ���¼�
extern CEvent ReqAccount;
extern CRITICAL_SECTION REQUEST_ID_IF;//if����ID�����ٽ���
extern CRITICAL_SECTION ifDealRtn;//����ɽ���Ϣ�����ٽ���
extern CRITICAL_SECTION ODREF;//if��Լ���������ٽ���
extern int rqIfID;//��ѯIF�ʻ�ʱ��У��ID
// USER_API����
extern CThostFtdcTraderApi* pUserApi;
bool rtnTradeIf;
// ���ò���
extern char BROKER_ID[];		// ���͹�˾����
extern char INVESTOR_ID[];		// Ͷ���ߴ���
extern char PASSWORD[];			// �û�����
extern char INSTRUMENT_ID[];	// ��Լ����
extern TThostFtdcPriceType	LIMIT_PRICE;	// �۸�
extern TThostFtdcDirectionType	DIRECTION;	// ��������
// ������
extern int iRequestID;
double availIf;
int rtnAvailIfID;//��ѯ�ʻ����ص�ID�������˶�

// �Ự����
TThostFtdcFrontIDType	FRONT_ID;	//ǰ�ñ��,ֻҪ�����ߣ����ֵ�������仯 
TThostFtdcSessionIDType	SESSION_ID;	//�Ự���,ֻҪ�����ߣ����ֵ�������仯 
TThostFtdcOrderRefType	ORDER_REF;	//��������,��������ڱ�������ʵ�ʲ���,���ǰѱ������ñ����ھֲ�������,��������̵߳���Ҫ 

std::map<CString,std::vector<CThostFtdcOrderField>> ifOrderRtn;
std::map<CString,std::vector<CThostFtdcTradeField>> ifTradeRtn;

void CTraderSpi::OnFrontConnected()
{
	//cerr << "--->>> " << "OnFrontConnected" << endl;
	///�û���¼����
	ReqUserLogin();
}

void CTraderSpi::ReqUserLogin()
{
	//clientDlg->SetDlgItemTextA(IDC_EDIT18,"��½");
	CThostFtdcReqUserLoginField req;
	memset(&req, 0, sizeof(req));
	strcpy(req.BrokerID, BROKER_ID);
	strcpy(req.UserID, INVESTOR_ID);
	strcpy(req.Password, PASSWORD);
	::EnterCriticalSection(&REQUEST_ID_IF);
	int iResult = pUserApi->ReqUserLogin(&req, ++iRequestID);
	::LeaveCriticalSection(&REQUEST_ID_IF);;
	//cerr << "--->>> �����û���¼����: " << ((iResult == 0) ? "�ɹ�" : "ʧ��") << endl;
}

void CTraderSpi::OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin,
		CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	//clientDlg->SetDlgItemTextA(IDC_EDIT18,"��½��Ӧ");
	//cerr << "--->>> " << "OnRspUserLogin" << endl;
	if (bIsLast && !IsErrorRspInfo(pRspInfo))
	{
		// ����Ự����
		FRONT_ID = pRspUserLogin->FrontID;
		SESSION_ID = pRspUserLogin->SessionID;
		TRACE("ǰ��ID=%d\r\n",FRONT_ID);
		TRACE("�ỰID=%d\r\n",SESSION_ID);
		::EnterCriticalSection(&ODREF);
		iNextOrderRef = atoi(pRspUserLogin->MaxOrderRef);
		iNextOrderRef++;
		sprintf(ORDER_REF, "%d", iNextOrderRef);
		::LeaveCriticalSection(&ODREF);
		///��ȡ��ǰ������
		//cerr << "--->>> ��ȡ��ǰ������ = " << pUserApi->GetTradingDay() << endl;
		///Ͷ���߽�����ȷ��
		ReqSettlementInfoConfirm();//�����Ǳ����,�����޷�����
		//�����ѯ�ʽ��˻�
		//ReqQryTradingAccount();//�������ڿ�ʼʱ���ð�,��Ϊ���ֹ�һ����ν�ĳ�ͻ(������û����)
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
	//cerr << "--->>> Ͷ���߽�����ȷ��: " << ((iResult == 0) ? "�ɹ�" : "ʧ��") << endl;
}

void CTraderSpi::OnRspSettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField *pSettlementInfoConfirm, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	//clientDlg->SetDlgItemTextA(IDC_EDIT18,"�����Ӧ");
	//cerr << "--->>> " << "OnRspSettlementInfoConfirm" << endl;
	if (bIsLast && !IsErrorRspInfo(pRspInfo))
	{
		///�����ѯ��Լ		
		//ReqQryInstrument();
		//ReqQryInvestorPosition();//��ѯ�ֲ�,��Ϊ�����ж�
	}
}

void CTraderSpi::ReqQryInstrument()
{
	//clientDlg->SetDlgItemTextA(IDC_EDIT18,"QryInstrument����");
	CThostFtdcQryInstrumentField req;
	memset(&req, 0, sizeof(req));
	strcpy(req.InstrumentID, INSTRUMENT_ID);
	::EnterCriticalSection(&REQUEST_ID_IF);
	int iResult = pUserApi->ReqQryInstrument(&req, ++iRequestID);
	::LeaveCriticalSection(&REQUEST_ID_IF);
	//cerr << "--->>> �����ѯ��Լ: " << ((iResult == 0) ? "�ɹ�" : "ʧ��") << endl;
}

void CTraderSpi::OnRspQryInstrument(CThostFtdcInstrumentField *pInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	//clientDlg->SetDlgItemTextA(IDC_EDIT18,"QryInstrument��Ӧ");
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
	//clientDlg->SetDlgItemTextA(IDC_EDIT18,"�����ѯTradingAccount");
	CThostFtdcQryTradingAccountField req;
	memset(&req, 0, sizeof(req));
	strcpy(req.BrokerID, BROKER_ID);
	strcpy(req.InvestorID, INVESTOR_ID);
	::EnterCriticalSection(&REQUEST_ID_IF);
	int iResult = pUserApi->ReqQryTradingAccount(&req, ++iRequestID);
	::LeaveCriticalSection(&REQUEST_ID_IF);
	//cerr << "--->>> �����ѯ�ʽ��˻�: " << ((iResult == 0) ? "�ɹ�" : "ʧ��") << endl;
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
		///�����ѯͶ���ֲ߳�
		//ReqQryInvestorPosition();
		if(rqIfID == nRequestID){
			ReqAccount.SetEvent();//���¼����ڴ���״̬
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
	//cerr << "--->>> �����ѯͶ���ֲ߳�: " << ((iResult == 0) ? "�ɹ�" : "ʧ��") << endl;
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
			TRACE("������%d positionIf\r\n",positionIf);
		}
		///����¼������
		//ReqOrderInsert();
	}
}

void CTraderSpi::ReqOrderInsert()
{
	CThostFtdcInputOrderField req;

	memset(&req, 0, sizeof(req));
	///���͹�˾����
	strcpy(req.BrokerID, BROKER_ID);
	///Ͷ���ߴ���
	strcpy(req.InvestorID, INVESTOR_ID);
	///��Լ����
	strcpy(req.InstrumentID, INSTRUMENT_ID);
	///��������
	strcpy(req.OrderRef, ORDER_REF);
	///�û�����
//	TThostFtdcUserIDType	UserID;
	///�����۸�����: �޼�
	req.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
	///��������: 
	req.Direction = DIRECTION;
	///��Ͽ�ƽ��־: ����
	req.CombOffsetFlag[0] = THOST_FTDC_OF_Close;
	///���Ͷ���ױ���־
	req.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
	///�۸�
	req.LimitPrice = LIMIT_PRICE;
	///����: 1
	req.VolumeTotalOriginal = 1;
	///��Ч������: ������Ч
	req.TimeCondition = THOST_FTDC_TC_GFD;
	///GTD����
//	TThostFtdcDateType	GTDDate;
	///�ɽ�������: �κ�����
	req.VolumeCondition = THOST_FTDC_VC_AV;
	///��С�ɽ���: 1
	req.MinVolume = 1;
	///��������: ����
	req.ContingentCondition = THOST_FTDC_CC_Immediately;
	///ֹ���
//	TThostFtdcPriceType	StopPrice;
	///ǿƽԭ��: ��ǿƽ
	req.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;
	///�Զ������־: ��
	req.IsAutoSuspend = 0;
	///ҵ��Ԫ
//	TThostFtdcBusinessUnitType	BusinessUnit;
	///������
//	TThostFtdcRequestIDType	RequestID;
	///�û�ǿ����־: ��
	req.UserForceClose = 0;
	::EnterCriticalSection(&REQUEST_ID_IF);
	int iResult = pUserApi->ReqOrderInsert(&req, ++iRequestID);
	::LeaveCriticalSection(&REQUEST_ID_IF);
	//cerr << "--->>> ����¼������: " << ((iResult == 0) ? "�ɹ�" : "ʧ��") << endl;
}

void CTraderSpi::OnRspOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	//cerr << "--->>> " << "OnRspOrderInsert" << endl;
	IsErrorRspInfo(pRspInfo);
	TRACE("InsertError!\r\n");
}

void CTraderSpi::ReqOrderAction(CThostFtdcOrderField *pOrder)
{
	//static bool ORDER_ACTION_SENT = false;		//�Ƿ����˱���
	//if (ORDER_ACTION_SENT)
	//	return;

	CThostFtdcInputOrderActionField req;
	memset(&req, 0, sizeof(req));
	///���͹�˾����
	strcpy(req.BrokerID, pOrder->BrokerID);
	///Ͷ���ߴ���
	strcpy(req.InvestorID, pOrder->InvestorID);
	///������������
//	TThostFtdcOrderActionRefType	OrderActionRef;
	///��������
	strcpy(req.OrderRef, pOrder->OrderRef);
	///������
//	TThostFtdcRequestIDType	RequestID;
	///ǰ�ñ��
	req.FrontID = FRONT_ID;
	///�Ự���
	req.SessionID = SESSION_ID;
	///����������
//	TThostFtdcExchangeIDType	ExchangeID;
	///�������
//	TThostFtdcOrderSysIDType	OrderSysID;
	///������־
	req.ActionFlag = THOST_FTDC_AF_Delete;
	///�۸�
//	TThostFtdcPriceType	LimitPrice;
	///�����仯
//	TThostFtdcVolumeType	VolumeChange;
	///�û�����
//	TThostFtdcUserIDType	UserID;
	///��Լ����
	strcpy(req.InstrumentID, pOrder->InstrumentID);
	::EnterCriticalSection(&REQUEST_ID_IF);
	int iResult = pUserApi->ReqOrderAction(&req, ++iRequestID);
	::LeaveCriticalSection(&REQUEST_ID_IF);
	//cerr << "--->>> ������������: " << ((iResult == 0) ? "�ɹ�" : "ʧ��") << endl;
	//ORDER_ACTION_SENT = true;
}

void CTraderSpi::OnRspOrderAction(CThostFtdcInputOrderActionField *pInputOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	//cerr << "--->>> " << "OnRspOrderAction" << endl;
	IsErrorRspInfo(pRspInfo);
}

///����֪ͨ
void CTraderSpi::OnRtnOrder(CThostFtdcOrderField *pOrder)
{
	TRACE("OnRtnOrder���б���֪ͨ%s\r\n",pOrder->OrderRef);
	if (IsMyOrder(pOrder))
	{
		TRACE("OnRtnOrder���Ự����֪ͨ%s\r\n",pOrder->OrderRef);
		::EnterCriticalSection(&ifDealRtn);
		CString orf(pOrder->OrderRef);
		std::map<CString,std::vector<CThostFtdcOrderField>>::iterator iOrderRtn;
		iOrderRtn = ifOrderRtn.find(orf);
		if(iOrderRtn != ifOrderRtn.end()){
			TRACE("�Ѿ����ڱ�������%s,����������Ŀ\r\n",orf);
			iOrderRtn->second.push_back(*pOrder);
		}
		else{
			TRACE("�µı�������%s,��Ҫ������Ŀ\r\n",orf);
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
		//cout << "--->>> �����ɹ�" << endl;
		}
		*/
	}
}
///�ɽ�֪ͨ
void CTraderSpi::OnRtnTrade(CThostFtdcTradeField *pTrade)
{
	::EnterCriticalSection(&ifDealRtn);
	CString SysID(pTrade->OrderSysID);//����ϵͳ��Ž��б��
	TRACE("�ɽ�����,%s\r\n",pTrade->OrderSysID);
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
	TRACE("����\r\n");
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
	// ���ErrorID != 0, ˵���յ��˴������Ӧ
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
