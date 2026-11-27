#include "friend_chat.h"

#include <base/io.h>
#include <base/secure.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/client.h>
#include <engine/external/json-parser/json.h>
#include <engine/graphics.h>
#include <engine/input.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <generated/protocol.h>

#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>

static const char *const FRIENDS_FILE = "galaxy_friends.tsv";
static const char *const PROTO = "GX1";
static const size_t MAX_FRIENDS = 24;
static const size_t MAX_SAVED_MESSAGES = 200;

static std::string Clean(const char *pText)
{
	std::string Out = pText;
	for(char &Ch : Out)
		if(Ch == '\t' || Ch == '\n' || Ch == '\r')
			Ch = ' ';
	return Out;
}

static std::vector<std::string> SplitTabs(const std::string &Line, size_t MaxParts)
{
	std::vector<std::string> vParts;
	size_t Start = 0;
	while(vParts.size() + 1 < MaxParts)
	{
		const size_t Tab = Line.find('\t', Start);
		if(Tab == std::string::npos)
			break;
		vParts.push_back(Line.substr(Start, Tab - Start));
		Start = Tab + 1;
	}
	vParts.push_back(Line.substr(Start));
	return vParts;
}

void CFriendChat::OnInit()
{
	EnsureUid();
	Load();
}

void CFriendChat::OnShutdown()
{
	Save();
}

const char *CFriendChat::Uid() const
{
	return g_Config.m_ClFriendUid;
}

void CFriendChat::EnsureUid()
{
	if(g_Config.m_ClFriendUid[0] != '\0')
		return;
	static const char *const ALPHABET = "abcdefghjkmnpqrstuvwxyz23456789";
	const int AlphabetLength = (int)str_length(ALPHABET);
	unsigned char aRandom[12];
	secure_random_fill(aRandom, sizeof(aRandom));
	char aUid[13];
	for(int i = 0; i < 12; i++)
		aUid[i] = ALPHABET[aRandom[i] % AlphabetLength];
	aUid[12] = '\0';
	str_copy(g_Config.m_ClFriendUid, aUid, sizeof(g_Config.m_ClFriendUid));
}

std::string CFriendChat::Server() const
{
	std::string Url = g_Config.m_ClFriendChatServer;
	while(!Url.empty() && Url.back() == '/')
		Url.pop_back();
	if(Url.empty())
		Url = "https://ntfy.sh";
	return Url;
}

CFriendChat::SFriend *CFriendChat::Find(const char *pUid)
{
	for(SFriend &Friend : m_vFriends)
		if(Friend.m_Uid == pUid)
			return &Friend;
	return nullptr;
}

int CFriendChat::TotalUnread() const
{
	int Sum = 0;
	for(const SFriend &Friend : m_vFriends)
		Sum += Friend.m_Unread;
	return Sum;
}

int CFriendChat::Pending() const
{
	int Sum = 0;
	for(const SFriend &Friend : m_vFriends)
		if(Friend.m_State == STATE_INCOMING)
			Sum++;
	return Sum;
}

// ---------- сохранение

void CFriendChat::Load()
{
	m_vFriends.clear();
	IOHANDLE File = Storage()->OpenFile(FRIENDS_FILE, IOFLAG_READ, IStorage::TYPE_SAVE);
	if(!File)
		return;
	char *pContent = io_read_all_str(File);
	io_close(File);
	if(pContent == nullptr)
		return;
	const std::string Content = pContent;
	free(pContent);

	size_t LineStart = 0;
	while(LineStart < Content.size())
	{
		size_t LineEnd = Content.find('\n', LineStart);
		if(LineEnd == std::string::npos)
			LineEnd = Content.size();
		const std::string Line = Content.substr(LineStart, LineEnd - LineStart);
		LineStart = LineEnd + 1;
		if(Line.size() < 3)
			continue;
		if(Line[0] == 'F')
		{
			const std::vector<std::string> vParts = SplitTabs(Line, 6);
			if(vParts.size() < 6 || m_vFriends.size() >= MAX_FRIENDS)
				continue;
			SFriend Friend;
			Friend.m_Uid = vParts[1];
			Friend.m_Name = vParts[2];
			Friend.m_State = std::clamp(atoi(vParts[3].c_str()), 0, 2);
			Friend.m_Secret = vParts[4];
			Friend.m_LastId = vParts[5];
			m_vFriends.push_back(Friend);
		}
		else if(Line[0] == 'M')
		{
			const std::vector<std::string> vParts = SplitTabs(Line, 5);
			if(vParts.size() < 5)
				continue;
			SFriend *pFriend = Find(vParts[1].c_str());
			if(pFriend == nullptr)
				continue;
			SMessage Msg;
			Msg.m_Time = atoll(vParts[2].c_str());
			Msg.m_Mine = vParts[3] == "1";
			Msg.m_Text = vParts[4];
			pFriend->m_vMessages.push_back(Msg);
		}
	}
}

void CFriendChat::Save()
{
	m_Dirty = false;
	std::string Content;
	for(const SFriend &Friend : m_vFriends)
	{
		Content += "F\t" + Friend.m_Uid + "\t" + Clean(Friend.m_Name.c_str()) + "\t" + std::to_string(Friend.m_State) + "\t" + Friend.m_Secret + "\t" + Friend.m_LastId + "\n";
		const size_t First = Friend.m_vMessages.size() > MAX_SAVED_MESSAGES ? Friend.m_vMessages.size() - MAX_SAVED_MESSAGES : 0;
		for(size_t i = First; i < Friend.m_vMessages.size(); i++)
		{
			const SMessage &Msg = Friend.m_vMessages[i];
			Content += "M\t" + Friend.m_Uid + "\t" + std::to_string(Msg.m_Time) + "\t" + (Msg.m_Mine ? "1" : "0") + "\t" + Clean(Msg.m_Text.c_str()) + "\n";
		}
	}
	IOHANDLE File = Storage()->OpenFile(FRIENDS_FILE, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
		return;
	io_write(File, Content.data(), Content.size());
	io_close(File);
}

// ---------- сеть

void CFriendChat::Publish(const std::string &Topic, const std::string &Body)
{
	const std::string Url = Server() + "/" + Topic;
	std::shared_ptr<IHttpRequest> pReq = HttpPost(Url.c_str(), (const unsigned char *)Body.data(), Body.size());
	pReq->WriteToMemory();
	pReq->LogProgress(HTTPLOG::NONE);
	Http()->Run(pReq);
	m_vSends.push_back(pReq);
}

void CFriendChat::SendPacket(const char *pToUid, const char *pType, const char *pPayload)
{
	const std::string Body = std::string(PROTO) + "\t" + pType + "\t" + Uid() + "\t" + Clean(g_Config.m_PlayerName) + "\t" + Clean(pPayload);
	Publish(std::string("gxc-in-") + pToUid, Body);
}

// Разбирает ответ ntfy (по JSON-объекту на строку) в пары (id, сообщение) и время
bool CFriendChat::ForEachLine(const std::shared_ptr<IHttpRequest> &pReq, std::vector<std::pair<std::string, std::string>> &vOut)
{
	if(pReq->State() != EHttpState::DONE)
		return false;
	unsigned char *pData = nullptr;
	size_t Length = 0;
	pReq->Result(&pData, &Length);
	if(pData == nullptr || Length == 0)
		return true;
	const std::string Text((const char *)pData, Length);
	size_t LineStart = 0;
	while(LineStart < Text.size())
	{
		size_t LineEnd = Text.find('\n', LineStart);
		if(LineEnd == std::string::npos)
			LineEnd = Text.size();
		const std::string Line = Text.substr(LineStart, LineEnd - LineStart);
		LineStart = LineEnd + 1;
		if(Line.size() < 2)
			continue;
		json_value *pValue = json_parse(Line.c_str(), Line.size());
		if(pValue == nullptr)
			continue;
		if(pValue->type == json_object)
		{
			const json_value &Event = (*pValue)["event"];
			const json_value &Id = (*pValue)["id"];
			const json_value &Message = (*pValue)["message"];
			const json_value &Time = (*pValue)["time"];
			if(Event.type == json_string && str_comp(Event.u.string.ptr, "message") == 0 && Id.type == json_string && Message.type == json_string)
			{
				std::string Entry = Message.u.string.ptr;
				const long long T = Time.type == json_integer ? (long long)Time.u.integer : 0;
				vOut.emplace_back(std::string(Id.u.string.ptr) + "\t" + std::to_string(T), Entry);
			}
		}
		json_value_free(pValue);
	}
	return true;
}

void CFriendChat::PollInbox()
{
	const std::string Url = Server() + "/gxc-in-" + Uid() + "/json?poll=1&since=" + (m_InboxLastId.empty() ? "12h" : m_InboxLastId);
	m_pInboxPoll = HttpGet(Url.c_str());
	m_pInboxPoll->WriteToMemory();
	m_pInboxPoll->Timeout(CTimeout{4000, 10000, 0, 0});
	m_pInboxPoll->MaxResponseSize(512 * 1024);
	m_pInboxPoll->LogProgress(HTTPLOG::NONE);
	Http()->Run(m_pInboxPoll);
}

void CFriendChat::PollChat(SFriend &Friend, bool Fast)
{
	(void)Fast;
	const std::string Url = Server() + "/gxc-ch-" + Friend.m_Secret + "/json?poll=1&since=" + (Friend.m_LastId.empty() ? "12h" : Friend.m_LastId);
	Friend.m_pPoll = HttpGet(Url.c_str());
	Friend.m_pPoll->WriteToMemory();
	Friend.m_pPoll->Timeout(CTimeout{4000, 10000, 0, 0});
	Friend.m_pPoll->MaxResponseSize(512 * 1024);
	Friend.m_pPoll->LogProgress(HTTPLOG::NONE);
	Http()->Run(Friend.m_pPoll);
}

void CFriendChat::AddMessage(SFriend &Friend, const char *pText, bool Mine, long long Time)
{
	SMessage Msg;
	Msg.m_Time = Time > 0 ? Time : (long long)::time(nullptr);
	Msg.m_Mine = Mine;
	Msg.m_Text = pText;
	Friend.m_vMessages.push_back(Msg);
	if(Friend.m_vMessages.size() > 400)
		Friend.m_vMessages.erase(Friend.m_vMessages.begin());
	m_Dirty = true;
}

void CFriendChat::HandleInbox(const char *pBody)
{
	const std::vector<std::string> vParts = SplitTabs(pBody, 5);
	if(vParts.size() < 5 || vParts[0] != PROTO)
		return;
	const std::string &Type = vParts[1], &From = vParts[2], &Name = vParts[3], &Payload = vParts[4];
	if(From.empty() || From == Uid() || From.size() > 24)
		return;

	SFriend *pFriend = Find(From.c_str());
	if(Type == "REQ")
	{
		if(Payload.empty() || Payload.size() > 40)
			return;
		if(pFriend == nullptr)
		{
			if(m_vFriends.size() >= MAX_FRIENDS)
				return;
			SFriend New;
			New.m_Uid = From;
			New.m_Name = Name;
			New.m_Secret = Payload;
			New.m_State = STATE_INCOMING;
			m_vFriends.push_back(New);
			str_format(m_aStatus, sizeof(m_aStatus), "Новый запрос в друзья: %s", Name.c_str());
		}
		else if(pFriend->m_State == STATE_OUTGOING)
		{
			// запросы встретились: сразу дружим, общий секрет — меньший из двух
			pFriend->m_Secret = std::min(pFriend->m_Secret, Payload);
			pFriend->m_State = STATE_FRIEND;
			pFriend->m_Name = Name;
			SendPacket(From.c_str(), "ACC", pFriend->m_Secret.c_str());
			str_format(m_aStatus, sizeof(m_aStatus), "%s теперь в друзьях", Name.c_str());
		}
		else
		{
			pFriend->m_Name = Name;
			pFriend->m_Secret = Payload;
			pFriend->m_State = STATE_INCOMING;
		}
		m_Dirty = true;
	}
	else if(Type == "ACC")
	{
		if(pFriend != nullptr && (pFriend->m_State == STATE_OUTGOING || pFriend->m_State == STATE_FRIEND))
		{
			if(!Payload.empty() && Payload.size() <= 40)
				pFriend->m_Secret = Payload;
			pFriend->m_State = STATE_FRIEND;
			pFriend->m_Name = Name;
			str_format(m_aStatus, sizeof(m_aStatus), "%s принял(а) запрос", Name.c_str());
			m_Dirty = true;
		}
	}
	else if(Type == "DEC")
	{
		if(pFriend != nullptr && pFriend->m_State == STATE_OUTGOING)
		{
			str_format(m_aStatus, sizeof(m_aStatus), "%s отклонил(а) запрос", Name.c_str());
			for(size_t i = 0; i < m_vFriends.size(); i++)
			{
				if(&m_vFriends[i] == pFriend)
				{
					m_vFriends.erase(m_vFriends.begin() + i);
					if(m_Selected == (int)i)
						m_Selected = -1;
					else if(m_Selected > (int)i)
						m_Selected--;
					break;
				}
			}
			m_Dirty = true;
		}
	}
}

void CFriendChat::Tick()
{
	if(g_Config.m_ClFriendUid[0] == '\0')
		EnsureUid();
	const int64_t Now = time_get();
	const int64_t Second = time_freq();

	// завершённые отправки
	m_vSends.erase(std::remove_if(m_vSends.begin(), m_vSends.end(), [](const std::shared_ptr<IHttpRequest> &pReq) { return pReq->Done(); }), m_vSends.end());

	// почтовый ящик
	if(m_pInboxPoll != nullptr && m_pInboxPoll->Done())
	{
		std::vector<std::pair<std::string, std::string>> vLines;
		if(ForEachLine(m_pInboxPoll, vLines))
		{
			for(const auto &Line : vLines)
			{
				const size_t Tab = Line.first.find('\t');
				m_InboxLastId = Line.first.substr(0, Tab);
				HandleInbox(Line.second.c_str());
			}
		}
		m_pInboxPoll = nullptr;
	}
	if(m_pInboxPoll == nullptr && Now >= m_InboxNextPoll)
	{
		PollInbox();
		m_InboxNextPoll = Now + (m_Open ? 3 : 20) * Second;
	}

	// чаты подтверждённых друзей
	for(size_t i = 0; i < m_vFriends.size(); i++)
	{
		SFriend &Friend = m_vFriends[i];
		if(Friend.m_pPoll != nullptr && Friend.m_pPoll->Done())
		{
			std::vector<std::pair<std::string, std::string>> vLines;
			if(ForEachLine(Friend.m_pPoll, vLines))
			{
				for(const auto &Line : vLines)
				{
					const size_t Tab = Line.first.find('\t');
					Friend.m_LastId = Line.first.substr(0, Tab);
					const long long Time = atoll(Line.first.substr(Tab + 1).c_str());
					const std::vector<std::string> vParts = SplitTabs(Line.second, 5);
					if(vParts.size() < 5 || vParts[0] != PROTO || vParts[1] != "MSG" || vParts[2] == Uid())
						continue;
					AddMessage(Friend, vParts[4].c_str(), false, Time);
					if(!(m_Open && m_Selected == (int)i))
						Friend.m_Unread++;
				}
				m_Dirty = true;
			}
			Friend.m_pPoll = nullptr;
		}
		if(Friend.m_State == STATE_FRIEND && Friend.m_pPoll == nullptr && Now >= Friend.m_NextPoll && !Friend.m_Secret.empty())
		{
			const bool Selected = m_Open && m_Selected == (int)i;
			PollChat(Friend, Selected);
			Friend.m_NextPoll = Now + (Selected ? 2 : (m_Open ? 8 : 30)) * Second;
		}
	}

	if(m_Dirty && Now >= m_NextSave)
	{
		Save();
		m_NextSave = Now + 4 * Second;
	}
}

void CFriendChat::OnRender()
{
	Tick();
}

// ---------- действия

void CFriendChat::SendRequest(const char *pCode)
{
	std::string Code = pCode;
	Code.erase(std::remove_if(Code.begin(), Code.end(), [](char Ch) { return Ch == ' ' || Ch == '\t'; }), Code.end());
	if(Code.empty())
		return;
	if(Code == Uid())
	{
		str_copy(m_aStatus, "Это твой собственный код");
		return;
	}
	if(Code.size() < 6 || Code.size() > 24)
	{
		str_copy(m_aStatus, "Код выглядит неправильно");
		return;
	}
	SFriend *pFriend = Find(Code.c_str());
	if(pFriend != nullptr && pFriend->m_State == STATE_FRIEND)
	{
		str_copy(m_aStatus, "Вы уже друзья");
		return;
	}
	if(pFriend != nullptr && pFriend->m_State == STATE_INCOMING)
	{
		Accept(*pFriend);
		return;
	}
	if(m_vFriends.size() >= MAX_FRIENDS)
	{
		str_copy(m_aStatus, "Список друзей заполнен");
		return;
	}
	unsigned char aRandom[16];
	secure_random_fill(aRandom, sizeof(aRandom));
	static const char *const ALPHABET = "abcdefghjkmnpqrstuvwxyz23456789";
	char aSecret[17];
	for(int i = 0; i < 16; i++)
		aSecret[i] = ALPHABET[aRandom[i] % 31];
	aSecret[16] = '\0';
	if(pFriend == nullptr)
	{
		SFriend New;
		New.m_Uid = Code;
		New.m_Name = Code;
		m_vFriends.push_back(New);
		pFriend = &m_vFriends.back();
	}
	pFriend->m_State = STATE_OUTGOING;
	pFriend->m_Secret = aSecret;
	SendPacket(Code.c_str(), "REQ", aSecret);
	str_copy(m_aStatus, "Запрос отправлен. Чат появится, когда друг его примет");
	m_Dirty = true;
}

void CFriendChat::Accept(SFriend &Friend)
{
	Friend.m_State = STATE_FRIEND;
	SendPacket(Friend.m_Uid.c_str(), "ACC", Friend.m_Secret.c_str());
	str_format(m_aStatus, sizeof(m_aStatus), "%s теперь в друзьях", Friend.m_Name.c_str());
	m_Dirty = true;
}

void CFriendChat::Remove(int Index)
{
	if(Index < 0 || Index >= (int)m_vFriends.size())
		return;
	if(m_vFriends[Index].m_State == STATE_INCOMING)
		SendPacket(m_vFriends[Index].m_Uid.c_str(), "DEC", "");
	m_vFriends.erase(m_vFriends.begin() + Index);
	if(m_Selected == Index)
		m_Selected = -1;
	else if(m_Selected > Index)
		m_Selected--;
	m_Dirty = true;
}

void CFriendChat::SendText(SFriend &Friend, const char *pText)
{
	if(pText[0] == '\0' || Friend.m_State != STATE_FRIEND || Friend.m_Secret.empty())
		return;
	const std::string Body = std::string(PROTO) + "\tMSG\t" + Uid() + "\t" + Clean(g_Config.m_PlayerName) + "\t" + Clean(pText);
	Publish(std::string("gxc-ch-") + Friend.m_Secret, Body);
	AddMessage(Friend, Clean(pText).c_str(), true, 0);
	Friend.m_NextPoll = 0;
}

// ---------- интерфейс

static bool IsEmoteMessage(const std::string &Text, int *pEmote)
{
	if(Text.size() >= 5 && Text.compare(0, 3, "[e:") == 0 && Text.back() == ']')
	{
		const int Value = atoi(Text.c_str() + 3);
		if(Value >= 0 && Value < (int)NUM_EMOTICONS)
		{
			*pEmote = Value;
			return true;
		}
	}
	return false;
}

void CFriendChat::RenderBubble(const CUIRect &Row, const SMessage &Msg, float *pHeightOut, bool Draw, float Width)
{
	const float FontSize = 11.0f;
	const float Pad = 6.0f;
	int Emote = 0;
	const bool IsEmote = IsEmoteMessage(Msg.m_Text, &Emote);
	float BubbleW, BubbleH;
	if(IsEmote)
	{
		BubbleW = 46.0f;
		BubbleH = 46.0f;
	}
	else
	{
		const float MaxText = Width * 0.72f - Pad * 2.0f;
		const STextBoundingBox Box = TextRender()->TextBoundingBox(FontSize, Msg.m_Text.c_str(), -1, MaxText);
		BubbleW = std::min(MaxText, Box.m_W) + Pad * 2.0f;
		BubbleH = Box.m_H + Pad * 2.0f;
	}
	char aTime[16];
	{
		const time_t T = (time_t)Msg.m_Time;
		tm *pLocal = ::localtime(&T);
		if(pLocal)
			str_format(aTime, sizeof(aTime), "%02d:%02d", pLocal->tm_hour, pLocal->tm_min);
		else
			aTime[0] = '\0';
	}
	const float TimeH = 11.0f;
	*pHeightOut = BubbleH + TimeH + 4.0f;
	if(!Draw)
		return;

	CUIRect Bubble;
	Bubble.w = std::max(BubbleW, 36.0f);
	Bubble.h = BubbleH;
	Bubble.y = Row.y;
	Bubble.x = Msg.m_Mine ? Row.x + Row.w - Bubble.w : Row.x;
	if(IsEmote)
	{
		Graphics()->TextureSet(GameClient()->m_EmoticonsSkin.m_aSpriteEmoticons[Emote]);
		Graphics()->QuadsSetSubset(0, 0, 1, 1);
		Graphics()->QuadsBegin();
		Graphics()->SetColor(1, 1, 1, 1);
		const IGraphics::CQuadItem Item(Bubble.x, Bubble.y, 44.0f, 44.0f);
		Graphics()->QuadsDrawTL(&Item, 1);
		Graphics()->QuadsEnd();
	}
	else
	{
		Bubble.Draw(Msg.m_Mine ? ColorRGBA(0.45f, 0.22f, 0.85f, 0.85f) : ColorRGBA(0.12f, 0.08f, 0.22f, 0.9f), IGraphics::CORNER_ALL, 8.0f);
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
		TextRender()->Text(Bubble.x + Pad, Bubble.y + Pad, FontSize, Msg.m_Text.c_str(), Bubble.w - Pad * 2.0f + 0.5f);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}
	CUIRect TimeRect(Msg.m_Mine ? Bubble.x : Bubble.x, Bubble.y + BubbleH + 1.0f, Bubble.w, TimeH);
	TextRender()->TextColor(0.7f, 0.65f, 0.85f, 0.8f);
	Ui()->DoLabel(&TimeRect, aTime, 8.0f, Msg.m_Mine ? TEXTALIGN_MR : TEXTALIGN_ML);
	TextRender()->TextColor(TextRender()->DefaultTextColor());
}

void CFriendChat::RenderPanel(CUIRect Rect)
{
	if(!m_Open)
		return;

	// фон: тёмная фиолетовая панель с тонкой неоновой рамкой
	{
		CUIRect Outer = Rect;
		Outer.Margin(-1.5f, &Outer);
		Outer.Draw(ColorRGBA(0.62f, 0.38f, 1.0f, 0.55f), IGraphics::CORNER_ALL, 12.0f);
		Rect.Draw4(ColorRGBA(0.16f, 0.06f, 0.30f, 0.96f), ColorRGBA(0.06f, 0.02f, 0.12f, 0.96f), ColorRGBA(0.10f, 0.03f, 0.20f, 0.96f), ColorRGBA(0.03f, 0.01f, 0.07f, 0.96f), IGraphics::CORNER_ALL, 10.0f);
	}
	CUIRect Inner;
	Rect.Margin(10.0f, &Inner);

	CUIRect Header;
	Inner.HSplitTop(24.0f, &Header, &Inner);
	Inner.HSplitTop(6.0f, nullptr, &Inner);
	static CButtonContainer s_CloseButton, s_BackButton, s_RemoveButton;
	CUIRect CloseRect;
	Header.VSplitRight(24.0f, &Header, &CloseRect);
	if(GameClient()->m_Menus.DoButton_Menu(&s_CloseButton, "x", 0, &CloseRect, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.45f, 0.2f, 0.8f, 0.5f)))
		m_Open = false;

	const ColorRGBA ButtonColor(0.45f, 0.22f, 0.85f, 0.55f);
	const bool InChat = m_Selected >= 0 && m_Selected < (int)m_vFriends.size() && m_vFriends[m_Selected].m_State == STATE_FRIEND;
	if(!InChat)
		m_Selected = -1;

	if(!InChat)
	{
		TextRender()->TextColor(0.85f, 0.75f, 1.0f, 1.0f);
		Ui()->DoLabel(&Header, "Чат друзей", 15.0f, TEXTALIGN_ML);
		TextRender()->TextColor(TextRender()->DefaultTextColor());

		// мой код
		CUIRect Row, Label, Button;
		Inner.HSplitTop(18.0f, &Row, &Inner);
		Row.VSplitRight(80.0f, &Label, &Button);
		char aBuf[96];
		str_format(aBuf, sizeof(aBuf), "Твой код: %s", Uid());
		Ui()->DoLabel(&Label, aBuf, 11.0f, TEXTALIGN_ML);
		static CButtonContainer s_CopyButton;
		if(GameClient()->m_Menus.DoButton_Menu(&s_CopyButton, "Копировать", 0, &Button, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ButtonColor))
		{
			Input()->SetClipboardText(Uid());
			str_copy(m_aStatus, "Код скопирован. Отправь его другу");
		}
		Inner.HSplitTop(6.0f, nullptr, &Inner);

		// добавить друга по коду
		Inner.HSplitTop(22.0f, &Row, &Inner);
		Row.VSplitRight(80.0f, &Label, &Button);
		Label.VSplitRight(4.0f, &Label, nullptr);
		Ui()->DoEditBox(&m_CodeInput, &Label, 11.0f);
		static CButtonContainer s_AddButton;
		if(GameClient()->m_Menus.DoButton_Menu(&s_AddButton, "Добавить", 0, &Button, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ButtonColor) || (m_CodeInput.IsActive() && Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER)))
		{
			SendRequest(m_CodeInput.GetString());
			m_CodeInput.Clear();
		}
		if(m_CodeInput.IsEmpty() && !m_CodeInput.IsActive())
		{
			CUIRect Hint = Label;
			Hint.VMargin(6.0f, &Hint);
			TextRender()->TextColor(0.8f, 0.8f, 0.85f, 0.45f);
			Ui()->DoLabel(&Hint, "Код друга", 10.0f, TEXTALIGN_ML);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
		}
		Inner.HSplitTop(4.0f, nullptr, &Inner);
		if(m_aStatus[0] != '\0')
		{
			CUIRect Status;
			Inner.HSplitTop(26.0f, &Status, &Inner);
			TextRender()->TextColor(0.78f, 0.68f, 1.0f, 0.95f);
			SLabelProperties Props;
			Props.m_MaxWidth = Status.w;
			Ui()->DoLabel(&Status, m_aStatus, 9.5f, TEXTALIGN_ML, Props);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
		}
		Inner.HSplitTop(4.0f, nullptr, &Inner);

		static CButtonContainer s_aOpen[MAX_FRIENDS], s_aYes[MAX_FRIENDS], s_aNo[MAX_FRIENDS];
		const char *apSections[3] = {"Запросы в друзья", "Друзья", "Ждут принятия"};
		const int aStates[3] = {STATE_INCOMING, STATE_FRIEND, STATE_OUTGOING};
		bool Any = false;
		for(int Section = 0; Section < 3; Section++)
		{
			bool HeaderDone = false;
			for(int i = 0; i < (int)m_vFriends.size() && i < (int)MAX_FRIENDS; i++)
			{
				SFriend &Friend = m_vFriends[i];
				if(Friend.m_State != aStates[Section])
					continue;
				if(Inner.h < 24.0f)
					break;
				Any = true;
				if(!HeaderDone)
				{
					HeaderDone = true;
					CUIRect Title;
					Inner.HSplitTop(16.0f, &Title, &Inner);
					TextRender()->TextColor(0.65f, 0.5f, 0.95f, 1.0f);
					Ui()->DoLabel(&Title, apSections[Section], 10.0f, TEXTALIGN_ML);
					TextRender()->TextColor(TextRender()->DefaultTextColor());
				}
				CUIRect Item;
				Inner.HSplitTop(24.0f, &Item, &Inner);
				Inner.HSplitTop(3.0f, nullptr, &Inner);
				if(Section == 0)
				{
					CUIRect NameRect, YesRect, NoRect;
					Item.VSplitRight(66.0f, &Item, &NoRect);
					Item.VSplitRight(66.0f, &Item, &YesRect);
					NameRect = Item;
					Ui()->DoLabel(&NameRect, Friend.m_Name.c_str(), 11.0f, TEXTALIGN_ML);
					if(GameClient()->m_Menus.DoButton_Menu(&s_aYes[i], "Принять", 0, &YesRect, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.25f, 0.7f, 0.4f, 0.55f)))
					{
						Accept(Friend);
						break;
					}
					if(GameClient()->m_Menus.DoButton_Menu(&s_aNo[i], "Отклонить", 0, &NoRect, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.8f, 0.2f, 0.3f, 0.55f)))
					{
						Remove(i);
						break;
					}
				}
				else if(Section == 1)
				{
					char aName[96];
					if(Friend.m_Unread > 0)
						str_format(aName, sizeof(aName), "%s  (+%d)", Friend.m_Name.c_str(), Friend.m_Unread);
					else
						str_copy(aName, Friend.m_Name.c_str());
					if(GameClient()->m_Menus.DoButton_Menu(&s_aOpen[i], aName, 0, &Item, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 6.0f, 0.0f, Friend.m_Unread > 0 ? ColorRGBA(0.75f, 0.3f, 0.9f, 0.6f) : ColorRGBA(0.3f, 0.16f, 0.6f, 0.45f)))
					{
						m_Selected = i;
						Friend.m_Unread = 0;
						Friend.m_NextPoll = 0;
						m_ScrollBack = 0.0f;
						break;
					}
				}
				else
				{
					CUIRect NameRect, NoRect;
					Item.VSplitRight(66.0f, &NameRect, &NoRect);
					char aName[96];
					str_format(aName, sizeof(aName), "%s: ждёт принятия", Friend.m_Name.c_str());
					TextRender()->TextColor(0.8f, 0.8f, 0.9f, 0.7f);
					Ui()->DoLabel(&NameRect, aName, 10.0f, TEXTALIGN_ML);
					TextRender()->TextColor(TextRender()->DefaultTextColor());
					if(GameClient()->m_Menus.DoButton_Menu(&s_aNo[i], "Отмена", 0, &NoRect, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.6f, 0.2f, 0.4f, 0.5f)))
					{
						Remove(i);
						break;
					}
				}
			}
		}
		if(!Any)
		{
			TextRender()->TextColor(0.8f, 0.8f, 0.9f, 0.55f);
			SLabelProperties Props;
			Props.m_MaxWidth = Inner.w;
			CUIRect Hint = Inner;
			Hint.HSplitTop(60.0f, &Hint, nullptr);
			Ui()->DoLabel(&Hint, "Пока никого нет. Отправь свой код другу и введи его код выше. Когда друг примет запрос, откроется чат.", 10.0f, TEXTALIGN_TL, Props);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
		}
		return;
	}

	SFriend &Friend = m_vFriends[m_Selected];

	// шапка чата: назад / имя / удалить
	{
		CUIRect Back, Remove_;
		Header.VSplitLeft(26.0f, &Back, &Header);
		Header.VSplitRight(60.0f, &Header, &Remove_);
		if(GameClient()->m_Menus.DoButton_Menu(&s_BackButton, "<", 0, &Back, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ButtonColor))
		{
			m_Selected = -1;
			return;
		}
		Header.VMargin(6.0f, &Header);
		TextRender()->TextColor(0.85f, 0.75f, 1.0f, 1.0f);
		Ui()->DoLabel(&Header, Friend.m_Name.c_str(), 14.0f, TEXTALIGN_ML);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
		if(GameClient()->m_Menus.DoButton_Menu(&s_RemoveButton, "Удалить", 0, &Remove_, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.7f, 0.2f, 0.3f, 0.5f)))
		{
			Remove(m_Selected);
			return;
		}
	}

	// ввод внизу
	CUIRect InputRow, EmoteBar;
	Inner.HSplitBottom(24.0f, &Inner, &InputRow);
	Inner.HSplitBottom(4.0f, &Inner, nullptr);
	if(m_EmoteOpen)
	{
		Inner.HSplitBottom(70.0f, &Inner, &EmoteBar);
		Inner.HSplitBottom(4.0f, &Inner, nullptr);
	}

	// сообщения: снизу вверх, последние видны
	{
		CUIRect Area = Inner;
		Area.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), IGraphics::CORNER_ALL, 8.0f);
		Area.Margin(6.0f, &Area);
		const int Count = (int)Friend.m_vMessages.size();
		if(Count == 0)
		{
			TextRender()->TextColor(0.8f, 0.8f, 0.9f, 0.55f);
			Ui()->DoLabel(&Area, "Напиши первое сообщение", 11.0f, TEXTALIGN_MC);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
		}
		else
		{
			// высоты снизу вверх, пока влезает
			std::vector<float> vHeights;
			float Total = 0.0f;
			int First = Count;
			while(First > 0 && Total < Area.h)
			{
				float Height = 0.0f;
				RenderBubble(Area, Friend.m_vMessages[First - 1], &Height, false, Area.w);
				Total += Height + 2.0f;
				First--;
			}
			// клип: рисуем только то, что влезает
			Ui()->ClipEnable(&Area);
			float Y = Area.y + Area.h;
			for(int i = Count - 1; i >= First; i--)
			{
				float Height = 0.0f;
				RenderBubble(Area, Friend.m_vMessages[i], &Height, false, Area.w);
				Y -= Height + 2.0f;
				CUIRect Row(Area.x, Y, Area.w, Height);
				RenderBubble(Row, Friend.m_vMessages[i], &Height, true, Area.w);
			}
			Ui()->ClipDisable();
		}
	}

	// панель эмоций
	if(m_EmoteOpen)
	{
		EmoteBar.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.3f), IGraphics::CORNER_ALL, 8.0f);
		CUIRect Bar;
		EmoteBar.Margin(4.0f, &Bar);
		const int PerRow = 8;
		const float Cell = std::min(Bar.w / PerRow, 32.0f);
		static CButtonContainer s_aEmotes[16];
		for(int e = 0; e < (int)NUM_EMOTICONS && e < 16; e++)
		{
			CUIRect Slot(Bar.x + (e % PerRow) * Cell, Bar.y + (e / PerRow) * (Cell + 2.0f), Cell, Cell);
			if(Ui()->DoButtonLogic(&s_aEmotes[e], 0, &Slot, BUTTONFLAG_LEFT))
			{
				char aEmote[16];
				str_format(aEmote, sizeof(aEmote), "[e:%d]", e);
				SendText(Friend, aEmote);
				m_EmoteOpen = false;
			}
			if(Ui()->HotItem() == &s_aEmotes[e])
				Slot.Draw(ColorRGBA(1, 1, 1, 0.15f), IGraphics::CORNER_ALL, 6.0f);
			Graphics()->TextureSet(GameClient()->m_EmoticonsSkin.m_aSpriteEmoticons[e]);
			Graphics()->QuadsSetSubset(0, 0, 1, 1);
			Graphics()->QuadsBegin();
			Graphics()->SetColor(1, 1, 1, 1);
			const IGraphics::CQuadItem Item(Slot.x + 1.0f, Slot.y + 1.0f, Cell - 2.0f, Cell - 2.0f);
			Graphics()->QuadsDrawTL(&Item, 1);
			Graphics()->QuadsEnd();
		}
	}

	// строка ввода
	{
		CUIRect EmoteButton, SendButton, TextRect;
		InputRow.VSplitLeft(30.0f, &EmoteButton, &InputRow);
		InputRow.VSplitRight(64.0f, &TextRect, &SendButton);
		TextRect.VMargin(4.0f, &TextRect);
		static CButtonContainer s_EmoteToggle, s_SendButton;
		if(GameClient()->m_Menus.DoButton_Menu(&s_EmoteToggle, ":)", m_EmoteOpen, &EmoteButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ButtonColor))
			m_EmoteOpen = !m_EmoteOpen;
		Ui()->DoEditBox(&m_TextInput, &TextRect, 11.0f);
		const bool Enter = m_TextInput.IsActive() && Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER);
		if(GameClient()->m_Menus.DoButton_Menu(&s_SendButton, "Отпр.", 0, &SendButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ButtonColor) || Enter)
		{
			SendText(Friend, m_TextInput.GetString());
			m_TextInput.Clear();
			m_TextInput.Activate(EInputPriority::UI);
		}
	}
}
