#include "openglwindow.h"

#include <glad/glad.h>
#include <QOpenGLContext>

#include <iostream>

#include "psx/util/log.h"

QOpenGLContext *OpenGLWindow::globalContext = nullptr;

OpenGLWindow::OpenGLWindow(QWindow *parent)
    : QWindow(parent) {
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

void OpenGLWindow::swapBuffers() {
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

