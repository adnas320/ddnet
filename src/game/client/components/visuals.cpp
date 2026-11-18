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

constexpr float HALO_WIDTH = 0.84f;
constexpr float HALO_HEIGHT = 0.42f;
constexpr float HALO_Y = -0.72f;
constexpr float HALO_Y_WITH_HAT = -0.96f;

// the snow field is one tile in world coordinates that repeats over the whole map
constexpr float SNOW_TILE_WIDTH = 1400.0f;
constexpr float SNOW_TILE_HEIGHT = 900.0f;

constexpr float PETAL_TILE_WIDTH = 1300.0f;
constexpr float PETAL_TILE_HEIGHT = 900.0f;
constexpr float BUBBLE_TILE_WIDTH = 1200.0f;
constexpr float BUBBLE_TILE_HEIGHT = 900.0f;
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
	m_HaloTexture = Graphics()->LoadTexture("visuals/halo.png", IStorage::TYPE_ALL);
	m_GlowTexture = Graphics()->LoadTexture("visuals/glow.png", IStorage::TYPE_ALL);
}

void CVisuals::OnReset()
{
	m_vBurstFlakes.clear();
	m_vFlashes.clear();
	m_vSparkles.clear();
	m_vRings.clear();
	m_vHearts.clear();
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

void CVisuals::DrawOrbs(vec2 BodyPos, float TeeSize, float Alpha, ColorRGBA Color, bool Front) const
{
	if(!g_Config.m_ClVisualsOrbs)
		return;
	const float Time = Client()->LocalTime();
	const ColorRGBA Light = Lighten(Color, 0.4f);
	Graphics()->BlendAdditive();
	for(int i = 0; i < 3; i++)
	{
		const float Angle = Time * 2.2f + i * 2.0f * pi / 3.0f;
		const float Depth = std::sin(Angle); // positive: in front of the tee
		if((Depth >= 0.0f) != Front)
			continue;
		const vec2 Pos = BodyPos + vec2(Depth * 0.38f, std::cos(Angle) * 0.95f) * TeeSize;
		const float Scale = 0.85f + 0.2f * Depth;
		DrawSprite(m_GlowTexture, Pos, 0.4f * TeeSize * Scale, 0.4f * TeeSize * Scale, 0.0f, false, ColorRGBA(Light.r, Light.g, Light.b, 0.9f * Alpha));
		DrawSprite(m_GlowTexture, Pos, 0.14f * TeeSize * Scale, 0.14f * TeeSize * Scale, 0.0f, false, ColorRGBA(1.0f, 1.0f, 1.0f, Alpha));
	}
	Graphics()->BlendNormal();
}

void CVisuals::RenderBehindTee(vec2 BodyPos, float TeeSize, float Alpha, ColorRGBA TeeColor)
{
	const float Time = Client()->LocalTime();
	const float DeltaTime = std::min(Client()->RenderFrameTime(), 0.05f);
	const ColorRGBA Light = Lighten(TeeColor, 0.3f);

	Graphics()->BlendAdditive();

	// soft glow around the tee in the color of its skin that slowly pulses
	if(g_Config.m_ClVisualsAura)
	{
		const float Pulse = 0.85f + 0.15f * std::sin(Time * 2.2f);
		DrawSprite(m_GlowTexture, BodyPos, 2.8f * TeeSize * Pulse, 2.8f * TeeSize * Pulse, 0.0f, false, ColorRGBA(Light.r, Light.g, Light.b, 0.6f * Alpha));
	}

	// sparkle trail and dust puffs, drawn behind the tee
	for(size_t i = 0; i < m_vSparkles.size();)
	{
		CSparkle &Sparkle = m_vSparkles[i];
		Sparkle.m_Life -= DeltaTime;
		if(Sparkle.m_Life <= 0.0f)
		{
			Sparkle = m_vSparkles.back();
			m_vSparkles.pop_back();
			continue;
		}
		Sparkle.m_Pos += Sparkle.m_Vel * DeltaTime;
		const float Remaining = std::clamp(Sparkle.m_Life / Sparkle.m_MaxLife, 0.0f, 1.0f);
		ColorRGBA Color = Sparkle.m_Color;
		Color.a = Remaining * Sparkle.m_AlphaScale;
		const float Size = Sparkle.m_Size * std::max(0.2f, 1.0f + Sparkle.m_Grow * (1.0f - Remaining));
		DrawSprite(m_GlowTexture, Sparkle.m_Pos, Size, Size, 0.0f, false, Color);
		i++;
	}
	Graphics()->BlendNormal();

	DrawOrbs(BodyPos, TeeSize, Alpha, TeeColor, false);
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
	const ColorRGBA Light = Lighten(TeeColor, 0.4f);
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

	// glowing embers rise from the tee
	if(g_Config.m_ClVisualsEmbers)
	{
		m_EmberAccumulator += DeltaTime * 22.0f;
		while(m_EmberAccumulator >= 1.0f)
		{
			m_EmberAccumulator -= 1.0f;
			if(m_vSparkles.size() >= MAX_SPARKLES)
				break;
			CSparkle Ember;
			Ember.m_Pos = BodyPos + vec2(random_float(-0.35f, 0.35f), random_float(-0.2f, 0.3f)) * TeeSize;
			Ember.m_Vel = vec2(random_float(-14.0f, 14.0f), random_float(-60.0f, -30.0f));
			Ember.m_MaxLife = random_float(0.6f, 1.1f);
			Ember.m_Life = Ember.m_MaxLife;
			Ember.m_Size = random_float(0.12f, 0.22f) * TeeSize;
			Ember.m_Grow = -0.7f;
			Ember.m_AlphaScale = 0.9f;
			Ember.m_Color = ColorRGBA(1.0f, random_float(0.35f, 0.65f), 0.1f, 1.0f);
			m_vSparkles.push_back(Ember);
		}
	}
	else
		m_EmberAccumulator = 0.0f;

	// hearts float up above the tee
	if(g_Config.m_ClVisualsHearts)
	{
		m_HeartAccumulator += DeltaTime * 1.4f;
		while(m_HeartAccumulator >= 1.0f)
		{
			m_HeartAccumulator -= 1.0f;
			if(m_vHearts.size() >= MAX_HEARTS)
				break;
			CHeart Heart;
			Heart.m_Pos = BodyPos + vec2(random_float(-0.4f, 0.4f), -0.6f) * TeeSize;
			Heart.m_MaxLife = random_float(1.4f, 2.2f);
			Heart.m_Life = Heart.m_MaxLife;
			Heart.m_Size = random_float(0.28f, 0.42f) * TeeSize;
			Heart.m_SwayPhase = random_float(0.0f, 2.0f * pi);
			Heart.m_Color = random_float() < 0.5f ? ColorRGBA(1.0f, 0.35f, 0.5f, 1.0f) : ColorRGBA(1.0f, 0.6f, 0.75f, 1.0f);
			m_vHearts.push_back(Heart);
		}
	}
	else
		m_HeartAccumulator = 0.0f;

	DrawStars(BodyPos, TeeSize, Alpha);
	DrawOrbs(BodyPos, TeeSize, Alpha, TeeColor, true);

	const bool Hat = g_Config.m_ClVisualsWinterHat;
	const bool Halo = g_Config.m_ClVisualsHalo;
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

	if(Halo)
	{
		ColorRGBA Color = g_Config.m_ClVisualsHaloRainbow ? Rainbow(0.0f) : ColorRGBA(1.0f, 0.82f, 0.20f, 1.0f);
		Color.a = Alpha;
		// the halo floats slowly up and down
		const float Bob = std::sin(Time * 2.5f) * 0.015f;
		const vec2 Center = BodyPos + vec2(0.0f, (Hat ? HALO_Y_WITH_HAT : HALO_Y) + Bob) * TeeSize;
		DrawSprite(m_HaloTexture, Center, HALO_WIDTH * TeeSize, HALO_HEIGHT * TeeSize, 0.0f, false, Color);
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

void CVisuals::DrawHeart(vec2 Center, float Size) const
{
	Graphics()->DrawCircle(Center.x - 0.22f * Size, Center.y - 0.12f * Size, 0.3f * Size, 12);
	Graphics()->DrawCircle(Center.x + 0.22f * Size, Center.y - 0.12f * Size, 0.3f * Size, 12);
	const IGraphics::CFreeformItem Item(
		Center.x - 0.5f * Size, Center.y - 0.02f * Size,
		Center.x + 0.5f * Size, Center.y - 0.02f * Size,
		Center.x, Center.y + 0.58f * Size,
		Center.x, Center.y + 0.58f * Size);
	Graphics()->QuadsDrawFreeform(&Item, 1);
}

void CVisuals::DrawStars(vec2 BodyPos, float TeeSize, float Alpha) const
{
	if(!g_Config.m_ClVisualsStars)
		return;
	const float Time = Client()->LocalTime();
	for(int i = 0; i < 5; i++)
	{
		const float Angle = i * 2.0f * pi / 5.0f + Time * 0.5f;
		const float Radius = TeeSize * (1.25f + 0.1f * std::sin(Time + i));
		const vec2 Pos = BodyPos + vec2(std::cos(Angle), std::sin(Angle) * 0.8f) * Radius;
		const float Twinkle = 0.5f + 0.5f * std::sin(Time * 3.0f + i * 1.9f);
		const float Size = 0.3f * TeeSize * (0.45f + 0.55f * Twinkle);

		Graphics()->BlendAdditive();
		DrawSprite(m_GlowTexture, Pos, Size * 2.2f, Size * 2.2f, 0.0f, false, ColorRGBA(1.0f, 0.9f, 0.5f, 0.6f * Twinkle * Alpha));
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(1.0f, 0.97f, 0.7f, (0.4f + 0.6f * Twinkle) * Alpha);
		// two crossed thin diamonds make a four-pointed star
		const IGraphics::CFreeformItem Horizontal(Pos.x - Size, Pos.y, Pos.x, Pos.y - 0.18f * Size, Pos.x, Pos.y + 0.18f * Size, Pos.x + Size, Pos.y);
		const IGraphics::CFreeformItem Vertical(Pos.x - 0.18f * Size, Pos.y, Pos.x, Pos.y - Size, Pos.x, Pos.y + Size, Pos.x + 0.18f * Size, Pos.y);
		Graphics()->QuadsDrawFreeform(&Horizontal, 1);
		Graphics()->QuadsDrawFreeform(&Vertical, 1);
		Graphics()->QuadsEnd();
		Graphics()->BlendNormal();
	}
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
}

ColorRGBA CVisuals::NameShimmerColor(bool CustomColored, ColorRGBA BodyColor) const
{
	// colors flow around the hue of the skin and the brightness waves up and down
	const ColorHSLA Base = CustomColored ? color_cast<ColorHSLA>(BodyColor) : ColorHSLA(0.12f, 0.8f, 0.6f);
	const float Phase = Client()->LocalTime() * 1.5f;
	const float Hue = std::fmod(Base.h + 0.06f * std::sin(Phase) + 1.0f, 1.0f);
	const float Saturation = std::max(Base.s, 0.6f);
	const float Lightness = 0.72f + 0.16f * std::sin(Phase * 1.3f + 1.0f);
	return color_cast<ColorRGBA>(ColorHSLA(Hue, Saturation, Lightness));
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
		m_vHearts.clear();
		return;
	}

	const bool Snow = g_Config.m_ClVisualsSnow;
	const bool Fireflies = g_Config.m_ClVisualsFireflies;
	const bool Predict = g_Config.m_ClCombatPredict;
	const bool Petals = g_Config.m_ClVisualsPetals;
	const bool Bubbles = g_Config.m_ClVisualsBubbles;
	if(!Snow && !Fireflies && !Predict && !Petals && !Bubbles && m_vBurstFlakes.empty() && m_vFlashes.empty() && m_vRings.empty() && m_vHearts.empty())
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

	// hearts floating up from the tee
	for(size_t i = 0; i < m_vHearts.size();)
	{
		CHeart &Heart = m_vHearts[i];
		Heart.m_Life -= DeltaTime;
		if(Heart.m_Life <= 0.0f)
		{
			Heart = m_vHearts.back();
			m_vHearts.pop_back();
			continue;
		}
		Heart.m_Pos.y -= 28.0f * DeltaTime;
		Heart.m_Pos.x += std::sin(Heart.m_SwayPhase + Heart.m_Life * 3.0f) * 10.0f * DeltaTime;
		const float Remaining = std::clamp(Heart.m_Life / Heart.m_MaxLife, 0.0f, 1.0f);
		Graphics()->SetColor(Heart.m_Color.r, Heart.m_Color.g, Heart.m_Color.b, std::min(1.0f, Remaining * 2.0f) * 0.9f);
		DrawHeart(Heart.m_Pos, Heart.m_Size);
		i++;
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
			const float HalfWidth = 1.6f;
			Graphics()->SetColor(Ring.m_Color.r, Ring.m_Color.g, Ring.m_Color.b, (1.0f - Progress) * 0.85f);
			for(int Segment = 0; Segment < SEGMENTS; Segment++)
			{
				const float A0 = 2.0f * pi * Segment / SEGMENTS;
				const float A1 = 2.0f * pi * (Segment + 1) / SEGMENTS;
				const vec2 Dir0(std::cos(A0), std::sin(A0) * 0.35f);
				const vec2 Dir1(std::cos(A1), std::sin(A1) * 0.35f);
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

void CVisuals::SpawnRing(vec2 Pos, float MaxRadius, ColorRGBA Color)
{
	if(m_vRings.size() >= MAX_RINGS)
		return;
	CRing Ring;
	Ring.m_Pos = Pos;
	Ring.m_MaxRadius = MaxRadius;
	Ring.m_Color = Color;
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

void CVisuals::OnLaserRendered(vec2 From, vec2 To, int StartTick, bool Shotgun)
{
	if(!g_Config.m_ClVisualsSnowWeapons || Client()->State() != IClient::STATE_ONLINE)
		return;

	// only react to freshly fired lasers
	const int Age = Client()->PredGameTick(g_Config.m_ClDummy) - StartTick;
	if(Age > 10 || Age < -10)
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

	const float Length = distance(From, To);
	if(Length < 1.0f)
		return;
	const vec2 Dir = (To - From) / Length;

	// only when the laser ends in a wall
	if(!Collision()->CheckPoint(To + Dir * 8.0f))
		return;

	SpawnBurst(To - Dir * 2.0f, -Dir, 1.2f, Shotgun ? 8 : 5, 30.0f, 90.0f);
	SpawnFlash(To - Dir * 2.0f, Shotgun ? 38.0f : 30.0f, 0.2f);
}

void CVisuals::OnExplosion(vec2 Pos)
{
	if(!g_Config.m_ClVisualsSnowWeapons || Client()->State() != IClient::STATE_ONLINE)
		return;
	SpawnBurst(Pos, vec2(0.0f, -1.0f), pi, 36, 60.0f, 260.0f);
	SpawnFlash(Pos, 110.0f, 0.3f);
}
