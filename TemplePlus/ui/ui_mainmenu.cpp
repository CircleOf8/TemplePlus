
#include "stdafx.h"

#include <QtQuick/QQuickView>
#include <QtQml/QQmlContext>

#include "ui_mainmenu.h"
#include "ui_systems.h"
#include "ui_legacysystems.h"

#include "anim.h"
#include "critter.h"
#include "party.h"
#include "fade.h"

#include "gamesystems/gamesystems.h"
#include "gamesystems/mapsystem.h"
#include "gamesystems/legacysystems.h"
#include "gamesystems/objects/objsystem.h"

#include "tig/tig_msg.h"
#include "movies.h"
#include "messages/messagequeue.h"

#include "widgets/widget_content.h"
#include "widgets/widget_doc.h"
#include "widgets/widget_styles.h"
#include "tig/tig_keyboard.h"

class ViewCinematicsDialog {
public:
	ViewCinematicsDialog();

	void Show();

private:
	std::unique_ptr<WidgetContainer> mWidget;

	WidgetScrollView *mListBox;
};

//*****************************************************************************
//* MM-UI
//*****************************************************************************

UiMM::UiMM(const UiSystemConf &config) {
	/*auto startup = temple::GetPointer<int(const UiSystemConf*)>(0x10117370);
	if (!startup(&config)) {
		throw TempleException("Unable to initialize game system MM-UI");
	}*/
	
	WidgetDoc widgetDoc(WidgetDoc::Load("templeplus/ui/main_menu.json"));
	mMainWidget = widgetDoc.TakeRootContainer();

	mViewCinematicsDialog = std::make_unique<ViewCinematicsDialog>();

	// This eats all mouse messages that reach the full-screen main menu
	mMainWidget->SetMouseMsgHandler([](auto msg) {
		return true;
	});
	mMainWidget->SetWidgetMsgHandler([](auto msg) {
		return true;
	});
	
	mMainWidget->SetKeyStateChangeHandler([this](const TigKeyStateChangeMsg &msg) {
		// Close the menu if it's the ingame menu
		if (msg.key == DIK_ESCAPE && !msg.down) {
			if (mCurrentPage == MainMenuPage::InGameNormal || mCurrentPage == MainMenuPage::InGameIronman) {
				Hide();
			}
		}
		return true;
	});

	mPagesWidget = widgetDoc.GetWindow("pages");

	mPageWidgets[MainMenuPage::MainMenu] = widgetDoc.GetWindow("page-main-menu");
	mPageWidgets[MainMenuPage::Difficulty] = widgetDoc.GetWindow("page-difficulty");
	mPageWidgets[MainMenuPage::InGameNormal] = widgetDoc.GetWindow("page-ingame-normal");
	mPageWidgets[MainMenuPage::InGameIronman] = widgetDoc.GetWindow("page-ingame-ironman");
	mPageWidgets[MainMenuPage::Options] = widgetDoc.GetWindow("page-options");
	mPageWidgets[MainMenuPage::SetPieces] = widgetDoc.GetWindow("page-set-pieces");

	// Wire up buttons on the main menu
	widgetDoc.GetButton("new-game")->SetClickHandler([this]() {
		Show(MainMenuPage::Difficulty);
	});
	widgetDoc.GetButton("load-game")->SetClickHandler([this]() {
		Hide();
		uiSystems->GetLoadGame().Show(true);
	});
	widgetDoc.GetButton("set-pieces")->SetClickHandler([this]() {
		Show(MainMenuPage::Options);
	});
	widgetDoc.GetButton("tutorial")->SetClickHandler([this]() {
		LaunchTutorial();
	});
	widgetDoc.GetButton("options")->SetClickHandler([this]() {
		Show(MainMenuPage::Options);
	});
	widgetDoc.GetButton("quit-game")->SetClickHandler([this]() {
		Message msg;
		msg.type = TigMsgType::EXIT;
		msg.arg1 = 0;
		messageQueue->Enqueue(msg);
	});

	// Wire up buttons on the difficulty selection page
	widgetDoc.GetButton("difficulty-normal")->SetClickHandler([this]() {
		gameSystems->SetIronman(false);
		Hide();
		uiSystems->GetPcCreation().Start();
	});
	widgetDoc.GetButton("difficulty-ironman")->SetClickHandler([this]() {
		gameSystems->SetIronman(true);
		Hide();
		uiSystems->GetPcCreation().Start();
	});
	widgetDoc.GetButton("difficulty-exit")->SetClickHandler([this]() {
		Show(MainMenuPage::MainMenu);
	});

	// Wire up buttons on the ingame menu (normal difficulty)
	widgetDoc.GetButton("ingame-normal-load")->SetClickHandler([this]() {
		Hide();
		uiSystems->GetLoadGame().Show(false);
	});
	widgetDoc.GetButton("ingame-normal-save")->SetClickHandler([this]() {
		Hide();
		uiSystems->GetSaveGame().Show(true);
	});
	widgetDoc.GetButton("ingame-normal-close")->SetClickHandler([this]() {
		Hide();
	});
	widgetDoc.GetButton("ingame-normal-quit")->SetClickHandler([this]() {
		Hide();
		gameSystems->ResetGame();
		uiSystems->Reset();
		Show(MainMenuPage::MainMenu);
	});

	// Wire up buttons on the ingame menu (ironman difficulty)
	widgetDoc.GetButton("ingame-ironman-close")->SetClickHandler([this]() {
		Hide();
	});
	widgetDoc.GetButton("ingame-ironman-save-quit")->SetClickHandler([this]() {
		if (gameSystems->SaveGameIronman()) {
			gameSystems->ResetGame();
			uiSystems->Reset();
			Show(MainMenuPage::MainMenu);
		}
	});

	// Wire up buttons on the ingame menu (ironman difficulty)
	widgetDoc.GetButton("options-show")->SetClickHandler([this]() {
		Hide();
		uiSystems->GetOptions().Show(true);
	});
	widgetDoc.GetButton("options-view-cinematics")->SetClickHandler([this]() {
		Hide();
		uiSystems->GetUtilityBar().Hide();
		// TODO ui_mm_msg_ui4();
		mViewCinematicsDialog->Show();
	});	
	widgetDoc.GetButton("options-credits")->SetClickHandler([this]() {
		Hide();

		static std::vector<int> creditsMovies{ 100, 110, 111, 112, 113 };
		for (auto movieId : creditsMovies) {
			movieFuncs.MovieQueueAdd(movieId);
		}		
		movieFuncs.MovieQueuePlay();

		Show(MainMenuPage::Options);
	});
	widgetDoc.GetButton("options-back")->SetClickHandler([this]() {
		Show(MainMenuPage::MainMenu);
	});

	RepositionWidgets(config.width, config.height);
}
UiMM::~UiMM() {
	//auto shutdown = temple::GetPointer<void()>(0x101164c0);
	//shutdown();
}
void UiMM::ResizeViewport(const UiResizeArgs& resizeArg) {
	/*auto resize = temple::GetPointer<void(const UiResizeArgs*)>(0x101172c0);
	resize(&resizeArg);*/

	RepositionWidgets(resizeArg.rect1.width, resizeArg.rect1.height);
}
const std::string &UiMM::GetName() const {
	static std::string name("MM-UI");
	return name;
}

bool UiMM::IsVisible() const
{
	// The main menu is defined as visible, if any of the pages is visible
	for (auto &entry : mPageWidgets) {
		if (entry.second->IsVisible()) {
			return true;
		}
	}
	return false;
}

void UiMM::Show(MainMenuPage page)
{
	// Was previously @ 0x10116500

	// In case the main menu is shown in-game, we have to take care of some business
	if (!IsVisible()) {
		if (page == MainMenuPage::InGameNormal || page == MainMenuPage::InGameIronman)
		{
			gameSystems->TakeSaveScreenshots();
			gameSystems->GetAnim().PushDisableFidget();
		}
	}

	// TODO: This seems wrong actually... This should come after hide()
	mCurrentPage = page;
	Hide();

	uiSystems->GetSaveGame().Hide();
	uiSystems->GetLoadGame().Hide();
	uiSystems->GetUtilityBar().HideOpenedWindows(false);
	uiSystems->GetChar().Hide();

	mMainWidget->Show();
	mMainWidget->BringToFront();

	auto view = uiManager->AddQmlWindow(0, 0, 800, 600, "ui/MainForm.ui.qml");
	QVariantList items;
	items << "New Game" << "Load Game" << "Quit Game";
	view->rootContext()->setContextProperty("menuItems", items);
		
	for (auto &entry : mPageWidgets) {
		entry.second->SetVisible(entry.first == page);
	}

	if (page != MainMenuPage::InGameNormal) {
		uiSystems->GetUtilityBar().Hide();
	}    
	uiSystems->GetInGame().ResetInput();
}

void UiMM::Hide()
{
	if (IsVisible()) {
		if (mCurrentPage == MainMenuPage::InGameNormal || mCurrentPage == MainMenuPage::InGameIronman) {
			gameSystems->GetAnim().PopDisableFidget();
		}
	}

	for (auto &entry : mPageWidgets) {
		entry.second->SetVisible(false);
	}
	mMainWidget->Hide();

	if (mCurrentPage == MainMenuPage::InGameNormal) {
		uiSystems->GetUtilityBar().Show();
	}
}

void UiMM::RepositionWidgets(int width, int height)
{
	// Attach the pages to the bottom of the screen
	mPagesWidget->SetY(height - mPagesWidget->GetHeight());
}

void UiMM::LaunchTutorial()
{
	auto velkorProto = objSystem->GetProtoHandle(13105);
	
	auto velkor = objSystem->CreateObject(velkorProto, locXY{480, 40});
	auto velkorObj = objSystem->GetObject(velkor);
	velkorObj->SetInt32(obj_f_pc_voice_idx, 11);
	critterSys.GenerateHp(velkor);
	party.AddToPCGroup(velkor);

	static auto spawn_velkor_equipment = temple::GetPointer<void(objHndl)>(0x1006d300);
	spawn_velkor_equipment(velkor);
	
	auto anim = objects.GetAnimHandle(velkor);
	objects.UpdateRenderHeight(velkor, *anim);
	objects.UpdateRadius(velkor, *anim);
	
	SetupTutorialMap();
	uiSystems->GetParty().UpdateAndShowMaybe();
	Hide();
	uiSystems->GetParty().Update();
}

// Was @ 10111AD0
void UiMM::SetupTutorialMap()
{	
	static auto ui_tutorial_isactive = temple::GetPointer<int()>(0x10124a10);
	static auto ui_tutorial_toggle = temple::GetPointer<int()>(0x101249e0);

	if (!ui_tutorial_isactive()) {
		ui_tutorial_toggle();
	}

	auto tutorialMap = gameSystems->GetMap().GetMapIdByType(MapType::TutorialMap);
	TransitionToMap(tutorialMap);
}

void UiMM::TransitionToMap(int mapId)
{
	FadeArgs fadeArgs;
	fadeArgs.field0 = 0;
	fadeArgs.color = 0;
	fadeArgs.field8 = 1;
	fadeArgs.transitionTime = 0;
	fadeArgs.field10 = 0;
	fade.PerformFade(fadeArgs);
	gameSystems->GetAnim().StartFidgetTimer();

	FadeAndTeleportArgs fadeTp;
	fadeTp.destLoc = gameSystems->GetMap().GetStartPos(mapId);
	fadeTp.destMap = mapId;
	fadeTp.flags = 4;
	fadeTp.somehandle = party.GetLeader();

	auto enterMovie = gameSystems->GetMap().GetEnterMovie(mapId, true);
	if (enterMovie) {
		fadeTp.flags |= 1;
		fadeTp.field20 = 0;
		fadeTp.movieId = enterMovie;
	}
	fadeTp.field48 = 1;
	fadeTp.field4c = 0xFF000000;
	fadeTp.field50 = 64;
	fadeTp.somefloat2 = 3.0;
	fadeTp.field58 = 0;
	fade.FadeAndTeleport(fadeTp);

	gameSystems->GetSoundGame().StopAll(false);
	uiSystems->GetWMapRnd().StartRandomEncounterTimer();
	gameSystems->GetAnim().PopDisableFidget();
}

ViewCinematicsDialog::ViewCinematicsDialog()
{
	WidgetDoc doc = WidgetDoc::Load("templeplus/ui/main_menu_cinematics.json");

	doc.GetButton("view")->SetClickHandler([this]() {

	});
	doc.GetButton("cancel")->SetClickHandler([this]() {
		mWidget->Hide();
		uiSystems->GetMM().Show(MainMenuPage::Options);
	});

	mListBox = doc.GetScrollView("cinematicsList");

	mWidget = std::move(doc.TakeRootContainer());
	mWidget->Hide();
}

void ViewCinematicsDialog::Show()
{
	mListBox->Clear();

	int y = 0;
	for (int i = 0; i < 100; i++) {
		auto button = std::make_unique<WidgetButton>();
		button->SetText(fmt::format("Cinematic {}", i));
		button->SetWidth(mListBox->GetInnerWidth());
		button->SetStyle(widgetButtonStyles->GetStyle("mm-cinematics-list-button"));
		button->SetY(y);
		y += button->GetHeight();
		mListBox->Add(std::move(button));
	}

	mWidget->Show();
}
