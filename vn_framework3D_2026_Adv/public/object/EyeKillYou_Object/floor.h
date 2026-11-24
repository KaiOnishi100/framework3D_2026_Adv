#pragma once

class Floor : public Object
{
public:
	Floor(const WCHAR* folder, const WCHAR* file ,float scaleX,float scaleZ);

private:
	XMVECTOR halfsize; //コライダーの大きさ
};