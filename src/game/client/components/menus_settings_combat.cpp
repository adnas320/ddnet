#include "menus.h"

#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/ui.h>

// All texts on this page are intentionally English only (no Localize).

void CMenus::RenderSettingsCombat(CUIRect MainView)
{
	CUIRect Left, Right;
	MainView.VSplitMid(&Left, &Right, 10.0f);

	const float Padding = 10.0f;
	const float TitleHeight = 30.0f;
	const float RowHeight = 20.0f;
	const float CardHeight = 2.0f * Padding + TitleHeight + RowHeight;

	CUIRect Card, Label, Button;
	Left.HSplitTop(CardHeight, &Card, nullptr);
	Card.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), IGraphics::CORNER_ALL, 10.0f);
	Card.Margin(Padding, &Card);
	Card.HSplitTop(TitleHeight, &Label, &Card);
	Ui()->DoLabel(&Label, "Weapon prediction", 20.0f, TEXTALIGN_ML);
	Card.HSplitTop(RowHeight, &Button, &Card);
	if(DoButton_CheckBox(&g_Config.m_ClCombatPredict, "Show where your shot will fly (bounces included)", g_Config.m_ClCombatPredict, &Button))
		g_Config.m_ClCombatPredict ^= 1;
}
