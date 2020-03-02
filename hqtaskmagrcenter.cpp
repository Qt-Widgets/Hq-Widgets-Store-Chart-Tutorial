﻿#include "hqtaskmagrcenter.h"
#include "dbservices/dbservices.h"
#include "basic_info/qsharebasicinfoworker.h"
#include "history/qsharehistoryinfomgr.h"
#include "real/qsinastkinfothread.h"
#include "real/qeastmoneynorthboundthread.h"
#include "real/qsinastkresultmergethread.h"
#include "block/qeastmoneyblockmangagerthread.h"
#include "basic_info/qsharecodeswork.h"
#include "basic_info/qsharehsgttop10work.h"
#include "basic_info/qsharefhspwork.h"
#include "basic_info/qsharefinancialinfowork.h"
#include "basic_info/qshareactivedateupdatethread.h"
#include "real/qhqindexthread.h"
#include "real/qhqeastmoneyrealinfothread.h"

HQTaskMagrCenter* HQTaskMagrCenter::m_pInstance = 0;
HQTaskMagrCenter::CGarbo HQTaskMagrCenter::s_Garbo;

HQTaskMagrCenter::HQTaskMagrCenter(QObject *parent) : \
    QObject(parent),\
    mHistoryInfoMgr(0),
    mTimeMonitorThread(0)
{
    //创建时间监控线程
    mTimeMonitorThread = new QShareActiveDateUpdateThread(this);
    connect(mTimeMonitorThread, SIGNAL(signalUpdateHistoryWorkDays()), this, SLOT(slotFinishUpdateWorkDays()));
    connect(mTimeMonitorThread, SIGNAL(signalNewWorkDateNow()), this, SLOT(slotFinishUpdateWorkDays()));
    connect(mTimeMonitorThread, SIGNAL(signalSystemStatus(qint64,int)), this, SIGNAL(signalCurSystemInfo(qint64,int)));

    connect(DATA_SERVICE, SIGNAL(signalDbInitFinished()), this, SLOT(slotDBInitFinished()));
    connect(DATA_SERVICE, SIGNAL(signalAllShareCodeList(QStringList)), this, SLOT(slotShareCodesListFinished(QStringList)));
    connect(this, SIGNAL(signalSearchCodesOfText(QString)), DATA_SERVICE, SIGNAL(signalSearchCodesOfText(QString)));
    connect(this, SIGNAL(signalSetFavCode(QString)), this, SLOT(slotSetFavCode(QString)));

    this->moveToThread(&mWorkThread);
    mWorkThread.start();
}

HQTaskMagrCenter*  HQTaskMagrCenter::instance()
{
    if(m_pInstance == 0)
    {
        m_pInstance = new HQTaskMagrCenter;
    }

    return m_pInstance;
}

HQTaskMagrCenter::~HQTaskMagrCenter()
{
    if(mTimeMonitorThread) mTimeMonitorThread->quit();
    mWorkThread.quit();
}

void HQTaskMagrCenter::start()
{
    //开启数据库初始化
    DATA_SERVICE->signalInitDBTables();
}

void HQTaskMagrCenter::slotDBInitFinished()
{
    if(mTimeMonitorThread) mTimeMonitorThread->start();

}

void HQTaskMagrCenter::slotFinishUpdateWorkDays()
{
//    emit signalWorkingDayfinished();
    qDebug()<<__FUNCTION__<<__LINE__;
    //数据库初始化完成,开始获取最新的代码列表
    QShareCodesWork *codeWorker = new QShareCodesWork(this);
    connect(codeWorker, SIGNAL(finished()), codeWorker, SLOT(deleteLater()));
    codeWorker->start();


    //开始获取当日沪深股通数据，线程常驻，直到当日数据确定获取
    QShareHsgtTop10Work* hsgtTop10 = new QShareHsgtTop10Work(this);
    connect(hsgtTop10, SIGNAL(finished()), hsgtTop10, SLOT(deleteLater()));
    hsgtTop10->start();
}

void HQTaskMagrCenter::slotNewWorDayChangeNow()
{
    if(mHistoryInfoMgr) mHistoryInfoMgr->signalStartStatic();
}


void HQTaskMagrCenter::slotSetFavCode(const QString &code)
{
    //if(mShareBasicWorker) mShareBasicWorker->signalUpdateFavCode(code);
    emit DATA_SERVICE->signalSetFavCode(code);
}



void HQTaskMagrCenter::slotUpdateHSGTOfCode(const QString &code)
{

}


void HQTaskMagrCenter::addSpecialConcern(const QString &code)
{
}

void HQTaskMagrCenter::reverseSortRule()
{
//    if(mBlockMgr)   mBlockMgr->reverseSortRule();
}

void HQTaskMagrCenter::setCurBlockType(int type)
{
//    if(mBlockMgr)   mBlockMgr->setCurBlockType(type);
}

void HQTaskMagrCenter::slotShareCodesListFinished(const QStringList& codes)
{
    emit signalSendAllShareCodesList(codes);
    //获取财务信息
    QShareFinancialInfoWork* finance = new QShareFinancialInfoWork(codes, this);
    connect(finance, SIGNAL(finished()), finance, SLOT(deleteLater()));
    finance->start();
    //获取分配信息
    QShareFHSPWork* fhsp = new QShareFHSPWork(this);
    connect(fhsp, SIGNAL(finished()), fhsp, SLOT(deleteLater()));
    fhsp->start();
    return;

    //更新日线数据
    mHistoryInfoMgr = new QShareHistoryInfoMgr(codes);
    connect(mHistoryInfoMgr, SIGNAL(signalUpdateHistoryMsg(QString)), this, SIGNAL(signalUpdateHistoryMsg(QString)));
    connect(mHistoryInfoMgr, SIGNAL(signalUpdateHistoryFinished()), this, SLOT(slotUpdateHistoryFinished()));
    mHistoryInfoMgr->signalStartGetHistory();
    return;
}

void HQTaskMagrCenter::slotUpdateHistoryFinished()
{
    QShareHistoryInfoMgr* ptr = qobject_cast<QShareHistoryInfoMgr*>(sender());
    if(ptr)
    {
        ptr->deleteLater();
    }

    emit signalUpdateHistoryMsg("");
}
