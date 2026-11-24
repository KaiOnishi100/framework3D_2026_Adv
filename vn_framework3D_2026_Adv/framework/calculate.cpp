#include "../framework.h"
#include "vn_environment.h"
#include "calculate.h"

float Calculate::Clamp(float v, float a, float b)
{
	return (v < a) ? a : (v > b) ? b : v;
}

float Calculate::Clamp01(float t)
{
	if (t < 0.0f) return 0.0f;
	if (t > 1.0f) return 1.0f;
	return t;
}

float Calculate::Lerp(float a, float b, float t)
{
	return a + (b - a) * t;
}

float Calculate::SmoothStep01(float t)
{
	t = (t < 0.0f) ? 0.0f : (t > 1.0f ? 1.0f : t);
	return t * t * (3.0f - 2.0f * t);
}
