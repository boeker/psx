#ifndef DEBUGGERWINDOW_H
#define DEBUGGERWINDOW_H

#include <QWidget>

namespace Ui {
class DebuggerWindow;
}

class DebuggerWindow : public QWidget
{
    Q_OBJECT

public:
    explicit DebuggerWindow(QWidget *parent = nullptr);
    ~DebuggerWindow();

    void closeEvent(QCloseEvent *event) override;

signals:
    void closed();

private:
    Ui::DebuggerWindow *ui;
};

#endif // DEBUGGERWINDOW_H
