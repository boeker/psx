#ifndef EMUTHREAD_H
#define EMUTHREAD_H

#include <QThread>
#include <glad/glad.h>
#include <QOpenGLContext>
//#include <QOpenGLFunctions>
//#include <QOpenGLVertexArrayObject>
//#include <QOpenGLBuffer>
//#include <QOpenGLShaderProgram>

class OpenGLWindow;
class QOpenGLContext;

class EmuThread : public QThread {
    Q_OBJECT

public:
    EmuThread(QObject *parent = nullptr);
    virtual ~EmuThread();

    static QFunctionPointer getProcAddress(const char *procName) {
        return openglContext->getProcAddress(procName);
    }

protected:
    void run();

private:
    OpenGLWindow *window;
    static QOpenGLContext *openglContext;

    //QOpenGLVertexArrayObject vao;
    //QOpenGLBuffer vbo;
    //QOpenGLShaderProgram *program;

    void createWindow();
};

#endif
