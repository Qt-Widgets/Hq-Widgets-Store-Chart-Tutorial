﻿#include "hqdbdatabase.h"
#include <QMutexLocker>
#include <QDebug>

#define     QDebug()            qDebug()<<__FUNCTION__<<__LINE__
#define     HISTORY_TABLE(code) HQ_SHARE_HISTORY_INFO_TABLE + code.right(6)


HQDBDataBase::HQDBDataBase(QObject *parent) : QObject(parent)
{
    mInitDBFlg = initSqlDB();
    if(mInitDBFlg)
    {
        mSQLQuery = QSqlQuery(mDB);
    }
    qDebug()<<"db init:"<<mInitDBFlg;
}

HQDBDataBase::~HQDBDataBase()
{
    if(mInitDBFlg)
    {
        mDB.close();
    }
}

bool HQDBDataBase::isDBOK()
{
    return mInitDBFlg;
}

bool HQDBDataBase::initSqlDB()
{
    //初始化本地数据库的连接
    qDebug()<<"database:"<<QSqlDatabase::database().databaseName();
    if(QSqlDatabase::contains("qt_sql_default_connection"))
    {
        mDB = QSqlDatabase::database("qt_sql_default_connection");
    }
    else
    {
        mDB = QSqlDatabase::addDatabase("QSQLITE");
    }
    mDB.setDatabaseName("db.dat");
    return mDB.open();
}

QString HQDBDataBase::getErrorString()
{
    QMutexLocker locker(&mSQlMutex);
    return QString("Error Content: \n %1 \n %2").arg(mSQLQuery.lastQuery()).arg(mSQLQuery.lastError().text());
}

bool HQDBDataBase::isTableExist(const QString &pTable)
{
    QMutexLocker locker(&mSQlMutex);
    if(!mSQLQuery.exec(tr("SELECT COUNT(*) FROM sqlite_master where type='table' and name='%1'").arg(pTable))) return false;
    while (mSQLQuery.next()) {
        return mSQLQuery.value(0).toBool();
    }
    return false;
}

bool HQDBDataBase::createTable(const QString &pTable, const TableColList& cols)
{
    QStringList colist;
    foreach (TABLE_COL_DEF data, cols) {
        colist.append(tr(" [%1] %2 ").arg(data.mKey).arg(data.mDef));
    }

    QString sql = tr("CREATE TABLE [%1] ( %2 )").arg(pTable).arg(colist.join(","));
    QMutexLocker locker(&mSQlMutex);
    return mSQLQuery.exec(sql);
}

bool HQDBDataBase::getBlockDataList(QMap<QString, BlockData*>& pBlockMap, int type)
{
    QMutexLocker locker(&mSQlMutex);
    QString filter = (type != 0 ? tr(" where %1 = %2").arg(HQ_TABLE_COL_BLOCK_TYPE).arg(type) : "");
    if(!mSQLQuery.exec(tr("select * from %1 %2").arg(HQ_BLOCK_TABLE).arg(filter))) return false;
    while (mSQLQuery.next()) {
        QString code = mSQLQuery.value(HQ_TABLE_COL_CODE).toString();
        BlockData *info = pBlockMap.value(code, 0);
        if(!info) info = new BlockData;
        info->mCode = mSQLQuery.value(HQ_TABLE_COL_CODE).toString();
        info->mName = mSQLQuery.value(HQ_TABLE_COL_NAME).toString();
        info->mChangePer = mSQLQuery.value(HQ_TABLE_COL_CHANGE_PERCENT).toDouble();
        info->mBlockType = mSQLQuery.value(HQ_TABLE_COL_BLOCK_TYPE).toInt();
        info->mIsFav = mSQLQuery.value(HQ_TABLE_COL_FAVORITE).toBool();
        info->mShareCodeList = mSQLQuery.value(HQ_TABLE_COL_SHARE_LIST).toStringList();
        info->mDate = mSQLQuery.value(HQ_TABLE_COL_DATE).toDate();
        pBlockMap[info->mCode] = info;
    }
    return true;
}

bool HQDBDataBase::saveBlockDataList(const QMap<QString, BlockData *>& pBlockMap)
{
    QSqlDatabase::database().transaction();
    foreach (QString key, pBlockMap.keys()) {
        bool sts = false;
        if(isBlockExist(key))
        {
            //更新
            sts = modifyBlock(*(pBlockMap.value(key)));
        } else
        {
            //添加
            sts = addBlock(*(pBlockMap.value(key)));
        }
        if(!sts)
        {
            QSqlDatabase::database().rollback();
            return false;
        }
    }
    QSqlDatabase::database().commit();
    return true;
}

bool HQDBDataBase::isBlockExist(const QString &code)
{
    QMutexLocker locker(&mSQlMutex);
    //先检查板块名是否存在
    if(!mSQLQuery.exec(tr("select 1 from %1 where %2 = '%3'").arg(HQ_BLOCK_TABLE).arg(HQ_TABLE_COL_CODE).arg(code))) return false;
    while (mSQLQuery.next()) {
        return true;
    }
    return false;
}

bool HQDBDataBase::addBlock(const BlockData &info)
{
    QMutexLocker locker(&mSQlMutex);
    mSQLQuery.prepare(QString("insert into %1 values ("
                      "?, ?, ?, ?, ?, ?, ?)").arg(HQ_BLOCK_TABLE));
    mSQLQuery.addBindValue(info.mCode);
    mSQLQuery.addBindValue(info.mName);
    mSQLQuery.addBindValue(info.mClose);
    mSQLQuery.addBindValue(info.mChangePer);
    mSQLQuery.addBindValue(info.mShareCodeList.join(","));
    mSQLQuery.addBindValue(info.mBlockType);
    mSQLQuery.addBindValue(info.mDate);
    return mSQLQuery.exec();
}

bool HQDBDataBase::modifyBlock(const BlockData &info)
{
    QMutexLocker locker(&mSQlMutex);
    mSQLQuery.prepare(QString("update %1 set "
                      " %2 = ?, %3 = ?, %4 = ?, %5 = ? "
                      " where %6 = ? ")
                      .arg(HQ_BLOCK_TABLE)
                      .arg(HQ_TABLE_COL_CLOSE)
                      .arg(HQ_TABLE_COL_CHANGE_PERCENT)
                      .arg(HQ_TABLE_COL_SHARE_LIST)
                      .arg(HQ_TABLE_COL_DATE)
                      .arg(HQ_TABLE_COL_CODE));
    mSQLQuery.addBindValue(info.mClose);
    mSQLQuery.addBindValue(info.mChangePer);
    mSQLQuery.addBindValue(info.mShareCodeList.join(","));
    mSQLQuery.addBindValue(info.mDate);
    mSQLQuery.addBindValue(info.mCode);

    return mSQLQuery.exec();
}

bool HQDBDataBase::deleteBlock(const QString& code)
{
    return mSQLQuery.exec(tr("delete from %1 where %2 = %1").arg(HQ_BLOCK_TABLE).arg(HQ_TABLE_COL_CODE).arg(code));
}

bool HQDBDataBase::createDBTables()
{
    if(!createBlockTable()) return false;
    if(!createShareBasicTable()) return false;
    if(createGeneralTable()) return false;
    return true;
}

bool HQDBDataBase::createBlockTable()
{
    if(isTableExist(HQ_BLOCK_TABLE)) return true;
    TableColList colist;
    colist.append({HQ_TABLE_COL_ID, "INTEGER  PRIMARY KEY AUTOINCREMENT NOT NULL"});
    colist.append({HQ_TABLE_COL_CODE, "VARCHAR(6) NOT NULL"});
    colist.append({HQ_TABLE_COL_NAME, "VARCHAR(100) NOT NULL"});
    colist.append({HQ_TABLE_COL_CLOSE, "NUMERIC NULL"});
    colist.append({HQ_TABLE_COL_CHANGE_PERCENT, "NUMERIC NULL"});
    colist.append({HQ_TABLE_COL_SHARE_LIST, "VARCHAR(10000) NULL"});
    colist.append({HQ_TABLE_COL_BLOCK_TYPE, "INTEGER NULL"});
    colist.append({HQ_TABLE_COL_DATE, "DATE NULL"});
    return createTable(HQ_BLOCK_TABLE, colist);
}

bool HQDBDataBase::createShareBasicTable()
{
    if(isTableExist(HQ_SHARE_BASIC_INFO_TABLE)) return true;
    TableColList colist;
    colist.append({HQ_TABLE_COL_ID, "INTEGER  PRIMARY KEY AUTOINCREMENT NOT NULL"});
    colist.append({HQ_TABLE_COL_CODE, "VARCHAR(6) NOT NULL"});
    colist.append({HQ_TABLE_COL_NAME, "VARCHAR(100) NOT NULL"});
    colist.append({HQ_TABLE_COL_PY_ABBR, "VARCHAR(10) NOT NULL"});
    colist.append({HQ_TABLE_COL_FAVORITE, "BOOL NULL"});
    colist.append({HQ_TABLE_COL_HSGT_TOP10, "BOOL NULL"});
    colist.append({HQ_TABLE_COL_TOTALMNT, "NUMERIC NULL"});
    colist.append({HQ_TABLE_COL_MUTAL, "NUMERIC NULL"});
    colist.append({HQ_TABLE_COL_PROFIT, "NUMERIC NULL"});
    colist.append({HQ_TABLE_COL_BLOCK_LIST, "VARCHAR(10000) NULL"});

    return createTable(HQ_SHARE_BASIC_INFO_TABLE, colist);
}

bool HQDBDataBase::createGeneralTable()
{
    if(isTableExist(HQ_GENERAL_TABLE)) return true;
    TableColList colist;
    colist.append({HQ_TABLE_COL_ID, "INTEGER  PRIMARY KEY AUTOINCREMENT NOT NULL"});
    colist.append({HQ_TABLE_COL_TABLE_NM, "VARCHAR(100) NOT NULL"});
    colist.append({HQ_TABLE_COL_DATE, "DATE NULL"});
    return createTable(HQ_GENERAL_TABLE, colist);
}

bool HQDBDataBase::updateHistoryDataList(const StockDataList &list)
{
    if(!createStockHistoryInfoTable()) return false;
    QSqlDatabase::database().transaction();
    foreach (StockData data, list) {
        if(data.mMoney < 10000) continue;
        bool exist = false;
        QList<HQ_QUERY_CONDITION> conList;
        conList.append(HQ_QUERY_CONDITION(HQ_TABLE_COL_CODE, data.mCode));
        conList.append(HQ_QUERY_CONDITION(HQ_TABLE_COL_DATE, data.mDate));
        if(!isRecordExist(exist, HQ_SHARE_HISTORY_INFO_TABLE, conList))
        {
            QSqlDatabase::database().rollback();
            return false;
        }
        if(!updateHistoryShare(data, exist))
        {
            QSqlDatabase::database().rollback();
            return false;
        }
    }
    //更新日期
    if(!updateDateOfTable(HQ_SHARE_HISTORY_INFO_TABLE))
    {
        QSqlDatabase::database().rollback();
        return false;
    }
    QSqlDatabase::database().commit();
    return true;
}

bool HQDBDataBase::createStockHistoryInfoTable()
{
    if(isTableExist(HQ_SHARE_HISTORY_INFO_TABLE)) return true;
    TableColList colist;
    colist.append({HQ_TABLE_COL_ID, "INTEGER  PRIMARY KEY AUTOINCREMENT NOT NULL"});
    colist.append({HQ_TABLE_COL_CODE, "VARCHAR(6) NOT NULL"});
    colist.append({HQ_TABLE_COL_NAME, "VARCHAR(100) NOT NULL"});
    colist.append({HQ_TABLE_COL_CLOSE, "NUMERIC NULL"});
    colist.append({HQ_TABLE_COL_CHANGE_PERCENT, "NUMERIC NULL"});
    colist.append({HQ_TABLE_COL_VOL, "NUMERIC NULL"});
    colist.append({HQ_TABLE_COL_MONEY, "NUMERIC NULL"});
    colist.append({HQ_TABLE_COL_ZJLX, "NUMERIC NULL"});
    colist.append({HQ_TABLE_COL_RZRQ, "NUMERIC NULL"});
    colist.append({HQ_TABLE_COL_HSGT_HAVE, "NUMERIC NULL"});
    colist.append({HQ_TABLE_COL_TOTALMNT, "NUMERIC NULL"});
    colist.append({HQ_TABLE_COL_MUTAL, "NUMERIC NULL"});
    colist.append({HQ_TABLE_COL_DATE, "DATE NULL"});
    return createTable(HQ_SHARE_HISTORY_INFO_TABLE, colist);
}

QDate HQDBDataBase::getLastUpdateDateOfTable(const QString& table)
{
    createGeneralTable();
    QMutexLocker locker(&mSQlMutex);
    QDate date = QDate(2017,3,16);
    if(mSQLQuery.exec(tr("select %1 from %2 where %3 = %4")\
                      .arg(HQ_TABLE_COL_DATE)\
                      .arg(HQ_GENERAL_TABLE)\
                      .arg(HQ_TABLE_COL_TABLE_NM)\
                      .arg(table)))
    {
        while (mSQLQuery.next()) {
            if(!mSQLQuery.value(0).isNull())
            {
                date = mSQLQuery.value(0).toDate();
                break;
            }
        }
    }
    return date;
}

bool HQDBDataBase::updateDateOfTable(const QString &table)
{
    QDate date = QDate::currentDate();
    QList<HQ_QUERY_CONDITION> conList;
    conList.append({HQ_TABLE_COL_TABLE_NM, table});
    if(!deleteRecord(HQ_GENERAL_TABLE, conList)) return false;

    QMutexLocker locker(&mSQlMutex);
    mSQLQuery.prepare(tr("insert into %1 (%2,%3) values (?, ?)")\
                      .arg(HQ_GENERAL_TABLE)\
                      .arg(HQ_TABLE_COL_TABLE_NM)\
                      .arg(HQ_TABLE_COL_DATE));
    mSQLQuery.addBindValue(table);
    mSQLQuery.addBindValue(date);
    return mSQLQuery.exec();
}

bool HQDBDataBase::getBasicShareDataList(QMap<QString, StockData*>& pShareMap)
{
    QMutexLocker locker(&mSQlMutex);
    if(!mSQLQuery.exec(tr("select * from %1").arg(HQ_SHARE_BASIC_INFO_TABLE))) return false;
    while (mSQLQuery.next()) {
        StockData *info = new StockData;
        info->mCode = mSQLQuery.value(HQ_TABLE_COL_CODE).toString();
        info->mName = mSQLQuery.value(HQ_TABLE_COL_NAME).toString();
        info->mPY = mSQLQuery.value(HQ_TABLE_COL_PY_ABBR).toString();
        info->mIsFavCode = mSQLQuery.value(HQ_TABLE_COL_FAVORITE).toBool();
        info->mHSGTTop10 = mSQLQuery.value(HQ_TABLE_COL_HSGT_TOP10).toBool();
        info->mTotalShare = mSQLQuery.value(HQ_TABLE_COL_TOTALMNT).toLongLong();
        info->mMutableShare = mSQLQuery.value(HQ_TABLE_COL_MUTAL).toLongLong();
        info->mProfit = mSQLQuery.value(HQ_TABLE_COL_PROFIT).toDouble();
        info->mBlockCodeList = mSQLQuery.value(HQ_TABLE_COL_BLOCK_LIST).toStringList();
        pShareMap[info->mCode.right(6)] = info;
    }
    return true;
}

bool HQDBDataBase::updateBasicShareDataList(QList<StockData*> dataList)
{
    if(dataList.length() == 0) return false;
    QSqlDatabase::database().transaction();
    foreach (StockData* data, dataList) {
        //检查记录是否存在
        bool exist = false;
        QList<HQ_QUERY_CONDITION> conList;
        conList.append({HQ_TABLE_COL_CODE, data->mCode});
        if(!isRecordExist(exist, HQ_SHARE_BASIC_INFO_TABLE, conList))
        {
            QSqlDatabase::database().rollback();
            return false;
        }
        if(!updateBasicShare(*data, exist))
        {
            QSqlDatabase::database().rollback();
            return false;
        }
    }

    //更新日期
    if(!updateDateOfTable(HQ_SHARE_BASIC_INFO_TABLE))
    {
        QSqlDatabase::database().rollback();
        return false;
    }
    QSqlDatabase::database().commit();
    return true;
}

bool HQDBDataBase::updateBasicShare(const StockData& data, bool exist)
{
    QMutexLocker locker(&mSQlMutex);
    if(!exist)
    {
        mSQLQuery.prepare(QString("insert into %1 ("
                                  "%2,%3,%4,%5,%6,"
                                  "%7,%8,%9,%10)"
                                  " values ("
                                  "?, ?, ?, ?, ?, "
                                  "?, ?, ?, ?)")\
                          .arg(HQ_SHARE_BASIC_INFO_TABLE)
                          .arg(HQ_TABLE_COL_NAME)\
                          .arg(HQ_TABLE_COL_PY_ABBR)\
                          .arg(HQ_TABLE_COL_FAVORITE)\
                          .arg(HQ_TABLE_COL_HSGT_TOP10)\
                          .arg(HQ_TABLE_COL_TOTALMNT)\
                          .arg(HQ_TABLE_COL_MUTAL)\
                          .arg(HQ_TABLE_COL_PROFIT)\
                          .arg(HQ_TABLE_COL_BLOCK_LIST)\
                          .arg(HQ_TABLE_COL_CODE));
    } else
    {
        mSQLQuery.prepare(QString(" update %1 set "
                                  " %2 = ?,%3 = ?,%4 = ?,%5 = ?,%6 = ?, "
                                  " %7 = ?,%8 = ?,%9 = ? where %10 = ? ")
                          .arg(HQ_SHARE_BASIC_INFO_TABLE)\
                          .arg(HQ_TABLE_COL_NAME)\
                          .arg(HQ_TABLE_COL_PY_ABBR)\
                          .arg(HQ_TABLE_COL_FAVORITE)\
                          .arg(HQ_TABLE_COL_HSGT_TOP10)\
                          .arg(HQ_TABLE_COL_TOTALMNT)\
                          .arg(HQ_TABLE_COL_MUTAL)\
                          .arg(HQ_TABLE_COL_PROFIT)\
                          .arg(HQ_TABLE_COL_BLOCK_LIST)\
                          .arg(HQ_TABLE_COL_CODE));
    }
    mSQLQuery.addBindValue(data.mName);
    mSQLQuery.addBindValue(data.mPY);
    mSQLQuery.addBindValue(data.mIsFavCode);
    mSQLQuery.addBindValue(data.mHSGTTop10);
    mSQLQuery.addBindValue(data.mTotalShare);
    mSQLQuery.addBindValue(data.mMutableShare);
    mSQLQuery.addBindValue(data.mProfit);
    mSQLQuery.addBindValue(data.mBlockCodeList);
    mSQLQuery.addBindValue(data.mCode);
    return mSQLQuery.exec();
}

bool HQDBDataBase::updateHistoryShare(const StockData &info, bool exist)
{
    QMutexLocker locker(&mSQlMutex);
    if(!exist)
    {
        mSQLQuery.prepare(QString(" insert into %1 ( "
                                  " %2, %3, %4, %5, %6, "
                                  " %7, %8, %9, %10, %11, "
                                  " %12, %13) "
                                  " values ( "
                                  " ?, ?, ?, ?, ?, "
                                  " ?, ?, ?, ?, ?, "
                                  " ?, ?)")\
                          .arg(HQ_SHARE_HISTORY_INFO_TABLE)\
                          .arg(HQ_TABLE_COL_NAME)\
                          .arg(HQ_TABLE_COL_CLOSE)\
                          .arg(HQ_TABLE_COL_CHANGE_PERCENT)\
                          .arg(HQ_TABLE_COL_VOL)\
                          .arg(HQ_TABLE_COL_MONEY)\
                          .arg(HQ_TABLE_COL_ZJLX)\
                          .arg(HQ_TABLE_COL_RZRQ)\
                          .arg(HQ_TABLE_COL_HSGT_HAVE)\
                          .arg(HQ_TABLE_COL_TOTALMNT)\
                          .arg(HQ_TABLE_COL_MUTAL)\
                          .arg(HQ_TABLE_COL_CODE)\
                          .arg(HQ_TABLE_COL_DATE));
    } else
    {
        mSQLQuery.prepare(QString(" update %1 set "
                                  " %2=?, %3=?, %4=?, %5=?, %6=?, "
                                  " %7=?, %8=?, %9=?, %10=?, %11=?, "
                                  " where %12=? and %13=? ")\
                          .arg(HQ_SHARE_HISTORY_INFO_TABLE)\
                          .arg(HQ_TABLE_COL_NAME)\
                          .arg(HQ_TABLE_COL_CLOSE)\
                          .arg(HQ_TABLE_COL_CHANGE_PERCENT)\
                          .arg(HQ_TABLE_COL_VOL)\
                          .arg(HQ_TABLE_COL_MONEY)\
                          .arg(HQ_TABLE_COL_ZJLX)\
                          .arg(HQ_TABLE_COL_RZRQ)\
                          .arg(HQ_TABLE_COL_HSGT_HAVE)\
                          .arg(HQ_TABLE_COL_TOTALMNT)\
                          .arg(HQ_TABLE_COL_MUTAL)\
                          .arg(HQ_TABLE_COL_CODE)\
                          .arg(HQ_TABLE_COL_DATE));
    }

    mSQLQuery.addBindValue(info.mName);
    mSQLQuery.addBindValue(info.mClose);
    mSQLQuery.addBindValue(info.mChgPercent);
    mSQLQuery.addBindValue(info.mVol);
    mSQLQuery.addBindValue(info.mMoney);
    mSQLQuery.addBindValue(info.mZJLX);
    mSQLQuery.addBindValue(info.mRZRQ);
    mSQLQuery.addBindValue(info.mForeignVol);
    mSQLQuery.addBindValue(info.mTotalShare);
    mSQLQuery.addBindValue(info.mMutableShare);
    mSQLQuery.addBindValue(info.mCode);
    mSQLQuery.addBindValue(info.mDate);
    return mSQLQuery.exec();
}



bool HQDBDataBase::deleteShare(const QString& table, const QString& col, const QVariant& val)
{
    QMutexLocker locker(&mSQlMutex);
    if(col.length() == 0)
    {
        return mSQLQuery.exec(QString("delete from %1").arg(table));
    }
    mSQLQuery.prepare(QString("delete from %1 where %2 = ?").arg(table).arg(col));
    mSQLQuery.addBindValue(val);
    return mSQLQuery.exec();
}

bool HQDBDataBase::isRecordExist(bool& exist,const QString &table, const QList<HQ_QUERY_CONDITION>& list)
{
    exist = false;
    QMutexLocker locker(&mSQlMutex);
    QStringList filterList;
    QVariantList valList;
    foreach (HQ_QUERY_CONDITION con, list) {
        filterList.append(QString("%1=?").arg(con.col));
        valList.append(con.val);
    }
    QString filter = filterList.join(" and ");
    mSQLQuery.prepare(QString("select count(1) from %1 where %2 ").arg(table).arg(filter));
    foreach (QVariant val, valList) {
        mSQLQuery.addBindValue(val);
    }
    if(!mSQLQuery.exec()) return false;
    while (mSQLQuery.next()) {
        exist = mSQLQuery.value(0).toBool();
        break;
    }
    return true;
}

bool HQDBDataBase::deleteRecord(const QString &table, const QList<HQ_QUERY_CONDITION> &list)
{
    QMutexLocker locker(&mSQlMutex);
    QStringList filterList;
    QVariantList valList;
    foreach (HQ_QUERY_CONDITION con, list) {
        filterList.append(QString("%1=?").arg(con.col));
        valList.append(con.val);
    }
    QString filter = "";
    if(filterList.length() > 0)
    {
        filter = " where " + filterList.join(" and ");
    }
    mSQLQuery.prepare(QString("delete from %1 %2").arg(table).arg(filter));
    foreach (QVariant val, valList) {
        mSQLQuery.addBindValue(val);
    }
    if(!mSQLQuery.exec()) return false;
    return true;
}

double HQDBDataBase::getMultiDaysChangePercent(const QString &code, HISTORY_CHANGEPERCENT type)
{
    double change = 0.0;
    QMutexLocker locker(&mSQlMutex);
    QString table = HQ_SHARE_HISTORY_INFO_TABLE;
    QString col = HQ_TABLE_COL_CHANGE_PERCENT;
    QString con = HQ_TABLE_COL_CODE;
    QString conVal = code;
    if(!mSQLQuery.exec(tr("select 1+%1 * 0.01 from %2 where %3=%4 order by date desc limit %5").arg(col).arg(table).arg(con).arg(conVal).arg(type)))
    {
        qDebug()<<errMsg();
        return change;
    }
    change = 1.0;
    while (mSQLQuery.next()) {
        change *= mSQLQuery.value(0).toDouble();
    }
    change = (change -1) * 100;
    return change;
}

double HQDBDataBase::getLastMoney(const QString &code)
{
    double change = 0.0;
    QMutexLocker locker(&mSQlMutex);
    QString table = HQ_SHARE_HISTORY_INFO_TABLE;
    QString col = HQ_TABLE_COL_MONEY;
    QString con = HQ_TABLE_COL_CODE;
    QString conVal = code;
    if(!mSQLQuery.exec(tr("select %1 from %2 where %3=%4 order by date desc limit 1").arg(col).arg(table).arg(con).arg(conVal)))
    {
        qDebug()<<errMsg();
        return change;
    }
    change = 1.0;
    while (mSQLQuery.next()) {
        change = mSQLQuery.value(0).toDouble() /10000.0;
        break;
    }
    return change;
}

bool HQDBDataBase::getLastForeignVol(qint64 &vol, qint64 &vol_chg, const QString &code)
{
    QMutexLocker locker(&mSQlMutex);
    QString table = HQ_SHARE_HISTORY_INFO_TABLE;
    QString col = HQ_TABLE_COL_HSGT_HAVE;
    QString con = HQ_TABLE_COL_CODE;
    QString conVal = code;
    if(!mSQLQuery.exec(tr("select %1 from %2 where %3=%4 order by date desc limit 2").arg(col).arg(table).arg(con).arg(conVal)))
    {
        qDebug()<<errMsg();
        return false;
    }
    QList<qint64> list;
    while (mSQLQuery.next()) {
        list.append(mSQLQuery.value(0).toLongLong());
    }
    if(list.length() == 0) return false;
    vol = list[0];
    vol_chg = 0;
    if(list.length() > 1)
    {
        vol_chg = list[0] - list[1];
    }

    return true;
}

bool HQDBDataBase::getHistoryDataOfCode(StockDataList& list, const QString &code)
{
    list.clear();
    QMutexLocker locker(&mSQlMutex);
    QString table = HQ_SHARE_HISTORY_INFO_TABLE;
    QString con = HQ_TABLE_COL_CODE;
    QString conVal = code;
    if(!mSQLQuery.exec(tr("select * from %1 where %3=%4 order by date desc limit 1000").arg(table).arg(con).arg(conVal)))
    {
        qDebug()<<errMsg();
        return false;
    }
    while (mSQLQuery.next()) {
        StockData data;
        data.mCode = mSQLQuery.value(HQ_TABLE_COL_CODE).toString();
        data.mName = mSQLQuery.value(HQ_TABLE_COL_NAME).toString();
        data.mClose = mSQLQuery.value(HQ_TABLE_COL_CLOSE).toDouble();
        data.mChgPercent = mSQLQuery.value(HQ_TABLE_COL_CHANGE_PERCENT).toDouble();
        data.mForeignVol = mSQLQuery.value(HQ_TABLE_COL_HSGT_HAVE).toLongLong();
        data.mForeignCap = data.mClose * data.mForeignVol;
        data.mDate = mSQLQuery.value(HQ_TABLE_COL_DATE).toDate();
        if(list.size() > 0)
        {
            StockData& last = list[list.size() -1];
            last.mForeignVolChg = last.mForeignVol - data.mForeignVol;
        }
        list.append(data);

    }
    if(list.length() == 0) return false;
    return true;
}

QString HQDBDataBase::errMsg()
{
    return QString("sql:%1\nerr:%2").arg(mSQLQuery.lastQuery()).arg(mSQLQuery.lastError().text());
}



