#include "vramviewer.h"

#include <QDebug>
#include <QPainter>

VRAMViewer::VRAMViewer(QWidget *parent)
    : QWidget(parent) {
}

VRAMViewer::~VRAMViewer() {
}

void VRAMViewer::paintEvent(QPaintEvent * e) {
    QRect rectangle(20, 20, 100, 100);

    QPainter painter(this);
    painter.drawRect(rectangle);
    painter.end();
}

