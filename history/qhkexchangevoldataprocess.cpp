﻿#include "qhkexchangevoldataprocess.h"
#include "utils/qhttpget.h"
#include <QDir>
#include <QRegularExpression>
#include "utils/comdatadefines.h"
#include <QTextCodec>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#define     POST_VAL            "__VIEWSTATE=%2FwEPDwUJNjIxMTYzMDAwZGQ79IjpLOM%2BJXdffc28A8BMMA9%2Byg%3D%3D&__VIEWSTATEGENERATOR=EC4ACD6F&__EVENTVALIDATION=%2FwEdAAdtFULLXu4cXg1Ju23kPkBZVobCVrNyCM2j%2BbEk3ygqmn1KZjrCXCJtWs9HrcHg6Q64ro36uTSn%2FZ2SUlkm9HsG7WOv0RDD9teZWjlyl84iRMtpPncyBi1FXkZsaSW6dwqO1N1XNFmfsMXJasjxX85jz8PxJxwgNJLTNVe2Bh%2Fbcg5jDf8%3D&today=TODAY_DATE&sortBy=stockcode&sortDirection=asc&alertMsg=&txtShareholdingDate=TXTSHAREDATE&btnSearch=%E6%90%9C%E5%B0%8B"
#define     HK_URL              "https://www.hkexnews.hk/sdw/search/mutualmarket_c.aspx?t=%1&t=%2"
#define     EASTMONEY_URL       "http://dcfm.eastmoney.com//em_mutisvcexpandinterface/api/js/get?type=HSGTHDSTA&token=70f12f2f4f091e459a279469fe49eca5&st=SHAREHOLDPRICE&sr=-1&p=%1&ps=100000&js={pages:(tp),data:(x)}&filter=(MARKET%20in%20(%27001%27,%27003%27))(HDDATE=^%2^)&rt=52030379"

QHKExchangeVolDataProcess::QHKExchangeVolDataProcess(const QDate& date, QObject* parent) : QRunnable()
{
    mDate = date;
    mParent = parent;
}

void QHKExchangeVolDataProcess::getVolInfoFromHKEX(ShareForignVolFileDataList& list, int& num, const QDate &date, int mkt)
{
    QString postVal = POST_VAL;
    postVal.replace("TODAY_DATE", QDate::currentDate().toString("yyyyMMdd"));
    postVal.replace("TXTSHAREDATE", QString("").sprintf("%s", date.toString("yyyy/MM/dd").toStdString().data()));
    QByteArray value = QHttpGet::getContentOfURLWithPost(QString(HK_URL).arg(mkt == 0? "sh":"sz").arg(mkt == 0? "sh":"sz"), postVal.toUtf8());

#ifdef TEST
    qDebug()<<"recv len:"<<value.length();
    qDebug()<<value;
    QFile file(QString("https_%1").arg(QDateTime::currentMSecsSinceEpoch()));
    if(file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        file.write(value);
        file.close();
    } else
    {
        qDebug()<<__FUNCTION__<<"open file error:"<<file.fileName();
    }
#endif

    QString res = QString::fromUtf8(value);
    int start_index = 0;
    qDebug()<<__FUNCTION__<<start_index;
    QRegExp codeExp("7[072]{1}[0-9]{3}|9[0-9]{4}");
    //QRegExp nameExp("[\u4e00-\u9fa5A-Z]{1,}");
    QRegExp volExp(">(([0-9]{1,3},){0,}[0-9]{1,})<");
    QRegExp dateExp(": (\\d{4}/\\d{2}/\\d{2})");
    QRegExp volPercentExp("([0-9]{1,3}\.[0-9]{2})%");

    start_index = dateExp.indexIn(res, start_index);
    QDate resDate;
    if(start_index >=0 )
    {
        resDate = QDate::fromString(dateExp.cap(1), "yyyy/MM/dd");
    }
    if(resDate != date) return;

    while ( (start_index = codeExp.indexIn(res, start_index)) >= 0) {
        QString tmpCode = codeExp.cap();
        if(tmpCode.left(1) == "7")
        {
            if(tmpCode.left(2)== "77")
            {
                tmpCode.replace(0, 2, "300");
            } else
            {
                tmpCode.replace(0, 1, "00");
            }
        } else
        {
            tmpCode.replace(0, 1, "60");
        }
        int code = tmpCode.toInt();
        //qDebug()<<"res:"<<codeExp.cap();
        start_index += 5;
        start_index = volExp.indexIn(res, start_index);
        if(start_index >= 0)
        {
            ShareForignVolFileData data;
            data.mCode = tmpCode.right(6).toUInt();
            data.mForeignVol = volExp.cap(1).remove(",").toLongLong();
//            data.mTime.setDate(date);
            //qDebug()<<"res:"<<volExp.cap();
            start_index += volExp.cap().length();
            start_index = volPercentExp.indexIn(res, start_index);
            if(start_index >= 0)
            {
                data.mMutuablePercent = volPercentExp.cap(1).toDouble();
                list.append(data);
                //qDebug()<<data.mCode<<data.mHsgtData.mVolTotal<<data.mHsgtData.mVolMutablePercent;
                start_index += volPercentExp.cap().length();
            } else
            {
                break;
            }

        } else
        {
            //qDebug()<<"code:"<<tmpCode<<"not found";
            break;
        }
//        qDebug()<<list.last();
    }
    num = list.size();
    return;
}

bool QHKExchangeVolDataProcess::getVolInfoFromFile(ShareForignVolFileDataList& list, const QDate& date)
{
    if(!QFile::exists(getFileName(date))) return false;
    //读取文件
    QFile file(getFileName(date));
    if(!file.open(QIODevice::ReadOnly)) return false;
    int count = 0;
    int obj_size = sizeof(ShareForignVolFileData);
    while (!file.atEnd() ) {
        ShareForignVolFileData data;
        int size = file.read((char*)(&data), obj_size);
        if(size != obj_size) break;
        list.append(data);
        count++;
    }
    file.close();
    return list.size() > 0;
}

bool QHKExchangeVolDataProcess::getVolInfoFromEastMoney(ShareForignVolFileDataList& list, const QDate &date)
{
    int page = 1;
    int totalpage = 1;
    while (page <= totalpage) {
        QByteArray value = QHttpGet::getContentOfURL(QString(EASTMONEY_URL).arg(page).arg(date.toString("yyyy-MM-dd")));
        value.replace("pages", "\"pages\"");
        value.replace("data", "\"data\"");
        page++;
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(value, &err);
        if(err.error != QJsonParseError::NoError)
        {
            qDebug()<<"error:"<<err.errorString();
            continue;
        }
        if(!doc.isObject()) continue;
        QJsonObject obj = doc.object();
        totalpage = obj.value("pages").toInt();
        QJsonArray array = obj.value("data").toArray();
        for(int i=0; i<array.size(); i++)
        {
            QJsonObject sub = array[i].toObject();
            ShareForignVolFileData data;
            data.mCode = sub.value("SCODE").toInt();
//            data.mName = sub.value("SNAME").toString();
//            data.setDate(QDate::fromString(sub.value("HDDATE").toString().left(10), "yyyy-MM-dd"));
            data.mForeignVol = qint64(sub.value("SHAREHOLDSUM").toDouble());
            data.mMutuablePercent = sub.value("SHARESRATE").toDouble();
//            data.mHsgtData.mVolChange = qint64(sub.value("ShareHoldSumChg").toDouble());
//            data.mClose = sub.value("CLOSEPRICE").toDouble();
//            data.mChgPercent = sub.value("ZDF").toDouble();
            list.append(data);

        }

    }

    return list.size() > 0;
}

void QHKExchangeVolDataProcess::getVolofDate(ShareForignVolFileDataList &list, const QDate &date)
{
    //从文件获取
    if(getVolInfoFromFile(list, date)) return;
    //先从东方财富获取
    if(!getVolInfoFromEastMoney(list, date))
    {
        //从港交所获取
        int errorNUm = 0;
        int sh_num = 0, sz_num = 0;
        do {
            getVolInfoFromHKEX(list, sh_num, date, 0);
            getVolInfoFromHKEX(list, sz_num, date, 1);
            errorNUm++;
            if(errorNUm == 3)
            {
                break;
            }
        } while (sh_num == 0 && sz_num == 0);

        if((sh_num == 0) ^ (sz_num == 0))
        {
            while (sh_num == 0) {
                getVolInfoFromHKEX(list, sh_num, date, 0);
            }
            while (sz_num == 0) {
                getVolInfoFromHKEX(list, sz_num, date, 1);
            }
        }
    }

    qDebug()<<"hk:"<<date.toString("yyyyMMdd")<<list.size();
    if(list.size() > 0)
    {
        saveData(date, list);
    }
}

void QHKExchangeVolDataProcess::run()
{
    ShareForignVolFileDataList list;
#if 0
    QDate start = mStartDate;
    while(start <= mEndDate)
    {
        //检查是不是市场开市时间
        if(ShareDate::getHisWorkingDay().contains(start))
        {
            getVolofDate(list, start);
        }
        //getVolofDateFromEastMoney(list, start);
        start = start.addDays(1);
    }
#endif
    getVolofDate(list, mDate);
    if(mParent)
    {
        QMetaObject::invokeMethod(mParent,\
                                  "slotUpdateForignVolInfo",\
                                  Qt::DirectConnection,\
                                  Q_ARG(ShareForignVolFileDataList, list),\
                                  Q_ARG(QDate, mDate)\
                                  );
    }
}

QString QHKExchangeVolDataProcess::getFileName(const QDate &date)
{
    //设定保存的文件名
    return QString("%1/%2")
            .arg(HQ_LGT_HISTORY_DIR)
            .arg(date.toString("yyyyMMdd"));
}

bool QHKExchangeVolDataProcess::saveData(const QDate &date, const ShareForignVolFileDataList &list)
{
    //检查保存目录是否存在，如果不存在，则创建目录
    QDir dir(HQ_LGT_HISTORY_DIR);
    if(!dir.exists()){
        if(!dir.mkpath(HQ_LGT_HISTORY_DIR))
        {
            qDebug()<<"create save file dir failed.dir:"<<dir.path();
            return false;
        }
    }
    //设定保存的文件名
    QString fileName = getFileName(date);
    //写文件
    QFile file(fileName);
    if(file.open(QIODevice::WriteOnly))
    {
        for(int i=0; i<list.length(); i++)
        {
            file.write((char*)(&(list[i])), sizeof(ShareForignVolFileData));
        }
        file.close();
    } else {
        qDebug()<<"Write file failed with date:"<<date.toString("yyyyMMdd");
        return false;
    }

    qDebug()<<"Write file success with date:"<<date.toString("yyyyMMdd");
    return true;
}
