#include "vramviewerwindow.h"
#include "ui_vramviewerwindow.h"

#include <QHBoxLayout>

#include "psx/core.h"
//#include "vramviewer.h"
#include "emuthread.h"
#include "openglwindow.h"

VRAMViewerWindow::VRAMViewerWindow(QWidget *parent, EmuThread *emuThread)
    : QDialog(parent),
      ui(new Ui::VRAMViewerWindow),
      emuThread(emuThread),
      vramWindow(nullptr) {
    ui->setupUi(this);
}

VRAMViewerWindow::~VRAMViewerWindow() {
    if (vramWindow) {
        vramWindow->setParent(nullptr);

        vramWindow = nullptr;
    }

    delete ui;
}

void VRAMViewerWindow::grabWindowFromEmuThread() {
    if (emuThread) {
        vramWindow = emuThread->getVRAMWindow();

        QWidget *widget = QWidget::createWindowContainer(vramWindow, this);
        widget->setMinimumSize(QSize(1024, 512));
        widget->setMaximumSize(QSize(1024, 512));

        this->layout()->addWidget(widget);
    }
}

void VRAMViewerWindow::closeEvent(QCloseEvent *event) {
    emit closed();
}

