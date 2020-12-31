﻿#ifndef HQMERGETABLEWIDGET_H
#define HQMERGETABLEWIDGET_H

#include <QTableWidget>
#include <QMenu>
#include "utils/comdatadefines.h"
#include <QResizeEvent>
#include <QGestureEvent>
#include <QDebug>
#include "HqTableWidget.h"

//enum OPT_MOVE_MODE{
//    OPT_LEFT = 0,
//    OPT_RIGHT,
//    OPT_UP,
//    OPT_DOWN,
//};

#define         HQ_NO_GESTURE

class HqSingleTableWidget : public QTableWidget
{
    Q_OBJECT
public:
    explicit HqSingleTableWidget(QWidget *parent = 0);
    bool isColVisible(int i) const {return !isColumnHidden(i);}
    bool isRowVisible(int i) const {return !isRowHidden(i);}
    int  getRowHeight() const {return mRowHeight;}

    int  getTotalColWidth() const;
    int  getColWidth() const {return mColWidth;}
    void setHeaders(const TableColDataList& list);
    void appendRow();
    void setCodeName(int row, int column,const QString& code,const QString& name);
    void setItemText(int row, int column, const QString& text, const QColor& color = Qt::white, Qt::AlignmentFlag flg = Qt::AlignCenter);
    void updateFavShareIconOfRow(int row, bool isFav);
    void prepareUpdateTable(int newRowCount);
    void removeRows(int start, int count);
    void updateColumn(int col);
    virtual void setSortType(int type) {}
    virtual void displayNextPage() {}
    virtual void displayPreviousPage() {}
    virtual void displayFirstPage(){}
    virtual void displayLastPage(){}
public slots:
    virtual void updateTable() {}
private:
protected:
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
private slots:

signals:
    void signalDisplayPage(int val);
    void signalSetSortType(int type);
    void signalSetSortRule(int rule);
    void signalPageSizeChanged(int pagesize);

public slots:
    virtual void slotCellDoubleClicked(int row, int col);
    virtual void slotCellClicked(int row, int col);
    void slotHeaderClicked(int col);

private:
    TableColDataList        mColDataList;
    int                     mColWidth;
    int                     mRowHeight;
    QList<int>              mColWidthList;
protected:

};

class HqMergeTableWidget : public QWidget
{
    Q_OBJECT
public:
    explicit HqMergeTableWidget(QWidget *parent = 0);
    ~HqMergeTableWidget();
    virtual void setHeaders(const TableColDataList &list, int move_count = 0);
    virtual void updateTable() {}
    void    resetDisplayRows();
public:
    void prepareUpdateTable(int newRowCount);
    void removeRows(int start, int count);

public slots:
    virtual void setSortType(int type){}

protected:
    void resizeEvent(QResizeEvent*);
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void keyPressEvent(QKeyEvent *event);

protected:
    HqSingleTableWidget*                mFixTable;
    HqSingleTableWidget*                mMoveTable;
    int                                 mMovColCount;
    int                                 mDisplayRowStart;
    int                                 mMaxDisRow;
private:
    QPoint                              mPressPnt;
    QPoint                              mMovePnt;
    int                                 mMoveDir;
    qint64                              mLastMoveTime;
    int                                 mRowHeight;
};

#endif // HQMERGETABLEWIDGET_H
