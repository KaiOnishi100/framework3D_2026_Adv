#pragma once

class vnSound
{
private:
	IXAudio2SourceVoice* pSourceVoice = nullptr;
	XAUDIO2_BUFFER xbuf{};              // 必ず0初期化しておく
	bool mLooping = false;              // ループ再生フラグ

	UINT64 lastSamplesPlayed = 0;
	UINT64 lastSamplesStopped = 0;
	WAVEFORMATEX wfex{};

public:

	struct SpatialMix
	{
		UINT32 channelCount = 0;
		std::array<float, XAUDIO2_MAX_AUDIO_CHANNELS> gains{};
	};


	//コンストラクタ
	vnSound(const WCHAR *path);
	//デストラクタ
	~vnSound();
	
	bool play(bool start=true);	//再生	start=true : 先頭から再生, start=false : 前回の停止位置から再生
	bool playLoop(bool start = true);   // ループ再生
	bool stop();				//停止
	
	bool isPlaying();	//再生中か調べる
	bool isStopped();	//停止中か調べる

	// WAVとSource Voiceの準備ができたか確認する
	bool IsLoaded() const
	{
		return pSourceVoice != nullptr;
	}


	//各種設定
	bool setLooping(bool looping);      // ループ設定切り替え（内部でバッファ再提出）
	bool setVolume(float volume);			//減衰 : DSBVOLUME_MAX (減衰なし:0) から DSBVOLUME_MIN (無音:-10,000)
	bool setFrequencyRatio(float frequency);//オーディオ バッファが再生されている周波数(Hz 単位) / デフォルト値 : DSBFREQUENCY_ORIGINAL
	
	//各種取得
	float getVolume();
	float getFrequencyRatio();

	float getPlayingTime();

	float getCurrentPlayingPosition();

	static float convertSecond(UINT64 data, int samplesPerSec, int bitsPerSample, int channels);

	static bool InitializeSpatialAudio(IXAudio2MasteringVoice* masterVoice);

	static SpatialMix BuildSpatialMix(
		const XMFLOAT3& listenerPosition,
		const XMFLOAT3& listenerForward,
		const XMFLOAT3& listenerUp,
		const XMFLOAT3& emitterPosition,
		float audibleRange
	);

	void ApplySpatialMix(const SpatialMix& mix);


};

