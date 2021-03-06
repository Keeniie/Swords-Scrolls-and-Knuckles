#ifndef JOYSTICK_HPP_
# define JOYSTICK_HPP_

# include <functional>
# include <OISJoyStick.h>
# include <array>
# include <memory>
# include "Input.hpp"

enum joystickState
{
    JS_UNASSIGNED = 0x00,
    JS_A = 0x01,
    JS_B = 0x02,
    JS_X = 0x03,
    JS_Y = 0x04,
    JS_LB = 0x05,
    JS_RB = 0x06,
    JS_SELECT = 0x07,
    JS_START = 0x08,
    JS_MENU = 0x09,
    JS_LJS = 0x0A,
    JS_RJS = 0x0B,
    JS_LLEFT = 0x0C,
    JS_LRIGHT = 0x0D,
    JS_LUP = 0x0E,
    JS_LDOWN = 0x0F,
    JS_LT = 0x10,
    JS_RLEFT = 0x11,
    JS_RRIGHT = 0x12,
    JS_RUP = 0x13,
    JS_RDOWN = 0x14,
    JS_RT = 0x15,
};

enum joystickAxe
{
    LEFT_HRZ = 0x00,
    LEFT_VRT = 0x01,
    LEFT_TOP = 0x02,
    RIGHT_HRZ = 0x03,
    RIGHT_VRT = 0x04,
    RIGHT_TOP = 0x05,
};

class Joystick : public Input<OIS::JoyStick, OIS::JoyStickListener>
{
    std::map<joystickState, std::function<void(bool, size_t)>> statesCallbacks;
    std::map<joystickState, int> states;
    std::map<joystickAxe, int> axes;
    size_t idx;

    /// Singleton instance
    static std::array<std::unique_ptr<Joystick>, 4> joystickInstances;

  protected:
    /// OIS::JoyStickListener
    virtual bool buttonPressed(OIS::JoyStickEvent const &arg, int button) override;
    virtual bool buttonReleased(OIS::JoyStickEvent const &arg, int button) override;
    virtual bool axisMoved(OIS::JoyStickEvent const &arg, int axis) override;

  public:
    Joystick(void) = delete;
    Joystick(size_t);
    Joystick(Joystick const &) = delete;
    Joystick(Joystick &&) = delete;
    virtual ~Joystick(void) = default;
    Joystick &operator=(Joystick const &) = delete;
    Joystick &operator=(Joystick &&) = delete;

    /// Register a state callback
    /// This callback takes in parameter a boolean, true if it's a current state,
    /// otherwise false.
    /// It should return false if the game should exit, true otherwise.
    void registerCallback(joystickState state, std::function<void(bool, size_t)> const &);

    /// Clear all Button callback
    void clearCallbacks(void);

    /// Joystick singleton getter & instance
    static std::array<std::unique_ptr<Joystick>, 4> &getJoysticks(void);
    static void registerGlobalCallback(joystickState state, std::function<void(bool, size_t)> const &);
    static void clearGlobalCallbacks(void);

    /// Equivalent to isKeyDown for JoyStick states.
    bool isStateUp(joystickState js) const;

    std::map<joystickAxe, int> &getAxes(void);

    std::map<joystickAxe, int> const &getAxes(void) const;

    bool operator[](joystickState) const;
};

#endif // !JOYSTICK_HPP_
