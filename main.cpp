#include "gst_scrollbar_app.h"
#include <QApplication>
#include <gst/gst.h>

int main(int argc, char *argv[])
{
    gst_init(&argc, &argv);

    QApplication a(argc, argv);
    gst_scrollbar_app w;
    w.show();

    return a.exec();
}
