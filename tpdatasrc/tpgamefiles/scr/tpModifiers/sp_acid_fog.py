from toee import *
import tpdp
from spell_utils import AoeSpellHandleModifier, AoESpellEffectModifier, applyBonus, applyDamagePacketBonus

print "Registering sp-Acid Fog"

acidFogSpell = AoeSpellHandleModifier("sp-Acid Fog") #spellId, duration, spellDc, eventId, empty

#### Acid Fog Effect ####

def acidFogEffectDealDamage(attachee, args, evt_obj):
    spellId = args.get_arg(0)
    spellPacket = tpdp.SpellPacket(spellId)
    spellDamageDice = dice_new("1d6")
    spellDamageDice.number = 2
    damageType = D20DT_ACID 
    attachee.spell_damage(spellPacket.caster, damageType, spellDamageDice, D20DAP_UNSPECIFIED, D20A_CAST_SPELL, spellId)
    return 0

def acidFogEffectSuppressFiveFoot(attachee, args, evt_obj):
    evt_obj.tb_status.flags |= TBSF_Movement
    return 0

def acidFogEffectConcealment(attachee, args, evt_obj):
    #Vanilla Solid Fog denies Concealment when attacker is under a True Seeing effect
    #I think this is wrong, True Seeing should not cancel fog effects, only "normal" concealment effects
    #Source: https://www.d20srd.org/srd/spells/trueSeeing.htm

    #While in Fog, can't be hit by ranged attacks except magical rays
    #Vanilla Solid Fog handles this by reducing damage to 0
    #I am unsure why not simply raising concealment to 100
    #and not quering for anything isn't a more obvious choice?
    attacker = evt_obj.attack_packet.attacker
    #bonusValue = 50 if attacker.distance_to(attachee) > 5.0 else 20 #Acid Fog grants 20% Concealment Bonus if attacker is within 5 feet, else 50% while in Fog
    if evt_obj.attack_packet.get_flags() & D20CAF_RANGED and not evt_obj.attack_packet.get_flags() & D20CAF_TOUCH_ATTACK:
        bonusValue = 100
        attachee.float_text_line("Can't be targeted by ranged attacks", tf_red)
    elif attacker.distance_to(attachee) > 5.0:
        bonusValue = 50
    else:
        bonusValue = 20
    bonusType = bonus_type_concealment
    bonusTag = game.get_mesline("mes\\bonus_description.mes", bonusType)
    evt_obj.bonus_list.add(bonusValue, bonusType, "{} : ~Acid Fog~[TAG_SPELLS_ACID_FOG]".format(bonusTag))
    return 0

def acidFogEffectMovementRestriction(attachee, args, evt_obj):
    if not attachee.d20_query(Q_Critter_Has_Freedom_of_Movement):
        capValue = 5 #Acid Fog limits movement to 5 feet
        capType = bonus_type_untyped
        bonusMesId = 258 #ID 258 = Solid Fog; would need to do create own ID for Acid Fog msg and it is not visible anyways
        evt_obj.bonus_list.set_overall_cap(1, capValue, capType, bonusMesId)
        evt_obj.bonus_list.set_overall_cap(2, capValue, capType, bonusMesId)
    return 0

acidFogEffect = AoESpellEffectModifier("Acid Fog") #spellId, duration, spellDc, eventId, empty
acidFogEffect.AddHook(ET_OnBeginRound, EK_NONE, acidFogEffectDealDamage, ())
acidFogEffect.AddHook(ET_OnTurnBasedStatusInit, EK_NONE, acidFogEffectSuppressFiveFoot, ())
acidFogEffect.AddHook(ET_OnGetDefenderConcealmentMissChance, EK_NONE, acidFogEffectConcealment, ())
acidFogEffect.AddHook(ET_OnGetMoveSpeed, EK_NONE, acidFogEffectMovementRestriction, ())
acidFogEffect.AddHook(ET_OnToHitBonus2, EK_NONE, applyBonus, (-2, bonus_type_untyped,)) #maybe new bonus_type_cloud would be interesting to avoid stacking cloud effects
acidFogEffect.AddHook(ET_OnDealingDamage2, EK_NONE, applyDamagePacketBonus, (-2, bonus_type_untyped,))
