#include "stdafx.h"
#include "common.h"
#include "skill.h"
#include "bonus.h"
#include "history.h"
#include "util/fixes.h"
#include "float_line.h"
#include "d20.h"
#include <infrastructure/json11.hpp>
#include <infrastructure/vfs.h>
#include "critter.h"

#pragma region SkillSystem Implementation
LegacySkillSystem skillSys;

class SkillFunctionReplacement : public TempleFix {
public:
	static BOOL SkillRoll(objHndl performer, SkillEnum skillEnum, int dc, int* resultDeltaFromDc, int flags);

	void apply() override; 

private:
	//Old version of the function to be used within the replacement
    int (*oldSkillRoll)(objHndl, SkillEnum, int, int*, int) = nullptr;
} skillFunctionReplacement;

void SkillFunctionReplacement::apply()
{
	logger->info("Replacing Skill-related Functions");

	oldSkillRoll = replaceFunction<BOOL(objHndl, SkillEnum, int, int*, int)>(0x1007D530, SkillRoll);
}

BOOL SkillFunctionReplacement::SkillRoll(objHndl performer, SkillEnum skillEnum, int dc, int* resultDeltaFromDc, int flags)
{
	//Check if the skill requested should be swapped with a different skill roll (some abilities allow this)
	auto swapSkill = d20Sys.D20QueryPython(performer, "Skill Swap", skillEnum);
	
	// A non zero return means that the value - 1 is the skill to swap out with
	if (swapSkill > 0) {
		skillEnum = static_cast<SkillEnum>(swapSkill - 1);
	}

	if (!d20Sys.D20QueryPython(performer, "PQ_Take_Ten", skillEnum)) {
		return skillFunctionReplacement.oldSkillRoll(performer, skillEnum, dc, resultDeltaFromDc, flags);
	}
	else { // Added by Sagenlicht; Taking 10 function for skills
		LegacySkillSystem skillCheck;
		return skillCheck.TakeTen(performer, skillEnum, dc, flags);
	}
	
}

bool LegacySkillSystem::IsEnabled(SkillEnum skillEnum) const{
	return !(skillPropsTable[skillEnum].classFlags & 0x80000000);
}

BOOL LegacySkillSystem::SkillRoll(objHndl performer, SkillEnum skillEnum, int dc, int* resultDeltaFromDc, int flags) const
{
	return skillFunctionReplacement.SkillRoll(performer, skillEnum, dc, resultDeltaFromDc, flags);
}

void LegacySkillSystem::FloatError(const objHndl& obj, int errorOffset){
	MesLine mesline(1000 + errorOffset);
	auto skillMes = temple::GetRef<MesHandle>(0x10AB7158);
	mesFuncs.GetLine_Safe(skillMes, &mesline);
	floatSys.floatMesLine(obj, 1, FloatLineColor::White, mesline.value);
}

// Originally @ 0x1007D720

BOOL LegacySkillSystem::SkillCheckDefaultDC(SkillEnum skillEnum, objHndl performer, int flag)
{
	if (skillPropsTable[skillEnum].stat == Stat::stat_intelligence && d20Sys.d20Query(performer, D20DispatcherKey::DK_QUE_CannotUseIntSkill))
		return FALSE;
	if (skillPropsTable[skillEnum].stat == Stat::stat_charisma && d20Sys.d20Query(performer, D20DispatcherKey::DK_QUE_CannotUseChaSkill))
		return FALSE;
	if (!skillPropsTable[skillEnum].unskilledUseAllow && critterSys.SkillBaseGet(performer, skillEnum) <= 0)
		return FALSE;

	BonusList bonList;
	auto skillResult = dispatch.dispatch1ESkillLevel(performer, skillEnum, &bonList, objHndl::null, flag);
	auto roll = Dice(1, 20, 0).Roll();
	return roll + skillResult >= 10;
}

BOOL LegacySkillSystem::TakeTen(objHndl performer, SkillEnum skillEnum, int dc, int flags)
{
	if (skillPropsTable[skillEnum].stat == Stat::stat_intelligence && d20Sys.d20Query(performer, D20DispatcherKey::DK_QUE_CannotUseIntSkill))
		return FALSE;
	if (skillPropsTable[skillEnum].stat == Stat::stat_charisma && d20Sys.d20Query(performer, D20DispatcherKey::DK_QUE_CannotUseChaSkill))
		return FALSE;
	if (!skillPropsTable[skillEnum].unskilledUseAllow && critterSys.SkillBaseGet(performer, skillEnum) <= 0)
		return FALSE;

	BonusList bonList;
	auto skillBonus = dispatch.dispatch1ESkillLevel(performer, skillEnum, &bonList, objHndl::null, flags);
	Dice dice(0, 0, 10);
	auto skillRoll = dice.Roll();
	// RollHistoryAddType2SkillRoll is bugged, I have to switch skillEnum and dc in params to get correct result in history window
	int rollHistId = histSys.RollHistoryAddType2SkillRoll(performer, dice.ToPacked(), skillRoll, skillEnum, dc, &bonList);
	histSys.CreateRollHistoryString(rollHistId);
	return skillRoll + skillBonus >= dc;
}

const char* LegacySkillSystem::GetSkillName(SkillEnum skillEnum)
{
	return temple::GetRef<const char* []>(0x10AB70B0)[skillEnum];
}

const char * LegacySkillSystem::GetSkillHelpTopic(SkillEnum skillEnum){
	MesLine mesline(10200 + skillEnum);
	auto skillRulesMes = temple::GetRef<MesHandle>(0x10AB72B8);
	if (mesFuncs.GetLine(skillRulesMes, &mesline))
		return mesline.value;
	return nullptr;
}

Stat LegacySkillSystem::GetSkillStat(SkillEnum skillEnum)
{
	return skillPropsTable[skillEnum].stat;
}

LegacySkillSystem::LegacySkillSystem(){
	bonus = &bonusSys;
	macRebase(skillPropsTable, 102CBA30)
}

void LegacySkillSystem::Init()
{
	LoadSkillsProps("rules\\skills_props.json");
}

void LegacySkillSystem::DoForAllSkills( std::function<void(SkillEnum)> cb, bool activeOnly)
{
	for (auto i = 0; i < SkillEnum::skill_count; ++i) {
		auto skEnum = (SkillEnum)i;
		if (activeOnly && !IsEnabled(skEnum)) {
			continue;
		}

		cb(skEnum);
	}
}

void LegacySkillSystem::LoadSkillsProps(const std::string& path)
{
	std::string error;
	if (!vfs->FileExists(path)) return;
	json11::Json json = json.parse(vfs->ReadAsString(path), error);

	if (json.is_null()) {
		throw TempleException("Unable to parse skills_props.json from {}: {}", path, error);
	}

	if (!json.is_array()) {
		throw TempleException("skills_props.json must start with an array at the root");
	}

	for (auto& item : json.array_items()) {
		if (!item.is_object()) {
			logger->warn("Skipping skill that is not an object.");
			continue;
		}
		auto idNode = item["id"];
		if (!idNode.is_number()) {
			logger->warn("Skipping skill that is missing 'id' attribute.");
			continue;
		}
		uint32_t id = (uint32_t)item["id"].int_value();
		if (id < 0 || id >= SkillEnum::skill_count) {
			logger->warn("Skipping skill that is out of bounds.");
			continue;
		}

		if (!item["enabled"].is_number()) {
			logger->warn("Skipping skill that is missing 'enabled' int attribute.");
			continue;
		}
		uint32_t enabled = (uint32_t)item["enabled"].int_value();
		if (enabled)
			skillPropsTable[id].classFlags &= ~0x80000000;
		else skillPropsTable[id].classFlags |= 0x80000000;
	}
}
#pragma endregion