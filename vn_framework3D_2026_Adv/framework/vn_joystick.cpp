//--------------------------------------------------------------//
//	"vn_joystick.cpp"											//
//		ジョイスティック入力(DirectInput)						//
//													2026/01/01	//
//														Ichii	//
//--------------------------------------------------------------//
#include "../framework.h"
#include "vn_environment.h"

bool vnJoystick::isValid(int id)
{
	return DirectInput::useJoystick(id);
}

bool vnJoystick::on(int button, int id)
{
	if(button<0 || button>=DirectInput::getJoystickButtonNum())return false;
	DIJOYSTATE *js = DirectInput::getCurrentJoystick(id);
	return js->rgbButtons[button]!=0;
}

bool vnJoystick::trg(int button, int id)
{
	if(button<0 || button>= DirectInput::getJoystickButtonNum())return false;
	DIJOYSTATE *jsc = DirectInput::getCurrentJoystick(id);
	DIJOYSTATE *jso = DirectInput::getOldJoystick(id);
	return jsc->rgbButtons[button]!=0 && jso->rgbButtons[button]==0;
}

bool vnJoystick::rel(int button, int id)
{
	if(button<0 || button>= DirectInput::getJoystickButtonNum())return false;
	DIJOYSTATE *jsc = DirectInput::getCurrentJoystick(id);
	DIJOYSTATE *jso = DirectInput::getOldJoystick(id);
	return jsc->rgbButtons[button]==0 && jso->rgbButtons[button]!=0;
}

float vnJoystick::leftX(int id)
{
	DIJOYSTATE *js = DirectInput::getCurrentJoystick(id);
	return (float)js->lX*DirectInput::getJoystickAxisRangeI();
}

float vnJoystick::leftY(int id)
{
	DIJOYSTATE *js = DirectInput::getCurrentJoystick(id);
	return (float)js->lY*DirectInput::getJoystickAxisRangeI();
}

float vnJoystick::rightX(int id)
{
	DIJOYSTATE *js = DirectInput::getCurrentJoystick(id);
	return (float)js->lZ*DirectInput::getJoystickAxisRangeI();
}

float vnJoystick::rightY(int id)
{
	DIJOYSTATE *js = DirectInput::getCurrentJoystick(id);
	return (float)js->lRz*DirectInput::getJoystickAxisRangeI();
}

int vnJoystick::pov(int id)
{
	DIJOYSTATE *js = DirectInput::getCurrentJoystick(id);
	return js->rgdwPOV[0];
}
