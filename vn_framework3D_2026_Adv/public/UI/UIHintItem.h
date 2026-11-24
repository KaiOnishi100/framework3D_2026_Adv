#pragma once

class vnScene;

//--------------------------------------------
// 「アイコン(50x50) + キー画像(下)」1セット
//--------------------------------------------
class UIHintItem
{
public:
    //アイコンとキーアイコン有りver & キーボードver
    void Create(vnScene* scene,
        float iconCX, float iconCY,
        const WCHAR* iconPath,
        const WCHAR* keyPath,
        float iconSize = 90.0f,
        float keyW = 50.0f, float keyH = 50.0f,
        int basePriority = 520);
    //アイコンとキーアイコン有りver & コントローラーver
    void Create(vnScene* scene,
        float iconCX, float iconCY,
        float keyCX,float keyCY,
        const WCHAR* iconPath,
        const WCHAR* keyPath,
        float iconSize = 90.0f,
        float keyW = 50.0f, float keyH = 50.0f,
        int basePriority = 520);

    //キー無しver
    void Create(vnScene* scene,
        float iconCX, float iconCY,
        const WCHAR* iconPath,
        float iconSize = 75,float basePriority = 520);

    void Destroy(vnScene* scene);
    void SetVisible(bool v);

    // 押しているかどうかでハイライト
    void UpdatePress(bool isDown, bool isTrigger, float dt);

    // on=true の間、interval秒ごとに Ripple.Trigger() を出す
    void SetAttention(bool on, float interval = 0.35f, bool triggerImmediately = true);

    void SetBaseColor(float r, float g, float b, float a = 0.85f);
    void ResetBaseColor() { SetBaseColor(1.0f, 1.0f, 1.0f, 0.85f); }

private:
    void SetColor(vnSprite* s, float r, float g, float b, float a);
   
private:
    vnSprite* mIcon = nullptr;
    vnSprite* mKey = nullptr;

    UIRipple mRipple;

    // 可視状態
    bool mVisible = true;

    bool  mAttention = false;
    float mAttentionInterval = 0.35f; // 何秒おきに出すか
    float mAttentionTimer = 0.0f;     // 次の発生までの残り

    float mBaseR = 1.0f;
    float mBaseG = 1.0f;
    float mBaseB = 1.0f;
    float mBaseA = 0.85f;
};