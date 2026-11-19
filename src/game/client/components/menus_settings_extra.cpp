#include "menus.h"

#include "musicplayer.h"
#include "paint.h"

#include <base/color.h>
#include <base/str.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/client/ui_listbox.h>

#include <algorithm>
#include <string>
#include <vector>

// Все тексты на этих страницах намеренно только на русском (без Localize).

namespace {

const float CARD_PADDING = 10.0f;
const float CARD_TITLE_HEIGHT = 30.0f;
const float ROW_HEIGHT = 20.0f;
const float ROW_SPACING = 5.0f;

std::vector<std::string> gs_vGunpacks;
bool gs_GunpacksLoaded = false;

int GunpackScan(const char *pName, int IsDir, int StorageType, void *pUser)
{
	if(pName[0] == '.')
		return 0;
	std::vector<std::string> &vList = *(std::vector<std::string> *)pUser;
	if(IsDir)
		vList.emplace_back(pName);
	else if(str_endswith(pName, ".png"))
		vList.emplace_back(std::string(pName, str_length(pName) - 4));
	return 0;
}

} // namespace

// Рисует карточку с заголовком и возвращает область для строк.
#define CARD_HELPERS \
	[[maybe_unused]] const auto BeginCard = [&](CUIRect &Column, const char *pTitle, int NumRows) { \
		CUIRect Card, Label; \
		const float Height = 2.0f * CARD_PADDING + CARD_TITLE_HEIGHT + NumRows * ROW_HEIGHT + (NumRows - 1) * ROW_SPACING; \
		Column.HSplitTop(Height, &Card, &Column); \
		Column.HSplitTop(10.0f, nullptr, &Column); \
		Card.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), IGraphics::CORNER_ALL, 10.0f); \
		Card.Margin(CARD_PADDING, &Card); \
		Card.HSplitTop(CARD_TITLE_HEIGHT, &Label, &Card); \
		Ui()->DoLabel(&Label, pTitle, 18.0f, TEXTALIGN_ML); \
		return Card; \
	}; \
	[[maybe_unused]] const auto NextRow = [&](CUIRect &Card, bool Last = false) { \
		CUIRect Row; \
		Card.HSplitTop(ROW_HEIGHT, &Row, &Card); \
		if(!Last) \
			Card.HSplitTop(ROW_SPACING, nullptr, &Card); \
		return Row; \
	}; \
	[[maybe_unused]] const auto DoToggle = [&](CUIRect &Card, int &Value, const char *pText, bool Last = false) { \
		CUIRect Button = NextRow(Card, Last); \
		if(DoButton_CheckBox(&Value, pText, Value, &Button)) \
			Value ^= 1; \
	}; \
	[[maybe_unused]] const auto DoText = [&](CUIRect &Card, const char *pText, bool Last = false) { \
		CUIRect Row = NextRow(Card, Last); \
		Ui()->DoLabel(&Row, pText, 12.0f, TEXTALIGN_ML); \
	}; \
	[[maybe_unused]] const auto DoColor = [&](CUIRect &Card, unsigned int &Value, const char *pText, bool Last = false) { \
		CUIRect Row = NextRow(Card, Last); \
		CUIRect Label, ColorButton; \
		Row.VSplitRight(60.0f, &Label, &ColorButton); \
		Ui()->DoLabel(&Label, pText, 13.0f, TEXTALIGN_ML); \
		DoButton_ColorPicker(&ColorButton, &Value, false); \
	};

// ---------------------------------------------------------------- Скин и цвета

void CMenus::RenderSettingsSkin(CUIRect MainView)
{
	CARD_HELPERS
	CUIRect Left, Right;
	MainView.VSplitMid(&Left, &Right, 10.0f);

	CUIRect Card = BeginCard(Left, "Автосмена скина и цветов", 14);
	DoToggle(Card, g_Config.m_ClVisualsAutoSkin, "Включить автосмену");
	{
		CUIRect Row = NextRow(Card);
		Ui()->DoScrollbarOption(&g_Config.m_ClVisualsAutoSkinInterval, &g_Config.m_ClVisualsAutoSkinInterval, &Row, "Менять каждые", 1, 120, &CUi::ms_LinearScrollbarScale, 0u, " сек");
	}
	DoText(Card, "Сервер часто принимает смену скина не чаще раза в 5 секунд.");
	{
		static CButtonContainer s_ColorsButton;
		const char *apModes[3] = {"Цвета не менять", "Цвета: радуга плавно", "Цвета: случайные резко"};
		CUIRect Row = NextRow(Card);
		if(DoButton_Menu(&s_ColorsButton, apModes[std::clamp(g_Config.m_ClVisualsAutoSkinColors, 0, 2)], 0, &Row))
			g_Config.m_ClVisualsAutoSkinColors = (g_Config.m_ClVisualsAutoSkinColors + 1) % 3;
	}
	DoToggle(Card, g_Config.m_ClVisualsAutoSkinList, "Менять скины по списку (до 5)");
	{
		static CLineInput s_aInputs[5] = {
			CLineInput(g_Config.m_ClVisualsAutoSkin1, sizeof(g_Config.m_ClVisualsAutoSkin1)),
			CLineInput(g_Config.m_ClVisualsAutoSkin2, sizeof(g_Config.m_ClVisualsAutoSkin2)),
			CLineInput(g_Config.m_ClVisualsAutoSkin3, sizeof(g_Config.m_ClVisualsAutoSkin3)),
			CLineInput(g_Config.m_ClVisualsAutoSkin4, sizeof(g_Config.m_ClVisualsAutoSkin4)),
			CLineInput(g_Config.m_ClVisualsAutoSkin5, sizeof(g_Config.m_ClVisualsAutoSkin5))};
		static CButtonContainer s_aCurrentButtons[5];
		for(int i = 0; i < 5; i++)
		{
			CUIRect Row = NextRow(Card), Edit, Current;
			Row.VSplitRight(90.0f, &Edit, &Current);
			Edit.VSplitRight(6.0f, &Edit, nullptr);
			Ui()->DoEditBox(&s_aInputs[i], &Edit, 13.0f);
			Current.HMargin(1.0f, &Current);
			if(DoButton_Menu(&s_aCurrentButtons[i], "Мой скин", 0, &Current))
				s_aInputs[i].Set(g_Config.m_ClPlayerSkin);
		}
	}
	DoText(Card, "Имена скинов смотрите во вкладке Tee.");
	{
		static CButtonContainer s_RestoreButton;
		CUIRect Row = NextRow(Card);
		if(DoButton_Menu(&s_RestoreButton, "Вернуть мой скин и цвета", 0, &Row))
		{
			GameClient()->m_Visuals.RestoreSkin();
			g_Config.m_ClVisualsAutoSkin = 0;
		}
	}
	{
		static CButtonContainer s_SaveButton;
		CUIRect Row = NextRow(Card, true);
		if(DoButton_Menu(&s_SaveButton, "Запомнить текущие как мои", 0, &Row))
			GameClient()->m_Visuals.SaveOriginalSkin();
	}

	Card = BeginCard(Right, "Радужный режим (видите только вы)", 8);
	DoToggle(Card, g_Config.m_ClRainbowTee, "Переливается тишка");
	DoToggle(Card, g_Config.m_ClRainbowHook, "Переливается хук");
	DoToggle(Card, g_Config.m_ClRainbowWeapon, "Переливается оружие");
	{
		static CButtonContainer s_StyleButton;
		const char *apStyles[3] = {"Стиль: радуга", "Стиль: свои цвета плавно", "Стиль: свои цвета резко"};
		CUIRect Row = NextRow(Card);
		if(DoButton_Menu(&s_StyleButton, apStyles[std::clamp(g_Config.m_ClRainbowStyle, 0, 2)], 0, &Row))
			g_Config.m_ClRainbowStyle = (g_Config.m_ClRainbowStyle + 1) % 3;
	}
	{
		CUIRect Row = NextRow(Card);
		Ui()->DoScrollbarOption(&g_Config.m_ClRainbowSpeed, &g_Config.m_ClRainbowSpeed, &Row, "Скорость", 1, 10, &CUi::ms_LinearScrollbarScale, 0u, "");
	}
	DoColor(Card, g_Config.m_ClRainbowColor1, "Свой цвет 1");
	DoColor(Card, g_Config.m_ClRainbowColor2, "Свой цвет 2");
	DoColor(Card, g_Config.m_ClRainbowColor3, "Свой цвет 3", true);
}

// ---------------------------------------------------------------- Ганпаки

void CMenus::RenderSettingsGunpack(CUIRect MainView)
{
	CARD_HELPERS
	if(!gs_GunpacksLoaded)
	{
		gs_GunpacksLoaded = true;
		gs_vGunpacks.clear();
		gs_vGunpacks.emplace_back("default");
		Storage()->ListDirectory(IStorage::TYPE_ALL, "assets/game", GunpackScan, &gs_vGunpacks);
		std::sort(gs_vGunpacks.begin() + 1, gs_vGunpacks.end());
		gs_vGunpacks.erase(std::unique(gs_vGunpacks.begin(), gs_vGunpacks.end()), gs_vGunpacks.end());
	}

	CUIRect Left, Middle, Right, Rest;
	MainView.VSplitLeft((MainView.w - 20.0f) / 3.0f, &Left, &Rest);
	Rest.VSplitLeft(10.0f, nullptr, &Rest);
	Rest.VSplitLeft((MainView.w - 20.0f) / 3.0f, &Middle, &Right);
	Right.VSplitLeft(10.0f, nullptr, &Right);

	const auto DoPackList = [&](CUIRect Area, const char *pTitle, char *pCurrent, size_t CurrentSize, bool ReloadSkin, CListBox &ListBox) {
		Area.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), IGraphics::CORNER_ALL, 10.0f);
		Area.Margin(CARD_PADDING, &Area);
		CUIRect Title;
		Area.HSplitTop(CARD_TITLE_HEIGHT, &Title, &Area);
		Ui()->DoLabel(&Title, pTitle, 16.0f, TEXTALIGN_ML);
		int Selected = -1;
		for(size_t i = 0; i < gs_vGunpacks.size(); i++)
		{
			if(str_comp(gs_vGunpacks[i].c_str(), pCurrent) == 0)
				Selected = (int)i;
		}
		ListBox.DoStart(24.0f, gs_vGunpacks.size(), 1, 3, Selected, &Area);
		for(size_t i = 0; i < gs_vGunpacks.size(); i++)
		{
			const CListboxItem Item = ListBox.DoNextItem(&gs_vGunpacks[i], Selected == (int)i);
			if(!Item.m_Visible)
				continue;
			Ui()->DoLabel(&Item.m_Rect, gs_vGunpacks[i].c_str(), 13.0f, TEXTALIGN_ML);
		}
		const int NewSelected = ListBox.DoEnd();
		if(NewSelected != Selected && NewSelected >= 0 && NewSelected < (int)gs_vGunpacks.size())
		{
			str_copy(pCurrent, gs_vGunpacks[NewSelected].c_str(), CurrentSize);
			GameClient()->LoadGameSkin(g_Config.m_ClAssetGame);
			(void)ReloadSkin;
		}
	};
	static CListBox s_FirstList, s_SecondList;
	DoPackList(Left, "Основной ганпак", g_Config.m_ClAssetGame, sizeof(g_Config.m_ClAssetGame), true, s_FirstList);
	DoPackList(Middle, "Второй ганпак (берём из него)", g_Config.m_ClGunpackSecond, sizeof(g_Config.m_ClGunpackSecond), false, s_SecondList);

	CUIRect Card = BeginCard(Right, "Что взять из второго", 10);
	static const char *s_apParts[7] = {"Оружие", "Прицелы", "Снаряды", "Вспышки выстрелов", "Хук", "Пикапы, сердца, флаги", "Звёзды и частицы"};
	for(int i = 0; i < 7; i++)
	{
		static int s_aToggles[7];
		s_aToggles[i] = (g_Config.m_ClGunpackMask >> i) & 1;
		CUIRect Row = NextRow(Card);
		if(DoButton_CheckBox(&s_aToggles[i], s_apParts[i], s_aToggles[i], &Row))
		{
			g_Config.m_ClGunpackMask ^= 1 << i;
			GameClient()->LoadGameSkin(g_Config.m_ClAssetGame);
		}
	}
	{
		static CButtonContainer s_ResetButton;
		CUIRect Row = NextRow(Card);
		if(DoButton_Menu(&s_ResetButton, "Сбросить смешивание", 0, &Row))
		{
			g_Config.m_ClGunpackMask = 0;
			g_Config.m_ClGunpackSecond[0] = '\0';
			GameClient()->LoadGameSkin(g_Config.m_ClAssetGame);
		}
	}
	{
		static CButtonContainer s_RefreshButton;
		CUIRect Row = NextRow(Card, true);
		if(DoButton_Menu(&s_RefreshButton, "Обновить список", 0, &Row))
			gs_GunpacksLoaded = false;
	}
	CUIRect Hint;
	Right.HSplitTop(60.0f, &Hint, &Right);
	Ui()->DoLabel(&Hint, "Выберите основной и второй ганпак, отметьте что взять из второго (например прицелы или оружие). Новые ганпаки кладите в папку assets/game.", 12.0f, TEXTALIGN_TL);
}

// ---------------------------------------------------------------- Рисование

void CMenus::RenderSettingsPaint(CUIRect MainView)
{
	CARD_HELPERS
	CUIRect Left, Right;
	MainView.VSplitMid(&Left, &Right, 10.0f);
	CPaint &Paint = GameClient()->m_Paint;

	CUIRect Card = BeginCard(Left, "Рисование в игре", 10);
	DoToggle(Card, g_Config.m_ClPaintEnabled, "Рисование включено");
	{
		CUIRect Row = NextRow(Card), Label;
		Row.VSplitLeft(150.0f, &Label, &Row);
		Ui()->DoLabel(&Label, "Клавиша (зажать):", 13.0f, TEXTALIGN_ML);
		static CButtonContainer s_KeyReader, s_KeyClear;
		const CBindSlot Current(g_Config.m_ClPaintKey > 0 ? g_Config.m_ClPaintKey : KEY_UNKNOWN, KeyModifier::NONE);
		const CKeyBinder::CKeyReaderResult Result = GameClient()->m_KeyBinder.DoKeyReader(&s_KeyReader, &s_KeyClear, &Row, Current, false);
		if(!Result.m_Aborted && Result.m_Bind != Current)
			Paint.SetKey(Result.m_Bind.m_Key == KEY_UNKNOWN ? 0 : Result.m_Bind.m_Key);
	}
	DoColor(Card, g_Config.m_ClPaintColor, "Цвет рисунка");
	DoToggle(Card, g_Config.m_ClPaintRainbow, "Радужный рисунок");
	{
		CUIRect Row = NextRow(Card);
		Ui()->DoScrollbarOption(&g_Config.m_ClPaintWidth, &g_Config.m_ClPaintWidth, &Row, "Толщина", 2, 40, &CUi::ms_LinearScrollbarScale, 0u, "");
	}
	{
		static int s_Infinite;
		s_Infinite = g_Config.m_ClPaintLifetime == 0;
		CUIRect Row = NextRow(Card);
		if(DoButton_CheckBox(&s_Infinite, "Рисунок не исчезает (бесконечно)", s_Infinite, &Row))
			g_Config.m_ClPaintLifetime = s_Infinite ? 60 : 0;
	}
	if(g_Config.m_ClPaintLifetime != 0)
	{
		CUIRect Row = NextRow(Card);
		Ui()->DoScrollbarOption(&g_Config.m_ClPaintLifetime, &g_Config.m_ClPaintLifetime, &Row, "Исчезает через", 5, 180, &CUi::ms_LinearScrollbarScale, 0u, " сек");
	}
	else
		DoText(Card, "Рисунки остаются до очистки.");
	{
		static CButtonContainer s_ClearButton;
		CUIRect Row = NextRow(Card);
		if(DoButton_Menu(&s_ClearButton, "Очистить всё", 0, &Row))
			Paint.Clear();
	}
	{
		static CButtonContainer s_UndoButton;
		CUIRect Row = NextRow(Card, true);
		if(DoButton_Menu(&s_UndoButton, "Отменить последний штрих", 0, &Row))
			Paint.Undo();
	}

	Right.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), IGraphics::CORNER_ALL, 10.0f);
	Right.Margin(CARD_PADDING, &Right);
	Ui()->DoLabel(&Right, "Как пользоваться:\n\n1. Выберите клавишу выше.\n2. В игре зажмите её и водите курсором: рисунок появится там, где курсор (в том числе когда вы наблюдаете).\n3. Рисунки видите только вы.\n\nКоманды консоли: paint_clear, paint_undo.", 14.0f, TEXTALIGN_TL);
}

// ---------------------------------------------------------------- Музыка

void CMenus::RenderSettingsMusic(CUIRect MainView)
{
	CARD_HELPERS
	CUIRect Left, Right;
	MainView.VSplitMid(&Left, &Right, 10.0f);
	CMusicPlayer &Music = GameClient()->m_MusicPlayer;

	CUIRect Card = BeginCard(Left, "Музыка", 11);
	{
		char aText[256];
		if(Music.Current() >= 0 && Music.Current() < (int)Music.Tracks().size())
			str_format(aText, sizeof(aText), "Сейчас: %s%s", Music.Tracks()[Music.Current()].c_str(), Music.IsPlaying() ? "" : " (пауза)");
		else
			str_copy(aText, "Ничего не играет");
		CUIRect Row = NextRow(Card);
		Ui()->DoLabel(&Row, aText, 13.0f, TEXTALIGN_ML);
	}
	{
		CUIRect Row = NextRow(Card), A, B, C;
		Row.VSplitLeft(Row.w / 3.0f, &A, &Row);
		Row.VSplitMid(&B, &C, 0.0f);
		static CButtonContainer s_Prev, s_Play, s_Next;
		A.VMargin(2.0f, &A);
		B.VMargin(2.0f, &B);
		C.VMargin(2.0f, &C);
		if(DoButton_Menu(&s_Prev, "Назад", 0, &A))
			Music.Previous();
		if(DoButton_Menu(&s_Play, Music.IsPlaying() ? "Пауза" : "Играть", 0, &B))
			Music.TogglePause();
		if(DoButton_Menu(&s_Next, "Вперёд", 0, &C))
			Music.Next();
	}
	{
		CUIRect Row = NextRow(Card);
		Ui()->DoScrollbarOption(&g_Config.m_ClMusicVolume, &g_Config.m_ClMusicVolume, &Row, "Громкость", 0, 100, &CUi::ms_LinearScrollbarScale, 0u, "%");
	}
	DoToggle(Card, g_Config.m_ClMusicShuffle, "Случайный порядок");
	DoToggle(Card, g_Config.m_ClMusicAutoplay, "Включать музыку при запуске игры");
	{
		static CButtonContainer s_RefreshButton;
		CUIRect Row = NextRow(Card);
		if(DoButton_Menu(&s_RefreshButton, "Обновить список", 0, &Row))
			Music.Refresh();
	}
	{
		static CButtonContainer s_FolderButton;
		CUIRect Row = NextRow(Card);
		if(DoButton_Menu(&s_FolderButton, "Открыть папку с музыкой", 0, &Row))
			Music.OpenFolder();
	}
	DoText(Card, "Положите сюда файлы mp3, opus или wv:");
	{
		char aFolder[IO_MAX_PATH_LENGTH];
		Music.GetFolder(aFolder, sizeof(aFolder));
		CUIRect Row = NextRow(Card);
		Ui()->DoLabel(&Row, aFolder, 10.0f, TEXTALIGN_ML);
	}
	if(Music.Error()[0] != '\0')
		DoText(Card, Music.Error(), true);

	Right.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), IGraphics::CORNER_ALL, 10.0f);
	Right.Margin(CARD_PADDING, &Right);
	CUIRect Title;
	Right.HSplitTop(CARD_TITLE_HEIGHT, &Title, &Right);
	Ui()->DoLabel(&Title, "Треки (нажмите, чтобы включить)", 16.0f, TEXTALIGN_ML);
	if(Music.Tracks().empty())
	{
		Ui()->DoLabel(&Right, "Папка пуста. Добавьте песни и нажмите «Обновить список».", 13.0f, TEXTALIGN_TL);
		return;
	}
	static CListBox s_ListBox;
	int Selected = Music.Current();
	s_ListBox.DoStart(24.0f, Music.Tracks().size(), 1, 3, Selected, &Right);
	for(size_t i = 0; i < Music.Tracks().size(); i++)
	{
		const CListboxItem Item = s_ListBox.DoNextItem(&Music.Tracks()[i], Selected == (int)i);
		if(!Item.m_Visible)
			continue;
		Ui()->DoLabel(&Item.m_Rect, Music.Tracks()[i].c_str(), 13.0f, TEXTALIGN_ML);
	}
	const int NewSelected = s_ListBox.DoEnd();
	if(NewSelected != Selected && NewSelected >= 0)
		Music.Play(NewSelected);
}
