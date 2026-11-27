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
		return ColorRGBA(K * (0.40f - 0.08f * Shift), K * 0.13f, K * (0.78f + 0.12f * Shift), 0.66f);
	};
	// TL, TR, BL, BR
	const ColorRGBA aC[4] = {Corner(0.5f, 1.3f), Corner(0.0f, 3.1f), Corner(1.0f, 0.0f), Corner(0.5f, 4.4f)};
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
