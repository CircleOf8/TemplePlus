from toee import *
import tpactions

def GetActionName():
    return "Activate Ki Blast"

def GetActionDefinitionFlags():
    return D20ADF_MagicEffectTargeting | D20ADF_Breaks_Concentration
    
def GetTargetingClassification():
    return D20TC_CastSpell

def GetActionCostType():
    return D20ACT_Full_Round_Action

def AddToSequence(d20action, action_seq, tb_status):
    if d20action.performer.d20_query(Q_Prone):
        d20aGetup = d20action
        d20aGetup.action_type = D20A_STAND_UP
        action_seq.add_action(d20aGetup)

    action_seq.add_action(d20action)
    return AEC_OK

def ProjectileHit(d20action, proj, obj2):
    d20action.performer.apply_projectile_hit_particles(proj, d20action.flags)
    tpactions.trigger_spell_effect(d20action.spell_id)
    tpactions.trigger_spell_projectile(d20action.spell_id, proj)
    return 1