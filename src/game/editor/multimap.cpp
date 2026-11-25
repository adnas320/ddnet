#include "multimap.h"

#include "editor.h"

#include <base/io.h>
#include <base/log.h>
#include <base/mem.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/graphics.h>
#include <engine/input.h>
#include <engine/keys.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <game/editor/mapitems/layer_group.h>
#include <game/editor/mapitems/layer_tiles.h>
#include <game/editor/mapitems/map.h>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <thread>

namespace {

constexpr int DEFAULT_PORT = 8750;
constexpr size_t MAX_MESSAGE = 96 * 1024 * 1024;
constexpr size_t TILES_PER_MESSAGE = 4000;

// ---- сериализация ----
struct CWriter
{
	std::vector<uint8_t> m_v;
	explicit CWriter(uint8_t Type)
	{
		m_v.resize(4);
		m_v.push_back(Type);
	}
	void U8(uint32_t V) { m_v.push_back((uint8_t)V); }
	void U16(uint32_t V)
	{
		U8(V & 0xff);
		U8((V >> 8) & 0xff);
	}
	void U32(uint32_t V)
	{
		U16(V & 0xffff);
		U16((V >> 16) & 0xffff);
	}
	void F32(float F)
	{
		uint32_t V;
		mem_copy(&V, &F, 4);
		U32(V);
	}
	void Str(const char *pStr, int Max)
	{
		const int Len = std::min<int>(str_length(pStr), Max);
		U8(Len);
		for(int i = 0; i < Len; i++)
			U8((uint8_t)pStr[i]);
	}
	void Bytes(const void *pData, size_t Size)
	{
		const uint8_t *p = (const uint8_t *)pData;
		m_v.insert(m_v.end(), p, p + Size);
	}
	std::vector<uint8_t> Finish()
	{
		const uint32_t Len = (uint32_t)(m_v.size() - 4);
		m_v[0] = Len & 0xff;
		m_v[1] = (Len >> 8) & 0xff;
		m_v[2] = (Len >> 16) & 0xff;
		m_v[3] = (Len >> 24) & 0xff;
		return std::move(m_v);
	}
};

struct CReader
{
	const uint8_t *m_pData;
	const uint8_t *m_pEnd;
	bool m_Ok = true;
	CReader(const uint8_t *pData, size_t Size) :
		m_pData(pData), m_pEnd(pData + Size) {}
	uint32_t U8()
	{
		if(m_pData >= m_pEnd)
		{
			m_Ok = false;
			return 0;
		}
		return *m_pData++;
	}
	uint32_t U16()
	{
		const uint32_t a = U8();
		return a | (U8() << 8);
	}
	uint32_t U32()
	{
		const uint32_t a = U16();
		return a | (U16() << 16);
	}
	float F32()
	{
		const uint32_t V = U32();
		float F;
		mem_copy(&F, &V, 4);
		return F;
	}
	void Str(char *pBuf, int BufSize)
	{
		const int Len = U8();
		int Out = 0;
		for(int i = 0; i < Len; i++)
		{
			const char c = (char)U8();
			if(Out < BufSize - 1 && c >= 32)
				pBuf[Out++] = c;
		}
		pBuf[Out] = '\0';
	}
	size_t Left() const { return m_pEnd - m_pData; }
};

int64_t Now()
{
	return time_get();
}

float Seconds(int64_t Since)
{
	return (float)(Now() - Since) / (float)time_freq();
}

} // namespace

CEditorMulti::CEditorMulti() = default;

CEditorMulti::~CEditorMulti()
{
	Stop(nullptr);
}

ColorRGBA CEditorMulti::ColorForId(int Id) const
{
	const float Hue = std::fmod(0.08f + Id * 0.19f, 1.0f);
	return color_cast<ColorRGBA>(ColorHSLA(Hue, 0.85f, 0.6f));
}

void CEditorMulti::SetStatus(const char *pFormat, ...)
{
	va_list Args;
	va_start(Args, pFormat);
	vsnprintf(m_aStatus, sizeof(m_aStatus), pFormat, Args);
	va_end(Args);
}

bool CEditorMulti::MapStillOpen() const
{
	if(m_pSyncMap == nullptr)
		return true;
	for(const auto &pMap : m_pEditor->m_vpMaps)
		if(pMap.get() == m_pSyncMap)
			return true;
	return false;
}

// ---------------------------------------------------------------- старт / стоп

void CEditorMulti::Stop(const char *pReason)
{
	if(m_Listen)
	{
		net_tcp_close(m_Listen);
		m_Listen = nullptr;
	}
	for(auto &pPeer : m_vpPeers)
		if(pPeer->m_Sock)
			net_tcp_close(pPeer->m_Sock);
	m_vpPeers.clear();
	if(m_Server.m_Sock)
	{
		net_tcp_close(m_Server.m_Sock);
		m_Server.m_Sock = nullptr;
	}
	m_Server.m_vRecv.clear();
	m_Server.m_vSend.clear();
	if(m_pConnect)
	{
		m_pConnect->m_Abandoned = true;
		m_pConnect.reset();
	}
	m_Mode = MODE_OFF;
	m_pSyncMap = nullptr;
	m_Shadows.clear();
	m_Remotes.clear();
	m_vFlashes.clear();
	m_vWaitForMap.clear();
	m_vSavingFor.clear();
	m_pSaveJob.reset();
	m_MyId = 0;
	m_NextId = 1;
	if(pReason)
		SetStatus("%s", pReason);
}

void CEditorMulti::StartHost(int Port)
{
	Stop(nullptr);
	NETADDR Bind = NETADDR_ZEROED;
	Bind.type = NETTYPE_ALL;
	Bind.port = Port;
	m_Listen = net_tcp_create(Bind);
	if(!m_Listen || net_tcp_listen(m_Listen, 8) != 0)
	{
		if(m_Listen)
			net_tcp_close(m_Listen);
		m_Listen = nullptr;
		SetStatus("Не удалось открыть порт %d (занят?)", Port);
		return;
	}
	net_set_non_blocking(m_Listen);
	m_Mode = MODE_HOST;
	m_pSyncMap = m_pEditor->Map();
	m_MyId = 0;
	m_Shadows.clear();
	SetStatus("Сессия открыта, порт %d. Ждём игроков.", Port);
}

void CEditorMulti::StartJoin(const char *pAddress, int Port)
{
	Stop(nullptr);
	char aHost[96];
	str_copy(aHost, pAddress);
	NETADDR Addr = NETADDR_ZEROED;
	int PortFromText = 0;
	if(net_addr_from_str(&Addr, aHost) != 0)
	{
		// возможно имя вместо ip
		char aName[96];
		str_copy(aName, aHost);
		char *pColon = (char *)str_rchr(aName, ':');
		if(pColon && pColon != aName)
		{
			*pColon = '\0';
			PortFromText = str_toint(pColon + 1);
		}
		if(net_host_lookup(aName, &Addr, NETTYPE_ALL) != 0)
		{
			SetStatus("Адрес не распознан: %s", pAddress);
			return;
		}
		Addr.port = PortFromText;
	}
	if(Addr.port == 0)
		Addr.port = Port;

	NETADDR Bind = NETADDR_ZEROED;
	Bind.type = Addr.type;
	NETSOCKET Sock = net_tcp_create(Bind);
	if(!Sock)
	{
		SetStatus("Не удалось создать сокет");
		return;
	}
	m_pConnect = std::make_shared<SConnectState>();
	m_pConnect->m_Sock = Sock;
	m_pConnect->m_Addr = Addr;
	std::shared_ptr<SConnectState> pState = m_pConnect;
	std::thread([pState]() {
		const int Result = net_tcp_connect(pState->m_Sock, &pState->m_Addr);
		if(pState->m_Abandoned)
		{
			net_tcp_close(pState->m_Sock);
			pState->m_Sock = nullptr;
		}
		pState->m_Result = Result == 0 ? 1 : -1;
	}).detach();
	m_Mode = MODE_CONNECTING;
	m_ConnectStart = Now();
	SetStatus("Подключаюсь...");
}

// ---------------------------------------------------------------- отправка

void CEditorMulti::FlushSend(SPeer &Peer)
{
	if(!Peer.m_Sock)
		return;
	while(!Peer.m_vSend.empty())
	{
		const int Chunk = (int)std::min<size_t>(Peer.m_vSend.size(), 65536);
		const int Sent = net_tcp_send(Peer.m_Sock, Peer.m_vSend.data(), Chunk);
		if(Sent > 0)
		{
			Peer.m_vSend.erase(Peer.m_vSend.begin(), Peer.m_vSend.begin() + Sent);
			continue;
		}
		if(Sent < 0 && net_would_block())
			break;
		// ошибка соединения
		net_tcp_close(Peer.m_Sock);
		Peer.m_Sock = nullptr;
		Peer.m_vSend.clear();
		break;
	}
}

void CEditorMulti::SendToPeer(SPeer &Peer, const std::vector<uint8_t> &vMsg)
{
	if(!Peer.m_Sock)
		return;
	Peer.m_vSend.insert(Peer.m_vSend.end(), vMsg.begin(), vMsg.end());
}

void CEditorMulti::Broadcast(const std::vector<uint8_t> &vMsg, const SPeer *pExcept, bool OnlyReady)
{
	for(auto &pPeer : m_vpPeers)
	{
		if(pPeer.get() == pExcept || !pPeer->m_Hello)
			continue;
		if(!pPeer->m_Ready)
		{
			if(OnlyReady)
				pPeer->m_vDeferred.push_back(vMsg);
			continue;
		}
		SendToPeer(*pPeer, vMsg);
	}
}

void CEditorMulti::SendUp(const std::vector<uint8_t> &vMsg)
{
	if(m_Mode == MODE_HOST)
		Broadcast(vMsg, nullptr);
	else if(m_Mode == MODE_CLIENT)
		SendToPeer(m_Server, vMsg);
}

// ---------------------------------------------------------------- карта

void CEditorMulti::QueueMap(int PeerId)
{
	if(std::find(m_vWaitForMap.begin(), m_vWaitForMap.end(), PeerId) == m_vWaitForMap.end())
		m_vWaitForMap.push_back(PeerId);
	if(m_pSaveJob || !m_pSyncMap)
		return;
	m_pEditor->Storage()->CreateFolder("multimap", IStorage::TYPE_SAVE);
	m_vSavingFor = m_vWaitForMap;
	const size_t Before = m_pEditor->m_WriterFinishJobs.size();
	const auto &&ErrorHandler = [this](const char *pMessage) {
		SetStatus("Не удалось сохранить карту: %s", pMessage);
	};
	if(!m_pSyncMap->Save(m_aMapPath, ErrorHandler))
	{
		m_vWaitForMap.clear();
		return;
	}
	if(m_pEditor->m_WriterFinishJobs.size() > Before)
		m_pSaveJob = m_pEditor->m_WriterFinishJobs.back();
	else
		m_pSaveJob = nullptr;
	if(!m_pSaveJob)
	{
		m_vWaitForMap.clear();
		SetStatus("Не удалось подготовить карту для отправки");
	}
}

void CEditorMulti::PollMapSave()
{
	if(!m_pSaveJob || m_pSaveJob->State() != IJob::STATE_DONE)
		return;
	m_pSaveJob.reset();

	void *pData = nullptr;
	unsigned Size = 0;
	if(!m_pEditor->Storage()->ReadFile(m_aMapPath, IStorage::TYPE_SAVE, &pData, &Size))
	{
		SetStatus("Не удалось прочитать сохранённую карту");
		m_vWaitForMap.clear();
		return;
	}
	CWriter W(MSG_MAP);
	W.Bytes(pData, Size);
	const std::vector<uint8_t> vMsg = W.Finish();
	free(pData);

	const std::vector<int> vSavingFor = m_vSavingFor;
	m_vSavingFor.clear();
	for(int Id : vSavingFor)
	{
		m_vWaitForMap.erase(std::remove(m_vWaitForMap.begin(), m_vWaitForMap.end(), Id), m_vWaitForMap.end());
		for(auto &pPeer : m_vpPeers)
		{
			if(pPeer->m_Id != Id)
				continue;
			SendToPeer(*pPeer, vMsg);
			pPeer->m_Ready = true;
			for(const auto &vDeferred : pPeer->m_vDeferred)
				SendToPeer(*pPeer, vDeferred);
			pPeer->m_vDeferred.clear();
		}
	}
	SetStatus("Сессия идёт. Участников: %d", (int)m_vpPeers.size() + 1);
	if(!m_vWaitForMap.empty())
		QueueMap(m_vWaitForMap[0]); // кто подключился во время сохранения — получит свежий снимок
}

bool CEditorMulti::LoadReceivedMap(const std::vector<uint8_t> &vData)
{
	m_pEditor->Storage()->CreateFolder("multimap", IStorage::TYPE_SAVE);
	const char *pPath = "multimap/joined.map";
	IOHANDLE File = m_pEditor->Storage()->OpenFile(pPath, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
		return false;
	io_write(File, vData.data(), vData.size());
	io_close(File);
	if(!m_pEditor->Load(pPath, IStorage::TYPE_SAVE))
		return false;
	m_pSyncMap = m_pEditor->Map();
	m_Shadows.clear();
	m_vFlashes.clear();
	return true;
}

// ---------------------------------------------------------------- приём

void CEditorMulti::DropPeer(size_t Index)
{
	SPeer &Peer = *m_vpPeers[Index];
	const int Id = Peer.m_Id;
	const bool HadHello = Peer.m_Hello;
	if(Peer.m_Sock)
		net_tcp_close(Peer.m_Sock);
	m_vWaitForMap.erase(std::remove(m_vWaitForMap.begin(), m_vWaitForMap.end(), Id), m_vWaitForMap.end());
	m_vpPeers.erase(m_vpPeers.begin() + Index);
	m_Remotes.erase(Id);
	if(HadHello)
	{
		CWriter W(MSG_LEAVE);
		W.U8(Id);
		Broadcast(W.Finish(), nullptr, false);
	}
	SetStatus("Участник вышел. Участников: %d", (int)m_vpPeers.size() + 1);
}

void CEditorMulti::PumpPeer(SPeer &Peer, bool IsServerSide)
{
	if(!Peer.m_Sock)
		return;
	uint8_t aBuf[65536];
	for(int Iter = 0; Iter < 64; Iter++)
	{
		const int Got = net_tcp_recv(Peer.m_Sock, aBuf, sizeof(aBuf));
		if(Got > 0)
		{
			Peer.m_vRecv.insert(Peer.m_vRecv.end(), aBuf, aBuf + Got);
			continue;
		}
		if(Got < 0 && net_would_block())
			break;
		// 0 = закрыто, <0 = ошибка
		net_tcp_close(Peer.m_Sock);
		Peer.m_Sock = nullptr;
		return;
	}

	size_t Offset = 0;
	while(Peer.m_vRecv.size() - Offset >= 5)
	{
		const uint8_t *p = Peer.m_vRecv.data() + Offset;
		const uint32_t Len = p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24);
		if(Len == 0 || Len > MAX_MESSAGE)
		{
			net_tcp_close(Peer.m_Sock);
			Peer.m_Sock = nullptr;
			return;
		}
		if(Peer.m_vRecv.size() - Offset < 4 + (size_t)Len)
			break;
		const uint8_t Type = p[4];
		// копия, потому что обработчик может менять буферы (загрузка карты)
		std::vector<uint8_t> vPayload(p + 5, p + 4 + Len);
		Offset += 4 + Len;
		HandleMessage(IsServerSide ? &Peer : nullptr, Type, vPayload.data(), vPayload.size());
		if(!Peer.m_Sock)
			return;
	}
	if(Offset > 0)
		Peer.m_vRecv.erase(Peer.m_vRecv.begin(), Peer.m_vRecv.begin() + Offset);
}

void CEditorMulti::HandleMessage(SPeer *pFrom, uint8_t Type, const uint8_t *pData, size_t Size)
{
	CReader R(pData, Size);
	switch(Type)
	{
	case MSG_HELLO:
		if(pFrom) // хост: клиент представился
		{
			R.Str(pFrom->m_aName, sizeof(pFrom->m_aName));
			if(pFrom->m_aName[0] == '\0')
				str_copy(pFrom->m_aName, "Игрок");
			pFrom->m_Hello = true;
			CWriter W(MSG_HELLO);
			W.U8(pFrom->m_Id);
			SendToPeer(*pFrom, W.Finish());
			QueueMap(pFrom->m_Id);
			SetStatus("Подключился %s", pFrom->m_aName);
		}
		else // клиент: хост сказал наш id
		{
			m_MyId = R.U8();
			SetStatus("Подключено. Жду карту от хоста...");
		}
		break;
	case MSG_MAP:
		if(!pFrom)
		{
			SetStatus("Получаю карту...");
			if(LoadReceivedMap(std::vector<uint8_t>(pData, pData + Size)))
				SetStatus("Вы в сессии. Правки тайловых слоёв общие.");
			else
				SetStatus("Не удалось загрузить полученную карту");
		}
		break;
	case MSG_CURSOR:
	{
		const int Id = pFrom ? pFrom->m_Id : (int)R.U8();
		if(pFrom)
			R.U8();
		SRemote Remote;
		Remote.m_Id = Id;
		Remote.m_Pos.x = R.F32();
		Remote.m_Pos.y = R.F32();
		Remote.m_Down = R.U8() != 0;
		R.Str(Remote.m_aName, sizeof(Remote.m_aName));
		R.Str(Remote.m_aInfo, sizeof(Remote.m_aInfo));
		if(!R.m_Ok || Id == m_MyId)
			break;
		Remote.m_Color = ColorForId(Id);
		Remote.m_LastUpdate = Now();
		auto It = m_Remotes.find(Id);
		if(It != m_Remotes.end())
			Remote.m_Edits = It->second.m_Edits;
		m_Remotes[Id] = Remote;
		if(pFrom) // хост пересылает остальным с настоящим id
		{
			CWriter W(MSG_CURSOR);
			W.U8(Id);
			W.F32(Remote.m_Pos.x);
			W.F32(Remote.m_Pos.y);
			W.U8(Remote.m_Down ? 1 : 0);
			W.Str(Remote.m_aName, 20);
			W.Str(Remote.m_aInfo, 36);
			Broadcast(W.Finish(), pFrom, false);
		}
		break;
	}
	case MSG_TILES:
	{
		const int FromId = pFrom ? pFrom->m_Id : -1;
		std::vector<uint8_t> vCopy;
		if(pFrom)
		{
			// подставляем реальный id отправителя и пересылаем остальным
			vCopy.assign(pData, pData + Size);
			if(!vCopy.empty())
				vCopy[0] = (uint8_t)pFrom->m_Id;
			ApplyTiles(vCopy.data(), vCopy.size(), FromId);
			CWriter W(MSG_TILES);
			W.Bytes(vCopy.data(), vCopy.size());
			Broadcast(W.Finish(), pFrom);
		}
		else
		{
			ApplyTiles(pData, Size, FromId);
		}
		break;
	}
	case MSG_LEAVE:
	{
		const int Id = R.U8();
		m_Remotes.erase(Id);
		break;
	}
	default:
		break;
	}
}

void CEditorMulti::ApplyTiles(const uint8_t *pData, size_t Size, int FromId)
{
	if(!m_pSyncMap)
		return;
	CReader R(pData, Size);
	const int SenderId = R.U8();
	const int Group = R.U16();
	const int Layer = R.U16();
	const int W = R.U16();
	const int H = R.U16();
	const int Count = R.U16();
	if(!R.m_Ok || Group >= (int)m_pSyncMap->m_vpGroups.size())
		return;
	auto &pGroup = m_pSyncMap->m_vpGroups[Group];
	if(Layer >= (int)pGroup->m_vpLayers.size())
		return;
	std::shared_ptr<CLayer> pLayer = pGroup->m_vpLayers[Layer];
	if(pLayer->m_Type != LAYERTYPE_TILES)
		return;
	CLayerTiles *pTiles = static_cast<CLayerTiles *>(pLayer.get());
	if(pTiles->m_Width != W || pTiles->m_Height != H || pTiles->m_HasTele || pTiles->m_HasSpeedup || pTiles->m_HasSwitch || pTiles->m_HasTune)
		return;

	SShadow &Shadow = m_Shadows[(Group << 16) | Layer];
	if(Shadow.m_W != W || Shadow.m_H != H)
	{
		Shadow.m_W = W;
		Shadow.m_H = H;
		Shadow.m_vTiles.assign(pTiles->m_pTiles, pTiles->m_pTiles + (size_t)W * H);
	}

	int MinX = W, MinY = H, MaxX = -1, MaxY = -1;
	for(int i = 0; i < Count; i++)
	{
		const uint32_t Idx = R.U32();
		CTile Tile;
		Tile.m_Index = R.U8();
		Tile.m_Flags = R.U8();
		Tile.m_Skip = R.U8();
		Tile.m_Reserved = R.U8();
		if(!R.m_Ok)
			break;
		if(Idx >= (uint32_t)W * H)
			continue;
		pTiles->m_pTiles[Idx] = Tile;
		Shadow.m_vTiles[Idx] = Tile;
		const int X = Idx % W, Y = Idx / W;
		MinX = std::min(MinX, X);
		MinY = std::min(MinY, Y);
		MaxX = std::max(MaxX, X);
		MaxY = std::max(MaxY, Y);
	}
	if(MaxX < 0)
		return;
	m_TilesReceived += Count;
	m_pSyncMap->OnModify();

	SFlash Flash;
	Flash.m_Group = Group;
	Flash.m_X0 = MinX;
	Flash.m_Y0 = MinY;
	Flash.m_X1 = MaxX + 1;
	Flash.m_Y1 = MaxY + 1;
	Flash.m_Color = ColorForId(SenderId);
	Flash.m_Time = Now();
	m_vFlashes.push_back(Flash);
	if(m_vFlashes.size() > 200)
		m_vFlashes.erase(m_vFlashes.begin(), m_vFlashes.begin() + 100);
	auto It = m_Remotes.find(SenderId);
	if(It != m_Remotes.end())
		It->second.m_Edits += Count;
	(void)FromId;
}

// ---------------------------------------------------------------- локальные правки

void CEditorMulti::ScanLocalChanges(CEditor *pEditor)
{
	if(!m_pSyncMap)
		return;
	for(int g = 0; g < (int)m_pSyncMap->m_vpGroups.size(); g++)
	{
		auto &pGroup = m_pSyncMap->m_vpGroups[g];
		for(int l = 0; l < (int)pGroup->m_vpLayers.size(); l++)
		{
			CLayer *pLayer = pGroup->m_vpLayers[l].get();
			if(pLayer->m_Type != LAYERTYPE_TILES)
				continue;
			CLayerTiles *pTiles = static_cast<CLayerTiles *>(pLayer);
			if(pTiles->m_HasTele || pTiles->m_HasSpeedup || pTiles->m_HasSwitch || pTiles->m_HasTune)
				continue;
			const int W = pTiles->m_Width, H = pTiles->m_Height;
			if(W <= 0 || H <= 0 || W > 65535 || H > 65535)
				continue;
			SShadow &Shadow = m_Shadows[(g << 16) | l];
			const size_t Count = (size_t)W * H;
			if(Shadow.m_W != W || Shadow.m_H != H)
			{
				Shadow.m_W = W;
				Shadow.m_H = H;
				Shadow.m_vTiles.assign(pTiles->m_pTiles, pTiles->m_pTiles + Count);
				continue;
			}
			if(mem_comp(Shadow.m_vTiles.data(), pTiles->m_pTiles, Count * sizeof(CTile)) == 0)
				continue;

			std::vector<uint32_t> vChanged;
			for(size_t i = 0; i < Count; i++)
			{
				if(mem_comp(&Shadow.m_vTiles[i], &pTiles->m_pTiles[i], sizeof(CTile)) != 0)
				{
					vChanged.push_back((uint32_t)i);
					Shadow.m_vTiles[i] = pTiles->m_pTiles[i];
				}
			}
			for(size_t Start = 0; Start < vChanged.size(); Start += TILES_PER_MESSAGE)
			{
				const size_t End = std::min(vChanged.size(), Start + TILES_PER_MESSAGE);
				CWriter Msg(MSG_TILES);
				Msg.U8(m_MyId);
				Msg.U16(g);
				Msg.U16(l);
				Msg.U16(W);
				Msg.U16(H);
				Msg.U16((uint32_t)(End - Start));
				for(size_t i = Start; i < End; i++)
				{
					const CTile &Tile = pTiles->m_pTiles[vChanged[i]];
					Msg.U32(vChanged[i]);
					Msg.U8(Tile.m_Index);
					Msg.U8(Tile.m_Flags);
					Msg.U8(Tile.m_Skip);
					Msg.U8(Tile.m_Reserved);
				}
				SendUp(Msg.Finish());
				m_TilesSent += (int)(End - Start);
			}
		}
	}
	(void)pEditor;
}

void CEditorMulti::SendCursor(CEditor *pEditor)
{
	const bool Here = pEditor->Map() == m_pSyncMap;
	vec2 Pos = Here ? pEditor->MapView()->MouseWorldNoParaPos() : vec2(-1.0e9f, -1.0e9f);
	const bool Down = Here && pEditor->Input()->KeyIsPressed(KEY_MOUSE_1) && pEditor->MapView()->IsFocused();

	char aInfo[40] = "";
	if(Here && pEditor->Map()->m_SelectedGroup >= 0 && pEditor->Map()->m_SelectedGroup < (int)pEditor->Map()->m_vpGroups.size() && !pEditor->Map()->m_vSelectedLayers.empty())
	{
		auto &pGroup = pEditor->Map()->m_vpGroups[pEditor->Map()->m_SelectedGroup];
		const int LayerIndex = pEditor->Map()->m_vSelectedLayers[0];
		if(LayerIndex >= 0 && LayerIndex < (int)pGroup->m_vpLayers.size())
		{
			const CLayer *pLayer = pGroup->m_vpLayers[LayerIndex].get();
			if(pLayer->m_aName[0] != '\0')
				str_copy(aInfo, pLayer->m_aName);
			else
				str_format(aInfo, sizeof(aInfo), "слой %d", LayerIndex);
		}
	}
	if(Down)
	{
		str_append(aInfo, aInfo[0] ? " · строит" : "строит");
	}

	const int64_t NowT = Now();
	const bool Moved = distance(Pos, m_LastSentPos) > 0.5f;
	if(!Moved && !Down && Seconds(m_LastCursorSend) < 0.5f)
		return;
	if(Seconds(m_LastCursorSend) < 0.04f)
		return;
	m_LastCursorSend = NowT;
	m_LastSentPos = Pos;

	CWriter W(MSG_CURSOR);
	W.U8(m_MyId);
	W.F32(Pos.x);
	W.F32(Pos.y);
	W.U8(Down ? 1 : 0);
	W.Str(pEditor->Config()->m_PlayerName[0] ? pEditor->Config()->m_PlayerName : "Игрок", 20);
	W.Str(aInfo, 36);
	std::vector<uint8_t> vMsg = W.Finish();
	if(m_Mode == MODE_HOST)
		Broadcast(vMsg, nullptr, false);
	else
		SendToPeer(m_Server, vMsg);
}

// ---------------------------------------------------------------- каждый кадр

void CEditorMulti::PumpHost()
{
	if(m_Listen)
	{
		for(int i = 0; i < 4; i++)
		{
			NETSOCKET New = nullptr;
			NETADDR Addr;
			if(net_tcp_accept(m_Listen, &New, &Addr) < 0 || !New)
				break;
			net_set_non_blocking(New);
			auto pPeer = std::make_unique<SPeer>();
			pPeer->m_Sock = New;
			pPeer->m_Id = m_NextId++;
			if(m_NextId > 250)
				m_NextId = 1;
			m_vpPeers.push_back(std::move(pPeer));
		}
	}
	for(size_t i = 0; i < m_vpPeers.size();)
	{
		PumpPeer(*m_vpPeers[i], true);
		FlushSend(*m_vpPeers[i]);
		if(!m_vpPeers[i]->m_Sock)
			DropPeer(i);
		else
			i++;
	}
}

void CEditorMulti::PumpClient()
{
	if(m_Mode == MODE_CONNECTING)
	{
		if(!m_pConnect)
		{
			Stop("Ошибка подключения");
			return;
		}
		const int Result = m_pConnect->m_Result;
		if(Result == 1)
		{
			m_Server.m_Sock = m_pConnect->m_Sock;
			m_pConnect.reset();
			net_set_non_blocking(m_Server.m_Sock);
			m_Mode = MODE_CLIENT;
			CWriter W(MSG_HELLO);
			W.Str(m_pEditor->Config()->m_PlayerName[0] ? m_pEditor->Config()->m_PlayerName : "Игрок", 20);
			SendToPeer(m_Server, W.Finish());
			SetStatus("Соединился, жду ответ хоста...");
		}
		else if(Result == -1)
		{
			if(m_pConnect->m_Sock)
				net_tcp_close(m_pConnect->m_Sock);
			Stop("Не удалось подключиться (проверьте адрес, порт и что хост запущен)");
		}
		else if(Seconds(m_ConnectStart) > 20.0f)
		{
			Stop("Время ожидания подключения вышло");
		}
		return;
	}

	PumpPeer(m_Server, false);
	FlushSend(m_Server);
	if(!m_Server.m_Sock)
		Stop("Соединение с хостом потеряно");
}

void CEditorMulti::Update(CEditor *pEditor)
{
	m_pEditor = pEditor;
	if(m_Mode == MODE_OFF)
		return;
	if(!MapStillOpen())
	{
		Stop("Карта закрыта — сессия остановлена");
		return;
	}

	if(m_Mode == MODE_HOST)
		PumpHost();
	else
		PumpClient();
	if(m_Mode == MODE_OFF)
		return;
	PollMapSave();

	if(m_Mode == MODE_HOST || (m_Mode == MODE_CLIENT && m_pSyncMap))
	{
		if(Seconds(m_LastDiff) > 0.05f)
		{
			m_LastDiff = Now();
			ScanLocalChanges(pEditor);
		}
		SendCursor(pEditor);
	}

	for(auto It = m_Remotes.begin(); It != m_Remotes.end();)
	{
		if(Seconds(It->second.m_LastUpdate) > 15.0f)
			It = m_Remotes.erase(It);
		else
			++It;
	}
	for(auto &pPeer : m_vpPeers)
		FlushSend(*pPeer);
	FlushSend(m_Server);
}

// ---------------------------------------------------------------- рисование в мире

void CEditorMulti::RenderWorld(CEditor *pEditor)
{
	if(!Active() || pEditor->Map() != m_pSyncMap || !m_pSyncMap)
		return;
	IGraphics *pGraphics = pEditor->Graphics();
	ITextRender *pText = pEditor->TextRender();

	// подсветка чужих правок
	for(size_t i = 0; i < m_vFlashes.size();)
	{
		const float Age = Seconds(m_vFlashes[i].m_Time);
		if(Age > 1.6f || m_vFlashes[i].m_Group >= (int)m_pSyncMap->m_vpGroups.size())
		{
			m_vFlashes.erase(m_vFlashes.begin() + i);
			continue;
		}
		const SFlash &F = m_vFlashes[i];
		m_pSyncMap->m_vpGroups[F.m_Group]->MapScreen();
		const float Fade = 1.0f - Age / 1.6f;
		pGraphics->TextureClear();
		pGraphics->QuadsBegin();
		pGraphics->SetColor(F.m_Color.r, F.m_Color.g, F.m_Color.b, 0.28f * Fade);
		IGraphics::CQuadItem Fill(F.m_X0 * 32.0f, F.m_Y0 * 32.0f, (F.m_X1 - F.m_X0) * 32.0f, (F.m_Y1 - F.m_Y0) * 32.0f);
		pGraphics->QuadsDrawTL(&Fill, 1);
		pGraphics->SetColor(F.m_Color.r, F.m_Color.g, F.m_Color.b, 0.9f * Fade);
		const float T = 2.0f;
		const float X = F.m_X0 * 32.0f, Y = F.m_Y0 * 32.0f, Wd = (F.m_X1 - F.m_X0) * 32.0f, Ht = (F.m_Y1 - F.m_Y0) * 32.0f;
		IGraphics::CQuadItem Edges[4] = {{X, Y, Wd, T}, {X, Y + Ht - T, Wd, T}, {X, Y, T, Ht}, {X + Wd - T, Y, T, Ht}};
		pGraphics->QuadsDrawTL(Edges, 4);
		pGraphics->QuadsEnd();
		i++;
	}

	// курсоры
	m_pSyncMap->m_pGameGroup->MapScreen();
	const CScreenRect Mapping = m_pSyncMap->m_pGameGroup->Mapping();
	const float S = Mapping.Width() / pGraphics->WindowWidth();

	for(const auto &[Id, R] : m_Remotes)
	{
		if(R.m_Pos.x < -1.0e8f)
			continue;
		const vec2 P = R.m_Pos;

		pGraphics->TextureClear();
		pGraphics->QuadsBegin();
		if(R.m_Down)
		{
			pGraphics->SetColor(R.m_Color.r, R.m_Color.g, R.m_Color.b, 0.35f);
			pGraphics->DrawCircle(P.x, P.y, 14.0f * S, 24);
		}
		// чёрная подложка и цветная стрелка
		pGraphics->SetColor(0.0f, 0.0f, 0.0f, 0.85f);
		const IGraphics::CFreeformItem Back(P.x - 1.5f * S, P.y - 1.5f * S, P.x - 1.5f * S, P.y - 1.5f * S, P.x - 1.5f * S, P.y + 19.0f * S, P.x + 14.5f * S, P.y + 14.0f * S);
		pGraphics->QuadsDrawFreeform(&Back, 1);
		pGraphics->SetColor(R.m_Color.r, R.m_Color.g, R.m_Color.b, 1.0f);
		const IGraphics::CFreeformItem Arrow(P.x, P.y, P.x, P.y, P.x, P.y + 16.0f * S, P.x + 11.0f * S, P.y + 11.0f * S);
		pGraphics->QuadsDrawFreeform(&Arrow, 1);
		pGraphics->QuadsEnd();

		char aLabel[80];
		if(R.m_aInfo[0])
			str_format(aLabel, sizeof(aLabel), "%s  (%s)", R.m_aName, R.m_aInfo);
		else
			str_copy(aLabel, R.m_aName);
		const float FontSize = 13.0f * S;
		pText->TextColor(R.m_Color);
		pText->TextOutlineColor(ColorRGBA(0.0f, 0.0f, 0.0f, 0.8f));
		pText->Text(P.x + 14.0f * S, P.y + 12.0f * S, FontSize, aLabel);
		pText->TextColor(pText->DefaultTextColor());
		pText->TextOutlineColor(pText->DefaultTextOutlineColor());
	}
}

// ---------------------------------------------------------------- кнопка и окно

void CEditorMulti::DoButton(CEditor *pEditor, const CUIRect &Rect)
{
	m_pEditor = pEditor;
	static char s_ButtonId;
	char aText[48];
	if(m_Mode == MODE_OFF)
		str_copy(aText, "Мультимаппинг");
	else
		str_format(aText, sizeof(aText), "Мультимаппинг: %d", (int)m_Remotes.size() + 1);
	if(pEditor->DoButton_Editor(&s_ButtonId, aText, Active() ? 1 : 0, &Rect, BUTTONFLAG_LEFT, "Строить карту вместе с другими игроками: видны их курсоры и правки."))
	{
		pEditor->Ui()->DoPopupMenu(&m_PopupId, Rect.x, Rect.y + Rect.h, 300.0f, 268.0f, this, PopupFn);
	}
}

CUi::EPopupMenuFunctionResult CEditorMulti::PopupFn(void *pContext, CUIRect View, bool Active)
{
	return static_cast<CEditorMulti *>(pContext)->RenderPopup(View);
}

CUi::EPopupMenuFunctionResult CEditorMulti::RenderPopup(CUIRect View)
{
	CEditor *pEditor = m_pEditor;
	CUi *pUi = pEditor->Ui();
	if(!m_InputsInit)
	{
		m_InputsInit = true;
		m_PortInput.Set("8750");
	}

	CUIRect Row;
	const auto NextRow = [&](float H = 16.0f) {
		View.HSplitTop(H, &Row, &View);
		View.HSplitTop(3.0f, nullptr, &View);
	};

	NextRow(14.0f);
	pUi->DoLabel(&Row, "Мультимаппинг", 12.0f, TEXTALIGN_ML);

	NextRow(26.0f);
	pUi->DoLabel(&Row, m_aStatus, 9.0f, TEXTALIGN_TL);

	const auto DoPortRow = [&]() {
		NextRow();
		CUIRect Label, Edit;
		Row.VSplitLeft(110.0f, &Label, &Edit);
		pUi->DoLabel(&Label, "Порт (TCP)", 10.0f, TEXTALIGN_ML);
		pEditor->DoEditBox(&m_PortInput, &Edit, 10.0f);
	};
	const int Port = std::clamp(str_toint(m_PortInput.GetString()), 1, 65535);

	if(m_Mode == MODE_OFF)
	{
		DoPortRow();
		NextRow(18.0f);
		static char s_HostButton;
		if(pEditor->DoButton_Editor(&s_HostButton, "Создать сессию (я хост)", 0, &Row, BUTTONFLAG_LEFT, "Открывает эту карту для совместной работы. Друзья подключаются к вашему IP и порту."))
			StartHost(Port);

		NextRow(14.0f);
		pUi->DoLabel(&Row, "Или подключиться к хосту (ip или ip:порт):", 9.0f, TEXTALIGN_ML);
		NextRow();
		pEditor->DoEditBox(&m_AddressInput, &Row, 10.0f);
		NextRow(18.0f);
		static char s_JoinButton;
		if(pEditor->DoButton_Editor(&s_JoinButton, "Подключиться", 0, &Row, BUTTONFLAG_LEFT, "Карта хоста откроется в новой вкладке редактора."))
		{
			if(m_AddressInput.IsEmpty())
				SetStatus("Введите адрес хоста");
			else
				StartJoin(m_AddressInput.GetString(), Port);
		}

		NextRow(52.0f);
		pUi->DoLabel(&Row, "Если хост за роутером: откройте на нём TCP-порт или используйте общую виртуальную сеть (Radmin VPN, ZeroTier, playit.gg) и дайте друзьям тот адрес.", 8.5f, TEXTALIGN_TL);
	}
	else
	{
		char aLine[96];
		str_format(aLine, sizeof(aLine), "Вы: %s   (отправлено тайлов: %d, получено: %d)", pEditor->Config()->m_PlayerName, m_TilesSent, m_TilesReceived);
		NextRow(14.0f);
		pUi->DoLabel(&Row, aLine, 9.0f, TEXTALIGN_ML);

		NextRow(12.0f);
		pUi->DoLabel(&Row, "Участники:", 9.0f, TEXTALIGN_ML);
		int Shown = 0;
		for(const auto &[Id, R] : m_Remotes)
		{
			if(Shown++ >= 6)
				break;
			NextRow(13.0f);
			CUIRect Dot, Name;
			Row.VSplitLeft(12.0f, &Dot, &Name);
			Dot.Margin(3.0f, &Dot);
			Dot.Draw(R.m_Color, IGraphics::CORNER_ALL, 3.0f);
			char aName[96];
			str_format(aName, sizeof(aName), "%s%s%s  — правок: %d", R.m_aName, R.m_aInfo[0] ? "  ·  " : "", R.m_aInfo, R.m_Edits);
			pUi->DoLabel(&Name, aName, 9.0f, TEXTALIGN_ML);
		}
		if(m_Remotes.empty())
		{
			NextRow(13.0f);
			pUi->DoLabel(&Row, "пока никого, кроме вас", 9.0f, TEXTALIGN_ML);
		}

		// кнопки внизу
		CUIRect Bottom;
		View.HSplitBottom(18.0f, &View, &Bottom);
		static char s_StopButton;
		if(pEditor->DoButton_Editor(&s_StopButton, "Отключиться", 0, &Bottom, BUTTONFLAG_LEFT, "Завершить мультимаппинг. Карта останется открытой."))
		{
			Stop("Отключено");
			return CUi::POPUP_CLOSE_CURRENT;
		}
		if(m_Mode == MODE_HOST)
		{
			View.HSplitBottom(3.0f, &View, nullptr);
			View.HSplitBottom(18.0f, &View, &Bottom);
			static char s_ResendButton;
			if(pEditor->DoButton_Editor(&s_ResendButton, "Отправить карту всем заново", 0, &Bottom, BUTTONFLAG_LEFT, "Нужно, если вы изменили структуру: добавили/удалили слой или группу, изменили размер слоя."))
			{
				for(auto &pPeer : m_vpPeers)
				{
					if(!pPeer->m_Hello)
						continue;
					pPeer->m_Ready = false;
					QueueMap(pPeer->m_Id);
				}
				SetStatus("Отправляю карту заново всем участникам");
			}
		}
	}
	return CUi::POPUP_KEEP_OPEN;
}
