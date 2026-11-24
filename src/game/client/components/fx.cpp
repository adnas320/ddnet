#include "fx.h"

#include <base/math.h>

#include <engine/client.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>

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

} // namespace

void CFx::OnReset()
{
	m_vConfetti.clear();
	m_JumpedInitialized = false;
}

void CFx::OnRender()
{
	const int State = Client()->State();
	if(State != IClient::STATE_ONLINE && State != IClient::STATE_DEMOPLAYBACK)
	{
		m_vConfetti.clear();
		return;
	}
	const bool Any = g_Config.m_ClFxSatellites || g_Config.m_ClFxHalo || g_Config.m_ClFxArcs || g_Config.m_ClFxJets || g_Config.m_ClFxHearts ||
			 g_Config.m_ClFxNotes || g_Config.m_ClFxButterflies || g_Config.m_ClFxConfetti || g_Config.m_ClFxSonar || g_Config.m_ClFxShield ||
			 g_Config.m_ClFxRain || g_Config.m_ClFxFog || g_Config.m_ClFxEmbers || g_Config.m_ClFxMatrix || g_Config.m_ClFxLightning ||
			 g_Config.m_ClFxRays || g_Config.m_ClFxVignette || g_Config.m_ClFxBorder || g_Config.m_ClFxCrosshair || g_Config.m_ClFxScan;
	if(!Any && m_vConfetti.empty())
		return;

	// the world is still mapped to the screen here, everything is drawn in world coordinates
	const CScreenRect View = Graphics()->GetScreen();
	const float L = View.m_TopLeft.x, T0 = View.m_TopLeft.y;
	const float W = View.m_BottomRight.x - View.m_TopLeft.x;
	const float H = View.m_BottomRight.y - View.m_TopLeft.y;
	const float Dt = std::min(Client()->RenderFrameTime(), 0.1f);
	const float Now = (float)std::fmod(Client()->LocalTime(), 10000.0);

	const auto &Snap = GameClient()->m_Snap;
	const bool HaveTee = Snap.m_pLocalCharacter && !Snap.m_SpecInfo.m_Active && Snap.m_LocalClientId >= 0 && Snap.m_LocalClientId < MAX_CLIENTS;
	const vec2 P = HaveTee ? GameClient()->m_aClients[Snap.m_LocalClientId].m_RenderPos : vec2(0.0f, 0.0f);
	const float Scale = std::clamp(H / 700.0f, 1.0f, 3.0f); // keeps the details visible when zoomed out

	// ---- drawing helpers ----
	const auto Disc = [&](vec2 C, float R, ColorRGBA Col) {
		Graphics()->SetColor(Col.r, Col.g, Col.b, Col.a);
		Graphics()->DrawCircle(C.x, C.y, R, 12);
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
	const auto Rect = [&](float X0, float Y0, float X1, float Y1, ColorRGBA TL, ColorRGBA TR, ColorRGBA BL, ColorRGBA BR) {
		Graphics()->SetColor4(TL, TR, BL, BR);
		const IGraphics::CFreeformItem Item(X0, Y0, X1, Y0, X0, Y1, X1, Y1);
		Graphics()->QuadsDrawFreeform(&Item, 1);
	};
	// ring or part of a ring
	const auto Arc = [&](vec2 C, float R, float Thick, float A0, float A1, ColorRGBA Col) {
		const int Segs = std::max(2, (int)((A1 - A0) / (2.0f * pi) * 40.0f));
		Graphics()->SetColor(Col.r, Col.g, Col.b, Col.a);
		for(int i = 0; i < Segs; i++)
		{
			const float a0 = A0 + (A1 - A0) * i / Segs, a1 = A0 + (A1 - A0) * (i + 1) / Segs;
			const IGraphics::CFreeformItem Item(C.x + std::cos(a0) * (R - Thick), C.y + std::sin(a0) * (R - Thick), C.x + std::cos(a0) * (R + Thick), C.y + std::sin(a0) * (R + Thick),
				C.x + std::cos(a1) * (R - Thick), C.y + std::sin(a1) * (R - Thick), C.x + std::cos(a1) * (R + Thick), C.y + std::sin(a1) * (R + Thick));
			Graphics()->QuadsDrawFreeform(&Item, 1);
		}
	};
	// filled heart made of triangles
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

	// ================= confetti state (needs events) =================
	if(HaveTee)
	{
		const int Jumped = Snap.m_pLocalCharacter->m_Jumped;
		if(g_Config.m_ClFxConfetti && m_JumpedInitialized && (Jumped & ~m_LastJumped) != 0)
		{
			for(int i = 0; i < 26; i++)
			{
				const float A = -pi / 2.0f + (Hash(i, 1) - 0.5f) * 2.4f;
				const float S = 140.0f + 220.0f * Hash(i, 2);
				m_vConfetti.push_back({P + vec2(0.0f, 10.0f), vec2(std::cos(A), std::sin(A)) * S, 1.0f + Hash(i, 3) * 0.6f, Hash(i, 4) * 6.28f, (Hash(i, 5) - 0.5f) * 16.0f, Hue(Hash(i, 6))});
			}
		}
		m_LastJumped = Jumped;
		m_JumpedInitialized = true;
	}
	else
		m_JumpedInitialized = false;
	for(CConfetti &C : m_vConfetti)
	{
		C.m_Life -= Dt;
		C.m_Vel.y += 600.0f * Dt;
		C.m_Vel *= 1.0f - std::min(1.0f, 1.5f * Dt);
		C.m_Pos += C.m_Vel * Dt;
		C.m_Angle += C.m_Spin * Dt;
	}
	m_vConfetti.erase(std::remove_if(m_vConfetti.begin(), m_vConfetti.end(), [](const CConfetti &C) { return C.m_Life <= 0.0f; }), m_vConfetti.end());
	if(m_vConfetti.size() > 300)
		m_vConfetti.erase(m_vConfetti.begin(), m_vConfetti.begin() + (m_vConfetti.size() - 300));

	Graphics()->TextureClear();

	// ================= pass 1: normal blending =================
	Graphics()->BlendNormal();
	Graphics()->QuadsBegin();

	if(g_Config.m_ClFxFog)
	{
		// big soft clouds that drift over the screen
		for(int i = 0; i < 7; i++)
		{
			const float R = H * (0.28f + 0.18f * Hash(i, 1));
			const float X = L + Frac(Hash(i, 2) + Now * (0.006f + 0.006f * Hash(i, 3)) * (i % 2 ? 1.0f : -1.0f)) * (W + 2.0f * R) - R;
			const float Y = T0 + H * (0.15f + 0.7f * Hash(i, 4)) + std::sin(Now * 0.2f + i) * H * 0.03f;
			for(int Layer = 0; Layer < 3; Layer++)
				Disc(vec2(X, Y), R * (1.0f - 0.25f * Layer), ColorRGBA(0.85f, 0.9f, 1.0f, 0.035f));
		}
	}
	if(g_Config.m_ClFxRain)
	{
		const int N = 150;
		for(int i = 0; i < N; i++)
		{
			const float Speed = 1.3f + 0.7f * Hash(i, 3);
			const float Y = Frac(Hash(i, 2) + Now * Speed) * (H + 0.1f * H) - 0.05f * H;
			const float X = L + Frac(Hash(i, 1) + Now * 0.03f * Speed) * W * 1.1f - W * 0.05f + (Y / H) * H * 0.08f;
			const float Len = H * (0.025f + 0.02f * Hash(i, 4));
			Line(vec2(X, T0 + Y), vec2(X - Len * 0.2f, T0 + Y + Len), 1.2f * Scale, ColorRGBA(0.7f, 0.82f, 1.0f, 0.22f + 0.2f * Hash(i, 5)));
		}
	}
	if(g_Config.m_ClFxMatrix)
	{
		const int Cols = 26;
		const float Cell = H * 0.016f;
		for(int i = 0; i < Cols; i++)
		{
			const float X = L + (i + 0.5f) / Cols * W;
			const float Speed = 0.12f + 0.18f * Hash(i, 1);
			const float Head = Frac(Hash(i, 2) + Now * Speed) * 1.4f - 0.2f;
			for(int k = 0; k < 14; k++)
			{
				const float Y = T0 + (Head * H) - k * Cell * 1.5f;
				if(Y < T0 || Y > T0 + H)
					continue;
				const float A = (1.0f - k / 14.0f) * 0.55f;
				const bool Flicker = Hash(i * 31 + k, (int)(Now * 6.0f)) < 0.2f;
				const ColorRGBA C = k == 0 ? ColorRGBA(0.85f, 1.0f, 0.9f, 0.8f) : ColorRGBA(0.1f, 1.0f, 0.35f, Flicker ? A * 0.3f : A);
				Rect(X - Cell * 0.5f, Y - Cell * 0.7f, X + Cell * 0.5f, Y + Cell * 0.7f, C, C, C, C);
			}
		}
	}
	for(const CConfetti &C : m_vConfetti)
	{
		const vec2 D(std::cos(C.m_Angle), std::sin(C.m_Angle));
		ColorRGBA Col = C.m_Color;
		Col.a = std::min(1.0f, C.m_Life * 2.0f);
		Line(C.m_Pos - D * 4.0f, C.m_Pos + D * 4.0f, 4.0f, Col);
	}
	if(HaveTee && g_Config.m_ClFxHearts)
	{
		for(int i = 0; i < 5; i++)
		{
			const float Ph = Frac(Now * 0.45f + i / 5.0f);
			const vec2 C = P + vec2(std::sin(Ph * 6.0f + i * 1.7f) * 16.0f + (Hash(i, 1) - 0.5f) * 24.0f, -30.0f - 80.0f * Ph);
			Heart(C, 6.0f + 4.0f * Hash(i, 2), ColorRGBA(1.0f, 0.3f + 0.2f * Hash(i, 3), 0.5f, std::sin(Ph * pi) * 0.9f));
		}
	}
	if(HaveTee && g_Config.m_ClFxNotes)
	{
		for(int i = 0; i < 4; i++)
		{
			const float Ph = Frac(Now * 0.35f + i / 4.0f);
			const vec2 C = P + vec2(std::sin(Ph * 5.0f + i * 2.0f) * 26.0f + (i % 2 ? 22.0f : -22.0f), -20.0f - 85.0f * Ph);
			const ColorRGBA Col(0.75f + 0.25f * Hash(i, 1), 0.9f, 1.0f, std::sin(Ph * pi) * 0.95f);
			Disc(C, 4.0f, Col);
			Line(C + vec2(3.5f, 0.0f), C + vec2(3.5f, -16.0f), 1.8f, Col);
			Line(C + vec2(3.5f, -16.0f), C + vec2(10.0f, -11.0f), 2.2f, Col);
		}
	}
	if(HaveTee && g_Config.m_ClFxButterflies)
	{
		for(int i = 0; i < 3; i++)
		{
			const float Ph = Now * (0.8f + 0.15f * i) + i * 2.1f;
			const vec2 C = P + vec2(std::cos(Ph) * 58.0f, std::sin(Ph * 1.4f) * 34.0f - 14.0f);
			const float Flap = 2.0f + std::fabs(std::sin(Now * 14.0f + i)) * 5.0f;
			const ColorRGBA Col = Hue(0.55f + i * 0.17f, 0.9f);
			Disc(C + vec2(-Flap, -1.5f), 3.6f, Col);
			Disc(C + vec2(Flap, -1.5f), 3.6f, Col);
			Disc(C + vec2(-Flap * 0.8f, 2.5f), 2.4f, Col);
			Disc(C + vec2(Flap * 0.8f, 2.5f), 2.4f, Col);
			Line(C + vec2(0.0f, -3.0f), C + vec2(0.0f, 4.0f), 1.6f, ColorRGBA(0.1f, 0.1f, 0.15f, 0.95f));
		}
	}
	if(g_Config.m_ClFxVignette)
	{
		const float Edge = H * 0.28f;
		const ColorRGBA Dark(0.0f, 0.0f, 0.0f, 0.5f), Clear(0.0f, 0.0f, 0.0f, 0.0f);
		Rect(L, T0, L + W, T0 + Edge, Dark, Dark, Clear, Clear);
		Rect(L, T0 + H - Edge, L + W, T0 + H, Clear, Clear, Dark, Dark);
		Rect(L, T0, L + Edge, T0 + H, Dark, Clear, Dark, Clear);
		Rect(L + W - Edge, T0, L + W, T0 + H, Clear, Dark, Clear, Dark);
	}
	if(g_Config.m_ClFxScan)
	{
		const float Step = H / 220.0f;
		const float Off = std::fmod(Now * Step * 3.0f, Step * 2.0f);
		const ColorRGBA Col(0.0f, 0.0f, 0.0f, 0.12f);
		for(float Y = T0 - Step * 2.0f + Off; Y < T0 + H; Y += Step * 2.0f)
			Rect(L, Y, L + W, Y + Step, Col, Col, Col, Col);
	}
	if(g_Config.m_ClFxBorder)
	{
		const int Segs = 48;
		const float Th = H * 0.012f;
		const float Flow = Now * 0.25f;
		for(int i = 0; i < Segs; i++)
		{
			const float F0 = (float)i / Segs, F1 = (float)(i + 1) / Segs;
			const ColorRGBA C0 = Hue(F0 + Flow, 0.9f), C1 = Hue(F1 + Flow, 0.9f);
			// the border runs once around: top, right, bottom, left, each a quarter
			const float Q = 4.0f * F0;
			const int Side = std::min(3, (int)Q);
			const float S0 = Q - Side, S1 = S0 + 4.0f / Segs;
			if(Side == 0)
				Rect(L + W * S0, T0, L + W * S1, T0 + Th, C0, C1, C0, C1);
			else if(Side == 1)
				Rect(L + W - Th, T0 + H * S0, L + W, T0 + H * S1, C0, C0, C1, C1);
			else if(Side == 2)
				Rect(L + W * (1.0f - S1), T0 + H - Th, L + W * (1.0f - S0), T0 + H, C1, C0, C1, C0);
			else
				Rect(L, T0 + H * (1.0f - S1), L + Th, T0 + H * (1.0f - S0), C1, C1, C0, C0);
		}
	}
	Graphics()->QuadsEnd();

	// ================= pass 2: glowing things, additive =================
	Graphics()->BlendAdditive();
	Graphics()->QuadsBegin();

	if(g_Config.m_ClFxRays)
	{
		for(int i = 0; i < 5; i++)
		{
			const float X0 = L + W * (0.05f + 0.22f * i) + std::sin(Now * 0.15f + i) * W * 0.02f;
			const float Width = W * (0.05f + 0.03f * Hash(i, 1));
			const float A = 0.07f + 0.05f * std::sin(Now * 0.4f + i * 1.9f);
			const ColorRGBA Top(1.0f, 0.93f, 0.6f, A), Bottom(1.0f, 0.93f, 0.6f, 0.0f);
			Graphics()->SetColor4(Top, Top, Bottom, Bottom);
			const float Slant = H * 0.55f;
			const IGraphics::CFreeformItem Item(X0, T0, X0 + Width, T0, X0 - Slant, T0 + H, X0 - Slant + Width * 1.6f, T0 + H);
			Graphics()->QuadsDrawFreeform(&Item, 1);
		}
	}
	if(g_Config.m_ClFxEmbers)
	{
		for(int i = 0; i < 60; i++)
		{
			const float Ph = Frac(Hash(i, 1) + Now * (0.05f + 0.06f * Hash(i, 2)));
			const float X = L + W * Hash(i, 3) + std::sin(Now * 1.2f + i) * W * 0.02f;
			const float Y = T0 + H * (1.05f - 1.1f * Ph);
			const float A = std::sin(Ph * pi);
			Disc(vec2(X, Y), (1.2f + 1.6f * Hash(i, 4)) * Scale, ColorRGBA(1.0f, 0.45f + 0.3f * Hash(i, 5), 0.1f, A * 0.8f));
		}
	}
	if(g_Config.m_ClFxLightning)
	{
		const float Slot = 7.0f;
		const int SlotIndex = (int)std::floor(Now / Slot);
		const float Local = Now - SlotIndex * Slot - (1.0f + Hash(SlotIndex, 9) * 4.5f);
		if(Local > 0.0f && Local < 0.5f)
		{
			// two quick pulses
			const float Pulse = std::max(std::max(0.0f, 1.0f - Local * 6.0f), std::max(0.0f, 0.7f - std::fabs(Local - 0.22f) * 6.0f));
			Rect(L, T0, L + W, T0 + H, ColorRGBA(0.6f, 0.7f, 1.0f, 0.22f * Pulse), ColorRGBA(0.6f, 0.7f, 1.0f, 0.22f * Pulse), ColorRGBA(0.6f, 0.7f, 1.0f, 0.22f * Pulse), ColorRGBA(0.6f, 0.7f, 1.0f, 0.22f * Pulse));
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
	if(g_Config.m_ClFxCrosshair && HaveTee)
	{
		const vec2 Target = GameClient()->m_Controls.m_aTargetPos[g_Config.m_ClDummy];
		const float R = 20.0f * Scale * (1.0f + 0.08f * std::sin(Now * 5.0f));
		for(int i = 0; i < 4; i++)
		{
			const float A0 = Now * 1.6f + i * pi / 2.0f;
			Arc(Target, R, 1.2f * Scale, A0, A0 + 0.9f, ColorRGBA(1.0f, 1.0f, 1.0f, 0.75f));
		}
		Disc(Target, 1.6f * Scale, ColorRGBA(1.0f, 0.4f, 0.4f, 0.9f));
	}

	if(HaveTee)
	{
		if(g_Config.m_ClFxSatellites)
		{
			for(int i = 0; i < 3; i++)
			{
				const ColorRGBA Col = Hue(0.5f + i * 0.2f);
				for(int Tail = 5; Tail >= 0; Tail--)
				{
					const float A = Now * 2.4f + i * 2.0944f - Tail * 0.12f;
					const vec2 C = P + vec2(std::cos(A) * 54.0f, std::sin(A) * 22.0f + std::sin(A * 0.5f) * 6.0f);
					Disc(C, (5.5f - Tail * 0.7f), ColorRGBA(Col.r, Col.g, Col.b, 0.9f - Tail * 0.15f));
				}
			}
		}
		if(g_Config.m_ClFxHalo)
		{
			const vec2 C = P + vec2(0.0f, -46.0f + std::sin(Now * 2.0f) * 2.0f);
			for(int i = 0; i < 2; i++)
			{
				const float Th = i ? 1.6f : 4.0f;
				Graphics()->SetColor(1.0f, 0.85f, 0.35f, i ? 0.95f : 0.25f);
				for(int Seg = 0; Seg < 32; Seg++)
				{
					const float a0 = 2.0f * pi * Seg / 32.0f, a1 = 2.0f * pi * (Seg + 1) / 32.0f;
					const auto Pt = [&](float a, float Off) { return C + vec2(std::cos(a) * (20.0f + Off), std::sin(a) * (6.0f + Off * 0.3f)); };
					const vec2 A0 = Pt(a0, -Th * 0.5f), A1 = Pt(a0, Th * 0.5f), B0 = Pt(a1, -Th * 0.5f), B1 = Pt(a1, Th * 0.5f);
					const IGraphics::CFreeformItem Item(A0.x, A0.y, A1.x, A1.y, B0.x, B0.y, B1.x, B1.y);
					Graphics()->QuadsDrawFreeform(&Item, 1);
				}
			}
		}
		if(g_Config.m_ClFxArcs)
		{
			const int Step = (int)(Now * 14.0f);
			for(int b = 0; b < 3; b++)
			{
				const float A = Hash(Step * 3 + b, 1) * 2.0f * pi;
				vec2 Cur = P + vec2(std::cos(A), std::sin(A)) * 24.0f;
				for(int k = 0; k < 5; k++)
				{
					const float Aj = A + (Hash(Step * 17 + b * 5 + k, 2) - 0.5f) * 0.9f;
					const vec2 Next = P + vec2(std::cos(Aj), std::sin(Aj)) * (24.0f + 9.0f * (k + 1));
					Line(Cur, Next, 3.5f, ColorRGBA(0.3f, 0.5f, 1.0f, 0.35f));
					Line(Cur, Next, 1.3f, ColorRGBA(0.9f, 0.97f, 1.0f, 0.95f));
					Cur = Next;
				}
			}
		}
		if(g_Config.m_ClFxJets)
		{
			for(int i = 0; i < 26; i++)
			{
				const float Ph = Frac(Now * 1.8f + Hash(i, 1));
				const float Side = (i % 2 ? 1.0f : -1.0f);
				const vec2 C = P + vec2(Side * (8.0f + 4.0f * Hash(i, 2)) + (Hash(i, 3) - 0.5f) * 8.0f * Ph, 26.0f + 46.0f * Ph);
				const float A = (1.0f - Ph);
				Disc(C, (5.0f - 3.5f * Ph) * (0.8f + 0.4f * Hash(i, 4)), ColorRGBA(1.0f, 0.75f - 0.55f * Ph, 0.15f, A * 0.8f));
			}
		}
		if(g_Config.m_ClFxSonar)
		{
			for(int Pulse = 0; Pulse < 2; Pulse++)
			{
				const float Ph = Frac(Now * 0.6f + Pulse * 0.5f);
				const float R = 24.0f + 80.0f * Ph;
				for(int D = 0; D < 14; D++)
				{
					const float A0 = Now * 0.8f + D * (2.0f * pi / 14.0f);
					Arc(P, R, 1.3f, A0, A0 + 0.28f, ColorRGBA(0.3f, 1.0f, 0.6f, (1.0f - Ph) * 0.7f));
				}
			}
		}
		if(g_Config.m_ClFxShield)
		{
			Disc(P, 44.0f, ColorRGBA(0.3f, 0.7f, 1.0f, 0.06f));
			for(int i = 0; i < 6; i++)
			{
				const float A0 = -Now * 1.1f + i * pi / 3.0f;
				Arc(P, 44.0f, 1.6f, A0, A0 + 0.75f, ColorRGBA(0.5f, 0.85f, 1.0f, 0.55f + 0.3f * std::sin(Now * 3.0f + i)));
			}
		}
	}
	Graphics()->QuadsEnd();
	Graphics()->BlendNormal();
}
