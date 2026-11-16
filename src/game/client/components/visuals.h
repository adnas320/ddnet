#ifndef GAME_CLIENT_COMPONENTS_VISUALS_H
#define GAME_CLIENT_COMPONENTS_VISUALS_H

#include <base/color.h>
#include <base/vmath.h>

#include <engine/graphics.h>

#include <game/client/component.h>

#include <vector>

// Cosmetic client side effects: winter hat, halo and horns on the own tee,
// snow that falls in the game world and snow bursts when weapons hit something.
class CVisuals : public CComponent
{
	enum
	{
		NUM_FLAKES = 60,
		MAX_BURST_FLAKES = 600,
		NUM_RECENT_LASERS = 32,
	};

	// flakes of the ambient snowfall, positions are in world coordinates
	struct CFlake
	{
		vec2 m_Pos;
		float m_FallSpeed;
		float m_DriftSpeed;
		float m_Size;
		float m_Alpha;
		float m_Angle;
		float m_Spin;
		float m_SwayPhase;
		float m_SwayFrequency;
	};

	// short living flakes thrown around by weapon hits
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

	struct CRecentLaser
	{
		int m_StartTick = -1;
		vec2 m_To = vec2(0.0f, 0.0f);
	};

	IGraphics::CTextureHandle m_HatTexture;
	IGraphics::CTextureHandle m_HaloTexture;
	IGraphics::CTextureHandle m_HornTexture;

	CFlake m_aFlakes[NUM_FLAKES];
	bool m_FlakesInitialized = false;
	std::vector<CBurstFlake> m_vBurstFlakes;
	CRecentLaser m_aRecentLasers[NUM_RECENT_LASERS];
	int m_NextRecentLaser = 0;

	void ResetFlake(CFlake &Flake, const CScreenRect &View, bool Anywhere);
	void SpawnBurst(vec2 Pos, vec2 Dir, float Spread, int Num, float MinSpeed, float MaxSpeed);
	void DrawSprite(IGraphics::CTextureHandle Texture, vec2 Center, float Width, float Height, float Rotation, bool Flip, ColorRGBA Color) const;
	void DrawFlake(vec2 Center, float Size, float Angle) const;
	ColorRGBA Rainbow(float Offset) const;

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnReset() override;
	void OnRender() override;

	// Draws the enabled accessories on the own tee. Position is the center of the tee body.
	void RenderTeeAccessories(vec2 BodyPos, float TeeSize, float Alpha) const;

	// weapon hooks
	void OnLaserRendered(vec2 From, vec2 To, int StartTick, bool Shotgun);
	void OnExplosion(vec2 Pos);
};

#endif
