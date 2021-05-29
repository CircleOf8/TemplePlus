from toee import *
import tpactions

def GetActionName():
	return "Arcane Boost"

def GetActionDefinitionFlags():
	return D20ADF_None
	
def GetTargetingClassification():
	return D20TC_CastSpell

def GetActionCostType():
	return D20ACT_Swift_Action

def AddToSequence(d20action, action_seq, tb_status):
	action_seq.add_action(d20action)
	return AEC_OK

def ModifyPicker( picker_args ):
	picker_args.mode_target = tpactions.ModeTarget.Personal
	return 1
