#ifndef EMUTHREAD_H
#define EMUTHREAD_H

#include <QThread>

class OpenGLWindow;
class QOpenGLContext;

class EmuThread : public QThread {
    Q_OBJECT

public:
    EmuThread(QObject *parent = nullptr);
    virtual ~EmuThread();

protected:
    void run();

private:
    OpenGLWindow *window;

    void createWindow();
};

#endif
