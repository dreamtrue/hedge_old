// testTraderApi.cpp : �������̨Ӧ�ó������ڵ㡣
//
#include "ThostFtdcTraderApi.h"
#include "TraderSpi.h"
#include "stdAfx.h"
#include "afxmt.h"

// UserApi����
CThostFtdcTraderApi* pUserApi;
extern CRITICAL_SECTION REQUEST_ID_IF;//if����ID�����ٽ���
// ���ò���
#define NUM_ADD_TRADE 24
char  FRONT_ADDR[NUM_ADD_TRADE][60] = {
"tcp://ctp1-front5.citicsf.com:41205",
"tcp://ctp1-front7.citicsf.com:41205",
"tcp://ctp1-front13.citicsf.com:41205",
"tcp://115.238.108.184:41205",
"tcp://ctp1-front11.citicsf.com:41205",
"tcp://ctp1-front12.citicsf.com:41205",
"tcp://ctp1-front13.citicsf.com:41205",
"tcp://121.15.139.123:41205",
"tcp://ctp1-front1.citicsf.com:41205",
"tcp://ctp1-front3.citicsf.com:41205",
"tcp://ctp1-front13.citicsf.com:41205",
"tcp://180.169.101.177:41205",
"tcp://ctp1-front6.citicsf.com:41205",
"tcp://ctp1-front8.citicsf.com:41205",
"tcp://ctp1-front13.citicsf.com:41205",
"tcp://221.12.30.12:41205",
"tcp://ctp1-front9.citicsf.com:41205",
"tcp://ctp1-front10.citicsf.com:41205",
"tcp://ctp1-front13.citicsf.com:41205",
"tcp://123.124.247.8:41205",
"tcp://ctp1-front2.citicsf.com:41205",
"tcp://ctp1-front4.citicsf.com:41205",
"tcp://ctp1-front13.citicsf.com:41205",
"tcp://27.115.78.193:41205"
};// ǰ�õ�ַ
TThostFtdcBrokerIDType	BROKER_ID = "66666";				// ���͹�˾����
TThostFtdcInvestorIDType INVESTOR_ID = "10127111";			// Ͷ���ߴ���
TThostFtdcPasswordType  PASSWORD = "003180";			// �û�����
TThostFtdcInstrumentIDType INSTRUMENT_ID = "IF1307";	// ��Լ����
TThostFtdcDirectionType	DIRECTION = THOST_FTDC_D_Buy;	// ��������
TThostFtdcPriceType	LIMIT_PRICE = 2500;				// �۸�

// ������
int iRequestID = 100;//������ϵͳ����һ��,ʹ��ͬһ������

UINT IfTradeEntry(LPVOID pParam)//����ϵͳ������ڣ���Ϊһ���������߳�
{
	// ��ʼ��UserApi
	pUserApi = CThostFtdcTraderApi::CreateFtdcTraderApi();			// ����UserApi
	CTraderSpi *pUserSpi = new CTraderSpi();
	pUserApi->RegisterSpi((CThostFtdcTraderSpi*)pUserSpi);			// ע���¼���
	pUserApi->SubscribePublicTopic(TERT_RESTART);					// ע�ṫ����
	pUserApi->SubscribePrivateTopic(TERT_RESTART);					// ע��˽����
	for(int i = 0;i < NUM_ADD_TRADE;i++){
		pUserApi->RegisterFront(FRONT_ADDR[i]);							
	}// connect
	pUserApi->Init();
	pUserApi->Join();
	CThostFtdcQryTradingAccountField myAccount = {"66666","10127111"};
	::EnterCriticalSection(&REQUEST_ID_IF);
	pUserApi->ReqQryTradingAccount(&myAccount,++iRequestID);
	::LeaveCriticalSection(&REQUEST_ID_IF);
//	pUserApi->Release();
	return 0;
}