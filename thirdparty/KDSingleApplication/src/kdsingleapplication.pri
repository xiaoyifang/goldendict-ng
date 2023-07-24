QT += core network
CONFIG += c++17
INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD

SOURCES += $$files($$PWD/*.cpp)
HEADERS += $$files($$PWD/*.h)

DEFINES += KDSINGLEAPPLICATION_STATIC_BUILD

win32:LIBS += -lkernel32
