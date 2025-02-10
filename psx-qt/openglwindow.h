#ifndef OPENGLWINDOW_H
#define OPENGLWINDOW_H

#include <QWindow>

#include "psx/screen.h"

class QOpenGLContext;

class OpenGLWindow : public QWindow, PSX::Screen {
    Q_OBJECT

public:
    OpenGLWindow(QWindow *parent = nullptr);
    ~OpenGLWindow();

    void createContext();
    void setUpViewport();
    void swapBuffers() override;

protected:
    bool event(QEvent *event) override;
    void exposeEvent(QExposeEvent *event) override;

private:
    QOpenGLContext *context;
    static QOpenGLContext *currentContext;
};

#endif
