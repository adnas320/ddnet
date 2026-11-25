#ifndef GAME_EDITOR_MULTIMAP_H
#define GAME_EDITOR_MULTIMAP_H

#include <base/color.h>
#include <base/net.h>
#include <base/vmath.h>

#include <engine/shared/jobs.h>

#include <game/client/lineinput.h>
#include <game/client/ui.h>
#include <game/mapitems.h>

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

class CEditor;
class CEditorMap;
class CDataFileWriterFinishJob;

// Совместное строительство карты: один игрок создаёт сессию (хост), остальные подключаются по адресу.
// Передаются: курсоры участников, что они делают, и изменения тайловых слоёв (обычные, game, front).
class CEditorMulti
{
public:
	CEditorMulti();
	~CEditorMulti();

	// каждый кадр, из CEditor::Render
	void Update(CEditor *pEditor);
	// курсоры и подсветка чужих правок, рисуется в мире карты
	void RenderWorld(CEditor *pEditor);
	// кнопка в левой панели редактора
	void DoButton(CEditor *pEditor, const CUIRect &Rect);

	bool Active() const { return m_Mode != MODE_OFF; }
	void Stop(const char *pReason);

	SPopupMenuId m_PopupId;

private:
	enum EMode
	{
		MODE_OFF,
		MODE_HOST,
		MODE_CONNECTING,
		MODE_CLIENT,
	};

	enum EMsg : uint8_t
	{
		MSG_HELLO = 1,
		MSG_MAP,
		MSG_CURSOR,
		MSG_TILES,
		MSG_LEAVE,
	};

	struct SPeer
	{
		NETSOCKET m_Sock = nullptr;
		int m_Id = 0;
		bool m_Ready = false; // карта уже отправлена
		bool m_Hello = false;
		std::vector<uint8_t> m_vRecv;
		std::vector<uint8_t> m_vSend;
		std::vector<std::vector<uint8_t>> m_vDeferred; // правки, пришедшие пока карта ещё не ушла
		char m_aName[24] = "";
	};

	struct SRemote
	{
		int m_Id = 0;
		vec2 m_Pos = vec2(0, 0);
		bool m_Down = false;
		ColorRGBA m_Color;
		char m_aName[24] = "";
		char m_aInfo[40] = "";
		int64_t m_LastUpdate = 0;
		int m_Edits = 0;
	};

	struct SFlash
	{
		int m_Group;
		int m_X0, m_Y0, m_X1, _Y1Dummy;
		int m_Y1;
		ColorRGBA m_Color;
		int64_t m_Time;
	};

	struct SConnectState
	{
		std::atomic<int> m_Result{0}; // 0 идёт, 1 успех, -1 ошибка
		std::atomic<bool> m_Abandoned{false};
		NETSOCKET m_Sock = nullptr;
		NETADDR m_Addr;
	};

	struct SShadow
	{
		int m_W = 0;
		int m_H = 0;
		std::vector<CTile> m_vTiles;
	};

	CEditor *m_pEditor = nullptr;
	EMode m_Mode = MODE_OFF;
	int m_MyId = 0;
	char m_aStatus[160] = "Не подключено";

	// сеть
	NETSOCKET m_Listen = nullptr;
	std::vector<std::unique_ptr<SPeer>> m_vpPeers; // хост: все клиенты
	SPeer m_Server; // клиент: соединение с хостом
	std::shared_ptr<SConnectState> m_pConnect;
	int64_t m_ConnectStart = 0;
	int m_NextId = 1;

	// карта
	CEditorMap *m_pSyncMap = nullptr;
	std::map<int, SShadow> m_Shadows; // ключ: группа << 16 | слой
	std::shared_ptr<CDataFileWriterFinishJob> m_pSaveJob;
	std::vector<int> m_vWaitForMap; // id клиентов, которым надо отправить карту
	std::vector<int> m_vSavingFor; // для кого сейчас сохраняется снимок
	char m_aMapPath[128] = "multimap/session.map";

	// участники
	std::map<int, SRemote> m_Remotes;
	std::vector<SFlash> m_vFlashes;
	int64_t m_LastCursorSend = 0;
	int64_t m_LastDiff = 0;
	vec2 m_LastSentPos = vec2(-1, -1);
	int m_TilesSent = 0;
	int m_TilesReceived = 0;

	// ввод в окне
	CLineInputBuffered<64> m_AddressInput;
	CLineInputBuffered<8> m_PortInput;
	bool m_InputsInit = false;

	static CUi::EPopupMenuFunctionResult PopupFn(void *pContext, CUIRect View, bool Active);
	CUi::EPopupMenuFunctionResult RenderPopup(CUIRect View);

	bool MapStillOpen() const;
	void StartHost(int Port);
	void StartJoin(const char *pAddress, int Port);
	void SetStatus(const char *pFormat, ...);

	void PumpHost();
	void PumpClient();
	void PumpPeer(SPeer &Peer, bool IsServerSide);
	void FlushSend(SPeer &Peer);
	void DropPeer(size_t Index);
	void HandleMessage(SPeer *pFrom, uint8_t Type, const uint8_t *pData, size_t Size);

	void QueueMap(int PeerId);
	void PollMapSave();
	void SendToPeer(SPeer &Peer, const std::vector<uint8_t> &vMsg);
	void Broadcast(const std::vector<uint8_t> &vMsg, const SPeer *pExcept, bool OnlyReady = true);
	void SendUp(const std::vector<uint8_t> &vMsg); // клиент -> хост, хост -> все

	void SendCursor(CEditor *pEditor);
	void ScanLocalChanges(CEditor *pEditor);
	void ApplyTiles(const uint8_t *pData, size_t Size, int FromId);
	bool LoadReceivedMap(const std::vector<uint8_t> &vData);
	ColorRGBA ColorForId(int Id) const;
};

#endif
