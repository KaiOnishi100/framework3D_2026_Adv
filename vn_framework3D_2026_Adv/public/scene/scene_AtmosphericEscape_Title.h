#pragma once


class SceneAtmosphericEscapeTitle : public vnScene
{
public:
	bool initialize();
	void terminate();
	void execute();
	void render();

private:
	// タイトル専用UI
	TitleUI mTitleUI;
	UIInputHandler mTitleInput;              // タイトルの選択・決定
	UIAtmosphericEscapeOptions mOptionsUI;   // オプション画面
	UIInputHandler mOptionsInput;            // オプション表示中の入力

	// 毎フレームの経過時間
	std::chrono::steady_clock::time_point mPrevTime;
};