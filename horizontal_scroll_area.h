#ifndef HORIZONTAL_SCROLL_AREA_H
#define HORIZONTAL_SCROLL_AREA_H

//Grabbed from stackoverflow.com/questions/46547089/qscrollarea-with-dynamically-added-widget
#include <QScrollArea>
#include <QGridLayout>
#include <QScrollBar>
#include <QResizeEvent>

class Horizontal_Scroll_Area :public QScrollArea
{
    QWidget *contentWidget;
    QGridLayout *grid;
    int nRows;
    int nColumns;
public:
    Horizontal_Scroll_Area(QWidget* parent = Q_NULLPTR);                     //add default constructor
    Horizontal_Scroll_Area(int rows, int cols, QWidget*parent = Q_NULLPTR);

    void setWidget(QWidget* widget);
    void addWidget(QWidget* w, int row, int col);
    int columnCount() const;

private:
    void adaptSize();

protected:
    void resizeEvent(QResizeEvent *event);
};

#endif // HORIZONTAL_SCROLL_AREA_H
