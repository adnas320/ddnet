#ifndef GAME_CLIENT_COMPONENTS_PAINT_H
#define GAME_CLIENT_COMPONENTS_PAINT_H

#include <base/color.h>
#include <base/vmath.h>

#include <engine/console.h>

#include <game/client/component.h>

#include <vector>

// Draw in the world with the cursor while a key is held. The drawings are only visible for you.
class CPaint : public CComponent
{
	struct CStroke
	{
		std::vector<vec2> m_vPoints;
		std::vector<ColorRGBA> m_vColors;
		float m_Width;
		double m_LastChange; // time of the last point
	};

	std::vector<CStroke> m_vStrokes;
	bool m_Active = false;
	bool m_Drawing = false; // the last stroke is still being drawn
	size_t m_NumPoints = 0;

	static void ConKeyPaint(IConsole::IResult *pResult, void *pUserData);
	static void ConPaintClear(IConsole::IResult *pResult, void *pUserData);
	static void ConPaintUndo(IConsole::IResult *pResult, void *pUserData);
	void RecountPoints();

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnConsoleInit() override;
	void OnReset() override;
	void OnRender() override;

	void Clear();
	void Undo();
	int NumStrokes() const { return (int)m_vStrokes.size(); }
	// binds the key that has to be held to draw (0 removes it)
	void SetKey(int Key);
};

#endif
