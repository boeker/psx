#ifndef VRAMVIEWERWINDOW_H
#define VRAMVIEWERWINDOW_H

#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui {
class VRAMViewerWindow;
}
QT_END_NAMESPACE

class VRAMViewerWindow : public QDialog {
    Q_OBJECT

public:
    explicit VRAMViewerWindow(QWidget *parent = nullptr);
    ~VRAMViewerWindow();

    void closeEvent(QCloseEvent *event) override;

signals:
    void closed();

private:
    Ui::VRAMViewerWindow *ui;
};

#endif

