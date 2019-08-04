from templeplus.pymod import PythonModifier
from toee import *
import tpdp

print "Registering Total Defense extender"

#The fighting defensively query needs to cover both fighting defensively and total defense
def FightingDefensivelyQuery(attachee, args, evt_obj):	
	if (args.get_arg(0) != 0):
		evt_obj.return_val = 1
		return 0
	return 0

modExtender = PythonModifier()
modExtender.ExtendExisting("Total Defense")
modExtender.AddHook(ET_OnD20Query, EK_Q_FightingDefensively, FightingDefensivelyQuery, ())
