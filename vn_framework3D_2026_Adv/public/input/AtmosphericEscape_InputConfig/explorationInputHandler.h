#pragma once
class IExplorationControllable;


//============================================================
// ExplorationInputHandler
// Atmospheric Escape専用Input
//============================================================
class ExplorationInputHandler
{
public:

    //========================================================
    // Input Update
    // padId: 使用するGamePad番号
    //========================================================
    void HandleInput(IExplorationControllable* target, int padId = 0);
};