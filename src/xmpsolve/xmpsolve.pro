# Project file for xmpsolve. Please note that this file assumes that you have
# compiled MPSolve in mpsolve-x.y.z/_build. 
#
# To accomplish this you can just enter the mpsolve-x.y.z folder and then
# $ mkdir _build
# $ cd _build 
# $ ../configure && make

TEMPLATE = app

QT       += core gui
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

#
# This section addresses Android specific configuration. This is a bit atypical,
# w.r.t other platforms, since you will probably have to compile a copy of GMP
# and a separate version of libmps. Those will need to be installed in a different
# directory and then xmpsolve will be linked against them.
#
android {

    CONFIG += mobility
    MOBILITY =

    # Customize this to match your current setup. This way the setup points to a directory at the
    # same level of mpsolve-x.y.z.
    ANDROID_ROOT = $${PWD}/../../../android-ext

    # We need -DMPS_USE_BUILTIN_COMPLEX since Android uses tiny bionic without complex
    # arithmetic support.
    QMAKE_CXXFLAGS += -I$${ANDROID_ROOT}/include \
        -include $${PWD}/../../_build/config.h \
        -DMPS_USE_BUILTIN_COMPLEX

    # Link against locally compiled libmps and libgmp.
    LIBS += $${ANDROID_ROOT}/lib/libmps.a $${ANDROID_ROOT}/lib/libgmp.a

}

!android {
    QMAKE_CXXFLAGS += -include $${PWD}/../../_build/config.h
    INCLUDEPATH += $${PWD}/../../include/
    LIBS += $${PWD}/../../_build/src/libmps/.libs/libmps.so -lgmp
}

# Input
HEADERS += ./mainwindow.h \
           ./mpsolveworker.h \
           ./polynomialsolver.h \
           ./root.h \
           ./rootsrenderer.h \
	   ./polynomialparser.h \
	   ./monomial.h \
	   ./polynomial.h \
           ./rootsmodel.h \
	   ./polsyntaxhighlighter.h \  
           ./polfileeditor.h \
           ./polfileeditorwindow.h

FORMS += ./mainwindow.ui \
        ./polfileeditor.ui \
        ./polfileeditorwindow.ui

SOURCES += ./main.cpp \
           ./mainwindow.cpp \
           ./mpsolveworker.cpp \
           ./polynomialsolver.cpp \
           ./root.cpp \
           ./rootsrenderer.cpp \
           ./polynomialparser.cpp \
           ./monomial.cpp \
	   ./polynomial.cpp \
	   ./rootsmodel.cpp \
	   ./polsyntaxhighlighter.cpp \
           ./polfileeditor.cpp \
           ./polfileeditorwindow.cpp

RESOURCES += \
    resources.qrc