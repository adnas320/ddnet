#include "menus.h"

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/ui.h>

// All texts on this page are intentionally English only (no Localize).

namespace {

enum
{
	VISUALS_TAB_NEW_YEAR = 0,
	VISUALS_TAB_SPIRITS,
	NUM_VISUALS_TABS,
};

const float CARD_PADDING = 10.0f;
const float CARD_TITLE_HEIGHT = 30.0f;
const float ROW_HEIGHT = 20.0f;
const float ROW_SPACING = 5.0f;
const float CARD_SPACING = 10.0f;

} // namespace

void CMenus::RenderSettingsVisuals(CUIRect MainView)
{
	static int s_Tab = VISUALS_TAB_NEW_YEAR;

	// sub tabs
	CUIRect TabBar, Button;
	MainView.HSplitTop(26.0f, &TabBar, &MainView);
	MainView.HSplitTop(15.0f, nullptr, &MainView);
	static CButtonContainer s_aTabButtons[NUM_VISUALS_TABS];
	const char *apTabNames[NUM_VISUALS_TABS] = {"New Year", "Spirits"};
	for(int Tab = 0; Tab < NUM_VISUALS_TABS; Tab++)
	{
		TabBar.VSplitLeft(150.0f, &Button, &TabBar);
		TabBar.VSplitLeft(5.0f, nullptr, &TabBar);
		if(DoButton_MenuTab(&s_aTabButtons[Tab], apTabNames[Tab], s_Tab == Tab, &Button, IGraphics::CORNER_T))
			s_Tab = Tab;
	}

	// Starts a rounded card with a title and returns the area for its rows.
	const auto BeginCard = [&](const char *pTitle, int NumRows) {
		CUIRect Card, Label;
		const float Height = 2.0f * CARD_PADDING + CARD_TITLE_HEIGHT + NumRows * ROW_HEIGHT + (NumRows - 1) * ROW_SPACING;
		MainView.HSplitTop(Height, &Card, &MainView);
		MainView.HSplitTop(CARD_SPACING, nullptr, &MainView);
		Card.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), IGraphics::CORNER_ALL, 10.0f);
		Card.Margin(CARD_PADDING, &Card);
		Card.HSplitTop(CARD_TITLE_HEIGHT, &Label, &Card);
		Ui()->DoLabel(&Label, pTitle, 20.0f, TEXTALIGN_ML);
		return Card;
	};

	// A check box row that toggles the given config value.
	const auto DoToggle = [&](CUIRect &Card, int &Value, const char *pText, bool Last = false) {
		Card.HSplitTop(ROW_HEIGHT, &Button, &Card);
		if(!Last)
			Card.HSplitTop(ROW_SPACING, nullptr, &Card);
		if(DoButton_CheckBox(&Value, pText, Value, &Button))
			Value ^= 1;
	};

	if(s_Tab == VISUALS_TAB_NEW_YEAR)
	{
		CUIRect Card = BeginCard("Winter hat", 1);
		DoToggle(Card, g_Config.m_ClVisualsWinterHat, "Winter hat on your own tee", true);

		Card = BeginCard("Snow", 2);
		DoToggle(Card, g_Config.m_ClVisualsSnow, "Falling snow");
		DoToggle(Card, g_Config.m_ClVisualsSnowWeapons, "Snow bursts when weapons hit something (laser, shotgun, grenade)", true);
	}
	else
	{
		CUIRect Card = BeginCard("Halo", 2);
		DoToggle(Card, g_Config.m_ClVisualsHalo, "Halo above your own tee");
		DoToggle(Card, g_Config.m_ClVisualsHaloRainbow, "Rainbow halo", true);

		Card = BeginCard("Demon horns", 2);
		DoToggle(Card, g_Config.m_ClVisualsHorns, "Demon horns on your own tee");
		DoToggle(Card, g_Config.m_ClVisualsHornsRainbow, "Rainbow horns", true);
	}
}
