#-------------------------------------------------
#
# Project created by QtCreator 2014-05-21T10:59:43
#
#-------------------------------------------------

QT       += core gui opengl

TARGET = Plater
TEMPLATE = app

RC_FILE += gui.rc

SOURCES += main.cpp\
    mainwindow.cpp \
    partslist.cpp \
    worker.cpp \
    viewer.cpp \
    wizard.cpp \
    platesviewer.cpp \
    about.cpp

HEADERS  += mainwindow.h \
    partslist.h \
    worker.h \
    viewer.h \
    wizard.h \
    platesviewer.h \
    about.h

FORMS    += mainwindow.ui \
    wizard.ui \
    platesviewer.ui \
    about.ui

CONFIG += c++11

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../plater/build/ -llibplater
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../plater/build/ -llibplater
else:symbian: LIBS += -llibplater
else:unix: LIBS += -L$$PWD/../plater/build/ -llibplater

INCLUDEPATH += $$PWD/../plater
DEPENDPATH += $$PWD/../plater

win32:CONFIG(release, debug|release): PRE_TARGETDEPS += $$PWD/../plater/build/libplater.lib
else:win32:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$PWD/../plater/build/libplater.lib
else:unix:!symbian: PRE_TARGETDEPS += $$PWD/../plater/build/liblibplater.a

win32: LIBS += -lopengl32 -lglu32
mac: LIBS += -framework OpenGL
else:unix: LIBS += -lGLU


RESOURCES += \
    icons.qrc

unix: target.path = /usr/local/bin/
unix: INSTALLS += target
