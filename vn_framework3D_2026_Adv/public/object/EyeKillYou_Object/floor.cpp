#include "../../../framework.h"
#include "../../../framework/vn_environment.h"

#include"floor.h"

Floor::Floor(const WCHAR* folder, const WCHAR* file, float scaleX, float scaleZ) : Object(folder, file)
{
	//初期位置の決定
	XMVECTOR pos = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
	setPosition(&pos);
	setScale(scaleX, 1.0f, scaleZ);
	//コライダーのサイズを決定
	halfsize = XMVectorSet(scaleX, 1.0f, scaleZ, 0.0f);
	offset = XMVectorSet(0.0f, -15.8f, 0.0f, 0.0f);
	//コライダーの大きさを決定　
	mCollider = new BoxCollider(this, halfsize);
}
