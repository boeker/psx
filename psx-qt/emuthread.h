#ifndef EMUTHREAD_H
#define EMUTHREAD_H

#include <QThread>
#include <QOpenGLFunctions>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>

class OpenGLWindow;

class EmuThread : public QThread {
    Q_OBJECT

public:
    EmuThread(QObject *parent = nullptr);
    virtual ~EmuThread();

protected:
    void run();

private:
    OpenGLWindow *window;
    QOpenGLContext *openglContext;

    QOpenGLVertexArrayObject vao;
    QOpenGLBuffer vbo;
    QOpenGLShaderProgram *program;

    void createWindow();
};

#endif
