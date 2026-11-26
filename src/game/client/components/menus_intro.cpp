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

// Возвращает true, пока играет заставка (пропустить нельзя).
// 1) вращающаяся фиолетовая спиральная галактика, 2) схлопывание и фиолетовый взрыв частиц со вспышкой и искрами,
// 3) название «G A L A X Y  C L I E N T» в плитках.
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
	const float Total = 9.4f;
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
	const float Tx = 3.4f; // момент взрыва
	const float Out = 1.0f - Ease((T - 8.5f) / 0.9f);
	const float Age = T - Tx;

	// фон: чёрный космос
	{
		CUIRect Full = Screen;
		Full.Draw(ColorRGBA(0.0f, 0.0f, 0.012f, Out), IGraphics::CORNER_NONE, 0.0f);
	}

	pG->BlendAdditive();
	pG->TextureClear();

	// ---------- 1) галактика ----------
	if(T < Tx + 0.3f)
	{
		const float Appear = Ease(T / 1.8f);
		const float Contract = Ease((T - 2.55f) / 0.85f);
		const float Alpha = Appear * (1.0f - Ease((T - Tx) / 0.3f)) * Out;
		const float Scale = (0.6f + 0.4f * Appear) * (1.0f - 0.8f * Contract);
		const float Rmax = H * 0.46f;
		const float Tilt = 0.52f;
		const float Ca = std::cos(-0.28f), Sa = std::sin(-0.28f);
		pG->QuadsBegin();
		// гало и яркое ядро
		for(int i = 0; i < 9; i++)
		{
			pG->SetColor(0.62f, 0.4f, 1.0f, 0.016f * Alpha * (1.0f + 2.5f * Contract));
			pG->DrawCircle(Center.x, Center.y, Rmax * Scale * (0.12f + 0.11f * i), 48);
		}
		for(int i = 0; i < 6; i++)
		{
			pG->SetColor(0.95f, 0.88f, 1.0f, 0.07f * Alpha * (1.0f + 1.5f * Contract));
			pG->DrawCircle(Center.x, Center.y, Rmax * Scale * (0.03f + 0.03f * i) * (1.0f + 0.6f * Contract), 32);
		}
		// частицы рукавов
		const int NumStars = 2600;
		for(int i = 0; i < NumStars; i++)
		{
			const float Rn = std::pow(Hash01(i * 7 + 1), 0.75f);
			const int Arm = i % 2;
			const float Spread = (Hash01(i * 7 + 2) - 0.5f) * (1.05f - 0.6f * Rn);
			const float Ang = Arm * 3.14159265f + Rn * 4.4f + Spread + T * (1.15f - 0.75f * Rn) * 0.55f;
			const float R = Rn * Rmax * Scale;
			const float Xr = std::cos(Ang) * R;
			const float Yr = std::sin(Ang) * R * Tilt;
			const float X = Center.x + Xr * Ca - Yr * Sa;
			const float Y = Center.y + Xr * Sa + Yr * Ca;
			const float Core = std::pow(1.0f - Rn, 1.6f);
			const float Size = (0.5f + 1.5f * Hash01(i * 7 + 3)) * (0.6f + 0.4f * Scale);
			const float A = (0.10f + 0.55f * Core + 0.12f * Hash01(i * 7 + 4)) * Alpha;
			pG->SetColor(0.55f + 0.4f * Core, 0.32f + 0.55f * Core, 0.95f + 0.05f * Core, A);
			const IGraphics::CQuadItem Q(X - Size / 2, Y - Size / 2, Size, Size);
			pG->QuadsDrawTL(&Q, 1);
		}
		pG->QuadsEnd();
	}

	// ---------- 2) взрыв ----------
	if(Age > 0.0f)
	{
		// вспышка на весь экран
		const float F1 = std::exp(-Age * 13.0f), F2 = std::exp(-Age * 3.2f);
		pG->QuadsBegin();
		pG->SetColor(1.0f, 0.95f, 1.0f, 0.55f * F1 * Out);
		const IGraphics::CQuadItem Full(0, 0, W, H);
		pG->QuadsDrawTL(&Full, 1);
		pG->SetColor(0.65f, 0.4f, 1.0f, 0.28f * F2 * Out);
		pG->QuadsDrawTL(&Full, 1);
		// фиолетовое зарево вокруг центра
		for(int i = 0; i < 7; i++)
		{
			pG->SetColor(0.55f, 0.28f, 1.0f, 0.03f * std::exp(-Age * 0.45f) * Out);
			pG->DrawCircle(Center.x, Center.y, H * (0.15f + 0.1f * i) * (0.6f + 0.5f * EaseOut(Age / 1.5f)), 48);
		}
		// белое горячее ядро
		for(int i = 0; i < 5; i++)
		{
			pG->SetColor(1.0f, 0.9f, 1.0f, 0.13f * std::exp(-Age * 5.0f) * Out);
			pG->DrawCircle(Center.x, Center.y, 10.0f + 22.0f * i * EaseOut(Age / 0.5f), 32);
		}
		pG->QuadsEnd();

		// ударная волна
		if(Age < 1.5f)
		{
			const float K = Age / 1.5f;
			const float R = 14.0f + 430.0f * EaseOut(K);
			pG->QuadsBegin();
			pG->SetColor(0.75f, 0.55f, 1.0f, (1.0f - K) * 0.55f * Out);
			const int Seg = 80;
			for(int i = 0; i < Seg; i++)
			{
				const float A0 = i * 6.2831853f / Seg, A1 = (i + 1) * 6.2831853f / Seg;
				Segment(pG, Center + vec2(std::cos(A0), std::sin(A0)) * R, Center + vec2(std::cos(A1), std::sin(A1)) * R, 0.5f + 1.8f * (1.0f - K));
			}
			pG->QuadsEnd();
		}

		// частицы-лучи
		pG->QuadsBegin();
		const int NumP = 1900;
		for(int i = 0; i < NumP; i++)
		{
			const float Life = 1.7f + 2.6f * Hash01(i * 9 + 1);
			if(Age > Life)
				continue;
			const float Ang = Hash01(i * 9 + 2) * 6.2831853f;
			const float Speed = 35.0f + 340.0f * std::pow(Hash01(i * 9 + 3), 1.5f);
			const vec2 Dir(std::cos(Ang), std::sin(Ang));
			const auto PosAt = [&](float At) {
				const float D = (1.0f - std::exp(-1.5f * At)) / 1.5f;
				return Center + Dir * Speed * D + vec2(0.0f, 16.0f * At * At);
			};
			const vec2 P1 = PosAt(Age), P0 = PosAt(std::max(0.0f, Age - 0.055f));
			if(P1.x < -10 || P1.x > W + 10 || P1.y < -10 || P1.y > H + 10)
				continue;
			const float K = Age / Life;
			float Cr, Cg, Cb;
			if(K < 0.35f)
			{
				const float M = K / 0.35f; // белый -> фиолетовый
				Cr = 1.0f + (0.66f - 1.0f) * M;
				Cg = 0.93f + (0.34f - 0.93f) * M;
				Cb = 1.0f;
			}
			else
			{
				const float M = (K - 0.35f) / 0.65f; // фиолетовый -> тёмно-фиолетовый
				Cr = 0.66f + (0.32f - 0.66f) * M;
				Cg = 0.34f + (0.1f - 0.34f) * M;
				Cb = 1.0f + (0.65f - 1.0f) * M;
			}
			pG->SetColor(Cr, Cg, Cb, std::pow(1.0f - K, 1.3f) * Out);
			Segment(pG, P0, P1, 0.3f + 0.45f * Hash01(i * 9 + 4));
		}
		pG->QuadsEnd();

		// блёстки: четырёхконечные звёздочки, которые мерцают и гаснут
		pG->QuadsBegin();
		for(int i = 0; i < 150; i++)
		{
			const float T0 = 0.6f + Hash01(i * 13 + 1) * 3.4f;
			const float Life = 1.3f + 1.8f * Hash01(i * 13 + 2);
			const float SAge = Age - T0;
			if(SAge < 0.0f || SAge > Life)
				continue;
			const float X = Center.x + (Hash01(i * 13 + 3) - 0.5f) * W * 0.95f + (Hash01(i * 13 + 6) - 0.5f) * 4.0f * SAge;
			const float Y = Center.y + (Hash01(i * 13 + 4) - 0.5f) * H * 0.95f + 3.0f * SAge;
			const float Size = 2.2f + 5.0f * Hash01(i * 13 + 5);
			const float Env = std::pow(std::sin(3.14159265f * SAge / Life), 1.2f);
			const float Tw = 0.6f + 0.4f * std::sin(T * (6.0f + 6.0f * Hash01(i * 13 + 7)) + (float)i);
			const float A = Env * Tw * Out;
			const float Mix = Hash01(i * 13 + 8);
			pG->SetColor(0.75f + 0.25f * Mix, 0.55f + 0.4f * Mix, 1.0f, A);
			const IGraphics::CQuadItem Qh(X - Size, Y - 0.3f, Size * 2.0f, 0.6f);
			const IGraphics::CQuadItem Qv(X - 0.3f, Y - Size * 1.3f, 0.6f, Size * 2.6f);
			pG->QuadsDrawTL(&Qh, 1);
			pG->QuadsDrawTL(&Qv, 1);
			pG->SetColor(0.8f, 0.6f, 1.0f, A * 0.25f);
			pG->DrawCircle(X, Y, Size * 0.55f, 14);
		}
		pG->QuadsEnd();
	}
	pG->BlendNormal();

	// ---------- 3) название в плитках ----------
	const float TitleStart = Tx + 0.55f;
	if(T >= TitleStart)
	{
		const char *apWords[2] = {"GALAXY", "CLIENT"};
		const float TW = std::min(26.0f, W * 0.88f / 13.9f);
		const float TH = TW * 1.18f;
		const float G = TW * 0.12f;
		const float WG = TW * 0.7f;
		const float TotalW = 12 * TW + 10 * G + WG + G; // 5 промежутков в каждом слове и промежуток между словами
		const float Total2 = 12 * TW + 10 * G + WG;
		(void)TotalW;
		float X = Center.x - Total2 / 2.0f;
		const float Y = Center.y - TH / 2.0f;
		TextRender()->SetPreferredFont("fonts/RussoOne-Regular.ttf");
		int Global = 0;
		for(int Wd = 0; Wd < 2; Wd++)
		{
			for(const char *p = apWords[Wd]; *p; p++, Global++)
			{
				const float Appear = Ease((T - TitleStart - Global * 0.11f) / 0.8f) * Out;
				if(Appear > 0.0f)
				{
					const float Rise = (1.0f - Appear) * 8.0f;
					const float Pulse = 0.5f + 0.5f * std::sin(T * 1.3f - Global * 0.4f);
					// мягкое свечение
					pG->BlendAdditive();
					pG->TextureClear();
					pG->QuadsBegin();
					for(int i = 0; i < 3; i++)
					{
						pG->SetColor(0.5f, 0.25f, 1.0f, 0.05f * Appear);
						pG->DrawCircle(X + TW / 2, Y + TH / 2 + Rise, TW * (0.7f + 0.25f * i), 24);
					}
					pG->QuadsEnd();
					pG->BlendNormal();
					// плитка: светлая рамка + тёмно-фиолетовая заливка
					CUIRect Tile(X, Y + Rise, TW, TH);
					Tile.Draw(ColorRGBA(0.72f + 0.2f * Pulse, 0.5f + 0.3f * Pulse, 1.0f, 0.85f * Appear), IGraphics::CORNER_ALL, TW * 0.18f);
					CUIRect Inner = Tile;
					Inner.Margin(TW * 0.05f, &Inner);
					Inner.Draw(ColorRGBA(0.16f, 0.07f, 0.34f, 0.92f * Appear), IGraphics::CORNER_ALL, TW * 0.14f);
					char aCh[2] = {*p, 0};
					const float FontSize = TW * 0.78f;
					const float Wch = TextRender()->TextWidth(FontSize, aCh);
					const float K = (0.35f + 0.65f * Pulse) * 0.8f;
					TextRender()->TextColor(0.78f + 0.22f * K, 0.6f + 0.4f * K, 1.0f, Appear);
					TextRender()->TextOutlineColor(0.2f, 0.08f, 0.5f, 0.0f);
					TextRender()->Text(X + (TW - Wch) / 2, Y + Rise + (TH - FontSize * 1.18f) / 2, FontSize, aCh);
				}
				X += TW + G;
			}
			X += WG - G;
		}
		TextRender()->TextColor(TextRender()->DefaultTextColor());
		TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor());
		TextRender()->SetPreferredFont("");
	}
	return true;
}
