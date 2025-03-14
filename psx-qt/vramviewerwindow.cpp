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
      emuThread(emuThread) {
    ui->setupUi(this);

    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setVersion(3,3);

    OpenGLWindow *window = new OpenGLWindow();
    window->setFormat(format);

    //QWidget *widget = QWidget::createWindowContainer(window);
    //widget->setMinimumSize(QSize(1024, 512));
    ////VRAMViewer *widget = new VRAMViewer();
    //QHBoxLayout *layout = new QHBoxLayout(this);
    //layout->addWidget(widget);
    //this->setLayout(layout);
}

VRAMViewerWindow::~VRAMViewerWindow() {
    delete ui;
}

void VRAMViewerWindow::grabWindowFromEmuThread() {
    if (emuThread) {
        OpenGLWindow *vramWindow = emuThread->getVRAMWindow();

        QWidget *widget = QWidget::createWindowContainer(vramWindow);
        widget->setMinimumSize(QSize(1024, 512));
        widget->setMaximumSize(QSize(1024, 512));
        //VRAMViewer *widget = new VRAMViewer();
        QHBoxLayout *layout = new QHBoxLayout(this);
        layout->addWidget(widget);
        this->setLayout(layout);
    }
}

void VRAMViewerWindow::closeEvent(QCloseEvent *event) {
    emit closed();
}

