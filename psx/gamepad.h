#ifndef PSX_GAMEPAD_H
#define PSX_GAMEPAD_H

#include <atomic>
#include <string>

namespace PSX {

class Gamepad {
private:
    std::atomic<bool> up;
    std::atomic<bool> down;
    std::atomic<bool> left;
    std::atomic<bool> right;

    std::atomic<bool> triangle;
    std::atomic<bool> cross;
    std::atomic<bool> square;
    std::atomic<bool> circle;

    std::atomic<bool> l1;
    std::atomic<bool> l2;
    std::atomic<bool> l3;
    std::atomic<bool> r1;
    std::atomic<bool> r2;
    std::atomic<bool> r3;

    std::atomic<bool> select;
    std::atomic<bool> start;

    enum State {
        IDLE,
        ACTIVE,
        ID_LO_SENT,
        ID_HI_SENT,
        SW_LO_SENT
        //SW_HI_SENT,
        //ADC0_SENT,
        //ADC1_SENT,
        //ADC2_SENT,
        //ADC3_SENT
    };
    std::string stateToString(State state);

    State state;
public:
    Gamepad();

    void reset();

    void setUp(bool pressed);
    void setDown(bool pressed);
    void setLeft(bool pressed);
    void setRight(bool pressed);

    void setTriangle(bool pressed);
    void setCross(bool pressed);
    void setSquare(bool pressed);
    void setCircle(bool pressed);

    void setL1(bool pressed);
    void setL2(bool pressed);
    void setL3(bool pressed);
    void setR1(bool pressed);
    void setR2(bool pressed);
    void setR3(bool pressed);

    void setSelect(bool pressed);
    void setStart(bool pressed);

    bool getUp() const { return up; };
    bool getDown() const { return down; };
    bool getLeft() const { return left; };
    bool getRight() const { return right; };

    bool getTriangle() const { return triangle; };
    bool getCross() const { return cross; };
    bool getSquare() const { return square; };
    bool getCircle() const { return circle; };

    bool getL1() const { return l1; };
    bool getL2() const { return l2; };
    bool getL3() const { return l3; };
    bool getR1() const { return r1; };
    bool getR2() const { return r2; };
    bool getR3() const { return r3; };

    bool getSelect() const { return select; };
    bool getStart() const { return start; };

    uint8_t send(uint8_t message);
    bool ackForLastByte();
};
}

#endif

