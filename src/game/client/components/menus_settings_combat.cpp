#include "menus.h"

#include <base/color.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/ui.h>

// Все тексты на этой странице намеренно только на русском (без Localize).

namespace {

const float CARD_PADDING = 10.0f;
const float CARD_TITLE_HEIGHT = 30.0f;
const float ROW_HEIGHT = 20.0f;
const float ROW_SPACING = 5.0f;

} // namespace

void CMenus::RenderSettingsCombat(CUIRect MainView)
{
	CUIRect Left, Right;
	MainView.VSplitMid(&Left, &Right, 10.0f);

	// Рисует карточку с заголовком и возвращает область для строк.
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

	const auto NextRow = [&](CUIRect &Card, bool Last) {
		CUIRect Row;
		Card.HSplitTop(ROW_HEIGHT, &Row, &Card);
		if(!Last)
			Card.HSplitTop(ROW_SPACING, nullptr, &Card);
		return Row;
	};

	const auto DoToggle = [&](CUIRect &Card, int &Value, const char *pText, bool Last = false) {
		CUIRect Button = NextRow(Card, Last);
		if(DoButton_CheckBox(&Value, pText, Value, &Button))
			Value ^= 1;
	};

	const auto DoSlider = [&](CUIRect &Card, int &Value, const char *pText, int Min, int Max, const char *pSuffix, bool Last = false) {
		CUIRect Row = NextRow(Card, Last);
		Ui()->DoScrollbarOption(&Value, &Value, &Row, pText, Min, Max, &CUi::ms_LinearScrollbarScale, 0u, pSuffix);
	};

	// Строка с названием, выбором цвета и кнопкой сброса.
	const auto DoColor = [&](CUIRect &Card, CButtonContainer &ResetButton, unsigned int &Value, unsigned int Default, const char *pText, bool Last = false) {
		CUIRect Row = NextRow(Card, Last);
		CUIRect Label, ColorButton, Reset;
		Row.VSplitRight(80.0f, &Row, &Reset);
		Row.VSplitRight(8.0f, &Row, nullptr);
		Row.VSplitRight(40.0f, &Label, &ColorButton);
		Ui()->DoLabel(&Label, pText, 14.0f, TEXTALIGN_ML);
		DoButton_ColorPicker(&ColorButton, &Value, false);
		Reset.HMargin(2.0f, &Reset);
		if(DoButton_Menu(&ResetButton, "Сброс", 0, &Reset))
			Value = Default;
	};

	CUIRect Card = BeginCard(Left, "Предикт оружия", 5);
	DoToggle(Card, g_Config.m_ClCombatPredict, "Показывать, куда полетит выстрел (с отскоками)");
	DoToggle(Card, g_Config.m_ClCombatPredictLaser, "Лазер");
	DoToggle(Card, g_Config.m_ClCombatPredictShotgun, "Дробовик");
	DoToggle(Card, g_Config.m_ClCombatPredictGrenade, "Граната");
	DoToggle(Card, g_Config.m_ClCombatPredictGun, "Пистолет", true);

	Card = BeginCard(Right, "Внешний вид", 9);
	DoSlider(Card, g_Config.m_ClCombatPredictAlpha, "Прозрачность", 10, 100, "%");
	DoSlider(Card, g_Config.m_ClCombatPredictWidth, "Толщина линии", 1, 12, "");
	static CButtonContainer s_LineReset, s_BounceReset, s_EndReset;
	DoColor(Card, s_LineReset, g_Config.m_ClCombatPredictColor, 8978399u, "Цвет линии");
	DoColor(Card, s_BounceReset, g_Config.m_ClCombatPredictBounceColor, 2555839u, "Цвет точек отскока");
	DoColor(Card, s_EndReset, g_Config.m_ClCombatPredictEndColor, 524204u, "Цвет конечной точки");
	DoToggle(Card, g_Config.m_ClCombatPredictLine, "Показывать саму линию");
	DoToggle(Card, g_Config.m_ClCombatPredictFade, "Линия угасает к концу");
	DoToggle(Card, g_Config.m_ClCombatPredictBounces, "Точки в местах отскока");
	DoToggle(Card, g_Config.m_ClCombatPredictEnd, "Точка в конце выстрела", true);
}
