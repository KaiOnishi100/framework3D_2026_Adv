#include "../../framework.h"
#include "../../framework/vn_environment.h"

#include "UIHintItem.h"

void UIHintItem::Create(vnScene* scene, 
    float iconCX, float iconCY,
    const WCHAR* iconPath, const WCHAR* keyPath, 
    float iconSize,
    float keyW, float keyH, 
    int basePriority)
{

    // アイコン
    mIcon = new vnSprite(iconCX, iconCY, iconSize, iconSize, iconPath);
    mIcon->setRenderPriority(basePriority);

    // キー（アイコンの少し下）
    mKey = new vnSprite(iconCX, iconCY + iconSize * 0.70f, keyW, keyH, keyPath);
    mKey->setRenderPriority(basePriority - 1);

    // リップル（リング）
    mRipple.Initialize(scene, L"data/image/UI/Circle.png", basePriority - 2);
    mRipple.SetBaseSize(iconSize);
    mRipple.SetCenter(iconCX, iconCY);
    mRipple.SetParams(0.28f, 0.55f, 1.05f, 1.65f);

    scene->registerObject(mIcon);
    scene->registerObject(mKey);
    // 通常色
    SetColor(mIcon, 1.0f, 1.0f, 1.0f, 0.85f);
    SetColor(mKey, 1.0f, 1.0f, 1.0f, 0.85f);

    mVisible = true;
    mRipple.SetVisibleAll(false);
}

void UIHintItem::Create(vnScene* scene,
    float iconCX, float iconCY,
    float keyCX, float keyCY, const
    WCHAR* iconPath, const WCHAR* keyPath,
    float iconSize,
    float keyW, float keyH,
    int basePriority)
{

    // アイコン
    mIcon = new vnSprite(iconCX, iconCY, iconSize, iconSize, iconPath);
    mIcon->setRenderPriority(basePriority);

    // キー
    mKey = new vnSprite(keyCX, keyCY, keyW, keyH, keyPath);
    mKey->setRenderPriority(basePriority - 1);

    // リップルはアイコン中心
        // リップル（リング）
    mRipple.Initialize(scene, L"data/image/UI/Circle.png", basePriority - 2);
    mRipple.SetBaseSize(iconSize);
    mRipple.SetCenter(iconCX, iconCY);
    mRipple.SetParams(0.28f, 0.55f, 1.05f, 1.65f);

    scene->registerObject(mIcon);
    scene->registerObject(mKey);

    // 通常色
    SetColor(mIcon, 1.0f, 1.0f, 1.0f, 0.85f);
    SetColor(mKey, 1.0f, 1.0f, 1.0f, 0.85f);

    mVisible = true;
    mRipple.SetVisibleAll(false);
}

void UIHintItem::Create(vnScene* scene, float iconCX, float iconCY, const WCHAR* iconPath, float iconSize, float basePriority)
{

    mIcon = new vnSprite(iconCX, iconCY, iconSize, iconSize, iconPath);
    mIcon->setRenderPriority((int)basePriority);

    scene->registerObject(mIcon);

    // リップル（リング）
    mRipple.Initialize(scene, L"data/image/UI/Circle.png", (int)basePriority - 2);
    mRipple.SetBaseSize(iconSize);
    mRipple.SetCenter(iconCX, iconCY);
    // 好みで調整（今の値そのまま）
    mRipple.SetParams(0.28f, 0.55f, 1.05f, 1.65f);

    SetColor(mIcon, 1.0f, 1.0f, 1.0f, 0.85f);

    mVisible = true;
    mRipple.SetVisibleAll(false);
}

void UIHintItem::Destroy(vnScene* scene)
{
    if (!scene) return;
    if (mIcon) scene->deleteObject(mIcon);
    if (mKey)  scene->deleteObject(mKey);
    mRipple.Terminate(scene);

    mIcon = mKey = nullptr;
    mVisible = false;
}

void UIHintItem::SetVisible(bool v)
{
    mVisible = v;

    if (mIcon) { mIcon->setRenderEnable(v);   mIcon->setExecuteEnable(v); }
    if (mKey) { mKey->setRenderEnable(v);    mKey->setExecuteEnable(v); }
    mRipple.SetVisibleAll(v);
}

void UIHintItem::UpdatePress(bool isDown, bool isTrigger, float dt)
{
    if (!mVisible) return;

    // -------------------------
    // 押してる時は明るくする
    // -------------------------
    const float bright = isDown ? 1.25f : 1.0f;
    const float a = isDown ? 1.00f : mBaseA;

    SetColor(mIcon, mBaseR * bright, mBaseG * bright, mBaseB * bright, a);
    if (mKey) SetColor(mKey, mBaseR * bright, mBaseG * bright, mBaseB * bright, a);

    // -------------------------
    // 押した瞬間にリップル開始
    // -------------------------
    if (isTrigger)
    {
        mRipple.Trigger();
    }

    // -------------------------
    // 注目モード：一定間隔で自動Trigger
    // -------------------------
    if (mAttention)
    {
        mAttentionTimer -= dt;
        if (mAttentionTimer <= 0.0f)
        {
            mRipple.Trigger();
            mAttentionTimer = mAttentionInterval;
        }
    }

    // -------------------------
    // リップル更新（サイズ拡大＋フェードアウト）
    // -------------------------
    mRipple.Update(dt);
}

void UIHintItem::SetAttention(bool on, float interval, bool triggerImmediately)
{
    if (mAttention == on)return;

    mAttention = on;
    // 0除算など防止
    mAttentionInterval = (interval >= 0.10f) ? interval : 0.10f;

    if (!mAttention)
    {
        // 注目OFF時：タイマーを初期化して終了
        mAttentionTimer = 0.0f;
        return;
    }

    // 注目ON時：すぐ出す or 次周期から出す
    if (triggerImmediately)
    {
        mRipple.Trigger();
        mAttentionTimer = mAttentionInterval;
    }
    else
    {
        mAttentionTimer = 0.0f; // 次Updateで即出す
    }
}

void UIHintItem::SetBaseColor(float r, float g, float b, float a)
{
    mBaseR = r;
    mBaseG = g;
    mBaseB = b;
    mBaseA = a;
}

void UIHintItem::SetColor(vnSprite* s, float r, float g, float b, float a)
{
    if (!s) return;
    for (int i = 0; i < vnSprite::VertexNum; ++i)
    {
        s->vtx[i].r = r;
        s->vtx[i].g = g;
        s->vtx[i].b = b;
        s->vtx[i].a = a;
    }
}
