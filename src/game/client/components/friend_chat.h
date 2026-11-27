#ifndef GAME_CLIENT_COMPONENTS_FRIEND_CHAT_H
#define GAME_CLIENT_COMPONENTS_FRIEND_CHAT_H

#include <engine/http.h>

#include <game/client/component.h>
#include <game/client/lineinput.h>
#include <game/client/ui_rect.h>

#include <memory>
#include <string>
#include <vector>

// Чат друзей между клиентами. Своего сервера нет, поэтому сообщения идут через публичный pub/sub (ntfy.sh):
// у каждого есть «почтовый ящик» (топик по его коду), запрос дружбы и ответ приходят туда,
// а после принятия у пары появляется общий секретный топик для переписки.
class CFriendChat : public CComponent
{
public:
	enum
	{
		STATE_INCOMING = 0, // тебе прислали запрос
		STATE_OUTGOING = 1, // ты отправил запрос, ждём принятия
		STATE_FRIEND = 2, // дружба подтверждена обеими сторонами
	};

	struct SMessage
	{
		long long m_Time = 0;
		bool m_Mine = false;
		std::string m_Text;
	};

	struct SFriend
	{
		std::string m_Uid;
		std::string m_Name;
		std::string m_Secret;
		int m_State = STATE_INCOMING;
		int m_Unread = 0;
		std::string m_LastId; // последний id сообщения в топике чата
		int64_t m_NextPoll = 0;
		std::shared_ptr<IHttpRequest> m_pPoll;
		std::vector<SMessage> m_vMessages;
	};

private:
	std::vector<SFriend> m_vFriends;
	std::string m_InboxLastId;
	int64_t m_InboxNextPoll = 0;
	std::shared_ptr<IHttpRequest> m_pInboxPoll;
	std::vector<std::shared_ptr<IHttpRequest>> m_vSends;
	bool m_Loaded = false;
	bool m_Dirty = false;
	int64_t m_NextSave = 0;

	// интерфейс
	bool m_Open = false;
	int m_Selected = -1;
	char m_aStatus[128] = "";
	CLineInputBuffered<32> m_CodeInput;
	CLineInputBuffered<200> m_TextInput;
	float m_ScrollBack = 0.0f;
	bool m_EmoteOpen = false;

	void Load();
	void Save();
	SFriend *Find(const char *pUid);
	void EnsureUid();
	std::string Server() const;
	void Publish(const std::string &Topic, const std::string &Body);
	void SendPacket(const char *pToUid, const char *pType, const char *pPayload);
	void PollInbox();
	void PollChat(SFriend &Friend, bool Fast);
	void HandleInbox(const char *pBody);
	void AddMessage(SFriend &Friend, const char *pText, bool Mine, long long Time);
	static bool ForEachLine(const std::shared_ptr<IHttpRequest> &pReq, std::vector<std::pair<std::string, std::string>> &vOut);
	void Tick();

	void SendRequest(const char *pCode);
	void Accept(SFriend &Friend);
	void Remove(int Index);
	void SendText(SFriend &Friend, const char *pText);

	void RenderBubble(const CUIRect &Row, const SMessage &Msg, float *pHeightOut, bool Draw, float Width);

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnRender() override;
	void OnShutdown() override;

	bool IsOpen() const { return m_Open; }
	void Toggle() { m_Open = !m_Open; }
	void SetOpen(bool Open) { m_Open = Open; }
	int TotalUnread() const;
	int Pending() const;
	// Рисует окно чата в заданном прямоугольнике (стартовое меню)
	void RenderPanel(CUIRect Rect);
	const char *Uid() const;
};

#endif
