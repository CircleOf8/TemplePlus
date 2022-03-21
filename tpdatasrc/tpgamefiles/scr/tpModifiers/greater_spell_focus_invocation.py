from toee import *
import tpdp
from feat_utils import FeatPythonModifier
from warlock import isInvocation

print "Registering Greater Spell Focus (Invocation)"

def dcBonus(attachee, args, evt_obj):
    spellEntry = evt_obj.spell_entry
    if isInvocation(spellEntry.spell_enum):
        bonusValue = 1
        bonusType = bonus_type_untyped # Stacking!
        bonusLabel = "~Greater Spell Foucs (Invocation)~[TAG_SPELL_FOCUS]"
        evt_obj.bonus_list.add(bonusValue, bonusType, bonusLabel)
    return 0

gsfInvocationFeat = FeatPythonModifier("Greater Spell Focus (Invocation)") #featEnum, empty
gsfInvocationFeat.AddHook(ET_OnGetSpellDcMod, EK_NONE, dcBonus, ())
