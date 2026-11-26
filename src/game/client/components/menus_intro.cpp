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

} // namespace

// Тёмный фиолетово-чёрный градиент, углы очень медленно и плавно «дышат».
// Frame — общий прямоугольник, по которому считается градиент, чтобы составные панели не имели шва.
void CMenus::DrawSparkiGradientIn(const CUIRect &Frame, const CUIRect &Rect, int Corners, float Rounding)
{
	const float T = (float)(time_get() % (time_freq() * 3600)) / (float)time_freq();
	const auto Corner = [&](float Base, float Phase) {
		const float K = std::clamp(Base + 0.24f * std::sin(T * 0.45f + Phase), 0.0f, 1.0f);
		const float Shift = 0.5f + 0.5f * std::sin(T * 0.25f + Phase * 1.7f);
		return ColorRGBA(K * (0.19f - 0.05f * Shift), K * 0.055f, K * (0.38f + 0.08f * Shift), 0.92f);
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

	// светящаяся галактика на фоне (появляется вместе с раскрытием, медленно вращается)
	{
		const float Gal = Ease((T - 0.9f) / 1.6f) * Fade;
		if(Gal > 0.01f)
		{
			const float R = H * 0.95f;
			const float Rot = T * 0.12f;
			const float Tilt = 0.38f;
			pG->BlendAdditive();
			pG->TextureClear();
			pG->QuadsBegin();
			// ядро и пылевое гало
			for(int i = 0; i < 7; i++)
			{
				pG->SetColor(0.65f, 0.35f, 1.0f, 0.03f * Gal);
				pG->DrawCircle(Center.x, Center.y, R * (0.08f + 0.075f * i), 40);
			}
			pG->SetColor(1.0f, 0.85f, 0.95f, 0.06f * Gal);
			pG->DrawCircle(Center.x, Center.y, R * 0.07f, 32);
			// звёзды спиральных рукавов
			const int NumStars = 900;
			for(int i = 0; i < NumStars; i++)
			{
				const float Tt = std::pow(Hash01(i * 7 + 1), 0.8f);
				const int Arm = i % 3;
				const float Ang = Arm * 2.0944f + Tt * 4.6f + Rot + (Hash01(i * 7 + 2) - 0.5f) * (0.55f - 0.3f * Tt);
				const float Rad = Tt * R * 0.62f + (Hash01(i * 7 + 3) - 0.5f) * R * 0.03f;
				const float Xr = std::cos(Ang) * Rad;
				const float Yr = std::sin(Ang) * Rad * Tilt;
				// наклон всей галактики
				const float Ca = std::cos(-0.35f), Sa = std::sin(-0.35f);
				const float X = Center.x + Xr * Ca - Yr * Sa;
				const float Y = Center.y + Xr * Sa + Yr * Ca;
				const float Tw = 0.6f + 0.4f * std::sin(T * (1.0f + Hash01(i * 7 + 4) * 2.0f) + i);
				const float Size = 0.7f + 1.6f * Hash01(i * 7 + 5) * (1.0f - 0.5f * Tt);
				// цвет: тёплое ядро -> сине-фиолетовые края
				const float Rr = 1.0f - 0.55f * Tt, Gg = 0.7f - 0.35f * Tt, Bb = 0.95f;
				pG->SetColor(Rr, Gg, Bb, (0.35f + 0.5f * (1.0f - Tt)) * Tw * Gal);
				const IGraphics::CQuadItem Q(X - Size / 2, Y - Size / 2, Size, Size);
				pG->QuadsDrawTL(&Q, 1);
			}
			pG->QuadsEnd();
			pG->BlendNormal();
		}
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

	// надпись «GALAXY CLIENT»: буквы разъезжаются из широкого интервала в плотный
	const float TextStart = 1.7f;
	if(T >= TextStart)
	{
		TextRender()->SetPreferredFont("fonts/RussoOne-Regular.ttf");
		const float Size = 44.0f;
		const char *apWords[2] = {"GALAXY", "CLIENT"};
		const float TrackP = Ease((T - TextStart) / 1.4f);
		const float Track = Size * (0.12f + 0.9f * (1.0f - TrackP));
		const float Gap2 = Size * 0.55f;

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
		const float Alpha = Ease((T - TextStart) / 0.8f) * Fade;

		// переливы: приятный пурпурный <-> белый, волной по буквам
		const ColorRGBA aBase[2] = {ColorRGBA(0.66f, 0.42f, 1.0f, 1.0f), ColorRGBA(0.66f, 0.42f, 1.0f, 1.0f)};
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
			X += Gap2 - Track;
		}
		TextRender()->TextColor(TextRender()->DefaultTextColor());
		TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor());

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
