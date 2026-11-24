#include "../../framework.h"
#include "../../framework/vn_environment.h"

#include "tutorialSettings.h"

static std::wstring GetExeDir()
{
    wchar_t buf[MAX_PATH]{};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);

    std::wstring path = buf;
    const size_t pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos) path.resize(pos + 1);
    return path;
}

std::wstring TutorialSettings::GetSettingPath()
{
    // exeÇÃâ°Ç…ï€ë∂Åiämé¿Ç…ìØÇ∂èÍèäÇå©ÇÈÅj
    return GetExeDir() + L"tutorial.ini";
}

void TutorialSettings::SaveEnabled(bool enabled)
{
    const std::wstring path = GetSettingPath();
    WritePrivateProfileStringW(
        L"Game", L"TutorialEnabled",
        enabled ? L"1" : L"0",
        path.c_str()
    );
}

bool TutorialSettings::LoadEnabled(bool defaultValue)
{
    const std::wstring path = GetSettingPath();

    const int def = defaultValue ? 1 : 0;
    const int v = GetPrivateProfileIntW(
        L"Game", L"TutorialEnabled",
        def,
        path.c_str()
    );
    return (v != 0);
}
