#include "emuthread.h"

#include <QDebug>
#include <QOpenGLContext>
#include <QSurfaceFormat>

#include "openglwindow.h"
#include "psx/core.h"
#include "psx/renderer/opengl/openglrenderer.h"

EmuThread::EmuThread(QObject *parent, PSX::Core *core)
    : QThread(parent),
      core(core),
      initialized(false),
      openGLWindow(nullptr),
      paused(true) {
}

EmuThread::~EmuThread() {
    delete renderer;
    delete openGLWindow;
}

void EmuThread::pauseEmulation() {
    paused.store(true);
}

bool EmuThread::emulationIsPaused() {
    return paused.load();
}

OpenGLWindow* EmuThread::getOpenGLWindow() {
    return openGLWindow;
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
    openGLWindow->setUpViewport();

    paused.store(false);
    while (!paused.load()) {
        core->emulateUntilVBLANK();
    }

    context->doneCurrent();
}

void EmuThread::initialize() {
    // create OpenGLWindow
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setVersion(3,3);

    openGLWindow = new OpenGLWindow();
    openGLWindow->setFormat(format);
    openGLWindow->resize(640, 480);

    connect(openGLWindow, &OpenGLWindow::closed,
            this, &EmuThread::openGLWindowClosed);

    // create OpenGLRenderer
    renderer = new PSX::OpenGLRenderer(openGLWindow);
    core->setRenderer(renderer);

    initialized = true;
}

