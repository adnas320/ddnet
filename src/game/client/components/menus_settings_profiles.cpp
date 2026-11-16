#include "menus.h"

#include <base/io.h>
#include <base/str.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <game/client/components/countryflags.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/client/ui.h>
#include <game/client/ui_listbox.h>

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

// Profiles: saved sets of skin, colors, eyes, name, clan and flag that can be loaded with one click.
// All texts on this page are intentionally English only (no Localize).

namespace {

enum
{
	PROFILE_SKIN = 1 << 0,
	PROFILE_COLORS = 1 << 1,
	PROFILE_EMOTE = 1 << 2,
	PROFILE_NAME = 1 << 3,
	PROFILE_CLAN = 1 << 4,
	PROFILE_FLAG = 1 << 5,
	NUM_PROFILE_PARTS = 6,
};

struct SProfile
{
	char m_aName[16] = "";
	char m_aClan[12] = "";
	char m_aSkin[24] = "default";
	int m_Country = 0;
	int m_UseCustomColor = 0;
	int m_ColorBody = 0;
	int m_ColorFeet = 0;
	int m_Emote = 0;
	int m_Parts = 0; // which of the PROFILE_* parts are stored in this profile
};

// the values of a player that profiles are made of
struct SPlayerValues
{
	char m_aName[16];
	char m_aClan[12];
	char m_aSkin[24];
	int m_Country;
	int m_UseCustomColor;
	int m_ColorBody;
	int m_ColorFeet;
	int m_Emote;
};

const char *PROFILES_FILE = "ddnet_profiles.tsv";

std::vector<SProfile> gs_vProfiles;
bool gs_ProfilesLoaded = false;

SPlayerValues GetCurrentValues(bool Dummy)
{
	SPlayerValues Values;
	if(Dummy)
	{
		str_copy(Values.m_aName, g_Config.m_ClDummyName);
		str_copy(Values.m_aClan, g_Config.m_ClDummyClan);
		str_copy(Values.m_aSkin, g_Config.m_ClDummySkin);
		Values.m_Country = g_Config.m_ClDummyCountry;
		Values.m_UseCustomColor = g_Config.m_ClDummyUseCustomColor;
		Values.m_ColorBody = g_Config.m_ClDummyColorBody;
		Values.m_ColorFeet = g_Config.m_ClDummyColorFeet;
		Values.m_Emote = g_Config.m_ClDummyDefaultEyes;
	}
	else
	{
		str_copy(Values.m_aName, g_Config.m_PlayerName);
		str_copy(Values.m_aClan, g_Config.m_PlayerClan);
		str_copy(Values.m_aSkin, g_Config.m_ClPlayerSkin);
		Values.m_Country = g_Config.m_PlayerCountry;
		Values.m_UseCustomColor = g_Config.m_ClPlayerUseCustomColor;
		Values.m_ColorBody = g_Config.m_ClPlayerColorBody;
		Values.m_ColorFeet = g_Config.m_ClPlayerColorFeet;
		Values.m_Emote = g_Config.m_ClPlayerDefaultEyes;
	}
	return Values;
}

// Returns the values after loading the given parts of a profile on top of the given values.
SPlayerValues ApplyProfile(SPlayerValues Values, const SProfile &Profile, int Parts, bool OverwriteEmptyClan)
{
	Parts &= Profile.m_Parts;
	if(Parts & PROFILE_SKIN)
		str_copy(Values.m_aSkin, Profile.m_aSkin);
	if(Parts & PROFILE_COLORS)
	{
		Values.m_UseCustomColor = Profile.m_UseCustomColor;
		Values.m_ColorBody = Profile.m_ColorBody;
		Values.m_ColorFeet = Profile.m_ColorFeet;
	}
	if(Parts & PROFILE_EMOTE)
		Values.m_Emote = Profile.m_Emote;
	if(Parts & PROFILE_NAME)
		str_copy(Values.m_aName, Profile.m_aName);
	if((Parts & PROFILE_CLAN) && (Profile.m_aClan[0] != '\0' || OverwriteEmptyClan))
		str_copy(Values.m_aClan, Profile.m_aClan);
	if(Parts & PROFILE_FLAG)
		Values.m_Country = Profile.m_Country;
	return Values;
}

void StoreValues(const SPlayerValues &Values, bool Dummy)
{
	if(Dummy)
	{
		str_copy(g_Config.m_ClDummyName, Values.m_aName);
		str_copy(g_Config.m_ClDummyClan, Values.m_aClan);
		str_copy(g_Config.m_ClDummySkin, Values.m_aSkin);
		g_Config.m_ClDummyCountry = Values.m_Country;
		g_Config.m_ClDummyUseCustomColor = Values.m_UseCustomColor;
		g_Config.m_ClDummyColorBody = Values.m_ColorBody;
		g_Config.m_ClDummyColorFeet = Values.m_ColorFeet;
		g_Config.m_ClDummyDefaultEyes = Values.m_Emote;
	}
	else
	{
		str_copy(g_Config.m_PlayerName, Values.m_aName);
		str_copy(g_Config.m_PlayerClan, Values.m_aClan);
		str_copy(g_Config.m_ClPlayerSkin, Values.m_aSkin);
		g_Config.m_PlayerCountry = Values.m_Country;
		g_Config.m_ClPlayerUseCustomColor = Values.m_UseCustomColor;
		g_Config.m_ClPlayerColorBody = Values.m_ColorBody;
		g_Config.m_ClPlayerColorFeet = Values.m_ColorFeet;
		g_Config.m_ClPlayerDefaultEyes = Values.m_Emote;
	}
}

std::string SanitizeField(const char *pText)
{
	std::string Result = pText;
	for(char &Character : Result)
	{
		if(Character == '\t' || Character == '\n' || Character == '\r')
			Character = ' ';
	}
	return Result;
}

void SaveProfiles(IStorage *pStorage)
{
	std::string Content;
	for(const SProfile &Profile : gs_vProfiles)
	{
		Content += SanitizeField(Profile.m_aName) + "\t" + SanitizeField(Profile.m_aClan) + "\t" + SanitizeField(Profile.m_aSkin) + "\t" +
			   std::to_string(Profile.m_Country) + "\t" + std::to_string(Profile.m_UseCustomColor) + "\t" +
			   std::to_string(Profile.m_ColorBody) + "\t" + std::to_string(Profile.m_ColorFeet) + "\t" +
			   std::to_string(Profile.m_Emote) + "\t" + std::to_string(Profile.m_Parts) + "\n";
	}
	IOHANDLE File = pStorage->OpenFile(PROFILES_FILE, IOHANDLE_WRITE, IStorage::TYPE_SAVE);
	if(!File)
		return;
	io_write(File, Content.data(), Content.size());
	io_close(File);
}

void LoadProfiles(IStorage *pStorage)
{
	gs_vProfiles.clear();
	IOHANDLE File = pStorage->OpenFile(PROFILES_FILE, IOHANDLE_READ, IStorage::TYPE_SAVE);
	if(!File)
		return;
	char *pContent = io_read_all_str(File);
	io_close(File);
	if(pContent == nullptr)
		return;
	const std::string Content = pContent;
	free(pContent);

	size_t LineStart = 0;
	while(LineStart < Content.size())
	{
		size_t LineEnd = Content.find('\n', LineStart);
		if(LineEnd == std::string::npos)
			LineEnd = Content.size();
		const std::string Line = Content.substr(LineStart, LineEnd - LineStart);
		LineStart = LineEnd + 1;

		std::vector<std::string> vFields;
		size_t FieldStart = 0;
		while(true)
		{
			const size_t Tab = Line.find('\t', FieldStart);
			if(Tab == std::string::npos)
			{
				vFields.push_back(Line.substr(FieldStart));
				break;
			}
			vFields.push_back(Line.substr(FieldStart, Tab - FieldStart));
			FieldStart = Tab + 1;
		}
		if(vFields.size() < 9)
			continue;

		SProfile Profile;
		str_copy(Profile.m_aName, vFields[0].c_str());
		str_copy(Profile.m_aClan, vFields[1].c_str());
		str_copy(Profile.m_aSkin, vFields[2].c_str());
		Profile.m_Country = atoi(vFields[3].c_str());
		Profile.m_UseCustomColor = atoi(vFields[4].c_str());
		Profile.m_ColorBody = atoi(vFields[5].c_str());
		Profile.m_ColorFeet = atoi(vFields[6].c_str());
		Profile.m_Emote = atoi(vFields[7].c_str());
		Profile.m_Parts = atoi(vFields[8].c_str());
		gs_vProfiles.push_back(Profile);
	}
}

} // namespace

void CMenus::RenderSettingsProfiles(CUIRect MainView)
{
	if(!gs_ProfilesLoaded)
	{
		LoadProfiles(Storage());
		gs_ProfilesLoaded = true;
	}

	static int s_Selected = -1;
	static int s_aPartEnabled[NUM_PROFILE_PARTS] = {1, 1, 1, 1, 1, 1};
	static int s_Dummy = 0;
	static int s_OverwriteEmptyClan = 0;
	static int s_AllowDeleting = 0;
	if(s_Selected >= (int)gs_vProfiles.size())
		s_Selected = -1;

	const int EnabledParts = (s_aPartEnabled[0] ? PROFILE_SKIN : 0) | (s_aPartEnabled[1] ? PROFILE_COLORS : 0) | (s_aPartEnabled[2] ? PROFILE_EMOTE : 0) |
				 (s_aPartEnabled[3] ? PROFILE_NAME : 0) | (s_aPartEnabled[4] ? PROFILE_CLAN : 0) | (s_aPartEnabled[5] ? PROFILE_FLAG : 0);

	// Draws a tee, the texts of a player and its flag in the given rect.
	const auto RenderPlayerValues = [&](CUIRect Rect, const SPlayerValues &Values) {
		CUIRect TeeRect, Texts, Flag, Line;
		Rect.VSplitLeft(Rect.h, &TeeRect, &Rect);
		Rect.VSplitLeft(10.0f, nullptr, &Rect);
		Rect.VSplitRight(Rect.h * 1.2f, &Texts, &Flag);
		Flag.HMargin((Flag.h - Flag.w / 2.0f) / 2.0f, &Flag);

		const CTeeRenderInfo TeeInfo = GetTeeRenderInfo(vec2(TeeRect.w, TeeRect.h), Values.m_aSkin, Values.m_UseCustomColor != 0, Values.m_ColorBody, Values.m_ColorFeet);
		RenderTools()->RenderIdleTeeAtCenter(&TeeInfo, vec2(TeeRect.x + TeeRect.w / 2.0f, TeeRect.y + TeeRect.h * 0.55f), std::clamp(Values.m_Emote, 0, 5));
		GameClient()->m_CountryFlags.Render(Values.m_Country, ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), Flag.x, Flag.y, Flag.w, Flag.h);

		char aText[64];
		const float LineHeight = Texts.h / 3.0f;
		Texts.HSplitTop(LineHeight, &Line, &Texts);
		str_format(aText, sizeof(aText), "Name: %s", Values.m_aName);
		Ui()->DoLabel(&Line, aText, 14.0f, TEXTALIGN_ML);
		Texts.HSplitTop(LineHeight, &Line, &Texts);
		str_format(aText, sizeof(aText), "Clan: %s", Values.m_aClan);
		Ui()->DoLabel(&Line, aText, 14.0f, TEXTALIGN_ML);
		Texts.HSplitTop(LineHeight, &Line, &Texts);
		str_format(aText, sizeof(aText), "Skin: %s", Values.m_aSkin);
		Ui()->DoLabel(&Line, aText, 14.0f, TEXTALIGN_ML);
	};

	const SPlayerValues Current = GetCurrentValues(s_Dummy != 0);

	// top part: the current player, the result of loading and the controls
	CUIRect Top, ListArea, Left, Right;
	MainView.HSplitTop(215.0f, &Top, &ListArea);
	ListArea.HSplitTop(10.0f, nullptr, &ListArea);
	Top.VSplitMid(&Left, &Right, 10.0f);

	Left.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), IGraphics::CORNER_ALL, 10.0f);
	Left.Margin(10.0f, &Left);
	CUIRect Label, Preview;
	Left.HSplitTop(28.0f, &Label, &Left);
	Ui()->DoLabel(&Label, "Your profile", 20.0f, TEXTALIGN_ML);
	Left.HSplitTop(60.0f, &Preview, &Left);
	RenderPlayerValues(Preview, Current);
	Left.HSplitTop(8.0f, nullptr, &Left);
	Left.HSplitTop(28.0f, &Label, &Left);
	Ui()->DoLabel(&Label, "After loading", 20.0f, TEXTALIGN_ML);
	Left.HSplitTop(60.0f, &Preview, &Left);
	if(s_Selected >= 0)
		RenderPlayerValues(Preview, ApplyProfile(Current, gs_vProfiles[s_Selected], EnabledParts, s_OverwriteEmptyClan != 0));
	else
		RenderPlayerValues(Preview, Current);

	Right.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), IGraphics::CORNER_ALL, 10.0f);
	Right.Margin(10.0f, &Right);
	CUIRect Checks, Buttons;
	Right.VSplitMid(&Checks, &Buttons, 10.0f);

	const char *apPartNames[NUM_PROFILE_PARTS] = {"Save/Load skin", "Save/Load colors", "Save/Load eyes", "Save/Load name", "Save/Load clan", "Save/Load flag"};
	for(int Part = 0; Part < NUM_PROFILE_PARTS; Part++)
	{
		CUIRect Row;
		Checks.HSplitTop(20.0f, &Row, &Checks);
		Checks.HSplitTop(5.0f, nullptr, &Checks);
		if(DoButton_CheckBox(&s_aPartEnabled[Part], apPartNames[Part], s_aPartEnabled[Part], &Row))
			s_aPartEnabled[Part] ^= 1;
	}

	CUIRect Button;
	Buttons.HSplitTop(34.0f, &Button, &Buttons);
	static CButtonContainer s_LoadButton;
	if(DoButton_Menu(&s_LoadButton, "Load", 0, &Button) && s_Selected >= 0)
	{
		StoreValues(ApplyProfile(Current, gs_vProfiles[s_Selected], EnabledParts, s_OverwriteEmptyClan != 0), s_Dummy != 0);
		if(s_Dummy)
			m_NeedSendDummyinfo = true;
		else
			m_NeedSendinfo = true;
	}
	Buttons.HSplitTop(8.0f, nullptr, &Buttons);
	Buttons.HSplitTop(34.0f, &Button, &Buttons);
	static CButtonContainer s_SaveButton;
	if(DoButton_Menu(&s_SaveButton, "Save", 0, &Button) && EnabledParts != 0)
	{
		SProfile Profile;
		str_copy(Profile.m_aName, Current.m_aName);
		str_copy(Profile.m_aClan, Current.m_aClan);
		str_copy(Profile.m_aSkin, Current.m_aSkin);
		Profile.m_Country = Current.m_Country;
		Profile.m_UseCustomColor = Current.m_UseCustomColor;
		Profile.m_ColorBody = Current.m_ColorBody;
		Profile.m_ColorFeet = Current.m_ColorFeet;
		Profile.m_Emote = Current.m_Emote;
		Profile.m_Parts = EnabledParts;
		gs_vProfiles.push_back(Profile);
		s_Selected = (int)gs_vProfiles.size() - 1;
		SaveProfiles(Storage());
	}
	Buttons.HSplitTop(8.0f, nullptr, &Buttons);
	Buttons.HSplitTop(20.0f, &Button, &Buttons);
	if(DoButton_CheckBox(&s_AllowDeleting, "Enable deleting", s_AllowDeleting, &Button))
		s_AllowDeleting ^= 1;
	bool DeleteSelected = false;
	if(s_AllowDeleting && s_Selected >= 0)
	{
		Buttons.HSplitTop(5.0f, nullptr, &Buttons);
		Buttons.HSplitTop(24.0f, &Button, &Buttons);
		static CButtonContainer s_DeleteButton;
		if(DoButton_Menu(&s_DeleteButton, "Delete selected", 0, &Button))
			DeleteSelected = true;
	}

	// options row
	CUIRect Options, OptionA, OptionB, OptionC;
	ListArea.HSplitTop(20.0f, &Options, &ListArea);
	ListArea.HSplitTop(10.0f, nullptr, &ListArea);
	Options.VSplitLeft(Options.w / 3.0f, &OptionA, &Options);
	Options.VSplitMid(&OptionB, &OptionC);
	if(DoButton_CheckBox(&s_Dummy, "Dummy", s_Dummy, &OptionA))
		s_Dummy ^= 1;
	if(DoButton_CheckBox(&s_OverwriteEmptyClan, "Overwrite clan even if the profile has none", s_OverwriteEmptyClan, &OptionC))
		s_OverwriteEmptyClan ^= 1;
	(void)OptionB;

	// list of the saved profiles
	ListArea.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), IGraphics::CORNER_ALL, 10.0f);
	ListArea.Margin(5.0f, &ListArea);
	static CListBox s_ListBox;
	s_ListBox.DoStart(50.0f, gs_vProfiles.size(), 2, 3, s_Selected, &ListArea);
	for(size_t i = 0; i < gs_vProfiles.size(); i++)
	{
		const SProfile &Profile = gs_vProfiles[i];
		const CListboxItem Item = s_ListBox.DoNextItem(&gs_vProfiles[i], s_Selected == (int)i);
		if(!Item.m_Visible)
			continue;

		SPlayerValues Values = GetCurrentValues(s_Dummy != 0);
		Values = ApplyProfile(Values, Profile, PROFILE_SKIN | PROFILE_COLORS | PROFILE_EMOTE | PROFILE_NAME | PROFILE_CLAN | PROFILE_FLAG, true);
		CUIRect Entry;
		Item.m_Rect.Margin(3.0f, &Entry);
		RenderPlayerValues(Entry, Values);
	}
	s_Selected = s_ListBox.DoEnd();

	if(DeleteSelected && s_Selected >= 0 && s_Selected < (int)gs_vProfiles.size())
	{
		gs_vProfiles.erase(gs_vProfiles.begin() + s_Selected);
		s_Selected = -1;
		SaveProfiles(Storage());
	}
}
