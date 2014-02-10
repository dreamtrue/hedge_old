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
//using namespace std;//不要用std,否则导致冲突,显式用std::调用吧
int lifeOfA50;
extern int iNextOrderRef;
extern std::map<CString,std::vector<CThostFtdcOrderField>> ifOrderRtn;
extern std::map<CString,std::vector<CThostFtdcTradeField>> ifTradeRtn;
double datumDiff;//交易基差
extern TThostFtdcFrontIDType	FRONT_ID;	//前置编号
extern TThostFtdcSessionIDType	SESSION_ID;	//会话编号
extern TThostFtdcOrderRefType	ORDER_REF;	//报单引用
void ReqOrderAction(CThostFtdcOrderField *pOrder,CThostFtdcInputOrderActionField t_req);
class IfInData{//交易if的输入参数
public:
	int numif;
	bool isbuy;
	bool isflat;
	int orf;
	IfInData(int num,bool ib,bool iflt,int orderref);
	IfInData();
};
UINT IfEntry(LPVOID pParam);//交易if的入口函数 
//IF价格临界区
extern CRITICAL_SECTION g_IfPrice;//临界区
extern CRITICAL_SECTION g_A50Price;
extern CRITICAL_SECTION g_index;
extern CRITICAL_SECTION tdi_a50;
extern CRITICAL_SECTION REQUEST_ID_IF;//if请求ID操作临界区
extern CRITICAL_SECTION avgPriceOfIf;
extern CRITICAL_SECTION ifDealRtn;//处理成交信息返回临界区
extern CRITICAL_SECTION ODREF;//if合约报单引用临界区
//同步事件对象
CEvent ReqAccount(true,true,NULL,NULL);//IF帐户余额查询同步,初始为触发,手动复位
CEvent UpdateTrade(false,false,NULL,NULL);//交易提醒同步,初始为未触发,自动复位
int rqIfID = -1;//查询IF帐户余额时的ID记录,用于核对,初始化为-1,保证已开始不存在这样的ID
double deviation;
double deviationHigh,deviationLow;
extern double a50Bid1,a50Ask1;
extern double a50Bid1Size,a50Ask1Size;
extern double ifAsk1,ifBid1,A50Index,HS300Index;
extern CClient2Dlg * clientDlg;
extern CThostFtdcTraderApi* pUserApi;
struct HoldAndDirec{
	double price;
	int numDirec;//带有方向性的持仓数量
};
std::vector<HoldAndDirec> holdHedge;//在只允许一个A50交易线程的情况下,暂不考虑同步
std::vector<HoldAndDirec> waiting_hold;//待交易的持仓,同上,也不考虑同步
std::vector<HoldAndDirec>::iterator itHedge;
int multiply = 15;//A50乘数
int aimOfLadder = 0;
double step = 20.0;
double premium = 0;//溢价
double premiumHigh = 0;//采用单边叫价获得的最高溢价
double premiumLow = 0;//采用单边叫价获得的最低溢价
double deviationHigh_save,deviationLow_save;
int netHold = 0;//净持有
//外盘可用资金
extern double availA50;
//内盘可用资金余额
extern int iRequestID;
extern double availIf;
extern int rtnAvailIfID;//查询帐户余额返回的ID，用来核对
void ReqQryIFTradingAccount();//查询if帐户余额的函数
//A50合约参数
double marginA50 = 625.0;
//300合约参数
double marginIf = 300 * 2553 * 0.12;
extern int iRequestID;
int ladder = 0;
int needHold = 0;
double dltInterest = 0;
double profitTarget = 0;//盈利目标
int BS(int needHold_temp,int netHold_temp);
int flatOrOpen(int num,bool isbuy,bool isflat);
//成交返回信息
extern bool rtnTradeIf;//if合约成交返回
//if合约信息
extern TThostFtdcInstrumentIDType INSTRUMENT_ID;	// 合约代码
extern TThostFtdcBrokerIDType	BROKER_ID;				// 经纪公司代码
extern TThostFtdcInvestorIDType INVESTOR_ID;			// 投资者代码
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
#define _weixin //微信传送消息
//只能开启一个交易线程,否则同步可能出现问题
UINT TradeEntry_00(LPVOID pParam)//启动入口，作为一个独立的线程
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
	iNextOrderRef++;//测试
	ifAsk1 = 2180;//
	IfInData *data;//测试
	data = new IfInData(1,false,true,iNextOrderRef); //测试
	AfxBeginThread(IfEntry,data,THREAD_PRIORITY_NORMAL,0,0,NULL);//测试
	return 0;//测试
#endif
	if(clientDlg == NULL){
		return -1;
	}
	if(pUserApi == NULL){
		return -1;
	}	
	//从对话框初始化
	step = clientDlg->m_step;
	multiply = clientDlg->m_multiply;
	aimOfLadder = clientDlg->m_aimOfLadder;
	datumDiff = clientDlg->m_DeltaDatumDiff + clientDlg->m_datumDiff;
	HoldAndDirec tempIni;
	tempIni.price = clientDlg->m_costINI - datumDiff;//持仓相对价格
	tempIni.numDirec = clientDlg->m_numINI;//带有方向的持仓
	holdHedge.push_back(tempIni);
	netHold = tempIni.numDirec;
	//溢价和基差计算
	while(true){
		UpdateTrade.Lock(INFINITE);//事件触发
		//获取系统时间
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
		//非交易日返回
		if(!isTradeDay(sys.wYear,sys.wMonth,sys.wDay))
		{
		continue;
		}
		//非常规交易时间
		if((sys.wHour == 9 && sys.wMinute < 10) || 
		(sys.wHour == 15 && sys.wMinute > 15) ||
		(sys.wHour == 11 && sys.wMinute > 30) ||
		(sys.wHour == 12)||
		(sys.wHour < 9) || (sys.wHour > 15)){
		continue;
		}
		*/
		if(clientDlg->tradeEnd){//终止
			//做清空处理,重新启动时会初始化
			holdHedge.clear();
			netHold = 0;
			return -1;
		}
		if(clientDlg->stop){//暂停
			continue;
		}
		if(_isnan(datumDiff) != 0 || _isnan(premium)!=0 ||_isnan(deviation)!=0){
			continue;//判断非零值错误
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
			continue;//排除开盘时有可能报价不全导致的错误溢价计算
		}
		if(deviation >= 0){
#ifdef _case1
			//临时用multiply代替需要的合约数目，因为现在只有一手，保证成交
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
			//临时用multiply代替需要的合约数目，因为现在只有一手，保证成交
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
		needHold = aimOfLadder * ladder;//需要的合约与偏差的关系,可以根据资金量进行修改
#ifdef _case1
		profitTarget = step;
#else
		profitTarget = step - 2.5;
#endif
		double profitUnit = 0;
		int needBuyFlat = 0,needSellFlat = 0;//始终大于0的，无正负
		double totalTradingValue = 0;//准备交易的总价值
		for(unsigned int j = 0;j < holdHedge.size();j++){
			itHedge = holdHedge.begin() + j;
			//if(itHedge->numDirec < 0){//其实只要nethold为负，就保证了所有的numDirec为负，不存在既有numDirec为正，又有为负的情况
			if(netHold < 0){
				profitUnit = itHedge->price - deviationHigh;
				if(profitUnit >= profitTarget){
					needBuyFlat = needBuyFlat - itHedge->numDirec;
					totalTradingValue = totalTradingValue + itHedge->price * (-itHedge->numDirec);
					//更新记录
					netHold = netHold - itHedge->numDirec;
					holdHedge.erase(itHedge);
					j--;//出现删除操作时,需要将指针向前倒退
				}
			}
			//else if(itHedge->numDirec > 0)
			else if(netHold > 0){
				profitUnit = -(itHedge->price - deviationLow);
				if(profitUnit >= profitTarget){
					needSellFlat = needSellFlat + itHedge->numDirec;
					totalTradingValue = totalTradingValue + itHedge->price * itHedge->numDirec;
					//更新记录
					netHold = netHold - itHedge->numDirec;
					holdHedge.erase(itHedge);
					j--;
				}
			}
			else{
				//等于0时什么也不做
			}
		}
		//平仓操作,,其实买平和卖平一定只有一个得到执行的，否则便出现了问题
		flatOrOpen(needBuyFlat,true,true);
		flatOrOpen(needSellFlat,false,true);
		//开仓操作
		BS(needHold,netHold);
	}
	return 0;//根本走不到这儿
}
int BS(int needHold_temp,int netHold_temp){
	if(needHold_temp > 0){
		if(needHold_temp > netHold_temp){
			if(netHold_temp < 0){//这其实是止损操作
				//买平，需要更新hold数组
				flatOrOpen(-netHold_temp,true,true);
				//更新记录
				netHold = 0;
				holdHedge.clear();
				//买开
				flatOrOpen(needHold_temp,true,false);
			}
			else{
				//买开
				flatOrOpen(needHold_temp - netHold_temp,true,false);
			}
		}
	}
	if(needHold_temp < 0){
		if(needHold_temp < netHold_temp){
			if(netHold_temp > 0){//止损操作
				//卖平
				flatOrOpen(netHold_temp,false,true);
				//更新记录
				netHold = 0;
				holdHedge.clear();
				//卖开
				flatOrOpen(-needHold_temp,false,false);
			}
			else{
				//卖开
				flatOrOpen(netHold_temp - needHold_temp,false,false);
			}
		}
	}
	if(needHold_temp == 0){
		/*
		if(deviation >= 1.0){//表示若持有多单，需要卖平
		if(netHold_temp > 0){
		flatOrOpen(netHold_temp,false,true);
		//更新记录
		netHold = 0;
		holdHedge.clear();
		}
		}
		else if(deviation < -1.0)
		{//表示若持有空单，需要买平
		if(netHold_temp < 0){
		flatOrOpen(-netHold_temp,true,true);
		//更新记录
		netHold = 0;
		holdHedge.clear();
		}
		}
		else{
		//中立区,什么也不干
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
	avgPriceIf.clear();//清空统计平均价格的数组
	::LeaveCriticalSection(&avgPriceOfIf);
	int numAvail = 0;//容许开仓数量
	if(!isflat){//表示开仓
		/*
		ReqAccount.ResetEvent();//事件同步,设置为未发信状态
		ReqQryIFTradingAccount();//查询if帐户可用资金
		ReqAccount.Lock(INFINITE);//等待
		if(rtnAvailIfID == rqIfID){
		//留有10和100人民币的余地
		marginIf = ifAsk1 * 300.0 * 0.12;//12%的保证金率
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
			TRACE("买平%d手\r\n",num);
		}
		else{
			TRACE("卖平%d手\r\n",num);
		}
	}
	else{
		if(isbuy){
			TRACE("买开%d手\r\n",num);
		}
		else{
			TRACE("卖开%d手\r\n",num);
		}
	}
	int numofA50 = num * multiply;
	int numofIf = num;
	std::vector<HANDLE> handleThread;//存放线程句柄的数组
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
		int iftraded = 0;//已经发出的if指令,A50每次交易满一手对冲,瞬间发出if指令
		int filledLast = 0;//最近一次的成交数量
		long idThisTrade;//交易id
		std::vector<rtn> rtnres;//交易结果返回
		int lastpos = -1;//上次检查返回新的vector的数据位置,从-1开始,考虑到0位置前用-1表示
		::EnterCriticalSection(&tdi_a50);
		idThisTrade = clientDlg->PlaceOrder_hedge(false,orderA50);
		clientDlg->m_idrtn.insert(std::pair<long,std::vector<rtn>>(idThisTrade,rtnres));
		::LeaveCriticalSection(&tdi_a50);
		while(true){	
			std::map<long,std::vector<rtn>>::iterator iter;
			std::vector<rtn>::iterator ivec;
			int seconds = 0;//计时
			while(!isA50Validate){
				//设置临界区,防止处理期间的m_idrtn变动
				//每次都要重新获取开始位置,因为map中元素次序会变
				::EnterCriticalSection(&tdi_a50);
				iter = clientDlg->m_idrtn.find(idThisTrade);
				if(iter != clientDlg->m_idrtn.end()){
					ivec = iter->second.begin();
					int size = iter->second.size();
					if(size > 0){
						isA50Validate = true;//有返回消息了
						TRACE("A50 HAS VALIDATED\r\n");
						::LeaveCriticalSection(&tdi_a50);
						break;
					}
				}
				TRACE("A50 NOT VALIDATED\r\n");
				::LeaveCriticalSection(&tdi_a50);
				::Sleep(50);//50毫秒检测一次
				seconds = seconds + 50;
				if(seconds > 100000){//100秒没反应,认为指令送达失败
					//重新发送指令
					::EnterCriticalSection(&tdi_a50);
					idThisTrade = clientDlg->PlaceOrder_hedge(false,orderA50);
					clientDlg->m_idrtn.insert(std::pair<long,std::vector<rtn>>(idThisTrade,rtnres));
					::LeaveCriticalSection(&tdi_a50);
					seconds = 0;//重新计时
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
									int needTradeIf = ((ivec + k)->filled - iftraded * multiply) / multiply;//计算需要新交易的if合约数
									if(needTradeIf >= 1){
										//交易if,生成新的线程
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
										//更新数据
										iftraded = iftraded + needTradeIf;
									}
									//更新上一次已经成交的数目
									filledLast = (ivec + k)->filled;
								}
								if((ivec + k)->status == "Filled" ||
									(ivec + k)->status == "ApiCancelled" ||
									(ivec + k)->status == "Cancelled"){
										//等待if交易线程结束,进行交易平均成本统计
										//等待所有的IF线程交易完毕,返回
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
												TRACE("等待超时,退出检测,因为没有被触发的对象了\r\n");
												break;
											case WAIT_FAILED:
												TRACE("等待失败\r\n");
												break;
											default:
												TRACE("已经全部返回\r\n");
												break;
											}
											delete []handle;
										}
										//统计交易成本,开仓则增加交易成本记录,并返回
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
												- A50Index * (lifeOfA50 - clientDlg->lifeOfIf) / 365.0 * 0.0631;//6.31%的无风险套利; 
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
										//删除这次交易ID对应的键值
										clientDlg->m_idrtn.erase(iter);
										TRACE("对冲结束,准备脱离临界区\r\n");
										::LeaveCriticalSection(&tdi_a50);
										delete data;
										TRACE("对冲结束,返回0值\r\n");
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
			UpdateTrade.Lock(INFINITE);//等待同步事件击发
			//判断价格是否合理,否则修改A50合约
			calDeviation();//重新计算偏差 
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
			//交易if,生成新的线程
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
				TRACE("等待超时,退出检测,因为没有被触发的对象了\r\n");
				break;
			case WAIT_FAILED:
				TRACE("等待失败\r\n");
				break;
			default:
				TRACE("已经全部返回\r\n");
				break;
			}
			delete []handle;
		}
		//统计交易成本,开仓则增加交易成本记录,并返回
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
				- A50Index * (lifeOfA50 - clientDlg->lifeOfIf) / 365.0 * 0.0631;//6.31%的无风险套利; 
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
		TRACE("对冲结束,准备脱离临界区\r\n");
		::LeaveCriticalSection(&tdi_a50);
		delete data;
		TRACE("对冲结束,返回0值\r\n");
		return 0;
	}
}
UINT IfEntry(LPVOID pParam){
	IfInData * data = (IfInData*)pParam;
	CThostFtdcInputOrderField req;
	memset(&req, 0, sizeof(req));
	///经纪公司代码
	strcpy(req.BrokerID, BROKER_ID);
	///投资者代码
	strcpy(req.InvestorID, INVESTOR_ID);
	///合约代码
	strcpy(req.InstrumentID, INSTRUMENT_ID);
	///报单引用
	//strcpy(req.OrderRef, ORDER_REF);
	sprintf(req.OrderRef,"%d",data->orf);
	///用户代码
	//	TThostFtdcUserIDType	UserID;
	///报单价格条件: 限价
	req.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
	///买卖方向: 
	if(data->isbuy){
		req.Direction = THOST_FTDC_D_Buy;
	}
	else{
		req.Direction = THOST_FTDC_D_Sell;
	}
	///组合开平标志: 开仓
	if(!data->isflat){
		req.CombOffsetFlag[0] = THOST_FTDC_OF_Open;
	}
	else{
		req.CombOffsetFlag[0] = THOST_FTDC_OF_Close;
	}
	///组合投机套保标志
	req.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
	///价格
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
	///数量: 1
	req.VolumeTotalOriginal = data->numif;
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

	std::vector<CThostFtdcOrderField> orderRes;
	std::map<CString,std::vector<CThostFtdcOrderField>>::iterator iOrderRtn;
	std::map<CString,std::vector<CThostFtdcTradeField>>::iterator iTradeRtn;
	std::vector<CThostFtdcOrderField>::iterator iOrderRtnVec;
	std::vector<CThostFtdcTradeField>::iterator iTradeRtnVec;
	CThostFtdcInputOrderActionField reqNew = {'0','0',0,'0',0,0,0,'0','0','0','0',0,0,'0','0'};//命令动作新的修改
	CThostFtdcOrderField orderOld;
	CString thisSysId;//系统编号，作为已成交合约的识别码
	CString thisRef(req.OrderRef);
	double valueTraded = 0;//成交总价值
	bool isBreakIn = false;//是否因为cancel命令提前终止消息循环
	int volumeTraded = 0;
	int endVolumeThisTrade = 0;
	SYSTEMTIME sys;
	while(true){
		GetLocalTime(&sys);
		if(sys.wHour == 9 && sys.wMinute <= 14){
			//9:15前不下单，等待
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
	int lastpos = -1;//命令返回消息队列已经处理的位置
	while(true){ 
		//消息返回处理
		int size = 0;
		SwitchToThread();
		::EnterCriticalSection(&ifDealRtn);
		iOrderRtn = ifOrderRtn.find(thisRef);
		if(iOrderRtn != ifOrderRtn.end() && iOrderRtn->second.size() != 0){//证明里边已经有了消息
			//if(iOrderRtn->second.size() != 0){
			size = iOrderRtn->second.size();
			for(int k = 0/*lastpos + 1*/;k < size;k++){
				iOrderRtnVec = iOrderRtn->second.begin() + k;
				orderOld = *iOrderRtnVec;//获取返回的报单命令,状态会不停变化，取到队列末尾，取最新的命令
				thisSysId = iOrderRtnVec->OrderSysID;//获取报单编号
				TRACE("报单引用=%s,报单编号=%s\r\n",iOrderRtnVec->OrderRef,thisSysId);
				/*
				//不管什么原因，均表示已经traded
				if(iOrderRtnVec->VolumeTraded == iOrderRtnVec->VolumeTotalOriginal){
				TRACE("%s\r\n","已经成交，但是从OrderRtn返回");
				::EnterCriticalSection(&avgPriceOfIf);
				valueTraded = valueTraded + iOrderRtnVec->VolumeTraded * iOrderRtnVec->LimitPrice;
				avgpIf avg(data->numif,valueTraded / data->numif);
				avgPriceIf.push_back(avg);
				::LeaveCriticalSection(&avgPriceOfIf);
				//删除键值
				ifOrderRtn.erase(iOrderRtn);
				TRACE("准备脱离ifDealRtn临界区\r\n");
				::LeaveCriticalSection(&ifDealRtn);
				delete (IfInData*)pParam;
				return 0;
				}
				*/
				//如果取消,重新下单
				if(iOrderRtnVec->OrderStatus == THOST_FTDC_OST_Canceled){
					endVolumeThisTrade = endVolumeThisTrade - (iOrderRtnVec->VolumeTotalOriginal - iOrderRtnVec->VolumeTraded);					
					//删除键值
					//ifOrderRtn.erase(iOrderRtn);
					/*
					isBreakIn = true;
					break;//终止内循环
					*/
				}
			}
			/*
			if(isBreakIn){
			isBreakIn = false;
			lastpos = -1;//重新设置
			::LeaveCriticalSection(&ifDealRtn);
			continue;
			}
			*/
			//lastpos = size - 1;
			//iOrderRtn->second.clear();//清空就可以从0头开始检索
			::LeaveCriticalSection(&ifDealRtn);
			//}
		}
		else{
			//还没生成消息队列，表示指令还未生效，返回继续搜索队列
			::LeaveCriticalSection(&ifDealRtn);
			TRACE("%s\r\n","还没生成消息队列,返回");
			continue;
		}
		//成交返回处理
		::EnterCriticalSection(&ifDealRtn);
		if(endVolumeThisTrade == 0){
			return 0;
		}
		else{
			iTradeRtn = ifTradeRtn.find(thisSysId);
			//证明已经有成交信息返回
			if(iTradeRtn != ifTradeRtn.end()){
				for(iTradeRtnVec = iTradeRtn->second.begin();iTradeRtnVec != iTradeRtn->second.end();iTradeRtnVec++){
					TRACE("%s报单编号是%s\r\n","成交,从TradeRtn返回,",iTradeRtn->second.begin()->OrderSysID);
					::EnterCriticalSection(&avgPriceOfIf);
					valueTraded = valueTraded + iTradeRtnVec->Price * iTradeRtnVec->Volume;
					volumeTraded = volumeTraded + iTradeRtnVec->Volume;
					if(volumeTraded == data->numif){//全部成交
						avgpIf avg(volumeTraded,valueTraded / volumeTraded);
						avgPriceIf.push_back(avg);
						::LeaveCriticalSection(&avgPriceOfIf);
						//删除键值
						ifTradeRtn.erase(iTradeRtn);
						ifOrderRtn.erase(iOrderRtn);
						::LeaveCriticalSection(&ifDealRtn);
						return 0;
					}
					::LeaveCriticalSection(&avgPriceOfIf);
				}
				iTradeRtn->second.clear();//将成交数组清空
			}
		}
		::LeaveCriticalSection(&ifDealRtn);
		if(volumeTraded == endVolumeThisTrade && endVolumeThisTrade < data->numif){//本次成交结束,但是还没有全部成交,需要重新下单
			TRACE("%s\r\n","重新下单");
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
			endVolumeThisTrade = data->numif;//重新设置
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
		//如果价格发生不利变化，取消订单 
		UpdateTrade.Lock(INFINITE);//等待同步事件击发
		::EnterCriticalSection(&g_IfPrice);
		if((req.Direction == THOST_FTDC_D_Buy && req.LimitPrice < ifBid1) ||//买
			(req.Direction == THOST_FTDC_D_Sell && req.LimitPrice > ifAsk1)){//卖
				if (IsTradingOrder(&orderOld)){
					TRACE("%s\r\n","改单动作，删除");
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
	::EnterCriticalSection(&REQUEST_ID_IF);//if请求ID操作临界区
	rqIfID = ++iRequestID;//记录查询ID
	int iResult = pUserApi->ReqQryTradingAccount(&req,iRequestID);	
	::LeaveCriticalSection(&REQUEST_ID_IF);
	//cerr << "--->>> 请求查询资金账户: " << ((iResult == 0) ? "成功" : "失败") << endl;
}
void ReqOrderAction(CThostFtdcOrderField *pOrder,CThostFtdcInputOrderActionField t_req){
	//CThostFtdcInputOrderActionField req;
	memset(&t_req, 0, sizeof(t_req));
	///经纪公司代码
	strcpy(t_req.BrokerID, BROKER_ID);
	///投资者代码
	strcpy(t_req.InvestorID, INVESTOR_ID);
	///报单操作引用
	//	TThostFtdcOrderActionRefType	OrderActionRef;
	///报单引用
	strcpy(t_req.OrderRef, pOrder->OrderRef);
	///请求编号
	//	TThostFtdcRequestIDType	RequestID;
	///前置编号
	t_req.FrontID = pOrder->FrontID;//FRONT_ID;
	///会话编号
	t_req.SessionID = pOrder->SessionID;//SESSION_ID;
	///交易所代码
	//	TThostFtdcExchangeIDType	ExchangeID;
	///报单编号
	//	TThostFtdcOrderSysIDType	OrderSysID;
	///操作标志
	t_req.ActionFlag = THOST_FTDC_AF_Delete;//只有撤单，没有改单操作
	///价格
	//TThostFtdcPriceType	LimitPrice;
	///数量变化
	//	TThostFtdcVolumeType	VolumeChange;
	///用户代码
	//	TThostFtdcUserIDType	UserID;
	///合约代码
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
	::EnterCriticalSection(&g_IfPrice);//这种嵌套使用只能用一次,第二次再用并且改变嵌套次序,容易导致死锁
	::EnterCriticalSection(&g_A50Price);
	::EnterCriticalSection(&g_index);
	if(clientDlg != NULL){
		//dltInterest = (lifeOfA50 - clientDlg->lifeOfIf) / 365.0 * 0.0631;//6.31%的无风险套利
		dltInterest = 0;//改版不计算息差
	}
	static bool first = true;
	if(first){
		TRACE("lifeA50%d,lifeIf%d",lifeOfA50,clientDlg->lifeOfIf);
		TRACE("合约利差%f\r\n",dltInterest * A50Index);
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
UINT TradeEntry_01(LPVOID pParam)//交易线程1改版
{
#define NUMLADDER 11
	double ladder_01[NUMLADDER] = {-100,-80,-60,-40,-20,0,20,40,60,80,100};//暂时设11档,一定要正负对称且呈阶梯状，否则逻辑混乱
	int needAmount_01[NUMLADDER] = {5,4,3,2,1,0,-1,-2,-3,-4,-5};//每档对应的持仓数量
	int holdA50_01 = 0;//A50持有数量
	int holdIf_01 = 0;//IF持有数量
	int needHoldA50_01 = 0;//A50应该持有数量
	int needHoldIf_01 = 0;//IF应该持有数量
	int nowLadder_01 = 0;//当前梯级
	bool isFirst = true;//第一次操作
	bool isFilledSectionLeft = false;//本区间左侧目标位的买卖操作是否完成
	bool isFilledSectionRight = false;//本区间右侧目标位的买卖操作是否完成
	int nowSection = 4444;//当前所在的区间,由左端点决定,取4444的初值表示还未找到区间
	if(clientDlg == NULL){
		return -1;
	}
	if(pUserApi == NULL){
		return -1;
	}	
	//从对话框初始化
	datumDiff = clientDlg->m_DeltaDatumDiff + clientDlg->m_datumDiff;
#ifdef _debug 
	holdA50_01 = 50;
#endif
	while(true){
		UpdateTrade.Lock(INFINITE);//事件触发
		//获取系统时间
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
		//非交易日返回
		if(!isTradeDay(sys.wYear,sys.wMonth,sys.wDay))
		{
			continue;
		}
		//非常规交易时间
		if((sys.wHour == 9 && sys.wMinute < 10) || 
			(sys.wHour == 15 && sys.wMinute > 15) ||
			(sys.wHour == 11 && sys.wMinute > 30) ||
			(sys.wHour == 12)||
			(sys.wHour < 9) || (sys.wHour > 15)){
				continue;
		}
		if(clientDlg->tradeEnd){//终止
			//做清空处理,重新启动时会初始化
			return -1;
		}
		if(clientDlg->stop){//暂停
			continue;
		}
		if(_isnan(datumDiff) != 0 || _isnan(premium)!=0 ||_isnan(deviation)!=0){
			continue;//判断非零值错误
		}
		if(deviationHigh >= 100 || deviationLow <= -100){
			continue;//超过范围，返回
		}
		for(int i = 0;i <= NUMLADDER - 1;i++){//计算当前的区间位置
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
			continue;//还未找到区间,返回重新寻找
		}
		bool isFilledL = false,isFilledR = false;
		int needA50L = 0,needA50R = 0;
		int needIfL = 0,needIfR = 0;
		needIfL = -needAmount_01[nowSection];
		needIfR = -needAmount_01[nowSection + 1];
		needA50L = needAmount_01[nowSection] * multiply;
		needA50R = needAmount_01[nowSection + 1] * multiply;
		//强平
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
		//查询a50账户可用资金
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
		//清空A50

		//更新数据
		holdA50_temp = 0;
	}
	if(holdIf_temp * objA50_temp > 0){
		//清空if

		//更新
		holdIf_temp = 0;
	}
	int availIf_temp,availA50_temp;//注意，恒为正值
	//?查询并计算if可开数
	//END
	//?查询并计算A50可开数
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
				//买平
			}
			//开仓
		}
	}
	else{
		if(holdA50_temp <= objA50_temp){
			return 0;
		}
		else{
			if(holdA50_temp > 0){
				//卖平
			}
			//开仓
		}
	}
	return 0;
}

