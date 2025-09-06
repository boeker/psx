#ifndef DEBUGGERWINDOW_H
#define DEBUGGERWINDOW_H

#include <QAbstractListModel>
#include <QAbstractTableModel>
#include <QWidget>

namespace PSX {
class Core;
}

namespace Ui {
class DebuggerWindow;
}

class InstructionModel : public QAbstractListModel {
private:
    PSX::Core *core;

public:
    InstructionModel(PSX::Core *core);

    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
};

class MemoryModel : public QAbstractTableModel {
private:
    PSX::Core *core;

public:
    MemoryModel(PSX::Core *core);

    int columnCount(const QModelIndex &parent) const override;
    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
};

class RegisterModel : public QAbstractTableModel {
private:
    PSX::Core *core;

public:
    RegisterModel(PSX::Core *core);

    int columnCount(const QModelIndex &parent) const override;
    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
};

class DebuggerWindow : public QWidget
{
    Q_OBJECT

public:
    explicit DebuggerWindow(PSX::Core *core, QWidget *parent = nullptr);
    ~DebuggerWindow();

    void closeEvent(QCloseEvent *event) override;

signals:
    void closed();

private:
    Ui::DebuggerWindow *ui;
};

#endif // DEBUGGERWINDOW_H
