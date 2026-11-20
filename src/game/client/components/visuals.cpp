#include "visuals.h"

#include <base/math.h>

#include <engine/client.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <game/client/gameclient.h>

#include <algorithm>
#include <cmath>

namespace {

// positions and sizes are in units of the tee size, relative to the center of the tee body

// hat: the picture shows a hat whose pompom hangs on the left side, this is used when looking right
constexpr float HAT_CENTER_X = -0.14f;
constexpr float HAT_CENTER_Y = -0.42f;
constexpr float HAT_WIDTH = 0.92f;
constexpr float HAT_ASPECT = 166.0f / 239.0f; // height / width of winter_hat.png
constexpr float HAT_PIVOT_Y = -0.22f; // the hat swings around this point on the head

// the snow field is one tile in world coordinates that repeats over the whole map
constexpr float SNOW_TILE_WIDTH = 1400.0f;
constexpr float SNOW_TILE_HEIGHT = 900.0f;

constexpr float PETAL_TILE_WIDTH = 1300.0f;
constexpr float PETAL_TILE_HEIGHT = 900.0f;
constexpr float BUBBLE_TILE_WIDTH = 1200.0f;
constexpr float BUBBLE_TILE_HEIGHT = 900.0f;
constexpr float STAR_TILE_WIDTH = 1500.0f;
constexpr float STAR_TILE_HEIGHT = 1000.0f;
constexpr float FIREFLY_TILE_WIDTH = 1100.0f;
constexpr float FIREFLY_TILE_HEIGHT = 800.0f;

// mixes a color with white
ColorRGBA Lighten(ColorRGBA Color, float Amount)
{
	return ColorRGBA(Color.r + (1.0f - Color.r) * Amount, Color.g + (1.0f - Color.g) * Amount, Color.b + (1.0f - Color.b) * Amount, Color.a);
}

} // namespace

void CVisuals::OnInit()
{
	m_HatTexture = Graphics()->LoadTexture("visuals/winter_hat.png", IStorage::TYPE_ALL);
	m_GlowTexture = Graphics()->LoadTexture("visuals/glow.png", IStorage::TYPE_ALL);
	m_StarTexture = Graphics()->LoadTexture("visuals/star.png", IStorage::TYPE_ALL);
	m_CircleTexture = Graphics()->LoadTexture("visuals/circle.png", IStorage::TYPE_ALL);
	m_SoulTexture = Graphics()->LoadTexture("visuals/soul.png", IStorage::TYPE_ALL);
}

void CVisuals::OnReset()
{
	m_vBurstFlakes.clear();
	m_vFlashes.clear();
	m_vSparkles.clear();
	m_vRings.clear();
	m_vGlows.clear();
	m_vSouls.clear();
	m_vBlackHoles.clear();
	m_vMeteors.clear();
	m_vEchoes.clear();
	m_AutoSkinTimer = 0.0f;
	m_vBalloons.clear();
	m_PetCelebrate = 0.0f;
	m_PetInitialized = false;
	m_JumpedInitialized = false;
	m_FlakesInitialized = false;
	m_HatInitialized = false;
	m_WasInAir = false;
	m_FallSpeed = 0.0f;
	for(CRecentLaser &Laser : m_aRecentLasers)
		Laser = CRecentLaser();
}

ColorRGBA CVisuals::Rainbow(float Offset) const
{
	const float Hue = std::fmod(Client()->LocalTime() * 0.35f + Offset, 1.0f);
	return color_cast<ColorRGBA>(ColorHSLA(Hue, 1.0f, 0.5f));
}

void CVisuals::DrawSprite(IGraphics::CTextureHandle Texture, vec2 Center, float Width, float Height, float Rotation, bool Flip, ColorRGBA Color) const
{
	if(!Texture.IsValid())
		return;
	Graphics()->TextureSet(Texture);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(Color);
	Graphics()->QuadsSetRotation(Rotation);
	if(Flip)
		Graphics()->QuadsSetSubset(1.0f, 0.0f, 0.0f, 1.0f);
	IGraphics::CQuadItem Item(Center.x, Center.y, Width, Height);
	Graphics()->QuadsDraw(&Item, 1);
	Graphics()->QuadsEnd();
	Graphics()->QuadsSetRotation(0.0f);
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
}

void CVisuals::SpawnGlow(vec2 Pos, vec2 Vel, float Life, float Size, float Grow, float AlphaScale, ColorRGBA Color, float Gravity)
{
	if(m_vGlows.size() >= MAX_GLOWS)
		return;
	CSparkle Glow;
	Glow.m_Pos = Pos;
	Glow.m_Vel = Vel;
	Glow.m_MaxLife = Life;
	Glow.m_Life = Life;
	Glow.m_Size = Size;
	Glow.m_Grow = Grow;
	Glow.m_AlphaScale = AlphaScale;
	Glow.m_Gravity = Gravity;
	Glow.m_Color = Color;
	m_vGlows.push_back(Glow);
}

void CVisuals::SpawnStar(vec2 Pos, vec2 Vel, float Life, float Size, float AlphaScale, ColorRGBA Color, float Gravity)
{
	if(m_vGlows.size() >= MAX_GLOWS)
		return;
	SpawnGlow(Pos, Vel, Life, Size, -0.5f, AlphaScale, Color, Gravity);
	CSparkle &Star = m_vGlows.back();
	Star.m_Star = true;
	Star.m_Angle = random_float(0.0f, 2.0f * pi);
	Star.m_Spin = random_float(-3.0f, 3.0f);
}

// a texture drawn as a flat tilted disc, rotating around its own axis
void CVisuals::DrawFlatCircle(IGraphics::CTextureHandle Texture, vec2 Center, float Radius, float Angle, float Squash, ColorRGBA Color) const
{
	if(!Texture.IsValid())
		return;
	const float SinAngle = std::sin(Angle);
	const float CosAngle = std::cos(Angle);
	const auto Point = [&](float X, float Y) {
		return Center + vec2((X * CosAngle - Y * SinAngle) * Radius, (X * SinAngle + Y * CosAngle) * Radius * Squash);
	};
	Graphics()->TextureSet(Texture);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(Color);
	Graphics()->QuadsSetSubsetFree(0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
	const IGraphics::CFreeformItem Item(Point(-1.0f, -1.0f), Point(1.0f, -1.0f), Point(-1.0f, 1.0f), Point(1.0f, 1.0f));
	Graphics()->QuadsDrawFreeform(&Item, 1);
	Graphics()->QuadsEnd();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
}

// a sprite cut into thin horizontal slices that sway sideways: the lower the slice the stronger the sway.
// Smooth waving of soft bodies (jellyfish tentacles, ghost hem) without any visible parts.
void CVisuals::DrawWavy(IGraphics::CTextureHandle Texture, vec2 Center, float Width, float Height, float Rotation, bool Flip, ColorRGBA Color, float WaveAmp, float WaveSpeed, float Phase) const
{
	if(!Texture.IsValid())
		return;
	constexpr int SLICES = 16;
	const float SinRot = std::sin(Rotation);
	const float CosRot = std::cos(Rotation);
	const auto Point = [&](float X, float Y) {
		return Center + vec2(X * CosRot - Y * SinRot, X * SinRot + Y * CosRot);
	};
	const float UL = Flip ? 1.0f : 0.0f;
	const float UR = Flip ? 0.0f : 1.0f;
	Graphics()->TextureSet(Texture);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(Color);
	for(int i = 0; i < SLICES; i++)
	{
		const float V0 = (float)i / SLICES;
		const float V1 = (float)(i + 1) / SLICES;
		const float Y0 = (V0 - 0.5f) * Height;
		const float Y1 = (V1 - 0.5f) * Height;
		const float O0 = std::sin(Phase + V0 * WaveSpeed) * WaveAmp * Width * V0 * V0;
		const float O1 = std::sin(Phase + V1 * WaveSpeed) * WaveAmp * Width * V1 * V1;
		Graphics()->QuadsSetSubsetFree(UL, V0, UR, V0, UL, V1, UR, V1);
		const IGraphics::CFreeformItem Item(Point(-Width / 2.0f + O0, Y0), Point(Width / 2.0f + O0, Y0), Point(-Width / 2.0f + O1, Y1), Point(Width / 2.0f + O1, Y1));
		Graphics()->QuadsDrawFreeform(&Item, 1);
	}
	Graphics()->QuadsEnd();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
}

// moves and draws glowing particles, the caller has to enable additive blending
void CVisuals::StepGlows(std::vector<CSparkle> &vGlows, float DeltaTime)
{
	for(size_t i = 0; i < vGlows.size();)
	{
		CSparkle &Glow = vGlows[i];
		Glow.m_Life -= DeltaTime;
		if(Glow.m_Life <= 0.0f)
		{
			Glow = vGlows.back();
			vGlows.pop_back();
			continue;
		}
		Glow.m_Vel.y += Glow.m_Gravity * DeltaTime;
		Glow.m_Pos += Glow.m_Vel * DeltaTime;
		const float Remaining = std::clamp(Glow.m_Life / Glow.m_MaxLife, 0.0f, 1.0f);
		ColorRGBA Color = Glow.m_Color;
		Color.a = Remaining * Glow.m_AlphaScale;
		const float Size = Glow.m_Size * std::max(0.2f, 1.0f + Glow.m_Grow * (1.0f - Remaining));
		if(Glow.m_Star)
		{
			Glow.m_Angle += Glow.m_Spin * DeltaTime;
			DrawSprite(m_StarTexture, Glow.m_Pos, Size * 1.7f, Size * 1.7f, Glow.m_Angle, false, Color);
		}
		else
			DrawSprite(m_GlowTexture, Glow.m_Pos, Size, Size, 0.0f, false, Color);
		i++;
	}
}

void CVisuals::GetEcho(int Index, vec2 &Pos, float &Alpha) const
{
	const CEcho &Echo = m_vEchoes[Index];
	Pos = Echo.m_Pos;
	Alpha = std::clamp(Echo.m_Life / Echo.m_MaxLife, 0.0f, 1.0f) * 0.4f;
}

void CVisuals::RenderCape(vec2 Position, vec2 BodyPos, float TeeSize, float Alpha, vec2 LookDir, vec2 Vel, bool InAir)
{
	const int Type = g_Config.m_ClVisualsCape;
	if(Type == 0)
		return;
	const float Time = Client()->LocalTime();
	const float DeltaTime = std::min(Client()->RenderFrameTime(), 0.05f);

	// the cape trails behind the movement and flutters up when falling
	const float TargetSway = std::clamp(-Vel.x * 3.0f, -0.55f * TeeSize, 0.55f * TeeSize);
	m_CapeSway += (TargetSway - m_CapeSway) * std::min(1.0f, DeltaTime * 7.0f);
	const float TargetLift = InAir ? std::clamp(Vel.y * 0.035f, -0.2f, 0.35f) : 0.0f;
	m_CapeLift += (TargetLift - m_CapeLift) * std::min(1.0f, DeltaTime * 6.0f);

	const bool Dark = Type == 3;
	const float TopY = BodyPos.y - 0.1f * TeeSize;
	const float FloorY = Position.y + 15.0f - (Dark ? 0.07f * TeeSize : 0.0f);
	const float Height = (FloorY - TopY) * (1.0f - m_CapeLift);
	const float CenterX = BodyPos.x - LookDir.x * 0.04f * TeeSize;
	const float TopHalf = 0.27f * TeeSize;
	const float HemHalf = 0.5f * TeeSize;
	constexpr int SLICES = 24;
	constexpr int HEM_SLICES = 2;

	const auto Mix = [](const ColorRGBA &A, const ColorRGBA &B, float T) {
		return ColorRGBA(A.r + (B.r - A.r) * T, A.g + (B.g - A.g) * T, A.b + (B.b - A.b) * T, 1.0f);
	};
	const auto Brighten = [](ColorRGBA Color, float Amount) {
		return ColorRGBA(std::clamp(Color.r + Amount, 0.0f, 1.0f), std::clamp(Color.g + Amount, 0.0f, 1.0f), std::clamp(Color.b + Amount, 0.0f, 1.0f), Color.a);
	};
	// color of the cloth at a place: T is 0 at the shoulders and 1 at the hem, Side is -1, 0 or 1 across
	const auto Cloth = [&](float T, float Side) {
		ColorRGBA Color;
		if(Type == 1)
			Color = Mix(ColorRGBA(0.82f, 0.09f, 0.18f, 1.0f), ColorRGBA(0.42f, 0.02f, 0.11f, 1.0f), T);
		else if(Type == 2)
			Color = color_cast<ColorRGBA>(ColorHSLA(std::fmod(Time * 0.3f + T * 0.55f + Side * 0.08f + 1.0f, 1.0f), 1.0f, 0.55f));
		else
			Color = Mix(ColorRGBA(0.17f, 0.17f, 0.22f, 1.0f), ColorRGBA(0.05f, 0.05f, 0.08f, 1.0f), T);
		// folds: the middle catches more light than the sides
		Color = Brighten(Color, Side == 0.0f ? 0.07f : -0.05f);
		Color.a = Alpha;
		return Color;
	};
	ColorRGBA Trim;
	if(Type == 1)
		Trim = ColorRGBA(1.0f, 0.82f, 0.25f, Alpha);
	else if(Type == 2)
		Trim = ColorRGBA(1.0f, 1.0f, 1.0f, Alpha);
	else
		Trim = ColorRGBA(0.45f, 0.42f, 0.6f, Alpha);

	struct SPoint
	{
		float m_Left, m_Mid, m_Right, m_Y;
	};
	SPoint aPoints[SLICES + 1];
	for(int i = 0; i <= SLICES; i++)
	{
		const float T = (float)i / SLICES;
		const float Half = TopHalf + (HemHalf - TopHalf) * std::pow(T, 0.8f) + std::fabs(m_CapeSway) * 0.25f * T;
		const float Wave = m_CapeSway * std::pow(T, 1.6f) + std::sin(Time * 3.2f + T * 5.0f) * 0.035f * TeeSize * T * (1.0f + std::fabs(m_CapeSway) / TeeSize * 2.0f);
		const float X = CenterX + Wave;
		aPoints[i] = {X - Half, X, X + Half, TopY + Height * T};
	}

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	const auto DrawQuad = [&](vec2 TL, vec2 TR, vec2 BL, vec2 BR, ColorRGBA CTL, ColorRGBA CTR, ColorRGBA CBL, ColorRGBA CBR) {
		Graphics()->SetColor4(CTL, CTR, CBL, CBR);
		const IGraphics::CFreeformItem Item(TL, TR, BL, BR);
		Graphics()->QuadsDrawFreeform(&Item, 1);
	};
	const float EdgeWidth = 0.05f * TeeSize;
	for(int i = 0; i < SLICES; i++)
	{
		const float T0 = (float)i / SLICES;
		const float T1 = (float)(i + 1) / SLICES;
		const SPoint &A = aPoints[i];
		const SPoint &B = aPoints[i + 1];
		const bool Hem = !Dark && i >= SLICES - HEM_SLICES;
		const ColorRGBA L0 = Hem ? Trim : Cloth(T0, -1.0f), M0 = Hem ? Trim : Cloth(T0, 0.0f), R0 = Hem ? Trim : Cloth(T0, 1.0f);
		const ColorRGBA L1 = Hem ? Trim : Cloth(T1, -1.0f), M1 = Hem ? Trim : Cloth(T1, 0.0f), R1 = Hem ? Trim : Cloth(T1, 1.0f);
		DrawQuad(vec2(A.m_Left, A.m_Y), vec2(A.m_Mid, A.m_Y), vec2(B.m_Left, B.m_Y), vec2(B.m_Mid, B.m_Y), L0, M0, L1, M1);
		DrawQuad(vec2(A.m_Mid, A.m_Y), vec2(A.m_Right, A.m_Y), vec2(B.m_Mid, B.m_Y), vec2(B.m_Right, B.m_Y), M0, R0, M1, R1);
		// trim along both edges
		DrawQuad(vec2(A.m_Left, A.m_Y), vec2(A.m_Left + EdgeWidth, A.m_Y), vec2(B.m_Left, B.m_Y), vec2(B.m_Left + EdgeWidth, B.m_Y), Trim, Trim, Trim, Trim);
		DrawQuad(vec2(A.m_Right - EdgeWidth, A.m_Y), vec2(A.m_Right, A.m_Y), vec2(B.m_Right - EdgeWidth, B.m_Y), vec2(B.m_Right, B.m_Y), Trim, Trim, Trim, Trim);
	}
	if(Dark)
	{
		// ragged hem with pointed ends
		const SPoint &End = aPoints[SLICES];
		constexpr int TEETH = 5;
		const float ToothWidth = (End.m_Right - End.m_Left) / TEETH;
		const ColorRGBA Hem = Cloth(1.0f, 0.0f);
		for(int Tooth = 0; Tooth < TEETH; Tooth++)
		{
			const vec2 BaseLeft(End.m_Left + Tooth * ToothWidth, End.m_Y);
			const vec2 BaseRight(End.m_Left + (Tooth + 1) * ToothWidth, End.m_Y);
			const vec2 Tip((BaseLeft.x + BaseRight.x) / 2.0f + std::sin(Time * 4.0f + Tooth) * 0.02f * TeeSize, End.m_Y + 0.12f * TeeSize);
			DrawQuad(BaseLeft, BaseRight, Tip, Tip, Hem, Hem, Hem, Hem);
		}
	}

	// collar at the neck
	const SPoint &Neck = aPoints[0];
	if(Type == 1)
	{
		// ermine: white fur with black tails
		const float CollarY = TopY + 0.03f * TeeSize;
		const float CollarHalf = (Neck.m_Right - Neck.m_Left) / 2.0f + 0.07f * TeeSize;
		const ColorRGBA Fur(0.98f, 0.97f, 0.95f, Alpha);
		DrawQuad(vec2(Neck.m_Mid - CollarHalf, CollarY - 0.07f * TeeSize), vec2(Neck.m_Mid + CollarHalf, CollarY - 0.07f * TeeSize), vec2(Neck.m_Mid - CollarHalf, CollarY + 0.07f * TeeSize), vec2(Neck.m_Mid + CollarHalf, CollarY + 0.07f * TeeSize), Fur, Fur, Fur, Fur);
		Graphics()->SetColor(Fur);
		Graphics()->DrawCircle(Neck.m_Mid - CollarHalf, CollarY, 0.07f * TeeSize, 12);
		Graphics()->DrawCircle(Neck.m_Mid + CollarHalf, CollarY, 0.07f * TeeSize, 12);
		Graphics()->SetColor(0.08f, 0.08f, 0.1f, Alpha);
		for(int Spot = -2; Spot <= 2; Spot++)
			Graphics()->DrawCircle(Neck.m_Mid + Spot * CollarHalf * 0.4f, CollarY + (Spot % 2 == 0 ? 0.0f : 0.02f) * TeeSize, 0.022f * TeeSize, 8);
	}
	// clasp
	Graphics()->SetColor(Type == 1 ? ColorRGBA(1.0f, 0.82f, 0.25f, Alpha) : ColorRGBA(0.9f, 0.9f, 1.0f, Alpha));
	Graphics()->DrawCircle(Neck.m_Mid, TopY + 0.13f * TeeSize, 0.045f * TeeSize, 12);
	Graphics()->QuadsEnd();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
}

void CVisuals::RenderBehindTee(vec2 BodyPos, float TeeSize, float Alpha, ColorRGBA TeeColor)
{
	const float Time = Client()->LocalTime();
	const float DeltaTime = std::min(Client()->RenderFrameTime(), 0.05f);
	const ColorRGBA Light = Lighten(TeeColor, 0.3f);

	// echoes get older (they are drawn by the player renderer before the tee)
	if(!g_Config.m_ClVisualsEcho)
		m_vEchoes.clear();
	for(size_t i = 0; i < m_vEchoes.size();)
	{
		m_vEchoes[i].m_Life -= DeltaTime;
		if(m_vEchoes[i].m_Life <= 0.0f)
		{
			m_vEchoes[i] = m_vEchoes.back();
			m_vEchoes.pop_back();
			continue;
		}
		i++;
	}

	Graphics()->BlendAdditive();

	// soft glow around the tee in the color of its skin that slowly pulses
	if(g_Config.m_ClVisualsAura)
	{
		const float Pulse = 0.85f + 0.15f * std::sin(Time * 2.2f);
		DrawSprite(m_GlowTexture, BodyPos, 2.8f * TeeSize * Pulse, 2.8f * TeeSize * Pulse, 0.0f, false, ColorRGBA(Light.r, Light.g, Light.b, 0.6f * Alpha));
	}

	// sparkle trail and dust puffs, drawn behind the tee
	StepGlows(m_vSparkles, DeltaTime);
	Graphics()->BlendNormal();
}

void CVisuals::UpdateHat(vec2 LookDir, vec2 Vel, float DeltaTime)
{
	if(!m_HatInitialized)
	{
		m_HatInitialized = true;
		m_SmoothVel = Vel;
		m_LastVel = Vel;
		m_HatFlip = m_HatFlipTarget = LookDir.x < 0.0f ? -1.0f : 1.0f;
		m_HatAngle = m_HatAngleSpeed = m_HatBob = m_HatBobSpeed = 0.0f;
	}
	if(DeltaTime <= 0.0f)
		return;

	// the hat turns to the side the tee is looking at (with some hysteresis around the middle)
	if(LookDir.x > 0.2f)
		m_HatFlipTarget = 1.0f;
	else if(LookDir.x < -0.2f)
		m_HatFlipTarget = -1.0f;
	m_HatFlip += (m_HatFlipTarget - m_HatFlip) * (1.0f - std::exp(-DeltaTime * 9.0f));

	// the velocity is filtered so the hat does not shake, the acceleration makes it swing by inertia
	m_SmoothVel += (Vel - m_SmoothVel) * (1.0f - std::exp(-DeltaTime * 14.0f));
	vec2 Accel = (m_SmoothVel - m_LastVel) / DeltaTime;
	m_LastVel = m_SmoothVel;
	Accel.x = std::clamp(Accel.x, -150.0f, 150.0f);
	Accel.y = std::clamp(Accel.y, -150.0f, 150.0f);

	const float TargetAngle = std::clamp(-m_SmoothVel.x * 0.03f, -0.45f, 0.45f) + std::sin(Client()->LocalTime() * 1.6f) * 0.02f;
	const float TargetBob = std::clamp(-m_SmoothVel.y * 0.005f, -0.05f, 0.05f);

	// soft damped springs, integrated in small steps so they stay stable at any frame rate
	const int Steps = std::clamp((int)std::ceil(DeltaTime / (1.0f / 120.0f)), 1, 12);
	const float Step = DeltaTime / Steps;
	for(int i = 0; i < Steps; i++)
	{
		const float AngleAccel = 60.0f * (TargetAngle - m_HatAngle) - 9.0f * m_HatAngleSpeed - Accel.x * 0.03f;
		m_HatAngleSpeed += AngleAccel * Step;
		m_HatAngle += m_HatAngleSpeed * Step;
		const float BobAccel = 80.0f * (TargetBob - m_HatBob) - 10.0f * m_HatBobSpeed - Accel.y * 0.0006f;
		m_HatBobSpeed += BobAccel * Step;
		m_HatBob += m_HatBobSpeed * Step;
	}
	m_HatAngle = std::clamp(m_HatAngle, -0.7f, 0.7f);
	m_HatBob = std::clamp(m_HatBob, -0.08f, 0.08f);
}

void CVisuals::RenderTeeAccessories(vec2 Position, vec2 BodyPos, float TeeSize, float Alpha, vec2 LookDir, vec2 Vel, bool InAir, int Jumped, ColorRGBA TeeColor)
{
	const float Time = Client()->LocalTime();
	const float DeltaTime = std::min(Client()->RenderFrameTime(), 0.05f);
	const vec2 Feet = Position + vec2(0.0f, 14.0f);
	const ColorRGBA RingColor = Lighten(TeeColor, 0.5f);

	// dark cape: a mask over the eyes (the cape itself is drawn behind the tee)
	if(g_Config.m_ClVisualsCape == 3)
	{
		const float EyeSeparation = (0.075f - 0.010f * std::fabs(LookDir.x)) * TeeSize;
		const vec2 EyeCenter = BodyPos + vec2(LookDir.x * 0.125f, -0.05f + LookDir.y * 0.10f) * TeeSize;
		const float Radius = 0.15f * TeeSize;
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(0.07f, 0.07f, 0.1f, Alpha);
		Graphics()->DrawCircle(EyeCenter.x - EyeSeparation * 1.5f, EyeCenter.y, Radius, 16);
		Graphics()->DrawCircle(EyeCenter.x + EyeSeparation * 1.5f, EyeCenter.y, Radius, 16);
		const ColorRGBA Mask(0.07f, 0.07f, 0.1f, Alpha);
		const IGraphics::CFreeformItem Band(
			vec2(EyeCenter.x - EyeSeparation * 1.5f, EyeCenter.y - Radius * 0.85f), vec2(EyeCenter.x + EyeSeparation * 1.5f, EyeCenter.y - Radius * 0.85f),
			vec2(EyeCenter.x - EyeSeparation * 1.5f, EyeCenter.y + Radius * 0.85f), vec2(EyeCenter.x + EyeSeparation * 1.5f, EyeCenter.y + Radius * 0.85f));
		Graphics()->SetColor(Mask);
		Graphics()->QuadsDrawFreeform(&Band, 1);
		// narrow light slits to look through
		Graphics()->SetColor(0.95f, 0.95f, 1.0f, Alpha);
		const vec2 Slit = vec2(LookDir.x, LookDir.y) * 0.02f * TeeSize;
		Graphics()->DrawCircle(EyeCenter.x - EyeSeparation + Slit.x, EyeCenter.y + Slit.y, 0.045f * TeeSize, 10);
		Graphics()->DrawCircle(EyeCenter.x + EyeSeparation + Slit.x, EyeCenter.y + Slit.y, 0.045f * TeeSize, 10);
		Graphics()->QuadsEnd();
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	}

	// landing: snow flies up from the ground
	if(InAir)
		m_FallSpeed = m_WasInAir ? std::max(m_FallSpeed, Vel.y) : 0.0f;
	else if(m_WasInAir)
	{
		const float Strength = std::clamp(m_FallSpeed / 14.0f, 0.3f, 1.4f);
		if(g_Config.m_ClVisualsSnowLanding && m_FallSpeed > 3.5f)
			SpawnBurst(Feet, vec2(0.0f, -1.0f), 1.3f, (int)(6 + 14 * Strength), 40.0f + 40.0f * Strength, 90.0f + 90.0f * Strength);
		m_FallSpeed = 0.0f;
	}
	m_WasInAir = InAir;

	// jump rings: exactly when the game says that a jump happened, no cooldown
	if(m_JumpedInitialized && g_Config.m_ClVisualsJumpRings && (Jumped & ~m_LastJumped) != 0)
		SpawnRing(Feet, 40.0f, RingColor);
	m_LastJumped = Jumped;
	m_JumpedInitialized = true;

	// sparkle trail
	if(g_Config.m_ClVisualsTrail)
	{
		m_TrailAccumulator += DeltaTime * std::clamp(length(Vel) * 8.0f, 0.0f, 70.0f);
		while(m_TrailAccumulator >= 1.0f)
		{
			m_TrailAccumulator -= 1.0f;
			if(m_vSparkles.size() >= MAX_SPARKLES)
				break;
			CSparkle Sparkle;
			Sparkle.m_Pos = BodyPos + vec2(random_float(-0.25f, 0.25f), random_float(-0.25f, 0.25f)) * TeeSize;
			Sparkle.m_Vel = vec2(random_float(-12.0f, 12.0f), random_float(-22.0f, 4.0f));
			Sparkle.m_MaxLife = random_float(0.5f, 0.9f);
			Sparkle.m_Life = Sparkle.m_MaxLife;
			Sparkle.m_Size = random_float(0.22f, 0.42f) * TeeSize;
			Sparkle.m_Grow = -0.6f;
			Sparkle.m_AlphaScale = 0.8f;
			Sparkle.m_Color = Rainbow(random_float(0.0f, 0.12f));
			m_vSparkles.push_back(Sparkle);
		}
	}
	else
		m_TrailAccumulator = 0.0f;

	// dust puffs while running on the ground
	if(g_Config.m_ClVisualsDust && !InAir && std::abs(Vel.x) > 3.0f)
	{
		m_DustAccumulator += DeltaTime * 28.0f;
		while(m_DustAccumulator >= 1.0f)
		{
			m_DustAccumulator -= 1.0f;
			if(m_vSparkles.size() >= MAX_SPARKLES)
				break;
			CSparkle Puff;
			Puff.m_Pos = Feet + vec2(random_float(-0.2f, 0.2f) * TeeSize, random_float(-2.0f, 2.0f));
			Puff.m_Vel = vec2(-(Vel.x > 0.0f ? 1.0f : -1.0f) * random_float(5.0f, 25.0f), random_float(-18.0f, -4.0f));
			Puff.m_MaxLife = random_float(0.4f, 0.7f);
			Puff.m_Life = Puff.m_MaxLife;
			Puff.m_Size = random_float(0.25f, 0.45f) * TeeSize;
			Puff.m_Grow = 1.0f;
			Puff.m_AlphaScale = 0.35f;
			Puff.m_Color = Lighten(TeeColor, 0.75f);
			m_vSparkles.push_back(Puff);
		}
	}
	else
		m_DustAccumulator = 0.0f;

	// echoes: faded copies of the tee stay behind when moving fast
	if(g_Config.m_ClVisualsEcho && length(Vel) > 5.0f)
	{
		m_EchoTimer += DeltaTime;
		if(m_EchoTimer >= 0.045f)
		{
			m_EchoTimer = 0.0f;
			if(m_vEchoes.size() < MAX_ECHOES)
			{
				CEcho Echo;
				Echo.m_Pos = Position;
				Echo.m_MaxLife = 0.3f;
				Echo.m_Life = Echo.m_MaxLife;
				m_vEchoes.push_back(Echo);
			}
		}
	}
	else
		m_EchoTimer = 0.045f;

	// pet
	if(g_Config.m_ClVisualsPet > 0)
	{
		UpdatePet(Position, TeeSize, LookDir, Vel, Jumped, DeltaTime);
		DrawPanda(m_PetPos, 0.7f * TeeSize, Alpha, m_PetVel);
	}
	else
		m_PetInitialized = false;

	const bool Hat = g_Config.m_ClVisualsWinterHat;
	if(!Hat)
		m_HatInitialized = false;

	if(Hat)
	{
		UpdateHat(LookDir, Vel, DeltaTime);

		// rotate the hat around its pivot on the head
		const vec2 Pivot(0.0f, HAT_PIVOT_Y);
		const vec2 Offset(HAT_CENTER_X * m_HatFlip, HAT_CENTER_Y - HAT_PIVOT_Y);
		const float SinAngle = std::sin(m_HatAngle);
		const float CosAngle = std::cos(m_HatAngle);
		const vec2 Rotated(Offset.x * CosAngle - Offset.y * SinAngle, Offset.x * SinAngle + Offset.y * CosAngle);
		const vec2 Center = BodyPos + (Pivot + Rotated + vec2(0.0f, m_HatBob)) * TeeSize;
		// while turning around the hat gets thin for a moment, and it squashes a little when bouncing
		const float WidthScale = std::max(std::abs(m_HatFlip), 0.08f);
		const float HeightScale = 1.0f - m_HatBob * 1.2f;
		DrawSprite(m_HatTexture, Center, HAT_WIDTH * WidthScale * TeeSize, HAT_WIDTH * HAT_ASPECT * HeightScale * TeeSize, m_HatAngle, m_HatFlip < 0.0f, ColorRGBA(1.0f, 1.0f, 1.0f, Alpha));
	}
}

void CVisuals::InitFlakes()
{
	for(CFlake &Flake : m_aFlakes)
	{
		Flake.m_Base = vec2(random_float(0.0f, SNOW_TILE_WIDTH), random_float(0.0f, SNOW_TILE_HEIGHT));
		Flake.m_FallSpeed = random_float(30.0f, 60.0f);
		Flake.m_DriftSpeed = random_float(12.0f, 30.0f); // positive: from left to right
		Flake.m_Size = random_float(1.6f, 4.4f);
		Flake.m_Alpha = random_float(0.30f, 0.70f);
		Flake.m_Angle = random_float(0.0f, 2.0f * pi);
		Flake.m_Spin = random_float(-1.0f, 1.0f);
		Flake.m_SwayPhase = random_float(0.0f, 2.0f * pi);
		Flake.m_SwayFrequency = random_float(0.5f, 1.5f);
		Flake.m_Type = std::min((int)(random_float() * 3.0f), 2);
	}
	for(CFlake &Petal : m_aPetals)
	{
		Petal.m_Base = vec2(random_float(0.0f, PETAL_TILE_WIDTH), random_float(0.0f, PETAL_TILE_HEIGHT));
		Petal.m_FallSpeed = random_float(20.0f, 40.0f);
		Petal.m_DriftSpeed = random_float(10.0f, 35.0f);
		Petal.m_Size = random_float(3.0f, 5.5f);
		Petal.m_Alpha = random_float(0.6f, 0.9f);
		Petal.m_Angle = random_float(0.0f, 2.0f * pi);
		Petal.m_Spin = random_float(-1.5f, 1.5f);
		Petal.m_SwayPhase = random_float(0.0f, 2.0f * pi);
		Petal.m_SwayFrequency = random_float(0.6f, 1.6f);
		Petal.m_Type = 0;
	}
	for(CFlake &Bubble : m_aBubbles)
	{
		Bubble.m_Base = vec2(random_float(0.0f, BUBBLE_TILE_WIDTH), random_float(0.0f, BUBBLE_TILE_HEIGHT));
		Bubble.m_FallSpeed = random_float(15.0f, 35.0f); // rise speed
		Bubble.m_DriftSpeed = 0.0f;
		Bubble.m_Size = random_float(6.0f, 16.0f);
		Bubble.m_Alpha = random_float(0.35f, 0.65f);
		Bubble.m_Angle = 0.0f;
		Bubble.m_Spin = 0.0f;
		Bubble.m_SwayPhase = random_float(0.0f, 2.0f * pi);
		Bubble.m_SwayFrequency = random_float(0.4f, 1.1f);
		Bubble.m_Type = 0;
	}
	for(CFirefly &Firefly : m_aFireflies)
	{
		Firefly.m_Base = vec2(random_float(0.0f, FIREFLY_TILE_WIDTH), random_float(0.0f, FIREFLY_TILE_HEIGHT));
		Firefly.m_Amp = vec2(random_float(25.0f, 70.0f), random_float(20.0f, 55.0f));
		Firefly.m_Freq = vec2(random_float(0.15f, 0.5f), random_float(0.15f, 0.5f));
		Firefly.m_Phase = vec2(random_float(0.0f, 2.0f * pi), random_float(0.0f, 2.0f * pi));
		Firefly.m_Size = random_float(11.0f, 20.0f);
		Firefly.m_TwinkleFreq = random_float(0.8f, 2.0f);
		Firefly.m_TwinklePhase = random_float(0.0f, 2.0f * pi);
		Firefly.m_Type = std::min((int)(random_float() * 3.0f), 2);
		if(Firefly.m_Type == 1)
		{
			// big and slow, warm orange
			Firefly.m_Size *= 1.35f;
			Firefly.m_TwinkleFreq *= 0.5f;
			Firefly.m_Freq *= 0.6f;
		}
		else if(Firefly.m_Type == 2)
		{
			// small, fast and cold blue
			Firefly.m_Size *= 0.7f;
			Firefly.m_TwinkleFreq *= 1.8f;
			Firefly.m_Amp *= 1.5f;
		}
	}
	for(CStar &Star : m_aStars)
	{
		Star.m_Base = vec2(random_float(0.0f, STAR_TILE_WIDTH), random_float(0.0f, STAR_TILE_HEIGHT));
		Star.m_Size = random_float(14.0f, 34.0f);
		Star.m_TwinkleFreq = random_float(0.5f, 1.8f);
		Star.m_TwinklePhase = random_float(0.0f, 2.0f * pi);
		Star.m_Spin = random_float(-0.3f, 0.3f);
		Star.m_Type = std::min((int)(random_float() * 4.0f), 3);
	}
	m_FlakesInitialized = true;
}

void CVisuals::DrawFlake(vec2 Center, float Size, float Angle, int Type) const
{
	if(Size < 2.0f)
	{
		// small flakes are simple dots
		Graphics()->DrawCircle(Center.x, Center.y, Size * 0.6f, 8);
		return;
	}

	const auto DrawBar = [&](vec2 BarCenter, vec2 Dir, float HalfLength, float HalfWidth) {
		const vec2 Perp = vec2(-Dir.y, Dir.x);
		const IGraphics::CFreeformItem Item(
			BarCenter - Dir * HalfLength - Perp * HalfWidth,
			BarCenter + Dir * HalfLength - Perp * HalfWidth,
			BarCenter - Dir * HalfLength + Perp * HalfWidth,
			BarCenter + Dir * HalfLength + Perp * HalfWidth);
		Graphics()->QuadsDrawFreeform(&Item, 1);
	};

	if(Type == 1)
	{
		// thin eight-armed star
		const float Radius = Size * 1.4f;
		for(int Arm = 0; Arm < 4; Arm++)
		{
			const float ArmAngle = Angle + Arm * pi / 4.0f;
			DrawBar(Center, vec2(std::cos(ArmAngle), std::sin(ArmAngle)), Radius, Radius * 0.07f);
		}
		return;
	}

	// six-armed star made of three crossed bars
	const float Radius = Size * 1.3f;
	for(int Arm = 0; Arm < 3; Arm++)
	{
		const float ArmAngle = Angle + Arm * pi / 3.0f;
		DrawBar(Center, vec2(std::cos(ArmAngle), std::sin(ArmAngle)), Radius, Radius * (Type == 2 ? 0.09f : 0.12f));
	}
	if(Type == 2)
	{
		// crystal: every arm has two small side branches
		for(int Arm = 0; Arm < 6; Arm++)
		{
			const float ArmAngle = Angle + Arm * pi / 3.0f;
			const vec2 Dir(std::cos(ArmAngle), std::sin(ArmAngle));
			const vec2 BranchBase = Center + Dir * Radius * 0.6f;
			for(int Side = -1; Side <= 1; Side += 2)
			{
				const float BranchAngle = ArmAngle + Side * pi / 3.0f;
				const vec2 BranchDir(std::cos(BranchAngle), std::sin(BranchAngle));
				DrawBar(BranchBase + BranchDir * Radius * 0.17f, BranchDir, Radius * 0.17f, Radius * 0.07f);
			}
		}
	}
}

void CVisuals::OnFinish()
{
	if(g_Config.m_ClVisualsPet <= 0)
		return;
	m_PetCelebrate = 20.0f;
	m_PetSpawnBalloons = true;
	m_AfkTimer = 0.0f;
}

void CVisuals::UpdatePet(vec2 Position, float TeeSize, vec2 LookDir, vec2 Vel, int Jumped, float DeltaTime)
{
	// the owner turns around: the panda rolls over to the other side
	float Desired = m_PetFacing;
	if(LookDir.x > 0.2f)
		Desired = 1.0f;
	else if(LookDir.x < -0.2f)
		Desired = -1.0f;
	if(Desired != m_PetFacing)
	{
		m_PetFacing = Desired;
		m_PetRoll = m_PetSleep > 0.5f ? 1.0f : 0.0f;
	}
	m_PetRoll = std::min(1.0f, m_PetRoll + DeltaTime / 0.75f);
	m_PetFacingShown = m_PetRoll < 0.5f ? -m_PetFacing : m_PetFacing;
	if(m_PetRoll >= 1.0f)
		m_PetFacingShown = m_PetFacing;

	// afk detection: any movement, aiming or input wakes the panda
	const CNetObj_PlayerInput &Input = GameClient()->m_Controls.m_aInputData[g_Config.m_ClDummy];
	const int InputBits = Input.m_Direction * 8 + Input.m_Jump * 2 + Input.m_Hook * 4 + Input.m_Fire * 16 + Jumped * 64;
	const bool Active = distance(Position, m_AfkLastPos) > 0.5f || distance(LookDir, m_AfkLastLook) > 0.01f || InputBits != m_AfkLastInput || length(Vel) > 0.1f;
	m_AfkLastPos = Position;
	m_AfkLastLook = LookDir;
	m_AfkLastInput = InputBits;
	if(Active)
	{
		if(m_PetSleep > 0.5f)
			m_PetWake = 1.0f;
		m_AfkTimer = 0.0f;
	}
	else
		m_AfkTimer += DeltaTime;
	const float SleepTarget = m_AfkTimer > 300.0f ? 1.0f : 0.0f;
	m_PetSleep += std::clamp(SleepTarget - m_PetSleep, -DeltaTime / 0.8f, DeltaTime / 1.6f);
	m_PetWake = std::max(0.0f, m_PetWake - DeltaTime / 0.9f);

	// the panda stands on the ground behind the owner and follows on a soft spring
	const float Time = Client()->LocalTime();
	const vec2 Target = vec2(Position.x - m_PetFacing * 1.5f * TeeSize, Position.y + 14.0f);
	if(!m_PetInitialized || distance(m_PetPos, Target) > 700.0f)
	{
		m_PetInitialized = true;
		m_PetPos = Target;
		m_PetVel = vec2(0.0f, 0.0f);
	}
	else
	{
		const int Steps = std::clamp((int)std::ceil(DeltaTime / (1.0f / 120.0f)), 1, 12);
		const float Step = DeltaTime / Steps;
		for(int i = 0; i < Steps; i++)
		{
			m_PetVel += ((Target - m_PetPos) * 30.0f - m_PetVel * 8.0f) * Step;
			m_PetPos += m_PetVel * Step;
		}
	}
	m_PetWalk += std::fabs(m_PetVel.x) * DeltaTime * 0.045f;

	// party: balloons fly up next to the panda
	if(m_PetSpawnBalloons)
	{
		m_PetSpawnBalloons = false;
		static const ColorRGBA s_aColors[6] = {ColorRGBA(1.0f, 0.3f, 0.35f, 1.0f), ColorRGBA(1.0f, 0.8f, 0.2f, 1.0f), ColorRGBA(0.3f, 0.85f, 0.4f, 1.0f), ColorRGBA(0.3f, 0.6f, 1.0f, 1.0f), ColorRGBA(0.8f, 0.4f, 1.0f, 1.0f), ColorRGBA(1.0f, 0.55f, 0.8f, 1.0f)};
		for(int i = 0; i < 9; i++)
		{
			CBalloon Balloon;
			Balloon.m_Pos = m_PetPos + vec2(random_float(-1.0f, 1.0f) * 0.9f * TeeSize, -0.4f * TeeSize);
			Balloon.m_Vel = vec2(random_float(-8.0f, 8.0f), -random_float(35.0f, 70.0f));
			Balloon.m_Life = random_float(5.0f, 8.0f);
			Balloon.m_Phase = random_float(0.0f, 6.28f);
			Balloon.m_Color = s_aColors[i % 6];
			m_vBalloons.push_back(Balloon);
		}
		for(int i = 0; i < 14; i++)
			SpawnGlow(m_PetPos + vec2(0.0f, -0.6f * TeeSize), vec2(random_float(-90.0f, 90.0f), -random_float(60.0f, 160.0f)), 1.2f, 5.0f, -0.5f, 0.9f, s_aColors[i % 6], 220.0f);
	}
	for(CBalloon &Balloon : m_vBalloons)
	{
		Balloon.m_Life -= DeltaTime;
		Balloon.m_Pos += Balloon.m_Vel * DeltaTime;
		Balloon.m_Pos.x += std::sin(Time * 1.7f + Balloon.m_Phase) * 12.0f * DeltaTime;
	}
	m_vBalloons.erase(std::remove_if(m_vBalloons.begin(), m_vBalloons.end(), [](const CBalloon &B) { return B.m_Life <= 0.0f; }), m_vBalloons.end());
	m_PetCelebrate = std::max(0.0f, m_PetCelebrate - DeltaTime);
}

// A panda made of shapes, drawn on four legs. Feet is the point on the ground below the middle of the body.
void CVisuals::DrawPanda(vec2 Feet, float Size, float Alpha, vec2 Vel) const
{
	const float Time = Client()->LocalTime();
	const float Sleep = m_PetSleep;
	const bool Celebrating = m_PetCelebrate > 0.0f;
	const float Facing = m_PetFacingShown;

	// roll over: a full somersault around the body while the panda hops a bit
	const float RollT = m_PetRoll;
	const float RollAngle = RollT < 1.0f ? RollT * 2.0f * pi * (m_PetFacing > 0.0f ? 1.0f : -1.0f) : 0.0f;
	const float RollHop = RollT < 1.0f ? std::sin(RollT * pi) * 0.45f : 0.0f;
	// idle: sways from side to side and breathes, the party makes it jump
	const float Sway = (1.0f - Sleep) * std::sin(Time * 1.7f) * 0.05f;
	const float Breath = 1.0f + 0.018f * std::sin(Time * (Sleep > 0.5f ? 1.2f : 2.4f));
	const float Jump = Celebrating ? std::fabs(std::sin(Time * 6.0f)) * 0.32f : 0.0f;
	const float WakeJump = m_PetWake > 0.0f ? std::sin(m_PetWake * pi) * 0.25f : 0.0f;
	const float Lift = RollHop + Jump + WakeJump;
	const float Squash = 1.0f - Sleep * 0.22f;
	const float Walk = std::min(1.0f, std::fabs(Vel.x) / 120.0f) * (1.0f - Sleep);

	const float Rot = RollAngle + Sway;
	const vec2 Pivot(0.0f, -0.4f);
	const float CosR = std::cos(Rot), SinR = std::sin(Rot);
	// local coordinates: x forward, y up is negative, one unit is Size
	const auto Tr = [&](vec2 P) {
		P.y *= Squash * Breath;
		const vec2 D = P - Pivot;
		const vec2 R(D.x * CosR - D.y * SinR, D.x * SinR + D.y * CosR);
		const vec2 W = Pivot + R;
		return Feet + vec2(W.x * Facing * Size, (W.y - Lift) * Size + Sleep * 0.03f * Size);
	};

	const ColorRGBA White(0.98f, 0.98f, 0.98f, Alpha);
	const ColorRGBA Black(0.1f, 0.1f, 0.12f, Alpha);
	const ColorRGBA FarBlack(0.2f, 0.2f, 0.23f, Alpha);
	const ColorRGBA Pink(1.0f, 0.62f, 0.7f, 0.85f * Alpha);

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();

	const auto Ellipse = [&](vec2 C, float Rx, float Ry, float Angle, ColorRGBA Color) {
		Graphics()->SetColor(Color);
		constexpr int N = 18;
		const float Ca = std::cos(Angle), Sa = std::sin(Angle);
		vec2 aP[N + 1];
		for(int i = 0; i <= N; i++)
		{
			const float T = 2.0f * pi * i / N;
			const float X = std::cos(T) * Rx, Y = std::sin(T) * Ry;
			aP[i] = Tr(C + vec2(X * Ca - Y * Sa, X * Sa + Y * Ca));
		}
		const vec2 Center = Tr(C);
		for(int i = 0; i < N; i++)
		{
			const IGraphics::CFreeformItem Item(Center, aP[i], aP[i + 1], aP[i + 1]);
			Graphics()->QuadsDrawFreeform(&Item, 1);
		}
	};
	const auto Line = [&](vec2 A, vec2 B, float Width, ColorRGBA Color) {
		Graphics()->SetColor(Color);
		const vec2 PA = Tr(A), PB = Tr(B);
		vec2 Dir = PB - PA;
		const float Len = length(Dir);
		if(Len < 0.001f)
			return;
		Dir /= Len;
		const vec2 Nrm(-Dir.y, Dir.x);
		const float W = Width * Size * 0.5f;
		const IGraphics::CFreeformItem Item(PA + Nrm * W, PA - Nrm * W, PB + Nrm * W, PB - Nrm * W);
		Graphics()->QuadsDrawFreeform(&Item, 1);
	};
	const auto Tri = [&](vec2 A, vec2 B, vec2 C, ColorRGBA Color) {
		Graphics()->SetColor(Color);
		const IGraphics::CFreeformItem Item(Tr(A), Tr(B), Tr(C), Tr(C));
		Graphics()->QuadsDrawFreeform(&Item, 1);
	};

	// shadow on the ground
	Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.18f * Alpha);
	{
		constexpr int N = 14;
		const float Shadow = Size * (0.5f - Lift * 0.3f);
		vec2 aP[N + 1];
		for(int i = 0; i <= N; i++)
			aP[i] = Feet + vec2(std::cos(2.0f * pi * i / N) * Shadow, std::sin(2.0f * pi * i / N) * Shadow * 0.18f);
		for(int i = 0; i < N; i++)
		{
			const IGraphics::CFreeformItem Item(Feet, aP[i], aP[i + 1], aP[i + 1]);
			Graphics()->QuadsDrawFreeform(&Item, 1);
		}
	}

	// legs, the far ones a little lighter. Lying down they fold under the body.
	const float LegLen = 0.17f * (1.0f - Sleep * 0.7f);
	const float aLegX[4] = {-0.34f, -0.2f, 0.2f, 0.34f};
	for(int i = 0; i < 4; i++)
	{
		const bool Far = i == 1 || i == 3;
		const float Swing = std::sin(m_PetWalk * 2.0f * pi + (i == 0 || i == 3 ? 0.0f : pi)) * 0.07f * Walk;
		const float Up = std::max(0.0f, std::cos(m_PetWalk * 2.0f * pi + (i == 0 || i == 3 ? 0.0f : pi))) * 0.05f * Walk;
		Ellipse(vec2(aLegX[i] + Swing, -LegLen * 0.8f - Up), 0.085f, LegLen, 0.0f, Far ? FarBlack : Black);
	}

	// tail
	const float TailWag = std::sin(Time * 4.0f) * 0.03f * (Celebrating ? 3.0f : 1.0f);
	Ellipse(vec2(-0.45f, -0.43f + TailWag), 0.07f, 0.065f, 0.0f, White);

	// body with black shoulders and hips
	const float BodyY = -0.33f - LegLen * 0.1f;
	Ellipse(vec2(0.0f, BodyY - 0.05f), 0.43f, 0.27f, 0.0f, White);
	Ellipse(vec2(0.2f, BodyY - 0.03f), 0.11f, 0.2f, 0.0f, Black);
	Ellipse(vec2(-0.3f, BodyY - 0.02f), 0.1f, 0.17f, 0.0f, Black);

	// head, nodding a little. Asleep it rests low.
	const float Nod = (1.0f - Sleep) * std::sin(Time * 2.2f + 0.8f) * 0.02f + Sleep * 0.1f;
	const vec2 Head(0.38f, -0.6f + Nod);
	// ears (they twitch every now and then)
	const float Twitch = std::fmod(Time, 4.0f) < 0.25f ? std::sin(std::fmod(Time, 4.0f) * 25.0f) * 0.03f : 0.0f;
	Ellipse(Head + vec2(-0.14f, -0.22f + Twitch), 0.08f, 0.08f, 0.0f, Black);
	Ellipse(Head + vec2(0.16f, -0.21f), 0.08f, 0.08f, 0.0f, Black);
	Ellipse(Head, 0.27f, 0.23f, 0.0f, White);
	// eye patches
	Ellipse(Head + vec2(-0.09f, 0.0f), 0.065f, 0.085f, 0.45f, Black);
	Ellipse(Head + vec2(0.15f, 0.0f), 0.065f, 0.085f, -0.45f, Black);
	// eyes: they blink, look happy when partying and are closed when asleep
	const bool Blink = std::fmod(Time + 1.3f, 3.5f) < 0.12f;
	const vec2 aEye[2] = {Head + vec2(-0.085f, -0.005f), Head + vec2(0.145f, -0.005f)};
	for(const vec2 &Eye : aEye)
	{
		if(Sleep > 0.5f || Blink || Celebrating)
		{
			// closed (or happy) eye: a small arc
			const float Curve = Celebrating && Sleep < 0.5f ? -0.03f : 0.025f;
			Line(Eye + vec2(-0.035f, 0.0f), Eye + vec2(0.0f, Curve), 0.018f, White);
			Line(Eye + vec2(0.0f, Curve), Eye + vec2(0.035f, 0.0f), 0.018f, White);
		}
		else
		{
			Ellipse(Eye, 0.03f, 0.034f, 0.0f, White);
			Ellipse(Eye + vec2(0.006f, 0.004f), 0.015f, 0.018f, 0.0f, Black);
		}
	}
	// nose, mouth and cheeks
	Ellipse(Head + vec2(0.1f, 0.09f), 0.04f, 0.028f, 0.0f, Black);
	Line(Head + vec2(0.1f, 0.115f), Head + vec2(0.1f, 0.135f), 0.012f, Black);
	Line(Head + vec2(0.06f, 0.15f), Head + vec2(0.1f, 0.14f), 0.012f, Black);
	Line(Head + vec2(0.1f, 0.14f), Head + vec2(0.14f, 0.15f), 0.012f, Black);
	Ellipse(Head + vec2(-0.13f, 0.08f), 0.045f, 0.03f, 0.0f, Pink);
	Ellipse(Head + vec2(0.2f, 0.08f), 0.045f, 0.03f, 0.0f, Pink);

	// party hat on the head
	if(Celebrating)
	{
		const vec2 Base = Head + vec2(0.02f, -0.19f);
		Tri(Base + vec2(-0.12f, 0.0f), Base + vec2(0.12f, 0.0f), Base + vec2(0.0f, -0.3f), ColorRGBA(1.0f, 0.4f, 0.55f, Alpha));
		Tri(Base + vec2(-0.06f, -0.1f), Base + vec2(-0.02f, -0.2f), Base + vec2(0.0f, -0.1f), ColorRGBA(1.0f, 0.9f, 0.3f, Alpha));
		Tri(Base + vec2(0.0f, -0.18f), Base + vec2(0.06f, -0.1f), Base + vec2(0.09f, -0.18f), ColorRGBA(0.4f, 0.8f, 1.0f, Alpha));
		Ellipse(Base + vec2(0.0f, -0.31f), 0.05f, 0.05f, 0.0f, ColorRGBA(1.0f, 0.95f, 0.5f, Alpha));
	}

	// sleeping: floating Z letters
	if(Sleep > 0.5f)
	{
		for(int i = 0; i < 3; i++)
		{
			const float Phase = std::fmod(Time * 0.45f + i / 3.0f, 1.0f);
			const float Scale = (0.5f + Phase * 0.7f) * 0.12f;
			const vec2 Pos(0.55f + Phase * 0.3f, -0.95f - Phase * 0.55f);
			const float A = std::sin(Phase * pi) * Sleep * Alpha;
			const ColorRGBA ZColor(0.75f, 0.85f, 1.0f, A);
			Line(Pos + vec2(-Scale, -Scale), Pos + vec2(Scale, -Scale), Scale * 0.3f, ZColor);
			Line(Pos + vec2(Scale, -Scale), Pos + vec2(-Scale, Scale), Scale * 0.3f, ZColor);
			Line(Pos + vec2(-Scale, Scale), Pos + vec2(Scale, Scale), Scale * 0.3f, ZColor);
		}
	}

	// balloons of the party, they live in world coordinates
	for(const CBalloon &Balloon : m_vBalloons)
	{
		const float Fade = std::min(1.0f, Balloon.m_Life);
		const ColorRGBA Color(Balloon.m_Color.r, Balloon.m_Color.g, Balloon.m_Color.b, Fade * Alpha);
		const float Rx = Size * 0.17f, Ry = Size * 0.21f;
		Graphics()->SetColor(Color);
		constexpr int N = 16;
		vec2 aP[N + 1];
		for(int i = 0; i <= N; i++)
			aP[i] = Balloon.m_Pos + vec2(std::cos(2.0f * pi * i / N) * Rx, std::sin(2.0f * pi * i / N) * Ry);
		for(int i = 0; i < N; i++)
		{
			const IGraphics::CFreeformItem Item(Balloon.m_Pos, aP[i], aP[i + 1], aP[i + 1]);
			Graphics()->QuadsDrawFreeform(&Item, 1);
		}
		// highlight, knot and string
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.5f * Fade * Alpha);
		const vec2 Shine = Balloon.m_Pos + vec2(-Rx * 0.4f, -Ry * 0.4f);
		const IGraphics::CFreeformItem ShineItem(Shine + vec2(-2.0f, 0.0f), Shine + vec2(0.0f, -2.0f), Shine + vec2(0.0f, 2.0f), Shine + vec2(2.0f, 0.0f));
		Graphics()->QuadsDrawFreeform(&ShineItem, 1);
		Graphics()->SetColor(Color);
		const vec2 Knot = Balloon.m_Pos + vec2(0.0f, Ry);
		const IGraphics::CFreeformItem KnotItem(Knot, Knot + vec2(-3.0f, 5.0f), Knot + vec2(3.0f, 5.0f), Knot + vec2(3.0f, 5.0f));
		Graphics()->QuadsDrawFreeform(&KnotItem, 1);
		Graphics()->SetColor(0.9f, 0.9f, 0.9f, 0.7f * Fade * Alpha);
		const vec2 StringEnd = Knot + vec2(std::sin(Time * 2.0f + Balloon.m_Phase) * 5.0f, Size * 0.45f);
		const IGraphics::CFreeformItem StringItem(Knot + vec2(-0.5f, 5.0f), Knot + vec2(0.5f, 5.0f), StringEnd + vec2(-0.5f, 0.0f), StringEnd + vec2(0.5f, 0.0f));
		Graphics()->QuadsDrawFreeform(&StringItem, 1);
	}

	Graphics()->QuadsEnd();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
}

ColorRGBA CVisuals::NameShimmerColor(int CharIndex) const
{
	// the chosen colors flow through the name from left to right
	const ColorRGBA aColors[3] = {
		color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClVisualsNameColor1)),
		color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClVisualsNameColor2)),
		color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClVisualsNameColor3)),
	};
	const double Phase = CharIndex * 0.1 - Client()->LocalTime() * g_Config.m_ClVisualsNameSpeed * 0.1;
	const float Position = (float)(Phase - std::floor(Phase)) * 3.0f;
	const int Index = std::min((int)Position, 2);
	float Blend = Position - Index;
	Blend = Blend * Blend * (3.0f - 2.0f * Blend);
	const ColorRGBA &From = aColors[Index];
	const ColorRGBA &To = aColors[(Index + 1) % 3];
	return ColorRGBA(From.r + (To.r - From.r) * Blend, From.g + (To.g - From.g) * Blend, From.b + (To.b - From.b) * Blend, 1.0f);
}

void CVisuals::OnRender()
{
	const int State = Client()->State();
	if(State != IClient::STATE_ONLINE && State != IClient::STATE_DEMOPLAYBACK)
	{
		m_vBurstFlakes.clear();
		m_vFlashes.clear();
		m_vSparkles.clear();
		m_vRings.clear();
		m_vGlows.clear();
		m_vSouls.clear();
		m_vBlackHoles.clear();
		m_vMeteors.clear();
		m_vEchoes.clear();
		return;
	}

	UpdateAutoSkin(std::min(Client()->RenderFrameTime(), 0.1f));

	const bool Snow = g_Config.m_ClVisualsSnow;
	const bool Fireflies = g_Config.m_ClVisualsFireflies;
	const bool Predict = g_Config.m_ClCombatPredict;
	const bool Petals = g_Config.m_ClVisualsPetals;
	const bool Bubbles = g_Config.m_ClVisualsBubbles;
	const bool Starfield = g_Config.m_ClVisualsStarfield;
	const bool Aurora = g_Config.m_ClVisualsAurora;
	const bool Meteors = g_Config.m_ClVisualsMeteors;
	if(!Snow && !Fireflies && !Predict && !Petals && !Bubbles && !Starfield && !Aurora && !Meteors && m_vBlackHoles.empty() && m_vBurstFlakes.empty() && m_vFlashes.empty() && m_vRings.empty() && m_vGlows.empty() && m_vSouls.empty() && m_vMeteors.empty())
		return;

	// The game world is still mapped to the screen here, so everything below is drawn in world coordinates.
	// This makes the snow part of the map instead of being attached to the screen.
	const CScreenRect View = Graphics()->GetScreen();
	const float DeltaTime = std::min(Client()->RenderFrameTime(), 0.1f);
	const double Time = Client()->LocalTime();

	if(!m_FlakesInitialized)
		InitFlakes();

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();

	if(Snow)
	{
		// The size grows when zooming out so the flakes stay visible; positions only depend on time,
		// so zooming does not move them.
		const float SizeScale = std::clamp(View.Size().y / 700.0f, 1.0f, 4.0f);
		const float Margin = 24.0f * SizeScale;
		for(const CFlake &Flake : m_aFlakes)
		{
			const double X = Flake.m_Base.x + Flake.m_DriftSpeed * Time + std::sin(Flake.m_SwayPhase + Flake.m_SwayFrequency * Time) * 8.0;
			const double Y = Flake.m_Base.y + Flake.m_FallSpeed * Time;
			const float PosX = (float)std::fmod(std::fmod(X, SNOW_TILE_WIDTH) + SNOW_TILE_WIDTH, (double)SNOW_TILE_WIDTH);
			const float PosY = (float)std::fmod(std::fmod(Y, SNOW_TILE_HEIGHT) + SNOW_TILE_HEIGHT, (double)SNOW_TILE_HEIGHT);
			const float Angle = Flake.m_Angle + (float)std::fmod(Flake.m_Spin * Time, 2.0 * pi);

			const int MinTileX = (int)std::ceil((View.m_TopLeft.x - Margin - PosX) / SNOW_TILE_WIDTH);
			const int MaxTileX = (int)std::floor((View.m_BottomRight.x + Margin - PosX) / SNOW_TILE_WIDTH);
			const int MinTileY = (int)std::ceil((View.m_TopLeft.y - Margin - PosY) / SNOW_TILE_HEIGHT);
			const int MaxTileY = (int)std::floor((View.m_BottomRight.y + Margin - PosY) / SNOW_TILE_HEIGHT);
			if((MaxTileX - MinTileX + 1) * (MaxTileY - MinTileY + 1) > 400)
				continue;

			Graphics()->SetColor(1.0f, 1.0f, 1.0f, Flake.m_Alpha);
			for(int TileY = MinTileY; TileY <= MaxTileY; TileY++)
			{
				for(int TileX = MinTileX; TileX <= MaxTileX; TileX++)
					DrawFlake(vec2(PosX + TileX * SNOW_TILE_WIDTH, PosY + TileY * SNOW_TILE_HEIGHT), Flake.m_Size * SizeScale, Angle, Flake.m_Type);
			}
		}
	}

	// calls Draw(position) for every copy of a tiled object that is inside the view
	const auto ForEachTile = [&](float PosX, float PosY, float TileWidth, float TileHeight, float Margin, const auto &Draw) {
		const int MinTileX = (int)std::ceil((View.m_TopLeft.x - Margin - PosX) / TileWidth);
		const int MaxTileX = (int)std::floor((View.m_BottomRight.x + Margin - PosX) / TileWidth);
		const int MinTileY = (int)std::ceil((View.m_TopLeft.y - Margin - PosY) / TileHeight);
		const int MaxTileY = (int)std::floor((View.m_BottomRight.y + Margin - PosY) / TileHeight);
		if((MaxTileX - MinTileX + 1) * (MaxTileY - MinTileY + 1) > 400)
			return;
		for(int TileY = MinTileY; TileY <= MaxTileY; TileY++)
		{
			for(int TileX = MinTileX; TileX <= MaxTileX; TileX++)
				Draw(vec2(PosX + TileX * TileWidth, PosY + TileY * TileHeight));
		}
	};
	const float FieldScale = std::clamp(View.Size().y / 700.0f, 1.0f, 4.0f);

	// blossom petals: pink diamonds that tumble down
	if(Petals)
	{
		for(const CFlake &Petal : m_aPetals)
		{
			const double X = Petal.m_Base.x + Petal.m_DriftSpeed * Time + std::sin(Petal.m_SwayPhase + Petal.m_SwayFrequency * Time) * 30.0;
			const double Y = Petal.m_Base.y + Petal.m_FallSpeed * Time;
			const float PosX = (float)std::fmod(std::fmod(X, PETAL_TILE_WIDTH) + PETAL_TILE_WIDTH, (double)PETAL_TILE_WIDTH);
			const float PosY = (float)std::fmod(std::fmod(Y, PETAL_TILE_HEIGHT) + PETAL_TILE_HEIGHT, (double)PETAL_TILE_HEIGHT);
			const float Angle = Petal.m_Angle + (float)std::fmod(Petal.m_Spin * Time, 2.0 * pi);
			const float Radius = Petal.m_Size * FieldScale;
			const vec2 Dir(std::cos(Angle), std::sin(Angle));
			const vec2 Perp(-Dir.y, Dir.x);
			Graphics()->SetColor(1.0f, 0.62f + 0.15f * std::sin(Petal.m_SwayPhase), 0.78f, Petal.m_Alpha);
			ForEachTile(PosX, PosY, PETAL_TILE_WIDTH, PETAL_TILE_HEIGHT, Radius * 2.0f, [&](vec2 Center) {
				const IGraphics::CFreeformItem Item(Center - Dir * Radius, Center + Perp * Radius * 0.55f, Center - Perp * Radius * 0.55f, Center + Dir * Radius);
				Graphics()->QuadsDrawFreeform(&Item, 1);
			});
		}
	}

	// soap bubbles: thin rings with a highlight that rise slowly
	if(Bubbles)
	{
		constexpr int BUBBLE_SEGMENTS = 16;
		for(const CFlake &Bubble : m_aBubbles)
		{
			const double X = Bubble.m_Base.x + std::sin(Bubble.m_SwayPhase + Bubble.m_SwayFrequency * Time) * 14.0;
			const double Y = Bubble.m_Base.y - Bubble.m_FallSpeed * Time;
			const float PosX = (float)std::fmod(std::fmod(X, BUBBLE_TILE_WIDTH) + BUBBLE_TILE_WIDTH, (double)BUBBLE_TILE_WIDTH);
			const float PosY = (float)std::fmod(std::fmod(Y, BUBBLE_TILE_HEIGHT) + BUBBLE_TILE_HEIGHT, (double)BUBBLE_TILE_HEIGHT);
			const float Radius = Bubble.m_Size * FieldScale;
			const float HalfWidth = std::max(0.7f, Radius * 0.08f);
			ForEachTile(PosX, PosY, BUBBLE_TILE_WIDTH, BUBBLE_TILE_HEIGHT, Radius * 2.0f, [&](vec2 Center) {
				Graphics()->SetColor(0.7f, 0.9f, 1.0f, Bubble.m_Alpha);
				for(int Segment = 0; Segment < BUBBLE_SEGMENTS; Segment++)
				{
					const float A0 = 2.0f * pi * Segment / BUBBLE_SEGMENTS;
					const float A1 = 2.0f * pi * (Segment + 1) / BUBBLE_SEGMENTS;
					const vec2 Dir0(std::cos(A0), std::sin(A0));
					const vec2 Dir1(std::cos(A1), std::sin(A1));
					const IGraphics::CFreeformItem Item(
						Center + Dir0 * (Radius - HalfWidth), Center + Dir1 * (Radius - HalfWidth),
						Center + Dir0 * (Radius + HalfWidth), Center + Dir1 * (Radius + HalfWidth));
					Graphics()->QuadsDrawFreeform(&Item, 1);
				}
				Graphics()->SetColor(1.0f, 1.0f, 1.0f, Bubble.m_Alpha + 0.2f);
				Graphics()->DrawCircle(Center.x - Radius * 0.4f, Center.y - Radius * 0.4f, Radius * 0.14f, 8);
			});
		}
	}

	for(size_t i = 0; i < m_vBurstFlakes.size();)
	{
		CBurstFlake &Flake = m_vBurstFlakes[i];
		Flake.m_Life -= DeltaTime;
		if(Flake.m_Life <= 0.0f)
		{
			Flake = m_vBurstFlakes.back();
			m_vBurstFlakes.pop_back();
			continue;
		}
		Flake.m_Vel.y += 90.0f * DeltaTime;
		Flake.m_Vel *= std::exp(-1.5f * DeltaTime);
		Flake.m_Pos += Flake.m_Vel * DeltaTime;
		Flake.m_Angle += Flake.m_Spin * DeltaTime;

		Graphics()->SetColor(1.0f, 1.0f, 1.0f, std::clamp(Flake.m_Life / Flake.m_MaxLife, 0.0f, 1.0f) * 0.9f);
		DrawFlake(Flake.m_Pos, Flake.m_Size, Flake.m_Angle, Flake.m_Type);
		i++;
	}

	// expanding rings (flat ellipses at the feet)
	if(!m_vRings.empty())
	{
		Graphics()->BlendAdditive();
		constexpr int SEGMENTS = 28;
		for(size_t i = 0; i < m_vRings.size();)
		{
			CRing &Ring = m_vRings[i];
			Ring.m_Life -= DeltaTime;
			if(Ring.m_Life <= 0.0f)
			{
				Ring = m_vRings.back();
				m_vRings.pop_back();
				continue;
			}
			const float Progress = 1.0f - std::clamp(Ring.m_Life / Ring.m_MaxLife, 0.0f, 1.0f);
			const float Radius = Ring.m_MaxRadius * (1.0f - (1.0f - Progress) * (1.0f - Progress));
			const float HalfWidth = Ring.m_Flat ? 1.6f : 2.4f;
			const float Squash = Ring.m_Flat ? 0.35f : 1.0f;
			Graphics()->SetColor(Ring.m_Color.r, Ring.m_Color.g, Ring.m_Color.b, (1.0f - Progress) * 0.85f);
			for(int Segment = 0; Segment < SEGMENTS; Segment++)
			{
				const float A0 = 2.0f * pi * Segment / SEGMENTS;
				const float A1 = 2.0f * pi * (Segment + 1) / SEGMENTS;
				const vec2 Dir0(std::cos(A0), std::sin(A0) * Squash);
				const vec2 Dir1(std::cos(A1), std::sin(A1) * Squash);
				const vec2 In0 = Ring.m_Pos + Dir0 * (Radius - HalfWidth);
				const vec2 In1 = Ring.m_Pos + Dir1 * (Radius - HalfWidth);
				const vec2 Out0 = Ring.m_Pos + Dir0 * (Radius + HalfWidth);
				const vec2 Out1 = Ring.m_Pos + Dir1 * (Radius + HalfWidth);
				const IGraphics::CFreeformItem Item(In0.x, In0.y, In1.x, In1.y, Out0.x, Out0.y, Out1.x, Out1.y);
				Graphics()->QuadsDrawFreeform(&Item, 1);
			}
			i++;
		}
		Graphics()->BlendNormal();
	}

	Graphics()->QuadsEnd();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);

	// glowing flashes
	if(!m_vFlashes.empty())
	{
		Graphics()->BlendAdditive();
		for(size_t i = 0; i < m_vFlashes.size();)
		{
			CFlash &Flash = m_vFlashes[i];
			Flash.m_Life -= DeltaTime;
			if(Flash.m_Life <= 0.0f)
			{
				Flash = m_vFlashes.back();
				m_vFlashes.pop_back();
				continue;
			}
			const float Remaining = std::clamp(Flash.m_Life / Flash.m_MaxLife, 0.0f, 1.0f);
			const float Size = Flash.m_Size * (1.0f + (1.0f - Remaining) * 0.5f);
			DrawSprite(m_GlowTexture, Flash.m_Pos, Size, Size, 0.0f, false, ColorRGBA(1.0f, 1.0f, 1.0f, Remaining * 0.9f));
			i++;
		}
		Graphics()->BlendNormal();
	}

	// ghosts that rise where a tee was killed: one smooth sprite whose hem waves
	for(size_t i = 0; i < m_vSouls.size();)
	{
		CSoul &Soul = m_vSouls[i];
		Soul.m_Life -= DeltaTime;
		if(Soul.m_Life <= 0.0f)
		{
			Soul = m_vSouls.back();
			m_vSouls.pop_back();
			continue;
		}
		Soul.m_Pos.y -= 40.0f * DeltaTime;
		Soul.m_Pos.x += std::sin(Soul.m_Phase + (float)Time * 2.0f) * 14.0f * DeltaTime;
		const float Remaining = std::clamp(Soul.m_Life / Soul.m_MaxLife, 0.0f, 1.0f);
		const float Age = 1.0f - Remaining;
		const float Alpha = std::min(1.0f, Age * 6.0f) * std::min(1.0f, Remaining * 2.2f) * 0.9f;
		const float Scale = 0.75f + 0.35f * std::min(1.0f, Age * 2.5f) + std::sin((float)Time * 3.0f + Soul.m_Phase) * 0.03f;
		DrawWavy(m_SoulTexture, Soul.m_Pos, Soul.m_Size * Scale, Soul.m_Size * Scale, std::sin((float)Time * 2.2f + Soul.m_Phase) * 0.1f, false,
			ColorRGBA(Soul.m_Color.r, Soul.m_Color.g, Soul.m_Color.b, Alpha), 0.1f, 7.0f, (float)Time * 5.0f + Soul.m_Phase);
		i++;
	}

	// black holes: a dark core, a rotating accretion disc and stars that spiral inwards
	for(size_t i = 0; i < m_vBlackHoles.size();)
	{
		CBlackHole &Hole = m_vBlackHoles[i];
		Hole.m_Life -= DeltaTime;
		if(Hole.m_Life <= 0.0f)
		{
			for(int Star = 0; Star < 12; Star++)
			{
				const float Angle = 2.0f * pi * Star / 12.0f;
				SpawnStar(Hole.m_Pos, vec2(std::cos(Angle), std::sin(Angle)) * random_float(120.0f, 260.0f), random_float(0.6f, 1.0f), random_float(14.0f, 22.0f), 0.95f,
					color_cast<ColorRGBA>(ColorHSLA(std::fmod(Hole.m_Hue + Star / 12.0f, 1.0f), 1.0f, 0.6f)));
			}
			SpawnFlash(Hole.m_Pos, 120.0f, 0.3f);
			Hole = m_vBlackHoles.back();
			m_vBlackHoles.pop_back();
			continue;
		}
		const float Progress = 1.0f - std::clamp(Hole.m_Life / Hole.m_MaxLife, 0.0f, 1.0f);
		const float Radius = 40.0f * std::sin(pi * Progress);
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(0.0f, 0.0f, 0.05f, 0.92f);
		Graphics()->DrawCircle(Hole.m_Pos.x, Hole.m_Pos.y, Radius * 0.55f, 24);
		Graphics()->QuadsEnd();
		Graphics()->BlendAdditive();
		const ColorRGBA DiscColor = color_cast<ColorRGBA>(ColorHSLA(std::fmod(Hole.m_Hue + (float)Time * 0.3f, 1.0f), 1.0f, 0.6f));
		DrawFlatCircle(m_CircleTexture, Hole.m_Pos, Radius * 1.9f, (float)Time * 2.5f, 1.0f, ColorRGBA(DiscColor.r, DiscColor.g, DiscColor.b, 0.85f));
		for(int Star = 0; Star < 10; Star++)
		{
			const float Angle = (float)Time * 7.0f * (1.0f + Progress) + Star * 2.0f * pi / 10.0f;
			const float Dist = Radius * 0.7f + 120.0f * (1.0f - Progress) * (0.4f + 0.6f * (Star % 3) / 2.0f);
			const ColorRGBA StarColor = color_cast<ColorRGBA>(ColorHSLA(std::fmod(Hole.m_Hue + Star / 10.0f, 1.0f), 1.0f, 0.65f));
			DrawSprite(m_StarTexture, Hole.m_Pos + vec2(std::cos(Angle), std::sin(Angle)) * Dist, 16.0f, 16.0f, Angle, false, StarColor);
		}
		Graphics()->BlendNormal();
		i++;
	}

	// glowing particles of weapon trails, sparks and stars
	if(!m_vGlows.empty())
	{
		Graphics()->BlendAdditive();
		StepGlows(m_vGlows, DeltaTime);
		Graphics()->BlendNormal();
	}

	// blue shooting stars that fall, shed embers and burn up
	if(Meteors)
	{
		m_MeteorTimer -= DeltaTime;
		if(m_MeteorTimer <= 0.0f && m_vMeteors.size() < MAX_METEORS)
		{
			m_MeteorTimer = random_float(2.5f, 6.0f);
			const vec2 ViewSize = View.Size();
			const float Side = random_float() < 0.5f ? -1.0f : 1.0f;
			CMeteor Meteor;
			Meteor.m_Pos = View.m_TopLeft + vec2(random_float(0.1f, 0.9f) * ViewSize.x, random_float(-0.05f, 0.12f) * ViewSize.y);
			Meteor.m_Vel = normalize(vec2(Side * random_float(0.25f, 0.55f), 1.0f)) * random_float(380.0f, 520.0f) * FieldScale;
			Meteor.m_MaxLife = random_float(1.6f, 2.3f);
			Meteor.m_Life = Meteor.m_MaxLife;
			Meteor.m_Length = random_float(170.0f, 250.0f) * FieldScale;
			Meteor.m_Size = random_float(3.0f, 4.5f) * FieldScale;
			Meteor.m_EmberTimer = 0.0f;
			m_vMeteors.push_back(Meteor);
		}
	}
	if(!m_vMeteors.empty())
	{
		Graphics()->BlendAdditive();
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		constexpr int TAIL_SEGMENTS = 10;
		for(size_t i = 0; i < m_vMeteors.size();)
		{
			CMeteor &Meteor = m_vMeteors[i];
			Meteor.m_Life -= DeltaTime;
			if(Meteor.m_Life <= 0.0f)
			{
				// it has burned up completely: a last blue flash and a few sparks
				SpawnGlow(Meteor.m_Pos, vec2(0.0f, 0.0f), 0.4f, Meteor.m_Size * 14.0f, 0.8f, 0.8f, ColorRGBA(0.35f, 0.6f, 1.0f, 1.0f));
				for(int Spark = 0; Spark < 7; Spark++)
					SpawnStar(Meteor.m_Pos, vec2(random_float(-70.0f, 70.0f), random_float(-60.0f, 60.0f)), random_float(0.5f, 0.9f), random_float(8.0f, 14.0f) * FieldScale, 0.9f, ColorRGBA(0.5f, 0.75f, 1.0f, 1.0f), 50.0f);
				Meteor = m_vMeteors.back();
				m_vMeteors.pop_back();
				continue;
			}
			Meteor.m_Pos += Meteor.m_Vel * DeltaTime;
			Meteor.m_Vel *= std::exp(-0.25f * DeltaTime);
			const float Remaining = std::clamp(Meteor.m_Life / Meteor.m_MaxLife, 0.0f, 1.0f);
			const float Progress = 1.0f - Remaining;
			const float Burn = Progress * Progress;
			const float Fade = std::min(1.0f, Remaining * 4.0f);
			const float TailLength = Meteor.m_Length * (1.0f - 0.55f * Progress);
			const float HeadScale = 1.0f - 0.75f * Burn;
			const vec2 Dir = normalize(Meteor.m_Vel);
			const vec2 Perp(-Dir.y, Dir.x);

			// embers break off and fall down while it burns
			Meteor.m_EmberTimer -= DeltaTime;
			while(Meteor.m_EmberTimer <= 0.0f)
			{
				Meteor.m_EmberTimer += 0.03f - 0.02f * Burn;
				SpawnGlow(Meteor.m_Pos - Dir * random_float(0.0f, TailLength * 0.5f) + Perp * random_float(-1.0f, 1.0f) * Meteor.m_Size,
					Perp * random_float(-25.0f, 25.0f) - Dir * random_float(10.0f, 40.0f), random_float(0.4f, 0.9f), Meteor.m_Size * random_float(1.2f, 2.4f), -0.8f, 0.85f * Fade,
					ColorRGBA(0.45f, 0.7f + random_float(0.0f, 0.3f), 1.0f, 1.0f), 70.0f);
			}

			for(int Segment = 0; Segment < TAIL_SEGMENTS; Segment++)
			{
				const float T0 = (float)Segment / TAIL_SEGMENTS;
				const float T1 = (float)(Segment + 1) / TAIL_SEGMENTS;
				const vec2 A = Meteor.m_Pos - Dir * TailLength * T0;
				const vec2 B = Meteor.m_Pos - Dir * TailLength * T1;
				const float WidthA = Meteor.m_Size * 0.9f * HeadScale * (1.0f - T0);
				const float WidthB = Meteor.m_Size * 0.9f * HeadScale * (1.0f - T1);
				// white-blue at the head, deep blue at the end of the tail
				const ColorRGBA ColorA(0.75f - 0.55f * T0, 0.88f - 0.5f * T0, 1.0f, std::pow(1.0f - T0, 1.5f) * 0.9f * Fade);
				const ColorRGBA ColorB(0.75f - 0.55f * T1, 0.88f - 0.5f * T1, 1.0f, std::pow(1.0f - T1, 1.5f) * 0.9f * Fade);
				Graphics()->SetColor4(ColorA, ColorA, ColorB, ColorB);
				const IGraphics::CFreeformItem Item(A - Perp * WidthA, A + Perp * WidthA, B - Perp * WidthB, B + Perp * WidthB);
				Graphics()->QuadsDrawFreeform(&Item, 1);
			}
			i++;
		}
		Graphics()->QuadsEnd();
		for(const CMeteor &Meteor : m_vMeteors)
		{
			const float Remaining = std::clamp(Meteor.m_Life / Meteor.m_MaxLife, 0.0f, 1.0f);
			const float Burn = (1.0f - Remaining) * (1.0f - Remaining);
			const float Fade = std::min(1.0f, Remaining * 4.0f);
			const float HeadScale = 1.0f - 0.75f * Burn;
			DrawSprite(m_GlowTexture, Meteor.m_Pos, Meteor.m_Size * 8.0f * HeadScale, Meteor.m_Size * 8.0f * HeadScale, 0.0f, false, ColorRGBA(0.3f, 0.55f, 1.0f, 0.9f * Fade));
			DrawSprite(m_StarTexture, Meteor.m_Pos, Meteor.m_Size * 7.0f * HeadScale, Meteor.m_Size * 7.0f * HeadScale, (float)Time * 3.0f, false, ColorRGBA(0.8f, 0.92f, 1.0f, Fade));
		}
		Graphics()->BlendNormal();
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	}

	// cosmic stars that twinkle in the world
	if(Starfield)
	{
		static const ColorRGBA s_aStarColors[4] = {ColorRGBA(0.8f, 0.9f, 1.0f, 1.0f), ColorRGBA(0.75f, 0.6f, 1.0f, 1.0f), ColorRGBA(1.0f, 0.7f, 0.9f, 1.0f), ColorRGBA(0.5f, 1.0f, 1.0f, 1.0f)};
		Graphics()->BlendAdditive();
		Graphics()->TextureSet(m_StarTexture);
		Graphics()->QuadsBegin();
		for(const CStar &Star : m_aStars)
		{
			const float Twinkle = 0.5f + 0.5f * std::sin((float)std::fmod(Time * Star.m_TwinkleFreq, 2.0 * pi) + Star.m_TwinklePhase);
			const ColorRGBA Color = s_aStarColors[Star.m_Type];
			Graphics()->SetColor(Color.r, Color.g, Color.b, 0.25f + 0.7f * Twinkle);
			Graphics()->QuadsSetRotation((float)std::fmod(Time * Star.m_Spin, 2.0 * pi));
			const float Size = Star.m_Size * FieldScale * (0.7f + 0.3f * Twinkle);
			ForEachTile(Star.m_Base.x, Star.m_Base.y, STAR_TILE_WIDTH, STAR_TILE_HEIGHT, Size, [&](vec2 Center) {
				IGraphics::CQuadItem Item(Center.x, Center.y, Size, Size);
				Graphics()->QuadsDraw(&Item, 1);
			});
		}
		Graphics()->QuadsSetRotation(0.0f);
		Graphics()->QuadsEnd();
		Graphics()->BlendNormal();
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	}

	if(Aurora)
		RenderAurora(View);

	// fireflies
	if(Fireflies)
	{
		const float SizeScale = std::clamp(View.Size().y / 700.0f, 1.0f, 4.0f);
		const float Margin = 100.0f + 20.0f * SizeScale;
		Graphics()->BlendAdditive();
		Graphics()->TextureSet(m_GlowTexture);
		Graphics()->QuadsBegin();
		for(const CFirefly &Firefly : m_aFireflies)
		{
			const float PosX = Firefly.m_Base.x + std::sin((float)std::fmod(Time * Firefly.m_Freq.x, 2.0 * pi) + Firefly.m_Phase.x) * Firefly.m_Amp.x;
			const float PosY = Firefly.m_Base.y + std::cos((float)std::fmod(Time * Firefly.m_Freq.y, 2.0 * pi) + Firefly.m_Phase.y) * Firefly.m_Amp.y;
			const float Twinkle = 0.5f + 0.5f * std::sin((float)std::fmod(Time * Firefly.m_TwinkleFreq, 2.0 * pi) + Firefly.m_TwinklePhase);
			static const ColorRGBA s_aFireflyColors[3] = {ColorRGBA(0.85f, 1.0f, 0.45f, 1.0f), ColorRGBA(1.0f, 0.68f, 0.3f, 1.0f), ColorRGBA(0.5f, 0.85f, 1.0f, 1.0f)};
			const ColorRGBA FireflyColor = s_aFireflyColors[Firefly.m_Type];
			Graphics()->SetColor(FireflyColor.r, FireflyColor.g, FireflyColor.b, 0.25f + 0.65f * Twinkle);

			const int MinTileX = (int)std::ceil((View.m_TopLeft.x - Margin - PosX) / FIREFLY_TILE_WIDTH);
			const int MaxTileX = (int)std::floor((View.m_BottomRight.x + Margin - PosX) / FIREFLY_TILE_WIDTH);
			const int MinTileY = (int)std::ceil((View.m_TopLeft.y - Margin - PosY) / FIREFLY_TILE_HEIGHT);
			const int MaxTileY = (int)std::floor((View.m_BottomRight.y + Margin - PosY) / FIREFLY_TILE_HEIGHT);
			if((MaxTileX - MinTileX + 1) * (MaxTileY - MinTileY + 1) > 400)
				continue;
			for(int TileY = MinTileY; TileY <= MaxTileY; TileY++)
			{
				for(int TileX = MinTileX; TileX <= MaxTileX; TileX++)
				{
					IGraphics::CQuadItem Item(PosX + TileX * FIREFLY_TILE_WIDTH, PosY + TileY * FIREFLY_TILE_HEIGHT, Firefly.m_Size * SizeScale, Firefly.m_Size * SizeScale);
					Graphics()->QuadsDraw(&Item, 1);
				}
			}
		}
		Graphics()->QuadsEnd();
		Graphics()->BlendNormal();
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	}

	if(Predict)
		RenderGunPredict(View);
}

void CVisuals::RenderGunPredict(const CScreenRect &View) const
{
	const auto &Snap = GameClient()->m_Snap;
	if(!Snap.m_pLocalCharacter || Snap.m_SpecInfo.m_Active || Snap.m_LocalClientId < 0 || Client()->State() != IClient::STATE_ONLINE)
		return;
	const int Weapon = Snap.m_pLocalCharacter->m_Weapon;
	if((Weapon == WEAPON_LASER && !g_Config.m_ClCombatPredictLaser) ||
		(Weapon == WEAPON_SHOTGUN && !g_Config.m_ClCombatPredictShotgun) ||
		(Weapon == WEAPON_GRENADE && !g_Config.m_ClCombatPredictGrenade) ||
		(Weapon == WEAPON_GUN && !g_Config.m_ClCombatPredictGun))
		return;
	if(Weapon != WEAPON_GUN && Weapon != WEAPON_SHOTGUN && Weapon != WEAPON_GRENADE && Weapon != WEAPON_LASER)
		return;

	const vec2 Pos = GameClient()->m_aClients[Snap.m_LocalClientId].m_RenderPos;
	const vec2 Mouse = GameClient()->m_Controls.m_aMousePos[g_Config.m_ClDummy];
	vec2 Dir = normalize(vec2((int)Mouse.x, (int)Mouse.y));
	if(Dir == vec2(0.0f, 0.0f))
		Dir = vec2(1.0f, 0.0f);
	const CTuningParams &Tuning = GameClient()->m_aTuning[g_Config.m_ClDummy];

	std::vector<vec2> vPoints;
	std::vector<vec2> vBounces;
	vec2 End = Pos;

	if(Weapon == WEAPON_LASER || Weapon == WEAPON_SHOTGUN)
	{
		// the same bouncing as a real laser: trickshots work
		vec2 Cur = Pos;
		vec2 CurDir = Dir;
		float Energy = Tuning.m_LaserReach;
		const float BounceCost = Tuning.m_LaserBounceCost;
		const int MaxBounces = Tuning.m_LaserBounceNum;
		int Bounces = 0;
		bool LastZero = false;
		vPoints.push_back(Cur);
		for(int Iteration = 0; Iteration < 20 && Energy >= 0.0f; Iteration++)
		{
			vec2 To = Cur + CurDir * Energy;
			vec2 Hit;
			const int Res = Collision()->IntersectLineTeleWeapon(Cur, To, &Hit, &To);
			vPoints.push_back(To);
			End = To;
			if(!Res)
				break;

			vec2 Next = To;
			vec2 NextDir = CurDir * 4.0f;
			Collision()->MovePoint(&Next, &NextDir, 1.0f, nullptr);

			const float Distance = distance(Cur, To);
			if(Distance == 0.0f && LastZero)
				break;
			Energy -= Distance + BounceCost;
			LastZero = Distance == 0.0f;
			Bounces++;
			vBounces.push_back(To);
			if(Bounces > MaxBounces)
				break;
			Cur = Next;
			CurDir = normalize(NextDir);
		}
	}
	else
	{
		// grenade and gun fly on a curve and stop at the first wall
		float Curvature, Speed, Lifetime;
		if(Weapon == WEAPON_GRENADE)
		{
			Curvature = Tuning.m_GrenadeCurvature;
			Speed = Tuning.m_GrenadeSpeed;
			Lifetime = Tuning.m_GrenadeLifetime;
		}
		else
		{
			Curvature = Tuning.m_GunCurvature;
			Speed = Tuning.m_GunSpeed;
			Lifetime = Tuning.m_GunLifetime;
		}
		const int TickSpeed = Client()->GameTickSpeed();
		const int Ticks = std::clamp((int)(Lifetime * TickSpeed), 1, 300);
		const vec2 Start = Pos + Dir * (CCharacterCore::PhysicalSize() * 0.75f);
		vec2 Prev = Start;
		vPoints.push_back(Start);
		End = Start;
		for(int Tick = 1; Tick <= Ticks; Tick++)
		{
			const vec2 Cur = CalcPos(Start, Dir, Curvature, Speed, Tick / (float)TickSpeed);
			vec2 Hit, Before;
			if(Collision()->IntersectLine(Prev, Cur, &Hit, &Before))
			{
				vPoints.push_back(Before);
				End = Before;
				break;
			}
			vPoints.push_back(Cur);
			End = Cur;
			Prev = Cur;
		}
	}

	if(vPoints.size() < 2)
		return;

	const float Scale = std::clamp(View.Size().y / 700.0f, 1.0f, 4.0f);
	const float HalfWidth = 0.2f * g_Config.m_ClCombatPredictWidth * Scale;
	const float BaseAlpha = g_Config.m_ClCombatPredictAlpha / 100.0f;
	const ColorRGBA LineColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClCombatPredictColor));
	const ColorRGBA BounceColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClCombatPredictBounceColor));
	const ColorRGBA EndColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClCombatPredictEndColor));
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	const size_t NumSegments = vPoints.size() - 1;
	for(size_t i = 0; i < NumSegments; i++)
	{
		const vec2 A = vPoints[i];
		const vec2 B = vPoints[i + 1];
		const float Length = distance(A, B);
		if(Length < 0.01f)
			continue;
		const vec2 Perp = vec2(-(B.y - A.y), B.x - A.x) / Length * HalfWidth;
		const float Fade = g_Config.m_ClCombatPredictFade ? 1.0f - 0.55f * (float)i / (float)NumSegments : 1.0f;
		Graphics()->SetColor(LineColor.r, LineColor.g, LineColor.b, BaseAlpha * Fade);
		const IGraphics::CFreeformItem Item(A - Perp, A + Perp, B - Perp, B + Perp);
		Graphics()->QuadsDrawFreeform(&Item, 1);
	}
	// small dots where a laser bounces and a bigger one where the shot ends
	if(g_Config.m_ClCombatPredictBounces)
	{
		Graphics()->SetColor(BounceColor.r, BounceColor.g, BounceColor.b, std::min(1.0f, BaseAlpha + 0.1f));
		for(const vec2 &Bounce : vBounces)
			Graphics()->DrawCircle(Bounce.x, Bounce.y, 2.2f * Scale, 12);
	}
	if(g_Config.m_ClCombatPredictEnd)
	{
		Graphics()->SetColor(EndColor.r, EndColor.g, EndColor.b, std::min(1.0f, BaseAlpha + 0.1f));
		Graphics()->DrawCircle(End.x, End.y, 3.5f * Scale, 14);
	}
	Graphics()->QuadsEnd();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
}

void CVisuals::SpawnBurst(vec2 Pos, vec2 Dir, float Spread, int Num, float MinSpeed, float MaxSpeed)
{
	const float BaseAngle = std::atan2(Dir.y, Dir.x);
	for(int i = 0; i < Num && m_vBurstFlakes.size() < MAX_BURST_FLAKES; i++)
	{
		const float Angle = BaseAngle + random_float(-Spread, Spread);
		CBurstFlake Flake;
		Flake.m_Pos = Pos;
		Flake.m_Vel = vec2(std::cos(Angle), std::sin(Angle)) * random_float(MinSpeed, MaxSpeed);
		Flake.m_MaxLife = random_float(0.7f, 1.4f);
		Flake.m_Life = Flake.m_MaxLife;
		Flake.m_Size = random_float(1.3f, 3.4f);
		Flake.m_Angle = random_float(0.0f, 2.0f * pi);
		Flake.m_Spin = random_float(-4.0f, 4.0f);
		Flake.m_Type = std::min((int)(random_float() * 3.0f), 2);
		m_vBurstFlakes.push_back(Flake);
	}
}

void CVisuals::SpawnRing(vec2 Pos, float MaxRadius, ColorRGBA Color, bool Flat)
{
	if(m_vRings.size() >= MAX_RINGS)
		return;
	CRing Ring;
	Ring.m_Pos = Pos;
	Ring.m_MaxRadius = MaxRadius;
	Ring.m_Color = Color;
	Ring.m_Flat = Flat;
	Ring.m_MaxLife = 0.45f;
	Ring.m_Life = Ring.m_MaxLife;
	m_vRings.push_back(Ring);
}

void CVisuals::SpawnFlash(vec2 Pos, float Size, float Life)
{
	CFlash Flash;
	Flash.m_Pos = Pos;
	Flash.m_Size = Size;
	Flash.m_MaxLife = Life;
	Flash.m_Life = Life;
	m_vFlashes.push_back(Flash);
}

void CVisuals::RenderAurora(const CScreenRect &View) const
{
	const float Time = Client()->LocalTime();
	constexpr int COLUMNS = 64;
	const float Width = View.Size().x;
	const float Height = View.Size().y;
	const float ColumnWidth = Width / COLUMNS;
	const float Top = View.m_TopLeft.y;
	Graphics()->BlendAdditive();
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	for(int i = 0; i < COLUMNS; i++)
	{
		const float X0 = View.m_TopLeft.x + i * ColumnWidth;
		const float X1 = X0 + ColumnWidth * 1.02f;
		const float U = (float)i / COLUMNS;
		const float Wave = std::sin(U * 7.0f + Time * 0.5f) * 0.5f + std::sin(U * 13.0f - Time * 0.8f) * 0.25f;
		const float Peak = Top + Height * (0.10f + 0.04f * Wave);
		const float Bottom = Peak + Height * (0.13f + 0.05f * Wave);
		// from green over cyan to violet, slowly drifting
		const float Hue = 0.36f + 0.28f * (0.5f + 0.5f * std::sin(U * 4.0f + Time * 0.3f));
		const ColorRGBA Base = color_cast<ColorRGBA>(ColorHSLA(Hue, 1.0f, 0.55f));
		const float Alpha = 0.17f + 0.06f * std::sin(U * 9.0f + Time * 1.3f);
		const ColorRGBA Clear(Base.r, Base.g, Base.b, 0.0f);
		const ColorRGBA Full(Base.r, Base.g, Base.b, Alpha);
		Graphics()->SetColor4(Clear, Clear, Full, Full);
		const IGraphics::CFreeformItem Upper(X0, Top, X1, Top, X0, Peak, X1, Peak);
		Graphics()->QuadsDrawFreeform(&Upper, 1);
		Graphics()->SetColor4(Full, Full, Clear, Clear);
		const IGraphics::CFreeformItem Lower(X0, Peak, X1, Peak, X0, Bottom, X1, Bottom);
		Graphics()->QuadsDrawFreeform(&Lower, 1);
	}
	Graphics()->QuadsEnd();
	Graphics()->BlendNormal();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
}

void CVisuals::SaveOriginalSkin()
{
	str_copy(g_Config.m_ClVisualsOrigSkin, g_Config.m_ClPlayerSkin);
	g_Config.m_ClVisualsOrigCustom = g_Config.m_ClPlayerUseCustomColor;
	g_Config.m_ClVisualsOrigBody = (int)g_Config.m_ClPlayerColorBody;
	g_Config.m_ClVisualsOrigFeet = (int)g_Config.m_ClPlayerColorFeet;
	g_Config.m_ClVisualsOrigSaved = 1;
}

void CVisuals::RestoreSkin()
{
	if(g_Config.m_ClVisualsOrigSaved)
	{
		str_copy(g_Config.m_ClPlayerSkin, g_Config.m_ClVisualsOrigSkin);
		g_Config.m_ClPlayerUseCustomColor = g_Config.m_ClVisualsOrigCustom;
		g_Config.m_ClPlayerColorBody = (unsigned)g_Config.m_ClVisualsOrigBody;
		g_Config.m_ClPlayerColorFeet = (unsigned)g_Config.m_ClVisualsOrigFeet;
		g_Config.m_ClVisualsOrigSaved = 0;
		if(Client()->State() == IClient::STATE_ONLINE)
			GameClient()->SendInfo(false);
	}
}

void CVisuals::UpdateAutoSkin(float DeltaTime)
{
	if(!g_Config.m_ClVisualsAutoSkin)
	{
		// switched off: everything is as it was before
		m_AutoSkinTimer = 0.0f;
		RestoreSkin();
		return;
	}
	if(!g_Config.m_ClVisualsOrigSaved)
		SaveOriginalSkin();
	if(Client()->State() != IClient::STATE_ONLINE)
		return;
	m_AutoSkinTimer += DeltaTime;
	if(m_AutoSkinTimer < (float)g_Config.m_ClVisualsAutoSkinInterval)
		return;
	m_AutoSkinTimer = 0.0f;

	// the next skin of the list (up to five)
	if(g_Config.m_ClVisualsAutoSkinList)
	{
		const char *apSkins[5] = {g_Config.m_ClVisualsAutoSkin1, g_Config.m_ClVisualsAutoSkin2, g_Config.m_ClVisualsAutoSkin3, g_Config.m_ClVisualsAutoSkin4, g_Config.m_ClVisualsAutoSkin5};
		for(int Try = 0; Try < 5; Try++)
		{
			m_AutoSkinIndex = (m_AutoSkinIndex + 1) % 5;
			if(apSkins[m_AutoSkinIndex][0] != '\0')
			{
				str_copy(g_Config.m_ClPlayerSkin, apSkins[m_AutoSkinIndex]);
				break;
			}
		}
	}

	if(g_Config.m_ClVisualsAutoSkinColors == 1)
	{
		// smooth rainbow: the hue moves a bit further with every step
		m_AutoHue = std::fmod(m_AutoHue + 0.07f, 1.0f);
		g_Config.m_ClPlayerUseCustomColor = 1;
		g_Config.m_ClPlayerColorBody = ColorHSLA(m_AutoHue, 1.0f, 0.5f).Pack(false);
		g_Config.m_ClPlayerColorFeet = ColorHSLA(std::fmod(m_AutoHue + 0.5f, 1.0f), 1.0f, 0.5f).Pack(false);
	}
	else if(g_Config.m_ClVisualsAutoSkinColors == 2)
	{
		// random jumps
		g_Config.m_ClPlayerUseCustomColor = 1;
		g_Config.m_ClPlayerColorBody = ColorHSLA(random_float(), random_float(0.6f, 1.0f), random_float(0.3f, 0.7f)).Pack(false);
		g_Config.m_ClPlayerColorFeet = ColorHSLA(random_float(), random_float(0.6f, 1.0f), random_float(0.3f, 0.7f)).Pack(false);
	}
	GameClient()->SendInfo(false);
}

// color of your tee, hook and weapon for the rainbow mode
ColorRGBA CVisuals::MeColor(float Offset) const
{
	const float Speed = g_Config.m_ClRainbowSpeed * 0.12f;
	const float T = (float)std::fmod(Client()->LocalTime() * Speed + Offset, 1.0);
	if(g_Config.m_ClRainbowStyle == 0)
		return color_cast<ColorRGBA>(ColorHSLA(T, 1.0f, 0.6f));
	const ColorRGBA aColors[3] = {
		color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClRainbowColor1)),
		color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClRainbowColor2)),
		color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClRainbowColor3))};
	const float Position = T * 3.0f;
	const int Index = std::min((int)Position, 2);
	if(g_Config.m_ClRainbowStyle == 2)
		return aColors[Index];
	const float Fraction = Position - Index;
	const float Smooth = Fraction * Fraction * (3.0f - 2.0f * Fraction);
	const ColorRGBA &From = aColors[Index];
	const ColorRGBA &To = aColors[(Index + 1) % 3];
	return ColorRGBA(From.r + (To.r - From.r) * Smooth, From.g + (To.g - From.g) * Smooth, From.b + (To.b - From.b) * Smooth, 1.0f);
}

void CVisuals::OnLaserRendered(vec2 From, vec2 To, int StartTick, bool Shotgun)
{
	if(Client()->State() != IClient::STATE_ONLINE)
		return;

	// only react to freshly fired lasers
	const int Age = Client()->PredGameTick(g_Config.m_ClDummy) - StartTick;
	if(Age > 10 || Age < -10)
		return;

	const float Length = distance(From, To);
	if(Length < 1.0f)
		return;
	const vec2 Dir = (To - From) / Length;

	// stardust drifts off the beam in all colors of the rainbow
	if(g_Config.m_ClVisualsLaserSparks && random_float() < 0.5f)
	{
		const vec2 Perp(-Dir.y, Dir.x);
		const vec2 Pos = From + (To - From) * random_float();
		SpawnStar(Pos, Perp * random_float(-45.0f, 45.0f) + Dir * random_float(-15.0f, 15.0f), random_float(0.5f, 0.9f), random_float(7.0f, 13.0f), 0.9f, Rainbow(random_float(0.0f, 0.4f)));
	}

	if(!g_Config.m_ClVisualsSnowWeapons)
		return;

	// every laser is rendered many times, only the first time counts
	for(const CRecentLaser &Recent : m_aRecentLasers)
	{
		if(Recent.m_StartTick == StartTick && distance(Recent.m_To, To) < 1.0f)
			return;
	}
	m_aRecentLasers[m_NextRecentLaser].m_StartTick = StartTick;
	m_aRecentLasers[m_NextRecentLaser].m_To = To;
	m_NextRecentLaser = (m_NextRecentLaser + 1) % NUM_RECENT_LASERS;

	// only when the laser ends in a wall: an RGB ring and a few stars
	if(!Collision()->CheckPoint(To + Dir * 8.0f))
		return;

	const vec2 Hit = To - Dir * 2.0f;
	SpawnRing(Hit, Shotgun ? 38.0f : 30.0f, Rainbow(0.0f), false);
	for(int i = 0; i < (Shotgun ? 5 : 4); i++)
	{
		const float Angle = std::atan2(-Dir.y, -Dir.x) + random_float(-1.1f, 1.1f);
		SpawnStar(Hit, vec2(std::cos(Angle), std::sin(Angle)) * random_float(40.0f, 120.0f), random_float(0.4f, 0.8f), random_float(9.0f, 15.0f), 0.95f, Rainbow(random_float(0.0f, 1.0f)));
	}
	SpawnFlash(Hit, Shotgun ? 34.0f : 26.0f, 0.2f);
}

void CVisuals::OnProjectileRendered(vec2 Pos, vec2 Vel, int Weapon, float Alpha)
{
	if(Client()->State() != IClient::STATE_ONLINE)
		return;
	if(Weapon == WEAPON_GRENADE)
	{
		// a plasma tail in changing RGB colors with the odd star
		if(!g_Config.m_ClVisualsGrenadeTrail)
			return;
		for(int i = 0; i < 2; i++)
			SpawnGlow(Pos + vec2(random_float(-3.0f, 3.0f), random_float(-3.0f, 3.0f)), vec2(random_float(-12.0f, 12.0f), random_float(-12.0f, 12.0f)), random_float(0.35f, 0.6f), random_float(11.0f, 18.0f), -0.8f, 0.8f * Alpha,
				Rainbow(random_float(0.0f, 0.15f)));
		if(random_float() < 0.15f)
			SpawnStar(Pos, vec2(random_float(-30.0f, 30.0f), random_float(-30.0f, 30.0f)), random_float(0.5f, 0.8f), random_float(8.0f, 13.0f), 0.9f * Alpha, Rainbow(random_float(0.0f, 0.5f)));
	}
	else if(Weapon == WEAPON_GUN || Weapon == WEAPON_SHOTGUN)
	{
		// a thin comet thread that shifts through the RGB colors
		if(!g_Config.m_ClVisualsBulletTrail)
			return;
		SpawnGlow(Pos, vec2(0.0f, 0.0f), random_float(0.25f, 0.4f), random_float(8.0f, 11.0f), -0.7f, 0.9f * Alpha, Rainbow(0.0f));
		if(random_float() < 0.12f)
			SpawnStar(Pos, vec2(random_float(-20.0f, 20.0f), random_float(-20.0f, 20.0f)), random_float(0.4f, 0.7f), random_float(6.0f, 10.0f), 0.9f * Alpha, Rainbow(0.5f));
	}
}

void CVisuals::OnHookRendered(vec2 Pos, vec2 HookPos)
{
	if(!g_Config.m_ClVisualsHookSparks || Client()->State() != IClient::STATE_ONLINE)
		return;
	const float Length = distance(Pos, HookPos);
	if(Length < 1.0f)
		return;
	const vec2 Dir = (HookPos - Pos) / Length;
	const vec2 Perp(-Dir.y, Dir.x);
	// stars run along the astral chain
	if(random_float() < 0.5f)
		SpawnStar(Pos + (HookPos - Pos) * random_float(), Perp * random_float(-25.0f, 25.0f), random_float(0.3f, 0.6f), random_float(7.0f, 12.0f), 0.85f, Rainbow(random_float(0.0f, 0.5f)));
	// and the head of the hook glows
	SpawnGlow(HookPos, vec2(0.0f, 0.0f), 0.12f, 20.0f, -0.5f, 0.6f, Rainbow(0.0f));
}

void CVisuals::OnKill(int Killer, int Victim)
{
	const int Effect = g_Config.m_ClVisualsKillEffect;
	if(Effect == 0 || Client()->State() != IClient::STATE_ONLINE || !in_range(Victim, MAX_CLIENTS - 1))
		return;
	if(g_Config.m_ClVisualsKillOwnOnly && (Killer < 0 || (Killer != GameClient()->m_aLocalIds[0] && Killer != GameClient()->m_aLocalIds[1])))
		return;
	const auto &Info = GameClient()->m_aClients[Victim];
	if(!Info.m_Active)
		return;
	const vec2 Pos = Info.m_RenderPos;

	if(Effect == 1)
	{
		// astral burst: two RGB rings and a circle of spinning stars
		SpawnRing(Pos, 90.0f, Rainbow(0.0f), false);
		SpawnRing(Pos, 55.0f, Rainbow(0.5f), false);
		for(int i = 0; i < 16; i++)
		{
			const float Angle = 2.0f * pi * i / 16.0f + random_float(-0.1f, 0.1f);
			SpawnStar(Pos, vec2(std::cos(Angle), std::sin(Angle)) * random_float(120.0f, 260.0f), random_float(0.8f, 1.2f), random_float(14.0f, 24.0f), 0.95f, Rainbow(i / 16.0f));
		}
		SpawnFlash(Pos, 100.0f, 0.3f);
	}
	else if(Effect == 2)
	{
		// supernova: a blinding flash, three shock waves and a cloud of hot sparks
		SpawnFlash(Pos, 190.0f, 0.45f);
		SpawnRing(Pos, 110.0f, ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), false);
		SpawnRing(Pos, 160.0f, ColorRGBA(1.0f, 0.8f, 0.4f, 1.0f), false);
		SpawnRing(Pos, 220.0f, ColorRGBA(0.4f, 0.6f, 1.0f, 1.0f), false);
		for(int i = 0; i < 30; i++)
		{
			const float Angle = random_float(0.0f, 2.0f * pi);
			const float Mix = random_float();
			SpawnGlow(Pos, vec2(std::cos(Angle), std::sin(Angle)) * random_float(150.0f, 420.0f), random_float(0.6f, 1.2f), random_float(12.0f, 22.0f), -0.6f, 0.95f,
				ColorRGBA(1.0f - 0.6f * Mix, 0.9f - 0.3f * Mix, 0.6f + 0.4f * Mix, 1.0f));
		}
		for(int i = 0; i < 8; i++)
		{
			const float Angle = random_float(0.0f, 2.0f * pi);
			SpawnStar(Pos, vec2(std::cos(Angle), std::sin(Angle)) * random_float(60.0f, 200.0f), random_float(0.8f, 1.3f), random_float(22.0f, 34.0f), 0.95f, ColorRGBA(1.0f, 0.95f, 0.8f, 1.0f));
		}
	}
	else if(Effect == 3)
	{
		CBlackHole Hole;
		Hole.m_Pos = Pos;
		Hole.m_MaxLife = 1.4f;
		Hole.m_Life = Hole.m_MaxLife;
		Hole.m_Hue = random_float();
		m_vBlackHoles.push_back(Hole);
	}
	else if(Effect == 4 && m_vSouls.size() < MAX_SOULS)
	{
		ColorRGBA Color(1.0f, 1.0f, 1.0f, 1.0f);
		if(Info.m_RenderInfo.m_CustomColoredSkin)
			Color = Lighten(Info.m_RenderInfo.m_ColorBody, 0.6f);
		CSoul Soul;
		Soul.m_Pos = Pos;
		Soul.m_MaxLife = 2.4f;
		Soul.m_Life = Soul.m_MaxLife;
		Soul.m_Size = 56.0f;
		Soul.m_Phase = random_float(0.0f, 2.0f * pi);
		Soul.m_Color = Color;
		m_vSouls.push_back(Soul);
	}
}

void CVisuals::OnExplosion(vec2 Pos)
{
	if(!g_Config.m_ClVisualsSnowWeapons || Client()->State() != IClient::STATE_ONLINE)
		return;
	// RGB rings and stars flying out of the explosion
	SpawnRing(Pos, 110.0f, Rainbow(0.0f), false);
	SpawnRing(Pos, 70.0f, Rainbow(0.5f), false);
	for(int i = 0; i < 14; i++)
	{
		const float Angle = random_float(0.0f, 2.0f * pi);
		SpawnStar(Pos, vec2(std::cos(Angle), std::sin(Angle)) * random_float(90.0f, 260.0f), random_float(0.5f, 1.0f), random_float(10.0f, 18.0f), 0.95f, Rainbow(random_float(0.0f, 1.0f)));
	}
	SpawnFlash(Pos, 110.0f, 0.3f);
}
