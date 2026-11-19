#include "bindwheel.h"

#include <base/io.h>
#include <base/str.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <game/client/gameclient.h>
#include <game/client/ui.h>

#include <algorithm>
#include <cmath>
#include <string>

static const char *const BINDWHEEL_FILE = "bindwheel.tsv";

void CBindWheel::ConKeyBindWheel(IConsole::IResult *pResult, void *pUserData)
{
	CBindWheel *pSelf = (CBindWheel *)pUserData;
	if(pSelf->GameClient()->m_Scoreboard.IsActive())
		return;
	if(!g_Config.m_ClBindWheelEnabled)
	{
		pSelf->m_Active = false;
		return;
	}
	pSelf->m_Active = pResult->GetInteger(0) != 0;
}

void CBindWheel::OnConsoleInit()
{
	Console()->Register("+bindwheel", "", CFGFLAG_CLIENT, ConKeyBindWheel, this, "Open the bind wheel");
}

void CBindWheel::OnInit()
{
	Load();
}

void CBindWheel::OnReset()
{
	m_Active = false;
	m_WasActive = false;
	m_Selected = -1;
}

void CBindWheel::OnRelease()
{
	OnReset();
}

bool CBindWheel::OnCursorMove(float x, float y, IInput::ECursorType CursorType)
{
	if(!m_Active)
		return false;
	Ui()->ConvertMouseMove(&x, &y, CursorType);
	m_Mouse += vec2(x, y);
	return true;
}

bool CBindWheel::OnInput(const IInput::CEvent &Event)
{
	if(m_Active && (Event.m_Flags & IInput::FLAG_PRESS) && Event.m_Key == KEY_ESCAPE)
	{
		OnRelease();
		return true;
	}
	return false;
}

void CBindWheel::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	if(!m_Active)
	{
		// the key was released: run the selected bind
		if(m_WasActive && m_Selected >= 0 && m_Selected < (int)m_vEntries.size())
			Console()->ExecuteLine(m_vEntries[m_Selected].m_aCommand, IConsole::CLIENT_ID_UNSPECIFIED);
		m_WasActive = false;
		m_Selected = -1;
		return;
	}

	if(GameClient()->m_Scoreboard.IsActive())
	{
		OnRelease();
		return;
	}
	if(!m_WasActive)
		m_Mouse = vec2(0.0f, 0.0f);
	m_WasActive = true;

	constexpr float OUTER_RADIUS = 230.0f;
	constexpr float LABEL_RADIUS = 160.0f;
	if(length(m_Mouse) > 200.0f)
		m_Mouse = normalize(m_Mouse) * 200.0f;

	const int Num = (int)m_vEntries.size();
	m_Selected = -1;
	if(Num > 0 && length(m_Mouse) > 40.0f)
		m_Selected = std::clamp((int)(std::fmod(angle(m_Mouse) + 2 * pi + pi / Num, 2 * pi) / (2 * pi) * Num), 0, Num - 1);

	const CUIRect Screen = *Ui()->Screen();
	const vec2 Center = Screen.Center();
	Ui()->MapScreen();

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.35f);
	Graphics()->DrawCircle(Center.x, Center.y, OUTER_RADIUS, 64);
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.12f);
	Graphics()->DrawCircle(Center.x, Center.y, 30.0f, 32);
	Graphics()->QuadsEnd();

	if(Num == 0)
	{
		CUIRect Label(Center.x - 150.0f, Center.y - 20.0f, 300.0f, 40.0f);
		Ui()->DoLabel(&Label, "Нет биндов. Добавьте их в настройках", 14.0f, TEXTALIGN_MC);
	}

	for(int i = 0; i < Num; i++)
	{
		const float Angle = 2 * pi * i / (float)Num;
		const vec2 Pos = Center + direction(Angle) * LABEL_RADIUS;
		const bool Selected = i == m_Selected;
		const float FontSize = Selected ? 17.0f : 14.0f;
		const float Width = std::max(70.0f, TextRender()->TextWidth(FontSize, m_vEntries[i].m_aName) + 24.0f);
		CUIRect Rect(Pos.x - Width / 2.0f, Pos.y - 16.0f, Width, 32.0f);
		Rect.Draw(Selected ? ColorRGBA(0.25f, 0.55f, 1.0f, 0.9f) : ColorRGBA(0.0f, 0.0f, 0.0f, 0.6f), IGraphics::CORNER_ALL, 8.0f);
		Ui()->DoLabel(&Rect, m_vEntries[i].m_aName, FontSize, TEXTALIGN_MC);
	}

	RenderTools()->RenderCursor(Center + m_Mouse, 24.0f);
}

bool CBindWheel::AddEntry(const char *pName, const char *pCommand)
{
	if(m_vEntries.size() >= MAX_ENTRIES || pName[0] == '\0' || pCommand[0] == '\0')
		return false;
	CEntry Entry;
	str_copy(Entry.m_aName, pName);
	str_copy(Entry.m_aCommand, pCommand);
	// tabs and new lines would break the file
	for(char *p = Entry.m_aName; *p; p++)
		if(*p == '\t' || *p == '\n' || *p == '\r')
			*p = ' ';
	for(char *p = Entry.m_aCommand; *p; p++)
		if(*p == '\t' || *p == '\n' || *p == '\r')
			*p = ' ';
	m_vEntries.push_back(Entry);
	Save();
	return true;
}

void CBindWheel::RemoveEntry(int Index)
{
	if(Index < 0 || Index >= (int)m_vEntries.size())
		return;
	m_vEntries.erase(m_vEntries.begin() + Index);
	Save();
}

void CBindWheel::Save() const
{
	std::string Content;
	for(const CEntry &Entry : m_vEntries)
		Content += std::string(Entry.m_aName) + "\t" + Entry.m_aCommand + "\n";
	IOHANDLE File = Storage()->OpenFile(BINDWHEEL_FILE, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
		return;
	io_write(File, Content.data(), Content.size());
	io_close(File);
}

void CBindWheel::Load()
{
	m_vEntries.clear();
	IOHANDLE File = Storage()->OpenFile(BINDWHEEL_FILE, IOFLAG_READ, IStorage::TYPE_SAVE);
	if(!File)
		return;
	char *pContent = io_read_all_str(File);
	io_close(File);
	if(pContent == nullptr)
		return;
	const std::string Content = pContent;
	free(pContent);

	size_t LineStart = 0;
	while(LineStart < Content.size() && m_vEntries.size() < MAX_ENTRIES)
	{
		size_t LineEnd = Content.find('\n', LineStart);
		if(LineEnd == std::string::npos)
			LineEnd = Content.size();
		const std::string Line = Content.substr(LineStart, LineEnd - LineStart);
		LineStart = LineEnd + 1;
		const size_t Tab = Line.find('\t');
		if(Tab == std::string::npos)
			continue;
		CEntry Entry;
		str_copy(Entry.m_aName, Line.substr(0, Tab).c_str());
		str_copy(Entry.m_aCommand, Line.substr(Tab + 1).c_str());
		if(Entry.m_aName[0] && Entry.m_aCommand[0])
			m_vEntries.push_back(Entry);
	}
}

void CBindWheel::SetOpenKey(int Key)
{
	if(g_Config.m_ClBindWheelKey > 0 && g_Config.m_ClBindWheelKey < KEY_LAST)
		GameClient()->m_Binds.Bind(g_Config.m_ClBindWheelKey, "");
	g_Config.m_ClBindWheelKey = std::clamp(Key, 0, (int)KEY_LAST - 1);
	if(g_Config.m_ClBindWheelKey > 0)
		GameClient()->m_Binds.Bind(g_Config.m_ClBindWheelKey, "+bindwheel");
}
