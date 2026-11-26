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

// толстая линия из одного четырёхугольника
void Segment(IGraphics *pG, vec2 A, vec2 B, float Thickness)
{
	vec2 D = B - A;
	const float L = length(D);
	if(L < 0.001f)
		return;
	const vec2 N = vec2(-D.y, D.x) / L * Thickness;
	const IGraphics::CFreeformItem Item(A + N, B + N, A - N, B - N);
	pG->QuadsDrawFreeform(&Item, 1);
}

// зигзаг молнии сверху вниз
std::vector<vec2> MakeBolt(vec2 Start, vec2 End, int Seed, float Jitter)
{
	std::vector<vec2> v;
	const int N = 10;
	for(int i = 0; i <= N; i++)
	{
		const float T = (float)i / N;
		vec2 P = mix(Start, End, T);
		if(i != 0 && i != N)
			P.x += (Hash01(Seed + i * 7) - 0.5f) * 2.0f * Jitter;
		v.push_back(P);
	}
	return v;
}

void DrawBolt(IGraphics *pG, const std::vector<vec2> &v, float Thick, ColorRGBA Color)
{
	pG->TextureClear();
	pG->QuadsBegin();
	pG->SetColor(Color);
	for(size_t i = 0; i + 1 < v.size(); i++)
		Segment(pG, v[i], v[i + 1], Thick);
	pG->QuadsEnd();
}

// значок-молния (как в заголовке): рисуется квадами, шрифт не нужен
void DrawBoltIcon(IGraphics *pG, vec2 C, float S, ColorRGBA Color)
{
	pG->TextureClear();
	pG->QuadsBegin();
	pG->SetColor(Color);
	const vec2 P0 = C + vec2(0.25f, -1.0f) * S;
	const vec2 P1 = C + vec2(-0.45f, 0.1f) * S;
	const vec2 P2 = C + vec2(0.05f, 0.1f) * S;
	const vec2 P3 = C + vec2(-0.25f, 1.0f) * S;
	const vec2 P4 = C + vec2(0.5f, -0.2f) * S;
	const vec2 P5 = C + vec2(0.0f, -0.2f) * S;
	const IGraphics::CFreeformItem aItems[3] = {
		IGraphics::CFreeformItem(P0, P0 + vec2(0.35f, 0.0f) * S, P5, P5 + vec2(0.3f, 0.0f) * S),
		IGraphics::CFreeformItem(P1, P2, P5, P4),
		IGraphics::CFreeformItem(P2, P3, P4, P4 + vec2(-0.05f, 0.0f) * S),
	};
	pG->QuadsDrawFreeform(aItems, 3);
	pG->QuadsEnd();
}

} // namespace

// Тёмный фиолетово-чёрный градиент, углы медленно «дышат»
void CMenus::DrawSparkiGradient(const CUIRect &Rect)
{
	const float T = (float)(time_get() % (time_freq() * 3600)) / (float)time_freq();
	const auto Corner = [&](float Base, float Phase) {
		const float K = std::clamp(Base + 0.28f * std::sin(T * 0.7f + Phase), 0.0f, 1.0f);
		// лёгкий сдвиг оттенка между фиолетовым и сине-индиго
		const float Shift = 0.5f + 0.5f * std::sin(T * 0.35f + Phase * 1.7f);
		return ColorRGBA(K * (0.24f - 0.06f * Shift), K * 0.07f, K * (0.46f + 0.10f * Shift), 0.62f);
	};
	// TL, TR, BL, BR: сильнее всего слева снизу, слабее всего справа сверху
	Rect.Draw4(Corner(0.45f, 1.3f), Corner(0.08f, 3.1f), Corner(0.95f, 0.0f), Corner(0.45f, 4.4f), IGraphics::CORNER_ALL, 10.0f);
}

// Возвращает true, пока играет заставка (меню под ней не рисуется)
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
	const float Total = 6.0f;
	if(T > Total || Ui()->MouseButtonClicked(0) || Input()->KeyPress(KEY_ESCAPE) || Input()->KeyPress(KEY_SPACE) || Input()->KeyPress(KEY_RETURN))
	{
		m_IntroDone = true;
		return false;
	}

	Ui()->MapScreen();
	const CUIRect Screen = *Ui()->Screen();
	const float W = Screen.w;
	const float H = Screen.h;
	IGraphics *pG = Graphics();

	const float Fade = 1.0f - Clamp01((T - 5.4f) / 0.6f); // выход
	const float Strike = 2.2f; // время удара
	const float Flash = T >= Strike ? Clamp01(1.0f - (T - Strike) / 0.4f) : 0.0f;
	const float Flash2 = T >= Strike + 0.3f ? Clamp01(1.0f - (T - Strike - 0.3f) / 0.25f) : 0.0f;
	const float FlashAll = std::max(Flash, Flash2 * 0.7f);

	// фон: тёмно-фиолетовое небо
	CUIRect Full = Screen;
	const float Lift = FlashAll * 0.25f;
	Full.Draw4(ColorRGBA(0.03f + Lift, 0.02f + Lift * 0.8f, 0.08f + Lift, Fade), ColorRGBA(0.03f + Lift, 0.02f + Lift * 0.8f, 0.08f + Lift, Fade),
		ColorRGBA(0.13f + Lift, 0.05f + Lift * 0.8f, 0.24f + Lift, Fade), ColorRGBA(0.13f + Lift, 0.05f + Lift * 0.8f, 0.24f + Lift, Fade), IGraphics::CORNER_NONE, 0.0f);

	// облака пролетают справа налево
	const float CloudAlpha = Fade * (1.0f - Clamp01((T - 3.4f) / 0.8f));
	if(CloudAlpha > 0.01f)
	{
		pG->TextureClear();
		pG->QuadsBegin();
		const int NumClouds = 14;
		for(int i = 0; i < NumClouds; i++)
		{
			const float Hy = Hash01(i * 3 + 1), Hs = Hash01(i * 3 + 2), Hx = Hash01(i * 3 + 3);
			const float Size = 16.0f + Hs * 26.0f;
			const float Speed = 22.0f + Hs * 50.0f;
			const float Span = W + 260.0f;
			float X = std::fmod(Hx * Span - T * Speed, Span);
			if(X < 0.0f)
				X += Span;
			X -= 130.0f;
			const float Y = H * (0.08f + 0.5f * Hy);
			const float Bright = 0.16f + FlashAll * 0.55f + (1.0f - Hy) * 0.05f;
			pG->SetColor(Bright * 0.9f, Bright * 0.8f, Bright * 1.4f, CloudAlpha * (0.55f + 0.4f * Hs));
			static const vec2 s_aOff[6] = {vec2(0, 0), vec2(0.9f, 0.25f), vec2(-0.9f, 0.3f), vec2(0.4f, -0.45f), vec2(-0.4f, -0.3f), vec2(1.7f, 0.5f)};
			static const float s_aRad[6] = {1.0f, 0.8f, 0.75f, 0.8f, 0.7f, 0.55f};
			for(int k = 0; k < 6; k++)
				pG->DrawCircle(X + s_aOff[k].x * Size, Y + s_aOff[k].y * Size * 0.7f, s_aRad[k] * Size, 20);
		}
		pG->QuadsEnd();
	}

	// молния: главный удар и повторная вспышка
	const vec2 Center(W / 2.0f, H / 2.0f);
	if(T >= Strike && T < Strike + 0.65f)
	{
		const float Since = T - Strike;
		const bool Visible = Since < 0.3f || (Since > 0.3f && Since < 0.5f);
		if(Visible)
		{
			const int Seed = Since < 0.3f ? 11 : 29;
			const vec2 Start(Center.x + (Seed == 11 ? -30.0f : 18.0f), H * 0.12f);
			const vec2 End(Center.x + (Seed == 11 ? 0.0f : -8.0f), Center.y + 6.0f);
			const std::vector<vec2> Bolt = MakeBolt(Start, End, Seed, 26.0f);
			pG->BlendAdditive();
			DrawBolt(pG, Bolt, 7.0f, ColorRGBA(0.45f, 0.2f, 1.0f, 0.35f * Fade));
			DrawBolt(pG, Bolt, 3.2f, ColorRGBA(0.75f, 0.55f, 1.0f, 0.7f * Fade));
			pG->BlendNormal();
			DrawBolt(pG, Bolt, 1.3f, ColorRGBA(1.0f, 1.0f, 1.0f, Fade));
			// ответвление
			const std::vector<vec2> Branch = MakeBolt(Bolt[4], Bolt[4] + vec2(Seed == 11 ? 50.0f : -50.0f, 70.0f), Seed + 5, 14.0f);
			DrawBolt(pG, Branch, 0.9f, ColorRGBA(0.9f, 0.85f, 1.0f, 0.85f * Fade));
		}
	}

	// белая вспышка на весь экран
	if(FlashAll > 0.01f)
	{
		pG->BlendAdditive();
		pG->TextureClear();
		pG->QuadsBegin();
		pG->SetColor(0.8f, 0.7f, 1.0f, FlashAll * 0.5f * Fade);
		const IGraphics::CQuadItem Q(0, 0, W, H);
		pG->QuadsDrawTL(&Q, 1);
		pG->QuadsEnd();
		pG->BlendNormal();
	}

	// надпись «Sparki ⚡ Client»: буквы появляются по очереди
	const float TextStart = Strike + 0.55f;
	if(T >= TextStart)
	{
		TextRender()->SetPreferredFont("fonts/RussoOne-Regular.ttf");
		const float Size = 42.0f;
		const char *pA = "Sparki";
		const char *pB = "Client";
		const float WA = TextRender()->TextWidth(Size, pA);
		const float WB = TextRender()->TextWidth(Size, pB);
		const float IconW = Size * 0.7f;
		const float Gap = Size * 0.3f;
		const float TotalW = WA + Gap + IconW + Gap + WB;
		const float X0 = Center.x - TotalW / 2.0f;
		const float Y0 = Center.y - Size * 0.55f;
		const float Step = 0.08f;

		// мягкое свечение за надписью
		const float Glow = Clamp01((T - TextStart) / 0.8f) * Fade;
		pG->BlendAdditive();
		pG->TextureClear();
		pG->QuadsBegin();
		for(int i = 0; i < 4; i++)
		{
			pG->SetColor(0.35f, 0.15f, 0.8f, 0.05f * Glow);
			pG->DrawCircle(Center.x, Center.y, TotalW * (0.25f + 0.1f * i), 40);
		}
		pG->QuadsEnd();
		pG->BlendNormal();

		const auto DoWord = [&](const char *pWord, float X, int FirstIndex) {
			const int Len = (int)str_length(pWord);
			for(int i = 0; i < Len; i++)
			{
				const float Appear = Clamp01((T - TextStart - (FirstIndex + i) * Step) / 0.22f);
				if(Appear <= 0.0f)
					continue;
				char aPrefix[16];
				str_copy(aPrefix, pWord, i + 1);
				char aCh[2] = {pWord[i], 0};
				const float Px = X + TextRender()->TextWidth(Size, aPrefix);
				const float Pop = 1.0f - Appear;
				const float Hot = 1.0f - Appear; // в момент появления буква светится фиолетовым
				TextRender()->TextColor(1.0f - Hot * 0.4f, 1.0f - Hot * 0.6f, 1.0f, Appear * Fade);
				TextRender()->TextOutlineColor(0.4f, 0.15f, 0.9f, 0.5f * Appear * Fade);
				TextRender()->Text(Px, Y0 + Pop * 10.0f, Size, aCh);
			}
		};
		DoWord(pA, X0, 0);
		DoWord(pB, X0 + WA + Gap + IconW + Gap, 7);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
		TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor());

		// тонкая полоса загрузки под надписью
		{
			const float Prog = Clamp01((T - TextStart) / (5.4f - TextStart));
			const float BarW = TotalW * 0.8f;
			const float BarX = Center.x - BarW / 2.0f;
			const float BarY = Center.y + Size * 0.95f;
			CUIRect Bar(BarX, BarY, BarW, 3.0f);
			Bar.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.12f * Fade), IGraphics::CORNER_ALL, 1.5f);
			CUIRect Fill(BarX, BarY, BarW * Prog, 3.0f);
			Fill.Draw4(ColorRGBA(0.4f, 0.2f, 0.9f, Fade), ColorRGBA(0.8f, 0.6f, 1.0f, Fade), ColorRGBA(0.4f, 0.2f, 0.9f, Fade), ColorRGBA(0.8f, 0.6f, 1.0f, Fade), IGraphics::CORNER_ALL, 1.5f);
		}
		TextRender()->SetPreferredFont("");

		// значок молнии между словами появляется после «Sparki»
		const float IconAppear = Clamp01((T - TextStart - 6 * Step) / 0.25f);
		if(IconAppear > 0.0f)
		{
			const vec2 IconC(X0 + WA + Gap + IconW / 2.0f, Center.y);
			const float S = Size * 0.5f * (0.6f + 0.4f * IconAppear);
			pG->BlendAdditive();
			DrawBoltIcon(pG, IconC, S * 1.35f, ColorRGBA(0.5f, 0.25f, 1.0f, 0.5f * IconAppear * Fade));
			pG->BlendNormal();
			DrawBoltIcon(pG, IconC, S, ColorRGBA(1.0f, 0.95f, 0.35f, IconAppear * Fade));
		}
	}
	return true;
}
