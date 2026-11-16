#include "snowfall.h"

#include <base/math.h>
#include <base/vmath.h>

#include <engine/client.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <algorithm>
#include <cmath>

void CSnowfall::ResetFlake(CFlake &Flake, bool RandomHeight)
{
	Flake.m_X = random_float();
	Flake.m_Y = RandomHeight ? random_float() : random_float(-0.10f, 0.0f);
	Flake.m_Size = random_float(1.4f, 4.6f);
	Flake.m_Speed = 0.06f + Flake.m_Size * 0.02f + random_float(0.0f, 0.03f);
	Flake.m_SwayAmplitude = random_float(0.005f, 0.020f);
	Flake.m_SwayFrequency = random_float(0.5f, 1.5f);
	Flake.m_SwayPhase = random_float(0.0f, 2.0f * pi);
	Flake.m_Alpha = std::clamp(random_float(0.45f, 0.95f) * (0.6f + Flake.m_Size / 8.0f), 0.0f, 1.0f);
	Flake.m_Angle = random_float(0.0f, 2.0f * pi);
	Flake.m_Spin = random_float(-1.0f, 1.0f);
}

void CSnowfall::OnRender()
{
	if(!g_Config.m_ClVisualsSnow)
		return;
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	if(!m_Initialized)
	{
		for(CFlake &Flake : m_aFlakes)
			ResetFlake(Flake, true);
		m_Initialized = true;
	}

	const float DeltaTime = std::min(Client()->RenderFrameTime(), 0.1f);

	const CScreenRect PreviousScreen = Graphics()->GetScreen();
	const float ScreenHeight = 600.0f;
	const float ScreenWidth = ScreenHeight * Graphics()->ScreenAspect();
	Graphics()->MapScreenToSize(ScreenWidth, ScreenHeight);

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();

	for(CFlake &Flake : m_aFlakes)
	{
		Flake.m_Y += Flake.m_Speed * DeltaTime;
		Flake.m_SwayPhase += Flake.m_SwayFrequency * DeltaTime;
		Flake.m_Angle += Flake.m_Spin * DeltaTime;
		if(Flake.m_Y > 1.05f)
			ResetFlake(Flake, false);

		const vec2 Center(
			(Flake.m_X + std::sin(Flake.m_SwayPhase) * Flake.m_SwayAmplitude) * ScreenWidth,
			Flake.m_Y * ScreenHeight);

		Graphics()->SetColor(1.0f, 1.0f, 1.0f, Flake.m_Alpha);
		if(Flake.m_Size < 2.6f)
		{
			// small flakes are simple dots
			Graphics()->DrawCircle(Center.x, Center.y, Flake.m_Size * 0.6f, 8);
		}
		else
		{
			// bigger flakes are six-armed stars made of three crossed bars
			const float Radius = Flake.m_Size * 1.3f;
			const float HalfWidth = Radius * 0.12f;
			for(int Arm = 0; Arm < 3; Arm++)
			{
				const float Angle = Flake.m_Angle + Arm * pi / 3.0f;
				const vec2 Dir = vec2(std::cos(Angle), std::sin(Angle));
				const vec2 Perp = vec2(-Dir.y, Dir.x);
				const vec2 P0 = Center - Dir * Radius - Perp * HalfWidth;
				const vec2 P1 = Center + Dir * Radius - Perp * HalfWidth;
				const vec2 P2 = Center + Dir * Radius + Perp * HalfWidth;
				const vec2 P3 = Center - Dir * Radius + Perp * HalfWidth;
				const IGraphics::CFreeformItem Item(P0, P1, P3, P2);
				Graphics()->QuadsDrawFreeform(&Item, 1);
			}
		}
	}

	Graphics()->QuadsEnd();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	Graphics()->MapScreen(PreviousScreen);
}
