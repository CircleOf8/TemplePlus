from toee import *
import char_editor

def CheckPrereq(attachee, classLevelled, abilityScoreRaised):

	# BAB enforced via prefeq properties
	#if attachee.get_base_attack_bonus() < 5:
	#	return 0
	
	#Check for any favored enemy feat
	for i in range (feat_favored_enemy_aberration , feat_favored_enemy_humanoid_human):
		if char_editor.has_feat(i):
			return 1
	
	return 0
