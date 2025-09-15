#include "emuthread.h"

#include <QDebug>
#include <QOpenGLContext>
#include <QSurfaceFormat>

#include "mainwindow.h"
#include "openglwindow.h"
#include "psx/core.h"
#include "psx/renderer/opengl/openglrenderer.h"

EmuThread::EmuThread(QObject *parent)
    : QThread(parent),
      initialized(false),
      openGLWindow(nullptr),
      vramOpenGLWindow(nullptr),
      paused(true),
      justOneStep(false) {
}

EmuThread::~EmuThread() {
}

void EmuThread::pauseEmulation() {
    paused.store(true);
}

bool EmuThread::emulationIsPaused() {
    return paused.load();
}

void EmuThread::setJustOneStep(bool justOneStep) {
    this->justOneStep = justOneStep;
}

void EmuThread::setOpenGLWindow(OpenGLWindow *window) {
    this->openGLWindow = window;
}

void EmuThread::setVRAMOpenGLWindow(OpenGLWindow *window) {
    this->vramOpenGLWindow = window;
}

void EmuThread::openGLWindowClosed() {
    emit emulationShouldStop();
}

void EmuThread::run() {
    if (!initialized) {
        initialize();
    }

    openGLWindow->show();
    QOpenGLContext *context = openGLWindow->getContext();
    context->makeCurrent(openGLWindow);


    if (justOneStep) {
        justOneStep = false;
        core->emulateStep();

    } else {
        paused.store(false);
        while (!paused.load()) {
            core->emulateUntilVBLANK();
        }
    }

    context->doneCurrent();
}

void EmuThread::initialize() {
    openGLWindow->createContext();
    renderer->initialize();
    initialized = true;
}

