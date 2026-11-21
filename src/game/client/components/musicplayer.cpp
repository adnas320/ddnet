#include "musicplayer.h"

#include <base/io.h>
#include <base/str.h>

#include <engine/client.h>
#include <engine/http.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <game/client/components/sounds.h>
#include <game/client/gameclient.h>

#include <engine/external/minimp3/minimp3_ex.h>

#include <algorithm>
#include <cstdlib>

static const char *const MUSIC_FOLDER = "music";

int CMusicPlayer::ScanCallback(const char *pName, int IsDir, int StorageType, void *pUser)
{
	if(IsDir || pName[0] == '.')
		return 0;
	if(!str_endswith_nocase(pName, ".mp3") && !str_endswith_nocase(pName, ".opus") && !str_endswith_nocase(pName, ".wv"))
		return 0;
	((CMusicPlayer *)pUser)->m_vTracks.emplace_back(pName);
	return 0;
}

void CMusicPlayer::OnInit()
{
	Storage()->CreateFolder(MUSIC_FOLDER, IStorage::TYPE_SAVE);
	Refresh();
}

void CMusicPlayer::OnShutdown()
{
	Unload();
}

void CMusicPlayer::GetFolder(char *pBuffer, unsigned BufferSize) const
{
	Storage()->GetCompletePath(IStorage::TYPE_SAVE, MUSIC_FOLDER, pBuffer, BufferSize);
}

void CMusicPlayer::OpenFolder()
{
	char aPath[IO_MAX_PATH_LENGTH];
	GetFolder(aPath, sizeof(aPath));
	Client()->ViewFile(aPath);
}

void CMusicPlayer::Refresh()
{
	m_vTracks.clear();
	Storage()->ListDirectory(IStorage::TYPE_SAVE, MUSIC_FOLDER, ScanCallback, this);
	std::sort(m_vTracks.begin(), m_vTracks.end());
	// keep the playing track selected if it is still there
	if(m_Current >= (int)m_vTracks.size())
		m_Current = -1;
}

void CMusicPlayer::Unload()
{
	if(m_Sample >= 0)
	{
		Sound()->Stop(m_Sample);
		Sound()->UnloadSample(m_Sample);
	}
	m_Sample = -1;
	m_Voice = ISound::CVoiceHandle();
}

bool CMusicPlayer::LoadTrack(int Index)
{
	Unload();
	m_aError[0] = '\0';
	if(Index < 0 || Index >= (int)m_vTracks.size())
		return false;
	char aPath[IO_MAX_PATH_LENGTH];
	str_format(aPath, sizeof(aPath), "%s/%s", MUSIC_FOLDER, m_vTracks[Index].c_str());
	return LoadFile(aPath);
}

// Loads a file of the save folder into the sound engine.
bool CMusicPlayer::LoadFile(const char *pPath)
{
	char aPath[IO_MAX_PATH_LENGTH];
	str_copy(aPath, pPath);
	void *pData = nullptr;
	unsigned Size = 0;
	if(!Storage()->ReadFile(aPath, IStorage::TYPE_SAVE, &pData, &Size))
	{
		str_copy(m_aError, "Не удалось прочитать файл");
		return false;
	}
	int Sample = -1;
	if(str_endswith_nocase(aPath, ".mp3"))
	{
		mp3dec_t Decoder;
		mp3dec_file_info_t Info;
		mp3dec_init(&Decoder);
		mp3dec_load_buf(&Decoder, (const uint8_t *)pData, Size, &Info, nullptr, nullptr);
		if(Info.buffer != nullptr && Info.samples > 0 && (Info.channels == 1 || Info.channels == 2))
		{
			// the sound engine takes ownership of the buffer
			Sample = Sound()->LoadPCM((short *)Info.buffer, (int)(Info.samples / Info.channels), Info.channels, Info.hz);
		}
		else
		{
			free(Info.buffer);
		}
	}
	else if(str_endswith_nocase(aPath, ".opus"))
		Sample = Sound()->LoadOpusFromMem(pData, Size, true, aPath);
	else
		Sample = Sound()->LoadWVFromMem(pData, Size, true, aPath);
	free(pData);

	if(Sample < 0)
	{
		str_copy(m_aError, "Не удалось открыть трек (формат не поддерживается?)");
		return false;
	}
	m_Sample = Sample;
	return true;
}

void CMusicPlayer::Play(int Index)
{
	if(m_vTracks.empty())
		return;
	Index = std::clamp(Index, 0, (int)m_vTracks.size() - 1);
	m_Current = Index;
	m_Paused = false;
	m_Playing = true;
	if(!LoadTrack(Index))
	{
		m_Playing = false;
		return;
	}
	m_OnlineCurrent = -1;
	StartPlayback();
}

void CMusicPlayer::StartPlayback()
{
	m_Voice = Sound()->Play(CSounds::CHN_GLOBAL, m_Sample, 0, g_Config.m_ClMusicVolume / 100.0f);
}

// Songs with a free license (Kevin MacLeod, incompetech.com, CC BY 4.0).
static const CMusicPlayer::SOnlineTrack gs_aOnlineTracks[] = {
	{"Sneaky Snitch", "Sneaky Snitch.mp3", "https://incompetech.com/music/royalty-free/mp3-royaltyfree/Sneaky%20Snitch.mp3"},
	{"Monkeys Spinning Monkeys", "Monkeys Spinning Monkeys.mp3", "https://incompetech.com/music/royalty-free/mp3-royaltyfree/Monkeys%20Spinning%20Monkeys.mp3"},
	{"Carefree", "Carefree.mp3", "https://incompetech.com/music/royalty-free/mp3-royaltyfree/Carefree.mp3"},
	{"Fluffing a Duck", "Fluffing a Duck.mp3", "https://incompetech.com/music/royalty-free/mp3-royaltyfree/Fluffing%20a%20Duck.mp3"},
	{"Wallpaper", "Wallpaper.mp3", "https://incompetech.com/music/royalty-free/mp3-royaltyfree/Wallpaper.mp3"},
	{"Cipher", "Cipher.mp3", "https://incompetech.com/music/royalty-free/mp3-royaltyfree/Cipher.mp3"},
	{"Run Amok", "Run Amok.mp3", "https://incompetech.com/music/royalty-free/mp3-royaltyfree/Run%20Amok.mp3"},
	{"Hyperfun", "Hyperfun.mp3", "https://incompetech.com/music/royalty-free/mp3-royaltyfree/Hyperfun.mp3"},
	{"Pixel Peeker Polka (faster)", "Pixel Peeker Polka - faster.mp3", "https://incompetech.com/music/royalty-free/mp3-royaltyfree/Pixel%20Peeker%20Polka%20-%20faster.mp3"},
	{"Ice Flow", "Ice Flow.mp3", "https://incompetech.com/music/royalty-free/mp3-royaltyfree/Ice%20Flow.mp3"},
	{"Local Forecast (Elevator)", "Local Forecast - Elevator.mp3", "https://incompetech.com/music/royalty-free/mp3-royaltyfree/Local%20Forecast%20-%20Elevator.mp3"},
};

int CMusicPlayer::NumOnlineTracks()
{
	return (int)std::size(gs_aOnlineTracks);
}

const CMusicPlayer::SOnlineTrack &CMusicPlayer::OnlineTrack(int Index)
{
	return gs_aOnlineTracks[std::clamp(Index, 0, NumOnlineTracks() - 1)];
}

void CMusicPlayer::PlayOnline(int Index)
{
	if(Index < 0 || Index >= NumOnlineTracks() || m_pDownload)
		return;
	const SOnlineTrack &Track = OnlineTrack(Index);
	Storage()->CreateFolder("music/online", IStorage::TYPE_SAVE);
	str_format(m_aDownloadPath, sizeof(m_aDownloadPath), "music/online/%s", Track.m_pFile);
	m_aError[0] = '\0';
	m_DownloadIndex = Index;

	// already downloaded: play it right away
	IOHANDLE File = Storage()->OpenFile(m_aDownloadPath, IOFLAG_READ, IStorage::TYPE_SAVE);
	if(File)
	{
		io_close(File);
		Unload();
		if(LoadFile(m_aDownloadPath))
		{
			m_Current = -1;
			m_OnlineCurrent = Index;
			m_Playing = true;
			m_Paused = false;
			StartPlayback();
		}
		m_DownloadIndex = -1;
		return;
	}

	str_format(m_aDownloadTmp, sizeof(m_aDownloadTmp), "%s.tmp", m_aDownloadPath);
	m_pDownload = HttpGetFile(Track.m_pUrl, Storage(), m_aDownloadTmp, IStorage::TYPE_SAVE);
	m_pDownload->MaxResponseSize(60 * 1024 * 1024);
	Http()->Run(m_pDownload);
}

void CMusicPlayer::TogglePause()
{
	if(m_Sample < 0)
	{
		Play(m_Current >= 0 ? m_Current : 0);
		return;
	}
	if(m_Paused)
	{
		m_Paused = false;
		m_Playing = true;
		m_Voice = Sound()->Play(CSounds::CHN_GLOBAL, m_Sample, 0, g_Config.m_ClMusicVolume / 100.0f);
	}
	else
	{
		m_Paused = true;
		Sound()->Pause(m_Sample);
	}
}

void CMusicPlayer::Next()
{
	if(m_vTracks.empty())
		return;
	int Index = m_Current + 1;
	if(g_Config.m_ClMusicShuffle && m_vTracks.size() > 1)
	{
		do
			Index = rand() % (int)m_vTracks.size();
		while(Index == m_Current);
	}
	Play(Index % (int)m_vTracks.size());
}

void CMusicPlayer::Previous()
{
	if(m_vTracks.empty())
		return;
	Play(m_Current <= 0 ? (int)m_vTracks.size() - 1 : m_Current - 1);
}

void CMusicPlayer::OnRender()
{
	if(m_pDownload && m_pDownload->State() != EHttpState::QUEUED && m_pDownload->State() != EHttpState::RUNNING)
	{
		const bool Done = m_pDownload->State() == EHttpState::DONE;
		const int Index = m_DownloadIndex;
		m_pDownload = nullptr;
		m_DownloadIndex = -1;
		if(Done)
		{
			Storage()->RenameFile(m_aDownloadTmp, m_aDownloadPath, IStorage::TYPE_SAVE);
			PlayOnline(Index);
		}
		else
		{
			Storage()->RemoveFile(m_aDownloadTmp, IStorage::TYPE_SAVE);
			str_copy(m_aError, "Не удалось скачать песню (нет интернета или сайт недоступен)");
		}
	}

	if(!m_AutoStarted)
	{
		m_AutoStarted = true;
		if(g_Config.m_ClMusicAutoplay && !m_vTracks.empty())
			Next();
		return;
	}
	if(m_Sample < 0 || !m_Playing || m_Paused)
		return;
	if(m_Voice.IsValid())
		Sound()->SetVoiceVolume(m_Voice, g_Config.m_ClMusicVolume / 100.0f);
	// the track ended: next one
	if(!Sound()->IsPlaying(m_Sample))
	{
		if(m_OnlineCurrent >= 0)
			PlayOnline((m_OnlineCurrent + 1) % NumOnlineTracks());
		else
			Next();
	}
}
