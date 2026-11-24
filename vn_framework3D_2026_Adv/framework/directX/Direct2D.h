#pragma once

class Direct2D
{
private:
	static ID3D11On12Device* pDevice11On12;
	static ID3D11DeviceContext* pDeviceContext11;

	static ID2D1Factory3* pD2dFactory;
	static ID2D1Device2* pD2dDevice;
	static ID2D1DeviceContext2* pD2dDeviceContext;
	static IDWriteFactory* pDWFactory;

	static ID3D11Resource* pWrappedBackBaffers[Direct3D::frameCount];
	static ID2D1Bitmap1* pD2dRenderTargets[Direct3D::frameCount];

public:
	//フレームワーク管理
	static int initialize(void);
	static void terminate(void);

	static void render();

	static ID2D1DeviceContext2* getDeviceContext();
	static IDWriteFactory* getDWFactory();

};