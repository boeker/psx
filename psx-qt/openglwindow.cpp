#include "openglwindow.h"

OpenGLWindow::OpenGLWindow(QWindow *parent)
    : QWindow(parent),
      openglContext(nullptr) {
    setSurfaceType(QWindow::OpenGLSurface);
}

OpenGLWindow::~OpenGLWindow() {
    openglContext->makeCurrent(this);

    vao.destroy();
    vbo.destroy();
    delete program;
}

void OpenGLWindow::initialize() {
    openglContext = new QOpenGLContext(this);
    openglContext->setFormat(requestedFormat());
    openglContext->create();

    openglContext->makeCurrent(this);

    initializeOpenGLFunctions();

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

void OpenGLWindow::render() {
	//glViewport(0, 0, width(), height());

	//glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	//glClear(GL_COLOR_BUFFER_BIT);

	//program->bind();
	//vao.bind();
	//glDrawArrays(GL_TRIANGLES, 0, 3);
	//vao.release();

    //openglContext->swapBuffers(this);
}

bool OpenGLWindow::event(QEvent *event) {
    switch (event->type()) {
        case QEvent::UpdateRequest:
            //render();
            return true;
        default:
            return QWindow::event(event);
    }
}

void OpenGLWindow::exposeEvent(QExposeEvent *event) {
    //render();
}
