/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "menus_start.h"

#include <base/time.h>

#include <engine/client/updater.h>
#include <engine/font_icons.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <generated/client_data.h>

#include <game/client/animstate.h>
#include <game/client/gameclient.h>
#include <game/client/lineinput.h>
#include <game/client/ui.h>
#include <game/localization.h>
#include <game/version.h>

#if defined(CONF_PLATFORM_ANDROID)
#include <android/android_main.h>
#endif

#include <algorithm>
#include <unordered_map>
#include <cmath>
#include <ctime>
#include <string>
#include <vector>

// уровни огня после 100: у каждого свой цвет и своё число искр вокруг
static const int STREAK_TIERS[] = {150, 200, 225, 275, 300, 350, 375, 400, 425, 450, 500};
static const int NUM_STREAK_TIERS = (int)(sizeof(STREAK_TIERS) / sizeof(STREAK_TIERS[0]));

static int StreakTier(int Count)
{
	int Tier = 0;
	for(int Value : STREAK_TIERS)
		if(Count >= Value)
			Tier++;
	return Tier;
}

static bool IsStreakMilestone(int Count)
{
	if(Count == 25 || Count == 50 || Count == 75 || Count == 100)
		return true;
	for(int Value : STREAK_TIERS)
		if(Count == Value)
			return true;
	return false;
}

static ColorRGBA StreakTierColor(int Tier, double Time)
{
	static const ColorRGBA s_aColors[10] = {
		ColorRGBA(0.80f, 0.32f, 1.00f, 1.0f), // 150
		ColorRGBA(1.00f, 0.36f, 0.82f, 1.0f), // 200
		ColorRGBA(1.00f, 0.30f, 0.45f, 1.0f), // 225
		ColorRGBA(0.34f, 0.52f, 1.00f, 1.0f), // 275
		ColorRGBA(0.30f, 0.80f, 1.00f, 1.0f), // 300
		ColorRGBA(0.20f, 1.00f, 0.88f, 1.0f), // 350
		ColorRGBA(0.45f, 1.00f, 0.50f, 1.0f), // 375
		ColorRGBA(1.00f, 0.84f, 0.30f, 1.0f), // 400
		ColorRGBA(1.00f, 0.95f, 0.72f, 1.0f), // 425
		ColorRGBA(0.86f, 0.95f, 1.00f, 1.0f), // 450
	};
	if(Tier <= 0)
		return ColorRGBA(0.62f, 0.28f, 1.0f, 1.0f);
	if(Tier <= 10)
		return s_aColors[Tier - 1];
	// 500: радужный
	const float H = std::fmod((float)Time * 0.25f, 1.0f);
	return color_cast<ColorRGBA>(ColorHSLA(H, 0.85f, 0.62f));
}

static std::string FormatPlayTime(int Seconds)
{
	char aBuf[48];
	const int Hours = Seconds / 3600;
	const int Minutes = (Seconds / 60) % 60;
	if(Hours > 0)
		str_format(aBuf, sizeof(aBuf), "%d ч %d мин", Hours, Minutes);
	else
		str_format(aBuf, sizeof(aBuf), "%d мин", Minutes);
	return aBuf;
}

void CMenusStart::RenderStartMenu(CUIRect MainView)
{
	GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_START);

	// render logo
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_BANNER].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1, 1, 1, 1);
	IGraphics::CQuadItem QuadItem(MainView.w / 2 - 170, 60, 360, 103);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();

	const float Rounding = 10.0f;
	const float VMargin = MainView.w / 2 - 190.0f;

	CUIRect Button;
	int NewPage = -1;

	// the big start menu buttons grow when the mouse is over them
	static std::unordered_map<const void *, float> s_StartGrow;
	const auto GrowStartButton = [&](CUIRect &Rect, const void *pId) {
		float &Grow = s_StartGrow[pId];
		const float Target = Ui()->HotItem() == pId ? 1.0f : 0.0f;
		Grow += (Target - Grow) * std::min(1.0f, Client()->RenderFrameTime() * 14.0f);
		Rect.x -= 8.0f * Grow;
		Rect.w += 16.0f * Grow;
		Rect.y -= 3.0f * Grow;
		Rect.h += 6.0f * Grow;
	};

	// ---- daily streak flame (bottom left)
	{
		const auto LocalDay = [&]() -> long long {
			const time_t Now = ::time(nullptr);
			tm Local = *::localtime(&Now);
			tm Utc = *::gmtime(&Now);
			Local.tm_isdst = 0;
			Utc.tm_isdst = 0;
			const long long Offset = (long long)::difftime(::mktime(&Local), ::mktime(&Utc));
			return ((long long)Now + Offset) / 86400 + g_Config.m_ClStreakDayOffset;
		};
		const long long Today = LocalDay();
		const bool HasStreak = g_Config.m_ClStreakLastDay > 0 && g_Config.m_ClStreakCount > 0;
		const long long Diff = HasStreak ? Today - g_Config.m_ClStreakLastDay : 999;
		const bool Alive = HasStreak && Diff <= 1; // not lost yet
		const bool Lit = HasStreak && Diff == 0; // already pressed today
		const int Count = Alive ? g_Config.m_ClStreakCount : 0;

		static double s_AnimStart = -100.0;
		static int s_AnimTier = 0;
		const double Now = (double)time_get() / (double)time_freq();

		const float SquareSize = 54.0f;
		CUIRect Square;
		Square.x = MainView.x + 30.0f;
		Square.w = SquareSize;
		Square.h = SquareSize;
		Square.y = MainView.y + MainView.h - 20.0f - 18.0f - SquareSize;
		CUIRect CountRect(Square.x - 10.0f, Square.y + Square.h + 1.0f, Square.w + 20.0f, 18.0f);
		CUIRect SkipLabel(Square.x - 10.0f, Square.y - 66.0f, 140.0f, 14.0f);
		CUIRect SkipInputRect(Square.x - 10.0f, Square.y - 50.0f, 56.0f, 20.0f);
		CUIRect SkipWith(Square.x - 10.0f, Square.y - 26.0f, 66.0f, 20.0f);
		CUIRect SkipWithout(Square.x + 60.0f, Square.y - 26.0f, 66.0f, 20.0f);

		// тестовый пропуск дней: сколько дней — выбираешь сам
		{
			static CLineInputBuffered<5> s_SkipDays;
			static bool s_SkipInit = false;
			if(!s_SkipInit)
			{
				s_SkipInit = true;
				s_SkipDays.Set("1");
			}
			TextRender()->TextColor(0.8f, 0.7f, 1.0f, 0.9f);
			Ui()->DoLabel(&SkipLabel, "Скип дней (тест):", 10.0f, TEXTALIGN_ML);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
			Ui()->DoEditBox(&s_SkipDays, &SkipInputRect, 11.0f);
			const int Days = std::clamp(str_toint(s_SkipDays.GetString()), 1, 1000);
			static CButtonContainer s_SkipWith, s_SkipWithout;
			if(GameClient()->m_Menus.DoButton_Menu(&s_SkipWith, "С огнём", 0, &SkipWith, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.55f, 0.25f, 0.9f, 0.45f)))
			{
				// как будто ты заходил и жал на огонь каждый из этих дней
				const int Base = Count;
				const int NewCount = std::min(100000, Base + Days);
				g_Config.m_ClStreakDayOffset = std::min(100000, g_Config.m_ClStreakDayOffset + Days);
				g_Config.m_ClStreakLastDay = (int)(Today + Days);
				g_Config.m_ClStreakCount = NewCount;
				int Hit = 0;
				for(int Value = Base + 1; Value <= NewCount; Value++)
					if(IsStreakMilestone(Value))
						Hit = Value;
				s_AnimStart = Now;
				s_AnimTier = Hit > 0 ? Hit : 1;
			}
			if(GameClient()->m_Menus.DoButton_Menu(&s_SkipWithout, "Без огня", 0, &SkipWithout, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.3f)))
				g_Config.m_ClStreakDayOffset = std::min(100000, g_Config.m_ClStreakDayOffset + Days);
		}

		static CButtonContainer s_FlameButton;
		const bool Hover = Ui()->HotItem() == &s_FlameButton;
		if(Ui()->DoButtonLogic(&s_FlameButton, 0, &Square, BUTTONFLAG_LEFT) && !Lit)
		{
			if(!Alive)
				g_Config.m_ClStreakCount = 1;
			else
				g_Config.m_ClStreakCount++;
			g_Config.m_ClStreakLastDay = (int)Today;
			const int NewCount = g_Config.m_ClStreakCount;
			if(IsStreakMilestone(NewCount))
			{
				s_AnimStart = Now;
				s_AnimTier = NewCount;
			}
			else
			{
				s_AnimStart = Now;
				s_AnimTier = 1; // a small flash for every new day
			}
		}

		// color by tier: orange, then a little more violet at 10, 25, 50, 75 and fully violet at 100
		const int Shown = std::max(Count, Lit ? g_Config.m_ClStreakCount : 0);
		float Violet = 0.0f;
		if(Shown >= 100)
			Violet = 1.0f;
		else if(Shown >= 75)
			Violet = 0.75f;
		else if(Shown >= 50)
			Violet = 0.5f;
		else if(Shown >= 25)
			Violet = 0.3f;
		else if(Shown >= 10)
			Violet = 0.12f;
		const ColorRGBA Orange(1.0f, 0.55f, 0.1f, 1.0f);
		const ColorRGBA Purple(0.62f, 0.28f, 1.0f, 1.0f);
		ColorRGBA FlameColor(Orange.r + (Purple.r - Orange.r) * Violet, Orange.g + (Purple.g - Orange.g) * Violet, Orange.b + (Purple.b - Orange.b) * Violet, 1.0f);
		const int Tier = StreakTier(Shown);
		if(Tier > 0)
			FlameColor = StreakTierColor(Tier, Now);
		if(Shown > g_Config.m_ClStreakBest)
			g_Config.m_ClStreakBest = Shown;
		const vec2 Center(Square.x + Square.w * 0.5f, Square.y + Square.h * 0.5f);

		// the square
		Square.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, Hover ? 0.45f : 0.3f), IGraphics::CORNER_ALL, 8.0f);
		if(Lit)
		{
			// a glow behind the lit flame
			Graphics()->TextureClear();
			Graphics()->BlendAdditive();
			Graphics()->QuadsBegin();
			for(int Ring = 0; Ring < 4; Ring++)
			{
				Graphics()->SetColor(FlameColor.r, FlameColor.g, FlameColor.b, 0.06f + 0.015f * std::sin((float)Now * 4.0f));
				Graphics()->DrawCircle(Center.x, Center.y, 12.0f + Ring * 5.0f, 24);
			}
			Graphics()->QuadsEnd();
			Graphics()->BlendNormal();
		}
		if(Lit && Tier > 0)
		{
			// аура уровня: чем выше уровень, тем больше искр вокруг пламени
			Graphics()->TextureClear();
			Graphics()->BlendAdditive();
			Graphics()->QuadsBegin();
			const int Orbs = 3 + Tier;
			for(int Orb = 0; Orb < Orbs; Orb++)
			{
				const float Angle = (float)Now * (0.8f + 0.06f * (Orb % 3)) + 2.0f * pi * Orb / Orbs;
				const float Radius = 24.0f + 4.0f * std::sin((float)Now * 1.7f + Orb);
				Graphics()->SetColor(FlameColor.r, FlameColor.g, FlameColor.b, 0.85f);
				Graphics()->DrawCircle(Center.x + std::cos(Angle) * Radius, Center.y + std::sin(Angle) * Radius * 0.9f, 1.6f, 8);
				Graphics()->SetColor(FlameColor.r, FlameColor.g, FlameColor.b, 0.18f);
				Graphics()->DrawCircle(Center.x + std::cos(Angle) * Radius, Center.y + std::sin(Angle) * Radius * 0.9f, 4.5f, 10);
			}
			for(int Ring = 0; Ring < 3; Ring++)
			{
				Graphics()->SetColor(FlameColor.r, FlameColor.g, FlameColor.b, 0.05f);
				Graphics()->DrawCircle(Center.x, Center.y, 26.0f + Ring * 4.0f + Tier, 28);
			}
			Graphics()->QuadsEnd();
			Graphics()->BlendNormal();
		}
		{
			const float Flicker = Lit ? 1.0f + 0.05f * std::sin((float)Now * 9.0f) + 0.03f * std::sin((float)Now * 23.0f) : 1.0f;
			float Pop = 1.0f;
			if(Now - s_AnimStart < 0.35)
				Pop += 0.35f * std::sin((float)((Now - s_AnimStart) / 0.35) * pi);
			TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
			const ColorRGBA Color = Lit ? FlameColor : ColorRGBA(0.38f, 0.38f, 0.4f, 1.0f);
			TextRender()->TextColor(Color);
			CUIRect IconRect = Square;
			IconRect.y -= 1.0f;
			Ui()->DoLabel(&IconRect, FontIcon::FIRE, 30.0f * Flicker * Pop, TEXTALIGN_MC);
			TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
		}
		if(Count > 0)
		{
			char aCount[16];
			str_format(aCount, sizeof(aCount), "%d", Count);
			TextRender()->TextColor(Lit ? FlameColor : ColorRGBA(0.7f, 0.7f, 0.72f, 1.0f));
			Ui()->DoLabel(&CountRect, aCount, 15.0f, TEXTALIGN_MC);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
		}

		// animations
		if(s_AnimTier > 0)
		{
			const float Duration = s_AnimTier >= 150 ? 3.8f : s_AnimTier >= 100 ? 3.0f : (s_AnimTier >= 75 ? 2.0f : (s_AnimTier >= 50 ? 2.4f : (s_AnimTier >= 25 ? 1.8f : 0.9f)));
			const float T = (float)((Now - s_AnimStart) / Duration);
			if(T >= 1.0f)
				s_AnimTier = 0;
			else
			{
				const auto Hash = [](int I, int Salt) {
					unsigned H = (unsigned)(I * 73856093) ^ (unsigned)(Salt * 19349663);
					H ^= H >> 13;
					H *= 0x5bd1e995u;
					H ^= H >> 15;
					return (H & 0xffffu) / 65535.0f;
				};
				const int Rings = s_AnimTier >= 150 ? 4 + StreakTier(s_AnimTier) / 3 : s_AnimTier >= 100 ? 4 : (s_AnimTier >= 75 ? 3 : (s_AnimTier >= 50 ? 3 : (s_AnimTier >= 25 ? 2 : 1)));
				const int Sparks = s_AnimTier >= 150 ? 80 + 6 * StreakTier(s_AnimTier) : s_AnimTier >= 100 ? 60 : (s_AnimTier >= 75 ? 30 : (s_AnimTier >= 50 ? 36 : (s_AnimTier >= 25 ? 18 : 6)));
				const float MaxR = s_AnimTier >= 150 ? 180.0f + 8.0f * StreakTier(s_AnimTier) : s_AnimTier >= 100 ? 170.0f : (s_AnimTier >= 75 ? 120.0f : (s_AnimTier >= 50 ? 130.0f : (s_AnimTier >= 25 ? 100.0f : 50.0f)));
				// the ring gets more violet with the streak: orange, then a bit more for 25, 50, 75 and fully violet for 100
				const float RingViolet = Violet >= 1.0f ? 1.0f : std::min(1.0f, Violet * 1.15f);
				ColorRGBA Tint(1.0f + (0.62f - 1.0f) * RingViolet, 0.62f + (0.3f - 0.62f) * RingViolet, 0.18f + (1.0f - 0.18f) * RingViolet, 1.0f);
				ColorRGBA Hot(1.0f + (0.8f - 1.0f) * RingViolet, 0.85f + (0.6f - 0.85f) * RingViolet, 0.45f + (1.0f - 0.45f) * RingViolet, 1.0f);
					if(StreakTier(s_AnimTier) > 0)
					{
						Tint = StreakTierColor(StreakTier(s_AnimTier), Now);
						Hot = ColorRGBA(Tint.r * 0.5f + 0.5f, Tint.g * 0.5f + 0.5f, Tint.b * 0.5f + 0.5f, 1.0f);
					}

				Graphics()->TextureClear();
				Graphics()->BlendAdditive();
				Graphics()->QuadsBegin();
				// rings that fly out
				for(int Ring = 0; Ring < Rings; Ring++)
				{
					const float Local = std::clamp(T * 1.4f - Ring * 0.12f, 0.0f, 1.0f);
					if(Local <= 0.0f || Local >= 1.0f)
						continue;
					const float R = MaxR * (1.0f - (1.0f - Local) * (1.0f - Local));
					const float Alpha = (1.0f - Local) * 0.9f;
					for(int Seg = 0; Seg < 48; Seg++)
					{
						const float A0 = 2.0f * pi * Seg / 48.0f, A1 = 2.0f * pi * (Seg + 1) / 48.0f;
						const float Th = 2.0f + 4.0f * (1.0f - Local);
						const IGraphics::CFreeformItem Item(Center.x + std::cos(A0) * (R - Th), Center.y + std::sin(A0) * (R - Th), Center.x + std::cos(A0) * (R + Th), Center.y + std::sin(A0) * (R + Th),
							Center.x + std::cos(A1) * (R - Th), Center.y + std::sin(A1) * (R - Th), Center.x + std::cos(A1) * (R + Th), Center.y + std::sin(A1) * (R + Th));
						Graphics()->SetColor(Tint.r, Tint.g, Tint.b, Alpha);
						Graphics()->QuadsDrawFreeform(&Item, 1);
					}
				}
				// rotating rays (75 and 100)
				if(s_AnimTier >= 100)
				{
					const int Rays = 10;
					const float Fade = std::sin(T * pi);
					for(int Ray = 0; Ray < Rays; Ray++)
					{
						const float Angle = 2.0f * pi * Ray / Rays + T * 1.5f;
						const float Len = MaxR * (0.6f + 0.4f * Hash(Ray, 3)) * Fade;
						const vec2 Dir(std::cos(Angle), std::sin(Angle)), Perp(-Dir.y, Dir.x);
						const float W = 4.0f;
						Graphics()->SetColor4(ColorRGBA(Hot.r, Hot.g, Hot.b, 0.5f * Fade), ColorRGBA(Hot.r, Hot.g, Hot.b, 0.5f * Fade), ColorRGBA(Tint.r, Tint.g, Tint.b, 0.0f), ColorRGBA(Tint.r, Tint.g, Tint.b, 0.0f));
						const IGraphics::CFreeformItem Item(Center.x - Perp.x * W, Center.y - Perp.y * W, Center.x + Perp.x * W, Center.y + Perp.y * W, Center.x + Dir.x * Len - Perp.x * 0.5f, Center.y + Dir.y * Len - Perp.y * 0.5f, Center.x + Dir.x * Len + Perp.x * 0.5f, Center.y + Dir.y * Len + Perp.y * 0.5f);
						Graphics()->QuadsDrawFreeform(&Item, 1);
					}
				}
				// sparks flying out (they swirl a little for 75 and above)
				for(int Spark = 0; Spark < Sparks; Spark++)
				{
					const float Angle = 2.0f * pi * Hash(Spark, 1) + (s_AnimTier >= 100 ? T * 2.0f : 0.0f);
					const float Speed = MaxR * (0.5f + 0.8f * Hash(Spark, 2));
					const float Dist = Speed * (1.0f - (1.0f - T) * (1.0f - T));
					const float Alpha = std::max(0.0f, 1.0f - T * (0.8f + 0.5f * Hash(Spark, 4)));
					const vec2 P = Center + vec2(std::cos(Angle), std::sin(Angle)) * Dist + vec2(0.0f, 30.0f * T * T);
					const ColorRGBA C = Hash(Spark, 5) < 0.5f ? Tint : Hot;
					Graphics()->SetColor(C.r, C.g, C.b, Alpha);
					Graphics()->DrawCircle(P.x, P.y, 1.5f + 2.5f * Hash(Spark, 6) * (1.0f - T), 8);
				}
				Graphics()->QuadsEnd();
				Graphics()->BlendNormal();

			}
		}
	}

	CUIRect Menu;
	MainView.VMargin(VMargin, &Menu);
	Menu.HSplitBottom(25.0f, &Menu, nullptr);

	Menu.HSplitBottom(40.0f, &Menu, &Button);
	static CButtonContainer s_QuitButton;
	GrowStartButton(Button, &s_QuitButton);
	bool UsedEscape = false;
	if(GameClient()->m_Menus.DoButton_Menu(&s_QuitButton, Localize("Quit"), 0, &Button, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, Rounding, 0.5f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)) || (UsedEscape = Ui()->ConsumeHotkey(CUi::HOTKEY_ESCAPE)) || CheckHotKey(KEY_Q))
	{
		if(UsedEscape || GameClient()->Editor()->HasUnsavedData() || (GameClient()->CurrentRaceTime() / 60 >= g_Config.m_ClConfirmQuitTime && g_Config.m_ClConfirmQuitTime >= 0))
		{
			GameClient()->m_Menus.ShowQuitPopup();
		}
		else
		{
			Client()->Quit();
		}
	}

	Menu.HSplitBottom(100.0f, &Menu, nullptr);
	Menu.HSplitBottom(40.0f, &Menu, &Button);
	static CButtonContainer s_SettingsButton;
	GrowStartButton(Button, &s_SettingsButton);
	if(GameClient()->m_Menus.DoButton_Menu(&s_SettingsButton, Localize("Settings"), 0, &Button, BUTTONFLAG_LEFT, g_Config.m_ClShowStartMenuImages ? "settings" : nullptr, IGraphics::CORNER_ALL, Rounding, 0.5f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)) || CheckHotKey(KEY_S))
		NewPage = CMenus::PAGE_SETTINGS;

	Menu.HSplitBottom(5.0f, &Menu, nullptr); // little space
	Menu.HSplitBottom(40.0f, &Menu, &Button);
	static CButtonContainer s_LocalServerButton;

	const char *pLocalServerImage = g_Config.m_ClShowStartMenuImages ? "local_server" : nullptr;
	const bool LocalServerRunning = GameClient()->m_LocalServer.IsServerRunning();
	GrowStartButton(Button, &s_LocalServerButton);
	if(LocalServerRunning)
	{
		// Split in the middle of the label area, the image is rendered on the right
		CUIRect JoinButton, StopButton;
		const float ImageWidth = pLocalServerImage == nullptr ? 0.0f : Button.h * CMenus::BUTTON_IMAGE_WIDTH_FACTOR;
		Button.VSplitLeft((Button.w - ImageWidth) / 2.0f, &JoinButton, &StopButton);
		if(GameClient()->m_Menus.DoButton_Menu(&s_LocalServerButton, Localize("Join"), 0, &JoinButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_L, Rounding, 0.5f, ColorRGBA(0.0f, 1.0f, 0.0f, 0.25f)) || CheckHotKey(KEY_J))
		{
			GameClient()->m_LocalServer.Connect();
		}
		static CButtonContainer s_StopServerButton;
		if(GameClient()->m_Menus.DoButton_Menu(&s_StopServerButton, Localize("Stop server"), 0, &StopButton, BUTTONFLAG_LEFT, pLocalServerImage, IGraphics::CORNER_R, Rounding, 0.5f, ColorRGBA(1.0f, 0.0f, 0.0f, 0.25f), &s_LocalServerButton) || CheckHotKey(KEY_R))
		{
			GameClient()->m_LocalServer.KillServer();
		}
	}
	else if(GameClient()->m_Menus.DoButton_Menu(&s_LocalServerButton, Localize("Run server"), 0, &Button, BUTTONFLAG_LEFT, pLocalServerImage, IGraphics::CORNER_ALL, Rounding, 0.5f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)) || CheckHotKey(KEY_R))
	{
		GameClient()->m_LocalServer.RunServer({});
	}

	Menu.HSplitBottom(5.0f, &Menu, nullptr); // little space
	Menu.HSplitBottom(40.0f, &Menu, &Button);
	static CButtonContainer s_MapEditorButton;
	GrowStartButton(Button, &s_MapEditorButton);
	if(GameClient()->m_Menus.DoButton_Menu(&s_MapEditorButton, Localize("Editor"), 0, &Button, BUTTONFLAG_LEFT, g_Config.m_ClShowStartMenuImages ? "editor" : nullptr, IGraphics::CORNER_ALL, Rounding, 0.5f, GameClient()->Editor()->HasUnsavedData() ? ColorRGBA(0.0f, 1.0f, 0.0f, 0.25f) : ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)) || CheckHotKey(KEY_E))
	{
		g_Config.m_ClEditor = 1;
		Input()->MouseModeRelative();
	}

	Menu.HSplitBottom(5.0f, &Menu, nullptr); // little space
	Menu.HSplitBottom(40.0f, &Menu, &Button);
	static CButtonContainer s_DemoButton;
	GrowStartButton(Button, &s_DemoButton);
	if(GameClient()->m_Menus.DoButton_Menu(&s_DemoButton, Localize("Demos"), 0, &Button, BUTTONFLAG_LEFT, g_Config.m_ClShowStartMenuImages ? "demos" : nullptr, IGraphics::CORNER_ALL, Rounding, 0.5f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)) || CheckHotKey(KEY_D))
	{
		NewPage = CMenus::PAGE_DEMOS;
	}

	Menu.HSplitBottom(5.0f, &Menu, nullptr); // little space
	Menu.HSplitBottom(40.0f, &Menu, &Button);
	static CButtonContainer s_PlayButton;
	GrowStartButton(Button, &s_PlayButton);
	if(GameClient()->m_Menus.DoButton_Menu(&s_PlayButton, Localize("Play", "Start menu"), 0, &Button, BUTTONFLAG_LEFT, g_Config.m_ClShowStartMenuImages ? "play_game" : nullptr, IGraphics::CORNER_ALL, Rounding, 0.5f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)) || (CLineInput::GetActiveInput() == nullptr && Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER)) || CheckHotKey(KEY_P))
	{
		NewPage = g_Config.m_UiPage >= CMenus::PAGE_INTERNET && g_Config.m_UiPage <= CMenus::PAGE_FAVORITE_COMMUNITY_5 ? g_Config.m_UiPage : CMenus::PAGE_INTERNET;
	}

	// скин игрока справа внизу: смотрит за курсором, моргает; при наведении его «гладят» (сердечки), по клику — профиль
	{
		const CSkins::CSkinContainer *pSkinContainer = GameClient()->m_Skins.FindContainerOrNullptr(g_Config.m_ClPlayerSkin[0] == '\0' ? "default" : g_Config.m_ClPlayerSkin);
		if(pSkinContainer != nullptr && pSkinContainer->IsSpecial())
			pSkinContainer = nullptr;
		const CSkin *pSkin = pSkinContainer == nullptr || pSkinContainer->Skin() == nullptr ? GameClient()->m_Skins.Find("default") : pSkinContainer->Skin().get();
		CTeeRenderInfo TeeInfo;
		TeeInfo.Apply(pSkin);
		TeeInfo.ApplyColors(g_Config.m_ClPlayerUseCustomColor, g_Config.m_ClPlayerColorBody, g_Config.m_ClPlayerColorFeet);
		TeeInfo.m_Size = 56.0f;
		vec2 OffsetToMid;
		CRenderTools::GetRenderTeeOffsetToRenderedTee(CAnimState::GetIdle(), &TeeInfo, OffsetToMid);
		const vec2 TeePos = vec2(MainView.x + MainView.w - 40.0f, MainView.y + MainView.h - 38.0f + OffsetToMid.y);
		const double Seconds = (double)time_get() / (double)time_freq();
		static double s_LastSeconds = 0.0;
		const float Dt = (float)std::clamp(Seconds - s_LastSeconds, 0.0, 0.1);
		s_LastSeconds = Seconds;

		// кнопка-область вокруг тишки
		static CButtonContainer s_TeeButton;
		static bool s_ProfileOpen = false;
		CUIRect TeeHit(TeePos.x - 38.0f, TeePos.y - 36.0f, 76.0f, 76.0f);
		if(Ui()->DoButtonLogic(&s_TeeButton, 0, &TeeHit, BUTTONFLAG_LEFT))
			s_ProfileOpen = !s_ProfileOpen;
		const bool Petting = Ui()->HotItem() == &s_TeeButton;

		// сердечки: летят вверх и тают, пока тишку гладят
		struct SHeart
		{
			vec2 m_Pos;
			double m_Birth;
			float m_Seed;
		};
		static std::vector<SHeart> s_vHearts;
		static double s_NextHeart = 0.0;
		static float s_PetAccum = 0.0f;
		if(Petting)
		{
			if(Seconds >= s_NextHeart)
			{
				s_NextHeart = Seconds + 0.2;
				const float R1 = (float)((int)(Seconds * 1000.0) % 997) / 997.0f;
				const float R2 = (float)((int)(Seconds * 7919.0) % 991) / 991.0f;
				s_vHearts.push_back({TeePos + vec2((R1 - 0.5f) * 40.0f, -22.0f - R2 * 8.0f), Seconds, R2});
			}
			s_PetAccum += Dt;
			if(s_PetAccum >= 1.2f)
			{
				s_PetAccum -= 1.2f;
				g_Config.m_ClStatPets = std::min(2000000000, g_Config.m_ClStatPets + 1);
			}
		}
		else
			s_PetAccum = 0.0f;

		const vec2 Delta = Ui()->MousePos() - TeePos;
		const vec2 Dir = length(Delta) < 0.001f ? vec2(0.0f, 1.0f) : normalize(Delta);
		// моргание: раз в ~3,6 секунды на 0,16 с
		const bool Blink = std::fmod(Seconds, 3.6) < 0.16;
		const int Emote = Petting ? EMOTE_HAPPY : (Blink ? EMOTE_BLINK : EMOTE_NORMAL);
		RenderTools()->RenderTee(CAnimState::GetIdle(), &TeeInfo, Emote, Dir, TeePos);

		if(!s_vHearts.empty())
		{
			s_vHearts.erase(std::remove_if(s_vHearts.begin(), s_vHearts.end(), [&](const SHeart &H) { return Seconds - H.m_Birth > 1.7; }), s_vHearts.end());
			TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
			for(const SHeart &Heart : s_vHearts)
			{
				const float T = (float)((Seconds - Heart.m_Birth) / 1.7);
				const vec2 P = Heart.m_Pos + vec2(std::sin(T * 6.0f + Heart.m_Seed * 6.28f) * 9.0f, -55.0f * T);
				const float Size = 12.0f + 8.0f * Heart.m_Seed;
				TextRender()->TextColor(1.0f, 0.35f, 0.5f, 1.0f - T * T);
				CUIRect HeartRect(P.x - 12.0f, P.y - 12.0f, 24.0f, 24.0f);
				Ui()->DoLabel(&HeartRect, FontIcon::HEART, Size, TEXTALIGN_MC);
			}
			TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
		}

		// окно профиля
		if(s_ProfileOpen)
		{
			CUIRect Panel(MainView.x + MainView.w - 14.0f - 250.0f, TeePos.y - 50.0f - 200.0f, 250.0f, 200.0f);
			Panel.y = std::max(Panel.y, MainView.y + 6.0f);
			CUIRect Outer = Panel;
			Outer.Margin(-1.5f, &Outer);
			static CButtonContainer s_PanelBlock, s_ProfileClose;
			Ui()->DoButtonLogic(&s_PanelBlock, 0, &Outer, BUTTONFLAG_LEFT); // клики не проходят сквозь окно
			Outer.Draw(ColorRGBA(0.62f, 0.38f, 1.0f, 0.55f), IGraphics::CORNER_ALL, 12.0f);
			Panel.Draw4(ColorRGBA(0.18f, 0.07f, 0.34f, 0.97f), ColorRGBA(0.06f, 0.02f, 0.12f, 0.97f), ColorRGBA(0.10f, 0.03f, 0.20f, 0.97f), ColorRGBA(0.03f, 0.01f, 0.07f, 0.97f), IGraphics::CORNER_ALL, 10.0f);
			CUIRect Inner;
			Panel.Margin(12.0f, &Inner);

			CUIRect Top, CloseRect;
			Inner.HSplitTop(70.0f, &Top, &Inner);
			Top.VSplitRight(22.0f, &Top, &CloseRect);
			CloseRect.h = 22.0f;
			if(GameClient()->m_Menus.DoButton_Menu(&s_ProfileClose, "x", 0, &CloseRect, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.45f, 0.2f, 0.8f, 0.5f)))
				s_ProfileOpen = false;

			CUIRect TeeArea, Name;
			Top.VSplitLeft(70.0f, &TeeArea, &Name);
			CTeeRenderInfo BigInfo = TeeInfo;
			BigInfo.m_Size = 60.0f;
			vec2 BigOffset;
			CRenderTools::GetRenderTeeOffsetToRenderedTee(CAnimState::GetIdle(), &BigInfo, BigOffset);
			RenderTools()->RenderTee(CAnimState::GetIdle(), &BigInfo, EMOTE_HAPPY, vec2(1.0f, 0.2f), vec2(TeeArea.x + 32.0f, TeeArea.y + 34.0f + BigOffset.y));

			Name.VMargin(4.0f, &Name);
			CUIRect NameLine, ClanLine, SkinLine;
			Name.HSplitTop(22.0f, &NameLine, &Name);
			Name.HSplitTop(16.0f, &ClanLine, &Name);
			Name.HSplitTop(16.0f, &SkinLine, &Name);
			TextRender()->TextColor(0.9f, 0.82f, 1.0f, 1.0f);
			Ui()->DoLabel(&NameLine, g_Config.m_PlayerName, 15.0f, TEXTALIGN_ML);
			TextRender()->TextColor(0.7f, 0.62f, 0.9f, 0.9f);
			Ui()->DoLabel(&ClanLine, g_Config.m_PlayerClan[0] ? g_Config.m_PlayerClan : "без клана", 10.0f, TEXTALIGN_ML);
			Ui()->DoLabel(&SkinLine, g_Config.m_ClPlayerSkin[0] ? g_Config.m_ClPlayerSkin : "default", 10.0f, TEXTALIGN_ML);
			TextRender()->TextColor(TextRender()->DefaultTextColor());

			Inner.HSplitTop(4.0f, nullptr, &Inner);
			const int StreakNow = std::max(g_Config.m_ClStreakCount, 0);
			char aStreak[64], aTier[48];
			str_format(aStreak, sizeof(aStreak), "%d (рекорд %d)", StreakNow, std::max(g_Config.m_ClStreakBest, StreakNow));
			const int StatTier = StreakTier(StreakNow);
			str_format(aTier, sizeof(aTier), "%d из %d", StatTier, 11);
			char aSessions[24], aPets[24];
			str_format(aSessions, sizeof(aSessions), "%d", g_Config.m_ClStatSessions);
			str_format(aPets, sizeof(aPets), "%d раз", g_Config.m_ClStatPets);
			const std::string PlayTime = FormatPlayTime(g_Config.m_ClStatPlaySeconds);
			const char *apLabels[5] = {"Наиграно на серверах", "Заходов на серверы", "Огонь (дни подряд)", "Уровень огня", "Тишку погладили"};
			const char *apValues[5] = {PlayTime.c_str(), aSessions, aStreak, aTier, aPets};
			for(int Line = 0; Line < 5; Line++)
			{
				CUIRect Row, Label, Value;
				Inner.HSplitTop(17.0f, &Row, &Inner);
				Row.VSplitMid(&Label, &Value);
				TextRender()->TextColor(0.72f, 0.66f, 0.9f, 0.9f);
				Ui()->DoLabel(&Label, apLabels[Line], 9.5f, TEXTALIGN_ML);
				TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
				Value.VSplitLeft(10.0f, nullptr, &Value);
				Ui()->DoLabel(&Value, apValues[Line], 10.5f, TEXTALIGN_ML);
				TextRender()->TextColor(TextRender()->DefaultTextColor());
			}
		}
	}

	// чат друзей: кнопка слева сверху и окно под ней
	{
		CFriendChat &Chat = GameClient()->m_FriendChat;
		CUIRect ChatButton(MainView.x + 12.0f, MainView.y + 12.0f, 120.0f, 26.0f);
		static CButtonContainer s_ChatButton;
		const int Badge = Chat.TotalUnread() + Chat.Pending();
		char aLabel[48];
		if(Badge > 0)
			str_format(aLabel, sizeof(aLabel), "Чат друзей (%d)", Badge);
		else
			str_copy(aLabel, "Чат друзей");
		if(GameClient()->m_Menus.DoButton_Menu(&s_ChatButton, aLabel, Chat.IsOpen(), &ChatButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 7.0f, 0.0f, Badge > 0 ? ColorRGBA(0.8f, 0.3f, 0.9f, 0.6f) : ColorRGBA(0.45f, 0.22f, 0.85f, 0.5f)))
			Chat.Toggle();
		if(Chat.IsOpen())
		{
			CUIRect Panel(MainView.x + 12.0f, MainView.y + 44.0f, 330.0f, std::min(430.0f, MainView.h - 60.0f));
			static CButtonContainer s_ChatBlock;
			Ui()->DoButtonLogic(&s_ChatBlock, 0, &Panel, BUTTONFLAG_LEFT); // клики не проходят к кнопкам меню под окном
			Chat.RenderPanel(Panel);
		}
	}

	CUIRect VersionUpdate;
	MainView.HSplitBottom(20.0f, nullptr, &VersionUpdate);
	VersionUpdate.VMargin(VMargin, &VersionUpdate);
#if defined(CONF_AUTOUPDATE)
	CUIRect UpdateButton;
	VersionUpdate.VSplitRight(100.0f, &VersionUpdate, &UpdateButton);
	VersionUpdate.VSplitRight(10.0f, &VersionUpdate, nullptr);

	char aBuf[128];
	const IUpdater::EUpdaterState State = Updater()->GetCurrentState();
	const bool NeedUpdate = str_comp(Client()->LatestVersion(), "0");

	if(State == IUpdater::CLEAN && NeedUpdate)
	{
		static CButtonContainer s_VersionUpdate;
		if(GameClient()->m_Menus.DoButton_Menu(&s_VersionUpdate, Localize("Update now"), 0, &UpdateButton, BUTTONFLAG_LEFT, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
		{
			Updater()->InitiateUpdate();
		}
	}
	else if(State == IUpdater::NEED_RESTART)
	{
		static CButtonContainer s_VersionUpdate;
		if(GameClient()->m_Menus.DoButton_Menu(&s_VersionUpdate, Localize("Restart"), 0, &UpdateButton, BUTTONFLAG_LEFT, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
		{
			Client()->Restart();
		}
	}
	else if(State >= IUpdater::GETTING_MANIFEST && State < IUpdater::NEED_RESTART)
	{
		Ui()->RenderProgressBar(UpdateButton, Updater()->GetCurrentPercent() / 100.0f);
	}

	if(State == IUpdater::CLEAN && NeedUpdate)
	{
		str_format(aBuf, sizeof(aBuf), Localize("DDNet %s is out!"), Client()->LatestVersion());
		TextRender()->TextColor(1.0f, 0.4f, 0.4f, 1.0f);
	}
	else if(State == IUpdater::CLEAN)
	{
		aBuf[0] = '\0';
	}
	else if(State >= IUpdater::GETTING_MANIFEST && State < IUpdater::NEED_RESTART)
	{
		char aCurrentFile[64];
		Updater()->GetCurrentFile(aCurrentFile, sizeof(aCurrentFile));
		str_format(aBuf, sizeof(aBuf), Localize("Downloading %s:"), aCurrentFile);
	}
	else if(State == IUpdater::FAIL)
	{
		str_copy(aBuf, Localize("Update failed! Check log…"));
		TextRender()->TextColor(1.0f, 0.4f, 0.4f, 1.0f);
	}
	else if(State == IUpdater::NEED_RESTART)
	{
		str_copy(aBuf, Localize("DDNet Client updated!"));
		TextRender()->TextColor(1.0f, 0.4f, 0.4f, 1.0f);
	}
	Ui()->DoLabel(&VersionUpdate, aBuf, 14.0f, TEXTALIGN_ML);
	TextRender()->TextColor(TextRender()->DefaultTextColor());
#elif defined(CONF_INFORM_UPDATE)
	if(str_comp(Client()->LatestVersion(), "0") != 0)
	{
		CUIRect DownloadButton;
		VersionUpdate.VSplitRight(100.0f, &VersionUpdate, &DownloadButton);
		VersionUpdate.VSplitRight(10.0f, &VersionUpdate, nullptr);

		static CButtonContainer s_DownloadButton;
		if(GameClient()->m_Menus.DoButton_Menu(&s_DownloadButton, Localize("Download"), 0, &DownloadButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
		{
			Client()->ViewLink("https://ddnet.org/downloads/");
		}

		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), Localize("DDNet %s is out!"), Client()->LatestVersion());
		SLabelProperties UpdateLabelProps;
		UpdateLabelProps.SetColor(ColorRGBA(1.0f, 0.4f, 0.4f, 1.0f));
		Ui()->DoLabel(&VersionUpdate, aBuf, 14.0f, TEXTALIGN_ML, UpdateLabelProps);
	}
#endif

	if(NewPage != -1)
	{
		GameClient()->m_Menus.SetShowStart(false);
		GameClient()->m_Menus.SetMenuPage(NewPage);
	}
}

bool CMenusStart::CheckHotKey(int Key) const
{
	return CLineInput::GetActiveInput() == nullptr && !Input()->ShiftIsPressed() && !Input()->ModifierIsPressed() && !Input()->AltIsPressed() && // no modifier
	       Input()->KeyPress(Key) &&
	       !GameClient()->m_GameConsole.IsActive();
}
