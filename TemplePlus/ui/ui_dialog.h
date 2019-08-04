
#pragma once
#include <obj.h>
#include "ui.h"
#include "../dialog.h"

struct TigMsg;

class UiDialog {
public:



	void Hide();

	bool IsActive();

	// this seems to be mostly internal for the python based picker
	DialogState *GetCurrentDialog();
	void ReShowDialog(DialogState *info, int line);
	void Unk();

	/*
		Show an ingame popover with the portrait of the speaker and the given text message.
		Can be used outside of dialog too. 
		The NPC also turns towards the target it is speaking to.
		The voice sample with the id given in speechId is played back if it is not -1.
	*/
	void ShowTextBubble(objHndl speaker, objHndl speakingTo, const string &text, int speechId = -1);

protected:
	BOOL WidgetsInit(int w, int h);
	BOOL ResponseWidgetsInit(int w, int h); // todo

	BOOL WndMsg(int widId, TigMsg* msg); // todo
	void WndRender(int widId); // todo

	int wndId, wnd2Id;
	int responseWndId;
	int scrollbarId;

	LgcyScrollBar scrollbar;

};

extern UiDialog uiDialog;
