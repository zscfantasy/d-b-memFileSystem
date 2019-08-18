#include "renderarea.h"

#include <QPainter>

//! [0]
RenderArea::RenderArea(QWidget *parent)
    : QWidget(parent)
{
    setBackgroundRole(QPalette::Base);
    setAutoFillBackground(true);
    rectSum = 0;

}

QSize RenderArea::minimumSizeHint() const
{
    return QSize(100, 100);
}

QSize RenderArea::sizeHint() const
{
    return QSize(400, 200);
}

void RenderArea::updateView()
{   
    update();
}



void RenderArea::paintEvent(QPaintEvent * /* event */)
{

    //printf("paint event,dbinum=%d,bbmnum=%d\n",dbinum,bbmnum);

    switch (mytype) {
    case DBI:
        drawRect(rectSum);
        drawDbi();
        break;
    case BBM:
        drawRect(rectSum);
        drawBbm();
    }


}

void RenderArea::drawRect(int num)
{
    int i = 0;
    int rectEdgeLength;
    QPainter painter(this);

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(palette().dark().color());
    painter.setBrush(Qt::white);
    painter.drawRect(QRect(0, 0, width(), height()));

    painter.setPen(QPen(Qt::black,1,Qt::SolidLine,Qt::RoundCap,Qt::MiterJoin));
    painter.setBrush(QBrush(Qt::green,Qt::SolidPattern));

    rectEdgeLength = 10;
    QRect rect(0, 0, rectEdgeLength, rectEdgeLength);
    for (int y = 0; y < height()/rectEdgeLength; y++) {
        if(i>num) break;
        for (int x = 0; x < width()/rectEdgeLength; x++) {
            //if(i > 50) painter.setBrush(QBrush(Qt::red,Qt::SolidPattern));
            i++;
            if(i>num) break;
            painter.save();
            painter.translate(x*10, y*10);
            painter.drawRect(rect);
            painter.restore();

        }
    }


}

void RenderArea::drawDbi()
{
    int y,x;
    int rectEdgeLength;
    QPainter painter(this);
    painter.setPen(QPen(Qt::black,1,Qt::SolidLine,Qt::RoundCap,Qt::MiterJoin));
    painter.setBrush(QBrush(Qt::red,Qt::SolidPattern));

    rectEdgeLength = 10;
    QRect rect(0, 0, rectEdgeLength, rectEdgeLength);

    //printf("lsFileViewInfo.count() = %d\n",lsFileViewInfo.count());
    QList<stFileViewInfo>::Iterator it = lsFileViewInfo.begin(),itend = lsFileViewInfo.end();
    for(;it != itend;it++){

        y = (it->dbiIdx)/(width()/rectEdgeLength);
        x = (it->dbiIdx)%(width()/rectEdgeLength);
        painter.save();
        painter.translate(x*10,y*10);
        painter.drawRect(rect);
        painter.restore();
    }
}

void RenderArea::drawBbm()
{
    int y,x;
    int rectEdgeLength;
    QPainter painter(this);
    painter.setPen(QPen(Qt::black,1,Qt::SolidLine,Qt::RoundCap,Qt::MiterJoin));
    painter.setBrush(QBrush(Qt::red,Qt::SolidPattern));

    rectEdgeLength = 10;
    QRect rect(0, 0, rectEdgeLength, rectEdgeLength);

    QList<stFileViewInfo>::Iterator it = lsFileViewInfo.begin(),itend = lsFileViewInfo.end();
    for(;it != itend;it++){

        painter.setBrush(QBrush(Qt::yellow,Qt::SolidPattern));
        QList<int>::Iterator it_bbm = it->bbmList.begin(),itend_bbm = it->bbmList.end();
        for(;it_bbm != itend_bbm;it_bbm++){
//printf("[drawBbm]*it_bbm%d\n",*it_bbm);
            y = (*it_bbm)/(width()/rectEdgeLength);
            x = (*it_bbm)%(width()/rectEdgeLength);
            painter.save();
            painter.translate(x*10,y*10);
            painter.drawRect(rect);
            painter.restore();
        }
    }

    //另外重新把首个bbm标记成红色
    for(it = lsFileViewInfo.begin();it != itend;it++){

//printf("标记文件idx=%d,[drawBbm]it->bbmStartIdx%d\n",it->dbiIdx, it->bbmStartIdx);
        painter.setBrush(QBrush(Qt::red,Qt::SolidPattern));
        y = (it->bbmStartIdx)/(width()/rectEdgeLength);
        x = (it->bbmStartIdx)%(width()/rectEdgeLength);
        painter.save();
        painter.translate(x*10,y*10);
        painter.drawRect(rect);
        painter.restore();

    }
}


