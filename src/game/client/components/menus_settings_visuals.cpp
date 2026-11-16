#include "menus.h"

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/ui.h>
#include <game/localization.h>

void CMenus::RenderSettingsVisuals(CUIRect MainView)
{
	CUIRect TabBar, Button, Label;

	// sub tabs of the Visuals page, currently there is only one
	MainView.HSplitTop(26.0f, &TabBar, &MainView);
	MainView.HSplitTop(15.0f, nullptr, &MainView);
	TabBar.VSplitLeft(150.0f, &Button, nullptr);
	static CButtonContainer s_NewYearTab;
	DoButton_MenuTab(&s_NewYearTab, Localize("New Year"), true, &Button, IGraphics::CORNER_T);

	// New Year tab
	MainView.HSplitTop(30.0f, &Label, &MainView);
	Ui()->DoLabel(&Label, Localize("New Year"), 20.0f, TEXTALIGN_ML);

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_ClVisualsWinterHat, Localize("Winter hat (only on your own tee)"), g_Config.m_ClVisualsWinterHat, &Button))
		g_Config.m_ClVisualsWinterHat ^= 1;

	MainView.HSplitTop(5.0f, nullptr, &MainView);
	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_ClVisualsSnow, Localize("Falling snow"), g_Config.m_ClVisualsSnow, &Button))
		g_Config.m_ClVisualsSnow ^= 1;
}
