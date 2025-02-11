#include "emuthread.h"

#include <QDebug>
#include <QSurfaceFormat>

#include "openglwindow.h"
#include "psx/core.h"
#include "psx/gl/glrender.h"

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

void EmuThread::run() {
    createWindow();

    PSX::Core core;
    core.bus.gpu.setScreen(window);

    PSX::GLRender render;

    while (true) {
        render.draw();
        window->swapBuffers();
    }

    //core.run();
}

