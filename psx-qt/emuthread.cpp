#include "emuthread.h"

#include <QDebug>
#include <QSurfaceFormat>

#include "openglwindow.h"
#include "psx/core.h"
#include "psx/renderer/opengl/openglrenderer.h"

EmuThread::EmuThread(QObject *parent)
    : QThread(parent) {
}

EmuThread::~EmuThread() {
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

void EmuThread::setBiosPath(const QString &biosPath) {
    this->biosPath = biosPath;
}

void EmuThread::run() {
    createWindow();

    PSX::OpenGLRenderer renderer(window);
    PSX::Core core(&renderer);

    core.bus.bios.readFromFile(biosPath.toStdString());

    core.run();
}

