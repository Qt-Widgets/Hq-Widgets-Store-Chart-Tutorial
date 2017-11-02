#include "qeastmoneyzjlxthread.h"
#include <QDebug>
#include "qhttpget.h"

QEastMoneyZjlxThread::QEastMoneyZjlxThread(QObject *parent) : QObject(parent)
{
    QString wkURL = QString("http://nufm.dfcfw.com/EM_Finance2014NumericApplication/JS.aspx/JS.aspx?type=ct&st=(BalFlowMain)&sr=-1&p=1&ps=5000&js=var%20jRmrwiia={pages:(pc),data:[(x)]}&token=894050c76af8597a853f5b408b759f5d&cmd=C._AB&sty=DCFFITA&rt=49490052");
    qRegisterMetaType<QList<zjlxData>>("const QList<zjlxData>&");
    mHttp = new QHttpGet(wkURL, true);
    mHttp->setUpdateInterval(60);
    connect(mHttp, SIGNAL(signalSendHttpConent(QByteArray)), this, SLOT(slotRecvHttpContent(QByteArray)));
    this->moveToThread(&mWorkThread);
    mWorkThread.start();
    mHttp->startGet();
}

QEastMoneyZjlxThread::~QEastMoneyZjlxThread()
{
    if(mHttp)
    {
        mHttp->deleteLater();
    }
    mWorkThread.quit();
    mWorkThread.wait();
}

void QEastMoneyZjlxThread::slotRecvHttpContent(const QByteArray& bytes)
{
    //开始解析数据
    QString result = QString::fromLocal8Bit(bytes.data());
    int startindex = -1, endindex = -1;
    startindex = result.indexOf("[") + 1;
    endindex = result.indexOf("]");
    if(startindex < 0 || endindex < 0) return;
    QString hqStr = result.mid(startindex, endindex - startindex);
    //qDebug()<<"zjlx start:"<<startindex<<" end:"<<endindex;
    if( !hqStr.isEmpty() )
    {
        QList<zjlxData> list;
        QStringList blockInfoList = hqStr.split(QRegExp("\""), QString::SkipEmptyParts);
        foreach (QString info, blockInfoList) {
            QStringList InfoList = info.split(QRegExp(","), QString::SkipEmptyParts);
            if(InfoList.size() < 6) continue;
            zjlxData data;
            data.code = InfoList.at(1);
            data.zjlx = InfoList.at(5).toDouble();
            //qDebug()<<"list "<<list.length() +1<<" code:"<<data.code<<" zjlx:"<<data.zjlx<<endl;
            list.append(data);
        }

        emit sendZjlxDataList(list);

    }
}

