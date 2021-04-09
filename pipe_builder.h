/****
 * Author: Chance Reimer
 * Purpose: create a pipeline based off of user settings found in xml
 * all errors should not fail the program but be printed out to debug
 */

#ifndef PIPE_BUILDER_H
#define PIPE_BUILDER_H
#include <QObject>
#include <gst/gst.h>
#include <QPlainTextEdit>
#include "settings_mngr.h"
#include <string>

//Note you can grab eleemnts from the pipe by using "gst_bin_get_by_name(pipe, <name of element>)"
//Maybe add vector of element names if needed at later date to access elements
//Not adding tee to this just to test that gst get by name thing, but it's easier to standardize the format we wish to save here, free and add the tee module as needed
typedef struct _pipe_elements {
    GstElement *pipeline;
    GstElement *tee;
    GstElement* queue_rec;
    GstElement* nvvidconv_rec;
    GstElement* h264_enc_rec;
    GstElement* mp4mux;
    GstElement* filesink;
    GstPad*     teepad_rec;
    GstBus     *bus;
    GMainLoop  *loop;
    string name_of_pipe;
    bool        isrecording;
} pipe_elements;

class pipe_builder : public QObject
{
Q_OBJECT

public:
    string myname;
    pipe_builder(QWidget* sink_overlay, cam_params* params, string name);
    ~pipe_builder();
    void create_rtsp_pipe();
    void create_device_pipe();
    pipe_elements* data_accessor();
    void add_recording_branch();    //remember gpointer is basically just a void pointer
    void remove_recording_branch();
    static GstPadProbeReturn unlink_vid_rec_pipe_cb(GstPad* pad, GstPadProbeInfo *info, gpointer user_data);
signals:
    void emit_bus_messages(QString msg);

public slots:
    void handle_play_stop();
    void handle_rec_start();
    void handle_rec_stop();

private:
    string save_path;
    string name_tee;
    pipe_elements data;
    cam_params* params;
    QWidget* sink_overlay;
    bool stream_on_flag;
    static void rtsp_callback_handler(GstElement* src, GstPad *new_pad, pipe_elements* data_struct);
    //static void gst_bus_callback(GstBus *bus, GstMessage *msg, data_custom* data);
};

#endif // PIPE_BUILDER_H
