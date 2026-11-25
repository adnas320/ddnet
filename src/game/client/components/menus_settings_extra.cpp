#include "menus.h"

#include "musicplayer.h"
#include "paint.h"

#include <base/color.h>
#include <base/str.h>

#include <engine/client.h>
#include <engine/graphics.h>
#include <engine/http.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/client/ui_listbox.h>
#include <game/client/ui_scrollregion.h>

#include <algorithm>
#include <cmath>
#include <memory>
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
	if(m_ShowGunpackMixer)
	{
		RenderGunpackMixer(MainView);
		return;
	}
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
	CUIRect ButtonMixer;
	Hint.VSplitRight(180.0f, &Hint, &ButtonMixer);
	static CButtonContainer s_ResetButton, s_RefreshButton, s_MixerButton;
	if(DoButton_Menu(&s_MixerButton, "Миксер (перетаскивание)", 0, &ButtonMixer))
		m_ShowGunpackMixer = true;
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


// ---------------------------------------------------------------- Шрифт

namespace {

int FontScan(const char *pName, int IsDir, int StorageType, void *pUser)
{
	if(IsDir || pName[0] == '.')
		return 0;
	if(!str_endswith_nocase(pName, ".ttf") && !str_endswith_nocase(pName, ".otf") && !str_endswith_nocase(pName, ".ttc"))
		return 0;
	((std::vector<std::string> *)pUser)->emplace_back(pName);
	return 0;
}

} // namespace

void CMenus::RenderSettingsFonts(CUIRect MainView)
{
	if(m_ShowFontEditor)
	{
		RenderFontEditor(MainView);
		return;
	}
	CARD_HELPERS
	static std::vector<std::string> s_vUserFonts;
	static bool s_Loaded = false;
	static std::shared_ptr<IHttpRequest> s_pDownload;
	static char s_aDownloadName[128] = "";
	static char s_aStatus[160] = "";
	static CLineInputBuffered<256> s_UrlInput;

	const auto Refresh = [&]() {
		s_vUserFonts.clear();
		Storage()->ListDirectory(IStorage::TYPE_SAVE, "fonts", FontScan, &s_vUserFonts);
		std::sort(s_vUserFonts.begin(), s_vUserFonts.end());
	};
	if(!s_Loaded)
	{
		s_Loaded = true;
		Storage()->CreateFolder("fonts", IStorage::TYPE_SAVE);
		Refresh();
	}

	// finished download
	if(s_pDownload && s_pDownload->State() != EHttpState::RUNNING && s_pDownload->State() != EHttpState::QUEUED)
	{
		if(s_pDownload->State() == EHttpState::DONE)
		{
			str_format(s_aStatus, sizeof(s_aStatus), "Готово: %s", s_aDownloadName);
			Refresh();
		}
		else
			str_copy(s_aStatus, "Не удалось скачать шрифт (проверьте ссылку).");
		s_pDownload = nullptr;
	}

	const auto Select = [&](const char *pPath) {
		str_copy(g_Config.m_ClCustomFont, pPath, sizeof(g_Config.m_ClCustomFont));
		if(!TextRender()->SetCustomFont(pPath))
		{
			g_Config.m_ClCustomFont[0] = '\0';
			TextRender()->SetCustomFont("");
			str_copy(s_aStatus, "Этот файл не удалось открыть как шрифт.");
		}
	};

	CUIRect Left, Right;
	MainView.VSplitMid(&Left, &Right, 10.0f);

	// bundled fonts
	static const char *s_apBundledNames[4] = {"Стандартный (DejaVu)", "Russo One — спортивный", "Press Start 2P — пиксельный", "Pacifico — рукописный"};
	static const char *s_apBundledPaths[4] = {"", "fonts/RussoOne-Regular.ttf", "fonts/PressStart2P-Regular.ttf", "fonts/Pacifico-Regular.ttf"};
	static CButtonContainer s_aBundled[4];
	CUIRect Card = BeginCard(Left, "Шрифты клиента", 6);
	for(int i = 0; i < 4; i++)
	{
		CUIRect Row = NextRow(Card);
		if(DoButton_Menu(&s_aBundled[i], s_apBundledNames[i], str_comp(g_Config.m_ClCustomFont, s_apBundledPaths[i]) == 0, &Row))
			Select(s_apBundledPaths[i]);
	}
	DoText(Card, "Меняет шрифт во всём интерфейсе и чате.");
	DoText(Card, "Русский текст есть у всех, кроме Press Start 2P.", true);

	// create own font
	Card = BeginCard(Left, "Сделать свой шрифт", 5);
	DoText(Card, "Рисуйте буквы мышкой (А Б В, а б в, A B C,");
	DoText(Card, "цифры, знаки, китайские иероглифы), нажмите");
	DoText(Card, "«Готово» и получите настоящий файл шрифта.");
	static CButtonContainer s_MakerButton;
	CUIRect Row = NextRow(Card);
	if(DoButton_Menu(&s_MakerButton, "Открыть редактор шрифта", 0, &Row))
		m_ShowFontEditor = true;
	DoText(Card, "Неготовые буквы берутся из обычного шрифта.", true);

	// user fonts
	const int NumShown = std::min<int>((int)s_vUserFonts.size(), 5);
	Card = BeginCard(Right, "Мои шрифты", 6);
	static CButtonContainer s_aUser[5];
	if(s_vUserFonts.empty())
		DoText(Card, "Пока пусто. Скачайте шрифт ниже или");
	if(s_vUserFonts.empty())
		DoText(Card, "положите .ttf / .otf в папку шрифтов.");
	for(int i = 0; i < NumShown; i++)
	{
		char aPath[160];
		str_format(aPath, sizeof(aPath), "fonts/%s", s_vUserFonts[i].c_str());
		CUIRect UserRow = NextRow(Card);
		if(DoButton_Menu(&s_aUser[i], s_vUserFonts[i].c_str(), str_comp(g_Config.m_ClCustomFont, aPath) == 0, &UserRow))
			Select(aPath);
	}
	static CButtonContainer s_FolderButton, s_RefreshButton;
	CUIRect Buttons = NextRow(Card);
	CUIRect FolderRect, RefreshRect;
	Buttons.VSplitMid(&FolderRect, &RefreshRect, 6.0f);
	if(DoButton_Menu(&s_FolderButton, "Открыть папку", 0, &FolderRect))
	{
		char aFull[IO_MAX_PATH_LENGTH];
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, "fonts", aFull, sizeof(aFull));
		Client()->ViewFile(aFull);
	}
	if(DoButton_Menu(&s_RefreshButton, "Обновить список", 0, &RefreshRect))
		Refresh();

	// download
	Card = BeginCard(Right, "Загрузить шрифт из интернета", 5);
	DoText(Card, "Ссылка на файл .ttf или .otf (например, с GitHub):");
	CUIRect Edit = NextRow(Card);
	Ui()->DoEditBox(&s_UrlInput, &Edit, 13.0f);
	static CButtonContainer s_DownloadButton;
	CUIRect DownRow = NextRow(Card);
	if(DoButton_Menu(&s_DownloadButton, s_pDownload ? "Скачивается..." : "Скачать", 0, &DownRow) && !s_pDownload)
	{
		const char *pUrl = s_UrlInput.GetString();
		char aName[128];
		const char *pSlash = str_rchr(pUrl, '/');
		str_copy(aName, pSlash ? pSlash + 1 : pUrl, sizeof(aName));
		for(char *pQ = aName; *pQ; pQ++)
			if(*pQ == '?' || *pQ == '#')
			{
				*pQ = '\0';
				break;
			}
		for(char *pC = aName; *pC; pC++)
			if(*pC == '/' || *pC == '\\' || *pC == ':' || *pC == '%')
				*pC = '_';
		if(!str_startswith(pUrl, "https://") && !str_startswith(pUrl, "http://"))
			str_copy(s_aStatus, "Ссылка должна начинаться с https://");
		else if(!str_endswith_nocase(aName, ".ttf") && !str_endswith_nocase(aName, ".otf"))
			str_copy(s_aStatus, "Ссылка должна вести прямо на файл .ttf или .otf");
		else
		{
			str_copy(s_aDownloadName, aName, sizeof(s_aDownloadName));
			char aDest[160];
			str_format(aDest, sizeof(aDest), "fonts/%s", aName);
			s_pDownload = HttpGetFile(pUrl, Storage(), aDest, IStorage::TYPE_SAVE);
			s_pDownload->MaxResponseSize(20 * 1024 * 1024);
			s_pDownload->LogProgress(HTTPLOG::NONE);
			Http()->Run(s_pDownload);
			str_copy(s_aStatus, "Скачиваю...");
		}
	}
	DoText(Card, s_aStatus[0] ? s_aStatus : "После загрузки шрифт появится в списке выше.", true);
	DoText(Card, "Шрифты лежат у вас в папке fonts.", true);
}


// ---------------------------------------------------------------- Эффекты

void CMenus::RenderSettingsFx(CUIRect MainView)
{
	CARD_HELPERS
	CUIRect Left, Middle, Right, Rest;
	MainView.VSplitLeft((MainView.w - 20.0f) / 3.0f, &Left, &Rest);
	Rest.VSplitLeft(10.0f, nullptr, &Rest);
	Rest.VSplitLeft((MainView.w - 20.0f) / 3.0f, &Middle, &Right);
	Right.VSplitLeft(10.0f, nullptr, &Right);
	const float Now = (float)std::fmod(Client()->LocalTime(), 10000.0);

	CUIRect Card = BeginCard(Left, "Хук", 2);
	DoToggle(Card, g_Config.m_ClFxHookRgb, "RGB-хук (радужная спираль)");
	DoToggle(Card, g_Config.m_ClFxHookPulse, "Импульсы и вспышка захвата", true);

	// one card for the hammer combo and one for the laser, both look the same: switch, power, color, style, preview
	const auto DoEffectCard = [&](CUIRect &Column, const char *pTitle, int &Enabled, int &Power, unsigned int &Color, unsigned int DefaultColor, int &Style, const char *const *apStyles, bool IsLaser) {
		Card = BeginCard(Column, pTitle, 10);
		DoToggle(Card, Enabled, "Включить");
		CUIRect Row = NextRow(Card);
		Ui()->DoScrollbarOption(&Power, &Power, &Row, "Мощность", 30, 200, &CUi::ms_LinearScrollbarScale, 0u, "%");
		{
			// цвет + кнопка сброса к стандартному
			CUIRect ColorRow = NextRow(Card), ColorLabel, ColorRest, ResetButton, ColorButton;
			ColorRow.VSplitRight(60.0f, &ColorRest, &ColorButton);
			ColorRest.VSplitRight(70.0f, &ColorLabel, &ResetButton);
			ResetButton.VSplitRight(66.0f, nullptr, &ResetButton);
			Ui()->DoLabel(&ColorLabel, "Цвет", 13.0f, TEXTALIGN_ML);
			static CButtonContainer s_aColorReset[2];
			if(DoButton_Menu(&s_aColorReset[IsLaser ? 1 : 0], "Сброс", 0, &ResetButton))
				Color = DefaultColor;
			DoButton_ColorPicker(&ColorButton, &Color, false);
		}
		for(int Line = 0; Line < 2; Line++)
		{
			CUIRect StyleRow = NextRow(Card), A, B;
			StyleRow.VSplitMid(&A, &B, 4.0f);
			static CButtonContainer s_aStyleButtons[2][2][2];
			for(int k = 0; k < 2; k++)
			{
				const int Index = Line * 2 + k;
				if(DoButton_Menu(&s_aStyleButtons[IsLaser ? 1 : 0][Line][k], apStyles[Index], Style == Index, k == 0 ? &A : &B))
					Style = Index;
			}
		}
		CUIRect Preview = NextRow(Card);
		CUIRect Last = Preview;
		for(int k = 0; k < 3; k++)
			Last = NextRow(Card, k == 2);
		Preview.h = Last.y + Last.h - Preview.y;
		Preview.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.35f), IGraphics::CORNER_ALL, 8.0f);
		const ColorRGBA Col = color_cast<ColorRGBA>(ColorHSLA(Color));
		const float PowerF = Power / 100.0f;
		if(IsLaser)
		{
			const vec2 From(Preview.x + 14.0f, Preview.y + Preview.h / 2.0f), To(Preview.x + Preview.w - 14.0f, Preview.y + Preview.h / 2.0f);
			// the beam of the game itself, the effect lies over it
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			Graphics()->SetColor(0.15f, 0.25f, 0.9f, 1.0f);
			const IGraphics::CFreeformItem Outer(From.x, From.y - 2.5f, To.x, To.y - 2.5f, From.x, From.y + 2.5f, To.x, To.y + 2.5f);
			Graphics()->QuadsDrawFreeform(&Outer, 1);
			Graphics()->SetColor(0.8f, 0.9f, 1.0f, 1.0f);
			const IGraphics::CFreeformItem Inner(From.x, From.y - 1.0f, To.x, To.y - 1.0f, From.x, From.y + 1.0f, To.x, To.y + 1.0f);
			Graphics()->QuadsDrawFreeform(&Inner, 1);
			Graphics()->QuadsEnd();
			GameClient()->m_Fx.DrawLaserFx(From, To, 0.65f + 0.35f * std::sin(Now * 3.0f), Now, Style, PowerF, Col, 7);
		}
		else
		{
			// a series of hits, then a pause, and the 100 combo animation at the end
			const float Cycle = std::fmod(Now, 8.0f);
			const float Step = 0.4f;
			const int Count = std::min(12, (int)(Cycle / Step) + 1);
			const float Since = Cycle - (Count - 1) * Step;
			const float Remaining = std::clamp(1.0f - Since, 0.0f, 1.0f);
			const float Milestone = (Cycle > 5.6f && Cycle < 6.9f) ? Cycle - 5.6f : -1.0f;
			if(Cycle < 5.0f + 0.4f || Milestone >= 0.0f)
				GameClient()->m_Fx.DrawCombo(vec2(Preview.x + Preview.w / 2.0f, Preview.y + Preview.h / 2.0f + 6.0f), Milestone >= 0.0f ? 100 : Count, Milestone >= 0.0f ? Milestone : Since, Milestone >= 0.0f ? 1.0f : Remaining, Milestone, Style, PowerF, Col);
		}
	};
	static const char *s_apComboStyles[4] = {"Цифры", "Рамка", "Искры", "Неон"};
	static const char *s_apLaserStyles[4] = {"Молния", "Пульс", "Плазма", "Кристалл"};
	DoEffectCard(Middle, "Комбо хаммера", g_Config.m_ClFxHammer, g_Config.m_ClFxHammerPower, g_Config.m_ClFxHammerColor, 1769369u, g_Config.m_ClFxHammerStyle, s_apComboStyles, false);
	DoEffectCard(Right, "Эффект лазера", g_Config.m_ClFxLaser, g_Config.m_ClFxLaserPower, g_Config.m_ClFxLaserColor, 9240486u, g_Config.m_ClFxLaserStyle, s_apLaserStyles, true);
	Card = BeginCard(Left, "Как работает комбо", 4);
	DoText(Card, "Бейте игрока хаммером: под ним");
	DoText(Card, "появляется x1, x2, x3...");
	DoText(Card, "Секунда без удара сбрасывает комбо,");
	DoText(Card, "на 100 ударах будет анимация.", true);
}

// ---------------------------------------------------------------- Скачивание скинов

void CMenus::RenderSkinBrowser(CUIRect MainView)
{
	struct CSource
	{
		const char *m_pName;
		const char *m_pBase;
	};
	static const CSource s_aSources[3] = {
		{"DDNet: сообщество", "https://skins.ddnet.org/skin/community/"},
		{"DDNet: основные", "https://skins.ddnet.org/skin/"},
		{"DDstats", "https://skins.ddstats.tw/"}};
	static std::vector<std::string> s_vNames;
	static std::vector<char> s_vHave;
	static std::vector<CButtonContainer> s_vButtons;
	static std::vector<int> s_vShown;
	static std::string s_LastFilter = "\x01";
	static int s_Source = 0;
	static CLineInputBuffered<32> s_Filter;
	static std::vector<std::shared_ptr<IHttpRequest>> s_vActive;
	static std::vector<int> s_vActiveIndex;
	static std::vector<int> s_vQueue;
	static size_t s_QueuePos = 0;
	static int s_Done = 0, s_Failed = 0;
	static bool s_Loaded = false;
	static std::string s_HoveredName;

	const auto SkinPath = [&](int Index, char *pBuf, size_t Size) { str_format(pBuf, Size, "skins/%s.png", s_vNames[Index].c_str()); };
	if(!s_Loaded)
	{
		s_Loaded = true;
		void *pData;
		unsigned Size;
		if(Storage()->ReadFile("skin_names.txt", IStorage::TYPE_ALL, &pData, &Size))
		{
			const std::string All((const char *)pData, Size);
			free(pData);
			size_t Pos = 0;
			while(Pos < All.size())
			{
				size_t End = All.find('\n', Pos);
				if(End == std::string::npos)
					End = All.size();
				std::string Line = All.substr(Pos, End - Pos);
				while(!Line.empty() && (Line.back() == '\r' || Line.back() == ' '))
					Line.pop_back();
				if(!Line.empty() && CSkin::IsValidName(Line.c_str()))
					s_vNames.push_back(Line);
				Pos = End + 1;
			}
		}
		s_vHave.assign(s_vNames.size(), 0);
		s_vButtons = std::vector<CButtonContainer>(s_vNames.size());
		Storage()->CreateFolder("skins", IStorage::TYPE_SAVE);
		for(size_t i = 0; i < s_vNames.size(); i++)
		{
			char aPath[128];
			SkinPath((int)i, aPath, sizeof(aPath));
			s_vHave[i] = Storage()->FileExists(aPath, IStorage::TYPE_ALL);
		}
	}

	// progress of the running downloads
	bool AnyFinished = false;
	for(size_t i = 0; i < s_vActive.size();)
	{
		const EHttpState State = s_vActive[i]->State();
		if(State == EHttpState::RUNNING || State == EHttpState::QUEUED)
		{
			i++;
			continue;
		}
		const int Index = s_vActiveIndex[i];
		if(State == EHttpState::DONE)
		{
			s_Done++;
			s_vHave[Index] = 1;
		}
		else
		{
			s_Failed++;
			char aPath[128];
			SkinPath(Index, aPath, sizeof(aPath));
			Storage()->RemoveFile(aPath, IStorage::TYPE_SAVE);
		}
		AnyFinished = true;
		s_vActive.erase(s_vActive.begin() + i);
		s_vActiveIndex.erase(s_vActiveIndex.begin() + i);
	}
	while(s_vActive.size() < 6 && s_QueuePos < s_vQueue.size())
	{
		const int Index = s_vQueue[s_QueuePos++];
		char aPath[128], aEscaped[128], aUrl[256];
		SkinPath(Index, aPath, sizeof(aPath));
		EscapeUrl(aEscaped, s_vNames[Index].c_str());
		str_format(aUrl, sizeof(aUrl), "%s%s.png", s_aSources[s_Source].m_pBase, aEscaped);
		std::shared_ptr<IHttpRequest> pRequest = HttpGetFile(aUrl, Storage(), aPath, IStorage::TYPE_SAVE);
		pRequest->MaxResponseSize(2 * 1024 * 1024);
		pRequest->LogProgress(HTTPLOG::NONE);
		Http()->Run(pRequest);
		s_vActive.push_back(pRequest);
		s_vActiveIndex.push_back(Index);
	}
	if(AnyFinished && s_vActive.empty() && s_QueuePos >= s_vQueue.size())
		GameClient()->m_Skins.SkinList().ForceRefresh();

	// source buttons
	CUIRect Row, Button;
	MainView.HSplitTop(24.0f, &Row, &MainView);
	static CButtonContainer s_aSourceButtons[3];
	const float ButtonWidth = (Row.w - 12.0f) / 3.0f;
	for(int i = 0; i < 3; i++)
	{
		Row.VSplitLeft(ButtonWidth, &Button, &Row);
		Row.VSplitLeft(6.0f, nullptr, &Row);
		if(DoButton_Menu(&s_aSourceButtons[i], s_aSources[i].m_pName, s_Source == i, &Button))
		{
			s_Source = i;
			// previews use the same download settings as the game, so they follow the chosen source
			g_Config.m_ClDownloadSkins = 1;
			if(i != 1)
			{
				g_Config.m_ClDownloadCommunitySkins = 1;
				str_copy(g_Config.m_ClSkinCommunityDownloadUrl, s_aSources[i].m_pBase, sizeof(g_Config.m_ClSkinCommunityDownloadUrl));
			}
			GameClient()->m_Skins.SkinList().ForceRefresh();
		}
	}
	MainView.HSplitTop(6.0f, nullptr, &MainView);

	// search and status
	CUIRect Search, Edit, Status;
	MainView.HSplitTop(22.0f, &Search, &MainView);
	Search.VSplitLeft(260.0f, &Edit, &Status);
	Ui()->DoEditBox(&s_Filter, &Edit, 13.0f);
	char aStatus[160];
	str_format(aStatus, sizeof(aStatus), "  Всего %d, скачано %d, ошибок %d, в очереди %d   %s", (int)s_vNames.size(), s_Done, s_Failed, (int)(s_vQueue.size() - s_QueuePos + s_vActive.size()), s_HoveredName.c_str());
	Ui()->DoLabel(&Status, aStatus, 12.0f, TEXTALIGN_ML);
	MainView.HSplitTop(6.0f, nullptr, &MainView);

	if(s_LastFilter != s_Filter.GetString())
	{
		s_LastFilter = s_Filter.GetString();
		s_vShown.clear();
		for(size_t i = 0; i < s_vNames.size(); i++)
		{
			if(s_LastFilter.empty() || str_find_nocase(s_vNames[i].c_str(), s_LastFilter.c_str()))
				s_vShown.push_back((int)i);
		}
	}

	// bottom: buttons and the hint for ddstats
	CUIRect Bottom;
	MainView.HSplitBottom(92.0f, &MainView, &Bottom);
	CUIRect ActionRow;
	Bottom.HSplitTop(22.0f, &ActionRow, &Bottom);
	CUIRect AllButton, StopButton, ApplyButton;
	ActionRow.VSplitLeft(250.0f, &AllButton, &ActionRow);
	ActionRow.VSplitLeft(6.0f, nullptr, &ActionRow);
	ActionRow.VSplitLeft(130.0f, &StopButton, &ActionRow);
	ActionRow.VSplitLeft(6.0f, nullptr, &ApplyButton);
	char aAll[96];
	str_format(aAll, sizeof(aAll), "Скачать все показанные (%d)", (int)s_vShown.size());
	static CButtonContainer s_AllButton, s_StopButton, s_ApplyButton;
	if(DoButton_Menu(&s_AllButton, aAll, 0, &AllButton) && s_QueuePos >= s_vQueue.size())
	{
		s_vQueue.clear();
		s_QueuePos = 0;
		s_Done = s_Failed = 0;
		for(const int Index : s_vShown)
			if(!s_vHave[Index])
				s_vQueue.push_back(Index);
	}
	if(DoButton_Menu(&s_StopButton, "Остановить", 0, &StopButton))
		s_QueuePos = s_vQueue.size();
	if(DoButton_Menu(&s_ApplyButton, "Включить скины сообщества в игре", g_Config.m_ClDownloadCommunitySkins, &ApplyButton))
	{
		g_Config.m_ClDownloadCommunitySkins = 1;
		str_copy(g_Config.m_ClSkinCommunityDownloadUrl, s_aSources[s_Source].m_pBase, sizeof(g_Config.m_ClSkinCommunityDownloadUrl));
	}
	Bottom.HSplitTop(4.0f, nullptr, &Bottom);
	CUIRect Hint;
	Bottom.HSplitTop(14.0f, &Hint, &Bottom);
	Ui()->DoLabel(&Hint, "Чтобы в игре показывались скины из ddstats, напишите в консоли (F1):", 11.0f, TEXTALIGN_ML);
	Bottom.HSplitTop(14.0f, &Hint, &Bottom);
	Ui()->DoLabel(&Hint, "cl_download_community_skins 1; cl_skin_community_download_url \"https://skins.ddstats.tw/\"", 11.0f, TEXTALIGN_ML);
	Bottom.HSplitTop(14.0f, &Hint, &Bottom);
	Ui()->DoLabel(&Hint, "Скачанные скины сразу попадают в папку со скинами. Имена, которых нет на выбранном сервере, дадут ошибку.", 10.0f, TEXTALIGN_ML);

	// grid of tees: the skin is shown as it looks in the game, the name only appears while hovering
	static CScrollRegion s_Scroll;
	static std::vector<char> s_vQueued;
	if(s_vQueued.size() != s_vNames.size())
		s_vQueued.assign(s_vNames.size(), 0);
	for(size_t i = 0; i < s_vActiveIndex.size(); i++)
		s_vQueued[s_vActiveIndex[i]] = 1;
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = 60.0f;
	CUIRect Content = MainView;
	s_Scroll.Begin(&Content, &ScrollParams);
	const float Cell = 50.0f, Gap = 4.0f;
	const int Columns = std::max(1, (int)((Content.w - 10.0f) / (Cell + Gap)));
	int RowIndex = 0;
	const void *pHovered = nullptr;
	int HoveredIndex = -1;
	for(size_t Start = 0; Start < s_vShown.size(); Start += Columns, RowIndex++)
	{
		CUIRect RowRect;
		Content.HSplitTop(Cell + Gap, &RowRect, &Content);
		if(!s_Scroll.AddRect(RowRect))
			continue;
		for(int c = 0; c < Columns && Start + c < s_vShown.size(); c++)
		{
			const int Index = s_vShown[Start + c];
			CUIRect CellRect = {RowRect.x + c * (Cell + Gap), RowRect.y, Cell, Cell};
			const void *pId = &s_vNames[Index];
			const int Clicked = Ui()->DoButtonLogic(pId, 0, &CellRect, BUTTONFLAG_LEFT);
			const bool Hot = Ui()->HotItem() == pId;
			if(Hot)
			{
				pHovered = pId;
				HoveredIndex = Index;
			}
			ColorRGBA Back(0.0f, 0.0f, 0.0f, 0.3f);
			if(s_vHave[Index])
				Back = ColorRGBA(0.2f, 0.7f, 0.3f, 0.45f);
			else if(s_vQueued[Index])
				Back = ColorRGBA(0.9f, 0.7f, 0.2f, 0.45f);
			if(Hot)
				Back.a += 0.25f;
			CellRect.Draw(Back, IGraphics::CORNER_ALL, 6.0f);
			const CSkin *pSkin = GameClient()->m_Skins.FindOrNullptr(s_vNames[Index].c_str());
			if(pSkin != nullptr)
			{
				CTeeRenderInfo Info;
				Info.Apply(pSkin);
				Info.m_Size = 38.0f;
				RenderTools()->RenderIdleTeeAtCenter(&Info, vec2(CellRect.x + CellRect.w / 2.0f, CellRect.y + CellRect.h / 2.0f));
			}
			else
				Ui()->DoLabel(&CellRect, "...", 12.0f, TEXTALIGN_MC);
			if(Clicked && !s_vHave[Index] && !s_vQueued[Index])
			{
				if(s_QueuePos >= s_vQueue.size())
				{
					s_vQueue.clear();
					s_QueuePos = 0;
				}
				s_vQueue.push_back(Index);
				s_vQueued[Index] = 1;
			}
		}
	}
	s_Scroll.End();
	(void)pHovered;
	s_HoveredName = HoveredIndex >= 0 ? s_vNames[HoveredIndex] : std::string();
}
