#include "../../framework.h"
#include "../vn_environment.h"

#include "light.h"

XMVECTOR	Light::Direction;	//平行光源の方向
XMVECTOR	Light::iDirection;	//平行光源の逆方向

XMVECTOR	Light::LightColor;	//平行光源の色
XMVECTOR	Light::Ambient;		//アンビエントの色

bool Light::initialize()
{
	Direction = XMVectorSet(-2.0f, -10.0f, -10.0f, 0.0f);

	iDirection = XMVector3Normalize(Direction);	//光源方向ベクトルの正規化
	iDirection = XMVectorNegate(iDirection);	//光源方向ベクトルの反転

	LightColor = XMVectorSet(0.8f, 0.8f, 0.8f, 1.0f);

	Ambient = XMVectorSet(0.2f, 0.2f, 0.2f, 0.0f);

	return true;
}

void Light::terminate()
{
}

XMVECTOR* Light::getILightDir()
{
	iDirection = XMVector3Normalize(Direction);
	iDirection = XMVectorNegate(iDirection);
	return &iDirection;
}

XMVECTOR* Light::getLightColor()
{
	return &LightColor;
}

XMVECTOR* Light::getAmbient()
{
	return &Ambient;
}

//取得
float Light::getLightDirX()
{
	return XMVectorGetX(Direction);
}

float Light::getLightDirY()
{
	return XMVectorGetY(Direction);
}

float Light::getLightDirZ()
{
	return XMVectorGetZ(Direction);
}

float Light::getLightColR()
{
	return XMVectorGetX(LightColor);
}

float Light::getLightColG()
{
	return XMVectorGetY(LightColor);
}

float Light::getLightColB()
{
	return XMVectorGetZ(LightColor);
}

float Light::getAmbientR()
{
	return XMVectorGetX(Ambient);
}

float Light::getAmbientG()
{
	return XMVectorGetY(Ambient);
}

float Light::getAmbientB()
{
	return XMVectorGetZ(Ambient);
}

//設定
void Light::setLightDir(float x, float y, float z)
{
	Direction = XMVectorSet(x, y, z, 0.0f);
}

void Light::setLightDirX(float value)
{
	Direction = XMVectorSetX(Direction, value);
}

void Light::setLightDirY(float value)
{
	Direction = XMVectorSetY(Direction, value);
}

void Light::setLightDirZ(float value)
{
	Direction = XMVectorSetZ(Direction, value);
}

void Light::setLightCol(float r, float g, float b)
{
	LightColor = XMVectorSet(r, g, b, 1.0f);
}

void Light::setLightColR(float value)
{
	LightColor = XMVectorSetX(LightColor, value);
}

void Light::setLightColG(float value)
{
	LightColor = XMVectorSetY(LightColor, value);
}

void Light::setLightColB(float value)
{
	LightColor = XMVectorSetZ(LightColor, value);
}

void Light::setAmbient(float r, float g, float b)
{
	Ambient = XMVectorSet(r, g, b, 0.0f);
}

void Light::setAmbientR(float value)
{
	Ambient = XMVectorSetX(Ambient, value);
}

void Light::setAmbientG(float value)
{
	Ambient = XMVectorSetY(Ambient, value);
}

void Light::setAmbientB(float value)
{
	Ambient = XMVectorSetZ(Ambient, value);
}

//加算
void Light::addLightDir(float x, float y, float z)
{
	Direction = XMVectorAdd(Direction, XMVectorSet(x, y, z, 0.0f));
}

void Light::addLightDirX(float value)
{
	Direction = XMVectorAdd(Direction, XMVectorSet(value, 0.0f, 0.0f, 0.0f));
}

void Light::addLightDirY(float value)
{
	Direction = XMVectorAdd(Direction, XMVectorSet(0.0f, value, 0.0f, 0.0f));
}

void Light::addLightDirZ(float value)
{
	Direction = XMVectorAdd(Direction, XMVectorSet(0.0f, 0.0f, value, 0.0f));
}

void Light::addLightCol(float r, float g, float b)
{
	LightColor = XMVectorAdd(LightColor, XMVectorSet(r, g, b, 0.0f));
}

void Light::addLightColR(float value)
{
	LightColor = XMVectorAdd(LightColor, XMVectorSet(value, 0.0f, 0.0f, 0.0f));
}

void Light::addLightColG(float value)
{
	LightColor = XMVectorAdd(LightColor, XMVectorSet(0.0f, value, 0.0f, 0.0f));
}

void Light::addLightColB(float value)
{
	LightColor = XMVectorAdd(LightColor, XMVectorSet(0.0f, 0.0f, value, 0.0f));
}

void Light::addAmbient(float r, float g, float b)
{
	Ambient = XMVectorAdd(Ambient, XMVectorSet(r, g, b, 0.0f));
}

void Light::addAmbientR(float value)
{
	Ambient = XMVectorAdd(Ambient, XMVectorSet(value, 0.0f, 0.0f, 0.0f));
}

void Light::addAmbientG(float value)
{
	Ambient = XMVectorAdd(Ambient, XMVectorSet(0.0f, value, 0.0f, 0.0f));
}

void Light::addAmbientB(float value)
{
	Ambient = XMVectorAdd(Ambient, XMVectorSet(0.0f, 0.0f, value, 0.0f));
}
