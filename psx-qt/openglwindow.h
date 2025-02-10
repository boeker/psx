#ifndef OPENGLWINDOW_H
#define OPENGLWINDOW_H

#include <QWindow>

class OpenGLWindow : public QWindow {
    Q_OBJECT

public:
    OpenGLWindow(QWindow *parent = nullptr);
    ~OpenGLWindow();

protected:
    bool event(QEvent *event) override;
    void exposeEvent(QExposeEvent *event) override;

public:
};

#endif
