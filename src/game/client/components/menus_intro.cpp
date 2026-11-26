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

// Тёмный фиолетово-чёрный градиент, углы медленно и плавно «дышат»
void CMenus::DrawSparkiGradient(const CUIRect &Rect, int Corners, float Rounding)
{
	const float T = (float)(time_get() % (time_freq() * 3600)) / (float)time_freq();
	const auto Corner = [&](float Base, float Phase) {
		const float K = std::clamp(Base + 0.22f * std::sin(T * 0.32f + Phase), 0.0f, 1.0f);
		const float Shift = 0.5f + 0.5f * std::sin(T * 0.17f + Phase * 1.7f);
		return ColorRGBA(K * (0.24f - 0.06f * Shift), K * 0.07f, K * (0.46f + 0.10f * Shift), 0.66f);
	};
	// TL, TR, BL, BR: сильнее всего слева снизу, слабее всего справа сверху
	Rect.Draw4(Corner(0.45f, 1.3f), Corner(0.08f, 3.1f), Corner(0.95f, 0.0f), Corner(0.45f, 4.4f), Corners, Rounding);
}

// Возвращает true, пока играет заставка (пропустить нельзя)
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
	const float Total = 5.4f;
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
	const float Fade = 1.0f - Ease((T - 4.8f) / 0.6f);

	// фон: тёмное небо с мягким фиолетовым свечением в центре
	{
		CUIRect Full = Screen;
		Full.Draw4(ColorRGBA(0.02f, 0.01f, 0.05f, Fade), ColorRGBA(0.02f, 0.01f, 0.05f, Fade), ColorRGBA(0.10f, 0.04f, 0.20f, Fade), ColorRGBA(0.10f, 0.04f, 0.20f, Fade), IGraphics::CORNER_NONE, 0.0f);
		const float Glow = Ease((T - 0.6f) / 1.2f) * Fade;
		pG->BlendAdditive();
		pG->TextureClear();
		pG->QuadsBegin();
		for(int i = 0; i < 6; i++)
		{
			pG->SetColor(0.30f, 0.12f, 0.70f, 0.035f * Glow);
			pG->DrawCircle(Center.x, Center.y, H * (0.18f + 0.12f * i), 48);
		}
		pG->QuadsEnd();
		pG->BlendNormal();
	}

	// искры, вылетающие с линии при раскрытии
	{
		pG->BlendAdditive();
		pG->TextureClear();
		pG->QuadsBegin();
		for(int i = 0; i < 48; i++)
		{
			const float T0 = 0.5f + Hash01(i * 5 + 1) * 1.6f;
			const float Age = T - T0;
			if(Age < 0.0f || Age > 1.6f)
				continue;
			const float Dir = (i % 2) ? 1.0f : -1.0f;
			const float X = Center.x + (Hash01(i * 5 + 2) - 0.5f) * W * 0.9f;
			const float Y = Center.y + Dir * Age * (25.0f + 70.0f * Hash01(i * 5 + 3));
			pG->SetColor(0.8f, 0.6f, 1.0f, (1.0f - Age / 1.6f) * 0.8f * Fade);
			pG->DrawCircle(X, Y, 0.8f + 1.2f * Hash01(i * 5 + 4), 8);
		}
		pG->QuadsEnd();
		pG->BlendNormal();
	}

	// «занавес»: линия разгорается от центра, затем створки раздвигаются вверх и вниз
	const float LineP = Ease(T / 0.9f);
	const float OpenP = Ease((T - 0.9f) / 0.9f);
	const float Gap = OpenP * H * 0.5f;
	{
		const float CurtainA = Fade;
		// закрывающие половины: чёрные, сужаются к краям экрана
		const float TopH = std::max(0.0f, Center.y - Gap);
		if(TopH > 0.0f)
		{
			CUIRect Top(0, 0, W, TopH);
			Top.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, CurtainA), IGraphics::CORNER_NONE, 0.0f);
			CUIRect Bottom(0, Center.y + Gap, W, H - (Center.y + Gap));
			Bottom.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, CurtainA), IGraphics::CORNER_NONE, 0.0f);
		}
		// светящиеся кромки створок
		const float HalfLen = LineP * W * 0.5f;
		pG->BlendAdditive();
		for(int Side = -1; Side <= 1; Side += 2)
		{
			const float Y = Center.y + Side * Gap;
			const float Len = (OpenP > 0.0f) ? W * 0.5f : HalfLen;
			pG->TextureClear();
			pG->QuadsBegin();
			pG->SetColor(0.45f, 0.2f, 1.0f, 0.28f * Fade);
			CUIRect Glow(Center.x - Len, Y - 4.0f, Len * 2.0f, 8.0f);
			const IGraphics::CQuadItem Q1(Glow.x, Glow.y, Glow.w, Glow.h);
			pG->QuadsDrawTL(&Q1, 1);
			pG->SetColor(0.85f, 0.7f, 1.0f, 0.9f * Fade);
			const IGraphics::CQuadItem Q2(Center.x - Len, Y - 0.8f, Len * 2.0f, 1.6f);
			pG->QuadsDrawTL(&Q2, 1);
			pG->QuadsEnd();
			if(Gap <= 0.0f)
				break;
		}
		pG->BlendNormal();
	}

	// надпись «SPARKI ⚡ CLIENT»: буквы разъезжаются из широкого интервала в плотный
	const float TextStart = 1.7f;
	if(T >= TextStart)
	{
		TextRender()->SetPreferredFont("fonts/RussoOne-Regular.ttf");
		const float Size = 44.0f;
		const char *apWords[2] = {"SPARKI", "CLIENT"};
		const float TrackP = Ease((T - TextStart) / 1.4f);
		const float Track = Size * (0.12f + 0.9f * (1.0f - TrackP));
		const float IconW = Size * 0.8f;
		const float Gap2 = Size * 0.35f;

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
		const float TotalW = aWordW[0] + Gap2 + IconW + Gap2 + aWordW[1];
		const float Y0 = Center.y - Size * 0.55f;
		const float Alpha = Ease((T - TextStart) / 0.8f) * Fade;

		// переливы: SPARKI — жёлтый <-> белый, CLIENT — приятный пурпурный <-> белый
		const ColorRGBA aBase[2] = {ColorRGBA(1.0f, 0.82f, 0.22f, 1.0f), ColorRGBA(0.66f, 0.42f, 1.0f, 1.0f)};
		float X = Center.x - TotalW / 2.0f;
		for(int Wd = 0; Wd < 2; Wd++)
		{
			int Index = 0;
			for(const char *p = apWords[Wd]; *p; p++, Index++)
			{
				char aCh[2] = {*p, 0};
				const float Mixv = 0.5f + 0.5f * std::sin(T * 1.6f - (Wd * 6 + Index) * 0.45f);
				const float K = Mixv * 0.85f;
				const ColorRGBA C(aBase[Wd].r + (1.0f - aBase[Wd].r) * K, aBase[Wd].g + (1.0f - aBase[Wd].g) * K, aBase[Wd].b + (1.0f - aBase[Wd].b) * K, 1.0f);
				TextRender()->TextColor(C.r, C.g, C.b, Alpha);
				TextRender()->TextOutlineColor(0.35f, 0.12f, 0.8f, 0.45f * Alpha);
				TextRender()->Text(X, Y0, Size, aCh);
				X += TextRender()->TextWidth(Size, aCh) + Track;
			}
			X += Gap2 + IconW + Gap2 - Track; // место под значок после первого слова
		}
		TextRender()->TextColor(TextRender()->DefaultTextColor());
		TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor());

		// молния-значок бьёт между словами со вспышкой
		const float Zap = 2.7f;
		if(T >= Zap)
		{
			const float App = Ease((T - Zap) / 0.25f);
			const vec2 IconC(Center.x - TotalW / 2.0f + aWordW[0] + Gap2 + IconW / 2.0f, Center.y);
			const float S = Size * 0.55f * (0.6f + 0.4f * App);
			const float Flash = Clamp01(1.0f - (T - Zap) / 0.35f);
			if(Flash > 0.0f)
			{
				const std::vector<vec2> Bolt = MakeBolt(vec2(IconC.x + 6.0f, 0.0f), IconC, 17, 24.0f);
				pG->BlendAdditive();
				DrawBolt(pG, Bolt, 6.0f, ColorRGBA(0.45f, 0.2f, 1.0f, 0.4f * Flash * Fade));
				DrawBolt(pG, Bolt, 2.0f, ColorRGBA(0.9f, 0.8f, 1.0f, 0.9f * Flash * Fade));
				pG->BlendNormal();
			}
			pG->BlendAdditive();
			DrawBoltIcon(pG, IconC, S * 1.4f, ColorRGBA(0.5f, 0.25f, 1.0f, 0.5f * App * Fade));
			pG->BlendNormal();
			DrawBoltIcon(pG, IconC, S, ColorRGBA(1.0f, 0.9f, 0.4f, App * Fade));
		}

		// полоса загрузки
		{
			const float Prog = Ease((T - TextStart) / (4.7f - TextStart));
			const float BarW = TotalW * 0.85f;
			const float BarX = Center.x - BarW / 2.0f;
			const float BarY = Center.y + Size * 1.0f;
			CUIRect Bar(BarX, BarY, BarW, 3.0f);
			Bar.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.12f * Fade), IGraphics::CORNER_ALL, 1.5f);
			CUIRect Fill(BarX, BarY, BarW * Prog, 3.0f);
			Fill.Draw4(ColorRGBA(0.4f, 0.2f, 0.9f, Alpha), ColorRGBA(0.85f, 0.65f, 1.0f, Alpha), ColorRGBA(0.4f, 0.2f, 0.9f, Alpha), ColorRGBA(0.85f, 0.65f, 1.0f, Alpha), IGraphics::CORNER_ALL, 1.5f);
		}
		TextRender()->SetPreferredFont("");
	}
	return true;
}
