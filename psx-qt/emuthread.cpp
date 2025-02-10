#include "emuthread.h"

#include <QSurfaceFormat>

#include "openglwindow.h"
#include "psx/core.h"

EmuThread::EmuThread(QObject *parent)
    : QThread(parent) {
}

EmuThread::~EmuThread() {
    openglContext->makeCurrent(window);

    vao.destroy();
    vbo.destroy();
    delete program;
}

void EmuThread::createWindow() {
	QSurfaceFormat format;
	format.setRenderableType(QSurfaceFormat::OpenGL);
	format.setProfile(QSurfaceFormat::CoreProfile);
	format.setVersion(3,3);

    window = new OpenGLWindow();
    window->setFormat(format);
	window->resize(640, 480);
    window->show();

    openglContext = new QOpenGLContext(window);
    openglContext->setFormat(window->requestedFormat());
    openglContext->create();
    openglContext->makeCurrent(window);

    glViewport(0, 0, window->width(), window->height());

    program = new QOpenGLShaderProgram();
    program->addShaderFromSourceFile(QOpenGLShader::Vertex,
                                     "shaders/shader.vs");

    program->addShaderFromSourceFile(QOpenGLShader::Fragment,
                                     "shaders/shader.fs");
    program->link();

	float vertices[] = {
		-0.5f, -0.5f, 0.0f,
		 0.5f, -0.5f, 0.0f,
		 0.0f,  0.5f, 0.0f
	};

	vbo = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
	vbo.create();
	vbo.setUsagePattern(QOpenGLBuffer::StaticDraw);

	vbo.bind();
	vbo.allocate(vertices, sizeof(vertices));

    vao.create();
	vao.bind();

	program->enableAttributeArray(0);
	program->setAttributeBuffer(0, GL_FLOAT, 0, 3);

    vbo.release();
    vao.release();
}

void EmuThread::run() {
    createWindow();

    unsigned int c = 0;
    while (true) {
        c++;
        glClearColor((c % 1000) / 1000.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        program->bind();
        vao.bind();
        glDrawArrays(GL_TRIANGLES, 0, 3);
        vao.release();

        openglContext->swapBuffers(window);
    }

    //PSX::Core core;

    //core.run();
}

