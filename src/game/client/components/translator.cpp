#include "translator.h"

#include <base/str.h>

#include <engine/client.h>
#include <engine/console.h>
#include <engine/external/json-parser/json.h>
#include <engine/shared/config.h>

#include <game/client/gameclient.h>

#include <algorithm>

static const int MAX_ACTIVE = 2;
static const size_t MAX_QUEUE = 24;
static const size_t MAX_CACHE = 400;
static const int NUM_SERVICES = 3;

static bool HasLetters(const char *pStr)
{
	const char *p = pStr;
	while(*p)
	{
		const int Code = str_utf8_decode(&p);
		if(Code > 0x40 && !(Code >= 0x5B && Code <= 0x60) && !(Code >= 0x7B && Code <= 0x7F))
			return true;
	}
	return false;
}

void CTranslator::OnConsoleInit()
{
	Console()->Register("tr_say", "s[lang] r[text]", CFGFLAG_CLIENT, ConTrSay, this, "Translate the text into the language (en, de, ru...) and send it to the chat");
}

void CTranslator::ConTrSay(IConsole::IResult *pResult, void *pUserData)
{
	static_cast<CTranslator *>(pUserData)->Translate(pResult->GetString(1), pResult->GetString(0), true, "", 0);
}

void CTranslator::OnReset()
{
	for(SJob &Job : m_vActive)
		if(Job.m_pRequest)
			Job.m_pRequest->Abort();
	m_vActive.clear();
	m_Queue.clear();
}

void CTranslator::RequestChat(int ClientId, const char *pText)
{
	if(!g_Config.m_ClTranslate || ClientId < 0 || ClientId >= MAX_CLIENTS)
		return;
	if(ClientId == GameClient()->m_Snap.m_LocalClientId && !g_Config.m_ClTranslateOwn)
		return;
	if(str_length(pText) < 2 || pText[0] == '/' || !HasLetters(pText))
		return;

	Translate(pText, g_Config.m_ClTranslateLang, false, GameClient()->m_aClients[ClientId].m_aName);
}

void CTranslator::Translate(const char *pText, const char *pTarget, bool Say, const char *pName, int Team)
{
	SJob Job;
	Job.m_Team = Team;
	Job.m_Text = pText;
	Job.m_Target = pTarget[0] ? pTarget : "ru";
	Job.m_Say = Say;
	Job.m_Name = pName;

	// the same text was already translated: show it at once
	const auto It = m_Cache.find(Job.m_Target + "|" + Job.m_Text);
	if(It != m_Cache.end())
	{
		Deliver(Job, It->second);
		return;
	}
	if(m_Queue.size() >= MAX_QUEUE)
		m_Queue.pop_front();
	m_Queue.push_back(std::move(Job));
}

void CTranslator::Start(SJob &Job)
{
	char aEscaped[2048];
	EscapeUrl(aEscaped, Job.m_Text.c_str());
	char aLang[32];
	EscapeUrl(aLang, Job.m_Target.c_str());
	char aUrl[2600];
	if(Job.m_Service == 0)
		str_format(aUrl, sizeof(aUrl), "https://translate.googleapis.com/translate_a/single?client=gtx&sl=auto&tl=%s&dt=t&q=%s", aLang, aEscaped);
	else if(Job.m_Service == 1)
	{
		char aPair[64];
		str_format(aPair, sizeof(aPair), "Autodetect|%s", Job.m_Target.c_str());
		char aPairEscaped[128];
		EscapeUrl(aPairEscaped, aPair);
		str_format(aUrl, sizeof(aUrl), "https://api.mymemory.translated.net/get?q=%s&langpair=%s", aEscaped, aPairEscaped);
	}
	else
		str_format(aUrl, sizeof(aUrl), "https://lingva.ml/api/v1/auto/%s/%s", aLang, aEscaped);

	Job.m_pRequest = HttpGet(aUrl);
	Job.m_pRequest->WriteToMemory();
	Job.m_pRequest->Timeout(CTimeout{4000, 8000, 0, 0});
	Job.m_pRequest->MaxResponseSize(256 * 1024);
	Job.m_pRequest->LogProgress(HTTPLOG::NONE);
	Http()->Run(Job.m_pRequest);
}

bool CTranslator::Parse(SJob &Job, std::string &Result, std::string &Detected) const
{
	json_value *pRoot = Job.m_pRequest->ResultJson();
	if(!pRoot)
		return false;
	bool Ok = false;
	Result.clear();
	Detected.clear();
	if(Job.m_Service == 0)
	{
		// [[["translated","original",...],...],null,"detected language"]
		if(pRoot->type == json_array && pRoot->u.array.length > 0 && (*pRoot)[0].type == json_array)
		{
			const json_value &Parts = (*pRoot)[0];
			for(unsigned i = 0; i < Parts.u.array.length; i++)
			{
				const json_value &Part = Parts[i];
				if(Part.type == json_array && Part.u.array.length > 0 && Part[0].type == json_string)
					Result += Part[0].u.string.ptr;
			}
			if(pRoot->u.array.length > 2 && (*pRoot)[2].type == json_string)
				Detected = (*pRoot)[2].u.string.ptr;
			Ok = !Result.empty();
		}
	}
	else if(Job.m_Service == 1)
	{
		if(pRoot->type == json_object)
		{
			const json_value &Data = (*pRoot)["responseData"];
			if(Data.type == json_object && Data["translatedText"].type == json_string)
			{
				Result = Data["translatedText"].u.string.ptr;
				Ok = !Result.empty() && str_find_nocase(Result.c_str(), "MYMEMORY WARNING") == nullptr && str_find_nocase(Result.c_str(), "INVALID") == nullptr;
			}
		}
	}
	else
	{
		if(pRoot->type == json_object && (*pRoot)["translation"].type == json_string)
		{
			Result = (*pRoot)["translation"].u.string.ptr;
			Ok = !Result.empty();
		}
	}
	json_value_free(pRoot);
	return Ok;
}

void CTranslator::Deliver(const SJob &Job, const std::string &Result)
{
	if(Job.m_Say)
	{
		GameClient()->m_Chat.m_SkipTranslate = true;
		GameClient()->m_Chat.SendChat(Job.m_Team, Result.c_str());
		GameClient()->m_Chat.m_SkipTranslate = false;
		return;
	}
	char aBuf[1024];
	if(Job.m_Name.empty())
		str_format(aBuf, sizeof(aBuf), "[Перевод] %s", Result.c_str());
	else
		str_format(aBuf, sizeof(aBuf), "[Перевод] %s: %s", Job.m_Name.c_str(), Result.c_str());
	GameClient()->m_Chat.Echo(aBuf);
}

void CTranslator::OnRender()
{
	const float Now = Client()->LocalTime();

	for(size_t i = 0; i < m_vActive.size();)
	{
		SJob &Job = m_vActive[i];
		const EHttpState State = Job.m_pRequest->State();
		if(State == EHttpState::QUEUED || State == EHttpState::RUNNING)
		{
			i++;
			continue;
		}

		std::string Result, Detected;
		const bool Ok = State == EHttpState::DONE && Parse(Job, Result, Detected);
		if(Ok)
		{
			m_Failures = 0;
			if(m_Cache.size() >= MAX_CACHE)
				m_Cache.clear();
			m_Cache[Job.m_Target + "|" + Job.m_Text] = Result;
			const bool SameLanguage = !Detected.empty() && str_comp_nocase_num(Detected.c_str(), Job.m_Target.c_str(), 2) == 0;
			const bool SameText = str_comp_nocase(Result.c_str(), Job.m_Text.c_str()) == 0;
			if(Job.m_Say || (!SameLanguage && !SameText))
				Deliver(Job, Result);
			m_vActive.erase(m_vActive.begin() + i);
			continue;
		}

		// the service failed: try the next one, wait a bit more each time
		Job.m_pRequest = nullptr;
		Job.m_Service++;
		if(Job.m_Service >= NUM_SERVICES)
		{
			Job.m_Service = 0;
			Job.m_Attempt++;
		}
		if(Job.m_Attempt < 2)
		{
			m_Failures++;
			m_Queue.push_front(std::move(Job));
		}
		else if(Job.m_Say)
		{
			GameClient()->m_Chat.m_SkipTranslate = true;
			GameClient()->m_Chat.SendChat(Job.m_Team, Job.m_Text.c_str()); // do not lose the message
			GameClient()->m_Chat.m_SkipTranslate = false;
		}
		m_vActive.erase(m_vActive.begin() + i);
		m_NextStart = Now + std::min(0.4f * m_Failures, 3.0f);
	}

	while(!m_Queue.empty() && (int)m_vActive.size() < MAX_ACTIVE && Now >= m_NextStart)
	{
		SJob Job = std::move(m_Queue.front());
		m_Queue.pop_front();
		Start(Job);
		m_vActive.push_back(std::move(Job));
	}
}

bool CTranslator::SendTranslated(int Team, const char *pLine)
{
	if(!g_Config.m_ClTranslateSend || str_length(pLine) < 2 || pLine[0] == '/' || !HasLetters(pLine))
		return false;
	Translate(pLine, g_Config.m_ClTranslateSendLang, true, "", Team);
	return true;
}
