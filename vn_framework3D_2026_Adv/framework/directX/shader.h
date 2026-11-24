#pragma once

class Shader
{
public:

	enum class eVertexShader
	{
		VS_3D,
		VS_3D_Skin,
		VS_3D_Line,
		VS_2D,
		VS_Copy,
		VS_Gray,		
		VS_Shadow,
		VS_ShadowSkin,
		VS_Skybox,
		VS_PrefilterEnvironment,
		VS_BRDFLUT,
		VS_ObjectMotion,
		VS_ObjectMotionSkin,
		VS_MAX,
	};

	enum class ePixelShader
	{
		PS_3D,
		PS_3D_NoLight,
		PS_3D_Line,
		PS_2D,
		PS_2D_HDR,
		PS_Copy,
		PS_Gray,

		//Bloom用 ポストプロセスシェーダー
		PS_BloomBright,
		PS_BloomBlurH,
		PS_BloomBlurV,
		PS_BloomComposite,

		//Vignette用 ポストプロセスシェーダー
		PS_Vignette,

		//ColorGrading用 ポストプロセスシェーダー
		PS_ColorGrading,

		//FXAA用 ポストプロセスシェーダー
		PS_Fxaa,

	    // Chromatic Aberration用 ポストプロセスシェーダー
	    // 色収差
		PS_ChromaticAberration,

		//トーンマッピング用
		PS_ToneMapping,

		// Environment Skybox
		PS_Skybox,

		// Equirectangular HDR → CubeMap
		PS_HDRToCube,

		// Environment Prefilter生成用
		PS_PrefilterEnvironment,

		// Diffuse IBL用Irradiance CubeMap生成
		PS_IrradianceEnvironment,

		// Split-Sum BRDF LUT生成用
		PS_BRDFLUT,

		// Lens Flare用
		PS_LensFlare,
		PS_LensFlareOcclusion,

		// Half Resolution Effect
		PS_EffectDepthDownsample,
		PS_EffectHalfComposite,

		// Motion Vector
		PS_MotionVector,
		PS_MotionVectorDebug,

		// Motion Blur
		PS_MotionBlur,

		// Object Motion Vector
		PS_ObjectMotion,

		// Volumetric Fog
		PS_VolumetricFog,
		PS_VolumetricFogShadowed,
		PS_VolumetricFogTemporal,
		PS_VolumetricFogComposite,
		PS_VolumetricFogUpsample,

		// Screen Space SSS
		PS_SSSMask,

		// Screen Space SSS Diffusion
		PS_SSSDiffusionBlur,
		PS_SSSDiffusionComposite,

		PS_SSSTemporal,

		//GrobalIum
		PS_GISurface,
		PS_SSGIRayMarch,
		PS_SSGIFilter,
		PS_SSGITemporal,
		PS_SSGIUpsample,
		PS_SSGIComposite,

		//デバック用
		PS_ShadowDebug,

		PS_MAX,
	};

	class ShaderObject
	{
	protected:
		const BYTE *code;
		long length;
	public:
		ShaderObject(const WCHAR *path);
		~ShaderObject();

		const BYTE *getCode(){ return code; }
		long getLength(){ return length; }
	};

private:
	static ShaderObject **vshader;
	static ShaderObject **pshader;

public:
	static bool initialize();
	static void terminate();

	static ShaderObject* getVShader(eVertexShader vs) { return vshader[(unsigned int)vs]; }
	static ShaderObject* getPShader(ePixelShader ps) { return pshader[(unsigned int)ps]; }
};