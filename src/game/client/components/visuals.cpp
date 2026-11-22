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
	m_vPixelTees.clear();
	m_vMeteors.clear();
	m_vEchoes.clear();
	m_vCursorTrail.clear();
	m_HavePrevTee = false;
	m_HavePrevMouse = false;
	m_AutoSkinTimer = 0.0f;
	m_vBalloons.clear();
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

	// the weapon prediction is drawn here so that it stays behind the tee
	if(g_Config.m_ClCombatPredict)
		RenderGunPredict(Graphics()->GetScreen());

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

	m_TeePos = Position;
	m_TeePosTime = Time;
	m_SmoothSpeed += (length(Vel) - m_SmoothSpeed) * std::min(1.0f, DeltaTime * 8.0f);

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

bool CVisuals::ShouldShimmer(int ClientId) const
{
	if(!g_Config.m_ClVisualsNameShimmer || ClientId < 0 || ClientId >= MAX_CLIENTS)
		return false;
	const int Mode = g_Config.m_ClVisualsNameMode;
	if(Mode == 0 || ClientId == GameClient()->m_Snap.m_LocalClientId)
		return true;
	if(Mode == 1)
		return false;
	// list of names separated by commas
	const char *pName = GameClient()->m_aClients[ClientId].m_aName;
	const char *pList = g_Config.m_ClVisualsNameList;
	while(*pList)
	{
		while(*pList == ',' || *pList == ' ')
			pList++;
		const char *pEnd = pList;
		while(*pEnd && *pEnd != ',')
			pEnd++;
		const char *pTrim = pEnd;
		while(pTrim > pList && pTrim[-1] == ' ')
			pTrim--;
		const int Length = (int)(pTrim - pList);
		if(Length > 0 && Length == str_length(pName) && str_comp_nocase_num(pName, pList, Length) == 0)
			return true;
		pList = pEnd;
	}
	return false;
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
		m_vPixelTees.clear();
	m_vPixelTees.clear();
		m_vMeteors.clear();
		m_vEchoes.clear();
		m_vCursorTrail.clear();
		m_HavePrevTee = false;
		m_HavePrevMouse = false;
		return;
	}

	UpdateAutoSkin(std::min(Client()->RenderFrameTime(), 0.1f));

	const bool Snow = g_Config.m_ClVisualsSnow;
	const bool Fireflies = g_Config.m_ClVisualsFireflies;
	const bool Predict = false; // drawn behind the tee instead, see RenderBehindTee
	const bool Petals = g_Config.m_ClVisualsPetals;
	const bool Bubbles = g_Config.m_ClVisualsBubbles;
	const bool SpeedFx = g_Config.m_ClVisualsCursorTrail || !m_vCursorTrail.empty();
	if(!SpeedFx && !Snow && !Fireflies && !Predict && !Petals && !Bubbles && m_vBlackHoles.empty() && m_vPixelTees.empty() && m_vBurstFlakes.empty() && m_vFlashes.empty() && m_vRings.empty() && m_vGlows.empty() && m_vSouls.empty())
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

	// tees that turn into big pixels and crumble away
	for(size_t i = 0; i < m_vPixelTees.size();)
	{
		CPixelTee &Tee = m_vPixelTees[i];
		Tee.m_Age += DeltaTime;
		if(Tee.m_Age >= Tee.m_Life)
		{
			Tee = m_vPixelTees.back();
			m_vPixelTees.pop_back();
			continue;
		}
		const float T = Tee.m_Age / Tee.m_Life;
		const auto Hash = [&](int X, int Y, int Salt) {
			unsigned H = Tee.m_Seed ^ (unsigned)(X * 73856093) ^ (unsigned)(Y * 19349663) ^ (unsigned)(Salt * 83492791);
			H ^= H >> 13;
			H *= 0x5bd1e995u;
			H ^= H >> 15;
			return (H & 0xffffu) / 65535.0f;
		};
		// the skin is cut into a grid of small pieces (about 110 for the body, a few for the feet).
		// Every piece shows its own part of the real skin texture and flies away at its own moment.
		constexpr int GRID = 12;
		const float BodySize = Tee.m_Size;
		const vec2 BodyCenter = Tee.m_Pos + vec2(0.0f, -0.06f * BodySize);
		const auto DrawPiece = [&](IGraphics::CTextureHandle Tex, ColorRGBA Color, vec2 Center, float PieceSize, float U0, float V0, float U1, float V1, int X, int Y, int Salt, float Detach, vec2 Local) {
			vec2 Pos = Center;
			float Alpha = 1.0f;
			float Rotation = 0.0f;
			float Size = PieceSize;
			if(T < Detach)
			{
				// the skin starts to tremble and tear before the pieces come off
				const float Shake = std::clamp((T - 0.05f) / 0.4f, 0.0f, 1.0f) * 1.6f;
				Pos += vec2((Hash(X, Y, Salt + 3) - 0.5f) * Shake, (Hash(X, Y, Salt + 4) - 0.5f) * Shake) * std::sin(Tee.m_Age * 45.0f + X * 3.0f + Y);
			}
			else
			{
				const float S = (T - Detach) * Tee.m_Life;
				const vec2 Dir = Local.x == 0.0f && Local.y == 0.0f ? vec2(0.0f, -1.0f) : normalize(Local);
				const float Speed = 40.0f + 150.0f * Hash(X, Y, Salt + 5);
				const vec2 Vel = Dir * Speed + vec2((Hash(X, Y, Salt + 6) - 0.5f) * 60.0f, -40.0f - 80.0f * Hash(X, Y, Salt + 7));
				Pos += Vel * S + vec2(0.0f, 0.5f * 480.0f * S * S);
				Rotation = (Hash(X, Y, Salt + 8) - 0.5f) * 14.0f * S;
				Alpha = std::clamp(1.0f - S / 1.0f, 0.0f, 1.0f);
				Size *= 1.0f - 0.3f * std::clamp(S, 0.0f, 1.0f);
			}
			if(Alpha <= 0.0f)
				return;
			Graphics()->TextureSet(Tex);
			Graphics()->QuadsBegin();
			Graphics()->QuadsSetSubset(U0, V0, U1, V1);
			Graphics()->SetColor(Color.r, Color.g, Color.b, Alpha);
			Graphics()->QuadsSetRotation(Rotation);
			// a tiny overlap hides the seams while the skin is still whole
			const float Draw = T < Detach ? Size * 1.06f : Size;
			IGraphics::CQuadItem Quad(Pos.x, Pos.y, Draw, Draw);
			Graphics()->QuadsDraw(&Quad, 1);
			Graphics()->QuadsSetRotation(0.0f);
			Graphics()->QuadsEnd();
		};
		const float Piece = BodySize / GRID;
		for(int Pass = 0; Pass < 2; Pass++)
		{
			const IGraphics::CTextureHandle Tex = Pass == 0 ? Tee.m_BodyOutlineTex : Tee.m_BodyTex;
			if(!Tex.IsValid())
				continue;
			for(int Y = 0; Y < GRID; Y++)
			{
				for(int X = 0; X < GRID; X++)
				{
					const vec2 Local(((X + 0.5f) / GRID - 0.5f) * BodySize, ((Y + 0.5f) / GRID - 0.5f) * BodySize);
					if(length(Local) > BodySize * 0.5f)
						continue;
					const float Detach = 0.12f + 0.4f * (0.6f * Hash(X, Y, 1) + 0.4f * (Local.y / BodySize + 0.5f));
					const float Shade = Pass == 1 ? 1.0f : 1.0f;
					DrawPiece(Tex, ColorRGBA(Tee.m_Body.r * Shade, Tee.m_Body.g * Shade, Tee.m_Body.b * Shade, 1.0f), BodyCenter + Local, Piece, X / (float)GRID, Y / (float)GRID, (X + 1) / (float)GRID, (Y + 1) / (float)GRID, X, Y, 0, Detach, Local);
				}
			}
		}
		// the two feet: 4x2 pieces each
		for(int Foot = 0; Foot < 2; Foot++)
		{
			for(int Pass = 0; Pass < 2; Pass++)
			{
				const IGraphics::CTextureHandle Tex = Pass == 0 ? Tee.m_FeetOutlineTex : Tee.m_FeetTex;
				if(!Tex.IsValid())
					continue;
				const float FootW = BodySize * 0.5f, FootH = BodySize * 0.25f;
				const vec2 FootCenter = Tee.m_Pos + vec2((Foot ? 1.0f : -1.0f) * 0.1f * BodySize, 0.25f * BodySize);
				for(int Y = 0; Y < 2; Y++)
				{
					for(int X = 0; X < 4; X++)
					{
						const vec2 Local(((X + 0.5f) / 4.0f - 0.5f) * FootW, ((Y + 0.5f) / 2.0f - 0.5f) * FootH);
						DrawPiece(Tex, Tee.m_Feet, FootCenter + Local, FootW / 4.0f, X / 4.0f, Y / 2.0f, (X + 1) / 4.0f, (Y + 1) / 2.0f, 20 + X + Foot * 4, 20 + Y, 40, 0.2f + 0.3f * Hash(X, Y + Foot * 2, 9), Local + vec2(0.0f, FootH));
					}
				}
			}
		}
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		i++;
	}

	// black holes: a black core with a glowing orange disc spins, shrinks, bursts and leaves orange neon behind
	for(size_t i = 0; i < m_vBlackHoles.size();)
	{
		CBlackHole &Hole = m_vBlackHoles[i];
		Hole.m_Life -= DeltaTime;
		if(Hole.m_Life <= 0.0f)
		{
			Hole = m_vBlackHoles.back();
			m_vBlackHoles.pop_back();
			continue;
		}
		const float Age = Hole.m_MaxLife - Hole.m_Life;
		const float P = Age / Hole.m_MaxLife;
		constexpr float EXPLODE = 0.58f;
		const ColorRGBA Orange(1.0f, 0.42f, 0.04f, 1.0f);
		const ColorRGBA Amber(1.0f, 0.78f, 0.25f, 1.0f);

		if(P >= EXPLODE && !Hole.m_Exploded)
		{
			Hole.m_Exploded = true;
			SpawnFlash(Hole.m_Pos, 150.0f, 0.35f);
			SpawnRing(Hole.m_Pos, 130.0f, Orange, false);
			SpawnRing(Hole.m_Pos, 80.0f, Amber, false);
			for(int Spark = 0; Spark < 26; Spark++)
			{
				const float Angle = random_float(0.0f, 2.0f * pi);
				SpawnGlow(Hole.m_Pos, vec2(std::cos(Angle), std::sin(Angle)) * random_float(60.0f, 230.0f), random_float(0.6f, 1.2f), random_float(10.0f, 20.0f), -0.6f, 0.95f, Spark % 2 ? Orange : Amber);
			}
		}

		// size: grows in, shrinks while spinning faster and faster
		float Radius = 0.0f;
		if(P < 0.14f)
		{
			const float G = P / 0.14f;
			Radius = 38.0f * (1.0f - (1.0f - G) * (1.0f - G));
		}
		else if(P < EXPLODE)
		{
			const float G = (P - 0.14f) / (EXPLODE - 0.14f);
			Radius = 38.0f - 27.0f * G * G * (3.0f - 2.0f * G);
		}
		const float Spin = Age * Age * 9.0f + Age * 3.0f;
		// the whole picture turns a little and faster as the hole shrinks
		const float Tilt = -0.18f + 0.35f * std::sin(Age * 1.1f) + 0.12f * Spin;
		const float CosT = std::cos(Tilt), SinT = std::sin(Tilt);
		const auto Rot = [&](vec2 Local) {
			return Hole.m_Pos + vec2(Local.x * CosT - Local.y * SinT, Local.x * SinT + Local.y * CosT);
		};

		if(Radius > 0.5f)
		{
			const ColorRGBA Core(1.0f, 0.95f, 0.82f, 1.0f);
			const ColorRGBA Mid(1.0f, 0.58f, 0.14f, 1.0f);
			const ColorRGBA Deep(0.85f, 0.25f, 0.03f, 1.0f);
			const float Fade = std::clamp(P / 0.1f, 0.0f, 1.0f);
			const auto Mix = [](ColorRGBA A, ColorRGBA B, float T) { return ColorRGBA(A.r + (B.r - A.r) * T, A.g + (B.g - A.g) * T, A.b + (B.b - A.b) * T, 1.0f); };
			const auto WithAlpha = [](ColorRGBA C, float A) { return ColorRGBA(C.r, C.g, C.b, A); };

			Graphics()->BlendAdditive();
			// one soft warm glow behind everything
			DrawSprite(m_GlowTexture, Hole.m_Pos, Radius * 6.0f, Radius * 4.0f, Tilt, false, ColorRGBA(0.9f, 0.32f, 0.05f, 0.22f * Fade));

			Graphics()->TextureClear();
			Graphics()->QuadsBegin();

			// the light of the disc bent over the top of the hole: a wide soft arc
			constexpr int ARC = 56;
			for(int s = 0; s < ARC; s++)
			{
				const float A0 = pi + pi * s / ARC, A1 = pi + pi * (s + 1) / ARC;
				const float E0 = std::pow(std::sin(pi * s / ARC), 0.6f) * 0.55f + 0.35f * std::pow(std::abs(std::cos(pi * s / ARC)), 3.0f);
				const float E1 = std::pow(std::sin(pi * (s + 1) / ARC), 0.6f) * 0.55f + 0.35f * std::pow(std::abs(std::cos(pi * (s + 1) / ARC)), 3.0f);
				const auto P2 = [&](float A, float R) { return Rot(vec2(std::cos(A), std::sin(A)) * R * Radius); };
				Graphics()->SetColor4(WithAlpha(Mid, 0.8f * E0 * Fade), WithAlpha(Deep, 0.0f), WithAlpha(Mid, 0.8f * E1 * Fade), WithAlpha(Deep, 0.0f));
				const IGraphics::CFreeformItem Item(P2(A0, 1.05f), P2(A0, 1.75f), P2(A1, 1.05f), P2(A1, 1.75f));
				Graphics()->QuadsDrawFreeform(&Item, 1);
			}
			// the faint lower arc beneath the disc
			for(int s = 0; s < 32; s++)
			{
				const float A0 = pi * s / 32.0f, A1 = pi * (s + 1) / 32.0f;
				const float E0 = std::sin(A0) * 0.8f + 0.2f, E1 = std::sin(A1) * 0.8f + 0.2f;
				const auto P2 = [&](float A, float R) { return Rot(vec2(std::cos(A) * 1.02f * R, std::sin(A) * 0.78f * R) * Radius); };
				Graphics()->SetColor4(WithAlpha(Mid, 0.45f * E0 * Fade), WithAlpha(Deep, 0.0f), WithAlpha(Mid, 0.45f * E1 * Fade), WithAlpha(Deep, 0.0f));
				const IGraphics::CFreeformItem Item(P2(A0, 1.04f), P2(A0, 1.3f), P2(A1, 1.04f), P2(A1, 1.3f));
				Graphics()->QuadsDrawFreeform(&Item, 1);
			}
			Graphics()->QuadsEnd();

			// the black core covers the middle of all this light
			Graphics()->BlendNormal();
			Graphics()->QuadsBegin();
			Graphics()->SetColor(0.0f, 0.0f, 0.0f, Fade);
			Graphics()->DrawCircle(Hole.m_Pos.x, Hole.m_Pos.y, Radius, 48);
			Graphics()->QuadsEnd();

			Graphics()->BlendAdditive();
			Graphics()->QuadsBegin();
			// thin bright photon ring right at the edge of the core
			constexpr int RING = 64;
			for(int s = 0; s < RING; s++)
			{
				const float A0 = 2.0f * pi * s / RING, A1 = 2.0f * pi * (s + 1) / RING;
				const float B0 = 0.7f + 0.3f * std::sin(A0 - pi * 0.5f - 0.4f), B1 = 0.7f + 0.3f * std::sin(A1 - pi * 0.5f - 0.4f);
				const auto P2 = [&](float A, float R) { return Rot(vec2(std::cos(A), std::sin(A)) * R * Radius); };
				Graphics()->SetColor4(WithAlpha(Mid, 0.0f), WithAlpha(Core, B0 * Fade), WithAlpha(Mid, 0.0f), WithAlpha(Core, B1 * Fade));
				const IGraphics::CFreeformItem Inner(P2(A0, 0.98f), P2(A0, 1.04f), P2(A1, 0.98f), P2(A1, 1.04f));
				Graphics()->QuadsDrawFreeform(&Inner, 1);
				Graphics()->SetColor4(WithAlpha(Core, B0 * Fade), WithAlpha(Mid, 0.0f), WithAlpha(Core, B1 * Fade), WithAlpha(Mid, 0.0f));
				const IGraphics::CFreeformItem Outer(P2(A0, 1.04f), P2(A0, 1.2f), P2(A1, 1.04f), P2(A1, 1.2f));
				Graphics()->QuadsDrawFreeform(&Outer, 1);
			}
			// the flat disc: a long thin lens with a white hot center line, drawn over the core
			constexpr int STRIPS = 56;
			const float Len = Radius * 5.5f;
			for(int s = 0; s < STRIPS; s++)
			{
				const float X0 = (s / (float)STRIPS * 2.0f - 1.0f), X1 = ((s + 1) / (float)STRIPS * 2.0f - 1.0f);
				const auto Env = [&](float X) { return std::pow(std::max(0.0f, 1.0f - std::abs(X)), 0.75f) * (X > 0.0f ? 1.0f : 0.78f); };
				const auto Half = [&](float X) { return Radius * (0.035f + 0.12f * std::pow(std::max(0.0f, 1.0f - std::abs(X)), 1.4f)); };
				const float E0 = Env(X0), E1 = Env(X1);
				const float H0 = Half(X0), H1 = Half(X1);
				const float Y = Radius * 0.2f; // the disc crosses slightly below the middle of the core
				const auto Pt = [&](float X, float Dy) { return Rot(vec2(X * Len, Y + Dy)); };
				const ColorRGBA C0 = Mix(Mid, Core, std::pow(E0, 1.5f)), C1 = Mix(Mid, Core, std::pow(E1, 1.5f));
				// upper and lower halves fade from the white center line to nothing
				Graphics()->SetColor4(WithAlpha(Deep, 0.0f), WithAlpha(C0, E0 * Fade), WithAlpha(Deep, 0.0f), WithAlpha(C1, E1 * Fade));
				const IGraphics::CFreeformItem Up(Pt(X0, -H0 * 2.4f), Pt(X0, 0.0f), Pt(X1, -H1 * 2.4f), Pt(X1, 0.0f));
				Graphics()->QuadsDrawFreeform(&Up, 1);
				Graphics()->SetColor4(WithAlpha(C0, E0 * Fade), WithAlpha(Deep, 0.0f), WithAlpha(C1, E1 * Fade), WithAlpha(Deep, 0.0f));
				const IGraphics::CFreeformItem Down(Pt(X0, 0.0f), Pt(X0, H0 * 2.4f), Pt(X1, 0.0f), Pt(X1, H1 * 2.4f));
				Graphics()->QuadsDrawFreeform(&Down, 1);
			}
			Graphics()->QuadsEnd();

			// a few tiny stars
			for(int Star = 0; Star < 8; Star++)
			{
				const float Angle = Star * 2.4f + Hole.m_Hue * 10.0f;
				const float Dist = Radius * (2.8f + 0.4f * Star);
				const float Twinkle = 0.5f + 0.5f * std::sin(Age * 5.0f + Star * 1.9f);
				DrawSprite(m_StarTexture, Hole.m_Pos + vec2(std::cos(Angle), std::sin(Angle) * 0.7f) * Dist, 5.0f, 5.0f, Angle, false, ColorRGBA(1.0f, 0.92f, 0.75f, 0.55f * Twinkle * Fade));
			}
			Graphics()->BlendNormal();
		}
		else if(P >= EXPLODE)
		{
			// after the burst only the orange neon is left, glowing and fading out
			const float Q = std::clamp((P - EXPLODE) / (1.0f - EXPLODE), 0.0f, 1.0f);
			const float Fade = std::pow(1.0f - Q, 1.4f);
			Graphics()->BlendAdditive();
			DrawSprite(m_GlowTexture, Hole.m_Pos, 150.0f * (0.7f + 0.5f * Q), 150.0f * (0.7f + 0.5f * Q), 0.0f, false, ColorRGBA(1.0f, 0.4f, 0.03f, 0.9f * Fade));
			DrawSprite(m_GlowTexture, Hole.m_Pos, 70.0f * (1.0f + Q), 70.0f * (1.0f + Q), 0.0f, false, ColorRGBA(1.0f, 0.75f, 0.3f, 0.9f * Fade));
			Graphics()->BlendNormal();
		}
		i++;
	}

	// glowing particles of weapon trails, sparks and stars
	if(!m_vGlows.empty())
	{
		Graphics()->BlendAdditive();
		StepGlows(m_vGlows, DeltaTime);
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

	RenderSpeedAndCursor(View, DeltaTime, Time);
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
	const size_t NumSegments = g_Config.m_ClCombatPredictLine ? vPoints.size() - 1 : 0;
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
		// switched off: everything is as it was before. The first change comes right after switching on.
		m_AutoSkinTimer = 1000.0f;
		RestoreSkin();
		return;
	}
	if(!g_Config.m_ClVisualsOrigSaved)
		SaveOriginalSkin();
	if(Client()->State() != IClient::STATE_ONLINE)
		return;
	m_AutoSkinTimer += DeltaTime;
	// servers ignore skin changes that come faster than sv_info_change_delay (5 seconds), so never go below 6
	if(m_AutoSkinTimer < (float)std::max(g_Config.m_ClVisualsAutoSkinInterval, 6))
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

	if(Effect == 1 && m_vPixelTees.size() < 12)
	{
		CPixelTee Tee;
		Tee.m_Pos = Pos;
		Tee.m_Age = 0.0f;
		Tee.m_Life = 2.4f;
		Tee.m_Body = Info.m_RenderInfo.m_CustomColoredSkin ? Info.m_RenderInfo.m_ColorBody : ColorRGBA(0.92f, 0.74f, 0.47f, 1.0f);
		Tee.m_Feet = Info.m_RenderInfo.m_CustomColoredSkin ? Info.m_RenderInfo.m_ColorFeet : ColorRGBA(0.75f, 0.55f, 0.32f, 1.0f);
		Tee.m_Seed = (unsigned)(random_float() * 4294967040.0f);
		// the real skin of the victim is cut into pieces
		const CTeeRenderInfo &Render = Info.m_RenderInfo;
		const CSkin::CSkinTextures &Skin = Render.m_CustomColoredSkin ? Render.m_ColorableRenderSkin : Render.m_OriginalRenderSkin;
		Tee.m_BodyTex = Skin.m_Body;
		Tee.m_BodyOutlineTex = Skin.m_BodyOutline;
		Tee.m_FeetTex = Skin.m_Feet;
		Tee.m_FeetOutlineTex = Skin.m_FeetOutline;
		Tee.m_Body = Render.m_CustomColoredSkin ? Render.m_ColorBody : ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
		Tee.m_Feet = Render.m_CustomColoredSkin ? Render.m_ColorFeet : ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
		Tee.m_Size = Render.m_Size > 8.0f ? Render.m_Size : 64.0f;
		m_vPixelTees.push_back(Tee);
	}
	else if(Effect == 2)
	{
		CBlackHole Hole;
		Hole.m_Pos = Pos;
		Hole.m_MaxLife = 3.0f;
		Hole.m_Life = Hole.m_MaxLife;
		Hole.m_Hue = 0.07f;
		m_vBlackHoles.push_back(Hole);
	}
	else if(Effect == 3 && m_vSouls.size() < MAX_SOULS)
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

// ---------------------------------------------------------------- cursor trail

void CVisuals::RenderSpeedAndCursor(const CScreenRect &View, float DeltaTime, double Time)
{
	const bool Fresh = Time - m_TeePosTime < 0.25;
	const float ViewH = View.Size().y;

	// ---- cursor trail: a white drop-shaped brush stroke that ends exactly at the crosshair
	if(g_Config.m_ClVisualsCursorTrail && Fresh)
	{
		const vec2 Mouse = GameClient()->m_Controls.m_aMousePos[g_Config.m_ClDummy];
		const vec2 Target = GameClient()->m_Controls.m_aTargetPos[g_Config.m_ClDummy];
		if(m_HavePrevMouse && DeltaTime > 0.0f)
		{
			const float ScreenSpeed = length(Mouse - m_PrevMouse) / DeltaTime / ViewH; // screens per second
			const float Wanted = std::clamp((ScreenSpeed - 0.6f) / 2.4f, 0.0f, 1.0f);
			m_TrailPower += (Wanted - m_TrailPower) * std::min(1.0f, DeltaTime * (Wanted > m_TrailPower ? 18.0f : 6.0f));
			const bool Moved = m_vCursorTrail.empty() || distance(m_vCursorTrail.back().m_Pos, Target) > 0.4f;
			if(m_TrailPower > 0.03f && Moved)
				m_vCursorTrail.push_back({Target, (float)Time, m_TrailPower});
		}
		m_PrevMouse = Mouse;
		m_HavePrevMouse = true;
	}
	else
	{
		m_HavePrevMouse = false;
		m_TrailPower = 0.0f;
	}

	const float Fade = 0.4f;
	size_t Dead = 0;
	while(Dead < m_vCursorTrail.size() && Time - m_vCursorTrail[Dead].m_Time > Fade)
		Dead++;
	if(Dead)
		m_vCursorTrail.erase(m_vCursorTrail.begin(), m_vCursorTrail.begin() + Dead);
	if(m_vCursorTrail.size() > 300)
		m_vCursorTrail.erase(m_vCursorTrail.begin(), m_vCursorTrail.begin() + (m_vCursorTrail.size() - 300));

	if(m_vCursorTrail.size() >= 2)
	{
		struct CSample
		{
			vec2 m_Pos;
			float m_Width;
			float m_Alpha;
			bool m_Break;
		};
		std::vector<CSample> vSamples;
		const int N = (int)m_vCursorTrail.size();
		const int Sub = 6;
		const auto WidthOf = [&](float PointTime, float Power) {
			const float Life = 1.0f - std::clamp((float)(Time - PointTime) / Fade, 0.0f, 1.0f);
			return ViewH * (0.0008f + 0.0042f * Power) * Life * Life;
		};
		for(int i = 0; i < N - 1; i++)
		{
			const CTrailPoint &P1 = m_vCursorTrail[i];
			const CTrailPoint &P2 = m_vCursorTrail[i + 1];
			const CTrailPoint &P0 = m_vCursorTrail[std::max(i - 1, 0)];
			const CTrailPoint &P3 = m_vCursorTrail[std::min(i + 2, N - 1)];
			const bool Gap = P2.m_Time - P1.m_Time > 0.1f;
			for(int s = 0; s < Sub; s++)
			{
				const float t = s / (float)Sub;
				const vec2 CatPos = ((P1.m_Pos * 2.0f) + (P2.m_Pos - P0.m_Pos) * t + (P0.m_Pos * 2.0f - P1.m_Pos * 5.0f + P2.m_Pos * 4.0f - P3.m_Pos) * (t * t) + (P1.m_Pos * 3.0f - P0.m_Pos - P2.m_Pos * 3.0f + P3.m_Pos) * (t * t * t)) * 0.5f;
				const float PointTime = mix(P1.m_Time, P2.m_Time, t);
				const float Power = mix(P1.m_Power, P2.m_Power, t);
				const float Life = 1.0f - std::clamp((float)(Time - PointTime) / Fade, 0.0f, 1.0f);
				vSamples.push_back({Gap ? P1.m_Pos : CatPos, WidthOf(PointTime, Power), Life, Gap && s == 0});
			}
		}
		vSamples.push_back({m_vCursorTrail.back().m_Pos, WidthOf(m_vCursorTrail.back().m_Time, m_vCursorTrail.back().m_Power), 1.0f, false});

		// light smoothing of the positions, then shared normals so that the stroke has no seams
		for(int Pass = 0; Pass < 2; Pass++)
			for(size_t k = 1; k + 1 < vSamples.size(); k++)
				if(!vSamples[k].m_Break && !vSamples[k + 1].m_Break)
					vSamples[k].m_Pos = vSamples[k].m_Pos * 0.5f + (vSamples[k - 1].m_Pos + vSamples[k + 1].m_Pos) * 0.25f;
		std::vector<vec2> vNormals(vSamples.size());
		for(size_t k = 0; k < vSamples.size(); k++)
		{
			vec2 Dir = vSamples[std::min(k + 1, vSamples.size() - 1)].m_Pos - vSamples[k > 0 ? k - 1 : 0].m_Pos;
			const float Len = length(Dir);
			Dir = Len > 0.001f ? Dir / Len : vec2(1.0f, 0.0f);
			vNormals[k] = vec2(-Dir.y, Dir.x);
		}

		Graphics()->TextureClear();
		Graphics()->BlendAdditive();
		Graphics()->QuadsBegin();
		const float Scale[3] = {2.2f, 1.2f, 0.55f};
		const float Strength[3] = {0.16f, 0.45f, 1.0f};
		for(int Layer = 0; Layer < 3; Layer++)
		{
			for(size_t k = 0; k + 1 < vSamples.size(); k++)
			{
				if(vSamples[k + 1].m_Break)
					continue;
				const CSample &A = vSamples[k];
				const CSample &B = vSamples[k + 1];
				const vec2 NA = vNormals[k] * A.m_Width * Scale[Layer];
				const vec2 NB = vNormals[k + 1] * B.m_Width * Scale[Layer];
				Graphics()->SetColor4(ColorRGBA(1, 1, 1, Strength[Layer] * A.m_Alpha), ColorRGBA(1, 1, 1, Strength[Layer] * A.m_Alpha), ColorRGBA(1, 1, 1, Strength[Layer] * B.m_Alpha), ColorRGBA(1, 1, 1, Strength[Layer] * B.m_Alpha));
				const IGraphics::CFreeformItem Seg(A.m_Pos.x - NA.x, A.m_Pos.y - NA.y, B.m_Pos.x - NB.x, B.m_Pos.y - NB.y,
					A.m_Pos.x + NA.x, A.m_Pos.y + NA.y, B.m_Pos.x + NB.x, B.m_Pos.y + NB.y);
				Graphics()->QuadsDrawFreeform(&Seg, 1);
			}
			const CSample &Head = vSamples.back();
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, Strength[Layer] * Head.m_Alpha);
			Graphics()->DrawCircle(Head.m_Pos.x, Head.m_Pos.y, Head.m_Width * Scale[Layer], 16);
		}
		Graphics()->QuadsEnd();
		Graphics()->BlendNormal();
	}
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
}
