#pragma once

class Collider;

class Object : public ModelBase
{
public:
	Object(const WCHAR* folder, const WCHAR* file) : ModelBase(folder, file), mCollider(nullptr) {};
	//キャラクター取得用
	virtual Character* AsCharacter() { return nullptr; }
	//シーン登録用
	Collider* GetCollider() const { return mCollider; }
protected:

	Collider* mCollider;
	//コライダーとモデルがずれている時の調整用
	XMVECTOR offset = XMVectorZero();
};