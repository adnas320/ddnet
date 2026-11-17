#ifndef GAME_CLIENT_COMPONENTS_VISUALS_H
#define GAME_CLIENT_COMPONENTS_VISUALS_H

#include <base/color.h>
#include <base/vmath.h>

#include <engine/graphics.h>

#include <game/client/component.h>

#include <vector>

// Cosmetic client side effects: winter hat, halo, aura, trail and rings on the own tee,
// snow that falls in the game world, snow bursts, frost and a garland on the screen.
class CVisuals : public CComponent
{
	enum
	{
		NUM_FLAKES = 60, // per tile of the snow field
		MAX_BURST_FLAKES = 600,
		MAX_SPARKLES = 300,
		MAX_STREAKS = 80,
		MAX_RINGS = 8,
		NUM_FIREFLIES = 14, // per tile of the firefly field
		NUM_RECENT_LASERS = 32,
	};

	// flakes of the ambient snowfall. The snow field is one tile of fixed size in world coordinates
	// that is repeated over the whole map, so zooming never moves or recreates flakes.
	struct CFlake
	{
		vec2 m_Base; // start position inside the tile
		float m_FallSpeed;
		float m_DriftSpeed;
		float m_Size;
		float m_Alpha;
		float m_Angle;
		float m_Spin;
		float m_SwayPhase;
		float m_SwayFrequency;
	};

	// short living flakes thrown around by weapon hits and landings
	struct CBurstFlake
	{
		vec2 m_Pos;
		vec2 m_Vel;
		float m_Life;
		float m_MaxLife;
		float m_Size;
		float m_Angle;
		float m_Spin;
	};

	// short white flash where a weapon hits something
	struct CFlash
	{
		vec2 m_Pos;
		float m_Life;
		float m_MaxLife;
		float m_Size;
	};

	// glowing particle of the sparkle trail
	struct CSparkle
	{
		vec2 m_Pos;
		vec2 m_Vel;
		float m_Life;
		float m_MaxLife;
		float m_Size;
		float m_Grow; // size change over the lifetime: -0.6 shrinks, 1.0 grows
		float m_AlphaScale;
		ColorRGBA m_Color;
	};

	// thin line in the air that stays behind a fast tee
	struct CStreak
	{
		vec2 m_Pos;
		vec2 m_Dir;
		float m_Length;
		float m_Life;
		float m_MaxLife;
		ColorRGBA m_Color;
	};

	// glowing dot of the firefly field, same tiling idea as the snow
	struct CFirefly
	{
		vec2 m_Base;
		vec2 m_Amp;
		vec2 m_Freq;
		vec2 m_Phase;
		float m_Size;
		float m_TwinkleFreq;
		float m_TwinklePhase;
	};

	// expanding ring at the feet of the tee
	struct CRing
	{
		vec2 m_Pos;
		float m_Life;
		float m_MaxLife;
		float m_MaxRadius;
		ColorRGBA m_Color;
	};

	struct CRecentLaser
	{
		int m_StartTick = -1;
		vec2 m_To = vec2(0.0f, 0.0f);
	};

	IGraphics::CTextureHandle m_HatTexture;
	IGraphics::CTextureHandle m_HaloTexture;
	IGraphics::CTextureHandle m_GlowTexture;

	CFlake m_aFlakes[NUM_FLAKES];
	CFirefly m_aFireflies[NUM_FIREFLIES];
	bool m_FlakesInitialized = false;
	std::vector<CBurstFlake> m_vBurstFlakes;
	std::vector<CFlash> m_vFlashes;
	std::vector<CSparkle> m_vSparkles;
	std::vector<CStreak> m_vStreaks;
	std::vector<CRing> m_vRings;
	CRecentLaser m_aRecentLasers[NUM_RECENT_LASERS];
	int m_NextRecentLaser = 0;

	// state of the hat animation, all springs are integrated in small fixed steps
	bool m_HatInitialized = false;
	float m_HatFlip = 1.0f; // 1: looking right, -1: looking left, animated in between
	float m_HatFlipTarget = 1.0f;
	float m_HatAngle = 0.0f;
	float m_HatAngleSpeed = 0.0f;
	float m_HatBob = 0.0f;
	float m_HatBobSpeed = 0.0f;
	vec2 m_SmoothVel = vec2(0.0f, 0.0f);
	vec2 m_LastVel = vec2(0.0f, 0.0f);

	// state of the own tee for landing, jump and trail effects
	bool m_WasInAir = false;
	float m_FallSpeed = 0.0f;
	float m_TrailAccumulator = 0.0f;
	int m_LastJumped = 0;
	bool m_JumpedInitialized = false;
	float m_DustAccumulator = 0.0f;
	float m_StreakAccumulator = 0.0f;

	void InitFlakes();
	void SpawnBurst(vec2 Pos, vec2 Dir, float Spread, int Num, float MinSpeed, float MaxSpeed);
	void SpawnFlash(vec2 Pos, float Size, float Life);
	void SpawnRing(vec2 Pos, float MaxRadius, ColorRGBA Color);
	void DrawSprite(IGraphics::CTextureHandle Texture, vec2 Center, float Width, float Height, float Rotation, bool Flip, ColorRGBA Color) const;
	void DrawFlake(vec2 Center, float Size, float Angle) const;
	void DrawOrbs(vec2 BodyPos, float TeeSize, float Alpha, ColorRGBA Color, bool Front) const;
	void RenderGunPredict(const CScreenRect &View) const;
	void UpdateHat(vec2 LookDir, vec2 Vel, float DeltaTime);
	ColorRGBA Rainbow(float Offset) const;

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnReset() override;
	void OnRender() override;

	// Only called for the own tee. Position is the tee position, BodyPos the center of the tee body.
	// Called every frame (also when all effects are off) so that the state stays in sync.
	// Aura, sparkle trail, speed lines and back orbs are drawn behind the tee,
	// so this has to be called before the tee is rendered. TeeColor is the body color of the skin.
	void RenderBehindTee(vec2 BodyPos, float TeeSize, float Alpha, ColorRGBA TeeColor);
	// Hat, halo and effect spawning, drawn on top of the tee. LookDir is the aim direction,
	// Vel the velocity of the tee in units per tick.
	void RenderTeeAccessories(vec2 Position, vec2 BodyPos, float TeeSize, float Alpha, vec2 LookDir, vec2 Vel, bool InAir, int Jumped, ColorRGBA TeeColor);

	// weapon hooks
	void OnLaserRendered(vec2 From, vec2 To, int StartTick, bool Shotgun);
	void OnExplosion(vec2 Pos);
};

#endif
