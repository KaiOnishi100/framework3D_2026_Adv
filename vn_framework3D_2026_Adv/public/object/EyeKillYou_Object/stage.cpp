#include "../../../framework.h"
#include "../../../framework/vn_environment.h"

#include "stage.h"

Stage::Stage(const WCHAR* folder, const WCHAR* file) : Object(folder,file)
{
	setPosition(0.0f,0.0f,0.0f);
	mCollider = nullptr;
}