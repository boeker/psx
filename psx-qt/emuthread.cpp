#include "emuthread.h"

#include <QDebug>
#include <QOpenGLContext>
#include <QSurfaceFormat>

#include "openglwindow.h"
#include "psx/core.h"
#include "psx/renderer/opengl/openglrenderer.h"

EmuThread::EmuThread(QObject *parent)
    : QThread(parent),
      window(nullptr),
      renderer(nullptr),
      core(nullptr),
      initialized(false),
      paused(true) {
}

EmuThread::~EmuThread() {
    delete core;
    delete renderer;
    delete window;
}

void EmuThread::createWindow() {
	QSurfaceFormat format;
	format.setRenderableType(QSurfaceFormat::OpenGL);
	format.setProfile(QSurfaceFormat::CoreProfile);
	format.setVersion(3,3);

    window = new OpenGLWindow();
    window->setFormat(format);
	window->resize(640, 480);
    window->show();

    window->createContext();
    window->setUpViewport();
}

void EmuThread::createPSXCore() {
    renderer = new PSX::OpenGLRenderer(window);
    core = new PSX::Core(renderer);
    core->bus.bios.readFromFile(biosPath.toStdString());
}

void EmuThread::setBiosPath(const QString &biosPath) {
    this->biosPath = biosPath;
}

void EmuThread::pauseEmulation() {
    paused.store(true);
}

bool EmuThread::emulationIsPaused() {
    return paused.load();
}

void EmuThread::run() {
    if (!initialized) {
        createWindow();
        createPSXCore();
        initialized = true;
    }

    window->context->makeCurrent(window);
    paused.store(false);
    while (!paused.load()) {
        core->emulateUntilVBLANK();
    }
    window->context->doneCurrent();
}

