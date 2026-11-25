#include "fx.h"

#include <base/math.h>

#include <engine/client.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/gameclient.h>

#include <algorithm>
#include <cmath>

namespace {

float Hash(int I, int Salt)
{
	unsigned H = (unsigned)(I * 73856093) ^ (unsigned)(Salt * 19349663);
	H ^= H >> 13;
	H *= 0x5bd1e995u;
	H ^= H >> 15;
	return (H & 0xffffu) / 65535.0f;
}

float Frac(float V)
{
	return V - std::floor(V);
}

ColorRGBA Hue(float H, float Alpha = 1.0f)
{
	ColorRGBA C = color_cast<ColorRGBA>(ColorHSLA(Frac(H), 1.0f, 0.5f));
	C.a = Alpha;
	return C;
}

// small drawing helpers that work on an open quad batch
void DrawDisc(IGraphics *pG, vec2 C, float R, ColorRGBA Col)
{
	pG->SetColor(Col.r, Col.g, Col.b, Col.a);
	pG->DrawCircle(C.x, C.y, std::max(0.5f, R), 14);
}

void DrawLine(IGraphics *pG, vec2 A, vec2 B, float Width, ColorRGBA Col)
{
	const vec2 D = B - A;
	const float Len = length(D);
	if(Len < 0.001f)
		return;
	const vec2 N = vec2(-D.y, D.x) / Len * Width * 0.5f;
	pG->SetColor(Col.r, Col.g, Col.b, Col.a);
	const IGraphics::CFreeformItem Item(A.x - N.x, A.y - N.y, A.x + N.x, A.y + N.y, B.x - N.x, B.y - N.y, B.x + N.x, B.y + N.y);
	pG->QuadsDrawFreeform(&Item, 1);
}

void DrawRing(IGraphics *pG, vec2 C, float R, float Thick, float A0, float A1, ColorRGBA Col)
{
	const int Segs = std::max(2, (int)(std::fabs(A1 - A0) / (2.0f * pi) * 48.0f));
	pG->SetColor(Col.r, Col.g, Col.b, Col.a);
	for(int i = 0; i < Segs; i++)
	{
		const float a0 = A0 + (A1 - A0) * i / Segs, a1 = A0 + (A1 - A0) * (i + 1) / Segs;
		const IGraphics::CFreeformItem Item(C.x + std::cos(a0) * (R - Thick), C.y + std::sin(a0) * (R - Thick), C.x + std::cos(a0) * (R + Thick), C.y + std::sin(a0) * (R + Thick),
			C.x + std::cos(a1) * (R - Thick), C.y + std::sin(a1) * (R - Thick), C.x + std::cos(a1) * (R + Thick), C.y + std::sin(a1) * (R + Thick));
		pG->QuadsDrawFreeform(&Item, 1);
	}
}

} // namespace

void CFx::OnReset()
{
	m_vCombos.clear();
	m_vHits.clear();
	m_vLasers.clear();
	m_FireInitialized = false;
}

void CFx::OnHammerHit(vec2 Pos)
{
	if(!g_Config.m_ClFxHammer)
		return;
	if(m_vHits.size() < 32)
		m_vHits.push_back({Pos, (float)std::fmod(Client()->LocalTime(), 10000.0)});
}

void CFx::OnLaser(vec2 From, vec2 To, float Fade)
{
	if(g_Config.m_ClFxLaser && m_vLasers.size() < 64)
		m_vLasers.push_back({From, To, Fade, (int)(From.x * 3.0f + From.y * 7.0f)});
}

// ---------------------------------------------------------------- hammer combo

void CFx::DrawCombo(vec2 Pos, int Count, float SinceHit, float Remaining, float SinceMilestone, int Style, float Power, ColorRGBA Col)
{
	const float P = std::clamp(Power, 0.2f, 2.0f);
	const float Pop = 1.0f + 0.55f * std::exp(-SinceHit * 11.0f); // every hit makes the number jump
	const float Alpha = std::clamp(Remaining * 4.0f, 0.0f, 1.0f);
	const float Size = 22.0f * P * Pop;
	char aText[16];
	str_format(aText, sizeof(aText), "x%d", Count);
	const bool Milestone = SinceMilestone >= 0.0f && SinceMilestone < 1.3f;

	// shapes first, the text comes afterwards
	Graphics()->TextureClear();
	Graphics()->BlendAdditive();
	Graphics()->QuadsBegin();
	if(Style == 1)
	{
		// a badge with a ring that runs out in one second: that is the time left for the next hit
		const float R = 17.0f * P * (0.9f + 0.1f * Pop);
		Graphics()->BlendNormal();
		DrawDisc(Graphics(), Pos, R, ColorRGBA(0.05f, 0.05f, 0.08f, 0.65f * Alpha));
		DrawRing(Graphics(), Pos, R, 1.6f * P, -pi / 2.0f, -pi / 2.0f + 2.0f * pi * std::clamp(Remaining, 0.0f, 1.0f), ColorRGBA(Col.r, Col.g, Col.b, Alpha));
	}
	else if(Style == 2)
	{
		// sparks fly off with every hit
		const float Out = 1.0f - std::exp(-SinceHit * 7.0f);
		for(int i = 0; i < 12; i++)
		{
			const float A = 2.0f * pi * i / 12.0f + Count * 0.7f;
			const vec2 D(std::cos(A), std::sin(A));
			const float R0 = 14.0f * P + 28.0f * P * Out * (0.7f + 0.3f * Hash(i, Count));
			DrawLine(Graphics(), Pos + D * R0, Pos + D * (R0 + 7.0f * P * (1.0f - Out)), 2.0f * P, ColorRGBA(Col.r, Col.g, Col.b, (1.0f - Out) * 0.9f * Alpha));
		}
	}
	else if(Style == 3)
	{
		// neon: a soft glow and a bar below that shows the time left
		for(int i = 3; i >= 1; i--)
			DrawDisc(Graphics(), Pos, (9.0f + 8.0f * i) * P * Pop, ColorRGBA(Col.r, Col.g, Col.b, 0.07f * Alpha));
		DrawLine(Graphics(), Pos + vec2(-24.0f * P, 19.0f * P), Pos + vec2(-24.0f * P + 48.0f * P * std::clamp(Remaining, 0.0f, 1.0f), 19.0f * P), 3.0f * P, ColorRGBA(Col.r, Col.g, Col.b, 0.9f * Alpha));
	}
	if(Milestone)
	{
		// 100 hits: three rings fly out
		for(int i = 0; i < 3; i++)
		{
			const float T = std::clamp((SinceMilestone - i * 0.12f) / 0.9f, 0.0f, 1.0f);
			if(T <= 0.0f || T >= 1.0f)
				continue;
			DrawRing(Graphics(), Pos, 14.0f + 70.0f * T, 3.0f * (1.0f - T) + 0.5f, 0.0f, 2.0f * pi, ColorRGBA(1.0f, 0.9f, 0.4f, 0.8f * (1.0f - T)));
		}
	}
	Graphics()->QuadsEnd();
	Graphics()->BlendNormal();

	// the number
	const float W = TextRender()->TextWidth(Size, aText);
	const ColorRGBA Text = Style == 3 ? ColorRGBA(std::min(1.0f, Col.r + 0.4f), std::min(1.0f, Col.g + 0.4f), std::min(1.0f, Col.b + 0.4f), 1.0f) : Col;
	TextRender()->TextColor(0.0f, 0.0f, 0.0f, 0.6f * Alpha);
	TextRender()->Text(Pos.x - W / 2.0f + 1.5f, Pos.y - Size * 0.55f + 1.5f, Size, aText);
	TextRender()->TextColor(Text.r, Text.g, Text.b, Alpha);
	TextRender()->Text(Pos.x - W / 2.0f, Pos.y - Size * 0.55f, Size, aText);
	if(Milestone)
	{
		const float S2 = 30.0f * P * (1.0f + 0.4f * std::exp(-SinceMilestone * 6.0f));
		const float W2 = TextRender()->TextWidth(S2, "100!");
		TextRender()->TextColor(1.0f, 0.9f, 0.4f, std::clamp(1.3f - SinceMilestone, 0.0f, 1.0f));
		TextRender()->Text(Pos.x - W2 / 2.0f, Pos.y - 46.0f * P - S2 * 0.5f, S2, "100!");
	}
	TextRender()->TextColor(TextRender()->DefaultTextColor());
}

// ---------------------------------------------------------------- laser

void CFx::DrawLaserFx(vec2 From, vec2 To, float Fade, float Time, int Style, float Power, ColorRGBA Col, int Seed)
{
	const float P = std::clamp(Power, 0.2f, 2.0f);
	const vec2 Delta = To - From;
	const float Len = length(Delta);
	if(Len < 4.0f || Fade <= 0.0f)
		return;
	const vec2 D = Delta / Len, N(-D.y, D.x);
	const ColorRGBA Glow(Col.r, Col.g, Col.b, 1.0f);

	Graphics()->TextureClear();
	Graphics()->BlendAdditive();
	Graphics()->QuadsBegin();
	if(Style == 0)
	{
		// lightning: a jagged bolt that jumps 30 times a second, with a short branch
		const int Segs = std::clamp((int)(Len / 18.0f), 4, 60);
		const int Step = (int)(Time * 30.0f);
		vec2 Prev = From;
		for(int i = 1; i <= Segs; i++)
		{
			const float Fr = (float)i / Segs;
			const float Taper = std::sin(Fr * pi) * 0.8f + 0.2f;
			const vec2 Pt = i == Segs ? To : From + D * (Len * Fr) + N * ((Hash(Step * 53 + i, Seed) - 0.5f) * 16.0f * P * Taper);
			DrawLine(Graphics(), Prev, Pt, 7.0f * P, ColorRGBA(Glow.r, Glow.g, Glow.b, 0.25f * Fade));
			DrawLine(Graphics(), Prev, Pt, 1.7f * P, ColorRGBA(1.0f, 1.0f, 1.0f, 0.95f * Fade));
			if(i % 7 == 3)
				DrawLine(Graphics(), Pt, Pt + N * ((Hash(Step + i, Seed + 5) - 0.5f) * 40.0f * P) + D * 10.0f, 1.2f * P, ColorRGBA(Glow.r, Glow.g, Glow.b, 0.7f * Fade));
			Prev = Pt;
		}
	}
	else if(Style == 1)
	{
		// pulses run along the beam
		DrawLine(Graphics(), From, To, 5.0f * P, ColorRGBA(Glow.r, Glow.g, Glow.b, 0.18f * Fade));
		const int Count = std::clamp((int)(Len / 80.0f) + 1, 1, 12);
		for(int i = 0; i < Count; i++)
		{
			const float S = Frac(Time * 2.2f + (float)i / Count);
			const vec2 Pt = From + D * (Len * S);
			DrawDisc(Graphics(), Pt, 7.0f * P, ColorRGBA(Glow.r, Glow.g, Glow.b, 0.3f * Fade));
			DrawDisc(Graphics(), Pt, 3.0f * P, ColorRGBA(1.0f, 1.0f, 1.0f, 0.95f * Fade));
		}
	}
	else if(Style == 2)
	{
		// plasma: a wide soft halo and sparks that swim around the beam
		DrawLine(Graphics(), From, To, 16.0f * P * (0.85f + 0.15f * std::sin(Time * 9.0f)), ColorRGBA(Glow.r, Glow.g, Glow.b, 0.12f * Fade));
		DrawLine(Graphics(), From, To, 8.0f * P, ColorRGBA(Glow.r, Glow.g, Glow.b, 0.2f * Fade));
		const int Count = std::clamp((int)(Len / 14.0f), 6, 50);
		for(int i = 0; i < Count; i++)
		{
			const float Along = Frac(Hash(i, Seed) + Time * (0.2f + 0.3f * Hash(i, Seed + 1)));
			const vec2 Pt = From + D * (Len * Along) + N * (std::sin(Time * 7.0f + i * 1.9f) * 11.0f * P);
			DrawDisc(Graphics(), Pt, (1.2f + 1.6f * Hash(i, Seed + 2)) * P, ColorRGBA(1.0f, 1.0f, 1.0f, 0.85f * Fade * (0.5f + 0.5f * std::sin(Time * 12.0f + i))));
		}
	}
	else
	{
		// crystal: diamonds on the beam that breathe
		DrawLine(Graphics(), From, To, 2.0f * P, ColorRGBA(Glow.r, Glow.g, Glow.b, 0.35f * Fade));
		const float Gap = 26.0f;
		const int Count = std::clamp((int)(Len / Gap), 1, 60);
		for(int i = 0; i < Count; i++)
		{
			const vec2 C = From + D * (Gap * (i + 0.5f) + std::fmod(Time * 40.0f, Gap));
			if(distance(From, C) > Len)
				continue;
			const float Br = 0.75f + 0.25f * std::sin(Time * 5.0f + i * 0.9f);
			const vec2 A = C + D * (8.0f * P * Br), B = C - D * (8.0f * P * Br), E = C + N * (4.0f * P * Br), F = C - N * (4.0f * P * Br);
			Graphics()->SetColor(Glow.r, Glow.g, Glow.b, 0.8f * Fade);
			const IGraphics::CFreeformItem Item(A.x, A.y, E.x, E.y, F.x, F.y, B.x, B.y);
			Graphics()->QuadsDrawFreeform(&Item, 1);
			DrawLine(Graphics(), A, B, 1.2f, ColorRGBA(1.0f, 1.0f, 1.0f, 0.8f * Fade));
		}
	}
	// the hit at the end of the beam
	const float Out = 1.0f - Fade;
	DrawDisc(Graphics(), To, (9.0f + 8.0f * Out) * P, ColorRGBA(Glow.r, Glow.g, Glow.b, 0.45f * Fade));
	DrawDisc(Graphics(), To, 4.0f * P, ColorRGBA(1.0f, 1.0f, 1.0f, 0.9f * Fade));
	for(int i = 0; i < 5; i++)
	{
		const float A = Time * 3.0f + i * 2.0f * pi / 5.0f;
		DrawLine(Graphics(), To, To + vec2(std::cos(A), std::sin(A)) * (14.0f + 14.0f * Out) * P, 1.6f * P, ColorRGBA(Glow.r, Glow.g, Glow.b, 0.8f * Fade));
	}
	DrawRing(Graphics(), To, (6.0f + 28.0f * Out) * P, 1.8f * P * Fade + 0.3f, 0.0f, 2.0f * pi, ColorRGBA(Glow.r, Glow.g, Glow.b, 0.7f * Fade));
	Graphics()->QuadsEnd();
	Graphics()->BlendNormal();
}

// ---------------------------------------------------------------- everything that is drawn between hooks and tees

void CFx::RenderEffects()
{
	const int State = Client()->State();
	if(State != IClient::STATE_ONLINE && State != IClient::STATE_DEMOPLAYBACK)
	{
		m_vCombos.clear();
		m_vHits.clear();
		m_vLasers.clear();
		return;
	}
	const bool Any = g_Config.m_ClFxHearts || g_Config.m_ClFxEmbers || g_Config.m_ClFxLightning || g_Config.m_ClFxHookRgb || g_Config.m_ClFxHookPulse || g_Config.m_ClFxHammer || g_Config.m_ClFxLaser;
	if(!Any)
	{
		m_vCombos.clear();
		m_vHits.clear();
		m_vLasers.clear();
		return;
	}

	// the world is still mapped to the screen here, everything is drawn in world coordinates
	const CScreenRect View = Graphics()->GetScreen();
	const float L = View.m_TopLeft.x, T0 = View.m_TopLeft.y;
	const float W = View.m_BottomRight.x - View.m_TopLeft.x;
	const float H = View.m_BottomRight.y - View.m_TopLeft.y;
	const float Now = (float)std::fmod(Client()->LocalTime(), 10000.0);
	const float Scale = std::clamp(H / 700.0f, 1.0f, 3.0f);

	const auto &Snap = GameClient()->m_Snap;
	const bool HaveTee = Snap.m_pLocalCharacter && !Snap.m_SpecInfo.m_Active && Snap.m_LocalClientId >= 0 && Snap.m_LocalClientId < MAX_CLIENTS;
	const vec2 P = HaveTee ? GameClient()->m_aClients[Snap.m_LocalClientId].m_RenderPos : vec2(0.0f, 0.0f);

	// ---- hammer combo: a hit of my hammer on another tee ----
	if(g_Config.m_ClFxHammer && HaveTee)
	{
		const int Fire = GameClient()->m_Controls.m_aInputData[g_Config.m_ClDummy].m_Fire;
		if(Snap.m_pLocalCharacter->m_Weapon == WEAPON_HAMMER && (Fire & 1))
			m_LastSwing = Now; // pressed or held
		else if(m_FireInitialized && Fire != m_LastFire && Snap.m_pLocalCharacter->m_Weapon == WEAPON_HAMMER)
			m_LastSwing = Now;
		m_LastFire = Fire;
		m_FireInitialized = true;
		for(const CHit &Hit : m_vHits)
		{
			if(Now - m_LastSwing > 0.6f)
				continue; // someone else's hit
			int Victim = -1;
			float Best = 80.0f;
			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				if(i == Snap.m_LocalClientId || !Snap.m_aCharacters[i].m_Active)
					continue;
				const float Dist = distance(GameClient()->m_aClients[i].m_RenderPos, Hit.m_Pos);
				if(Dist < Best)
				{
					Best = Dist;
					Victim = i;
				}
			}
			if(Victim < 0)
				continue;
			CCombo *pCombo = nullptr;
			for(CCombo &C : m_vCombos)
				if(C.m_ClientId == Victim)
					pCombo = &C;
			if(pCombo == nullptr)
			{
				m_vCombos.push_back({Victim, 0, -100.0f, -1.0f, -100.0f});
				pCombo = &m_vCombos.back();
			}
			if(Hit.m_Time - pCombo->m_LastHitTime < 0.05f)
				continue; // the same hit arrived twice (predicted and from the server)
			pCombo->m_LastHitTime = Hit.m_Time;
			pCombo->m_Last = Now;
			pCombo->m_Count++;
			if(pCombo->m_Count % 100 == 0)
				pCombo->m_Milestone = Now;
		}
	}
	m_vHits.clear();
	// the combo is lost after one second without a hit
	m_vCombos.erase(std::remove_if(m_vCombos.begin(), m_vCombos.end(), [&](const CCombo &C) {
		return Now - C.m_Last > 1.0f && (C.m_Milestone < 0.0f || Now - C.m_Milestone > 1.3f);
	}),
		m_vCombos.end());

	// ---- helpers for the old effects (kept: hearts, embers, storm, hook) ----
	// ---- drawing helpers ----
	const auto Disc = [&](vec2 C, float R, ColorRGBA Col) {
		Graphics()->SetColor(Col.r, Col.g, Col.b, Col.a);
		Graphics()->DrawCircle(C.x, C.y, R, 14);
	};
	const auto Line = [&](vec2 A, vec2 B, float Width, ColorRGBA Col) {
		const vec2 D = B - A;
		const float Len = length(D);
		if(Len < 0.001f)
			return;
		const vec2 N = vec2(-D.y, D.x) / Len * Width * 0.5f;
		Graphics()->SetColor(Col.r, Col.g, Col.b, Col.a);
		const IGraphics::CFreeformItem Item(A.x - N.x, A.y - N.y, A.x + N.x, A.y + N.y, B.x - N.x, B.y - N.y, B.x + N.x, B.y + N.y);
		Graphics()->QuadsDrawFreeform(&Item, 1);
	};
	const auto Rect = [&](float X0, float Y0, float X1, float Y1, ColorRGBA C) {
		Graphics()->SetColor(C.r, C.g, C.b, C.a);
		const IGraphics::CFreeformItem Item(X0, Y0, X1, Y0, X0, Y1, X1, Y1);
		Graphics()->QuadsDrawFreeform(&Item, 1);
	};
	[[maybe_unused]] const auto Arc = [&](vec2 C, float R, float Thick, float A0, float A1, ColorRGBA Col) {
		const int Segs = std::max(2, (int)(std::fabs(A1 - A0) / (2.0f * pi) * 48.0f));
		Graphics()->SetColor(Col.r, Col.g, Col.b, Col.a);
		for(int i = 0; i < Segs; i++)
		{
			const float a0 = A0 + (A1 - A0) * i / Segs, a1 = A0 + (A1 - A0) * (i + 1) / Segs;
			const IGraphics::CFreeformItem Item(C.x + std::cos(a0) * (R - Thick), C.y + std::sin(a0) * (R - Thick), C.x + std::cos(a0) * (R + Thick), C.y + std::sin(a0) * (R + Thick),
				C.x + std::cos(a1) * (R - Thick), C.y + std::sin(a1) * (R - Thick), C.x + std::cos(a1) * (R + Thick), C.y + std::sin(a1) * (R + Thick));
			Graphics()->QuadsDrawFreeform(&Item, 1);
		}
	};
	[[maybe_unused]] const auto Tri = [&](vec2 A, vec2 B, vec2 C, ColorRGBA CA, ColorRGBA CB, ColorRGBA CC) {
		Graphics()->SetColor4(CA, CB, CC, CC);
		const IGraphics::CFreeformItem Item(A.x, A.y, B.x, B.y, C.x, C.y, C.x, C.y);
		Graphics()->QuadsDrawFreeform(&Item, 1);
	};
	const auto Heart = [&](vec2 C, float Size, ColorRGBA Col) {
		Graphics()->SetColor(Col.r, Col.g, Col.b, Col.a);
		const int N = 20;
		for(int i = 0; i < N; i++)
		{
			const float t0 = 2.0f * pi * i / N, t1 = 2.0f * pi * (i + 1) / N;
			const auto Pt = [&](float t) {
				const float s = std::sin(t);
				return C + vec2(16.0f * s * s * s, -(13.0f * std::cos(t) - 5.0f * std::cos(2 * t) - 2.0f * std::cos(3 * t) - std::cos(4 * t))) * (Size / 17.0f);
			};
			const vec2 A = Pt(t0), B = Pt(t1);
			const IGraphics::CFreeformItem Item(C.x, C.y, A.x, A.y, B.x, B.y, B.x, B.y);
			Graphics()->QuadsDrawFreeform(&Item, 1);
		}
	};
	[[maybe_unused]] const auto Star = [&](vec2 C, float R, float Angle, int Rays, float Width, ColorRGBA Col) {
		for(int i = 0; i < Rays; i++)
		{
			const float A = Angle + 2.0f * pi * i / Rays;
			Line(C - vec2(std::cos(A), std::sin(A)) * R * 0.15f, C + vec2(std::cos(A), std::sin(A)) * R, Width, Col);
		}
	};


	Graphics()->TextureClear();

	// ================= pass 1: normal blending =================
	Graphics()->BlendNormal();
	Graphics()->QuadsBegin();
	if(HaveTee && g_Config.m_ClFxHearts)
	{
		for(int i = 0; i < 5; i++)
		{
			const float Ph = Frac(Now * 0.45f + i / 5.0f);
			const vec2 C = P + vec2(std::sin(Ph * 6.0f + i * 1.7f) * 16.0f + (Hash(i, 1) - 0.5f) * 24.0f, -30.0f - 80.0f * Ph);
			Heart(C, 6.0f + 4.0f * Hash(i, 2), ColorRGBA(1.0f, 0.3f + 0.2f * Hash(i, 3), 0.5f, std::sin(Ph * pi) * 0.9f));
		}
	}
	Graphics()->QuadsEnd();

	// ================= pass 2: glowing things, additive =================
	Graphics()->BlendAdditive();
	Graphics()->QuadsBegin();

	if(g_Config.m_ClFxEmbers)
	{
		for(int i = 0; i < 60; i++)
		{
			const float Ph = Frac(Hash(i, 1) + Now * (0.05f + 0.06f * Hash(i, 2)));
			const float X = L + W * Hash(i, 3) + std::sin(Now * 1.2f + i) * W * 0.02f;
			const float Y = T0 + H * (1.05f - 1.1f * Ph);
			Disc(vec2(X, Y), (1.2f + 1.6f * Hash(i, 4)) * Scale, ColorRGBA(1.0f, 0.45f + 0.3f * Hash(i, 5), 0.1f, std::sin(Ph * pi) * 0.8f));
		}
	}
	if(g_Config.m_ClFxLightning)
	{
		const float Slot = 7.0f;
		const int SlotIndex = (int)std::floor(Now / Slot);
		const float Local = Now - SlotIndex * Slot - (1.0f + Hash(SlotIndex, 9) * 4.5f);
		if(Local > 0.0f && Local < 0.5f)
		{
			const float Pulse = std::max(std::max(0.0f, 1.0f - Local * 6.0f), std::max(0.0f, 0.7f - std::fabs(Local - 0.22f) * 6.0f));
			Rect(L, T0, L + W, T0 + H, ColorRGBA(0.6f, 0.7f, 1.0f, 0.22f * Pulse));
			vec2 Cur(L + W * (0.15f + 0.7f * Hash(SlotIndex, 1)), T0);
			for(int k = 0; k < 9; k++)
			{
				const vec2 Next = Cur + vec2((Hash(SlotIndex * 13 + k, 2) - 0.5f) * W * 0.12f, H * 0.11f);
				Line(Cur, Next, 5.0f * Scale, ColorRGBA(0.5f, 0.6f, 1.0f, 0.35f * Pulse));
				Line(Cur, Next, 1.8f * Scale, ColorRGBA(1.0f, 1.0f, 1.0f, 0.9f * Pulse));
				Cur = Next;
			}
		}
	}


	// ---- hook ----
	if(HaveTee && (g_Config.m_ClFxHookRgb || g_Config.m_ClFxHookPulse))
	{
		const CNetObj_Character &Cur = GameClient()->m_aClients[Snap.m_LocalClientId].m_RenderCur;
		if(Cur.m_HookState > 0)
		{
			const vec2 A = P, B(Cur.m_HookX, Cur.m_HookY);
			const float Len = distance(A, B);
			if(Len > 12.0f)
			{
				const vec2 D = (B - A) / Len, Nrm(-D.y, D.x);
				const bool Grabbed = Cur.m_HookState == HOOK_GRABBED;
				const float Fade = std::clamp(Len / 40.0f, 0.0f, 1.0f);
				if(g_Config.m_ClFxHookRgb)
				{
					const int N = std::clamp((int)(Len / 6.0f), 8, 90);
					vec2 Prev[3];
					for(int i = 0; i <= N; i++)
					{
						const float Fr = (float)i / N;
						const float Taper = std::sin(Fr * pi) * 0.7f + 0.3f; // calm at the ends
						const float Wave = std::sin(Fr * Len * 0.045f - Now * 12.0f) * 7.0f * Taper;
						const vec2 Base = A + D * (Len * Fr);
						const vec2 Pt[3] = {Base, Base + Nrm * Wave, Base - Nrm * Wave};
						const float Hh = Fr * 1.2f - Now * 0.9f;
						if(i > 0)
						{
							// glow, two rainbow strands and a bright core
							Line(Prev[0], Pt[0], 16.0f, Hue(Hh, 0.13f * Fade));
							Line(Prev[0], Pt[0], 7.0f, Hue(Hh + 0.1f, 0.35f * Fade));
							Line(Prev[1], Pt[1], 3.2f, Hue(Hh, 0.95f * Fade));
							Line(Prev[2], Pt[2], 3.2f, Hue(Hh + 0.5f, 0.95f * Fade));
							Line(Prev[0], Pt[0], 2.0f, ColorRGBA(1.0f, 1.0f, 1.0f, 0.85f * Fade));
						}
						for(int k = 0; k < 3; k++)
							Prev[k] = Pt[k];
					}
					// the head of the hook
					for(int k = 0; k < 6; k++)
					{
						const float Ak = Now * 4.0f + k * pi / 3.0f;
						Disc(B + vec2(std::cos(Ak), std::sin(Ak)) * 9.0f, 2.6f, Hue(k / 6.0f - Now * 0.9f, 0.9f));
					}
					Disc(B, 9.0f, Hue(-Now * 0.9f, 0.3f));
					Disc(B, 4.0f, ColorRGBA(1.0f, 1.0f, 1.0f, 0.9f));
				}
				if(g_Config.m_ClFxHookPulse)
				{
					// orbs run from the tee to the hook, bursts of rainbow rings at the place that was grabbed
					for(int i = 0; i < 6; i++)
					{
						const float S = Frac(Now * 1.6f + i / 6.0f);
						const vec2 Pt = A + D * (Len * S);
						Disc(Pt, 6.0f, Hue(S - Now * 0.7f, 0.28f * Fade));
						Disc(Pt, 3.0f, ColorRGBA(1.0f, 1.0f, 1.0f, 0.9f * Fade));
					}
					if(Grabbed)
					{
						for(int Ring = 0; Ring < 2; Ring++)
						{
							const float Ph = Frac(Now * 1.4f + Ring * 0.5f);
							Arc(B, 6.0f + 40.0f * Ph, 2.5f * (1.0f - Ph) + 0.4f, 0.0f, 2.0f * pi, Hue(Ph + Now * 0.5f, 0.85f * (1.0f - Ph)));
						}
						for(int i = 0; i < 10; i++)
						{
							const float Ph = Frac(Now * 2.0f + Hash(i, 1));
							const float Ai = 2.0f * pi * Hash(i, 2) + Now;
							Disc(B + vec2(std::cos(Ai), std::sin(Ai)) * (8.0f + 28.0f * Ph), 2.4f * (1.0f - Ph) + 0.4f, Hue(Hash(i, 3) + Now * 0.4f, 1.0f - Ph));
						}
					}
				}
			}
		}
	}

	Graphics()->QuadsEnd();
	Graphics()->BlendNormal();

	// ---- laser effects attached to the real beams ----
	if(g_Config.m_ClFxLaser)
	{
		const ColorRGBA LaserCol = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClFxLaserColor));
		for(const CLaserSeg &S : m_vLasers)
			DrawLaserFx(S.m_From, S.m_To, S.m_Fade, Now, std::clamp(g_Config.m_ClFxLaserStyle, 0, 3), g_Config.m_ClFxLaserPower / 100.0f, LaserCol, S.m_Seed);
	}
	m_vLasers.clear();

	// ---- combo numbers under the tee that was hit ----
	if(g_Config.m_ClFxHammer)
	{
		const ColorRGBA ComboCol = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClFxHammerColor));
		for(const CCombo &C : m_vCombos)
		{
			if(!Snap.m_aCharacters[C.m_ClientId].m_Active)
				continue;
			const vec2 Pos = GameClient()->m_aClients[C.m_ClientId].m_RenderPos + vec2(0.0f, 56.0f);
			DrawCombo(Pos, C.m_Count, Now - C.m_Last, std::clamp(1.0f - (Now - C.m_Last), 0.0f, 1.0f), C.m_Milestone < 0.0f ? -1.0f : Now - C.m_Milestone, std::clamp(g_Config.m_ClFxHammerStyle, 0, 3), g_Config.m_ClFxHammerPower / 100.0f, ComboCol);
		}
	}
}
