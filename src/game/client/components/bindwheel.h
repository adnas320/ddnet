#ifndef GAME_CLIENT_COMPONENTS_BINDWHEEL_H
#define GAME_CLIENT_COMPONENTS_BINDWHEEL_H

#include <base/vmath.h>

#include <engine/console.h>
#include <engine/input.h>

#include <game/client/component.h>

#include <vector>

// Wheel of your own binds like in TClient: hold the key, move the mouse to a bind and let go to run it.
class CBindWheel : public CComponent
{
public:
	enum
	{
		MAX_ENTRIES = 16,
		MAX_NAME_LENGTH = 24,
		MAX_COMMAND_LENGTH = 128,
	};

	struct CEntry
	{
		char m_aName[MAX_NAME_LENGTH];
		char m_aCommand[MAX_COMMAND_LENGTH];
	};

private:
	bool m_Active = false;
	bool m_WasActive = false;
	vec2 m_Mouse = vec2(0.0f, 0.0f);
	int m_Selected = -1;
	std::vector<CEntry> m_vEntries;

	static void ConKeyBindWheel(IConsole::IResult *pResult, void *pUserData);

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnConsoleInit() override;
	void OnInit() override;
	void OnReset() override;
	void OnRelease() override;
	void OnRender() override;
	bool OnCursorMove(float x, float y, IInput::ECursorType CursorType) override;
	bool OnInput(const IInput::CEvent &Event) override;

	bool IsActive() const { return m_Active; }
	const std::vector<CEntry> &Entries() const { return m_vEntries; }
	bool AddEntry(const char *pName, const char *pCommand);
	void RemoveEntry(int Index);
	void Save() const;
	void Load();
	// binds the open key of the wheel (KEY_UNKNOWN removes it)
	void SetOpenKey(int Key);
};

#endif
