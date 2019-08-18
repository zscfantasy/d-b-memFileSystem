#include "devinfodialog.h"
#include "ui_devinfodialog.h"


devInfoDialog::devInfoDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::devInfoDialog)
{
    ui->setupUi(this);
}

devInfoDialog::~devInfoDialog()
{
    delete ui;
}

void devInfoDialog::getDevInfo(BLK_DEV_INFO dev_info,QString curDevName)
{

    ui->label_curDevName->setText(QStringLiteral("当前设备名:")+curDevName);
    ui->lineEdit_FormatSum->setText(QString::number(dev_info.disk_size,10));
    ui->lineEdit_DBISum->setText(QString::number(dev_info.dbi_max_num,10));
    ui->lineEdit_DBUSize->setText(QString::number(dev_info.dbu_size,10));
    ui->lineEdit_DBIStart->setText(QString::number(dev_info.dbi_start_sector,10));
    ui->lineEdit_BOOTStart->setText(QString::number(dev_info.boot_start_sector,10));
    ui->lineEdit_SectorSize->setText(QString::number(dev_info.sector_size,10));
    ui->lineEdit_devTotalSize->setText(QString::number(dev_info.disk_total_size,10));
    ui->lineEdit_BBMSectorSize->setText(QString::number(dev_info.one_bbm_sector_num,10));

}

BLK_DEV_INFO devInfoDialog::devInfoDialog::putDevInfo()
{
    BLK_DEV_INFO dev_info;
    dev_info.disk_size = ui->lineEdit_FormatSum->text().toInt();
    dev_info.disk_size = ui->lineEdit_FormatSum->text().toInt();
    dev_info.dbi_max_num = ui->lineEdit_DBISum->text().toInt();
    dev_info.dbu_size = ui->lineEdit_DBUSize->text().toInt();
    dev_info.dbi_start_sector = ui->lineEdit_DBIStart->text().toInt();
    dev_info.boot_start_sector = ui->lineEdit_BOOTStart->text().toInt();
    dev_info.sector_size = ui->lineEdit_SectorSize->text().toInt();
    dev_info.disk_total_size = ui->lineEdit_devTotalSize->text().toInt();
    dev_info.one_bbm_sector_num = ui->lineEdit_BBMSectorSize->text().toInt();

    return dev_info;

}
