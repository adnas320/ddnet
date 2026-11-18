#include "menus.h"

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/ui.h>

// Все тексты на этой странице намеренно только на русском (без Localize).

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

	CUIRect Card = BeginCard(Left, "Новый год", 4);
	DoToggle(Card, g_Config.m_ClVisualsWinterHat, "Новогодняя шапка на вашей тишке");
	DoToggle(Card, g_Config.m_ClVisualsSnow, "Падающий снег");
	DoToggle(Card, g_Config.m_ClVisualsSnowWeapons, "Снег и вспышка при попадании оружия");
	DoToggle(Card, g_Config.m_ClVisualsSnowLanding, "Снег при приземлении", true);

	Card = BeginCard(Left, "Духи", 2);
	DoToggle(Card, g_Config.m_ClVisualsHalo, "Нимб над вашей тишкой");
	DoToggle(Card, g_Config.m_ClVisualsHaloRainbow, "Радужный нимб", true);

	Card = BeginCard(Left, "Мир", 3);
	DoToggle(Card, g_Config.m_ClVisualsFireflies, "Светлячки");
	DoToggle(Card, g_Config.m_ClVisualsPetals, "Лепестки сакуры");
	DoToggle(Card, g_Config.m_ClVisualsBubbles, "Мыльные пузыри", true);

	Card = BeginCard(Right, "Эффекты", 4);
	DoToggle(Card, g_Config.m_ClVisualsAura, "Аура цвета вашего скина");
	DoToggle(Card, g_Config.m_ClVisualsTrail, "Радужный искристый след за тишкой");
	DoToggle(Card, g_Config.m_ClVisualsJumpRings, "Кольца при прыжке");
	DoToggle(Card, g_Config.m_ClVisualsNameShimmer, "Переливающийся ник цвета скина", true);

	Card = BeginCard(Right, "Дополнительно", 5);
	DoToggle(Card, g_Config.m_ClVisualsDust, "Пыль при беге");
	DoToggle(Card, g_Config.m_ClVisualsOrbs, "Шарики, вращающиеся вокруг тишки");
	DoToggle(Card, g_Config.m_ClVisualsEmbers, "Искры, поднимающиеся от тишки");
	DoToggle(Card, g_Config.m_ClVisualsHearts, "Летящие сердечки");
	DoToggle(Card, g_Config.m_ClVisualsStars, "Мерцающие звёздочки", true);
}
