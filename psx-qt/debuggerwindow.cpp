#include "debuggerwindow.h"
#include "ui_debuggerwindow.h"

DebuggerWindow::DebuggerWindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::DebuggerWindow)
{
    ui->setupUi(this);
}

DebuggerWindow::~DebuggerWindow()
{
    delete ui;
}

void DebuggerWindow::closeEvent(QCloseEvent *event) {
    emit closed();
}

