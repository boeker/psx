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
    QOpenGLContext* getContext();
    int getHeight() override;
    int getWidth() override;
    void setUpViewport() override;
    void swapBuffers() override;
    void makeContextCurrent() override;
    bool isVisible() override;

signals:
    void closed();

protected:
    bool event(QEvent *event) override;
    void exposeEvent(QExposeEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    static QOpenGLContext *globalContext;

    std::atomic<bool> resizeRequested;
};

#endif
