#include "openglwindow.h"

#include <glad/glad.h>
#include <QOpenGLContext>

#include <iostream>

#include "psx/util/log.h"

QOpenGLContext *OpenGLWindow::currentContext = nullptr;

OpenGLWindow::OpenGLWindow(QWindow *parent)
    : QWindow(parent),
      context(nullptr),
      resizeRequested(false) {
    setSurfaceType(QWindow::OpenGLSurface);
    show();
    createContext();
}

OpenGLWindow::~OpenGLWindow() {
}

QOpenGLContext* OpenGLWindow::getContext() {
    return context;
}

int OpenGLWindow::getHeight() {
    return height();
}

int OpenGLWindow::getWidth() {
    return width();
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
    if (resizeRequested.load()) {
        setUpViewport();
    }

    context->swapBuffers(this);
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
    LOG_MISC("Creating OpenGL context");
    context = new QOpenGLContext(this);
    context->setFormat(requestedFormat());
    context->create();
    context->makeCurrent(this);

    LOG_MISC("Initializing GLAD");
    currentContext = context;
    auto getProcAddress = [](const char *name) -> QFunctionPointer {
        return currentContext->getProcAddress(name);
    };

    if (!gladLoadGLLoader((GLADloadproc)+getProcAddress)) {
        LOG_MISC("Failed to initialize GLAD");
    }
}

