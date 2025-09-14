#include <format>

#include "debuggerwindow.h"
#include "ui_debuggerwindow.h"

#include <QFontDatabase>
#include <QStringListModel>
#include <QVariant>

#include "psx/core.h"

//#define MAIN_RAM_SIZE (2048 * 1024)
//#define DCACHE_SIZE 1024
//#define BIOS_SIZE (512 * 1024)

#define TOTAL_MAPPED_SIZE (MAIN_RAM_SIZE + DCACHE_SIZE + BIOS_SIZE)

bool isMapped(uint32_t address) {
    return ((address & 0x1FE00000) == 0x00000000) // Main RAM
           || ((address & 0x1FFFFC00) == 0x1F8FFC00) // D-Cache (Scratchpad)
           || ((address & 0x1FF80000) == 0x1FC00000); // BIOS ROM
}

uint32_t addressToLine(uint32_t address) {
    if ((address & 0x1FE00000) == 0x00000000) { // Main RAM
        return address & 0x001FFFFF;

    } else if ((address & 0x1FFFFC00) == 0x1F8FFC00) { // D-Cache (Scratchpad)
        return MAIN_RAM_SIZE + (address & 0x3FF);

    } else { // ((address & 0x1FF80000) == 0x1FC00000); // BIOS ROM
        return MAIN_RAM_SIZE + DCACHE_SIZE + (address & 0x7FFFFF);
    }
}

uint32_t lineToAddress(uint32_t line) {
    uint32_t address = 0x80000000;

    if (line < MAIN_RAM_SIZE) {
        address += line;

    } else if (line < MAIN_RAM_SIZE + DCACHE_SIZE) {
        address += 0x1F800000 + (line - MAIN_RAM_SIZE);

    } else {
        address += 0x1FC00000 + (line - MAIN_RAM_SIZE - DCACHE_SIZE);
    }

    return address;
}

InstructionModel::InstructionModel(PSX::Core *core, QObject *parent)
    : QAbstractListModel(parent), core(core) {
}

int InstructionModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }

    return TOTAL_MAPPED_SIZE / 4;
}

QVariant InstructionModel::data(const QModelIndex &index, int role) const {
    if (role == Qt::DisplayRole) {
        uint32_t address = lineToAddress(index.row() * 4);
        return QVariant(QString::fromStdString(std::format("0x{:08X}: 0x{:08X}", address, core->bus.debugRead<uint32_t>(address))));
    }

    return QVariant();
}

StackModel::StackModel(PSX::Core *core, QObject *parent)
    : QAbstractListModel(parent), core(core) {
}

int StackModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }

    return TOTAL_MAPPED_SIZE / 4;
}

QVariant StackModel::data(const QModelIndex &index, int role) const {
    if (role == Qt::DisplayRole) {
        uint32_t address = lineToAddress(TOTAL_MAPPED_SIZE - (index.row() + 1) * 4);
        return QVariant(QString::fromStdString(std::format("0x{:08X}: 0x{:08X}", address, core->bus.debugRead<uint32_t>(address))));
    }

    return QVariant();
}

MemoryModel::MemoryModel(PSX::Core *core, QObject *parent)
    : QAbstractTableModel(parent), core(core) {
}

int MemoryModel::columnCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }

    return 18;
}

int MemoryModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }

    return TOTAL_MAPPED_SIZE / 16;
}

QVariant MemoryModel::data(const QModelIndex &index, int role) const {
    if (role == Qt::DisplayRole) {
        if (index.column() == 0) {
            uint32_t address = lineToAddress(index.row() * 16);
            return QVariant(QString::fromStdString(std::format("0x{:08X}   ", address)));

        } else if (index.column() <= 16) {
            uint32_t address = lineToAddress(index.row() * 16 + (index.column() - 1));
            return QVariant(QString::fromStdString(std::format("{:02X}", core->bus.debugRead<uint8_t>(address))));

        } else {
            uint32_t address = lineToAddress(index.row() * 16);

            char str[17];
            for (int i = 0; i < 16; ++i) {
                str[i] = core->bus.debugRead<uint8_t>(address + i);
                if (str[i] < 32 || str[i] > 176) {
                    str[i] = '.';
                }
            }
            str[16] = '\0';

            return QVariant(QString("   ") + QString(str));
        }
    }

    return QVariant();
}

RegisterModel::RegisterModel(PSX::Core *core, QObject *parent)
    : QAbstractTableModel(parent), core(core) {
}

int RegisterModel::columnCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }

    return 2;
}

int RegisterModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }

    return 17;
}

QVariant RegisterModel::data(const QModelIndex &index, int role) const {
    if (role == Qt::DisplayRole) {
        if (index.row() == 0 && index.column() == 0) {
            return QVariant(QString::fromStdString(std::format("pc 0x{:08X}",
                                                               core->bus.cpu.regs.getPC())));
        } else if (index.row() < 16) {
            uint8_t registerNumber = (index.column() > 0 ? 16 : 0) + index.row();
            return QVariant(QString::fromStdString(std::format("{:s} 0x{:08X}",
                                                               core->bus.cpu.regs.getRegisterName(registerNumber),
                                                               core->bus.cpu.regs.getRegister(registerNumber))));
        } else if (index.row() == 16) {
            if (index.column() == 0) {
                return QVariant(QString::fromStdString(std::format("hi 0x{:08X}",
                                                                   core->bus.cpu.regs.getHi())));
            } else {
                return QVariant(QString::fromStdString(std::format("lo 0x{:08X}",
                                                                   core->bus.cpu.regs.getLo())));
            }
        }
    }

    return QVariant();
}

void RegisterModel::update() {
    QModelIndex topLeft = index(0, 0);
    QModelIndex botRight = index(16, 1);
    emit dataChanged(topLeft, botRight);
}

DebuggerWindow::DebuggerWindow(PSX::Core *core, QWidget *parent)
    : QWidget(parent),
      ui(new Ui::DebuggerWindow),
      core(core),
      instructionModel(new InstructionModel(core)),
      stackModel(new StackModel(core)),
      registerModel(new RegisterModel(core)),
      memoryModel(new MemoryModel(core)) {
    ui->setupUi(this);

    // Monospace font
    const QFont fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);

    // Set up instruction view
    // Is this okay? (Do all items have the same size?)
    ui->instructionView->setModel(instructionModel);
    ui->instructionView->setFont(fixedFont);
    ui->instructionView->setUniformItemSizes(true);

    // Set up the stack view
    ui->stackView->setModel(stackModel);
    ui->stackView->setFont(fixedFont);
    ui->stackView->setUniformItemSizes(true);

    // Set up register View
    ui->registerView->setModel(registerModel);
    ui->registerView->setFont(fixedFont);
    ui->registerView->setShowGrid(false);
    ui->registerView->verticalHeader()->hide();
    ui->registerView->horizontalHeader()->hide();
    ui->registerView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    // Set up memory view
    ui->memoryView->setModel(memoryModel);
    ui->memoryView->setFont(fixedFont);
    ui->memoryView->setShowGrid(false);
    ui->memoryView->verticalHeader()->hide();
    ui->memoryView->horizontalHeader()->hide();
    ui->memoryView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->memoryView->horizontalHeader()->setMinimumSectionSize(0);
    ui->memoryView->horizontalHeader()->setSectionResizeMode(17, QHeaderView::Stretch);

    jumpToState();
}

DebuggerWindow::~DebuggerWindow() {
    delete ui;
}

void DebuggerWindow::closeEvent(QCloseEvent *event) {
    emit closed();
}

void DebuggerWindow::jumpToState() {
    uint32_t pc = core->bus.cpu.regs.getPC();

    QModelIndex instructionIndex = instructionModel->index(addressToLine(pc) / 4, 0);
    ui->instructionView->setCurrentIndex(instructionIndex);

    //uint32_t sp = core->bus.cpu.regs.getRegister(29); // Register 29 is the stack pointer
    //QModelIndex stackIndex = stackModel->index(128, 0);
    //ui->stackView->setCurrentIndex(stackIndex);
}

void DebuggerWindow::update() {
    registerModel->update();
}

