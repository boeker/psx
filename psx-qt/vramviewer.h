#ifndef VRAMVIEWER_H
#define VRAMVIEWER_H

#include <QWidget>

class VRAMViewer : public QWidget {
    Q_OBJECT

public:
    VRAMViewer(QWidget*parent = nullptr);
    ~VRAMViewer();

    void paintEvent(QPaintEvent * e) override;
};

#endif
