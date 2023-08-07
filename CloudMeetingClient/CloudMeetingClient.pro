QT       += core gui multimedia multimediawidgets network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    AudioInput.cpp \
    AudioOutput.cpp \
    MyVideoSurface.cpp \
    NetHeader.cpp \
    Partner.cpp \
    RecvDeal.cpp \
    SendImg.cpp \
    SendText.cpp \
    TcpSock.cpp \
    login.cpp \
    main.cpp \
    register.cpp \
    widget.cpp

HEADERS += \
    AudioInput.h \
    AudioOutput.h \
    MyVideoSurface.h \
    NetHeader.h \
    Partner.h \
    RecvDeal.h \
    SendImg.h \
    SendText.h \
    TcpSock.h \
    login.h \
    register.h \
    widget.h

FORMS += \
    login.ui \
    register.ui \
    widget.ui

LIBS += -lz

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    res.qrc
