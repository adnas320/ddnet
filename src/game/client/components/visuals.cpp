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
constexpr float HAT_CENTER_X = -0.10f;
constexpr float HAT_CENTER_Y = -0.47f;
constexpr float HAT_WIDTH = 1.12f;
constexpr float HAT_ASPECT = 166.0f / 239.0f; // height / width of winter_hat.png

constexpr float HORN_OFFSET_X = 0.22f;
constexpr float HORN_CENTER_Y = -0.55f;
constexpr float HORN_WIDTH = 0.34f;
constexpr float HORN_HEIGHT = 0.51f;
constexpr float HORN_TILT = 0.28f;

constexpr float HALO_WIDTH = 0.72f;
constexpr float HALO_HEIGHT = 0.36f;
constexpr float HALO_Y = -0.78f;
constexpr float HALO_Y_WITH_HAT = -1.04f;

} // namespace

void CVisuals::OnInit()
{
	m_HatTexture = Graphics()->LoadTexture("visuals/winter_hat.png", IStorage::TYPE_ALL);
	m_HaloTexture = Graphics()->LoadTexture("visuals/halo.png", IStorage::TYPE_ALL);
	m_HornTexture = Graphics()->LoadTexture("visuals/horn.png", IStorage::TYPE_ALL);
}

void CVisuals::OnReset()
{
	m_vBurstFlakes.clear();
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

void CVisuals::RenderTeeAccessories(vec2 BodyPos, float TeeSize, float Alpha) const
{
	const bool Horns = g_Config.m_ClVisualsHorns;
	const bool Hat = g_Config.m_ClVisualsWinterHat;
	const bool Halo = g_Config.m_ClVisualsHalo;
	if(!Horns && !Hat && !Halo)
		return;

	// horns, drawn first so that the hat can cover their bases
	if(Horns)
	{
		ColorRGBA Color = g_Config.m_ClVisualsHornsRainbow ? Rainbow(0.5f) : ColorRGBA(0.85f, 0.07f, 0.10f, 1.0f);
		Color.a = Alpha;
		for(int Side = -1; Side <= 1; Side += 2)
		{
			const vec2 Center = BodyPos + vec2(Side * HORN_OFFSET_X, HORN_CENTER_Y) * TeeSize;
			DrawSprite(m_HornTexture, Center, HORN_WIDTH * TeeSize, HORN_HEIGHT * TeeSize, Side * HORN_TILT, Side < 0, Color);
		}
	}

	if(Hat)
	{
		const vec2 Center = BodyPos + vec2(HAT_CENTER_X, HAT_CENTER_Y) * TeeSize;
		DrawSprite(m_HatTexture, Center, HAT_WIDTH * TeeSize, HAT_WIDTH * HAT_ASPECT * TeeSize, 0.0f, false, ColorRGBA(1.0f, 1.0f, 1.0f, Alpha));
	}

	if(Halo)
	{
		ColorRGBA Color = g_Config.m_ClVisualsHaloRainbow ? Rainbow(0.0f) : ColorRGBA(1.0f, 0.82f, 0.20f, 1.0f);
		Color.a = Alpha;
		// the halo floats slowly up and down
		const float Bob = std::sin(Client()->LocalTime() * 2.5f) * 0.015f;
		const vec2 Center = BodyPos + vec2(0.0f, (Hat ? HALO_Y_WITH_HAT : HALO_Y) + Bob) * TeeSize;
		DrawSprite(m_HaloTexture, Center, HALO_WIDTH * TeeSize, HALO_HEIGHT * TeeSize, 0.0f, false, Color);
	}
}

void CVisuals::ResetFlake(CFlake &Flake, const CScreenRect &View, bool Anywhere)
{
	const vec2 Size = View.Size();
	Flake.m_Pos.x = View.m_TopLeft.x + random_float() * Size.x;
	Flake.m_Pos.y = Anywhere ? View.m_TopLeft.y + random_float() * Size.y : View.m_TopLeft.y - random_float(0.0f, 24.0f);
	Flake.m_FallSpeed = random_float(22.0f, 42.0f);
	Flake.m_DriftSpeed = random_float(10.0f, 24.0f); // positive: from left to right
	Flake.m_Size = random_float(1.2f, 3.2f);
	Flake.m_Alpha = random_float(0.18f, 0.45f);
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
		m_FlakesInitialized = false;
		return;
	}

	const bool Snow = g_Config.m_ClVisualsSnow;
	if(!Snow && m_vBurstFlakes.empty())
	{
		m_FlakesInitialized = false;
		return;
	}

	// The game world is still mapped to the screen here, so everything below is drawn in world coordinates.
	// This makes the snow part of the map instead of being attached to the screen.
	const CScreenRect View = Graphics()->GetScreen();
	const float DeltaTime = std::min(Client()->RenderFrameTime(), 0.1f);

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
		const float Margin = 24.0f;
		const vec2 WrapMin = View.m_TopLeft - vec2(Margin, Margin);
		const vec2 WrapSize = View.Size() + vec2(2.0f * Margin, 2.0f * Margin);
		const auto Wrap = [](float Value, float Min, float Size) {
			return Min + std::fmod(std::fmod(Value - Min, Size) + Size, Size);
		};

		for(CFlake &Flake : m_aFlakes)
		{
			Flake.m_SwayPhase += Flake.m_SwayFrequency * DeltaTime;
			Flake.m_Angle += Flake.m_Spin * DeltaTime;
			Flake.m_Pos.x += (Flake.m_DriftSpeed + std::sin(Flake.m_SwayPhase) * 6.0f) * DeltaTime;
			Flake.m_Pos.y += Flake.m_FallSpeed * DeltaTime;
			Flake.m_Pos.x = Wrap(Flake.m_Pos.x, WrapMin.x, WrapSize.x);
			Flake.m_Pos.y = Wrap(Flake.m_Pos.y, WrapMin.y, WrapSize.y);

			Graphics()->SetColor(1.0f, 1.0f, 1.0f, Flake.m_Alpha);
			DrawFlake(Flake.m_Pos, Flake.m_Size, Flake.m_Angle);
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
}

void CVisuals::OnExplosion(vec2 Pos)
{
	if(!g_Config.m_ClVisualsSnowWeapons || Client()->State() != IClient::STATE_ONLINE)
		return;
	SpawnBurst(Pos, vec2(0.0f, -1.0f), pi, 36, 60.0f, 260.0f);
}
