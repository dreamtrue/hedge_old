// testTraderApi.cpp : 定义控制台应用程序的入口点。
//
#include "ThostFtdcTraderApi.h"
#include "TraderSpi.h"
#include "stdAfx.h"
#include "afxmt.h"

// UserApi对象
CThostFtdcTraderApi* pUserApi;
extern CRITICAL_SECTION REQUEST_ID_IF;//if请求ID操作临界区
// 配置参数
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
};// 前置地址
TThostFtdcBrokerIDType	BROKER_ID = "66666";				// 经纪公司代码
TThostFtdcInvestorIDType INVESTOR_ID = "10127111";			// 投资者代码
TThostFtdcPasswordType  PASSWORD = "003180";			// 用户密码
TThostFtdcInstrumentIDType INSTRUMENT_ID = "IF1307";	// 合约代码
TThostFtdcDirectionType	DIRECTION = THOST_FTDC_D_Buy;	// 买卖方向
TThostFtdcPriceType	LIMIT_PRICE = 2500;				// 价格

// 请求编号
int iRequestID = 100;//跟行情系统保持一致,使用同一个变量

UINT IfTradeEntry(LPVOID pParam)//交易系统启动入口，作为一个独立的线程
{
	// 初始化UserApi
	pUserApi = CThostFtdcTraderApi::CreateFtdcTraderApi();			// 创建UserApi
	CTraderSpi *pUserSpi = new CTraderSpi();
	pUserApi->RegisterSpi((CThostFtdcTraderSpi*)pUserSpi);			// 注册事件类
	pUserApi->SubscribePublicTopic(TERT_RESTART);					// 注册公有流
	pUserApi->SubscribePrivateTopic(TERT_RESTART);					// 注册私有流
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