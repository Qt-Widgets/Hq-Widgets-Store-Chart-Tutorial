﻿#ifndef QSHAREHSGTTOP10WORK_H
#define QSHAREHSGTTOP10WORK_H

#include <QThread>
#include "data_structure/sharedata.h"

class QShareHsgtTop10Work :  public QThread
{
    Q_OBJECT
public:
    explicit QShareHsgtTop10Work( QObject *parent = 0);
    ~QShareHsgtTop10Work();
    void run();
private:
    bool getDataFromEastMoney(ShareDataList& list, const ShareWorkingDate& date);
    bool getDataFromHKEX(ShareDataList& list, const ShareWorkingDate& date);
signals:
    void signalChinaAShareTop10Updated(const ShareDataList& list, const QString& date);

private:
};

#endif // QSHAREHSGTTOP10WORK_H
