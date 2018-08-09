#pragma once

#include "common.h"
#include <temple/dll.h>

struct TurnBasedSys : temple::AddressTable
{
	objHndl * turnBasedCurrentActor;
	void(__cdecl* _CloneInitiativeFromObj)(objHndl obj, objHndl sourceObj);
	GroupArray * groupInitiativeList;

	TurnBasedSys();
	objHndl turnBasedGetCurrentActor();
	void turnBasedSetCurrentActor(objHndl objHnd);
	void CloneInitiativeFromObj(objHndl obj, objHndl sourceObj);
	void TbSysNextSthg_100DF5A0(objHndl obj, int idx);
	void InitiativeListSort();
	unsigned GetInitiativeListLength();
	int InitiativeRefresh(int initiative, int initiativeNext);
	void InitiativeListNextActor(); // move initiative to next actor
	int GetInitiativeListIdx() const;

	void AddToInitiative(objHndl handle) ;
		void AddToInitiativeGroup(objHndl handle) const;
		void ArbitrateInitiativeConflicts() ;
	
	bool IsInInitiativeList(objHndl handle) const;

	void CreateInitiativeListWithParty(); // resets initiative list and fills it with the party.
	void ExecuteExitCombatScriptForInitiativeList();
	void TbCombatEnd();
};

extern TurnBasedSys tbSys;


void _turnBasedSetCurrentActor(objHndl objHnd);
objHndl __cdecl _turnBasedGetCurrentActor();