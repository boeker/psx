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

    void setBiosPath(const QString &biosPath);

protected:
    void run();

private:
    QString biosPath;
    OpenGLWindow *window;

    void createWindow();
};

#endif
