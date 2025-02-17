#ifndef EMUTHREAD_H
#define EMUTHREAD_H

#include <atomic>
#include <QThread>

class OpenGLWindow;
class QOpenGLContext;

namespace PSX {
class Core;
class OpenGLRenderer;
}

class EmuThread : public QThread {
    Q_OBJECT

public:
    EmuThread(QObject *parent = nullptr);
    virtual ~EmuThread();

    void setBiosPath(const QString &biosPath);
    void pauseEmulation();
    bool emulationIsPaused();

protected:
    void run();

private:
    QString biosPath;

    OpenGLWindow *window;
    PSX::OpenGLRenderer *renderer;
    PSX::Core *core;
    bool initialized;

    std::atomic<bool> paused;

    void createWindow();
    void createPSXCore();
};

#endif
