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
	CheckTeleport(Position, Time);

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
		DrawPanda(m_PetPos, TeeSize, Alpha, m_PetVel);
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
	m_PetCelebrate = 14.0f;
	m_PetSpawnBalloons = true;
	m_AfkTimer = 0.0f;
}

void CVisuals::UpdatePet(vec2 Position, float TeeSize, vec2 LookDir, vec2 Vel, int Jumped, float DeltaTime)
{
	// the owner turns around: the panda rolls over to the other side
	float Desired = m_PetFacing;
	if(LookDir.x > 0.25f)
		Desired = 1.0f;
	else if(LookDir.x < -0.25f)
		Desired = -1.0f;
	if(Desired != m_PetFacing)
	{
		m_PetFacing = Desired;
		m_PetRoll = m_PetSleep > 0.5f ? 1.0f : 0.0f;
	}
	m_PetRoll = std::min(1.0f, m_PetRoll + DeltaTime / 1.1f);

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
	const float SleepTarget = m_AfkTimer > 180.0f ? 1.0f : 0.0f;
	m_PetSleep += std::clamp(SleepTarget - m_PetSleep, -DeltaTime / 0.8f, DeltaTime / 2.0f);
	m_PetWake = std::max(0.0f, m_PetWake - DeltaTime / 0.9f);

	// the panda flies above and behind the owner on a soft spring
	const float Time = Client()->LocalTime();
	const vec2 Target = vec2(Position.x - m_PetFacing * 1.5f * TeeSize, Position.y - (0.85f - 0.15f * m_PetSleep) * TeeSize);
	if(!m_PetInitialized || distance(m_PetPos, Target) > 900.0f)
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
			m_PetVel += ((Target - m_PetPos) * 16.0f - m_PetVel * 7.0f) * Step;
			m_PetPos += m_PetVel * Step;
		}
	}

	// party: balloons fly up next to the panda
	if(m_PetSpawnBalloons)
	{
		m_PetSpawnBalloons = false;
		static const ColorRGBA s_aColors[6] = {ColorRGBA(1.0f, 0.3f, 0.35f, 1.0f), ColorRGBA(1.0f, 0.8f, 0.2f, 1.0f), ColorRGBA(0.3f, 0.85f, 0.4f, 1.0f), ColorRGBA(0.3f, 0.6f, 1.0f, 1.0f), ColorRGBA(0.8f, 0.4f, 1.0f, 1.0f), ColorRGBA(1.0f, 0.55f, 0.8f, 1.0f)};
		for(int i = 0; i < 10; i++)
		{
			CBalloon Balloon;
			Balloon.m_Pos = m_PetPos + vec2(random_float(-1.0f, 1.0f) * 0.9f * TeeSize, -0.4f * TeeSize);
			Balloon.m_Vel = vec2(random_float(-8.0f, 8.0f), -random_float(35.0f, 70.0f));
			Balloon.m_Life = random_float(6.0f, 10.0f);
			Balloon.m_Phase = random_float(0.0f, 6.28f);
			Balloon.m_Color = s_aColors[i % 6];
			m_vBalloons.push_back(Balloon);
		}
		for(int i = 0; i < 18; i++)
			SpawnGlow(m_PetPos + vec2(0.0f, -0.6f * TeeSize), vec2(random_float(-110.0f, 110.0f), -random_float(60.0f, 170.0f)), 1.3f, 6.0f, -0.5f, 0.95f, s_aColors[i % 6], 220.0f);
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

// A chubby pixel art panda that hovers in the sky on four paws and rests on a little cloud.
// Feet is the middle of the paws.
void CVisuals::DrawPanda(vec2 Feet, float Size, float Alpha, vec2 Vel) const
{
	const float Time = Client()->LocalTime();
	const float Sleep = m_PetSleep;
	const bool Asleep = Sleep > 0.5f;
	const bool Celebrating = m_PetCelebrate > 0.0f;
	constexpr float PARTY = 14.0f;
	const float Party = PARTY - m_PetCelebrate; // seconds since the party began
	const float PartyFade = Celebrating ? std::min(1.0f, m_PetCelebrate / 0.8f) : 0.0f;
	const auto Smooth = [](float X) {
		X = std::clamp(X, 0.0f, 1.0f);
		return X * X * (3.0f - 2.0f * X);
	};

	// roll over: eased somersault with a hop; the panda turns like a card while it spins
	const float R = m_PetRoll;
	const float E = R * R * R * (R * (6.0f * R - 15.0f) + 10.0f);
	const bool Rolling = R < 1.0f;
	const float RollAngle = Rolling ? E * 2.0f * pi * m_PetFacing : 0.0f;
	const float ScaleX = Rolling ? -m_PetFacing * std::cos(pi * E) : m_PetFacing;
	const float RollHop = Rolling ? std::sin(pi * E) * 0.5f : 0.0f;
	const float Tuck = Rolling ? std::sin(pi * E) : 0.0f;
	float SquashY = 1.0f;
	if(Rolling && R < 0.12f)
		SquashY -= 0.1f * std::sin(R / 0.12f * pi);
	else if(R > 0.88f && Rolling)
		SquashY -= 0.08f * std::sin((R - 0.88f) / 0.12f * pi);

	// hovering: slow bob, and when celebrating jumps and sways
	const float Bob = std::sin(Time * 2.1f) * 0.025f;
	const float Dance = Celebrating ? Smooth((Party - 1.5f) / 0.5f) * PartyFade : 0.0f;
	const float Jump = Dance * std::fabs(std::sin(Time * 5.0f)) * 0.28f;
	const float WakeJump = m_PetWake > 0.0f ? std::sin(m_PetWake * pi) * 0.2f : 0.0f;
	const float Lift = Bob + RollHop + Jump + WakeJump;
	const float Sway = (Asleep ? 0.0f : std::sin(Time * 1.5f) * 0.05f) + Dance * std::sin(Time * 5.0f) * 0.14f;
	const float Breath = 1.0f + (Asleep ? 0.03f : 0.015f) * std::sin(Time * (Asleep ? 1.4f : 2.4f));

	const float Rot = RollAngle + Sway;
	const vec2 Pivot(0.0f, -0.5f);
	const float CosR = std::cos(Rot), SinR = std::sin(Rot);
	const auto Tr = [&](vec2 P) {
		P.y *= SquashY * Breath;
		vec2 D = P - Pivot;
		D.x *= ScaleX;
		const vec2 Rotated(D.x * CosR - D.y * SinR, D.x * SinR + D.y * CosR);
		const vec2 W = Pivot + Rotated;
		return Feet + vec2(W.x * Size, (W.y - Lift) * Size);
	};

	const auto Shade = [&](ColorRGBA C, float A) { return ColorRGBA(C.r, C.g, C.b, C.a * A); };
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();

	// ---- the little cloud under the paws (world coordinates, it does not roll) ----
	{
		const float CloudBob = -Bob * 0.5f;
		const float Grow = 1.0f + 0.25f * Sleep;
		struct SPuff
		{
			float m_X, m_Y, m_Rx, m_Ry;
		};
		static const SPuff s_aPuffs[6] = {{-0.55f, 0.06f, 0.26f, 0.11f}, {-0.25f, 0.0f, 0.3f, 0.15f}, {0.1f, 0.03f, 0.34f, 0.16f}, {0.45f, 0.06f, 0.28f, 0.12f}, {0.0f, 0.1f, 0.6f, 0.09f}, {0.65f, 0.09f, 0.18f, 0.08f}};
		for(int Layer = 0; Layer < 2; Layer++)
		{
			const ColorRGBA CloudColor = Layer == 0 ? ColorRGBA(0.72f, 0.82f, 0.98f, 0.55f * Alpha) : ColorRGBA(1.0f, 1.0f, 1.0f, 0.85f * Alpha);
			Graphics()->SetColor(CloudColor);
			for(const SPuff &Puff : s_aPuffs)
			{
				const vec2 C = Feet + vec2(Puff.m_X * Grow, Puff.m_Y + CloudBob + (Layer == 0 ? 0.03f : 0.0f)) * Size;
				const float Rx = Puff.m_Rx * Grow * Size * (Layer == 0 ? 1.1f : 1.0f);
				const float Ry = Puff.m_Ry * Size * (Layer == 0 ? 1.1f : 1.0f);
				constexpr int N = 14;
				vec2 aP[N + 1];
				for(int i = 0; i <= N; i++)
					aP[i] = C + vec2(std::cos(2.0f * pi * i / N) * Rx, std::sin(2.0f * pi * i / N) * Ry);
				for(int i = 0; i < N; i++)
				{
					const IGraphics::CFreeformItem Item(C, aP[i], aP[i + 1], aP[i + 1]);
					Graphics()->QuadsDrawFreeform(&Item, 1);
				}
			}
		}
	}

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

	// ---- pixel art ----
	// One pixel is PX units of Size. The picture is 64 x 46 pixels, its origin is the middle of the paws.
	constexpr float PX = 0.024f;
	const auto PixelColor = [&](char C, float Dim, float A) {
		ColorRGBA Color;
		switch(C)
		{
		case 'W': Color = ColorRGBA(0.98f, 0.98f, 0.98f, 1.0f); break;
		case 'w': Color = ColorRGBA(0.87f, 0.89f, 0.92f, 1.0f); break;
		case 'K': Color = ColorRGBA(0.11f, 0.11f, 0.13f, 1.0f); break;
		case 'O': Color = ColorRGBA(0.27f, 0.28f, 0.34f, 1.0f); break;
		case 'P': Color = ColorRGBA(1.0f, 0.59f, 0.67f, 1.0f); break;
		case 'E': Color = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f); break;
		case 'N': Color = ColorRGBA(0.05f, 0.05f, 0.06f, 1.0f); break;
		case 'G': Color = ColorRGBA(0.31f, 0.31f, 0.36f, 1.0f); break;
		case 'R': Color = ColorRGBA(1.0f, 0.39f, 0.55f, 1.0f); break;
		case 'Y': Color = ColorRGBA(1.0f, 0.86f, 0.31f, 1.0f); break;
		default: Color = ColorRGBA(0.39f, 0.75f, 1.0f, 1.0f); break;
		}
		return ColorRGBA(Color.r * Dim, Color.g * Dim, Color.b * Dim, A * Alpha);
	};
	const auto Pixels = [&](const char *const *ppRows, int NumRows, int Width, float Px, float Py, float Angle, vec2 PivotPx, float Dim = 1.0f, float A = 1.0f) {
		const float Ca = std::cos(Angle), Sa = std::sin(Angle);
		const auto ToLocal = [&](float X, float Y) {
			const float Dx = X - PivotPx.x, Dy = Y - PivotPx.y;
			const float Rx = PivotPx.x + Dx * Ca - Dy * Sa, Ry = PivotPx.y + Dx * Sa + Dy * Ca;
			return Tr(vec2((Rx - 32.0f) * PX, (Ry - 46.0f) * PX));
		};
		for(int Row = 0; Row < NumRows; Row++)
		{
			for(int Col = 0; Col < Width;)
			{
				const char C = ppRows[Row][Col];
				if(C == '.')
				{
					Col++;
					continue;
				}
				int End = Col + 1;
				while(End < Width && ppRows[Row][End] == C)
					End++;
				Graphics()->SetColor(PixelColor(C, Dim, A));
				const float X0 = Px + Col, X1 = Px + End, Y0 = Py + Row, Y1 = Py + Row + 1.0f;
				const IGraphics::CFreeformItem Item(ToLocal(X0, Y0), ToLocal(X1, Y0), ToLocal(X0, Y1), ToLocal(X1, Y1));
				Graphics()->QuadsDrawFreeform(&Item, 1);
				Col = End;
			}
		}
	};

	static const char *const s_apHeadOpen[30] = {
		"....OOOOOO..............OOOOOO....",
		"...OKKKKKKO............OKKKKKKO...",
		"..OKKKKKKKKO..........OKKKKKKKKO..",
		".OKKKKKKKKKKOOOOOOOOOOKKKKKKKKKKO.",
		".OKKKKGGKKKWWWWWWWWWWWWKKKGGKKKKO.",
		".OKKKGGGGWWWWWWWWWWWWWWWWGGGGKKKO.",
		".OKKKGGWWWWWWWWWWWWWWWWWWWWGGKKKO.",
		".OKKKGWWWWWWWWWWWWWWWWWWWWWWGKKKO.",
		".OKKKWWWWWWWWWWWWWWWWWWWWWWWWKKKO.",
		"..OKWWWWWWKWWWWWWWWWWWWKWWWWWWKO..",
		"..OWWWWWKKKKKWWWWWWWWKKKKKWWWWWO..",
		"..OWWWWKKKKKKKWWWWWWKKKKKKKWWWWO..",
		".OWWWWKKKKKKKKWWWWWWKKKKKKKKWWWWO.",
		".OWWWKKKEEKKKKWWWWWWKKKEEKKKKWWWO.",
		".OWWWKKKEEKKKKWWWWWWKKKEEKKKKWWWO.",
		".OWWWKKKEKKKKKWWWWWWKKKEKKKKKWWWO.",
		".OWWWKKKKKKKKKWWWWWWKKKKKKKKKWWWO.",
		".OWWWKKKKKKKKWWWWWWWWKKKKKKKKWWWO.",
		".OWWWKKKKKKKWWWWWWWWWWKKKKKKKWWWO.",
		".OWWWWKKKKKWWWWNNNNWWWWKKKKKWWWWO.",
		".OWPPPPPKWWwwwNNNNNNwwwWWKPPPPPWO.",
		".OWPPPPPwwwwwwNNNNNNwwwwwwPPPPPWO.",
		".OWPPPPPwwwwwwwNNNNwwwwwwwPPPPPWO.",
		".OWPPPPPwwwwwwwNwNwNwwwwwwPPPPPWO.",
		"..OwwwwwwwwwwwwwNwNwwwwwwwwwwwwO..",
		"...OOwwwwwwwwwwwwNwwwwwwwwwwwOO...",
		".....OwwwwwwwwwwwwwwwwwwwwwwO.....",
		"......OOOwwwwwwwwwwwwwwwwOOO......",
		".........OOOOOOOOOOOOOOOO.........",
		".................................."
};
	static const char *const s_apHeadClosed[30] = {
		"....OOOOOO..............OOOOOO....",
		"...OKKKKKKO............OKKKKKKO...",
		"..OKKKKKKKKO..........OKKKKKKKKO..",
		".OKKKKKKKKKKOOOOOOOOOOKKKKKKKKKKO.",
		".OKKKKGGKKKWWWWWWWWWWWWKKKGGKKKKO.",
		".OKKKGGGGWWWWWWWWWWWWWWWWGGGGKKKO.",
		".OKKKGGWWWWWWWWWWWWWWWWWWWWGGKKKO.",
		".OKKKGWWWWWWWWWWWWWWWWWWWWWWGKKKO.",
		".OKKKWWWWWWWWWWWWWWWWWWWWWWWWKKKO.",
		"..OKWWWWWWKWWWWWWWWWWWWKWWWWWWKO..",
		"..OWWWWWKKKKKWWWWWWWWKKKKKWWWWWO..",
		"..OWWWWKKKKKKKWWWWWWKKKKKKKWWWWO..",
		".OWWWWKKKKKKKKWWWWWWKKKKKKKKWWWWO.",
		".OWWWKKKKKKKKKWWWWWWKKKKKKKKKWWWO.",
		".OWWWKKKKKKKKKWWWWWWKKKKKKKKKWWWO.",
		".OWWWKKKwwwKKKWWWWWWKKKwwwKKKWWWO.",
		".OWWWKKKKKKKKKWWWWWWKKKKKKKKKWWWO.",
		".OWWWKKKKKKKKWWWWWWWWKKKKKKKKWWWO.",
		".OWWWKKKKKKKWWWWWWWWWWKKKKKKKWWWO.",
		".OWWWWKKKKKWWWWNNNNWWWWKKKKKWWWWO.",
		".OWPPPPPKWWwwwNNNNNNwwwWWKPPPPPWO.",
		".OWPPPPPwwwwwwNNNNNNwwwwwwPPPPPWO.",
		".OWPPPPPwwwwwwwNNNNwwwwwwwPPPPPWO.",
		".OWPPPPPwwwwwwwNwNwNwwwwwwPPPPPWO.",
		"..OwwwwwwwwwwwwwNwNwwwwwwwwwwwwO..",
		"...OOwwwwwwwwwwwwNwwwwwwwwwwwOO...",
		".....OwwwwwwwwwwwwwwwwwwwwwwO.....",
		"......OOOwwwwwwwwwwwwwwwwOOO......",
		".........OOOOOOOOOOOOOOOO.........",
		".................................."
};
	static const char *const s_apHeadHappy[30] = {
		"....OOOOOO..............OOOOOO....",
		"...OKKKKKKO............OKKKKKKO...",
		"..OKKKKKKKKO..........OKKKKKKKKO..",
		".OKKKKKKKKKKOOOOOOOOOOKKKKKKKKKKO.",
		".OKKKKGGKKKWWWWWWWWWWWWKKKGGKKKKO.",
		".OKKKGGGGWWWWWWWWWWWWWWWWGGGGKKKO.",
		".OKKKGGWWWWWWWWWWWWWWWWWWWWGGKKKO.",
		".OKKKGWWWWWWWWWWWWWWWWWWWWWWGKKKO.",
		".OKKKWWWWWWWWWWWWWWWWWWWWWWWWKKKO.",
		"..OKWWWWWWKWWWWWWWWWWWWKWWWWWWKO..",
		"..OWWWWWKKKKKWWWWWWWWKKKKKWWWWWO..",
		"..OWWWWKKKKKKKWWWWWWKKKKKKKWWWWO..",
		".OWWWWKKKKKKKKWWWWWWKKKKKKKKWWWWO.",
		".OWWWKKKKKKKKKWWWWWWKKKKKKKKKWWWO.",
		".OWWWKKKKKKKKKWWWWWWKKKKKKKKKWWWO.",
		".OWWWKKKKwKKKKWWWWWWKKKKwKKKKWWWO.",
		".OWWWKKKwKwKKKWWWWWWKKKwKwKKKWWWO.",
		".OWWWKKKKKKKKWWWWWWWWKKKKKKKKWWWO.",
		".OWWWKKKKKKKWWWWWWWWWWKKKKKKKWWWO.",
		".OWWWWKKKKKWWWWNNNNWWWWKKKKKWWWWO.",
		".OWPPPPPKWWwwwNNNNNNwwwWWKPPPPPWO.",
		".OWPPPPPwwwwwwNNNNNNwwwwwwPPPPPWO.",
		".OWPPPPPwwwwwwwNNNNwwwwwwwPPPPPWO.",
		".OWPPPPPwwwwwwwwwNwwwwwwwwPPPPPWO.",
		"..OwwwwwwwwwwwwNRRRNwwwwwwwwwwwO..",
		"...OOwwwwwwwwwwNNNNNwwwwwwwwwOO...",
		".....OwwwwwwwwwwwwwwwwwwwwwwO.....",
		"......OOOwwwwwwwwwwwwwwwwOOO......",
		".........OOOOOOOOOOOOOOOO.........",
		".................................."
};
	static const char *const s_apBody[26] = {
		"............................................",
		"................OOOOOOOOOOOO................",
		"............OOOOWWWWWWWWWWWWOOOO............",
		".........OOOWWWWWWWWWWWWWWWWWWKKOOO.........",
		".......OOWWWWWWWWWWWWWWWWWWWWKKKKKKOO.......",
		"......OWWWWWWWWWWWWWWWWWWWWWKKKKKKKKKO......",
		".....OWWWWWWWWWWWWWWWWWWWWWWKKKKKKKKKKO.....",
		"....OWWWWWWWWWWWWWWWWWWWWWWWKKKKKKKKKKWO....",
		"...OWWWWWWWWWWWWWWWWWWWWWWWKKKKKKKKKKKKWO...",
		"..OWWWWWWWWWWWWWWWWWWWWWWWWKKKKKKKKKKKKWWO..",
		"..OWWWWWWWWWWWWWWWWWWWWWWWWKKKKKKKKKKKKWWO..",
		".OWWWWKKKKKKKKWWWWWWWWWWWWWKKKKKKKKKKKKWWWO.",
		".OWWWKKKKKKKKKKWWWWWWWWWWWWKKKKKKKKKKKKWWWO.",
		".OWWKKKKKKKKKKKKWWWWWWWWWWWKKKKKKKKKKKKWWWO.",
		".OWKKKKKKKKKKKKKKWWWWWWWWWWKKKKKKKKKKKKWWWO.",
		"..OKKKKKKKKKKKKKKWWWWWWWWWWKKKKKKKKKKKKWWO..",
		"..OKKKKKKKKKKKKKKWWWWWWWWWWKKKKKKKKKKKKwwO..",
		"...OKKKKKKKKKKKKKwwwwwwwwwwKKKKKKKKKKKKwO...",
		"....OKKKKKKKKKKKKwwwwwwwwwwwKKKKKKKKKKwO....",
		".....OKKKKKKKKKKKwwwwwwwwwwwKKKKKKKKKKO.....",
		"......OKKKKKKKKKKwwwwwwwwwwwKKKKKKKKKO......",
		".......OOKKKKKKKKwwwwwwwwwwwwKKKKKKOO.......",
		".........OOOKKKKwwwwwwwwwwwwwwKKOOO.........",
		"............OOOOwwwwwwwwwwwwOOOO............",
		"................OOOOOOOOOOOO................",
		"............................................"
};
	static const char *const s_apLeg[14] = {
		"...OOOOO...",
		"..OKKKKKO..",
		".OKKKKKKKO.",
		".OKKKKKKKO.",
		"OKKKKKKKKKO",
		"OKKKKKKKKKO",
		"OKKKKKKKKKO",
		"OKKKKKKKKKO",
		"OKKKKKKKKKO",
		"OKKKKKKKKKO",
		"OKGGGGGGGKO",
		"OKGGGGGGGKO",
		"OKKKKKKKKKO",
		".OOKKKKKOO."
};
	static const char *const s_apTail[9] = {
		"...OOO...",
		"..OWWWO..",
		".OWWWWWO.",
		"OWWWWWWWO",
		"OWwwwwwWO",
		"OwwwwwwwO",
		".OwwwwwO.",
		"..OwwwO..",
		"...OOO..."
};
	static const char *const s_apHat[20] = {
		".....OWWWO.....",
		"....OWWYWWO....",
		"....OWWWWWO....",
		".....OWWWO.....",
		".....ORRYO.....",
		".....ORYYO.....",
		"....ORYYYRO....",
		"....OYYYRRO....",
		"....OYYRRRO....",
		"...OYYRRRYYO...",
		"...OYRRRYYYO...",
		"...ORRRYYYRO...",
		"..ORRRYYYRRRO..",
		"..ORRYYYRRRYO..",
		".ORRYYYRRRYYYO.",
		".ORYYYRRRYYYRO.",
		".OYYYRRRYYYRRO.",
		"OBBBBBBBBBBBBBO",
		"OBBBBBBBBBBBBBO",
		".OOOOOOOOOOOOO."
};
	static const char *const s_apLie[22] = {
		"..............................................",
		"..............OOOOOOOOOOOOOOOOOO..............",
		"..........OOOOWWWWWWWWWWWWWWWWWWOOOO..........",
		"........OOWWWWWWWWWWWWWWWWWWWWWWWWWWOO........",
		"......OOWWWWWWWWWWWWWWWWWWWWWWWWKKKKWWOO......",
		".....OWWWWWWWWWWWWWWWWWWWWWWWWKKKKKKKKWWO.....",
		"...OOWWWWWWWWWWWWWWWWWWWWWWWWWKKKKKKKKWWWOO...",
		"..OWWWWWWWWWWWWWWWWWWWWWWWWWWKKKKKKKKKKWWWWO..",
		"..OWWWWKKKKKKWWWWWWWWWWWWWWWKKKKKKKKKKKKWWWO..",
		".OWWWKKKKKKKKKKWWWWWWWWWWWWWKKKKKKKKKKKKWWWWO.",
		".OWWKKKKKKKKKKKKWWWWWWWWWWWWKKKKKKKKKKKKWWWWO.",
		".OWWKKKKKKKKKKKKWWWWWWWWWWWWKKKKKKKKKKKKWWWWO.",
		".OWKKKKKKKKKKKKKKWWWWWWWWWWWKKKKKKKKKKKKWWWWO.",
		"..OKKKKKKKKKKKKKKWWWWWWWWWWWKKKKKKKKKKKKWWWO..",
		"..OKKKKKKKKKKKKKKWWWWWWWWWWWKKKKKKKKKKKKwwwO..",
		"...OOKKKKKKKKKKKKWWWWWWWWWWWKKKKKKKKKKKKwOO...",
		".....OKKKKKKKKKKWWWWWWWWWWWWKKKKKKKKKKKKO.....",
		"......OOKKKKKKKKWWWWWWWWWWWWKKKKKKKKKKOO......",
		"........OOKKKKKwwwwwwwwwwwwwwKKKKKKKOO........",
		"..........OOOOwwwwwwwwwwwwwwwwKKOOOO..........",
		"..............OOOOOOOOOOOOOOOOOO..............",
		".............................................."
};

	const bool Blink = std::fmod(Time + 1.3f, 3.5f) < 0.12f;
	const bool Happy = Celebrating && Party > 0.3f;
	const char *const *ppHead = (Asleep || Blink) ? s_apHeadClosed : (Happy ? s_apHeadHappy : s_apHeadOpen);

	// head: nods, looks around, bounces along with the dance
	const float Nod = Asleep ? 1.5f + std::sin(Time * 1.4f) * 0.5f : std::sin(Time * 2.0f + 0.8f) * 0.8f;
	const float Tilt = Asleep ? 0.14f : std::sin(Time * 1.3f) * 0.06f + Dance * std::sin(Time * 5.0f + 1.0f) * 0.14f;

	if(!Asleep)
	{
		// legs hang down and paddle a little, curling up while rolling
		const float Speed = std::min(1.0f, length(Vel) / 160.0f);
		const auto LegAngle = [&](float Phase) {
			return std::sin(Time * 3.0f + Phase) * (0.12f + 0.25f * Speed) - Tuck * 1.1f;
		};
		// party: the paws wave, one of them puts the hat on first
		const float HoldRaise = Celebrating ? Smooth(Party / 0.45f) * (1.0f - Smooth((Party - 1.3f) / 0.35f)) : 0.0f;
		const float DanceRaise = Dance;
		const float NearFront = -2.45f * HoldRaise + (-1.9f + std::sin(Time * 6.0f) * 0.55f) * DanceRaise + (1.0f - HoldRaise) * (1.0f - DanceRaise) * LegAngle(0.0f);
		const float FarFront = (-1.9f + std::sin(Time * 6.0f + pi) * 0.55f) * DanceRaise + (1.0f - DanceRaise) * LegAngle(pi);

		Pixels(s_apLeg, 14, 11, 11.0f, 32.0f, LegAngle(pi * 0.5f), vec2(16.5f, 34.0f), 0.78f);
		Pixels(s_apLeg, 14, 11, 33.0f, 32.0f, FarFront, vec2(38.5f, 34.0f), 0.78f);
		Pixels(s_apTail, 9, 9, 0.0f, 24.0f, std::sin(Time * 4.0f) * (Celebrating ? 0.35f : 0.12f), vec2(5.0f, 28.0f));
		Pixels(s_apBody, 26, 44, 3.0f, 16.0f, 0.0f, vec2(0.0f, 0.0f));
		Pixels(s_apLeg, 14, 11, 6.0f, 32.0f, LegAngle(0.0f), vec2(11.5f, 34.0f));
		Pixels(s_apLeg, 14, 11, 29.0f, 32.0f, NearFront, vec2(34.5f, 34.0f));
		Pixels(ppHead, 30, 34, 29.0f, 0.0f + Nod, Tilt, vec2(46.0f, 26.0f));

		// the party hat: first held in the paw, then set on the head
		if(Celebrating)
		{
			const float Appear = Smooth(Party / 0.25f) * PartyFade;
			const float Move = Smooth((Party - 0.45f) / 0.7f);
			const vec2 Tip = vec2(34.5f, 34.0f) + vec2(-11.0f * std::sin(NearFront), 11.0f * std::cos(NearFront));
			const vec2 HeadSpot(37.0f, -14.0f + Nod);
			const vec2 Held = Tip - vec2(7.5f, 19.0f);
			const vec2 Pos = Held + (HeadSpot - Held) * Move;
			Pixels(s_apHat, 20, 15, Pos.x, Pos.y, Tilt * Move, vec2(46.0f, 26.0f), 1.0f, Appear);
		}
	}
	else
	{
		// asleep: lying curled up on the cloud with the head on the front paws
		const float Breathe = std::sin(Time * 1.4f) * 0.4f;
		Pixels(s_apTail, 9, 9, 1.0f, 33.0f, 0.0f, vec2(5.0f, 38.0f));
		Pixels(s_apLie, 22, 46, 5.0f, 24.0f - Breathe, 0.0f, vec2(0.0f, 0.0f));
		Pixels(s_apHeadClosed, 30, 34, 31.0f, 13.0f + Nod, Tilt, vec2(48.0f, 38.0f));
		Pixels(s_apLeg, 14, 11, 44.5f, 36.0f, -1.5f, vec2(50.0f, 41.0f));
		Pixels(s_apLeg, 14, 11, 48.5f, 38.0f, -1.5f, vec2(54.0f, 43.0f));

		// floating Z letters like in a comic
		for(int i = 0; i < 3; i++)
		{
			const float Phase = std::fmod(Time * 0.4f + i / 3.0f, 1.0f);
			const float Scale = (0.5f + Phase * 0.8f) * 0.15f * Sleep;
			const vec2 Pos(0.55f + Phase * 0.35f, -0.85f - Phase * 0.7f);
			const float A = std::sin(Phase * pi) * Sleep * Alpha;
			const ColorRGBA ZColor(0.8f, 0.88f, 1.0f, A);
			Line(Pos + vec2(-Scale, -Scale), Pos + vec2(Scale, -Scale), Scale * 0.35f, ZColor);
			Line(Pos + vec2(Scale, -Scale), Pos + vec2(-Scale, Scale), Scale * 0.35f, ZColor);
			Line(Pos + vec2(-Scale, Scale), Pos + vec2(Scale, Scale), Scale * 0.35f, ZColor);
		}
	}
	(void)Shade;

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
	const bool Starfield = g_Config.m_ClVisualsStarfield;
	const bool Meteors = g_Config.m_ClVisualsMeteors;
	const bool SpeedFx = g_Config.m_ClVisualsSpeedLines || g_Config.m_ClVisualsCursorTrail || !m_vCursorTrail.empty();
	if(!SpeedFx && !Snow && !Fireflies && !Predict && !Petals && !Bubbles && !Starfield && !Meteors && m_vBlackHoles.empty() && m_vPixelTees.empty() && m_vBurstFlakes.empty() && m_vFlashes.empty() && m_vRings.empty() && m_vGlows.empty() && m_vSouls.empty() && m_vMeteors.empty())
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
		const int Cells = T < 0.10f ? 16 : (T < 0.20f ? 10 : (T < 0.30f ? 6 : 4));
		const float CellSize = 64.0f / Cells;
		const auto Hash = [&](int X, int Y, int Salt) {
			unsigned H = Tee.m_Seed ^ (unsigned)(X * 73856093) ^ (unsigned)(Y * 19349663) ^ (unsigned)(Salt * 83492791);
			H ^= H >> 13;
			H *= 0x5bd1e995u;
			H ^= H >> 15;
			return (H & 0xffffu) / 65535.0f;
		};
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		for(int Y = 0; Y < Cells; Y++)
		{
			for(int X = 0; X < Cells; X++)
			{
				const vec2 Local((X + 0.5f) * CellSize - 32.0f, (Y + 0.5f) * CellSize - 32.0f);
				if(length(Local) > 32.0f + CellSize * 0.15f)
					continue;
				const float Detach = 0.3f + 0.35f * (0.65f * Hash(X, Y, 1) + 0.35f * (Local.y + 32.0f) / 64.0f);
				ColorRGBA Color = Local.y > 14.0f ? Tee.m_Feet : Tee.m_Body;
				const float Shade = 0.86f + 0.28f * Hash(X, Y, 2);
				Color = ColorRGBA(std::min(1.0f, Color.r * Shade), std::min(1.0f, Color.g * Shade), std::min(1.0f, Color.b * Shade), 1.0f);
				// the two eyes
				if(Cells >= 6 && std::abs(std::abs(Local.x) - 9.0f) < CellSize * 0.55f && std::abs(Local.y + 4.0f) < CellSize * 0.55f)
					Color = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
				vec2 Pos = Tee.m_Pos + Local;
				float Alpha = 1.0f;
				float Rotation = 0.0f;
				float Size = CellSize;
				if(T < Detach)
				{
					// restless right before it falls apart
					const float Shake = std::clamp((T - 0.15f) / 0.5f, 0.0f, 1.0f) * 1.8f;
					Pos += vec2((Hash(X, Y, 3) - 0.5f) * Shake, (Hash(X, Y, 4) - 0.5f) * Shake) * std::sin(Tee.m_Age * 40.0f + X * 3.0f);
				}
				else
				{
					const float S = (T - Detach) * Tee.m_Life;
					const float Dir = (Local.x >= 0.0f ? 1.0f : -1.0f);
					const vec2 Vel(Local.x * 2.0f + Dir * Hash(X, Y, 5) * 40.0f, -30.0f - 90.0f * Hash(X, Y, 6));
					Pos += Vel * S + vec2(0.0f, 0.5f * 520.0f * S * S);
					Rotation = (Hash(X, Y, 7) - 0.5f) * 10.0f * S;
					Alpha = std::clamp(1.0f - S / 0.75f, 0.0f, 1.0f);
					Size *= 1.0f - 0.35f * std::clamp(S / 0.75f, 0.0f, 1.0f);
				}
				if(Alpha <= 0.0f)
					continue;
				Graphics()->SetColor(Color.r, Color.g, Color.b, Alpha);
				Graphics()->QuadsSetRotation(Rotation);
				IGraphics::CQuadItem Quad(Pos.x, Pos.y, Size, Size);
				Graphics()->QuadsDraw(&Quad, 1);
			}
		}
		Graphics()->QuadsSetRotation(0.0f);
		Graphics()->QuadsEnd();
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
			Radius = 46.0f * (1.0f - (1.0f - G) * (1.0f - G));
		}
		else if(P < EXPLODE)
		{
			const float G = (P - 0.14f) / (EXPLODE - 0.14f);
			Radius = 46.0f - 33.0f * G * G * (3.0f - 2.0f * G);
		}
		const float Spin = Age * Age * 9.0f + Age * 3.0f;
		const float Tilt = -0.35f;

		Graphics()->BlendAdditive();
		if(Radius > 0.5f)
		{
			// big soft halo
			DrawSprite(m_GlowTexture, Hole.m_Pos, Radius * 6.5f, Radius * 6.5f, 0.0f, false, ColorRGBA(1.0f, 0.4f, 0.05f, 0.55f));
			// the accretion disc: a flattened ring with a bright spot that runs around it
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			constexpr int SEGMENTS = 48;
			const auto Point = [&](float Angle, float R) {
				const float X = std::cos(Angle) * R;
				const float Y = std::sin(Angle) * R * 0.36f;
				return Hole.m_Pos + vec2(X * std::cos(Tilt) - Y * std::sin(Tilt), X * std::sin(Tilt) + Y * std::cos(Tilt));
			};
			for(int Seg = 0; Seg < SEGMENTS; Seg++)
			{
				const float A0 = 2.0f * pi * Seg / SEGMENTS;
				const float A1 = 2.0f * pi * (Seg + 1) / SEGMENTS;
				const float B0 = 0.35f + 0.65f * std::pow(0.5f + 0.5f * std::cos(A0 - Spin), 2.0f);
				const float B1 = 0.35f + 0.65f * std::pow(0.5f + 0.5f * std::cos(A1 - Spin), 2.0f);
				const ColorRGBA In0(1.0f, 0.85f, 0.45f, B0), Out0(1.0f, 0.3f, 0.0f, 0.0f);
				const ColorRGBA In1(1.0f, 0.85f, 0.45f, B1), Out1(1.0f, 0.3f, 0.0f, 0.0f);
				Graphics()->SetColor4(In0, Out0, In1, Out1);
				const IGraphics::CFreeformItem Item(Point(A0, Radius * 1.05f), Point(A0, Radius * 2.9f), Point(A1, Radius * 1.05f), Point(A1, Radius * 2.9f));
				Graphics()->QuadsDrawFreeform(&Item, 1);
			}
			Graphics()->QuadsEnd();
			// matter that spirals into the hole
			for(int Star = 0; Star < 14; Star++)
			{
				const float Phase = std::fmod(Age * 0.9f + Star / 14.0f, 1.0f);
				const float Angle = Spin * 1.4f + Star * 2.4f + Phase * 5.0f;
				const float Dist = Radius * (3.2f - 2.1f * Phase);
				const vec2 Off(std::cos(Angle) * Dist, std::sin(Angle) * Dist * 0.5f);
				DrawSprite(m_StarTexture, Hole.m_Pos + Off, 14.0f * (1.0f - Phase * 0.5f), 14.0f * (1.0f - Phase * 0.5f), Angle, false, ColorRGBA(1.0f, 0.62f, 0.15f, 0.9f));
			}
		}
		Graphics()->BlendNormal();
		if(Radius > 0.5f)
		{
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			Graphics()->SetColor(0.0f, 0.0f, 0.0f, 1.0f);
			Graphics()->DrawCircle(Hole.m_Pos.x, Hole.m_Pos.y, Radius, 32);
			Graphics()->QuadsEnd();
			// thin bright photon ring around the core
			Graphics()->BlendAdditive();
			DrawSprite(m_GlowTexture, Hole.m_Pos, Radius * 2.5f, Radius * 2.5f, 0.0f, false, ColorRGBA(1.0f, 0.55f, 0.1f, 0.35f));
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

	// blue meteors: they appear above the screen, glide down smoothly and simply fall out of sight
	{
		const vec2 ViewSize = View.Size();
		if(g_Config.m_ClVisualsMeteors)
		{
			m_MeteorTimer -= DeltaTime;
			if(m_MeteorTimer <= 0.0f && m_vMeteors.size() < MAX_METEORS)
			{
				m_MeteorTimer = random_float(1.2f, 3.2f);
				const float Side = random_float() < 0.5f ? -1.0f : 1.0f;
				CMeteor Meteor;
				Meteor.m_Length = random_float(260.0f, 380.0f) * FieldScale;
				Meteor.m_Size = random_float(5.0f, 7.5f) * FieldScale;
				// start fully outside of the screen so nothing pops into existence
				Meteor.m_Pos = View.m_TopLeft + vec2(random_float(0.0f, 1.2f) * ViewSize.x - (Side > 0.0f ? 0.1f : 0.0f) * ViewSize.x, -Meteor.m_Length * 0.8f - Meteor.m_Size * 8.0f);
				Meteor.m_Vel = normalize(vec2(Side * random_float(0.3f, 0.6f), 1.0f)) * random_float(330.0f, 460.0f) * FieldScale;
				Meteor.m_MaxLife = 1.0f;
				Meteor.m_Life = 1.0f;
				Meteor.m_EmberTimer = 0.0f;
				m_vMeteors.push_back(Meteor);
			}
		}
		if(!m_vMeteors.empty())
		{
			Graphics()->BlendAdditive();
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			constexpr int TAIL_SEGMENTS = 14;
			const float Margin = 120.0f * FieldScale;
			for(size_t i = 0; i < m_vMeteors.size();)
			{
				CMeteor &Meteor = m_vMeteors[i];
				Meteor.m_Pos += Meteor.m_Vel * DeltaTime;
				const vec2 Dir = normalize(Meteor.m_Vel);
				const vec2 Perp(-Dir.y, Dir.x);
				const vec2 Tail = Meteor.m_Pos - Dir * Meteor.m_Length;
				const bool Gone = Tail.y > View.m_BottomRight.y + Margin || (Meteor.m_Pos.x < View.m_TopLeft.x - Meteor.m_Length - Margin) || (Meteor.m_Pos.x > View.m_BottomRight.x + Meteor.m_Length + Margin);
				if(Gone)
				{
					Meteor = m_vMeteors.back();
					m_vMeteors.pop_back();
					continue;
				}

				// glowing dust drifts off the tail
				Meteor.m_EmberTimer -= DeltaTime;
				while(Meteor.m_EmberTimer <= 0.0f)
				{
					Meteor.m_EmberTimer += 0.025f;
					SpawnGlow(Meteor.m_Pos - Dir * random_float(0.0f, Meteor.m_Length * 0.6f) + Perp * random_float(-1.0f, 1.0f) * Meteor.m_Size,
						Perp * random_float(-20.0f, 20.0f), random_float(0.5f, 1.0f), Meteor.m_Size * random_float(1.6f, 3.0f), -0.8f, 0.8f,
						ColorRGBA(0.4f, 0.65f + random_float(0.0f, 0.3f), 1.0f, 1.0f));
				}

				for(int Segment = 0; Segment < TAIL_SEGMENTS; Segment++)
				{
					const float T0 = (float)Segment / TAIL_SEGMENTS;
					const float T1 = (float)(Segment + 1) / TAIL_SEGMENTS;
					const vec2 A = Meteor.m_Pos - Dir * Meteor.m_Length * T0;
					const vec2 B = Meteor.m_Pos - Dir * Meteor.m_Length * T1;
					const float WidthA = Meteor.m_Size * 1.1f * (1.0f - T0 * 0.9f);
					const float WidthB = Meteor.m_Size * 1.1f * (1.0f - T1 * 0.9f);
					const ColorRGBA ColorA(0.8f - 0.6f * T0, 0.92f - 0.5f * T0, 1.0f, std::pow(1.0f - T0, 1.4f));
					const ColorRGBA ColorB(0.8f - 0.6f * T1, 0.92f - 0.5f * T1, 1.0f, std::pow(1.0f - T1, 1.4f));
					Graphics()->SetColor4(ColorA, ColorA, ColorB, ColorB);
					const IGraphics::CFreeformItem Item(A - Perp * WidthA, A + Perp * WidthA, B - Perp * WidthB, B + Perp * WidthB);
					Graphics()->QuadsDrawFreeform(&Item, 1);
				}
				i++;
			}
			Graphics()->QuadsEnd();
			for(const CMeteor &Meteor : m_vMeteors)
			{
				DrawSprite(m_GlowTexture, Meteor.m_Pos, Meteor.m_Size * 14.0f, Meteor.m_Size * 14.0f, 0.0f, false, ColorRGBA(0.25f, 0.5f, 1.0f, 0.9f));
				DrawSprite(m_GlowTexture, Meteor.m_Pos, Meteor.m_Size * 6.0f, Meteor.m_Size * 6.0f, 0.0f, false, ColorRGBA(0.8f, 0.95f, 1.0f, 1.0f));
				DrawSprite(m_StarTexture, Meteor.m_Pos, Meteor.m_Size * 8.0f, Meteor.m_Size * 8.0f, (float)Time * 3.0f, false, ColorRGBA(0.85f, 0.95f, 1.0f, 1.0f));
			}
			Graphics()->BlendNormal();
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		}
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

// ---------------------------------------------------------------- teleport, speed lines, cursor trail

void CVisuals::CheckTeleport(vec2 Position, float Time)
{
	const bool Fx = g_Config.m_ClVisualsTeleFx && Collision();
	if(Fx && m_HavePrevTee && distance(Position, m_PrevTeePos) > 110.0f)
	{
		// a jump: look for a teleporter next to the place where the tee was a moment ago
		float Best = 3.0f * 32.0f;
		bool Found = false;
		vec2 Tile(0.0f, 0.0f);
		const int BaseX = (int)(m_PrevTeePos.x / 32.0f);
		const int BaseY = (int)(m_PrevTeePos.y / 32.0f);
		for(int Y = BaseY - 2; Y <= BaseY + 2; Y++)
		{
			for(int X = BaseX - 2; X <= BaseX + 2; X++)
			{
				const vec2 Center(X * 32.0f + 16.0f, Y * 32.0f + 16.0f);
				const int Index = Collision()->GetPureMapIndex(Center);
				if(Index < 0 || (!Collision()->IsTeleport(Index) && !Collision()->IsEvilTeleport(Index)))
					continue;
				const float Dist = distance(Center, m_PrevTeePos);
				if(Dist < Best)
				{
					Best = Dist;
					Tile = Center;
					Found = true;
				}
			}
		}
		if(Found)
			TeleportEffect(Tile, Position);
	}
	m_PrevTeePos = Position;
	m_HavePrevTee = true;
}

void CVisuals::TeleportEffect(vec2 From, vec2 To)
{
	const ColorRGBA Cyan(0.25f, 0.8f, 1.0f, 1.0f);
	const ColorRGBA Violet(0.85f, 0.4f, 1.0f, 1.0f);
	const ColorRGBA White(1.0f, 1.0f, 1.0f, 1.0f);

	// departure: everything is sucked into the entrance
	SpawnFlash(From, 80.0f, 0.3f);
	SpawnRing(From, 70.0f, Cyan, false);
	SpawnRing(From, 40.0f, White, false);
	for(int i = 0; i < 18; i++)
	{
		const float Angle = random_float(0.0f, 2.0f * pi);
		const vec2 Dir(std::cos(Angle), std::sin(Angle));
		const float Radius = random_float(40.0f, 75.0f);
		SpawnGlow(From + Dir * Radius, -Dir * Radius / 0.4f, 0.4f, random_float(9.0f, 16.0f), -0.5f, 0.95f, i % 2 ? Cyan : White);
	}

	// arrival: burst out of the exit
	SpawnFlash(To, 130.0f, 0.45f);
	SpawnRing(To, 110.0f, Violet, false);
	SpawnRing(To, 70.0f, White, false);
	for(int i = 0; i < 22; i++)
	{
		const float Angle = random_float(0.0f, 2.0f * pi);
		const vec2 Dir(std::cos(Angle), std::sin(Angle));
		SpawnStar(To + Dir * 10.0f, Dir * random_float(90.0f, 260.0f), random_float(0.5f, 0.9f), random_float(10.0f, 20.0f), 0.95f, i % 3 == 0 ? White : (i % 2 ? Violet : Cyan));
	}
}

void CVisuals::RenderSpeedAndCursor(const CScreenRect &View, float DeltaTime, double Time)
{
	const bool Fresh = Time - m_TeePosTime < 0.25;
	const vec2 Center = (View.m_TopLeft + View.m_BottomRight) * 0.5f;
	const float ViewH = View.Size().y;
	const float ViewW = View.Size().x;

	Graphics()->TextureClear();

	// ---- speed lines: streaks flying out from the middle of the screen, longer the faster you are
	if(g_Config.m_ClVisualsSpeedLines && Fresh)
	{
		const float Intensity = std::clamp((m_SmoothSpeed - 8.0f) / 16.0f, 0.0f, 1.0f);
		if(Intensity > 0.01f)
		{
			const float Radius = std::sqrt(ViewW * ViewW + ViewH * ViewH) * 0.5f;
			Graphics()->BlendAdditive();
			Graphics()->QuadsBegin();
			const int NumLines = 40;
			for(int i = 0; i < NumLines; i++)
			{
				// fixed pseudo random values per line
				const float R1 = std::fmod(std::sin(i * 12.9898f) * 43758.5453f, 1.0f);
				const float R2 = std::fmod(std::sin(i * 78.233f + 1.7f) * 24634.6345f, 1.0f);
				const float R3 = std::fmod(std::sin(i * 39.346f + 4.1f) * 12345.6789f, 1.0f);
				const float Angle = std::fabs(R1) * 2.0f * pi;
				const float Phase = std::fabs(R2);
				const float Rate = 1.4f + 1.6f * std::fabs(R3) + Intensity;
				const float T = (float)std::fmod(Phase + Time * Rate, 1.0);
				const vec2 Dir(std::cos(Angle), std::sin(Angle));
				const vec2 Perp(-Dir.y, Dir.x);
				const float R0 = Radius * (0.38f + 0.75f * T * T);
				const float Len = Radius * (0.06f + 0.22f * Intensity) * (0.4f + T);
				const float Width = ViewH * (0.0016f + 0.0016f * std::fabs(R3)) * (0.6f + Intensity);
				const float Alpha = Intensity * 0.55f * std::sin(pi * T);
				if(Alpha <= 0.01f)
					continue;
				const vec2 A = Center + Dir * R0;
				const vec2 B = Center + Dir * (R0 + Len);
				Graphics()->SetColor(0.85f, 0.95f, 1.0f, Alpha);
				const IGraphics::CFreeformItem Streak(A.x - Perp.x * Width, A.y - Perp.y * Width, B.x - Perp.x * Width * 0.2f, B.y - Perp.y * Width * 0.2f,
					A.x + Perp.x * Width, A.y + Perp.y * Width, B.x + Perp.x * Width * 0.2f, B.y + Perp.y * Width * 0.2f);
				Graphics()->QuadsDrawFreeform(&Streak, 1);
			}
			Graphics()->QuadsEnd();
			Graphics()->BlendNormal();
		}
	}

	// ---- cursor trail: a fading holographic laser line when the mouse moves fast
	if(g_Config.m_ClVisualsCursorTrail && Fresh)
	{
		const vec2 Mouse = GameClient()->m_Controls.m_aMousePos[g_Config.m_ClDummy];
		if(m_HavePrevMouse && DeltaTime > 0.0f)
		{
			const float ScreenSpeed = length(Mouse - m_PrevMouse) / DeltaTime / ViewH; // screens per second
			const float Power = std::clamp((ScreenSpeed - 1.0f) / 3.0f, 0.0f, 1.0f);
			if(Power > 0.02f)
				m_vCursorTrail.push_back({m_TeePos + Mouse, (float)Time, Power});
		}
		m_PrevMouse = Mouse;
		m_HavePrevMouse = true;
	}
	else
		m_HavePrevMouse = false;

	const float Fade = 0.5f;
	while(!m_vCursorTrail.empty() && Time - m_vCursorTrail.front().m_Time > Fade)
		m_vCursorTrail.erase(m_vCursorTrail.begin());
	if(m_vCursorTrail.size() > 400)
		m_vCursorTrail.erase(m_vCursorTrail.begin(), m_vCursorTrail.begin() + (m_vCursorTrail.size() - 400));

	if(m_vCursorTrail.size() >= 2)
	{
		// smooth the recorded points with a Catmull-Rom spline so that the stroke is a continuous curve
		struct CSample
		{
			vec2 m_Pos;
			float m_Width;
			float m_Alpha;
			bool m_Break; // the stroke does not continue from the previous sample
		};
		std::vector<CSample> vSamples;
		const int N = (int)m_vCursorTrail.size();
		const int Sub = 4;
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
				const vec2 Pos = Gap ? P1.m_Pos : CatPos;
				const float PointTime = mix(P1.m_Time, P2.m_Time, t);
				const float Power = mix(P1.m_Power, P2.m_Power, t);
				const float Life = 1.0f - std::clamp((float)(Time - PointTime) / Fade, 0.0f, 1.0f);
				// drop shape: thin tail, thick toward the cursor
				const float Width = ViewH * (0.004f + 0.017f * Power) * std::pow(Life, 1.4f);
				vSamples.push_back({Pos, Width, Life * (0.5f + 0.5f * Power), Gap && s == 0});
			}
		}
		{
			const CTrailPoint &Last = m_vCursorTrail.back();
			const float Life = 1.0f - std::clamp((float)(Time - Last.m_Time) / Fade, 0.0f, 1.0f);
			vSamples.push_back({Last.m_Pos, ViewH * (0.004f + 0.017f * Last.m_Power) * std::pow(Life, 1.4f), Life * (0.5f + 0.5f * Last.m_Power), false});
		}

		// a normal for each sample, shared by both neighbor quads so there are no seams
		std::vector<vec2> vNormals(vSamples.size());
		for(size_t k = 0; k < vSamples.size(); k++)
		{
			const vec2 Prev = vSamples[k > 0 ? k - 1 : 0].m_Pos;
			const vec2 Next = vSamples[std::min(k + 1, vSamples.size() - 1)].m_Pos;
			vec2 Dir = Next - Prev;
			const float Len = length(Dir);
			Dir = Len > 0.001f ? Dir / Len : vec2(1.0f, 0.0f);
			vNormals[k] = vec2(-Dir.y, Dir.x);
		}

		Graphics()->BlendAdditive();
		Graphics()->QuadsBegin();
		const float Scale[3] = {2.1f, 1.1f, 0.45f};
		const ColorRGBA Colors[3] = {ColorRGBA(0.1f, 0.55f, 1.0f, 0.3f), ColorRGBA(0.2f, 0.95f, 1.0f, 0.75f), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f)};
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
				Graphics()->SetColor(Colors[Layer].r, Colors[Layer].g, Colors[Layer].b, Colors[Layer].a * A.m_Alpha);
				const IGraphics::CFreeformItem Seg(A.m_Pos.x - NA.x, A.m_Pos.y - NA.y, B.m_Pos.x - NB.x, B.m_Pos.y - NB.y,
					A.m_Pos.x + NA.x, A.m_Pos.y + NA.y, B.m_Pos.x + NB.x, B.m_Pos.y + NB.y);
				Graphics()->QuadsDrawFreeform(&Seg, 1);
			}
			// rounded head
			const CSample &Head = vSamples.back();
			Graphics()->SetColor(Colors[Layer].r, Colors[Layer].g, Colors[Layer].b, Colors[Layer].a * Head.m_Alpha);
			Graphics()->DrawCircle(Head.m_Pos.x, Head.m_Pos.y, Head.m_Width * Scale[Layer], 16);
		}
		Graphics()->QuadsEnd();
		Graphics()->BlendNormal();
	}
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);

	// ---- speed readout
	if(g_Config.m_ClVisualsSpeedLines && g_Config.m_ClVisualsSpeedReadout && Fresh)
	{
		char aBuf[32];
		str_format(aBuf, sizeof(aBuf), "%.1f т/с", m_SmoothSpeed * 50.0f / 32.0f);
		const float Size = ViewH * 0.035f;
		const float Width = TextRender()->TextWidth(Size, aBuf, -1, -1.0f);
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.8f);
		TextRender()->Text(Center.x - Width * 0.5f, View.m_BottomRight.y - ViewH * 0.14f, Size, aBuf, -1.0f);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}
}
