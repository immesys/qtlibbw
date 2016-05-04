QT += qml quick
CONFIG += qt plugin c++11

SOURCES += \
    $$PWD/bosswave_plugin.cpp \
    $$PWD/bosswave.cpp

HEADERS += \
    $$PWD/bosswave_plugin.h \
    $$PWD/bosswave.h
