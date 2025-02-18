#include "vramviewerwindow.h"
#include "ui_vramviewerwindow.h"

VRAMViewerWindow::VRAMViewerWindow(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::VRAMViewerWindow) {
    ui->setupUi(this);
}

VRAMViewerWindow::~VRAMViewerWindow() {
    delete ui;
}

void VRAMViewerWindow::closeEvent(QCloseEvent *event) {
    emit closed();
}

