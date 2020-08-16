﻿#include "qetfscalethread.h"
#include "utils/qhttpget.h"
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>
#include "utils/profiles.h"
#include "utils/comdatadefines.h"
#include <QDir>


#define     PROFILES_SEC            "ETF"
#define     PROFILES_KEY            "UpdateDate"

QEtfScaleThread::QEtfScaleThread(QObject *parent) : QThread(parent)
{
    qRegisterMetaType<QList<EtfScaleData>>("const QList<EtfScaleData>&");
    PROFILES_INS->setDefault(PROFILES_SEC, PROFILES_KEY, "2019-12-31");
    QDir dir(HQ_ETF_DIR);
    if(!dir.exists()) dir.mkpath(HQ_ETF_DIR);
}

QEtfScaleThread::~QEtfScaleThread()
{
    qDebug()<<__FUNCTION__;
}


void QEtfScaleThread::run()
{
    QDate last_update = QDate::fromString(PROFILES_INS->value(PROFILES_SEC, PROFILES_KEY).toString(), "yyyy-MM-dd").addDays(1);
    while (last_update < QDate::currentDate()) {
        mEtfVolMap.clear();
        parseShEtf(last_update);
        parseSzEtf(last_update);
        if(mEtfVolMap.size() > 0)
        {
            //write fo file
            QString fileName = QString("%1/%2.txt").arg(HQ_ETF_DIR).arg(last_update.toString("yyyyMMdd"));
            QFile file(fileName);
            if(file.open(QIODevice::WriteOnly | QIODevice::Text))
            {
                foreach (EtfScaleData data, mEtfVolMap) {
                    QStringList list;
                    list.append(data.mCode);
                    list.append(data.mName);
                    list.append(data.mDate.toString("yyyy-MM-dd"));
                    list.append(QString::number(data.mScale, 'f', 2));
                    list.append("\n");
                    file.write(list.join(",").toUtf8());
                }
                file.close();
                PROFILES_INS->setValue(PROFILES_SEC, PROFILES_KEY, last_update.toString("yyyy-MM-dd"));
            }

        }
        last_update = last_update.addDays(1);
    }
}

void QEtfScaleThread::parseShEtf(const QDate& date)
{
    QList<QNetworkCookie> list;
    list.append(QNetworkCookie("Referer", "http://www.sse.com.cn/market/funddata/volumn/etfvolumn/"));
    list.append(QNetworkCookie("Cookie", "yfx_c_g_u_id_10000042=_ck20072115125217721926968350309; yfx_f_l_v_t_10000042=f_t_1595315572764__r_t_1595579117057__v_t_1595579117057__r_c_1; VISITED_MENU=%5B%228864%22%2C%228528%22%2C%2210025%22%2C%228547%22%2C%228491%22%5D"));

    QByteArray recv = QHttpGet::getContentOfURL(QString("http://query.sse.com.cn/commonQuery.do?&jsonCallBack=&isPagination=true&sqlId=COMMON_SSE_ZQPZ_ETFZL_XXPL_ETFGM_SEARCH_L&pageHelp.pageSize=10000&STAT_DATE=%1&_=%2")
                                                .arg(date.toString("yyyy-MM-dd")).arg(QDateTime::currentDateTime().toMSecsSinceEpoch()), list);
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(recv, &error);
    if(!doc.isObject()) return;
    QJsonArray dataArray = doc.object().value("pageHelp").toObject().value("data").toArray();
    foreach (QJsonValue val, dataArray) {
        QJsonObject obj = val.toObject();
        EtfScaleData data;
        data.mCode = obj.value("SEC_CODE").toString();
        data.mName = obj.value("SEC_NAME").toString();
        data.mScale = obj.value("TOT_VOL").toString().toDouble();
        data.mDate = date;
        mEtfVolMap[data.mName] = data;
    }
}

void QEtfScaleThread::parseSzEtf(const QDate& date)
{
    int page_num = 1;
    int page_size = 10000;
    int page_count = 1;
    int total_record_count = 0;
    while (page_num <= page_count) {
        QString url = QString("http://www.szse.cn/api/report/ShowReport/data?SHOWTYPE=JSON&CATALOGID=1945&tab1PAGENO=%1&tab1PAGESIZE=%2&random=0.48925979447422674")
                .arg(page_num)
                .arg(page_size);
        QByteArray recv = QHttpGet::getContentOfURL(url);
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(recv, &error);
        if(!doc.isArray()) return;
        QJsonArray array = doc.array();
        QJsonObject metaObj = array[0].toObject().value("metadata").toObject();
        if(metaObj.isEmpty()) break;
        QDate curDate = QDate::fromString(metaObj.value("subname").toString(), "yyyy-MM-dd");
        if(!curDate.isValid()) break;
        if(curDate != date) break;
        page_count = metaObj.value("pagecount").toInt();
        if(page_count <= 0) break;
        total_record_count = metaObj.value("recordcount").toInt();

        QJsonArray dataArray = array[0].toObject().value("data").toArray();
        QRegExp codeExp("<u>(\\d{6})</u>");
        QRegExp nameEXp("<u>(.{1,})</u>");
        foreach (QJsonValue val, dataArray) {
            QJsonObject obj = val.toObject();
            QString code = obj.value("sys_key").toString();
            int index = codeExp.indexIn(code);
            if(index > 0)
            {
                code = codeExp.cap(1);
            }
            QString name = obj.value("kzjcurl").toString();
            index = nameEXp.indexIn(name);
            if(index > 0)
            {
                name = nameEXp.cap(1);
            }
            double vol = obj.value("dqgm").toString().remove(",").toDouble();
            EtfScaleData data;
            data.mCode = code;
            data.mName = name;
            data.mScale = vol;
            data.mDate = date;
            mEtfVolMap[data.mName] = data;
        }
        page_num++;
    }
}
