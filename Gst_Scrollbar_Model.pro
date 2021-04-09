#-------------------------------------------------
#
# Project created by QtCreator 2021-03-24T00:07:50
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = Gst_Scrollbar_Model
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


SOURCES += \
        main.cpp \
        gst_scrollbar_app.cpp \
        pipe_builder.cpp \
    settings_mngr.cpp \
    horizontal_scroll_area.cpp

HEADERS += \
        gst_scrollbar_app.h \
        pipe_builder.h \
        rapidxml-1.13/rapidxml.hpp \
        rapidxml-1.13/rapidxml_iterators.hpp \
        rapidxml-1.13/rapidxml_print.hpp \
        rapidxml-1.13/rapidxml_utils.hpp \
    settings_mngr.h \
    rapidxml-1.13/rapidxml_ext.hpp \
    horizontal_scroll_area.h

FORMS += \
        gst_scrollbar_app.ui \
    add_camera_ui.ui \
    remove_camera_ui.ui

CONFIG += link_pkgconfig
PKGCONFIG += gstreamer-1.0 \
             gstreamer-video-1.0
