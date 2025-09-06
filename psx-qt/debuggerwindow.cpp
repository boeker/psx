#include <format>

#include "debuggerwindow.h"
#include "ui_debuggerwindow.h"

#include <QAbstractListModel>
#include <QFontDatabase>
#include <QStringListModel>
#include <QVariant>

#include "psx/core.h"

class InstructionModel : public QAbstractListModel {
private:
    PSX::Core *core;

public:
    InstructionModel(PSX::Core *core)
        : core(core) {
    }

    int rowCount(const QModelIndex &parent) const {
        if (parent.isValid()) {
            return 0;
        }

        return (2048 * 1024) / 4;
    }

    QVariant data(const QModelIndex &index, int role) const {
        if (role == Qt::DisplayRole) {
            uint32_t address = index.row() * 4;
            return QVariant(QString::fromStdString(std::format("0x{:08X}: 0x{:08X}", address, core->bus.debugRead<uint32_t>(address))));
	    }

	    return QVariant();
    }
};

DebuggerWindow::DebuggerWindow(PSX::Core *core, QWidget *parent)
    : QWidget(parent),
      ui(new Ui::DebuggerWindow) {
    ui->setupUi(this);
    InstructionModel *memoryModel = new InstructionModel(core);

    // set monospace font
    const QFont fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    ui->instructionView->setFont(fixedFont);

    // all items have the same size, I promise
    ui->instructionView->setUniformItemSizes(true);

    // set model
    ui->instructionView->setModel(memoryModel);
}

DebuggerWindow::~DebuggerWindow() {
    delete ui;
}

void DebuggerWindow::closeEvent(QCloseEvent *event) {
    emit closed();
}

