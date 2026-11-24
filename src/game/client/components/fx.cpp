#include "fx.h"

#include <base/math.h>

#include <engine/client.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/gameclient.h>
#include <game/collision.h>

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

vec2 Rot(vec2 V, float A)
{
	const float C = std::cos(A), S = std::sin(A);
	return vec2(V.x * C - V.y * S, V.x * S + V.y * C);
}

ColorRGBA WeaponColor(int Weapon)
{
	switch(Weapon)
	{
	case WEAPON_HAMMER: return ColorRGBA(1.0f, 0.8f, 0.4f, 1.0f);
	case WEAPON_GUN: return ColorRGBA(1.0f, 0.9f, 0.5f, 1.0f);
	case WEAPON_SHOTGUN: return ColorRGBA(1.0f, 0.55f, 0.25f, 1.0f);
	case WEAPON_GRENADE: return ColorRGBA(0.6f, 1.0f, 0.5f, 1.0f);
	case WEAPON_LASER: return ColorRGBA(0.4f, 0.7f, 1.0f, 1.0f);
	default: return ColorRGBA(0.8f, 0.5f, 1.0f, 1.0f);
	}
}

} // namespace

void CFx::OnReset()
{
	m_vEvents.clear();
	m_AttackInitialized = false;
}

void CFx::Spawn(int Kind, vec2 Pos, vec2 Dir, vec2 End, float Life, ColorRGBA Color)
{
	if(m_vEvents.size() > 200)
		return;
	m_vEvents.push_back({Kind, Pos, Dir, End, 0.0f, Life, m_NextSeed++, Color});
}

void CFx::OnBoom(vec2 Pos)
{
	if(g_Config.m_ClFxBoomFire)
		Spawn(K_BOOMFIRE, Pos, vec2(1.0f, 0.0f), Pos, 0.7f, ColorRGBA(1.0f, 0.6f, 0.2f, 1.0f));
	if(g_Config.m_ClFxBoomShards)
		Spawn(K_BOOMSHARDS, Pos, vec2(1.0f, 0.0f), Pos, 0.8f, ColorRGBA(1.0f, 0.85f, 0.5f, 1.0f));
}

void CFx::OnHammerHit(vec2 Pos)
{
	if(g_Config.m_ClFxHamWave)
		Spawn(K_HAMWAVE, Pos, vec2(1.0f, 0.0f), Pos, 0.45f, ColorRGBA(1.0f, 0.75f, 0.35f, 1.0f));
}

void CFx::RenderEffects()
{
	const int State = Client()->State();
	if(State != IClient::STATE_ONLINE && State != IClient::STATE_DEMOPLAYBACK)
	{
		m_vEvents.clear();
		m_AttackInitialized = false;
		return;
	}
	const bool Any = g_Config.m_ClFxHearts || g_Config.m_ClFxEmbers || g_Config.m_ClFxLightning || g_Config.m_ClFxHamArc || g_Config.m_ClFxHamWave || g_Config.m_ClFxHamSparks ||
			 g_Config.m_ClFxHamFire || g_Config.m_ClFxAimSpread || g_Config.m_ClFxAimAmmo || g_Config.m_ClFxAimLine || g_Config.m_ClFxAimPulse || g_Config.m_ClFxShotFlash ||
			 g_Config.m_ClFxShotRing || g_Config.m_ClFxShotShell || g_Config.m_ClFxShotSmoke || g_Config.m_ClFxBoomFire || g_Config.m_ClFxBoomShards || g_Config.m_ClFxPelletFan ||
			 g_Config.m_ClFxShotCone || g_Config.m_ClFxLaserFlash || g_Config.m_ClFxLaserBolt || g_Config.m_ClFxHookRgb || g_Config.m_ClFxHookPulse;
	if(!Any && m_vEvents.empty())
		return;

	// the world is still mapped to the screen here, everything is drawn in world coordinates
	const CScreenRect View = Graphics()->GetScreen();
	const float L = View.m_TopLeft.x, T0 = View.m_TopLeft.y;
	const float W = View.m_BottomRight.x - View.m_TopLeft.x;
	const float H = View.m_BottomRight.y - View.m_TopLeft.y;
	const float Dt = std::min(Client()->RenderFrameTime(), 0.1f);
	const float Now = (float)std::fmod(Client()->LocalTime(), 10000.0);
	const float Scale = std::clamp(H / 700.0f, 1.0f, 3.0f);

	const auto &Snap = GameClient()->m_Snap;
	const bool HaveTee = Snap.m_pLocalCharacter && !Snap.m_SpecInfo.m_Active && Snap.m_LocalClientId >= 0 && Snap.m_LocalClientId < MAX_CLIENTS;
	const vec2 P = HaveTee ? GameClient()->m_aClients[Snap.m_LocalClientId].m_RenderPos : vec2(0.0f, 0.0f);
	const vec2 Target = GameClient()->m_Controls.m_aTargetPos[g_Config.m_ClDummy];
	vec2 Aim = HaveTee ? Target - P : vec2(1.0f, 0.0f);
	if(length(Aim) < 1.0f)
		Aim = vec2(1.0f, 0.0f);
	const vec2 Dir = normalize(Aim);
	const int Weapon = HaveTee ? Snap.m_pLocalCharacter->m_Weapon : WEAPON_GUN;
	const ColorRGBA WCol = WeaponColor(Weapon);

	// ---- shots of the own tee ----
	if(HaveTee)
	{
		const int Tick = Snap.m_pLocalCharacter->m_AttackTick;
		if(m_AttackInitialized && Tick != m_LastAttackTick)
		{
			m_LastShotTime = Now;
			const vec2 Muzzle = P + Dir * 30.0f;
			if(Weapon == WEAPON_HAMMER)
			{
				if(g_Config.m_ClFxHamArc)
					Spawn(K_HAMARC, P, Dir, P, 0.28f, WCol);
				if(g_Config.m_ClFxHamSparks)
					Spawn(K_HAMSPARKS, P + Dir * 40.0f, Dir, P, 0.55f, WCol);
				if(g_Config.m_ClFxHamFire)
					Spawn(K_HAMFIRE, P, Dir, P, 0.45f, ColorRGBA(1.0f, 0.5f, 0.1f, 1.0f));
			}
			else if(Weapon != WEAPON_NINJA)
			{
				const float Reach = Weapon == WEAPON_LASER ? GameClient()->m_aTuning[g_Config.m_ClDummy].m_LaserReach : 700.0f;
				vec2 End = P + Dir * Reach;
				vec2 Hit;
				if(Collision()->IntersectLine(P, End, &Hit, nullptr))
					End = Hit;
				// the shot stops at the first player it hits
				for(int i = 0; i < MAX_CLIENTS; i++)
				{
					if(i == Snap.m_LocalClientId || !Snap.m_aCharacters[i].m_Active)
						continue;
					const vec2 Other = GameClient()->m_aClients[i].m_RenderPos;
					const vec2 Seg = End - P;
					const float SegLen = length(Seg);
					if(SegLen < 1.0f)
						break;
					const vec2 SegDir = Seg / SegLen;
					const float Along = dot(Other - P, SegDir);
					if(Along < 0.0f || Along > SegLen + 28.0f)
						continue;
					const float Off = length(P + SegDir * Along - Other);
					if(Off > 28.0f)
						continue;
					const float Enter = std::max(0.0f, Along - std::sqrt(28.0f * 28.0f - Off * Off));
					if(Enter < SegLen)
						End = P + SegDir * Enter;
				}
				if(g_Config.m_ClFxShotFlash)
					Spawn(K_FLASH, Muzzle, Dir, End, 0.14f, WCol);
				if(g_Config.m_ClFxShotRing)
					Spawn(K_RING, Muzzle, Dir, End, 0.32f, WCol);
				if(g_Config.m_ClFxShotShell && (Weapon == WEAPON_GUN || Weapon == WEAPON_SHOTGUN))
					Spawn(K_SHELL, P + vec2(0.0f, -4.0f), Dir, End, 0.9f, ColorRGBA(1.0f, 0.8f, 0.35f, 1.0f));
				if(g_Config.m_ClFxShotSmoke)
					Spawn(K_SMOKE, Muzzle, Dir, End, 0.9f, ColorRGBA(0.8f, 0.8f, 0.8f, 1.0f));
				if(Weapon == WEAPON_SHOTGUN)
				{
					if(g_Config.m_ClFxPelletFan)
						Spawn(K_PELLETS, Muzzle, Dir, End, 0.4f, WCol);
					if(g_Config.m_ClFxShotCone)
						Spawn(K_CONE, Muzzle, Dir, End, 0.16f, WCol);
				}
				if(Weapon == WEAPON_LASER)
				{
					if(g_Config.m_ClFxLaserFlash)
						Spawn(K_LASERFLASH, End, Dir, End, 0.45f, WCol);
					if(g_Config.m_ClFxLaserBolt)
						Spawn(K_LASERBOLT, P, Dir, End, 0.28f, WCol);
				}
			}
		}
		m_LastAttackTick = Tick;
		m_AttackInitialized = true;
	}
	else
		m_AttackInitialized = false;

	for(CEvent &E : m_vEvents)
		E.m_Age += Dt;
	m_vEvents.erase(std::remove_if(m_vEvents.begin(), m_vEvents.end(), [](const CEvent &E) { return E.m_Age >= E.m_Life; }), m_vEvents.end());

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
	const auto Arc = [&](vec2 C, float R, float Thick, float A0, float A1, ColorRGBA Col) {
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
	const auto Tri = [&](vec2 A, vec2 B, vec2 C, ColorRGBA CA, ColorRGBA CB, ColorRGBA CC) {
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
	const auto Star = [&](vec2 C, float R, float Angle, int Rays, float Width, ColorRGBA Col) {
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
	for(const CEvent &E : m_vEvents)
	{
		const float T = E.m_Age / E.m_Life;
		if(E.m_Kind == K_SMOKE)
		{
			for(int i = 0; i < 5; i++)
			{
				const vec2 C = E.m_Pos + E.m_Dir * (18.0f + 40.0f * T * (0.5f + Hash(E.m_Seed + i, 1))) + vec2((Hash(E.m_Seed + i, 2) - 0.5f) * 14.0f * T, -30.0f * T * T - 10.0f * T);
				Disc(C, (4.0f + 16.0f * T) * (0.7f + 0.6f * Hash(E.m_Seed + i, 3)), ColorRGBA(0.75f, 0.75f, 0.78f, 0.28f * (1.0f - T)));
			}
		}
		else if(E.m_Kind == K_SHELL)
		{
			const float Side = E.m_Dir.x >= 0.0f ? -1.0f : 1.0f;
			const float A = E.m_Age;
			const vec2 C = E.m_Pos + vec2(Side * 90.0f * A, -160.0f * A + 520.0f * A * A);
			const float Ang = A * 18.0f * Side;
			const vec2 D(std::cos(Ang), std::sin(Ang));
			Line(C - D * 3.0f, C + D * 3.0f, 3.0f, ColorRGBA(E.m_Color.r, E.m_Color.g, E.m_Color.b, 1.0f - T * T));
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

	// ---- shot events ----
	for(const CEvent &E : m_vEvents)
	{
		const float T = E.m_Age / E.m_Life;
		const float Fade = 1.0f - T;
		const float Ease = 1.0f - Fade * Fade;
		const ColorRGBA C = E.m_Color;
		const float Ang = std::atan2(E.m_Dir.y, E.m_Dir.x);
		switch(E.m_Kind)
		{
		case K_HAMARC:
		{
			// a crescent that is drawn quickly in the direction of the swing
			const float Sweep = std::min(1.0f, T * 3.0f);
			const float A0 = Ang - 1.2f, A1 = A0 + 2.4f * Sweep;
			const float R = 58.0f + 18.0f * T;
			Arc(E.m_Pos, R, 9.0f * Fade, A0, A1, ColorRGBA(C.r, C.g, C.b, 0.25f * Fade));
			Arc(E.m_Pos, R, 3.5f * Fade, A0, A1, ColorRGBA(1.0f, 1.0f, 1.0f, 0.9f * Fade));
			break;
		}
		case K_HAMWAVE:
			Arc(E.m_Pos, 8.0f + 70.0f * Ease, 6.0f * Fade, 0.0f, 2.0f * pi, ColorRGBA(C.r, C.g, C.b, 0.7f * Fade));
			Arc(E.m_Pos, 4.0f + 40.0f * Ease, 2.5f * Fade, 0.0f, 2.0f * pi, ColorRGBA(1.0f, 1.0f, 1.0f, 0.8f * Fade));
			Disc(E.m_Pos, 14.0f * Fade, ColorRGBA(1.0f, 0.9f, 0.6f, 0.5f * Fade));
			break;
		case K_HAMSPARKS:
			for(int i = 0; i < 16; i++)
			{
				const float Spread = (Hash(E.m_Seed + i, 1) - 0.5f) * 1.5f;
				const float Speed = 160.0f + 260.0f * Hash(E.m_Seed + i, 2);
				const vec2 V = Rot(E.m_Dir, Spread) * Speed;
				const vec2 Pt = E.m_Pos + V * E.m_Age + vec2(0.0f, 340.0f * E.m_Age * E.m_Age);
				const vec2 Back = Pt - normalize(V + vec2(0.0f, 680.0f * E.m_Age)) * (4.0f + 10.0f * Fade);
				Line(Back, Pt, 2.0f, ColorRGBA(1.0f, 0.85f - 0.4f * T, 0.3f, Fade));
			}
			break;
		case K_HAMFIRE:
			for(int i = 0; i < 20; i++)
			{
				const float Fr = i / 19.0f;
				const float A = Ang - 1.0f + 2.0f * Fr * std::min(1.0f, T * 3.0f + 0.2f);
				const float R = 52.0f + 8.0f * Hash(E.m_Seed + i, 1);
				const vec2 Pt = E.m_Pos + vec2(std::cos(A), std::sin(A)) * R + vec2(0.0f, -26.0f * T * (0.5f + Hash(E.m_Seed + i, 2)));
				Disc(Pt, (7.0f - 5.0f * T) * (0.7f + 0.6f * Hash(E.m_Seed + i, 3)), ColorRGBA(1.0f, 0.55f - 0.35f * T, 0.1f, 0.75f * Fade));
			}
			break;
		case K_FLASH:
			Disc(E.m_Pos, 13.0f * Fade + 3.0f, ColorRGBA(C.r, C.g, C.b, 0.55f * Fade));
			Star(E.m_Pos, 30.0f * Fade + 4.0f, Ang, 4, 3.0f, ColorRGBA(1.0f, 1.0f, 0.85f, 0.95f * Fade));
			Star(E.m_Pos, 18.0f * Fade, Ang + pi / 4.0f, 4, 2.0f, ColorRGBA(C.r, C.g, C.b, 0.8f * Fade));
			break;
		case K_RING:
			Arc(E.m_Pos, 4.0f + 36.0f * Ease, 2.2f * Fade + 0.4f, 0.0f, 2.0f * pi, ColorRGBA(C.r, C.g, C.b, 0.75f * Fade));
			break;
		case K_BOOMFIRE:
			Disc(E.m_Pos, 70.0f * Ease + 6.0f, ColorRGBA(1.0f, 0.45f, 0.1f, 0.35f * Fade));
			Disc(E.m_Pos, 52.0f * Ease + 4.0f, ColorRGBA(1.0f, 0.7f, 0.2f, 0.5f * Fade));
			Disc(E.m_Pos, 30.0f * Ease + 2.0f, ColorRGBA(1.0f, 0.95f, 0.7f, 0.7f * Fade * Fade));
			for(int i = 0; i < 10; i++)
			{
				const float A = 2.0f * pi * Hash(E.m_Seed + i, 1);
				const float D = 60.0f * Ease * (0.4f + 0.6f * Hash(E.m_Seed + i, 2));
				Disc(E.m_Pos + vec2(std::cos(A), std::sin(A)) * D + vec2(0.0f, -20.0f * T), 8.0f * Fade + 1.0f, ColorRGBA(1.0f, 0.5f, 0.1f, 0.6f * Fade));
			}
			break;
		case K_BOOMSHARDS:
			Arc(E.m_Pos, 12.0f + 120.0f * Ease, 4.0f * Fade + 0.5f, 0.0f, 2.0f * pi, ColorRGBA(C.r, C.g, C.b, 0.7f * Fade));
			for(int i = 0; i < 18; i++)
			{
				const float A = 2.0f * pi * (i + 0.5f * Hash(E.m_Seed + i, 1)) / 18.0f;
				const vec2 D(std::cos(A), std::sin(A));
				const float R = 170.0f * Ease * (0.55f + 0.45f * Hash(E.m_Seed + i, 2));
				Line(E.m_Pos + D * R, E.m_Pos + D * (R + 6.0f + 16.0f * Fade), 2.5f, ColorRGBA(1.0f, 0.9f, 0.6f, Fade));
			}
			break;
		case K_PELLETS:
		{
			const float MaxD = distance(E.m_Pos, E.m_End);
			for(int i = 0; i < 9; i++)
			{
				const vec2 D = Rot(E.m_Dir, (i - 4) * 0.075f + (Hash(E.m_Seed + i, 1) - 0.5f) * 0.03f);
				const float Head = std::min(MaxD, 1900.0f * E.m_Age);
				const float Tail = std::max(0.0f, Head - 70.0f);
				Line(E.m_Pos + D * Tail, E.m_Pos + D * Head, 3.0f * Fade + 0.6f, ColorRGBA(1.0f, 0.75f, 0.3f, 0.85f * Fade));
				Disc(E.m_Pos + D * Head, 2.6f, ColorRGBA(1.0f, 1.0f, 0.8f, Fade));
			}
			break;
		}
		case K_CONE:
		{
			const float Len = 110.0f * (0.4f + 0.6f * Ease);
			Tri(E.m_Pos, E.m_Pos + Rot(E.m_Dir, 0.38f) * Len, E.m_Pos + Rot(E.m_Dir, -0.38f) * Len, ColorRGBA(1.0f, 0.95f, 0.7f, 0.7f * Fade), ColorRGBA(C.r, C.g, C.b, 0.0f), ColorRGBA(C.r, C.g, C.b, 0.0f));
			break;
		}
		case K_LASERFLASH:
			Disc(E.m_End, 22.0f * Fade + 3.0f, ColorRGBA(C.r, C.g, C.b, 0.5f * Fade));
			Star(E.m_End, 38.0f * Ease + 4.0f, T * 3.0f, 6, 2.5f, ColorRGBA(0.85f, 0.95f, 1.0f, 0.9f * Fade));
			Arc(E.m_End, 6.0f + 46.0f * Ease, 2.2f * Fade + 0.3f, 0.0f, 2.0f * pi, ColorRGBA(C.r, C.g, C.b, 0.8f * Fade));
			break;
		case K_LASERBOLT:
		{
			const int N = 14;
			const vec2 D = E.m_End - E.m_Pos;
			const float Len = length(D);
			if(Len < 20.0f)
				break;
			const vec2 Nrm = vec2(-D.y, D.x) / Len;
			const int Step = (int)(E.m_Age * 40.0f);
			vec2 Prev = E.m_Pos + normalize(D) * 28.0f;
			for(int i = 1; i <= N; i++)
			{
				const float Fr = (float)i / N;
				const vec2 Pt = i == N ? E.m_End : E.m_Pos + D * Fr + Nrm * ((Hash(Step * 31 + i, E.m_Seed) - 0.5f) * 16.0f);
				Line(Prev, Pt, 6.0f * Fade, ColorRGBA(C.r, C.g, C.b, 0.3f * Fade));
				Line(Prev, Pt, 1.6f, ColorRGBA(0.95f, 0.98f, 1.0f, 0.9f * Fade));
				Prev = Pt;
			}
			break;
		}
		default:
			break;
		}
	}

	// ---- crosshair ----
	if(HaveTee)
	{
		const float Since = Now - m_LastShotTime;
		if(g_Config.m_ClFxAimSpread)
		{
			const float S = std::max(0.0f, 1.0f - Since / 0.28f);
			const float D = (9.0f + 26.0f * S * S) * Scale;
			for(int i = 0; i < 4; i++)
			{
				const vec2 V(std::cos(i * pi / 2.0f + pi / 4.0f), std::sin(i * pi / 2.0f + pi / 4.0f));
				Line(Target + V * D, Target + V * (D + 8.0f * Scale), 2.2f * Scale, ColorRGBA(1.0f, 0.85f - 0.4f * S, 0.4f, 0.9f));
			}
		}
		if(g_Config.m_ClFxAimAmmo && Weapon != WEAPON_HAMMER && Weapon != WEAPON_NINJA && Snap.m_pLocalCharacter->m_AmmoCount >= 0)
		{
			const float Fr = std::clamp(Snap.m_pLocalCharacter->m_AmmoCount / 10.0f, 0.0f, 1.0f);
			const float R = 28.0f * Scale;
			Arc(Target, R, 1.2f * Scale, 0.0f, 2.0f * pi, ColorRGBA(1.0f, 1.0f, 1.0f, 0.15f));
			if(Fr > 0.0f)
				Arc(Target, R, 2.2f * Scale, -pi / 2.0f, -pi / 2.0f + 2.0f * pi * Fr, ColorRGBA(WCol.r, WCol.g, WCol.b, 0.9f));
		}
		if(g_Config.m_ClFxAimLine)
		{
			const float Dist = distance(P, Target);
			const float Off = std::fmod(Now * 30.0f, 14.0f);
			for(float D = 44.0f + Off; D < Dist - 14.0f; D += 14.0f)
				Disc(P + Dir * D, 1.7f * Scale, ColorRGBA(WCol.r, WCol.g, WCol.b, 0.55f * (1.0f - D / std::max(1.0f, Dist) * 0.7f)));
		}
		if(g_Config.m_ClFxAimPulse)
		{
			const float Pulse = 0.5f + 0.5f * std::sin(Now * 5.0f);
			Disc(Target, (2.0f + 1.2f * Pulse) * Scale, ColorRGBA(0.4f, 0.95f, 1.0f, 0.95f));
			Disc(Target, (5.0f + 3.0f * Pulse) * Scale, ColorRGBA(0.4f, 0.95f, 1.0f, 0.2f));
			for(int i = 0; i < 4; i++)
			{
				const float A = Now * 0.9f + i * pi / 2.0f;
				const vec2 V(std::cos(A), std::sin(A)), Nrm(-V.y, V.x);
				const float D = (22.0f + 3.0f * Pulse) * Scale;
				Tri(Target + V * (D - 7.0f * Scale), Target + V * (D + 4.0f * Scale) + Nrm * 4.0f * Scale, Target + V * (D + 4.0f * Scale) - Nrm * 4.0f * Scale, ColorRGBA(0.4f, 0.95f, 1.0f, 0.95f), ColorRGBA(0.4f, 0.95f, 1.0f, 0.3f), ColorRGBA(0.4f, 0.95f, 1.0f, 0.3f));
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
}
