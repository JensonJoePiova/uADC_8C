QT += serialbus serialport widgets
requires(qtConfig(combobox))

TARGET = modbusmaster
TEMPLATE = app
CONFIG += c++11
greaterThan(QT_MAJOR_VERSION, 4):QT +=widgets printsupport

SOURCES += main.cpp\
        mainwindow.cpp \
        settingsdialog.cpp \
        writeregistermodel.cpp \
    qcustomplot.cpp \
    axistag.cpp

HEADERS  += mainwindow.h \
         settingsdialog.h \
        writeregistermodel.h \
    qcustomplot.h \
    axistag.h

FORMS    += mainwindow.ui \
         settingsdialog.ui

RESOURCES += \
    master.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/serialbus/modbus/master
INSTALLS += target
