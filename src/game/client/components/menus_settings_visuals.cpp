#include "menus.h"

#include <engine/textrender.h>

#include <game/client/ui.h>
#include <game/localization.h>

void CMenus::RenderSettingsVisuals(CUIRect MainView)
{
	// Placeholder page: visual options will be added here.
	CUIRect Label;
	MainView.HSplitTop(30.0f, &Label, &MainView);
	Ui()->DoLabel(&Label, Localize("Visuals"), 20.0f, TEXTALIGN_ML);
}
