
#include "stdafx.h"

#include <QQuickView>
#include <QQmlContext>
#include <QQuickItem>
#include <QQmlProperty>
#include <QJSValue>

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

	mView = uiManager->AddQmlWindow(0, 0, 800, 600, "ui/MainMenu/MainMenu.qml");
	mView->setResizeMode(QQuickView::SizeRootObjectToView);
	mView->resize(config.width, config.height);

	mViewCinematics = uiManager->AddQmlWindow(0, 0, 800, 600, "ui/MainMenu/ViewCinematics.qml");
	mViewCinematics->setResizeMode(QQuickView::SizeRootObjectToView);
	mViewCinematics->resize(config.width, config.height);

	auto cinView = mViewCinematics->rootObject();
	connect(cinView, SIGNAL(close()), SLOT(closeViewCinematics()));

	auto root = mView->rootObject();
	connect(root, SIGNAL(action(QString)), SLOT(action(QString)));
	
}
UiMM::~UiMM() {
	auto shutdown = temple::GetPointer<void()>(0x101164c0);
	shutdown();
}
void UiMM::ResizeViewport(const UiResizeArgs& resizeArg) {
	mView->resize(resizeArg.rect1.width, resizeArg.rect1.height);
}
const std::string &UiMM::GetName() const {
	static std::string name("MM-UI");
	return name;
}

bool UiMM::IsVisible() const
{
	return mView->isVisible();
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

	mView->show();
	// TODO: Bringt to front
	auto root = mView->rootObject();
	root->setProperty("page", (int) page);

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

	mView->hide();

	if (mCurrentPage == MainMenuPage::InGameNormal) {
		uiSystems->GetUtilityBar().Show();
	}
}

void UiMM::action(QString action) {
	if (action == "load_game") {
		Hide();
		uiSystems->GetLoadGame().Show(true); 
	} else if (action == "tutorial") {
		LaunchTutorial();
	} else if (action == "quit") {
		Message msg;
		msg.type = TigMsgType::EXIT;
		msg.arg1 = 0;
		messageQueue->Enqueue(msg);
	} else if (action == "start_game_normal") {
		gameSystems->SetIronman(false);
		Hide();
		uiSystems->GetPcCreation().Start();
	} else if (action == "start_game_ironman") {
		gameSystems->SetIronman(true);
		Hide();
		uiSystems->GetPcCreation().Start();
	} else if (action == "ingame_normal_load_game") {
		Hide();
		uiSystems->GetLoadGame().Show(false);
	} else if (action == "ingame_normal_save_game") {
		Hide();
		uiSystems->GetSaveGame().Show(true);
	} else if (action == "ingame_close") {
		Hide();
	} else if (action == "ingame_normal_quit") {
		Hide();
		gameSystems->ResetGame();
		uiSystems->Reset();
		Show(MainMenuPage::MainMenu);
	} else if (action == "ingame_ironman_save_and_quit") {
		if (gameSystems->SaveGameIronman()) {
			gameSystems->ResetGame();
			uiSystems->Reset();
			Show(MainMenuPage::MainMenu);
		}
	} else if (action == "options_show") {
		Hide();
		uiSystems->GetOptions().Show(true);
	} else if (action == "options_view_cinematics") {
		Hide();
		uiSystems->GetUtilityBar().Hide();
		// TODO ui_mm_msg_ui4();
		mViewCinematics->show();
	} else if (action == "options_credits") {
		Hide();

		static std::vector<int> creditsMovies{ 100, 110, 111, 112, 113 };
		for (auto movieId : creditsMovies) {
			movieFuncs.MovieQueueAdd(movieId);
		}
		movieFuncs.MovieQueuePlay();

		Show(MainMenuPage::Options);
	} else {
		qDebug() << "Unknown action triggered by main menu:" << action;
	}

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

void UiMM::closeViewCinematics()
{
	mViewCinematics->hide();
	Show(MainMenuPage::Options);
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

