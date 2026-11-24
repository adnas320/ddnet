#ifndef GAME_CLIENT_COMPONENTS_FX_H
#define GAME_CLIENT_COMPONENTS_FX_H

#include <base/color.h>
#include <base/vmath.h>

#include <game/client/component.h>

#include <vector>

// 20 additional cosmetic effects: around the own tee, in the world and on the whole screen.
// Almost everything is a function of the time, so there is little state to keep.
class CFx : public CComponent
{
	struct CConfetti
	{
		vec2 m_Pos;
		vec2 m_Vel;
		float m_Life;
		float m_Angle;
		float m_Spin;
		ColorRGBA m_Color;
	};
	std::vector<CConfetti> m_vConfetti;
	int m_LastJumped = 0;
	bool m_JumpedInitialized = false;

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnReset() override;
	void OnRender() override;
};

#endif
