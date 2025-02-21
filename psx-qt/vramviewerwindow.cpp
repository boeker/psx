#include "vramviewerwindow.h"
#include "ui_vramviewerwindow.h"

#include <QHBoxLayout>

#include "psx/core.h"
#include "vramviewer.h"

VRAMViewerWindow::VRAMViewerWindow(QWidget *parent, PSX::Core *core)
    : QDialog(parent),
      ui(new Ui::VRAMViewerWindow),
      core(core) {
    ui->setupUi(this);

    VRAMViewer *widget = new VRAMViewer();
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->addWidget(widget);
    this->setLayout(layout);
}

VRAMViewerWindow::~VRAMViewerWindow() {
    delete ui;
}

void VRAMViewerWindow::closeEvent(QCloseEvent *event) {
    emit closed();
}

