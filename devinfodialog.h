#ifndef DEVINFODIALOG_H
#define DEVINFODIALOG_H

#include <QDialog>
#include "DiskIO.h"

namespace Ui {
class devInfoDialog;
}

class devInfoDialog : public QDialog
{
    Q_OBJECT

public:
    explicit devInfoDialog(QWidget *parent = nullptr);
    ~devInfoDialog();
    void getDevInfo(BLK_DEV_INFO dev_info,QString curDevName);
    BLK_DEV_INFO putDevInfo();

private:
    Ui::devInfoDialog *ui;
};

#endif // DEVINFODIALOG_H
