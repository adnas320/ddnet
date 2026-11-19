#include "paint.h"

#include <engine/client.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/gameclient.h>

#include <algorithm>
#include <cmath>

static constexpr size_t MAX_POINTS = 30000;

void CPaint::ConKeyPaint(IConsole::IResult *pResult, void *pUserData)
{
	CPaint *pSelf = (CPaint *)pUserData;
	pSelf->m_Active = pResult->GetInteger(0) != 0 && g_Config.m_ClPaintEnabled;
}

void CPaint::ConPaintClear(IConsole::IResult *pResult, void *pUserData)
{
	((CPaint *)pUserData)->Clear();
}

void CPaint::ConPaintUndo(IConsole::IResult *pResult, void *pUserData)
{
	((CPaint *)pUserData)->Undo();
}

void CPaint::OnConsoleInit()
{
	Console()->Register("+paint", "", CFGFLAG_CLIENT, ConKeyPaint, this, "Draw in the world while the key is held");
	Console()->Register("paint_clear", "", CFGFLAG_CLIENT, ConPaintClear, this, "Remove all drawings");
	Console()->Register("paint_undo", "", CFGFLAG_CLIENT, ConPaintUndo, this, "Remove the last drawing");
}

void CPaint::OnReset()
{
	m_Active = false;
	Clear();
}

void CPaint::RecountPoints()
{
	m_NumPoints = 0;
	for(const CStroke &Stroke : m_vStrokes)
		m_NumPoints += Stroke.m_vPoints.size();
}

void CPaint::Clear()
{
	m_vStrokes.clear();
	m_Drawing = false;
	m_NumPoints = 0;
}

void CPaint::Undo()
{
	if(m_vStrokes.empty())
		return;
	m_vStrokes.pop_back();
	m_Drawing = false;
	RecountPoints();
}

void CPaint::SetKey(int Key)
{
	if(g_Config.m_ClPaintKey > 0)
		GameClient()->m_Binds.Bind(g_Config.m_ClPaintKey, "");
	g_Config.m_ClPaintKey = std::clamp(Key, 0, 1023);
	if(g_Config.m_ClPaintKey > 0)
		GameClient()->m_Binds.Bind(g_Config.m_ClPaintKey, "+paint");
}

void CPaint::OnRender()
{
	const int State = Client()->State();
	if(State != IClient::STATE_ONLINE && State != IClient::STATE_DEMOPLAYBACK)
		return;

	const double Now = Client()->LocalTime();
	const float Lifetime = (float)g_Config.m_ClPaintLifetime;

	// drawing with the cursor
	if(m_Active)
	{
		const vec2 Pos = GameClient()->m_Controls.m_aTargetPos[g_Config.m_ClDummy];
		const float Width = (float)g_Config.m_ClPaintWidth;
		if(!m_Drawing)
		{
			CStroke Stroke;
			Stroke.m_Width = Width;
			Stroke.m_LastChange = Now;
			m_vStrokes.push_back(Stroke);
			m_Drawing = true;
		}
		CStroke &Stroke = m_vStrokes.back();
		if(Stroke.m_vPoints.empty() || distance(Stroke.m_vPoints.back(), Pos) >= std::max(2.0f, Width * 0.3f))
		{
			ColorRGBA Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClPaintColor));
			if(g_Config.m_ClPaintRainbow)
				Color = color_cast<ColorRGBA>(ColorHSLA(std::fmod((float)Stroke.m_vPoints.size() * 0.012f + (float)Now * 0.2f, 1.0f), 1.0f, 0.6f));
			Color.a = 1.0f;
			Stroke.m_vPoints.push_back(Pos);
			Stroke.m_vColors.push_back(Color);
			Stroke.m_LastChange = Now;
			m_NumPoints++;
		}
	}
	else
		m_Drawing = false;

	// forget old drawings
	if(Lifetime > 0.0f)
	{
		const size_t Before = m_vStrokes.size();
		for(size_t i = 0; i < m_vStrokes.size();)
		{
			if(!(m_Drawing && i + 1 == m_vStrokes.size()) && Now - m_vStrokes[i].m_LastChange > Lifetime)
				m_vStrokes.erase(m_vStrokes.begin() + i);
			else
				i++;
		}
		if(Before != m_vStrokes.size())
			RecountPoints();
	}
	// too much: drop the oldest
	while(m_NumPoints > MAX_POINTS && m_vStrokes.size() > 1)
	{
		m_NumPoints -= m_vStrokes.front().m_vPoints.size();
		m_vStrokes.erase(m_vStrokes.begin());
	}
	if(m_vStrokes.empty())
		return;

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	for(const CStroke &Stroke : m_vStrokes)
	{
		// fade out during the last seconds
		float Fade = 1.0f;
		if(Lifetime > 0.0f)
		{
			const float Left = Lifetime - (float)(Now - Stroke.m_LastChange);
			Fade = std::clamp(Left / std::min(3.0f, Lifetime), 0.0f, 1.0f);
		}
		const float Half = Stroke.m_Width * 0.5f;
		for(size_t i = 0; i < Stroke.m_vPoints.size(); i++)
		{
			ColorRGBA Color = Stroke.m_vColors[i];
			Color.a = Fade;
			Graphics()->SetColor(Color);
			Graphics()->DrawCircle(Stroke.m_vPoints[i].x, Stroke.m_vPoints[i].y, Half, 12);
			if(i == 0)
				continue;
			const vec2 A = Stroke.m_vPoints[i - 1];
			const vec2 B = Stroke.m_vPoints[i];
			const float Length = distance(A, B);
			if(Length < 0.01f)
				continue;
			const vec2 Dir = (B - A) / Length;
			const vec2 Perp(-Dir.y * Half, Dir.x * Half);
			const IGraphics::CFreeformItem Item(A - Perp, A + Perp, B - Perp, B + Perp);
			Graphics()->QuadsDrawFreeform(&Item, 1);
		}
	}
	Graphics()->QuadsEnd();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
}
