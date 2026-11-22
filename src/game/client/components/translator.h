#ifndef GAME_CLIENT_COMPONENTS_TRANSLATOR_H
#define GAME_CLIENT_COMPONENTS_TRANSLATOR_H

#include <engine/console.h>
#include <engine/http.h>

#include <game/client/component.h>

#include <deque>
#include <map>
#include <memory>
#include <string>

// Translates incoming chat messages. Requests run in the background with a queue, a cache,
// several services as fallback and retries, so it works for the whole session.
class CTranslator : public CComponent
{
	struct SJob
	{
		std::string m_Text;
		std::string m_Name; // empty: no name prefix
		std::string m_Target;
		bool m_Say = false; // send the translation to the chat instead of showing it
		int m_Service = 0; // 0 = Google, 1 = MyMemory, 2 = Lingva
		int m_Attempt = 0;
		std::shared_ptr<IHttpRequest> m_pRequest;
	};
	std::deque<SJob> m_Queue;
	std::vector<SJob> m_vActive;
	std::map<std::string, std::string> m_Cache;
	float m_NextStart = 0.0f;
	int m_Failures = 0;

	void Start(SJob &Job);
	bool Parse(SJob &Job, std::string &Result, std::string &Detected) const;
	void Deliver(const SJob &Job, const std::string &Result);
	static void ConTrSay(IConsole::IResult *pResult, void *pUserData);

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnConsoleInit() override;
	void OnRender() override;
	void OnReset() override;

	// Called for each new chat line from another player.
	void RequestChat(int ClientId, const char *pText);
	void Translate(const char *pText, const char *pTarget, bool Say, const char *pName = "");
	int Pending() const { return (int)(m_Queue.size() + m_vActive.size()); }
};

#endif
