from utilities import *

from math import *
import random_encounter

import os
import sys
from toee import *
#name convention for encounterpacks is EncounterPack + Creator name + pack number
#the file must contain a function called EncounterExpander which takes 4 parameters (setup, encounter, re_list, m)
#and returns the list of encounters re_list
expand = False
if(os.path.exists('.\encounterpacks')):
	try:
		sys.path.insert(0, '.\encounterpacks')
		encounterpacks = os.listdir('.\encounterpacks')    #to install a new encounterpack simply create a folder called encounterpacks in your main TOEE folder and save the file there
		for f in encounterpacks:
			print (f)
		from py00439script_daemon import *
		from py00334generic_spawner import *
		from co8Util.ObjHandling import *
		from co8Util.PersistentData import *
		expand = True
	except ImportError:
		expand = False
class RE_entry:
	# Description:

	# RE_entry is an object that is initialized in the following format:
	#	 RE_entry( base DC,    ( (proto, min, max), (proto2, min, max, [optional - DC increment for each one of proto2]) ),      encounter ID (no idea how it's used?) )
	# You may then use the method get_enemies() to get a normal list of enemies, as the game expects.
	# It's more convenient than handling a bunch of 'ifs'.
	# Note that the number of protos is not limited, and you can add the optional DC modifier to any of them
	# e.g.
	#	RE_entry( 2 ,    ( (14053, 1, 3, 0.5), (14052, 2, 2) ),      1000 )
	#	returns an object RE_entry with the above proto lists, DC etc.

	# usage:
	# For every RE entry you want, write:
	# 	re_list.append(RE_entry(... ) )
	# The above creates a list of RE_entry objects.
	# Then - the script randomly selects from the list, and uses the get_enemies method to return the final list
	enemies = ()
	dc = 0
	def __init__(self, dc_in = 1, enemy_in = ( (14070, 1 , 1 , 0.5), ), id_in = 1000):
		self.dc_base = dc_in
		self.enemy_pool = enemy_in
		self.id = id_in
	def get_enemies(self):
		enemy_list_output = ()
		dc_mod = 0.0
		for tup_x in self.enemy_pool:
			nn = game.random_range( tup_x[1], tup_x[2] )
			if nn > 0:
				enemy_list_output += ( ( tup_x[0], nn ), )
				if len(tup_x) == 4: # DC modifier
					dc_mod += tup_x[3] * nn
				
		self.enemies = enemy_list_output
		self.dc = self.dc_base + int(dc_mod)
		return enemy_list_output




def encounter_exists( setup, encounter ):
	if not expand:
		return random_encounter.encounter_exists(setup, encounter)
	print "Testing encounter_exists"
	if (setup.flags & ES_F_SLEEP_ENCOUNTER):
		check = check_sleep_encounter(setup,encounter)
	else:
		check = check_random_encounter(setup,encounter)
	# Added by Sitra Achara
	if game.party[0].map in [5066, 5067, 5005]:
		# If you rest inside the Temple or Moathouse, execute the Reactive Behavior scripts
		san_new_map(game.party[0], game.party[0])
	if game.party[0].map == 5095:
		# # Resting in Hickory Branch - execute reactive behavior
		hickory_branch_check() # run scripting for enabling lieutenants inside the cave
		hb_blockage_obj = refHandle( Co8PersistentData.getData( "HB_BLOCKAGE_SERIAL" ) ) # get a handle on the cave blockage and then enable it
		if (hb_blockage_obj != OBJ_HANDLE_NULL) and ( str(hb_blockage_obj).find('Blockage') >= 0 ):
			hb_blockage_obj.object_script_execute( hb_blockage_obj, 10 ) # execute its san_first_heartbeat script
	print 'Result: ' + str(check)
	#encounter.map = 5074 # TESTING!!! REMOVE!!!
	return check

def encounter_create( encounter ):
	print "Testing encounter_create with id=" + str(encounter.id) + ", map = " + str(encounter.map)
	# encounter_create adds all the objects to the scene
	# WIP temp location for now

	target = game.leader
	if (encounter.id >= 4000 or encounter.id == 3000):
		location = game.leader.location
		range = 1
	else:
		location = Spawn_Point(encounter)
		range = 6 #this is a "sub-range" actually, relative to the above spawn point
			
		numP = len(game.party) - 1
		xxx = game.random_range(0,numP)
		target = game.party[xxx]
		if (target == OBJ_HANDLE_NULL):
			target = game.party[0]

	if (encounter.map == 5078):
		Slaughtered_Caravan()
		
	if (encounter.map == 5072 and game.global_flags[855] == 1 and game.global_flags[875] == 0):
		game.global_flags[875] = 1
		flower = game.obj_create( 12900, location_from_axis (499L, 459L))

	is_camp = game.random_range(1,1) ## Added by Cerulean the Blue
	enemies = []
	i = 0
	total = len(encounter.enemies)
	#target = OBJ_HANDLE_NULL ## TESTING!!! REMOVE!!!
	#location = location_from_axis( 505, 479) # TESTING!!! REMOVE!!!
	print 'Spawning encounter enemies, total: ' + str(total)
	while (i < total):
		j = 0
		while (j < encounter.enemies[i][1]):
#################################################################################################
## Random Encounter Encampment - idea and map by Shiningted, execution  by Cerulean the Blue
			if ( ( encounter.map == 5074 ) and not(is_daytime()) and ( is_camp == 1 ) ):
				target = OBJ_HANDLE_NULL
				encounter.location = location_from_axis( 470, 480 )
				location = location_from_axis( 505, 479)
				range = 6
#################################################################################################

			spawn_loc = random_location(location, range, target)
				
			if (encounter.id >= 4000):
				numP = len(game.party) - 1
				xxx = game.random_range(0, numP)
				target = game.party[xxx]
				location = target.location
				if (target == OBJ_HANDLE_NULL):
					target = game.party[0]
					location = game.party[0].location
				if game.leader.map == 5066:
				#scripting wonnilon's hideout
					legit_list = []
					barney = 0
					for moe in game.party:
						xx, yy = location_to_axis(moe.location)
						if xx > 423 or xx < 410 or yy > 390 or yy < 369:
							barney = 1
							legit_list.append(moe.location)
					if barney == 0:
						location = game.party[0].location
					else:
						xxx = game.random_range(0, len(legit_list) - 1)
						for moe in game.party:
							if moe.location == legit_list[xxx]:
								target = moe
						location = legit_list[xxx]
				spawn_loc = location
				
			npc = game.obj_create( encounter.enemies[i][0], spawn_loc )
			if (npc != OBJ_HANDLE_NULL):
				if (target != OBJ_HANDLE_NULL):
					npc.turn_towards(target)
				if (game.leader.area == 1 and game.leader.reputation_has(1) == 92) or ((encounter.id < 2000) or (encounter.id >= 4000)):
					if target != OBJ_HANDLE_NULL:
						npc.attack(target)
					npc.npc_flag_set(ONF_KOS)
					enemies.append(npc)
				# m_count = m_count + 1
			j = j + 1
		i = i + 1
		
	return

def check_random_encounter( setup, encounter ):
	if (game.random_range(1,20) == 1):

		# encounter.location -- the location to teleport the player to
		r = game.random_range(1,3)
		if (r == 1):
			encounter.location = location_from_axis( 470, 480 )
		elif (r == 2):
			encounter.location = location_from_axis( 503, 478 )
		else:
			encounter.location = location_from_axis( 485, 485 )

		if (check_predetermined_encounter(setup,encounter)):
			set_f('qs_is_repeatable_encounter', 0)
			return 1
		elif (check_unrepeatable_encounter(setup,encounter)):
			Survival_Check(encounter)
			set_f('qs_is_repeatable_encounter', 0)
			return 1
		else: 
			if game.global_flags[403] == 1:
				if get_f('qs_disable_random_encounters'):
					return 0
			check = check_repeatable_encounter(setup,encounter)
			#encounter.location = location_from_axis( 503, 478 ) #for testing only
			Survival_Check(encounter)
			set_f('qs_is_repeatable_encounter', 1)

			return check
	#print 'nope'
	return 0

def check_sleep_encounter( setup, encounter ):
	NPC_Self_Buff()##	THIS WAS ADDED TO AID IN NPC SELF BUFFING			##

	## Revamped the chance system slightly. 
	## ran_factor determines chance of encounter. Translated to chance of encounter in an 8 hour rest period:
	## 10 - 56 percent
	## 11 - 60
	## 12 - 64
	## 13 - 67
	## 14 - 70
	## 15 - 72
	## 16 - 75
	## 17 - 77
	## 18 - 80
	## 19 - 81
	## 20 - 83
	## 21 - 85
	## 22 - 86
	## 23 - 87
	## 24 - 89
	## 25 - 90
	## 26 - 91
	## 27 - 91.9
	## 28 - 93
	## 29 - 93.5
	## 30 - 94.2
	## 31 - 94.8
	## 32 - 95.4
	## 33 - 95.9
	## 34 - 96.4
	## 35 - 96.8
	## 36 - 97.1
	##
	if game.leader.map == 5015 or game.leader.map == 5016 or game.leader.map == 5017:
	#resting in Burne's tower
		ran_factor = 31 
	elif game.leader.map == 5001 or game.leader.map == 5007:
	#resting in Hommlet Exterior
		ran_factor = 18 
#	elif game.leader.map == 5067:
	#resting in Temple level 2
#		if game.global_flags[144] == 1:
		#temple on alert
#			if game.global_flags[105] == 0 or game.global_flags[106] == 0 or game.global_flags[107] == 0 or game.global_flags[139] == 0:
			#any elemental high priest is alive
#				ran_factor = 16
#			else:
#				ran_factor = 10
#		elif game.global_flags[144] == 0:
		#temple not on alert
#			ran_factor = 10
#	elif game.leader.map == 5079 or game.leader.map == 5080 or game.leader.map == 5105:
	#resting in Temple level 3 or 4
#		if game.global_flags[144] == 1:
		#temple on alert
#			if game.global_flags[146] == 0 or game.global_flags[147] == 0:
			#hedrack or senshock is alive
#				ran_factor = 21
#			else:
#				ran_factor = 10
#		elif game.global_flags[144] == 0:
		#temple not on alert
#			ran_factor = 10
	elif game.leader.map == 5143 or game.leader.map == 5144 or game.leader.map == 5145 or game.leader.map == 5146 or game.leader.map == 5147:
	#resting in Verbobonc castle
		ran_factor = 36
	elif game.leader.map == 5128 or game.leader.map == 5129 or game.leader.map == 5130 or game.leader.map == 5131:
	#resting in Verbobonc Underdark interior
		ran_factor = 30
	elif game.leader.map == 5127:
	#resting in Verbobonc Underdark interior entryway - not safe but won't get attacked
		ran_factor = 0
	elif game.leader.map == 5191:
	#resting in Hickory Branch Crypt
		ran_factor = 25
	elif game.leader.map == 5093 or game.leader.map == 5192 or game.leader.map == 5193:
	#resting in Welkwood Bog
		if (game.global_flags[976] == 0):	# Mathel alive - not safe but won't get attacked
			ran_factor = 0
		elif (game.global_flags[976] == 1):	# Mathel dead
			ran_factor = 10
	else: 
	#default - 56 percent chance of encounter in 8 hour rest
		ran_factor = 10
	if (game.random_range(1,100) <= ran_factor):
		encounter.id = 4000
		if (game.leader.area == 1):	# ----- Hommlet
			if (game.leader.reputation_has(29) == 1 or game.leader.reputation_has(30) == 1) and not (game.leader.reputation_has(92) == 1 or game.leader.reputation_has(32) == 1):
				#Slightly naughty
				enemy_list = ( ( 14371,1,2,1 ), )
				x = get_sleep_encounter_enemies(enemy_list,encounter)
				return x
			elif (game.leader.reputation_has(92) == 1 or game.leader.reputation_has(32) == 1) and get_v(439) < 9:
				#Moderately naughty
				enemy_list = ( ( 14371,2,4,1 ), )
				x = get_sleep_encounter_enemies(enemy_list,encounter)
				return x
			elif (game.leader.reputation_has(92) == 1 or game.leader.reputation_has(32) == 1) and get_v(439) >= 9 and get_v(439) <= 19:
				#Very Naughty
				num = game.random_range(4,7)
				temp = [ (14371, num ) ]
				if game.global_flags[336] == 0:
					temp.append( (14004,1) )
					#temp = (temp[0], (14004,1) )
					set_v(439, get_v(439) | 256 )
				if game.global_flags[437] == 0 :
					#p = len(temp)
					#if p == 2:
					#	temp = ( temp[0], temp[1], (14006,1), )
					#elif p == 1:
					#	temp = ( temp[0], (14006,1), )
					temp.append((14006,1))
					set_v(439, get_v(439) | 512 )
				if get_v(439) & 1024 == 0 :
					#p = len(temp)
					#if p == 3:
					#	temp = ( temp[0], temp[1], temp[2], (14012,1), )
					#elif p == 2:
					#	temp = ( temp[0], temp[1], (14012,1), )
					#elif p == 1:
					#	temp = ( temp[0], (14012,1), )
					temp.append((14012,1))
					set_v(439, get_v(439) | 1024)
				record_time_stamp(443)
				encounter.enemies = temp
				encounter.dc = 1
				return 1
			elif (game.leader.reputation_has(92) == 1 or game.leader.reputation_has(32) == 1) and get_v(439) >= 20 and game.time.time_game_in_seconds(game.time) < get_v(443) + 2*30*24*60*60:
				# NAUGHTINESS OVERWHELMING
				# You've exterminated all the badgers!
				# And not enough time has passed since you started massacring the badgers for a big revenge encounter
				return 0
			else:
				return 0
		elif (game.leader.area == 2):	# ----- Moat house
			if (game.leader.map == 5002):	# moathouse ruins
				# frog, tick, willowisp, wolf, crayfish, lizard, rat, snake, spider, brigand
				enemy_list = ( ( 14057,1,3,1 ), ( 14089,2,4,1 ), ( 14291,1,4,6 ), ( 14050,2,3,1 ), ( 14094,1,2,3 ), ( 14090,2,4,1 ), ( 14056,4,9,1 ), ( 14630,1,3,1 ), ( 14047,2,4,1 ), ( 14070,2,5,1 ) )
				x = get_sleep_encounter_enemies(enemy_list,encounter)				
				return x
			elif (game.leader.map == 5004):	# moathouse interior
				# rat, tick, lizard, snake, brigand
				enemy_list = ( ( 14056,4,9,1 ), ( 14089,2,4,1 ), ( 14090,2,4,1 ), ( 14630,1,3,1 ), ( 14070,2,5,1 ) )
				x = get_sleep_encounter_enemies(enemy_list,encounter)
				return x
			elif (game.leader.map == 5005):	# moathouse dungeon
				# rat, lizard, zombie, bugbear, gnoll (unless gone), Lareth guard (unless Lareth killed or in group)
				enemy_list = ( ( 14056,4,9,1 ), ( 14090,2,4,1 ), ( 14092,1,3,1 ), ( 14093,1,3,2), ( 14067,1,3,1 ), ( 14074,2,4,1 ) )
				x = get_sleep_encounter_enemies(enemy_list,encounter)
				if (x == 1):
					if (encounter.enemies[0][0] == 14067):	# check gnolls
						if (game.global_flags[288] == 1):
							return 0
					elif (encounter.enemies[0][0] == 14074):	# check Lareth
						if ((game.global_flags[37] == 1) or (game.global_flags[50] == 1)):
							return 0
				return x
			return 0
		elif (game.leader.area == 3):	# ----- Nulb
			return 0	# WIP, thieves?
		elif (game.leader.area == 4 or game.leader.map == 5105):	# ----- Temple
			if (game.leader.map == 5062):	# temple exterior
				# bandit, drelb (night only), rat, snake, spider
				enemy_list = ( ( 14070,2,5,1 ), ( 14275,1,1,4 ), ( 14056,4,9,1 ), ( 14630,1,3,1 ), ( 14047,2,4,1 ) )
				x = get_sleep_encounter_enemies(enemy_list,encounter)
				if (x == 1):
					if (encounter.enemies[0][0] == 14275):	#check drelb
						if (is_daytime()):
							return 0
				return x
			elif (game.leader.map == 5064):	# temple interior
				# bandit, drelb (night only), rat, GT patrol
				enemy_list = ( ( 14070,2,5,1 ), ( 14275,1,1,4 ), ( 14056,4,9,1 ), ( 14170,2,5,2 ) )
				x = get_sleep_encounter_enemies(enemy_list,encounter)
				if (x == 1):
					if (encounter.enemies[0][0] == 14275):	#check drelb
						if (is_daytime()):
							return 0
				return x
			elif (game.leader.map == 5065):	# temple tower
				# bandit, GT patrol
				enemy_list = ( ( 14070,2,5,1 ), ( 14170,2,5,2 ) )
				x = get_sleep_encounter_enemies(enemy_list,encounter)
				return x
			elif (game.leader.map == 5066):	# temple dungeon 1
				enc_abort = 1
				for joe in game.party:
					xx, yy = location_to_axis(joe.location)
					if xx > 423 or xx < 410 or yy > 390 or yy < 369:
						enc_abort = 0
				if enc_abort == 1:
					return 0
				# bandit, gnoll, ghoul, gelatinous cube, gray ooze, ogre, GT patrol
				enemy_list = ( ( 14070,2,5,1 ), ( 14078,2,5,1 ), ( 14128,2,5,1 ), ( 14139,1,1,3 ), ( 14140,1,1,4 ), ( 14448,1,1,2 ), ( 14170,2,5,2 ) )
				x = get_sleep_encounter_enemies(enemy_list,encounter)
				return x
			elif (game.leader.map == 5067):	# temple dungeon 2
				# bandit, bugbear, carrion crawler, ochre jelly, ogre, troll
				enemy_list = ( ( 14070,2,5,1 ), ( 14170,4,6,2 ), ( 14190,1,1,4 ), ( 14142,1,1,5 ), ( 14448,2,4,2 ), ( 14262,1,2,5 ) )
				x = get_sleep_encounter_enemies(enemy_list,encounter)
				return x
			elif (game.leader.map == 5105):	# temple dungeon 3 lower
				# black pudding, ettin, gargoyle, hill giant, ogre, troll
				enemy_list = ( ( 14143,1,1,7 ), ( 14697,1,2,5 ), ( 14239,5,8,4 ), ( 14221,2,3,7 ), ( 14448,5,8,2 ), ( 14262,2,3,5 ) )
				x = get_sleep_encounter_enemies(enemy_list,encounter)
				return x
			elif (game.leader.map == 5079):	# temple dungeon 3 upper
				return 0
			elif (game.leader.map == 5080):	# temple dungeon 4
				# black pudding, ettin, troll, gargoyle, hill giant, ogre + bugbear
				enemy_list = ( ( 14143,1,1,7 ), ( 14697,1,1,5 ), ( 14262,1,2,5 ), ( 14239,3,6,4 ), ( 14220,1,2,7 ), ( 14448,1,4,3 ) )
				x = get_sleep_encounter_enemies(enemy_list,encounter)
				if (x == 1):
					if (encounter.enemies[0][0] == 14448):	# reinforce ogres with bugbears
						encounter.enemies.append( ( 14174, game.random_range(2,5) ) )
				return x
			elif (game.leader.map == 5081):	# air node
				# air elemental, ildriss grue, vapor rat, vortex, windwalker
				enemy_list = ( ( 14292,1,2,5 ), ( 14192,1,2,4 ), ( 14068,1,4,2 ), ( 14293,1,2,2 ), ( 14294,1,1,4 ) )
				x = get_sleep_encounter_enemies(enemy_list,encounter)
				return x
			elif (game.leader.map == 5082):	# earth node
				# basilisk, chaggrin grue, crystal ooze, earth elemental
				enemy_list = ( ( 14295,1,1,5 ), ( 14191,1,4,4 ), ( 14141,1,1,4 ), ( 14296,1,2,5 ) )
				x = get_sleep_encounter_enemies(enemy_list,encounter)
				return x
			elif (game.leader.map == 5083):	# fire node
				# fire bats, fire elemental, fire snake, fire toad
				enemy_list = ( ( 14297,2,5,2 ), ( 14298,1,2,5 ), ( 14299,1,2,1 ), ( 14300,1,2,3 ) )
				x = get_sleep_encounter_enemies(enemy_list,encounter)
				return x
			elif (game.leader.map == 5084):	# water node
				# floating eye, ice lizard, lizard man, vodyanoi, water elemental, kopoacinth, lacedon, merrow
				enemy_list = ( ( 14301,1,1,1 ), ( 14109,1,1,3 ), ( 14084,2,4,1 ), ( 14261,1,1,7 ), ( 14302,1,2,5 ), ( 14240,2,3,4 ), ( 14132,3,5,1 ), ( 14108,3,5,2 ) )
				x = get_sleep_encounter_enemies(enemy_list,encounter)
				return x
			return 0
		elif (game.leader.map == 5143 or game.leader.map == 5144 or game.leader.map == 5145 or game.leader.map == 5146 or game.leader.map == 5147):	# ----- Verbobonc castle
			# ghost
			enemy_list = ( ( 14819,1,1,1 ), )
			x = get_sleep_encounter_enemies(enemy_list,encounter)
			return x
		elif (game.leader.map == 5128 or game.leader.map == 5129 or game.leader.map == 5130 or game.leader.map == 5131):	# ----- Verbobonc Underdark inside
			if (game.quests[69].state != qs_completed) and (game.quests[74].state != qs_completed):
				# large spider, fiendish small monstrous spider, fiendish medium monstrous spider, fiendish large monstrous spider
				enemy_list = ( ( 14047,5,10,1 ), ( 14672,4,8,1 ), ( 14620,3,6,1 ), ( 14671,2,4,1 ) )
				x = get_sleep_encounter_enemies(enemy_list,encounter)
				return x
			elif (game.quests[69].state == qs_completed) and (game.quests[74].state == qs_completed):
				# dire rat
				enemy_list = ( ( 14056,6,12,1 ), )
				x = get_sleep_encounter_enemies(enemy_list,encounter)
				return x
		elif (game.leader.map == 5132):	# ----- Verbobonc Underdark outside
			# wolf
			enemy_list = ( ( 14050,4,8,1 ), )
			x = get_sleep_encounter_enemies(enemy_list,encounter)
			return x
		elif (game.leader.map == 5093):	# ----- Welkwood Bog outside
			# wolf, jackal, giant frog, giant lizard, carrion crawler, wild boar
			enemy_list = ( ( 14050,2,6,1 ), ( 14051,2,6,1 ), ( 14057,1,3,1 ), ( 14090,1,3,1 ), ( 14190,1,1,1 ), ( 14522,2,4,1 ) )
			x = get_sleep_encounter_enemies(enemy_list,encounter)
			return x
		elif (game.leader.map == 5192 or game.leader.map == 5193):	# ----- Welkwood Bog inside
			# dire rat
			enemy_list = ( ( 14056,6,12,1 ), )
			x = get_sleep_encounter_enemies(enemy_list,encounter)
			return x
		elif (game.leader.map == 5095):	# ----- Hickory Branch
			if (game.quests[62].state != qs_completed):
				# hill giant, gnoll, orc fighter, orc bowman, bugbear, ogre
				enemy_list = ( ( 14988,1,2,1 ), ( 14475,3,6,1 ), ( 14745,3,6,1 ), ( 14467,2,4,1 ), ( 14476,2,4,1 ), ( 14990,1,2,1 ) )
				x = get_sleep_encounter_enemies(enemy_list,encounter)
				return x
			elif (game.quests[62].state == qs_completed):
				# black bear, brown bear, worg, dire wolf, dire bear, dire boar, wild boar
				enemy_list = ( ( 14052,1,2,1 ), ( 14053,1,1,1 ), ( 14352,1,2,1 ), ( 14391,1,2,1 ), ( 14506,1,1,1 ), ( 14507,1,1,1 ), ( 14522,2,4,1 ) )
				x = get_sleep_encounter_enemies(enemy_list,encounter)
				return x
		elif (game.leader.map == 5191):	# ----- Hickory Branch Crypt
			# dire rat
			enemy_list = ( ( 14056,6,12,1 ), )
			x = get_sleep_encounter_enemies(enemy_list,encounter)
			return x
		elif (game.leader.map == 5141):	# ----- Verbobonc Drainage Tunnels
			# dire rat
			enemy_list = ( ( 14433,9,15,1 ), )
			x = get_sleep_encounter_enemies(enemy_list,encounter)
			return x
		elif (game.leader.map == 5120):	# ----- Gnarley Forest
			# stirge, will-o'-wisp, basilisk, dire lizard
			enemy_list = ( ( 14182,5,10,1 ), ( 14291,4,8,1 ), ( 14295,1,3,1 ), ( 14450,1,3,1 ) )
			x = get_sleep_encounter_enemies(enemy_list,encounter)
			return x
		else :
			party_level = group_average_level(game.leader)
			get_repeatable_encounter_enemies(setup,encounter)
			while (encounter.dc > (party_level+2)):
			#while (encounter.dc > (party_level+2) or ( game.random_range(1, party_level) >  encounter.dc  ) ): # makes it more likely for high level parties to skip mundane encounters. Needs some adjustment so level 12 parties don't encounter 50 trolls all the time TODO
				get_repeatable_encounter_enemies(setup,encounter)
			encounter.id = 4000
			return 1
	return 0

def get_sleep_encounter_enemies( enemy_list, encounter ):
	for f in encounterpacks:		
		enemy_list = __import__(os.path.splitext(f)[0]).SleepEncounterExpander(encounter, enemy_list)
	total = len( enemy_list )
	n = game.random_range(0,total-1)	
	encounter.dc = enemy_list[n][3]
	party_level = group_average_level(game.leader)
	if (encounter.dc > (party_level+2)):
		# try again
		n = game.random_range(0,total-1)	
		encounter.dc = enemy_list[n][3]
		if (encounter.dc > (party_level+2)):
			return 0
	num = game.random_range(enemy_list[n][1],enemy_list[n][2])
	encounter.enemies = [ ( enemy_list[n][0], num ), ]
	reinforcements = __import__(os.path.splitext(f)[0]).SleepEncounterReinforcements(encounter, enemy_list)
	for r in reinforcements:
		encounter.enemies.append(r)
	return 1

def check_predetermined_encounter( setup, encounter ):
	while (len(game.encounter_queue) > 0):
		id = game.encounter_queue[0]
		del game.encounter_queue[0]
		if (game.global_flags[id - 3000 + 277] == 0):
			game.global_flags[id - 3000 + 277] = 1
			encounter.id = id
			encounter.dc = 1000	# unavoidable
			encounter.map = get_map_from_terrain(setup.terrain)
			if (id == 3000): # Assassin
				encounter.enemies = ( ( 14303, 1 ), )
				if (encounter.map == 5074): # 5074 is the encampment map - unsuitable for these encounters
					encounter.map = 5070
			elif (id == 3001): # Thrommel Reward
				encounter.enemies = ( ( 14307, 1 ), ( 14308, 10 ) )
				if (encounter.map == 5074):
					encounter.map = 5070
			elif (id == 3002): # Tillahi Reward
				encounter.enemies = ( ( 14305, 1 ), ( 14306, 6 ) )
				if (encounter.map == 5074):
					encounter.map = 5070
			elif (id == 3003): # Sargen's Courier
				encounter.enemies = ( ( 14304, 1 ), )
				if (encounter.map == 5074):
					encounter.map = 5070
			elif (id == 3004): # Skole Goons
				if (game.global_flags[202] == 1):
					encounter.enemies = ( ( 14315, 4 ), )
				else:
					return 0
			elif (id == 3159):
			# Following the trader's tracks, you find them camping
				encounter.enemies = ( (14014,1), (14018,1) )
				set_v(437,101)
				encounter.map = 5074
			elif (id == 3434):				# ranth's bandits
				encounter.enemies = ( ( 14485, 1 ), ( 14489, 1 ), ( 14490, 1 ), ( 14486, 4 ), ( 14487, 4 ), ( 14488, 4 ) )
				encounter.map = 5071
			elif (id == 3435): 				# Scarlet Brotherhood
				encounter.enemies = ( ( 14653, 3 ), ( 14652, 1 ) )
				if (encounter.map == 5070):
					encounter.map = 5071
				if (encounter.map == 5074):
					encounter.map = 5075
				encounter.location = location_from_axis( 480, 480 )
				if (game.global_vars[945] == 4):
					game.global_vars[945] = 7
				if (game.global_vars[945] == 5):
					game.global_vars[945] = 8
				if (game.global_vars[945] == 6):
					game.global_vars[945] = 9
			elif (id == 3436):				# gremlich 1
				encounter.enemies = ( ( 2146, 1 ), )
				game.encounter_queue.append(3437)
				game.global_vars[927] = 1
			elif (id == 3437):				# gremlich 2
				encounter.enemies = ( ( 2148, 1 ), )
				game.encounter_queue.append(3438)
				game.global_vars[927] = 2
			elif (id == 3438):				# gremlich 3
				encounter.enemies = ( ( 2147, 1 ), )
				game.encounter_queue.append(3439)
				game.global_vars[927] = 3
			elif (id == 3439):				# gremlich 4
				encounter.enemies = ( ( 2149, 1 ), )
				game.global_vars[927] = 4
			elif (id == 3440):				# gremlich for real
				encounter.enemies = ( ( 14752, 1 ), )
				if (encounter.map == 5074):
					encounter.map = 5070
				game.sound( 4126, 1 )
			elif (id == 3441):				# sport - pirates vs brigands
				if (encounter.map == 5074):
					encounter.map = 5070
				encounter.enemies = ( ( 14290, 8 ), )
			elif (id == 3442):				# sport - bugbears vs orcs melee
				if (encounter.map == 5074):
					encounter.map = 5070
				encounter.enemies = ( ( 14173, 8 ), )
			elif (id == 3443):				# sport - bugbears vs orcs ranged
				if (encounter.map == 5074):
					encounter.map = 5070
				encounter.enemies = ( ( 14912, 8 ), )
			elif (id == 3444):				# sport - hill giants vs ettins
				if (encounter.map == 5074):
					encounter.map = 5070
				encounter.enemies = ( ( 14572, 2 ), )
			elif (id == 3445):				# sport - female vs male bugbears
				if (encounter.map == 5074):
					encounter.map = 5070
				encounter.enemies = ( ( 14686, 8 ), )
			elif (id == 3446):				# sport - zombies vs lacedons
				if (encounter.map == 5074):
					encounter.map = 5070
				encounter.enemies = ( ( 14123, 8 ), )
			elif (id == 3447):				# bethany
				encounter.enemies = ( ( 14773, 1 ), )
				encounter.map = 5071
				encounter.location = location_from_axis( 484, 462 )
			elif (id == 3579):				# gnolls
				encounter.enemies = ( ( 14066, 1 ), ( 14078, 3 ), ( 14079, 3 ), ( 14080, 3 ), ( 14067, 3 ) )
			elif (id == 3605):				# slaughtered caravan
				encounter.enemies = ( ( 14459, 1 ), )
				encounter.map = 5078
				encounter.location = location_from_axis( 465, 475 )
			else:
				return 0
			return 1
	return 0

def check_unrepeatable_encounter( setup, encounter ):
	#make some fraction of all encounters an unrepeatable encounter (i.e. Special Encounters)
	# The chance starts at 10% and gets lower as you use them up because two consecutive rolls are made...
	if (game.random_range(1,10) == 1):
		id = game.random_range(2000,2002)
		if (game.global_flags[id - 2000 + 227] == 0):
			encounter.id = id
			encounter.map = get_map_from_terrain(setup.terrain)
			if (id == 2000): # ochre jellies
				encounter.dc = 9
				encounter.enemies = ( ( 14142, 4 ), )
			elif (id == 2001): # zaxis
				encounter.dc = 5
				encounter.enemies = ( ( 14331, 1 ), )
			elif (id == 2002): #adventuring party
				encounter.dc = 9
				encounter.enemies = ( ( 14332, 1 ), ( 14333, 1 ), ( 14334, 1 ), ( 14335, 1 ), ( 14336, 1 ), ( 14622, 1))
			else:
				return 0
			party_level = group_average_level(game.leader)
			if (encounter.dc > (party_level+2)):
				return 0
			game.global_flags[id - 2000 + 227] = 1
			return 1

	return 0

def check_repeatable_encounter( setup, encounter ):
	encounter.map = get_map_from_terrain(setup.terrain)
	#encounter.map = 5074 #  for testing only
	if ( ( encounter.map == 5074 ) and not(is_daytime()) ):
		encounter.location = location_from_axis( 470, 480 )
	if (encounter.map == 5072 or encounter.map == 5076):
		encounter.location = location_from_axis( 485, 485 )
	party_level = group_average_level(game.leader)
	get_repeatable_encounter_enemies(setup,encounter)
	#while (encounter.dc > (party_level+2)):
	countt = 0
	while countt < 5 and (   encounter.dc > (party_level+2)      or     ( party_level > 10 and encounter.dc < 5 and game.global_flags[500] == 1 and game.random_range(1,100) <= 87 )            ) : 
		# will reroll the encounter for higher levels (but still about 13% chance for lower level encounter, i.e. about 1/7 )
		# will also reroll if you're low level so that poor player doesn't get his ass kicked too early
		# note that party_level is referenced to party size 4 - bigger parties will have a higher effective level thus skewing the calculation
		# e.g. a size 7 + animal companion party will count as two levels higher I think
		get_repeatable_encounter_enemies(setup,encounter)
		countt += 1
	if countt == 5:
		return 0
	return 1

## NEW MAP GENERATOR BY CERULEAN THE BLUE
def get_map_from_terrain(terrain):
	map = 5069
	if (map == 5069):
		map = 5069 + game.random_range(1,8)
	return map

## NEW RANDOM ENCOUNTER ENEMY GENERATOR BY CERULEAN THE BLUE
def get_repeatable_encounter_enemies( setup, encounter ):
	if ((encounter.map == 5070) or (encounter.map == 5074)):
		if (is_daytime()):
			get_scrub_daytime(encounter)
		else: 
			get_scrub_nighttime(encounter)
	elif ((encounter.map == 5071) or (encounter.map == 5075)):
		if (is_daytime()):
			get_forest_daytime(encounter)
		else:
			get_forest_nighttime(encounter)
	elif ((encounter.map == 5072) or (encounter.map == 5076)):
		if (is_daytime()):
			get_swamp_daytime(encounter)
		else:
			get_swamp_nighttime(encounter)
	else:	# TERRAIN_RIVERSIDE
		if (is_daytime()):
			get_riverside_daytime(encounter)
		else:
			get_riverside_nighttime(encounter)
	return
## END NEW RANDOM ENCOUNTER ENEMY GENERATOR

def get_scrub_daytime( encounter ):
	m = Get_Multiplier(encounter)
	m2 =1
	if m > 1:
		m2 = m - 1
			
	re_list = []
	
	re_list.append( RE_entry( 2,    ( (14069, m, m), (14070, 2*m, 5*m, 0.5) ),      1000 )     ) # bandits

	re_list.append( RE_entry( 3,    ( (14067, 2*m, 4*m),  ),      1018 )     ) # gnolls
	re_list.append( RE_entry( 4,    ( (14067, 1*m, 3*m), (14050, 1*m, 3*m) ),      1019 )     ) # gnolls and wolves
	re_list.append( RE_entry( 1,    ( (14184, 3*m, 6*m),  ),      1003 )     ) # goblins
	re_list.append( RE_entry( 3,    ( (14184, 3*m, 6*m),  (14050, 1*m, 3*m) ),      1004 )     ) # goblins and wolves
	re_list.append( RE_entry( 1,    ( (14640, 3*m, 10*m),  (14641, 1*m, 1*m) ),      1003 )     ) # kobolds and kobold sergeant
	re_list.append( RE_entry( 1,    ( (14051, 2*m, 4*m),  ),      1006 )     ) # jackals

	# Higher level encounters
	re_list.append( RE_entry( 4,    ( (14697, 1*m, 4*m, 1),  ),      1041 )     ) # ettins (potentially)
	re_list.append( RE_entry( 7,    ( (14217, 1*m, 4*m, 1),  ),      1045 )     ) # hill giants
	re_list.append( RE_entry( 7,    ( (14697, m, m), (14053, 1*m, 2*m) ),      1040 )     ) # ettin vs. brown bears
	re_list.append( RE_entry( 6,    ( (14697, m, m), (14188, 3*m, 6*m) ),      1039 )     ) # ettin vs. hobgoblins

	if game.global_flags[500] == 1:
		re_list.append( RE_entry( 11,    ( (14892, 1*m, 2*m, 0.5), (14891, 2*m, 4*m, 0.5) ,(14890, 2*m2, 3*m2, 0.5) ,(14889, 1*m2, 2*m2, 0.5), ),      1022 )     ) # Lizardman battlegroup
		re_list.append( RE_entry( 11,    ( (14888, 3, 5, 0.5), (14891, 0*m, 1*m, 0.5) ,(14696, 0*m2, 1*m2, 0.5) ,(14896, 0*m2, 1*m2, 0.5), (14506, 0*m2, 1*m2, 0.5), (14527, 0*m2, 1*m2, 0.5), (14525, 0*m2, 1*m2, 0.5), ),      1024 )     ) # Cult of the Siren + Random Thrall
		re_list.append( RE_entry( 10,    ( (14898, 2*m2, 4*m2, 0.5), (14897, 2*m2, 4*m2, 0.5) ,),      1022 )     ) # Leucrottas + Jackalweres
		re_list.append( RE_entry( 11,    ( (14248, 1, 1, 1), (14249, 3*m, 5*m), ) ,      1016 )     ) # Ogre chief and ogres
		re_list.append( RE_entry( 11,    ( (14248, 1, 1, 1), (14249, 3*m, 5*m), (14697, 3*m, 5*m), ) ,      1023 )     ) # Ogre chief and ogres vs. ettins (clash of the titans! :) )
		if game.party_alignment &  ALIGNMENT_EVIL != 0:
			re_list.append( RE_entry( 11,    ( (14896, 2, 4, 1), (14895, 3, 5), (14894, 2, 3, 0.5) ,),      1022 )     ) # Holy Rollers
	
	for f in encounterpacks:		
		re_list = __import__(os.path.splitext(f)[0]).EncounterExpander(encounter, re_list)

	aaa = game.random_range(0, len(re_list)-1 )
	encounter.enemies = re_list[aaa].get_enemies()
	encounter.dc = re_list[aaa].dc
	encounter.id = re_list[aaa].id
	

	return

def get_scrub_nighttime( encounter ):
	m = Get_Multiplier(encounter)
	m2 =1
	if m > 1:
		m2 = m - 1

	re_list = []
	
	re_list.append( RE_entry( 3,    ( (14093, m, m), (14184, 3*m, 6*m) ),      1026 )     ) # bugbears and goblins
	re_list.append( RE_entry( 4,    ( (14093, m, m), (14188, 4*m, 9*m) ),      1027 )     ) # bugbears and hobgoblins
	re_list.append( RE_entry( 1,    ( (14093, 2*m, 4*m, 1),  ),      1028)     ) # bugbears

	re_list.append( RE_entry( 3,    ( (14067, 2*m, 4*m),  ),      1018 )     ) # gnolls
	re_list.append( RE_entry( 4,    ( (14067, 1*m, 3*m), (14050, 1*m, 3*m) ),      1019 )     ) # gnolls and wolves
	re_list.append( RE_entry( 1,    ( (14184, 3*m, 6*m),  ),      1003 )     ) # goblins
	re_list.append( RE_entry( 3,    ( (14184, 3*m, 6*m),  (14050, 1*m, 3*m) ),      1004 )     ) # goblins and wolves
	re_list.append( RE_entry( 1,    ( (14640, 3*m, 10*m),  (14641, 1*m, 1*m) ),      1003 )     ) # kobolds and kobold sergeant
	re_list.append( RE_entry( 1,    ( (14092, 1*m, 6*m, 0.3),  ),      1014 )     ) # zombies

	# Higher level encounters
	re_list.append( RE_entry( 7,    ( (14093, 2*m, 4*m), (14050, 2*m, 4*m), ),      1029 )     ) # bugbears and wolves

	if game.global_flags[500] == 1:
		re_list.append( RE_entry( 11,    ( (14892, 1*m2, 2*m2, 0.5), (14891, 2*m, 4*m, 0.5) ,(14890, 2*m2, 3*m2, 0.5) ,(14889, 1*m2, 2*m2, 0.5), ),      1022 )     ) # Lizardman battlegroup
		re_list.append( RE_entry( 10,    ( (14898, 2*m2, 4*m2, 0.5), (14897, 2*m2, 4*m2, 0.5) ,),      1022 )     ) # Leucrottas + Jackalweres
		re_list.append( RE_entry( 9,    ( (14542, 2*m, 4*m, 1),  ) ,      1017 )     ) # Invisible Stalkers
		re_list.append( RE_entry( 11,    ( (14248, 1, 1, 1), (14249, 3*m, 5*m), ) ,      1016 )     ) # Ogre chief and ogres
		re_list.append( RE_entry( 11,    ( (14510, 1*m, 3*m, 1), (14299, 1*m, 3*m) ) ,      1028 )     ) # Huge Fire elementals and fire snakes
		re_list.append( RE_entry( 14,    ( (14958, 1*m, 1*m, 1), (14893, 2*m, 4*m, 1), ) ,      1017 )     ) # Nightwalker and Greater Shadows
	
	for f in encounterpacks:		
		re_list = __import__(os.path.splitext(f)[0]).EncounterExpander(encounter, re_list)

	aaa = game.random_range(0, len(re_list)-1 )
	encounter.enemies = re_list[aaa].get_enemies()
	encounter.dc = re_list[aaa].dc
	encounter.id = re_list[aaa].id		


	return

def get_forest_daytime( encounter ):
	m = Get_Multiplier(encounter)
	m2 =1
	if m > 1:
		m2 = m - 1
		
	re_list = []
		
	re_list.append( RE_entry( 2,    ( (14069, m, m), (14070, 2*m, 5*m, 0.5) ),      1000 )     ) # bandits
	re_list.append( RE_entry( 3,    ( (14052, 1*m, 3*m), ),      1001 )     ) # black bears
	re_list.append( RE_entry( 1,    ( (14188, 1*m, 3*m, 0.3), (14184, 1*m, 6*m, 0.2) ),      1002 )     ) # hobgoblins and goblins
	re_list.append( RE_entry( 2,    ( (14188, 3*m, 6*m), ),      1003 )     ) # hobgoblins
	re_list.append( RE_entry( 3,    ( (14188, 1*m, 3*m), (14050, 1*m, 3*m) ),      1004 )     ) # hobgoblins and wolves
	re_list.append( RE_entry( 1,    ( (14448, 2*m, 4*m, 1), ),      1005 )     ) # ogres
	re_list.append( RE_entry( 3,    ( (14046, 1*m, 3*m, 1), ),      1006 )     ) # owlbears
	re_list.append( RE_entry( 0,    ( (14047, 2*m, 4*m, 1), ),      1007 )     ) # large spiders
	re_list.append( RE_entry( 2,    ( (14182, 3*m, 6*m), ),      1008 )     ) # stirges
	re_list.append( RE_entry( 0,    ( (14089, 2*m, 4*m, 1), ),      1009 )     ) # giant ticks
	re_list.append( RE_entry( 2,    ( (14050, 2*m, 3*m), ),      1010 )     ) # wolves

	# Higher Level Encounters
	re_list.append( RE_entry( 5,    ( (14053, 1*m, 3*m), ),      1011 )     ) # brown bears
	re_list.append( RE_entry( 5,    ( (14243, 1*m, 3*m), ),      1012 )     ) # harpies

	if game.global_flags[500] == 1:
		re_list.append( RE_entry( 11,    ( (14898, 2*m2, 4*m2, 0.5), (14897, 2*m2, 4*m2, 0.5) ,),      1012 )     ) # Leucrottas + Jackalweres
		re_list.append( RE_entry( 12,    ( (14888, 3, 5, 0.5), (14891, 0*m, 1*m, 0.5) ,(14696, 0*m2, 1*m2, 0.5) ,(14896, 0*m2, 1*m2, 0.5), (14506, 0*m2, 1*m2, 0.5), (14527, 0*m2, 1*m2, 0.5), (14525, 0*m2, 1*m2, 0.5), ),      1013 )     ) # Cult of the Siren + Random Thrall
		re_list.append( RE_entry( 10,    ( (14542, 2*m, 4*m, 1),  ) ,      1014 )     ) # Invisible Stalkers
		re_list.append( RE_entry( 12,    ( (14248, 1, 1, 1), (14249, 3*m, 5*m), ) ,      1015 )     ) # Ogre chief and ogres
		if game.party_alignment &  ALIGNMENT_EVIL != 0:
			re_list.append( RE_entry( 12,    ( (14896, 2*m, 4*m, 1), (14895, 3*m, 5*m), (14894, 2*m, 3*m, 0.5) ,),      1016 )     ) # Holy Rollers

	for f in encounterpacks:		
		re_list = __import__(os.path.splitext(f)[0]).EncounterExpander(encounter, re_list)
	aaa = game.random_range(0, len(re_list)-1 )
	encounter.enemies = re_list[aaa].get_enemies()
	encounter.dc = re_list[aaa].dc
	encounter.id = re_list[aaa].id	


	return

def get_forest_nighttime( encounter ):
	m = Get_Multiplier(encounter)
	m2 =1
	if m > 1:
		m2 = m - 1
	
	re_list = []
		
	re_list.append( RE_entry( 1,    ( (14188, 1*m, 3*m, 0.3), (14184, 1*m, 6*m, 0.2) ),      1009 )     ) # hobgoblins and goblins
	re_list.append( RE_entry( 2,    ( (14188, 3*m, 6*m), ),      1010 )     ) # hobgoblins	
	re_list.append( RE_entry( 3,    ( (14188, 1*m, 3*m), (14050, 1*m, 3*m) ),      1011 )     ) # hobgoblins and wolves
	re_list.append( RE_entry( 2,    ( (14182, 3*m, 6*m), ),      1012 )     ) # stirges
	re_list.append( RE_entry( 1,    ( (14092, 1*m, 6*m, 0.3),  ),      1013 )     ) # zombies

	# higher level encounters
	re_list.append( RE_entry( 6,    ( (14291, 2*m, 3*m, 1),  ) ,      1014 )     ) # Will o' wisps

	if game.global_flags[500] == 1:
		re_list.append( RE_entry( 11,    ( (14898, 2*m2, 4*m2, 0.5), (14897, 2*m2, 4*m2, 0.5) ,),      1015 )     ) # Leucrottas + Jackalweres
		re_list.append( RE_entry( 10,    ( (14674, 2, 3, 1),  (14280, 1, 2, 1), (14137, 2*m, 4*m), ) ,      1016 )     ) # mohrgs and groaning spirits and ghasts
		re_list.append( RE_entry( 12,    ( (14248, 1, 1, 1), (14249, 3*m, 5*m), ) ,      1017 )     ) # Ogre chief and ogres
		re_list.append( RE_entry( 9,    ( (14542, 2*m, 4*m, 1),  ) ,      1018 )     ) # Invisible Stalkers
		re_list.append( RE_entry( 14,    ( (14958, 1, 1, 1), (14893, 2*m, 4*m, 0.5), ) ,      1019 )     ) # Nightwalker and Greater Shadows


	for f in encounterpacks:		
		re_list = __import__(os.path.splitext(f)[0]).EncounterExpander(encounter, re_list)
	aaa = game.random_range(0, len(re_list)-1 )
	encounter.enemies = re_list[aaa].get_enemies()
	encounter.dc = re_list[aaa].dc
	encounter.id = re_list[aaa].id	
	

	return

def get_swamp_daytime( encounter ):
	m = Get_Multiplier(encounter)
	m2 =1
	if m > 1:
		m2 = m - 1
	
	re_list = []

	re_list.append( RE_entry( 2,    ( (14182, 3*m, 6*m), ),      1013 )     ) # stirges
	re_list.append( RE_entry( 0,    ( (14089, 2*m, 4*m, 1), ),      1014 )     ) # giant ticks
	re_list.append( RE_entry( 2,    ( (14094, 1*m, 2*m, 1), ),      1015 )     ) # crayfish
	re_list.append( RE_entry( 2,    ( (14057, 1*m, 2*m), ),      1016 )     ) # frogs
	re_list.append( RE_entry( 0,    ( (14090, 2*m, 4*m, 1), ),      1017 )     ) # lizards
	re_list.append( RE_entry( 2,    ( (14084, 2*m, 3*m), ),      1018 )     ) # lizardmen
	re_list.append( RE_entry( 4,    ( (14084, 1*m, 3*m), (14090, 1*m, 1*m)),      1019 )     ) # lizardmen with lizard
	re_list.append( RE_entry( 1,    ( (14056, 4*m, 9*m, 0.144), ),      1020 )     ) # rats
	re_list.append( RE_entry( 3,    ( (14630, 1*m, 3*m, 0.5), ),      1021 )     ) # snakes

	# Higher Level Encounters
	re_list.append( RE_entry( 4,    ( (14262, 1*m, 4*m, 1), ),      1022 )     ) # trolls


	if game.global_flags[500] == 1:
		re_list.append( RE_entry( 12,    ( (14892, 1*m, 2*m, 0.5), (14891, 2*m, 4*m, 0.5) ,(14890, 2*m2, 3*m2, 0.5) ,(14889, 1*m2, 2*m2, 0.5), ),      1023 )     ) # Lizardman battlegroup
		re_list.append( RE_entry( 11,    ( (14892, 1, 2, 0.5), (14891, 2, 4, 0.5) ,(14890, 2*m2, 3*m2, 0.5) ,(14889, 1*m2, 2*m2, 0.5), (14343, 1*m2, 2*m2), (14090, 1*m2, 2*m2) ),      1024 )     ) # Lizardman battlegroup + lizards + hydras
		re_list.append( RE_entry( 9,    ( (14343, 1*m, 2*m, 1),  ),      1025 )     ) # Hydras
		re_list.append( RE_entry( 12,    ( (14261, 1*m, 4*m, 1), ),      1026 )     ) # Vodyanoi
		re_list.append( RE_entry( 9,    ( (14279, 2, 3, 1), (14375, 2, 4),  ),      1027 )     ) # Seahags and watersnakes

	for f in encounterpacks:		
		re_list = __import__(os.path.splitext(f)[0]).EncounterExpander(encounter, re_list)
	aaa = game.random_range(0, len(re_list)-1 )
	encounter.enemies = re_list[aaa].get_enemies()
	encounter.dc = re_list[aaa].dc
	encounter.id = re_list[aaa].id	


	return

def get_swamp_nighttime( encounter ):
	m = Get_Multiplier(encounter)
	m2 =1
	if m > 1:
		m2 = m - 1
		
		
	re_list = []

	re_list.append( RE_entry( 2,    ( (14052, 2*m, 5*m, 0.5), ),      1001 )     ) # black bears
	re_list.append( RE_entry( 2,    ( (14182, 3*m, 6*m), ),      1002 )     ) # stirges

	# higher level encounters
	re_list.append( RE_entry( 5,    ( (14291, 1*m, 4*m, 1), ),      1003 )     ) # willowisps
	re_list.append( RE_entry( 4,    ( (14262, 1*m, 4*m, 1), ),      1004 )     ) # trolls
	re_list.append( RE_entry( 8,    ( (14280, 1*m, 1*m), ),      1005 )     ) # groaning spirit
	re_list.append( RE_entry( 4,    ( (14128, 1*m, 3*m, 0.5), ),      1006 )     ) # ghouls
	re_list.append( RE_entry( 5,    ( (14135, 1*m, 1*m), (14128, 2*m, 4*m) ),      1007 )     ) # ghasts and ghouls

	if game.global_flags[500] == 1:
		re_list.append( RE_entry( 12,    ( (14892, 1*m2, 2*m2, 0.5), (14891, 2*m, 4*m, 0.5) ,(14890, 2*m2, 3*m2, 0.5) ,(14889, 1*m2, 2*m2, 0.5), ),      1008 )     ) # Lizardman battlegroup
		re_list.append( RE_entry( 12,    ( (14892, 1, 2, 0.5), (14891, 2, 4, 0.5) ,(14890, 2*m2, 3*m2, 0.5) ,(14889, 1*m2, 2*m2, 0.5), (14343, 1*m2, 2*m2), (14090, 1*m2, 2*m2) ),      1009 )     ) # Lizardman battlegroup + lizards + hydras
		re_list.append( RE_entry( 14,    ( (14958, 1, 1, 1), (14893, 2*m, 4*m, 1), ) ,      1010 )     ) # Nightwalker and Greater Shadows
		re_list.append( RE_entry( 9,    ( (14343, 1*m, 2*m, 1),  ),      1011 )     ) # Hydras
		re_list.append( RE_entry( 12,    ( (14261, 1*m, 4*m, 1), ),      1012 )     ) # Vodyanoi
		re_list.append( RE_entry( 9,    ( (14279, 1*m, 3*m, 1), (14375, 1*m, 3*m),  ),      1013 )     ) # Seahags and watersnakes
		re_list.append( RE_entry( 9,    ( (14824, 1*m, 3*m, 1), (14825, 1*m, 3*m, 1), ),      1014 )     ) # Ettin & Hill giant zombies

	for f in encounterpacks:		
		re_list = __import__(os.path.splitext(f)[0]).EncounterExpander(encounter, re_list)
	aaa = game.random_range(0, len(re_list)-1 )
	encounter.enemies = re_list[aaa].get_enemies()
	encounter.dc = re_list[aaa].dc
	encounter.id = re_list[aaa].id	
		

	return

def get_riverside_daytime( encounter ):
	m = Get_Multiplier(encounter)
	m2 =1
	if m > 1:
		m2 = m - 1
		

	re_list = []

	re_list.append( RE_entry( 2,    ( (14094, 1*m, 2*m, 1), ),      1002 )     ) # crayfish
	re_list.append( RE_entry( 0,    ( (14090, 2*m, 4*m, 1), ),      1003 )     ) # lizards
	re_list.append( RE_entry( 2,    ( (14084, 2*m, 3*m), ),      1004 )     ) # lizardmen
	re_list.append( RE_entry( 4,    ( (14084, 1*m, 3*m), (14090, 1*m, 1*m)),      1005 )     ) # lizardmen with lizard
	re_list.append( RE_entry( 1,    ( (14290, 2*m, 5*m, 0.5), ),      1006 )     ) # pirates

	# higher level encounters
	re_list.append( RE_entry( 4,    ( (14262, 1*m, 4*m, 1), ),      1007 )     ) # trolls

	if game.global_flags[500] == 1:
		re_list.append( RE_entry( 12,    ( (14892, 1*m, 2*m, 0.5), (14891, 2*m, 4*m, 0.5) ,(14890, 2*m2, 3*m2, 0.5) ,(14889, 1*m2, 2*m2, 0.5), ),      1008 )     ) # Lizardman battlegroup
		re_list.append( RE_entry( 12,    ( (14888, 3, 5, 0.5), (14891, 0*m, 1*m, 0.5) ,(14696, 0*m2, 1*m2, 0.5) ,(14896, 0*m2, 1*m2, 0.5), (14506, 0*m2, 1*m2, 0.5), (14527, 0*m2, 1*m2, 0.5), (14525, 0*m2, 1*m2, 0.5), ),      1009 )     ) # Cult of the Siren + Random Thrall
		re_list.append( RE_entry( 12,    ( (14892, 1*m, 2*m, 0.5), (14891, 2*m, 4*m, 0.5) ,(14890, 2*m2, 3*m2, 0.5) ,(14889, 1*m2, 2*m2, 0.5), (14279, 1*m, 3*m ), ),      1010 )     ) # Lizardman battlegroup + seahag
		re_list.append( RE_entry( 12,    ( (14261, 1*m, 4*m, 1), ),      1011 )     ) # Vodyanoi
		re_list.append( RE_entry( 10,    ( (14279, 1*m2, 3*m2, 1), (14375, 1*m, 3*m), (14240, 1*m, 3*m), ),      1012 )     ) # Seahags and watersnakes and kapoacinths
		if game.party_alignment &  ALIGNMENT_EVIL != 0:
			re_list.append( RE_entry( 12,    ( (14896, 2*m2, 4*m2, 1), (14895, 3*m2, 5*m2), (14894, 2*m2, 3*m2, 0.5) ,),      1013 )     ) # Holy Rollers
	
	for f in encounterpacks:		
		re_list = __import__(os.path.splitext(f)[0]).EncounterExpander(encounter, re_list)
	aaa = game.random_range(0, len(re_list)-1 )
	encounter.enemies = re_list[aaa].get_enemies()
	encounter.dc = re_list[aaa].dc
	encounter.id = re_list[aaa].id	

		


	return

def get_riverside_nighttime( encounter ):
	m = Get_Multiplier(encounter)
	m2 =1
	if m > 1:
		m2 = m - 1
		
		
	re_list = []

	re_list.append( RE_entry( 1,    ( (14130, 1*m, 3*m, 0.5), ),      1007 )     ) # lacedons
	re_list.append( RE_entry( 1,    ( (14081, 2*m, 4*m), ),      1008 )     ) # skeleton gnolls
	re_list.append( RE_entry( 1,    ( (14107, 2*m, 4*m), ),      1009 )     ) # skeletons


	# Higher level encounters
	re_list.append( RE_entry( 4,    ( (14262, 1*m, 4*m, 1), ),      1010 )     ) # trolls
	
	if game.global_flags[500] == 1:
		re_list.append( RE_entry( 12,    ( (14892, 1*m, 2*m, 0.5), (14891, 2*m, 4*m, 0.5) ,(14890, 2*m2, 3*m2, 0.5) ,(14889, 1*m2, 2*m2, 0.5), ),      1011 )     ) # Lizardman battlegroup
		re_list.append( RE_entry( 12,    ( (14892, 1*m, 2*m, 0.5), (14891, 2*m, 4*m, 0.5) ,(14890, 2*m2, 3*m2, 0.5) ,(14889, 1*m2, 2*m2, 0.5), (14279, 1*m, 3*m ), ),      1012 )     ) # Lizardman battlegroup + seahag
		re_list.append( RE_entry( 12,    ( (14261, 1*m, 4*m, 1), ),      1013 )     ) # Vodyanoi
		re_list.append( RE_entry( 9,    ( (14279, 1*m, 3*m, 1), (14375, 1*m, 3*m), (14240, 1*m, 3*m), ),      1014 )     ) # Seahags and watersnakes and kapoacinths

	for f in encounterpacks:		
		re_list = __import__(os.path.splitext(f)[0]).EncounterExpander(encounter, re_list)
	aaa = game.random_range(0, len(re_list)-1 )
	encounter.enemies = re_list[aaa].get_enemies()
	encounter.dc = re_list[aaa].dc
	encounter.id = re_list[aaa].id	



	return

def can_sleep():
	# can_sleep is called to test the safety of sleeping
	# it must return one of the following
	#   SLEEP_SAFE
	#      * it is totally safe to sleep
	#   SLEEP_DANGEROUS
	#      * it may provoke a random encounter
	#   SLEEP_IMPOSSIBLE
	#      * rest is not possible here
	#   SLEEP_PASS_TIME_ONLY
	#      * resting here actually only passes time, no healing or spells are retrieved
	if (game.leader.map == 5115):
	# Hickory Branch Cave
		return SLEEP_SAFE
	elif (game.leader.area == 1):
	# Hommlet
		if (game.party[0].reputation_has(53) == 1 or game.party[0].reputation_has(61) == 1):
		#Hommlet Deserter or Hommlet Destroyer - town is empty
			return SLEEP_SAFE
		if (game.party[0].reputation_has(92) == 1 or game.party[0].reputation_has(29) == 1 or game.party[0].reputation_has(30) == 1 or game.party[0].reputation_has(32) == 1) and game.party[0].map != 5014:
			return SLEEP_DANGEROUS
		if ((game.leader.map == 5007) or (game.leader.map == 5008)):
		# inn first or second floor
			if ((game.global_flags[56] == 1) or (game.quests[18].state == qs_completed)):
				return SLEEP_SAFE
		return SLEEP_PASS_TIME_ONLY
	elif (game.leader.map == 5050):
	# Herdsman House
		return SLEEP_PASS_TIME_ONLY
	elif (game.leader.area == 2):
	# Moathouse
		if (game.leader.map == 5003):
		# tower
			return SLEEP_SAFE
		return SLEEP_DANGEROUS
	elif (game.leader.area == 3):
	# Nulb
		if ((game.leader.map == 5085) and (game.global_flags[94] == 1)):
			return SLEEP_SAFE
		elif (((game.leader.map == 5060) or (game.leader.map == 5061)) and (game.global_flags[289] == 1)):
			return SLEEP_SAFE	# WIP, thieves?
		return SLEEP_PASS_TIME_ONLY
	elif (game.leader.map == 5089):
	# Mona's Store
		return SLEEP_PASS_TIME_ONLY
	elif ((game.leader.map == 5090) and (game.global_flags[94] == 1)):
	# Nulb House Crazy
		return SLEEP_SAFE
	elif (game.leader.area == 4):
	# Temple
		if (game.leader.map == 5065 and game.global_flags[840] == 1):
	# bandit hideout
			return SLEEP_SAFE
		return SLEEP_DANGEROUS
	elif (game.leader.map == 5066):
	# wonnilon hideout
		#if game.global_flags[404] == 1:
		#	return SLEEP_SAFE
		#else:
			return SLEEP_DANGEROUS
	elif ((game.leader.map >= 5096) and (game.leader.map <= 5104)):
	# Vignettes
		return SLEEP_IMPOSSIBLE
	elif (game.leader.map == 5107):
	# ShopMap
		return SLEEP_SAFE
	elif (((game.leader.map >= 5121) and (game.leader.map <= 5126)) or ((game.leader.map >= 5133) and (game.leader.map <= 5140)) or (game.leader.map == 5142) or ((game.leader.map >= 5148) and (game.leader.map <= 5150)) or ((game.leader.map >= 5153) and (game.leader.map <= 5173)) or ((game.leader.map >= 5175) and (game.leader.map <= 5188))):
	#Verbobonc maps
		return SLEEP_PASS_TIME_ONLY
	elif ((game.leader.map == 5143) or (game.leader.map == 5144) or (game.leader.map == 5145) or (game.leader.map == 5146) or (game.leader.map == 5147)):
	#Verbobonc Castle
		if (game.global_flags[966] == 0):
			return SLEEP_PASS_TIME_ONLY
		elif (game.global_flags[966] == 1):
			if (game.global_vars[765] == 0):
				return SLEEP_SAFE
			elif (game.global_vars[765] >= 1):
				if (game.quests[83].state == qs_completed):
					return SLEEP_SAFE
				elif (game.global_flags[869] == 1):
					return SLEEP_IMPOSSIBLE
			return SLEEP_DANGEROUS
	elif ((game.leader.map == 5151) or (game.leader.map == 5152)):
	# Verbobonc Inn by Allyx
		if (game.global_flags[997] == 1):
			return SLEEP_SAFE
		return SLEEP_PASS_TIME_ONLY
	elif (game.leader.map == 5174):
	# Jylee's Inn
		if (game.global_flags[967] == 1):
			return SLEEP_SAFE
		return SLEEP_PASS_TIME_ONLY
	elif (game.leader.map == 5119):
	# Arena of Heroes
		return SLEEP_PASS_TIME_ONLY
	elif game.leader.map == 5116 or game.leader.map == 5118: # Tutorial maps 1 & 3
		return SLEEP_IMPOSSIBLE
	elif game.leader.map == 5117: # Tutorial map 2
		return SLEEP_SAFE
	return SLEEP_DANGEROUS

#######################################################################
##  Random Encounter Check tweaking my Cerulean the Blue
#######################################################################

def Survival_Check(encounter):
	if encounter.dc < 1000:
		PC_roll = game.random_range(1,20)
		NPC_roll = game.random_range(1,20)
		PC_mod = PC_Modifier()
		NPC_mod = NPC_Modifier(encounter)
		print 'PC roll: ' + str(PC_roll) + ' + ' + str(PC_mod/3) + ' vs  NPC roll: ' + str( NPC_roll) +  ' + '+str(NPC_mod/3)
		if PC_roll + (PC_mod/3) >= NPC_roll + (NPC_mod/3):
			encounter.dc = 1
		else:
			encounter.dc = 1000
		game.global_vars[35] = NPC_roll + NPC_mod/3 - (PC_roll + PC_mod/3)
	return 1

def PC_Modifier():
	high = 0
	level = 0
	wild = 0
	listen = 0
	spot = 0
	for obj in game.party[0].group_list():
		listen = obj.skill_level_get(skill_listen)
		spot = obj.skill_level_get(skill_spot)
		wild = obj.skill_level_get(skill_wilderness_lore)
		level = spot + listen + wild
		if level > high:
			high = level
	return high

def NPC_Modifier(encounter):
	print 'Getting NPC survival modifier.'
	high = 0
	level = 0
	wild = 0
	listen = 0
	spot = 0
	for i in encounter.enemies:
		obj = game.obj_create(i[0], game.party[0].location)
		listen = obj.skill_level_get(skill_listen)
		spot = obj.skill_level_get(skill_spot)
		wild = obj.skill_level_get(skill_wilderness_lore)
		print 'NPC ' +  str(i) +  ' , Listen = ' + str(listen) + ' , Spot = ' + str(spot) + ' , Survival = ' + str(wild)
		level = spot + listen + wild
		if level > high:
			high = level
		obj.destroy()
	print 'Highest NPC result was: ' + str(high)
	return high
	

def Spawn_Point(encounter):
	diff = game.global_vars[35]
	distance = 10 - diff
	if distance < 0:
		distance = 0
	if distance > 15:
		distance = 15
	
	if (encounter.location == location_from_axis( 503, 478 ) ):
		if (distance > 8):
			print 'Reducing distance to 8 for encounter because it is at the edges. Old distance was ' + str(distance)
			distance = 8
	
	print 'Distance = ', distance
		
	p_list = get_circle_point_list( game.party[0].location, distance, 16)
	return p_list[game.random_range(0, len(p_list) - 1)]

def random_location(loc, range, target):
	print 'Generating Location'
	x, y = location_to_axis(loc)
	print 'Target: ' + str(target)

	t_x, t_y = location_to_axis(target.location)

	rand_x = game.random_range(1,3)
	rand_y = game.random_range(1,3)
	loc_x = t_x
	loc_y = t_y
		
	while sqrt( (loc_x - t_x)**2 + (loc_y - t_y)**2 ) < sqrt( (x - t_x)**2 + (y - t_y)**2 ):
		if rand_x == 1:
			loc_x = ( x + game.random_range(1,range) )
		elif rand_x == 2:
			loc_x = ( x - game.random_range(1,range) )
		else:
			loc_x = x
			
		if rand_y == 1:
			loc_y = ( y + game.random_range(1,range) )
		elif rand_y == 2:
			loc_y = ( y - game.random_range(1,range) )
		else:
			loc_y = y
			
		rand_x = game.random_range(1,3)
		rand_y = game.random_range(1,3)
		
	location = location_from_axis( loc_x, loc_y )
	print 'Location: ' + str(loc_x) + ' ' + str(loc_y)
	return location

def group_skill_level( pc, skill ):
	high = 0
	level = 0
	for obj in pc.group_list():
		level = obj.skill_level_get( skill )
		if (level > high):
			high = level

	if anyone( game.party[0].group_list(), "has_item", 12677):
		# If your party has the spyglass, double the roll!
		high = high*2
	if (high == 0):

		return 1
	return high



def Get_Multiplier(encounter):
	m_range = (group_average_level(game.leader)/4)
	if m_range < 1:
		m_range = 1
	multiplier = game.random_range(1,min(m_range, 3) )
	if (  encounter.map != 5074  or is_daytime() ):
		chance = 40/group_average_level(game.leader)
		if ( game.random_range(1, chance) != 1 ):
			multiplier = 1
	if (encounter.id >= 2000):
		multipler = 1
	return multiplier

##############################################################

def get_circle_point_list(center, radius,num_points): # By Darmagon
	p_list=[]
	offx, offy = location_to_axis(center)
	i = 0.00
	while i < 2*pi:
		posx = int(cos(i)*radius)+offx
		posy = int(sin(i)*radius)+offy
		loc = location_from_axis(posx,posy)
		p_list.append(loc)
		i = i + pi/(num_points/2)
	return p_list

def Slaughtered_Caravan(): # By Livonya
	dead = game.obj_create( 2112, location_from_axis (477L, 484L))	
	dead.rotation = game.random_range(1,20)
	dead = game.obj_create( 2118, location_from_axis (486L, 462L))
	dead.rotation = game.random_range(1,20)
	dead = game.obj_create( 2125, location_from_axis (465L, 475L))
	dead.rotation = game.random_range(1,20)
	dead = game.obj_create( 2112, location_from_axis (477L, 467L))	
	dead.rotation = game.random_range(1,20)
	dead = game.obj_create( 2112, location_from_axis (481L, 473L))
	dead.rotation = game.random_range(1,20)
	dead = game.obj_create( 2125, location_from_axis (489L, 498L))
	dead.rotation = game.random_range(1,20)
	chest = game.obj_create( 1004, location_from_axis (482L, 465L))	
	chest.rotation = 2
	dead = game.obj_create( 2118, location_from_axis (476L, 506L))	
	dead.rotation = game.random_range(1,20)
	ticker = game.obj_create( 14638, location_from_axis (465, 475 ))
	tree = game.obj_create( 2017, location_from_axis (474L, 455L))	
	tree.rotation = game.random_range(1,20)
	tree = game.obj_create( 2017, location_from_axis (504L, 489L))	
	tree.rotation = game.random_range(1,20)
	tree = game.obj_create( 2017, location_from_axis (474L, 496L))	
	tree.rotation = game.random_range(1,20)
	tree = game.obj_create( 2017, location_from_axis (471L, 469L))	
	tree.rotation = game.random_range(1,20)
	return 1

def NPC_Self_Buff(): # By Livonya
##
##	THIS WAS ADDED TO AID IN NPC SELF BUFFING			##
##										##
	for i in range(712, 733):
		print i
		game.global_vars[i] = 0
	return 1
##										##
##	THIS WAS ADDED TO AID IN NPC SELF BUFFING			##
##