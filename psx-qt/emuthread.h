#ifndef EMUTHREAD_H
#define EMUTHREAD_H

#include <atomic>
#include <QThread>

class OpenGLWindow;

class EmuThread : public QThread {
    Q_OBJECT

public:
    EmuThread(QObject *parent = nullptr);
    virtual ~EmuThread();

    void pauseEmulation();
    bool emulationIsPaused();

    void setJustOneStep(bool justOneStep);

    void setOpenGLWindow(OpenGLWindow *window);
    void setVRAMOpenGLWindow(OpenGLWindow *window);

public slots:
    void openGLWindowClosed();

signals:
    void emulationShouldStop();

protected:
    void run();

private:
    void initialize();
    bool initialized;
    OpenGLWindow *openGLWindow;
    OpenGLWindow *vramOpenGLWindow;

    std::atomic<bool> paused;
    bool justOneStep;
};

#endif
