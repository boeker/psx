#ifndef PSX_SCREEN_H
#define PSX_SCREEN_H

namespace PSX {

class Screen {
public:
    Screen() = default;
    Screen(const Screen &) = delete;
    virtual ~Screen() = default;

    virtual void swapBuffers() = 0;
};

}

#endif
