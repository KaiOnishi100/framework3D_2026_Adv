#include "../../framework.h"
#include "../vn_environment.h"

extern HWND hWnd;

Shader::ShaderObject **Shader::vshader = NULL;
Shader::ShaderObject **Shader::pshader = NULL;

bool Shader::initialize()
{
	//頂点シェーダの作成
	vshader = new ShaderObject * [(unsigned int)eVertexShader::VS_MAX];
	if (vshader)
	{
		vshader[(unsigned int)eVertexShader::VS_3D] = new ShaderObject(L"shader/vs_3d.cso");
		vshader[(unsigned int)eVertexShader::VS_3D_Skin] = new ShaderObject(L"shader/vs_3d_skin.cso");
		vshader[(unsigned int)eVertexShader::VS_3D_Line] = new ShaderObject(L"shader/vs_3d_line.cso");
		vshader[(unsigned int)eVertexShader::VS_2D] = new ShaderObject(L"shader/vs_2d.cso");
		vshader[(unsigned int)eVertexShader::VS_Copy] = new ShaderObject(L"shader/postprocess/vs_copy.cso");
		vshader[(unsigned int)eVertexShader::VS_Gray] = new ShaderObject(L"shader/postprocess/vs_gray.cso");
		vshader[(unsigned int)eVertexShader::VS_Shadow] = new ShaderObject(L"shader/vs_shadow.cso");
		vshader[(unsigned int)eVertexShader::VS_ShadowSkin] = new ShaderObject(L"shader/vs_shadow_skin.cso");
		vshader[(unsigned int)eVertexShader::VS_Skybox] = new ShaderObject(L"shader/ibl/vs_skybox.cso");
		vshader[(unsigned int)eVertexShader::VS_PrefilterEnvironment] = new ShaderObject(L"shader/ibl/vs_prefilter_environment.cso");
		vshader[(unsigned int)eVertexShader::VS_BRDFLUT] = new ShaderObject(L"shader/ibl/vs_brdf_lut.cso");
		vshader[(unsigned int)eVertexShader::VS_ObjectMotion] = new ShaderObject(L"shader/postprocess/vs_object_motion.cso");
		vshader[(unsigned int)eVertexShader::VS_ObjectMotionSkin] = new ShaderObject(L"shader/postprocess/vs_object_motion_skin.cso");
	}

	//ピクセルシェーダの作成
	pshader = new ShaderObject * [(unsigned int)ePixelShader::PS_MAX];
	if (pshader)
	{
		pshader[(unsigned int)ePixelShader::PS_3D] = new ShaderObject(L"shader/ps_3d.cso");
		pshader[(unsigned int)ePixelShader::PS_3D_NoLight] = new ShaderObject(L"shader/ps_3d_no_light.cso");
		pshader[(unsigned int)ePixelShader::PS_3D_Line] = new ShaderObject(L"shader/ps_3d_line.cso");
		pshader[(unsigned int)ePixelShader::PS_2D] = new ShaderObject(L"shader/ps_2d.cso");
		pshader[(unsigned int)ePixelShader::PS_2D_HDR] = new ShaderObject(L"shader/ps_2d_hdr.cso");
		pshader[(unsigned int)ePixelShader::PS_Copy] = new ShaderObject(L"shader/postprocess/ps_copy.cso");
		pshader[(unsigned int)ePixelShader::PS_Gray] = new ShaderObject(L"shader/postprocess/ps_gray.cso");

		pshader[(unsigned int)ePixelShader::PS_BloomBright] = new ShaderObject(L"shader/postprocess/ps_bloom_bright.cso");
		pshader[(unsigned int)ePixelShader::PS_BloomBlurH] = new ShaderObject(L"shader/postprocess/ps_bloom_blur_h.cso");
		pshader[(unsigned int)ePixelShader::PS_BloomBlurV] = new ShaderObject(L"shader/postprocess/ps_bloom_blur_v.cso");
		pshader[(unsigned int)ePixelShader::PS_BloomComposite] = new ShaderObject(L"shader/postprocess/ps_bloom_composite.cso");
		pshader[(unsigned int)ePixelShader::PS_Vignette] = new ShaderObject(L"shader/postprocess/ps_vignette.cso");
		pshader[(unsigned int)ePixelShader::PS_ColorGrading] = new ShaderObject(L"shader/postprocess/ps_color_grading.cso");
		pshader[(unsigned int)ePixelShader::PS_Fxaa] = new ShaderObject(L"shader/postprocess/ps_fxaa.cso");
		pshader[(unsigned int)ePixelShader::PS_ChromaticAberration] = new ShaderObject(L"shader/postprocess/ps_chromatic_aberration.cso");
		pshader[(unsigned int)ePixelShader::PS_ToneMapping] = new ShaderObject(L"shader/postprocess/ps_toneMapping.cso");
		pshader[(unsigned int)ePixelShader::PS_Skybox] = new ShaderObject(L"shader/ibl/ps_skybox.cso");
		pshader[(unsigned int)ePixelShader::PS_HDRToCube] = new ShaderObject(L"shader/ibl/ps_hdr_to_cube.cso");
		pshader[(unsigned int)ePixelShader::PS_PrefilterEnvironment] = new ShaderObject(L"shader/ibl/ps_prefilter_environment.cso");
		pshader[(unsigned int)ePixelShader::PS_IrradianceEnvironment] = new ShaderObject(L"shader/ibl/ps_irradiance_environment.cso");
		pshader[(unsigned int)ePixelShader::PS_BRDFLUT] = new ShaderObject(L"shader/ibl/ps_brdf_lut.cso");
		pshader[(unsigned int)ePixelShader::PS_LensFlare] = new ShaderObject(L"shader/postprocess/ps_lens_flare.cso");
		pshader[(unsigned int)ePixelShader::PS_LensFlareOcclusion] = new ShaderObject(L"shader/postprocess/ps_lens_flare_occlusion.cso");
		pshader[(unsigned int)ePixelShader::PS_EffectDepthDownsample] = new ShaderObject(L"shader/ps_effect_depth_downsample.cso");
		pshader[(unsigned int)ePixelShader::PS_EffectHalfComposite] = new ShaderObject(L"shader/ps_effect_half_composite.cso");
		pshader[(unsigned int)ePixelShader::PS_MotionVector] = new ShaderObject(L"shader/postprocess/ps_motion_vector.cso");
		pshader[(unsigned int)ePixelShader::PS_MotionVectorDebug] = new ShaderObject(L"shader/postprocess/ps_motion_vector_debug.cso");
		pshader[(unsigned int)ePixelShader::PS_MotionBlur] = new ShaderObject(L"shader/postprocess/ps_motion_blur.cso");
		pshader[(unsigned int)ePixelShader::PS_ObjectMotion] = new ShaderObject(L"shader/postprocess/ps_object_motion.cso");
		pshader[(unsigned int)ePixelShader::PS_VolumetricFog] = new ShaderObject(L"shader/postprocess/ps_volumetric_fog.cso");
		pshader[(unsigned int)ePixelShader::PS_VolumetricFogShadowed] = new ShaderObject(L"shader/postprocess/ps_volumetric_fog_shadowed.cso");
		pshader[(unsigned int)ePixelShader::PS_VolumetricFogTemporal] = new ShaderObject(L"shader/postprocess/ps_volumetric_fog_temporal.cso");
		pshader[(unsigned int)ePixelShader::PS_VolumetricFogComposite] = new ShaderObject(L"shader/postprocess/ps_volumetric_fog_composite.cso");
		pshader[(unsigned int)ePixelShader::PS_VolumetricFogUpsample] = new ShaderObject(L"shader/postprocess/ps_volumetric_fog_upsample.cso");
		pshader[(unsigned int)ePixelShader::PS_SSSMask] = new ShaderObject(L"shader/ps_sss_mask.cso");
		pshader[(unsigned int)ePixelShader::PS_SSSDiffusionBlur] = new ShaderObject(L"shader/postprocess/ps_sss_diffusion_blur.cso");
		pshader[(unsigned int)ePixelShader::PS_SSSDiffusionComposite] = new ShaderObject(L"shader/postprocess/ps_sss_diffusion_composite.cso");
		pshader[(unsigned int)ePixelShader::PS_SSSTemporal] = new ShaderObject(L"shader/postprocess/ps_sss_temporal.cso");
		pshader[(unsigned int)ePixelShader::PS_GISurface] = new ShaderObject(L"shader/gi/geometry/ps_gi_surface.cso");
		pshader[(unsigned int)ePixelShader::PS_SSGIRayMarch] = new ShaderObject(L"shader/gi/postprocess/ps_ssgi_raymarch.cso");
		pshader[(unsigned int)ePixelShader::PS_SSGIFilter] = new ShaderObject(L"shader/gi/postprocess/ps_ssgi_filter.cso");
		pshader[(unsigned int)ePixelShader::PS_SSGITemporal] = new ShaderObject(L"shader/gi/postprocess/ps_ssgi_temporal.cso");
		pshader[(unsigned int)ePixelShader::PS_SSGIUpsample] = new ShaderObject(L"shader/gi/postprocess/ps_ssgi_upsample.cso");
		pshader[(unsigned int)ePixelShader::PS_SSGIComposite] = new ShaderObject(L"shader/gi/postprocess/ps_ssgi_composite.cso");
		pshader[(unsigned int)ePixelShader::PS_ShadowDebug] = new ShaderObject(L"shader/debug/ps_shadowDebug.cso");
	}
	return (vshader!=NULL && pshader!=NULL);
}

void Shader::terminate()
{
	for (int i = 0; i < (unsigned int)eVertexShader::VS_MAX; i++)
	{
		delete vshader[i];
		vshader[i] = NULL;
	}
	delete[] vshader;
	vshader = NULL;

	for (int i = 0; i < (unsigned int)ePixelShader::PS_MAX; i++)
	{
		delete pshader[i];
		pshader[i] = NULL;
	}
	delete[] pshader;
	pshader = NULL;
}

Shader::ShaderObject::ShaderObject(const WCHAR *path)
{
	code = NULL;
	length = 0;

	FILE *fp = NULL;
	if ((_wfopen_s(&fp, path, L"rb")) != 0)
	{
		WCHAR text[256];
		swprintf_s(text, L"cannot open file \"%s\"", path);
		MessageBox(hWnd, text, L"vnShader Load Error", MB_OK);
		assert(false);
		return;
	}
	fseek(fp, 0, SEEK_END);
	length = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	code = new BYTE[length];
	fread((void*)code, length, 1, fp);

	fclose(fp);
}

Shader::ShaderObject::~ShaderObject()
{
	if(code)delete[] code;
}

