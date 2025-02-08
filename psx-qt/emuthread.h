#ifndef EMUTHREAD_H
#define EMUTHREAD_H

#include <QThread>

class EmuThread : public QThread {
    Q_OBJECT

public:
    EmuThread();
    virtual ~EmuThread();

protected:
    void run();
};

#endif
