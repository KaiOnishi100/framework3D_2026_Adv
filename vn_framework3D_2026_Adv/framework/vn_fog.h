//--------------------------------------------------------------//
//	"vn_fog.h"													//
//		フォグ													//
//													2026/04/01	//
//														Ichii	//
//--------------------------------------------------------------//
#pragma once

class vnFog
{
private:
	static bool Enable;	//有効フラグ

	static XMVECTOR	Color;	//色
	
	static XMVECTOR	Info;	//情報

	static float Start;
	static float End;
	static float Density;

	static void setInfo();
	
public:
	static bool initialize();
	static void terminate();

	//シェーダー関連
	static XMVECTOR	*getColor();
	static XMVECTOR	*getInfo();

	//有効フラグ
	static bool isEnable();
	static void setEnable(bool value);

	//取得
	static float getStart();
	static float getEnd();
	static float getDensity();
	static float getR();
	static float getG();
	static float getB();
	
	//設定
	static void setStart(float value);
	static void setEnd(float value);
	static void setDensity(float value);
	static void setColor(float r, float g, float b);
	static void setR(float value);
	static void setG(float value);
	static void setB(float value);
	
	//加算
	static void addStart(float value);
	static void addEnd(float value);
	static void addDensity(float value);
	static void addR(float value);
	static void addG(float value);
	static void addB(float value);
	
};
