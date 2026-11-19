#include "menus.h"

#include "bindwheel.h"
#include "binds.h"

#include <base/color.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/gameclient.h>
#include <game/client/ui.h>

// Все тексты на этой странице намеренно только на русском (без Localize).

void CMenus::RenderSettingsBindWheel(CUIRect MainView)
{
	CBindWheel &Wheel = GameClient()->m_BindWheel;
	CUIRect Left, Right;
	MainView.VSplitMid(&Left, &Right, 10.0f);

	// --- левая колонка: клавиша и добавление ---
	CUIRect Card, Row, Label;
	Left.HSplitTop(150.0f, &Card, &Left);
	Card.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), IGraphics::CORNER_ALL, 10.0f);
	Card.Margin(10.0f, &Card);
	Card.HSplitTop(30.0f, &Label, &Card);
	Ui()->DoLabel(&Label, "Открытие колеса", 18.0f, TEXTALIGN_ML);

	Card.HSplitTop(20.0f, &Row, &Card);
	if(DoButton_CheckBox(&g_Config.m_ClBindWheelEnabled, "Колесо биндов включено", g_Config.m_ClBindWheelEnabled, &Row))
		g_Config.m_ClBindWheelEnabled ^= 1;
	Card.HSplitTop(8.0f, nullptr, &Card);

	Card.HSplitTop(24.0f, &Row, &Card);
	Row.VSplitLeft(150.0f, &Label, &Row);
	Ui()->DoLabel(&Label, "Клавиша:", 14.0f, TEXTALIGN_ML);
	{
		static CButtonContainer s_KeyReader, s_KeyClear;
		const CKeyBinder::CKeyReaderResult Result = GameClient()->m_KeyBinder.DoKeyReader(&s_KeyReader, &s_KeyClear, &Row, CBindSlot(g_Config.m_ClBindWheelKey > 0 ? g_Config.m_ClBindWheelKey : KEY_UNKNOWN, KeyModifier::NONE), false);
		if(!Result.m_Aborted && Result.m_Bind != CBindSlot(g_Config.m_ClBindWheelKey > 0 ? g_Config.m_ClBindWheelKey : KEY_UNKNOWN, KeyModifier::NONE))
			Wheel.SetOpenKey(Result.m_Bind.m_Key == KEY_UNKNOWN ? 0 : Result.m_Bind.m_Key);
	}
	Card.HSplitTop(8.0f, nullptr, &Card);
	Card.HSplitTop(40.0f, &Row, &Card);
	Ui()->DoLabel(&Row, "Зажмите клавишу, наведите мышь на бинд и отпустите клавишу.", 12.0f, TEXTALIGN_TL);

	Left.HSplitTop(10.0f, nullptr, &Left);
	Left.HSplitTop(210.0f, &Card, &Left);
	Card.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), IGraphics::CORNER_ALL, 10.0f);
	Card.Margin(10.0f, &Card);
	Card.HSplitTop(30.0f, &Label, &Card);
	Ui()->DoLabel(&Label, "Добавить бинд", 18.0f, TEXTALIGN_ML);

	static CLineInputBuffered<CBindWheel::MAX_NAME_LENGTH> s_NameInput;
	static CLineInputBuffered<CBindWheel::MAX_COMMAND_LENGTH> s_CommandInput;
	Card.HSplitTop(16.0f, &Label, &Card);
	Ui()->DoLabel(&Label, "Название на колесе:", 12.0f, TEXTALIGN_ML);
	Card.HSplitTop(24.0f, &Row, &Card);
	Ui()->DoEditBox(&s_NameInput, &Row, 14.0f);
	Card.HSplitTop(8.0f, nullptr, &Card);
	Card.HSplitTop(16.0f, &Label, &Card);
	Ui()->DoLabel(&Label, "Команда (например: say привет, или emote 3):", 12.0f, TEXTALIGN_ML);
	Card.HSplitTop(24.0f, &Row, &Card);
	Ui()->DoEditBox(&s_CommandInput, &Row, 14.0f);
	Card.HSplitTop(10.0f, nullptr, &Card);
	Card.HSplitTop(28.0f, &Row, &Card);
	{
		static CButtonContainer s_AddButton;
		if(DoButton_Menu(&s_AddButton, "Добавить", 0, &Row))
		{
			if(Wheel.AddEntry(s_NameInput.GetString(), s_CommandInput.GetString()))
			{
				s_NameInput.Clear();
				s_CommandInput.Clear();
			}
		}
	}

	// --- правая колонка: список ---
	Right.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), IGraphics::CORNER_ALL, 10.0f);
	Right.Margin(10.0f, &Right);
	char aTitle[64];
	str_format(aTitle, sizeof(aTitle), "Ваши бинды (%d/%d)", (int)Wheel.Entries().size(), (int)CBindWheel::MAX_ENTRIES);
	Right.HSplitTop(30.0f, &Label, &Right);
	Ui()->DoLabel(&Label, aTitle, 18.0f, TEXTALIGN_ML);

	if(Wheel.Entries().empty())
		Ui()->DoLabel(&Right, "Пока пусто. Добавьте первый бинд слева.", 14.0f, TEXTALIGN_TL);

	int Remove = -1;
	for(int i = 0; i < (int)Wheel.Entries().size(); i++)
	{
		Right.HSplitTop(26.0f, &Row, &Right);
		Right.HSplitTop(4.0f, nullptr, &Right);
		CUIRect Delete;
		Row.VSplitRight(80.0f, &Row, &Delete);
		Row.VSplitRight(6.0f, &Row, nullptr);
		char aText[CBindWheel::MAX_NAME_LENGTH + CBindWheel::MAX_COMMAND_LENGTH + 8];
		str_format(aText, sizeof(aText), "%s  →  %s", Wheel.Entries()[i].m_aName, Wheel.Entries()[i].m_aCommand);
		Ui()->DoLabel(&Row, aText, 13.0f, TEXTALIGN_ML);
		static CButtonContainer s_aDeleteButtons[CBindWheel::MAX_ENTRIES];
		if(DoButton_Menu(&s_aDeleteButtons[i], "Удалить", 0, &Delete))
			Remove = i;
	}
	if(Remove >= 0)
		Wheel.RemoveEntry(Remove);
}
