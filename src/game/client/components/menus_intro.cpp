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

// Тёмный фиолетово-чёрный полупрозрачный градиент с медленным переливом (диагональные волны).
// Frame — общий прямоугольник, по которому считается градиент, чтобы составные панели не имели шва.
void CMenus::DrawSparkiGradientIn(const CUIRect &Frame, const CUIRect &Rect, int Corners, float Rounding)
{
	const float T = (float)(time_get() % (time_freq() * 3600)) / (float)time_freq();
	// основа: тёмный фиолетовый слева снизу -> чёрный справа сверху
	const auto Base = [&](float X, float Y) {
		const float U = Frame.w > 0.0f ? std::clamp((X - Frame.x) / Frame.w, 0.0f, 1.0f) : 0.0f;
		const float V = Frame.h > 0.0f ? std::clamp((Y - Frame.y) / Frame.h, 0.0f, 1.0f) : 0.0f;
		const float K = std::clamp(0.55f * (1.0f - U) * 0.6f + 0.55f * V * 0.7f + 0.1f, 0.0f, 1.0f);
		return ColorRGBA(0.20f * K, 0.05f * K, 0.40f * K, 0.62f);
	};
	Rect.Draw4(Base(Rect.x, Rect.y), Base(Rect.x + Rect.w, Rect.y), Base(Rect.x, Rect.y + Rect.h), Base(Rect.x + Rect.w, Rect.y + Rect.h), Corners, Rounding);

	// перелив: две медленные волны, бегущие по диагонали; рисуются полосами внутри панели
	const auto Sheen = [&](float X, float Y) {
		const float U = Frame.w > 0.0f ? (X - Frame.x) / Frame.w : 0.0f;
		const float V = Frame.h > 0.0f ? (Y - Frame.y) / Frame.h : 0.0f;
		const float W1 = 0.5f + 0.5f * std::sin((U * 1.0f - V * 0.9f) * 5.5f - T * 0.55f);
		const float W2 = 0.5f + 0.5f * std::sin((U * 0.7f + V * 0.8f) * 8.0f + T * 0.4f);
		const float K = std::pow(0.6f * W1 + 0.4f * W2, 1.6f);
		return ColorRGBA(0.34f, 0.10f, 0.70f, 0.38f * K);
	};
	CUIRect Inner = Rect;
	const float Inset = Rounding * 0.35f;
	Inner.Margin(Inset, &Inner);
	if(Inner.w > 4.0f && Inner.h > 4.0f)
	{
		const int Strips = 40;
		const float Sw = Inner.w / Strips;
		for(int i = 0; i < Strips; i++)
		{
			const float X0 = Inner.x + Sw * i, X1 = X0 + Sw + 0.4f;
			CUIRect Strip(X0, Inner.y, X1 - X0, Inner.h);
			Strip.Draw4(Sheen(X0, Inner.y), Sheen(X1, Inner.y), Sheen(X0, Inner.y + Inner.h), Sheen(X1, Inner.y + Inner.h), IGraphics::CORNER_NONE, 0.0f);
		}
	}
}

// Неоновая обводка: тонкая линия и две «кометы» по замкнутому контуру. Контур задан вершинами (x, y, радиус скругления)
void CMenus::DrawSparkiBorderPath(const std::vector<vec3> &vCorners)
{
	if(vCorners.size() < 3)
		return;
	const float T = (float)(time_get() % (time_freq() * 3600)) / (float)time_freq();

	// собираем ломаную: у каждой вершины с радиусом > 0 точки дуги (квадратичная кривая между точками касания)
	std::vector<vec2> vPts;
	const int Count = (int)vCorners.size();
	for(int i = 0; i < Count; i++)
	{
		const vec3 &Cur = vCorners[i];
		const vec2 V(Cur.x, Cur.y);
		if(Cur.z <= 0.01f)
		{
			vPts.push_back(V);
			continue;
		}
		const vec3 &Pv = vCorners[(i + Count - 1) % Count];
		const vec3 &Nx = vCorners[(i + 1) % Count];
		const vec2 ToPrev = vec2(Pv.x, Pv.y) - V, ToNext = vec2(Nx.x, Nx.y) - V;
		const float R = std::min(Cur.z, std::min(length(ToPrev), length(ToNext)) * 0.5f);
		const vec2 A = V + normalize(ToPrev) * R, B = V + normalize(ToNext) * R;
		const int Seg = 10;
		for(int s = 0; s <= Seg; s++)
		{
			const float U = (float)s / Seg;
			vPts.push_back(A * ((1 - U) * (1 - U)) + V * (2 * (1 - U) * U) + B * (U * U));
		}
	}

	std::vector<float> vCum(vPts.size() + 1, 0.0f);
	for(size_t i = 0; i < vPts.size(); i++)
		vCum[i + 1] = vCum[i] + length(vPts[(i + 1) % vPts.size()] - vPts[i]);
	const float P = vCum.back();
	if(P <= 1.0f)
		return;
	const auto PointAt = [&](float S) {
		S = std::fmod(S, P);
		if(S < 0.0f)
			S += P;
		const size_t Idx = std::min<size_t>(std::upper_bound(vCum.begin(), vCum.end(), S) - vCum.begin() - 1, vPts.size() - 1);
		const float Len = vCum[Idx + 1] - vCum[Idx];
		const float U = Len > 0.0001f ? (S - vCum[Idx]) / Len : 0.0f;
		return vPts[Idx] + (vPts[(Idx + 1) % vPts.size()] - vPts[Idx]) * U;
	};

	const int N = std::max(120, (int)(P / 5.0f));
	const float Lap = 12.0f; // секунд на полный круг — медленно
	const float Head = std::fmod(T / Lap, 1.0f) * P;
	const float TailLen = P * 0.18f;
	IGraphics *pG = Graphics();
	pG->TextureClear();
	pG->BlendAdditive();
	pG->QuadsBegin();
	for(int i = 0; i < N; i++)
	{
		const float S0 = P * i / N, S1 = P * (i + 1) / N;
		const vec2 A = PointAt(S0), B = PointAt(S1);
		float Best = 1e9f;
		for(int c = 0; c < 2; c++)
		{
			float D = std::fmod(Head + c * P * 0.5f - S0, P);
			if(D < 0.0f)
				D += P;
			Best = std::min(Best, D);
		}
		const float Comet = Best < TailLen ? std::pow(1.0f - Best / TailLen, 1.6f) : 0.0f;
		vec2 Dv = B - A;
		const float L = length(Dv);
		if(L < 0.0001f)
			continue;
		const vec2 Nv = vec2(-Dv.y, Dv.x) / L;
		const auto Line = [&](float Th, float Alpha) {
			pG->SetColor(0.62f, 0.38f, 1.0f, Alpha);
			const IGraphics::CFreeformItem It(A + Nv * Th, B + Nv * Th, A - Nv * Th, B - Nv * Th);
			pG->QuadsDrawFreeform(&It, 1);
		};
		Line(0.35f, 0.25f);
		if(Comet > 0.0f)
		{
			Line(2.2f, 0.07f * Comet); // свечение
			Line(1.2f, 0.18f * Comet);
			pG->SetColor(0.9f, 0.8f, 1.0f, 0.95f * Comet);
			const IGraphics::CFreeformItem It(A + Nv * 0.55f, B + Nv * 0.55f, A - Nv * 0.55f, B - Nv * 0.55f);
			pG->QuadsDrawFreeform(&It, 1);
		}
	}
	pG->QuadsEnd();
	pG->BlendNormal();
}

// Прямоугольник: скругления только там, где они есть у панели (Corners — как у CUIRect::Draw)
void CMenus::DrawSparkiBorder(const CUIRect &Rect, int Corners, float Rounding)
{
	const float X0 = Rect.x, Y0 = Rect.y, X1 = Rect.x + Rect.w, Y1 = Rect.y + Rect.h;
	const auto Rad = [&](int Flag) { return (Corners & Flag) ? Rounding : 0.0f; };
	DrawSparkiBorderPath({vec3(X0, Y0, Rad(IGraphics::CORNER_TL)), vec3(X1, Y0, Rad(IGraphics::CORNER_TR)), vec3(X1, Y1, Rad(IGraphics::CORNER_BR)), vec3(X0, Y1, Rad(IGraphics::CORNER_BL))});
}

// Настройки: панель + верхний блок правой колонки, контур идёт по общей форме
void CMenus::DrawSparkiBorderSettings(const CUIRect &Main, const CUIRect &Top, float Rounding)
{
	const float X0 = Main.x, Y0 = Main.y, X1 = Main.x + Main.w, Y1 = Main.y + Main.h;
	const float XR = Top.x + Top.w, YT = Top.y + Top.h;
	DrawSparkiBorderPath({vec3(X0, Y0, 0.0f), vec3(XR, Y0, 0.0f), vec3(XR, YT, Rounding), vec3(X1, YT, 0.0f), vec3(X1, Y1, Rounding), vec3(X0, Y1, Rounding)});
}

// Фиолетовый «галактический» фон меню: тёмный градиент, медленные туманности и мерцающие звёзды
void CMenus::RenderPurpleBackground()
{
	Ui()->MapScreen();
	const CUIRect Screen = *Ui()->Screen();
	const float T = (float)(time_get() % (time_freq() * 3600)) / (float)time_freq();
	IGraphics *pG = Graphics();
	pG->TextureClear();

	Screen.Draw4(ColorRGBA(0.16f, 0.05f, 0.30f, 1.0f), ColorRGBA(0.03f, 0.01f, 0.08f, 1.0f), ColorRGBA(0.07f, 0.02f, 0.16f, 1.0f), ColorRGBA(0.02f, 0.0f, 0.05f, 1.0f), IGraphics::CORNER_NONE, 0.0f);

	pG->BlendAdditive();
	pG->QuadsBegin();
	// туманности: большие мягкие пятна из колец, плавно дрейфуют
	for(int i = 0; i < 6; i++)
	{
		const float Px = Screen.w * (0.12f + 0.76f * Hash01(i * 7 + 1)) + std::sin(T * 0.05f + i * 1.7f) * 40.0f;
		const float Py = Screen.h * (0.12f + 0.76f * Hash01(i * 7 + 2)) + std::cos(T * 0.04f + i * 2.3f) * 30.0f;
		const float Base = Screen.h * (0.22f + 0.18f * Hash01(i * 7 + 3));
		const bool Pink = Hash01(i * 7 + 4) > 0.6f;
		for(int Ring = 0; Ring < 10; Ring++)
		{
			const float K = (float)Ring / 10.0f;
			pG->SetColor(Pink ? 0.55f : 0.30f, Pink ? 0.12f : 0.10f, Pink ? 0.60f : 0.75f, 0.012f);
			pG->DrawCircle(Px, Py, Base * (1.0f - K * 0.8f), 40);
		}
	}
	// звёзды
	for(int i = 0; i < 160; i++)
	{
		const float Px = Screen.w * Hash01(i * 5 + 11), Py = Screen.h * Hash01(i * 5 + 12);
		const float Size = 0.6f + 1.6f * Hash01(i * 5 + 13) * Hash01(i * 5 + 13);
		const float Tw = 0.5f + 0.5f * std::sin(T * (0.6f + 1.6f * Hash01(i * 5 + 14)) + Hash01(i * 5 + 15) * 6.28f);
		const float A = 0.15f + 0.75f * Tw * (0.4f + 0.6f * Hash01(i * 5 + 16));
		pG->SetColor(0.85f, 0.78f, 1.0f, A);
		pG->DrawCircle(Px, Py, Size, 8);
		if(Size > 1.7f)
		{
			pG->SetColor(0.7f, 0.5f, 1.0f, A * 0.18f);
			pG->DrawCircle(Px, Py, Size * 3.2f, 12);
		}
	}
	pG->QuadsEnd();
	pG->BlendNormal();
}

void CMenus::DrawSparkiGradient(const CUIRect &Rect, int Corners, float Rounding)
{
	DrawSparkiGradientIn(Rect, Rect, Corners, Rounding);
}

// Возвращает true, пока играет заставка (пропустить нельзя): звёзды на фоне и название со свечением
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
	const float Total = 6.6f;
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
	const float Fade = Ease(T / 0.8f) * (1.0f - Ease((T - 5.8f) / 0.8f));

	// тёмный фон
	{
		CUIRect Full = Screen;
		Full.Draw4(ColorRGBA(0.004f, 0.002f, 0.014f, Fade), ColorRGBA(0.006f, 0.003f, 0.02f, Fade), ColorRGBA(0.014f, 0.006f, 0.04f, Fade), ColorRGBA(0.008f, 0.004f, 0.026f, Fade), IGraphics::CORNER_NONE, 0.0f);
	}

	// звёзды на фоне: неподвижные, мягко мерцают
	pG->TextureClear();
	pG->BlendAdditive();
	pG->QuadsBegin();
	for(int i = 0; i < 260; i++)
	{
		const float X = Hash01(i * 7 + 1) * W, Y = Hash01(i * 7 + 2) * H;
		const float Tw = 0.55f + 0.45f * std::sin(T * (0.7f + Hash01(i * 7 + 3) * 1.8f) + (float)i * 1.3f);
		const float Sz = 0.5f + 1.5f * Hash01(i * 7 + 4);
		const float Tint = Hash01(i * 7 + 5);
		pG->SetColor(0.75f + 0.25f * Tint, 0.78f + 0.12f * Tint, 1.0f, (0.25f + 0.65f * Hash01(i * 7 + 6)) * Tw * Fade);
		const IGraphics::CQuadItem Q(X - Sz / 2, Y - Sz / 2, Sz, Sz);
		pG->QuadsDrawTL(&Q, 1);
	}
	pG->QuadsEnd();
	pG->BlendNormal();

	// название: квадратные «значки»-буквы с сильным свечением
	const float TextStart = 0.9f;
	TextRender()->SetPreferredFont("fonts/RussoOne-Regular.ttf");
	const char *apWords[2] = {"GALAXY", "CLIENT"};
	const float B = std::min(26.0f, W * 0.0625f);
	const float Gap = B * 0.18f;
	const float WordGap = B * 0.6f;
	const float TotalW = 12 * B + 10 * Gap + WordGap;
	const float FontSize = B * 0.66f;
	const float Y0 = Center.y - B / 2.0f;

	// общее мягкое свечение за всей надписью
	{
		const float GlowA = Ease((T - TextStart) / 1.5f) * Fade;
		pG->BlendAdditive();
		pG->TextureClear();
		pG->QuadsBegin();
		for(int i = 0; i < 8; i++)
		{
			pG->SetColor(0.45f, 0.2f, 1.0f, 0.03f * GlowA);
			pG->DrawCircle(Center.x, Center.y, TotalW * (0.2f + 0.08f * i), 48);
		}
		pG->QuadsEnd();
		pG->BlendNormal();
	}

	float X = Center.x - TotalW / 2.0f;
	int Global = 0;
	for(int Wd = 0; Wd < 2; Wd++)
	{
		for(const char *pCh = apWords[Wd]; *pCh; pCh++, Global++)
		{
			const float P = Ease((T - TextStart - Global * 0.12f) / 0.8f);
			const float Appear = P * Fade;
			const float Y = Y0 + (1.0f - P) * 8.0f;
			const float Shimmer = (0.5f + 0.5f * std::sin(T * 1.4f - Global * 0.45f)) * 0.8f;
			if(Appear > 0.001f)
			{
				// свечение вокруг каждой буквы
				pG->BlendAdditive();
				pG->TextureClear();
				pG->QuadsBegin();
				for(int g = 0; g < 6; g++)
				{
					pG->SetColor(0.55f, 0.28f, 1.0f, 0.055f * Appear);
					pG->DrawCircle(X + B / 2.0f, Y + B / 2.0f, B * (0.65f + 0.16f * g), 28);
				}
				pG->QuadsEnd();
				pG->BlendNormal();
				CUIRect Outer(X - 1.0f, Y - 1.0f, B + 2.0f, B + 2.0f);
				Outer.Draw(ColorRGBA(0.7f, 0.45f, 1.0f, 0.35f * Appear), IGraphics::CORNER_ALL, B * 0.22f);
				CUIRect Edge(X, Y, B, B);
				Edge.Draw(ColorRGBA(0.66f, 0.4f, 1.0f, 0.85f * Appear), IGraphics::CORNER_ALL, B * 0.2f);
				CUIRect Inner(X + 1.0f, Y + 1.0f, B - 2.0f, B - 2.0f);
				Inner.Draw(ColorRGBA(0.05f, 0.02f, 0.12f, 0.92f * Appear), IGraphics::CORNER_ALL, B * 0.18f);
				char aCh[2] = {*pCh, 0};
				const float Cw = TextRender()->TextWidth(FontSize, aCh);
				const float Tx = X + (B - Cw) / 2.0f, Ty = Y + (B - FontSize * 1.15f) / 2.0f;
				// свечение букв: тот же символ с широким полупрозрачным контуром, затем сам символ
				TextRender()->TextColor(0.7f, 0.45f, 1.0f, 0.5f * Appear);
				TextRender()->TextOutlineColor(0.55f, 0.28f, 1.0f, 0.5f * Appear);
				TextRender()->Text(Tx, Ty, FontSize * 1.03f, aCh);
				TextRender()->TextColor(0.66f + 0.34f * Shimmer, 0.42f + 0.58f * Shimmer, 1.0f, Appear);
				TextRender()->TextOutlineColor(0.3f, 0.1f, 0.7f, 0.45f * Appear);
				TextRender()->Text(Tx, Ty, FontSize, aCh);
			}
			X += B + Gap;
		}
		X += WordGap - Gap;
	}
	TextRender()->TextColor(TextRender()->DefaultTextColor());
	TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor());
	TextRender()->SetPreferredFont("");
	return true;
}
