#ifndef GAME_CLIENT_COMPONENTS_MUSICPLAYER_H
#define GAME_CLIENT_COMPONENTS_MUSICPLAYER_H

#include <engine/sound.h>

#include <game/client/component.h>

#include <string>
#include <vector>

// Plays your own music files (mp3, opus, wv) from the "music" folder while you play.
class CMusicPlayer : public CComponent
{
	std::vector<std::string> m_vTracks;
	int m_Current = -1;
	int m_Sample = -1;
	ISound::CVoiceHandle m_Voice;
	bool m_Playing = false; // the user wants music
	bool m_Paused = false;
	bool m_AutoStarted = false;
	char m_aError[128] = "";

	void Unload();
	bool LoadTrack(int Index);
	static int ScanCallback(const char *pName, int IsDir, int StorageType, void *pUser);

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnRender() override;
	void OnShutdown() override;

	void Refresh();
	void Play(int Index);
	void TogglePause();
	void Next();
	void Previous();

	const std::vector<std::string> &Tracks() const { return m_vTracks; }
	int Current() const { return m_Current; }
	bool IsPlaying() const { return m_Playing && !m_Paused; }
	const char *Error() const { return m_aError; }
	void GetFolder(char *pBuffer, unsigned BufferSize) const;
	void OpenFolder();
};

#endif
