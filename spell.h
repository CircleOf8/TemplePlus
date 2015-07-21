#pragma once

#include "stdafx.h"
#include "common.h"
#include "idxtables.h"
#include "tig\tig_mes.h"


struct PickerArgs;

#define MAX_SPELLS_KNOWN 384

#pragma region Spell Structs

struct SpellEntryLevelSpec
{
	uint32_t classCode;
	uint32_t slotLevel;
};

enum SpellRangeType : uint32_t;
enum Domain;

struct SpellEntry
{
	uint32_t spellEnum;
	uint32_t spellSchoolEnum;
	uint32_t spellSubSchoolEnum;
	uint32_t spellDescriptorBitmask;
	uint32_t spellComponentBitmask;
	uint32_t costGP;
	uint32_t costXP;
	uint32_t castingTimeType;
	SpellRangeType spellRangeType;
	uint32_t spellRange;
	uint32_t savingThrowType;
	uint32_t spellResistanceCode;
	SpellEntryLevelSpec spellLvls[10];
	uint32_t spellLvlsNum;
	uint32_t projectileFlag;
	uint64_t flagsTargetBitmask;
	uint64_t incFlagsTargetBitmask;
	uint64_t excFlagsTargetBitmask;
	uint64_t modeTargetSemiBitmask;
	uint32_t minTarget;
	uint32_t maxTarget;
	uint32_t radiusTarget; //note:	if it's negative, it means it's an index(up to - 7); if it's positive, it's a specified number(in feet ? )
	float_t degreesTarget;
	uint32_t aiTypeBitmask;
	uint32_t pad;
};

const uint32_t TestSizeOfSpellEntry = sizeof(SpellEntry); // should be 0xC0  ( 192 )

struct SpellPacketBody
{
	uint32_t spellEnum;
	uint32_t spellEnumOriginal; // used for spontaneous casting in order to debit the "original" spell
	uint32_t flagSthg;
	void * pSthg;
	objHndl objHndCaster;
	uint32_t casterPartsysId;
	uint32_t casterClassCode;
	uint32_t spellKnownSlotLevel;
	uint32_t baseCasterLevel;
	uint32_t spellDC;
	uint32_t unknowns[515];
	uint32_t targetListNumItemsCopy;
	uint32_t targetListNumItems;
	objHndl targetListHandles[32];
	uint32_t targetListPartsysIds[32];
	uint32_t numProjectiles;
	uint32_t field_9C4;
	uint32_t field_9C8;
	uint32_t field_9CC;
	SpellPacketBody * spellPktBods[8];
	LocFull locFull;
	uint32_t field_A04;
	SpellEntry spellEntry;
	uint32_t spellDuration;
	int field_ACC;
	uint32_t spellRange;
	uint32_t field_AD4;
	uint32_t field_AD8_maybe_itemSpellLevel;
	uint32_t metaMagicData;
	uint32_t spellId;
	uint32_t field_AE4;
};

const uint32_t TestSizeOfSpellPacketBody = sizeof(SpellPacketBody); // should be 0xAE8  (2792)

struct SpellPacket
{
	uint32_t key;
	uint32_t isActive;
	SpellPacketBody spellPktBody;
};

const uint32_t TestSizeOfSpellPacket = sizeof(SpellPacket); // should be 0xAF0  (2800)

#pragma endregion

struct CondStruct;

struct SpellSystem : AddressTable
{
	IdxTable<SpellPacket> * spellCastIdxTable;
	
	uint32_t spellRegistryCopy(uint32_t spellEnum, SpellEntry* spellEntry);
	uint32_t ConfigSpellTargetting(PickerArgs* pickerArgs, SpellPacketBody* spellPacketBody);
	MesHandle * spellEnumMesHandle;

	uint32_t getBaseSpellCountByClassLvl(uint32_t classCode, uint32_t classLvl, uint32_t slotLvl, uint32_t unknown1);
	uint32_t getWizSchool(objHndl objHnd);
	uint32_t getStatModBonusSpellCount(objHndl objHnd, uint32_t classCode, uint32_t slotLvl);
	void spellPacketBodyReset(SpellPacketBody * spellPktBody);
	void spellPacketSetCasterLevel(SpellPacketBody * spellPktBody);
	uint32_t getSpellEnum(const char* spellName);
	uint32_t GetSpellEnumFromSpellId(uint32_t spellId);
	uint32_t GetSpellPacketBody(uint32_t spellId, SpellPacketBody* spellPktBodyOut);
	void UpdateSpellPacket(const SpellPacketBody &spellPktBody);
	uint32_t spellKnownQueryGetData(objHndl objHnd, uint32_t spellEnum, uint32_t* classCodesOut, uint32_t* slotLevelsOut, uint32_t* count);
	uint32_t spellCanCast(objHndl objHnd, uint32_t spellEnum, uint32_t spellClassCode, uint32_t spellLevel);
	uint32_t spellMemorizedQueryGetData(objHndl objHnd, uint32_t spellEnum, uint32_t* classCodesOut, uint32_t* slotLevelsOut, uint32_t* count);
	bool numSpellsKnownTooHigh(objHndl objHnd);
	bool numSpellsMemorizedTooHigh(objHndl objHnd);
	bool isDomainSpell(uint32_t spellClassCode);
	uint32_t pickerArgsFromSpellEntry(SpellEntry * spellEntry, PickerArgs * pickArgs, objHndl objHnd, uint32_t casterLevel);

	void (__cdecl *SpellEnd)(int, int);
	void (__cdecl *SpellRemove)(int);

	CondStruct *GetCondFromSpellIdx(int id);
	uint32_t(__cdecl * spellRemoveFromStorage)(objHndl objHnd, obj_f fieldIdx, SpellStoreData * spellData, int unknown);
	uint32_t (__cdecl * spellsPendingToMemorized)(objHndl objHnd);
	int (__cdecl *SpellKnownAdd)(objHndl ObjHnd, int nSpellIdx, int nSpellClassCode, int nSpellCasterLevel, int nSpellStoreData, int nMetamagicData);
	int (__cdecl *SpellMemorizedAdd)(objHndl ObjHnd, int nSpellIdx, int nSpellClassCode, int nSpellCasterLevel, int nSpellStoreData, int nMetamagicData);
	void ForgetMemorized(objHndl handle);
	SpellSystem()
	{
		rebase(spellCastIdxTable, 0x10AAF218);
		macRebase(spellEnumMesHandle, 10AAF210)

		rebase(SpellEnd, 0x10079980);
		rebase(SpellRemove, 0x10079A20);
		rebase(_getSpellCountByClassLvl, 0x100F4D10);
		rebase(_getStatModBonusSpellCount, 0x100F4C30);
		rebase(spellRemoveFromStorage, 0x100758A0);
		rebase(spellsPendingToMemorized, 0x100757D0);
		macRebase(_spellPacketBodyReset, 1008A350)
		macRebase(_spellPacketSetCasterLevel, 10079B70)
		rebase(SpellKnownAdd, 0x10079EE0);
		rebase(SpellMemorizedAdd, 0x10075A10);
		macRebase(_pickerArgsFromSpellEntry, 100772A0)
	}
private:

	uint32_t(__cdecl * _getSpellCountByClassLvl)();
	uint32_t(__cdecl* _getStatModBonusSpellCount)();
	void(__cdecl * _spellPacketBodyReset)(SpellPacketBody * spellPktBody);
	void(__cdecl * _spellPacketSetCasterLevel)(SpellPacketBody * spellPktBody);
	uint32_t(__cdecl * _pickerArgsFromSpellEntry)(SpellEntry * spellEntry, PickerArgs * pickArgs, objHndl objHnd, uint32_t casterLevel);
	//uint32_t(__cdecl * Spell_Range_Sthg_From_SpellEntry_sub_100772A0)(SpellEntry *spellEntry, );
};

extern SpellSystem spellSys;

extern IdxTableWrapper<SpellEntry> spellEntryRegistry;
extern IdxTableWrapper<SpellPacket> spellsCastRegistry;


struct SpontCastSpellLists : AddressTable
{
public:
	uint32_t spontCastSpellsDruid[11];
	uint32_t spontCastSpellsEvilCleric[11];
	uint32_t spontCastSpellsGoodCleric[11];
	uint32_t spontCastSpellsDruidSummons[11];
	SpontCastSpellLists()
	{
		int _spontCastSpellsDruid[] = { -1, 476, 477, 478, 479, 480, 481, 482, 483, 484, 4000 };
		int _spontCastSpellsEvilCleric[] = { 248, 247, 249, 250, 246, 61, 581, 582, 583, 583, 0 };
		int _spontCastSpellsGoodCleric[] = { 91, 90, 92, 93, 89, 221, 577, 578, 579, 579, 0 };
		int _spontCastSpellsDruidSummons[] = { -1, 2000, 2100, 2200, 2300, 2400, 2500, 2600, 2700, 2800, 0 };
		memcpy(spontCastSpellsDruid, _spontCastSpellsDruid, 11 * sizeof(int));
		memcpy(spontCastSpellsEvilCleric, _spontCastSpellsEvilCleric, 11 * sizeof(int));
		memcpy(spontCastSpellsGoodCleric, _spontCastSpellsGoodCleric, 11 * sizeof(int));
		memcpy(spontCastSpellsDruidSummons, _spontCastSpellsDruidSummons, 11 * sizeof(int));
	}
};

extern SpontCastSpellLists spontCastSpellLists;



uint32_t _DruidRadialSelectSummons(uint32_t spellSlotLevel);
void DruidRadialSelectSummonsHook();
uint32_t _DruidRadialSpontCastSpellEnumHook(uint32_t spellSlotLevel);
void DruidRadialSpontCastSpellEnumHook();
uint32_t _GoodClericRadialSpontCastSpellEnumHook(uint32_t spellSlotLevel);
uint32_t _EvilClericRadialSpontCastSpellEnumHook(uint32_t spellSlotLevel);
void EvilClericRadialSpontCastSpellEnumHook();
void GoodClericRadialSpontCastSpellEnumHook();

const uint32_t TestSizeOfSpellStoreData = sizeof(SpellStoreData);

const uint32_t TestSizeOfMetaMagicData = sizeof(MetaMagicData);
const uint32_t TestSizeOfSpellStoreType = sizeof(SpellStoreType);
const uint32_t TestSizeOfSpellStoreState = sizeof(SpellStoreState);

//const uint32_t bbb = sizeof(int32_t);


uint32_t _getWizSchool(objHndl objHnd);
uint32_t _getSpellEnum(const char * spellName);
uint32_t _spellKnownQueryGetData(objHndl objHnd, uint32_t spellEnum, uint32_t * classCodesOut, uint32_t *slotLevelsOut, uint32_t * count);
uint32_t _spellMemorizedQueryGetData(objHndl objHnd, uint32_t spellEnum, uint32_t * classCodesOut, uint32_t *slotLevelsOut, uint32_t * count);
uint32_t _spellCanCast(objHndl objHnd, uint32_t spellEnum, uint32_t spellClassCode, uint32_t spellLevel);
uint32_t __cdecl _spellRegistryCopy(uint32_t spellEnum, SpellEntry * spellEntry);
uint32_t _GetSpellEnumFromSpellId(uint32_t spellId);
uint32_t _GetSpellPacketBody(uint32_t spellId, SpellPacketBody * spellPktBodyOut);






enum SpellRangeType : uint32_t
{
	SRT_Specified = 0,
		SRT_Personal = 1,
		SRT_Touch,
		SRT_Close,
		SRT_Medium,
		SRT_Long,
		SRT_Unlimited,
		SRT_Special_Inivibility_Purge
};




enum Domain
{
	Domain_None = 0,
		Domain_Air = 1,
		Domain_Animal = 2,
		Domain_Chaos = 3,
		Domain_Death = 4,
		Domain_Destruction = 5,
		Domain_Earth = 6,
		Domain_Evil = 7,
		Domain_Fire = 8,
		Domain_Good = 9,
		Domain_Healing = 10,
		Domain_Knowledge = 11,
		Domain_Law = 12,
		Domain_Luck = 13,
		Domain_Magic = 14,
		Domain_Plant = 15,
		Domain_Protection = 16,
		Domain_Strength = 17,
		Domain_Sun = 18,
		Domain_Travel = 19,
		Domain_Trickery = 20,
		Domain_War = 21,
		Domain_Water = 22,
		Domain_Special = 23
};
