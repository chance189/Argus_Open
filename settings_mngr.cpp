#include "settings_mngr.h"
#include "ui_add_camera_ui.h"
#include "ui_remove_camera_ui.h"
#include <unordered_map>
#include <map>
#include <string>
#include <QDialog>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <gst/gst.h>
#include <fstream>
#include <vector>
#include "rapidxml-1.13/rapidxml.hpp"
#include "rapidxml-1.13/rapidxml_ext.hpp"
#include "rapidxml-1.13/rapidxml_print.hpp"
#include <QComboBox>
#include <QString>
#include <sstream>  //For the split function
#include <utility>
#include <algorithm>  //For find

using namespace std;
using namespace rapidxml;

//Set our const vars
const string settings_file_name         = "settings.secret";
const string root_node_name             = "camera_list";
const string cam_node_name              = "camera";
const string param_dev_name             = "device_name";
const string param_manufacturer_name    = "manufacturer";
const string param_type_cam             = "type_cam";
const string param_pixel_fmt            = "pixel_fmt_use";
const string param_size_use             = "user_size_select";
const string param_dev_path             = "device_path";
const string camera_types[]               = {"RTSP", "Direct", "Direct-Custom"};

/***
 * Name: constructor
 * Purpose: Setup both QDialogs and hide them
 */
settings_mngr::settings_mngr(QObject *parent) : QObject(parent)
{
    //Set private pointers to NULL
    cam_param_new = NULL;

    //Add the dialogs
    //remove
    rm_diag = new QDialog();
    rm_diag->setWindowTitle(QString("Remove Device"));
    rm_ui = new Ui::remove_camera_ui();
    rm_ui->setupUi(rm_diag);

    //add
    add_diag = new QDialog();
    add_diag->setWindowTitle((QString("Add Device")));
    add_ui = new Ui::add_camera_ui();
    add_ui->setupUi(add_diag);

    for(string type : camera_types) {
        add_ui->Camera_Type_Combo->addItem(QString::fromStdString(type));
    }

    //hide both dialogs till button press
    add_diag->hide();
    rm_diag->hide();

    //Check for XML and build map
    //build_lib_from_xml();

    //Connect all needed signals to the GUIs
    //rm connected
    connect(rm_ui->buttonBox, &QDialogButtonBox::accepted, this, &settings_mngr::cam_rm_onSubmit);
    connect(rm_ui->buttonBox, &QDialogButtonBox::rejected, this, &settings_mngr::cam_rm_onCancel);
    //connect QComboBox with selection

    //add connected
    connect(add_ui->buttonBox, &QDialogButtonBox::accepted, this, &settings_mngr::cam_add_onSubmit);
    connect(add_ui->buttonBox, &QDialogButtonBox::rejected, this, &settings_mngr::cam_add_onCancel);
    connect(add_ui->cam_name_conv, &QComboBox::currentTextChanged, this, &settings_mngr::on_camera_device_selected);
    connect(add_ui->Camera_Type_Combo, &QComboBox::currentTextChanged, this, &settings_mngr::on_camera_type_selection);
    connect(add_ui->pix_format_combo, &QComboBox::currentTextChanged, this, &settings_mngr::on_pix_selected);
    connect(add_ui->size_combo_box, &QComboBox::currentTextChanged, this, &settings_mngr::on_size_selected);

    //Add for when editing is done
    connect(add_ui->pix_format_combo, &QComboBox::editTextChanged, this, &settings_mngr::on_pix_selected_edit);
    connect(add_ui->size_combo_box, &QComboBox::editTextChanged, this, &settings_mngr::on_size_selected_edit);

    //RTSP add
    connect(add_ui->RTSP_Addr_Edit, &QLineEdit::textEdited, this, &settings_mngr::on_rtsp_addr_added);
    connect(add_ui->RTSP_Usr_Edit, &QLineEdit::textEdited, this, &settings_mngr::on_rtsp_usr_added);
    connect(add_ui->RTSP_Pass_Edit, &QLineEdit::textEdited, this, &settings_mngr::on_rtsp_pass_added);
}

settings_mngr::~settings_mngr() {
    delete rm_ui;
    delete rm_diag;

    delete add_ui;
    delete add_diag;

    //Now delete all memory settings from cameras
    if(cam_param_new != NULL)
        delete cam_param_new;

    map<string, cam_params*>::iterator it;
    for(it = camera_avail.begin(); it != camera_avail.end(); it++) {
        if(it->second != NULL)
            delete it->second;
    }
}

void settings_mngr::build_lib_from_xml() {
    //check if settings file exists
    if(!does_settings_exist()) {
        settings_exist_flag = false;        //Grab state so we don't have to search again
        return;
    }

    cam_params* new_camera;
    //Progress with the xml reading! Use rapidxml library
    //Q: Why didn't you use Qt's Xml parser? A: I didnt' find it until after I copied this from stackexchange and I'm lazy.
    //Declare xml variables
    xml_document<> doc;
    xml_node<> * root_node;
    xml_node<>* device_nodes;
    ifstream myFile(settings_file_name);
    vector<char> buffer((istreambuf_iterator<char>(myFile)), istreambuf_iterator<char>());
    buffer.push_back('\0');  //must ensure there is null terminator at end of buffer
    doc.parse<0>(&buffer[0]);

    //Now find root node
    root_node = doc.first_node(root_node_name.c_str());

    //Build our available device lists
    grab_all_gst_devices();

    //Iterate over all saved devices
    for(device_nodes = root_node->first_node(cam_node_name.c_str()); device_nodes; device_nodes = device_nodes->next_sibling()) {
        //Iterate over each node in file
        new_camera = new cam_params;
        for(xml_attribute<> *attr = device_nodes->first_attribute(); attr; attr = attr-> next_attribute()) {
            if(param_dev_name == string(attr->name())) {
                printf("Found attribute: %s, value of: %s\n", attr->name(), attr->value());
                new_camera->name = string(attr->value());
            }
            else if(string(attr->name()) == param_manufacturer_name){
                printf("Found attribute: %s, value of: %s\n", attr->name(), attr->value());
                new_camera->manufacturer = string(attr->value());
            }
            else if(string(attr->name()) == param_type_cam) {
                printf("Found attribute: %s, value of: %s\n", attr->name(), attr->value());
                new_camera->type = string(attr->value());
            }
            else if(string(attr->name()) == param_pixel_fmt) {
                printf("Found attribute: %s, value of: %s\n", attr->name(), attr->value());
                new_camera->pixel_fmt = string(attr->value());
            }
            else if(string(attr->name()) == param_size_use) {
                printf("Found attribute: %s, value of: %s\n", attr->name(), attr->value());
                //always do widthxheight
                new_camera->size_use = size_str_to_pair(string(attr->value()));
            }
            else if(string(attr->name()) == param_dev_path) {
                printf("Found attribute: %s, value of: %s\n", attr->name(), attr->value());
                //always do widthxheight
                new_camera->dev_path = string(attr->value());
            }
            else {
                printf("Unknown attribute to camera device: \"%s\", with value of: \"%s\". Ignoring.\n", attr->name(), attr->value());
            }
        }

        //if RTSP cam, add it
        if(new_camera->type != camera_types[0]) {
            //Here we finish for each, check if the name exists in our available devices, or error out
            if(!search_for_gst_device_path(new_camera->name, new_camera)) {
                printf("Error finding camera name: %s, please check if it is currently added to device.\n", new_camera->name.c_str());
                delete new_camera;
                continue;
            }
        }

        //else we continue to add and emit our signals
        emit create_new_camera(new_camera);
        camera_avail.insert(pair<string, cam_params*>(new_camera->name, new_camera));
    }

    //Now check if we need to build our lib
}

/***
 * Name: does_settings_exist
 * Params:
 *      *None
 * Purpose: returns true if specified file name exists, very small search so use dirent.h
 */
bool settings_mngr::does_settings_exist() {
    //define vars for use
    DIR *dir;
    struct dirent *ent;
    char cwd[PATH_MAX];

    //ensure CWD is grabbed
    if(!(getcwd(cwd, sizeof(cwd)) != NULL))
        return false;

    printf("Current working directory is: %s\n", cwd);

    //now open directory and search for filename
    if((dir = opendir(cwd)) != NULL) {
        //iterate through the file
        while((ent = readdir(dir)) != NULL) {
            if(string(ent->d_name) == settings_file_name) {
                //close the directory
                closedir(dir);
                printf("Found settings file.\n");
                return true;
            }
        }
        printf("Settings file does not exist!\n");
        return false;           //Failed to grab the settings
    }
    else
        return false;
}

/***
 * Name: save_settings
 * Params:
 *      *None
 * Purpose: Called on either onSubmit slots, used for updating XML file,
 *          Note error checking is done before this step
 */
void settings_mngr::save_settings() {
    //all data from GUI will be stored based off of cam_params_new
    //setup the xml
    xml_document<> doc;
    xml_node<> *root;

    //Note, rapidxml doesn't store string, only pointer to it, so vector<char> MUST live as long as this method
    ifstream myFile;
    vector<char> buffer;

    //First check if our settings file exists
    if(settings_exist_flag) {
        //Load the existing settings file
        myFile = ifstream(settings_file_name);
        buffer = vector<char>((istreambuf_iterator<char>(myFile)), istreambuf_iterator<char>());
        buffer.push_back('\0');  //must ensure there is null terminator at end of buffer
        doc.parse<0>(&buffer[0]);

        //so now grab root
        root = doc.first_node(root_node_name.c_str());
        printf("Settings file found, grabbed root.\n");
    }
    else {
        //Have to build my own root
        printf("Settings file not found, making new one!\n");
        root = doc.allocate_node(node_element, root_node_name.c_str());
        doc.append_node(root);
        settings_exist_flag = true;
    }

    //Append to root node
    xml_node<> *new_camera = doc.allocate_node(node_element, cam_node_name.c_str());
    new_camera->append_attribute(doc.allocate_attribute(param_dev_name.c_str(), cam_param_new->name.c_str()));
    new_camera->append_attribute(doc.allocate_attribute(param_manufacturer_name.c_str(), cam_param_new->manufacturer.c_str()));
    new_camera->append_attribute(doc.allocate_attribute(param_type_cam.c_str(), cam_param_new->type.c_str()));
    new_camera->append_attribute(doc.allocate_attribute(param_pixel_fmt.c_str(), cam_param_new->pixel_fmt.c_str()));
    string size_use = size_pair_to_str(cam_param_new->size_use).c_str();
    new_camera->append_attribute(doc.allocate_attribute(param_size_use.c_str(), size_use.c_str()));
    new_camera->append_attribute(doc.allocate_attribute(param_dev_path.c_str(), cam_param_new->dev_path.c_str()));
    printf("Here is output of size to string: %s\n", size_use.c_str());

    root->append_node(new_camera);

    //now save it to a file
    ofstream storeFile(settings_file_name.c_str());
    string xml_string;
    print(back_inserter(xml_string), doc);
    storeFile << xml_string;
    storeFile.close();
}

/***
 * Name: grab_all_gst_devices
 * Params:
 *      *None
 * Purpose: Uses gst-device-monitor to query device information on the platform that is running
 */
void settings_mngr::grab_all_gst_devices() {
    GstDeviceMonitor* monitor = gst_device_monitor_new();
    if(!gst_device_monitor_start(monitor)) {
        g_printerr("Warning, monitor failed!\n");
        return;
    }

    //GList is doubly linked list with data as void* typedef to "gpointer"
    GList* devices = gst_device_monitor_get_devices(monitor);           //Do we need to free this?
    GList* glist_element;

    //map<string, GstDevice*>::iterator it;
    current_devices.clear();            //Wipe all the devices we have

    //Iterate through all the devices on the pointer from devices
    for(glist_element = devices; glist_element; glist_element = glist_element->next) {
        //Let's just go through all these and print out their properties
        //Grab data form GList, consists of "gpointer" which is just void*
        GstDevice* dev = (GstDevice*)glist_element->data;

        //Debug printouts
        printf("\n***** \n\tFound new Device, name is: %s\n", gst_device_get_display_name(dev));
        //printf("Classes of device: %s\n", gst_device_get_device_class(dev));

        //Iterate through the devices classes
        bool isSource = false, isVideo = false;
        //gst_device_get_device_class(*GstDevice) returns char* delimited by '/' char
        string class_str = string((char*)gst_device_get_device_class(dev));     //note g_char is just typedef of char, cast to get rid of compiler errs
        vector<string> classes = split(class_str, '/');                         //Grab list of vectors
        for(int i = 0; i < classes.size(); i++) {                               //iterate over vectors
            if(classes.at(i) == string("Video")) {
                isVideo = true;
            }
            if(classes.at(i) == string("Source")) {
                isSource = true;
            }
        }

        if(!(isSource && isVideo)) {
            printf("Device does not have required elements Video/Source. Elements are: %s\n", class_str.c_str());
            continue;
        }

        //We found a video source device, add it to our list  (Don't free device until later)
        current_devices.insert(pair<string, GstDevice*>(string((char*)gst_device_get_display_name(dev)), dev));
        glist_element->data = NULL;

        //Grab all caps of the device
        GstCaps* caps = gst_device_get_caps(dev);
        g_print("Caps of device: %s\n", gst_caps_to_string(caps));
        //gst caps features
        g_print("Here are gst_cap_features: %s\n", gst_caps_features_to_string(gst_caps_get_features(caps, 0)));
        gst_caps_unref(caps);
        g_print("Here are the device properties:\n");
        GstStructure* dev_properties = gst_device_get_properties(dev);              //Grab properties
        g_print("Gst structure as string: %s\n", gst_structure_to_string(dev_properties));

        //Free all devices
        gst_structure_free(dev_properties);
        g_list_free(devices);
    }
}

vector<string> settings_mngr::grab_pixel_formats_from_device(GstDevice* dev) {
    GstCaps* caps = gst_device_get_caps(dev);
    //gst cap features are a dead end, look into parsing gstcaps
    vector<string> strings;
    int num_caps = gst_caps_get_size(caps);
    for(int i = 0; i < num_caps; i++) {
        GstStructure* caps_at_index = gst_caps_get_structure(caps, i);      //Don't need to free this pointer, it belongs to caps
        //******
        //Debugging stuff
        //g_print("Grabbed caps at index: %d\nName %s\nValue: %s\n", i, gst_structure_get_name(caps_at_index), gst_structure_to_string(caps_at_index));
        //Attempt to grab field
        //Have to convert GValue to string, GValue is struct and can contain any data type apparently
        //refer to developer.gnome.org/gobject/stable/gobject-Generic-values.html
        //g_print("Getting value format: %s\n", g_value_get_string(gst_structure_get_value(caps_at_index, "format")));
        string structure_name = string(gst_structure_get_name(caps_at_index));

        //Check type
        if(structure_name == "video/x-raw" || structure_name == "video/x-raw(memory:NVMM)") {
            //Here append the string that is name
            string format = g_value_get_string(gst_structure_get_value(caps_at_index, "format"));
            if(find(strings.begin(), strings.end(), format) == strings.end()) {   //if DNE, then add
                strings.push_back(format);
                printf("Adding to our vector: %s\n", format.c_str());
            }
        }
        else {
            if(find(strings.begin(), strings.end(), structure_name) == strings.end()) {
                strings.push_back(structure_name);
            }
        }
    }
    gst_caps_unref(caps);
    return strings;
}

vector<string> settings_mngr::grab_sizes_as_string(GstDevice* dev) {
    GstCaps* caps = gst_device_get_caps(dev);
    //gst cap features are a dead end, look into parsing gstcaps
    vector<string> strings;
    int num_caps = gst_caps_get_size(caps);
    for(int i = 0; i < num_caps; i++) {
        GstStructure* caps_at_index = gst_caps_get_structure(caps, i);      //Don't need to free this pointer, it belongs to caps
        //We know these devices are video sources, so we know to look height and width, must ensure pix format is conserved
        string caps_name = gst_structure_get_name(caps_at_index);
        if(caps_name == "video/x-raw" || caps_name == "video/x-raw(memory:NVMM)") {
            string format = g_value_get_string(gst_structure_get_value(caps_at_index, "format"));
            if(format == cam_param_new->pixel_fmt) {
                string adder = to_string(g_value_get_int(gst_structure_get_value(caps_at_index, "width"))) + "x" + to_string(g_value_get_int(gst_structure_get_value(caps_at_index, "height")));
                if(find(strings.begin(), strings.end(), adder) == strings.end()) {   //if DNE, then add
                    strings.push_back(adder);
                }
            }
        }
        else {
            if(cam_param_new->pixel_fmt == caps_name) {
                string adder = to_string(g_value_get_int(gst_structure_get_value(caps_at_index, "width"))) + "x" + to_string(g_value_get_int(gst_structure_get_value(caps_at_index, "height")));
                if(find(strings.begin(), strings.end(), adder) == strings.end()) {   //if DNE, then add
                    strings.push_back(adder);
                }
            }
        }
    }

    return strings;
}

bool settings_mngr::search_for_gst_device_path(string name_device, cam_params* new_cam) {
    map<string, GstDevice*>::iterator it = current_devices.find(name_device);
    if(it == current_devices.end()) {
        printf("Error, Check if device name was added correctly to the temporary object.\n");
        return false;
    }

    GstStructure* dev_properties = gst_device_get_properties(it->second);
    new_cam->dev_path = string(g_value_get_string(gst_structure_get_value(dev_properties, "device.path")));
    gst_structure_free(dev_properties);
    return true;
}

//Accessor function
map<string, cam_params*>* settings_mngr::get_cameras() {
    return &camera_avail;
}

vector<string> settings_mngr::split(string &s, char del) {
    vector<string> tokens;
    string token;
    istringstream tokenStream(s);
    while(getline(tokenStream, token, del)) {
        tokens.push_back(token);
    }
    return tokens;
}

string settings_mngr::size_pair_to_str(pair<int, int> p) {
    return to_string(p.first) + "x" + to_string(p.second);
}

pair<int, int> settings_mngr::size_str_to_pair(string p) {
    vector<string> split_vals = split(p, 'x');
    int x,y;

    //Do error checking
    if(split_vals.size() != 2)
        return pair<int, int>(640, 480);

    y = stoi(split_vals.back());
    split_vals.pop_back();
    x = stoi(split_vals.back());
    printf("Converted string %s into x: %d, y: %d\n", p.c_str(), x, y);
    return pair<int, int>(x,y);
}

void settings_mngr::reset_add_settings() {
    add_ui->cam_name_conv->hide();
    add_ui->cam_name_label->hide();
    add_ui->RTSP_Addr_Edit->hide();
    add_ui->rtsp_addr_label->hide();
    add_ui->rtsp_pass_label->hide();
    add_ui->RTSP_Pass_Edit->hide();
    add_ui->RTSP_Usr_Edit->hide();
    add_ui->rtsp_usr_addr->hide();
    add_ui->size_combo_box->hide();
    add_ui->size_label->hide();
    add_ui->pix_format_combo->hide();
    add_ui->pix_format_label->hide();
}

//Slots WYSWYG
void settings_mngr::cam_rm_onSubmit() {
    //Delete the thing
}

void settings_mngr::cam_rm_onCancel() {
    rm_diag->hide();
}

//when we call the show to the widget, we must
void settings_mngr::cam_rm_show() {
    rm_ui->cam_name_combo->clear();         //clear all entries
    int cntr = 0;
    //Now add all existing entries
    map<string, cam_params*>::iterator it;
    for(it = camera_avail.begin(); it != camera_avail.end(); it++) {
        rm_ui->cam_name_combo->addItem(QString(it->second->name.c_str()));
        cntr++;
    }

    if(!cntr)
        rm_ui->cam_name_combo->addItem("No devices are currently saved.\n");

    rm_diag->show();
}

//Here we just call save_settings
void settings_mngr::cam_add_onSubmit() {
    //We have submitted, add device path
    if(add_ui->Camera_Type_Combo->currentText().toStdString() == camera_types[0]) {
        //rtsp full path add
        cam_param_new->name = "RTSP_Camera";
        cam_param_new->pixel_fmt = add_ui->pix_format_combo->currentText().toStdString();
        cam_param_new->size_use = size_str_to_pair(add_ui->size_combo_box->currentText().toStdString());
        cam_param_new->dev_path = "rtsp://" + add_ui->RTSP_Usr_Edit->text().toStdString() + ":" + add_ui->RTSP_Pass_Edit->text().toStdString() + "@" + add_ui->RTSP_Addr_Edit->text().toStdString();
    }
    else {
        if(!search_for_gst_device_path(cam_param_new->name, cam_param_new)) {
            return;
        }
        printf("Path to device is: %s\n", cam_param_new->dev_path.c_str());
    }

    //Add it now to our map of devices
    camera_avail.insert(pair<string, cam_params*>(cam_param_new->name, cam_param_new));
    emit create_new_camera(cam_param_new);

    //Save completed settings
    save_settings();

    //We added it to map, unref our pointer
    cam_param_new = NULL;
    add_diag->hide();
}

void settings_mngr::cam_add_onCancel() {
    add_diag->hide();
}

void settings_mngr::cam_add_show() {
    //Hide all the info save for the type indicator
    reset_add_settings();
    add_diag->show();
}

void settings_mngr::on_camera_type_selection(QString text) {
    //Only have two possible selections
    if(cam_param_new != NULL) {
        delete cam_param_new;
        cam_param_new = NULL;
    }

    printf("Attempting to create new params.\n");
    cam_param_new = new cam_params;

    printf("New params created!\n");
    reset_add_settings();

    //RTSP
    if(QString(camera_types[0].c_str()) == text) {
        cam_param_new->type = camera_types[0];

        //clear all values in other used comboboxes
        add_ui->pix_format_combo->clear();
        add_ui->size_combo_box->clear();

        //Setup all RTSP ones
        add_ui->rtsp_addr_label->show();
        add_ui->RTSP_Addr_Edit->show();
        add_ui->rtsp_usr_addr->show();
        add_ui->RTSP_Usr_Edit->show();
        add_ui->RTSP_Addr_Edit->show();
        add_ui->rtsp_pass_label->show();
        add_ui->RTSP_Pass_Edit->show();
        add_ui->pix_format_label->show();
        add_ui->pix_format_combo->show();
        add_ui->pix_format_combo->setEditable(true);
        add_ui->size_label->show();
        add_ui->size_combo_box->show();
        add_ui->size_combo_box->setEditable(true);
    }
    else if(QString(camera_types[1].c_str()) == text) {
        cam_param_new->type = camera_types[1];

        //Setup all for device used
        grab_all_gst_devices();
        add_ui->cam_name_conv->clear();
        int i = 0;
        map<string, GstDevice*>::iterator it;
        for(it = current_devices.begin(); it != current_devices.end(); it++) {
            add_ui->cam_name_conv->addItem(QString(gst_device_get_display_name(it->second)));
            i++;
        }
        add_ui->cam_name_label->show();
        add_ui->cam_name_conv->show();
        add_ui->cam_name_conv->setEditable(false);

        //check how many items we have
        if(i >= 2) {
            return;
        }
        //Only one device, just auto add the next param
        else if(i == 1) {
            add_ui->pix_format_label->show();
            add_ui->pix_format_combo->show();
            add_ui->pix_format_combo->setEditable(false);
            add_ui->pix_format_combo->clear();

            //grab the pixel formats supported (like UYVY, ect)
            string search = add_ui->cam_name_conv->itemText(0).toStdString();
            cam_param_new->name = search;
            printf("This is what is printed: %s\n", search.c_str());
            vector<string> pixel_types = grab_pixel_formats_from_device(current_devices.find(search)->second);
            for(int j = 0; j < pixel_types.size(); j++) {
                add_ui->pix_format_combo->addItem(QString(pixel_types.at(j).c_str()));
            }
        }
        else {
            add_ui->cam_name_conv->addItem(QString("No Cameras Found on Device"));
        }
    }
    else {
        printf("Error: Unknown new text in add_camera->type_selection.\n");
    }
}

//Set to device selection, grab the device name and add it to the name
void settings_mngr::on_camera_device_selected(QString text) {
    vector<string> pix_types;
    string selected = text.toStdString();

    map<string, GstDevice*>::iterator it = current_devices.find(selected);
    if(it == current_devices.end()) {
        printf("Error, Somehow selected wrong device. Is QComboBox Editable?\n");
        return;
    }

    printf("\n****Debug: selected new Camera Device: %s\n", selected.c_str());
    cam_param_new->name = selected;
    //cam_param_new->dev = it->second;
    add_ui->pix_format_label->show();
    add_ui->pix_format_combo->show();
    add_ui->pix_format_combo->setEditable(false);

    //Otherwise we got a hit
    pix_types = grab_pixel_formats_from_device(it->second);
    for(int j = 0; j < pix_types.size(); j++) {
        add_ui->pix_format_combo->addItem(QString(pix_types.at(j).c_str()));
    }
}

//For devices
void settings_mngr::on_size_selected(QString text) {
    string size_use = text.toStdString();
    cam_param_new->size_use = size_str_to_pair(size_use);

    printf("Debug: Selected size to use as: %s\n", size_use.c_str());
}
void settings_mngr::on_size_selected_edit(QString text) {

}
void settings_mngr::on_pix_selected_edit(QString text) {
}

//For device selection
void settings_mngr::on_pix_selected(QString text) {
    //Okay so now we have the name, and the pixel format
    //Since we populate and don't allow editing here, don't do error checking
    printf("\n****Debug, pixel format was selected for device.\n");
    string pix_val = text.toStdString();
    cam_param_new->pixel_fmt = pix_val;

    //Grab value from name
    map<string, GstDevice*>::iterator it = current_devices.find(cam_param_new->name);
    if(it == current_devices.end()) {
        printf("Error, Check if device name was added correctly to the temporary object.\n");
        return;
    }

    vector<string> size_vector = grab_sizes_as_string(it->second);

    //Clear all data then add to sizes we have
    add_ui->size_combo_box->clear();
    for(int i = 0; i < size_vector.size(); i++) {
        add_ui->size_combo_box->addItem(QString(size_vector.at(i).c_str()));
    }

    add_ui->size_label->show();
    add_ui->size_combo_box->show();
    add_ui->size_combo_box->setEditable(false);
}

void settings_mngr::on_rtsp_addr_added(QString text) {

}

void settings_mngr::on_rtsp_usr_added(QString text) {

}

void settings_mngr::on_rtsp_pass_added(QString text) {

}
