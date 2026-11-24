//--------------------------------------------------------------//
//	"vn_fog.h"													//
//		フォグ													//
//													2026/04/01	//
//														Ichii	//
//--------------------------------------------------------------//
#include "../framework.h"
#include "vn_environment.h"

bool		vnFog::Enable;	//有効フラグ
XMVECTOR	vnFog::Color;	//色
XMVECTOR	vnFog::Info;	//情報
float		vnFog::Start;
float		vnFog::End;
float		vnFog::Density;

bool vnFog::initialize()
{
	Enable = false;

	Color = XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f);

	Start = 25.0f;
	End = 50.0f;
	Density = 1.0f;

	setInfo();
	return true;
}

void vnFog::terminate()
{
}

void vnFog::setInfo()
{
	Info = XMVectorSet(Start, End, Density, 1.0f / (End - Start));
}

XMVECTOR	*vnFog::getColor(){ return &Color; }
XMVECTOR	*vnFog::getInfo() { setInfo(); return &Info; }

//有効フラグ
bool vnFog::isEnable() { return Enable; }
void vnFog::setEnable(bool value) { Enable = value; }

//取得
float vnFog::getStart() { return Start; }

float vnFog::getEnd() { return End; }

float vnFog::getDensity() { return Density; }

float vnFog::getR()
{
	return XMVectorGetX(Color);
}

float vnFog::getG()
{
	return XMVectorGetY(Color);
}

float vnFog::getB()
{
	return XMVectorGetZ(Color);
}

//設定
void vnFog::setStart(float value) { Start = value; }

void vnFog::setEnd(float value) { End = value; }

void vnFog::setDensity(float value) { Density = value; }

void vnFog::setColor(float r, float g, float b)
{
	Color = XMVectorSet(r, g, b, 0.0f);
}

void vnFog::setR(float value)
{
	Color = XMVectorSetX(Color, value);
}

void vnFog::setG(float value)
{
	Color = XMVectorSetY(Color, value);
}

void vnFog::setB(float value)
{
	Color = XMVectorSetZ(Color, value);
}

//加算
void vnFog::addStart(float value) { Start += value; }

void vnFog::addEnd(float value) { End += value; }

void vnFog::addDensity(float value) { Density += value; }

void vnFog::addR(float value)
{
	Color = XMVectorAdd(Color, XMVectorSet(value, 0.0f, 0.0f, 0.0f));
}

void vnFog::addG(float value)
{
	Color = XMVectorAdd(Color, XMVectorSet(0.0f, value, 0.0f, 0.0f));
}

void vnFog::addB(float value)
{
	Color = XMVectorAdd(Color, XMVectorSet(0.0f, 0.0f, value, 0.0f));
}
