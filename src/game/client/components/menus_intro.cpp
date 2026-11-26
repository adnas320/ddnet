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
// Сцена: вращающаяся фиолетовая галактика -> сжатие в ядро -> фиолетовый взрыв лучами -> звёздная пыль и название.
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
	const float Total = 9.2f;
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
	const float Boom = 3.4f; // момент взрыва
	const float Age = T - Boom; // время с взрыва
	const float Fade = Ease(T / 0.9f) * (1.0f - Ease((T - 8.4f) / 0.8f));

	// фон: почти чёрный с фиолетовым оттенком
	{
		CUIRect Full = Screen;
		Full.Draw4(ColorRGBA(0.004f, 0.002f, 0.014f, Fade), ColorRGBA(0.006f, 0.003f, 0.02f, Fade), ColorRGBA(0.012f, 0.005f, 0.035f, Fade), ColorRGBA(0.008f, 0.004f, 0.026f, Fade), IGraphics::CORNER_NONE, 0.0f);
	}

	pG->TextureClear();
	pG->BlendAdditive();

	// ---------- галактика ----------
	const float GalIn = Ease(T / 1.6f);
	const float GalOut = 1.0f - Ease((Age - 0.05f) / 0.5f);
	const float GalA = GalIn * GalOut * Fade;
	if(GalA > 0.005f)
	{
		const float Charge = Ease((T - 2.5f) / (Boom - 2.5f)); // сжатие перед взрывом
		const float Scale = (0.72f + 0.28f * Ease(T / 3.0f)) * (1.0f - 0.55f * Charge);
		const float R = H * 0.46f * Scale;
		const float Rot = T * 0.28f + Charge * Charge * 3.0f; // перед взрывом закручивается быстрее
		const float Tilt = -0.2f;
		const float Ct = std::cos(Tilt), St = std::sin(Tilt);
		const auto Project = [&](float Rad, float Ang) {
			const float X = std::cos(Ang) * Rad;
			const float Y = std::sin(Ang) * Rad * 0.55f;
			return vec2(Center.x + X * Ct - Y * St, Center.y + X * St + Y * Ct);
		};

		// свечение ядра и диска
		pG->QuadsBegin();
		for(int i = 0; i < 9; i++)
		{
			const float K = (float)(i + 1) / 9.0f;
			pG->SetColor(0.55f, 0.3f, 1.0f, (0.022f + 0.02f * (1.0f - K)) * GalA * (1.0f + Charge));
			pG->DrawCircle(Center.x, Center.y, R * 0.75f * K, 40);
		}
		pG->SetColor(1.0f, 0.95f, 1.0f, (0.5f + 0.5f * Charge) * GalA);
		pG->DrawCircle(Center.x, Center.y, R * (0.07f + 0.05f * Charge), 28);
		pG->SetColor(0.9f, 0.8f, 1.0f, 0.35f * GalA);
		pG->DrawCircle(Center.x, Center.y, R * 0.16f, 32);
		// звёзды двух рукавов
		const int NumStars = 1500;
		for(int i = 0; i < NumStars; i++)
		{
			const float U = std::pow(Hash01(i * 9 + 1), 1.35f);
			const int Arm = i % 2;
			const float Wind = U * 5.4f;
			const float Diff = Rot * (1.0f - 0.55f * U); // дифференциальное вращение
			const float Scatter = (Hash01(i * 9 + 2) - 0.5f) * (0.5f - 0.3f * U);
			const vec2 Pos = Project(U * R, Arm * 3.14159f + Wind + Diff + Scatter);
			const float Tw = 0.75f + 0.25f * std::sin(T * (1.0f + Hash01(i * 9 + 3) * 2.0f) + (float)i);
			const float Sz = 0.5f + 1.3f * Hash01(i * 9 + 4) * (1.0f - 0.5f * U);
			const float Kc = U; // центр белый -> края глубокий пурпур
			pG->SetColor(1.0f - 0.45f * Kc, 0.92f - 0.6f * Kc, 1.0f - 0.1f * Kc, (0.18f + 0.6f * (1.0f - U)) * Tw * GalA);
			const IGraphics::CQuadItem Q(Pos.x - Sz / 2, Pos.y - Sz / 2, Sz, Sz);
			pG->QuadsDrawTL(&Q, 1);
		}
		// светлые дуги рукавов — «завихрения»
		for(int i = 0; i < 220; i++)
		{
			const float U = 0.1f + 0.9f * Hash01(i * 5 + 1);
			const int Arm = i % 2;
			const float Ang = Arm * 3.14159f + U * 5.4f + Rot * (1.0f - 0.55f * U) + (Hash01(i * 5 + 2) - 0.5f) * 0.25f;
			const vec2 A = Project(U * R, Ang);
			const vec2 B = Project(U * R, Ang + 0.07f + 0.06f * Hash01(i * 5 + 3));
			pG->SetColor(0.85f, 0.7f, 1.0f, 0.22f * (1.0f - 0.5f * U) * GalA);
			Segment(pG, A, B, 0.5f);
		}
		pG->QuadsEnd();
	}

	// ---------- взрыв ----------
	if(Age > 0.0f)
	{
		pG->QuadsBegin();
		// вспышка и затухающее свечение
		const float FlashA = std::max(0.0f, 1.0f - Age / 0.5f);
		pG->SetColor(0.75f, 0.55f, 1.0f, 0.55f * FlashA * Fade);
		const IGraphics::CQuadItem Full(0, 0, W, H);
		pG->QuadsDrawTL(&Full, 1);
		for(int i = 0; i < 8; i++)
		{
			const float K = (float)(i + 1) / 8.0f;
			const float Glow = (0.05f + 0.1f * std::exp(-Age * 1.2f)) * (1.0f - 0.7f * Ease(Age / 6.0f));
			pG->SetColor(0.5f, 0.22f, 1.0f, Glow * Fade * 0.6f);
			pG->DrawCircle(Center.x, Center.y, H * (0.12f + 0.55f * K) * (0.5f + 0.5f * std::min(1.0f, Age)), 48);
		}
		pG->SetColor(1.0f, 0.97f, 1.0f, std::max(0.0f, 1.0f - Age / 0.7f) * Fade);
		pG->DrawCircle(Center.x, Center.y, H * (0.05f + 0.16f * Age), 32);

		// лучи-частицы: быстрый старт с трением, хвост длиннее у быстрых
		const int NumRays = 1700;
		const float K = 1.25f;
		for(int i = 0; i < NumRays; i++)
		{
			const float Ang = Hash01(i * 13 + 1) * 6.2831853f;
			const float V = 70.0f + std::pow(Hash01(i * 13 + 2), 1.6f) * 560.0f;
			const float Life = 2.0f + Hash01(i * 13 + 3) * 2.6f;
			if(Age > Life)
				continue;
			const float Dist = V * (1.0f - std::exp(-K * Age)) / K;
			const float Speed = V * std::exp(-K * Age);
			const vec2 Dir(std::cos(Ang), std::sin(Ang));
			const vec2 Pos = Center + Dir * Dist;
			const float Len = std::max(0.8f, Speed * 0.07f);
			const float L = 1.0f - Age / Life;
			const float Ch = Hash01(i * 13 + 4);
			// белое горячее ядро -> лавандовый -> насыщенный пурпур
			const float R2 = 0.62f + 0.38f * (1.0f - Ch) * L;
			const float G2 = 0.3f + 0.6f * (1.0f - Ch) * L;
			pG->SetColor(R2, G2, 1.0f, std::pow(L, 1.2f) * 0.85f * Fade);
			Segment(pG, Pos - Dir * Len, Pos, 0.22f + 0.4f * Hash01(i * 13 + 5));
		}
		// ударная волна
		{
			const float RingP = Age / 1.6f;
			if(RingP < 1.0f)
			{
				const float RingR = 440.0f * (1.0f - (1.0f - RingP) * (1.0f - RingP));
				const float RA = (1.0f - RingP) * 0.5f * Fade;
				const int Seg = 72;
				for(int i = 0; i < Seg; i++)
				{
					const float A0 = (float)i / Seg * 6.2831853f, A1 = (float)(i + 1) / Seg * 6.2831853f;
					pG->SetColor(0.7f, 0.5f, 1.0f, RA);
					Segment(pG, Center + vec2(std::cos(A0), std::sin(A0)) * RingR, Center + vec2(std::cos(A1), std::sin(A1)) * RingR, 1.3f * (1.0f - RingP) + 0.3f);
				}
			}
		}
		// четырёхлучевые блики и пыль, как после салюта
		const float GlintIn = Ease((Age - 0.6f) / 1.2f);
		const float GlintOut = 1.0f - Ease((T - 8.3f) / 0.8f);
		for(int i = 0; i < 90; i++)
		{
			const float Gx = Hash01(i * 7 + 1) * W, Gy = Hash01(i * 7 + 2) * H;
			const float Tw = std::max(0.0f, std::sin(T * (0.6f + Hash01(i * 7 + 3) * 1.6f) + (float)i * 1.7f));
			const float Sz = (2.5f + 6.0f * Hash01(i * 7 + 4)) * Tw;
			const float A = Tw * 0.8f * GlintIn * GlintOut * Fade;
			if(A < 0.02f)
				continue;
			pG->SetColor(0.85f, 0.7f, 1.0f, A);
			Segment(pG, vec2(Gx - Sz, Gy), vec2(Gx + Sz, Gy), 0.25f);
			Segment(pG, vec2(Gx, Gy - Sz), vec2(Gx, Gy + Sz), 0.25f);
			pG->SetColor(1.0f, 0.95f, 1.0f, A);
			const IGraphics::CQuadItem Dot(Gx - 0.5f, Gy - 0.5f, 1.0f, 1.0f);
			pG->QuadsDrawTL(&Dot, 1);
		}
		pG->QuadsEnd();
	}
	pG->BlendNormal();

	// ---------- название: квадратные «значки»-буквы с широким интервалом ----------
	const float TextStart = Boom + 0.7f;
	if(T >= TextStart)
	{
		TextRender()->SetPreferredFont("fonts/RussoOne-Regular.ttf");
		const char *apWords[2] = {"GALAXY", "CLIENT"};
		const float B = std::min(26.0f, W * 0.0625f); // сторона значка
		const float Gap = B * 0.18f;
		const float WordGap = B * 0.6f;
		const float TotalW = 12 * B + 10 * Gap + WordGap;
		const float FontSize = B * 0.66f;
		const float Y0 = Center.y - B / 2.0f;
		float X = Center.x - TotalW / 2.0f;
		int Global = 0;
		for(int Wd = 0; Wd < 2; Wd++)
		{
			for(const char *pCh = apWords[Wd]; *pCh; pCh++, Global++)
			{
				const float Appear = Ease((T - TextStart - Global * 0.11f) / 0.7f) * Fade;
				const float Pop = 1.0f - Ease((T - TextStart - Global * 0.11f) / 0.7f);
				const float Y = Y0 + Pop * 8.0f;
				const float Shimmer = (0.5f + 0.5f * std::sin(T * 1.4f - Global * 0.45f)) * 0.8f;
				if(Appear > 0.001f)
				{
					// мягкое свечение, рамка и тёмная подложка значка
					pG->BlendAdditive();
					pG->TextureClear();
					pG->QuadsBegin();
					for(int g = 0; g < 3; g++)
					{
						pG->SetColor(0.5f, 0.25f, 1.0f, 0.07f * Appear);
						pG->DrawCircle(X + B / 2.0f, Y + B / 2.0f, B * (0.7f + 0.2f * g), 24);
					}
					pG->QuadsEnd();
					pG->BlendNormal();
					CUIRect Outer(X, Y, B, B);
					Outer.Draw(ColorRGBA(0.62f, 0.38f, 1.0f, 0.75f * Appear), IGraphics::CORNER_ALL, B * 0.2f);
					CUIRect Inner(X + 1.0f, Y + 1.0f, B - 2.0f, B - 2.0f);
					Inner.Draw(ColorRGBA(0.045f, 0.02f, 0.11f, 0.92f * Appear), IGraphics::CORNER_ALL, B * 0.18f);
					char aCh[2] = {*pCh, 0};
					const float Cw = TextRender()->TextWidth(FontSize, aCh);
					TextRender()->TextColor(0.66f + 0.34f * Shimmer, 0.42f + 0.58f * Shimmer, 1.0f, Appear);
					TextRender()->TextOutlineColor(0.3f, 0.1f, 0.7f, 0.4f * Appear);
					TextRender()->Text(X + (B - Cw) / 2.0f, Y + (B - FontSize * 1.15f) / 2.0f, FontSize, aCh);
				}
				X += B + Gap;
			}
			X += WordGap - Gap;
		}
		TextRender()->TextColor(TextRender()->DefaultTextColor());
		TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor());
		TextRender()->SetPreferredFont("");
	}
	return true;
}
