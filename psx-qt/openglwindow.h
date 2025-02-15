#ifndef OPENGLWINDOW_H
#define OPENGLWINDOW_H

#include <atomic>
#include <QWindow>

#include "psx/renderer/screen.h"

class QOpenGLContext;

class OpenGLWindow : public QWindow, public PSX::Screen {
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
    void resizeEvent(QResizeEvent *event) override;

private:
    QOpenGLContext *context;
    static QOpenGLContext *currentContext;

    std::atomic<bool> resizeRequested;
};

#endif
