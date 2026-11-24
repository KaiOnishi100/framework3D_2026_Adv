#pragma once

//İ’è•Û‘¶—pƒNƒ‰ƒX
class TutorialSettings
{
public:
    static void SaveEnabled(bool enabled);
    static bool LoadEnabled(bool defaultValue = true);
private:
    static std::wstring GetSettingPath();
};