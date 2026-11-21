#include "menus.h"

#include <base/color.h>

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
	// три колонки карточек, разделы идут друг под другом
	CUIRect Left, Middle, Right, Rest;
	MainView.VSplitLeft((MainView.w - 20.0f) / 3.0f, &Left, &Rest);
	Rest.VSplitLeft(10.0f, nullptr, &Rest);
	Rest.VSplitLeft((MainView.w - 20.0f) / 3.0f, &Middle, &Right);
	Right.VSplitLeft(10.0f, nullptr, &Right);

	// Рисует карточку с заголовком и возвращает область для строк.
	const auto BeginCard = [&](CUIRect &Column, const char *pTitle, int NumRows) {
		CUIRect Card, Label;
		const float Height = 2.0f * CARD_PADDING + CARD_TITLE_HEIGHT + NumRows * ROW_HEIGHT + (NumRows - 1) * ROW_SPACING;
		Column.HSplitTop(Height, &Card, &Column);
		Column.HSplitTop(10.0f, nullptr, &Column);
		Card.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), IGraphics::CORNER_ALL, 10.0f);
		Card.Margin(CARD_PADDING, &Card);
		Card.HSplitTop(CARD_TITLE_HEIGHT, &Label, &Card);
		Ui()->DoLabel(&Label, pTitle, 18.0f, TEXTALIGN_ML);
		return Card;
	};

	const auto NextRow = [&](CUIRect &Card, bool Last) {
		CUIRect Row;
		Card.HSplitTop(ROW_HEIGHT, &Row, &Card);
		if(!Last)
			Card.HSplitTop(ROW_SPACING, nullptr, &Card);
		return Row;
	};

	// Строка-переключатель для настройки.
	const auto DoToggle = [&](CUIRect &Card, int &Value, const char *pText, bool Last = false) {
		CUIRect Button = NextRow(Card, Last);
		if(DoButton_CheckBox(&Value, pText, Value, &Button))
			Value ^= 1;
	};

	// Строка с названием, выбором цвета и кнопкой сброса.
	const auto DoColor = [&](CUIRect &Card, CButtonContainer &ResetButton, unsigned int &Value, unsigned int Default, const char *pText, bool Last = false) {
		CUIRect Row = NextRow(Card, Last);
		CUIRect Label, ColorButton, Reset;
		Row.VSplitRight(60.0f, &Row, &Reset);
		Row.VSplitRight(6.0f, &Row, nullptr);
		Row.VSplitRight(36.0f, &Label, &ColorButton);
		Ui()->DoLabel(&Label, pText, 13.0f, TEXTALIGN_ML);
		DoButton_ColorPicker(&ColorButton, &Value, false);
		Reset.HMargin(2.0f, &Reset);
		if(DoButton_Menu(&ResetButton, "Сброс", 0, &Reset))
			Value = Default;
	};

	// левая колонка
	CUIRect Card = BeginCard(Left, "Новый год", 3);
	DoToggle(Card, g_Config.m_ClVisualsWinterHat, "Шапка на вашей тишке");
	DoToggle(Card, g_Config.m_ClVisualsSnow, "Падающий снег");
	DoToggle(Card, g_Config.m_ClVisualsSnowLanding, "Снег при приземлении", true);

	Card = BeginCard(Left, "Мир", 5);
	DoToggle(Card, g_Config.m_ClVisualsFireflies, "Светлячки");
	DoToggle(Card, g_Config.m_ClVisualsPetals, "Лепестки сакуры");
	DoToggle(Card, g_Config.m_ClVisualsBubbles, "Мыльные пузыри");
	DoToggle(Card, g_Config.m_ClVisualsStarfield, "Звёздное небо");
	DoToggle(Card, g_Config.m_ClVisualsMeteors, "Синие метеоры", true);

	// средняя колонка
	Card = BeginCard(Middle, "Эффекты тишки", 5);
	DoToggle(Card, g_Config.m_ClVisualsAura, "Аура цвета скина");
	DoToggle(Card, g_Config.m_ClVisualsTrail, "Радужный след");
	DoToggle(Card, g_Config.m_ClVisualsJumpRings, "Кольца при прыжке");
	DoToggle(Card, g_Config.m_ClVisualsEcho, "Эхо-силуэты на скорости");
	DoToggle(Card, g_Config.m_ClVisualsDust, "Пыль при беге", true);

	Card = BeginCard(Middle, "Плащ", 1);
	{
		static CButtonContainer s_CapeButton;
		const char *apCapes[4] = {"Выключен", "Королевский", "RGB", "Тёмный с маской"};
		char aText[64];
		str_format(aText, sizeof(aText), "Плащ: %s", apCapes[std::clamp(g_Config.m_ClVisualsCape, 0, 3)]);
		CUIRect Button = NextRow(Card, true);
		if(DoButton_Menu(&s_CapeButton, aText, 0, &Button))
			g_Config.m_ClVisualsCape = (g_Config.m_ClVisualsCape + 1) % 4;
	}

	Card = BeginCard(Middle, "Питомец", 1);
	{
		static CButtonContainer s_PetButton;
		CUIRect Button = NextRow(Card, true);
		if(DoButton_CheckBox(&s_PetButton, "Панда-питомец (спит при AFK, празднует финиш)", g_Config.m_ClVisualsPet != 0, &Button))
			g_Config.m_ClVisualsPet = g_Config.m_ClVisualsPet != 0 ? 0 : 1;
	}

	Card = BeginCard(Middle, "Автосмена скина", 2);
	{
		static CButtonContainer s_AutoSkinButton;
		const char *apModes[3] = {"Выключена", "Меняет цвета скина", "Меняет весь скин"};
		char aText[64];
		str_format(aText, sizeof(aText), "Режим: %s", apModes[std::clamp(g_Config.m_ClVisualsAutoSkin, 0, 2)]);
		CUIRect Button = NextRow(Card, false);
		if(DoButton_Menu(&s_AutoSkinButton, aText, 0, &Button))
			g_Config.m_ClVisualsAutoSkin = (g_Config.m_ClVisualsAutoSkin + 1) % 3;
		CUIRect Row = NextRow(Card, true);
		Ui()->DoScrollbarOption(&g_Config.m_ClVisualsAutoSkinInterval, &g_Config.m_ClVisualsAutoSkinInterval, &Row, "Каждые", 5, 120, &CUi::ms_LinearScrollbarScale, 0u, " сек");
	}

	// правая колонка
	Card = BeginCard(Right, "Эффект убийства", 2);
	{
		static CButtonContainer s_KillButton;
		const char *apKills[4] = {"Выключен", "Пиксельный распад", "Чёрная дыра", "Призрак"};
		char aText[64];
		str_format(aText, sizeof(aText), "Эффект: %s", apKills[std::clamp(g_Config.m_ClVisualsKillEffect, 0, 3)]);
		CUIRect Button = NextRow(Card, false);
		if(DoButton_Menu(&s_KillButton, aText, 0, &Button))
			g_Config.m_ClVisualsKillEffect = (g_Config.m_ClVisualsKillEffect + 1) % 4;
		DoToggle(Card, g_Config.m_ClVisualsKillOwnOnly, "Только мои убийства", true);
	}

	Card = BeginCard(Right, "Переливающийся ник (везде)", 7);
	DoToggle(Card, g_Config.m_ClVisualsNameShimmer, "Переливающийся ник");
	{
		static CButtonContainer s_NameModeButton;
		const char *apModes[3] = {"У всех игроков", "Только у меня", "У меня и у ников из списка"};
		char aText[96];
		str_format(aText, sizeof(aText), "Кому: %s", apModes[std::clamp(g_Config.m_ClVisualsNameMode, 0, 2)]);
		CUIRect Button = NextRow(Card, false);
		if(DoButton_Menu(&s_NameModeButton, aText, 0, &Button))
			g_Config.m_ClVisualsNameMode = (g_Config.m_ClVisualsNameMode + 1) % 3;
		static CLineInput s_NameListInput(g_Config.m_ClVisualsNameList, sizeof(g_Config.m_ClVisualsNameList));
		s_NameListInput.SetEmptyText("Ники через запятую (режим «список»)");
		CUIRect Row = NextRow(Card, false);
		Ui()->DoEditBox(&s_NameListInput, &Row, 12.0f);
	}
	static CButtonContainer s_Color1Reset, s_Color2Reset, s_Color3Reset;
	DoColor(Card, s_Color1Reset, g_Config.m_ClVisualsNameColor1, 14942130u, "Цвет 1");
	DoColor(Card, s_Color2Reset, g_Config.m_ClVisualsNameColor2, 8847270u, "Цвет 2");
	DoColor(Card, s_Color3Reset, g_Config.m_ClVisualsNameColor3, 2228134u, "Цвет 3");
	{
		CUIRect Row = NextRow(Card, true);
		Ui()->DoScrollbarOption(&g_Config.m_ClVisualsNameSpeed, &g_Config.m_ClVisualsNameSpeed, &Row, "Скорость", 1, 10, &CUi::ms_LinearScrollbarScale, 0u, "");
	}
}
