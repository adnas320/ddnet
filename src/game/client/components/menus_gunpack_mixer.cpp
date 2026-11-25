// Gunpack mixer: drag parts of the game image from a donor pack into a result image and save it as a new gunpack.
#include "menus.h"

#include <base/color.h>
#include <base/io.h>
#include <base/str.h>

#include <engine/client.h>
#include <engine/gfx/image_loader.h>
#include <engine/gfx/image_manipulation.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <generated/client_data.h>

#include <game/client/gameclient.h>
#include <game/client/ui.h>

#include <algorithm>
#include <string>
#include <vector>

namespace {

struct CMixPart
{
	int m_X, m_Y, m_W, m_H; // in grid cells of the game image (32 x 16)
};

int MixerScan(const char *pName, int IsDir, int StorageType, void *pUser)
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

struct CMixerState
{
	std::vector<std::string> m_vPacks;
	std::vector<CMixPart> m_vParts;
	std::vector<char> m_vChanged;
	int m_Donor = 0;
	int m_Base = 0;
	CImageInfo m_DonorImage, m_ResultImage, m_BaseImage;
	IGraphics::CTextureHandle m_DonorTexture, m_ResultTexture;
	bool m_DonorDirty = true, m_ResultDirty = true, m_BaseDirty = true;
	int m_Drag = -1;
	char m_aStatus[160] = "";
	CLineInputBuffered<32> m_Name;
	bool m_Init = false;
};

} // namespace

void CMenus::RenderGunpackMixer(CUIRect MainView)
{
	static CMixerState s_S;
	CMixerState &S = s_S;

	const auto LoadPack = [&](const std::string &Name, CImageInfo &Image) -> bool {
		Image.Free();
		bool Loaded;
		char aPath[IO_MAX_PATH_LENGTH];
		if(Name == "default")
			Loaded = Graphics()->LoadPng(Image, "game.png", IStorage::TYPE_ALL);
		else
		{
			str_format(aPath, sizeof(aPath), "assets/game/%s.png", Name.c_str());
			Loaded = Graphics()->LoadPng(Image, aPath, IStorage::TYPE_ALL);
			if(!Loaded)
			{
				str_format(aPath, sizeof(aPath), "assets/game/%s/game.png", Name.c_str());
				Loaded = Graphics()->LoadPng(Image, aPath, IStorage::TYPE_ALL);
			}
		}
		if(Loaded && Image.m_Format != CImageInfo::FORMAT_RGBA)
			Loaded = ConvertToRgba(Image);
		return Loaded && Image.m_Width >= 32 && Image.m_Height >= 16;
	};

	if(!S.m_Init)
	{
		S.m_Init = true;
		S.m_Name.Set("my_asset");
		S.m_vPacks.emplace_back("default");
		std::vector<std::string> vFound;
		Storage()->ListDirectory(IStorage::TYPE_ALL, "assets/game", MixerScan, &vFound);
		std::sort(vFound.begin(), vFound.end());
		vFound.erase(std::unique(vFound.begin(), vFound.end()), vFound.end());
		for(const std::string &Name : vFound)
			if(Name != "default")
				S.m_vPacks.push_back(Name);
		// all sprites of the game image can be moved on their own
		const CDataSpriteset *pGameSet = g_pData->m_aSprites[SPRITE_PICKUP_HEALTH].m_pSet;
		for(int i = 0; i < NUM_SPRITES; i++)
		{
			const CDataSprite &Sprite = g_pData->m_aSprites[i];
			if(Sprite.m_pSet == pGameSet && Sprite.m_W > 0 && Sprite.m_H > 0)
				S.m_vParts.push_back({Sprite.m_X, Sprite.m_Y, Sprite.m_W, Sprite.m_H});
		}
		S.m_vChanged.assign(S.m_vParts.size(), 0);
	}

	// the donor and the base of the result are loaded on demand
	if(S.m_DonorDirty)
	{
		S.m_DonorDirty = false;
		Graphics()->UnloadTexture(&S.m_DonorTexture);
		if(LoadPack(S.m_vPacks[S.m_Donor], S.m_DonorImage))
			S.m_DonorTexture = Graphics()->LoadTextureRaw(S.m_DonorImage, 0, "mixer donor");
		else
			str_copy(S.m_aStatus, "Не удалось открыть ганпак-донор.");
	}
	if(S.m_BaseDirty)
	{
		S.m_BaseDirty = false;
		if(LoadPack(S.m_vPacks[S.m_Base], S.m_BaseImage))
		{
			S.m_ResultImage.Free();
			S.m_ResultImage = S.m_BaseImage.DeepCopy();
			std::fill(S.m_vChanged.begin(), S.m_vChanged.end(), 0);
			S.m_ResultDirty = true;
		}
		else
			str_copy(S.m_aStatus, "Не удалось открыть основу результата.");
	}
	if(S.m_ResultDirty && S.m_ResultImage.m_pData != nullptr)
	{
		S.m_ResultDirty = false;
		Graphics()->UnloadTexture(&S.m_ResultTexture);
		S.m_ResultTexture = Graphics()->LoadTextureRaw(S.m_ResultImage, 0, "mixer result");
	}

	// copies the area of one part from one image to another, the images may have different sizes
	const auto CopyPart = [&](const CImageInfo &From, CImageInfo &To, const CMixPart &Part) {
		if(From.m_pData == nullptr || To.m_pData == nullptr)
			return;
		const size_t DX0 = Part.m_X * To.m_Width / 32, DX1 = (Part.m_X + Part.m_W) * To.m_Width / 32;
		const size_t DY0 = Part.m_Y * To.m_Height / 16, DY1 = (Part.m_Y + Part.m_H) * To.m_Height / 16;
		for(size_t y = DY0; y < DY1 && y < To.m_Height; y++)
			for(size_t x = DX0; x < DX1 && x < To.m_Width; x++)
			{
				const size_t SX = std::min(From.m_Width - 1, (x - DX0) * (Part.m_W * From.m_Width / 32) / std::max<size_t>(1, DX1 - DX0) + Part.m_X * From.m_Width / 32);
				const size_t SY = std::min(From.m_Height - 1, (y - DY0) * (Part.m_H * From.m_Height / 16) / std::max<size_t>(1, DY1 - DY0) + Part.m_Y * From.m_Height / 16);
				for(int c = 0; c < 4; c++)
					To.m_pData[(y * To.m_Width + x) * 4 + c] = From.m_pData[(SY * From.m_Width + SX) * 4 + c];
			}
	};

	// ---- top row: close, name, export, reset ----
	CUIRect Top, Button;
	MainView.HSplitTop(30.0f, &Top, &MainView);
	static CButtonContainer s_Close, s_Export, s_Reset, s_Folder;
	Top.VSplitLeft(30.0f, &Button, &Top);
	if(DoButton_Menu(&s_Close, "X", 0, &Button))
		m_ShowGunpackMixer = false;
	Top.VSplitLeft(6.0f, nullptr, &Top);
	CUIRect NameRect, ExportRect, ResetRect, FolderRect;
	Top.VSplitRight(110.0f, &Top, &FolderRect);
	Top.VSplitRight(6.0f, &Top, nullptr);
	Top.VSplitRight(90.0f, &Top, &ResetRect);
	Top.VSplitRight(6.0f, &Top, nullptr);
	Top.VSplitRight(130.0f, &NameRect, &ExportRect);
	Ui()->DoEditBox(&S.m_Name, &NameRect, 13.0f);
	if(DoButton_Menu(&s_Export, "Скачать ганпак", 0, &ExportRect))
	{
		char aName[64];
		str_copy(aName, S.m_Name.GetString(), sizeof(aName));
		for(char *p = aName; *p; p++)
			if(!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_' || *p == '-'))
				*p = '_';
		if(aName[0] == '\0' || str_comp(aName, "default") == 0)
			str_copy(aName, "my_asset");
		Storage()->CreateFolder("assets", IStorage::TYPE_SAVE);
		Storage()->CreateFolder("assets/game", IStorage::TYPE_SAVE);
		char aPath[IO_MAX_PATH_LENGTH];
		str_format(aPath, sizeof(aPath), "assets/game/%s.png", aName);
		IOHANDLE File = Storage()->OpenFile(aPath, IOFLAG_WRITE, IStorage::TYPE_SAVE);
		if(CImageLoader::SavePng(File, aPath, S.m_ResultImage))
		{
			str_copy(g_Config.m_ClAssetGame, aName, sizeof(g_Config.m_ClAssetGame));
			GameClient()->LoadGameSkin(g_Config.m_ClAssetGame);
			str_format(S.m_aStatus, sizeof(S.m_aStatus), "Сохранено: assets/game/%s.png, ганпак включён.", aName);
		}
		else
			str_copy(S.m_aStatus, "Не удалось сохранить файл.");
	}
	if(DoButton_Menu(&s_Reset, "Сбросить", 0, &ResetRect))
		S.m_BaseDirty = true;
	if(DoButton_Menu(&s_Folder, "Папка ганпаков", 0, &FolderRect))
	{
		char aFull[IO_MAX_PATH_LENGTH];
		Storage()->CreateFolder("assets", IStorage::TYPE_SAVE);
		Storage()->CreateFolder("assets/game", IStorage::TYPE_SAVE);
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, "assets/game", aFull, sizeof(aFull));
		Client()->ViewFile(aFull);
	}
	MainView.HSplitTop(8.0f, nullptr, &MainView);

	// ---- two panels ----
	CUIRect Bottom;
	MainView.HSplitBottom(22.0f, &MainView, &Bottom);
	CUIRect PanelLeft, PanelRight;
	MainView.VSplitMid(&PanelLeft, &PanelRight, 26.0f);
	Ui()->DoLabel(&Bottom, S.m_aStatus[0] ? S.m_aStatus : "Перетащите часть слева направо. Правый клик по части справа возвращает её как было.", 11.0f, TEXTALIGN_MC);

	static CButtonContainer s_aPrev[2], s_aNext[2];
	CUIRect aImageRect[2];
	for(int Side = 0; Side < 2; Side++)
	{
		CUIRect Panel = Side == 0 ? PanelLeft : PanelRight;
		Panel.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.3f), IGraphics::CORNER_ALL, 10.0f);
		Panel.Margin(8.0f, &Panel);
		CUIRect Header, Prev, Next;
		Panel.HSplitTop(24.0f, &Header, &Panel);
		Header.VSplitLeft(26.0f, &Prev, &Header);
		Header.VSplitRight(26.0f, &Header, &Next);
		int &Index = Side == 0 ? S.m_Donor : S.m_Base;
		int Step = 0;
		if(DoButton_Menu(&s_aPrev[Side], "<", 0, &Prev))
			Step = -1;
		if(DoButton_Menu(&s_aNext[Side], ">", 0, &Next))
			Step = 1;
		if(Step != 0)
		{
			Index = (Index + Step + (int)S.m_vPacks.size()) % (int)S.m_vPacks.size();
			(Side == 0 ? S.m_DonorDirty : S.m_BaseDirty) = true;
		}
		char aTitle[96];
		str_format(aTitle, sizeof(aTitle), "%s: %s", Side == 0 ? "Донор" : "Результат (основа)", S.m_vPacks[Index].c_str());
		Ui()->DoLabel(&Header, aTitle, 14.0f, TEXTALIGN_MC);
		Panel.HSplitTop(6.0f, nullptr, &Panel);
		// the image keeps the 2:1 proportion
		float W = Panel.w, H = W / 2.0f;
		if(H > Panel.h)
		{
			H = Panel.h;
			W = H * 2.0f;
		}
		aImageRect[Side] = {Panel.x + (Panel.w - W) / 2.0f, Panel.y, W, H};
	}

	// which part is under the mouse
	const vec2 Mouse = Ui()->MousePos();
	const auto PartAt = [&](const CUIRect &Rect) -> int {
		if(!Ui()->MouseInside(&Rect))
			return -1;
		const float CX = (Mouse.x - Rect.x) / Rect.w * 32.0f, CY = (Mouse.y - Rect.y) / Rect.h * 16.0f;
		int Best = -1;
		int BestArea = 1 << 30;
		for(size_t i = 0; i < S.m_vParts.size(); i++)
		{
			const CMixPart &P = S.m_vParts[i];
			if(CX >= P.m_X && CX < P.m_X + P.m_W && CY >= P.m_Y && CY < P.m_Y + P.m_H && P.m_W * P.m_H < BestArea)
			{
				Best = (int)i;
				BestArea = P.m_W * P.m_H;
			}
		}
		return Best;
	};
	const int HoverDonor = PartAt(aImageRect[0]);
	const int HoverResult = PartAt(aImageRect[1]);

	if(Ui()->MouseButtonClicked(0) && HoverDonor >= 0)
		S.m_Drag = HoverDonor;
	if(Ui()->MouseButtonClicked(1) && HoverResult >= 0)
	{
		CopyPart(S.m_BaseImage, S.m_ResultImage, S.m_vParts[HoverResult]);
		S.m_vChanged[HoverResult] = 0;
		S.m_ResultDirty = true;
		str_copy(S.m_aStatus, "Часть возвращена.");
	}
	const bool Released = S.m_Drag >= 0 && !Ui()->MouseButton(0);
	if(Released)
	{
		if(Ui()->MouseInside(&aImageRect[1]))
		{
			CopyPart(S.m_DonorImage, S.m_ResultImage, S.m_vParts[S.m_Drag]);
			S.m_vChanged[S.m_Drag] = 1;
			S.m_ResultDirty = true;
			str_copy(S.m_aStatus, "Часть перенесена в результат.");
		}
		S.m_Drag = -1;
	}

	// draw the images and the outlines
	const IGraphics::CTextureHandle aTextures[2] = {S.m_DonorTexture, S.m_ResultTexture};
	for(int Side = 0; Side < 2; Side++)
	{
		const CUIRect &R = aImageRect[Side];
		R.Draw(ColorRGBA(0.12f, 0.12f, 0.15f, 0.9f), IGraphics::CORNER_ALL, 4.0f);
		if(aTextures[Side].IsValid())
		{
			Graphics()->TextureSet(aTextures[Side]);
			Graphics()->QuadsBegin();
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
			const IGraphics::CQuadItem Item(R.x, R.y, R.w, R.h);
			Graphics()->QuadsDrawTL(&Item, 1);
			Graphics()->QuadsEnd();
		}
	}
	const auto Outline = [&](const CUIRect &R, int Part, ColorRGBA Col) {
		const CMixPart &P = S.m_vParts[Part];
		const float X = R.x + P.m_X / 32.0f * R.w, Y = R.y + P.m_Y / 16.0f * R.h, W = P.m_W / 32.0f * R.w, H = P.m_H / 16.0f * R.h;
		const float T = 1.5f;
		CUIRect{X, Y, W, T}.Draw(Col, IGraphics::CORNER_NONE, 0.0f);
		CUIRect{X, Y + H - T, W, T}.Draw(Col, IGraphics::CORNER_NONE, 0.0f);
		CUIRect{X, Y, T, H}.Draw(Col, IGraphics::CORNER_NONE, 0.0f);
		CUIRect{X + W - T, Y, T, H}.Draw(Col, IGraphics::CORNER_NONE, 0.0f);
	};
	for(size_t i = 0; i < S.m_vParts.size(); i++)
		if(S.m_vChanged[i])
			Outline(aImageRect[1], (int)i, ColorRGBA(0.3f, 1.0f, 0.5f, 0.8f));
	if(S.m_Drag >= 0)
	{
		Outline(aImageRect[0], S.m_Drag, ColorRGBA(1.0f, 0.85f, 0.3f, 1.0f));
		if(Ui()->MouseInside(&aImageRect[1]))
			Outline(aImageRect[1], S.m_Drag, ColorRGBA(1.0f, 0.85f, 0.3f, 0.7f));
		// the dragged part follows the mouse
		if(S.m_DonorTexture.IsValid())
		{
			const CMixPart &P = S.m_vParts[S.m_Drag];
			const float W = P.m_W / 32.0f * aImageRect[0].w, H = P.m_H / 16.0f * aImageRect[0].h;
			Graphics()->TextureSet(S.m_DonorTexture);
			Graphics()->QuadsBegin();
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.8f);
			Graphics()->QuadsSetSubset(P.m_X / 32.0f, P.m_Y / 16.0f, (P.m_X + P.m_W) / 32.0f, (P.m_Y + P.m_H) / 16.0f);
			const IGraphics::CQuadItem Item(Mouse.x - W / 2.0f, Mouse.y - H / 2.0f, W, H);
			Graphics()->QuadsDrawTL(&Item, 1);
			Graphics()->QuadsEnd();
		}
	}
	else
	{
		if(HoverDonor >= 0)
			Outline(aImageRect[0], HoverDonor, ColorRGBA(1.0f, 1.0f, 1.0f, 0.6f));
		if(HoverResult >= 0)
			Outline(aImageRect[1], HoverResult, ColorRGBA(1.0f, 1.0f, 1.0f, 0.6f));
	}

	// arrows between the images
	CUIRect Arrow = {PanelLeft.x + PanelLeft.w, PanelLeft.y, 26.0f, PanelLeft.h};
	Ui()->DoLabel(&Arrow, ">", 26.0f, TEXTALIGN_MC);
}
