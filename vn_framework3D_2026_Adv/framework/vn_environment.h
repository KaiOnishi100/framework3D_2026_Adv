//--------------------------------------------------------------//
//フレームワーク共通環境ヘッダー								//
//--------------------------------------------------------------//
#pragma once

//ライブラリ
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "XAudio2.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "xinput.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "bcrypt.lib")

#if (_MSC_VER>=1930)

//Visual Studio 2022
#ifdef _DEBUG
#pragma comment(lib, "DirectXTex/Bin/Desktop_2022_Win10/x64/Debug/DirectXTex.lib")
#else
#pragma comment(lib, "DirectXTex/Bin/Desktop_2022_Win10/x64/Release/DirectXTex.lib")
#endif

#else

//Visual Studio 2019
#ifdef _DEBUG
#pragma comment(lib, "DirectXTex/Bin/Desktop_2019_Win10/x64/Debug/DirectXTex.lib")
#else
#pragma comment(lib, "DirectXTex/Bin/Desktop_2019_Win10/x64/Release/DirectXTex.lib")
#endif

#endif

//共通ヘッダファイルのインクルード
#include <stdio.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <queue>
#include <assert.h>
#include <mmsystem.h>
#include <locale.h>
#include <time.h>
#include <random>
#include <chrono>
#include <map>
#include <algorithm>
#include <cmath>
#include <functional>
#include <array>
#include <bcrypt.h>
#include <mutex>
#include <cwctype>
#include <atomic>
#include <thread>
#include <future>
#include <pix3.h>
#include <wrl/client.h>


//DirectXヘッダー
#include <d2d1_3.h>
#include <dwrite.h>
#include <d3d11on12.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3d12sdklayers.h>
#include <dxgidebug.h>

#define	DIRECTINPUT_VERSION	0x0800
#include <dinput.h>
#include <Xinput.h>
#include <xaudio2.h>
#include <x3daudio.h>


#include <DirectXMath.h>
#include <DirectXCollision.h>
#include "../DirectXTex/DirectXTex.h"

//ネームスペース
using namespace DirectX;
using Microsoft::WRL::ComPtr;

//一般マクロ
#define SCREEN_WIDTH	(1920)	//クライアント領域の幅(ピクセル)
#define SCREEN_HEIGHT	(1080)	//クライアント領域の高さ(ピクセル)

//リリース
#define SAFE_RELEASE(p) {if(p){(p)->Release();(p)=NULL;}}

#include "calculate.h"
#include "PBRDebug.h"

//フレームワークヘッダー
#include "directX/shader.h"
#include "directX/Direct3D.h"
#include "directX/Direct2D.h"
#include "directX/DirectInput.h"
#include "directX/vn_XInput.h"
#include "directX/vn_XAudio2.h"


//ポストプロセス用ヘッダー
#include "render_format.h"
#include "postProcess/pingPongPostRT.h"
#include "postProcess/bloomPass.h"
#include "postProcess/vignettePass.h"
#include "postProcess/colorGradingPass.h"
#include "postProcess/fxaaPass.h"
#include "postProcess/chromaticAberrationPass.h"
#include "postProcess/toneMappingPass.h"
#include "postProcess/lensFlarePass.h"
#include "postProcess/motionVectorPass.h"
#include "postProcess/motionBlurPass.h"
#include "postProcess/volumetricFogPass.h"
#include "postProcess/subsurfaceScatteringPass.h"
#include "postProcess/SSGIPass/giSurfacePass.h"
#include "postProcess/SSGIPass/ssgiPass.h"
#include "postProcessManager.h"

#include "vn_mouse.h"
#include "vn_keyboard.h"
#include "vn_joystick.h"
#include "vn_sound.h"
#include "SoundPool.h"
#include "SoundBank.h"
#include "pad.h"

//light用ヘッダー
#include "light/light.h"
#include "light/localLightManager.h"
#include "light/AtmosphericEscape/AtmosphericEscapeLightBudget.h"

#include "vn_font.h"

#include "vn_camera.h"
#include "cinematicCamera.h"
#include "vn_fog.h"
#include "shadow/shadowMap.h"
#include "shadow/spotLightShadowManager.h"
#include "shadow/pointLightShadowManager.h"
#include "shadowMapManager.h"
#include "environmentMapManager.h"
#include "vn_debugDraw.h"
#include "AEUIFont.h"

//interface関連
#include "../public/interface/IPersistentWorldObject.h"

#include "../public/objectBase.h"
#include "../public/modelBase.h"
#include "../public/vn_billboard.h"
#include "../public/emitter.h"
#include "../public/vn_character.h"
#include "../public/vn_sprite.h"
#include "../public/vn_collide.h"
#include "../public/direction.h"
#include "../public/object.h"
#include "../public/slowSystem.h"
#include "../public/motionData.h"
#include "../public/skeletalModel.h"
#include "../public/skeletalAnimationController.h"

#include "../public/input/allInput.h"
#include "../public/effect/allEffect.h"
#include "../public/object/EyeKillYou_Object/allObject.h"
#include "../public/tutorial/allTutorial.h"
#include "../public/collide/allCollider.h"
#include "../public/collide/capsuleCollider.h"
#include "../public/UI/allUI.h"


#include "../public/world/AtmosphericEscapeWorld/atmosphericEscapeWorld.h"


#include "../public/scene/vn_scene.h"
#include "../public/scene/scene_table.h"



#include "vn_mainframe.h"

void SetGameplayCursorHidden(bool hidden);