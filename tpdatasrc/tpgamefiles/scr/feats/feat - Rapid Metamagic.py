from toee import *
import char_editor

def CheckPrereq(attachee, classLevelled, abilityScoreRaised):
	spont_casting_lvl = attachee.spontaneous_spell_level_can_cast()
	if spont_casting_lvl <= 0:
		return 0
	spellcraft_level = char_editor.skill_ranks_get(skill_spellcraft)
	if spellcraft_level < 12:
		return 0
	return 1