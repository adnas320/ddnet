#ifndef GAME_CLIENT_COMPONENTS_FX_H
#define GAME_CLIENT_COMPONENTS_FX_H

#include <base/color.h>
#include <base/vmath.h>

#include <game/client/component.h>

#include <vector>

// Weapon, crosshair and hook effects (tab "Effects") and three atmosphere effects (embers, hearts, storm).
// Most things are a function of time; shots and explosions are short lived events.
class CFx : public CComponent
{
	enum
	{
		K_HAMARC,
		K_HAMWAVE,
		K_HAMSPARKS,
		K_HAMFIRE,
		K_FLASH,
		K_RING,
		K_SHELL,
		K_SMOKE,
		K_BOOMFIRE,
		K_BOOMSHARDS,
		K_PELLETS,
		K_CONE,
		K_LASERFLASH,
		K_LASERBOLT,
	};
	struct CEvent
	{
		int m_Kind;
		vec2 m_Pos;
		vec2 m_Dir;
		vec2 m_End;
		float m_Age;
		float m_Life;
		int m_Seed;
		ColorRGBA m_Color;
	};
	std::vector<CEvent> m_vEvents;
	int m_LastAttackTick = -1;
	bool m_AttackInitialized = false;
	float m_LastShotTime = -100.0f;
	int m_NextSeed = 1;

	void Spawn(int Kind, vec2 Pos, vec2 Dir, vec2 End, float Life, ColorRGBA Color);

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnReset() override;
	// called by the players component: after the hooks were drawn and before the tees, so the effects cover the hook but not the tee
	void RenderEffects();

	// explosion of a grenade and the hit of a hammer, called when the events arrive
	void OnBoom(vec2 Pos);
	void OnHammerHit(vec2 Pos);
};

#endif
