#ifndef EMUTHREAD_H
#define EMUTHREAD_H

#include <atomic>
#include <QThread>

class OpenGLWindow;

namespace PSX {
class Core;
class OpenGLRenderer;
}

class EmuThread : public QThread {
    Q_OBJECT

public:
    EmuThread(QObject *parent = nullptr, PSX::Core *core = nullptr);
    virtual ~EmuThread();

    void pauseEmulation();
    bool emulationIsPaused();

    OpenGLWindow* getOpenGLWindow();
    OpenGLWindow* getVRAMWindow();

public slots:
    void openGLWindowClosed();

signals:
    void emulationShouldStop();
    void initializedWindows();

protected:
    void run();

private:
    PSX::Core *core;

    void initialize();
    bool initialized;
    OpenGLWindow *openGLWindow;
    OpenGLWindow *vramOpenGLWindow;
    PSX::OpenGLRenderer *renderer;

    std::atomic<bool> paused;
};

#endif
