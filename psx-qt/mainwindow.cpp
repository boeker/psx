#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <limits>
#include <QDir>
#include <QFileDialog>
#include <QFileSystemModel>
#include <QOpenGLContext>
#include <QOverload>
#include <QScrollBar>

#include "debuggerwindow.h"
#include "emuthread.h"
#include "openglwindow.h"
#include "vramviewerwindow.h"

#include "psx/core.h"
#include "psx/cd.h"
#include "psx/gamepad.h"
#include "psx/renderer/software/softwarerenderer.h"
#include "psx/util/log.h"

bool running = false;
PSX::Core *core = nullptr;
PSX::SoftwareRenderer *renderer = nullptr;
EmuThread *emuThread = nullptr;

PlainTextEditLog::PlainTextEditLog(QPlainTextEdit *plainTextEdit)
    : Log(true),
      plainTextEdit(plainTextEdit) {
}

bool PlainTextEditLog::print(const std::string &message) {
        QString qString = QString::fromStdString(message);
        emit logString(qString);
        emit moveScrollBar(std::numeric_limits<int>::max());

        return false;
}

MainWindow::MainWindow(const QString &biosPath, QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      biosFSModel(nullptr),
      vramViewerWindow(nullptr) {
    ui->setupUi(this);

    // Bios Picker
    QDir dir = QDir::currentPath();
    biosFSModel = new QFileSystemModel(ui->treeView);
    biosFSModel->setRootPath(dir.path());
    ui->treeView->setModel(biosFSModel);
    ui->treeView->setRootIndex(biosFSModel->index(dir.path()));
    ui->treeView->hideColumn(2);
    ui->treeView->hideColumn(3);
    ui->treeView->header()->setSectionResizeMode(0, QHeaderView::Stretch);

    std::vector<QString> biosLocations;
    if (!biosPath.isEmpty()) {
        biosLocations.push_back(biosPath);
    }
    biosLocations.push_back("SCPH1001.BIN");
    biosLocations.push_back("BIOS/SCPH1001.BIN");

    for (const QString &bios: biosLocations) {
        if (dir.exists(bios)) {
            ui->treeView->setCurrentIndex(biosFSModel->index(dir.path() + "/" + bios));
            break;
        }
    }

    // OpenGL window
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setVersion(3,3);

    openGLWindow = new OpenGLWindow();
    openGLWindow->setFormat(format);
    openGLWindow->resize(640, 480);

    openGLWindowWidget = QWidget::createWindowContainer(openGLWindow, this);
    openGLWindowWidget->setMinimumSize(QSize(640, 480));
    //openGLWindowWidget->setMaximumSize(QSize(640, 480));
    ui->centralwidget->layout()->addWidget(openGLWindowWidget);
    openGLWindowWidget->hide();
    // Setting the parent to nullptr causes the screen to be shown in its own window
    //openGLWindow->setParent(nullptr);

    // Re-add text edit to make sure it is add the bottom
    ui->centralwidget->layout()->removeWidget(ui->plainTextEditLog);
    ui->centralwidget->layout()->addWidget(ui->plainTextEditLog);

    // VRAM Window
    vramViewerWindow = new VRAMViewerWindow();

    // Core
    core = new PSX::Core();

    // Debugger Window
    debuggerWindow = new DebuggerWindow(core);

    // Renderer
    renderer = new PSX::SoftwareRenderer(openGLWindow, vramViewerWindow->getOpenGLWindow());
    core->setRenderer(renderer);

    // Emulation thread
    emuThread = new EmuThread(this);
    emuThread->setOpenGLWindow(openGLWindow);
    emuThread->setVRAMOpenGLWindow(vramViewerWindow->getOpenGLWindow());

    // Logging
    std::shared_ptr<PlainTextEditLog> plainTextEditLog = std::make_shared<PlainTextEditLog>(ui->plainTextEditLog);
    connect(plainTextEditLog.get(), &PlainTextEditLog::logString, ui->plainTextEditLog, &QPlainTextEdit::appendPlainText);
    connect(plainTextEditLog.get(), &PlainTextEditLog::moveScrollBar, ui->plainTextEditLog->verticalScrollBar(), &QScrollBar::setValue);
    util::logPack.installAdditionalLog(plainTextEditLog);

    // Connections
    makeConnections();

    grabKeyboard(); // Grab keyboard to make input from arrow keys work

    LOG_MISC("Application startup");
}

MainWindow::~MainWindow() {
    stopEmulation();

    delete debuggerWindow;
    delete vramViewerWindow;

    delete ui;
    delete renderer;
    delete core;
}

void MainWindow::makeConnections() {
    connect(ui->actionLoadExecutable, &QAction::triggered,
            this, qOverload<>(&MainWindow::loadExecutable));
    connect(ui->actionLoadCDImage, &QAction::triggered,
            this, qOverload<>(&MainWindow::loadCDImage));
    connect(ui->actionExit, &QAction::triggered,
            QCoreApplication::instance(), &QCoreApplication::quit);

    connect(ui->actionStart, &QAction::triggered,
            this, &MainWindow::startPauseEmulation);
    connect(ui->actionToolbarStart, &QAction::triggered,
            this, &MainWindow::startPauseEmulation);

    connect(ui->actionPause, &QAction::triggered,
            this, &MainWindow::startPauseEmulation);

    connect(ui->actionStop, &QAction::triggered,
            this, &MainWindow::stopEmulation);
    connect(ui->actionToolbarStop, &QAction::triggered,
            this, &MainWindow::stopEmulation);

    connect(ui->actionStep, &QAction::triggered,
            this, &MainWindow::emulateOneStep);

    connect(emuThread, &EmuThread::emulationShouldStop,
            this, &MainWindow::stopEmulation);

    // Debugger window
    connect(ui->actionDebugger, &QAction::toggled,
            debuggerWindow, &QWidget::setVisible);
    connect(debuggerWindow, &DebuggerWindow::closed,
            ui->actionDebugger, &QAction::toggle);

    // VRAM viewer window
    connect(ui->actionVRAMViewer, &QAction::toggled,
            vramViewerWindow, &QWidget::setVisible);
    connect(vramViewerWindow, &VRAMViewerWindow::closed,
            ui->actionVRAMViewer, &QAction::toggle);
}

void MainWindow::loadExecutable() {
    setExecutableFileName(QFileDialog::getOpenFileName(this, tr("Load Executable")));
}

void MainWindow::setExecutableFileName(const QString &fileName) {
    executableFileName = fileName;
}

void MainWindow::loadCDImage() {
    setCDImageFileName(QFileDialog::getOpenFileName(this, tr("Load CD Image")));
}

void MainWindow::setCDImageFileName(const QString &fileName) {
    cdImageFileName = fileName;
}

void MainWindow::startPauseEmulation() {
    if (!running) {
        core->reset();

        QString selectedBios = biosFSModel->filePath(ui->treeView->currentIndex());
        core->bus.bios.readFromFile(selectedBios.toStdString());
        if (!executableFileName.isEmpty()) {
            core->bus.executable.readFromFile(executableFileName.toStdString());
        }

        if (!cdImageFileName.isEmpty()) {
            core->bus.cdrom.setCD(std::make_unique<PSX::CD>(cdImageFileName.toStdString()));
        }

        ui->treeView->setHidden(true);
        openGLWindowWidget->show();

        running = true;
    }

    if (emuThread->emulationIsPaused()) {
        continueEmulation();

    } else {
        LOG_MISC("Pausing emulation");
        pauseEmulation();
    }
}

void MainWindow::continueEmulation() {
    LOG_MISC("Starting emulation");
    ui->actionToolbarStart->setText("Pause");
    ui->actionToolbarStop->setEnabled(true);

    ui->actionStart->setEnabled(false);
    ui->actionPause->setEnabled(true);
    ui->actionStop->setEnabled(true);
    ui->actionStep->setEnabled(false);

    emuThread->start();
}

void MainWindow::pauseEmulation() {
    ui->actionToolbarStart->setText("Start");
    ui->actionStart->setEnabled(true);
    ui->actionPause->setEnabled(false);
    ui->actionStep->setEnabled(true);

    emuThread->pauseEmulation();
    debuggerWindow->jumpToState();
    debuggerWindow->update();
}

void MainWindow::stopEmulation() {
    if (running) {
        ui->actionToolbarStart->setText("Start");
        ui->actionToolbarStop->setEnabled(false);

        ui->actionStart->setEnabled(true);
        ui->actionPause->setEnabled(false);
        ui->actionStop->setEnabled(false);

        emuThread->pauseEmulation();
        emuThread->wait();
        running = false;

        openGLWindowWidget->hide();
        ui->treeView->setHidden(false);
        LOG_MISC("Stopped emulation");
    }
}

void MainWindow::emulateOneStep() {
    LOG_MISC("Emulating a single step");

    emuThread->setJustOneStep(true);
    emuThread->start();
    emuThread->wait();
    debuggerWindow->jumpToState();
    debuggerWindow->update();
}

void MainWindow::triggerVRAMViewerWindow() {
    ui->actionVRAMViewer->trigger();
}

void MainWindow::triggerDebuggerWindow() {
    ui->actionDebugger->trigger();
}

void MainWindow::closeEvent(QCloseEvent *event) {
    stopEmulation();
    vramViewerWindow->close();
}

bool MainWindow::handleKeyEvent(QKeyEvent *event, bool pressed) {
    if (running) {
        PSX::Gamepad &pad = core->bus.gamepad;

        switch (event->key()) {
        case Qt::Key_Up:
            pad.setUp(pressed);
            break;
        case Qt::Key_Down:
            pad.setDown(pressed);
            break;
        case Qt::Key_Left:
            pad.setLeft(pressed);
            break;
        case Qt::Key_Right:
            pad.setRight(pressed);
            break;
        case Qt::Key_E:
            pad.setTriangle(pressed);
            break;
        case Qt::Key_X:
            pad.setCross(pressed);
            break;
        case Qt::Key_S:
            pad.setSquare(pressed);
            break;
        case Qt::Key_D:
            pad.setCircle(pressed);
            break;
        case Qt::Key_W:
            pad.setL1(pressed);
            break;
        case Qt::Key_Q:
            pad.setL2(pressed);
            break;
        case Qt::Key_A:
            pad.setL3(pressed);
            break;
        case Qt::Key_R:
            pad.setR1(pressed);
            break;
        case Qt::Key_T:
            pad.setR2(pressed);
            break;
        case Qt::Key_F:
            pad.setR3(pressed);
            break;
        case Qt::Key_C:
            pad.setSelect(pressed);
            break;
        case Qt::Key_V:
            pad.setStart(pressed);
            break;
        default:
            return false;
        }

        return true;
    }

    return false;
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
    if (!handleKeyEvent(event, true)) {
        QMainWindow::keyPressEvent(event);
    }
}

void MainWindow::keyReleaseEvent(QKeyEvent *event) {
    if (!handleKeyEvent(event, false)) {
        QMainWindow::keyReleaseEvent(event);
    }
}

