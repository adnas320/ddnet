/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "menus.h"

#include <base/str.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/components/countryflags.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/client/ui_listbox.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

void CMenus::RenderLanguageSettings(CUIRect MainView)
{
	RenderLanguageSelection(MainView);
}

bool CMenus::RenderLanguageSelection(CUIRect MainView)
{
	static int s_SelectedLanguage = -2; // -2 = unloaded, -1 = unset
	static CListBox s_ListBox;

	if(s_SelectedLanguage == -2)
	{
		s_SelectedLanguage = -1;
		for(size_t i = 0; i < g_Localization.Languages().size(); i++)
		{
			if(str_comp(g_Localization.Languages()[i].m_Filename.c_str(), g_Config.m_ClLanguagefile) == 0)
			{
				s_SelectedLanguage = i;
				s_ListBox.ScrollToSelected();
				break;
			}
		}
	}

	const int OldSelected = s_SelectedLanguage;

	// only English, Russian and Chinese are offered
	const auto Allowed = [](const CLanguage &Language) {
		return Language.m_Filename.empty() || Language.m_Filename == "languages/russian.txt" || Language.m_Filename == "languages/simplified_chinese.txt";
	};
	int NumAllowed = 0;
	int SelectedShown = -1;
	for(size_t i = 0; i < g_Localization.Languages().size(); i++)
	{
		if(!Allowed(g_Localization.Languages()[i]))
			continue;
		if((int)i == s_SelectedLanguage)
			SelectedShown = NumAllowed;
		NumAllowed++;
	}
	int ShownIndex = SelectedShown;
	s_ListBox.DoStart(24.0f, NumAllowed, 1, 3, ShownIndex, &MainView);

	for(const auto &Language : g_Localization.Languages())
	{
		if(!Allowed(Language))
			continue;
		const CListboxItem Item = s_ListBox.DoNextItem(&Language.m_Name, s_SelectedLanguage != -1 && !str_comp(g_Localization.Languages()[s_SelectedLanguage].m_Name.c_str(), Language.m_Name.c_str()));
		if(!Item.m_Visible)
			continue;

		CUIRect FlagRect, Label;
		Item.m_Rect.VSplitLeft(Item.m_Rect.h * 2.0f, &FlagRect, &Label);
		FlagRect.VMargin(6.0f, &FlagRect);
		FlagRect.HMargin(3.0f, &FlagRect);
		GameClient()->m_CountryFlags.Render(Language.m_CountryCode, ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), FlagRect.x, FlagRect.y, FlagRect.w, FlagRect.h);

		Ui()->DoLabel(&Label, Language.m_Name.c_str(), 16.0f, TEXTALIGN_ML);
	}

	const int NewShown = s_ListBox.DoEnd();
	if(NewShown != SelectedShown && NewShown >= 0)
	{
		int Counter = 0;
		for(size_t i = 0; i < g_Localization.Languages().size(); i++)
		{
			if(!Allowed(g_Localization.Languages()[i]))
				continue;
			if(Counter++ == NewShown)
			{
				s_SelectedLanguage = (int)i;
				break;
			}
		}
	}

	if(OldSelected != s_SelectedLanguage && s_SelectedLanguage >= 0)
	{
		str_copy(g_Config.m_ClLanguagefile, g_Localization.Languages()[s_SelectedLanguage].m_Filename.c_str());
		GameClient()->OnLanguageChange();
	}

	return s_ListBox.WasItemActivated();
}
