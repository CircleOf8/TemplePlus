
#include "stdafx.h"
#include "temple_functions.h"
#include "tig/tig_msg.h"
#include "tig/tig_startup.h"
#include "tig/tig_mouse.h"
#include "tig/tig_init.h"
#include "gamesystems.h"
#include "util/fixes.h"
#include "graphics.h"
#include "tig/tig_shader.h"
#include "ui/ui.h"
#include "ui/ui_mainmenu.h"
#include "ui/ui_browser.h"
#include "movies.h"
#include "util/exception.h"
#include "util/stopwatch.h"
#include "python/pythonglobal.h"
#include "mainloop.h"

class TempleMutex {
public:
	TempleMutex() : mMutex(nullptr) {
		
	}

	~TempleMutex() {
		if (mMutex) {
			ReleaseMutex(mMutex);
		}
	}

	bool acquire() {
		mMutex = CreateMutexA(nullptr, FALSE, "TempleofElementalEvilMutex");
		if (GetLastError() == ERROR_ALREADY_EXISTS) {
			MessageBoxA(nullptr, "Temple of Elemental Evil is already running.", "Temple of Elemental Evil", MB_OK | MB_ICONERROR);
			return false;
		}
		return true;
	}
private:
	HANDLE mMutex;
};

GlobalPrimitive<bool, 0x10BDDD9C> noRandomEncounters;
GlobalPrimitive<bool, 0x10300974> msMouseZEnabled;
GlobalPrimitive<uint32_t, 0x102AF7C0> pathLimit;
GlobalPrimitive<uint32_t, 0x102AF7C4> pathTimeLimit;
GlobalPrimitive<uint32_t, 0x10BD3B6C> bufferstuffFlag;
GlobalPrimitive<uint32_t, 0x102F6A7C> bufferstuffWidth;
GlobalPrimitive<uint32_t, 0x102F6A80> bufferstuffHeight;
GlobalPrimitive<uint32_t, 0x108EDA9C> activeRngType;
GlobalPrimitive<bool, 0x108ED0D0> showDialogLineNo;
GlobalPrimitive<uint32_t, 0x10307374> scrollDistance;
GlobalPrimitive<uint32_t, 0x108254A0> mapFoggingInited;
GlobalPrimitive<uint32_t, 0x102F7778> uiOptionsSupportedModeCount;
GlobalPrimitive<uint32_t, 0x10BD3A48> startMap;

struct StartupRelevantFuncs : AddressTable {
	

	int (__cdecl *SetScreenshotKeyhandler)(TigMsgGlobalKeyCallback *callback);
	bool (__cdecl *TigWindowBufferstuffCreate)(int *bufferStuffIdx);
	void (__cdecl *TigWindowBufferstuffFree)(int bufferStuffIdx);
	void (__cdecl *RunBatchFile)(const char *filename);
	void (__cdecl *RunMainLoop)();
	int (__cdecl *TempleMain)(HINSTANCE hInstance, HINSTANCE hInstancePrev, const char *commandLine, int showCmd);

	/*
		This is here just for the editor
	*/
	int(__cdecl *MapOpenInGame)(int mapId, int a3, int a4);

	StartupRelevantFuncs() {
		rebase(SetScreenshotKeyhandler, 0x101DCB30);
		rebase(TigWindowBufferstuffCreate, 0x10113EB0);
		rebase(TigWindowBufferstuffFree, 0x101DF2C0);		
		rebase(RunBatchFile, 0x101DFF10);
		rebase(RunMainLoop, 0x100010F0);		
		rebase(TempleMain, 0x100013D0);
		
		rebase(MapOpenInGame, 0x10072A90);
	}

} startupRelevantFuncs;

static void applyGlobalConfig();
static void setMiles3dProvider();
static void addScreenshotHotkey();
static void applyGameConfig();
static bool setDefaultCursor();

class TigBufferstuffInitializer {
public:
	TigBufferstuffInitializer() {
		StopwatchReporter reporter("Game scratch buffer initialized in {}");
		logger->info("Creating game scratch buffer");
		if (!startupRelevantFuncs.TigWindowBufferstuffCreate(&mBufferIdx)) {
			throw TempleException("Unable to initialize TIG buffer");
		}
	}
	~TigBufferstuffInitializer() {
		logger->info("Freeing game scratch buffer");
		startupRelevantFuncs.TigWindowBufferstuffFree(mBufferIdx);
	}
	int bufferIdx() const {
		return mBufferIdx;
	}
private:
	int mBufferIdx = -1;
};

class GameSystemsInitializer {
public:
	GameSystemsInitializer(const TigConfig &tigConfig) {
		StopwatchReporter reporter("Game systems initialized in {}");
		logger->info("Loading game systems");

		memset(&mConfig, 0, sizeof(mConfig));
		mConfig.editor = ::config.editor ? 1 : 0;
		mConfig.width = tigConfig.width;
		mConfig.height = tigConfig.height;
		mConfig.field_10 = temple_address(0x10002530); // Callback 1
		mConfig.renderfunc = temple_address(0x10002650); // Callback 1
		mConfig.bufferstuffIdx = tigBuffer.bufferIdx();

		gameSystemFuncs.NewInit(mConfig);
		// if (!gameSystemFuncs.Init(&mConfig)) {
		//	throw TempleException("Unable to initialize game systems!");
		// }
	}
	~GameSystemsInitializer() {
		logger->info("Unloading game systems");
		gameSystemFuncs.Shutdown();
	}
	const GameSystemConf &config() const {
		return mConfig;
	}
private:
	GameSystemConf mConfig;
	TigBufferstuffInitializer tigBuffer;
};

class GameSystemsModuleInitializer {
public:
	GameSystemsModuleInitializer(const string &moduleName) {
		StopwatchReporter reporter("Game module loaded in {}");
		logger->info("Loading game module {}", moduleName);
		if (!gameSystemFuncs.LoadModule(moduleName.c_str())) {
			throw TempleException(format("Unable to load game module {}", moduleName));
		}
	}
	~GameSystemsModuleInitializer() {
		logger->info("Unloading game module");
		gameSystemFuncs.UnloadModule();
	}
};

int TempleMain(HINSTANCE hInstance, const string &commandLine) {

	if (!config.engineEnhancements) {
		temple_set<0x10307284>(800);
		temple_set<0x10307288>(600);
		if (config.skipLegal) {
			temple_set<0x102AB360>(0); // Disable legal movies
		}
		const char *cmdLine = GetCommandLineA();

		return startupRelevantFuncs.TempleMain(hInstance, nullptr, cmdLine, SW_NORMAL);
	}

	TempleMutex mutex;

	if (!mutex.acquire()) {
		return 1;
	}
	
	/*
		Write ToEE global config vars from our config
	*/
	applyGlobalConfig();

	TigInitializer tig(hInstance);

	setMiles3dProvider();
	addScreenshotHotkey();

	// It's pretty unclear what this is used for
	bufferstuffFlag = bufferstuffFlag | 0x40;
	bufferstuffWidth = tig.config().width;
	bufferstuffHeight = tig.config().height;
	
	// Hides the cursor during loading
	mouseFuncs.HideCursor();

	GameSystemsInitializer gameSystems(tig.config());

	/*
		Process options applicable after initialization of game systems
	*/
	applyGameConfig();

	if (!setDefaultCursor()) {
		return 1;
	}

	UiLoader uiLoader(gameSystems.config());

	GameSystemsModuleInitializer gameModule(config.defaultModule);

	// Python should now be initialized. Do the global hooks
	PythonGlobalExtension::installExtensions();

	// Notify the UI system that the module has been loaded
	UiModuleLoader uiModuleLoader(uiLoader);

	UiBrowser browser;
	
	if (!config.skipIntro) {
		movieFuncs.PlayMovie("movies\\introcinematic.bik", 0, 0, 0);
	}

	ui.ResizeScreen(0, video->current_width, video->current_height);

	// Show the main menu
	mouseFuncs.ShowCursor();
	if (!config.editor) {
		uiMainMenuFuncs.ShowPage(0);
	} else {
		startupRelevantFuncs.MapOpenInGame(5001, 0, 1);
	}
	temple_set<0x10BD3A68>(1); // Purpose unknown and unconfirmed, may be able to remove

	// Run console commands from "startup.txt" (working dir)
	logger->info("[Running Startup.txt]");
	startupRelevantFuncs.RunBatchFile("Startup.txt");
	logger->info("[Beginning Game]");		

	RunMainLoop(browser);
	// startupRelevantFuncs.RunMainLoop();

	return 0;
}

static void applyGlobalConfig() {
	if (config.noRandomEncounters) {
		noRandomEncounters = true;
	}
	if (config.noMsMouseZ) {
		msMouseZEnabled = false;
	}
	if (config.pathLimit) {
		pathLimit = config.pathLimit;
	}
	if (config.pathTimeLimit) {
		pathTimeLimit = config.pathTimeLimit;
	}
}

void setMiles3dProvider() {
	// TODO Investigate whether we should set a specific 3D sound provider that works best on modern systems (Windows 7+)
}

void addScreenshotHotkey() {
	TigMsgGlobalKeyCallback spec;
	spec.keycode = 0xB7; // This is a DINPUT key for the print key
	spec.callback = nullptr; // Would override the default thing (which we override elsewhere)
	startupRelevantFuncs.SetScreenshotKeyhandler(&spec);
}

void applyGameConfig() {
	// MT is the default
	switch (config.rngType) {
	case RngType::MERSENNE_TWISTER: 
		activeRngType = 0;
		break;
	case RngType::ARCANUM: 
		activeRngType = 1;
		break;
	default: 
		logger->error("Unknown RNG type specified!");
		break;
	}
	
	// TODO Dice test
	// TODO Default party support (mem location 0x10BDDD1C)
	// TODO Start map support (mem location 0x10BD3A48)
	// TODO Dialog check
	showDialogLineNo = config.showDialogLineNos;
	scrollDistance = config.scrollDistance;

	// Removes the initialized flag from the fog subsystem
	if (config.disableFogOfWar) {
		mapFoggingInited = mapFoggingInited & ~1;
	}

	/*
		This is only used for displaying the mode-dropdown in fullscreen mode,
		which we currently do not use anymore. I don't know why the main method
		sets this. It specifies the number of modes in the supported mode table 
		@ 102F76B8.
	*/
	uiOptionsSupportedModeCount = 12;

}

bool setDefaultCursor() {
	int cursorShaderId;
	auto result = shaderFuncs.GetId("art\\interface\\cursors\\MainCursor.mdf", &cursorShaderId);
	if (result) {
		logger->error("Unable to load cursor material: {}", result);
		return false;
	}

	result = mouseFuncs.SetCursor(cursorShaderId);
	if (result) {
		logger->error("Unable to set default cursor: {}", result);
		return false;
	}
	return true;
}
