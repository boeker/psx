#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow) {
    ui->setupUi(this);

    openGLWindow = new OpenGLWindow();

	openGLWindow->resize(640, 480);
    openGLWindow->show();
    openGLWindow->initialize();

    //emuthread = std::make_shared<EmuThread>();
    //emuthread->start();
}

MainWindow::~MainWindow() {
    openGLWindow->close();
    delete openGLWindow;

    delete ui;
}
