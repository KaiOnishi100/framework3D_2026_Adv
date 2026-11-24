#pragma once
#include"UIInputCommand.h"

class UIInputConfig
{
public:
	static void SetupTitle(UIInputHandler& h, int padId = 0);

	static void SetupResult(UIInputHandler& h, int padId = 0);

	static void SetupPause(UIInputHandler& h, int padId = 0);

	static void SetupMap(UIInputHandler& h,int padId = 0);

	static void SetupInventory(UIInputHandler& h,int padId = 0);

	static void SetupExplorationMenu(UIInputHandler& h,int padId = 0);
};