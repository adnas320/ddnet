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

// horns: one picture with both horns and a red glow, drawn behind the tee
constexpr float HORNS_WIDTH = 1.10f;
constexpr float HORNS_ASPECT = 344.0f / 548.0f; // height / width of horns.png
constexpr float HORNS_CENTER_Y = -0.556f;

constexpr float HALO_WIDTH = 0.84f;
constexpr float HALO_HEIGHT = 0.42f;
constexpr float HALO_Y = -0.72f;
constexpr float HALO_Y_WITH_HAT = -0.96f;

} // namespace

void CVisuals::OnInit()
{
	m_HatTexture = Graphics()->LoadTexture("visuals/winter_hat.png", IStorage::TYPE_ALL);
	m_HaloTexture = Graphics()->LoadTexture("visuals/halo.png", IStorage::TYPE_ALL);
	m_HornsTexture = Graphics()->LoadTexture("visuals/horns.png", IStorage::TYPE_ALL);
	m_GlowTexture = Graphics()->LoadTexture("visuals/glow.png", IStorage::TYPE_ALL);
}

void CVisuals::OnReset()
{
	m_vBurstFlakes.clear();
	m_vFlashes.clear();
	m_FlakesInitialized = false;
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
	if(!g_Config.m_ClVisualsHorns)
		return;
	const vec2 Center = BodyPos + vec2(0.0f, HORNS_CENTER_Y) * TeeSize;
	DrawSprite(m_HornsTexture, Center, HORNS_WIDTH * TeeSize, HORNS_WIDTH * HORNS_ASPECT * TeeSize, 0.0f, false, ColorRGBA(1.0f, 1.0f, 1.0f, Alpha));
}

void CVisuals::RenderTeeAccessories(vec2 BodyPos, float TeeSize, float Alpha, vec2 LookDir, vec2 Vel)
{
	const bool Hat = g_Config.m_ClVisualsWinterHat;
	const bool Halo = g_Config.m_ClVisualsHalo;
	if(!Hat && !Halo)
		return;

	const float Time = Client()->LocalTime();

	if(Hat)
	{
		const float DeltaTime = std::min(Client()->RenderFrameTime(), 0.05f);

		// the hat turns to the side the tee is looking at (with some hysteresis around the middle)
		if(LookDir.x > 0.15f)
			m_HatFlipTarget = 1.0f;
		else if(LookDir.x < -0.15f)
			m_HatFlipTarget = -1.0f;
		m_HatFlip += (m_HatFlipTarget - m_HatFlip) * std::min(1.0f, DeltaTime * 12.0f);

		// the hat hangs on springs: it leans against the movement and bounces when the tee jumps or falls
		const float TargetAngle = std::clamp(-Vel.x * 0.035f, -0.5f, 0.5f) + std::sin(Time * 2.0f) * 0.03f;
		m_HatAngleSpeed += (120.0f * (TargetAngle - m_HatAngle) - 12.0f * m_HatAngleSpeed) * DeltaTime;
		m_HatAngle += m_HatAngleSpeed * DeltaTime;
		const float TargetBob = std::clamp(-Vel.y * 0.008f, -0.06f, 0.06f);
		m_HatBobSpeed += (150.0f * (TargetBob - m_HatBob) - 13.0f * m_HatBobSpeed) * DeltaTime;
		m_HatBob += m_HatBobSpeed * DeltaTime;

		// rotate the hat around its pivot on the head
		const vec2 Pivot(0.0f, HAT_PIVOT_Y);
		const vec2 Offset(HAT_CENTER_X * m_HatFlip, HAT_CENTER_Y - HAT_PIVOT_Y);
		const float SinAngle = std::sin(m_HatAngle);
		const float CosAngle = std::cos(m_HatAngle);
		const vec2 Rotated(Offset.x * CosAngle - Offset.y * SinAngle, Offset.x * SinAngle + Offset.y * CosAngle);
		const vec2 Center = BodyPos + (Pivot + Rotated + vec2(0.0f, m_HatBob)) * TeeSize;
		// while turning around the hat gets thin for a moment
		const float WidthScale = std::max(std::abs(m_HatFlip), 0.08f);
		DrawSprite(m_HatTexture, Center, HAT_WIDTH * WidthScale * TeeSize, HAT_WIDTH * HAT_ASPECT * TeeSize, m_HatAngle, m_HatFlip < 0.0f, ColorRGBA(1.0f, 1.0f, 1.0f, Alpha));
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

void CVisuals::ResetFlake(CFlake &Flake, const CScreenRect &View, bool Anywhere)
{
	// sizes and speeds are given for a view that is 600 units high and get scaled to the real view when drawing
	const vec2 Size = View.Size();
	Flake.m_Pos.x = View.m_TopLeft.x + random_float() * Size.x;
	Flake.m_Pos.y = Anywhere ? View.m_TopLeft.y + random_float() * Size.y : View.m_TopLeft.y - random_float(0.0f, 24.0f);
	Flake.m_FallSpeed = random_float(22.0f, 42.0f);
	Flake.m_DriftSpeed = random_float(10.0f, 24.0f); // positive: from left to right
	Flake.m_Size = random_float(1.6f, 4.4f);
	Flake.m_Alpha = random_float(0.30f, 0.70f);
	Flake.m_Angle = random_float(0.0f, 2.0f * pi);
	Flake.m_Spin = random_float(-1.0f, 1.0f);
	Flake.m_SwayPhase = random_float(0.0f, 2.0f * pi);
	Flake.m_SwayFrequency = random_float(0.5f, 1.5f);
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

void CVisuals::OnRender()
{
	const int State = Client()->State();
	if(State != IClient::STATE_ONLINE && State != IClient::STATE_DEMOPLAYBACK)
	{
		m_vBurstFlakes.clear();
		m_vFlashes.clear();
		m_FlakesInitialized = false;
		return;
	}

	const bool Snow = g_Config.m_ClVisualsSnow;
	if(!Snow && m_vBurstFlakes.empty() && m_vFlashes.empty())
	{
		m_FlakesInitialized = false;
		return;
	}

	// The game world is still mapped to the screen here, so everything below is drawn in world coordinates.
	// This makes the snow part of the map instead of being attached to the screen.
	const CScreenRect View = Graphics()->GetScreen();
	const float DeltaTime = std::min(Client()->RenderFrameTime(), 0.1f);
	const float Scale = View.Size().y / 600.0f;

	// white flashes where weapons hit something
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

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();

	if(Snow)
	{
		if(!m_FlakesInitialized)
		{
			for(CFlake &Flake : m_aFlakes)
				ResetFlake(Flake, View, true);
			m_FlakesInitialized = true;
		}

		// flakes that leave the view on one side come back on the opposite side
		const float Margin = 24.0f * Scale;
		const vec2 WrapMin = View.m_TopLeft - vec2(Margin, Margin);
		const vec2 WrapSize = View.Size() + vec2(2.0f * Margin, 2.0f * Margin);
		const auto Wrap = [](float Value, float Min, float Length) {
			return Min + std::fmod(std::fmod(Value - Min, Length) + Length, Length);
		};

		for(CFlake &Flake : m_aFlakes)
		{
			Flake.m_SwayPhase += Flake.m_SwayFrequency * DeltaTime;
			Flake.m_Angle += Flake.m_Spin * DeltaTime;
			Flake.m_Pos.x += (Flake.m_DriftSpeed + std::sin(Flake.m_SwayPhase) * 6.0f) * Scale * DeltaTime;
			Flake.m_Pos.y += Flake.m_FallSpeed * Scale * DeltaTime;
			Flake.m_Pos.x = Wrap(Flake.m_Pos.x, WrapMin.x, WrapSize.x);
			Flake.m_Pos.y = Wrap(Flake.m_Pos.y, WrapMin.y, WrapSize.y);

			Graphics()->SetColor(1.0f, 1.0f, 1.0f, Flake.m_Alpha);
			DrawFlake(Flake.m_Pos, Flake.m_Size * Scale, Flake.m_Angle);
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
		m_vBurstFlakes.push_back(Flake);
	}
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
