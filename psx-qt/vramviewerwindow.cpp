#include "vramviewerwindow.h"
#include "ui_vramviewerwindow.h"

#include <QHBoxLayout>

#include "psx/core.h"
//#include "vramviewer.h"
#include "emuthread.h"
#include "mainwindow.h"
#include "openglwindow.h"

VRAMViewerWindow::VRAMViewerWindow(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::VRAMViewerWindow),
      vramWindow(nullptr) {
    ui->setupUi(this);

    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setVersion(3,3);

    vramWindow = new OpenGLWindow();
    vramWindow->setFormat(format);
    vramWindow->resize(1024, 512);

    vramWindowWidget = QWidget::createWindowContainer(vramWindow, this);
    vramWindowWidget->setMinimumSize(QSize(1024, 512));
    //vramWindowWidget->setMaximumSize(QSize(1024, 512));
    this->layout()->addWidget(vramWindowWidget);
}

VRAMViewerWindow::~VRAMViewerWindow() {
    delete ui;
}

OpenGLWindow* VRAMViewerWindow::getOpenGLWindow() {
    return vramWindow;
}

void VRAMViewerWindow::closeEvent(QCloseEvent *event) {
    emit closed();
}

