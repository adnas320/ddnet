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
	m_PetWingTexture = Graphics()->LoadTexture("visuals/pet_dragon_wing.png", IStorage::TYPE_ALL);
	static const char *s_apPetFiles[4] = {"visuals/pet_cat.png", "visuals/pet_dragon.png", "visuals/pet_fox.png", "visuals/pet_jelly.png"};
	for(int i = 0; i < 4; i++)
		m_aPetTextures[i] = Graphics()->LoadTexture(s_apPetFiles[i], IStorage::TYPE_ALL);
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

	// magic circle under the feet: two rotating RGB runes, flat like on the ground
	if(g_Config.m_ClVisualsMagicCircle)
	{
		m_CircleAngle = std::fmod(m_CircleAngle + DeltaTime * 0.8f, 2.0f * pi);
		const vec2 FeetCenter = BodyPos + vec2(0.0f, 0.55f * TeeSize);
		const ColorRGBA Outer = Rainbow(0.0f);
		const ColorRGBA Inner = Rainbow(0.5f);
		DrawFlatCircle(m_CircleTexture, FeetCenter, 1.6f * TeeSize, m_CircleAngle, 0.3f, ColorRGBA(Outer.r, Outer.g, Outer.b, 0.8f * Alpha));
		DrawFlatCircle(m_CircleTexture, FeetCenter, 1.0f * TeeSize, -m_CircleAngle * 1.7f, 0.3f, ColorRGBA(Inner.r, Inner.g, Inner.b, 0.6f * Alpha));
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
		UpdatePet(BodyPos, TeeSize, LookDir, DeltaTime);
		DrawPet(g_Config.m_ClVisualsPet, m_PetPos, 0.95f * TeeSize, m_PetFacing, Alpha, m_PetVel);
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

void CVisuals::UpdatePet(vec2 BodyPos, float TeeSize, vec2 LookDir, float DeltaTime)
{
	if(LookDir.x > 0.2f)
		m_PetFacing = 1.0f;
	else if(LookDir.x < -0.2f)
		m_PetFacing = -1.0f;

	// the pet hovers behind the shoulder of the tee and follows it on a soft spring
	const float Time = Client()->LocalTime();
	const vec2 Target = BodyPos + vec2(-m_PetFacing * 1.4f * TeeSize, -1.0f * TeeSize + std::sin(Time * 2.6f) * 0.1f * TeeSize);
	if(!m_PetInitialized || distance(m_PetPos, Target) > 700.0f)
	{
		m_PetInitialized = true;
		m_PetPos = Target;
		m_PetVel = vec2(0.0f, 0.0f);
		return;
	}
	const int Steps = std::clamp((int)std::ceil(DeltaTime / (1.0f / 120.0f)), 1, 12);
	const float Step = DeltaTime / Steps;
	for(int i = 0; i < Steps; i++)
	{
		m_PetVel += ((Target - m_PetPos) * 45.0f - m_PetVel * 9.0f) * Step;
		m_PetPos += m_PetVel * Step;
	}
}

void CVisuals::DrawPet(int Type, vec2 Center, float Size, float Facing, float Alpha, vec2 Vel) const
{
	const float Time = Client()->LocalTime();
	const int Index = std::clamp(Type, 1, 4) - 1;
	const float Tilt = std::clamp(Vel.x * 0.0012f, -0.25f, 0.25f);
	const ColorRGBA White(1.0f, 1.0f, 1.0f, Alpha);
	// the pet turns its tail away from the tee
	const bool Flip = Facing > 0.0f;

	Graphics()->BlendAdditive();
	DrawSprite(m_GlowTexture, Center, Size * 1.7f, Size * 1.7f, 0.0f, false, ColorRGBA(0.6f, 0.5f, 1.0f, 0.22f * Alpha));
	Graphics()->BlendNormal();

	if(Type == 2)
	{
		// dragon: two wings that flap behind the body
		const float WingSize = Size * 0.8f;
		const float FlapAngle = -0.45f + 0.55f * std::sin(Time * 9.0f);
		for(int Side = -1; Side <= 1; Side += 2)
		{
			const vec2 Attach = Center + vec2(Side * 0.18f * Size, -0.1f * Size);
			const vec2 Dir(Side * std::cos(FlapAngle), std::sin(FlapAngle));
			DrawSprite(m_PetWingTexture, Attach + Dir * 0.42f * WingSize, WingSize, WingSize, Side * FlapAngle, Side < 0, ColorRGBA(1.0f, 1.0f, 1.0f, 0.95f * Alpha));
		}
	}

	if(Type == 4)
	{
		// jellyfish: pulsing bell and waving tentacles
		const float Pulse = std::sin(Time * 2.4f);
		DrawWavy(m_aPetTextures[Index], Center + vec2(0.0f, -Pulse * 0.05f * Size), Size * (1.0f - 0.05f * Pulse), Size * (1.0f + 0.06f * Pulse), Tilt, false, White, 0.09f, 7.0f, Time * 4.0f);
	}
	else
	{
		// breathing and a gentle tilt in the direction of movement
		const float Breath = std::sin(Time * 3.2f);
		DrawSprite(m_aPetTextures[Index], Center + vec2(0.0f, Breath * 0.015f * Size), Size * (1.0f - 0.02f * Breath), Size * (1.0f + 0.03f * Breath), Tilt + std::sin(Time * 1.7f) * 0.03f, Flip, White);
	}
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

void CVisuals::UpdateAutoSkin(float DeltaTime)
{
	if(!g_Config.m_ClVisualsAutoSkin)
	{
		m_AutoSkinTimer = 0.0f;
		return;
	}
	m_AutoSkinTimer += DeltaTime;
	if(m_AutoSkinTimer < (float)g_Config.m_ClVisualsAutoSkinInterval)
		return;
	m_AutoSkinTimer = 0.0f;

	if(g_Config.m_ClVisualsAutoSkin == 2)
	{
		// whole skin: random skin with random colors
		GameClient()->m_Skins.RandomizeSkin(0);
	}
	else
	{
		// only the colors of the current skin
		ColorHSLA Body;
		Body.h = random_float();
		Body.s = random_float(0.6f, 1.0f);
		Body.l = random_float(0.3f, 0.7f);
		ColorHSLA Feet;
		Feet.h = std::fmod(Body.h + random_float(0.25f, 0.75f), 1.0f);
		Feet.s = random_float(0.6f, 1.0f);
		Feet.l = random_float(0.3f, 0.7f);
		g_Config.m_ClPlayerUseCustomColor = 1;
		g_Config.m_ClPlayerColorBody = Body.Pack(false);
		g_Config.m_ClPlayerColorFeet = Feet.Pack(false);
	}
	GameClient()->SendInfo(false);
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
