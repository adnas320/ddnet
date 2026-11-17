#include "menus.h"

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/ui.h>

// All texts on this page are intentionally English only (no Localize).

namespace {

const float CARD_PADDING = 10.0f;
const float CARD_TITLE_HEIGHT = 30.0f;
const float ROW_HEIGHT = 20.0f;
const float ROW_SPACING = 5.0f;

} // namespace

void CMenus::RenderSettingsVisuals(CUIRect MainView)
{
	// two columns of cards, the sections are placed vertically one below the other
	CUIRect Left, Right;
	MainView.VSplitMid(&Left, &Right, 10.0f);

	// Draws a rounded card with a title and returns the area for its rows.
	const auto BeginCard = [&](CUIRect &Column, const char *pTitle, int NumRows) {
		CUIRect Card, Label;
		const float Height = 2.0f * CARD_PADDING + CARD_TITLE_HEIGHT + NumRows * ROW_HEIGHT + (NumRows - 1) * ROW_SPACING;
		Column.HSplitTop(Height, &Card, &Column);
		Column.HSplitTop(10.0f, nullptr, &Column);
		Card.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), IGraphics::CORNER_ALL, 10.0f);
		Card.Margin(CARD_PADDING, &Card);
		Card.HSplitTop(CARD_TITLE_HEIGHT, &Label, &Card);
		Ui()->DoLabel(&Label, pTitle, 20.0f, TEXTALIGN_ML);
		return Card;
	};

	// A check box row that toggles the given config value.
	const auto DoToggle = [&](CUIRect &Card, int &Value, const char *pText, bool Last = false) {
		CUIRect Button;
		Card.HSplitTop(ROW_HEIGHT, &Button, &Card);
		if(!Last)
			Card.HSplitTop(ROW_SPACING, nullptr, &Card);
		if(DoButton_CheckBox(&Value, pText, Value, &Button))
			Value ^= 1;
	};

	CUIRect Card = BeginCard(Left, "New Year", 4);
	DoToggle(Card, g_Config.m_ClVisualsWinterHat, "Winter hat on your own tee");
	DoToggle(Card, g_Config.m_ClVisualsSnow, "Falling snow");
	DoToggle(Card, g_Config.m_ClVisualsSnowWeapons, "Snow and flash on weapon hits");
	DoToggle(Card, g_Config.m_ClVisualsSnowLanding, "Snow when you land on the ground", true);

	Card = BeginCard(Left, "Spirits", 2);
	DoToggle(Card, g_Config.m_ClVisualsHalo, "Halo above your own tee");
	DoToggle(Card, g_Config.m_ClVisualsHaloRainbow, "Rainbow halo", true);

	Card = BeginCard(Right, "Effects", 6);
	DoToggle(Card, g_Config.m_ClVisualsAura, "Aura in the color of your skin");
	DoToggle(Card, g_Config.m_ClVisualsTrail, "Rainbow sparkle trail behind you");
	DoToggle(Card, g_Config.m_ClVisualsJumpRings, "Rings when you jump");
	DoToggle(Card, g_Config.m_ClVisualsLandRing, "Shockwave when you land hard");
	DoToggle(Card, g_Config.m_ClVisualsNameShimmer, "Shimmering name in skin colors");
	DoToggle(Card, g_Config.m_ClVisualsSpeedLines, "Speed lines when you are fast", true);

	Card = BeginCard(Right, "Extras", 3);
	DoToggle(Card, g_Config.m_ClVisualsDust, "Dust puffs when running");
	DoToggle(Card, g_Config.m_ClVisualsOrbs, "Orbs orbiting your tee");
	DoToggle(Card, g_Config.m_ClVisualsFireflies, "Fireflies in the world", true);
}
