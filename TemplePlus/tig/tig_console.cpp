
#include "stdafx.h"
#include "tig_console.h"
#include "debugui.h"
#include "party.h"
#include "d20_level.h"
#include "gamesystems/gamesystems.h"
#include "gamesystems/objects/objsystem.h"
#include "critter.h"

#include "ui/ui_debug.h"
#include "combat.h"
#include "action_sequence.h"

Console::Console() : mLog(1024), mCommandHistory(100), mCommandBuf(1024, '\0') {
}

Console::~Console() {
}

void Console::Render()
{
	UIRenderDebug();

	if (!mOpen) {
		return;
	}

	constexpr auto consoleWidgeFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;

	auto size = ImGui::GetIO().DisplaySize;
	size.y = max(300.0f, size.y*0.4f);
	ImGui::SetNextWindowSize(size);
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	if (!ImGui::Begin("Console", &mOpen, consoleWidgeFlags))
	{
		ImGui::End();
		return;
	}

	RenderCheatsMenu();
	
	ImGui::BeginChild("ScrollingRegion", ImVec2(0, -ImGui::GetItemsLineHeightWithSpacing()), false, ImGuiWindowFlags_HorizontalScrollbar);
	if (ImGui::BeginPopupContextWindow())
	{
		if (ImGui::Selectable("Clear")) Clear();
		ImGui::EndPopup();
	}

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing

	for (auto it = mLog.begin(); it != mLog.end(); it++)
	{
		auto item = it->c_str();
		ImVec4 col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // A better implementation may store a type per-item. For the sample let's just parse the text.
		if (strstr(item, "[error]")) col = ImColor(1.0f, 0.4f, 0.4f, 1.0f);
		else if (strncmp(item, "# ", 2) == 0) col = ImColor(1.0f, 0.78f, 0.58f, 1.0f);
		ImGui::PushStyleColor(ImGuiCol_Text, col);
		ImGui::TextUnformatted(item);
		ImGui::PopStyleColor();
	}

	if (mScrollToBottom)
		ImGui::SetScrollHere();
	mScrollToBottom = false;
	ImGui::PopStyleVar();
	ImGui::EndChild();
	ImGui::Separator();


	ImGui::PushItemWidth(12);
	if (ImGui::Button("X")){
		ImGui::End();
		Hide();
		return;
	}
	ImGui::SameLine();
	ImGui::PushItemWidth(-1);
	if (ImGui::InputText("Input", &mCommandBuf[0], mCommandBuf.size(), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory, Console::CommandEditCallback, this))
	{
		std::string command = mCommandBuf;
		command.resize(command.length()); // This discards trailing null-bytes
		trim(command);
		Execute(command);

		std::fill(mCommandBuf.begin(), mCommandBuf.end(), '\0'); // Clear command buffer

		// Refocus the control
		ImGui::SetKeyboardFocusHere(-1);
	}

	ImGui::End();
}

void Console::Clear()
{
	mLog.clear();
}

void Console::Execute(const std::string &command, bool skipHistory)
{
	if (!skipHistory) {
		// Remove it from the history first
		auto it = eastl::find(mCommandHistory.begin(), mCommandHistory.end(), command);
		if (it != mCommandHistory.end()) {
			mCommandHistory.erase(it);
		}

		// Append it at the end
		mCommandHistory.push_back(command);
		
		// Reset cur pos within the list
		mCommandHistoryPos = -1;
	}

	if (!ExecuteCheat(command)) {
		ExecuteScript(command);
	}

}

void Console::RunBatchFile(const std::string & path)
{
}

void Console::Show()
{
	mOpen = true;
}

void Console::Toggle()
{
	mOpen = !mOpen;
}

void Console::Hide()
{
	mOpen = false;
}

void Console::Append(const char * text)
{
	std::string line(text);
	rtrim(line);
	mLog.push_back(line);
	ScrollToBottom();
}

void Console::SetCheats(Cheat * cheats, size_t count)
{
	mCheats.resize(count);
	for (size_t i = 0; i < count; i++) {
		mCheats[i] = cheats[i];
	}
}

void Console::SetCommandInterpreter(std::function<void(const std::string&)> interpreter)
{
	mInterpreter = interpreter;
}

bool Console::ExecuteCheat(const std::string & command)
{
	for (auto &cheat : mCheats) {

		if (!cheat.handler) {
			continue; // Dont bother if it doesn't have a handler
		}

		int nameLen = strlen(cheat.name);
		// This is equivalent to a case insensitive "startsWith"
		if (!_strnicmp(&command[0], cheat.name, nameLen)) {
			if (cheat.handler(command.c_str()) == 1) {
				return true;
			}
		}
	}

	return false;
}

void Console::ExecuteScript(const std::string & command)
{
	if (mInterpreter) {
		mInterpreter(command);
	}
}

void Console::RenderCheatsMenu()
{
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("Cheats"))
		{
			if (ImGui::MenuItem("Level Up")) {
				for (auto i = 0u; i < party.GroupListGetLen(); i++) {
					auto handle = party.GroupListGetMemberN(i);
					if (!party.ObjIsAIFollower(handle) && !d20LevelSys.CanLevelup(handle)) {
						auto obj = gameSystems->GetObj().GetObject(handle);

						auto curLvl = objects.StatLevelGet(handle, stat_level);
						auto xpReq = d20LevelSys.GetXpRequireForLevel(curLvl + 1);

						auto curXp = obj->GetInt32(obj_f_critter_experience);
						if ((int)xpReq > curXp)
							critterSys.AwardXp(handle, xpReq - curXp);
					}
				}
			}

			if (combatSys.isCombatActive()){
				ImGui::PushItemWidth(100);
				if (ImGui::MenuItem("Refresh Turn")) {
					auto curSeq = *actSeqSys.actSeqCur;
					if (curSeq){
						curSeq->tbStatus.hourglassState = 4;
						curSeq->tbStatus.surplusMoveDistance = 0;
						curSeq->tbStatus.tbsFlags = 0;
					}
					
				}
			}
			


			static char cheatsInput[256] = { 0, };
			static std::string cheatsInputDescr;
			auto editCheatInputCallback = [](ImGuiTextEditCallbackData *self) -> int {
				int protoNum = 5001; // DO NOT USE arrow
				sscanf(cheatsInput, "%d", &protoNum);
				if (!strlen(cheatsInput))
					return 0;
				if (protoNum <= 1000 || protoNum >= 20000)
					return 0;
				auto protoHandle = gameSystems->GetObj().GetProtoHandle(protoNum);
				if (!protoHandle)
					return 0;

				cheatsInputDescr.clear();
				auto desc = description.getDisplayName(protoHandle);
				if (desc) {
					cheatsInputDescr = desc;
				}
				return 0;
			};
			ImGui::PushItemWidth(100);
			if (ImGui::InputText("##giveProtoId", &cheatsInput[0], 256, ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory, editCheatInputCallback, (void*)this))
			{
				editCheatInputCallback(nullptr);
			}

			ImGui::SameLine();
			if (ImGui::Button("Give Item")) {
				int protoNum = 5001; // DO NOT USE arrow
				sscanf(cheatsInput, "%d", &protoNum);

				auto protoValid = true;
				if (protoNum < 1000 || protoNum > 20000) {
					logger->warn("Invalid proto for give command: {}", protoNum);
					protoValid = false;
				}

				auto protoHandle = gameSystems->GetObj().GetProtoHandle(protoNum);
				if (!protoHandle) {
					protoValid = false;
				}

				auto leader = party.GetConsciousPartyLeader();
				auto loc = objects.GetLocation(leader);

				if (protoValid) {
					auto handleNew = gameSystems->GetObj().CreateObject(protoHandle, loc);
					if (handleNew) {
						if (!inventory.SetItemParent(handleNew, leader, ItemInsertFlags::IIF_None)) {
							objects.Destroy(handleNew);
						} else {
							auto obj = gameSystems->GetObj().GetObject(handleNew);
							obj->SetItemFlag(OIF_IDENTIFIED, 1);
						}

					}
				}
			}

			ImGui::SameLine();
			if (ImGui::Button("Create")) {
				int protoNum = 5001; // DO NOT USE arrow
				sscanf(cheatsInput, "%d", &protoNum);
				auto protoHandle = gameSystems->GetObj().GetProtoHandle(protoNum);
				if (protoHandle) {

					auto leader = party.GetConsciousPartyLeader();
					auto loc = objects.GetLocation(leader);

					auto handleNew = gameSystems->GetObj().CreateObject(protoHandle, loc);
					if (handleNew) {
						critterSys.GenerateHp(handleNew);
					}
					auto& consoleNewlyCreatedObj = temple::GetRef<objHndl>(0x10AA31B8);
					consoleNewlyCreatedObj = handleNew;
				}
			}

			if (!cheatsInputDescr.empty()) {
				ImGui::SameLine();
				ImGui::Text(cheatsInputDescr.c_str());
			}

			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Debug")) {
			if (ImGui::MenuItem("UI System")) {
				UIShowDebug();
			}
			
			static char debugAddrText[10] = { 0, };
			static int debugAddr;
			static int debugValue = 0;
			if (ImGui::Button("Debug Memory Value")){
				if (debugAddr >= 0x10000000 && debugAddr <= 0x20000000){
					debugValue = temple::GetRef<int>(debugAddr);
				}
			}
			if (ImGui::InputText("##debugMemoryAddr", debugAddrText, 10)){
				debugAddr = strtol(debugAddrText, NULL, 16);
			}

			if (debugAddr >= 0x10000000 && debugAddr <= 0x20000000){
				ImGui::Text(fmt::format("{}", debugValue).c_str());
			}
			
			

			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

}

int Console::CommandEditCallback(ImGuiTextEditCallbackData * data)
{
	Console *console = (Console*)data->UserData;
	if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory)
	{
		// Example of HISTORY
		const int prev_history_pos = console->mCommandHistoryPos;
		if (data->EventKey == ImGuiKey_UpArrow)
		{
			if (console->mCommandHistoryPos == -1)
				console->mCommandHistoryPos = console->mCommandHistory.size() - 1;
			else if (console->mCommandHistoryPos > 0)
				console->mCommandHistoryPos--;
		}
		else if (data->EventKey == ImGuiKey_DownArrow)
		{
			if (console->mCommandHistoryPos != -1)
				if (++console->mCommandHistoryPos >= (int) console->mCommandHistory.size())
					console->mCommandHistoryPos = -1;
		}

		// A better implementation would preserve the data on the current input line along with cursor position.
		if (prev_history_pos != console->mCommandHistoryPos)
		{
			data->CursorPos = data->SelectionStart = data->SelectionEnd = data->BufTextLen = (int)snprintf(data->Buf, (size_t)data->BufSize, "%s", (console->mCommandHistoryPos >= 0) ? console->mCommandHistory[console->mCommandHistoryPos].c_str() : "");
			data->BufDirty = true;
		}
	}
	return 0;
}
