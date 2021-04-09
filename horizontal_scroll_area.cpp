#include "horizontal_scroll_area.h"
#include <QScrollArea>
#include <QGridLayout>
#include <QScrollBar>
#include <QResizeEvent>
#include <QDebug>
#include <stdio.h>

Horizontal_Scroll_Area::Horizontal_Scroll_Area(QWidget *parent) : QScrollArea(parent) {
    printf("In constructor parent pointer\n");
    nRows = 1; nColumns = 1;
    setWidgetResizable(true);
    contentWidget = new QWidget(this);
    setWidget(contentWidget);
    grid = new QGridLayout(contentWidget);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}


Horizontal_Scroll_Area::Horizontal_Scroll_Area(int rows, int cols, QWidget *parent) : QScrollArea(parent), nRows(rows), nColumns(cols) {
    setWidgetResizable(true);
    contentWidget = new QWidget(this);
    setWidget(contentWidget);
    grid = new QGridLayout(contentWidget);
   // setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

//If you use QtDesigner it will force install a widget, must make grid align to this
void Horizontal_Scroll_Area::setWidget(QWidget *widget) {
    //Now set widget
    contentWidget = widget;
    contentWidget->setParent(this);
    QScrollArea::setWidget(widget);
    grid = new QGridLayout(contentWidget);
    //setVerticalScrollBarPolicy((Qt::ScrollBarAlwaysOff));
}



void Horizontal_Scroll_Area::addWidget(QWidget *w, int row, int col) {
    grid->addWidget(w, row, col);
    adaptSize();
}

int Horizontal_Scroll_Area::columnCount() const{
    printf("In Column Count\n");
    if(grid->count() == 0) {
        return 0;
    }

    return grid->columnCount();
}

void Horizontal_Scroll_Area::adaptSize() {
    if(columnCount() >= nColumns) {
        int w = 1.0*(width() - grid->horizontalSpacing()*(nColumns+1.6))/nColumns;
        int wCorrected = w*columnCount() + grid->horizontalSpacing()*(columnCount()+2);
        contentWidget->setFixedWidth(wCorrected);
    }

    contentWidget->setFixedHeight(viewport()->height());
}

void Horizontal_Scroll_Area::resizeEvent(QResizeEvent *event) {
    QScrollArea::resizeEvent(event);        //call super
    adaptSize();
}
