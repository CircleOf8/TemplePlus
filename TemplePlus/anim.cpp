#include "stdafx.h"
#include "anim.h"
#include "util/fixes.h"
#include "temple_functions.h"
#include "gamesystems/timeevents.h"
#include "gamesystems/gamesystems.h"
#include "gamesystems/objects/objsystem.h"
#include "config/config.h"
#include "obj.h"
#include "critter.h"
#include "pathfinding.h"
#include "dice.h"
#include "util/folderutils.h"

#include <map>
#include <set>
#include <fstream>
#include "python/pythonglobal.h"
#include "python/python_debug.h"
#include "python/python_integration_spells.h"
#include "party.h"
#include "action_sequence.h"
#include <temple/meshes.h>

#include <infrastructure/json11.hpp>
#include "combat.h"
#include "location.h"
#include "gamesystems/legacysystems.h"

#define ANIM_RUN_SLOT_CAP 512

#pragma pack(push, 1)

// Has to be 0x10 in size
union AnimParam {
  objHndl obj;
  LocAndOffsets location;
  int number;
  int spellId;
  float floatNum;
};

struct AnimSlotGoalStackEntry {
  AnimGoalType goalType;
  int unk1;
  AnimParam self; // 0x8
  AnimParam target; // 0x18
  AnimParam block; // 0x28
  AnimParam scratch; //0x38
  AnimParam parent; // 0x48
  AnimParam targetTile; //0x58
  AnimParam range; //0x68
  AnimParam animId; //0x78
  AnimParam animIdPrevious; // 0x88
  AnimParam animData; // 0x98
  AnimParam spellData; // 0xA8
  AnimParam skillData; // 0xB8
  AnimParam flagsData; // 0xC8
  AnimParam scratchVal1; // 0xD8
  AnimParam scratchVal2; //0xE8
  AnimParam scratchVal3;
  AnimParam scratchVal4;
  AnimParam scratchVal5;
  AnimParam scratchVal6;
  AnimParam soundHandle;
  uint32_t soundStreamId;
  uint32_t soundStreamId2; // Probably padding
  uint32_t padding[2];
  TimeEventObjInfo selfTracking;
  TimeEventObjInfo targetTracking;
  TimeEventObjInfo blockTracking;
  TimeEventObjInfo scratchTracking;
  TimeEventObjInfo parentTracking;

  BOOL InitWithInterrupt(objHndl obj, AnimGoalType goalType);
  BOOL Push(AnimSlotId* idNew);
  BOOL Init(objHndl handle, AnimGoalType, BOOL withInterrupt = 0);

  AnimSlotGoalStackEntry(objHndl handle, AnimGoalType, BOOL withInterrupt);
  AnimSlotGoalStackEntry() { memset(this, 0, sizeof(AnimSlotGoalStackEntry)); };
};

const auto testSizeofTimeEventObjInfo = sizeof(TimeEventObjInfo); // should be 40
const auto TestSizeOfAnimSlotGoalStackEntry = sizeof(AnimSlotGoalStackEntry); // should be 544 (0x220)

struct AnimPath
{
	int flags;
	int8_t deltas[200]; // xy delta pairs describing deltas for drawing a line in screenspace
	int range;
	int fieldD0;
	int fieldD4;
	int fieldD8;
	int fieldDC;
	int pathLength;
	int fieldE4;
	locXY objLoc;
	locXY tgtLoc;
};

const int testSizeofAnimPath = sizeof(AnimPath); //should be 248 (0xF8)

struct LineRasterPacket{
	int counter; 
	int interval; 
	int deltaIdx;
	int unused;
	int64_t x;
	int64_t y;
	int8_t * deltaXY;

	LineRasterPacket(){
		counter = 0;
		interval = 10;
		deltaIdx = 0;
		x = 0i64;
		y = 0i64;
		deltaXY = nullptr;
		
	}
};

struct AnimSlot {
  AnimSlotId id;
  int flags; // See AnimSlotFlag
  int currentState;
  int field_14;
  GameTime nextTriggerTime;
  objHndl animObj;
  int currentGoal;
  int field_2C;
  AnimSlotGoalStackEntry goals[8];
  AnimSlotGoalStackEntry *pCurrentGoal;
  uint32_t unk1; // field_1134
  AnimPath animPath;
  PathQueryResult path;
  AnimParam param1; // Used as parameters for goal states
  AnimParam param2; // Used as parameters for goal states
  uint32_t someGoalType;
  uint32_t unknown[5];
  uint64_t gametimeSth;
  uint32_t currentPing;    // I.e. used in
  uint32_t uniqueActionId; // ID assigned when triggered by a D20 action
};

const int testSizeofAnimslot = sizeof(AnimSlot); // should be 11416 (0x2C98)

enum AnimStateTransitionFlags : uint32_t
{
	ASTF_GOAL_DESTINATION_REMOVE = 0x2000000,
	ASTF_REWIND = 0x10000000, // will transition back to state 0
	ASTF_POP_GOAL = 0x30000000,
	ASTF_POP_GOAL_TWICE = 0x38000000,
	ASTF_PUSH_GOAL = 0x40000000,
	ASTF_POP_ALL = 0x90000000,
	ASTF_MASK = 0xFF000000 // the general mask for the special state transition flags
};

struct AnimStateTransition {
  uint32_t newState;
  // Delay in milliseconds or one of the constants below
  int delay;

  // Delay is read from runSlot->animDelay
  static const uint32_t DelaySlot = -2;
  // Delay is read from 0x10307534 (written by some goal states)
  static const uint32_t DelayCustom = -3;
  // Specifies a random 0-300 ms delay
  static const uint32_t DelayRandom = -4;
};

struct AnimGoalState {
  BOOL(__cdecl *callback)(AnimSlot &slot);
  int argInfo1;
  int argInfo2;
  int refToOtherGoalType;
  AnimStateTransition afterFailure;
  AnimStateTransition afterSuccess;
};

struct AnimGoal {
  int statecount;
  AnimGoalPriority priority;
  int interruptAll;
  int field_C;
  int field_10;
  int relatedGoal1;
  int relatedGoal2;
  int relatedGoal3;
  AnimGoalState states[16];
  AnimGoalState state_special;
};
#pragma pack(pop)

const char *animGoalTypeNames[ag_count] = {
    "ag_animate",
    "ag_animate_loop",
    "ag_anim_idle",
    "ag_anim_fidget",
    "ag_move_to_tile",
    "ag_run_to_tile",
    "ag_attempt_move",
    "ag_move_to_pause",
    "ag_move_near_tile",
    "ag_move_near_obj",
    "ag_move_straight",
    "ag_attempt_move_straight",
    "ag_open_door",
    "ag_attempt_open_door",
    "ag_unlock_door",
    "ag_jump_window",
    "ag_pickup_item",
    "ag_attempt_pickup",
    "ag_pickpocket",
    "ag_attack",
    "ag_attempt_attack",
    "ag_talk",
    "ag_pick_weapon",
    "ag_chase",
    "ag_follow",
    "ag_follow_to_location",
    "ag_flee",
    "ag_throw_spell",
    "ag_attempt_spell",
    "ag_shoot_spell",
    "ag_hit_by_spell",
    "ag_hit_by_weapon",
	"ag_dodge", // This was missing from the name-table and was probably added for ToEE
    "ag_dying",
    "ag_destroy_obj",
    "ag_use_skill_on",
    "ag_attempt_use_skill_on",
    "ag_skill_conceal",
    "ag_projectile",
    "ag_throw_item",
    "ag_use_object",
    "ag_use_item_on_object",
    "ag_use_item_on_object_with_skill",
    "ag_use_item_on_tile",
    "ag_use_item_on_tile_with_skill",
    "ag_knockback",
    "ag_floating",
    "ag_close_door",
    "ag_attempt_close_door",
    "ag_animate_reverse",
    "ag_move_away_from_obj",
    "ag_rotate",
    "ag_unconceal",
    "ag_run_near_tile",
    "ag_run_near_obj",
    "ag_animate_stunned",
    "ag_animate_kneel_magic_hands",
    "ag_attempt_move_near",
    "ag_knock_down",
    "ag_anim_get_up",
    "ag_attempt_move_straight_knockback",
    "ag_wander",
    "ag_wander_seek_darkness",
    "ag_use_picklock_skill_on",
    "ag_please_move",
    "ag_attempt_spread_out",
    "ag_animate_door_open",
    "ag_animate_door_closed",
    "ag_pend_closing_door",
    "ag_throw_spell_friendly",
    "ag_attempt_spell_friendly",
    "ag_animate_loop_fire_dmg",
    "ag_attempt_move_straight_spell",
    "ag_move_near_obj_combat",
    "ag_attempt_move_near_combat",
    "ag_use_container",
    "ag_throw_spell_w_cast_anim",
    "ag_attempt_spell_w_cast_anim",
    "ag_throw_spell_w_cast_anim_2ndary",
    "ag_back_off_from",
    "ag_attempt_use_pickpocket_skill_on",
    "ag_use_disable_device_skill_on_data"
};


ostream &operator<<(ostream &str, const AnimSlotId &id) {
  str << id.ToString();
  return str;
}

std::string GetAnimGoalTypeName(AnimGoalType type) {
  auto i = (size_t)type;
  if (type < ag_count) {
    return animGoalTypeNames[type];
  }
  return fmt::format("Unknown Goal Type [{}]", i);
}

ostream &operator<<(ostream &str, AnimGoalType type) {
  size_t i = (size_t)type;
  if (type < ag_count) {
    str << animGoalTypeNames[type];
  } else {
    str << fmt::format("Unknown Goal Type [{}]", i);
  }
  return str;
}

enum AnimGoalDataItem {
  AGDATA_SELF_OBJ = 0,     // Type: 1 (Object)
  AGDATA_TARGET_OBJ,       // Type: 1
  AGDATA_BLOCK_OBJ,        // Type: 1
  AGDATA_SCRATCH_OBJ,      // Type: 1
  AGDATA_PARENT_OBJ,       // Type: 1
  AGDATA_TARGET_TILE,      // Type: 2 (Location)
  AGDATA_RANGE_DATA,       // Type: 2
  AGDATA_ANIM_ID,          // Type: 0 (just a 32-bit number it seems)
  AGDATA_ANIM_ID_PREVIOUS, // Type: 0
  AGDATA_ANIM_DATA,        // Type: 0
  AGDATA_SPELL_DATA,       // Type: 0
  AGDATA_SKILL_DATA,       // Type: 0
  AGDATA_FLAGS_DATA,       // Type: 0
  AGDATA_SCRATCH_VAL1,     // Type: 0
  AGDATA_SCRATCH_VAL2,     // Type: 0
  AGDATA_SCRATCH_VAL3,     // Type: 0
  AGDATA_SCRATCH_VAL4,     // Type: 0
  AGDATA_SCRATCH_VAL5,     // Type: 0
  AGDATA_SCRATCH_VAL6,     // Type: 0
  AGDATA_SOUND_HANDLE      // Type: 0
};

const char *AnimGoalDataNames[] = {
    "AGDATA_SELF_OBJ",     "AGDATA_TARGET_OBJ",   "AGDATA_BLOCK_OBJ",
    "AGDATA_SCRATCH_OBJ",  "AGDATA_PARENT_OBJ",   "AGDATA_TARGET_TILE",
    "AGDATA_RANGE_DATA",   "AGDATA_ANIM_ID",      "AGDATA_ANIM_ID_PREV",
    "AGDATA_ANIM_DATA",    "AGDATA_SPELL_DATA",   "AGDATA_SKILL_DATA",
    "AGDATA_FLAGS_DATA",   "AGDATA_SCRATCH_VAL1", "AGDATA_SCRATCH_VAL2",
    "AGDATA_SCRATCH_VAL3", "AGDATA_SCRATCH_VAL4", "AGDATA_SCRATCH_VAL5",
    "AGDATA_SCRATCH_VAL6", "AGDATA_SOUND_HANDLE"};

enum AnimSlotFlag {
  ASF_ACTIVE = 1,
  ASF_STOP_PROCESSING = 2, // Used in context with "killing the animation slot"
  ASF_UNK3 = 4, // Seen in goalstatefunc_82, goalstatefunc_83, set with 0x8 in
                // goalstatefunc_42
  ASF_UNK4 = 8, // Seen in goalstatefunc_82, goalstatefunc_83, set with 0x8 in
                // goalstatefunc_42
  ASF_UNK5 = 0x10, // Seen in goalstatefunc_84_animated_forever, set in
                   // goalstatefunc_87
  ASF_UNK7 =
      0x20, // Seen as 0x30 is cleared in goalstatefunc_7 and goalstatefunc_8
  ASF_RUNNING = 0x40,
  ASF_SPEED_RECALC = 0x80,
  ASF_UNK8 = 0x400,   // Seen in goal_calc_path_to_loc, goalstatefunc_13_rotate,
                      // set in goalstatefunc_18
  ASF_UNK10 = 0x800,  // Seen in goalstatefunc_37
  ASF_UNK9 = 0x4000,  // Seen in goalstatefunc_19
  ASF_UNK11 = 0x8000, // Test in goalstatefunc_43
  ASF_UNK1 = 0x10000,
  ASF_UNK6 =
      0x20000, // Probably sound related (seen in anim_goal_free_sound_id)
  ASF_UNK12 = 0x40000 // set goalstatefunc_48, checked in goalstatefunc_50
};

static void assertStructSizes() {
  static_assert(temple::validate_size<ObjectId, 0x18>::value,
                "Goal Stack entry has incorrect size.");
  static_assert(temple::validate_size<AnimParam, 0x10>::value,
                "Anim Param union has the wrong size.");
  static_assert(temple::validate_size<AnimSlotGoalStackEntry, 0x220>::value,
                "Goal Stack entry has incorrect size.");
  static_assert(temple::validate_size<AnimSlot, 0x2C98>::value,
                "AnimSlot has incorrect size");
}

static class AnimAddressTable : temple::AddressTable {
public:
  uint32_t *nextUniqueId;
  uint32_t *slotsInUse;

  /*
  Some goal states want to set a dynamic delay for transitioning into the next
  state. They use this global variable to transport this value into the state
  machine controller below.
  */
  int *customDelayInMs;

  void(__cdecl *GetAnimName)(int animId, char *animNameOut);
  void(__cdecl *PushGoalDying)(objHndl obj, int rotation);
  void(__cdecl *InterruptAllForTbCombat)();

  /*
  Interrupts animations in the given animation slot. Exact behaviour is not
  known yet.
  */
  BOOL(__cdecl *Interrupt)(const AnimSlotId &id, AnimGoalPriority priority);

  int (*anim_first_run_idx_for_obj)(objHndl obj);
  BOOL (*anim_run_id_for_obj)(objHndl obj, AnimSlotId *slotIdOut);
  BOOL(*anim_frame_advance_maybe)(objHndl, AnimSlot& runSlot, uint32_t animHandle, uint32_t* eventOut);
  AnimSlotId tmpId;

  AnimAddressTable() {
    rebase(GetAnimName, 0x102629D0);
    rebase(nextUniqueId, 0x11E61520);
    rebase(slotsInUse, 0x10AA4BBC);
    rebase(customDelayInMs, 0x10307534);

    rebase(PushGoalDying, 0x100157B0);
	rebase(anim_frame_advance_maybe, 0x10016530);
    rebase(InterruptAllForTbCombat, 0x1000C950);
    rebase(Interrupt, 0x10056090);

    rebase(anim_first_run_idx_for_obj, 0x10054E20);
    rebase(anim_run_id_for_obj, 0x1000C430);
  }

} animAddresses;

static const uint32_t TRANSITION_LOOP = 0x10000000;
static const uint32_t TRANSITION_END = 0x20000000;
static const uint32_t TRANSITION_GOAL = 0x40000000;
static const uint32_t TRANSITION_UNK1 = 0x90000000;

static struct AnimationAdresses : temple::AddressTable {

  bool(__cdecl *PushRotate)(objHndl obj, float rotation);

  bool(__cdecl *PushUseSkillOn)(objHndl actor, objHndl target,
                                objHndl scratchObj, SkillEnum skill,
                                int goalFlags);

  int(__cdecl *PushAttackAnim)(objHndl actor, objHndl target, int unk1,
                               int hitAnimIdx, int playCrit,
                               int useSecondaryAnim);

  bool(__cdecl *PushRunNearTile)(objHndl actor, LocAndOffsets target,
                                 int radiusFeet);

  bool(__cdecl *PushUnconceal)(objHndl actor);

  bool(__cdecl *Interrupt)(objHndl actor, AnimGoalPriority priority, bool all);

  void(__cdecl *PushFallDown)(objHndl actor, int unk);

  int(__cdecl *GetAnimIdSthgSub_1001ABB0)(objHndl actor);

  int(__cdecl *PushAttemptAttack)(objHndl, objHndl);

  int(__cdecl *PushAnimate)(objHndl obj, int anim);

  AnimSlotId *animIdGlobal;

  void(__cdecl*Debug)();

  AnimationAdresses() {

    rebase(Interrupt, 0x1000C7E0);
    rebase(PushAnimate, 0x10015290);
    rebase(PushRotate, 0x100153E0);
    rebase(PushFallDown, 0x100157B0);
    rebase(PushUnconceal, 0x10015E00);

    rebase(PushAttemptAttack, 0x1001A540);
    rebase(GetAnimIdSthgSub_1001ABB0, 0x1001ABB0);

    rebase(PushUseSkillOn, 0x1001C690);
    rebase(PushRunNearTile, 0x1001C1B0);

    rebase(PushAttackAnim, 0x1001C370);

	rebase(animIdGlobal, 0x102AC880);

	rebase(Debug, 0x10055130);
  }

} addresses;

AnimationGoals animationGoals;


class GoalStateFuncs
{
#define GoalStateFunc(fname) static int __cdecl GoalStateFunc ## fname ## (AnimSlot& slot);
public:

	static int __cdecl GoalStateFunc35(AnimSlot& ars);
	static int __cdecl GoalStateFunc54(AnimSlot& slot); // used in ag_attempt_attack
	static int __cdecl GoalStateFunc133(AnimSlot& slot);
	static int __cdecl GoalStateFuncIsCritterProne(AnimSlot& slot);
	static int __cdecl GoalStateFuncIsParam1Concealed(AnimSlot& slot);
	static int __cdecl GoalStateFunc78_IsHeadingOk(AnimSlot &slot);
	static int __cdecl GoalStateFunc82(AnimSlot& slot);
	static int __cdecl GoalStateFunc83(AnimSlot& slot);
	static int __cdecl GoalStateFunc65(AnimSlot& slot); // belongs in ag_hit_by_weapon
	static int __cdecl GoalStateFunc70(AnimSlot& slot); // 
	static int __cdecl GoalStateFunc100_IsCurrentPathValid(AnimSlot& slot);
	static int __cdecl GoalStateFunc106(AnimSlot& slot);
	static int __cdecl GoalStateFunc130(AnimSlot& slot);
	static int __cdecl GoalStateFuncPickpocket(AnimSlot &slot);
	
} gsFuncs;

bool AnimationGoals::PushRotate(objHndl obj, float rotation) {
  return addresses.PushRotate(obj, rotation);
}

bool AnimationGoals::PushUseSkillOn(objHndl actor, objHndl target,
                                    SkillEnum skill, objHndl scratchObj,
                                    int goalFlags) {
  return addresses.PushUseSkillOn(actor, target, scratchObj, skill, goalFlags);
}

bool AnimationGoals::PushRunNearTile(objHndl actor, LocAndOffsets target,
                                     int radiusFeet) {
  return addresses.PushRunNearTile(actor, target, radiusFeet);
}

bool AnimationGoals::PushRunToTile(objHndl handle, LocAndOffsets loc, PathQueryResult * pqr)
{

	// To DO!
	if (!handle
		|| critterSys.IsDeadOrUnconscious(handle) 
		|| (!critterSys.IsPC(handle) && temple::GetRef<objHndl(__cdecl)(objHndl)>(0x10053CA0)(handle))  ) // npc is conversing with pc
		return false;


	AnimSlotGoalStackEntry goalData;
	AnimSlot *runSlot;

	if (temple::GetRef<BOOL(__cdecl)(objHndl, AnimGoalType, AnimSlotId&)>(0x10054F30)(handle, ag_run_to_tile, *addresses.animIdGlobal)){ // is obj doing related goal?
		goalData.Init(handle, ag_run_to_tile, 0);
		goalData.targetTile.location = loc;
		if ( !animationGoals.Interrupt(handle, AnimGoalPriority::AGP_3, 0)
			 || !goalData.Push(addresses.animIdGlobal)
			 || !GetSlot(addresses.animIdGlobal, &runSlot)){
			return true;
		};
	} 
	else{
		goalData.Init(handle, ag_run_to_tile, 0);
		// To DO!
	}
	
	return false;
}

bool AnimationGoals::PushUnconceal(objHndl actor) {
  return addresses.PushUnconceal(actor);
}

bool AnimationGoals::Interrupt(objHndl actor, AnimGoalPriority priority,
                               bool all) {
  return addresses.Interrupt(actor, priority, all);
}

void AnimationGoals::PushFallDown(objHndl actor, int unk) {
  addresses.PushFallDown(actor, unk);
}

int AnimationGoals::PushAttackAnim(objHndl actor, objHndl target, int unk1,
                                   int hitAnimIdx, int playCrit,
                                   int useSecondaryAnim) {
  return addresses.PushAttackAnim(actor, target, unk1, hitAnimIdx, playCrit,
                                  useSecondaryAnim);
}

int AnimationGoals::GetActionAnimId(objHndl objHndl) {
  return addresses.GetAnimIdSthgSub_1001ABB0(objHndl);
}

int AnimationGoals::PushAttemptAttack(objHndl attacker, objHndl defender) {
  return addresses.PushAttemptAttack(attacker, defender);
}

int AnimationGoals::PushDodge(objHndl attacker, objHndl dodger){
	return temple::GetRef<BOOL(__cdecl)(objHndl, objHndl)>(0x100158E0)(attacker, dodger);
}

int AnimationGoals::PushAnimate(objHndl obj, int anim) {
  return addresses.PushAnimate(obj, anim);
}

BOOL AnimationGoals::PushSpellCast(SpellPacketBody & spellPkt, objHndl item)
{

	// note: the original included the spell ID generation & registration, this is separated here.
	auto caster = spellPkt.caster;
	auto casterObj = objSystem->GetObject(caster);
	AnimSlotGoalStackEntry goalData;
	if (!goalData.InitWithInterrupt(caster, ag_throw_spell_w_cast_anim))
		return FALSE;

	goalData.skillData.number = spellPkt.spellId;

	SpellEntry spEntry(spellPkt.spellEnum);

	// if self-targeted spell
	if (spEntry.IsBaseModeTarget(UiPickerType::Single) && spellPkt.targetCount == 0 ){
		goalData.target.obj = spellPkt.caster;
		
		if (spellPkt.aoeCenter.location.location == 0){
			goalData.targetTile.location = casterObj->GetLocationFull();
		} 
		else{
			goalData.targetTile.location = spellPkt.aoeCenter.location;
		}
	} 

	else{
		auto tgt = spellPkt.targetListHandles[0];
		goalData.target.obj = tgt;
		if (tgt && spellPkt.aoeCenter.location.location == 0 ){
			goalData.targetTile.location = objSystem->GetObject(tgt)->GetLocationFull();
		}
		else {
			goalData.targetTile.location = spellPkt.aoeCenter.location;
		}
	}

	if (inventory.UsesWandAnim(item)){
		goalData.animIdPrevious.number = temple::GetRef<int(__cdecl)(int)>(0x100757C0)(spEntry.spellSchoolEnum); // GetAnimIdWand	
	}
	else{
		goalData.animIdPrevious.number = temple::GetRef<int(__cdecl)(int)>(0x100757B0)(spEntry.spellSchoolEnum); // GetSpellSchoolAnimId
	}

	AnimSlotId slotIdNew;
	return goalData.Push(&slotIdNew);
}

BOOL AnimationGoals::PushSpellInterrupt(const objHndl& caster, objHndl item, AnimGoalType animGoalType, int spellSchool){
	AnimSlotGoalStackEntry goalData;
	goalData.InitWithInterrupt(caster, animGoalType);
	goalData.target.obj = (*actSeqSys.actSeqCur)->spellPktBody.caster;
	goalData.skillData.number = 0;
	if (inventory.UsesWandAnim(item))
		goalData.animIdPrevious.number = temple::GetRef<int(__cdecl)(int)>(0x100757C0)(spellSchool); // GetAnimIdWand	
	else
		goalData.animIdPrevious.number = temple::GetRef<int(__cdecl)(int)>(0x100757B0)(spellSchool); // GetSpellSchoolAnimId
	
	AnimSlotId idNew;
	return goalData.Push(&idNew);
}

void AnimationGoals::PushForMouseTarget(objHndl handle, AnimGoalType type, objHndl tgt, locXY loc, objHndl scratchObj, int someFlag){

	AnimSlotGoalStackEntry goalData;
	temple::GetRef<void(__cdecl)(AnimSlotGoalStackEntry&, objHndl, AnimGoalType, objHndl, locXY, objHndl, int)>(0x10113470)(goalData, handle, type, tgt, loc, scratchObj, someFlag);
}

void AnimationGoals::Debug(){
	gameSystems->GetAnim().DebugGoals();
}

const AnimGoal* AnimationGoals::GetGoal(AnimGoalType goalType)
{
	auto gArray = *gameSystems->GetAnim().mGoals;
	return &gArray[goalType];
}

BOOL AnimationGoals::GetSlot(AnimSlotId * runId, AnimSlot **runSlotOut){
	if (!runId){
		logger->error("Null runId in GetSlot()");
		Debug();
	}
	if (!runSlotOut){
		logger->error("Null runSlotOut in GetSlot()");
		Debug();
	}

	if (runId->slotIndex == -1){
		*runSlotOut = nullptr;
		return FALSE;
	}

	for (auto i=0; i < ANIM_RUN_SLOT_CAP; i++){
		auto slot = &gameSystems->GetAnim().mSlots[i];
		if (slot->id.slotIndex == runId->slotIndex && slot->id.uniqueId == runId->uniqueId){
			*runSlotOut = slot;
			return TRUE;
		}
	}

	return TRUE;
}



//*****************************************************************************
//* Anim
//*****************************************************************************

AnimSystem::AnimSystem(const GameSystemConf &config) {
  auto startup = temple::GetPointer<int(const GameSystemConf *)>(0x10016bb0);
  if (!startup(&config)) {
    throw TempleException("Unable to initialize game system Anim");
  }
}
AnimSystem::~AnimSystem() {
  auto shutdown = temple::GetPointer<void()>(0x1000c110);
  shutdown();
}
void AnimSystem::Reset() {
  auto reset = temple::GetPointer<void()>(0x1000c120);
  reset();
}
bool AnimSystem::SaveGame(TioFile *file) {
  auto save = temple::GetPointer<int(TioFile *)>(0x1001cab0);
  return save(file) == 1;
}
bool AnimSystem::LoadGame(GameSystemSaveFile *saveFile) {
  auto load = temple::GetPointer<int(GameSystemSaveFile *)>(0x1001d250);
  return load(saveFile) == 1;
}
const std::string &AnimSystem::GetName() const {
  static std::string name("Anim");
  return name;
}

void AnimSystem::ClearGoalDestinations() {
  static auto clear = temple::GetPointer<void()>(0x100BACC0);
  clear();
}

void AnimSystem::InterruptAll() {
  static auto anim_interrupt_all = temple::GetPointer<BOOL()>(0x1000c890);
  anim_interrupt_all();
}

BOOL AnimSystem::ProcessAnimEvent(const TimeEvent *evt) {

  if (mAllSlotsUsed) {
    static auto anim_goal_interrupt_all_goals_of_priority =
        temple::GetPointer<signed int(signed int priority)>(0x1000c8d0);
    anim_goal_interrupt_all_goals_of_priority(AGP_3);
    mAllSlotsUsed = FALSE;
  }

  // The animation slot id we're triggered for
  AnimSlotId triggerId = {evt->params[0].int32, evt->params[1].int32,
                          evt->params[2].int32};

  assert(triggerId.slotIndex < 512);

  auto &slot = mSlots[triggerId.slotIndex];

  // This seems like a pretty stupid check since slots cannot "move"
  // and the first part of their ID must be the slot index
  // Shouldn't this really check for the unique id of the animation instead?
  if (slot.id.slotIndex != triggerId.slotIndex) {
    logger->debug("{} != {}", slot.id, triggerId);
    return TRUE;
  }

  // Slot did belong to "us", but it was deactivated earlier
  if (!(slot.flags & ASF_ACTIVE)) {
    ProcessActionCallbacks();
    return TRUE;
  }

  // Interesting how this reschedules in at least 100ms which seems steep for
  // animation processing
  // Have to check where and why this is set
  if (slot.flags & ASF_UNK1) {
    ProcessActionCallbacks();

    auto delay = std::max(slot.path.someDelay, 100);
    
    return RescheduleEvent(delay, slot, evt);
  }

  if (slot.currentGoal < 0) {
    logger->warn("Found slot {} with goal < 0", slot.id);
    slot.currentGoal = 0;
  }

  // This sets the current stack pointer, although it should already be set.
  // They used
  // a lot of safeguard against themselves basically
  auto currentGoal = &slot.goals[slot.currentGoal];
  slot.pCurrentGoal = currentGoal;

  bool stopProcessing = false;
  const AnimGoal *goal = nullptr;
 // auto oldGoal = goal;

  // And another safeguard
  if (currentGoal->goalType < 0 || currentGoal->goalType >= ag_count) {
    slot.flags |= ASF_STOP_PROCESSING;
    stopProcessing = true;
  } else {
    goal = mGoals[currentGoal->goalType];
    if (!goal) {
      logger->error("Animation slot {} references null goal {}.", slot.id,
                    currentGoal->goalType);
    }
  }

  // This validates object references found in the animation slot
  if (!PrepareSlotForGoalState(slot, nullptr)) {
    ProcessActionCallbacks();
    return TRUE;
  }

  // Validates that the object the animation runs for is not destroyed
  if (slot.animObj) {
    if (objects.GetFlags(slot.animObj) & OF_DESTROYED) {
      logger->warn("Processing animation slot {} for destroyed object.",
                   slot.id);
    }
  } else {
    // Animation is no longer associated with an object after validation
    slot.flags |= ASF_STOP_PROCESSING;
    stopProcessing = true;
  }

  int delay = 0;
  mCurrentlyProcessingSlotIdx = slot.id.slotIndex;
  // TODO: Clean up this terrible control flow
  if (!stopProcessing) {

    mCurrentlyProcessingSlotIdx = slot.id.slotIndex;

    // TODO: processing
    int loopNr = 0;

    while (!stopProcessing) {
      ++loopNr;

      // This only applies to in-development i think, since as of now there
      // should be no infi-looping goals
      if (loopNr >= 100) {
        logger->error("Goal {} loops infinitely in animation {}!",
                      slot.pCurrentGoal->goalType, slot.id);
        combatSys.CombatAdvanceTurn(slot.animObj);
        mCurrentlyProcessingSlotIdx = -1;
        animAddresses.Interrupt(slot.id, AGP_HIGHEST);
        ProcessActionCallbacks();
        return TRUE;
      }

      auto &currentState = goal->states[slot.currentState];

      // Prepare for the current state
      if (!PrepareSlotForGoalState(slot, &currentState)) {
        ProcessActionCallbacks();
        return TRUE;
      }

	  /*


	  *******  PROCESSING *******
	  
	  
	  */

      auto stateResult = currentState.callback(slot);

      // Check flags on the slot that may have been set by the callbacks.
      if (slot.flags & ASF_UNK1) {
        stopProcessing = true;
      }

      if (!(slot.flags & ASF_ACTIVE)) {
        mCurrentlyProcessingSlotIdx = -1;
        ProcessActionCallbacks();
        return TRUE;
      }

      if (slot.flags & ASF_STOP_PROCESSING) {
        break;
      }

      auto &transition =
          stateResult ? currentState.afterSuccess : currentState.afterFailure;
      auto nextState = transition.newState;
      delay = transition.delay;

      // Special transitions
      if (nextState & ASTF_MASK) {
		/*  if (currentGoal->goalType != ag_anim_idle)
			logger->debug("ProcessAnimEvent: Special transition; currentState: {:x}", slot.currentState);*/
        if (nextState & ASTF_REWIND) {
			/*if (currentGoal->goalType != ag_anim_idle)
				logger->debug("Setting currentState to 0");*/
          slot.currentState = 0;
          stopProcessing = true;
        }

		if ((nextState & ASTF_POP_GOAL_TWICE) == ASTF_POP_GOAL_TWICE) {
		//	logger->debug("Popping 2 goals due to 0x38000000");
			auto newGoal = &goal;
			auto popFlags = nextState;
			PopGoal(slot, popFlags, newGoal, &currentGoal, &stopProcessing);
			PopGoal(slot, popFlags, newGoal, &currentGoal, &stopProcessing);
			//oldGoal = goal;
		} 
		else if ( (nextState & ASTF_POP_GOAL) == ASTF_POP_GOAL) {
		//  logger->debug("Popping 1 goals due to 0x30000000");
          auto newGoal = &goal;
          auto popFlags = nextState;
          PopGoal(slot, popFlags, newGoal, &currentGoal, &stopProcessing);
		  //oldGoal = goal;
        }

        if (nextState & ASTF_PUSH_GOAL) {
			if (slot.currentGoal >= 7) {
				logger->error("Unable to push goal, because anim slot %s has overrun!", slot.id.ToString());
				logger->error("Current sub goal stack is:");
				
				for (auto i = 0; i < slot.currentGoal; i++) {
					logger->info("\t[{}]: Goal {}", i, animGoalTypeNames[slot.goals[i].goalType]);
				}

				//oldGoal = goal;
				slot.flags |= ASF_STOP_PROCESSING;
				slot.currentState = 0;
				stopProcessing = true;
			} 
			else {
				slot.currentState = 0;
				slot.currentGoal++;

				currentGoal = &slot.goals[slot.currentGoal];
				slot.pCurrentGoal = currentGoal;

				// Apparently if 0x30 00 00 00 is also set, it copies the previous goal????
				if (slot.currentGoal > 0 && (nextState & ASTF_POP_GOAL) != ASTF_POP_GOAL) {
				//	logger->debug("Copying previous goal");
					slot.goals[slot.currentGoal] = slot.goals[slot.currentGoal - 1];
				}

				auto newGoalType = static_cast<AnimGoalType>(nextState & 0xFFF);
				goal = mGoals[newGoalType];
				slot.goals[slot.currentGoal].goalType = newGoalType;

				static auto animNumActiveGoals_inc = temple::GetPointer<void(AnimSlot &_slot, const AnimGoal *pGoalNode)>(0x10055bf0);
				animNumActiveGoals_inc(slot, goal);
			}
        }
        if ((nextState & ASTF_POP_ALL) == ASTF_POP_ALL) {
		//  logger->debug("ProcessAnimEvent: 0x90 00 00 00");
		  currentGoal = &slot.goals[0];
          auto prio = mGoals[slot.goals[0].goalType]->priority;
		  goal = mGoals[slot.goals[0].goalType];
		  if (prio < AnimGoalPriority::AGP_7) {
			//    logger->debug("ProcessAnimEvent: root goal priority less than 7");
				slot.flags |= AnimSlotFlag::ASF_STOP_PROCESSING;
			
				for (auto i = 1; i < slot.currentGoal; i++) {
					auto _goal = mGoals[slot.goals[i].goalType];

					if (_goal->state_special.callback) {
						if (PrepareSlotForGoalState(slot, &_goal->state_special)) {
						  _goal->state_special.callback(slot);
						}
					}
				}

				auto goal0 = mGoals[slot.goals[0].goalType];
				if (goal0->state_special.callback) {
				  if (PrepareSlotForGoalState(slot, &goal0->state_special))
					goal0->state_special.callback(slot);
				}
				slot.animPath.flags |= 1u;
				slot.currentState = 0;
				slot.path.flags &= ~PF_COMPLETE;
				GoalDestinationsRemove(slot.path.mover);
				//oldGoal = goal;
				slot.field_14 = -1;
				stopProcessing = true;
			} 
			else {
			//	logger->debug("ProcessAnimEvent: root goal priority equal to 7");
				currentGoal = &slot.goals[slot.currentGoal];
				goal = mGoals[currentGoal->goalType];
				// oldGoal = goal;
				while (goal->priority < AnimGoalPriority::AGP_7) {
				  PopGoal(slot, ASTF_POP_GOAL, &goal, &currentGoal, &stopProcessing);
				//  logger->debug("ProcessAnimEvent: Popped goal for {}.", description.getDisplayName(slot.animObj));
				  currentGoal = &slot.goals[slot.currentGoal];
				  goal = mGoals[currentGoal->goalType];
				  // oldGoal = goal;
				}
			}
        }
      } else {
        // Normal jump to another state without special flags
        --nextState; // Jumps are 1-based, although the array is 0-based
        if (slot.currentState == nextState) {
          logger->error("State {} of goal {} transitioned into itself.",
                        slot.currentState, currentGoal->goalType);
        }
        slot.currentState = nextState;
      }

      if (delay) {
        switch (delay) {
        case AnimStateTransition::DelaySlot:
          // Use the delay specified in the slot. Reasoning currently unknown.
          // NOTE: Could mean that it's waiting for pathing to complete
          delay = slot.path.someDelay;
          break;
        case AnimStateTransition::DelayCustom:
          // Used by some goal states to set their desired dynamic delay
          delay = *animAddresses.customDelayInMs;
          break;
        case AnimStateTransition::DelayRandom:
          // Calculates the animation delay randomly in a range from 0 to 300
          delay = RandomIntRange(0, 300);
          break;
        default:
          // Keep predefined delay
          break;
        }

        stopProcessing = true;
      }

      // If no delay has been set, the next state is immediately processed
    }
  }

  mCurrentlyProcessingSlotIdx = -1;
  AnimSlotFlag slotFlags = static_cast<AnimSlotFlag>(slot.flags); // for debug
  // Does Flag 2 mean "COMPLETED" ?
  if (!(slot.flags & ASF_STOP_PROCESSING)) {
    if (slot.flags & ASF_ACTIVE) {
      // This actually seems to be the "GOOD" case
      auto result = RescheduleEvent(delay, slot, evt);
	  ProcessActionCallbacks();
	  if (slot.pCurrentGoal->goalType != ag_anim_idle)  {
		//  logger->debug("ProcessAnimEvent: rescheduled for {}, goal {}", description.getDisplayName(slot.animObj), animGoalTypeNames[slot.pCurrentGoal->goalType]);
	  } else
	  {
		  int da = 1;
	  }
	  return result;
    }
    ProcessActionCallbacks();
    return TRUE;
  }

  auto result = TRUE;
  if (slot.animObj) {

	// preserve the values i case the slot gets deallocated below
	auto animObj = slot.animObj;
	auto actionAnimId = slot.uniqueActionId;
	//logger->debug("ProcessAnimEvent: Interrupting goals.");
    // Interrupt everything for the slot
    result = animAddresses.Interrupt(slot.id, AGP_HIGHEST);

	if (!slot.animObj)
	{
		int aha = 0;
	}

    if (animObj && objects.IsCritter(animObj)) {
      //PushActionCallback(slot);
		if (!actionAnimId)
		{
			int dummy = 1;
		}
		mActionCallbacks.push_back({ animObj, actionAnimId });
    }
  }

  ProcessActionCallbacks();
  return result;
}

void AnimSystem::PushDisableFidget()
{
	static auto call = temple::GetPointer<void()>(0x100603F0);
	call();
}

void AnimSystem::PopDisableFidget()
{
	static auto call = temple::GetPointer<void()>(0x10060410);
	call();
}

void AnimSystem::ProcessActionCallbacks() {
	// changed to manual iteration because PerformOnAnimComplete can alter the vector
	auto initSize = mActionCallbacks.size();
	for (size_t i = 0; i < mActionCallbacks.size(); i++) {
	auto& callback = mActionCallbacks[i];
    actSeqSys.PerformOnAnimComplete(callback.obj, callback.uniqueId);
	if (initSize != mActionCallbacks.size())
	{
		int dummy = 1;
	}
	//callback.obj = 0i64;
	mActionCallbacks[i].obj = 0;
  }

  mActionCallbacks.clear();
}

void AnimSystem::PushActionCallback(AnimSlot &slot) {

  if (slot.uniqueActionId == 0) {
    return;
  }

  mActionCallbacks.push_back({slot.animObj, slot.uniqueActionId});
}

void AnimSystem::PopGoal(AnimSlot &slot, uint32_t popFlags,
                         const AnimGoal **newGoal,
                         AnimSlotGoalStackEntry **newCurrentGoal,
                         bool *stopProcessing) {
	//logger->debug("Pop goal for {} with popFlags {:x}  (slot flags: {:x}, state {:x})", description.getDisplayName(slot.animObj), popFlags, static_cast<uint32_t>(slot.flags), slot.currentState);
  if (!slot.currentGoal && !(popFlags & ASTF_PUSH_GOAL)) {
    slot.flags |= AnimSlotFlag::ASF_STOP_PROCESSING;
  }

  if ((*newGoal)->state_special.callback) {
    if (!(popFlags & 0x70000000) || !(popFlags & 0x4000000)) {
      if (PrepareSlotForGoalState(slot, &(*newGoal)->state_special)) {
		//  logger->debug("Pop goal for {}: doing state special callback.", description.getDisplayName(slot.animObj));
        (*newGoal)->state_special.callback(slot);
      }
    }
  }

  if (!(popFlags & 0x1000000)) {
    slot.flags &= ~0x83C;
    slot.animPath.pathLength = 0; // slot->anim_path.maxPathLength = 0;
  }

  if (popFlags & ASTF_GOAL_DESTINATION_REMOVE) {
    objHndl mover = slot.path.mover;
    slot.animPath.flags = 1;
    slot.path.flags = PF_NONE;
    GoalDestinationsRemove(mover);
  }

  static auto animNumActiveGoals_dec =
      temple::GetRef<void(__cdecl)(AnimSlot &, const AnimGoal *)>(0x10055CA0);
  animNumActiveGoals_dec(slot, *newGoal);
  slot.currentGoal--;
  slot.currentState = 0;
  if (slot.currentGoal < 0) {
    if (!(popFlags & ASTF_PUSH_GOAL)) {
      slot.flags |= AnimSlotFlag::ASF_STOP_PROCESSING;
	//  logger->debug("Pop goal for {}: stopping processing (last goal was {}).", description.getDisplayName(slot.animObj), animGoalTypeNames[slot.pCurrentGoal->goalType]);
    }
  } else {
    auto prevGoal = &slot.goals[slot.currentGoal];
	//logger->debug("Popped goal {}, new goal is {}", animGoalTypeNames[slot.pCurrentGoal->goalType], animGoalTypeNames[prevGoal->goalType]);
	slot.pCurrentGoal = *newCurrentGoal = &slot.goals[slot.currentGoal];
    *newGoal = mGoals[(*newCurrentGoal)->goalType];
    *stopProcessing = false;
    if (prevGoal->goalType == ag_anim_fidget) {
		int dummy = 1;
      // FIX: prevents ag_anim_fidget from queueing an AnimComplete call (which
      // creates the phantom animId = 0 bullshit)
    } else if ((*newCurrentGoal)->goalType == ag_anim_idle &&
               !(popFlags & ASTF_PUSH_GOAL)) {
      PushActionCallback(slot);
    }
  }
 // logger->debug("PopGoal: Ending with slot flags {:x}, state {:x}", static_cast<uint32_t>(slot.flags), slot.currentState);
}

/*
When an event should be re-executed at a later time, but unmodified, this
method is used. It also checks whether animations should "catch up" (by skipping
frames essentially), or whether they should be run at whatever speed was
intended,
but visibly slowing down.
*/
BOOL AnimSystem::RescheduleEvent(int delayMs, AnimSlot &slot,
                                 const TimeEvent *oldEvt) {
  TimeEvent evt;
  evt.system = TimeEventType::Anim;
  evt.params[0].int32 = slot.id.slotIndex;
  evt.params[1].int32 = slot.id.uniqueId;
  evt.params[2].int32 =
      1111; // Some way to identify these rescheduled events???

  if (config.animCatchup) {
	return gameSystems->GetTimeEvent().ScheduleAbsolute(evt, oldEvt->time, delayMs,
                                                 &slot.nextTriggerTime);
  } else {
	return gameSystems->GetTimeEvent().Schedule(evt, delayMs, &slot.nextTriggerTime);
  }
}

void AnimSystem::GoalDestinationsRemove(objHndl obj) {
  static auto goal_destinations_remove =
      temple::GetPointer<void(objHndl)>(0x100bac80);
  goal_destinations_remove(obj);
}

bool AnimSystem::InterruptGoals(AnimSlot &slot, AnimGoalPriority priority) {


  assert(slot.id.slotIndex < 512);

  AnimSlot * newSlot;

  auto animpriv_get_slot = temple::GetRef<BOOL(AnimSlotId&, AnimSlot**)>(0x10016C40);
  if (!animpriv_get_slot(slot.id, &newSlot))
  {
	  return false;
  }
	  

  if (!(slot.flags & ASF_ACTIVE)) {
	  return false;
  }

  if (!gameSystems->IsResetting() && slot.currentGoal != -1) {
    auto &stackTop = slot.goals[slot.currentGoal];
    auto goal = mGoals[stackTop.goalType];
    assert(goal);

    if (priority < AnimGoalPriority::AGP_HIGHEST){
		if (goal->interruptAll) {
			return true;
		}
		if (goal->priority == AnimGoalPriority::AGP_5) {
			return false;
		}
    }
    

    if (goal->priority == AnimGoalPriority::AGP_3) {
      if (priority < AnimGoalPriority::AGP_3) {
        return false;
      }
    } else if (goal->priority == AnimGoalPriority::AGP_2) {
      if (priority < 2) {
        return false;
      }
    } else if (goal->priority >= priority) {
      if (goal->priority != AnimGoalPriority::AGP_7) {
        return false;
      }
      slot.flags &= ~ASF_UNK5;
    }
  }

  auto goalType = mGoals[slot.goals[0].goalType];
  if (goalType->priority >= AnimGoalPriority::AGP_7 &&
      priority < AnimGoalPriority::AGP_7) {
    auto pNewStackTopOut = &slot.goals[slot.currentGoal];
    for (goalType = mGoals[pNewStackTopOut->goalType];
         goalType->priority < AnimGoalPriority::AGP_7;
         goalType = mGoals[pNewStackTopOut->goalType]) {
      bool stopProcessing = false;
      PopGoal(slot, 0x30000000, &goalType, &pNewStackTopOut, &stopProcessing);
      pNewStackTopOut = &slot.goals[slot.currentGoal];
    }
    return true;
  }

  slot.flags |= ASF_STOP_PROCESSING;

  if (mCurrentlyProcessingSlotIdx == slot.id.slotIndex) {
    return true;
  }

  // Removes all time events for the slot
  gameSystems->GetTimeEvent().Remove(
      TimeEventType::Anim, [&](const TimeEvent &evt) {
        return evt.params[0].int32 == slot.id.slotIndex;
      });

  if (slot.currentGoal != -1) {
    if (!slot.pCurrentGoal) {
      slot.pCurrentGoal = &slot.goals[slot.currentGoal];
    }
    for (auto i = slot.currentGoal; i >= 0; i--) {
      auto goal = mGoals[slot.goals[i].goalType];
      if (!gameSystems->IsResetting()) {
        if (goal->state_special.callback) {
          if (PrepareSlotForGoalState(slot, &goal->state_special)) {
            goal->state_special.callback(slot);
          }
        }
      }
    }
  }

  static auto deallocateSlot = temple::GetPointer<BOOL(AnimSlot &)>(0x10055d30);
  deallocateSlot(slot);

  return false;
}

bool AnimSystem::PrepareSlotForGoalState(AnimSlot &slot,
                                         const AnimGoalState *state) {
  static auto anim_prepare_run_for_goalstate =
      temple::GetPointer<int(AnimSlot * runSlot, const AnimGoalState *)>(
          0x10055700);
  return anim_prepare_run_for_goalstate(&slot, state) == TRUE;
}

void AnimSystem::DebugGoals()
{
	logger->debug("Currently Existing Animations");
	logger->debug("------------------------------------------------");
	for (auto i = 0; i < ANIM_RUN_SLOT_CAP; i++) {
		auto &slot = mSlots[i];
		if (slot.flags){
			logger->debug("In slot {}", i);
		}
	}
	logger->debug("------------------------------------------------");
}

std::string AnimSlotId::ToString() const {
  return format("[{}:{}r{}]", slotIndex, uniqueId, field_8);
}

int GoalStateFuncs::GoalStateFunc106(AnimSlot &slot) {
	//logger->debug("GSF 106 for {}, goal {}, flags {:x}, currentState {:x}", description.getDisplayName(slot.animObj), animGoalTypeNames[slot.pCurrentGoal->goalType], (uint32_t)slot.flags, (uint32_t)slot.currentState);
  auto obj = slot.param1.obj;
  assert(slot.param1.obj);

  auto aasHandle = objects.GetAnimHandle(obj);
  assert(aasHandle);

  if (objects.getInt32(obj, obj_f_spell_flags) & SpellFlags::SF_10000) {
	  //logger->debug("GSF 106: return FALSE due to obj_f_spell_flags 0x10000");
    return FALSE;
  }

  auto animId = aasHandle->GetAnimId();

  if ((!objects.IsCritter(obj) ||
       !(objects.getInt32(obj, obj_f_critter_flags) &
         (OCF_PARALYZED | OCF_STUNNED)) ||
       !animId.IsWeaponAnim() &&
           (animId.GetNormalAnimType() == gfx::NormalAnimType::Death ||
            animId.GetNormalAnimType() == gfx::NormalAnimType::Death2 ||
            animId.GetNormalAnimType() == gfx::NormalAnimType::Death3))) {
    static auto anim_frame_advance_maybe =
        temple::GetPointer<BOOL(objHndl , AnimSlot & runSlot,
                                uint32_t animHandle, uint32_t * eventOut)>(
            0x10016530);
    uint32_t eventOut = 0;
    anim_frame_advance_maybe(obj, slot, aasHandle->GetHandle(), &eventOut);

    // This is the ACTION trigger
    if (eventOut & 1) {
      slot.flags |= AnimSlotFlag::ASF_UNK3;
	  //logger->debug("GSF 106: Set flag 4, returned TRUE");
      return TRUE;
    }

	// If the animation is a looping animation, it does NOT have a
	// defined end, so we just trigger the end here anyway otherwise
	// this'll loop endlessly
	bool looping = false;
	/*if (animId.IsWeaponAnim() && ( animId.GetWeaponAnim() == gfx::WeaponAnim::Idle || animId.GetWeaponAnim() == gfx::WeaponAnim::CombatIdle)) {*/

	

	if (animId.IsWeaponAnim() && (animId.GetWeaponAnim() == gfx::WeaponAnim::Idle )) {
	//	// We will continue anyway down below, because the character is idling, so log a message
		if (!(eventOut & 2)) {
			logger->info("Ending wait for animation action/end in goal {}, because the idle animation would never end.",
				animGoalTypeNames[slot.pCurrentGoal->goalType]);
		}
		looping = true;
	}

    // This is the END trigger
    if (!looping && !(eventOut & 2))
    {
		//logger->debug("GSF 106: eventOut & 2, returned TRUE");
		return TRUE;
    }
      

    // Clears WaypointDelay flag
    auto gameObj = objSystem->GetObject(obj);
    if (objects.IsNPC(obj)) {
      auto flags = gameObj->GetInt64(obj_f_npc_ai_flags64);
      gameObj->SetInt64(obj_f_npc_ai_flags64, flags & 0xFFFFFFFFFFFFFFFDui64);
    }

    // Clear 0x10 slot flag
	slot.flags &= ~AnimSlotFlag::ASF_UNK5;
  }
  //logger->debug("GSF 106: returned FALSE");
  return FALSE;
}

int GoalStateFuncs::GoalStateFunc130(AnimSlot& slot)
{
	//logger->debug("GSF 130");
	uint32_t eventOut = 0;
	auto handle = slot.param1.obj;
	if (!handle)
	{
		logger->warn("Missing anim object!");
		return FALSE;
	}

	auto obj = gameSystems->GetObj().GetObject(handle);
	auto aasHandle = objects.GetAnimHandle(handle);
	if (!aasHandle || !aasHandle->GetHandle())
	{
		logger->warn("No aas handle!");
		return FALSE;
	}

	if (obj->GetInt32(obj_f_spell_flags) & SpellFlags::SF_10000)
		return FALSE;
	
	if (obj->IsCritter())
	{
		if (obj->GetInt32(obj_f_critter_flags) & (OCF_PARALYZED | OCF_STUNNED))
			return FALSE;
	}
	animAddresses.anim_frame_advance_maybe(handle, slot, aasHandle->GetHandle(), &eventOut);

	if (eventOut & 1)
		slot.flags |= AnimSlotFlag::ASF_UNK3;

	if (eventOut & 2) {
		slot.flags &= ~(AnimSlotFlag::ASF_UNK5);
		return FALSE;
	}
	return TRUE;
}

int GoalStateFuncs::GoalStateFuncPickpocket(AnimSlot & slot)
{

	auto tgt = slot.param2.obj;
	auto handle = slot.param1.obj;
	int gotCaught = 0;

	
	if ( !tgt 
		|| (objects.GetFlags(handle) & (OF_OFF | OF_DESTROYED))
		|| (objects.GetFlags(tgt) & (OF_OFF | OF_DESTROYED)) )
		return FALSE;

	critterSys.Pickpocket(handle, tgt, gotCaught);
	if (!gotCaught){
		slot.flags |= AnimSlotFlag::ASF_UNK12;
	}
	return TRUE;
}

int GoalStateFuncs::GoalStateFunc83(AnimSlot &slot) {
  //logger->debug("GSF83 for {}, current goal {} ({})", description.getDisplayName(slot.animObj), animGoalTypeNames[slot.pCurrentGoal->goalType], slot.currentGoal);
  auto flags = slot.flags;
  if (flags & AnimSlotFlag::ASF_UNK3 && !(flags & AnimSlotFlag::ASF_UNK4)) {
    slot.flags = flags | AnimSlotFlag::ASF_UNK4;
	//logger->debug("GSF83 return TRUE");
    return TRUE;
  } else {
    return FALSE;
  }
}

static BOOL goalstatefunc_45(AnimSlot &slot) {
	//logger->debug("GSF45");
  auto obj = slot.param1.obj;
  assert(obj);
  auto aasHandle = objects.GetAnimHandle(obj);
  assert(aasHandle);

  auto animId = aasHandle->GetAnimId();
  return animId.IsConjuireAnimation() ? TRUE : FALSE;
}

static BOOL goalstatefunc_41(AnimSlot &slot) {
	//logger->debug("GSF41");
  auto spell = spellsCastRegistry.get(slot.param1.number);
  return FALSE;
}

static BOOL goalstatefunc_42(AnimSlot &slot) {
	//logger->debug("GSF42");
  auto obj = slot.param1.obj;
  assert(obj);

  auto spellId = slot.param2.number;
  slot.flags |= 0xCu; // Sets 8 and 4

  if (spellId) {
    actSeqSys.ActionFrameProcess(obj);
    pySpellIntegration.SpellTrigger(spellId, SpellEvent::SpellEffect);

    auto spell = spellsCastRegistry.get(spellId);
    if (spell) {
      auto targetCount = spell->spellPktBody.targetCount;
      bool found = false;
      for (uint32_t i = 0; i < targetCount; i++) {
        if (spell->spellPktBody.targetListHandles[i] ==
            spell->spellPktBody.caster) {
          found = true;
          break;
        }
      }

      if (found) {
        DispIOTurnBasedStatus dispIo;
        dispIo.tbStatus = actSeqSys.curSeqGetTurnBasedStatus();
        dispatch.dispatchTurnBasedStatusInit(spell->spellPktBody.caster,
                                             &dispIo);
      }
    }
  }

  return TRUE;
}

static class AnimSystemHooks : public TempleFix {
public:

  static void Dump();

  static void RasterPoint(int64_t x, int64_t y, LineRasterPacket & s300);
  static int RasterizeLineBetweenLocs(locXY loc, locXY tgtLoc, int8_t *deltas);
  static void RasterizeLineScreenspace(int64_t x, int64_t y, int64_t tgtX, int64_t tgtY, LineRasterPacket &s300, void(__cdecl*callback)(int64_t, int64_t, LineRasterPacket &));

  static BOOL TargetDistChecker(objHndl handle, objHndl tgt);

  void apply() override {


	  // Animation pathing stuff
	  replaceFunction(0x1003DF30, RasterPoint);

	  replaceFunction(0x1003FB40, RasterizeLineBetweenLocs);

	  static BOOL(__cdecl *orgGoalstate_20)(AnimSlot&) = replaceFunction<BOOL(__cdecl)(AnimSlot &)>(0x1000EC10, [](AnimSlot &runSlot)  {
		  return orgGoalstate_20(runSlot);
	  });
	  //static bool useNew = false;


	  replaceFunction(0x10017BF0, TargetDistChecker);

	  // GetSlot
	  replaceFunction<BOOL(__cdecl)(AnimSlotId*, AnimSlot**)>(0x10016C40, [](AnimSlotId* runId, AnimSlot** runSlotOut){
		  return animationGoals.GetSlot(runId, runSlotOut);
	  });

	  // Push Goal
	  replaceFunction<BOOL(__cdecl)(AnimSlotGoalStackEntry*, AnimSlotId*)>(0x10056D20, [](AnimSlotGoalStackEntry* gdata, AnimSlotId*runId){
		  if (temple::GetRef<int>(0x10AA4BC4)) // animPrivEditorMode
			  return FALSE;

		  if (config.debugMessageEnable)
			  logger->debug("Goal pushed: {}", gdata->goalType);
		  return temple::GetRef<BOOL(__cdecl)(AnimSlotGoalStackEntry*, AnimSlotId*, int, int)>(0x10056600)(gdata, runId, 1, 0); // Push Goal Impl
	  });

	 
	static int (*orgProcessAnimEvt)(const TimeEvent*)	=  replaceFunction<int(const TimeEvent *)>(
			  0x1001B830, [](const TimeEvent *evt) -> int {
		auto result = 0;

		/*AnimSlotId triggerId = { evt->params[0].int32, evt->params[1].int32,
			evt->params[2].int32 };*/
		/*auto &slot = gameSystems->GetAnim().mSlots[triggerId.slotIndex];
		auto currentGoal = &slot.goals[slot.currentGoal];
		bool showNext = false;
		if (currentGoal->goalType != ag_anim_idle)
		{
			logger->debug("ProcessAnimEvent: {} Current goal: {}, current state: {:x}, current flags: {:x}", description.getDisplayName(slot.animObj), animGoalTypeNames[currentGoal->goalType], slot.currentState, slot.flags);
			showNext = true;
		}*/

		//if (useNew)	{
			result = gameSystems->GetAnim().ProcessAnimEvent(evt);
			temple::GetRef<int>(0x102B2654) = gameSystems->GetAnim().mCurrentlyProcessingSlotIdx;
		/*} 
		else{
			result = orgProcessAnimEvt(evt);
			if (!result){
				int dummy = 1;
			}
		}*/

		//currentGoal = &slot.goals[slot.currentGoal];
		/*if (showNext || (slot.pCurrentGoal && slot.pCurrentGoal->goalType != ag_anim_idle )){
			logger->debug("ProcessAnimEvent: {} After processing - Current goal: {}, current state: {:x}, current flags {:x}", description.getDisplayName(slot.animObj), animGoalTypeNames[slot.pCurrentGoal->goalType], slot.currentState, slot.flags);
		}*/

		return result;
			 
			  //return TRUE;
		  });
	  


	// AnimSlotInterruptGoals
	static BOOL(* orgInterruptGoals)(AnimSlotId &, AnimGoalPriority) = replaceFunction<BOOL(AnimSlotId &, AnimGoalPriority)>(0x10056090, [](AnimSlotId &animId, AnimGoalPriority priority) {
		auto &slot = gameSystems->GetAnim().mSlots[animId.slotIndex];
		auto result = 0;

		/*if (priority == 5)
		{
			int asd = 0;
		}

		logger->debug("InterruptGoals: Interrupting slot {} (cur. goal {}, flags {:x}) with priority {}", slot.id.slotIndex, animGoalTypeNames[slot.pCurrentGoal->goalType] ,slot.flags, static_cast<uint32_t>(priority));
		if (useNew)	{*/
			result = gameSystems->GetAnim().InterruptGoals(slot, priority);
			//return result ? TRUE : FALSE;
		/*} 
		else {
			result=  orgInterruptGoals(animId, priority);
		}
		logger->info("InterruptGoals: slot {} new cur. goal is {}, flags {:x}", slot.id.slotIndex, slot.pCurrentGoal?animGoalTypeNames[slot.pCurrentGoal->goalType]:"none", slot.flags);
		*/
		return result;
	});

	// anim_pop_goal
	 static void(*orgPopGoal)(AnimSlot &, const uint32_t *, const AnimGoal **,  AnimSlotGoalStackEntry **, BOOL *) = replaceFunction<void(AnimSlot &, const uint32_t *, const AnimGoal **, AnimSlotGoalStackEntry **, BOOL *)>
		 (   0x10016FC0,
	    [](AnimSlot &slot, const uint32_t *popFlags, const AnimGoal **newGoal,
	       AnimSlotGoalStackEntry **newCurrentGoal, BOOL *stopProcessing) {
	    
		 //bool useNew = true;

		// if (useNew) {
			 bool stopProcessingBool = *stopProcessing == TRUE;
			 gameSystems->GetAnim().PopGoal(slot, *popFlags, newGoal,
				 newCurrentGoal, &stopProcessingBool);
			 *stopProcessing = stopProcessingBool ? TRUE : FALSE;
		/* } 
		 else	 {
			 orgPopGoal(slot, popFlags, newGoal, newCurrentGoal, stopProcessing);

			 if (slot.flags & ASF_STOP_PROCESSING)
				 logger->debug("Pop goal for {}: stopping processing (last goal was {}).", description.getDisplayName(slot.animObj), animGoalTypeNames[slot.pCurrentGoal->goalType]);
			 else
			 {
				 logger->debug("Pop goal for {}: new goal is {}.", description.getDisplayName(slot.animObj), animGoalTypeNames[slot.pCurrentGoal->goalType]);
			 }
		 }*/
		   
	    });

	 replaceFunction<void()>(0x10016A30, [](){
		 gameSystems->GetAnim().ProcessActionCallbacks();
	 });


	// goalstatefunc_133
	replaceFunction<BOOL(AnimSlot &)>(0x1001C100, gsFuncs.GoalStateFunc133);
    // goalstatefunc_106
    replaceFunction<BOOL(AnimSlot &)>(0x100185e0, gsFuncs.GoalStateFunc106);
	// goalstatefunc_100
	replaceFunction<BOOL(AnimSlot &)>(0x1000D150, gsFuncs.GoalStateFunc100_IsCurrentPathValid);
    // goalstatefunc_83_checks_flag4_set_flag8
    replaceFunction<BOOL(AnimSlot &)>(0x10012c80, gsFuncs.GoalStateFunc83);
	// goalstatefunc_82
	replaceFunction<BOOL(AnimSlot &)>(0x10012C70, gsFuncs.GoalStateFunc82);
	//goalstatefunc_78_isheadingok
	replaceFunction<BOOL(AnimSlot &)>(0x100125F0, gsFuncs.GoalStateFunc78_IsHeadingOk);
	//goalstatefunc_70
//	replaceFunction<BOOL(AnimSlot &)>(0x10011D40, gsFuncs.GoalStateFunc70);  // not sure this is correct, not needed anyway right now
	// goalstatefunc_65
	replaceFunction<BOOL(AnimSlot &)>(0x10011880, gsFuncs.GoalStateFunc65);
	// gsf54
	replaceFunction<BOOL(AnimSlot &)>(0x10010D60, gsFuncs.GoalStateFunc54);
	// goalstatefunc_48
	replaceFunction<BOOL(AnimSlot &)>(0x100107E0, gsFuncs.GoalStateFuncPickpocket);
    // goalstatefunc_45
    replaceFunction<BOOL(AnimSlot &)>(0x10010520, goalstatefunc_45);
    // goalstatefunc_41
    replaceFunction<BOOL(AnimSlot &)>(0x10010290, goalstatefunc_41);
    // goalstatefunc_42
    replaceFunction<BOOL(AnimSlot &)>(0x100102c0, goalstatefunc_42);
	//goalstatefunc_35
	replaceFunction<BOOL(AnimSlot&)>(0x1000FF10, gsFuncs.GoalStateFunc35);
	//goalstate is concealed
	replaceFunction<BOOL(AnimSlot&)>(0x1000E250, gsFuncs.GoalStateFuncIsParam1Concealed);
	// goal_is_critter_prone
	replaceFunction<BOOL(AnimSlot&)>(0x1000E270, gsFuncs.GoalStateFuncIsCritterProne);


    // Register a debug function for dumping the anims
    RegisterDebugFunction("dump_anim_goals", Dump);
  }
} animHooks;

static string getDelayText(AnimStateTransition trans) {
  string delay = "";
  if (trans.delay == AnimStateTransition::DelayRandom) {
    delay = ", delay: random";
  } else if (trans.delay == AnimStateTransition::DelayCustom) {
    delay = ", delay: custom";
  } else if (trans.delay == AnimStateTransition::DelaySlot) {
    delay = ", delay: slot";
  } else if (trans.delay != 0) {
    delay = format(", delay: {}", trans.delay);
  }
  return delay;
}

static void getTransitionText(string &diagramText, int &j,
                              const AnimStateTransition &transition,
                              const char *condition) {
  string delay = getDelayText(transition);
  auto newState = transition.newState;
  if (newState & 0xFF000000) {
    logger->info("New state flags {:x}", newState);
    if ((newState & 0x30000000) == 0x30000000) {
      diagramText += format("state{} --> [*] : [{}{}]\n", j, condition, delay);
    } else if ((newState & TRANSITION_GOAL) == TRANSITION_GOAL) {
      auto newGoal = newState & 0xFFF;
      diagramText += format("state{} --> [*] : [{}{}] to {}\n", j, condition,
                            delay, animGoalTypeNames[newGoal]);
    } else if ((newState & TRANSITION_UNK1) == TRANSITION_UNK1) {
      diagramText += format("state{} --> [*] : [{}{}, flags: 0x90]\n", j,
                            condition, delay);
    } else if (newState & TRANSITION_LOOP) {
      diagramText +=
          format("state{} --> state0 : [{}{}, reset]\n", j, condition, delay);
    } else {
      diagramText += format("state{} --> state0 : [{}{}, flags: {}]\n", j,
                            condition, delay, newState);
    }
  } else {
    // Normal transition
    diagramText += format("state{} --> state{} : [{}{}]\n", j, newState - 1,
                          condition, delay);
  }
}

std::string GetAnimParamName(int animParamType) {
  if (animParamType < 21) {
    return AnimGoalDataNames[animParamType];
  } else if (animParamType == 31) {
    return "SELF_OBJ_PRECISE_LOC";
  } else if (animParamType == 32) {
    return "TARGET_OBJ_PRECISE_LOC";
  } else if (animParamType == 33) {
    return "NULL_HANDLE";
  } else if (animParamType == 34) {
    return "TARGET_LOC_PRECISE";
  } else {
    return to_string(animParamType);
  }
}

static json11::Json::object
TransitionToJson(const AnimStateTransition &transition) {
  using namespace json11;
  Json::object result;

  if (transition.delay == 0) {
    result["delay"] = nullptr;
  } else if (transition.delay == AnimStateTransition::DelayCustom) {
    result["delay"] = "custom";
  } else if (transition.delay == AnimStateTransition::DelayRandom) {
    result["delay"] = "random";
  } else if (transition.delay == AnimStateTransition::DelaySlot) {
    result["delay"] = "slot";
  } else {
    result["delay"] = transition.delay;
  }

  result["newState"] = (int)(transition.newState & 0xFFFFFF);

  auto flags = (int)((transition.newState >> 24) & 0xFF);
  result["flags"] = flags;

  return result;
}

static json11::Json::object StateToJson(const AnimGoalState &state,
                                        map<uint32_t, string> &goalFuncNames,
                                        map<uint32_t, string> &goalFuncDescs) {
  using namespace json11;
  Json::object result{
      {"callback", fmt::format("0x{:x}", (uint32_t)state.callback)},
      {"name", goalFuncNames[(uint32_t)state.callback]},
      {"description", goalFuncDescs[(uint32_t)state.callback]},
      {"refToOtherGoalType", state.refToOtherGoalType}};

  if (state.afterSuccess.newState == state.afterFailure.newState &&
      state.afterSuccess.delay == state.afterFailure.delay) {
    result["transition"] = TransitionToJson(state.afterSuccess);
  } else {
    result["trueTransition"] = TransitionToJson(state.afterSuccess);
    result["falseTransition"] = TransitionToJson(state.afterFailure);
  }

  if (state.argInfo1 == -1) {
    result["arg1"] = Json();
  } else {
    result["arg1"] = GetAnimParamName(state.argInfo1);
  }

  if (state.argInfo2 == -1) {
    result["arg2"] = Json();
  } else {
    result["arg2"] = GetAnimParamName(state.argInfo2);
  }

  return result;
}

BOOL AnimSlotGoalStackEntry::InitWithInterrupt(objHndl obj, AnimGoalType goalType){
	return temple::GetRef<BOOL(AnimSlotGoalStackEntry *, objHndl, AnimGoalType )>(0x100556C0)(this, obj, goalType);
}

BOOL AnimSlotGoalStackEntry::Push(AnimSlotId* idNew){
	return temple::GetRef<BOOL(__cdecl)(AnimSlotGoalStackEntry *, AnimSlotId*)>(0x10056D20)(this, idNew);
}

BOOL AnimSlotGoalStackEntry::Init(objHndl handle, AnimGoalType goalType, BOOL withInterrupt){
	auto gdata = this;

	if (!gdata){
		logger->error("Null goalData ptr");
		animationGoals.Debug();
	}

	if ( (goalType & 0x80000000) || goalType >= ag_count){
		logger->error("Illegal goalType");
		animationGoals.Debug();
	}
	gdata->animId.number = -1;
	gdata->animIdPrevious.number = -1;
	gdata->animData.number = -1;
	gdata->spellData.number = -1;
	gdata->flagsData.number = -1;
	gdata->soundStreamId = -1;
	gdata->goalType = goalType;
	gdata->self.obj = handle;
	gdata->target.obj = objHndl::null;
	gdata->block.obj = objHndl::null;
	gdata->scratch.obj = objHndl::null;
	gdata->parent.obj = objHndl::null;
	gdata->targetTile.location = LocAndOffsets::null;
	gdata->range.location = LocAndOffsets::null;
	gdata->skillData.number = 0;
	gdata->scratchVal1 .number= 0;
	gdata->scratchVal2 .number = 0;
	gdata->scratchVal3.number = 0;
	gdata->scratchVal4.number = 0;
	gdata->scratchVal5.number = 0;
	gdata->scratchVal6.number = 0;
	gdata->soundHandle.number = 0;
	if (withInterrupt)
	{
		auto ag = animationGoals.GetGoal(goalType);
		if (!ag){
			logger->error("pGoalNode != NULL assertion failed");
			animationGoals.Debug();
		}
		return animationGoals.Interrupt(handle, ag->priority, ag->interruptAll);
	}
	return TRUE;
}

AnimSlotGoalStackEntry::AnimSlotGoalStackEntry(objHndl handle, AnimGoalType, BOOL withInterrupt){
	Init(handle, goalType, withInterrupt);
}

int GoalStateFuncs::GoalStateFunc35(AnimSlot& slot)
{
	//logger->debug("GoalStateFunc35");
	if (slot.param1.obj)
	{
		if (!gameSystems->GetObj().GetObject(slot.param1.obj)->IsCritter()
			|| !critterSys.IsDeadNullDestroyed(slot.param1.obj))
			return 1;
	}

	return 0;
}

int GoalStateFuncs::GoalStateFunc54(AnimSlot& slot){
	//logger->debug("GSF54 ag_attempt_attack action frame");
	assert(slot.param1.obj);
	assert(slot.param2.obj);

	if (! (gameSystems->GetObj().GetObject(slot.param2.obj)->GetFlags() & (OF_OFF | OF_DESTROYED)) )
	{
		actSeqSys.ActionFrameProcess(slot.param1.obj);
	}
	return TRUE;
}

int GoalStateFuncs::GoalStateFunc133(AnimSlot& slot)
{
	// 1001C100
	//logger->debug("GoalStateFunc133");
	auto sub_10017BF0 = temple::GetRef<BOOL(__cdecl)(objHndl, objHndl)>(0x10017BF0);
	auto result = sub_10017BF0(slot.param1.obj, slot.param2.obj);
	if (!result)
	{
		if (combatSys.isCombatActive())
		{
			logger->debug("Anim sys for {} ending turn...", description.getDisplayName(slot.param1.obj));
			combatSys.CombatAdvanceTurn(slot.param1.obj);
		}
	}

	return result;
}

int GoalStateFuncs::GoalStateFuncIsCritterProne(AnimSlot& slot)
{
	// 1000E270
	//logger->debug("GoalStateFunc IsCritterProne");
	if (slot.param1.obj){
		return objects.IsCritterProne(slot.param1.obj);
	}

	logger->debug("Anim Assertion failed: obj != OBJ_HANDLE_NULL");
	return FALSE;
}

int GoalStateFuncs::GoalStateFuncIsParam1Concealed(AnimSlot& slot)
{
	//logger->debug("GoalState IsConcealed");
	return (critterSys.IsConcealed(slot.param1.obj));
}

int GoalStateFuncs::GoalStateFunc78_IsHeadingOk(AnimSlot & slot){
	if (!slot.pCurrentGoal) {
		slot.pCurrentGoal = &slot.goals[slot.currentGoal];
	}

	if (slot.path.nodeCount <= 0)
		return 1;

	// get the mover's location
	auto obj = gameSystems->GetObj().GetObject(slot.param1.obj);
	auto objLoc = obj->GetLocationFull();
	float objAbsX, objAbsY;
	locSys.GetOverallOffset(objLoc, &objAbsX, &objAbsY);

	// get node loc
	if (slot.path.currentNode > 200 || slot.path.currentNode < 0) {
		logger->info("Anim: Illegal current node detected!");
		return 1;
	}

	auto nodeLoc = slot.path.nodes[slot.path.currentNode];
	float nodeAbsX, nodeAbsY;
	locSys.GetOverallOffset(nodeLoc, &nodeAbsX, &nodeAbsY);

	if (objAbsX == nodeAbsX && objAbsY == nodeAbsY) {
		if (slot.path.currentNode + 1 >= slot.path.nodeCount)
			return 1;
		nodeLoc = slot.path.nodes[slot.path.currentNode + 1];
		locSys.GetOverallOffset(nodeLoc, &nodeAbsX, &nodeAbsY);
	}

	auto &rot = slot.pCurrentGoal->scratchVal2.floatNum;
	rot = (float)(M_PI_2 + M_PI * 0.75 - atan2(nodeAbsY - objAbsY, nodeAbsX - objAbsX) );
	if (rot < 0.0) {
		rot += (float)6.2831853;//M_PI * 2;
	}
	if (rot > 6.2831853) {
		rot -= (float) 6.2831853;//M_PI * 2;
	}

	
	auto objRot = obj->GetFloat(obj_f_rotation);


	if (sin(objRot - rot) > 0.017453292)    // 1 degree
		return 0;
	
	if (cos(objRot) - cos(rot) > 0.017453292) // in case it's a 180 degrees difference
		return 0;

	return 1;
}

int GoalStateFuncs::GoalStateFunc82(AnimSlot& slot)
{ //10012C70
	/*if (slot.pCurrentGoal && slot.pCurrentGoal->goalType != ag_anim_idle) {
		logger->debug("GSF82 for {}, current goal {} ({}). Flags: {:x}, currentState: {:x}", description.getDisplayName(slot.animObj), animGoalTypeNames[slot.pCurrentGoal->goalType], slot.currentGoal, slot.flags, slot.currentState);
		if(slot.pCurrentGoal->goalType == ag_hit_by_weapon)
		{
			int u = 1;
		}
	}*/
	//return (slot.flags & AnimSlotFlag::ASF_UNK5) == 0? TRUE: FALSE;
	return (~(slot.flags >> 4)) & 1;
	//return ~(slot.flags >> 4) & 1;
}

int GoalStateFuncs::GoalStateFunc65(AnimSlot& slot)
{
	//logger->debug("GSF65");
	if (!slot.param1.obj)
	{
		logger->warn("Error in GSF65");
		return FALSE;
	}
	auto obj = gameSystems->GetObj().GetObject(slot.param1.obj);
	auto locFull = obj->GetLocationFull();
	float worldX, worldY;
	locSys.GetOverallOffset(locFull, &worldX, &worldY);
	
	auto obj2 = gameSystems->GetObj().GetObject(slot.param2.obj);
	auto loc2 = obj2->GetLocationFull();
	float worldX2, worldY2;
	locSys.GetOverallOffset(loc2, &worldX2, &worldY2);

	auto rot = obj->GetFloat(obj_f_rotation);

	auto newRot = atan2(worldY2 - worldY, worldX2 - worldX) + M_PI * 3 / 4 - rot;
	while (newRot > M_PI * 2) newRot -= M_PI * 2;
	while (newRot < 0) newRot += M_PI * 2;

	auto newRotAdj = newRot - M_PI / 4;

	auto weaponIdParam = 10;
	if (newRotAdj < M_PI / 4)
		weaponIdParam = 10;
	else if (newRotAdj < M_PI*3/4)
		weaponIdParam = 13;
	else if (newRotAdj < M_PI * 5 / 4)
		weaponIdParam = 19;
	else if (newRotAdj < M_PI * 7 / 4)
		weaponIdParam = 16;

	auto critterGetWeaponAnimId = temple::GetRef<int(__cdecl)(objHndl, int)>(0x10020C60);
	auto weaponAnimId = critterGetWeaponAnimId(slot.param1.obj, weaponIdParam);

	auto anim_obj_set_aas_anim_id = temple::GetRef<int(__cdecl)(objHndl, int)>(0x10021D50);
	anim_obj_set_aas_anim_id(slot.param1.obj, weaponAnimId);
	return TRUE;

}

int GoalStateFuncs::GoalStateFunc70(AnimSlot & slot)
{
	auto someGoal = slot.someGoalType;

	if (someGoal == -1){
		SpellPacketBody spPkt;
		spellSys.GetSpellPacketBody(slot.param1.number, &spPkt);
		if (spPkt.animFlags & 8){
			slot.pCurrentGoal->animId.number = spPkt.spellRange; // weird shit!
			return TRUE;
		}
		return FALSE;
	} 
	else{
		slot.pCurrentGoal->animId.number = someGoal;
		return TRUE;
	}
}

int GoalStateFuncs::GoalStateFunc100_IsCurrentPathValid(AnimSlot & slot)
{
	return slot.path.flags & PathFlags::PF_COMPLETE;
}

void AnimSystemHooks::Dump() {

  map<uint32_t, string> goalFuncNames;
  map<uint32_t, string> goalFuncDescs;
#define MakeName(a, b) goalFuncNames[a] = b
#define MakeDesc(a, b) goalFuncDescs[a] = b
#include "goalfuncnames.h"

  auto outputFolder = GetUserDataFolder() + L"animationGoals";
  CreateDirectory(outputFolder.c_str(), NULL);

  auto animSys = gameSystems->GetAnim();

  using namespace json11;
  Json::array goalsArray;

  for (int i = 0; i < ag_count; ++i) {
    auto goal = animSys.mGoals[i];
    if (!goal)
      continue;
    auto goalName = animGoalTypeNames[i];

    Json::object goalObj{{"id", i},
                         {"name", goalName},
                         {"priority", (int)goal->priority},
                         {"field8", goal->interruptAll},
                         {"fieldc", goal->field_C},
                         {"field10", goal->field_10},
                         {"relatedGoal1", goal->relatedGoal1},
                         {"relatedGoal2", goal->relatedGoal2},
                         {"relatedGoal3", goal->relatedGoal3}};
    if (goal->state_special.callback) {
      auto specialState =
          StateToJson(goal->state_special, goalFuncNames, goalFuncDescs);
      // Those are not used for the cleanup
      specialState.erase("transition");
      specialState.erase("trueTransition");
      specialState.erase("falseTransition");
      specialState.erase("refToOtherGoalType");
      goalObj["specialState"] = specialState;
    }

    std::vector<Json::object> states;

    string diagramText = "@startuml\n";

    diagramText += "[*] --> state0\n";

    for (int j = 0; j < goal->statecount; ++j) {
      auto &state = goal->states[j];

      states.push_back(StateToJson(state, goalFuncNames, goalFuncDescs));

      auto stateText = "**" + goalFuncNames[(uint32_t)state.callback] + "**";
      auto stateDesc = goalFuncDescs[(uint32_t)state.callback];

      if (state.callback && stateText == "****") {
        logger->warn("No name for goal func {}", (void *)state.callback);
      }

      auto param1 = state.argInfo1;
      if (param1 != -1) {
        stateText += "\\nParam 1: " + GetAnimParamName(param1);
      }

      auto param2 = state.argInfo2;
      if (param2 != -1) {
        stateText += "\\nParam 2:" + GetAnimParamName(param2);
      }

      if (state.afterSuccess.newState == state.afterFailure.newState &&
          state.afterSuccess.delay == state.afterFailure.delay) {
        getTransitionText(diagramText, j, state.afterSuccess, "always");
      } else {
        getTransitionText(diagramText, j, state.afterSuccess, "true");
        getTransitionText(diagramText, j, state.afterFailure, "false");
      }

      diagramText += format("state \"{}\" as state{}\n", stateText, j);
      if (!stateDesc.empty()) {
        diagramText += format("state{} : {}\n", j, stateDesc);
      }
    }

    goalObj["states"] = states;
    goalsArray.push_back(goalObj);

    diagramText += "\n@enduml\n";

    ofstream o(format(L"{}/{:02d}_{}.txt", outputFolder, i, goalName));
    o << diagramText;
  }

  ofstream jsonO(format(L"{}/goals.json", outputFolder));
  jsonO << Json(goalsArray).dump();

  logger->info("DONE");
}

void AnimSystemHooks::RasterPoint(int64_t x, int64_t y, LineRasterPacket & rast)
{
	auto someIdx = rast.deltaIdx;
	if (someIdx == -1 || someIdx >= 200) {
		rast.deltaIdx = -1;
		return;
	}

	rast.counter++;

	if (rast.counter == rast.interval) {
		rast.deltaXY[someIdx] = x - rast.x;
		rast.deltaXY[rast.deltaIdx + 1] = y - rast.y;
		rast.x = x;
		rast.y = y;
		rast.deltaIdx += 2;
		rast.counter = 0;
	}
}

int AnimSystemHooks::RasterizeLineBetweenLocs(locXY loc, locXY tgtLoc, int8_t * deltas){
	// implementation of the Bresenham line algorithm
	LineRasterPacket rast;
	int64_t locTransX, locTransY, tgtTransX, tgtTransY;
	auto getTranslation = temple::GetRef<void(__cdecl)(locXY, int64_t &, int64_t &)>(0x10028E10);
	getTranslation(loc, locTransX, locTransY);
	locTransX += 20;
	locTransY += 14;
	getTranslation(tgtLoc, tgtTransX, tgtTransY);
	tgtTransX += 20;
	tgtTransY += 14;

	rast.x = locTransX;
	rast.y = locTransY;
	rast.deltaXY = deltas;

	animHooks.RasterizeLineScreenspace(locTransX, locTransY, tgtTransX, tgtTransY, rast, animHooks.RasterPoint);

	if (rast.deltaIdx == -1)
		return 0;

	while (rast.counter){
		animHooks.RasterPoint(tgtTransX, tgtTransY, rast);
	}

	return rast.deltaIdx == -1 ? 0 : rast.deltaIdx;

}

void AnimSystemHooks::RasterizeLineScreenspace(int64_t x0, int64_t y0, int64_t tgtX, int64_t tgtY, LineRasterPacket & s300, void(*callback)(int64_t, int64_t, LineRasterPacket &)){
	auto x = x0, y = y0;
	auto deltaX = tgtX - x0, deltaY = tgtY - y0;
	auto deltaXAbs = abs(deltaX), deltaYAbs = abs(deltaY);


	auto extentX = 2 * deltaXAbs, extentY = 2 * deltaYAbs;

	auto deltaXSign = 0, deltaYSign = 0;
	if (deltaX > 0)
		deltaXSign = 1;
	else if (deltaX < 0)
		deltaXSign = -1;

	if (deltaY > 0)
		deltaYSign = 1;
	else if (deltaY < 0)
		deltaYSign = -1;

	
	if (extentX <= extentY){

		int64_t D = extentX - (extentY / 2);
		callback(x0, y0, s300);
		while (y != tgtY){
			if (D >= 0){
				x += deltaXSign;
				D -= extentY;
			}
			D += extentX;
			y += deltaYSign;
			callback(x, y, s300);
		}
	} 
	else
	{
		int64_t D = extentY - (extentX / 2);
		callback(x0, y0, s300);
		while (x != tgtX){
			
			if (D >= 0){
				y += deltaYSign;
				D -= extentX;
			}
			D += extentY;
			x += deltaXSign;
			callback(x, y, s300);
		}
	}
}

BOOL AnimSystemHooks::TargetDistChecker(objHndl handle, objHndl tgt){

	if (!handle || !objects.IsCritter(handle) || critterSys.IsDeadOrUnconscious(handle)
		|| !tgt || !objects.IsCritter(tgt) || critterSys.IsDeadNullDestroyed(tgt))
		return FALSE;

	if ((objects.getInt32(handle, obj_f_spell_flags) & SpellFlags::SF_4)
		&& !(objects.getInt32(handle, obj_f_critter_flags2) & CritterFlags2::OCF2_ELEMENTAL))
	{
		if (config.debugMessageEnable)
			logger->debug("TargetDistChecker: spell flag 4 error");
		return FALSE;
	}
		
	auto tgtObj = objSystem->GetObject(tgt);
	auto tgtLoc = tgtObj->GetLocation();
	objHndl obstructor = objHndl::null;
	temple::GetRef<void(__cdecl)(objHndl, locXY, objHndl&)>(0x10058CA0)(handle, tgtLoc, obstructor);
	if (config.debugMessageEnable)
		logger->debug("TargetDistChecker: tgt2 is {}", obstructor);

	if (!obstructor
		|| obstructor == tgt|| obstructor == handle)
	{
		return TRUE;
	}
		


	AnimPath animPath;
	animPath.flags = 0;
	animPath.pathLength = 0;
	animPath.fieldE4 = 0;
	animPath.fieldD4 = 0;
	animPath.fieldD8 = 0;
	animPath.fieldD0 = 0;
	//animPath.range = 200;

	auto animpathMaker = temple::GetRef<BOOL(__cdecl)(objHndl, locXY, AnimPath&, int)>(0x10017AD0);
	
	if (animpathMaker(handle, tgtLoc, animPath, 1)){
		if (config.debugMessageEnable)
			logger->debug("animpath successful");
		return TRUE;
	}
		
	if (config.debugMessageEnable)
		logger->debug("animpath failed!");
	return FALSE;
}
