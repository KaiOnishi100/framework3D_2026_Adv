//--------------------------------------------------------------//
//	"vn_mouse.cpp"												//
//		マウス入力(DirectInput)									//
//													2026/01/01	//
//														Ichii	//
//--------------------------------------------------------------//
#include "../framework.h"
#include "vn_environment.h"

int	vnMouse::x = 0;
int	vnMouse::y = 0;
int	vnMouse::r = 0;
int	vnMouse::dx = 0;
int vnMouse::dy = 0;
int	vnMouse::On = 0;
int	vnMouse::Trg = 0;
int	vnMouse::Rel = 0;

int	vnMouse::getX(void){return x;}
int	vnMouse::getY(void){return y;}
int	vnMouse::getR(void){return r;}
int	vnMouse::getDX(void){return dx;}
int	vnMouse::getDY(void){return dy;}
bool vnMouse::on(eButton btn){return btn&On?true:false;}
bool vnMouse::trg(eButton btn){return btn&Trg?true:false;}
bool vnMouse::rel(eButton btn){return btn&Rel?true:false;}

void vnMouse::setMouseStatus(void)
{
	x = DirectInput::getMouseX();
	y = DirectInput::getMouseY();
	r = DirectInput::getMouseState()->lZ;
	dx = DirectInput::getMouseState()->lX;
	dy = DirectInput::getMouseState()->lY;
	int Old=On;
	On = 0;
	if(DirectInput::getMouseState()->rgbButtons[0])On|=LT;
	if(DirectInput::getMouseState()->rgbButtons[1])On|=RT;
	if(DirectInput::getMouseState()->rgbButtons[2])On|=MD;
	
	Trg = (On^Old)&On;
	Rel = (On^Old)&Old;
}

