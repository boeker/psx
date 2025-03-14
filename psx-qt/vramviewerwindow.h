#ifndef VRAMVIEWERWINDOW_H
#define VRAMVIEWERWINDOW_H

#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui {
class VRAMViewerWindow;
}
QT_END_NAMESPACE

namespace PSX {
class Core;
}

class EmuThread;

class VRAMViewerWindow : public QDialog {
    Q_OBJECT

public:
    explicit VRAMViewerWindow(QWidget *parent = nullptr, EmuThread *emuThread = nullptr);
    ~VRAMViewerWindow();

    void closeEvent(QCloseEvent *event) override;

public slots:
    void grabWindowFromEmuThread();

signals:
    void closed();

private:
    Ui::VRAMViewerWindow *ui;

    EmuThread *emuThread;
};

#endif

