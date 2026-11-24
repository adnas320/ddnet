// Font editor: the player draws the letters with the mouse, "Done" builds a real .ttf file from the strokes.
#include "menus.h"

#include <base/color.h>
#include <base/io.h>
#include <base/str.h>

#include <engine/client.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <game/client/gameclient.h>
#include <game/client/ui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

using CStrokes = std::vector<std::vector<vec2>>; // points are 0..1 in the drawing area

struct CPt
{
	int m_X;
	int m_Y;
};

void AddContour(std::vector<std::vector<CPt>> &vOut, std::vector<CPt> vContour)
{
	// the outer contours of a TrueType glyph run clockwise (y up), so the signed area has to be negative
	long long Area = 0;
	for(size_t i = 0; i < vContour.size(); i++)
	{
		const CPt &A = vContour[i];
		const CPt &B = vContour[(i + 1) % vContour.size()];
		Area += (long long)A.m_X * B.m_Y - (long long)B.m_X * A.m_Y;
	}
	if(Area > 0)
		std::reverse(vContour.begin(), vContour.end());
	vOut.push_back(std::move(vContour));
}

struct CGlyphOutline
{
	int m_Codepoint = 0;
	std::vector<std::vector<CPt>> m_vContours;
	int m_Advance = 400;
	int m_XMin = 0, m_YMin = 0, m_XMax = 0, m_YMax = 0;
};

// strokes -> a set of overlapping contours (quad per segment, octagon per point); the fill rule of the rasterizer unites them
CGlyphOutline OutlineFromStrokes(int Codepoint, const CStrokes &Strokes, int Thick)
{
	CGlyphOutline Out;
	Out.m_Codepoint = Codepoint;
	const float Half = Thick * 0.5f;
	for(const auto &Stroke : Strokes)
	{
		std::vector<vec2> vPoints;
		for(const vec2 &P : Stroke)
		{
			const vec2 F(P.x * 800.0f, 800.0f - P.y * 1000.0f);
			if(vPoints.empty() || distance(vPoints.back(), F) >= 8.0f)
				vPoints.push_back(F);
		}
		if(vPoints.empty())
			continue;
		for(size_t i = 0; i < vPoints.size(); i++)
		{
			std::vector<CPt> vOct;
			for(int k = 0; k < 8; k++)
			{
				const float A = (k + 0.5f) * pi / 4.0f;
				vOct.push_back({(int)std::lround(vPoints[i].x + std::cos(A) * Half), (int)std::lround(vPoints[i].y + std::sin(A) * Half)});
			}
			AddContour(Out.m_vContours, vOct);
			if(i + 1 < vPoints.size())
			{
				const vec2 D = vPoints[i + 1] - vPoints[i];
				const float Len = length(D);
				if(Len < 1.0f)
					continue;
				const vec2 N = vec2(-D.y, D.x) / Len * Half;
				const auto Q = [](vec2 V) { return CPt{(int)std::lround(V.x), (int)std::lround(V.y)}; };
				AddContour(Out.m_vContours, {Q(vPoints[i] + N), Q(vPoints[i + 1] + N), Q(vPoints[i + 1] - N), Q(vPoints[i] - N)});
			}
		}
	}
	if(Out.m_vContours.empty())
		return Out;
	int XMin = 1 << 30, YMin = 1 << 30, XMax = -(1 << 30), YMax = -(1 << 30);
	for(const auto &C : Out.m_vContours)
		for(const CPt &P : C)
		{
			XMin = std::min(XMin, P.m_X);
			XMax = std::max(XMax, P.m_X);
			YMin = std::min(YMin, P.m_Y);
			YMax = std::max(YMax, P.m_Y);
		}
	// the letter always starts a little to the right of the origin and brings its own width
	const int Shift = 60 - XMin;
	for(auto &C : Out.m_vContours)
		for(CPt &P : C)
			P.m_X += Shift;
	Out.m_XMin = 60;
	Out.m_XMax = XMax + Shift;
	Out.m_YMin = YMin;
	Out.m_YMax = YMax;
	Out.m_Advance = std::clamp(Out.m_XMax + 70, 300, 1400);
	return Out;
}

struct CBuf
{
	std::vector<uint8_t> m_vData;
	void U8(unsigned V) { m_vData.push_back((uint8_t)V); }
	void U16(unsigned V)
	{
		U8((V >> 8) & 0xff);
		U8(V & 0xff);
	}
	void I16(int V) { U16((unsigned)(V & 0xffff)); }
	void U32(uint32_t V)
	{
		U16(V >> 16);
		U16(V & 0xffff);
	}
	void Pad4()
	{
		while(m_vData.size() % 4)
			U8(0);
	}
};

uint32_t Checksum(const std::vector<uint8_t> &vData)
{
	uint32_t Sum = 0;
	for(size_t i = 0; i < vData.size(); i += 4)
	{
		uint32_t W = 0;
		for(size_t k = 0; k < 4; k++)
			W = (W << 8) | (i + k < vData.size() ? vData[i + k] : 0);
		Sum += W;
	}
	return Sum;
}

bool BuildTtf(const std::map<int, CStrokes> &Glyphs, int Thick, const char *pFamily, std::vector<uint8_t> &vOut)
{
	// glyph 0 is .notdef, the rest are sorted by code point, the space is always present
	std::vector<CGlyphOutline> vGlyphs;
	CGlyphOutline Space;
	Space.m_Codepoint = 0x20;
	Space.m_Advance = 330;
	bool SpaceDone = false;
	for(const auto &[Codepoint, Strokes] : Glyphs)
	{
		if(Codepoint == 0x20)
			continue;
		if(!SpaceDone && Codepoint > 0x20)
		{
			vGlyphs.push_back(Space);
			SpaceDone = true;
		}
		CGlyphOutline G = OutlineFromStrokes(Codepoint, Strokes, Thick);
		if(!G.m_vContours.empty())
			vGlyphs.push_back(std::move(G));
	}
	if(!SpaceDone)
		vGlyphs.push_back(Space);
	std::sort(vGlyphs.begin(), vGlyphs.end(), [](const CGlyphOutline &A, const CGlyphOutline &B) { return A.m_Codepoint < B.m_Codepoint; });
	if(vGlyphs.size() < 2)
		return false;
	const int NumGlyphs = (int)vGlyphs.size() + 1;

	// glyf + loca
	CBuf Glyf;
	std::vector<uint32_t> vLoca;
	int MaxPoints = 0, MaxContours = 0, GXMin = 0, GYMin = -200, GXMax = 1000, GYMax = 800, MaxAdvance = 500;
	vLoca.push_back(0); // .notdef is empty
	vLoca.push_back(0);
	for(size_t g = 0; g < vGlyphs.size(); g++)
	{
		const CGlyphOutline &G = vGlyphs[g];
		if(!G.m_vContours.empty())
		{
			int Points = 0;
			for(const auto &C : G.m_vContours)
				Points += (int)C.size();
			MaxPoints = std::max(MaxPoints, Points);
			MaxContours = std::max(MaxContours, (int)G.m_vContours.size());
			Glyf.I16((int)G.m_vContours.size());
			Glyf.I16(G.m_XMin);
			Glyf.I16(G.m_YMin);
			Glyf.I16(G.m_XMax);
			Glyf.I16(G.m_YMax);
			int End = -1;
			for(const auto &C : G.m_vContours)
			{
				End += (int)C.size();
				Glyf.U16(End);
			}
			Glyf.U16(0); // no instructions
			for(int i = 0; i < Points; i++)
				Glyf.U8(0x01); // on curve, 16 bit deltas
			int Prev = 0;
			for(const auto &C : G.m_vContours)
				for(const CPt &P : C)
				{
					Glyf.I16(P.m_X - Prev);
					Prev = P.m_X;
				}
			Prev = 0;
			for(const auto &C : G.m_vContours)
				for(const CPt &P : C)
				{
					Glyf.I16(P.m_Y - Prev);
					Prev = P.m_Y;
				}
			Glyf.Pad4();
			GXMax = std::max(GXMax, G.m_XMax);
			GYMax = std::max(GYMax, G.m_YMax);
			GYMin = std::min(GYMin, G.m_YMin);
		}
		MaxAdvance = std::max(MaxAdvance, G.m_Advance);
		vLoca.push_back((uint32_t)Glyf.m_vData.size());
	}
	CBuf Loca;
	for(const uint32_t O : vLoca)
		Loca.U32(O);

	// hmtx
	CBuf Hmtx;
	Hmtx.U16(500);
	Hmtx.I16(0);
	for(const CGlyphOutline &G : vGlyphs)
	{
		Hmtx.U16(G.m_Advance);
		Hmtx.I16(G.m_vContours.empty() ? 0 : G.m_XMin);
	}

	// cmap, format 12
	std::vector<std::array<uint32_t, 3>> vGroups;
	for(size_t g = 0; g < vGlyphs.size(); g++)
	{
		const uint32_t Cp = (uint32_t)vGlyphs[g].m_Codepoint, Gid = (uint32_t)g + 1;
		if(!vGroups.empty() && vGroups.back()[1] + 1 == Cp && vGroups.back()[2] + (vGroups.back()[1] - vGroups.back()[0]) + 1 == Gid)
			vGroups.back()[1] = Cp;
		else
			vGroups.push_back({Cp, Cp, Gid});
	}
	CBuf Cmap;
	Cmap.U16(0);
	Cmap.U16(1);
	Cmap.U16(3);
	Cmap.U16(10);
	Cmap.U32(12);
	Cmap.U16(12);
	Cmap.U16(0);
	Cmap.U32(16 + 12 * (uint32_t)vGroups.size());
	Cmap.U32(0);
	Cmap.U32((uint32_t)vGroups.size());
	for(const auto &Group : vGroups)
	{
		Cmap.U32(Group[0]);
		Cmap.U32(Group[1]);
		Cmap.U32(Group[2]);
	}

	// head
	CBuf Head;
	Head.U32(0x00010000);
	Head.U32(0x00010000);
	Head.U32(0); // checkSumAdjustment, filled in later
	Head.U32(0x5F0F3CF5);
	Head.U16(3);
	Head.U16(1000);
	Head.U32(0);
	Head.U32(0);
	Head.U32(0);
	Head.U32(0);
	Head.I16(GXMin);
	Head.I16(GYMin);
	Head.I16(GXMax);
	Head.I16(GYMax);
	Head.U16(0);
	Head.U16(8);
	Head.I16(2);
	Head.I16(1); // long loca
	Head.I16(0);

	// hhea
	CBuf Hhea;
	Hhea.U32(0x00010000);
	Hhea.I16(800);
	Hhea.I16(-200);
	Hhea.I16(0);
	Hhea.U16(MaxAdvance);
	Hhea.I16(0);
	Hhea.I16(0);
	Hhea.I16(GXMax);
	Hhea.I16(1);
	Hhea.I16(0);
	Hhea.I16(0);
	for(int i = 0; i < 4; i++)
		Hhea.I16(0);
	Hhea.I16(0);
	Hhea.U16(NumGlyphs);

	// maxp
	CBuf Maxp;
	Maxp.U32(0x00010000);
	Maxp.U16(NumGlyphs);
	Maxp.U16(MaxPoints);
	Maxp.U16(MaxContours);
	Maxp.U16(0);
	Maxp.U16(0);
	Maxp.U16(1);
	for(int i = 0; i < 8; i++)
		Maxp.U16(0);

	// name
	CBuf Name;
	{
		const char *apStrings[4] = {pFamily, "Regular", pFamily, pFamily};
		const int aIds[4] = {1, 2, 4, 6};
		Name.U16(0);
		Name.U16(4);
		Name.U16(6 + 4 * 12);
		unsigned Offset = 0;
		for(int i = 0; i < 4; i++)
		{
			const unsigned Len = (unsigned)str_length(apStrings[i]) * 2;
			Name.U16(3);
			Name.U16(1);
			Name.U16(0x409);
			Name.U16(aIds[i]);
			Name.U16(Len);
			Name.U16(Offset);
			Offset += Len;
		}
		for(int i = 0; i < 4; i++)
			for(const char *p = apStrings[i]; *p; p++)
			{
				Name.U8(0);
				Name.U8((unsigned char)*p);
			}
	}

	// post, format 3
	CBuf Post;
	Post.U32(0x00030000);
	Post.U32(0);
	Post.I16(-100);
	Post.I16(50);
	Post.U32(0);
	for(int i = 0; i < 4; i++)
		Post.U32(0);

	struct CTable
	{
		const char *m_pTag;
		CBuf *m_pBuf;
	};
	CTable aTables[9] = {{"cmap", &Cmap}, {"glyf", &Glyf}, {"head", &Head}, {"hhea", &Hhea}, {"hmtx", &Hmtx}, {"loca", &Loca}, {"maxp", &Maxp}, {"name", &Name}, {"post", &Post}};
	CBuf File;
	File.U32(0x00010000);
	File.U16(9);
	File.U16(128);
	File.U16(3);
	File.U16(16);
	uint32_t Offset = 12 + 16 * 9;
	size_t HeadOffset = 0;
	for(CTable &T : aTables)
	{
		for(int k = 0; k < 4; k++)
			File.U8((unsigned char)T.m_pTag[k]);
		File.U32(Checksum(T.m_pBuf->m_vData));
		File.U32(Offset);
		File.U32((uint32_t)T.m_pBuf->m_vData.size());
		if(str_comp(T.m_pTag, "head") == 0)
			HeadOffset = Offset;
		Offset += ((uint32_t)T.m_pBuf->m_vData.size() + 3) & ~3u;
	}
	for(CTable &T : aTables)
	{
		File.m_vData.insert(File.m_vData.end(), T.m_pBuf->m_vData.begin(), T.m_pBuf->m_vData.end());
		File.Pad4();
	}
	const uint32_t Adjust = 0xB1B0AFBAu - Checksum(File.m_vData);
	File.m_vData[HeadOffset + 8] = (Adjust >> 24) & 0xff;
	File.m_vData[HeadOffset + 9] = (Adjust >> 16) & 0xff;
	File.m_vData[HeadOffset + 10] = (Adjust >> 8) & 0xff;
	File.m_vData[HeadOffset + 11] = Adjust & 0xff;
	vOut = std::move(File.m_vData);
	return true;
}

struct CFontEditorState
{
	std::map<int, CStrokes> m_Glyphs;
	std::vector<int> m_vCustom;
	int m_Selected = 0x410;
	int m_Lang = 0; // 0 Russian, 1 English, 2 Chinese
	int m_Sub = 0;
	bool m_Preview = false;
	int m_Thick = 80;
	bool m_Drawing = false;
	char m_aStatus[160] = "";
	CLineInputBuffered<32> m_Name;
	CLineInputBuffered<16> m_CustomInput;
	bool m_Init = false;
};

const char *LangName(int Lang)
{
	static const char *s_apNames[3] = {"Русский", "English", "中文"};
	return s_apNames[Lang];
}

const char *SubName(int Lang, int Sub)
{
	static const char *s_aapNames[3][5] = {
		{"А–Я", "а–я", "0–9", "Знаки", "Свои"},
		{"A–Z", "a–z", "0–9", "Знаки", "Свои"},
		{"常用 1", "常用 2", "0–9", "标点", "Свои"}};
	return s_aapNames[Lang][Sub];
}

std::vector<int> DecodeAll(const char *pText)
{
	std::vector<int> v;
	while(*pText)
		v.push_back(str_utf8_decode(&pText));
	return v;
}

std::vector<int> GroupChars(const CFontEditorState &S, int Lang, int Sub)
{
	std::vector<int> v;
	if(Sub == 4)
		return S.m_vCustom;
	if(Sub == 2)
	{
		for(int c = '0'; c <= '9'; c++)
			v.push_back(c);
		return v;
	}
	if(Lang == 0)
	{
		if(Sub == 0)
		{
			v.push_back(0x401);
			for(int c = 0x410; c <= 0x42F; c++)
				v.push_back(c);
		}
		else if(Sub == 1)
		{
			v.push_back(0x451);
			for(int c = 0x430; c <= 0x44F; c++)
				v.push_back(c);
		}
		else
			return DecodeAll(".,!?-:;()+=/'\"@#%&*_");
	}
	else if(Lang == 1)
	{
		if(Sub == 0)
			for(int c = 'A'; c <= 'Z'; c++)
				v.push_back(c);
		else if(Sub == 1)
			for(int c = 'a'; c <= 'z'; c++)
				v.push_back(c);
		else
			return DecodeAll(".,!?-:;()+=/'\"@#%&*_");
	}
	else
	{
		// the most common simplified characters, in two pages
		static const char *s_pCommon = "的一是不了人我在有他这为之大来以个中上们到说国和地也子时道出而要于就下得可你年生自会那后能对着事其里所去行过家十用发天如然作方成者多日都三小军二无同么经法当起与好看学进种将还分此心前面又定见只主没公从";
		static const char *s_pPunct = "，。！？：；、（）“”《》—…·";
		if(Sub == 3)
			return DecodeAll(s_pPunct);
		const std::vector<int> All = DecodeAll(s_pCommon);
		const size_t From = Sub == 0 ? 0 : 48, To = std::min(All.size(), From + 48);
		for(size_t i = From; i < To; i++)
			v.push_back(All[i]);
	}
	return v;
}

std::string Utf8Of(int Cp)
{
	char aBuf[8] = {0};
	str_utf8_encode(aBuf, Cp);
	return aBuf;
}

} // namespace

void CMenus::RenderFontEditor(CUIRect MainView)
{
	static CFontEditorState s_S;
	CFontEditorState &S = s_S;
	if(!S.m_Init)
	{
		S.m_Init = true;
		S.m_Name.Set("myfont");
	}

	const auto DraftPath = [&](char *pBuf, size_t Size) { str_format(pBuf, Size, "fonts/%s.fontdraw", S.m_Name.GetString()); };
	const auto SaveDraft = [&]() {
		Storage()->CreateFolder("fonts", IStorage::TYPE_SAVE);
		char aPath[128];
		DraftPath(aPath, sizeof(aPath));
		IOHANDLE File = Storage()->OpenFile(aPath, IOFLAG_WRITE, IStorage::TYPE_SAVE);
		if(!File)
		{
			str_copy(S.m_aStatus, "Не удалось сохранить черновик.");
			return;
		}
		std::ostringstream Out;
		Out << "thick " << S.m_Thick << "\n";
		for(const int Cp : S.m_vCustom)
			Out << "custom " << Cp << "\n";
		for(const auto &[Cp, Strokes] : S.m_Glyphs)
		{
			if(Strokes.empty())
				continue;
			Out << "g " << Cp << " " << Strokes.size() << "\n";
			for(const auto &Stroke : Strokes)
			{
				Out << "s " << Stroke.size();
				for(const vec2 &P : Stroke)
					Out << " " << (int)std::lround(P.x * 10000.0f) << " " << (int)std::lround(P.y * 10000.0f);
				Out << "\n";
			}
		}
		const std::string Text = Out.str();
		io_write(File, Text.data(), (unsigned)Text.size());
		io_close(File);
		str_copy(S.m_aStatus, "Черновик сохранён.");
	};
	const auto LoadDraft = [&]() {
		char aPath[128];
		DraftPath(aPath, sizeof(aPath));
		void *pData;
		unsigned Size;
		if(!Storage()->ReadFile(aPath, IStorage::TYPE_SAVE, &pData, &Size))
		{
			str_copy(S.m_aStatus, "Черновика с таким именем нет.");
			return;
		}
		std::istringstream In(std::string((const char *)pData, Size));
		free(pData);
		S.m_Glyphs.clear();
		S.m_vCustom.clear();
		std::string Word;
		int CurrentCp = 0, StrokesLeft = 0;
		while(In >> Word)
		{
			if(Word == "thick")
				In >> S.m_Thick;
			else if(Word == "custom")
			{
				int Cp;
				In >> Cp;
				S.m_vCustom.push_back(Cp);
			}
			else if(Word == "g")
				In >> CurrentCp >> StrokesLeft;
			else if(Word == "s")
			{
				int N;
				In >> N;
				std::vector<vec2> Stroke;
				for(int i = 0; i < N && In; i++)
				{
					int X, Y;
					In >> X >> Y;
					Stroke.emplace_back(X / 10000.0f, Y / 10000.0f);
				}
				S.m_Glyphs[CurrentCp].push_back(std::move(Stroke));
			}
		}
		S.m_Thick = std::clamp(S.m_Thick, 30, 160);
		str_copy(S.m_aStatus, "Черновик загружен.");
	};

	// language and group buttons
	CUIRect Row, Button;
	MainView.HSplitTop(18.0f, &Row, &MainView);
	static CButtonContainer s_aLangButtons[3];
	const float LangWidth = (Row.w - 2.0f * 3.0f) / 3.0f;
	for(int i = 0; i < 3; i++)
	{
		Row.VSplitLeft(LangWidth, &Button, &Row);
		Row.VSplitLeft(3.0f, nullptr, &Row);
		if(DoButton_Menu(&s_aLangButtons[i], LangName(i), S.m_Lang == i, &Button))
		{
			S.m_Lang = i;
			S.m_Sub = 0;
			const std::vector<int> vChars = GroupChars(S, S.m_Lang, S.m_Sub);
			if(!vChars.empty())
				S.m_Selected = vChars[0];
		}
	}
	MainView.HSplitTop(3.0f, nullptr, &MainView);
	MainView.HSplitTop(18.0f, &Row, &MainView);
	static CButtonContainer s_aGroupButtons[5];
	const float GroupWidth = (Row.w - 4.0f * 3.0f) / 5.0f;
	for(int i = 0; i < 5; i++)
	{
		Row.VSplitLeft(GroupWidth, &Button, &Row);
		Row.VSplitLeft(3.0f, nullptr, &Row);
		if(DoButton_Menu(&s_aGroupButtons[i], SubName(S.m_Lang, i), S.m_Sub == i, &Button))
		{
			S.m_Sub = i;
			const std::vector<int> vChars = GroupChars(S, S.m_Lang, S.m_Sub);
			if(!vChars.empty())
				S.m_Selected = vChars[0];
		}
	}
	MainView.HSplitTop(4.0f, nullptr, &MainView);

	if(S.m_Preview)
	{
		// preview: the same text with the normal font and with the letters that were drawn
		CUIRect Back;
		MainView.HSplitBottom(22.0f, &MainView, &Back);
		static CButtonContainer s_PreviewBack;
		if(DoButton_Menu(&s_PreviewBack, "Назад к рисованию", 0, &Back))
			S.m_Preview = false;
		static const char *s_apSamples[3] = {"Привет, мир! 0123", "Hello World 0123", "你好，世界 0123"};
		CUIRect Half, Mine;
		MainView.VSplitMid(&Half, &Mine, 6.0f);
		Half.Draw(ColorRGBA(0.08f, 0.08f, 0.1f, 0.9f), IGraphics::CORNER_ALL, 5.0f);
		Mine.Draw(ColorRGBA(0.08f, 0.08f, 0.1f, 0.9f), IGraphics::CORNER_ALL, 5.0f);
		CUIRect Title1, Title2;
		Half.HSplitTop(16.0f, &Title1, &Half);
		Mine.HSplitTop(16.0f, &Title2, &Mine);
		Ui()->DoLabel(&Title1, "Как сейчас в клиенте", 10.0f, TEXTALIGN_MC);
		Ui()->DoLabel(&Title2, "Ваш шрифт (то, что нарисовано)", 10.0f, TEXTALIGN_MC);
		// big current letter
		CUIRect Big1 = Half, Big2 = Mine;
		Big1.h = Big2.h = std::min(Half.h * 0.55f, 120.0f);
		const std::string Cur = Utf8Of(S.m_Selected);
		Ui()->DoLabel(&Big1, Cur.c_str(), Big1.h * 0.8f, TEXTALIGN_MC);
		const auto DrawGlyphAt = [&](int Cp, CUIRect Box, float Size, bool *pDrawn) {
			// Box is one character cell: height Size, width computed from the strokes
			const auto It = S.m_Glyphs.find(Cp);
			*pDrawn = It != S.m_Glyphs.end() && !It->second.empty();
			if(!*pDrawn)
				return 0.0f;
			float XMin = 1.0f, XMax = 0.0f;
			for(const auto &Stroke : It->second)
				for(const vec2 &P : Stroke)
				{
					XMin = std::min(XMin, P.x);
					XMax = std::max(XMax, P.x);
				}
			const float Scale = Size; // canvas height 1.0 -> Size
			const float PixelWidth = S.m_Thick / 1000.0f * Scale;
			const vec2 Origin(Box.x + 0.06f * Size - XMin * 0.8f * Scale, Box.y);
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.95f);
			for(const auto &Stroke : It->second)
				for(size_t i = 0; i < Stroke.size(); i++)
				{
					const vec2 A(Origin.x + Stroke[i].x * 0.8f * Scale, Origin.y + Stroke[i].y * Scale);
					Graphics()->DrawCircle(A.x, A.y, PixelWidth * 0.5f, 8);
					if(i + 1 < Stroke.size())
					{
						const vec2 B(Origin.x + Stroke[i + 1].x * 0.8f * Scale, Origin.y + Stroke[i + 1].y * Scale);
						const vec2 D = B - A;
						const float Len = length(D);
						if(Len > 0.01f)
						{
							const vec2 N = vec2(-D.y, D.x) / Len * PixelWidth * 0.5f;
							const IGraphics::CFreeformItem Item(A.x - N.x, A.y - N.y, A.x + N.x, A.y + N.y, B.x - N.x, B.y - N.y, B.x + N.x, B.y + N.y);
							Graphics()->QuadsDrawFreeform(&Item, 1);
						}
					}
				}
			Graphics()->QuadsEnd();
			return (XMax - XMin) * 0.8f * Scale + 0.2f * Size;
		};
		bool Drawn;
		DrawGlyphAt(S.m_Selected, Big2, Big2.h, &Drawn);
		if(!Drawn)
			Ui()->DoLabel(&Big2, "не нарисована", 10.0f, TEXTALIGN_MC);
		// sample line
		const std::vector<int> vSample = DecodeAll(s_apSamples[S.m_Lang]);
		CUIRect Line1 = Half, Line2 = Mine;
		Line1.y += Big1.h + 6.0f;
		Line2.y += Big1.h + 6.0f;
		Line1.h = Line2.h = 40.0f;
		const std::string Sample = s_apSamples[S.m_Lang];
		Ui()->DoLabel(&Line1, Sample.c_str(), 24.0f, TEXTALIGN_ML);
		float X = Line2.x + 4.0f;
		const float Size = 30.0f;
		for(const int Cp : vSample)
		{
			CUIRect Cell = Line2;
			Cell.x = X;
			Cell.y = Line2.y;
			float Advance = DrawGlyphAt(Cp, Cell, Size, &Drawn);
			if(!Drawn)
			{
				// not drawn yet: the normal font is used, like in the finished font
				const std::string One = Utf8Of(Cp);
				CUIRect Fallback = Cell;
				Fallback.w = 18.0f;
				Fallback.h = Size;
				TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.4f);
				Ui()->DoLabel(&Fallback, One.c_str(), 22.0f, TEXTALIGN_ML);
				TextRender()->TextColor(TextRender()->DefaultTextColor());
				Advance = Cp == ' ' ? 10.0f : 17.0f;
			}
			X += Advance + 2.0f;
			if(X > Line2.x + Line2.w - 10.0f)
				break;
		}
		return;
	}

	CUIRect Left, Middle, Right;
	MainView.VSplitLeft(MainView.w * 0.31f, &Left, &MainView);
	MainView.VSplitLeft(5.0f, nullptr, &MainView);
	const float CanvasH = MainView.h;
	const float CanvasW = std::min(CanvasH * 0.8f, MainView.w * 0.5f);
	MainView.VSplitLeft(CanvasW, &Middle, &Right);
	Right.VSplitLeft(6.0f, nullptr, &Right);

	// the grid with the letters
	{
		const std::vector<int> vChars = GroupChars(S, S.m_Lang, S.m_Sub);
		const float Cell = 25.0f;
		const int Columns = std::max(1, (int)(Left.w / (Cell + 2.0f)));
		static std::vector<CButtonContainer> s_vCharButtons(128);
		for(size_t i = 0; i < vChars.size() && i < s_vCharButtons.size(); i++)
		{
			CUIRect Cell0 = Left;
			Cell0.x += (i % Columns) * (Cell + 2.0f);
			Cell0.y += (i / Columns) * 22.0f;
			Cell0.w = Cell;
			Cell0.h = 20.0f;
			const std::string Label = Utf8Of(vChars[i]);
			const bool Drawn = S.m_Glyphs.count(vChars[i]) && !S.m_Glyphs[vChars[i]].empty();
			const std::string Shown = Drawn ? "[" + Label + "]" : Label;
			if(DoButton_Menu(&s_vCharButtons[i], Shown.c_str(), S.m_Selected == vChars[i], &Cell0))
				S.m_Selected = vChars[i];
		}
		if(S.m_Sub == 4 && vChars.empty())
			Ui()->DoLabel(&Left, "Впишите символы справа и нажмите «Добавить». Подойдут китайские иероглифы и любые другие.", 10.0f, TEXTALIGN_TL);
	}

	// the drawing area
	{
		Middle.Draw(ColorRGBA(0.08f, 0.08f, 0.1f, 0.9f), IGraphics::CORNER_ALL, 5.0f);
		const auto GuideLine = [&](float V, ColorRGBA Col) {
			CUIRect L = Middle;
			L.y += V * Middle.h;
			L.h = 1.0f;
			L.Draw(Col, IGraphics::CORNER_NONE, 0.0f);
		};
		GuideLine(0.2f, ColorRGBA(0.3f, 1.0f, 0.5f, 0.35f)); // cap height
		GuideLine(0.45f, ColorRGBA(1.0f, 1.0f, 1.0f, 0.15f)); // x height
		GuideLine(0.8f, ColorRGBA(0.4f, 0.6f, 1.0f, 0.6f)); // baseline
		CStrokes &Strokes = S.m_Glyphs[S.m_Selected];

		// the letter in the normal font, faint, to trace over
		{
			const std::string Ref = Utf8Of(S.m_Selected);
			CUIRect RefBox = Middle;
			RefBox.y = Middle.y + Middle.h * 0.2f;
			RefBox.h = Middle.h * 0.6f;
			TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.13f);
			Ui()->DoLabel(&RefBox, Ref.c_str(), Middle.h * 0.55f, TEXTALIGN_MC);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
		}

		// pointer input
		const bool Inside = Ui()->MouseInside(&Middle);
		const vec2 Mouse = Ui()->MousePos();
		const vec2 Norm(std::clamp((Mouse.x - Middle.x) / Middle.w, 0.0f, 1.0f), std::clamp((Mouse.y - Middle.y) / Middle.h, 0.0f, 1.0f));
		if(Ui()->MouseButton(0))
		{
			if(!S.m_Drawing && Inside)
			{
				S.m_Drawing = true;
				Strokes.emplace_back();
				Strokes.back().push_back(Norm);
			}
			else if(S.m_Drawing && !Strokes.empty())
			{
				const vec2 Last = Strokes.back().back();
				if(distance(vec2(Last.x * Middle.w, Last.y * Middle.h), vec2(Norm.x * Middle.w, Norm.y * Middle.h)) >= 3.0f)
					Strokes.back().push_back(Norm);
			}
		}
		else
			S.m_Drawing = false;

		// the strokes themselves, at the thickness the font will have
		const float PixelWidth = S.m_Thick / 1000.0f * Middle.h;
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.95f);
		for(const auto &Stroke : Strokes)
		{
			for(size_t i = 0; i < Stroke.size(); i++)
			{
				const vec2 A(Middle.x + Stroke[i].x * Middle.w, Middle.y + Stroke[i].y * Middle.h);
				Graphics()->DrawCircle(A.x, A.y, PixelWidth * 0.5f, 10);
				if(i + 1 < Stroke.size())
				{
					const vec2 B(Middle.x + Stroke[i + 1].x * Middle.w, Middle.y + Stroke[i + 1].y * Middle.h);
					const vec2 D = B - A;
					const float Len = length(D);
					if(Len > 0.01f)
					{
						const vec2 N = vec2(-D.y, D.x) / Len * PixelWidth * 0.5f;
						const IGraphics::CFreeformItem Item(A.x - N.x, A.y - N.y, A.x + N.x, A.y + N.y, B.x - N.x, B.y - N.y, B.x + N.x, B.y + N.y);
						Graphics()->QuadsDrawFreeform(&Item, 1);
					}
				}
			}
		}
		Graphics()->QuadsEnd();
		CUIRect Tag = Middle;
		Tag.VMargin(4.0f, &Tag);
		const std::string Title = "Рисуем: " + Utf8Of(S.m_Selected);
		Ui()->DoLabel(&Tag, Title.c_str(), 10.0f, TEXTALIGN_TL);
	}

	// controls
	{
		CUIRect Half;
		const auto NextRow = [&](float Height) {
			CUIRect R;
			Right.HSplitTop(Height, &R, &Right);
			Right.HSplitTop(4.0f, nullptr, &Right);
			return R;
		};
		static CButtonContainer s_Undo, s_Clear, s_Minus, s_Plus, s_SaveDraft, s_LoadDraft, s_Add, s_Done, s_Back;
		CStrokes &Strokes = S.m_Glyphs[S.m_Selected];

		Row = NextRow(20.0f);
		Row.VSplitMid(&Button, &Half, 4.0f);
		if(DoButton_Menu(&s_Undo, "Отменить штрих", 0, &Button) && !Strokes.empty())
			Strokes.pop_back();
		if(DoButton_Menu(&s_Clear, "Стереть букву", 0, &Half))
			Strokes.clear();

		Row = NextRow(20.0f);
		CUIRect Label;
		Row.VSplitLeft(Row.w * 0.5f, &Label, &Row);
		char aThick[48];
		str_format(aThick, sizeof(aThick), "Толщина %d", S.m_Thick);
		Ui()->DoLabel(&Label, aThick, 11.0f, TEXTALIGN_ML);
		Row.VSplitMid(&Button, &Half, 4.0f);
		if(DoButton_Menu(&s_Minus, "-", 0, &Button))
			S.m_Thick = std::max(30, S.m_Thick - 10);
		if(DoButton_Menu(&s_Plus, "+", 0, &Half))
			S.m_Thick = std::min(160, S.m_Thick + 10);

		Row = NextRow(20.0f);
		Ui()->DoEditBox(&S.m_Name, &Row, 12.0f);

		Row = NextRow(20.0f);
		Row.VSplitMid(&Button, &Half, 4.0f);
		if(DoButton_Menu(&s_SaveDraft, "Сохранить", 0, &Button))
			SaveDraft();
		if(DoButton_Menu(&s_LoadDraft, "Загрузить", 0, &Half))
			LoadDraft();

		Row = NextRow(20.0f);
		CUIRect AddRow;
		Row.VSplitRight(60.0f, &Row, &AddRow);
		Ui()->DoEditBox(&S.m_CustomInput, &Row, 12.0f);
		if(DoButton_Menu(&s_Add, "Добавить", 0, &AddRow))
		{
			const char *pStr = S.m_CustomInput.GetString();
			while(*pStr)
			{
				const int Cp = str_utf8_decode(&pStr);
				if(Cp > 0x20 && std::find(S.m_vCustom.begin(), S.m_vCustom.end(), Cp) == S.m_vCustom.end() && S.m_vCustom.size() < 96)
					S.m_vCustom.push_back(Cp);
				if(Cp > 0x20)
					S.m_Selected = Cp;
			}
			S.m_CustomInput.Clear();
			S.m_Sub = 4;
		}

		Row = NextRow(26.0f);
		if(DoButton_Menu(&s_Done, "ГОТОВО: создать шрифт", 0, &Row))
		{
			Storage()->CreateFolder("fonts", IStorage::TYPE_SAVE);
			std::vector<uint8_t> vFont;
			char aName[64];
			str_copy(aName, S.m_Name.GetString(), sizeof(aName));
			for(char *p = aName; *p; p++)
				if(!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_' || *p == '-'))
					*p = '_';
			if(aName[0] == '\0')
				str_copy(aName, "myfont");
			if(!BuildTtf(S.m_Glyphs, S.m_Thick, aName, vFont))
				str_copy(S.m_aStatus, "Нарисуйте хотя бы одну букву.");
			else
			{
				char aPath[128];
				str_format(aPath, sizeof(aPath), "fonts/%s.ttf", aName);
				IOHANDLE File = Storage()->OpenFile(aPath, IOFLAG_WRITE, IStorage::TYPE_SAVE);
				if(!File)
					str_copy(S.m_aStatus, "Не удалось записать файл шрифта.");
				else
				{
					io_write(File, vFont.data(), (unsigned)vFont.size());
					io_close(File);
					SaveDraft();
					str_copy(g_Config.m_ClCustomFont, aPath, sizeof(g_Config.m_ClCustomFont));
					if(TextRender()->SetCustomFont(aPath, true))
						str_format(S.m_aStatus, sizeof(S.m_aStatus), "Готово! Шрифт %s.ttf включён.", aName);
					else
						str_copy(S.m_aStatus, "Файл записан, но шрифт не открылся.");
				}
			}
		}

		Row = NextRow(32.0f);
		Ui()->DoLabel(&Row, S.m_aStatus[0] ? S.m_aStatus : "Нарисуйте буквы, потом нажмите «Готово». Без рисунка символ берётся из обычного шрифта.", 9.0f, TEXTALIGN_TL);

		Row = NextRow(20.0f);
		Row.VSplitMid(&Button, &Half, 4.0f);
		static CButtonContainer s_PreviewButton;
		if(DoButton_Menu(&s_PreviewButton, "Сравнить", 0, &Button))
			S.m_Preview = true;
		if(DoButton_Menu(&s_Back, "Назад", 0, &Half))
			m_ShowFontEditor = false;
	}
}
