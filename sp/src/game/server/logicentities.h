#pragma once

#include "cbase.h"
#include "entityinput.h"

#define MAX_LOGIC_CASES 16

class CLogicCase : public CLogicalEntity
{
	DECLARE_CLASS(CLogicCase, CLogicalEntity);
private:
	string_t m_nCase[MAX_LOGIC_CASES];

	int m_nShuffleCases;
	int m_nLastShuffleCase;
	unsigned char m_uchShuffleCaseMap[MAX_LOGIC_CASES];

	void Spawn(void);

	int BuildCaseMap(unsigned char *puchMap);

	// Inputs
	void InputValue(inputdata_t &inputdata);
	void InputPickRandom(inputdata_t &inputdata);
	void InputPickRandomShuffle(inputdata_t &inputdata);
public:
	// Outputs
	COutputEvent m_OnCase[MAX_LOGIC_CASES];		// Fired when the input value matches one of the case values.
	COutputVariant m_OnDefault;					// Fired when no match was found.

	DECLARE_DATADESC();
};

class CLogicCollisionPair : public CLogicalEntity
{
	DECLARE_CLASS(CLogicCollisionPair, CLogicalEntity);
public:

	// Finds the named physics object.  If no name, returns the world
	// If a name is specified and an object not found - errors are reported
	IPhysicsObject *FindPhysicsObjectByNameOrWorld(string_t name, CBaseEntity *pErrorEntity)
	{
		if (!name)
			return g_PhysWorldObject;

		IPhysicsObject *pPhysics = FindPhysicsObjectByName(name.ToCStr(), pErrorEntity);
		if (!pPhysics)
		{
			DevWarning("%s: can't find %s\n", pErrorEntity->GetClassname(), name.ToCStr());
		}
		return pPhysics;
	}

	void EnableCollisions(bool bEnable)
	{
		IPhysicsObject *pPhysics0 = FindPhysicsObjectByNameOrWorld(m_nameAttach1, this);
		IPhysicsObject *pPhysics1 = FindPhysicsObjectByNameOrWorld(m_nameAttach2, this);

		// need two different objects to do anything
		if (pPhysics0 && pPhysics1 && pPhysics0 != pPhysics1)
		{
			m_disabled = !bEnable;
			m_succeeded = true;
			if (bEnable)
			{
				PhysEnableEntityCollisions(pPhysics0, pPhysics1);
			}
			else
			{
				PhysDisableEntityCollisions(pPhysics0, pPhysics1);
			}
		}
		else
		{
			m_succeeded = false;
		}
	}

	void Activate(void)
	{
		if (m_disabled)
		{
			EnableCollisions(false);
		}
		BaseClass::Activate();
	}

	void InputDisableCollisions(inputdata_t &inputdata)
	{
		if (m_succeeded && m_disabled)
			return;
		EnableCollisions(false);
	}

	void InputEnableCollisions(inputdata_t &inputdata)
	{
		if (m_succeeded && !m_disabled)
			return;
		EnableCollisions(true);
	}
	// If Activate() becomes PostSpawn()
	//void OnRestore() { Activate(); }

	DECLARE_DATADESC();

private:
	string_t		m_nameAttach1;
	string_t		m_nameAttach2;
	bool			m_disabled;
	bool			m_succeeded;
};

class CMathColorBlend : public CLogicalEntity
{
public:

	DECLARE_CLASS(CMathColorBlend, CLogicalEntity);

	void Spawn(void);

	// Keys
	float m_flInMin;
	float m_flInMax;
	color32 m_OutColor1;		// Output color when input is m_fInMin
	color32 m_OutColor2;		// Output color when input is m_fInMax

	// Inputs
	void InputValue(inputdata_t &inputdata);

	// Outputs
	COutputColor32 m_OutValue;

	DECLARE_DATADESC();
};

class CLogicBranch : public CLogicalEntity
{
	DECLARE_CLASS(CLogicBranch, CLogicalEntity);

public:

	void UpdateOnRemove();

	void AddLogicBranchListener(CBaseEntity *pEntity);
	inline bool GetLogicBranchState();
	virtual int DrawDebugTextOverlays(void);

private:

	enum LogicBranchFire_t
	{
		LOGIC_BRANCH_FIRE,
		LOGIC_BRANCH_NO_FIRE,
	};

	// Inputs
	void InputSetValue(inputdata_t &inputdata);
	void InputSetValueTest(inputdata_t &inputdata);
	void InputToggle(inputdata_t &inputdata);
	void InputToggleTest(inputdata_t &inputdata);
	void InputTest(inputdata_t &inputdata);

	void UpdateValue(bool bNewValue, CBaseEntity *pActivator, LogicBranchFire_t eFire);

	bool m_bInValue;					// Place to hold the last input value for a future test.

	CUtlVector<EHANDLE> m_Listeners;	// A list of logic_branch_listeners that are monitoring us.

	// Outputs
	COutputEvent m_OnTrue;				// Fired when the value is true.
	COutputEvent m_OnFalse;				// Fired when the value is false.

	DECLARE_DATADESC();
};

class CEnvGlobal : public CLogicalEntity
{
public:
	DECLARE_CLASS(CEnvGlobal, CLogicalEntity);

	void Spawn(void);

	// Input handlers
	void InputTurnOn(inputdata_t &inputdata);
	void InputTurnOff(inputdata_t &inputdata);
	void InputRemove(inputdata_t &inputdata);
	void InputToggle(inputdata_t &inputdata);
	void InputSetCounter(inputdata_t &inputdata);
	void InputAddToCounter(inputdata_t &inputdata);
	void InputGetCounter(inputdata_t &inputdata);

	int DrawDebugTextOverlays(void);

	DECLARE_DATADESC();

	COutputInt m_outCounter;

	string_t	m_globalstate;
	int			m_triggermode;
	int			m_initialstate;
	int			m_counter;			// A counter value associated with this global.
};

class CLogicCompare : public CLogicalEntity
{
	DECLARE_CLASS(CLogicCompare, CLogicalEntity);

public:
	int DrawDebugTextOverlays(void);

private:
	// Inputs
	void InputSetValue(inputdata_t &inputdata);
	void InputSetValueCompare(inputdata_t &inputdata);
	void InputSetCompareValue(inputdata_t &inputdata);
	void InputCompare(inputdata_t &inputdata);

	void DoCompare(CBaseEntity *pActivator, float flInValue);

	float m_flInValue;					// Place to hold the last input value for a recomparison.
	float m_flCompareValue;				// The value to compare the input value against.

	// Outputs
	COutputFloat m_OnLessThan;			// Fired when the input value is less than the compare value.
	COutputFloat m_OnEqualTo;			// Fired when the input value is equal to the compare value.
	COutputFloat m_OnNotEqualTo;		// Fired when the input value is not equal to the compare value.
	COutputFloat m_OnGreaterThan;		// Fired when the input value is greater than the compare value.

	DECLARE_DATADESC();
};

class CLogicCompareInteger : public CLogicalEntity
{
public:
	DECLARE_CLASS(CLogicCompareInteger, CLogicalEntity);

	// outputs
	COutputEvent m_OnEqual;
	COutputEvent m_OnNotEqual;

	// data
	int m_iIntegerValue;
	int m_iShouldCompareToValue;

	DECLARE_DATADESC();

	CMultiInputVar m_AllIntCompares;

	// Input handlers
	void InputValue(inputdata_t &inputdata);
	void InputCompareValues(inputdata_t &inputdata);
};

#define MS_MAX_TARGETS 32

class CMultiSource : public CLogicalEntity
{
public:
	DECLARE_CLASS(CMultiSource, CLogicalEntity);

	void Spawn();
	bool KeyValue(const char *szKeyName, const char *szValue);
	void Use(::CBaseEntity *pActivator, ::CBaseEntity *pCaller, USE_TYPE useType, float value);
	int	ObjectCaps(void) { return(BaseClass::ObjectCaps() | FCAP_MASTER); }
	bool IsTriggered(::CBaseEntity *pActivator);
	void Register(void);

	DECLARE_DATADESC();

	EHANDLE		m_rgEntities[MS_MAX_TARGETS];
	int			m_rgTriggered[MS_MAX_TARGETS];

	COutputEvent m_OnTrigger;		// Fired when all connections are triggered.

	int			m_iTotal;
	string_t	m_globalstate;
};

#define MAX_LOGIC_BRANCH_NAMES 16

class CLogicBranchList : public CLogicalEntity
{
	DECLARE_CLASS(CLogicBranchList, CLogicalEntity);

	virtual void Spawn();
	virtual void Activate();
	virtual int DrawDebugTextOverlays(void);

private:

	enum LogicBranchListenerLastState_t
	{
		LOGIC_BRANCH_LISTENER_NOT_INIT = 0,
		LOGIC_BRANCH_LISTENER_ALL_TRUE,
		LOGIC_BRANCH_LISTENER_ALL_FALSE,
		LOGIC_BRANCH_LISTENER_MIXED,
	};

	void DoTest(CBaseEntity *pActivator);

	string_t m_nLogicBranchNames[MAX_LOGIC_BRANCH_NAMES];
	CUtlVector<EHANDLE> m_LogicBranchList;
	LogicBranchListenerLastState_t m_eLastState;

	// Inputs
	void Input_OnLogicBranchRemoved(inputdata_t &inputdata);
	void Input_OnLogicBranchChanged(inputdata_t &inputdata);
	void InputTest(inputdata_t &inputdata);

	// Outputs
	COutputEvent m_OnAllTrue;			// Fired when all the registered logic_branches become true.
	COutputEvent m_OnAllFalse;			// Fired when all the registered logic_branches become false.
	COutputEvent m_OnMixed;				// Fired when one of the registered logic branches changes, but not all are true or false.

	DECLARE_DATADESC();
};



class CTimerEntity : public CLogicalEntity
{
public:
	DECLARE_CLASS(CTimerEntity, CLogicalEntity);

	void Spawn(void);
	void Think(void);

	void Toggle(void);
	void Enable(void);
	void Disable(void);
	void FireTimer(void);

	int DrawDebugTextOverlays(void);

	// outputs
	COutputEvent m_OnTimer;
	COutputEvent m_OnTimerHigh;
	COutputEvent m_OnTimerLow;

	// inputs
	void InputToggle(inputdata_t &inputdata);
	void InputEnable(inputdata_t &inputdata);
	void InputDisable(inputdata_t &inputdata);
	void InputFireTimer(inputdata_t &inputdata);
	void InputRefireTime(inputdata_t &inputdata);
	void InputResetTimer(inputdata_t &inputdata);
	void InputAddToTimer(inputdata_t &inputdata);
	void InputSubtractFromTimer(inputdata_t &inputdata);

	int m_iDisabled;
	float m_flRefireTime;
	bool m_bUpDownState;
	int m_iUseRandomTime;
	float m_flLowerRandomBound;
	float m_flUpperRandomBound;

	// methods
	void ResetTimer(void);

	DECLARE_DATADESC();
};

class CMathCounter : public CLogicalEntity
{
	DECLARE_CLASS(CMathCounter, CLogicalEntity);
private:
	float m_flMin;		// Minimum clamp value. If min and max are BOTH zero, no clamping is done.
	float m_flMax;		// Maximum clamp value.
	bool m_bHitMin;		// Set when we reach or go below our minimum value, cleared if we go above it again.
	bool m_bHitMax;		// Set when we reach or exceed our maximum value, cleared if we fall below it again.

	bool m_bDisabled;

	bool KeyValue(const char *szKeyName, const char *szValue);
	void Spawn(void);

	int DrawDebugTextOverlays(void);

	void UpdateOutValue(CBaseEntity *pActivator, float fNewValue);

	// Inputs
	void InputAdd(inputdata_t &inputdata);
	void InputDivide(inputdata_t &inputdata);
	void InputMultiply(inputdata_t &inputdata);
	void InputSetValue(inputdata_t &inputdata);
	void InputSetValueNoFire(inputdata_t &inputdata);
	void InputSubtract(inputdata_t &inputdata);
	void InputSetHitMax(inputdata_t &inputdata);
	void InputSetHitMin(inputdata_t &inputdata);
	void InputGetValue(inputdata_t &inputdata);
	void InputEnable(inputdata_t &inputdata);
	void InputDisable(inputdata_t &inputdata);
public:
	// Outputs
	COutputFloat m_OutValue;
	COutputFloat m_OnGetValue;	// Used for polling the counter value.
	COutputEvent m_OnHitMin;
	COutputEvent m_OnHitMax;

	DECLARE_DATADESC();
};

class CLogicAutosave : public CLogicalEntity
{
	DECLARE_CLASS(CLogicAutosave, CLogicalEntity);

protected:
	// Inputs
	void InputSave(inputdata_t &inputdata);
	void InputSaveDangerous(inputdata_t &inputdata);
	void InputSetMinHitpointsThreshold(inputdata_t &inputdata);

	DECLARE_DATADESC();
	bool m_bForceNewLevelUnit;
	int m_minHitPoints;
	int m_minHitPointsToCommit;
};

class CLogicLineToEntity : public CLogicalEntity
{
public:
	DECLARE_CLASS(CLogicLineToEntity, CLogicalEntity);

	void Activate(void);
	void Spawn(void);
	void Think(void);

	// outputs
	COutputVector m_Line;

	DECLARE_DATADESC();

private:
	string_t m_SourceName;
	EHANDLE	m_StartEntity;
	EHANDLE m_EndEntity;
};

class CLogicActiveAutosave : public CLogicAutosave
{
	DECLARE_CLASS(CLogicActiveAutosave, CLogicAutosave);

	void InputEnable(inputdata_t &inputdata)
	{
		m_flStartTime = -1;
		SetThink(&CLogicActiveAutosave::SaveThink);
		SetNextThink(gpGlobals->curtime);
	}

	void InputDisable(inputdata_t &inputdata)
	{
		SetThink(NULL);
	}

	void SaveThink()
	{
		CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
		if (pPlayer)
		{
			if (m_flStartTime < 0)
			{
				if (pPlayer->GetHealth() <= m_minHitPoints)
				{
					m_flStartTime = gpGlobals->curtime;
				}
			}
			else
			{
				if (pPlayer->GetHealth() >= m_TriggerHitPoints)
				{
					inputdata_t inputdata;
					DevMsg(2, "logic_active_autosave (%s, %d) triggered\n", STRING(GetEntityName()), entindex());
					if (!m_flDangerousTime)
					{
						InputSave(inputdata);
					}
					else
					{
						inputdata.value.SetFloat(m_flDangerousTime);
						InputSaveDangerous(inputdata);
					}
					m_flStartTime = -1;
				}
				else if (m_flTimeToTrigger > 0 && gpGlobals->curtime - m_flStartTime > m_flTimeToTrigger)
				{
					m_flStartTime = -1;
				}
			}
		}

		float thinkInterval = (m_flStartTime < 0) ? 1.0 : 0.5;
		SetNextThink(gpGlobals->curtime + thinkInterval);
	}

	DECLARE_DATADESC();

	int m_TriggerHitPoints;
	float m_flTimeToTrigger;
	float m_flStartTime;
	float m_flDangerousTime;
};

class CMathRemap : public CLogicalEntity
{
public:

	DECLARE_CLASS(CMathRemap, CLogicalEntity);

	void Spawn(void);

	// Keys
	float m_flInMin;
	float m_flInMax;
	float m_flOut1;		// Output value when input is m_fInMin
	float m_flOut2;		// Output value when input is m_fInMax

	bool  m_bEnabled;

	// Inputs
	void InputValue(inputdata_t &inputdata);
	void InputEnable(inputdata_t &inputdata);
	void InputDisable(inputdata_t &inputdata);

	// Outputs
	COutputFloat m_OutValue;

	DECLARE_DATADESC();
};