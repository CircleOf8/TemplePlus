
#include "stdafx.h"
#include "common.h"
#include "turn_based.h"
#include "util/fixes.h"
#include "description.h"
#include "party.h"
#include "combat.h"
#include "obj.h"
#include "temple_functions.h"
#include "d20_obj_registry.h"
#include "gamesystems/gamesystems.h"
#include "gamesystems/objects/objsystem.h"
#include "ai.h"
#include "condition.h"
#include "rng.h"
#include "tutorial.h"
#include "legacyscriptsystem.h"
#include "python/python_integration_obj.h"
#include "gamesystems/timeevents.h"

class TurnBasedReplacements : public TempleFix
{
public: 
	static void PortraitDragChangeInitiative(objHndl obj, int newInitiativeListIdx)
	{
		logger->debug("Changing {}'s combat initiative slot to {}", obj, newInitiativeListIdx);
		orgPortraitDragChangeInitiative(obj, newInitiativeListIdx);
	};
	static void (__cdecl*orgPortraitDragChangeInitiative)(objHndl obj, int idx);

	static void InitiativeNextActor();

	void apply() override 
	{
		logger->debug("Replacing Turn Based System functions");

		replaceFunction(0x100DEE10, _turnBasedSetCurrentActor); 
		replaceFunction(0x100DEE40, _turnBasedGetCurrentActor); 
		replaceFunction(0x100DF310, InitiativeNextActor);
		orgPortraitDragChangeInitiative = replaceFunction(0x100DF5A0, PortraitDragChangeInitiative);
		replaceFunction<void(__cdecl)(objHndl)>(0x100DF1E0, [](objHndl handle){
			tbSys.AddToInitiative(handle);
		});
		replaceFunction<void(__cdecl)()>(0x100DF450, [](){
			tbSys.CreateInitiativeListWithParty();
		});
	}
} tbReplacements;

void TurnBasedReplacements::InitiativeNextActor()
{
	tbSys.InitiativeListNextActor();
}

void(__cdecl*TurnBasedReplacements::orgPortraitDragChangeInitiative)(objHndl obj, int idx);

#pragma region Turn Based System Implementation
struct TurnBasedSys tbSys;

TurnBasedSys::TurnBasedSys()
{
	rebase(groupInitiativeList, 0x10BCAC78);
	rebase(turnBasedCurrentActor,0x10BCAD88); 
	rebase(_CloneInitiativeFromObj, 0x100DF570);
}

objHndl TurnBasedSys::turnBasedGetCurrentActor()
{
	return *turnBasedCurrentActor;
}

void TurnBasedSys::turnBasedSetCurrentActor(objHndl objHnd)
{
	*turnBasedCurrentActor = objHnd;
}

void TurnBasedSys::CloneInitiativeFromObj(objHndl obj, objHndl sourceObj)
{
	_CloneInitiativeFromObj(obj, sourceObj);
}

void TurnBasedSys::TbSysNextSthg_100DF5A0(objHndl obj, int idx)
{

}

void TurnBasedSys::InitiativeListSort()
{
	party.GroupArraySort(combatSys.groupInitiativeList);
}

unsigned TurnBasedSys::GetInitiativeListLength()
{
	return groupInitiativeList->GroupSize;
}

int TurnBasedSys::InitiativeRefresh(int initiative, int initiativeNext)
{
	auto func = temple::GetRef< int(__cdecl)(int, int)>(0x100DFE40);
	return func(initiative, initiativeNext);
}

void TurnBasedSys::InitiativeListNextActor()
{
	logger->debug("InitiativeListNextActor: Actor {} finishing.", *turnBasedCurrentActor);
	auto actorInitiative = objects.getInt32(*turnBasedCurrentActor, obj_f_initiative);
	auto actorInitiativeIdx = party.ObjFindInGroupArray(groupInitiativeList, *turnBasedCurrentActor);
	auto nextInitiativeIdx = actorInitiativeIdx + 1;
	auto initiativeListLen = GetInitiativeListLength();

	if (nextInitiativeIdx >= initiativeListLen) // time for next round
	{
		temple::GetRef<int>(0x10BCAD90) = 0; // surprise round
		for (auto i = 0u; i < GetInitiativeListLength(); i++) // refreshing the init list length in case it changes
		{
			auto combatant = groupInitiativeList->GroupMembers[i];
			auto dispatcher = objects.GetDispatcher(combatant);
			if (dispatch.dispatcherValid(dispatcher)){
				dispatch.DispatcherProcessor(dispatcher, dispTypeInitiative, 0, 0);
			}	
		}
		nextInitiativeIdx = 0;

		// If initiative hasn't already counted down to 0 (or less), make
		// that happen. Original always counted to 0 here, but that is
		// problematic.
		if (actorInitiative > 0) {
			InitiativeRefresh(actorInitiative, 0);
			actorInitiative = 0;
		}
	}
	
	objHndl actorNext = objHndl::null;
	if (nextInitiativeIdx < groupInitiativeList->GroupSize)
		actorNext = groupInitiativeList->GroupMembers[nextInitiativeIdx];
	*turnBasedCurrentActor = actorNext;
	if (actorNext)
	{
		logger->debug("InitiativeListNextActor: Turn Based actor changed to {}", actorNext);
		auto nextActorInitiative = objects.getInt32(actorNext, obj_f_initiative);
		if (actorInitiative != nextActorInitiative)
		{
			InitiativeRefresh(actorInitiative, nextActorInitiative);
		}
	} else
	{
		logger->debug("Turn Based actor changed to NULL");
	}

}

int TurnBasedSys::GetInitiativeListIdx() const
{
	return party.ObjFindInGroupArray(groupInitiativeList, *turnBasedCurrentActor );
}

void TurnBasedSys::AddToInitiativeGroup(objHndl handle) const
{
	party.ObjAddToGroupArray(groupInitiativeList, handle);
}

void TurnBasedSys::ArbitrateInitiativeConflicts()  {

	auto N = GetInitiativeListLength();
	if (N <= 1)
		return;

	for (auto i=0u; i < N; i++){
		auto combatant    = combatSys.GetInitiativeListMember(i);
		if (!combatant){
			continue;
		}
		auto combatantObj = objSystem->GetObject(combatant);
		auto initiative = combatantObj->GetInt32(obj_f_initiative);
		auto subinitiative = combatantObj->GetInt32(obj_f_subinitiative);

		for (auto j= i + 1; j < N; j++){
			auto combatant2 = combatSys.GetInitiativeListMember(j);
			if (!combatant2){
				continue;
			}
				
			auto combatantObj2 = objSystem->GetObject(combatant2);
			auto initiative2 = combatantObj2->GetInt32(obj_f_initiative);
			if (initiative != initiative2)
				break;

			auto subinitiative2 = combatantObj2->GetInt32(obj_f_subinitiative);
			if (subinitiative != subinitiative2)
				break;
			combatantObj2->SetInt32(obj_f_subinitiative, subinitiative2 - 1);
		}
	}

	/*auto arbConf = temple::GetRef<void(__cdecl)()>(0x100DEFA0);
	arbConf();*/
}

void TurnBasedSys::AddToInitiative(objHndl handle) 
{
	if (IsInInitiativeList(handle))
		return;
	
	auto obj = gameSystems->GetObj().GetObject(handle);
	if (critterSys.IsDeadNullDestroyed(handle))
		return;
	if (critterSys.IsDeadOrUnconscious(handle))
		return;

	if (!d20Sys.d20Query(handle, DK_QUE_EnterCombat))
		return;


	aiSys.AiOnInitiativeAdd(handle);

	auto initiativeRoll = rngSys.GetInt(1, 20);
	int initiativeMod = 0;
	auto dispatcher = obj->GetDispatcher();
	if (dispatcher->IsValid()){
		DispIoObjBonus dispIo;
		dispIo.bonOut = &dispIo.bonlist;
		dispatcher->Process(dispTypeInitiativeMod, DK_NONE, &dispIo);
		initiativeMod = dispIo.bonlist.GetEffectiveBonusSum();
	}

	obj->SetInt32(obj_f_initiative, initiativeMod + initiativeRoll);
	AddToInitiativeGroup(handle);
	auto dexScore = objects.StatLevelGet(handle, stat_dexterity);
	obj->SetInt32(obj_f_subinitiative, 100 * dexScore);
	ArbitrateInitiativeConflicts();
	conds.AddTo(handle, "Flatfooted", {});
	auto isSurpriseRound = temple::GetRef<BOOL>(0x10BCAD90);
	if (isSurpriseRound){
		conds.AddTo(handle, "Surprised", {});
	}

	/*auto addToInit = temple::GetRef<void(__cdecl)(objHndl)>(0x100DF1E0);
	addToInit(handle);*/
}

bool TurnBasedSys::IsInInitiativeList(objHndl handle) const   // 0x100DEDD0
{
	return party.ObjIsInGroupArray(groupInitiativeList, handle) != 0;
}

void TurnBasedSys::CreateInitiativeListWithParty(){
	groupInitiativeList->Reset();
	groupInitiativeList->sortFunc = temple::GetRef<int(__cdecl)(void*, void*)>(0x100DEF20);

	if (tutorial.IsTutorialActive() && scriptSys.GetGlobalFlag(4)){
		tutorial.ShowTopic(11);
	}

	auto N = party.GroupListGetLen();
	for (auto i=0u; i < N; i++){
		auto partyMember = party.GroupListGetMemberN(i);
		if (d20Sys.d20Query(partyMember, DK_QUE_EnterCombat)){
			AddToInitiative(partyMember);
		}
	}

	auto newTBActor = combatSys.GetInitiativeListMember(0);
	turnBasedSetCurrentActor(newTBActor);
	temple::GetRef<int>(0x10BCAD80) = objSystem->GetObject(newTBActor)->GetInt32(obj_f_initiative);
}

void TurnBasedSys::ExecuteExitCombatScriptForInitiativeList(){
	auto N = GetInitiativeListLength();
	for (auto i = 0u; i < N; i++){
		auto combatant = groupInitiativeList->GroupMembers[i];
		pythonObjIntegration.ExecuteObjectScript(combatant, combatant, ObjScriptEvent::ExitCombat);
	}
}

void TurnBasedSys::TbCombatEnd(bool isResetting){

	// Added in Temple+
	// Enforcing exiting combat mode for critters
	// Otherwise, there could be an infinite enter/exit combat loop for concealed enemy combatants:
	// 1. AI would enter combat
	// 2. AI could not approach party
	// 3. AI was concealed
	// 4. Combat ended (all unconcealed enemies far from party) - see LegacyCombatSys::AllCombatantsFarFromParty()
	// 5. AI still in combat mode, would restart combat immediately
	// 6. repeat
	if (!isResetting){
		for (auto i = 0u; i < groupInitiativeList->GroupSize; i++) {
			auto combatant = groupInitiativeList->GroupMembers[i];
			if (!combatant) continue;
			auto combatantObj = objSystem->GetObject(combatant);
			auto critterFlags = critterSys.GetCritterFlags(combatant);
			if (critterFlags & OCF_COMBAT_MODE_ACTIVE) {
				combatantObj->SetInt32(obj_f_critter_flags, critterFlags & ~OCF_COMBAT_MODE_ACTIVE);
			}
			if (combatantObj->IsNPC()) {
				auto aiFlags = static_cast<AiFlag>(combatantObj->GetInt64(obj_f_npc_ai_flags64));
				if (aiFlags & AiFlag::Fighting) {
					combatantObj->SetInt64(obj_f_npc_ai_flags64, aiFlags & ~AiFlag::Fighting);
				}
			}

		}
	}
	

	groupInitiativeList->Reset();
	*turnBasedCurrentActor = objHndl::null; // Temple+: added this reset so no invalid handles are retained after combat


	auto gameTime = gameSystems->GetTimeEvent().GetTime();
	auto &combatAbsoluteEndTimeInSeconds = temple::GetRef<int>(0x11E61538);
	combatAbsoluteEndTimeInSeconds = temple::GetRef<int(__cdecl)(GameTime&)>(0x1005FD50)(gameTime);
	if (tutorial.IsTutorialActive()){
		if (scriptSys.GetGlobalFlag(4)){
			scriptSys.SetGlobalFlag(4, 0);
			scriptSys.SetGlobalFlag(2, 1);
			tutorial.ShowTopic(18);
		}
	}
}

void _turnBasedSetCurrentActor(objHndl objHnd)
{
	if (objHnd)
		logger->debug("Turn Based Actor changed to {}", objHnd);
	tbSys.turnBasedSetCurrentActor(objHnd);
}

objHndl _turnBasedGetCurrentActor()
{
	return tbSys.turnBasedGetCurrentActor();
}
#pragma endregion

#pragma region Turn Based Function Replacements

#pragma endregion 