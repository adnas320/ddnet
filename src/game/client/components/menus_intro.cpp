#include "menus.h"

#include <base/time.h>

#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/gameclient.h>
#include <game/client/ui.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

float Hash01(int n)
{
	unsigned x = (unsigned)n * 374761393u + 668265263u;
	x = (x ^ (x >> 13)) * 1274126177u;
	x ^= x >> 16;
	return (float)(x & 0xffff) / 65535.0f;
}

float Clamp01(float v)
{
	return std::clamp(v, 0.0f, 1.0f);
}

// толстая линия из одного четырёхугольника (штрих звезды)
void Segment(IGraphics *pG, vec2 A, vec2 B, float Thickness)
{
	vec2 D = B - A;
	float L = length(D);
	if(L < 0.001f)
	{
		D = vec2(1.0f, 0.0f);
		L = 1.0f;
		B = A + D * 0.5f;
	}
	const vec2 N = vec2(-D.y, D.x) / L * Thickness;
	const IGraphics::CFreeformItem Item(A + N, B + N, A - N, B - N);
	pG->QuadsDrawFreeform(&Item, 1);
}

} // namespace

// Тёмный фиолетово-чёрный градиент, углы очень медленно и плавно «дышат».
// Frame — общий прямоугольник, по которому считается градиент, чтобы составные панели не имели шва.
void CMenus::DrawSparkiGradientIn(const CUIRect &Frame, const CUIRect &Rect, int Corners, float Rounding)
{
	const float T = (float)(time_get() % (time_freq() * 3600)) / (float)time_freq();
	const auto Corner = [&](float Base, float Phase) {
		const float K = std::clamp(Base + 0.24f * std::sin(T * 0.45f + Phase), 0.0f, 1.0f);
		const float Shift = 0.5f + 0.5f * std::sin(T * 0.25f + Phase * 1.7f);
		return ColorRGBA(K * (0.19f - 0.05f * Shift), K * 0.055f, K * (0.38f + 0.08f * Shift), 0.5f);
	};
	// TL, TR, BL, BR
	const ColorRGBA aC[4] = {Corner(0.45f, 1.3f), Corner(0.06f, 3.1f), Corner(0.9f, 0.0f), Corner(0.45f, 4.4f)};
	const auto At = [&](float X, float Y) {
		const float U = Frame.w > 0.0f ? std::clamp((X - Frame.x) / Frame.w, 0.0f, 1.0f) : 0.0f;
		const float V = Frame.h > 0.0f ? std::clamp((Y - Frame.y) / Frame.h, 0.0f, 1.0f) : 0.0f;
		ColorRGBA R;
		R.r = (aC[0].r * (1 - U) + aC[1].r * U) * (1 - V) + (aC[2].r * (1 - U) + aC[3].r * U) * V;
		R.g = (aC[0].g * (1 - U) + aC[1].g * U) * (1 - V) + (aC[2].g * (1 - U) + aC[3].g * U) * V;
		R.b = (aC[0].b * (1 - U) + aC[1].b * U) * (1 - V) + (aC[2].b * (1 - U) + aC[3].b * U) * V;
		R.a = aC[0].a;
		return R;
	};
	Rect.Draw4(At(Rect.x, Rect.y), At(Rect.x + Rect.w, Rect.y), At(Rect.x, Rect.y + Rect.h), At(Rect.x + Rect.w, Rect.y + Rect.h), Corners, Rounding);
}

void CMenus::DrawSparkiGradient(const CUIRect &Rect, int Corners, float Rounding)
{
	DrawSparkiGradientIn(Rect, Rect, Corners, Rounding);
}

// Возвращает true, пока играет заставка (пропустить нельзя). Спокойный «космос»: влёт сквозь звёзды, туманности, планета.
bool CMenus::RenderIntro()
{
	if(m_IntroDone)
		return false;
	if(!g_Config.m_ClSparkiIntro)
	{
		m_IntroDone = true;
		return false;
	}
	if(m_IntroStart == 0)
		m_IntroStart = time_get();

	const float T = (float)(time_get() - m_IntroStart) / (float)time_freq();
	const float Total = 5.6f;
	if(T > Total)
	{
		m_IntroDone = true;
		return false;
	}

	Ui()->MapScreen();
	const CUIRect Screen = *Ui()->Screen();
	const float W = Screen.w;
	const float H = Screen.h;
	const vec2 Center(W / 2.0f, H / 2.0f);
	IGraphics *pG = Graphics();
	const auto Ease = [](float x) {
		x = Clamp01(x);
		return x * x * (3.0f - 2.0f * x);
	};
	const auto EaseOut = [](float x) {
		x = Clamp01(x);
		return 1.0f - (1.0f - x) * (1.0f - x) * (1.0f - x);
	};
	const float In = Ease(T / 1.0f); // плавное появление из темноты
	const float Fade = (1.0f - Ease((T - 4.9f) / 0.7f)) * In;

	// почти чёрный космос с лёгким градиентом
	{
		CUIRect Full = Screen;
		Full.Draw4(ColorRGBA(0.005f, 0.005f, 0.02f, Fade), ColorRGBA(0.01f, 0.005f, 0.03f, Fade), ColorRGBA(0.02f, 0.01f, 0.05f, Fade), ColorRGBA(0.01f, 0.01f, 0.04f, Fade), IGraphics::CORNER_NONE, 0.0f);
	}

	// туманности: мягкие цветные облака, медленно дрейфуют
	{
		struct SNeb
		{
			float m_X, m_Y, m_R, m_Cr, m_Cg, m_Cb, m_Drift;
		};
		const SNeb aNeb[5] = {
			{0.30f, 0.40f, 0.55f, 0.35f, 0.12f, 0.65f, 3.0f},
			{0.72f, 0.30f, 0.45f, 0.10f, 0.20f, 0.55f, -2.0f},
			{0.55f, 0.72f, 0.50f, 0.55f, 0.10f, 0.40f, 2.0f},
			{0.15f, 0.80f, 0.35f, 0.05f, 0.30f, 0.45f, -3.0f},
			{0.85f, 0.75f, 0.40f, 0.30f, 0.12f, 0.55f, 1.5f},
		};
		const float NebA = Ease((T - 0.3f) / 2.0f) * Fade;
		pG->BlendAdditive();
		pG->TextureClear();
		pG->QuadsBegin();
		for(const SNeb &N : aNeb)
		{
			const float X = W * N.m_X + N.m_Drift * T;
			const float Y = H * N.m_Y + std::sin(T * 0.3f + N.m_X * 6.0f) * 4.0f;
			for(int i = 0; i < 7; i++)
			{
				const float K = (float)(i + 1) / 7.0f;
				pG->SetColor(N.m_Cr, N.m_Cg, N.m_Cb, 0.018f * NebA);
				pG->DrawCircle(X, Y, H * N.m_R * K, 40);
			}
		}
		pG->QuadsEnd();
		pG->BlendNormal();
	}

	// звёзды: влёт (полосы от центра), затем замедление до неподвижных мерцающих точек
	{
		const float P = Clamp01((T - 0.5f) / 2.3f);
		const float PPrev = Clamp01((T - 0.5f - 0.04f) / 2.3f);
		pG->BlendAdditive();
		pG->TextureClear();
		pG->QuadsBegin();
		const int NumStars = 420;
		for(int i = 0; i < NumStars; i++)
		{
			const float Depth = 0.25f + 0.75f * Hash01(i * 11 + 1);
			const float Ang = Hash01(i * 11 + 2) * 6.2831853f;
			const float Rf = std::sqrt(Hash01(i * 11 + 3)) * std::sqrt(W * W + H * H) * 0.5f; // конечное расстояние от центра
			const vec2 Dir(std::cos(Ang), std::sin(Ang));
			const float R1 = Rf * (0.15f + 0.85f * EaseOut(P));
			const float R0 = Rf * (0.15f + 0.85f * EaseOut(PPrev));
			const float Streak = (R1 - R0) * (4.0f + 8.0f * Depth); // длина полосы зависит от скорости
			const vec2 Pos = Center + Dir * R1;
			if(Pos.x < -5 || Pos.x > W + 5 || Pos.y < -5 || Pos.y > H + 5)
				continue;
			const float Tw = P >= 1.0f ? 0.65f + 0.35f * std::sin(T * (0.8f + Hash01(i * 11 + 4) * 2.2f) + (float)i) : 1.0f;
			const float Tint = Hash01(i * 11 + 5);
			pG->SetColor(0.75f + 0.25f * Tint, 0.8f + 0.1f * Tint, 1.0f - 0.3f * Tint, (0.3f + 0.6f * Depth) * Tw * Fade * Clamp01(P * 3.0f));
			Segment(pG, Pos - Dir * Streak, Pos, 0.25f + 0.55f * Depth);
		}
		pG->QuadsEnd();
		pG->BlendNormal();
	}

	// планета в нижнем левом углу: тёмный диск с тонкой атмосферой
	{
		const float PlanetA = Ease((T - 1.6f) / 1.8f) * Fade;
		const float R = H * 0.62f;
		const vec2 C(W * 0.16f, H * 1.12f - 10.0f * Ease((T - 1.6f) / 3.5f));
		pG->TextureClear();
		pG->BlendAdditive();
		pG->QuadsBegin();
		for(int i = 0; i < 5; i++)
		{
			pG->SetColor(0.35f, 0.3f, 0.9f, 0.03f * PlanetA);
			pG->DrawCircle(C.x, C.y, R * (1.02f + 0.014f * i), 64);
		}
		pG->SetColor(0.55f, 0.5f, 1.0f, 0.11f * PlanetA);
		pG->DrawCircle(C.x, C.y, R * 1.006f, 64);
		pG->QuadsEnd();
		pG->BlendNormal();
		pG->QuadsBegin();
		pG->SetColor(0.01f, 0.008f, 0.03f, PlanetA);
		pG->DrawCircle(C.x, C.y, R, 64);
		pG->QuadsEnd();
	}

	// название: спокойно проявляется, широкий интервал, пурпурный <-> белый
	const float TextStart = 2.6f;
	if(T >= TextStart)
	{
		TextRender()->SetPreferredFont("fonts/RussoOne-Regular.ttf");
		const float Size = 30.0f;
		const char *apWords[2] = {"GALAXY", "CLIENT"};
		const float Track = Size * 0.32f;
		const float Gap2 = Size * 0.9f;
		float aWordW[2];
		for(int Wd = 0; Wd < 2; Wd++)
		{
			float Wsum = 0.0f;
			for(const char *p = apWords[Wd]; *p; p++)
			{
				char aCh[2] = {*p, 0};
				Wsum += TextRender()->TextWidth(Size, aCh) + Track;
			}
			aWordW[Wd] = Wsum - Track;
		}
		const float TotalW = aWordW[0] + Gap2 + aWordW[1];
		const float Y0 = Center.y - Size * 0.55f;
		const ColorRGBA Base(0.66f, 0.42f, 1.0f, 1.0f);
		float X = Center.x - TotalW / 2.0f;
		int Global = 0;
		for(int Wd = 0; Wd < 2; Wd++)
		{
			for(const char *p = apWords[Wd]; *p; p++, Global++)
			{
				char aCh[2] = {*p, 0};
				const float Appear = Ease((T - TextStart - Global * 0.09f) / 0.9f) * Fade;
				const float K = (0.5f + 0.5f * std::sin(T * 1.3f - Global * 0.4f)) * 0.8f;
				TextRender()->TextColor(Base.r + (1.0f - Base.r) * K, Base.g + (1.0f - Base.g) * K, 1.0f, Appear);
				TextRender()->TextOutlineColor(0.25f, 0.1f, 0.6f, 0.35f * Appear);
				TextRender()->Text(X, Y0, Size, aCh);
				X += TextRender()->TextWidth(Size, aCh) + Track;
			}
			X += Gap2 - Track;
		}
		TextRender()->TextColor(TextRender()->DefaultTextColor());
		TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor());
		TextRender()->SetPreferredFont("");

		// тонкая линия-«горизонт» под названием
		const float LineA = Ease((T - TextStart - 0.8f) / 1.2f) * Fade;
		CUIRect Line(Center.x - TotalW * 0.4f * LineA, Center.y + Size * 0.95f, TotalW * 0.8f * LineA, 1.0f);
		Line.Draw4(ColorRGBA(0.6f, 0.45f, 1.0f, 0.0f), ColorRGBA(0.85f, 0.75f, 1.0f, 0.6f * Fade), ColorRGBA(0.6f, 0.45f, 1.0f, 0.0f), ColorRGBA(0.85f, 0.75f, 1.0f, 0.6f * Fade), IGraphics::CORNER_NONE, 0.0f);
	}
	return true;
}
