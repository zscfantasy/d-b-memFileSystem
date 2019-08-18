#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "renderarea.h"

#include <stdio.h>
#include <stdlib.h>
#include <QtWidgets>
#include <QDebug>
#include <QString>
#include <QModelIndex>

/*QString统一用toUtf8.data()转，这样在本机（macOS默认系统字符集是unicode）才支持中文*/
/*
    * 在windows下，QString转char*，使用toLocal8Bit().data()就能获得，当然前提你要是QString中是正确的中文，即
    str="我是中国人";
    qDebug()<<str;   //能输出"我是中国人"。
    如果是在linux下，就跟系统有关系了。有区别的，而且如果是qt4，linux下中文还需要翻译器。
    QString的中文变量声明也是有区别的。
    linux下TextCodec是GBK的，而windows下是GB18030的。所以你可以在main.cpp中定义

    QTextCodec::setCodecForLocale(QTextCodec::codecForName("GBK"));
    QTextCodec::setCodecForCStrings(QTextCodec::codecForName("GBK"));
    QTextCodec::setCodecForTr(QTextCodec::codecForName("GBK"));

或是
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("GB18030"));
    QTextCodec::setCodecForCStrings(QTextCodec::codecForName("GB18030"));
    QTextCodec::setCodecForTr(QTextCodec::codecForName("GB18030"));

    然后在使用QString中文是，采用QString::fromLocal8Bit("我是中国人").或tr("我是中国人")来进行变量声明。这样才能正确显示在窗体控件上，而不是乱码。
*/

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    //手动配置几个菜单的槽
    connect(ui->insertDevAct, SIGNAL(triggered()), this, SLOT(insertDev()));
    connect(ui->formatDevAct, SIGNAL(triggered()), this, SLOT(formatDev()));
    connect(ui->importFileAct, SIGNAL(triggered()), this, SLOT(importFile()));
    connect(ui->parmSetAct, SIGNAL(triggered()), this, SLOT(parmSet()));
    connect(ui->detailStatusAct, SIGNAL(triggered()), this, SLOT(detailStatus()));
    //设置菜单栏和tableView等基础控件
    basicSet();
    //设置显示DBI和BBM的绘图区域
    renderAreaSet();
    //更新tableView视图
    updateTableView();
    setWindowTitle(tr("DDBM文件系统"));
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::basicSet()
{

    //设置tableView及其model内容
    model = new QStandardItemModel();
    model->setColumnCount(5);
    model->setHeaderData(0,Qt::Horizontal,QString::fromLocal8Bit("文件名"));
    model->setHeaderData(1,Qt::Horizontal,QString::fromLocal8Bit("大小"));
    model->setHeaderData(2,Qt::Horizontal,QString::fromLocal8Bit("DBI编号"));
    model->setHeaderData(3,Qt::Horizontal,QString::fromLocal8Bit("起始BBM"));
    model->setHeaderData(4,Qt::Horizontal,QString::fromLocal8Bit("BBM数量"));

    tableView = new QTableView;
    tableView->setModel(model);
    tableView->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
    tableView->setColumnWidth(0,160);

    //设置右键菜单
    tableView->setContextMenuPolicy(Qt::CustomContextMenu);  //少这句，右键没有任何反应,只有这样才能使用信号
    tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    exportFileAct = new QAction(QStringLiteral("文件导出"),this);  //文件导出
    deleteFileAct = new QAction(QStringLiteral("文件删除"),this);  //文件删除
    rightMenu = new QMenu(tableView);    //配置该菜单为tableView的子控件
    rightMenu->addAction(exportFileAct);
    rightMenu->addAction(deleteFileAct);
    connect(tableView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(clicked_rightMenu(QPoint)));
    //配置两个右键菜单的事件和槽
    connect(exportFileAct, SIGNAL(triggered()), this, SLOT(exportFile()));
    connect(deleteFileAct, SIGNAL(triggered()), this, SLOT(deleteFile()));
}

void MainWindow::renderAreaSet()
{
    QWidget *widget = new QWidget();
    this->setCentralWidget(widget);
    DbiShowArea = new RenderArea;
    BbmShowArea = new RenderArea;
    DbiShowArea->mytype = RenderArea::ViewType(0);
    BbmShowArea->mytype = RenderArea::ViewType(1);

    QGridLayout *mainLayout = new QGridLayout;
    QLabel *dbilabel = new QLabel(QStringLiteral("DBI视图"),this);
    QLabel *bbmlabel = new QLabel(QStringLiteral("BBM视图"),this);
    mainLayout->setRowStretch(0, 3);
    mainLayout->setRowStretch(2, 1);
    mainLayout->setRowStretch(4, 5);
    mainLayout->addWidget(tableView, 0, 0);
    mainLayout->addWidget(dbilabel, 1, 0);
    mainLayout->addWidget(DbiShowArea, 2, 0);
    mainLayout->addWidget(bbmlabel, 3, 0);
    mainLayout->addWidget(BbmShowArea, 4, 0);
    centralWidget()->setLayout(mainLayout);


    //DbiShowArea->update();  第一次设置会自动绘制,不需要手动强制更新
    //BbmShowArea->update();
}

bool MainWindow::insertDev()
{
    uint32_ddbm index;
    QAction *delDevAct;
    QAction *curDevAct;
    QString fileName;
    QString devName;
    FILE *vFileDev_fp = NULL;
    int8_ddbm *c_devPath;
    int8_ddbm *c_devName;
    fileName = QFileDialog::getOpenFileName(this);
    if (fileName.isEmpty())
        return false;

    c_devName = fileName.toUtf8().data();   //qstring转char*
    c_devPath = fileName.toUtf8().data();   //这里的文件名是完整路径，不单单是文件名
    printf("[insertDev]virtual devName full path name = %s\n",c_devPath);
    //注意,QString有最小长度限制，如果文件名太短太长好像都不行，所以不用QT分割字符串的方式，待讨论
    //QString devName = fileName.split("/").last();
    //QString devName = fileName.right(fileName.length()-fileName.lastIndexOf("/")-1);
    for(index = strlen(c_devName)-1; index >= 0; index--){
        if(c_devName[index] == '/')
            break;
    }
    c_devName += index + 1;
    //devName = QString(QLatin1String(c_devName));   //中文不在QLatin1String字符集中，所以这种方式不支持中文
    devName = QString::fromLocal8Bit(c_devName);    //fromUtf8效果应该是一样的
    //如果存在同名设备，则报错返回
    QList<QString>::Iterator it = devList.begin(),itend = devList.end();
    for(;it != itend;it++){
        if(*it == devName)
        {
            QMessageBox::critical(this, QStringLiteral("提示"),QStringLiteral("已加载同名设备"),QMessageBox::Ok);
            return false;
        }
    }

    vFileDev_fp = fopen(c_devPath,"rb+");
    if(vFileDev_fp == NULL)
    {
        printf("要打开的虚拟设备文件路径错误！\n");
        return false;
    }
    fseek(vFileDev_fp,0,SEEK_END);
    curVFileTotalSize = ftell(vFileDev_fp);
    fseek(vFileDev_fp,0,SEEK_SET);

    //printf("virtual dev name = %s, %d bytes\n",c_devName,dev_info.disk_size);  //想要再qDebug输出，需要运行在release环境下。另外,tr()中只能是英文
    //qDebug<<QStringLiteral("虚拟设备容量是")<<dev_info.disk_size<<QStringLiteral("字节");

    //不管创建磁盘是否成功，都添加devList，以保证就算不符合条件的磁盘也能显示菜单并正常卸载
    curDevName = devName;
    devList.append(devName);
    //Ddbm_Disk_Create最后一个个参数传入文件句柄
    if(MEM_BLK_DEV* curDev = Ddbm_Disk_Create((uint8_ddbm*)c_devName,vFileDev_fp))
    {

        statusBar()->showMessage(QStringLiteral("虚拟设备")+devName+QStringLiteral("已加载"));
        //加载虚拟设备之后给dev_Info赋值
        memcpy(&dev_info, &(curDev->dev_info), sizeof(dev_info));
        dev_info.disk_total_size = curVFileTotalSize;//因为Ddbm_Disk_Create并没有在dev_info中返回磁盘总容量信息，所以需要额外赋值
        updateTableView();

    }
    else
    {
        statusBar()->showMessage(QStringLiteral("虚拟设备")+devName+QStringLiteral("加载失败"));
        QMessageBox::warning(this, QStringLiteral("警告"),QStringLiteral("可能是第一次使用改虚拟设备文件，需要格式化"),QMessageBox::Ok);

        /*加载失败，需要格式化，并选择磁盘基本信息配置进行格式化(这一块准备后续放到initFileSystem或者格式化磁盘中去，格式化里面一定会需要的)*/
        dev_info.disk_total_size = curVFileTotalSize;
        dev_info.disk_size = curVFileTotalSize;            /*默认格式化按照最大容量来格式化*/
        dev_info.boot_start_sector = BOOT_START_SECTOR;      /*boot扇区起始数量*/
        dev_info.sector_size = SECTOR_SIZE;			/*扇区大小*/
        dev_info.dbi_max_num = DBI_MAX_NUM;            /*DBI数量*/
        dev_info.dbi_size = DBI_SIZE;				/*DBI区域的大小*/
        dev_info.dbi_start_sector = DBI_START_SECTOR;		/*DBI从第128个扇区开始，也就是第2个64k*/
        dev_info.one_bbm_sector_num = ONE_BBM_SECTOR_NUM;		/*每个BBM占用的扇区大小*/
        dev_info.dbu_size = DBU_SIZE;				/*DBU大小64KB*/
        dev_info.one_dbu_sector_num = ONE_DBU_SECTOR_NUM;		/*每个DBU占用的扇区大小*/
        //有一个参数bbm_num在格式化磁盘中自动计算,但为了updateview中显示正常，这里也先按照格式化的方法计算一下
        /*根据磁盘的信息计算理论上的最大BBM的数量*/
        dev_info.bbm_num = (dev_info.disk_size/dev_info.sector_size - 2*(dev_info.dbi_start_sector+dev_info.dbi_max_num))/(dev_info.one_dbu_sector_num+2*dev_info.one_bbm_sector_num);
        /*如果bbm总数不是2的倍数，则减少一个*/
        if(dev_info.bbm_num%2) dev_info.bbm_num--;

        //开始确认格式化（直接调用槽函数）
        if(formatDev())
        {
            QMessageBox::information(this, QStringLiteral("提示"),QStringLiteral("新磁盘按照默认推荐设置格式化完成"),QMessageBox::Ok);
            updateTableView();
            printf("磁盘%s第一次格式化成功！",c_devName);
        }
        else
        {
            QMessageBox::critical(this, QStringLiteral("提示"),QStringLiteral("新磁盘格式化失败"),QMessageBox::Ok);
            devList.removeOne(devName);  //因为本系统不允许重名设备，所以这个方法是有效的
            if(!devList.isEmpty())
            {
                curDevName = devList[0];  //中括号对不对？
                updateTableView();
            }
            else   /*如果没有任何文件，则需要做相应的处理！*/
            {
                curDevName.clear();
                model->removeRows(0,model->rowCount());

                DbiShowArea->rectSum = 0;
                BbmShowArea->rectSum = 0;
                DbiShowArea->lsFileViewInfo.clear();
                BbmShowArea->lsFileViewInfo.clear();

                DbiShowArea->repaint();  //repaint和update不同，每次只要有事件就会重绘
                BbmShowArea->repaint();
            }

            return false;
        }


    }

    //处理删除action
    delDevAct = new QAction(devName, this);
    //curDevAct->setObjectName(devName);
    delDevAct->setStatusTip(QStringLiteral("虚拟设备")+devName);
    ui->menuRemove->addAction(delDevAct);
    connect(delDevAct,SIGNAL(triggered()), this, SLOT(removeDev()));
    //处理状态action
    curDevAct = new QAction(devName, this);
    curDevAct->setObjectName(devName);
    curDevAct->setStatusTip(QStringLiteral("选择当前虚拟设备为")+devName);
    ui->menuStatus->addAction(curDevAct);
    connect(curDevAct,SIGNAL(triggered()), this, SLOT(curDevSet()));  //connect和disconnect不需要成对使用，只要sender和receiver之一不存在,conect自动失效。                                                                    //所以不需要在控件的析构函数中调用disconnect


    return true;

}

bool MainWindow::removeDev()
{
    int8_ddbm* c_devName;
    QString devName;
    if(QAction *delDevAct = dynamic_cast<QAction*>(sender()))
    {
        devName = delDevAct->text(); //原来用的是delDevAct->objectName()，也可以，但是objectName是唯一的，放到status下用
        c_devName = devName.toUtf8().data();
        //printf("dev name is %s\n",c_devName);
        if(Ddbm_Disk_Unload((uint8_ddbm*)c_devName)==0)
        {    
            ui->menuRemove->removeAction(delDevAct);  //从开始菜单中删除这个Action
            /*还需要从当前菜单中中找到同名Action，并删除之*/
            //QAction *curDevActTmp = ui->menuStatus->findChild<QAction*>(devName);  //采用这种方法是错误的，因为Action没有设置成menuStatus的子控件
            QAction *curDevActTmp = this->findChild<QAction*>(devName);
            printf("found action, name is %s\n",curDevActTmp->text().toUtf8().data());
            ui->menuStatus->removeAction(curDevActTmp);  //从状态菜单中删除这个Action

            devList.removeOne(devName);  //因为本系统不允许重名设备，所以这个方法是有效的
            if(!devList.isEmpty())
            {
                curDevName = devList[0];  //中括号对不对？
                updateTableView();
            }
            else   /*如果没有任何文件，则需要做相应的处理！*/
            {
                curDevName.clear();
                model->removeRows(0,model->rowCount());

                DbiShowArea->rectSum = 0;
                BbmShowArea->rectSum = 0;
                DbiShowArea->lsFileViewInfo.clear();
                BbmShowArea->lsFileViewInfo.clear();

                DbiShowArea->repaint();  //repaint和update不同，每次只要有事件就会重绘
                BbmShowArea->repaint();
            }

            statusBar()->showMessage(QStringLiteral("虚拟设备")+devName+QStringLiteral("弹出成功"));
            printf("DDBM dev %s unload succeed\n",devName.toUtf8().data());

            return true;
        }
        else
        {
            statusBar()->showMessage(QStringLiteral("虚拟设备")+devName+QStringLiteral("弹出失败"));
            printf("DDBM dev %s unload failed\n",devName.toUtf8().data());
            return false;
        }

    }
    else{
        return false;
    }


}

bool MainWindow::formatDev()
{
    if(devList.isEmpty())
    {
        QMessageBox::critical(this, QStringLiteral("提示"),QStringLiteral("未加载任何设备，请先插入设备"),QMessageBox::Ok);
        return false;
    }

    QMessageBox::StandardButton result;
    QMessageBox::StandardButton defaultBtn = QMessageBox::NoButton;

    result = QMessageBox::question(this,QStringLiteral("格式化确认"),QStringLiteral("确认格式化?"),
                                   QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel,defaultBtn);

    if(result == QMessageBox::No || result == QMessageBox::Cancel)
    {
        return false;  //取消格式化
    }
    else if(result == QMessageBox::Yes)
    {

        //注意，格式化的时候会用到重要信息dev_info.disk_total_size这个值，在插入虚拟磁盘的时候赋值
        if(Ddbm_Format((uint8_ddbm*)(curDevName.toUtf8().data()), dev_info) == DISKIO_OK)
        {
            statusBar()->showMessage(QStringLiteral("虚拟设备")+curDevName+QStringLiteral("格式化成功"));
            printf("格式化磁盘成功\n");   //这个中文也能打印出来，不需要转换,待研究
            updateTableView();
            return true;
        }

        else
        {
            statusBar()->showMessage(QStringLiteral("虚拟设备")+curDevName+QStringLiteral("格式化失败"));
            printf("格式化磁盘失败\n");
            return false;
        }
    }
    else
        return false;

}
//设置当前的设备
void MainWindow::curDevSet()
{
    int8_ddbm* c_devName;
    QString devName;
    QAction *curDevAct = dynamic_cast<QAction*>(sender());
    //curDevAct->setChecked(true);

    devName = curDevAct->text(); //原来用的是delDevAct->objectName()
    c_devName = devName.toUtf8().data();
    statusBar()->showMessage(QStringLiteral("当前设备是")+devName);
    printf("cur dev name is %s\n",c_devName);
    curDevName = devName;
    /*接下来根据当前的dev名称，刷新listView和而二维显示区域的内容*/
    updateTableView();

}

bool MainWindow::importFile()
{
    STRUCT_FILE *fp = NULL;
    uint32_ddbm index;
    int32_ddbm re;
    int32_ddbm ui_FsCount;
    int8_ddbm *c_devName;
    int8_ddbm *c_filePath;
    int8_ddbm *c_fileName;
    uint64_ddbm win_fileSize;
    uint8_ddbm *buf;

    QString fileName;
    FILE *winFile_fp = NULL;

    if(devList.isEmpty())
    {
        QMessageBox::critical(this, QStringLiteral("提示"),QStringLiteral("未加载任何设备，请先插入设备"),QMessageBox::Ok);
        return false;
    }

    fileName = QFileDialog::getOpenFileName(this);
    if (fileName.isEmpty())
        return false;

    //c_fileName = fileName.toUtf8().data();
    c_filePath = c_fileName = fileName.toUtf8().data();   //这里的文件名是完整路径，不单单是文件名
    printf("[importFile]virtual fileName full path name = %s\n",c_filePath);

    for(index = strlen(c_fileName)-1; index >= 0; index--){
        if(c_fileName[index] == '/')
            break;
    }
    c_fileName += index + 1;
    //判断当前虚拟磁盘中是否存在同名文件，如果已经存在，则无法导入，弹出提示消息
    c_devName = curDevName.toUtf8().data();
    ui_FsCount = Ddbm_GetFileSystem_Index(c_devName);
    if(ui_FsCount>=FS_NUM || ui_FsCount<0)  return false;
    //pDev = getMemBlkDev((uint8_ddbm*)c_devName);
    if(Ddbm_Query_File(ui_FsCount, c_fileName) >= 0)
    {
        QMessageBox::warning(this, QStringLiteral("警告"),QStringLiteral("文件名重复"),QMessageBox::Cancel);
        return false;
    }

    winFile_fp = fopen(c_filePath,"rb+");
    if(winFile_fp == NULL)
    {
        printf("[importFile]要打开的文件路径错误！\n");
        return false;
    }
    fseek(winFile_fp,0,SEEK_END);
    win_fileSize = ftell(winFile_fp);  //获取待写入的文件的大小
    printf("[importFile]待写入的文件大小是 %llu 字节\n",win_fileSize);
    /*
    if(win_fileSize>dev_info.disk_size)
    {
        printf("[importFile]待写入的数据量太大 !\n");
        return false;
    }
    */
    buf = (uint8_ddbm *)malloc(win_fileSize);
    fseek(winFile_fp,0,SEEK_SET);
    fread(buf, win_fileSize, 1, winFile_fp);

    fp = (STRUCT_FILE *)Ddbm_Open_File(curDevName.toUtf8().data(),c_fileName, FILE_RDWRT);   //我保证如果直接传c_devName进来一定出问题
    if(fp != NULL)
    {
        printf("[importFile]文件%s在DDBM中打开(创建)成功\n",c_fileName);
    }
    else{
        QMessageBox::critical(this, QStringLiteral("错误"),QStringLiteral("文件导入DDBM失败！"),QMessageBox::Cancel);
        printf("[importFile]文件%s在DDBM中(创建)失败\n",c_fileName);
        fclose(winFile_fp);
        return false;
    }
    //向DDBM系统中写数据
    re = Ddbm_Write_File(buf, win_fileSize, 1, fp);
    if(re < 0)
    {
        printf("[importFile]向DDBM中写文件%s失败!\n",c_fileName);
        fclose(winFile_fp);
        return false;
    }
    fclose(winFile_fp);

    //Ddbm_Seek_File(fp, 0);
    //printf("分配到的首个BBM号是%d\n",fp->ui_BbmIdx);
    Ddbm_Close_File(fp);  //关闭文件

    updateTableView();
    return true;
}
//格式化参数设置
void MainWindow::parmSet()
{

    mydevinfodialog = new devInfoDialog(this);
    mydevinfodialog->setModal(true);
    mydevinfodialog->show();
    mydevinfodialog->getDevInfo(dev_info,curDevName);    //设置默认值

    connect(mydevinfodialog,SIGNAL(accepted()), this, SLOT(recvDevInfoDialogData()));


}

//详细信息
void MainWindow::detailStatus()
{
    QMessageBox::about(this, QStringLiteral("提示"),QStringLiteral("版本 1.0\nCopyright © 2003–2019 zsc\n保留一切权利。"));
}



void MainWindow::clicked_rightMenu(QPoint pos)
{
    QModelIndex index = tableView->indexAt(pos);
    if(index.isValid())
    {
        rightMenu->exec(QCursor::pos());  //效果，能在tableview有可选择行的情况下染出menu菜单
    }
}

bool MainWindow::exportFile()
{
    FILE *winFile_fp = NULL;
    STRUCT_FILE *fp = NULL;
    int32_ddbm re;
    int8_ddbm *c_fileName;
    int8_ddbm *c_filePath;
    uint64_ddbm win_fileSize = 0;
    uint8_ddbm *buf;
    QString fileName;
    QString filePath;

    int row = tableView->currentIndex().row();
    QModelIndex index = model->index(row,0);
    if(row<0)
    {
        QMessageBox::critical(this, QStringLiteral("提示"),QStringLiteral("没有任何选择"),QMessageBox::Ok);
        return false;
    }

    fileName = model->data(index).toString();
    if(fileName == NULL) return false;

    //c_fileName = fileName.toUtf8().data();    //用这种方法会出问题，只要在Ddbm_Open_File中c_fileName进行传参就会失败
    c_fileName = (int8_ddbm*)malloc(strlen(fileName.toUtf8().data())+1);
    strcpy(c_fileName,fileName.toUtf8().data());
    printf("导出之前，文件名为%s,strlen(c_fileName)=%lu\n",c_fileName,strlen(c_fileName));

    filePath = QFileDialog::getSaveFileName(this,QStringLiteral("文件导出"));
    if (filePath.isEmpty())
        return false;

    c_filePath = filePath.toUtf8().data();
    //从DDBM中读取内容
    fp = (STRUCT_FILE *)Ddbm_Open_File(curDevName.toUtf8().data(), c_fileName, FILE_RDONLY);
    if(fp != NULL)	printf("当前设备%s,文件%s在DDBM中打开(创建)成功,文件大小%llu\n",curDevName.toUtf8().data(),c_fileName,fp->ull_FileSize);
    else{
        printf("当前设备%s,文件%s在DDBM中打开(创建)失败\n",curDevName.toUtf8().data(),c_fileName);
        return false;
    }

    buf = (uint8_ddbm *)malloc(fp->ull_FileSize);

    re = Ddbm_Read_File(buf, win_fileSize, 1, fp);
    if(re < 0)
    {
        printf("从DDBM中读文件%s失败!\n",c_fileName);
        return false;
    }

    //向windows中写文件
    winFile_fp = fopen(c_filePath,"wb+");
    if(winFile_fp == NULL)
    {
        printf("要导出的文件路径错误！\n");
        return false;
    }
    if(fwrite(buf, fp->ull_FileSize, 1, winFile_fp))
    {
        printf("向windows写文件成功\n");
    }
    fclose(winFile_fp);

    Ddbm_Close_File(fp);  //关闭文件

    return true;

}

bool MainWindow::deleteFile()
{

    //STRUCT_FILE *fp = NULL;
    int8_ddbm *c_devName;
    int8_ddbm *c_fileName;
    QString fileName;
    uint32_ddbm ui_FsCount;


    int row = tableView->currentIndex().row();
    QModelIndex index = model->index(row,0);
    if(row<0)
    {
        QMessageBox::warning(this, QStringLiteral("提示"),QStringLiteral("没有任何选择"),QMessageBox::Ok);
        return false;
    }

    fileName = model->data(index).toString();
    //c_fileName = fileName.toUtf8().data();    //用这种方法会出问题，只要在Ddbm_Open_File中c_fileName进行传参就会失败
    c_fileName = (int8_ddbm*)malloc(strlen(fileName.toUtf8().data())+1);
    strcpy(c_fileName,fileName.toUtf8().data());

    if(fileName == NULL) return false;
    else
    {
        printf("[deleteFile]c_fileName文件名为%s,strlen(c_fileName)=%lu\n",c_fileName,strlen(c_fileName));
    }

    c_devName = curDevName.toUtf8().data();
    ui_FsCount = Ddbm_GetFileSystem_Index(c_devName);
    //直接在文件系统中删除文件
    if(Ddbm_Delete_File(ui_FsCount, c_fileName) == -1)
    {
        printf("删除文件%s失败\n",c_fileName);
        return false;
    }
    else
    {
        printf("删除文件%s成功\n",c_fileName);
    }

    updateTableView();
    return true;
}

void MainWindow::updateTableView()
{
    MEM_BLK_DEV *pDev;
    int8_ddbm *c_devName;
    int8_ddbm c_fileName[256];
    int32_ddbm index = 0;
    int32_ddbm tableLine = 0;
    int32_ddbm ui_FsCount;
    stFileViewInfo myFileViewInfo;
    uint32_ddbm *dbiChain;    //每次使用需要主动释放
    uint32_ddbm *bbmChain;    //每次使用需要主动释放
    int32_ddbm dbiValidSum;
    int32_ddbm bbmChainSum;

    STRUCT_ATTR *st_fileAttr;

    if(devList.isEmpty() || curDevName == NULL)
    {
        return;
    }

    c_devName = curDevName.toUtf8().data();

    c_devName = (int8_ddbm*)malloc(strlen(curDevName.toUtf8().data())+1);
    strcpy(c_devName,curDevName.toUtf8().data());

    ui_FsCount = Ddbm_GetFileSystem_Index(c_devName);
    if(ui_FsCount>=FS_NUM || ui_FsCount<0)  return;
    //printf("当前ui_FsCount是%d,gst_BootConfig[ui_FsCount].ui_DbiSum=%d\n",ui_FsCount,gst_BootConfig[ui_FsCount].ui_DbiSum);
    pDev = getMemBlkDev((uint8_ddbm*)c_devName);

    model->removeRows(0,model->rowCount());

#if 1
    /*每一次都遍历一下当前磁盘curDevName所有文件*/
    //遍历前先删除
    DbiShowArea->lsFileViewInfo.clear();
    BbmShowArea->lsFileViewInfo.clear();

    dbiValidSum = Ddbm_TraverseFiles(c_devName, &dbiChain);
    //printf("cur dbi valid sum = %d\n",dbiValidSum);

    for(index = 0; index < dbiValidSum; index++)
    {
        st_fileAttr = Ddbm_getFileAttr((int8_ddbm*)c_devName, dbiChain[index]);
        strcpy(c_fileName, st_fileAttr->uc_FileName);
        bbmChainSum = Ddbm_File_BbmInfo((int8_ddbm*)c_devName, c_fileName, &bbmChain);
        if(bbmChainSum == -1)    return;

        //printf("cur idx = %d\n",dbiChain[index]);
        //printf("curFileName = %s\n",st_fileAttr->uc_FileName);
        //printf("cur bbmChainSum = %d\n",bbmChainSum);

        model->setItem(tableLine,0,new QStandardItem(QString::fromLocal8Bit(c_fileName)));
        model->setItem(tableLine,1,new QStandardItem(QString::number(st_fileAttr->ull_FileSize,10)));
        model->setItem(tableLine,2,new QStandardItem(QString::number(dbiChain[index],10)));
        model->setItem(tableLine,3,new QStandardItem(QString::number(bbmChain[0],10)));
        model->setItem(tableLine,4,new QStandardItem(QString::number(bbmChainSum,10)));

        //用于画图的当前文件的信息
        myFileViewInfo.fileName = QString::fromLocal8Bit(c_fileName);
        myFileViewInfo.bbmNum = bbmChainSum;
        myFileViewInfo.bbmStartIdx = bbmChain[0];
        myFileViewInfo.dbiIdx = dbiChain[index];
        //append函数在内部进行了数据的复制，并且这种复制只是浅拷贝
        for(int i=0;i<bbmChainSum;i++)
        {
            myFileViewInfo.bbmList.append(bbmChain[i]);
        }

        DbiShowArea->lsFileViewInfo.append(myFileViewInfo);
        BbmShowArea->lsFileViewInfo.append(myFileViewInfo);

        free(bbmChain); //

        tableLine++;

    }

    free(dbiChain);  //
#endif

    DbiShowArea->rectSum = dev_info.dbi_max_num;
    BbmShowArea->rectSum = dev_info.bbm_num;
    DbiShowArea->repaint();  //repaint和update不同，每次只要有事件就会重绘
    BbmShowArea->repaint();

}


void MainWindow::recvDevInfoDialogData()
{
    if(devInfoDialog *devinfodialog = dynamic_cast<devInfoDialog*>(sender()))
    {
        dev_info = devinfodialog->putDevInfo();
        printf("[recvDevInfoDialogData]dev_info got from dialog!\n");
    }

}


