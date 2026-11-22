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
		Ui()->DoScrollbarOption(&g_Config.m_ClVisualsAutoSkinInterval, &g_Config.m_ClVisualsAutoSkinInterval, &Row, "Менять каждые", 6, 120, &CUi::ms_LinearScrollbarScale, 0u, " сек");
	}
	DoText(Card, "Сервер принимает смену скина не чаще раза в 5 секунд (минимум здесь 6).");
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
	if(g_Config.m_ClGunpackSecond[0] == '\0')
		str_copy(g_Config.m_ClGunpackSecond, "default");

	// the two packs are shown as textures: the first on the left half, the second on the right half
	static IGraphics::CTextureHandle s_aTextures[2];
	static char s_aLoadedNames[2][64] = {"\x01", "\x01"};
	char *apNames[2] = {g_Config.m_ClAssetGame, g_Config.m_ClGunpackSecond};
	for(int Side = 0; Side < 2; Side++)
	{
		if(str_comp(s_aLoadedNames[Side], apNames[Side]) == 0)
			continue;
		str_copy(s_aLoadedNames[Side], apNames[Side], sizeof(s_aLoadedNames[Side]));
		Graphics()->UnloadTexture(&s_aTextures[Side]);
		CImageInfo Image;
		char aPath[IO_MAX_PATH_LENGTH];
		bool Loaded;
		if(str_comp(apNames[Side], "default") == 0)
			Loaded = Graphics()->LoadPng(Image, "game.png", IStorage::TYPE_ALL);
		else
		{
			str_format(aPath, sizeof(aPath), "assets/game/%s.png", apNames[Side]);
			Loaded = Graphics()->LoadPng(Image, aPath, IStorage::TYPE_ALL);
			if(!Loaded)
			{
				str_format(aPath, sizeof(aPath), "assets/game/%s/game.png", apNames[Side]);
				Loaded = Graphics()->LoadPng(Image, aPath, IStorage::TYPE_ALL);
			}
		}
		if(Loaded)
			s_aTextures[Side] = Graphics()->LoadTextureRawMove(Image, 0, "gunpack preview");
	}

	// an element is one part of a gunpack that can be taken from either side: bit number, name, preview region of the game image (in grid cells)
	struct SElement
	{
		const char *m_pName;
		int m_X, m_Y, m_W, m_H;
	};
	static const SElement s_aElements[13] = {
		{"Молоток", 2, 1, 4, 3}, {"Пистолет", 2, 4, 4, 2}, {"Дробовик", 2, 6, 8, 2}, {"Граната", 2, 8, 7, 2}, {"Лазер", 2, 10, 8, 2}, {"Меч ниндзя", 2, 12, 7, 3},
		{"Прицелы", 0, 4, 2, 2}, {"Снаряды", 6, 4, 2, 2}, {"Вспышки выстрелов", 8, 4, 4, 2}, {"Хук", 3, 0, 2, 1}, {"Сердца, броня, пикапы", 10, 2, 4, 2}, {"Флаги", 21, 0, 4, 4}, {"Звёзды и частицы", 15, 0, 2, 2}};

	MainView.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), IGraphics::CORNER_ALL, 10.0f);
	MainView.Margin(CARD_PADDING, &MainView);
	CUIRect Header;
	MainView.HSplitTop(34.0f, &Header, &MainView);
	CUIRect HeaderLeft, HeaderRight;
	Header.VSplitMid(&HeaderLeft, &HeaderRight, 10.0f);

	// pack choosers: "<" name ">" for each half
	static CButtonContainer s_aPrev[2], s_aNext[2];
	for(int Side = 0; Side < 2; Side++)
	{
		CUIRect Area = Side == 0 ? HeaderLeft : HeaderRight, Prev, Next, Name;
		Area.VSplitLeft(36.0f, &Prev, &Area);
		Area.VSplitRight(36.0f, &Name, &Next);
		int Current = 0;
		for(size_t i = 0; i < gs_vGunpacks.size(); i++)
			if(str_comp(gs_vGunpacks[i].c_str(), apNames[Side]) == 0)
				Current = (int)i;
		int Step = 0;
		if(DoButton_Menu(&s_aPrev[Side], "<", 0, &Prev))
			Step = -1;
		if(DoButton_Menu(&s_aNext[Side], ">", 0, &Next))
			Step = 1;
		if(Step != 0)
		{
			const int Count = (int)gs_vGunpacks.size();
			const int NewIndex = (Current + Step + Count) % Count;
			str_copy(apNames[Side], gs_vGunpacks[NewIndex].c_str(), 50);
			GameClient()->LoadGameSkin(g_Config.m_ClAssetGame);
		}
		char aTitle[96];
		str_format(aTitle, sizeof(aTitle), "%s: %s", Side == 0 ? "Ганпак 1" : "Ганпак 2", apNames[Side]);
		Ui()->DoLabel(&Name, aTitle, 15.0f, TEXTALIGN_MC);
	}
	MainView.HSplitTop(4.0f, nullptr, &MainView);

	// every element: the left cell takes it from the first pack, the right cell from the second one
	const float RowHeight = std::min(44.0f, (MainView.h - 40.0f) / 13.0f);
	static CButtonContainer s_aCells[13][2];
	for(int Element = 0; Element < 13; Element++)
	{
		CUIRect Row;
		MainView.HSplitTop(RowHeight, &Row, &MainView);
		Row.HMargin(1.0f, &Row);
		CUIRect Left, Right, Middle;
		Row.VSplitMid(&Left, &Right, 10.0f);
		const bool FromSecond = (g_Config.m_ClGunpackMask >> Element) & 1;
		for(int Side = 0; Side < 2; Side++)
		{
			CUIRect Cell = Side == 0 ? Left : Right;
			const bool Chosen = (Side == 1) == FromSecond;
			Cell.Draw(Chosen ? ColorRGBA(0.2f, 0.85f, 0.5f, 0.35f) : ColorRGBA(1.0f, 1.0f, 1.0f, 0.06f), IGraphics::CORNER_ALL, 6.0f);
			if(Chosen)
			{
				CUIRect Edge = Cell;
				Edge.Draw(ColorRGBA(0.3f, 1.0f, 0.6f, 0.0f), IGraphics::CORNER_ALL, 6.0f);
			}
			const SElement &Info = s_aElements[Element];
			CUIRect Preview, Label;
			Cell.VSplitLeft(Cell.h * 1.8f, &Preview, &Label);
			Label.VMargin(6.0f, &Label);
			Ui()->DoLabel(&Label, Info.m_pName, 13.0f, TEXTALIGN_ML);
			if(s_aTextures[Side].IsValid())
			{
				// the picture keeps its proportions inside the preview area
				const float Aspect = (float)Info.m_W / (float)Info.m_H;
				float W = Preview.w - 6.0f, H = Preview.h - 4.0f;
				if(W / H > Aspect)
					W = H * Aspect;
				else
					H = W / Aspect;
				Graphics()->TextureSet(s_aTextures[Side]);
				Graphics()->QuadsBegin();
				Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
				Graphics()->QuadsSetSubset(Info.m_X / 32.0f, Info.m_Y / 16.0f, (Info.m_X + Info.m_W) / 32.0f, (Info.m_Y + Info.m_H) / 16.0f);
				const IGraphics::CQuadItem Item(Preview.x + (Preview.w - W) / 2.0f, Preview.y + (Preview.h - H) / 2.0f, W, H);
				Graphics()->QuadsDrawTL(&Item, 1);
				Graphics()->QuadsEnd();
			}
			// the whole cell is the button
			if(Ui()->DoButtonLogic(&s_aCells[Element][Side], 0, &Cell, BUTTONFLAG_LEFT) && !Chosen)
			{
				g_Config.m_ClGunpackMask ^= 1 << Element;
				GameClient()->LoadGameSkin(g_Config.m_ClAssetGame);
			}
		}
	}

	CUIRect Bottom;
	MainView.HSplitTop(30.0f, &Bottom, &MainView);
	Bottom.HMargin(3.0f, &Bottom);
	CUIRect Hint, ButtonA, ButtonB;
	Bottom.VSplitRight(180.0f, &Hint, &ButtonB);
	Hint.VSplitRight(180.0f, &Hint, &ButtonA);
	static CButtonContainer s_ResetButton, s_RefreshButton;
	if(DoButton_Menu(&s_ResetButton, "Всё из ганпака 1", 0, &ButtonA))
	{
		g_Config.m_ClGunpackMask = 0;
		GameClient()->LoadGameSkin(g_Config.m_ClAssetGame);
	}
	if(DoButton_Menu(&s_RefreshButton, "Обновить список", 0, &ButtonB))
		gs_GunpacksLoaded = false;
	Ui()->DoLabel(&Hint, "Нажмите на левую или правую сторону у каждой части, чтобы выбрать, откуда её взять. Ганпаки кладите в assets/game.", 12.0f, TEXTALIGN_ML);
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

}

// ---------------------------------------------------------------- Музыка

void CMenus::RenderSettingsMusic(CUIRect MainView)
{
	CARD_HELPERS
	CUIRect Left, Right;
	MainView.VSplitMid(&Left, &Right, 10.0f);
	CMusicPlayer &Music = GameClient()->m_MusicPlayer;

	CUIRect Card = BeginCard(Left, "Музыка", 12);
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
	DoText(Card, "Песни из интернета: Kevin MacLeod, incompetech.com (CC BY 4.0)");
	if(Music.Error()[0] != '\0')
		DoText(Card, Music.Error(), true);

	Right.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), IGraphics::CORNER_ALL, 10.0f);
	Right.Margin(CARD_PADDING, &Right);
	CUIRect OnlineArea;
	Right.HSplitMid(&OnlineArea, &Right, 8.0f);

	// песни из интернета: скачиваются один раз сами и сразу играют
	{
		CUIRect Title;
		OnlineArea.HSplitTop(CARD_TITLE_HEIGHT, &Title, &OnlineArea);
		Ui()->DoLabel(&Title, Music.IsDownloading() ? "Песни из интернета (скачивается...)" : "Песни из интернета (нажмите, чтобы слушать)", 16.0f, TEXTALIGN_ML);
		static CListBox s_OnlineList;
		int OnlineSelected = Music.OnlineCurrent();
		s_OnlineList.DoStart(24.0f, CMusicPlayer::NumOnlineTracks(), 1, 3, OnlineSelected, &OnlineArea);
		for(int i = 0; i < CMusicPlayer::NumOnlineTracks(); i++)
		{
			const CListboxItem Item = s_OnlineList.DoNextItem(&CMusicPlayer::OnlineTrack(i), OnlineSelected == i);
			if(!Item.m_Visible)
				continue;
			char aName[128];
			str_format(aName, sizeof(aName), "%s%s", CMusicPlayer::OnlineTrack(i).m_pTitle, Music.DownloadIndex() == i ? "  ..." : "");
			Ui()->DoLabel(&Item.m_Rect, aName, 13.0f, TEXTALIGN_ML);
		}
		const int NewOnline = s_OnlineList.DoEnd();
		if(NewOnline != OnlineSelected && NewOnline >= 0)
			Music.PlayOnline(NewOnline);
	}

	CUIRect Title;
	Right.HSplitTop(CARD_TITLE_HEIGHT, &Title, &Right);
	Ui()->DoLabel(&Title, "Ваши треки (нажмите, чтобы включить)", 16.0f, TEXTALIGN_ML);
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

// ---------------------------------------------------------------- Перевод

void CMenus::RenderSettingsTranslate(CUIRect MainView)
{
	CARD_HELPERS
	CUIRect Left, Right;
	MainView.VSplitMid(&Left, &Right, 10.0f);

	static const char *s_apCodes[3] = {"ru", "en", "zh-CN"};
	static const char *s_apNames[3] = {"Русский", "English", "中文"};
	static CButtonContainer s_aButtons[2][3];

	// три кнопки с языками
	const auto DoLanguages = [&](CUIRect &Card, int Which, char *pLang, size_t LangSize) {
		CUIRect Row = NextRow(Card);
		const float Width = Row.w / 3.0f;
		for(int k = 0; k < 3; k++)
		{
			CUIRect Button(Row.x + k * Width + 2.0f, Row.y, Width - 4.0f, Row.h);
			if(DoButton_Menu(&s_aButtons[Which][k], s_apNames[k], str_comp(pLang, s_apCodes[k]) == 0, &Button))
				str_copy(pLang, s_apCodes[k], LangSize);
		}
	};

	CUIRect Card = BeginCard(Left, "Мои сообщения", 5);
	DoToggle(Card, g_Config.m_ClTranslateSend, "Переводить то, что я пишу");
	DoText(Card, "На какой язык:");
	DoLanguages(Card, 0, g_Config.m_ClTranslateSendLang, sizeof(g_Config.m_ClTranslateSendLang));
	DoText(Card, "В чат сразу уходит перевод, от вашего имени.");
	DoText(Card, "Команды (/...) не переводятся.", true);

	Card = BeginCard(Right, "Сообщения других игроков", 5);
	DoToggle(Card, g_Config.m_ClTranslate, "Переводить чужие сообщения");
	DoText(Card, "На какой язык:");
	DoLanguages(Card, 1, g_Config.m_ClTranslateLang, sizeof(g_Config.m_ClTranslateLang));
	DoToggle(Card, g_Config.m_ClTranslateOwn, "Переводить и мои сообщения");
	DoText(Card, "Перевод идёт строкой [Перевод] под сообщением.", true);
}
