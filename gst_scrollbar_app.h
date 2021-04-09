#ifndef GST_SCROLLBAR_APP_H
#define GST_SCROLLBAR_APP_H

#include <QMainWindow>
#include <settings_mngr.h>
#include "pipe_builder.h"
#include <map>
#include <string>

namespace Ui {
class gst_scrollbar_app;
}

class gst_scrollbar_app : public QMainWindow
{
    Q_OBJECT

public:
    explicit gst_scrollbar_app(QWidget *parent = 0);
    ~gst_scrollbar_app();
    int gst_scroll_widget_add();
    static void gst_bus_callback(GstBus *bus, GstMessage *msg, pipe_builder* data);

public slots:
    void create_cam(cam_params* cam);
    void start_all_recording();
    void stop_all_recording();

private:
    Ui::gst_scrollbar_app *ui;
    settings_mngr* file_mngr;
    map<string, pipe_builder*> pipes;
    int row, col;
    int cntr;
};

#endif // GST_SCROLLBAR_APP_H
