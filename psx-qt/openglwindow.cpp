#include "openglwindow.h"

#include <glad/glad.h>
#include <QOpenGLContext>
#include <QDebug>

QOpenGLContext *OpenGLWindow::currentContext = nullptr;

OpenGLWindow::OpenGLWindow(QWindow *parent)
    : QWindow(parent),
      context(nullptr) {
    setSurfaceType(QWindow::OpenGLSurface);
}

OpenGLWindow::~OpenGLWindow() {
}

void OpenGLWindow::createContext() {
    qDebug() << "Creating OpenGL context";
    context = new QOpenGLContext(this);
    context->setFormat(requestedFormat());
    context->create();
    context->makeCurrent(this);

    qDebug() << "Initializing GLAD";
    currentContext = context;
    auto getProcAddress = [](const char *name) -> QFunctionPointer {
        return currentContext->getProcAddress(name);
    };

    if (!gladLoadGLLoader((GLADloadproc)+getProcAddress)) {
        qDebug() << "Failed to initialize GLAD";
    }
}

void OpenGLWindow::setUpViewport() {
    glViewport(0, 0, width(), height());
}

void OpenGLWindow::swapBuffers() {
    context->swapBuffers(this);
}

bool OpenGLWindow::event(QEvent *event) {
    switch (event->type()) {
        case QEvent::UpdateRequest:
            return true;
        default:
            return QWindow::event(event);
    }
}

void OpenGLWindow::exposeEvent(QExposeEvent *event) {
}
