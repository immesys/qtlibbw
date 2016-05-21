QT += qml quick
CONFIG += qt plugin c++11

INCLUDEPATH += $$PWD 

SOURCES += \
    $$PWD/bosswave_plugin.cpp \
    $$PWD/bosswave.cpp \
    $$PWD/libbw.cpp \
    $$PWD/agentconnection.cpp \
    $$PWD/message.cpp

HEADERS += \
    $$PWD/bosswave_plugin.h \
    $$PWD/bosswave.h \
    $$PWD/libbw.h \
    $$PWD/utils.h \
    $$PWD/agentconnection.h \
    $$PWD/allocations.h \
    $$PWD/message.h

include($$PWD/vendor/qmsgpack/qmsgpack.pri)
