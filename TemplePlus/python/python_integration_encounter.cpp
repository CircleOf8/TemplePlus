
#include "stdafx.h"
#include <util/fixes.h>
#include "python_integration_obj.h"
#include <structmember.h>
#include <party.h>
#include <gamesystems/random_encounter.h>
#include "gamesystems/gamesystems.h"
#include "gamesystems/legacysystems.h"

static struct PyRandomEncounterAddresses : temple::AddressTable {
	int* sleepStatus;

	int(__cdecl *GetTerrainType)(locXY worldMapLoc);
	void(__cdecl *MapChange)();

	PyRandomEncounterAddresses() {
		rebase(sleepStatus, 0x109DD854);
		rebase(GetTerrainType, 0x10045BE0);
		rebase(MapChange, 0x100458D0);
	}
} addresses;

/*
Calls into random_encounter.can_sleep to update how the party
can rest in the current area.
*/
void UpdateSleepStatus() {
	auto result = pythonObjIntegration.ExecuteScript("random_encounter", "can_sleep");
	if (!result) {
		logger->error("Unable to check for a random encounter - can_sleep");
		PyErr_Print();
	}

	if (result) {
		*addresses.sleepStatus = PyInt_AsLong(result);
		Py_DECREF(result);
	}
}

/*
	This type is used to get the random encounter premise into python.
*/
struct PyRandomEncounterSetup {
	PyObject_HEAD;
	int terrain; // Type of terrain (from terrain.bmp)
	int flags;
};

static PyMemberDef PyRandomEncounterSetupMembers[] = {
	{ "terrain", T_INT, offsetof(PyRandomEncounterSetup, terrain), NULL, NULL },
	{ "flags", T_INT, offsetof(PyRandomEncounterSetup, flags), NULL, NULL },
	{ NULL, NULL, NULL, NULL, NULL }
};

PyTypeObject PyRandomEncounterSetupType = {
	PyObject_HEAD_INIT(NULL)
	0, /*ob_size*/
	"toee.PyRandomEncounterSetup", /*tp_name*/
	sizeof(PyRandomEncounterSetup), /*tp_basicsize*/
	0, /*tp_itemsize*/
	(destructor)PyObject_Del, /*tp_dealloc*/
	0, /*tp_print*/
	0, /*tp_getattr*/
	0, /*tp_setattr*/
	0, /*tp_compare*/
	0, /*tp_repr*/
	0, /*tp_as_number*/
	0, /*tp_as_sequence*/
	0, /*tp_as_mapping*/
	0, /*tp_hash */
	0, /*tp_call*/
	0, /*tp_str*/
	PyObject_GenericGetAttr, /*tp_getattro*/
	PyObject_GenericSetAttr, /*tp_setattro*/
	0, /*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT, /*tp_flags*/
	0, /* tp_doc */
	0, /* tp_traverse */
	0, /* tp_clear */
	0, /* tp_richcompare */
	0, /* tp_weaklistoffset */
	0, /* tp_iter */
	0, /* tp_iternext */
	0, /* tp_methods */
	PyRandomEncounterSetupMembers, /* tp_members */
	0
};

PyObject *PyRandomEncounterSetup_Create(int terrain, int flags) {
	auto self = PyObject_NEW(PyRandomEncounterSetup, &PyRandomEncounterSetupType);
	self->terrain = terrain;
	self->flags = flags;
	return (PyObject*)self;
}

/*
	This type is used to get the result of random encounter creation from Python into C
*/
struct PyRandomEncounter {
	PyObject_HEAD;
	int id;
	int flags;
	int title;
	int dc;
	int map;
	PyObject *enemies;
	locXY location;
};

static PyMemberDef PyRandomEncounterMembers[] = {
	{ "id", T_INT, offsetof(PyRandomEncounter, id), NULL, NULL },
	{ "flags", T_INT, offsetof(PyRandomEncounter, flags), NULL, NULL },
	{ "title", T_INT, offsetof(PyRandomEncounter, title), NULL, NULL },
	{ "dc", T_INT, offsetof(PyRandomEncounter, dc), NULL, NULL },
	{ "map", T_INT, offsetof(PyRandomEncounter, map), NULL, NULL },
	{ "enemies", T_OBJECT_EX, offsetof(PyRandomEncounter, enemies), NULL, NULL },
	{ "location", T_LONGLONG, offsetof(PyRandomEncounter, location), NULL, NULL },
	{ NULL, NULL, NULL, NULL, NULL }
};

PyTypeObject PyRandomEncounterType = {
	PyObject_HEAD_INIT(NULL)
	0, /*ob_size*/
	"toee.PyRandomEncounter", /*tp_name*/
	sizeof(PyRandomEncounter), /*tp_basicsize*/
	0, /*tp_itemsize*/
	(destructor)PyObject_Del, /*tp_dealloc*/
	0, /*tp_print*/
	0, /*tp_getattr*/
	0, /*tp_setattr*/
	0, /*tp_compare*/
	0, /*tp_repr*/
	0, /*tp_as_number*/
	0, /*tp_as_sequence*/
	0, /*tp_as_mapping*/
	0, /*tp_hash */
	0, /*tp_call*/
	0, /*tp_str*/
	PyObject_GenericGetAttr, /*tp_getattro*/
	PyObject_GenericSetAttr, /*tp_setattro*/
	0, /*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT, /*tp_flags*/
	0, /* tp_doc */
	0, /* tp_traverse */
	0, /* tp_clear */
	0, /* tp_richcompare */
	0, /* tp_weaklistoffset */
	0, /* tp_iter */
	0, /* tp_iternext */
	0, /* tp_methods */
	PyRandomEncounterMembers, /* tp_members */
	0
};

PyRandomEncounter *PyRandomEncounter_Create() {
	if (PyType_Ready(&PyRandomEncounterType)) {
		logger->error("Unable to initialize PyRandomEncounter type");
		return 0;
	}

	auto self = PyObject_New(PyRandomEncounter, &PyRandomEncounterType);
	self->id = 0;
	self->flags = 0;
	self->title = 0;
	self->dc = 0;
	self->map = 0;
	self->enemies = PyTuple_New(0);
	self->location.locx = 0;
	self->location.locy = 0;
	return self;
}

/*
	Calls into random_encounter.encounter_create.
*/
static void __cdecl RandomEncounterCreate(RandomEncounter* encounter) {
	auto args = PyTuple_New(1);

	auto encounterObj = PyRandomEncounter_Create();	
	PyTuple_SET_ITEM(args, 0, (PyObject*)encounterObj);

	encounterObj->id = encounter->id;
	encounterObj->location = encounter->location;
	encounterObj->dc = encounter->dc;
	encounterObj->title = encounter->title;
	encounterObj->flags = encounter->flags;
	encounterObj->map = encounter->map;
	encounterObj->enemies = PyTuple_New(encounter->enemiesCount);
	for (int i = 0; i < encounter->enemiesCount; ++i) {
		auto enemy = encounter->enemies[i];
		auto enemyObj = Py_BuildValue("ii", enemy.protoId, enemy.count);
		PyTuple_SET_ITEM(encounterObj->enemies, i, enemyObj);
	}

	if (encounter->enemies) {
		free(encounter->enemies);
	}
	free(encounter);

	if (encounterObj->map != -1) {
		addresses.MapChange(); // No idea what this does
	}
	auto result = pythonObjIntegration.ExecuteScript("random_encounter", "encounter_create", args);

	Py_DECREF(args);

	if (!result) {
		logger->error("Unable to create a random encounter");
		PyErr_Print();
		return;
	}
	Py_DECREF(result);
}

/*
	Calls into random_encounter.encounter_exists.
*/
static BOOL __cdecl RandomEncounterExists(const RandomEncounterSetup* setup, RandomEncounter **pEncounterOut) {
	auto args = PyTuple_New(2);
	auto terrain = gameSystems->GetRandomEncounter().GetTerrainType((int)setup->location.locx, (int)setup->location.locy);
	//auto terrain = addresses.GetTerrainType(setup->location);
	PyTuple_SET_ITEM(args, 0, PyRandomEncounterSetup_Create(terrain, setup->flags));
	auto encounter = PyRandomEncounter_Create();
	// Flag 1 means "sleep encounter"
	if (setup->flags & 1) {
		encounter->map = -1;
		encounter->location = objects.GetLocation(party.GetLeader());
	}
	PyTuple_SET_ITEM(args, 1, (PyObject*)encounter);
	Py_INCREF(encounter);

	auto result = pythonObjIntegration.ExecuteScript("expanded_encounters", "encounter_exists", args);

	Py_DECREF(args);

	if (!result) {
		logger->error("Unable to check for a random encounter");
		PyErr_Print();
		return FALSE;
	}

	auto encounterExists = PyObject_IsTrue(result);
	Py_DECREF(result);

	if (encounterExists) {
		auto newEncounter = (RandomEncounter*) malloc(sizeof(RandomEncounter));
		*pEncounterOut = newEncounter;

		// Copy over the props to the encounter structure
		newEncounter->id = encounter->id;
		newEncounter->location = encounter->location;
		newEncounter->dc = encounter->dc;
		newEncounter->flags = encounter->flags;
		newEncounter->map = encounter->map;
		newEncounter->title = encounter->title;

		// Convert the array of enemies
		int count = PySequence_Size(encounter->enemies);
		newEncounter->enemiesCount = count;
		newEncounter->enemies = (RandomEncounterEnemy*) malloc(sizeof(RandomEncounterEnemy) * count);
		for (int i = 0; i < count; ++i) {
			auto item = PySequence_GetItem(encounter->enemies, i);
			newEncounter->enemies[i].protoId = PyInt_AsLong(PySequence_GetItem(item, 0));
			newEncounter->enemies[i].count = PyInt_AsLong(PySequence_GetItem(item, 1));
		}
	}
	Py_DECREF(encounter);
	
	return encounterExists;
}

// Python Script Integration Extensions (Random Encounters)
static class PythonRandomEncounterFix : public TempleFix {
public:
	void apply() override {
		replaceFunction(0x10045850, UpdateSleepStatus);
		replaceFunction(0x10046030, RandomEncounterCreate);
		replaceFunction(0x100461E0, RandomEncounterExists);
	}
} fix;
