/*****
 * Author: Chance Reimer
 * Purpose: Class is tied to a add camera window, or a remove camera window. If adding a camera that is not RTSP,
 *          then it uses V4l2-ctl and other file names to grab data as needed to fill combo boxes.
 *          Note, on program start, will look for a settings file that will be saved to local dir. If not found,
 *          then program is started normally. All user settings will be stored to this file.
 *          cmds of interest are:
 * v4l2-ctl  --device=/dev/video<num> --list-formats-ext
 * GstDeviceMonitor is bae
 */

#ifndef SETTINGS_MNGR_H
#define SETTINGS_MNGR_H

#include <QObject>
#include <QDialog>
#include <unordered_map>
#include <map>
#include <string>
#include <vector>
#include <utility>
#include <gst/gst.h>

//Add headers to our QWidget layouts
//Each has a getter fuction to grab elements off the user interface

using namespace std;
extern const string settings_file_name;
extern const string root_node_name;
extern const string cam_node_name;
extern const string param_dev_name;
extern const string param_manufacturer_name;
extern const string param_type_cam;
extern const string param_pixel_fmt;
extern const string param_size_use;
extern const string param_dev_path;
extern const string camera_types[];

enum cam_type {
    RTSP = 0,
    DIRECT
};


namespace Ui {
class add_camera_ui;
class remove_camera_ui;
}

//ensure using correct namespace for maps
using namespace std;

//struct idea, must pass all necessary information from settings mngr to main thread to build the pipe
//Idea is to use only the needed params to build camera with smallest "footprint"
//      * name : just name of device, maybe used for QTooltip
//      * manufactureur: eh?
//      * type : RTSP or direct connect,
//      * pixel_fmt: All the supported types
//      * size_use: selected width + height
//      * dev_path: source to camera, if device it will be some filename path, if rtsp, will be address to use
typedef struct _cam_params {
    string name;
    string manufacturer;
    string type;
    string pixel_fmt;
    pair<int, int> size_use;
    string dev_path;
} cam_params;

class settings_mngr : public QObject
{
    Q_OBJECT
public:
    explicit settings_mngr(QObject *parent = nullptr);
    ~settings_mngr();

    /***
     * Name: build_lib_from_xml
     * Params:
     *      *None
     * Purpose: search for XML file in local dir, if not found then continue
     *          if found, load XML file into unor
     */
    void build_lib_from_xml();

    /***
     * Name: does_settings_exist
     * Params:
     *      *None
     * Purpose: returns true if specified file name exists
     */
    bool does_settings_exist();

    /***
     * Name: save_settings
     * Params:
     *      *None
     * Purpose: Called on either onSubmit slots, used for updating XML file
     */
    void save_settings();

    void grab_all_gst_devices();
    void grab_sizes_for_device();
    vector<string> grab_pixel_formats_from_device(GstDevice* dev);
    vector<string> grab_sizes_as_string(GstDevice* dev);
    bool search_for_gst_device_path(string name_device, cam_params* new_cam);

    //Accessor function
    map<string, cam_params*>* get_cameras();
    vector<string> split(string& s, char del);
    string size_pair_to_str(pair<int, int> p);
    pair<int, int> size_str_to_pair(string p);
    void reset_add_settings();

signals:
    //Signals WYSWYG
    void error_loading_camera(QString name_of_camera);
    void create_new_camera(cam_params* new_cam);

public slots:
    //Slots WYSWYG
    void cam_rm_onSubmit();
    void cam_rm_onCancel();
    void cam_rm_show();
    void cam_add_onSubmit();
    void cam_add_onCancel();
    void cam_add_show();
    void on_camera_type_selection(QString text);
    void on_camera_device_selected(QString text);
    void on_size_selected(QString text);
    void on_size_selected_edit(QString text);
    void on_pix_selected_edit(QString text);
    void on_pix_selected(QString text);
    void on_rtsp_addr_added(QString text);
    void on_rtsp_usr_added(QString text);
    void on_rtsp_pass_added(QString text);

private:
    map<string, cam_params*> camera_avail;
    map<string, GstDevice*> current_devices;
    cam_params* cam_param_new;
    Ui::remove_camera_ui *rm_ui;
    Ui::add_camera_ui *add_ui;
    QDialog* rm_diag;
    QDialog* add_diag;
    bool settings_exist_flag;
};

#endif // SETTINGS_MNGR_H
