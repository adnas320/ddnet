#include "editor.h"
#include "editor_actions.h"

#include <engine/keys.h>

#include <game/client/gameclient.h>
#include <game/mapitems.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iterator>
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

// Generates a DDRace course in a zone of the game layer. The course runs from a start room (spawn and
// start line) to a finish room (finish line) and is built from sections: open chambers with platforms,
// fields of blocks, zigzag walls, freeze floors, freeze pits, vertical shafts, hook sections with
// stalactites, staircases, narrow freeze tunnels and (optionally) teleporter parts. Difficulty and
// block style (1x1 or 3x3) change sizes, freeze density and which sections are used.
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
	if(W < 32 || H < 16)
	{
		ShowFileDialogError("The zone is too small. It has to be at least 32 x 16 tiles.");
		return;
	}

	std::mt19937 Rng(m_GeneratorSeed++ * 2654435761u + (unsigned)std::chrono::steady_clock::now().time_since_epoch().count());
	const auto Rand = [&](int Min, int Max) {
		if(Max < Min)
			Max = Min;
		return std::uniform_int_distribution<int>(Min, Max)(Rng);
	};

	// ---- settings ----
	const int Diff = std::clamp(m_GeneratorDifficulty, 0, 2); // 0 novice, 1 moderate, 2 brutal
	const int B = m_GeneratorStyle == 1 ? 3 : 1; // thickness of blocks and walls
	const int aHMin[3] = {12, 8, 6};
	const int aHMax[3] = {20, 14, 11};

	// teleporters are only possible when the tele layer (if there is one) covers the zone
	std::shared_ptr<CLayerTele> pTeleLayer = Map()->m_pTeleLayer;
	const bool Fly = m_GeneratorFly && m_GeneratorStyle == 1;
	bool Teleports = m_GeneratorTeleports && W >= 64;
	if(pTeleLayer && (pTeleLayer->m_Width <= X1 || pTeleLayer->m_Height <= Y1))
		Teleports = false;
	int TeleBase = 1;
	if(pTeleLayer)
		TeleBase = std::clamp(pTeleLayer->FindNextFreeNumber(false), 1, 240);

	// With more thinking time several complete candidates are built and the best one is kept: it has to be
	// connected from the spawn to the finish and is rated by variety.
	std::vector<unsigned char> vOutBuf, vOutTeleType, vOutTeleNum;
	float BestScore = -1.0f;
	const int aAttempts[3] = {1, 12, 120};
	const int Attempts = aAttempts[std::clamp(m_GeneratorThink, 0, 2)];
	for(int Attempt = 0; Attempt < Attempts; Attempt++)
	{
	// ---- local buffers, everything starts as solid ----
	enum
	{
		T_AIR = TILE_AIR,
		T_SOLID = TILE_SOLID,
		T_DEATH = TILE_DEATH,
		T_NOHOOK = TILE_NOHOOK,
		T_HOOKTHROUGH = TILE_THROUGH_CUT,
		T_FREEZE = TILE_FREEZE,
		T_UNFREEZE = TILE_UNFREEZE,
		T_START = TILE_START,
		T_FINISH = TILE_FINISH,
		T_SPAWN = ENTITY_OFFSET + ENTITY_SPAWN,
		T_JETON = TILE_JETPACK_ENABLE,
		T_JETOFF = TILE_JETPACK_DISABLE,
	};
	std::vector<unsigned char> vBuf((size_t)W * H, (unsigned char)T_SOLID);
	std::vector<unsigned char> vTeleType((size_t)W * H, 0);
	std::vector<unsigned char> vTeleNum((size_t)W * H, 0);
	const auto At = [&](int x, int y) -> unsigned char & { return vBuf[(size_t)y * W + x]; };
	// the outermost ring of the zone always stays solid
	const auto Inside = [&](int x, int y) { return x >= 1 && x < W - 1 && y >= 1 && y < H - 1; };
	const auto Put = [&](int x, int y, int Tile) {
		if(Inside(x, y))
			At(x, y) = (unsigned char)Tile;
	};
	const auto PutIfAir = [&](int x, int y, int Tile) {
		if(Inside(x, y) && At(x, y) == T_AIR)
			At(x, y) = (unsigned char)Tile;
	};
	const auto Fill = [&](int x, int y, int w, int h, int Tile) {
		for(int yy = y; yy < y + h; yy++)
			for(int xx = x; xx < x + w; xx++)
				Put(xx, yy, Tile);
	};

	// vertical limits of the air of the course
	const int MinC = 2, MaxF = H - 3;

	// chooses the rows (c to f) of the next chamber, it wanders up and down and overlaps the previous one
	const auto ChooseRange = [&](int PrevC, int PrevF, int HMin, int HMax, bool NeedOverlap, int &c, int &f) {
		const int Avail = MaxF - MinC + 1;
		HMax = std::min(HMax, Avail);
		HMin = std::min(HMin, HMax);
		for(int Try = 0; Try < 60; Try++)
		{
			const int h = Rand(HMin, HMax);
			const int Center = (PrevC + PrevF) / 2 + Rand(-7, 7);
			c = std::clamp(Center - h / 2, MinC, MaxF - h + 1);
			f = c + h - 1;
			if(!NeedOverlap || std::min(f, PrevF) - std::max(c, PrevC) + 1 >= 3)
				return;
		}
		c = PrevC;
		f = PrevF;
	};

	// ---- sections ----
	enum
	{
		S_OPEN,
		S_PLATFORMS,
		S_BLOCKS,
		S_ZIGZAG,
		S_FREEZEFLOOR,
		S_PIT,
		S_SHAFT,
		S_HOOK,
		S_STAIRS,
		S_SQUEEZE,
		S_NOHOOK,
		S_FREEZELINED,
		S_CAVE,
		S_FREEZEROOM,
		S_LADDER,
		S_FRLINES,
		S_TELELINE,
		S_HALL,
		S_FLY,
		S_PSEUDO,
		S_DUMMYGAP,
		S_PSEUDOV,
		NUM_SECTIONS,
	};
	// minimum height of the chamber a section needs
	const int aNeedHeight[NUM_SECTIONS] = {5, 9, 7, 8, 6, 6, 5, 9, 9, 5, 7, 6, 10, 10, 12, 13, 10, 10, 14, 10, 12, 17};
	static const int s_aNovice[] = {S_HALL, S_HALL, S_CAVE, S_CAVE, S_CAVE, S_FREEZEROOM, S_PLATFORMS, S_STAIRS, S_NOHOOK, S_ZIGZAG, S_SHAFT, S_BLOCKS};
	static const int s_aModerate[] = {S_TELELINE, S_HALL, S_TELELINE, S_FRLINES, S_FREEZEROOM, S_FRLINES, S_CAVE, S_PLATFORMS, S_BLOCKS, S_ZIGZAG, S_FREEZEFLOOR, S_PIT, S_SHAFT, S_HOOK, S_STAIRS, S_NOHOOK, S_FREEZELINED, S_SQUEEZE, S_BLOCKS};
	static const int s_aBrutal[] = {S_TELELINE, S_LADDER, S_LADDER, S_LADDER, S_FRLINES, S_BLOCKS, S_ZIGZAG, S_PIT, S_SHAFT, S_HOOK, S_SQUEEZE, S_FREEZELINED, S_SQUEEZE, S_PIT, S_PLATFORMS, S_BLOCKS};
	const int *pPool = Diff == 0 ? s_aNovice : (Diff == 1 ? s_aModerate : s_aBrutal);
	const int PoolSize = Diff == 0 ? (int)std::size(s_aNovice) : (Diff == 1 ? (int)std::size(s_aModerate) : (int)std::size(s_aBrutal));

	int TeleUsed = 0;
	int DummyParts = 0;
	unsigned UsedMask = 0;
	bool UsedFreeze = false;
	const auto FreezeAir = [&](int x, int y) {
		if(Inside(x, y) && At(x, y) == T_AIR)
		{
			At(x, y) = T_FREEZE;
			UsedFreeze = true;
		}
	};
	// rough patches of unhookable tiles in the ceiling
	const auto NoHookCeiling = [&](int x0, int x1, int c) {
		for(int x = x0 + 1; x < x1 - 1;)
		{
			const int Run = Rand(4, 8);
			if(Rand(0, 1))
				for(int i = 0; i < Run && x + i < x1 - 1; i++)
					Put(x + i, c - 1, T_NOHOOK);
			x += Run + Rand(2, 5);
		}
	};

	// Builds the content of one chamber. The air (rows c to f, columns x0 to x1 - 1) is already carved.
	const auto Section = [&](int Id, int x0, int x1, int c, int f) {
		const int Height = f - c + 1;
		switch(Id)
		{
		case S_NOHOOK:
			NoHookCeiling(x0, x1, c);
			[[fallthrough]];
		case S_OPEN:
			if(Diff == 0 && Height >= 8)
			{
				// low mounds on the floor
				for(int mx = x0 + 4; mx < x1 - 12; mx += Rand(10, 14))
				{
					const int w = 4 * B + Rand(2, 4);
					Fill(mx, f - B + 1, w, B, T_SOLID);
					Fill(mx + B, f - 2 * B + 1, w - 2 * B, B, T_SOLID);
				}
			}
			break;
		case S_PLATFORMS:
		{
			int Index = 0;
			for(int px = x0 + 3; px + 7 < x1; px += Rand(8, 11), Index++)
			{
				const int w = Rand(4, 7) + (B == 3 ? 2 : 0);
				const int py = Rand(c + 2, std::max(c + 2, f - B - 2));
				const int Tile = (Diff == 2 && Index % 3 == 2) ? T_HOOKTHROUGH : T_SOLID;
				Fill(px, py, w, B, Tile);
				if(Diff >= 1)
				{
					for(int xx = px; xx < px + w; xx++)
					{
						FreezeAir(xx, py + B);
						if(Diff == 2)
							FreezeAir(xx, py - 1);
					}
				}
			}
			break;
		}
		case S_BLOCKS:
		{
			// staggered field of blocks, with freeze above and below them on harder difficulties
			const int Gap = Diff == 0 ? 4 : (Diff == 1 ? 3 : 2);
			const int P = B + Gap;
			for(int i = 0;; i++)
			{
				const int bx = x0 + 3 + i * P;
				if(bx + B > x1 - 3)
					break;
				const int Offset = (i % 2) * (P / 2);
				for(int by = c + 1 + Offset; by + B <= f; by += P)
				{
					if(Rand(0, 99) >= (Diff == 0 ? 55 : 80))
						continue;
					Fill(bx, by, B, B, T_SOLID);
					if(Diff >= 1 && (Diff == 2 || Rand(0, 1)))
					{
						for(int xx = bx; xx < bx + B; xx++)
						{
							FreezeAir(xx, by - 1);
							FreezeAir(xx, by + B);
						}
					}
				}
			}
			break;
		}
		case S_ZIGZAG:
		{
			int Passage = Diff == 0 ? 6 : (Diff == 1 ? 4 : 3);
			Passage = std::min(Passage, std::max(3, Height - 3));
			const int Len = Height - Passage;
			const int Pitch = B + Rand(5, 7);
			for(int i = 0;; i++)
			{
				const int bx = x0 + 3 + i * Pitch;
				if(bx + B > x1 - 3)
					break;
				const bool FromTop = i % 2 == 0;
				if(FromTop)
					Fill(bx, c, B, Len, T_SOLID);
				else
					Fill(bx, f - Len + 1, B, Len, T_SOLID);
				if(Diff >= 1)
					for(int xx = bx; xx < bx + B; xx++)
						FreezeAir(xx, FromTop ? c + Len : f - Len);
			}
			break;
		}
		case S_FREEZEFLOOR:
		case S_FREEZELINED:
		case S_HOOK:
		{
			// freeze on the floor with safe landings, in the lined variant also on the ceiling
			for(int x = x0 + 1; x < x1 - 1; x++)
			{
				const int Rel = (x - x0) % 9;
				if(Id != S_HOOK && Rel >= 7)
					continue;
				FreezeAir(x, f);
				if(Id == S_FREEZELINED || (Id == S_HOOK && Diff == 2))
					FreezeAir(x, c);
			}
			// stalactites to hook on
			for(int px = x0 + 4; px + B < x1 - 3; px += Rand(6, 9))
				Fill(px, c, B, Rand(2, std::max(2, Height / 2 - 1)), T_SOLID);
			if(Id == S_HOOK && Diff >= 1)
				NoHookCeiling(x0, x1, c);
			if(Id == S_FREEZELINED)
			{
				// a few platforms in the middle give something to land on
				for(int px = x0 + 5; px + 6 < x1; px += Rand(9, 13))
					Fill(px, (c + f) / 2, Rand(3, 5), B, T_SOLID);
			}
			break;
		}
		case S_PIT:
		{
			const int Depth = std::min(3, H - 3 - f);
			if(Depth < 2)
				break;
			const int w = Rand(4, 6) + (B == 3 ? 2 : 0);
			for(int px = x0 + 4; px + w + 3 < x1; px += w + Rand(6, 9))
			{
				for(int dx = 0; dx < w; dx++)
					for(int y = f; y <= f + Depth; y++)
						Put(px + dx, y, (Diff == 2 && y == f + Depth) ? T_DEATH : T_FREEZE);
				UsedFreeze = true;
			}
			break;
		}
		case S_STAIRS:
		{
			const int StepH = B == 3 ? 3 : 2;
			const int StepW = B + 2;
			const int N = std::min((Height - 5) / StepH, (x1 - x0 - 6) / (2 * StepW));
			if(N < 2)
				break;
			for(int k = 0; k < N; k++)
			{
				const int Up = x0 + 2 + k * StepW;
				const int Down = x0 + 2 + (N + k) * StepW;
				const int HUp = (k + 1) * StepH;
				const int HDown = (N - k) * StepH;
				Fill(Up, f - HUp + 1, StepW, HUp, T_SOLID);
				Fill(Down, f - HDown + 1, StepW, HDown, T_SOLID);
				if(Diff >= 1 && k % 2 == 0)
				{
					for(int xx = 0; xx < StepW; xx++)
					{
						FreezeAir(Up + xx, f - HUp);
						FreezeAir(Down + xx, f - HDown);
					}
				}
			}
			break;
		}
		case S_PSEUDO:
		{
			// pseudofly corridor: 3 tiles high with freeze on the floor and the ceiling, nothing to hook.
			// It cannot be passed alone, the tee has to be flown by hammering a partner (dummy).
			DummyParts++;
			const int Top = f - 4;
			Fill(x0 + 2, c, x1 - x0 - 4, std::max(0, Top - c), T_SOLID);
			for(int xx = x0 + 2; xx < x1 - 2; xx++)
			{
				Put(xx, Top, T_FREEZE);
				Put(xx, f, T_FREEZE);
			}
			// the tee waits in a small pocket before and after the corridor
			Put(x0 + 1, f, T_UNFREEZE);
			Put(x1 - 2, f, T_UNFREEZE);
			UsedFreeze = true;
			break;
		}
		case S_DUMMYGAP:
		{
			// a wide gap of freeze under an unhookable ceiling, it is too wide to jump and there is nothing to hook.
			// The partner hooks the tee over it.
			DummyParts++;
			const int Plat = 6;
			Fill(x0 + 1, c, x1 - x0 - 2, std::max(2, Height / 3), T_NOHOOK);
			for(int xx = x0 + 1 + Plat; xx < x1 - 1 - Plat; xx++)
			{
				Fill(xx, f - 1, 1, 2, T_FREEZE);
				Put(xx, f + 1, T_NOHOOK);
			}
			// a nohook bar and a freeze block floating above the gap make it look like a real part
			const int Mid = (x0 + x1) / 2;
			Fill(Mid - 3, (c + f) / 2, 6, 1, T_NOHOOK);
			Fill(Mid, (c + f) / 2 + 2, 3, 3, T_FREEZE);
			UsedFreeze = true;
			break;
		}
		case S_PSEUDOV:
		{
			// stacked pseudofly corridors, 3 tiles high with freeze above and below, joined at alternating ends
			DummyParts++;
			Fill(x0 + 1, c, x1 - x0 - 2, Height, T_SOLID);
			const int Lanes = std::max(1, (Height - 2) / 7);
			for(int Lane = 0; Lane < Lanes; Lane++)
			{
				const int yy = c + 1 + Lane * 7;
				Fill(x0 + 2, yy, x1 - x0 - 4, 3, T_AIR);
				for(int xx = x0 + 2; xx < x1 - 2; xx++)
				{
					Put(xx, yy - 1, T_FREEZE);
					Put(xx, yy + 3, T_FREEZE);
				}
			}
			for(int Lane = 0; Lane + 1 < Lanes; Lane++)
			{
				const int yy = c + 1 + Lane * 7;
				const int Cx = Lane % 2 == 0 ? x1 - 5 : x0 + 2;
				Fill(Cx, yy + 3, 3, 5, T_AIR);
			}
			// open the first and the last lane towards the neighbours
			Fill(x0 + 1, c + 1, 1, 3, T_AIR);
			Fill(x1 - 2, c + 1 + (Lanes - 1) * 7, 1, 3, T_AIR);
			UsedFreeze = true;
			break;
		}
		case S_FLY:
		{
			// a fly part: a big open space with jetpack on at the entrance and off at the exit, freeze on the ceiling and floor and freeze obstacles to fly around
			if(Height < 12)
			{
				for(int xx = x0 + 1; xx < x1 - 1; xx++)
					FreezeAir(xx, f);
				break;
			}
			for(int yy = c; yy <= f; yy++)
			{
				Put(x0 + 1, yy, T_JETON);
				Put(x1 - 2, yy, T_JETOFF);
			}
			for(int xx = x0 + 2; xx < x1 - 2; xx++)
			{
				for(int Layer = 0; Layer < 3; Layer++)
				{
					FreezeAir(xx, c + Layer);
					FreezeAir(xx, f - Layer);
				}
			}
			int Side = Rand(0, 1);
			for(int px = x0 + 7; px + 8 < x1 - 3; px += Rand(9, 13))
			{
				const int Len = Rand(4, std::max(4, Height - 9));
				const int Top = Side ? c + 3 : f - 2 - Len;
				for(int dx = 0; dx < 3; dx++)
					for(int yy = Top; yy < Top + Len; yy++)
						FreezeAir(px + dx, yy);
				Side ^= 1;
				// a loose block of freeze in the middle of the free part
				if(Rand(0, 99) < 55)
				{
					const int by = Side ? c + 4 : f - 6;
					for(int dx = 0; dx < 3; dx++)
						for(int dy = 0; dy < 3; dy++)
							FreezeAir(px + 5 + dx, by + dy);
				}
			}
			break;
		}
		case S_HALL:
		{
			// long freeze bars across the hall, like the crosses of the studied maps
			if(Diff >= 1)
			{
				for(int px = x0 + 6; px + 4 < x1 - 3; px += Rand(12, 20))
				{
					const bool Down = Rand(0, 1);
					const int Len = Rand(Height / 2, std::max(Height / 2, Height - 3));
					for(int yy = 0; yy < Len; yy++)
						FreezeAir(px, Down ? c + yy : f - yy);
					if(Rand(0, 99) < 40)
						for(int dx = 0; dx < Rand(8, 18); dx++)
							FreezeAir(px + dx, Down ? c + Len : f - Len);
				}
			}
			// a big hall: notched ledges on the walls, ceiling and floor like stacked blocks, an unhookable strip in the ceiling
			for(int px = x0 + 2; px < x1 - 8; px += Rand(8, 14))
			{
				const int w = Rand(3, 7) * (B == 3 ? 2 : 1);
				const int h = Rand(1, 2) * B;
				Fill(px, c, w, h, T_SOLID);
				if(Rand(0, 1))
					Fill(px + B, c + h, std::max(1, w - 2 * B), B, T_SOLID);
			}
			for(int px = x0 + 3; px < x1 - 8; px += Rand(9, 15))
			{
				const int w = Rand(3, 8) * (B == 3 ? 2 : 1);
				const int h = Rand(1, 3);
				Fill(px, f - h + 1, w, h, T_SOLID);
				if(Diff >= 1)
					for(int xx = px; xx < px + w; xx++)
						FreezeAir(xx, f - h);
			}
			for(int px = x0 + 6; px + 10 < x1 - 3; px += Rand(14, 20))
				Fill(px, (c + f) / 2 + Rand(-2, 2), Rand(6, 10), B, Diff == 0 ? T_NOHOOK : T_SOLID);
			NoHookCeiling(x0, x1, c);
			break;
		}
		case S_TELELINE:
		{
			// the floor is a plate with a line of teleporters on top that throws you back to the start of the hall
			if(!Teleports || Diff == 0 || TeleBase + TeleUsed >= 250 || Height < 8)
			{
				for(int xx = x0 + 1; xx < x1 - 1; xx++)
					FreezeAir(xx, f);
				break;
			}
			const int Plate = Diff == 2 ? 2 : 1;
			const int Line = f - Plate;
			const int Number = TeleBase + TeleUsed++;
			Fill(x0, Line + 1, x1 - x0, Plate, T_SOLID);
			for(int xx = x0 + 4; xx < x1 - 1; xx++)
			{
				vTeleType[(size_t)Line * W + xx] = TILE_TELEIN;
				vTeleNum[(size_t)Line * W + xx] = (unsigned char)Number;
			}
			vTeleType[(size_t)(Line - 2) * W + x0 + 2] = TILE_TELEOUT;
			vTeleNum[(size_t)(Line - 2) * W + x0 + 2] = (unsigned char)Number;
			// ledges that reach over the line so it can be passed by hooking
			for(int px = x0 + 8; px + 6 < x1 - 3; px += Rand(10, 14))
				Fill(px, c, Rand(3, 6), Rand(2, std::max(2, Height / 3)), T_SOLID);
			for(int px = x0 + 10; px + 8 < x1 - 3; px += Rand(12, 18))
				Fill(px, Rand(c + 3, std::max(c + 3, Line - 4)), Rand(4, 8), B, T_SOLID);
			break;
		}
		case S_CAVE:
		{
			// a big cave: uneven ceiling and floor, pools of freeze and floating unhookable bars
			for(int px = x0 + 2; px < x1 - 2; px += Rand(3, 6))
			{
				Fill(px, c, Rand(2, 4), Rand(1, std::max(1, Height / 4)), T_SOLID);
				if(Rand(0, 99) < 60)
					Fill(px + Rand(0, 2), f - Rand(0, std::max(1, Height / 5)), Rand(2, 5), 6, T_SOLID);
			}
			for(int px = x0 + 4; px + 9 < x1 - 2; px += Rand(11, 16))
			{
				const int w = Rand(5, 9);
				for(int dx = 0; dx < w; dx++)
				{
					FreezeAir(px + dx, f);
					FreezeAir(px + dx, f - 1);
					if(Diff >= 1)
						FreezeAir(px + dx, f - 2);
				}
			}
			for(int bx = x0 + 5; bx + 9 < x1 - 3; bx += Rand(9, 14))
			{
				const int w = Rand(5, 9);
				const int by = Rand(c + 3, std::max(c + 3, f - 4));
				Fill(bx, by, w, B, T_NOHOOK);
				if(Diff >= 1)
					for(int dx = 0; dx < w; dx++)
					{
						FreezeAir(bx + dx, by - 1);
						FreezeAir(bx + dx, by + B);
					}
			}
			break;
		}
		case S_FREEZEROOM:
		{
			// the lower part of the room is a freeze field with solid islands and pillars, unhookable bars hang above
			const int Depth = std::max(3, Height / 2 - 1);
			for(int xx = x0 + 1; xx < x1 - 1; xx++)
				for(int yy = f - Depth + 1; yy <= f; yy++)
					FreezeAir(xx, yy);
			for(int px = x0 + 3; px + 6 < x1 - 2; px += Rand(8, 12))
			{
				if(Rand(0, 1))
					Fill(px, f - Depth + 1, Rand(2, 3) * (B == 3 ? 2 : 1), Depth, T_SOLID); // pillar
				else
					Fill(px, f - Depth + 2 + Rand(0, Depth / 2), Rand(4, 6), B, T_SOLID); // island
			}
			for(int bx = x0 + 4; bx + 10 < x1 - 3; bx += Rand(9, 13))
				Fill(bx, Rand(c + 1, std::max(c + 1, f - Depth - 2)), Rand(5, 9), B, T_NOHOOK);
			break;
		}
		case S_LADDER:
		{
			// a diagonal ladder of blocks, every block is wrapped in freeze and the ceiling and floor are freeze too
			const int Pitch = B + 3;
			const int Span = std::max(1, Height - B - 2);
			for(int i = 0;; i++)
			{
				const int bx = x0 + 3 + i * Pitch;
				if(bx + B > x1 - 3)
					break;
				const int by = f - B - ((i * (B + 1)) % Span);
				Fill(bx, by, B, B, T_SOLID);
				for(int xx = bx - 1; xx <= bx + B; xx++)
				{
					FreezeAir(xx, by - 1);
					FreezeAir(xx, by + B);
				}
				for(int yy = by; yy < by + B; yy++)
				{
					FreezeAir(bx - 1, yy);
					FreezeAir(bx + B, yy);
				}
			}
			for(int xx = x0 + 1; xx < x1 - 1; xx++)
			{
				if((xx - x0) % 11 < 8)
				{
					FreezeAir(xx, c);
					FreezeAir(xx, f);
				}
			}
			break;
		}
		case S_FRLINES:
		{
			// serpentine: floors with a line of freeze on top, gaps alternate between the left and the right side
			const int Step = 4 + B;
			int Index = 0;
			for(int yy = c + 2; yy + B < f; yy += Step, Index++)
			{
				const int GapW = Diff == 2 ? 4 : 6;
				const bool GapLeft = Index % 2 == 0;
				for(int xx = x0 + 1; xx < x1 - 1; xx++)
				{
					const bool InGap = GapLeft ? xx < x0 + 1 + GapW : xx >= x1 - 1 - GapW;
					if(InGap)
						continue;
					Fill(xx, yy + 1, 1, B, T_SOLID);
					FreezeAir(xx, yy);
					if(Diff == 2)
						FreezeAir(xx, yy + 1 + B);
				}
			}
			break;
		}
		default:
			break;
		}
	};

	// ---- start room ----
	const int StartHeight = Diff == 0 ? 8 : 6;
	int PrevC = std::clamp(H / 2 - StartHeight / 2, MinC, MaxF - StartHeight + 1);
	int PrevF = PrevC + StartHeight - 1;
	for(int x = 2; x <= 10; x++)
		Fill(x, PrevC, 1, StartHeight, T_AIR);
	Put(5, PrevF, T_SPAWN);
	for(int y = PrevC; y <= PrevF; y++)
		Put(10, y, T_START);

	// ---- sections ----
	const int FinishStart = W - 12;
	int x = 11;
	int SinceTele = 0;
	int &NumTele = TeleUsed;
	while(x < FinishStart - 8)
	{
		int Id = pPool[Rand(0, PoolSize - 1)];
		if(Fly && Rand(0, 99) < 45)
		{
			static const int s_aDummy[] = {S_PSEUDO, S_PSEUDO, S_DUMMYGAP, S_PSEUDOV};
			Id = s_aDummy[Rand(0, 3)];
		}
		UsedMask |= 1u << Id;
		int Len = Rand(Diff == 0 ? 18 : 16, Diff == 0 ? 28 : 26) + (B == 3 ? 4 : 0);
		if(Id == S_PSEUDO || Id == S_DUMMYGAP)
			Len = Rand(30, 44);
		else if(Id == S_PSEUDOV)
			Len = Rand(26, 36);
		int End = std::min(x + Len, FinishStart);
		if(FinishStart - End < 14)
			End = FinishStart;
		UsedFreeze = false;

		// teleporter part: a dead end with a teleporter and an arrival room behind a wall
		if(Teleports && SinceTele >= 2 && x + 34 < FinishStart && Rand(0, 99) < 45 && TeleBase + NumTele < 250)
		{
			const int In = TeleBase + NumTele;
			// chamber of the dead end, same rows as before
			int c = PrevC, f = PrevF;
			if(f - c + 1 < 5)
				f = c + 4;
			for(int xx = x; xx < x + 12; xx++)
				Fill(xx, std::min(PrevC, c), 1, std::max(PrevF, f) - std::min(PrevC, c) + 1, T_AIR);
			for(int xx = x + 3; xx < x + 9; xx++)
				FreezeAir(xx, f);
			// tele in tiles at the end, the wall behind them is solid
			for(int xx = x + 10; xx < x + 12; xx++)
				for(int yy = c; yy <= f; yy++)
				{
					vTeleType[(size_t)yy * W + xx] = TILE_TELEIN;
					vTeleNum[(size_t)yy * W + xx] = (unsigned char)In;
				}
			// arrival room
			const int AX = x + 15;
			int c2, f2;
			ChooseRange(PrevC, PrevF, aHMin[Diff], aHMax[Diff], false, c2, f2);
			if(f2 - c2 + 1 < 5)
				f2 = std::min(MaxF, c2 + 4);
			for(int xx = AX; xx < AX + 8; xx++)
				Fill(xx, c2, 1, f2 - c2 + 1, T_AIR);
			vTeleType[(size_t)f2 * W + AX + 2] = TILE_TELEOUT;
			vTeleNum[(size_t)f2 * W + AX + 2] = (unsigned char)In;
			NumTele++;
			SinceTele = 0;
			x = AX + 8;
			PrevC = c2;
			PrevF = f2;
			continue;
		}
		SinceTele++;

		// vertical shaft: the next chamber is far above or below
		if(Id == S_SHAFT)
		{
			const int ShaftW = Diff == 0 ? 6 : 4;
			int c2 = 0, f2 = 0;
			bool Found = false;
			for(int Try = 0; Try < 40 && !Found; Try++)
			{
				ChooseRange(PrevC, PrevF, aHMin[Diff], aHMax[Diff], false, c2, f2);
				Found = std::abs(c2 - PrevC) >= 6;
			}
			if(!Found || x + 5 + ShaftW + 6 >= FinishStart)
				Id = S_OPEN;
			else
			{
				const int Top = std::min(PrevC, c2), Bottom = std::max(PrevF, f2);
				const int SX = x + 5;
				for(int xx = x; xx < SX; xx++)
					Fill(xx, PrevC, 1, PrevF - PrevC + 1, T_AIR);
				for(int xx = SX; xx < SX + ShaftW; xx++)
					Fill(xx, Top, 1, Bottom - Top + 1, T_AIR);
				for(int xx = SX + ShaftW; xx < SX + ShaftW + 6; xx++)
					Fill(xx, c2, 1, f2 - c2 + 1, T_AIR);
				// freeze along the walls of the shaft, outside of the openings of the chambers
				for(int yy = Top; yy <= Bottom; yy++)
				{
					if(Diff >= 1 && (yy < PrevC || yy > PrevF))
						PutIfAir(SX, yy, T_FREEZE);
					if(Diff == 2 && (yy < c2 || yy > f2))
						PutIfAir(SX + ShaftW - 1, yy, T_FREEZE);
				}
				x = SX + ShaftW + 6;
				PrevC = c2;
				PrevF = f2;
				continue;
			}
		}

		// narrow tunnel that wanders up and down, with freeze on the floor (and the ceiling when brutal)
		if(Id == S_SQUEEZE)
		{
			const int Th = 5;
			if(PrevF - PrevC + 1 < Th)
				Id = S_OPEN;
			else
			{
				int ct = Rand(PrevC, PrevF - Th + 1);
				for(int xx = x; xx < End; xx++)
				{
					Fill(xx, ct, 1, Th, T_AIR);
					if(xx == x)
					{
						for(int yy = ct; yy < ct + Th; yy++)
							Put(xx, yy, T_UNFREEZE);
					}
					else if(Diff >= 1 && xx > x + 1 && (xx - x) % 7 != 0)
					{
						FreezeAir(xx, ct + Th - 1);
						if(Diff == 2)
							FreezeAir(xx, ct);
					}
					if(Rand(0, 99) < 22)
					{
						const int Next = std::clamp(ct + Rand(-1, 1), MinC, MaxF - Th + 1);
						// the columns have to touch, so the new rows are carved in the same column
						Fill(xx, std::min(ct, Next), 1, Th + std::abs(Next - ct), T_AIR);
						ct = Next;
					}
				}
				PrevC = ct;
				PrevF = ct + Th - 1;
				x = End;
				continue;
			}
		}

		// normal chamber
		int HMin = std::max(aHMin[Diff], aNeedHeight[Id]), HMax = std::max(aHMax[Diff], aNeedHeight[Id]);
		int c, f;
		ChooseRange(PrevC, PrevF, HMin, HMax, true, c, f);
		// the opening between the old and the new chamber
		Fill(x, std::min(PrevC, c), 1, std::max(PrevF, f) - std::min(PrevC, c) + 1, T_AIR);
		for(int xx = x + 1; xx < End; xx++)
			Fill(xx, c, 1, f - c + 1, T_AIR);
		Section(Id, x + 1, End, c, f);
		// unfreeze strip at the start of parts with freeze, so a freeze from before never carries over
		if(UsedFreeze && Diff >= 1)
			for(int yy = c; yy <= f; yy++)
				if(Inside(x + 1, yy) && At(x + 1, yy) == T_AIR)
					At(x + 1, yy) = T_UNFREEZE;
		PrevC = c;
		PrevF = f;
		x = End;
	}

	// ---- finish room ----
	if(PrevF - PrevC + 1 < 5)
		PrevF = std::min(MaxF, PrevC + 4);
	for(int xx = x; xx <= W - 3; xx++)
		Fill(xx, PrevC, 1, PrevF - PrevC + 1, T_AIR);
	for(int yy = PrevC; yy <= PrevF; yy++)
		Put(W - 7, yy, T_FINISH);

	// ---- unhookable outer frame, one more solid ring stays inside it ----
	for(int yy = 0; yy < H; yy++)
		for(int xx = 0; xx < W; xx++)
			if(xx == 0 || yy == 0 || xx == W - 1 || yy == H - 1)
				At(xx, yy) = (unsigned char)T_NOHOOK;

	// ---- rate this candidate ----
	{
		const auto Passable = [&](int Index) {
			const unsigned char Tile = vBuf[Index];
			return Tile != T_SOLID && Tile != T_NOHOOK && Tile != T_DEATH && Tile != T_FREEZE;
		};
		int SpawnIndex = -1;
		int FinishIndex = -1;
		int FreezeCount = 0;
		int AirCount = 0;
		for(int i = 0; i < W * H; i++)
		{
			if(vBuf[i] == T_SPAWN)
				SpawnIndex = i;
			else if(vBuf[i] == T_FINISH && FinishIndex < 0)
				FinishIndex = i;
			if(vBuf[i] == T_FREEZE)
				FreezeCount++;
			if(vBuf[i] != T_SOLID && vBuf[i] != T_NOHOOK)
				AirCount++;
		}
		float Score = 0.0f;
		if(SpawnIndex >= 0 && FinishIndex >= 0)
		{
			std::vector<unsigned char> vSeen((size_t)W * H, 0);
			std::vector<int> vStack;
			vStack.push_back(SpawnIndex);
			vSeen[SpawnIndex] = 1;
			int Reached = 0;
			while(!vStack.empty())
			{
				const int Cur = vStack.back();
				vStack.pop_back();
				Reached++;
				const int cx = Cur % W, cy = Cur / W;
				const int aNext[4] = {cx > 0 ? Cur - 1 : -1, cx < W - 1 ? Cur + 1 : -1, cy > 0 ? Cur - W : -1, cy < H - 1 ? Cur + W : -1};
				for(const int Next : aNext)
					if(Next >= 0 && !vSeen[Next] && Passable(Next))
					{
						vSeen[Next] = 1;
						vStack.push_back(Next);
					}
			}
			Score = vSeen[FinishIndex] ? 1000.0f : 100.0f * (float)Reached / (float)std::max(1, AirCount);
		}
		const float AirFraction = (float)AirCount / (float)(W * H);
		const float FreezeFraction = (float)FreezeCount / (float)std::max(1, AirCount);
		Score += 100.0f - std::abs(AirFraction - 0.45f) * 200.0f;
		Score += 60.0f - std::abs(FreezeFraction - (Diff == 0 ? 0.04f : 0.09f)) * 400.0f;
		int Variety = 0;
		for(unsigned Mask = UsedMask; Mask; Mask >>= 1)
			Variety += Mask & 1u;
		Score += (float)Variety * 12.0f + (float)TeleUsed * 15.0f + (float)(Rng() % 1000) / 100.0f;
		if(Fly)
			Score += (float)std::min(DummyParts, 6) * 40.0f;
		if(Score > BestScore)
		{
			BestScore = Score;
			vOutBuf = vBuf;
			vOutTeleType = vTeleType;
			vOutTeleNum = vTeleNum;
		}
	}
	}

	// ---- hollow out: only a wall around the course is kept and the rest of the rock becomes empty ----
	// The walls are mostly unhookable with a few hookable patches, like in the maps that were studied.
	{
		const int Wall = B == 3 ? 3 : 1;
		std::vector<unsigned char> vNew = vOutBuf;
		const auto IsRock = [&](int xx, int yy) {
			const unsigned char Tile = vOutBuf[(size_t)yy * W + xx];
			return Tile == TILE_SOLID || Tile == TILE_NOHOOK;
		};
		for(int yy = 0; yy < H; yy++)
		{
			for(int xx = 0; xx < W; xx++)
			{
				if(!IsRock(xx, yy))
					continue;
				if(xx < Wall || yy < Wall || xx >= W - Wall || yy >= H - Wall)
				{
					vNew[(size_t)yy * W + xx] = TILE_NOHOOK;
					continue;
				}
				bool Near = false;
				for(int dy = -Wall; dy <= Wall && !Near; dy++)
					for(int dx = -Wall; dx <= Wall && !Near; dx++)
					{
						const int nx = xx + dx, ny = yy + dy;
						if(nx >= 0 && ny >= 0 && nx < W && ny < H && !IsRock(nx, ny))
							Near = true;
					}
				if(!Near)
					vNew[(size_t)yy * W + xx] = TILE_AIR;
				else if(vOutBuf[(size_t)yy * W + xx] == TILE_SOLID)
					vNew[(size_t)yy * W + xx] = (((xx / 6) * 7 + (yy / 6) * 13 + (xx / 6) * (yy / 6)) % 9 == 0) ? TILE_SOLID : TILE_NOHOOK;
			}
		}
		// freeze lines that hug the floors (and on brutal also the ceilings) of the halls in long runs
		if(Diff >= 1)
		{
			for(int Pass = 0; Pass < (Diff == 2 ? 2 : 1); Pass++)
			{
				const int Dy = Pass == 0 ? 1 : -1;
				int Run = 0;
				for(int yy = 3; yy < H - 3; yy++)
				{
					Run = 0;
					for(int xx = 14; xx < W - 14; xx++)
					{
						const bool Floor = vNew[(size_t)yy * W + xx] == TILE_AIR && vNew[(size_t)(yy + Dy) * W + xx] != TILE_AIR && vNew[(size_t)(yy + Dy) * W + xx] <= TILE_NOHOOK && vNew[(size_t)(yy + Dy) * W + xx] != 0;
						if(Run > 0 && Floor)
						{
							vNew[(size_t)yy * W + xx] = TILE_FREEZE;
							Run--;
						}
						else if(Run > 0)
							Run = 0;
						else if(Floor && Rand(0, 99) < (Diff == 2 ? 3 : 2))
							Run = Rand(8, 28);
					}
				}
			}
		}
		vOutBuf = vNew;
	}

	// ---- hand the result to the job that builds it column by column ----
	SGeneratorJob &Job = m_GeneratorJob;
	Job = SGeneratorJob();
	Job.m_Running = true;
	Job.m_X0 = X0;
	Job.m_Y0 = Y0;
	Job.m_W = W;
	Job.m_H = H;
	Job.m_vTile = vOutBuf;
	Job.m_vTeleType = vOutTeleType;
	Job.m_vTeleNum = vOutTeleNum;
	Job.m_GameGroupIndex = std::find(Map()->m_vpGroups.begin(), Map()->m_vpGroups.end(), Map()->m_pGameGroup) - Map()->m_vpGroups.begin();
	Job.m_pTeleLayer = pTeleLayer;

	bool HasTele = false;
	for(const unsigned char Type : vOutTeleType)
		HasTele |= Type != 0;
	if(HasTele && !Job.m_pTeleLayer)
	{
		Job.m_pTeleLayer = std::make_shared<CLayerTele>(Map(), pGameLayer->m_Width, pGameLayer->m_Height);
		Map()->MakeTeleLayer(Job.m_pTeleLayer);
		Map()->m_pGameGroup->AddLayer(Job.m_pTeleLayer);
		Job.m_vpActions.push_back(std::make_shared<CEditorActionAddLayer>(Map(), Job.m_GameGroupIndex, (int)Map()->m_pGameGroup->m_vpLayers.size() - 1));
	}

	// the thinking time grows with the size of the zone
	const float Area = (float)W * (float)H;
	const float aMax[3] = {4.0f, 90.0f, 300.0f};
	const float aPerTile[3] = {0.0005f, 0.012f, 0.05f};
	Job.m_Duration = std::clamp(Area * aPerTile[std::clamp(m_GeneratorThink, 0, 2)], 1.5f, aMax[std::clamp(m_GeneratorThink, 0, 2)]);
}

// Applies the next columns of the running job to the layers. With Finish everything that is left is applied
// at once. When all columns are done the whole generation is recorded as one undo step.
void CEditor::StepGeneratorJob(bool Finish)
{
	SGeneratorJob &Job = m_GeneratorJob;
	if(!Job.m_Running)
		return;
	std::shared_ptr<CLayerTiles> pGameLayer = Map()->m_pGameLayer;
	if(!pGameLayer)
	{
		Job = SGeneratorJob();
		return;
	}

	const float Progress = Finish ? 1.0f : std::clamp(Job.m_Elapsed / Job.m_Duration, 0.0f, 1.0f);
	// ease in and out so that it looks like it first plans and then builds
	const float Eased = Progress * Progress * (3.0f - 2.0f * Progress);
	const int Target = Progress >= 1.0f ? Job.m_W : (int)(Eased * Job.m_W);
	for(int xx = Job.m_Revealed; xx < Target; xx++)
	{
		for(int y = 0; y < Job.m_H; y++)
		{
			const int LayerX = Job.m_X0 + xx, LayerY = Job.m_Y0 + y;
			const unsigned char NewTile = Job.m_vTile[(size_t)y * Job.m_W + xx];
			const CTile Old = pGameLayer->m_pTiles[(size_t)LayerY * pGameLayer->m_Width + LayerX];
			if(Old.m_Index != NewTile || Old.m_Flags != 0)
			{
				pGameLayer->SetTile(LayerX, LayerY, CTile{NewTile});
				Job.m_Changes++;
			}
			if(Job.m_pTeleLayer)
			{
				const int TeleIndex = LayerY * Job.m_pTeleLayer->m_Width + LayerX;
				const unsigned char NewType = Job.m_vTeleType[(size_t)y * Job.m_W + xx];
				const unsigned char NewNumber = Job.m_vTeleNum[(size_t)y * Job.m_W + xx];
				if(Job.m_pTeleLayer->m_pTeleTile[TeleIndex].m_Type != NewType || (NewType != 0 && Job.m_pTeleLayer->m_pTeleTile[TeleIndex].m_Number != NewNumber))
				{
					Job.m_pTeleLayer->SetTeleTile(LayerX, LayerY, NewType, NewNumber);
					Job.m_Changes++;
				}
			}
		}
	}
	Job.m_Revealed = Target;
	if(Job.m_Revealed < Job.m_W)
		return;

	if(Job.m_Changes > 0)
	{
		Job.m_vpActions.push_back(std::make_shared<CEditorBrushDrawAction>(Map(), Job.m_GameGroupIndex));
		char aDisplay[256];
		str_format(aDisplay, sizeof(aDisplay), "Generate DDRace gameplay (x%d)", Job.m_Changes);
		Map()->m_EditorHistory.RecordAction(std::make_shared<CEditorActionBulk>(Map(), Job.m_vpActions, aDisplay, true));
	}
	Job = SGeneratorJob();
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
