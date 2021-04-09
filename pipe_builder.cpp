#include "pipe_builder.h"
#include <glib.h>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>
#include <QPushButton>
#include <QWidget>
#include <QThread>
#include <QPlainTextEdit>
#include <QString>
#include <settings_mngr.h>

/********
 * Gstreamer notes for the See3Cam Cu30
 * Provides 2 modes, see these by using : v4l2-ctl --device=/dev/video1 --list-formats-ext (This is only if video1 is the current pointer to this camera)
 * First useful for UYVY:
 *      gst-launch-1.0 v4l2src device=/dev/video1 ! "video/x-raw, format=(string)UYVY, width=(int)1920, height=(int)1080" \
 *                     ! nvvidconv ! "video/x-raw(memory:NVMM), format=(string)I420, width=(int)1920, height=(int)1080" ! autovideosink
 *      //For UYVY to xvimagesink (we need xvimagesink to frame over QtWidget:
 *      gst-launch-1.0 v4l2src device=/dev/video1 ! "video/x-raw, format=(string)UYVY, width=(int)1920, height=(int)1080" ! queue ! xvimagesink
 *
 * For recording UYVY, this works intermittently, maybe cable length is too long?
 *      gst-launch-1.0 v4l2src device=/dev/video2 ! "video/x-raw, format=(string)UYVY, width=(int)1920, height=(int)1080" ! nvvideoconvert ! omxh264enc ! flvmux ! filesink location=newfile.flv
 * For MJPEG the following provides error free streaming:
 *      gst-launch-1.0 v4l2src device=/dev/video1 do-timestamp=true ! "image/jpeg, width=(int)2304, height=(int)1536"  \
 *                     ! jpegdec  ! "video/x-raw" ! nvvidconv ! "video/x-raw(memory:NVMM), \
 *                     format=(string)I420, width=(int)2304, height=(int)1536" ! autovideosink
 * For xvimagesink:
 *      gst-launch-1.0 v4l2src device=/dev/video1 do-timestamp=true ! "image/jpeg, width=(int)2304, height=(int)1536" ! jpegdec  ! "video/x-raw" ! queue ! xvimagesink
 *
 * Note for both, the streaming format used for autovideosink is I420. Currently used since autovideosink is expecting it.
 * To find any information on the gstreamer plugins use the command "gst-inspect-1.0 <name-of-plugin>", i.e.
 *      "gst-inspect-1.0 nvvidconv" to get information like supported caps, (Klass), ect.
 *
 *For RTSP:
 * gst-launch-1.0 rtspsrc location=rtsp://<uname>:<pass>@<url> latency=100 ! queue ! rtph264depay ! h264parse ! nvv4l2decoder ! videoconvert ! videoscale ! 'video/x-raw(memory:NVMM),width=640,height=480' ! nvvidconv ! xvimagesink
 * save: gst-launch-1.0 rtspsrc location=rtsp://<uname>:<pass>@<url> latency=100 ! queue ! rtph264depay ! h264parse ! nvv4l2decoder ! videoconvert ! videoscale ! 'video/x-raw(memory:NVMM),width=640,height=480' ! nvvidconv ! omxh264enc ! flvmux ! filesink location=rtsp.flv
*/
pipe_builder::pipe_builder(QWidget* sink_overlay, cam_params* params, string name_pipe)
{
    this->sink_overlay = sink_overlay;
    stream_on_flag = FALSE;
    name_tee = "tee";
    data.isrecording = false;
    myname = name_pipe;

    //Check the type of camera that we are initing (only support RTSP and camera on device)
    printf("Attempting to create new camera: %s\n", params->name.c_str());
    this->params = params;
    if(params->type == camera_types[0]) {
        create_rtsp_pipe();
    }
    else {
        create_device_pipe();
    }
}

pipe_builder::~pipe_builder()
{
    //Cleanup gstreamer app
    gst_element_set_state(data.pipeline, GST_STATE_NULL);
    gst_object_unref(data.pipeline);
}

pipe_elements* pipe_builder::data_accessor() {
    return &data;
}

//Create all the necessary data and checks for the RTSP pipeline
//gst-launch-1.0 rtspsrc location=rtsp://<usr>:<pass>@<ip_addr> latency=100 ! queue ! rtph264depay ! h264parse ! nvv4l2decoder ! videoconvert ! videoscale ! 'video/x-raw(memory:NVMM),width=640,height=480' ! nvvidconv ! 'video/x-raw,format=UYVY,width=640,height=480' ! queue ! xvimagesink
//gst-launch-1.0 rtspsrc location=rtsp://<usr>:<pass>@<ip_addr> latency=100 ! queue ! rtph264depay ! h264parse ! nvv4l2decoder ! videoconvert ! videoscale ! 'video/x-raw(memory:NVMM),width=640,height=480' ! nvvidconv ! 'video/x-raw,format=(string)UYVY,width=(int)640,height=(int)480' ! tee name=t ! queue ! xvimagesink t. ! queue ! nvvidconv ! omxh264enc ! mp4mux ! filesink location=test.mp4 -e
//Note for above, adding the caps after nvvidconv are 100% needed to get recording branch to work, but has following errors:
//  *gst_mini_object_unref: assertion 'mini_object != NULL' failed  -> this guy is an nvidia only hooked to nvvl412decoder, add omxh264 for the other errors. My guess is fix by addign caps?
//  *gst_caps_get_structure: assertion 'GST_IS_CAPS (caps)' failed
//  *gst_structure_copy: assertion 'structure != NULL' failed
//  *gst_caps_append_structure_full: assertion 'GST_IS_CAPS (caps)' failed
//  So not sure how to fix these, but the data records fine so... eh?
void pipe_builder::create_rtsp_pipe() {
    GstCaps *videosrc_caps, *videosink_caps;
    //Instantiate all elements
    GstElement *rtsp_src, *queue, *rtph264depay, * h264parse, *nvdl2decoder, *videoconvert, *videoscale, *nvvidconv, *sink, *queue2;
    GstElement *caps, *caps_sink;

    // Create all pipe elements
    rtsp_src = gst_element_factory_make ("rtspsrc", "rtsp_source");                  //source generation from rtsp source
    if(!rtsp_src)
        g_print("rtsp_src failed to be made!\n");

    queue = gst_element_factory_make("queue", "queue-rtsp");                                      //queue data from source for glitch free stream
    if(!queue)
        g_print("queue failed to be made!\n");

    rtph264depay = gst_element_factory_make("rtph264depay", "h264_rtp_extracter");   //extracts h264 video from rtp packets
    if(!rtph264depay)
        g_print("h264 rtp extracter failed to be made!\n");

    h264parse = gst_element_factory_make("h264parse", "h264_parser");
    if(!h264parse)
        g_print("H264 parse failed to be made!\n");

    nvdl2decoder = gst_element_factory_make("nvv4l2decoder", "nvidia_decoder");
    if(!nvdl2decoder)
        g_print("Failed to create nvdl2decoder!\n");

    videoconvert = gst_element_factory_make("videoconvert", "video_convert");
    if(!videoconvert)
        g_print("Failed to create videoconvert!\n");

    videoscale   = gst_element_factory_make("videoscale", "videoscale");
    if(!videoscale)
        g_print("Failed to create videoscale!\n");

    //need nvvidconv between videoscale and xvimagesink
    nvvidconv = gst_element_factory_make("nvvidconv", "nvidia_convert");
    if(!nvvidconv)
        g_print("Failed to create nvvidconv.\n");

    data.tee = gst_element_factory_make("tee", name_tee.c_str());
    if(!data.tee)
        g_printerr("Couldn't create tee in RTSP!\n");

    queue2 = gst_element_factory_make("queue", "queue-2");
    if(!queue2)
        g_printerr("Couldn't create tee queue!\n");

    //the video x-raw are just caps that you add to the stream
    //there appears to be bug in nvarguscamerasrc -> forced to use gst_caps_from_string instead of gst_caps_new_simple
    //videosrc_caps = gst_caps_new_simple("video/x-raw(memory:NVMM)", "width", G_TYPE_INT, 640, "height", G_TYPE_INT, 480, NULL); //null terminated video src cap to link at end of videoscale
    string video_caps_str = "video/x-raw(memory:NVMM),width=" + to_string(params->size_use.first) + ",height=" + to_string(params->size_use.second);
    //videosrc_caps = gst_caps_from_string("video/x-raw(memory:NVMM),width=640,height=480");
    //videosrc_caps = gst_caps_new_simple("video/x-raw", "width", G_TYPE_INT, params->size_use.first, "height", G_TYPE_INT, params->size_use.second, NULL);
    videosrc_caps = gst_caps_from_string(video_caps_str.c_str());
    videosink_caps = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "UYVY", "width", G_TYPE_INT, params->size_use.first, "height", G_TYPE_INT, params->size_use.second, NULL);
    sink = gst_element_factory_make("xvimagesink", "sink");
    if(!sink)
        g_print("Failed to create autovideosink\n");

    //note adding caps to an element is pretty easy
    caps = gst_element_factory_make("capsfilter", "filter");
    g_assert(caps != NULL);  //always check that the caps are made before we assign the g_object

    caps_sink = gst_element_factory_make("capsfilter", "filter2");
    g_assert(caps != NULL);

    //this returns a GST_BIN element used to contain other GSTElements
    data.pipeline = gst_pipeline_new ("rtsp_pipeline");

    //Error checking to ensure that pointers got instantiated
    if (!data.pipeline || !rtsp_src || !queue || !rtph264depay || !h264parse || !nvdl2decoder || !videoconvert || !videoscale || !caps || !caps_sink || !sink || !nvvidconv || !queue2 || !data.tee) {
        g_printerr ("Not all elements could be created.\n");
        gst_caps_unref(videosrc_caps);
        gst_caps_unref(videosink_caps);
        //return;
    }

    // Build the pipeline
    //Could also use gst_bin_add to add individual elements to pipeline class
    gst_bin_add_many (GST_BIN (data.pipeline), rtsp_src, queue, rtph264depay, h264parse, nvdl2decoder, videoconvert, videoscale, caps, caps_sink, nvvidconv, sink, data.tee, queue2, NULL);
    //must add elements to the pipeline BEFORE linking them!
    //Also note, gst_element_link_many must be terminated by a NULL statement for gst to know elements are done
    //Note 2 --> rtsp_src has no pads, it must grab pads first, and send them on to the next sink pad (queue sink)
    if (gst_element_link_many (queue, rtph264depay, h264parse, nvdl2decoder, videoconvert, videoscale, caps, nvvidconv, caps_sink, data.tee, queue2, sink, NULL) != TRUE) {
        g_printerr ("Elements could not be linked.\n");
        gst_object_unref (data.pipeline);
        gst_caps_unref(videosrc_caps);
        //return;
    }

    //Set filter to appropriate caps specified in GstCaps
    g_object_set(G_OBJECT(caps), "caps", videosrc_caps, NULL);
    gst_caps_unref(videosrc_caps);
    g_object_set(G_OBJECT(caps_sink), "caps", videosink_caps, NULL);
    gst_caps_unref(videosink_caps);

    //Here setup rtsp src
    //basically point to gst_element, then do name:value pairs, end on NULL
    g_object_set(rtsp_src, "location", params->dev_path.c_str(), "latency", 100, NULL);
    data.loop = g_main_loop_new(NULL, FALSE);

    //add bus
    data.bus = gst_element_get_bus(data.pipeline);

    //now connect the callback
    g_signal_connect(rtsp_src, "pad-added", G_CALLBACK(rtsp_callback_handler), &data);
    //g_signal_connect(data.bus, "message", G_CALLBACK(gst_bus_callback), &data);

    //QWidget* vid_wig = ui->centralWidget->findChild<QWidget*>("video_widget");
    this->sink_overlay->resize(params->size_use.first, params->size_use.second);       //resize to the size of our stream
    //vid_wig->show();
    WId xwinid = this->sink_overlay->winId();
    g_print("Grabbed window ID of the video widget, video is: %d\n", (int)xwinid);
    gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(sink), xwinid);      //requires pkg-config add of gstreamre-video-1.0

    //Immediately set the pipe to playing
    GstStateChangeReturn ret;
    ret = gst_element_set_state(data.pipeline, GST_STATE_PLAYING);
    if(ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr("UNABLE TO START THE PIPE!\n");
    }
    stream_on_flag = true;

    //g_main_loop_run(data.loop);
}

//cannot emit from static functions
//void pipe_builder::gst_bus_callback(GstBus *bus, GstMessage *msg, data_custom* data) {
//    emit emit_bus_messages("Bus callback occurred\n");
//    g_print("Bus callback occurred!\n");
//    switch (GST_MESSAGE_TYPE (msg)) {
//        case GST_MESSAGE_ERROR:
//            GError *err;
//            gchar *debug_info;
//            gst_message_parse_error (msg, &err, &debug_info);
//            //emit emit_bus_messages(QString("Error received from element %1: %2\n").arg(GST_OBJECT_NAME (msg->src)).arg(err->message));
//            g_printerr ("Error received from element %s: %s\n",
//                GST_OBJECT_NAME (msg->src), err->message);
//            g_printerr ("Debugging information: %s\n",
//                debug_info ? debug_info : "none");
//            g_clear_error (&err);
//            g_free (debug_info);
//            break;
//        case GST_MESSAGE_EOS:
//            g_print ("End-Of-Stream reached.\n");
//            break;
//        case GST_MESSAGE_STATE_CHANGED:
//            if(GST_MESSAGE_SRC(msg) == GST_OBJECT(data->pipeline)) {
//                GstState old_state, new_state, pending_state;
//                gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
//                g_print("Pipeline changed from %s to %s, and pending is %s.\n", gst_element_state_get_name(old_state), gst_element_state_get_name(new_state), gst_element_state_get_name(pending_state));
//                //emit emit_bus_messages(QString("Pipeline changed from %1 to %2, and pending is %3.\n").arg(gst_element_state_get_name(old_state)).arg(gst_element_state_get_name(new_state)).arg(gst_element_state_get_name(pending_state)));

//            }
//            break;
//        default:
//            /* We should not reach here because we only asked for ERRORs and EOS */
//            g_printerr ("Unexpected message received.\n");
//            break;
//    }
/******
 * gst-launch-1.0 v4l2src device=/dev/video1 ! "video/x-raw, format=(string)UYVY, width=(int)1920, height=(int)1080" ! queue ! xvimagesink
 * gst-launch-1.0 v4l2src device=/dev/video1 do-timestamp=true ! "image/jpeg, width=(int)2304, height=(int)1536" ! jpegdec  ! "video/x-raw" ! queue ! xvimagesink
 *
 * Using tee for storing video and watching videos
 * Explanation: must add queues at end of tee as mandated by the gods of gstreamer. Thou shalt also add -e (end of stream) when doing mp4 files because you'll get a corrupted file otherwise.
 *          gst-launch-1.0 -e v4l2src device=/dev/video1 ! "video/x-raw, format=(string)UYVY, width=(int)1920, height=(int)1080" ! tee name=t ! queue ! xvimagesink t. ! queue ! nvvidconv ! omxh264enc ! mp4mux ! filesink location=test.mp4
 * Need to figure out how to send EOS and dynamically add remove tee pads to pipeline
 */
void pipe_builder::create_device_pipe() {
    //Instantiate all elements
    GstElement *v4l2_src, *decoder, *queue, *sink, *rotate;
    //GstElement *queue_rec_branch, *nvvidconv, *h264enc, *mp4mux, *filesink;
    GstElement *caps_src, *caps_sink;
    GstCaps *video_source_caps, *video_sink_caps;

    //Regardless of type we will make a tee
    data.tee = gst_element_factory_make("tee", name_tee.c_str());
    if(!data.tee) {
        g_printerr("Failed to make tee!\n");
    }

    //Now check or cams to see what pixel format used
    // build pipe for video x_raw
    if(params->pixel_fmt == "UYVY") {
        v4l2_src = gst_element_factory_make("v4l2src", "source");
        if(!v4l2_src)
            g_printerr("v4l2 source not created!\n");
        queue    = gst_element_factory_make("queue", "queue");
        if(!queue)
            g_printerr("queue not created!\n");
        sink     = gst_element_factory_make("xvimagesink", "sink");
        if(!sink)
            g_printerr("sink not created!\n");

        rotate = gst_element_factory_make("videoflip", "rotate1");
        if(!rotate) {
            g_printerr("Error didn't create rotate\n!");
        }

        //note adding caps to an element is pretty easy
        caps_src = gst_element_factory_make("capsfilter", "filter");
        g_assert(caps_src != NULL);  //always check that the caps are made before we assign the g_object

        //this returns a GST_BIN element used to contain other GSTElements
        data.pipeline = gst_pipeline_new ("pipeline");

        //create caps needed
        video_source_caps = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, params->pixel_fmt.c_str(), "width", G_TYPE_INT, params->size_use.first, "height", G_TYPE_INT, params->size_use.second, NULL);

        //Error checking to ensure that pointers got instantiated
        if (!data.pipeline || !v4l2_src || !queue || !caps_src || !sink || !rotate || !data.tee) {
            g_printerr ("Not all elements could be created.\n");
            gst_caps_unref(video_source_caps);
            //return;
        }

        // Build the pipeline
        gst_bin_add_many (GST_BIN (data.pipeline), v4l2_src, caps_src, queue, sink, rotate, data.tee, NULL);
        if (gst_element_link_many (v4l2_src, caps_src, rotate, data.tee, queue, sink, NULL) != TRUE) {
            g_printerr ("Elements could not be linked.\n");
            gst_object_unref (data.pipeline);
            gst_caps_unref(video_source_caps);
            //return;
        }

        //Set filter to appropriate caps specified in GstCaps
        g_object_set(G_OBJECT(caps_src), "caps", video_source_caps, NULL);
        gst_caps_unref(video_source_caps);

        //Here setup rtsp src
        //basically point to gst_element, then do name:value pairs, end on NULL
        g_object_set(v4l2_src, "device", params->dev_path.c_str(), NULL);
        g_object_set(rotate, "method", 2, NULL);

        //add bus
        data.bus = gst_element_get_bus(data.pipeline);
    }
    else {
        v4l2_src = gst_element_factory_make("v4l2src", "source");
        if(!v4l2_src)
            g_printerr("v4l2 source not created!\n");
        decoder = gst_element_factory_make("jpegdec", "jpeg-decoder");
        if(!decoder)
            g_printerr("Could not create decoder!\n");
        queue    = gst_element_factory_make("queue", "queue");
        if(!queue)
            g_printerr("queue not created!\n");

        sink     = gst_element_factory_make("xvimagesink", "sink");
        if(!sink)
            g_printerr("sink not created!\n");

        //note adding caps to an element is pretty easy
        caps_src = gst_element_factory_make("capsfilter", "caps_src");
        caps_sink = gst_element_factory_make("capsfilter", "caps_sink");
        g_assert(caps_src  != NULL);  //always check that the caps are made before we assign the g_object
        g_assert(caps_sink != NULL);

        //this returns a GST_BIN element used to contain other GSTElements
        data.pipeline = gst_pipeline_new ("pipeline");

        //create caps needed
        video_source_caps = gst_caps_new_simple("image/jpeg", "width", G_TYPE_INT, params->size_use.first, "height", G_TYPE_INT, params->size_use.second, NULL);
        video_sink_caps   = gst_caps_new_simple("video/x-raw", "width", G_TYPE_INT, params->size_use.first, "height", G_TYPE_INT, params->size_use.second, NULL);

        //Error checking to ensure that pointers got instantiated
        if (!data.pipeline || !v4l2_src || !queue || !caps_src || !caps_sink || !decoder || !sink || !data.tee) {
            g_printerr ("Not all elements could be created.\n");
            gst_caps_unref(video_source_caps);
            gst_caps_unref(video_sink_caps);
        }

        // Build the pipeline
        gst_bin_add_many (GST_BIN (data.pipeline), v4l2_src, caps_src, caps_sink, decoder, queue, sink, data.tee, NULL);
        if (gst_element_link_many (v4l2_src, caps_src, decoder, caps_sink, data.tee, queue, sink, NULL) != TRUE) {
            g_printerr ("Elements could not be linked.\n");
            gst_object_unref (data.pipeline);
            gst_caps_unref(video_source_caps);
            gst_caps_unref(video_sink_caps);
            //return;
        }

        //Set filter to appropriate caps specified in GstCaps
        g_object_set(G_OBJECT(caps_src), "caps", video_source_caps, NULL);
        g_object_set(G_OBJECT(caps_sink), "caps", video_sink_caps, NULL);
        gst_caps_unref(video_source_caps);
        gst_caps_unref(video_sink_caps);

        //Here setup rtsp src
        //basically point to gst_element, then do name:value pairs, end on NULL
        g_object_set(v4l2_src, "device", params->dev_path.c_str(), "do-timestamp", true, NULL);

        //add bus
        data.bus = gst_element_get_bus(data.pipeline);
    }

    data.loop = g_main_loop_new(NULL, FALSE);

    //For both types we msut setup the window (both will be xvimagesink, or the sink element)
    //QWidget* vid_wig = ui->centralWidget->findChild<QWidget*>("video_widget");
    this->sink_overlay->resize(params->size_use.first, params->size_use.second);       //resize to the size of our stream
    //vid_wig->show();
    WId xwinid = this->sink_overlay->winId();
    g_print("Grabbed window ID of the video widget, video is: %d\n", (int)xwinid);
    gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(sink), xwinid);      //requires pkg-config add of gstreamre-video-1.0

    //Immediately set the pipe to playing
    GstStateChangeReturn ret;
    ret = gst_element_set_state(data.pipeline, GST_STATE_PLAYING);
    if(ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr("UNABLE TO START THE PIPE!\n");
    }
    stream_on_flag = true;

    //Now set main loop
    //g_main_loop_run(data.loop);
}
//queue ! nvvidconv ! omxh264enc ! mp4mux ! filesink location=test.mp4
//To play files, we can't use VLC cause it sucks on arm, use Gstreamer because its good
//gst-launch-1.0 playbin uri=file:///media/chance189/xavier_nx_nvme/ADT_TAKEOVER_LIB/device_0_record.mp4
void pipe_builder::add_recording_branch() {    //remember gpointer is basically just a void pointer
    //Create necessary pointers
    GstPadTemplate *temp1;
    //GstElement* tee;
    GstPad *sinkpad;

    //Check if recording
    if(data.isrecording) {
        g_printerr("CANNOT START ANOTHER RECORDING! ALREADY RECORDING!\n");
        return;
    }

    //If not, create the new tree
    //tee = gst_bin_get_by_name(GST_BIN(data.pipeline), name_tee.c_str());
    if(!data.tee) {
        g_printerr("Unable to grab tee element from pipe!\n");
        return;
    }
    g_print("Grabbed tee from pipe!\n");

    //Add all needed modules for saving as h264 file
    data.queue_rec = gst_element_factory_make("queue", "queue_rec");
    if(!data.queue_rec) {
        g_printerr("Failed to add recording queue!\n");
        return;
    }
    data.nvvidconv_rec = gst_element_factory_make("nvvidconv", "nvvidconv_rec");
    if(!data.nvvidconv_rec) {
        g_printerr("Failed to add recording video convert!\n");
        return;
    }
    data.h264_enc_rec = gst_element_factory_make("omxh265enc", "h264rec_rec");
    if(!data.h264_enc_rec) {
        g_printerr("Failed to add recording encoder!\n");
        return;
    }
    //qtmux, mp4mux, work
    //to convert super quick, just change the mux around and demux, like below
    //gst-launch-1.0 filesrc location=/media/chance189/xavier_nx_nvme/ADT_TAKEOVER_LIB/device_0_record_4_8_21.mp4 ! qtdemux ! mp4mux ! filesink location=/media/chance189/xavier_nx_nvme/ADT_TAKEOVER_LIB/device_0_record_mp4_conv_4_8.mp4
    data.mp4mux = gst_element_factory_make("mp4mux", "mp4mux_rec");         //Note mp4mux is needed for windows, qtmux doesn't work for some reason to play the files
    if(!data.mp4mux) {
        g_printerr("Failed to add recording mux!\n");
        return;
    }
    data.filesink = gst_element_factory_make("filesink", "filesink_rec");
    if(!data.filesink) {
        g_printerr("Failed to add recording filesink!\n");
        return;
    }

    //Now add new source pad to tee
    temp1 = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(data.tee), "src_%u");
    data.teepad_rec = gst_element_request_pad(data.tee, temp1, NULL, NULL);

    //Set all the needed stuff
    string file_path = "/media/chance189/xavier_nx_nvme/ADT_TAKEOVER_LIB/" + myname + "_record.mp4";  //change to .mp4 as needed
    g_object_set(data.filesink, "location", file_path.c_str(), NULL);

    //set h264 encoder settings so video isn't shit
    //quality-control
    //g_object_set(data.h264_enc_rec, "profile", 8, "preset-level", 0, NULL);

    //Try to link all the necessary pads for recording
    gst_bin_add_many (GST_BIN (data.pipeline), data.queue_rec, data.nvvidconv_rec, data.h264_enc_rec, data.mp4mux, data.filesink, NULL);
    if(gst_element_link_many(data.queue_rec, data.nvvidconv_rec, data.h264_enc_rec, data.mp4mux, data.filesink, NULL) != TRUE) {
        //unref all these pads and return
        //Now remove all these elements
        gst_bin_remove(GST_BIN(data.pipeline), data.queue_rec);
        gst_bin_remove(GST_BIN(data.pipeline), data.nvvidconv_rec);
        gst_bin_remove(GST_BIN(data.pipeline), data.h264_enc_rec);
        gst_bin_remove(GST_BIN(data.pipeline), data.mp4mux);
        gst_bin_remove(GST_BIN(data.pipeline), data.filesink);

        gst_object_unref(data.queue_rec);
        gst_object_unref(data.nvvidconv_rec);
        gst_object_unref(data.h264_enc_rec);
        gst_object_unref(data.mp4mux);
        gst_object_unref(data.filesink);
        g_printerr("COULD NOT LINK THE RECORDING PIPE!!!\n");
        return;
    }

    //Now try to link stuff together
    sinkpad = gst_element_get_static_pad(data.queue_rec, "sink");

    GstPadLinkReturn ret = gst_pad_link(data.teepad_rec, sinkpad);
    if(GST_PAD_LINK_FAILED(ret)) {
        g_printerr("Failed to link pads on %s!\n", myname.c_str());
        return;
    }

    //Sync the elemeents
    gst_element_sync_state_with_parent(data.queue_rec);
    gst_element_sync_state_with_parent(data.nvvidconv_rec);
    gst_element_sync_state_with_parent(data.h264_enc_rec);
    gst_element_sync_state_with_parent(data.mp4mux);
    gst_element_sync_state_with_parent(data.filesink);

    gst_object_unref(sinkpad);
    data.isrecording = true;
    g_print("SUCCESSFULLY STARTED RECORDING BRANCH ON %s!!!\n", myname.c_str());
}

void pipe_builder::remove_recording_branch() {
    if(!data.isrecording) {
        g_printerr("NOTHING TO STOP RECORDING!\n");
        return;
    }
    //Let's unlink this pipe!
    //also note we don't want to use (GDestroyNotify) g_free to the last param, that'll destroy our private struct that we need
    gst_pad_add_probe(data.teepad_rec, GST_PAD_PROBE_TYPE_IDLE, unlink_vid_rec_pipe_cb, gpointer (&this->data), NULL );
    data.isrecording = false;
}

//Grabbed from stackexchange
GstPadProbeReturn pipe_builder::unlink_vid_rec_pipe_cb(GstPad* pad, GstPadProbeInfo *info, gpointer user_data) {
    g_print("ENDING THE VIDEO STREAM!\n");
    pipe_elements* data_usr = (pipe_elements*)user_data;
    //GstElement* tee = gst_bin_get_by_name(GST_BIN(data_usr->pipeline), "tee");
    GstPad *sinkpad = gst_element_get_static_pad(data_usr->queue_rec, "sink");
    gst_pad_unlink(data_usr->teepad_rec, sinkpad);
    gst_object_unref(sinkpad);

    //Send the EOS to the encoder first
    gst_element_send_event(data_usr->h264_enc_rec, gst_event_new_eos());     //Kill this vestigial branch

    //Now remove all these elements
    gst_bin_remove(GST_BIN(data_usr->pipeline), data_usr->queue_rec);
    gst_bin_remove(GST_BIN(data_usr->pipeline), data_usr->nvvidconv_rec);
    gst_bin_remove(GST_BIN(data_usr->pipeline), data_usr->h264_enc_rec);
    gst_bin_remove(GST_BIN(data_usr->pipeline), data_usr->mp4mux);
    gst_bin_remove(GST_BIN(data_usr->pipeline), data_usr->filesink);

    //Set all the states to NULL
    gst_element_set_state(data_usr->queue_rec, GST_STATE_NULL);
    gst_element_set_state(data_usr->nvvidconv_rec, GST_STATE_NULL);
    gst_element_set_state(data_usr->h264_enc_rec, GST_STATE_NULL);
    gst_element_set_state(data_usr->mp4mux, GST_STATE_NULL);
    gst_element_set_state(data_usr->filesink, GST_STATE_NULL);

    //Now destroy
    gst_object_unref(data_usr->queue_rec);
    gst_object_unref(data_usr->nvvidconv_rec);
    gst_object_unref(data_usr->h264_enc_rec);
    gst_object_unref(data_usr->mp4mux);
    gst_object_unref(data_usr->filesink);

    //Now release pads
    gst_element_release_request_pad(data_usr->tee, data_usr->teepad_rec);
    gst_object_unref(data_usr->teepad_rec);

    g_print("UNLINKED ALL THE THINGS\n");

    return GST_PAD_PROBE_REMOVE;
}


//Sets the pipeline into paused or ready states
void pipe_builder::handle_play_stop() {
    GstStateChangeReturn ret;
    if(stream_on_flag) {
        ret = gst_element_set_state(data.pipeline, GST_STATE_PAUSED);
        if(ret == GST_STATE_CHANGE_FAILURE) {
            g_printerr("UNABLE TO STOP THE PIPE!!!\n");
        }
        stream_on_flag = false;
    }
    else {
        ret = gst_element_set_state(data.pipeline, GST_STATE_PLAYING);
        if(ret == GST_STATE_CHANGE_FAILURE) {
            g_printerr("UNABLE TO START THE PIPE!\n");
        }
        stream_on_flag = true;
    }
}

void pipe_builder::handle_rec_start() {

}

void pipe_builder::handle_rec_stop() {
    GstStateChangeReturn ret;
    if(stream_on_flag) {
        ret = gst_element_set_state(data.pipeline, GST_STATE_PAUSED);
        if(ret == GST_STATE_CHANGE_FAILURE) {
            g_printerr("UNABLE TO STOP THE PIPE!!!\n");
        }
        stream_on_flag = false;
    }
    else {
        ret = gst_element_set_state(data.pipeline, GST_STATE_PLAYING);
        if(ret == GST_STATE_CHANGE_FAILURE) {
            g_printerr("UNABLE TO START THE PIPE!\n");
        }
        stream_on_flag = true;
    }
}

//This is callback for GST
//For static functions don't redo the static in the source file or the compiler gets angry
void pipe_builder::rtsp_callback_handler(GstElement* src, GstPad *new_pad, pipe_elements* data) {
    GstPad *sink_pad = gst_element_get_static_pad(gst_bin_get_by_name(GST_BIN(data->pipeline), "queue-rtsp"), "sink");
    GstPadLinkReturn ret;
    GstCaps *new_pad_caps = NULL;
    GstStructure *new_pad_struct = NULL;
    const gchar *new_pad_type = NULL;

    g_print("Grabbed new pad '%s' from '%s'.\n", GST_PAD_NAME(new_pad), GST_ELEMENT_NAME(src));

    //Check if we already linked, ignore if it's already linked
    if(gst_pad_is_linked(sink_pad)) {
        g_print("Already linked this pad, exiting.\n");
        goto exit;
    }
    new_pad_caps = gst_pad_get_current_caps(new_pad);
    new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
    new_pad_type = gst_structure_get_name(new_pad_struct);
    // if(!g_str_has_prefix(new_pad_type, "video/x-raw")) {
    //   g_print("Type is '%s' which isn't video.\n", new_pad_type);
    //   goto exit;
    // }

    //attempt the link
    ret = gst_pad_link(new_pad, sink_pad);
    if(GST_PAD_LINK_FAILED(ret)) {
        g_print("Type is %s but link failed\n", new_pad_type);
    }
    else {
        g_print("LInk succeeded (type: %s)\n", new_pad_type);
    }
    exit:
    if(new_pad_caps != NULL)
        gst_caps_unref(new_pad_caps);
    gst_object_unref(sink_pad);
}
