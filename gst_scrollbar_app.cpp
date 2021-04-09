#include "gst_scrollbar_app.h"
#include "ui_gst_scrollbar_app.h"
#include "settings_mngr.h"
#include "pipe_builder.h"
#include <QPushButton>
#include <QWidget>

gst_scrollbar_app::gst_scrollbar_app(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::gst_scrollbar_app)
{
    row = 0; col = 0; cntr = 0;
    ui->setupUi(this);
    file_mngr = new settings_mngr();

    //Connect slots for settings_mngr and buttons
    connect(ui->add_camera_button, &QPushButton::released, file_mngr, &settings_mngr::cam_add_show);
    connect(ui->rm_camera_button, &QPushButton::released, file_mngr, &settings_mngr::cam_rm_show);
    connect(file_mngr, &settings_mngr::create_new_camera, this, &gst_scrollbar_app::create_cam);

    //Add recording buttons
    connect(ui->start_rec_button, &QPushButton::released, this, &gst_scrollbar_app::start_all_recording);
    connect(ui->stop_rec_button, &QPushButton::released, this, &gst_scrollbar_app::stop_all_recording);

    //Now all signals have been made, build our lib
    file_mngr->build_lib_from_xml();
}

gst_scrollbar_app::~gst_scrollbar_app()
{
    delete ui;
    delete file_mngr;
}

void gst_scrollbar_app::start_all_recording() {
    //loop through all of our pipes
    map<string, pipe_builder*>::iterator it;
    for(it = pipes.begin(); it != pipes.end(); it++) {
        g_print("Attempting to start recording on device %s\n", it->second->myname.c_str());
        it->second->add_recording_branch();
    }
    g_print("All streams are recording!\n");
}

void gst_scrollbar_app::stop_all_recording() {
    map<string, pipe_builder*>::iterator it;
    for(it = pipes.begin(); it != pipes.end(); it++) {
        it->second->remove_recording_branch();
    }
    g_print("All streams stopped recording!\n");
}

void gst_scrollbar_app::create_cam(cam_params *cam) {
    QWidget* newWindow = new QWidget();
    newWindow->show();
    ui->Gst_Frame_Scroll->addWidget(newWindow, row, col);
    col++;

    string name_cam = "device_" + to_string(cntr);
    pipe_builder* new_pipe = new pipe_builder(newWindow, cam, name_cam);

    //For some reason attaching the bus messages when detaching causes an error? Check back later
    connect(new_pipe, &pipe_builder::emit_bus_messages, ui->Debug_out_textbox, &QPlainTextEdit::appendPlainText);

    //gst_bus_add_signal_watch(new_pipe->data_accessor()->bus);
    //g_signal_connect(new_pipe->data_accessor()->bus, "message", G_CALLBACK(gst_bus_callback), new_pipe);

    pipes.insert(pair<string, pipe_builder*>(cam->name, new_pipe));
    cntr++;
}//Note: static functions do not have a connection to "this" pointer thus cannot emit signals

void gst_scrollbar_app::gst_bus_callback(GstBus *bus, GstMessage *msg, pipe_builder* data) {
    //test to see if the slot works
    //emit data->emit_bus_messages (QString("Bus callback occurred\n"));
    //g_print("Bus callback occurred!\n");
    switch (GST_MESSAGE_TYPE (msg)) {
        case GST_MESSAGE_ERROR:
            GError *err;
            gchar *debug_info;
            gst_message_parse_error (msg, &err, &debug_info);
            emit data->emit_bus_messages(QString("\n****** Source of message: %1 ******\nError received from element %2: %3\n").arg(data->myname.c_str()).arg(GST_OBJECT_NAME (msg->src)).arg(err->message));
            g_clear_error (&err);
            g_free (debug_info);
            break;
        case GST_MESSAGE_EOS:
            //g_print ("\n******\nSource of message: %s\nEnd-Of-Stream reached.\n", data->myname.c_str());
            emit data->emit_bus_messages(QString("\n****** Source of message: %1 ******\nEnd-Of-Stream reached.\n").arg(data->myname.c_str()));
            break;
        case GST_MESSAGE_STATE_CHANGED:
            if(GST_MESSAGE_SRC(msg) == GST_OBJECT(data->data_accessor()->pipeline)) {
                GstState old_state, new_state, pending_state;
                gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
                //g_print("Pipeline changed from %s to %s, and pending is %s.\n", gst_element_state_get_name(old_state), gst_element_state_get_name(new_state), gst_element_state_get_name(pending_state));
                emit data->emit_bus_messages(QString("\n****** Source of message: %1 ******\n Pipeline changed from %2 to %3, and pending is %4.\n").arg(data->myname.c_str()).arg(gst_element_state_get_name(old_state)).arg(gst_element_state_get_name(new_state)).arg(gst_element_state_get_name(pending_state)));

            }
            break;
        default:
            /* We should not reach here because we only asked for ERRORs and EOS */
            //g_printerr ("Unexpected message received.\n");
            //emit data->emit_bus_messages(QString("Unhandled message from element %1\n").arg(GST_OBJECT_NAME (msg->src)));
            break;
    }
}



