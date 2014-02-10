// testTraderApi.cpp : 定义控制台应用程序的入口点。
//

#include "ThostFtdcMdApi.h"
#include "MdSpi.h"
#include "stdAfx.h"
#include "Afxsock.h"
#include "afxmt.h"
#define NUM_ADD_MD 24
// UserApi对象
CThostFtdcMdApi* pUserApiMD;
extern CRITICAL_SECTION REQUEST_ID_IF;//if请求ID操作临界区
// 配置参数
char FRONT_ADDR_MD[NUM_ADD_MD][60]= {
	"tcp://ctp1-md5.citicsf.com:41213",
	"tcp://ctp1-md7.citicsf.com:41213",
	"tcp://ctp1-md13.citicsf.com:41213",
	"tcp://115.238.108.184:41213",
	"tcp://ctp1-md11.citicsf.com:41213",
	"tcp://ctp1-md12.citicsf.com:41213",
	"tcp://ctp1-md13.citicsf.com:41213",
	"tcp://121.15.139.123:41213",
	"tcp://ctp1-md1.citicsf.com:41213",
	"tcp://ctp1-md3.citicsf.com:41213",
	"tcp://ctp1-md13.citicsf.com:41213",
	"tcp://180.169.101.177:41213",
	"tcp://ctp1-md6.citicsf.com:41213",
	"tcp://ctp1-md8.citicsf.com:41213",
	"tcp://ctp1-md13.citicsf.com:41213",
	"tcp://221.12.30.12:41213",
	"tcp://ctp1-md9.citicsf.com:41213",
	"tcp://ctp1-md10.citicsf.com:41213",
	"tcp://ctp1-md13.citicsf.com:41213",
	"tcp://123.124.247.8:41213",
	"tcp://ctp1-md2.citicsf.com:41213",
	"tcp://ctp1-md4.citicsf.com:41213",
	"tcp://ctp1-md13.citicsf.com:41213",
	"tcp://27.115.78.193:41213"
};//前置地址
extern TThostFtdcInstrumentIDType INSTRUMENT_ID;
char *ppInstrumentID[] = {INSTRUMENT_ID};     		// 行情订阅列表
int iInstrumentID = 1;									// 行情订阅数量

// 请求编号
extern int iRequestID;

UINT IfThreadEntry(LPVOID pParam)//行情获取启动入口，作为一个独立的线程
{
	// 初始化UserApi
	pUserApiMD = CThostFtdcMdApi::CreateFtdcMdApi();			// 创建UserApi
	CThostFtdcMdSpi* pUserSpi = new CMdSpi();
	pUserApiMD->RegisterSpi(pUserSpi);						// 注册事件类
	//多前置注册
	for(int i = 0;i < NUM_ADD_MD;i++){
		pUserApiMD->RegisterFront(FRONT_ADDR_MD[i]);							
	}// connect
	pUserApiMD->Init();
	pUserApiMD->Join();
	pUserApiMD->Release();
	return 0;
}