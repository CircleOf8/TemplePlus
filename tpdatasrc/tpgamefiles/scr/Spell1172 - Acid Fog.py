from toee import *

def OnBeginSpellCast(spell):
    print "Acid Fog OnBeginSpellCast"
    print "spell.target_list=", spell.target_list
    print "spell.caster=", spell.caster, " caster.level= ", spell.caster_level

def OnSpellEffect(spell):
    print "Acid Fog OnSpellEffect"

    targetsToRemove = []
    spell.duration = 1 * spell.caster_level # 1 round/cl

    cloudObject = game.obj_create(OBJECT_SPELL_GENERIC, spell.target_loc)

    casterInitiative = spell.caster.get_initiative()
    cloudObject.d20_status_init()
    cloudObject.set_initiative(casterInitiative)

    cloudRadius = 20.0 #+ (spell.target_list[0].obj.radius / 12.0)
    cloudEventId = cloudObject.object_event_append(OLC_CRITTERS, cloudRadius)

    for spellTarget in spell.target_list:
         targetsToRemove.append(spellTarget.obj)
    spell.target_list.remove_list(targetsToRemove)

    cloudObject.condition_add_with_args("sp-Acid Fog", spell.id, spell.duration, spell.dc, cloudEventId, 0, 0)

    spell.target_list.remove_list(targetsToRemove)

def OnBeginRound(spell):
    print "Acid Fog OnBeginRound"

def OnAreaOfEffectHit(spell):
    print "Acid Fog OnAreaOfEffectHit"

def OnEndSpellCast(spell):
    print "Acid Fog OnEndSpellCast"

