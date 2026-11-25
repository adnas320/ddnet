#ifndef GAME_CLIENT_COMPONENTS_FX_H
#define GAME_CLIENT_COMPONENTS_FX_H

#include <base/color.h>
#include <base/vmath.h>

#include <game/client/component.h>

#include <vector>

// Hook effects, hammer combo and laser effects (tab "Effects") and three atmosphere effects (embers, hearts, storm).
class CFx : public CComponent
{
	struct CCombo
	{
		int m_ClientId;
		int m_Count;
		float m_Last; // time of the last hit
		float m_Milestone; // time when a multiple of 100 was reached, or -1
		float m_LastHitTime;
	};
	struct CHit
	{
		vec2 m_Pos;
		float m_Time;
	};
	struct CLaserSeg
	{
		vec2 m_From;
		vec2 m_To;
		float m_Fade;
		int m_Seed;
	};
	std::vector<CCombo> m_vCombos;
	std::vector<CHit> m_vHits;
	std::vector<CLaserSeg> m_vLasers;
	int m_LastFire = 0;
	bool m_FireInitialized = false;
	float m_LastSwing = -100.0f;

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnReset() override;
	// called by the players component: after the hooks were drawn and before the tees, so the effects cover the hook but not the tee
	void RenderEffects();

	// a hit of a hammer, called when the event arrives
	void OnHammerHit(vec2 Pos);
	// a laser beam that is drawn right now (called by the items component every frame)
	void OnLaser(vec2 From, vec2 To, float Fade);

	// also used by the settings page for the previews
	void DrawCombo(vec2 Pos, int Count, float SinceHit, float Remaining, float SinceMilestone, int Style, float Power, ColorRGBA Col);
	void DrawLaserFx(vec2 From, vec2 To, float Fade, float Time, int Style, float Power, ColorRGBA Col, int Seed);
};

#endif
