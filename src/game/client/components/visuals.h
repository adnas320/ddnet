#ifndef GAME_CLIENT_COMPONENTS_VISUALS_H
#define GAME_CLIENT_COMPONENTS_VISUALS_H

#include <base/color.h>
#include <base/vmath.h>

#include <engine/graphics.h>

#include <game/client/component.h>

#include <vector>

// Cosmetic client side effects: winter hat, aura, trails, pets and more on the own tee,
// fields of snow, petals, bubbles, fireflies and cosmic stars in the game world,
// cosmic weapon and hook effects, kill effects, an aurora and the weapon prediction.
class CVisuals : public CComponent
{
	enum
	{
		NUM_FLAKES = 60, // per tile of the snow field
		NUM_PETALS = 22, // per tile of the petal field
		NUM_BUBBLES = 16, // per tile of the bubble field
		NUM_FIREFLIES = 14, // per tile of the firefly field
		NUM_STARS = 40, // per tile of the star field
		MAX_BURST_FLAKES = 600,
		MAX_SPARKLES = 300,
		MAX_GLOWS = 500,
		MAX_RINGS = 24,
		MAX_ECHOES = 12,
		MAX_SOULS = 20,
		MAX_METEORS = 8,
		NUM_RECENT_LASERS = 32,
	};

	// flakes of the ambient snowfall, petals and bubbles. Every field is one tile of fixed size in world
	// coordinates that is repeated over the whole map, so zooming never moves or recreates anything.
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
		int m_Type; // look of the flake
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
		int m_Type;
	};

	// short white flash where a weapon hits something
	struct CFlash
	{
		vec2 m_Pos;
		float m_Life;
		float m_MaxLife;
		float m_Size;
	};

	// glowing particle (trails, sparks, fireworks)
	struct CSparkle
	{
		vec2 m_Pos;
		vec2 m_Vel;
		float m_Life;
		float m_MaxLife;
		float m_Size;
		float m_Grow; // size change over the lifetime: -0.6 shrinks, 1.0 grows
		float m_AlphaScale;
		float m_Gravity;
		ColorRGBA m_Color;
		bool m_Star = false; // drawn as a spinning four-pointed star instead of a soft glow
		float m_Angle = 0.0f;
		float m_Spin = 0.0f;
	};

	// expanding ring at the feet of the tee
	struct CRing
	{
		vec2 m_Pos;
		float m_Life;
		float m_MaxLife;
		float m_MaxRadius;
		ColorRGBA m_Color;
		bool m_Flat = true; // flat ellipse at the feet or a round ring
	};

	// glowing dot of the firefly field
	struct CFirefly
	{
		vec2 m_Base;
		vec2 m_Amp;
		vec2 m_Freq;
		vec2 m_Phase;
		float m_Size;
		float m_TwinkleFreq;
		float m_TwinklePhase;
		int m_Type; // color and behavior
	};

	// star of the star field
	struct CStar
	{
		vec2 m_Base;
		float m_Size;
		float m_TwinkleFreq;
		float m_TwinklePhase;
		float m_Spin;
		int m_Type; // color
	};

	// ghost that rises from the place where a tee was killed
	struct CSoul
	{
		vec2 m_Pos;
		float m_Life;
		float m_MaxLife;
		float m_Size;
		float m_Phase;
		ColorRGBA m_Color;
	};

	// black hole of the kill effect
	struct CBlackHole
	{
		vec2 m_Pos;
		float m_Life;
		float m_MaxLife;
		float m_Hue;
		bool m_Exploded = false;
	};

	// tee that turns into pixels and crumbles
	struct CPixelTee
	{
		vec2 m_Pos;
		float m_Age;
		float m_Life;
		ColorRGBA m_Body;
		ColorRGBA m_Feet;
		unsigned m_Seed;
	};

	// shooting star
	struct CMeteor
	{
		vec2 m_Pos;
		vec2 m_Vel;
		float m_Life;
		float m_MaxLife;
		float m_Length;
		float m_Size;
		float m_EmberTimer;
	};

	// faded copy of the own tee that is left behind when moving fast
	struct CEcho
	{
		vec2 m_Pos;
		float m_Life;
		float m_MaxLife;
	};

	struct CRecentLaser
	{
		int m_StartTick = -1;
		vec2 m_To = vec2(0.0f, 0.0f);
	};

	IGraphics::CTextureHandle m_HatTexture;
	IGraphics::CTextureHandle m_GlowTexture;
	IGraphics::CTextureHandle m_StarTexture;
	IGraphics::CTextureHandle m_CircleTexture;
	IGraphics::CTextureHandle m_SoulTexture;

	CFlake m_aFlakes[NUM_FLAKES];
	CFlake m_aPetals[NUM_PETALS];
	CFlake m_aBubbles[NUM_BUBBLES];
	CFirefly m_aFireflies[NUM_FIREFLIES];
	CStar m_aStars[NUM_STARS];
	bool m_FlakesInitialized = false;
	std::vector<CBurstFlake> m_vBurstFlakes;
	std::vector<CFlash> m_vFlashes;
	std::vector<CSparkle> m_vSparkles; // drawn behind the own tee
	std::vector<CSparkle> m_vGlows; // drawn in the world
	std::vector<CRing> m_vRings;
	std::vector<CSoul> m_vSouls;
	std::vector<CBlackHole> m_vBlackHoles;
	std::vector<CPixelTee> m_vPixelTees;
	std::vector<CMeteor> m_vMeteors;
	std::vector<CEcho> m_vEchoes;
	CRecentLaser m_aRecentLasers[NUM_RECENT_LASERS];
	int m_NextRecentLaser = 0;
	float m_MeteorTimer = 1.0f;
	float m_AutoSkinTimer = 0.0f;
	float m_CapeSway = 0.0f;
	float m_CapeLift = 0.0f;
	float m_CircleAngle = 0.0f;

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

	// state of the own tee for landing, jump, trail and echo effects
	bool m_WasInAir = false;
	float m_FallSpeed = 0.0f;
	float m_TrailAccumulator = 0.0f;
	int m_LastJumped = 0;
	bool m_JumpedInitialized = false;
	float m_DustAccumulator = 0.0f;
	float m_EchoTimer = 0.0f;

	// speed lines, holographic cursor trail and teleport effect
	struct CTrailPoint
	{
		vec2 m_Pos;
		float m_Time;
		float m_Power;
	};
	std::vector<CTrailPoint> m_vCursorTrail;
	vec2 m_PrevMouse = vec2(0.0f, 0.0f);
	bool m_HavePrevMouse = false;
	vec2 m_TeePos = vec2(0.0f, 0.0f);
	float m_TeePosTime = -100.0f;
	vec2 m_PrevTeePos = vec2(0.0f, 0.0f);
	bool m_HavePrevTee = false;
	float m_SmoothSpeed = 0.0f; // pixels per tick
	void CheckTeleport(vec2 Position, float Time);
	void TeleportEffect(vec2 From, vec2 To);
	void RenderSpeedAndCursor(const CScreenRect &View, float DeltaTime, double Time);

	// state of the pet that follows the own tee
	bool m_PetInitialized = false;
	vec2 m_PetPos = vec2(0.0f, 0.0f);
	vec2 m_PetVel = vec2(0.0f, 0.0f);
	float m_PetFacing = 1.0f;
	vec2 m_PetSpeedShown = vec2(0.0f, 0.0f);
	float m_PetFacingShown = 1.0f; // facing while rolling over
	float m_PetRoll = 1.0f; // 0..1 progress of the roll when the owner turns around
	float m_PetSleep = 0.0f; // 0 awake, 1 asleep
	float m_PetWalk = 0.0f;
	float m_PetCelebrate = 0.0f; // seconds left of the party after finishing
	float m_PetWake = 0.0f; // stretch after waking up
	bool m_PetSpawnBalloons = false;
	float m_AfkTimer = 0.0f;
	vec2 m_AfkLastPos = vec2(0.0f, 0.0f);
	vec2 m_AfkLastLook = vec2(0.0f, 0.0f);
	int m_AfkLastInput = 0;

	struct CBalloon
	{
		vec2 m_Pos;
		vec2 m_Vel;
		float m_Life;
		float m_Phase;
		ColorRGBA m_Color;
	};
	std::vector<CBalloon> m_vBalloons;

	void InitFlakes();
	void SpawnBurst(vec2 Pos, vec2 Dir, float Spread, int Num, float MinSpeed, float MaxSpeed);
	void SpawnFlash(vec2 Pos, float Size, float Life);
	void SpawnRing(vec2 Pos, float MaxRadius, ColorRGBA Color, bool Flat = true);
	void SpawnStar(vec2 Pos, vec2 Vel, float Life, float Size, float AlphaScale, ColorRGBA Color, float Gravity = 0.0f);
	void UpdateAutoSkin(float DeltaTime);
	int m_AutoSkinIndex = -1;
	float m_AutoHue = 0.0f;
	void DrawFlatCircle(IGraphics::CTextureHandle Texture, vec2 Center, float Radius, float Angle, float Squash, ColorRGBA Color) const;
	void DrawWavy(IGraphics::CTextureHandle Texture, vec2 Center, float Width, float Height, float Rotation, bool Flip, ColorRGBA Color, float WaveAmp, float WaveSpeed, float Phase) const;
	void SpawnGlow(vec2 Pos, vec2 Vel, float Life, float Size, float Grow, float AlphaScale, ColorRGBA Color, float Gravity = 0.0f);
	void StepGlows(std::vector<CSparkle> &vGlows, float DeltaTime);
	void DrawSprite(IGraphics::CTextureHandle Texture, vec2 Center, float Width, float Height, float Rotation, bool Flip, ColorRGBA Color) const;
	void DrawFlake(vec2 Center, float Size, float Angle, int Type) const;
	void DrawPanda(vec2 Feet, float Size, float Alpha, vec2 Vel) const;
	void RenderGunPredict(const CScreenRect &View) const;
	void UpdateHat(vec2 LookDir, vec2 Vel, float DeltaTime);
	void UpdatePet(vec2 Position, float TeeSize, vec2 LookDir, vec2 Vel, int Jumped, float DeltaTime);
	ColorRGBA Rainbow(float Offset) const;

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnReset() override;
	void OnRender() override;

	// Only called for the own tee. Position is the tee position, BodyPos the center of the tee body.
	// Called every frame (also when all effects are off) so that the state stays in sync.
	// Aura and sparkle trail are drawn behind the tee, so this has to be called before the tee is rendered.
	// long cape behind the tee, reaching the floor and waving when moving
	void RenderCape(vec2 Position, vec2 BodyPos, float TeeSize, float Alpha, vec2 LookDir, vec2 Vel, bool InAir);
	void RenderBehindTee(vec2 BodyPos, float TeeSize, float Alpha, ColorRGBA TeeColor);
	// Hat, pet and effect spawning, drawn on top of the tee. LookDir is the aim direction,
	// Vel the velocity of the tee in units per tick.
	void RenderTeeAccessories(vec2 Position, vec2 BodyPos, float TeeSize, float Alpha, vec2 LookDir, vec2 Vel, bool InAir, int Jumped, ColorRGBA TeeColor);

	// echoes of the own tee that have to be drawn (faded copies) before the tee itself
	int NumEchoes() const { return (int)m_vEchoes.size(); }
	void GetEcho(int Index, vec2 &Pos, float &Alpha) const;

	// color of the shimmering own name in the scoreboard for the character with the given index
	ColorRGBA NameShimmerColor(int CharIndex) const;
	// whether the name of this player shimmers (everyone, only you or the names of a list)
	bool ShouldShimmer(int ClientId) const;

	// hooks of other components
	// called when a kill message arrives
	void OnKill(int Killer, int Victim);
	// called when the own tee finishes the map: the panda celebrates
	void OnFinish();
	// puts back the skin and colors you had before the automatic changes
	void RestoreSkin();
	// remembers the current skin and colors as your own
	void SaveOriginalSkin();
	// color for the rainbow mode of your own tee, hook and weapon
	ColorRGBA MeColor(float Offset) const;
};

#endif
