# ============================================================================
#  einstein_gui.pro —— 爱恩斯坦棋 Qt 图形版
#
#  core.cpp 里没有任何 Qt 代码（规则 / AI / 存档），
#  其余三个文件才是界面层。
# ============================================================================

QT += widgets

CONFIG += c++17
TEMPLATE = app
TARGET = einstein_gui

SOURCES += \
    core.cpp \
    boardwidget.cpp \
    mainwindow.cpp \
    main.cpp

HEADERS += \
    core.h \
    boardwidget.h \
    mainwindow.h
