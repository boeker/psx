#include "mainwindow.h"
#include "openglwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    //MainWindow w;
    //w.show();

	QSurfaceFormat format;
	format.setRenderableType(QSurfaceFormat::OpenGL);
	format.setProfile(QSurfaceFormat::CoreProfile);
	format.setVersion(3,3);

	OpenGLWindow window;
	window.setFormat(format);
	window.resize(640, 480);
	window.show();

    window.initialize();

    return a.exec();
}
