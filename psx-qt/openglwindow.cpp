#include "openglwindow.h"

#include <glad/glad.h>
#include <QOpenGLContext>

#include <iostream>

#include "psx/util/log.h"

QOpenGLContext *OpenGLWindow::globalContext = nullptr;

OpenGLWindow::OpenGLWindow(QWindow *parent)
    : QWindow(parent),
      resizeRequested(false) {
    setSurfaceType(QWindow::OpenGLSurface);
    show();
}

OpenGLWindow::~OpenGLWindow() {
}

QOpenGLContext* OpenGLWindow::getContext() {
    return globalContext;
}

int OpenGLWindow::getHeight() {
    return devicePixelRatio() * height();
}

int OpenGLWindow::getWidth() {
    return devicePixelRatio() * width();
}

void OpenGLWindow::setUpViewport() {
    int windowWidth = width();
    int windowHeight = height();

    int height = windowHeight;
    int width = (windowHeight / 3) * 4;
    if (width > windowWidth) {
        height = (windowWidth / 4) * 3;
        width = windowWidth;
    }
    int blackBarWidth = (windowWidth - width) / 2;
    int blackBarHeight = (windowHeight - height) / 2;

    const qreal scalingFactor = devicePixelRatio();
    glViewport(blackBarWidth, blackBarHeight, scalingFactor * width, scalingFactor * height);
}

void OpenGLWindow::swapBuffers() {
    //if (resizeRequested.load()) {
    //    setUpViewport();
    //}

    globalContext->swapBuffers(this);
}

void OpenGLWindow::makeContextCurrent() {
    if (globalContext) {
        globalContext->makeCurrent(this);
    }
}

bool OpenGLWindow::isVisible() {
    return QWindow::isVisible();
}

bool OpenGLWindow::event(QEvent *event) {
    switch (event->type()) {
        case QEvent::UpdateRequest:
            // TODO update
            return true;
        default:
            return QWindow::event(event);
    }
}

void OpenGLWindow::exposeEvent(QExposeEvent *event) {
    // TODO update
}

void OpenGLWindow::resizeEvent(QResizeEvent *event) {
    resizeRequested.store(true);
}

void OpenGLWindow::closeEvent(QCloseEvent *event) {
    emit closed();
}

void OpenGLWindow::createContext() {
    if (globalContext != nullptr) {
        return;
    }

    LOG_MISC("Creating OpenGL context");
    globalContext = new QOpenGLContext(this);
    globalContext->setFormat(requestedFormat());
    globalContext->create();
    globalContext->makeCurrent(this);

    LOG_MISC("Initializing GLAD");
    auto getProcAddress = [](const char *name) -> QFunctionPointer {
        return globalContext->getProcAddress(name);
    };

    if (!gladLoadGLLoader((GLADloadproc)+getProcAddress)) {
        LOG_MISC("Failed to initialize GLAD");
    }
}

