#pragma once

#include "../framework/vn_motionData.h"

class vnCharacter : public ObjectBase
{
private:
	//パーツ数
	int PartsNum;

	//パーツごとのオブジェクトデータ
	ObjectBase** pParts;

	//ボーンデータ
	vnModel_BoneData* pBoneData;

	//スキニング変換用パーツワールドマトリクス
	XMMATRIX* pPartsMatrix;

	//適用中のモーションデータ
	vnMotionData* pMotion;

	//適用中のチャンネルデータ
	struct stBoneMotion
	{
		ObjectBase* pBone;
		vnMotionData_Channel* pChannel;
		vnMotionData_KeyFrame* pKey;
	};
	stBoneMotion* pBoneChannel;

	//モーションの再生/停止状態
	bool Play;
	//モーションの再生位置
	float Time;
	//モーションの再生速度
	float Speed;

	//現在のフレームでのモーションの計算の必要性
	bool updateMotion;

	//モーションの適用
	void applyMotion();

	//指定した名前とチャンネルを持つチャンネル情報を取得
	vnMotionData_Channel* getMotionChannel(const vnMotionData* motion, eMotionChannel channel, const WCHAR* name);

	//指定したボーンに適用されているモーションチャンネルがもつキーフレーム配列の先頭アドレスおよびキーフレーム数を取得
	int getKeyFrames(const vnModel_BoneData* bone, const vnMotionData* motion, eMotionChannel channel, vnMotionData_KeyFrame** out);

public:
	vnCharacter(const WCHAR* folder, const WCHAR* file);
	~vnCharacter();

	virtual void execute();
	virtual void render();

	virtual void setRenderEnable(bool flag);

	virtual void useQuaternion(bool flag);

	void registerObject();

	void setCastShadow(bool flag)override;

	void setReceiveShadow(bool flag)override;

	//パーツモデルの取得
	ObjectBase* getParts(int id);
	ObjectBase* getParts(const WCHAR *name);
	const WCHAR* getPartsName(int id);
	int getPartsNum();

	//バインドポーズに戻す(全パーツ)
	void bindPose();
	//バインドポーズに戻す(指定パーツ)
	void bindPose(int id);



	//モーションデータを設定
	void setMotion(vnMotionData* motion);

	//モーションデータが適用されているか
	bool hasMotion();
	//モーションデータの取得
	vnMotionData* getMotion();
	//モーションデータの長さ(モーションデータがなければ0)
	float getMotionLength();

	//モーションの再生状態の設定
	void playMotion(bool play = true);
	//モーションの一時停止
	void pauseMotion();

	//モーションの再生状態の取得
	bool isPlayMotion();

	//モーションの再生位置の取得
	float getTime();
	//モーションの再生位置の設定
	void setTime(float time);

	//モーションの再生速度の取得
	float getPlaySpeed();
	//モーションの再生速度の設定
	void setPlaySpeed(float speed = 1.0f);

	static vnMotionData* loadMotionFile(const WCHAR* path);
};