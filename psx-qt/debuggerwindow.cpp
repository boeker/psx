#include <format>

#include "debuggerwindow.h"
#include "ui_debuggerwindow.h"

#include <QFontDatabase>
#include <QStringListModel>
#include <QVariant>

#include "psx/core.h"

InstructionModel::InstructionModel(PSX::Core *core, QObject *parent)
    : QAbstractListModel(parent), core(core) {
}

int InstructionModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }

    return (2048 * 1024) / 4;
}

QVariant InstructionModel::data(const QModelIndex &index, int role) const {
    if (role == Qt::DisplayRole) {
        uint32_t address = index.row() * 4;
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

    return (2048 * 1024) / 16;
}

QVariant MemoryModel::data(const QModelIndex &index, int role) const {
    if (role == Qt::DisplayRole) {
        if (index.column() == 0) {
            uint32_t address = index.row() * 16;
            return QVariant(QString::fromStdString(std::format("0x{:08X}   ", address)));

        } else if (index.column() <= 16) {
            uint32_t address = index.row() * 16 + (index.column() - 1);
            return QVariant(QString::fromStdString(std::format("{:02X}", core->bus.debugRead<uint8_t>(address))));

        } else {
            uint32_t address = index.row() * 16;

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

DebuggerWindow::DebuggerWindow(PSX::Core *core, QWidget *parent)
    : QWidget(parent),
      ui(new Ui::DebuggerWindow),
      instructionModel(new InstructionModel(core)),
      registerModel(new RegisterModel(core)),
      memoryModel(new MemoryModel(core)) {
    ui->setupUi(this);

    // Set models
    ui->instructionView->setModel(instructionModel);
    ui->registerView->setModel(registerModel);
    ui->memoryView->setModel(memoryModel);

    // Set monospace font in all views
    const QFont fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    ui->instructionView->setFont(fixedFont);
    ui->registerView->setFont(fixedFont);
    ui->memoryView->setFont(fixedFont);

    // Set up instruction view
    // Is this okay? (Do all items have the same size?)
    ui->instructionView->setUniformItemSizes(true);


    // Set up register View
    ui->registerView->setShowGrid(false);
    ui->registerView->verticalHeader()->hide();
    ui->registerView->horizontalHeader()->hide();
    ui->registerView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    // Set up memory view
    ui->memoryView->setShowGrid(false);
    ui->memoryView->verticalHeader()->hide();
    ui->memoryView->horizontalHeader()->hide();
    ui->memoryView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->memoryView->horizontalHeader()->setMinimumSectionSize(0);
    ui->memoryView->horizontalHeader()->setSectionResizeMode(17, QHeaderView::Stretch);
}

DebuggerWindow::~DebuggerWindow() {
    delete ui;
}

void DebuggerWindow::closeEvent(QCloseEvent *event) {
    emit closed();
}

