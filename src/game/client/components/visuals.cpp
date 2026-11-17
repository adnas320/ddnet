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

void CVisuals::RenderBehindTee(vec2 BodyPos, float TeeSize, float Alpha) const
{
	if(!g_Config.m_ClVisualsAura)
		return;
	// soft icy glow around the tee that slowly pulses
	const float Time = Client()->LocalTime();
	const float Pulse = 0.85f + 0.15f * std::sin(Time * 2.2f);
	Graphics()->BlendAdditive();
	DrawSprite(m_GlowTexture, BodyPos, 2.8f * TeeSize * Pulse, 2.8f * TeeSize * Pulse, 0.0f, false, ColorRGBA(0.45f, 0.75f, 1.0f, 0.55f * Alpha));
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

void CVisuals::RenderTeeAccessories(vec2 Position, vec2 BodyPos, float TeeSize, float Alpha, vec2 LookDir, vec2 Vel, bool InAir)
{
	const float Time = Client()->LocalTime();
	const float DeltaTime = std::min(Client()->RenderFrameTime(), 0.05f);
	const vec2 Feet = Position + vec2(0.0f, 14.0f);

	// landing: snow flies up from the ground
	if(InAir)
		m_FallSpeed = m_WasInAir ? std::max(m_FallSpeed, Vel.y) : 0.0f;
	else if(m_WasInAir)
	{
		if(g_Config.m_ClVisualsSnowLanding && m_FallSpeed > 3.5f)
		{
			const float Strength = std::clamp(m_FallSpeed / 14.0f, 0.3f, 1.4f);
			SpawnBurst(Feet, vec2(0.0f, -1.0f), 1.3f, (int)(6 + 14 * Strength), 40.0f + 40.0f * Strength, 90.0f + 90.0f * Strength);
		}
		m_FallSpeed = 0.0f;
	}

	// jump rings: a sudden strong upward push (jump or air jump)
	if(g_Config.m_ClVisualsJumpRings && Vel.y < -9.0f && Vel.y - m_LastTeeVel.y < -2.5f && Time - m_LastTeeTime > 0.25)
	{
		SpawnRing(Feet, 40.0f);
		m_LastTeeTime = Time;
	}
	m_LastTeeVel = Vel;
	m_WasInAir = InAir;

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
			Sparkle.m_Color = Rainbow(random_float(0.0f, 0.12f));
			m_vSparkles.push_back(Sparkle);
		}
	}
	else
		m_TrailAccumulator = 0.0f;

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
	}
	m_FlakesInitialized = true;
}

void CVisuals::DrawFlake(vec2 Center, float Size, float Angle) const
{
	if(Size < 2.0f)
	{
		// small flakes are simple dots
		Graphics()->DrawCircle(Center.x, Center.y, Size * 0.6f, 8);
		return;
	}

	// bigger flakes are six-armed stars made of three crossed bars
	const float Radius = Size * 1.3f;
	const float HalfWidth = Radius * 0.12f;
	for(int Arm = 0; Arm < 3; Arm++)
	{
		const float ArmAngle = Angle + Arm * pi / 3.0f;
		const vec2 Dir = vec2(std::cos(ArmAngle), std::sin(ArmAngle));
		const vec2 Perp = vec2(-Dir.y, Dir.x);
		const IGraphics::CFreeformItem Item(
			Center - Dir * Radius - Perp * HalfWidth,
			Center + Dir * Radius - Perp * HalfWidth,
			Center - Dir * Radius + Perp * HalfWidth,
			Center + Dir * Radius + Perp * HalfWidth);
		Graphics()->QuadsDrawFreeform(&Item, 1);
	}
}

void CVisuals::DrawFrost(const CScreenRect &View) const
{
	const float Time = Client()->LocalTime();
	const vec2 Size = View.Size();
	const float Thickness = Size.y * 0.14f;
	const float Strength = 0.40f + 0.05f * std::sin(Time * 0.8f);
	const ColorRGBA Edge(0.78f, 0.92f, 1.0f, Strength);
	const ColorRGBA Clear(0.78f, 0.92f, 1.0f, 0.0f);
	const float Left = View.m_TopLeft.x;
	const float Top = View.m_TopLeft.y;

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	IGraphics::CQuadItem Item;

	Graphics()->SetColor4(Edge, Edge, Clear, Clear);
	Item = IGraphics::CQuadItem(Left, Top, Size.x, Thickness);
	Graphics()->QuadsDrawTL(&Item, 1);

	Graphics()->SetColor4(Clear, Clear, Edge, Edge);
	Item = IGraphics::CQuadItem(Left, Top + Size.y - Thickness, Size.x, Thickness);
	Graphics()->QuadsDrawTL(&Item, 1);

	Graphics()->SetColor4(Edge, Clear, Edge, Clear);
	Item = IGraphics::CQuadItem(Left, Top, Thickness, Size.y);
	Graphics()->QuadsDrawTL(&Item, 1);

	Graphics()->SetColor4(Clear, Edge, Clear, Edge);
	Item = IGraphics::CQuadItem(Left + Size.x - Thickness, Top, Thickness, Size.y);
	Graphics()->QuadsDrawTL(&Item, 1);

	Graphics()->QuadsEnd();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
}

void CVisuals::DrawGarland(const CScreenRect &View) const
{
	const float Time = Client()->LocalTime();
	const vec2 Size = View.Size();
	const float SwagWidth = Size.y * 0.42f;
	const float Sag = Size.y * 0.045f;
	const float TopY = View.m_TopLeft.y + Size.y * 0.012f;
	const float WireHalf = Size.y * 0.002f;
	const float BulbRadius = Size.y * 0.011f;
	const int NumSwags = (int)std::ceil(Size.x / SwagWidth);
	constexpr int SEGMENTS = 14;
	constexpr int BULBS_PER_SWAG = 4;
	const ColorRGBA aPalette[4] = {
		ColorRGBA(1.0f, 0.20f, 0.20f, 1.0f),
		ColorRGBA(1.0f, 0.80f, 0.20f, 1.0f),
		ColorRGBA(0.30f, 1.0f, 0.40f, 1.0f),
		ColorRGBA(0.30f, 0.60f, 1.0f, 1.0f),
	};

	const auto PointAt = [&](int Swag, float U) {
		return vec2(View.m_TopLeft.x + (Swag + U) * SwagWidth, TopY + Sag * std::sin(U * pi));
	};
	const auto BulbBrightness = [&](int Index) {
		return 0.5f + 0.5f * std::sin(Time * 3.0f + Index * 1.7f);
	};

	// wire and bulbs
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0.08f, 0.30f, 0.12f, 0.95f);
	for(int Swag = 0; Swag < NumSwags; Swag++)
	{
		for(int i = 0; i < SEGMENTS; i++)
		{
			const vec2 A = PointAt(Swag, (float)i / SEGMENTS);
			const vec2 B = PointAt(Swag, (float)(i + 1) / SEGMENTS);
			const IGraphics::CFreeformItem Item(A.x, A.y - WireHalf, B.x, B.y - WireHalf, A.x, A.y + WireHalf, B.x, B.y + WireHalf);
			Graphics()->QuadsDrawFreeform(&Item, 1);
		}
	}
	for(int Swag = 0; Swag < NumSwags; Swag++)
	{
		for(int k = 0; k < BULBS_PER_SWAG; k++)
		{
			const int Index = Swag * BULBS_PER_SWAG + k;
			const vec2 Pos = PointAt(Swag, (k + 0.5f) / BULBS_PER_SWAG) + vec2(0.0f, BulbRadius * 0.9f);
			ColorRGBA Color = aPalette[Index % 4];
			const float Light = 0.45f + 0.55f * BulbBrightness(Index);
			Graphics()->SetColor(Color.r * Light, Color.g * Light, Color.b * Light, 1.0f);
			Graphics()->DrawCircle(Pos.x, Pos.y, BulbRadius, 12);
		}
	}
	Graphics()->QuadsEnd();

	// glow around the bulbs
	Graphics()->BlendAdditive();
	Graphics()->TextureSet(m_GlowTexture);
	Graphics()->QuadsBegin();
	for(int Swag = 0; Swag < NumSwags; Swag++)
	{
		for(int k = 0; k < BULBS_PER_SWAG; k++)
		{
			const int Index = Swag * BULBS_PER_SWAG + k;
			const vec2 Pos = PointAt(Swag, (k + 0.5f) / BULBS_PER_SWAG) + vec2(0.0f, BulbRadius * 0.9f);
			const ColorRGBA Color = aPalette[Index % 4];
			Graphics()->SetColor(Color.r, Color.g, Color.b, 0.55f * BulbBrightness(Index));
			IGraphics::CQuadItem Item(Pos.x, Pos.y, BulbRadius * 7.0f, BulbRadius * 7.0f);
			Graphics()->QuadsDraw(&Item, 1);
		}
	}
	Graphics()->QuadsEnd();
	Graphics()->BlendNormal();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
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
		return;
	}

	const bool Snow = g_Config.m_ClVisualsSnow;
	const bool Frost = g_Config.m_ClVisualsFrost;
	const bool Garland = g_Config.m_ClVisualsGarland;
	if(!Snow && !Frost && !Garland && m_vBurstFlakes.empty() && m_vFlashes.empty() && m_vSparkles.empty() && m_vRings.empty())
		return;

	// The game world is still mapped to the screen here, so everything below is drawn in world coordinates.
	// This makes the snow part of the map instead of being attached to the screen.
	const CScreenRect View = Graphics()->GetScreen();
	const float DeltaTime = std::min(Client()->RenderFrameTime(), 0.1f);
	const double Time = Client()->LocalTime();

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();

	if(Snow)
	{
		if(!m_FlakesInitialized)
			InitFlakes();

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
					DrawFlake(vec2(PosX + TileX * SNOW_TILE_WIDTH, PosY + TileY * SNOW_TILE_HEIGHT), Flake.m_Size * SizeScale, Angle);
			}
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
		DrawFlake(Flake.m_Pos, Flake.m_Size, Flake.m_Angle);
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
			Graphics()->SetColor(0.8f, 0.95f, 1.0f, (1.0f - Progress) * 0.85f);
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

	// glowing things: flashes and the sparkle trail
	if(!m_vFlashes.empty() || !m_vSparkles.empty())
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
			Color.a = Remaining * 0.8f;
			const float Size = Sparkle.m_Size * (0.4f + 0.6f * Remaining);
			DrawSprite(m_GlowTexture, Sparkle.m_Pos, Size, Size, 0.0f, false, Color);
			i++;
		}
		Graphics()->BlendNormal();
	}

	if(Frost)
		DrawFrost(View);
	if(Garland)
		DrawGarland(View);
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
		m_vBurstFlakes.push_back(Flake);
	}
}

void CVisuals::SpawnRing(vec2 Pos, float MaxRadius)
{
	if(m_vRings.size() >= MAX_RINGS)
		return;
	CRing Ring;
	Ring.m_Pos = Pos;
	Ring.m_MaxRadius = MaxRadius;
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
