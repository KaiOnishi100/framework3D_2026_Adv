#pragma once

#include "../../framework/vn_environment.h"
#include "../world/AtmosphericEscapeWorld/save/atmosphericEscapeSaveData.h"

//============================================================
// IPersistentWorldObject
// Save対象となるWorld Objectが実装するInterface
//============================================================
class IPersistentWorldObject
{
public:

    virtual ~IPersistentWorldObject() = default;

    //========================================================
    // Persistent ID
    //========================================================
    virtual const std::string& GetPersistentId() const = 0;


    //========================================================
    // Save
    //
    // Runtime State
    //      ↓
    // WorldObjectSaveState
    //========================================================
    virtual WorldObjectSaveState CaptureSaveState() const = 0;


    //========================================================
    // Load
    //
    // WorldObjectSaveState
    //      ↓
    // Runtime State
    //========================================================
    virtual void RestoreSaveState(const WorldObjectSaveState& state) = 0;
};