#ifndef VRAMVIEWERWINDOW_H
#define VRAMVIEWERWINDOW_H

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class VRAMViewerWindow;
}
QT_END_NAMESPACE

namespace PSX {
class Core;
}

class EmuThread;
class OpenGLWindow;

class VRAMViewerWindow : public QWidget {
    Q_OBJECT

public:
    explicit VRAMViewerWindow(QWidget *parent = nullptr);
    ~VRAMViewerWindow();

    OpenGLWindow* getOpenGLWindow();
    void closeEvent(QCloseEvent *event) override;

signals:
    void closed();

private:
    Ui::VRAMViewerWindow *ui;

    OpenGLWindow *vramWindow;
    QWidget *vramWindowWidget;
};

#endif

