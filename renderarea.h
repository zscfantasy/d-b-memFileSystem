
#ifndef RENDERAREA_H
#define RENDERAREA_H

#include <QBrush>
#include <QPen>
#include <QPixmap>
#include <QWidget>


struct stFileViewInfo
{
    QString fileName;
    int dbiIdx;
    int bbmStartIdx;
    int bbmNum;
    QList<int> bbmList;
};

class RenderArea : public QWidget
{
    Q_OBJECT

public:
    RenderArea(QWidget *parent = 0);

    QSize minimumSizeHint() const;
    QSize sizeHint() const;

    enum ViewType { DBI, BBM };
    ViewType mytype;
    int rectSum;
    QList<stFileViewInfo> lsFileViewInfo;



public slots:
    void updateView();


protected:
    void paintEvent(QPaintEvent *event);

private:

    void drawRect(int num);
    void drawDbi();
    void drawBbm();


};


#endif // RENDERAREA_H
