from toee import *
from utilities import *
from scripts import *

def OnBeginSpellCast( spell ):
	print "Summon Monster VII OnBeginSpellCast"
	print "spell.target_list=", spell.target_list
	print "spell.caster=", spell.caster, " caster.level= ", spell.caster_level
	game.particles( "sp-conjuration-conjure", spell.caster )

def	OnSpellEffect ( spell ):
	print "Summon Monster VII OnSpellEffect"
	
	teststr = "; summon monster 7\n" #change this to the header line for the spell in spells_radial_menu_options.mes
	options = get_options_from_mes(teststr)
		
	if spell.caster.name == 14950:  # Noble Salamander
		spell.dc = 19               # 10 + 7 + 2 (charisma)
		spell.caster_level = 15
		options = [14902,14300,14898,14291]  # huge fire elem, fire toad, dire hell hound, wisp 
		
	spell.duration = 1 * spell.caster_level

	## Solves Radial menu problem for Wands/NPCs
	spell_arg = spell.spell_get_menu_arg( RADIAL_MENU_PARAM_MIN_SETTING )
	if spell_arg not in options:
		x = game.random_range(0,len(options)-1)
		spell_arg = options[x]
		
	# create monster, monster should be added to target_list
	spell.summon_monsters( 1, spell_arg)

	SummonMonster_Rectify_Initiative(spell, spell_arg) # Added by S.A. - sets iniative to caster's initiative -1, so that it gets to act in the same round
	
	spell.spell_end(spell.id)

def OnBeginRound( spell ):
	print "Summon Monster VII OnBeginRound"

def OnEndSpellCast( spell ):
	print "Summon Monster VII OnEndSpellCast"
	
