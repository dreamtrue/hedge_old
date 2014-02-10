// testTraderApi.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include "ThostFtdcMdApi.h"
#include "MdSpi.h"
#include "stdAfx.h"
#include "Afxsock.h"
#include "afxmt.h"
#define NUM_ADD_MD 24
// UserApi����
CThostFtdcMdApi* pUserApiMD;
extern CRITICAL_SECTION REQUEST_ID_IF;//if����ID�����ٽ���
// ���ò���
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
};//ǰ�õ�ַ
extern TThostFtdcInstrumentIDType INSTRUMENT_ID;
char *ppInstrumentID[] = {INSTRUMENT_ID};     		// ���鶩���б�
int iInstrumentID = 1;									// ���鶩������

// ������
extern int iRequestID;

UINT IfThreadEntry(LPVOID pParam)//�����ȡ������ڣ���Ϊһ���������߳�
{
	// ��ʼ��UserApi
	pUserApiMD = CThostFtdcMdApi::CreateFtdcMdApi();			// ����UserApi
	CThostFtdcMdSpi* pUserSpi = new CMdSpi();
	pUserApiMD->RegisterSpi(pUserSpi);						// ע���¼���
	//��ǰ��ע��
	for(int i = 0;i < NUM_ADD_MD;i++){
		pUserApiMD->RegisterFront(FRONT_ADDR_MD[i]);							
	}// connect
	pUserApiMD->Init();
	pUserApiMD->Join();
	pUserApiMD->Release();
	return 0;
}