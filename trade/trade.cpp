#include "stdAfx.h"
#include "client2.h"
#include "client2Dlg.h"
#include "ThostFtdcTraderApi.h"
#include "TraderSpi.h"
#include <vector>
#include <map>
#include "Order.h"
#include "afxmt.h"
#include "calendar.h"
//using namespace std;//��Ҫ��std,�����³�ͻ,��ʽ��std::���ð�
int lifeOfA50;
extern int iNextOrderRef;
extern std::map<CString,std::vector<CThostFtdcOrderField>> ifOrderRtn;
extern std::map<CString,std::vector<CThostFtdcTradeField>> ifTradeRtn;
double datumDiff;//���׻���
extern TThostFtdcFrontIDType	FRONT_ID;	//ǰ�ñ��
extern TThostFtdcSessionIDType	SESSION_ID;	//�Ự���
extern TThostFtdcOrderRefType	ORDER_REF;	//��������
void ReqOrderAction(CThostFtdcOrderField *pOrder,CThostFtdcInputOrderActionField t_req);
class IfInData{//����if���������
public:
	int numif;
	bool isbuy;
	bool isflat;
	int orf;
	IfInData(int num,bool ib,bool iflt,int orderref);
	IfInData();
};
UINT IfEntry(LPVOID pParam);//����if����ں��� 
//IF�۸��ٽ���
extern CRITICAL_SECTION g_IfPrice;//�ٽ���
extern CRITICAL_SECTION g_A50Price;
extern CRITICAL_SECTION g_index;
extern CRITICAL_SECTION tdi_a50;
extern CRITICAL_SECTION REQUEST_ID_IF;//if����ID�����ٽ���
extern CRITICAL_SECTION avgPriceOfIf;
extern CRITICAL_SECTION ifDealRtn;//����ɽ���Ϣ�����ٽ���
extern CRITICAL_SECTION ODREF;//if��Լ���������ٽ���
//ͬ���¼�����
CEvent ReqAccount(true,true,NULL,NULL);//IF�ʻ�����ѯͬ��,��ʼΪ����,�ֶ���λ
CEvent UpdateTrade(false,false,NULL,NULL);//��������ͬ��,��ʼΪδ����,�Զ���λ
int rqIfID = -1;//��ѯIF�ʻ����ʱ��ID��¼,���ں˶�,��ʼ��Ϊ-1,��֤�ѿ�ʼ������������ID
double deviation;
double deviationHigh,deviationLow;
extern double a50Bid1,a50Ask1;
extern double a50Bid1Size,a50Ask1Size;
extern double ifAsk1,ifBid1,A50Index,HS300Index;
extern CClient2Dlg * clientDlg;
extern CThostFtdcTraderApi* pUserApi;
struct HoldAndDirec{
	double price;
	int numDirec;//���з����Եĳֲ�����
};
std::vector<HoldAndDirec> holdHedge;//��ֻ����һ��A50�����̵߳������,�ݲ�����ͬ��
std::vector<HoldAndDirec> waiting_hold;//�����׵ĳֲ�,ͬ��,Ҳ������ͬ��
std::vector<HoldAndDirec>::iterator itHedge;
int multiply = 15;//A50����
int aimOfLadder = 0;
double step = 20.0;
double premium = 0;//���
double premiumHigh = 0;//���õ��߽мۻ�õ�������
double premiumLow = 0;//���õ��߽мۻ�õ�������
double deviationHigh_save,deviationLow_save;
int netHold = 0;//������
//���̿����ʽ�
extern double availA50;
//���̿����ʽ����
extern int iRequestID;
extern double availIf;
extern int rtnAvailIfID;//��ѯ�ʻ����ص�ID�������˶�
void ReqQryIFTradingAccount();//��ѯif�ʻ����ĺ���
//A50��Լ����
double marginA50 = 625.0;
//300��Լ����
double marginIf = 300 * 2553 * 0.12;
extern int iRequestID;
int ladder = 0;
int needHold = 0;
double dltInterest = 0;
double profitTarget = 0;//ӯ��Ŀ��
int BS(int needHold_temp,int netHold_temp);
int flatOrOpen(int num,bool isbuy,bool isflat);
//�ɽ�������Ϣ
extern bool rtnTradeIf;//if��Լ�ɽ�����
//if��Լ��Ϣ
extern TThostFtdcInstrumentIDType INSTRUMENT_ID;	// ��Լ����
extern TThostFtdcBrokerIDType	BROKER_ID;				// ���͹�˾����
extern TThostFtdcInvestorIDType INVESTOR_ID;			// Ͷ���ߴ���
struct avgpIf{
public:
	int num;
	double avgprice;
	avgpIf::avgpIf(int num,double price);
};
void calDeviation();
bool IsTradingOrder(CThostFtdcOrderField *pOrder);
std::vector<avgpIf> avgPriceIf;
int buyOrSell_01(int holdA50_temp,int holdIf_temp,int objA50_temp,bool isbuy);
//#define _debug
#define _case1
#define _weixin //΢�Ŵ�����Ϣ
//ֻ�ܿ���һ�������߳�,����ͬ�����ܳ�������
UINT TradeEntry_00(LPVOID pParam)//������ڣ���Ϊһ���������߳�
{
	CThostFtdcQryInstrumentField req;
	memset(&req, 0, sizeof(req));
	strcpy(req.InstrumentID, INSTRUMENT_ID);
	::EnterCriticalSection(&REQUEST_ID_IF);
	int iResult = pUserApi->ReqQryInstrument(&req, ++iRequestID);
	::LeaveCriticalSection(&REQUEST_ID_IF);

	CThostFtdcQryInvestorPositionField req00;
	memset(&req00, 0, sizeof(req00));
	strcpy(req00.BrokerID, BROKER_ID);
	strcpy(req00.InvestorID, INVESTOR_ID);
	strcpy(req00.InstrumentID, INSTRUMENT_ID);
	::EnterCriticalSection(&REQUEST_ID_IF);
	iResult = pUserApi->ReqQryInvestorPosition(&req00, ++iRequestID);
	::LeaveCriticalSection(&REQUEST_ID_IF);

#ifdef _debug
	iNextOrderRef++;//����
	ifAsk1 = 2180;//
	IfInData *data;//����
	data = new IfInData(1,false,true,iNextOrderRef); //����
	AfxBeginThread(IfEntry,data,THREAD_PRIORITY_NORMAL,0,0,NULL);//����
	return 0;//����
#endif
	if(clientDlg == NULL){
		return -1;
	}
	if(pUserApi == NULL){
		return -1;
	}	
	//�ӶԻ����ʼ��
	step = clientDlg->m_step;
	multiply = clientDlg->m_multiply;
	aimOfLadder = clientDlg->m_aimOfLadder;
	datumDiff = clientDlg->m_DeltaDatumDiff + clientDlg->m_datumDiff;
	HoldAndDirec tempIni;
	tempIni.price = clientDlg->m_costINI - datumDiff;//�ֲ���Լ۸�
	tempIni.numDirec = clientDlg->m_numINI;//���з���ĳֲ�
	holdHedge.push_back(tempIni);
	netHold = tempIni.numDirec;
	//��ۺͻ������
	while(true){
		UpdateTrade.Lock(INFINITE);//�¼�����
		//��ȡϵͳʱ��
		//SYSTEMTIME sys;
		//GetLocalTime(&sys);
		CString PREMIUM;	
		calDeviation();
		deviationHigh_save = deviationHigh;
		deviationLow_save = deviationLow;
		PREMIUM.Format(_T("%.4f"),premiumLow);	
		clientDlg->SetDlgItemTextA(IDC_EDIT18,PREMIUM);
		PREMIUM.Format(_T("%.4f"),premiumHigh);	
		clientDlg->SetDlgItemTextA(IDC_EDIT16,PREMIUM);
		/*
		//�ǽ����շ���
		if(!isTradeDay(sys.wYear,sys.wMonth,sys.wDay))
		{
		continue;
		}
		//�ǳ��潻��ʱ��
		if((sys.wHour == 9 && sys.wMinute < 10) || 
		(sys.wHour == 15 && sys.wMinute > 15) ||
		(sys.wHour == 11 && sys.wMinute > 30) ||
		(sys.wHour == 12)||
		(sys.wHour < 9) || (sys.wHour > 15)){
		continue;
		}
		*/
		if(clientDlg->tradeEnd){//��ֹ
			//����մ���,��������ʱ���ʼ��
			holdHedge.clear();
			netHold = 0;
			return -1;
		}
		if(clientDlg->stop){//��ͣ
			continue;
		}
		if(_isnan(datumDiff) != 0 || _isnan(premium)!=0 ||_isnan(deviation)!=0){
			continue;//�жϷ���ֵ����
		}
		::EnterCriticalSection(&g_A50Price);
		::EnterCriticalSection(&g_IfPrice);
		::EnterCriticalSection(&g_index);
		if(a50Bid1 < 1 || a50Ask1 < 1 || ifAsk1 < 1 || ifBid1 < 1 || A50Index < 1 || HS300Index < 1){
			::LeaveCriticalSection(&g_IfPrice);
			::LeaveCriticalSection(&g_A50Price);
			::LeaveCriticalSection(&g_index);;
			continue;
		}
		else{
			::LeaveCriticalSection(&g_IfPrice);
			::LeaveCriticalSection(&g_A50Price);
			::LeaveCriticalSection(&g_index);
		}
		if(fabs(premium) > 300 || fabs(premium) < 0.01){
			continue;//�ų�����ʱ�п��ܱ��۲�ȫ���µĴ�����ۼ���
		}
		if(deviation >= 0){
#ifdef _case1
			//��ʱ��multiply������Ҫ�ĺ�Լ��Ŀ����Ϊ����ֻ��һ�֣���֤�ɽ�
			if(a50Bid1Size >= multiply * 1){
				ladder = (int)(deviationLow / step);
			}
			else{
				continue;
			}
#else
			ladder = (int)((deviationLow + 2.5) / step);
#endif
		}
		else{
#ifdef _case1
			//��ʱ��multiply������Ҫ�ĺ�Լ��Ŀ����Ϊ����ֻ��һ�֣���֤�ɽ�
			if(a50Ask1Size >= multiply * 1){
				ladder = -(int)(-deviationHigh / step);
			}
			else{
				continue;
			}
#else
			ladder = -(int)((-deviationHigh + 2.5) / step);
#endif
		}
		if(ladder > 1){
			ladder = 1;
		}
		else if(ladder < -1){
			ladder = -1;
		}
		needHold = aimOfLadder * ladder;//��Ҫ�ĺ�Լ��ƫ��Ĺ�ϵ,���Ը����ʽ��������޸�
#ifdef _case1
		profitTarget = step;
#else
		profitTarget = step - 2.5;
#endif
		double profitUnit = 0;
		int needBuyFlat = 0,needSellFlat = 0;//ʼ�մ���0�ģ�������
		double totalTradingValue = 0;//׼�����׵��ܼ�ֵ
		for(unsigned int j = 0;j < holdHedge.size();j++){
			itHedge = holdHedge.begin() + j;
			//if(itHedge->numDirec < 0){//��ʵֻҪnetholdΪ�����ͱ�֤�����е�numDirecΪ���������ڼ���numDirecΪ��������Ϊ�������
			if(netHold < 0){
				profitUnit = itHedge->price - deviationHigh;
				if(profitUnit >= profitTarget){
					needBuyFlat = needBuyFlat - itHedge->numDirec;
					totalTradingValue = totalTradingValue + itHedge->price * (-itHedge->numDirec);
					//���¼�¼
					netHold = netHold - itHedge->numDirec;
					holdHedge.erase(itHedge);
					j--;//����ɾ������ʱ,��Ҫ��ָ����ǰ����
				}
			}
			//else if(itHedge->numDirec > 0)
			else if(netHold > 0){
				profitUnit = -(itHedge->price - deviationLow);
				if(profitUnit >= profitTarget){
					needSellFlat = needSellFlat + itHedge->numDirec;
					totalTradingValue = totalTradingValue + itHedge->price * itHedge->numDirec;
					//���¼�¼
					netHold = netHold - itHedge->numDirec;
					holdHedge.erase(itHedge);
					j--;
				}
			}
			else{
				//����0ʱʲôҲ����
			}
		}
		//ƽ�ֲ���,,��ʵ��ƽ����ƽһ��ֻ��һ���õ�ִ�еģ���������������
		flatOrOpen(needBuyFlat,true,true);
		flatOrOpen(needSellFlat,false,true);
		//���ֲ���
		BS(needHold,netHold);
	}
	return 0;//�����߲������
}
int BS(int needHold_temp,int netHold_temp){
	if(needHold_temp > 0){
		if(needHold_temp > netHold_temp){
			if(netHold_temp < 0){//����ʵ��ֹ�����
				//��ƽ����Ҫ����hold����
				flatOrOpen(-netHold_temp,true,true);
				//���¼�¼
				netHold = 0;
				holdHedge.clear();
				//��
				flatOrOpen(needHold_temp,true,false);
			}
			else{
				//��
				flatOrOpen(needHold_temp - netHold_temp,true,false);
			}
		}
	}
	if(needHold_temp < 0){
		if(needHold_temp < netHold_temp){
			if(netHold_temp > 0){//ֹ�����
				//��ƽ
				flatOrOpen(netHold_temp,false,true);
				//���¼�¼
				netHold = 0;
				holdHedge.clear();
				//����
				flatOrOpen(-needHold_temp,false,false);
			}
			else{
				//����
				flatOrOpen(netHold_temp - needHold_temp,false,false);
			}
		}
	}
	if(needHold_temp == 0){
		/*
		if(deviation >= 1.0){//��ʾ�����ж൥����Ҫ��ƽ
		if(netHold_temp > 0){
		flatOrOpen(netHold_temp,false,true);
		//���¼�¼
		netHold = 0;
		holdHedge.clear();
		}
		}
		else if(deviation < -1.0)
		{//��ʾ�����пյ�����Ҫ��ƽ
		if(netHold_temp < 0){
		flatOrOpen(-netHold_temp,true,true);
		//���¼�¼
		netHold = 0;
		holdHedge.clear();
		}
		}
		else{
		//������,ʲôҲ����
		}
		*/
	}
	return 0;
}
int flatOrOpen(int num,bool isbuy,bool isflat){
	if(num <= 0){
		return -1;
	}
	::EnterCriticalSection(&avgPriceOfIf);
	avgPriceIf.clear();//���ͳ��ƽ���۸������
	::LeaveCriticalSection(&avgPriceOfIf);
	int numAvail = 0;//����������
	if(!isflat){//��ʾ����
		/*
		ReqAccount.ResetEvent();//�¼�ͬ��,����Ϊδ����״̬
		ReqQryIFTradingAccount();//��ѯif�ʻ������ʽ�
		ReqAccount.Lock(INFINITE);//�ȴ�
		if(rtnAvailIfID == rqIfID){
		//����10��100����ҵ����
		marginIf = ifAsk1 * 300.0 * 0.12;//12%�ı�֤����
		if(multiply != 0){
		numAvail = min((int)((availA50  - 10.0) / (marginA50 * multiply)),(int)((availIf - 100.0) / marginIf));
		}
		else{
		numAvail = num;
		}
		}
		if(numAvail <= 0){
		return -1;
		}
		num = min(num,numAvail);
		*/
	}
	if(isflat){
		if(isbuy){
			TRACE("��ƽ%d��\r\n",num);
		}
		else{
			TRACE("��ƽ%d��\r\n",num);
		}
	}
	else{
		if(isbuy){
			TRACE("��%d��\r\n",num);
		}
		else{
			TRACE("����%d��\r\n",num);
		}
	}
	int numofA50 = num * multiply;
	int numofIf = num;
	std::vector<HANDLE> handleThread;//����߳̾��������
	IfInData *data;
	if(isbuy && isflat){
		data = new IfInData(1,false,true,0); 
	}
	else if(isbuy && !isflat)
	{
		data = new IfInData(1,false,false,0);
	}
	else if(!isbuy && isflat){
		data = new IfInData(1,true,true,0);
	}
	else{
		data = new IfInData(1,true,false,0);
	}
	if(numofA50 != 0){
#define PUT(prop,value) orderA50.get()->##prop## = (##value##);
#define GET(prop) orderA50.get()->##prop
		std::auto_ptr<Order> orderA50(new Order());
		double valueTradeA50 = 0;
		::EnterCriticalSection(&g_A50Price);
		if(isbuy){
			PUT(action,"BUY");
			PUT(totalQuantity,numofA50);
#ifdef _case1
			PUT(lmtPrice,a50Ask1 + 100.0);
#else
			PUT(lmtPrice,a50Bid1);
#endif
			PUT(orderType,"LMT");
		}
		else{
			PUT(action,"SELL");
			PUT(totalQuantity,numofA50);
#ifdef _case1
			PUT(lmtPrice,a50Bid1 - 100.0);
#else
			PUT(lmtPrice,a50Ask1);
#endif
			PUT(orderType,"LMT");
		}
		::LeaveCriticalSection(&g_A50Price);
		bool isA50Validate = false;
		int iftraded = 0;//�Ѿ�������ifָ��,A50ÿ�ν�����һ�ֶԳ�,˲�䷢��ifָ��
		int filledLast = 0;//���һ�εĳɽ�����
		long idThisTrade;//����id
		std::vector<rtn> rtnres;//���׽������
		int lastpos = -1;//�ϴμ�鷵���µ�vector������λ��,��-1��ʼ,���ǵ�0λ��ǰ��-1��ʾ
		::EnterCriticalSection(&tdi_a50);
		idThisTrade = clientDlg->PlaceOrder_hedge(false,orderA50);
		clientDlg->m_idrtn.insert(std::pair<long,std::vector<rtn>>(idThisTrade,rtnres));
		::LeaveCriticalSection(&tdi_a50);
		while(true){	
			std::map<long,std::vector<rtn>>::iterator iter;
			std::vector<rtn>::iterator ivec;
			int seconds = 0;//��ʱ
			while(!isA50Validate){
				//�����ٽ���,��ֹ�����ڼ��m_idrtn�䶯
				//ÿ�ζ�Ҫ���»�ȡ��ʼλ��,��Ϊmap��Ԫ�ش�����
				::EnterCriticalSection(&tdi_a50);
				iter = clientDlg->m_idrtn.find(idThisTrade);
				if(iter != clientDlg->m_idrtn.end()){
					ivec = iter->second.begin();
					int size = iter->second.size();
					if(size > 0){
						isA50Validate = true;//�з�����Ϣ��
						TRACE("A50 HAS VALIDATED\r\n");
						::LeaveCriticalSection(&tdi_a50);
						break;
					}
				}
				TRACE("A50 NOT VALIDATED\r\n");
				::LeaveCriticalSection(&tdi_a50);
				::Sleep(50);//50������һ��
				seconds = seconds + 50;
				if(seconds > 100000){//100��û��Ӧ,��Ϊָ���ʹ�ʧ��
					//���·���ָ��
					::EnterCriticalSection(&tdi_a50);
					idThisTrade = clientDlg->PlaceOrder_hedge(false,orderA50);
					clientDlg->m_idrtn.insert(std::pair<long,std::vector<rtn>>(idThisTrade,rtnres));
					::LeaveCriticalSection(&tdi_a50);
					seconds = 0;//���¼�ʱ
				}
			}
			::EnterCriticalSection(&tdi_a50);
			iter = clientDlg->m_idrtn.find(idThisTrade);
			if(iter != clientDlg->m_idrtn.end()){
				ivec = iter->second.begin();
				int size = iter->second.size();
				if(size - 1 > lastpos){
					for(int k = lastpos + 1;k < size;k++){
						if((ivec + k)->status == "Submitted" || 
							(ivec + k)->status == "Filled" || 
							(ivec + k)->status == "ApiCancelled" ||
							(ivec + k)->status == "Cancelled"){
								if((ivec + k)->filled > filledLast){
									int needTradeIf = ((ivec + k)->filled - iftraded * multiply) / multiply;//������Ҫ�½��׵�if��Լ��
									if(needTradeIf >= 1){
										//����if,�����µ��߳�
										TRACE("begin%d\r\n",needTradeIf);
										data->numif = needTradeIf;
										::EnterCriticalSection(&ODREF);
										iNextOrderRef++;
										data->orf = iNextOrderRef;
										::LeaveCriticalSection(&ODREF);
										IfInData *indata = new IfInData;
										*indata = *data;
										CWinThread *tradeThread = AfxBeginThread(IfEntry,indata,THREAD_PRIORITY_NORMAL,0,0,NULL);
										handleThread.push_back(tradeThread->m_hThread);
										//��������
										iftraded = iftraded + needTradeIf;
									}
									//������һ���Ѿ��ɽ�����Ŀ
									filledLast = (ivec + k)->filled;
								}
								if((ivec + k)->status == "Filled" ||
									(ivec + k)->status == "ApiCancelled" ||
									(ivec + k)->status == "Cancelled"){
										//�ȴ�if�����߳̽���,���н���ƽ���ɱ�ͳ��
										//�ȴ����е�IF�߳̽������,����
										int nCount = handleThread.size();
										if(nCount >= 1){
											HANDLE * handle = new HANDLE[nCount];
											for(int i = 0 ; i < nCount;i++){
												handle[i] = handleThread[i];
											}
											int nIndex = 0;
											DWORD dwRet = 0;
											dwRet = WaitForMultipleObjects(nCount,handle,true,INFINITE);
											switch(dwRet){
											case WAIT_TIMEOUT:
												TRACE("�ȴ���ʱ,�˳����,��Ϊû�б������Ķ�����\r\n");
												break;
											case WAIT_FAILED:
												TRACE("�ȴ�ʧ��\r\n");
												break;
											default:
												TRACE("�Ѿ�ȫ������\r\n");
												break;
											}
											delete []handle;
										}
										//ͳ�ƽ��׳ɱ�,���������ӽ��׳ɱ���¼,������
										::EnterCriticalSection(&avgPriceOfIf);
										double avgIf = 0;
										std::vector<avgpIf>::iterator iavg;
										double totalvalue = 0;
										int totalnum = 0;
										for(iavg = avgPriceIf.begin();iavg != avgPriceIf.end();iavg++){
											totalvalue = totalvalue + iavg->num * iavg->avgprice;
											totalnum = totalnum + iavg->num;
										}
										::LeaveCriticalSection(&avgPriceOfIf);
										if(totalnum>=1){
											avgIf = totalvalue / totalnum;
											::EnterCriticalSection(&g_index);
											double premium_temp = (ivec + k)->avgFillPrice - avgIf * A50Index / HS300Index
												- A50Index * (lifeOfA50 - clientDlg->lifeOfIf) / 365.0 * 0.0631;//6.31%���޷�������; 
											::LeaveCriticalSection(&g_index);
											double deviation_temp = premium_temp - datumDiff;
#ifdef _weixin
											CString Price_temp("");
											if(!isflat && isbuy){
												Price_temp.Format("BK:%d,CB:%f,HS300:%f,IF:%f,A50:%f,XINA50:%f\r\n",totalnum,deviation_temp,HS300Index,avgIf,A50Index,(ivec + k)->avgFillPrice);
											}
											else if(!isflat && !isbuy){
												Price_temp.Format("SK:%d,CB:%f,HS300:%f,IF:%f,A50:%f,XINA50:%f\r\n",totalnum,deviation_temp,HS300Index,avgIf,A50Index,(ivec + k)->avgFillPrice);
											}
											else if(isflat && isbuy){
												Price_temp.Format("BP:%d,CB:%f,HS300:%f,IF:%f,A50:%f,XINA50:%f\r\n",totalnum,deviation_temp,HS300Index,avgIf,A50Index,(ivec + k)->avgFillPrice);
											}
											else{
												Price_temp.Format("SP:%d,CB:%f,HS300:%f,IF:%f,A50:%f,XINA50:%f\r\n",totalnum,deviation_temp,HS300Index,avgIf,A50Index,(ivec + k)->avgFillPrice);
											}
											clientDlg->SendMsg(Price_temp);
#endif
											if(!isflat){
												HoldAndDirec hd;
												hd.price = deviation_temp;
												if(!isbuy){
													netHold = netHold - totalnum;
													hd.numDirec = -totalnum;
												}
												else{
													netHold = netHold + totalnum;
													hd.numDirec = totalnum;
												}	
												holdHedge.push_back(hd);
											}
										}
										//ɾ����ν���ID��Ӧ�ļ�ֵ
										clientDlg->m_idrtn.erase(iter);
										TRACE("�Գ����,׼�������ٽ���\r\n");
										::LeaveCriticalSection(&tdi_a50);
										delete data;
										TRACE("�Գ����,����0ֵ\r\n");
										return 0;
								}
						}			
						else{
							//NOTHING TO DO
						}
					}
					lastpos = size -1;
				}
			}
			::LeaveCriticalSection(&tdi_a50);
			UpdateTrade.Lock(INFINITE);//�ȴ�ͬ���¼�����
			//�жϼ۸��Ƿ����,�����޸�A50��Լ
			calDeviation();//���¼���ƫ�� 
			::EnterCriticalSection(&g_A50Price);
			if(isbuy){
				if(deviationHigh <= deviationHigh_save - 5.0){
					PUT(lmtPrice,a50Ask1);
					clientDlg->ModifyOrder_hedge(idThisTrade,orderA50);
				}
				else if(GET(lmtPrice) < a50Bid1){
					PUT(lmtPrice,a50Bid1);
					clientDlg->ModifyOrder_hedge(idThisTrade,orderA50);
				}
			}
			else{
				if(deviationLow >= deviationLow_save + 5.0){
					PUT(lmtPrice,a50Bid1);
					clientDlg->ModifyOrder_hedge(idThisTrade,orderA50);
				}
				else if(GET(lmtPrice) > a50Ask1){
					PUT(lmtPrice,a50Ask1);	
					clientDlg->ModifyOrder_hedge(idThisTrade,orderA50);
				}
			}
			::LeaveCriticalSection(&g_A50Price);
		}
		delete data;
		return 0;
#undef PUT
#undef GET
	}
	else{
		int needTradeIf = numofIf;
		if(needTradeIf >= 1){
			//����if,�����µ��߳�
			TRACE("begin%d\r\n",needTradeIf);
			data->numif = needTradeIf;
			::EnterCriticalSection(&ODREF);
			iNextOrderRef++;
			data->orf = iNextOrderRef;
			::LeaveCriticalSection(&ODREF);
			IfInData *indata = new IfInData;
			*indata = *data;
			CWinThread *tradeThread = AfxBeginThread(IfEntry,indata,THREAD_PRIORITY_NORMAL,0,0,NULL);
			handleThread.push_back(tradeThread->m_hThread);
		}
		int nCount = handleThread.size();
		if(nCount >= 1){
			HANDLE * handle = new HANDLE[nCount];
			for(int i = 0 ; i < nCount;i++){
				handle[i] = handleThread[i];
			}
			int nIndex = 0;
			DWORD dwRet = 0;
			dwRet = WaitForMultipleObjects(nCount,handle,true,INFINITE);
			switch(dwRet){
			case WAIT_TIMEOUT:
				TRACE("�ȴ���ʱ,�˳����,��Ϊû�б������Ķ�����\r\n");
				break;
			case WAIT_FAILED:
				TRACE("�ȴ�ʧ��\r\n");
				break;
			default:
				TRACE("�Ѿ�ȫ������\r\n");
				break;
			}
			delete []handle;
		}
		//ͳ�ƽ��׳ɱ�,���������ӽ��׳ɱ���¼,������
		::EnterCriticalSection(&avgPriceOfIf);
		double avgIf = 0;
		std::vector<avgpIf>::iterator iavg;
		double totalvalue = 0;
		int totalnum = 0;
		double avgFillPriceA50 = 0;
		if(isbuy){
			avgFillPriceA50 = a50Ask1;
		}
		else{
			avgFillPriceA50 = a50Bid1;
		}
		for(iavg = avgPriceIf.begin();iavg != avgPriceIf.end();iavg++){
			totalvalue = totalvalue + iavg->num * iavg->avgprice;
			totalnum = totalnum + iavg->num;
		}
		::LeaveCriticalSection(&avgPriceOfIf);
		if(totalnum>=1){
			avgIf = totalvalue / totalnum;
			::EnterCriticalSection(&g_index);
			double premium_temp = avgFillPriceA50 - avgIf * A50Index / HS300Index
				- A50Index * (lifeOfA50 - clientDlg->lifeOfIf) / 365.0 * 0.0631;//6.31%���޷�������; 
			::LeaveCriticalSection(&g_index);
			double deviation_temp = premium_temp - datumDiff;
#ifdef _weixin
			CString Price_temp("");
			if(!isflat && isbuy){
				Price_temp.Format("BK:%d,CB:%f,HS300:%f,IF:%f,A50:%f,XINA50:%f\r\n",totalnum,deviation_temp,HS300Index,avgIf,A50Index,avgFillPriceA50);
			}
			else if(!isflat && !isbuy){
				Price_temp.Format("SK:%d,CB:%f,HS300:%f,IF:%f,A50:%f,XINA50:%f\r\n",totalnum,deviation_temp,HS300Index,avgIf,A50Index,avgFillPriceA50);
			}
			else if(isflat && isbuy){
				Price_temp.Format("BP:%d,CB:%f,HS300:%f,IF:%f,A50:%f,XINA50:%f\r\n",totalnum,deviation_temp,HS300Index,avgIf,A50Index,avgFillPriceA50);
			}
			else{
				Price_temp.Format("SP:%d,CB:%f,HS300:%f,IF:%f,A50:%f,XINA50:%f\r\n",totalnum,deviation_temp,HS300Index,avgIf,A50Index,avgFillPriceA50);
			}
			clientDlg->SendMsg(Price_temp);
#endif
			if(!isflat){
				HoldAndDirec hd;
				hd.price = deviation_temp;
				if(!isbuy){
					netHold = netHold - totalnum;
					hd.numDirec = -totalnum;
				}
				else{
					netHold = netHold + totalnum;
					hd.numDirec = totalnum;
				}	
				holdHedge.push_back(hd);
			}
		}
		TRACE("�Գ����,׼�������ٽ���\r\n");
		::LeaveCriticalSection(&tdi_a50);
		delete data;
		TRACE("�Գ����,����0ֵ\r\n");
		return 0;
	}
}
UINT IfEntry(LPVOID pParam){
	IfInData * data = (IfInData*)pParam;
	CThostFtdcInputOrderField req;
	memset(&req, 0, sizeof(req));
	///���͹�˾����
	strcpy(req.BrokerID, BROKER_ID);
	///Ͷ���ߴ���
	strcpy(req.InvestorID, INVESTOR_ID);
	///��Լ����
	strcpy(req.InstrumentID, INSTRUMENT_ID);
	///��������
	//strcpy(req.OrderRef, ORDER_REF);
	sprintf(req.OrderRef,"%d",data->orf);
	///�û�����
	//	TThostFtdcUserIDType	UserID;
	///�����۸�����: �޼�
	req.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
	///��������: 
	if(data->isbuy){
		req.Direction = THOST_FTDC_D_Buy;
	}
	else{
		req.Direction = THOST_FTDC_D_Sell;
	}
	///��Ͽ�ƽ��־: ����
	if(!data->isflat){
		req.CombOffsetFlag[0] = THOST_FTDC_OF_Open;
	}
	else{
		req.CombOffsetFlag[0] = THOST_FTDC_OF_Close;
	}
	///���Ͷ���ױ���־
	req.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
	///�۸�
	::EnterCriticalSection(&g_IfPrice);
	if(!data->isbuy){
		req.LimitPrice = ifBid1 - 150.0;
#ifdef _debug
		req.LimitPrice = ifAsk1 + 3;
#endif
	}
	else{
		req.LimitPrice = ifAsk1 + 150.0;
#ifdef _debug
		req.LimitPrice = ifBid1 - 3;
#endif
	}
	::LeaveCriticalSection(&g_IfPrice);
	///����: 1
	req.VolumeTotalOriginal = data->numif;
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

	std::vector<CThostFtdcOrderField> orderRes;
	std::map<CString,std::vector<CThostFtdcOrderField>>::iterator iOrderRtn;
	std::map<CString,std::vector<CThostFtdcTradeField>>::iterator iTradeRtn;
	std::vector<CThostFtdcOrderField>::iterator iOrderRtnVec;
	std::vector<CThostFtdcTradeField>::iterator iTradeRtnVec;
	CThostFtdcInputOrderActionField reqNew = {'0','0',0,'0',0,0,0,'0','0','0','0',0,0,'0','0'};//������µ��޸�
	CThostFtdcOrderField orderOld;
	CString thisSysId;//ϵͳ��ţ���Ϊ�ѳɽ���Լ��ʶ����
	CString thisRef(req.OrderRef);
	double valueTraded = 0;//�ɽ��ܼ�ֵ
	bool isBreakIn = false;//�Ƿ���Ϊcancel������ǰ��ֹ��Ϣѭ��
	int volumeTraded = 0;
	int endVolumeThisTrade = 0;
	SYSTEMTIME sys;
	while(true){
		GetLocalTime(&sys);
		if(sys.wHour == 9 && sys.wMinute <= 14){
			//9:15ǰ���µ����ȴ�
			continue;
		}
		else{
			break;
		}
	}
	::EnterCriticalSection(&REQUEST_ID_IF);
	int iResult = pUserApi->ReqOrderInsert(&req, ++iRequestID);
	endVolumeThisTrade = req.VolumeTotalOriginal;
	::LeaveCriticalSection(&REQUEST_ID_IF);
	int lastpos = -1;//�������Ϣ�����Ѿ������λ��
	while(true){ 
		//��Ϣ���ش���
		int size = 0;
		SwitchToThread();
		::EnterCriticalSection(&ifDealRtn);
		iOrderRtn = ifOrderRtn.find(thisRef);
		if(iOrderRtn != ifOrderRtn.end() && iOrderRtn->second.size() != 0){//֤������Ѿ�������Ϣ
			//if(iOrderRtn->second.size() != 0){
			size = iOrderRtn->second.size();
			for(int k = 0/*lastpos + 1*/;k < size;k++){
				iOrderRtnVec = iOrderRtn->second.begin() + k;
				orderOld = *iOrderRtnVec;//��ȡ���صı�������,״̬�᲻ͣ�仯��ȡ������ĩβ��ȡ���µ�����
				thisSysId = iOrderRtnVec->OrderSysID;//��ȡ�������
				TRACE("��������=%s,�������=%s\r\n",iOrderRtnVec->OrderRef,thisSysId);
				/*
				//����ʲôԭ�򣬾���ʾ�Ѿ�traded
				if(iOrderRtnVec->VolumeTraded == iOrderRtnVec->VolumeTotalOriginal){
				TRACE("%s\r\n","�Ѿ��ɽ������Ǵ�OrderRtn����");
				::EnterCriticalSection(&avgPriceOfIf);
				valueTraded = valueTraded + iOrderRtnVec->VolumeTraded * iOrderRtnVec->LimitPrice;
				avgpIf avg(data->numif,valueTraded / data->numif);
				avgPriceIf.push_back(avg);
				::LeaveCriticalSection(&avgPriceOfIf);
				//ɾ����ֵ
				ifOrderRtn.erase(iOrderRtn);
				TRACE("׼������ifDealRtn�ٽ���\r\n");
				::LeaveCriticalSection(&ifDealRtn);
				delete (IfInData*)pParam;
				return 0;
				}
				*/
				//���ȡ��,�����µ�
				if(iOrderRtnVec->OrderStatus == THOST_FTDC_OST_Canceled){
					endVolumeThisTrade = endVolumeThisTrade - (iOrderRtnVec->VolumeTotalOriginal - iOrderRtnVec->VolumeTraded);					
					//ɾ����ֵ
					//ifOrderRtn.erase(iOrderRtn);
					/*
					isBreakIn = true;
					break;//��ֹ��ѭ��
					*/
				}
			}
			/*
			if(isBreakIn){
			isBreakIn = false;
			lastpos = -1;//��������
			::LeaveCriticalSection(&ifDealRtn);
			continue;
			}
			*/
			//lastpos = size - 1;
			//iOrderRtn->second.clear();//��վͿ��Դ�0ͷ��ʼ����
			::LeaveCriticalSection(&ifDealRtn);
			//}
		}
		else{
			//��û������Ϣ���У���ʾָ�δ��Ч�����ؼ�����������
			::LeaveCriticalSection(&ifDealRtn);
			TRACE("%s\r\n","��û������Ϣ����,����");
			continue;
		}
		//�ɽ����ش���
		::EnterCriticalSection(&ifDealRtn);
		if(endVolumeThisTrade == 0){
			return 0;
		}
		else{
			iTradeRtn = ifTradeRtn.find(thisSysId);
			//֤���Ѿ��гɽ���Ϣ����
			if(iTradeRtn != ifTradeRtn.end()){
				for(iTradeRtnVec = iTradeRtn->second.begin();iTradeRtnVec != iTradeRtn->second.end();iTradeRtnVec++){
					TRACE("%s���������%s\r\n","�ɽ�,��TradeRtn����,",iTradeRtn->second.begin()->OrderSysID);
					::EnterCriticalSection(&avgPriceOfIf);
					valueTraded = valueTraded + iTradeRtnVec->Price * iTradeRtnVec->Volume;
					volumeTraded = volumeTraded + iTradeRtnVec->Volume;
					if(volumeTraded == data->numif){//ȫ���ɽ�
						avgpIf avg(volumeTraded,valueTraded / volumeTraded);
						avgPriceIf.push_back(avg);
						::LeaveCriticalSection(&avgPriceOfIf);
						//ɾ����ֵ
						ifTradeRtn.erase(iTradeRtn);
						ifOrderRtn.erase(iOrderRtn);
						::LeaveCriticalSection(&ifDealRtn);
						return 0;
					}
					::LeaveCriticalSection(&avgPriceOfIf);
				}
				iTradeRtn->second.clear();//���ɽ��������
			}
		}
		::LeaveCriticalSection(&ifDealRtn);
		if(volumeTraded == endVolumeThisTrade && endVolumeThisTrade < data->numif){//���γɽ�����,���ǻ�û��ȫ���ɽ�,��Ҫ�����µ�
			TRACE("%s\r\n","�����µ�");
			::EnterCriticalSection(&g_IfPrice);
			if(req.Direction == THOST_FTDC_D_Buy){//BUY
				req.LimitPrice = ifBid1;
#ifdef _debug
				req.LimitPrice = ifBid1;
#endif
			}
			else{//SELL
				req.LimitPrice = ifAsk1;
#ifdef _debug
				req.LimitPrice = ifAsk1;
#endif
			}
			::LeaveCriticalSection(&g_IfPrice);
			req.VolumeTotalOriginal = data->numif - endVolumeThisTrade;
			endVolumeThisTrade = data->numif;//��������
			::EnterCriticalSection(&ODREF);
			iNextOrderRef++;
			TThostFtdcOrderRefType orderRefNew;
			sprintf(orderRefNew,"%d", iNextOrderRef);
			::LeaveCriticalSection(&ODREF);
			strcpy(req.OrderRef,orderRefNew);
			thisRef = CString(orderRefNew);
			::EnterCriticalSection(&REQUEST_ID_IF);
			int iResult = pUserApi->ReqOrderInsert(&req, ++iRequestID);
			::LeaveCriticalSection(&REQUEST_ID_IF);
		}
		//����۸��������仯��ȡ������ 
		UpdateTrade.Lock(INFINITE);//�ȴ�ͬ���¼�����
		::EnterCriticalSection(&g_IfPrice);
		if((req.Direction == THOST_FTDC_D_Buy && req.LimitPrice < ifBid1) ||//��
			(req.Direction == THOST_FTDC_D_Sell && req.LimitPrice > ifAsk1)){//��
				if (IsTradingOrder(&orderOld)){
					TRACE("%s\r\n","�ĵ�������ɾ��");
					ReqOrderAction(&orderOld,reqNew);
				}
		}
		::LeaveCriticalSection(&g_IfPrice);
	}
	delete (IfInData*)pParam;
	return iResult;
}
void ReqQryIFTradingAccount()
{
	CThostFtdcQryTradingAccountField req;
	memset(&req, 0, sizeof(req));
	strcpy(req.BrokerID, BROKER_ID);
	strcpy(req.InvestorID, INVESTOR_ID);
	::EnterCriticalSection(&REQUEST_ID_IF);//if����ID�����ٽ���
	rqIfID = ++iRequestID;//��¼��ѯID
	int iResult = pUserApi->ReqQryTradingAccount(&req,iRequestID);	
	::LeaveCriticalSection(&REQUEST_ID_IF);
	//cerr << "--->>> �����ѯ�ʽ��˻�: " << ((iResult == 0) ? "�ɹ�" : "ʧ��") << endl;
}
void ReqOrderAction(CThostFtdcOrderField *pOrder,CThostFtdcInputOrderActionField t_req){
	//CThostFtdcInputOrderActionField req;
	memset(&t_req, 0, sizeof(t_req));
	///���͹�˾����
	strcpy(t_req.BrokerID, BROKER_ID);
	///Ͷ���ߴ���
	strcpy(t_req.InvestorID, INVESTOR_ID);
	///������������
	//	TThostFtdcOrderActionRefType	OrderActionRef;
	///��������
	strcpy(t_req.OrderRef, pOrder->OrderRef);
	///������
	//	TThostFtdcRequestIDType	RequestID;
	///ǰ�ñ��
	t_req.FrontID = pOrder->FrontID;//FRONT_ID;
	///�Ự���
	t_req.SessionID = pOrder->SessionID;//SESSION_ID;
	///����������
	//	TThostFtdcExchangeIDType	ExchangeID;
	///�������
	//	TThostFtdcOrderSysIDType	OrderSysID;
	///������־
	t_req.ActionFlag = THOST_FTDC_AF_Delete;//ֻ�г�����û�иĵ�����
	///�۸�
	//TThostFtdcPriceType	LimitPrice;
	///�����仯
	//	TThostFtdcVolumeType	VolumeChange;
	///�û�����
	//	TThostFtdcUserIDType	UserID;
	///��Լ����
	strcpy(t_req.InstrumentID,INSTRUMENT_ID );
	::EnterCriticalSection(&REQUEST_ID_IF);
	int iResult = pUserApi->ReqOrderAction(&t_req, ++iRequestID);
	::LeaveCriticalSection(&REQUEST_ID_IF);
}
IfInData::IfInData(int num,bool ib,bool iflt,int orderref){
	this->numif = num;
	this->isbuy = ib;
	this->isflat = iflt;
	this->orf = orderref;
}
IfInData::IfInData(){
	this->numif = 0;
	this->isbuy = false;
	this->isflat = false;
	this->orf = 0;
}
avgpIf::avgpIf(int num,double price){
	this->num = num;
	this->avgprice = price;
}
void calDeviation(){
	::EnterCriticalSection(&g_IfPrice);//����Ƕ��ʹ��ֻ����һ��,�ڶ������ò��Ҹı�Ƕ�״���,���׵�������
	::EnterCriticalSection(&g_A50Price);
	::EnterCriticalSection(&g_index);
	if(clientDlg != NULL){
		//dltInterest = (lifeOfA50 - clientDlg->lifeOfIf) / 365.0 * 0.0631;//6.31%���޷�������
		dltInterest = 0;//�İ治����Ϣ��
	}
	static bool first = true;
	if(first){
		TRACE("lifeA50%d,lifeIf%d",lifeOfA50,clientDlg->lifeOfIf);
		TRACE("��Լ����%f\r\n",dltInterest * A50Index);
		first = false;
	}
	premium = (a50Bid1 + a50Ask1) / 2.0 - (ifAsk1 + ifBid1) / 2.0 * A50Index / HS300Index
		- A50Index * dltInterest; 
	premiumHigh = a50Ask1 - ifBid1 * A50Index / HS300Index
		- A50Index * dltInterest;
	premiumLow = a50Bid1 - ifAsk1 * A50Index / HS300Index
		- A50Index * dltInterest;
	::LeaveCriticalSection(&g_index);
	::LeaveCriticalSection(&g_A50Price);
	::LeaveCriticalSection(&g_IfPrice);
	deviation = premium - datumDiff;
	deviationHigh = premiumHigh - datumDiff;
	deviationLow = premiumLow - datumDiff;
}
bool IsTradingOrder(CThostFtdcOrderField *pOrder)
{
	return ((pOrder->OrderStatus != THOST_FTDC_OST_PartTradedNotQueueing) &&
		(pOrder->OrderStatus != THOST_FTDC_OST_Canceled) &&
		(pOrder->OrderStatus != THOST_FTDC_OST_AllTraded));
}
UINT TradeEntry_01(LPVOID pParam)//�����߳�1�İ�
{
#define NUMLADDER 11
	double ladder_01[NUMLADDER] = {-100,-80,-60,-40,-20,0,20,40,60,80,100};//��ʱ��11��,һ��Ҫ�����Գ��ҳʽ���״�������߼�����
	int needAmount_01[NUMLADDER] = {5,4,3,2,1,0,-1,-2,-3,-4,-5};//ÿ����Ӧ�ĳֲ�����
	int holdA50_01 = 0;//A50��������
	int holdIf_01 = 0;//IF��������
	int needHoldA50_01 = 0;//A50Ӧ�ó�������
	int needHoldIf_01 = 0;//IFӦ�ó�������
	int nowLadder_01 = 0;//��ǰ�ݼ�
	bool isFirst = true;//��һ�β���
	bool isFilledSectionLeft = false;//���������Ŀ��λ�����������Ƿ����
	bool isFilledSectionRight = false;//�������Ҳ�Ŀ��λ�����������Ƿ����
	int nowSection = 4444;//��ǰ���ڵ�����,����˵����,ȡ4444�ĳ�ֵ��ʾ��δ�ҵ�����
	if(clientDlg == NULL){
		return -1;
	}
	if(pUserApi == NULL){
		return -1;
	}	
	//�ӶԻ����ʼ��
	datumDiff = clientDlg->m_DeltaDatumDiff + clientDlg->m_datumDiff;
#ifdef _debug 
	holdA50_01 = 50;
#endif
	while(true){
		UpdateTrade.Lock(INFINITE);//�¼�����
		//��ȡϵͳʱ��
		SYSTEMTIME sys;
		GetLocalTime(&sys);
		CString PREMIUM;	
		calDeviation();
		deviationHigh_save = deviationHigh;
		deviationLow_save = deviationLow;
		PREMIUM.Format(_T("%.4f"),premiumLow);	
		clientDlg->SetDlgItemTextA(IDC_EDIT18,PREMIUM);
		PREMIUM.Format(_T("%.4f"),premiumHigh);	
		clientDlg->SetDlgItemTextA(IDC_EDIT16,PREMIUM);
		//�ǽ����շ���
		if(!isTradeDay(sys.wYear,sys.wMonth,sys.wDay))
		{
			continue;
		}
		//�ǳ��潻��ʱ��
		if((sys.wHour == 9 && sys.wMinute < 10) || 
			(sys.wHour == 15 && sys.wMinute > 15) ||
			(sys.wHour == 11 && sys.wMinute > 30) ||
			(sys.wHour == 12)||
			(sys.wHour < 9) || (sys.wHour > 15)){
				continue;
		}
		if(clientDlg->tradeEnd){//��ֹ
			//����մ���,��������ʱ���ʼ��
			return -1;
		}
		if(clientDlg->stop){//��ͣ
			continue;
		}
		if(_isnan(datumDiff) != 0 || _isnan(premium)!=0 ||_isnan(deviation)!=0){
			continue;//�жϷ���ֵ����
		}
		if(deviationHigh >= 100 || deviationLow <= -100){
			continue;//������Χ������
		}
		for(int i = 0;i <= NUMLADDER - 1;i++){//���㵱ǰ������λ��
			if(deviationLow >= ladder_01[i] && deviationHigh <= ladder_01[i + 1]){
				if(isFirst){
					nowSection = i;
					isFirst = false;
					break;
				}
				else{
					if(nowSection != i){
						nowSection = i;
						break;
					}
				}
			}
		}
		if(nowSection == 4444){
			continue;//��δ�ҵ�����,��������Ѱ��
		}
		bool isFilledL = false,isFilledR = false;
		int needA50L = 0,needA50R = 0;
		int needIfL = 0,needIfR = 0;
		needIfL = -needAmount_01[nowSection];
		needIfR = -needAmount_01[nowSection + 1];
		needA50L = needAmount_01[nowSection] * multiply;
		needA50R = needAmount_01[nowSection + 1] * multiply;
		//ǿƽ
		if(holdA50_01 > needA50L){
			//FTA50(holdA50_01 - needA50L,false);
			holdA50_01 = needA50L;
		}
		if(holdIf_01 < needIfL){
			//FTIF(needIfL - holdIf_01,true);
			holdIf_01 = needIfL;
		}
		if(holdA50_01 < needA50R){
			//FTA50(needA50R - holdA50_01,true);
			holdA50_01 = needA50R;
		}
		if(holdIf_01 > needIfR){
			//FTIF(holdIf_01 - needIfR,false);
			holdIf_01 = needIfR;
		}
		//��ѯa50�˻������ʽ�
		//

		if(nowSection < (NUMLADDER - 1) / 2){
			if(holdA50_01 < needA50L || holdIf_01 < needIfL){
				isFilledL = false;
			}
			if(holdA50_01 < needA50R || holdIf_01 < needIfR){
				isFilledR = false;
			} 
		}

		/*
		if(deviation <= (ladder_01[nowSection] + ladder_01[nowSection + 1]) / 2.0){
		double A50Price = ladder_01[nowSection] + datumDiff + ifBid1 * A50Index / HS300Index + A50Index * dltInterest;
		buyOrSell_01(holdA50_01,holdIf_01,needAmount_01[nowSection] * multiply,true);
		}
		else{
		double A50Price = ladder_01[nowSection + 1] + datumDiff + ifAsk1 * A50Index / HS300Index + A50Index * dltInterest;
		buyOrSell_01(holdA50_01,holdIf_01,needAmount_01[nowSection + 1] * multiply,false);
		}
		*/
	}
	return 0;
}
int buyOrSell_01(int holdA50_temp,int holdIf_temp,int objA50_temp,bool isbuy){
	int needIf_temp;
	if(holdA50_temp * objA50_temp < 0){
		//���A50

		//��������
		holdA50_temp = 0;
	}
	if(holdIf_temp * objA50_temp > 0){
		//���if

		//����
		holdIf_temp = 0;
	}
	int availIf_temp,availA50_temp;//ע�⣬��Ϊ��ֵ
	//?��ѯ������if�ɿ���
	//END
	//?��ѯ������A50�ɿ���
	//END
	if(objA50_temp >= 0){
		availA50_temp = min(availA50_temp,(- availIf_temp - holdIf_temp) * multiply + multiply / 2 - holdA50_temp);
	}
	else{
		availA50_temp = min(availA50_temp,(availIf_temp + holdIf_temp) * multiply + multiply / 2 + holdA50_temp);
	}
	/*
	if((objA50_temp) >= 0){
	needIf_temp = (objA50_temp + multiply / 2) / multiply - holdIf_temp;	
	} 
	else{
	needIf_temp = -(-(objA50_temp) + multiply / 2) / multiply - holdIf_temp;
	}
	*/
	if(isbuy){
		if(holdA50_temp >= objA50_temp){
			return 0;
		}
		else{
			if(holdA50_temp < 0){
				//��ƽ
			}
			//����
		}
	}
	else{
		if(holdA50_temp <= objA50_temp){
			return 0;
		}
		else{
			if(holdA50_temp > 0){
				//��ƽ
			}
			//����
		}
	}
	return 0;
}

