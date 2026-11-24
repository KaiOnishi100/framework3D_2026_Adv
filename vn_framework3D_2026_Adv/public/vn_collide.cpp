#include "../framework.h"
#include "../framework/vn_environment.h"

using namespace vnCollide;

//2点から直線の情報を作成
void stLine::fromPoints(const XMVECTOR* p1, const XMVECTOR* p2)
{
	Pos = *p1;
	Dir = *p2 - *p1;
	Dir = XMVector3Normalize(Dir);
}

//2点から線分の情報を作成
void stSegment::fromPoints(const XMVECTOR* p1, const XMVECTOR* p2)
{
	Pos = *p1;
	Dir = *p2 - *p1;
	Length = XMVectorGetX(XMVector3Length(Dir));
	Dir = XMVector3Normalize(Dir);
}

//3点から平面の情報を作成
void stPlane::fromPoints(const XMVECTOR* p1, const XMVECTOR* p2, const XMVECTOR* p3)
{
	XMVECTOR v, w;
	v = *p3 - *p1;
	w = *p2 - *p1;
	Normal = XMVector3Cross(v, w);
	Normal = XMVector3Normalize(Normal);
	Distance = -XMVectorGetX(XMVector3Dot(Normal, *p1));
}

//1点と法線から平面の情報を作成
void stPlane::fromPointNormal(const XMVECTOR* p, const XMVECTOR* n)
{
	Normal = XMVector3Normalize(*n);
	Distance = -XMVectorGetX(XMVector3Dot(Normal, *p));
}

//3点から三角形の情報を作成
void stTriangle::fromPoints(const XMVECTOR* p1, const XMVECTOR* p2, const XMVECTOR* p3)
{
	plane.fromPoints(p1, p2, p3);
	v[0] = *p1;
	v[1] = *p2;
	v[2] = *p3;
}

//直線と点の最近傍距離を示す直線上の点を求める
void vnCollide::closestPoint(XMVECTOR* ret, const stLine* line, const XMVECTOR* pos)
{
	XMVECTOR v;
	v = *pos - line->Pos;

	float vx = XMVectorGetX(v);
	float vy = XMVectorGetY(v);
	float vz = XMVectorGetZ(v);
	if (ISZERO(vx) && ISZERO(vy) && ISZERO(vz))	//Line始点と点が一致
	{
		*ret = line->Pos;
		return;
	}

	XMVECTOR vDot = XMVector3Dot(v, line->Dir);
	float dot = XMVectorGetX(vDot);
	*ret = line->Pos + line->Dir * dot;
}


//直線と平面の交点を調べる
bool vnCollide::isCollide(XMVECTOR* hit, const stLine* line, const stPlane* plane)
{
	float d, s;
	s = XMVectorGetX(XMVector3Dot(plane->Normal, line->Pos)) + plane->Distance;
	d = XMVectorGetX(XMVector3Dot(plane->Normal, (line->Pos + line->Dir))) + plane->Distance;

	if (fabsf(d) < EPSILON)return false;
	if (fabsf(s - d) < EPSILON)return false;

	*hit = line->Pos + line->Dir * (s / (s - d));

	return true;
}

//直線と三角形の交点を調べる
bool vnCollide::isCollide(XMVECTOR* hit, const stLine* line, const stTriangle* tri)
{
	if (vnCollide::isCollide(hit, line, &tri->plane) == false)
	{
		return false;
	}

	//線と平面の衝突点が三角形内にあるかチェック
	XMVECTOR u, w, n(tri->plane.Normal);
	u = tri->v[1] - tri->v[0];
	w = *hit - tri->v[0];
	w = XMVector3Cross(u, w);
	//if (XMVectorGetX(XMVector3Dot(w, w)) < EPSILON)	return true;	//辺上に点がある
	if (XMVectorGetX(XMVector3Dot(w, n)) > EPSILON)	return false;	//はみ出ている
	u = tri->v[2] - tri->v[1];
	w = *hit - tri->v[1];
	w = XMVector3Cross(u, w);
	//if (XMVectorGetX(XMVector3Dot(w, w)) < EPSILON)	return true;	//辺上に点がある
	if (XMVectorGetX(XMVector3Dot(w, n)) > EPSILON)	return false;	//はみ出ている
	u = tri->v[0] - tri->v[2];
	w = *hit - tri->v[2];
	w = XMVector3Cross(u, w);
	//if (XMVectorGetX(XMVector3Dot(w, w)) < EPSILON)	return true;	//辺上に点がある
	if (XMVectorGetX(XMVector3Dot(w, n)) > EPSILON)	return false;	//はみ出ている
	return true;
}

//直線と球の交点を調べる
int vnCollide::isCollide(XMVECTOR* hit, const stLine* line, const stSphere* sphere)
{
	int ret = 0;
	XMVECTOR v, nv;
	v = line->Pos - sphere->Pos;

	float vx = XMVectorGetX(v);
	float vy = XMVectorGetY(v);
	float vz = XMVectorGetZ(v);
	if (ISZERO(vx) && ISZERO(vy) && ISZERO(vz))	//Line始点と球中心が一致
	{
		v = line->Dir * sphere->Radius;
		hit[ret++] = v + line->Pos;
		hit[ret++] = v * -1.0f + line->Pos;
		return ret;
	}

	nv = XMVector3Normalize(v);
	XMVECTOR vdt = XMVector3Dot(nv, line->Dir);
	XMVECTOR vdl = XMVector3Length(v);

	float dt = XMVectorGetX(vdt);
	float dl = XMVectorGetX(vdl);
	float dr = fabsf(dt) >= 1.0f ? 0.0f : dl * sqrtf(1.0f - dt * dt);	//球と直線の距離
	//球と直線の距離が半径以内なら衝突
	if (dr <= sphere->Radius)
	{
		float dq = sqrtf(sphere->Radius * sphere->Radius - dr * dr);
		float ds = dl * dt;
		float dm = ds - dq, dp = ds + dq;	//直線始点からの各衝突位置
		hit[ret++] = line->Dir * -dm + line->Pos;
		if (!ISZERO(dq))	//接線状態ではない
		{
			hit[ret++] = line->Dir * -dp + line->Pos;
		}
		return ret;
	}
	return ret;
}

//線分と平面の交点を調べる
bool vnCollide::isCollide(XMVECTOR* hit, const stSegment* seg, const stPlane* plane)
{
	float d, s;
	s = XMVectorGetX(XMVector3Dot(plane->Normal, seg->Pos)) + plane->Distance;
	d = XMVectorGetX(XMVector3Dot(plane->Normal, (seg->Pos + seg->Dir))) + plane->Distance;

	if (fabsf(d) < EPSILON)return false;
	if (fabsf(s - d) < EPSILON)return false;

	float t = (s / (s - d));
	if (t < 0.0f || t > seg->Length)return false;

	*hit = seg->Pos + seg->Dir * t;

	return true;
}

//線分と三角形の交点を調べる
bool vnCollide::isCollide(XMVECTOR* hit, const stSegment* seg, const stTriangle* tri)
{
	if (isCollide(hit, seg, &tri->plane) == false)
	{
		return false;
	}

	//線と平面の衝突点が三角形内にあるかチェック
	XMVECTOR u, w, n(tri->plane.Normal);
	u = tri->v[1] - tri->v[0];
	w = *hit - tri->v[0];
	w = XMVector3Cross(u, w);
	//if (XMVectorGetX(XMVector3Dot(w, w)) < EPSILON)	return true;	//辺上に点がある
	if (XMVectorGetX(XMVector3Dot(w, n)) > EPSILON)	return false;	//はみ出ている
	u = tri->v[2] - tri->v[1];
	w = *hit - tri->v[1];
	w = XMVector3Cross(u, w);
	//if (XMVectorGetX(XMVector3Dot(w, w)) < EPSILON)	return true;	//辺上に点がある
	if (XMVectorGetX(XMVector3Dot(w, n)) > EPSILON)	return false;	//はみ出ている
	u = tri->v[0] - tri->v[2];
	w = *hit - tri->v[2];
	w = XMVector3Cross(u, w);
	//if (XMVectorGetX(XMVector3Dot(w, w)) < EPSILON)	return true;	//辺上に点がある
	if (XMVectorGetX(XMVector3Dot(w, n)) > EPSILON)	return false;	//はみ出ている
	return true;
}

XMVECTOR vnCollide::ClosestPointOnTriangle(const XMVECTOR& p, const stTriangle& tri)
{
	//三角形の頂点
	XMVECTOR a = tri.v[0];
	XMVECTOR b = tri.v[1];
	XMVECTOR c = tri.v[2];

	//各辺
	XMVECTOR ab = b - a;
	XMVECTOR ac = c - a;
	XMVECTOR ap = p - a;

	//内積
	float dot1 = XMVectorGetX(XMVector3Dot(ab, ap));
	float dot2 = XMVectorGetX(XMVector3Dot(ac, ap));

	//頂点aが最近
	if (dot1 <= 0.0f && dot2 <= 0.0f)return a;

	XMVECTOR bp = p - b;
	float dot3 = XMVectorGetX(XMVector3Dot(ab, bp));
	float dot4 = XMVectorGetX(XMVector3Dot(ac, bp));

	//頂点bが最近
	if (dot3 >= 0.0f && dot4 <= dot3)return b;

	//AB辺上
	float vc = dot1 * dot4 - dot3 * dot2;
	if (vc <= 0.0f && dot1 >= 0.0f && dot3 <= 0.0f)
	{
		float v = dot1 / (dot1 - dot3);
		return a + ab * v;
	}

	XMVECTOR cp = p - c;
	float dot5 = XMVectorGetX(XMVector3Dot(ab, cp));
	float dot6 = XMVectorGetX(XMVector3Dot(ac, cp));

	//頂点cが最近
	if (dot6 >= 0.0f && dot5 <= dot6)return c;

	//ac辺上
	float vb = dot5 * dot2 - dot1 * dot6;
	if (vb <= 0.0f && dot2 >= 0.0f && dot6 <= 0.0f)
	{
		float w = dot2 / (dot2 - dot6);
		return a + ac * w;
	}

	//bc辺上
	float va = dot3 * dot6 - dot5 * dot4;
	if (va <= 0.0f && (dot4 - dot3) >= 0.0f && (dot5 - dot6) >= 0.0f)
	{
		float w = (dot4 - dot3) / ((dot4 - dot3) + (dot5 - dot6));
		return b + (c - b) * w;
	}

	//三角形内部 → 平面へ射影
	float denom = 1.0f / (va + vb + vc);
	float v = vb * denom;
	float w = vc * denom;

	return a + ab * v + ac * w;
}
