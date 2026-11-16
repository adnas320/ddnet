#ifndef GAME_CLIENT_COMPONENTS_SNOWFALL_H
#define GAME_CLIENT_COMPONENTS_SNOWFALL_H

#include <game/client/component.h>

// Falling snowflakes drawn over the game view (below the HUD).
class CSnowfall : public CComponent
{
	enum
	{
		NUM_FLAKES = 140,
	};

	struct CFlake
	{
		float m_X; // 0..1, fraction of screen width
		float m_Y; // 0..1, fraction of screen height
		float m_Size;
		float m_Speed; // screen heights per second
		float m_SwayAmplitude; // fraction of screen width
		float m_SwayFrequency;
		float m_SwayPhase;
		float m_Alpha;
		float m_Angle;
		float m_Spin;
	};

	CFlake m_aFlakes[NUM_FLAKES];
	bool m_Initialized = false;

	static void ResetFlake(CFlake &Flake, bool RandomHeight);

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
};

#endif
