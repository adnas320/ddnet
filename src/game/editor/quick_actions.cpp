#include "editor.h"
#include "editor_actions.h"

#include <engine/keys.h>

#include <game/client/gameclient.h>
#include <game/mapitems.h>

#include <algorithm>
#include <random>

void CEditor::FillGameTiles(EGameTileOp FillTile) const
{
	std::shared_ptr<CLayerTiles> pTileLayer = std::static_pointer_cast<CLayerTiles>(Map()->SelectedLayerType(0, LAYERTYPE_TILES));
	if(pTileLayer)
		pTileLayer->FillGameTiles(FillTile);
}

bool CEditor::CanFillGameTiles() const
{
	std::shared_ptr<CLayerTiles> pTileLayer = std::static_pointer_cast<CLayerTiles>(Map()->SelectedLayerType(0, LAYERTYPE_TILES));
	if(pTileLayer)
		return pTileLayer->CanFillGameTiles();
	return false;
}

// Generates a DDRace course in a zone of the game layer: a winding tunnel from a start room with the
// start line to a finish room with the finish line. Between them the tunnel is made of floor with
// freeze, freeze pits, hanging and standing pillars, squeezes, unhookable ceilings and steps.
void CEditor::GenerateGameplay(int X0, int Y0, int X1, int Y1)
{
	std::shared_ptr<CLayerTiles> pGameLayer = Map()->m_pGameLayer;
	if(!pGameLayer || !Map()->m_pGameGroup)
		return;

	// clip to the layer
	X0 = std::max(X0, 0);
	Y0 = std::max(Y0, 0);
	X1 = std::min(X1, pGameLayer->m_Width - 1);
	Y1 = std::min(Y1, pGameLayer->m_Height - 1);
	const int W = X1 - X0 + 1;
	const int H = Y1 - Y0 + 1;
	if(W < 24 || H < 12)
	{
		ShowFileDialogError("The zone is too small. It has to be at least 24 x 12 tiles.");
		return;
	}

	std::mt19937 Rng(m_GeneratorSeed++ * 2654435761u + (unsigned)time_get());
	const auto Rand = [&](int Min, int Max) { return std::uniform_int_distribution<int>(Min, Max)(Rng); };

	// local buffer, everything is solid at the start
	std::vector<unsigned char> vBuf((size_t)W * H, TILE_SOLID);
	const auto At = [&](int x, int y) -> unsigned char & { return vBuf[(size_t)y * W + x]; };
	const auto Inside = [&](int x, int y) { return x >= 1 && x < W - 1 && y >= 1 && y < H - 1; };
	const auto Carve = [&](int x, int Top, int Bottom) {
		for(int y = Top; y <= Bottom; y++)
			if(Inside(x, y))
				At(x, y) = TILE_AIR;
	};

	// the tunnel is between Top and Bottom (both air rows)
	const int MinTop = 3, MaxBottom = H - 4;
	int Top = std::clamp(H / 2 - 3, MinTop, MaxBottom - 5);
	int Bottom = Top + 5;
	int x = 2;

	// start room
	for(int i = 0; i < 8; i++, x++)
		Carve(x, Top - 1, Bottom);
	At(4, Bottom) = (unsigned char)(ENTITY_OFFSET + ENTITY_SPAWN);
	for(int y = Top - 1; y <= Bottom; y++)
		if(Inside(x - 1, y))
			At(x - 1, y) = TILE_START;

	const int FinishRoomStart = W - 10;
	while(x < FinishRoomStart)
	{
		const int Pattern = Rand(0, 8);
		const int Len = Rand(7, 13);
		const int End = std::min(x + Len, FinishRoomStart);
		const int Height = Bottom - Top + 1;
		switch(Pattern)
		{
		case 0: // plain corridor, the height changes a bit
		{
			const int Change = Rand(-1, 1);
			for(; x < End; x++)
				Carve(x, Top, Bottom);
			if(Change != 0 && Bottom - Top + Change >= 4 && Bottom - Top + Change <= 8)
			{
				Top -= Change;
				Bottom += Change;
				Top = std::max(Top, MinTop);
				Bottom = std::min(Bottom, MaxBottom);
				Carve(x, Top, Bottom);
			}
			break;
		}
		case 1: // step up or down
		case 2:
		{
			const int Delta = (Rand(0, 1) ? 1 : -1) * Rand(2, 4);
			const int NewTop = std::clamp(Top + Delta, MinTop, MaxBottom - (Height - 1));
			const int NewBottom = NewTop + Height - 1;
			for(; x < End - 2; x++)
				Carve(x, Top, Bottom);
			// connection between the two heights
			for(int i = 0; i < 3; i++, x++)
				Carve(x, std::min(Top, NewTop), std::max(Bottom, NewBottom));
			Top = NewTop;
			Bottom = NewBottom;
			break;
		}
		case 3: // freeze floor
			for(; x < End; x++)
			{
				Carve(x, Top, Bottom);
				if(x > End - 2 || x < End - Len + 1)
					continue;
				At(x, Bottom) = TILE_FREEZE;
			}
			break;
		case 4: // freeze pit in the floor
		{
			const int PitWidth = Rand(3, 5);
			const int PitStart = x + Rand(2, 4);
			for(; x < End + PitWidth; x++)
			{
				Carve(x, Top, Bottom);
				if(x >= PitStart && x < PitStart + PitWidth)
				{
					for(int y = Bottom; y <= Bottom + 2; y++)
						if(Inside(x, y))
							At(x, y) = TILE_FREEZE;
				}
			}
			break;
		}
		case 5: // hanging and standing pillars
		{
			bool FromTop = Rand(0, 1) != 0;
			for(; x < End; x++)
			{
				Carve(x, Top, Bottom);
				if((x - End) % 3 == 0)
				{
					const int PillarLen = std::max(1, Height - 4);
					for(int i = 0; i < PillarLen; i++)
					{
						const int y = FromTop ? Top + i : Bottom - i;
						if(Inside(x, y))
							At(x, y) = TILE_SOLID;
					}
					// freeze tip
					const int TipY = FromTop ? Top + PillarLen : Bottom - PillarLen;
					if(Inside(x, TipY))
						At(x, TipY) = TILE_FREEZE;
					FromTop = !FromTop;
				}
			}
			break;
		}
		case 6: // squeeze with freeze on both sides
		{
			const int Squeeze = Rand(3, 4);
			const int Mid = (Top + Bottom) / 2;
			for(; x < End; x++)
			{
				Carve(x, Top, Bottom);
				if(x < End - 1 && x > End - Len + 1)
				{
					for(int y = Top; y <= Bottom; y++)
					{
						if(y < Mid - Squeeze / 2 || y > Mid + Squeeze / 2)
							At(x, y) = (y == Mid - Squeeze / 2 - 1 || y == Mid + Squeeze / 2 + 1) ? TILE_FREEZE : TILE_SOLID;
					}
				}
			}
			break;
		}
		case 7: // unhookable ceiling and floor
			for(; x < End; x++)
			{
				Carve(x, Top, Bottom);
				if(Inside(x, Top - 1))
					At(x, Top - 1) = TILE_NOHOOK;
				if(Inside(x, Bottom + 1))
					At(x, Bottom + 1) = TILE_NOHOOK;
			}
			break;
		default: // freeze ceiling
			for(; x < End; x++)
			{
				Carve(x, Top, Bottom);
				if(x > End - Len + 1 && x < End - 1)
					At(x, Top) = TILE_FREEZE;
			}
			break;
		}
	}

	// finish room
	for(; x < W - 2; x++)
		Carve(x, Top - 1, Bottom);
	for(int y = Top - 1; y <= Bottom; y++)
		if(Inside(W - 6, y))
			At(W - 6, y) = TILE_FINISH;

	// remember the border: the outermost ring stays solid
	// apply to the game layer and record the changes for undo
	EditorTileStateChangeHistory<STileStateChange> Changes;
	for(int y = 0; y < H; y++)
	{
		for(int xx = 0; xx < W; xx++)
		{
			CTile &Tile = pGameLayer->m_pTiles[(Y0 + y) * pGameLayer->m_Width + X0 + xx];
			const CTile Previous = Tile;
			CTile New{};
			New.m_Index = At(xx, y);
			if(Previous.m_Index == New.m_Index && Previous.m_Flags == 0)
				continue;
			Tile = New;
			Changes[Y0 + y][X0 + xx] = STileStateChange{true, Previous, New};
		}
	}

	const int GameGroupIndex = std::find(Map()->m_vpGroups.begin(), Map()->m_vpGroups.end(), Map()->m_pGameGroup) - Map()->m_vpGroups.begin();
	const int GameLayerIndex = std::find(Map()->m_pGameGroup->m_vpLayers.begin(), Map()->m_pGameGroup->m_vpLayers.end(), std::static_pointer_cast<CLayer>(pGameLayer)) - Map()->m_pGameGroup->m_vpLayers.begin();
	Map()->m_EditorHistory.RecordAction(std::make_shared<CEditorActionTileChanges>(Map(), GameGroupIndex, GameLayerIndex, "Generate DDRace gameplay", Changes));
}

void CEditor::AddQuadOrSound()
{
	std::shared_ptr<CLayer> pLayer = Map()->SelectedLayer(0);
	if(!pLayer)
		return;
	if(pLayer->m_Type != LAYERTYPE_QUADS && pLayer->m_Type != LAYERTYPE_SOUNDS)
		return;

	std::shared_ptr<CLayerGroup> pGroup = Map()->SelectedGroup();

	CScreenRect GroupRect = pGroup->Mapping();
	int x = GroupRect.m_TopLeft.x + GroupRect.Width() / 2;
	int y = GroupRect.m_TopLeft.y + GroupRect.Height() / 2;
	if(m_Dialog == DIALOG_NONE && CLineInput::GetActiveInput() == nullptr && Input()->KeyPress(KEY_Q) && Input()->ModifierIsPressed())
	{
		x += MapView()->MouseWorldPos().x - (MapView()->GetWorldOffset().x * pGroup->m_ParallaxX / 100) - pGroup->m_OffsetX;
		y += MapView()->MouseWorldPos().y - (MapView()->GetWorldOffset().y * pGroup->m_ParallaxY / 100) - pGroup->m_OffsetY;
	}

	if(pLayer->m_Type == LAYERTYPE_QUADS)
		Map()->m_EditorHistory.Execute(std::make_shared<CEditorActionNewEmptyQuad>(Map(), Map()->m_SelectedGroup, Map()->m_vSelectedLayers[0], x, y));
	else if(pLayer->m_Type == LAYERTYPE_SOUNDS)
		Map()->m_EditorHistory.Execute(std::make_shared<CEditorActionNewEmptySound>(Map(), Map()->m_SelectedGroup, Map()->m_vSelectedLayers[0], x, y));
}

void CEditor::AddGroup()
{
	Map()->NewGroup();
	Map()->m_SelectedGroup = Map()->m_vpGroups.size() - 1;
	Map()->m_EditorHistory.RecordAction(std::make_shared<CEditorActionGroup>(Map(), Map()->m_SelectedGroup, false));
}

void CEditor::AddSoundLayer()
{
	std::shared_ptr<CLayer> pSoundLayer = std::make_shared<CLayerSounds>(Map());
	Map()->m_vpGroups[Map()->m_SelectedGroup]->AddLayer(pSoundLayer);
	int LayerIndex = Map()->m_vpGroups[Map()->m_SelectedGroup]->m_vpLayers.size() - 1;
	Map()->SelectLayer(LayerIndex);
	Map()->m_vpGroups[Map()->m_SelectedGroup]->m_Collapse = false;
	Map()->m_EditorHistory.RecordAction(std::make_shared<CEditorActionAddLayer>(Map(), Map()->m_SelectedGroup, LayerIndex));
}

void CEditor::AddTileLayer()
{
	std::shared_ptr<CLayer> pTileLayer = std::make_shared<CLayerTiles>(Map(), Map()->m_pGameLayer->m_Width, Map()->m_pGameLayer->m_Height);
	Map()->m_vpGroups[Map()->m_SelectedGroup]->AddLayer(pTileLayer);
	int LayerIndex = Map()->m_vpGroups[Map()->m_SelectedGroup]->m_vpLayers.size() - 1;
	Map()->SelectLayer(LayerIndex);
	Map()->m_vpGroups[Map()->m_SelectedGroup]->m_Collapse = false;
	Map()->m_EditorHistory.RecordAction(std::make_shared<CEditorActionAddLayer>(Map(), Map()->m_SelectedGroup, LayerIndex));
}

void CEditor::AddQuadsLayer()
{
	std::shared_ptr<CLayer> pQuadLayer = std::make_shared<CLayerQuads>(Map());
	Map()->m_vpGroups[Map()->m_SelectedGroup]->AddLayer(pQuadLayer);
	int LayerIndex = Map()->m_vpGroups[Map()->m_SelectedGroup]->m_vpLayers.size() - 1;
	Map()->SelectLayer(LayerIndex);
	Map()->m_vpGroups[Map()->m_SelectedGroup]->m_Collapse = false;
	Map()->m_EditorHistory.RecordAction(std::make_shared<CEditorActionAddLayer>(Map(), Map()->m_SelectedGroup, LayerIndex));
}

void CEditor::AddSwitchLayer()
{
	std::shared_ptr<CLayer> pSwitchLayer = std::make_shared<CLayerSwitch>(Map(), Map()->m_pGameLayer->m_Width, Map()->m_pGameLayer->m_Height);
	Map()->MakeSwitchLayer(pSwitchLayer);
	Map()->m_vpGroups[Map()->m_SelectedGroup]->AddLayer(pSwitchLayer);
	int LayerIndex = Map()->m_vpGroups[Map()->m_SelectedGroup]->m_vpLayers.size() - 1;
	Map()->SelectLayer(LayerIndex);
	m_pBrush->Clear();
	Map()->m_EditorHistory.RecordAction(std::make_shared<CEditorActionAddLayer>(Map(), Map()->m_SelectedGroup, LayerIndex));
}

void CEditor::AddFrontLayer()
{
	std::shared_ptr<CLayer> pFrontLayer = std::make_shared<CLayerFront>(Map(), Map()->m_pGameLayer->m_Width, Map()->m_pGameLayer->m_Height);
	Map()->MakeFrontLayer(pFrontLayer);
	Map()->m_vpGroups[Map()->m_SelectedGroup]->AddLayer(pFrontLayer);
	int LayerIndex = Map()->m_vpGroups[Map()->m_SelectedGroup]->m_vpLayers.size() - 1;
	Map()->SelectLayer(LayerIndex);
	m_pBrush->Clear();
	Map()->m_EditorHistory.RecordAction(std::make_shared<CEditorActionAddLayer>(Map(), Map()->m_SelectedGroup, LayerIndex));
}

void CEditor::AddTuneLayer()
{
	std::shared_ptr<CLayer> pTuneLayer = std::make_shared<CLayerTune>(Map(), Map()->m_pGameLayer->m_Width, Map()->m_pGameLayer->m_Height);
	Map()->MakeTuneLayer(pTuneLayer);
	Map()->m_vpGroups[Map()->m_SelectedGroup]->AddLayer(pTuneLayer);
	int LayerIndex = Map()->m_vpGroups[Map()->m_SelectedGroup]->m_vpLayers.size() - 1;
	Map()->SelectLayer(LayerIndex);
	m_pBrush->Clear();
	Map()->m_EditorHistory.RecordAction(std::make_shared<CEditorActionAddLayer>(Map(), Map()->m_SelectedGroup, LayerIndex));
}

void CEditor::AddSpeedupLayer()
{
	std::shared_ptr<CLayer> pSpeedupLayer = std::make_shared<CLayerSpeedup>(Map(), Map()->m_pGameLayer->m_Width, Map()->m_pGameLayer->m_Height);
	Map()->MakeSpeedupLayer(pSpeedupLayer);
	Map()->m_vpGroups[Map()->m_SelectedGroup]->AddLayer(pSpeedupLayer);
	int LayerIndex = Map()->m_vpGroups[Map()->m_SelectedGroup]->m_vpLayers.size() - 1;
	Map()->SelectLayer(LayerIndex);
	m_pBrush->Clear();
	Map()->m_EditorHistory.RecordAction(std::make_shared<CEditorActionAddLayer>(Map(), Map()->m_SelectedGroup, LayerIndex));
}

void CEditor::AddTeleLayer()
{
	std::shared_ptr<CLayer> pTeleLayer = std::make_shared<CLayerTele>(Map(), Map()->m_pGameLayer->m_Width, Map()->m_pGameLayer->m_Height);
	Map()->MakeTeleLayer(pTeleLayer);
	Map()->m_vpGroups[Map()->m_SelectedGroup]->AddLayer(pTeleLayer);
	int LayerIndex = Map()->m_vpGroups[Map()->m_SelectedGroup]->m_vpLayers.size() - 1;
	Map()->SelectLayer(LayerIndex);
	m_pBrush->Clear();
	Map()->m_EditorHistory.RecordAction(std::make_shared<CEditorActionAddLayer>(Map(), Map()->m_SelectedGroup, LayerIndex));
}

bool CEditor::IsNonGameTileLayerSelected() const
{
	std::shared_ptr<CLayer> pLayer = Map()->SelectedLayer(0);
	if(!pLayer)
		return false;
	if(pLayer->m_Type != LAYERTYPE_TILES)
		return false;
	if(
		pLayer == Map()->m_pGameLayer ||
		pLayer == Map()->m_pFrontLayer ||
		pLayer == Map()->m_pSwitchLayer ||
		pLayer == Map()->m_pTeleLayer ||
		pLayer == Map()->m_pSpeedupLayer ||
		pLayer == Map()->m_pTuneLayer)
		return false;

	return true;
}

void CEditor::LayerSelectImage()
{
	if(!IsNonGameTileLayerSelected())
		return;

	std::shared_ptr<CLayer> pLayer = Map()->SelectedLayer(0);
	std::shared_ptr<CLayerTiles> pTiles = std::static_pointer_cast<CLayerTiles>(pLayer);

	static SLayerPopupContext s_LayerPopupContext = {};
	s_LayerPopupContext.m_pEditor = this;
	Ui()->DoPopupMenu(&s_LayerPopupContext, Ui()->MouseX(), Ui()->MouseY(), 150, 300, &s_LayerPopupContext, PopupLayer);
	PopupSelectImageInvoke(pTiles->m_Image, Ui()->MouseX(), Ui()->MouseY());
}

void CEditor::MapDetails()
{
	const CUIRect *pScreen = Ui()->Screen();
	Map()->m_MapInfoTmp.Copy(Map()->m_MapInfo);
	static SPopupMenuId s_PopupMapInfoId;
	constexpr float PopupWidth = 400.0f;
	constexpr float PopupHeight = 170.0f;
	Ui()->DoPopupMenu(
		&s_PopupMapInfoId,
		pScreen->w / 2.0f - PopupWidth / 2.0f,
		pScreen->h / 2.0f - PopupHeight / 2.0f,
		PopupWidth,
		PopupHeight,
		this,
		PopupMapInfo);
	Ui()->SetActiveItem(nullptr);
}

void CEditor::GotoPosition()
{
	static SPopupMenuId s_PopupGotoId;
	Ui()->DoPopupMenu(&s_PopupGotoId, Ui()->MouseX(), Ui()->MouseY(), 120.0f, 52.0f, this, PopupGoto);
	Ui()->SetActiveItem(nullptr);
}

void CEditor::DeleteSelectedLayer()
{
	std::shared_ptr<CLayer> pCurrentLayer = Map()->SelectedLayer(0);
	if(!pCurrentLayer)
		return;
	if(Map()->m_pGameLayer == pCurrentLayer)
		return;

	Map()->m_EditorHistory.RecordAction(std::make_shared<CEditorActionDeleteLayer>(Map(), Map()->m_SelectedGroup, Map()->m_vSelectedLayers[0]));

	if(pCurrentLayer == Map()->m_pFrontLayer)
		Map()->m_pFrontLayer = nullptr;
	if(pCurrentLayer == Map()->m_pTeleLayer)
		Map()->m_pTeleLayer = nullptr;
	if(pCurrentLayer == Map()->m_pSpeedupLayer)
		Map()->m_pSpeedupLayer = nullptr;
	if(pCurrentLayer == Map()->m_pSwitchLayer)
		Map()->m_pSwitchLayer = nullptr;
	if(pCurrentLayer == Map()->m_pTuneLayer)
		Map()->m_pTuneLayer = nullptr;
	Map()->m_vpGroups[Map()->m_SelectedGroup]->DeleteLayer(Map()->m_vSelectedLayers[0]);

	Map()->SelectPreviousLayer();
}

void CEditor::TestMapLocally()
{
	const char *pFilenameNoMaps = str_startswith(Map()->m_aFilename, "maps/");
	if(!pFilenameNoMaps)
	{
		ShowFileDialogError("The map isn't saved in the maps/ folder. It must be saved there to load on the server.");
		return;
	}

	char aFilenameNoExt[IO_MAX_PATH_LENGTH];
	fs_split_file_extension(pFilenameNoMaps, aFilenameNoExt, sizeof(aFilenameNoExt));

	if(Client()->RconAuthed())
	{
		if(net_addr_is_local(&Client()->ServerAddress()))
		{
			OnClose();
			g_Config.m_ClEditor = 0;
			char aMapChange[IO_MAX_PATH_LENGTH + 64];
			str_format(aMapChange, sizeof(aMapChange), "change_map %s", aFilenameNoExt);
			Client()->Rcon(aMapChange);
			return;
		}
	}

	CGameClient *pGameClient = (CGameClient *)Kernel()->RequestInterface<IGameClient>();
	if(pGameClient->m_LocalServer.IsServerRunning())
	{
		m_PopupEventType = CEditor::POPEVENT_RESTART_SERVER;
		m_PopupEventActivated = true;
	}
	else
	{
		char aMapChange[IO_MAX_PATH_LENGTH + 64];
		str_format(aMapChange, sizeof(aMapChange), "change_map %s", aFilenameNoExt);
		if(pGameClient->m_LocalServer.RunServer({"sv_register 0", aMapChange}))
		{
			OnClose();
			g_Config.m_ClEditor = 0;
			pGameClient->m_LocalServer.Connect();
		}
		else
		{
			ShowFileDialogError("Local server could not be started.");
		}
	}
}
