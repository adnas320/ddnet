#include "musicplayer.h"

#include <base/io.h>
#include <base/str.h>

#include <engine/client.h>
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
	m_Voice = Sound()->Play(CSounds::CHN_GLOBAL, m_Sample, 0, g_Config.m_ClMusicVolume / 100.0f);
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
		Next();
}
