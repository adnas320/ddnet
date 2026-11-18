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
	CUIRect Card = BeginCard(Left, "Новый год", 4);
	DoToggle(Card, g_Config.m_ClVisualsWinterHat, "Шапка на вашей тишке");
	DoToggle(Card, g_Config.m_ClVisualsSnow, "Падающий снег");
	DoToggle(Card, g_Config.m_ClVisualsSnowWeapons, "Снег при попаданиях");
	DoToggle(Card, g_Config.m_ClVisualsSnowLanding, "Снег при приземлении", true);

	Card = BeginCard(Left, "Мир", 5);
	DoToggle(Card, g_Config.m_ClVisualsFireflies, "Светлячки");
	DoToggle(Card, g_Config.m_ClVisualsPetals, "Лепестки сакуры");
	DoToggle(Card, g_Config.m_ClVisualsBubbles, "Мыльные пузыри");
	DoToggle(Card, g_Config.m_ClVisualsButterflies, "Бабочки");
	DoToggle(Card, g_Config.m_ClVisualsMeteors, "Падающие звёзды", true);

	// средняя колонка
	Card = BeginCard(Middle, "Эффекты", 6);
	DoToggle(Card, g_Config.m_ClVisualsAura, "Аура цвета скина");
	DoToggle(Card, g_Config.m_ClVisualsTrail, "Радужный след");
	DoToggle(Card, g_Config.m_ClVisualsJumpRings, "Кольца при прыжке");
	DoToggle(Card, g_Config.m_ClVisualsEcho, "Эхо-силуэты на скорости");
	DoToggle(Card, g_Config.m_ClVisualsStars, "Созвездие над тишкой");
	DoToggle(Card, g_Config.m_ClVisualsDust, "Пыль при беге", true);

	Card = BeginCard(Middle, "Питомец", 1);
	{
		static CButtonContainer s_PetButton;
		const char *apPetNames[4] = {"Выключен", "Призрак", "Котик", "Летучая мышь"};
		char aText[64];
		str_format(aText, sizeof(aText), "Питомец: %s", apPetNames[std::clamp(g_Config.m_ClVisualsPet, 0, 3)]);
		CUIRect Button = NextRow(Card, true);
		if(DoButton_Menu(&s_PetButton, aText, 0, &Button))
			g_Config.m_ClVisualsPet = (g_Config.m_ClVisualsPet + 1) % 4;
	}

	// правая колонка
	Card = BeginCard(Right, "Оружие", 4);
	DoToggle(Card, g_Config.m_ClVisualsGrenadeTrail, "Огонь за гранатой");
	DoToggle(Card, g_Config.m_ClVisualsBulletTrail, "Спираль за пулями");
	DoToggle(Card, g_Config.m_ClVisualsLaserSparks, "Искры на лазере");
	DoToggle(Card, g_Config.m_ClVisualsHookSparks, "Искры на хуке", true);

	Card = BeginCard(Right, "Смерть", 2);
	DoToggle(Card, g_Config.m_ClVisualsDeathSoul, "Призрак над местом");
	DoToggle(Card, g_Config.m_ClVisualsDeathFirework, "Фейерверк", true);

	Card = BeginCard(Right, "Ник в таблице (Tab)", 5);
	DoToggle(Card, g_Config.m_ClVisualsNameShimmer, "Переливающийся ник");
	static CButtonContainer s_Color1Reset, s_Color2Reset, s_Color3Reset;
	DoColor(Card, s_Color1Reset, g_Config.m_ClVisualsNameColor1, 14942130u, "Цвет 1");
	DoColor(Card, s_Color2Reset, g_Config.m_ClVisualsNameColor2, 8847270u, "Цвет 2");
	DoColor(Card, s_Color3Reset, g_Config.m_ClVisualsNameColor3, 2228134u, "Цвет 3");
	{
		CUIRect Row = NextRow(Card, true);
		Ui()->DoScrollbarOption(&g_Config.m_ClVisualsNameSpeed, &g_Config.m_ClVisualsNameSpeed, &Row, "Скорость", 1, 10, &CUi::ms_LinearScrollbarScale, 0u, "");
	}
}
