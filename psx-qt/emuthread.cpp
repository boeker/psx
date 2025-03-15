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
      vramOpenGLWindow(nullptr),
      paused(true) {
}

EmuThread::~EmuThread() {
    delete renderer;
    delete openGLWindow;
    delete vramOpenGLWindow;
}

void EmuThread::pauseEmulation() {
    paused.store(true);
}

bool EmuThread::emulationIsPaused() {
    return paused.load();
}

void EmuThread::setOpenGLWindow(OpenGLWindow *openGLWindow) {
    this->openGLWindow = openGLWindow;
}

OpenGLWindow* EmuThread::getOpenGLWindow() {
    return openGLWindow;
}

OpenGLWindow* EmuThread::getVRAMWindow() {
    return vramOpenGLWindow;
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

    //openGLWindow = new OpenGLWindow();
    //openGLWindow->setFormat(format);
    //openGLWindow->resize(640, 480);

    openGLWindow->createContext();

    //connect(openGLWindow, &OpenGLWindow::closed,
    //        this, &EmuThread::openGLWindowClosed);

    vramOpenGLWindow = new OpenGLWindow();
    vramOpenGLWindow->setFormat(format);
    vramOpenGLWindow->resize(1024, 512);

    // create OpenGLRenderer
    renderer = new PSX::OpenGLRenderer(openGLWindow, vramOpenGLWindow);
    core->setRenderer(renderer);

    initialized = true;

    emit initializedWindows();
}

