#ifndef PSX_RENDERER_SCREEN_H
#define PSX_RENDERER_SCREEN_H

namespace PSX {

class Screen {
public:
    Screen() = default;
    Screen(const Screen &) = delete;
    virtual ~Screen() = default;

    virtual int getHeight() = 0;
    virtual int getWidth() = 0;
    virtual void setUpViewport() = 0;
    virtual void swapBuffers() = 0;
};

}

#endif
