######################################################################
# Automatically generated by qmake (3.0) Thu Feb 25 16:11:28 2021
######################################################################
TEMPLATE = app
TARGET = LLSM_Processing_GUI
QT += widgets
VERSION = 0.4.0.0
DEFINES += VERSION_STRING=\\\"$${VERSION}\\\"

MATLAB_VER_WIN = R2021b
MATLAB_VER_CLUSTER = r2021a

MATLAB_ROOT_WIN = C:/Program Files/MATLAB
MATLAB_ROOT_CLUSTER = /global/software/sl-7.x86_64/modules/tools/matlab

# Input
HEADERS += \
           datapath.h \
           datapaths.h \
           datapathsrecursive.h \
           deconadvanced.h \
           dsradvanced.h \
           jobadvanced.h \
           jobtext.h \
           jobtextmanager.h \
           mainadvanced.h \
           mainwindow.h \
           matlaboutputthread.h \
           matlaboutputwindow.h \
           matlaboutputwindowthread.h \
           matlabthread.h \
           matlabthreadmanager.h \
           loadprevioussettings.h \
           outputbox.h \
           simreconjobadvanced.h \
           simreconmainadvanced.h \
           simreconreconadvanced.h \
           submissionchecks.h

FORMS += \
         datapaths.ui \
         datapathsrecursive.ui \
         deconadvanced.ui \
         dsradvanced.ui \
         jobadvanced.ui \
         jobtext.ui \
         mainadvanced.ui \
         mainwindow.ui \
         matlaboutputwindow.ui \
         loadprevioussettings.ui \
         simreconjobadvanced.ui \
         simreconmainadvanced.ui \
         simreconreconadvanced.ui

SOURCES += \
           datapath.cpp \
           datapaths.cpp \
           datapathsrecursive.cpp \
           deconadvanced.cpp \
           dsradvanced.cpp \
           jobadvanced.cpp \
           jobtext.cpp \
           jobtextmanager.cpp \
           main.cpp \
           mainadvanced.cpp \
           mainwindow.cpp \
           matlaboutputthread.cpp \
           matlaboutputwindow.cpp \
           matlaboutputwindowthread.cpp \
           matlabthread.cpp \
           matlabthreadmanager.cpp \
           loadprevioussettings.cpp \
           outputbox.cpp \
           simreconjobadvanced.cpp \
           simreconmainadvanced.cpp \
           simreconreconadvanced.cpp \
           submissionchecks.cpp

TRANSLATIONS += LLSM_Processing_GUI_en_US.ts

# Remove possible other optimization flags
QMAKE_CXXFLAGS_RELEASE -= -O
QMAKE_CXXFLAGS_RELEASE -= -O1
QMAKE_CXXFLAGS_RELEASE -= -O2

# Add -O3 if not present
QMAKE_CXXFLAGS_RELEASE *= -O3

unix:!macx {
LIBS += "$${MATLAB_ROOT_CLUSTER}/$${MATLAB_VER_CLUSTER}/extern/bin/glnxa64/libMatlabEngine.so" \
        "$${MATLAB_ROOT_CLUSTER}/$${MATLAB_VER_CLUSTER}/extern/bin/glnxa64/libMatlabDataArray.so"

INCLUDEPATH += "$${MATLAB_ROOT_CLUSTER}/$${MATLAB_VER_CLUSTER}/extern/include"
}

win32 {
LIBS += -L"$${MATLAB_ROOT_WIN}/$${MATLAB_VER_WIN}/extern/lib/win64/mingw64" -llibeng \
       -L"$${MATLAB_ROOT_WIN}/$${MATLAB_VER_WIN}/extern/lib/win64/mingw64" -llibmx \
       -L"$${MATLAB_ROOT_WIN}/$${MATLAB_VER_WIN}/extern/lib/win64/mingw64" -llibMatlabEngine \
       -L"$${MATLAB_ROOT_WIN}/$${MATLAB_VER_WIN}/extern/lib/win64/mingw64" -llibMatlabDataArray


INCLUDEPATH += "$${MATLAB_ROOT_WIN}/$${MATLAB_VER_WIN}/extern/include"

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
}
