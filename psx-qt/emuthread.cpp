#include "emuthread.h"

#include "openglwindow.h"
#include "psx/core.h"

EmuThread::EmuThread() {
}

EmuThread::~EmuThread() {
}

void EmuThread::run() {
    window = new OpenGLWindow();

	window->resize(640, 480);
    window->show();
    window->initialize();

    unsigned int c = 0;
    while (true) {
        c++;
        glViewport(0, 0, window->width(), window->height());


        glClearColor((c % 1000) / 1000.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        window->program->bind();
        window->vao.bind();
        glDrawArrays(GL_TRIANGLES, 0, 3);
        window->vao.release();

        window->openglContext->swapBuffers(window);
    }

    //PSX::Core core;

    //core.run();
}

