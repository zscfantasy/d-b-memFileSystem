#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStandardItemModel>
#include <QTableWidget>
//ddbm_lib
#include "DiskIO.h"
#include "Ddbm_Corelib.h"
#include "Ddbm_Partition.h"   //这个Include家的迫不得已，后续写了封装接口函数会考虑删除
//render
#include "renderarea.h"
//pop dialog
#include "devinfodialog.h"

namespace Ui {
class MainWindow;
}



class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

public slots:
    void recvDevInfoDialogData();


private slots:
    bool insertDev();
    bool removeDev();
    bool formatDev();
    bool importFile();
    void parmSet();
    void detailStatus();
    void curDevSet();
    void clicked_rightMenu(QPoint pos);
    bool exportFile();
    bool deleteFile();

private:
    Ui::MainWindow *ui;
    devInfoDialog* mydevinfodialog;

    int curVFileTotalSize;
    QStandardItemModel *model;
    QString curDevName; 
    QStringList devList;
    BLK_DEV_INFO dev_info;
    //tableView右键菜单
    QMenu *rightMenu;  //tableView右键菜单
    QAction *exportFileAct;  //文件导出
    QAction *deleteFileAct;  //文件删除
    //列表显示，两个方块显示区域
    QTableView *tableView;
    RenderArea *DbiShowArea;
    RenderArea *BbmShowArea;
    //一些基本函数
    void basicSet();
    void renderAreaSet();
    void updateTableView();

};

#endif // MAINWINDOW_H
