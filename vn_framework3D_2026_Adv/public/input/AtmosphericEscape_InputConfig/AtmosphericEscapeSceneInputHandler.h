#pragma once


//============================================================
// AtmosphericEscapeSceneInputHandler
// Scene全体に関係するInputを扱う
// ※Player操作とは分離する
//============================================================
class AtmosphericEscapeSceneInputHandler
{
public:

    //========================================================
    // Pause Trigger
    //
    // Keyboard : Tab
    // GamePad  : START / Options
    //========================================================
    bool IsPauseTriggered(int padId = 0) const;

    bool IsDebugLoadTriggered(int padId = 0) const;

    bool IsMapTriggered(int padId = 0) const;

    bool IsInventoryTriggered(int padId = 0) const;
};