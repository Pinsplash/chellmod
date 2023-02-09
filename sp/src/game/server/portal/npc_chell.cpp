//TODO-02 change obstacle progress to be an int, not enums

//========= Copyright ME, All rights reserved. =========//
// Purpose: The downtrodden chells of City 17.

#include "cbase.h"
#include "npc_chell.h"
#include "weapon_portalgun_shared.h"
#include "portal_placement.h"
#include "globalstate.h"
#include "BasePropDoor.h"
#include "hl2_player.h"
#include "eventqueue.h"
#include "ai_pathfinder.h"
#include "ai_navigator.h"
#include "ai_route.h"
#include "ai_hint.h"
#include "ai_interactions.h"
#include "ai_looktarget.h"
#include "ai_networkmanager.h"
#include "sceneentity.h"
#include "prop_portal.h"
#include "ai_link.h"
#include "entityoutput.h"
#include "doors.h"
#include "triggers.h"
//#include "triggers.cpp"
#include "vphysics/friction.h"
#include "logicentities.h"
#include "logicrelay.h"
#include "weapon_physcannon.h"
#include "tier0/icommandline.h"
//#include "portal/weapon_physcannon.cpp"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar chell_plrctrl_disable("chell_plrctrl_disable", "0", 0);
ConVar chell_plrctrl_enable("chell_plrctrl_enable", "0", 0);
ConVar chell_plrctrl_aim("chell_plrctrl_aim", "1", 0);
ConVar chell_plrctrl_aim_unlock_when_done("chell_plrctrl_aim_unlock_when_done", "1", 0);
ConVar chell_plrctrl_walk("chell_plrctrl_walk", "1", 0);
ConVar chell_plrctrl_walk_noletup("chell_plrctrl_walk_noletup", "0", 0);
/*
namespace
{
	extern Vector FindBumpVectorInCorner(const Vector &ptCorner1, const Vector &ptCorner2, const Vector &ptIntersectionPoint1, const Vector &ptIntersectionPoint2, const Vector &vIntersectionDirection1, const Vector &vIntersectionDirection2, const Vector &vIntersectionBumpDirection1, const Vector &vIntersectionBumpDirection2);
	extern void FitPortalAroundOtherPortals(const CProp_Portal *pIgnorePortal, Vector &vOrigin, const Vector &vForward, const Vector &vRight, const Vector &vUp);
	extern bool FitPortalOnSurface(const CProp_Portal *pIgnorePortal,
		Vector &vOrigin,
		const Vector &vForward,
		const Vector &vRight,
		const Vector &vTopEdge,
		const Vector &vBottomEdge,
		const Vector &vRightEdge,
		const Vector &vLeftEdge,
		int iPlacedBy,
		ITraceFilter *pTraceFilterPortalShot,
		int iRecursions,
		const CPortalCornerFitData *pPortalCornerFitData,
		const int *p_piIntersectionIndex,
		const int *piIntersectionCount);
	extern bool IsMaterialInList(const csurface_t &surface, char *g_ppszMaterials[]);
	//bool IsNoPortalMaterial(const csurface_t &surface);
	extern bool IsPassThroughMaterial(const csurface_t &surface);
	extern bool IsPortalIntersectingNoPortalVolume(const Vector &vOrigin, const QAngle &qAngles, const Vector &vForward);
	extern bool IsPortalOnValidSurface(const Vector &vOrigin, const Vector &vForward, const Vector &vRight, const Vector &vUp, ITraceFilter *traceFilterPortalShot);
	extern bool IsPortalOverlappingOtherPortals(const CProp_Portal *pIgnorePortal, const Vector &vOrigin, const QAngle &qAngles, bool bFizzle);
	extern bool TraceBumpingEntities(const Vector &vStart, const Vector &vEnd, trace_t &tr);
	extern bool TracePortalCorner(const CProp_Portal *pIgnorePortal, const Vector &vOrigin, const Vector &vCorner, const Vector &vForward, int iPlacedBy, ITraceFilter *pTraceFilterPortalShot, trace_t &tr, bool &bSoftBump);
	extern void TracePortals(const CProp_Portal *pIgnorePortal, const Vector &vForward, const Vector &vStart, const Vector &vEnd, trace_t &tr);
	//float VerifyPortalPlacement(const CProp_Portal *pIgnorePortal, Vector &vOrigin, QAngle &qAngles, int iPlacedBy, bool bTest);
	extern bool g_bBumpedByLinkedPortal;
	extern CUtlVector<CBaseEntity *> g_FuncBumpingEntityList;
	extern ConVar sv_portal_placement_debug;
	extern ConVar sv_portal_placement_never_bump;
}
*/

const float CHELL_PORTAL_FIND_WIDTH = 64;//When trying to find valid places to put a portal, how far right the next portal should be
const float CHELL_PORTAL_FIND_HEIGHT = 128;//The actual height is 112 but we're gonna round up cause it will probably just work better if we do.

const float NPC_CHELL_THINKRATE = 0.075;

//const float NPC_CHELL_NAV_ARRIVE_TOLERANCE = 16;//We have to be this close to a waypoint to consider it arrived at
const float NPC_CHELL_NAV_ARRIVE_TOLERANCE_BOX = 60;
const float NPC_CHELL_BOX_BUTTON_PROXIMITY = 90;//info_target goal should spawn this far away from the button trigger

LINK_ENTITY_TO_CLASS( npc_chell, CNPC_Chell );

BEGIN_DATADESC( CNPC_Chell )//Saves play basically no role in developing or using this mod. All the saves I use are created without an npc_chell already in them.
	DEFINE_INPUTFUNC(FIELD_FLOAT,	"Wait",				InputWait),//Scripted waiting. Chamber 0's cube will not drop until you hit a certain trigger,
	//causing a plethora of problems. I tried my best to work around it, but it proved surprisingly tough.
//	DEFINE_INPUTFUNC(FIELD_VOID, "TestComplexNav", InputTestComplexNav),
	DEFINE_INPUTFUNC(FIELD_STRING,	"GoToEntity",		InputGoToEntity),
	DEFINE_INPUTFUNC(FIELD_STRING, "LookAtEntity", InputLookAtEntity),
//	DEFINE_INPUTFUNC(FIELD_FLOAT, "Test", InputTest),
	DEFINE_FIELD	(m_pPlayer,		FIELD_EHANDLE),
END_DATADESC()

CNPC_Chell::~CNPC_Chell(void)
{
	//delete portal shots i made memory for
	//lot of memory related crashes happening...
	//for (int i = 0; i < m_PortalShotsMem.Size(); i++)
	//{
	//	delete &m_PortalShotsMem[i];
	//}
}

void CNPC_Chell::Precache()
{
	PrecacheModel("models/roller.mdl");
	SetModelName(AllocPooledString("models/roller.mdl"));//prev: models/Humans/Group01/female_07.mdl
	BaseClass::Precache();
}

void CNPC_Chell::Spawn()
{
	Msg("Spawn started\n");
	BaseClass::Spawn();
	CapabilitiesAdd(bits_CAP_MOVE_JUMP | bits_CAP_MOVE_GROUND | bits_CAP_SKIP_NAV_GROUND_CHECK );
	AddSolidFlags(FSOLID_NOT_SOLID);
	m_iHealth = 999;//Shouldn't be a problem?
	SetMoveType(MOVETYPE_NONE);
	AddEFlags( EFL_NO_DISSOLVE | EFL_NO_MEGAPHYSCANNON_RAGDOLL | EFL_NO_PHYSCANNON_INTERACTION );
	SetNextThink(gpGlobals->curtime);
	NPCInit();
	m_LookGoal.Init();
	m_vecHypoPortalPos.Zero();
	m_bCanCrossZones = true;
	m_bSolveBlockers = true;
	Msg("Spawn finished\n");
}

void CNPC_Chell::PostNPCInit()
{
	Msg("PostNPCInit started\n");
	AddSpawnFlags(SF_NPC_NO_PLAYER_PUSHAWAY);//We are the player, dont move out of our own way
	m_vecWaypointEyePos = Vector(0, 0, 0);

	m_pPlayer = UTIL_GetLocalPlayer();//Attach to the player
	if (m_pPlayer)
	{
		//Msg("Chell found player\n");
		SetAbsOrigin(m_pPlayer->GetAbsOrigin());
		SetAbsAngles(m_pPlayer->GetAbsAngles());
		SetParent(m_pPlayer);
		m_pPlayer->SetNavIgnore();//Ignore the player for navigation purposes. Otherwise, chell thinks she's stuck inside a solid box and wont do stuff
		GetMotor()->SetYawLocked(true);//Don't turn from AI!

		m_iGunInfo = GUN_NONE;
		CBasePlayer *pPlayer = static_cast<CBasePlayer *>(m_pPlayer);
		if (pPlayer->HasWeapons())//The only weapon in Portal is the portalgun, so this should do fine.
		{
			CWeaponPortalgun *pPortalGun = static_cast<CWeaponPortalgun *>(pPlayer->GetActiveWeapon());
			if (pPortalGun->CanFirePortal1())
			{
				m_iGunInfo = GUN_BLUE;
				if (pPortalGun->CanFirePortal2())
				{
					m_iGunInfo = GUN_BOTH;
				}
			}
		}
	}
	else
	{
		Warning("Chell didn't find player?\n");
	}
	BaseClass::PostNPCInit();
	Msg("PostNPCInit finished\n");
}

//
//update the state of the gun so we know about our new abilities. called when the player picks up the gun.
//
void CNPC_Chell::UpdateGunKnowledge(CBaseCombatWeapon *pWeapon)
{
	CWeaponPortalgun *pPortalGun = static_cast<CWeaponPortalgun *>(pWeapon);
	if (pPortalGun->CanFirePortal1())
	{
		Wait(2);
		m_iGunInfo = GUN_BLUE;
		if (pPortalGun->CanFirePortal2())
		{
			m_iGunInfo = GUN_BOTH;
		}
	}
}

//return true if a blocker was found
bool CNPC_Chell::ScanAheadForBlockers(AI_Waypoint_t *waypoint)
{
	bool bFoundBlocker = false;
	//Draw lines between every waypoint of our path (which is currently ignoring all entities) to find things blocking us.
	while (waypoint != NULL && waypoint->GetNext() != NULL)
	{
		//Msg("CNPC_Chell::ScanAheadForBlockers loop (%i)\n", waypoint->iNodeID);
		trace_t tr;
		UTIL_TraceLine(waypoint->GetPos() + Vector(0, 0, 1), waypoint->GetNext()->GetPos() + Vector(0, 0, 1), MASK_SOLID, m_pPlayer, COLLISION_GROUP_NONE, &tr);
		if (tr.m_pEnt != NULL)
		{
			//Msg("CNPC_Chell::ScanAheadForBlockers BAD %0.1f %0.1f %0.1f %0.1f %0.1f %0.1f\n", waypoint->GetPos().x, waypoint->GetPos().y, waypoint->GetPos().z + 1, waypoint->GetNext()->GetPos().x, waypoint->GetNext()->GetPos().y, waypoint->GetNext()->GetPos().z + 1);
			//We found a blocker
			if (NotifyNavBlocker(tr.m_pEnt) == SHOULDHIT_YES)//What about SHOULDHIT_DEFAULT?
			{
				bFoundBlocker = true;
			}
			//Do not "simplify" a path around the point if it goes through a portal! otherwise chell may not think it's correct to go through
			if (m_pPortal1Node && m_pPortal2Node)
			{
				//Msg("Checking DoNotReduce for node %i\n", waypoint->iNodeID);
				if (waypoint->iNodeID == m_pPortal1Node->m_iID || waypoint->iNodeID == m_pPortal2Node->m_iID)
				{
					//Warning("Should not simplify path around node %i (%0.1f, %0.1f, %0.1f)\n", waypoint->iNodeID, waypoint->GetPos().x, waypoint->GetPos().y, waypoint->GetPos().z);
					//if (waypoint->GetPrev() && waypoint->GetNext() && waypoint->GetNext()->iNodeID != m_pPortal1Node->m_iID && waypoint->GetNext()->iNodeID != m_pPortal2Node->m_iID)//Test 01: there's a -1 that ought to be simplfied but apparently we're fucking that up here
					//{
					//___ - bad
					//0__ - bad
					//_0_ - doubled back
					//__0 - doubled back quickly?
					//_00 - doubled back
					//0_0 - doubled back
					//00_ - doubled back
					//000 - doubled back
					waypoint->ModifyFlags(bits_WP_DONT_SIMPLIFY, true);
					if (waypoint->GetPrev())
						waypoint->GetPrev()->ModifyFlags(bits_WP_DONT_SIMPLIFY, true);
					if (waypoint->GetNext())
						waypoint->GetNext()->ModifyFlags(bits_WP_DONT_SIMPLIFY, true);
				}
				else
				{
					//Msg("OK to simplify path around node %i (%0.1f, %0.1f, %0.1f)\n", waypoint->iNodeID, waypoint->GetPos().x, waypoint->GetPos().y, waypoint->GetPos().z);
				}
			}

		}
		else
		{
			//Msg("CNPC_Chell::ScanAheadForBlockers GOOD %0.1f %0.1f %0.1f %0.1f %0.1f %0.1f\n", waypoint->GetPos().x, waypoint->GetPos().y, waypoint->GetPos().z + 1, waypoint->GetNext()->GetPos().x, waypoint->GetNext()->GetPos().y, waypoint->GetNext()->GetPos().z + 1);
		}
		waypoint = waypoint->GetNext();
	}
	return bFoundBlocker;
}

void CNPC_Chell::FindObstacles()
{
	//Msg("CNPC_Chell::FindObstacles marker 1\n");
	if (GetNavigator()->GetPath() != NULL)
	{
		//Msg("CNPC_Chell::FindObstacles marker 2\n");
		AI_Waypoint_t *waypoint = GetNavigator()->GetPath()->GetCurWaypoint();
		ScanAheadForBlockers(waypoint);
	}
	else
	{
		//Msg("CNPC_Chell::FindObstacles marker 6\n");
	}
}

//
//Do something about whatever is in our way
//
void CNPC_Chell::EvaluateObstacle()
{
	if (gpGlobals->curtime < m_flWaitTime)
	{
		for (int i = 0; i < m_Obstacles.Count(); i++)
		{
			m_Obstacles[i].iObstacleState = OBST_STATE_UNSOLVED;//Obstacles probably aren't really in progress if we're in wait time. this is heavy handed!
		}
	}
	//if (m_ChellGoal == NULL)
	//	return;
	if (m_flEvaluateObstacleTime > gpGlobals->curtime)
		return;
	for (int i = 0; i < m_Obstacles.Count(); i++)
	{
		//Msg("CNPC_Chell::EvaluateObstacle marker 1 type is '%s'\n", m_Obstacles[i].type);
		if (m_Obstacles[i].iObstacleState == OBST_STATE_SOLVED)
		{
			//Msg("CNPC_Chell::EvaluateObstacle marker 1a\n");
			continue;
		}
		if (strcmp(m_Obstacles[i].type, "test_chamber_door") == 0)
		{
			//Msg("CNPC_Chell::EvaluateObstacle marker 2\n");
			SolveDoor(&m_Obstacles[i]);
		}
	}
	m_flEvaluateObstacleTime = gpGlobals->curtime + .2;

	//hypo-portals: Our nav goal requires us to go through a portal that may not exist at a given point in time.
	if (m_vecHypoPortalPos.Length())
	{
		//Msg("HypoPortal exists\n");
		bool bOpen = false;
		//Find out if the hypo-portal exists.
		CBaseEntity *pPortalEnt = gEntList.FindEntityByClassname(NULL, "prop_portal");
		while (pPortalEnt)
		{
			if ((pPortalEnt->GetAbsOrigin() - m_vecHypoPortalPos).Length() < 25)//Portal may not be precisely where we thought it would be, so allow some tolerance.
			{
				//portal must also be open, as we're currently trying to determine where we should walk to.
				CProp_Portal *pPortal = static_cast<CProp_Portal*>(pPortalEnt);
				if (pPortal->IsActivedAndLinked())
				{
					bOpen = true;
					break;
				}
			}
			pPortalEnt = gEntList.FindEntityByClassname(pPortalEnt, "prop_portal");
		}
		//If the portal is not there, go stand where it will be.
		if (!bOpen && !m_SavedGoal)
		{
			m_SavedGoal = m_ChellGoal;//Remember where we were going before!
			CBaseEntity *pTarget = CreateEntityByName("info_target");
			pTarget->SetAbsOrigin(m_vecHypoPortalPos);
			pTarget->SetName(MAKE_STRING("chelldest_hypoportal"));
			ChellNavToEntity("chelldest_hypoportal", "going to stand by where a portal will be", 0);
		}
		//If it is there, go to the original goal.
		if (bOpen && m_SavedGoal)
		{
			const char *goalname = m_SavedGoal->GetEntityName().ToCStr();
			ChellNavToEntity(goalname, "hypoportal appeared, going back to regular goal", 0);
			//Do not erase m_vecHypoPortalPos here, the portal could go away again before we reach it! see portalcross
			m_SavedGoal = NULL;
		}
	}
}

//
//Find a way to open a door
//
void CNPC_Chell::SolveDoor(obstacle_t *obst)
{
	//Msg("Solving door.\n");
	if (obst->pEnt->GetParent())
	{
		CBaseEntity *pEnt = obst->pEnt->GetParent();
		CBaseDoor *pFuncDoor = dynamic_cast<CBaseDoor*>(pEnt);
		if (pFuncDoor)
		{
			if (pFuncDoor->m_toggle_state == TS_AT_TOP || pFuncDoor->m_toggle_state == TS_GOING_UP)
			{
				Msg("\n\n\nDOOR SOLVED\n\n\n");
				//This door is open. If we hit this, then the door was solved. Avoid false positives!!!
				obst->iObstacleState = OBST_STATE_SOLVED;
				if (m_flCurrentPathLength == 0)
				{
					GetNavigator()->OnNavComplete();
					m_LookGoal.Zero();
					m_vecWaypointEyePos.Zero();
					m_bLookedToCurWaypoint = false;
					m_pCurWaypoint = NULL;
					m_pLastWaypoint = NULL;
					if (m_bInCycle)
					{
						ChellNavToEntity("chelldest_cyclewaitingplace", "going back to static portal after finished dropping cube", 0);
					}
					else
					{
						ChellNavToEntity(m_strChellGoal, "continuing to final goal after opening test chamber door", 0);
					}
				}
				if (GetPlayerHeldEntity(m_pPlayer))
				{
					PlayerUseDrop("probably in the middle of putting a cube onto a button");
					m_bDroppingObject = false;
					PlayerDuckEnd();
				}
				return;
			}

			//Find a button that will open this door
			CBaseEntity *pBoxTrigger = FindButtonForDoor(pFuncDoor);
			if (pBoxTrigger)
			{
				//TODO: replace with a real check
				if (/*obst->iObstacleState != OBST_STATE_SOLVING ||*/ !GetNavigator()->GetPath()->GetCurWaypoint())
				{
					CBaseEntity *pBestBox = FindWeightForButton();
					if (pBestBox && pBestBox != GetPlayerHeldEntity(m_pPlayer))
					{
						chellNavQuery_t resToBox = ComplexNavTest(GetAbsOrigin(), pBestBox->GetAbsOrigin(), QF_SIMPLE_ONLY | QF_DONT_DETECT_CYCLE);
						if (m_bInCycle && resToBox.result == CNQR_NO)
						{
							ChellNavToEntity("chelldest_cyclewaitingplace", "waiting for portal to change so i can pick up cube", QF_DONT_DETECT_CYCLE);
						}
						else
						{
							obst->iObstacleState = OBST_STATE_SOLVING;
							ApproachCubeForPickup(pBestBox);
							return;
						}
					}
				}
				//We should hit this only after picking up the cube
				if (GetPlayerHeldEntity(m_pPlayer) && m_flCurrentPathLength == 0 && !m_bDroppingObject && strcmp(STRING(m_ChellGoal->GetEntityName()), "chelldest_boxtobutton"))
				{
					//If we found a path to a box, and we haven't begun a new path since finishing the last one
					ApproachButtonWithCube(pBoxTrigger);
					return;
				}
			}

			if (obst->iObstacleState != OBST_STATE_SOLVING)
			{
				//test 02, the 2nd door opens after picking up the portal gun.
				if (m_iGunInfo == GUN_NONE)
				{
					CBaseEntity *pGun = FindUnheldPortalGun();
					if (pGun)
					{
						ChellNavToEntity(STRING(pGun->GetEntityName()), "going to go pick up portal gun", 0);
						obst->iObstacleState = OBST_STATE_SOLVING;
						return;
					}
				}
				//find trigger for door
				CBaseEntity *pDoorTrigger = FindTriggerForDoor(pFuncDoor);
				if (pDoorTrigger)
				{
					ChellNavToEntity(STRING(pDoorTrigger->GetEntityName()), "going to go activate trigger which will open door", 0);
					obst->iObstacleState = OBST_STATE_SOLVING;
					return;
				}
			}
		}
	}
}

CBaseEntity *CNPC_Chell::FindButtonForDoor(CBaseDoor *pFuncDoor)
{
	bool bFoundBoxTrigger = false;
	AI_Waypoint_t *pFirstWaypoint;
	CBaseEntity *pBoxTrigger = gEntList.FindEntityByClassname(NULL, "trigger_multiple");
	CTriggerMultiple *pTrigger;//make a test route to the button to see if possible to reach
	while (pBoxTrigger)
	{
		//Wait WHAT? does this check really make sense?
		//if (m_pHeldObject != NULL)//No point in doing this before we have a cube
		//{
			//Msg("Test route\n");
			pFirstWaypoint = GetPathfinder()->BuildRoute(GetAbsOrigin(), pBoxTrigger->GetAbsOrigin(), pBoxTrigger, NPC_CHELL_NAV_ARRIVE_TOLERANCE_BOX, true, GetNavType());
		//}
			if (!pFirstWaypoint && GetPlayerHeldEntity(m_pPlayer))
		{
			//ConColorMsg(Color(255, 0, 255, 255), "Button rejected because unreachable at %0.1f %0.1f %0.1f\n", pBoxTrigger->GetAbsOrigin().x, pBoxTrigger->GetAbsOrigin().y, pBoxTrigger->GetAbsOrigin().z);
			pBoxTrigger = gEntList.FindEntityByClassname(pBoxTrigger, "trigger_multiple");
			continue;
		}
		else
		{
			//ConColorMsg(Color(255, 0, 255, 255), "Reachable button at %0.1f %0.1f %0.1f\n", pBoxTrigger->GetAbsOrigin().x, pBoxTrigger->GetAbsOrigin().y, pBoxTrigger->GetAbsOrigin().z);
		}
		pTrigger = dynamic_cast<CTriggerMultiple*>(pBoxTrigger);
		if (pTrigger)
		{
			CEventAction *ev = pTrigger->m_OnTrigger.m_ActionList;
			while (ev != NULL)
			{
				if (strcmp(STRING(ev->m_iTarget), STRING(pFuncDoor->GetEntityName())) && strcmp(STRING(ev->m_iTargetInput), "Open"))
				{
					//ConColorMsg(Color(255, 0, 255, 255), "Found a floor button to open the door that's blocking me!\n");
					bFoundBoxTrigger = true;//This trigger makes the door open
					break;
				}
				ev = ev->m_pNext;
			}
			if (bFoundBoxTrigger)
				break;
		}
		else
		{
			pBoxTrigger = gEntList.FindEntityByClassname(pBoxTrigger, "trigger_multiple");
		}
	}
	return pBoxTrigger;
}

CBaseEntity *CNPC_Chell::FindTriggerForDoor(CBaseDoor *pFuncDoor)
{
	bool bFoundDoorTrigger = false;
	AI_Waypoint_t *pFirstWaypoint;
	CBaseEntity *pDoorTrigger = gEntList.FindEntityByClassname(NULL, "trigger_once");
	CTriggerMultiple *pTrigger;//make a test route to the trigger to see if possible to reach
	while (pDoorTrigger)
	{
		pFirstWaypoint = GetPathfinder()->BuildRoute(GetAbsOrigin(), pDoorTrigger->GetAbsOrigin(), pDoorTrigger, NPC_CHELL_NAV_ARRIVE_TOLERANCE, true, GetNavType());
		if (!pFirstWaypoint)
		{
			//ConColorMsg(Color(255, 0, 255, 255), "Trigger rejected because unreachable at %0.1f %0.1f %0.1f\n", pDoorTrigger->GetAbsOrigin().x, pDoorTrigger->GetAbsOrigin().y, pDoorTrigger->GetAbsOrigin().z);
			pDoorTrigger = gEntList.FindEntityByClassname(pDoorTrigger, "trigger_once");
			continue;
		}
		else
		{
			//ConColorMsg(Color(255, 0, 255, 255), "Reachable trigger at %0.1f %0.1f %0.1f\n", pDoorTrigger->GetAbsOrigin().x, pDoorTrigger->GetAbsOrigin().y, pDoorTrigger->GetAbsOrigin().z);
		}
		pTrigger = dynamic_cast<CTriggerMultiple*>(pDoorTrigger);
		if (pTrigger)
		{
			CEventAction *ev = pTrigger->m_OnTrigger.m_ActionList;
			while (ev != NULL)
			{
				if (!strcmp(STRING(ev->m_iTarget), STRING(pFuncDoor->GetEntityName())) && !strcmp(STRING(ev->m_iTargetInput), "Open"))
				{
					//ConColorMsg(Color(255, 0, 255, 255), "Found a door trigger to open the door that's blocking me!\n");
					bFoundDoorTrigger = true;//This trigger makes the door open
					break;
				}
				ev = ev->m_pNext;
			}
			if (bFoundDoorTrigger)
				break;
		}
		//else
		//{
			pDoorTrigger = gEntList.FindEntityByClassname(pDoorTrigger, "trigger_once");
		//}
	}
	//Msg("Returning\n");
	return pDoorTrigger;
}

CBaseEntity *CNPC_Chell::FindWeightForButton()
{
	ConColorMsg(Color(255, 255, 0, 255), "FindWeightForButton()\n");
	CBaseEntity *pBestBox = NULL;
	CBaseEntity *pBox = gEntList.FindEntityByName(NULL, "box&*");
	float flBestDist = 0;
	while (pBox)
	{
		//TODO-5: Dont try using cubes already in use
		ConColorMsg(Color(255, 255, 0, 255), "Current box name: '%s' pos: %0.1f %0.1f %0.1f\n", STRING(pBox->GetEntityName()), pBox->GetAbsOrigin().x, pBox->GetAbsOrigin().y, pBox->GetAbsOrigin().z);

		//CPhysicsProp* pBoxProp = dynamic_cast<CPhysicsProp*>(pBox);
		if (pBox == GetPlayerHeldEntity(m_pPlayer))
		{
			//Surely the box we're holding is the best possible one?
			ConColorMsg(Color(255, 255, 0, 255), "We are holding this box\n");
			pBestBox = pBox;
			break;
		}


		trace_t tr;
		//previously the ignore entity here was obst->pEnt. why?
		UTIL_TraceLine(pBox->GetAbsOrigin(), pBox->GetAbsOrigin() - Vector(0, 0, 1024), MASK_SOLID, NULL, COLLISION_GROUP_DEBRIS, &tr);
		chellNavQuery_t resToBox = ComplexNavTest(GetAbsOrigin(), tr.endpos, 0);
		if (resToBox.result == CNQR_NO)
		{
			bool bReachable = false;//must find at least one way to reach it
			//this bit of code would be extremely useful in its own function, but should i put that function here, or in the main test function, or somewhere else???
			if (m_bInCycle && !m_pCyclePortals.IsEmpty() && m_pStaticPortal)
			{
				//first try going from current pos to cycle to static to box
				chellNavQuery_t resStaticToBox = ComplexNavTest(m_pStaticPortal->GetAbsOrigin(), tr.endpos, 0);
				if (resStaticToBox.result != CNQR_NO)
				{
					for (int i = 0; i < m_pCyclePortals.Count(); i++)
					{
						chellNavQuery_t resToCycle = ComplexNavTest(GetAbsOrigin(), m_pCyclePortals[i]->GetAbsOrigin(), 0);
						if (resToCycle.result != CNQR_NO)
						{
							bReachable = true;
							break;
						}
					}
				}
				//now try cur pos to static to cycle to box
				if (!bReachable)
				{
					chellNavQuery_t resToStatic = ComplexNavTest(GetAbsOrigin(), m_pStaticPortal->GetAbsOrigin(), 0);
					if (resToStatic.result != CNQR_NO)
					{
						for (int i = 0; i < m_pCyclePortals.Count(); i++)
						{
							chellNavQuery_t resCycleToBox = ComplexNavTest(m_pCyclePortals[i]->GetAbsOrigin(), tr.endpos, 0);
							if (resCycleToBox.result != CNQR_NO)
							{
								bReachable = true;
								break;
							}
						}
					}
				}
			}
			if (!bReachable)
			{
				ConColorMsg(Color(255, 255, 0, 255), "Box '%s' rejected because unreachable %0.1f %0.1f %0.1f\n", STRING(pBox->GetEntityName()), tr.endpos.x, tr.endpos.y, tr.endpos.z);
				pBox = gEntList.FindEntityByName(pBox, "box&*");
				continue;
			}
		}
		else
		{
			ConColorMsg(Color(255, 255, 0, 255), "Reachable box '%s' at %0.1f %0.1f %0.1f\n", STRING(pBox->GetEntityName()), tr.endpos.x, tr.endpos.y, tr.endpos.z);
		}

		//UNDONE: Only accept cubes that are on the ground. This causes too many problems in the first test chamber.
		//bool bOnGround = false;
		//IPhysicsFrictionSnapshot *pSnapshot = pBox->VPhysicsGetObject()->CreateFrictionSnapshot();
		//
		//while (pSnapshot->IsValid())
		//{
		//	IPhysicsObject *pOther = pSnapshot->GetObject(1);
		//	CBaseEntity *pEntity0 = static_cast<CBaseEntity *>(pOther->GetGameData());
		//	if (FStrEq(pEntity0->GetClassname(), "worldspawn"))
		//	{
		//		ConColorMsg(Color(255, 255, 0, 255), "Box '%s' is touching world, so we like it.\n", STRING(pBox->GetEntityName()));
		//		bOnGround = true;
		//		break;
		//	}
		//	pSnapshot->NextFrictionData();
		//}
		//pSnapshot->DeleteAllMarkedContacts(true);
		//pBox->VPhysicsGetObject()->DestroyFrictionSnapshot(pSnapshot);
		//if (!bOnGround)
		//{
		//	ConColorMsg(Color(255, 255, 0, 255), "Box '%s' rejected because in midair\n", STRING(pBox->GetEntityName()));
		//	pBox = gEntList.FindEntityByName(pBox, "box&*");
		//	continue;
		//}




		if (!pBestBox)
		{
			ConColorMsg(Color(255, 255, 0, 255), "No best box, making it our first one '%s'\n", STRING(pBox->GetEntityName()));
			pBestBox = pBox;
			flBestDist = (pBox->GetAbsOrigin() - GetAbsOrigin()).LengthSqr();
		}
		float flNewDist = (pBox->GetAbsOrigin() - GetAbsOrigin()).LengthSqr();
		if (flNewDist <= flBestDist)
		{
			ConColorMsg(Color(255, 255, 0, 255), "New best box! name '%s' new dist: %f old dist: %f\n", STRING(pBox->GetEntityName()), flNewDist, flBestDist);
			ConColorMsg(Color(255, 255, 0, 255), "New pos: %0.1f %0.1f %0.1f\n", pBox->GetAbsOrigin().x, pBox->GetAbsOrigin().y, pBox->GetAbsOrigin().z);
			pBestBox = pBox;
			flBestDist = flNewDist;
		}
		pBox = gEntList.FindEntityByName(pBox, "box&*");
	}
	if (!pBestBox)
	{
		if (m_bInCycle)
		{
			m_bLastBoxFindFailed = true;
		}
		//Warning("TODO-19: Use self on button");
		//No box! we just have to stand on the button. Flesh this out only once necessary, or this will probably be confusing whenever box detection is bunked up
	}
	return pBestBox;
}

void CNPC_Chell::ApproachCubeForPickup(CBaseEntity *pBox)
{
	if (m_bLastBoxFindFailed)
	{
		if (m_bInCycle)
		{
			m_flLastCyclePortalTime = gpGlobals->curtime;
		}
		m_bLastBoxFindFailed = false;
	}
	CBaseEntity *pUseEntity = GetPlayerHeldEntity(m_pPlayer);
	if (pUseEntity && pUseEntity != pBox)
	{
		PlayerUseDrop("we're holding something we probably don't need to hold.");
	}
	ChellNavToEntity(pBox->GetEntityName().ToCStr(), "going to pick up cube", QF_DONT_DETECT_CYCLE);
}

void CNPC_Chell::ApproachButtonWithCube(CBaseEntity *pBoxTrigger)
{
	AI_Waypoint_t *pFirstWaypoint = GetPathfinder()->BuildRoute(GetAbsOrigin(), pBoxTrigger->GetAbsOrigin(), NULL, NPC_CHELL_NAV_ARRIVE_TOLERANCE_BOX, true, GetNavType());
	//It's time to navigate to the button, but we don't want to walk directly onto it.
	//create an info_target on the ground near the button in the direction of the waypoint before the button

	//CAI_Path tempPath;//We have to go into CAI_Path code cause apparently otherwise the waypoint doesn't have a pNext or pPrev TODO-2: Probably safe to cut all this junk. We later learned that's just how local nav is.
	//GetPathfinder()->UnlockRouteNodes(pFirstWaypoint);//Is this necessary?
	//tempPath.SetWaypoints(pFirstWaypoint);

	//pFirstWaypoint = tempPath.GetCurWaypoint();
	//AI_Waypoint_t *pFirstClipped = GetNavigator()->m_pClippedWaypoints->GetFirst();
	//GetNavigator()->m_pClippedWaypoints->Set(NULL);
	//pFirstClipped->ModifyFlags(bits_WP_DONT_SIMPLIFY, true);
	//tempPath.PrependWaypoints(pFirstClipped);
	//pFirstWaypoint = pFirstClipped;

	//It's possible for the path to be local nav (only one waypoint) instead of node based (multiple waypoints).
	AI_Waypoint_t *pLastWaypoint;
	Vector vecTravelDirection;
	Vector vecIdealStopSpot;
	//Don't go straight to the button, but rather stop just before it. The goal is to put the cube, not chell, on the button.
	if (pFirstWaypoint->GetNext() != NULL)
	{
		pLastWaypoint = pFirstWaypoint->GetLast();
		vecTravelDirection = pLastWaypoint->GetPos() - pLastWaypoint->GetPrev()->GetPos();
		vecTravelDirection.z = 0;
		VectorNormalize(vecTravelDirection);
		vecTravelDirection *= NPC_CHELL_BOX_BUTTON_PROXIMITY;
		vecIdealStopSpot = pLastWaypoint->GetPos() - vecTravelDirection;
		vecIdealStopSpot.z = pLastWaypoint->GetPrev()->GetPos().z;
	}
	else
	{
		pLastWaypoint = pFirstWaypoint;
		vecTravelDirection = pLastWaypoint->GetPos() - GetAbsOrigin();
		vecTravelDirection.z = 0;
		VectorNormalize(vecTravelDirection);
		vecTravelDirection *= NPC_CHELL_BOX_BUTTON_PROXIMITY;
		vecIdealStopSpot = pLastWaypoint->GetPos() - vecTravelDirection;
		vecIdealStopSpot.z = GetAbsOrigin().z;
	}
	//Check for pre-existing info_targets so we don't make a new one every think. That would be bad.
	CBaseEntity *pTargetEnt = gEntList.FindEntityByName(NULL, "chelldest_boxtobutton");
	if (pTargetEnt)
	{
		if ((pTargetEnt->GetAbsOrigin() - vecIdealStopSpot).Length() < 16)
		{
			Msg("Found a very near boxtobutton target\n");
			ChellNavToEntity("chelldest_boxtobutton", "going to go put cube on button", 0);
		}
		else
		{
			Msg("Removed old boxtobutton target\n");
			pTargetEnt->SUB_Remove();
		}
	}
	else
	{
		Msg("No old boxtobutton target\n");
	}
	CBaseEntity *pTarget = CreateEntityByName("info_target");
	pTarget->SetAbsOrigin(vecIdealStopSpot);
	pTarget->SetName(MAKE_STRING("chelldest_boxtobutton"));
	pTarget->m_target = pBoxTrigger->GetEntityName();//Make the info_target remember the button it's for

	//have chell go to the info_target instead of the button's box trigger
	ChellNavToEntity("chelldest_boxtobutton", "going to go put cube on button", 0);
}

CBaseEntity *CNPC_Chell::FindUnheldPortalGun()
{
	bool bUnheldPortalGun = false;
	CBaseEntity *pGun = gEntList.FindEntityByClassname(NULL, "weapon_portalgun");
	while (pGun)
	{
		CWeaponPortalgun *pPortalGun = static_cast<CWeaponPortalgun *>(pGun);
		if (!pPortalGun->GetOwner())
		{
			bUnheldPortalGun = true;
			break;
		}
		pGun = gEntList.FindEntityByClassname(pGun, "weapon_portalgun");
	}
	if (!bUnheldPortalGun)
	{
		pGun = NULL;
	}
	return pGun;
}

//
//Look somewhere
//
void CNPC_Chell::SteerIncremental()
{
	//Avoid losing grip of a cube if it gets caught on the wall next to a portal.
	bool bForceCubeOut = false;
	if (GetPlayerHeldEntity(m_pPlayer) && m_bWalkReverse && m_flCrossTime != 0)
	{
		if (gpGlobals->curtime > m_flCrossTime && m_iForceCubeOut == DONT_WIGGLE)
		{
			Msg("Wiggle left\n");
			bForceCubeOut = true;
			m_iForceCubeOut = WIGGLE_LEFT;
		}
		if (gpGlobals->curtime > m_flCrossTime + 1 && m_iForceCubeOut == WIGGLE_LEFT)
		{
			Msg("Wiggle right\n");
			bForceCubeOut = true;
			m_iForceCubeOut = WIGGLE_RIGHT;
		}
		if (gpGlobals->curtime > m_flCrossTime + 2 && m_iForceCubeOut == WIGGLE_RIGHT)
		{
			Msg("Stop wiggle\n");
			bForceCubeOut = false;
			m_iForceCubeOut = DONT_WIGGLE;
		}
	}
	else if (m_iForceCubeOut != DONT_WIGGLE)
	{
		Msg("Stop wiggle\n");
		bForceCubeOut = false;
		m_iForceCubeOut = DONT_WIGGLE;
	}
	if (m_LookGoal.Length())//Figuring out in what directions to turn the camera based off:
	{
		Vector vPlayerOrigin = m_pPlayer->EyePosition();//Where we are looking from
		QAngle angPlayerView = m_pPlayer->EyeAngles();//Direction we are looking in as an angle
		//Vector vPlayerView = ((CBaseCombatCharacter*)m_pPlayer)->EyeDirection3D();//What direction we're looking in as a vector
		Vector vTargetView = m_LookGoal - vPlayerOrigin;//Direction we want to look in as a vector

		if (m_bWalkReverse)
		{
			vTargetView = -vTargetView;
		}

		//Direction we want to look in as an angle (m_angTargetView)
		VectorAngles(vTargetView, m_angTargetView);//Convert vector to an angle
		if (m_angTargetView[0] > 180)//Pretty sure this should be >
			m_angTargetView[0] -= 360;//To fix a discrepancy in how the two angles are found
		if (m_angTargetView[1] > 180)
			m_angTargetView[1] -= 360;

		//Msg("LookGoal: %0.1f, %0.1f, %0.1f \n", m_LookGoal[0], m_LookGoal[1], m_LookGoal[2]);
		//Msg("PlayerPos: %0.1f, %0.1f, %0.1f ", vPlayerOrigin[0], vPlayerOrigin[1], vPlayerOrigin[2]);
		//Msg("PlayerView: %0.1f, %0.1f, %0.1f ", angPlayerView[0], angPlayerView[1], angPlayerView[2]);
		//Msg("TargetViewXYZ: %0.1f, %0.1f, %0.1f \n", vTargetView[0], vTargetView[1], vTargetView[2]);
		//Msg("TargetViewPYR: %0.1f, %0.1f, %0.1f \n", m_angTargetView[0], m_angTargetView[1], m_angTargetView[2]);

		//Compare the current and target pitches and yaws. For the record, the player camera does have roll (angPlayerView[2]) when the
		//view is reorienting after going through portals, but it shouldn't be a big deal (correcting for it would probably be overkill)

		//If player pitch is greater than target, we're looking below it
		bool lookUp = angPlayerView[0] - m_angTargetView[0] > 0;
		//Msg("\ncur P: %0.1f, tar P: %0.1f (cur - tar = %0.1f)\n", angPlayerView[0], m_angTargetView[0], angPlayerView[0] - m_angTargetView[0]);
		m_bPitchOkay = abs(angPlayerView[0] - m_angTargetView[0]) < GetAimTolerance(abs(angPlayerView[0] - m_angTargetView[0]), true);//Close enough
		//if (!m_bPitchOkay)
		//	Msg("Look %s\n", lookUp ? "UP" : "DOWN");

		//If player yaw is less than target, we're looking to the right of it
		//Our yaw is somewhere between -180 and 180, if the current is -179 and the target is 179, we'd rather turn 2 degrees right than 358 left
		float angle = m_angTargetView[1] - angPlayerView[1];//So copy this from stackoverflow
		angle += (angle > 180) ? -360 : (angle < -180) ? 360 : 0;
		bool lookLeft = angle > 0;
		//Msg("cur Y: %0.1f, tar Y: %0.1f (diff: = %0.1f)\n", angPlayerView[1], m_angTargetView[1], abs(angle));
		m_bYawOkay = abs(angle) < GetAimTolerance(abs(angle), false);//Close enough
		//if (!m_bYawOkay)
		//	Msg("Look %s\n", lookLeft ? "LEFT" : "RIGHT");

		if (!chell_plrctrl_disable.GetBool())//Now aim!
		{
			if (m_bPitchOkay)//we're at the target pitch now, quit adjusting it
			{
				//Msg("Pitch is good\n");
				SendCommand("-lookup");
				SendCommand("-lookdown");
			}
			else//Not at target pitch yet
			{
				if (lookUp)
				{
					//Msg("Looking up\n");
					PlayerLookUp(abs(angPlayerView[0] - m_angTargetView[0]));
				}
				else
				{
					//Msg("Looking down\n");
					PlayerLookDown(abs(angPlayerView[0] - m_angTargetView[0]));
				}
			}
			if (m_bYawOkay)//we're at the target yaw now, quit adjusting it
			{
				//Msg("Yaw is good\n");
				SendCommand("-left");
				SendCommand("-right");
			}
			else// if (!bForceCubeOut)//Not at target yaw yet
			{
				//Msg("lookLeft = %s\n", lookLeft ? "TRUE" : "FALSE");
				//Msg("m_iForceCubeOut = %i\n", m_iForceCubeOut);
				if (lookLeft || (bForceCubeOut && m_iForceCubeOut == WIGGLE_LEFT))
				{
					//Msg("Looking left\n");
					//if (bForceCubeOut)
					//	m_iForceCubeOut = WIGGLE_RIGHT;
					PlayerLookLeft(abs(angle));
				}
				if (!lookLeft || (bForceCubeOut && m_iForceCubeOut != WIGGLE_RIGHT))
				{
					//Msg("Looking right\n");
					//if (bForceCubeOut)
					//	m_iForceCubeOut = WIGGLE_RIGHT;
					PlayerLookRight(abs(angle));
				}
			}
			//we are looking at goal
			//or if we're picking up an object, we are able to pick it up before looking directly at it
			if (m_bPitchOkay && m_bYawOkay || (m_bPickingUpObject && m_pPlayer->FindUseEntity() == m_ChellGoal))
			{
				//if (chell_plrctrl_aim_unlock_when_done.GetBool())//quit trying to aim at the target
				//{
					//Msg("AIMED TO GOAL\n");
					if (m_bPickingUpObject)
					{
						Msg("Looked to pickup object\n\n");
						m_bPickingUpObject = false;
						PlayerUsePickUp("Picking up object.");
						Msg("in cycle: %s\n", m_bInCycle ? "YES" : "NO");
						if (m_bInCycle)
						{
							ChellNavToEntity("chelldest_cyclewaitingplace", "going back to static portal after finished picking up cube", 0);
						}
					}
					if (m_bDroppingObject && GetPlayerHeldEntity(m_pPlayer))
					{
						Msg("P Vel: %0.1f\n", m_pPlayer->GetAbsVelocity().Length());
						if (!m_pPlayer->GetAbsVelocity().Length())
						{
							//Fully stopped. Don't drop while coasting, otherwise Inertia (tm)
							CBaseEntity *pUseEntity = GetPlayerHeldEntity(m_pPlayer);
							Msg("Dist between %0.1f %0.1f %0.1f and %0.1f %0.1f %0.1f is %0.1f\n", pUseEntity->GetAbsOrigin().x, pUseEntity->GetAbsOrigin().y, pUseEntity->GetAbsOrigin().z, m_LookGoal.x, m_LookGoal.y, m_LookGoal.z, (pUseEntity->GetAbsOrigin() - m_LookGoal).Length2D());
							if ((pUseEntity->GetAbsOrigin() - m_LookGoal).Length2D() < 25)
							{
								//the cube is above the button
								//PlayerUseDrop("Dropping object.");
								//m_bDroppingObject = false;
								PlayerDuckStart();
								GetNavigator()->StopMoving();
							}
							else
							{
								//Chell is not at the correct distance from the button. Very rare occurance without user influence.
								if ((pUseEntity->GetAbsOrigin() - GetAbsOrigin()).Length2D() > (m_LookGoal - GetAbsOrigin()).Length2D())
								{
									//closer to button than box, back up
									SendCommand("+back;wait 10;-back");
								}
								else
								{
									SendCommand("+forward;wait 10;-forward");
								}
							}
						}
						//else
						//{
						//	m_flWaitTime = gpGlobals->curtime + NPC_CHELL_THINKRATE;
						//}
					}
					if (m_bShootingPortal)
					{
						//Uhhhh once we have the dual portal device we're revisiting this.
						PlayerShootBlue();
						m_bShootingPortal = false;
						//Do we need to do anything to have chell resume on the path?
					}
					if (m_LookGoal == m_vecWaypointEyePos)
					{
						m_bLookedToCurWaypoint = true;
						//m_vecWaypointEyePos.Zero();
					}

					//Removing this apparently fixed a sort of rare bug where the player drifts off the path eventually hitting a wall. I tested it 10 times!
					//m_LookGoal.Zero();
				//}
			}
		}
	}
	else
	{
		//Warning("No look goal\n");
		PlayerStopAiming();
		m_angTargetView = QAngle(0, 0, 0);
	}
}
//
//Find a spot along a path that we should go to
//
void CNPC_Chell::GetWaypointCurChell()
{
	//HACK: Apparently chell is confused by path simplification that happens at the very start of a route? this GetPrev check fixes it.
	//Msg("m_pCurWaypoint == %s, GetCurWaypoint == %s\n", m_pCurWaypoint != NULL ? "TRUE" : "FALSE", GetNavigator()->GetPath()->GetCurWaypoint() != NULL ? "TRUE" : "FALSE");
	if (m_pCurWaypoint && GetNavigator()->GetPath()->GetCurWaypoint())
	{
		//Msg("(%i vs %i) GetPrev %s, distance %0.1f\n", m_pCurWaypoint->iNodeID, GetNavigator()->GetPath()->GetCurWaypoint()->iNodeID,
		//	GetNavigator()->GetPath()->GetCurWaypoint()->GetPrev() != NULL ? "EXISTS" : "NULL",
		//	(m_pCurWaypoint->GetPos() - GetNavigator()->GetPath()->GetCurWaypoint()->GetPos()).Length());
	}
	if (m_pCurWaypoint != NULL && GetNavigator()->GetPath()->GetCurWaypoint() != NULL)
	{
		if (GetNavigator()->GetPath()->GetCurWaypoint()->GetPrev() != NULL &&
			(m_pCurWaypoint->GetPos() - GetNavigator()->GetPath()->GetCurWaypoint()->GetPos()).Length() > 64 ||
			m_pCurWaypoint->iNodeID != GetNavigator()->GetPath()->GetCurWaypoint()->iNodeID)//TODO is portal node checking necessary?
		{
			//The place we thought we were going to does not match the real place. This probably means the path got simplified.
			//This doesn't actually change the path, rather just allows us to react properly to it changing
			Warning("Chell path simplified\n");
			m_bLookedToCurWaypoint = false;
			m_LookGoal.Zero();
			m_vecWaypointEyePos.Zero();
			m_pLastWaypoint = m_pCurWaypoint;
			m_pCurWaypoint = NULL;
		}
	}
	//Currently don't know of a waypoint. should we?
	if (!m_bCrossingPortal && !m_pCurWaypoint && GetNavigator()->GetPath()->GetCurWaypoint())
	{
		//Msg("Chell find waypoint part 1\n");
		if (!GetNavigator()->GetPath()->IsEmpty() && GetNavigator()->GetPath()->GetCurWaypoint()->GetPos().Length())
		{
			//Msg("Chell find waypoint part 3\n");
			m_pCurWaypoint = GetNavigator()->GetPath()->GetCurWaypoint();
			//Don't allow waypoints in the air. This confuses chell!
			/*trace_t tr;
			UTIL_TraceLine(m_pCurWaypoint->GetPos(), m_pCurWaypoint->GetPos() + Vector(0, 0, -1000), MASK_SOLID, m_pPlayer, COLLISION_GROUP_NONE, &tr);
			m_pCurWaypoint->SetPos(tr.endpos);*/
			//NOTE: Apparently GetPathLength() returns 0 if using local nav! This *could* be a problem.
			if (GetNavigator()->GetPath()->GetPathLength() > m_flCurrentPathLength)
			{
				m_flCurrentPathLength = GetNavigator()->GetPath()->GetPathLength();
			}
			m_vecWaypointEyePos = Vector(m_pCurWaypoint->GetPos()[0], m_pCurWaypoint->GetPos()[1], GetLookGoalZ(m_pCurWaypoint->GetPos()[2] + 64));//look ahead, not at ground
			//Msg("Chell find waypoint part 2\n");
			//We don't care about the waypoint if we're very close to it
			//float flLengthToGoal = (Vector(GetNavigator()->GetPath()->GetCurWaypoint()->GetPos()[0],
			//	GetNavigator()->GetPath()->GetCurWaypoint()->GetPos()[1],
			//	GetLookGoalZ(GetNavigator()->GetPath()->GetCurWaypoint()->GetPos()[2] + 64)) - m_pPlayer->EyePosition()).Length2D();
			Vector vecWPPos = GetNavigator()->GetPath()->GetCurWaypoint()->GetPos();
			Vector origin = GetAbsOrigin();
			float flLengthToGoal = (vecWPPos - origin).Length2D();
			//Msg("LENGTH TO GOAL: %0.1f\n", flLengthToGoal);
			if (!m_bPickingUpObject && flLengthToGoal > NPC_CHELL_NAV_ARRIVE_TOLERANCE)//TODO-2: Change to GetArriveTolerance()?
			{
				//Not at our waypoint yet
				if (GetNavigator()->GetPath()->GetCurWaypoint())
				{
					//Msg("Cur to Start dist: %0.1f\n", (GetNavigator()->GetPath()->GetCurWaypoint()->GetPos() - m_vecWalkingStartPos).Length2D());
					//TODO: This should be -1, not 0...
					if (GetNavigator()->GetPath()->GetCurWaypoint()->iNodeID <= 0 && (GetNavigator()->GetPath()->GetCurWaypoint()->GetPos() - m_vecWalkingStartPos).Length2D() < NPC_CHELL_NAV_ARRIVE_TOLERANCE)
					{
						//Should this be normal or final goal?
						ChellNavToEntity(STRING(m_ChellGoal->GetEntityName()), "Hacky advance!\n", 0);
						//GetNavigator()->AdvancePath();//HACK: This waypoint is in an invalid place, skip it!
						//m_LookGoal.Zero();
						//m_vecWaypointEyePos.Zero();
						//m_pCurWaypoint = NULL;
						//m_pLastWaypoint = NULL;
						return;
					}
				}
				//
			}
			else
			{
				//We are at a waypoint. Look for special waypoint cases here.
				//Check for the rare scenario where we are directly above or below the goal but on a different "story" of the building
				trace_t tr;
				UTIL_TraceLine(GetAbsOrigin(), m_ChellGoal->GetAbsOrigin(), MASK_SOLID, m_pPlayer, COLLISION_GROUP_NONE, &tr);
				if ((GetAbsOrigin() - m_ChellGoal->GetAbsOrigin()).Length2D() < GetArriveTolerance() && (tr.fraction != 0 || tr.m_pEnt == m_ChellGoal))
				{
					//we are at the goal.
					//Warning("ARRIVED AT GOAL ENTITY (%0.1f %0.1f %0.1f)\n", GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z);
					m_flStopReverseTime = 0;
					m_bWalkReverse = false;
					m_flCurrentPathLength = 0;
					GetNavigator()->OnNavComplete();
					//Drop a cube onto a button
					CBaseEntity *goal = m_ChellGoal;
					CBaseEntity *pBoxTrigger = gEntList.FindEntityByName(NULL, STRING(goal->m_target));
					if (pBoxTrigger)
						Msg("m_pHeldObject %s dist %0.1f m_flCurrentPathLength %0.1f\n", GetPlayerHeldEntity(m_pPlayer) ? "TRUE" : "FALSE", GetPlayerHeldEntity(m_pPlayer) ? (GetPlayerHeldEntity(m_pPlayer)->GetAbsOrigin() - pBoxTrigger->GetAbsOrigin()).Length2D() : 1234, m_flCurrentPathLength);
					if (GetPlayerHeldEntity(m_pPlayer) && pBoxTrigger && GetPlayerHeldEntity(m_pPlayer) != pBoxTrigger && m_flCurrentPathLength == 0)
					{
						m_bDroppingObject = true;
						m_iForceCubeOut = DONT_WIGGLE;
						GetNavigator()->StopMoving(true);
						PlayerStopMoving();
						//Look to the button trigger
						Vector vecLookToButton = pBoxTrigger->GetAbsOrigin();
						//vecLookToButton.z = m_pPlayer->EyePosition().z;
						ChellLookAtPos(vecLookToButton, "going to drop cube");
					}
					//Msg("m_bInCycle: %s, %s and %s\n", m_bInCycle ? "YES" : "NO", m_strChellGoal, STRING(m_ChellGoal->GetEntityName()));
					//One of the cycle portals should be able to reach the exit, check for blockers between that cycle portal and the elevator.
					if (m_bInCycle && strcmp(m_strChellGoal, STRING(m_ChellGoal->GetEntityName() ) ) )
					{
						for (int i = 0; i < m_pCyclePortals.Count(); i++)
						{
							EHANDLE pOriginalGoal = gEntList.FindEntityByName(NULL, m_strChellGoal);
							CAI_Node *pCyclePortalNode = NearestNodeToPoint(m_pCyclePortals[i]->GetAbsOrigin(), true, false);
							AI_Waypoint_t *pCycleWaypoint = GetPathfinder()->BuildRoute(pCyclePortalNode->GetPosition(GetHullType()), pOriginalGoal->GetAbsOrigin(), pOriginalGoal, NPC_CHELL_NAV_ARRIVE_TOLERANCE, true, GetNavType());
							if (pCycleWaypoint != NULL)
							{
								//Msg("-Can go from %0.1f %0.1f %0.1f (%i) to %0.1f %0.1f %0.1f\n",
								//	m_pCyclePortals[i]->GetAbsOrigin().x, m_pCyclePortals[i]->GetAbsOrigin().y, m_pCyclePortals[i]->GetAbsOrigin().z,
								//	iCyclePortalNodeID, pOriginalGoal->GetAbsOrigin().x, pOriginalGoal->GetAbsOrigin().y, pOriginalGoal->GetAbsOrigin().z);
								ScanAheadForBlockers(pCycleWaypoint);
							}
							else
							{
								//Msg("CANNOT go from %0.1f %0.1f %0.1f (%i) to %0.1f %0.1f %0.1f\n",
								//	m_pCyclePortals[i]->GetAbsOrigin().x, m_pCyclePortals[i]->GetAbsOrigin().y, m_pCyclePortals[i]->GetAbsOrigin().z,
								//	iCyclePortalNodeID, pOriginalGoal->GetAbsOrigin().x, pOriginalGoal->GetAbsOrigin().y, pOriginalGoal->GetAbsOrigin().z);
							}
						}
						//Msg("(A) InCycle set to false, was %s\n", m_bInCycle ? "true" : "false");
						//m_bInCycle = false;//why?
					}
				}
				else// if (GetNavigator()->GetPath()->GetCurWaypoint()->GetNext()->iNodeID != m_pPortal1Node->GetId() && GetNavigator()->GetPath()->GetCurWaypoint()->GetNext()->iNodeID != m_pPortal2Node->GetId())
				{
					//regular waypoint. TODO-2: pretty sure there's no reason to have another "hit waypoint" check in MaintainPath()...
					//Msg("Chell advancing from waypoint (%i)\n", GetNavigator()->GetPath()->GetCurWaypoint()->iNodeID);
					//if (GetNavigator()->GetPath()->GetCurWaypoint()->iNodeID == 0)
					//	Msg("d");
					GetNavigator()->AdvancePath();
					if (!m_bShootingPortal)
						m_LookGoal.Zero();
					m_vecWaypointEyePos.Zero();
					m_pCurWaypoint = NULL;
					m_pLastWaypoint = NULL;
				}
			}
		}
	}
}

//
//Go on a path!
//
void CNPC_Chell::MaintainPath()
{
	//has to go somewhere...
	if (m_flLastCyclePortalTime < gpGlobals->curtime)
	{
		m_flLastCyclePortalTime += m_flCycleTime;
	}

	//Msg("!GetNavigator()->GetPath()->IsEmpty() = %s\n", !GetNavigator()->GetPath()->IsEmpty() ? "TRUE" : "FALSE");
	//Msg("m_pCurWaypoint = %s\n", m_pCurWaypoint ? "TRUE" : "FALSE");
	//Msg("m_pCurWaypoint->GetPos().Length() = %s\n", (m_pCurWaypoint && m_pCurWaypoint->GetPos().Length()) ? "TRUE" : "FALSE");
	if (!GetNavigator()->GetPath()->IsEmpty() && m_pCurWaypoint && m_pCurWaypoint->GetPos().Length())
	{
		
		//Msg("Length achieved\n");
		//Msg("Chell looking to waypoint: %0.1f, %0.1f, %0.1f ID: %i\n", m_vecWaypointEyePos[0], m_vecWaypointEyePos[1], m_vecWaypointEyePos[2], m_pCurWaypoint->iNodeID);
		if (!m_bLookedToCurWaypoint)
		{
			GetNavigator()->GetPath()->SetGoalTolerance(NPC_CHELL_NAV_ARRIVE_TOLERANCE);
			GetNavigator()->GetPath()->SetWaypointTolerance(NPC_CHELL_NAV_ARRIVE_TOLERANCE);
			if (!m_bPickingUpObject && !m_pCurWaypoint->GetShot() && !m_bShootingPortal)
			{
				ChellLookAtPos(m_vecWaypointEyePos, "looking at waypoint");
			}
			else
			{
				//Warning("Colliding, looking to goal not waypoint\n");
			}
		}
		if (m_pCurWaypoint->GetShot() && !m_bShootingPortal)
		{
			ExecuteShot(m_pCurWaypoint->GetShot());
		}
		if (m_vecWaypointEyePos.Length())
		{
			//Msg("DISTANCE TO WAYPOINT C: %0.1f\n", (m_vecWaypointEyePos - m_pPlayer->EyePosition()).Length());
		}
		if (m_vecWaypointEyePos.Length() && (m_vecWaypointEyePos - m_pPlayer->EyePosition()).Length2D() < GetArriveTolerance())
		{
			//Warning("Hit waypoint\n");
			//Stop trying to look at it, even if we didn't succeed in looking at it, and stop trying to go to it
			m_bLookedToCurWaypoint = false;
			if (!m_bPickingUpObject && !m_bShootingPortal)
			{
				m_LookGoal.Zero();
			}
			m_vecWaypointEyePos.Zero();
			PlayerStopAiming();
			//if (m_pCurWaypoint->GetNext() == NULL)
			//{
			PlayerStopMoving();
			//}
			/*if (m_pCurWaypoint && m_pPortal1Node && m_pPortal2Node && (m_pCurWaypoint->iNodeID == m_pPortal1Node->GetId() || m_pCurWaypoint->iNodeID == m_pPortal2Node->GetId()))
			{
				//___ - direct path (bad)

				//1__ - doubled back
				//_1_ - doubled back
				//__1 - doubled back

				//_11 - doubled back
				//1_1 - doubled back
				//11_ - direct path (bad)

				//111 - doubled back
				m_pCurWaypoint->ModifyFlags(bits_WP_DONT_SIMPLIFY, true);
				if (m_pCurWaypoint->GetPrev())
					m_pCurWaypoint->GetPrev()->ModifyFlags(bits_WP_DONT_SIMPLIFY, true);
				if (m_pCurWaypoint->GetNext())
					m_pCurWaypoint->GetNext()->ModifyFlags(bits_WP_DONT_SIMPLIFY, true);
			}*/
			//MyNearestNode(true)
			//GetWPNode(m_pCurWaypoint)
			if (CheckCrossFuture(MyNearestNode(true), m_pCurWaypoint, false))
			{
				return;
			}

			
			//m_pCurWaypoint = NULL; is important, without this we will never go to a new waypoint.
			if (m_pLastWaypoint != m_pCurWaypoint)
			{
				//Warning("That check\n");
				//GetNavigator()->AdvancePath();
				//m_vecWaypointEyePos.Zero();
				m_pCurWaypoint = NULL;
				//m_pLastWaypoint = NULL;
			}
		}
		UpdateMoveDirToWayPoint(m_vecWaypointEyePos);
	}
	else
	{
		//Warning("Not Walking\n");
		if (m_bPickingUpObject || (m_pCurWaypoint && m_pCurWaypoint->GetNext() == NULL))
		{
			//Msg("Stopping cause picking up object or at end of path\n");
			PlayerStopMoving();
		}
	}
	if (m_bCrossingPortal)
	{
		CrossPortal();
	}
	//HACK: (?) I don't know how this situation can exist, but it is happening in test 02!
	if (GetNavigator()->GetPath()->IsEmpty() && m_pCurWaypoint)
	{
		Warning("Hacky check\n");
		m_flCurrentPathLength = 0;
		m_vecWaypointEyePos.Zero();
		m_pCurWaypoint = NULL;
		m_pLastWaypoint = NULL;
	}
}

/*bool CNPC_Chell::CheckCross(CAI_Node *pNode1, CAI_Node *pNode2, bool bTest)
{
	if (!pNode1)
	{
		Msg("No node 1\n");
		return false;
	}
	if (!pNode2)
	{
		Msg("No node 2\n");
		return false;
	}
	//This is where we find out if we need to cross through a portal
	ConColorMsg(Color(255, 128, bTest ? 70 : 0, 255), "CROSSPORTALS: %i == %i || %i == %i\n", pNode1->GetId(), m_pPortal1Node ? m_pPortal1Node->GetId() : 1234, pNode1->GetId(), m_pPortal2Node ? m_pPortal2Node->GetId() : 1234);
	if (m_pPortal1Node && m_pPortal2Node && (pNode1->GetId() == m_pPortal1Node->GetId() || pNode1->GetId() == m_pPortal2Node->GetId()))
	{
		//also check if the waypoint after is a portal node waypoint.
		bool bCrossingPortal1 = pNode1->GetId() == m_pPortal1Node->GetId();
		ConColorMsg(Color(255, 128, bTest ? 70 : 0, 255), "CROSSPORTALS 2: %i == %i || %i == %i\n", pNode2->GetId(), m_pPortal1Node ? m_pPortal1Node->GetId() : 1234, pNode2->GetId(), m_pPortal2Node ? m_pPortal2Node->GetId() : 1234);
		if ((!bCrossingPortal1 && pNode2->GetId() == m_pPortal1Node->GetId()) || (bCrossingPortal1 && pNode2->GetId() == m_pPortal2Node->GetId()))
		{
			if (!bTest)
			{
				CrossPortal();
			}
			return true;
		}
	}
	return false;
}*/

bool CNPC_Chell::CheckCrossFuture(CAI_Node *pNode1, AI_Waypoint_t *pWaypoint, bool bTest)
{
	if (!pNode1 || !pWaypoint)
		return false;
	//ConColorMsg(Color(255, 128, bTest ? 128 : 0, 255), "bTest: %s\n", bTest ? "TRUE" : "FALSE");
	//This is where we find out if we need to cross through a portal
	//ConColorMsg(Color(255, 128, bTest ? 128 : 0, 255), "fCROSSPORTALS: %i == %i || %i == %i\n", pNode1->GetId(), m_pPortal1Node ? m_pPortal1Node->GetId() : 1234, pNode1->GetId(), m_pPortal2Node ? m_pPortal2Node->GetId() : 1234);
	if (m_pPortal1Node && m_pPortal2Node && (pNode1->GetId() == m_pPortal1Node->GetId() || pNode1->GetId() == m_pPortal2Node->GetId()))
	{
		while (pWaypoint)
		{
			//Get the id of the node nearest to the waypoint, sometimes the waypoint itself will not store an actual node's ID, which is normal.
			int wpID = pWaypoint->iNodeID;
			if (wpID == -1)
			{
				wpID = NearestNodeToPoint(pWaypoint->GetPos(), true, false)->GetId();
			}
			//Msg("waypoint is at %0.1f %0.1f %0.1f, id %i\n", pWaypoint->GetPos().x, pWaypoint->GetPos().x, pWaypoint->GetPos().z, wpID);
			bool bCrossingPortal1 = pNode1->GetId() == m_pPortal1Node->GetId();

			//update our timer for how long we have until the portal closes
			//CBaseEntity *pPortalBeingCrossed = bCrossingPortal1 ? m_hPortal1 : m_hPortal2;

			//also check if any future waypoint is on the other portal node.
			//if we check only the next waypoint, we may do some screwey double-back stuff which is likely caused by forcing the portal waypoints to not simplify.
			//ConColorMsg(Color(255, 128, bTest ? 128 : 0, 255), "fCROSSPORTALS 2: %i == %i || %i == %i\n", wpID, m_pPortal1Node ? m_pPortal1Node->GetId() : 1234, wpID, m_pPortal2Node ? m_pPortal2Node->GetId() : 1234);
			if ((!bCrossingPortal1 && wpID == m_pPortal1Node->GetId()) || (bCrossingPortal1 && wpID == m_pPortal2Node->GetId()))
			{
				if (!bTest)
				{
					CrossPortal();
				}
				return true;
			}
			pWaypoint = pWaypoint->GetNext();
		}
	}
	return false;
}

//
//Check if at this point in our path we should go through a portal
//
/*bool CNPC_Chell::CrossPortalCheck(AI_Waypoint_t *pWaypoint, bool bTest)
{
	//This is where we find out if we need to cross through a portal
	ConColorMsg(Color(255, 128, bTest ? 70 : 0, 255), "CROSSPORTALS: %i == %i || %i == %i\n", pWaypoint->iNodeID, m_pPortal1Node ? m_pPortal1Node->GetId() : 1234, pWaypoint->iNodeID, m_pPortal2Node ? m_pPortal2Node->GetId() : 1234);
	if (m_pPortal1Node && m_pPortal2Node && pWaypoint->GetNext() && (pWaypoint->iNodeID == m_pPortal1Node->GetId() || pWaypoint->iNodeID == m_pPortal2Node->GetId()))
	{
		//also check if the waypoint after is a portal node waypoint.
		ConColorMsg(Color(255, 128, bTest ? 70 : 0, 255), "CROSSPORTALS 2: %i == %i || %i == %i\n", pWaypoint->GetNext()->iNodeID, m_pPortal1Node ? m_pPortal1Node->GetId() : 1234, pWaypoint->GetNext()->iNodeID, m_pPortal2Node ? m_pPortal2Node->GetId() : 1234);
		if (((pWaypoint->GetNext()->iNodeID == m_pPortal1Node->GetId() || pWaypoint->GetNext()->iNodeID == m_pPortal2Node->GetId())))
		{
			if (!bTest)
			{
				CrossPortal();
			}
			return true;
		}
	}
	return false;
}*/

//
//Check if we're meant to go through a portal right at the beginning of a path.
//HACK: I am 95% sure this should not be necessary and might be an indication that our MaintainPath() code for avoiding simplification is bugged.
//
/*bool CNPC_Chell::CrossPortalCheckInitial(AI_Waypoint_t *pFirstWaypoint)
{
	//This is where we find out if we need to cross through a portal
	ConColorMsg(Color(0, 128, 255, 255), "iCROSSPORTALS: %i == %i || %i == %i\n", MyNearestNode(true)->GetId(), m_pPortal1Node ? m_pPortal1Node->GetId() : 1234, MyNearestNode(true)->GetId(), m_pPortal2Node ? m_pPortal2Node->GetId() : 1234);
	if (m_pPortal1Node && m_pPortal2Node && pFirstWaypoint->GetNext() && (MyNearestNode(true)->GetId() == m_pPortal1Node->GetId() || MyNearestNode(true)->GetId() == m_pPortal2Node->GetId()))
	{
		//also check if the waypoint after is a portal node waypoint. (0B)
		ConColorMsg(Color(0, 128, 255, 255), "iCROSSPORTALS 2: %i == %i || %i == %i\n", pFirstWaypoint->iNodeID, m_pPortal1Node ? m_pPortal1Node->GetId() : 1234, pFirstWaypoint->iNodeID, m_pPortal2Node ? m_pPortal2Node->GetId() : 1234);
		if (((pFirstWaypoint->iNodeID == m_pPortal1Node->GetId() || pFirstWaypoint->iNodeID == m_pPortal2Node->GetId())))
		{
			CrossPortal();
			return true;
		}
	}
	return false;
}*/

//
//tell chell to cross through a portal
//
void CNPC_Chell::CrossPortal()
{
	m_bCrossingPortal = true;
	if (GetPlayerHeldEntity(m_pPlayer))
	{
		//sometimes a cube will be through the portal already before we get here. if we start going in reverse now, we will lose the cube!
		trace_t tr;
		UTIL_TraceLine(GetAbsOrigin(), GetPlayerHeldEntity(m_pPlayer)->GetAbsOrigin(), MASK_SOLID, GetPlayerHeldEntity(m_pPlayer), COLLISION_GROUP_NONE, &tr);
		if ((GetAbsOrigin() - GetPlayerHeldEntity(m_pPlayer)->GetAbsOrigin()).Length() < 100)
		{
			m_bWalkReverse = true;
			m_flStopReverseTime = gpGlobals->curtime + 0.05;
		}
		else
		{
			//m_bWalkReverse = false;
		}
	}
	bool bCrossingPortal1 = false;
	if (m_pCurWaypoint && m_pCurWaypoint->iNodeID == m_pPortal1Node->GetId())
	{
		Msg("m_pCurWaypoint->iNodeID (%i) == m_pPortal1Node->GetId() (%i)\n", m_pCurWaypoint->iNodeID, m_pPortal1Node->GetId());
	}
	if (MyNearestNode(true)->GetId() == m_pPortal1Node->GetId())
	{
		Msg("MyNearestNode(true)->GetId() (%i) == m_pPortal1Node->GetId() (%i)\n", MyNearestNode(true)->GetId(), m_pPortal1Node->GetId());
	}
	if ((m_pCurWaypoint && m_pCurWaypoint->iNodeID == m_pPortal1Node->GetId()) || (MyNearestNode(true)->GetId() == m_pPortal1Node->GetId()))
		bCrossingPortal1 = true;


	ConColorMsg(Color(255, 128, 0, 255), "TIME TO CROSS PORTALS FROM %s TO %s\n", bCrossingPortal1 ? "BLUE" : "ORANGE", !bCrossingPortal1 ? "BLUE" : "ORANGE");
	//direct player to the portal
	m_vecWaypointEyePos = Vector((bCrossingPortal1 ? m_hPortal1 : m_hPortal2)->GetAbsOrigin()[0], (bCrossingPortal1 ? m_hPortal1 : m_hPortal2)->GetAbsOrigin()[1], m_pPlayer->EyePosition()[2]);
	ChellLookAtPos(m_vecWaypointEyePos, "going through portal");
	UpdateMoveDirToWayPoint(m_vecWaypointEyePos);

	//HACK: Force clear any waypoints close to us. If not, we may try to go back to the portal we entered through.
	if (m_pCurWaypoint)
	{
		AI_Waypoint_t *waypoint = m_pCurWaypoint;
		while (waypoint && (waypoint->GetPos() - GetAbsOrigin()).Length2D() < 32)
		{
			Msg("ef\n");
			GetNavigator()->AdvancePath();
			waypoint = GetNavigator()->GetPath()->GetCurWaypoint();
			m_pCurWaypoint = NULL;
		}
	}
}

//
//Find a prop_portal with no inputs being sent to it that would cause it be removed. yucky.
//
CBaseEntity *CNPC_Chell::FindStaticPortal()
{
	//CBaseEntity *pGoodPortal = NULL;
	CBaseEntity *inputSource = gEntList.NextEnt(NULL);
	while (inputSource)
	{
		datamap_t *dmap = inputSource->GetDataDescMap();
		bool found = false;
		while (dmap && !found)
		{
			int fields = dmap->dataNumFields;
			for (int i = 0; i < fields; i++)
			{
				typedescription_t *dataDesc = &dmap->dataDesc[i];
				if ((dataDesc->fieldType == FIELD_CUSTOM) && (dataDesc->flags & FTYPEDESC_OUTPUT))
				{
					CBaseEntityOutput *pOutput = (CBaseEntityOutput *)((int)inputSource + (int)dataDesc->fieldOffset[0]);
					CEventAction *entEvent = pOutput->m_ActionList;
					while (entEvent != NULL)
					{
						//Msg("%s: %s %s %s %s\n", inputSource->GetClassname(), dataDesc->externalName, STRING(entEvent->m_iTarget), STRING(entEvent->m_iTargetInput), STRING(entEvent->m_iParameter));
						if ((!strcmp(STRING(entEvent->m_iParameter), "0") && !strcmp(STRING(entEvent->m_iTargetInput), "SetActivatedState")) || !strcmp(STRING(entEvent->m_iTargetInput), "Fizzle"))//do not deactivate or fizzle me
						{
							CBaseEntity *pPortalEnt = gEntList.FindEntityByClassname(NULL, "prop_portal");
							while (pPortalEnt != NULL)
							{
								//do NOT match
								if (strcmp(STRING(entEvent->m_iTarget), STRING(pPortalEnt->GetEntityName())))
								{
									//Avoid picking up the portals from test 00. Check if we can walk to the static portal or any cycle portal it's linked to. I think most scenarios where this would give a false negative are pretty contrived.
									chellNavQuery_t resToStatic;
									chellNavQuery_t resToLinked;
									CProp_Portal *pStaticPortal = static_cast<CProp_Portal*>(pPortalEnt);
									//Using m_hLinkedPortal here is not really smart, but it does work. In reality it could return NULL but still be the correct portal. Also, a static portal will have multiple cycle portals linked to it. The real way to find cycle portals is by checking link ids.
									CProp_Portal *pLinkedPortal = pStaticPortal->m_hLinkedPortal.Get();
									//Msg("Testing path to candidate static portal at %0.1f %0.1f %0.1f\n", pStaticPortal->GetAbsOrigin().x, pStaticPortal->GetAbsOrigin().y, pStaticPortal->GetAbsOrigin().z);
									resToStatic = ComplexNavTest(GetAbsOrigin(), pStaticPortal->GetAbsOrigin(), QF_DONT_DETECT_CYCLE | QF_NO_PORTAL_TRAVEL, pStaticPortal);
									if (pLinkedPortal)
									{
										resToLinked = ComplexNavTest(GetAbsOrigin(), pLinkedPortal->GetAbsOrigin(), QF_DONT_DETECT_CYCLE | QF_NO_PORTAL_TRAVEL, pLinkedPortal);
										if (resToLinked.result != CNQR_NO)
										{
											return pPortalEnt;
										}
									}
									if (resToStatic.result != CNQR_NO)
									{
										return pPortalEnt;
									}
								}
								pPortalEnt = gEntList.FindEntityByClassname(pPortalEnt, "prop_portal");
							}
						}
						entEvent = entEvent->m_pNext;
					}
				}
			}
			dmap = dmap->baseMap;
		}
		inputSource = gEntList.NextEnt(inputSource);
	}
	return NULL;
}

//
//Check if we're at test 01. (Or hypothetically any test chamber like it.)
//
bool CNPC_Chell::DetectCycle()
{
	Msg("DetectCycle running. This message should not repeat periodically!\n");
	if (gEntList.FindEntityByName(NULL, "chelldest_cyclewaitingplace"))
		return true;

	bool bDone = false;
	//m_pCyclePortals.RemoveAll();
	CBaseEntity *pPortalEnt = FindStaticPortal();
	CProp_Portal *pPortal = static_cast<CProp_Portal*>(pPortalEnt);
	if (pPortal)// && pPortal->IsPortal2())
	{
		//Msg("DetectCycle portal at %0.1f %0.1f %0.1f\n", pPortalEnt->GetAbsOrigin().x, pPortalEnt->GetAbsOrigin().y, pPortalEnt->GetAbsOrigin().z);
		CBaseEntity *pTimerEnt = gEntList.FindEntityByClassname(NULL, "logic_timer");
		while (pTimerEnt)
		{
			//Msg("new logic_timer '%s'\n", STRING(pTimerEnt->GetEntityName()));
			CTimerEntity *pTimer = static_cast<CTimerEntity*>(pTimerEnt);
			CEventAction *timerEvent = pTimer->m_OnTimer.m_ActionList;
			while (timerEvent != NULL)
			{
				const char *strTimerTargetName = STRING(timerEvent->m_iTarget);
				CBaseEntity *pCounterEnt = gEntList.FindEntityByName(NULL, strTimerTargetName);
				//Msg("logic_timer targets %s input %s param %s\n", strTimerTargetName, STRING(timerEvent->m_iTargetInput), STRING(timerEvent->m_iParameter));
				if (pCounterEnt != NULL && !strcmp(pCounterEnt->GetClassname(), "math_counter") && !strcmp(STRING(timerEvent->m_iTargetInput), "Add") && !strcmp(STRING(timerEvent->m_iParameter), "1"))
				{
					//the timer increments a math_counter
					CMathCounter *pCounter = static_cast<CMathCounter*>(pCounterEnt);
					CEventAction *counterEvent = pCounter->m_OutValue.m_ActionList;
					while (counterEvent != NULL)
					{
						const char *strCounterTargetName = STRING(counterEvent->m_iTarget);
						CBaseEntity *pCaseEnt = gEntList.FindEntityByName(NULL, strCounterTargetName);
						//Msg("math_counter targets %s %s input %s\n", pCaseEnt->GetClassname(), strCounterTargetName, STRING(counterEvent->m_iTargetInput));
						if (pCaseEnt != NULL && !strcmp(pCaseEnt->GetClassname(), "logic_case") && !strcmp(STRING(counterEvent->m_iTargetInput), "InValue"))
						{
							//the counter sends its value to a case
							CLogicCase *pCase = static_cast<CLogicCase*>(pCaseEnt);
							CEventAction *caseEvent;
							for (int i = 0; i < MAX_LOGIC_CASES; i++)//for every OnCase output
							{
								//Msg("Checking OnCase #%i\n", i);
								caseEvent = pCase->m_OnCase[i].m_ActionList;
								while (caseEvent != NULL)//for every action of an output
								{
									const char *strCaseTargetName = STRING(caseEvent->m_iTarget);//currently assuming the input being sent is Trigger
									CBaseEntity *pRelayEnt = gEntList.FindEntityByName(NULL, strCaseTargetName);
									//the logic_case triggers logic_relays
									//it would be fairly unreasonable for multiple logic_relays to have the same name, so I won't worry about it
									//Msg("logic_case targets %s %s with input %s\n", pRelayEnt->GetClassname(), strCaseTargetName, caseEvent->m_iTargetInput);
									if (pRelayEnt != NULL && !strcmp(pRelayEnt->GetClassname(), "logic_relay") && !strcmp(STRING(caseEvent->m_iTargetInput), "Trigger"))
									{
										CLogicRelay *pRelay = static_cast<CLogicRelay*>(pRelayEnt);
										CEventAction *relayEvent = pRelay->m_OnTrigger.m_ActionList;
										while (relayEvent != NULL)
										{
											//Msg("logic_relay sends input %s param %s\n", relayEvent->m_iTargetInput, relayEvent->m_iParameter);
											if (!strcmp(STRING(relayEvent->m_iTargetInput), "SetActivatedState") && !strcmp(STRING(relayEvent->m_iParameter), "1"))
											{
												//remember every portal we find here
												const char *strRelayTargetName = STRING(relayEvent->m_iTarget);
												CBaseEntity *pCyclePortalEnt = gEntList.FindEntityByName(NULL, strRelayTargetName);
												if (pCyclePortalEnt != NULL)
												{
													CProp_Portal *pCyclePortal = static_cast<CProp_Portal*>(pCyclePortalEnt);
													if ((pCyclePortal->GetLinkageGroup() == pPortal->GetLinkageGroup()) && (pCyclePortal->IsPortal2() != pPortal->IsPortal2()))
													{
														Msg("accepted cycle portal at %0.1f %0.1f %0.1f\n", pCyclePortalEnt->GetAbsOrigin().x, pCyclePortalEnt->GetAbsOrigin().y, pCyclePortalEnt->GetAbsOrigin().z);
														m_flCycleTime = pTimer->m_flRefireTime;
														m_flLastCyclePortalTime = gpGlobals->curtime + m_flCycleTime;
														m_pCyclePortals.AddToTail(pCyclePortalEnt);
														m_pStaticPortal = pPortalEnt;
														bDone = true;
													}
												}
											}
											relayEvent = relayEvent->m_pNext;
										}
									}
									caseEvent = caseEvent->m_pNext;
								}
							}
						}
						if (bDone)
							break;
						counterEvent = counterEvent->m_pNext;
					}
				}
				if (bDone)
					break;
				timerEvent = timerEvent->m_pNext;
			}
			if (bDone)
				break;
			pTimerEnt = gEntList.FindEntityByClassname(pTimerEnt, "logic_timer");
		}
	}
	else
	{
		//Warning("DetectCycle portal at %0.1f %0.1f %0.1f was bad?\n", pPortalEnt->GetAbsOrigin().x, pPortalEnt->GetAbsOrigin().y, pPortalEnt->GetAbsOrigin().z);
	}
	if (m_pCyclePortals.Count() > 1)
	{
		return true;
	}
	else
	{
		m_pStaticPortal = NULL;
		m_pCyclePortals.RemoveAll();
		return false;
	}
}

//
//detect the cycle room in chamber 01 and go into the trigger that intitiates it
//
/*void CNPC_Chell::BeginCycle()
{
	//A trigger_once starts everything in chamber 01
	CBaseEntity *pTriggerEnt = gEntList.FindEntityByClassname(NULL, "trigger_once");
	//Msg("Cycle marker 1\n");
	if (!m_bInCycle)
	{
		if (DetectCycle(pTriggerEnt))
		{
			if (gEntList.FindEntityByName(NULL, "chelldest_cyclewaitingplace") == NULL)
			{
				AI_Waypoint_t *pWPToTrigger = GetPathfinder()->BuildRoute(GetAbsOrigin(), pTriggerEnt->GetAbsOrigin(), pTriggerEnt, NPC_CHELL_NAV_ARRIVE_TOLERANCE, true, GetNavType());
				if (pWPToTrigger != NULL && pWPToTrigger->GetNext() != NULL)//We shouldn't be checking for next here but????? somehow this fixes trying to go to the trigger when in chamber 00
				{
					//Make chell go into the starting area for the portal cycle
					CBaseEntity *pTarget = CreateEntityByName("info_target");
					pTarget->SetAbsOrigin(pTriggerEnt->GetAbsOrigin());
					pTarget->SetName(MAKE_STRING("chelldest_cyclewaitingplace"));
					//const char *infotargetname = pTarget->GetEntityName().ToCStr();
					ChellNavToEntity("chelldest_cyclewaitingplace");
					//Msg("InCycle set to true, was %s\n", m_bInCycle ? "true" : "false");
					m_bInCycle = true;
				}
			}
		}
	}
}*/

//
//If there's two usable portals, we want to remember that as they can help us navigate
//Sometimes only one portal will be open!
//
void CNPC_Chell::UpdatePortalNodeLinks()//
{
	CBaseEntity *pPortalEnt = gEntList.FindEntityByClassname(NULL, "prop_portal");
	bool bFoundPortal1 = false;
	bool bFoundPortal2 = false;
	//bool bNewPortal1 = false;
	//bool bNewPortal2 = false;
	
	//EHANDLE hOldPortal1 = m_hPortal1;
	//EHANDLE hOldPortal2 = m_hPortal2;
	CAI_Node *pOldPortal1Node = m_pPortal1Node;
	CAI_Node *pOldPortal2Node = m_pPortal2Node;

	while (pPortalEnt)
	{
		//Msg("UpdatePortalNodeLinks loop\n");
		CProp_Portal *pPortal = dynamic_cast<CProp_Portal*>(pPortalEnt);
		if (pPortal && pPortal->m_bActivated)
		{
			if (!pPortal->IsPortal2())
			{
				//Msg("Found blue portal\n");
				m_hPortal1 = pPortal;
				bFoundPortal1 = true;
				//This check worked on the assumption that the new portal was a completely different portal like in chamber 1, not the same one being teleported like how the gun does it
				//if (m_hPortal1.ToInt() && hOldPortal1.ToInt() && m_hPortal1.ToInt() != hOldPortal1.ToInt())
				//{
					//new portal, get the new node
				m_pPortal1Node = NearestNodeToPoint(m_hPortal1->GetAbsOrigin(), true, false);
					//Warning("Blue portal node: %i\n", m_pPortal1Node->GetId());
					if (m_pPortal1Node != pOldPortal1Node)//portal has changed place (significantly) or just been found
					{
						m_bCrossingPortal = false;//if we were crossing through a portal, the portal has now moved, so don't try to cross anymore.
						//Warning("Blue portal moved\n");
						//bNewPortal1 = true;
						if (pOldPortal1Node && pOldPortal2Node && pOldPortal1Node->HasLink(pOldPortal2Node->m_iID))//get rid of old connection
						{
							//Warning("Breaking old AI nav link between %0.1f %0.1f %0.1f and %0.1f %0.1f %0.1f\n",
							//	pOldPortal1Node->m_vOrigin[0], pOldPortal1Node->m_vOrigin[1], pOldPortal1Node->m_vOrigin[2],
							//	pOldPortal2Node->m_vOrigin[0], pOldPortal2Node->m_vOrigin[1], pOldPortal2Node->m_vOrigin[2]);
							pOldPortal1Node->HasLink(pOldPortal2Node->m_iID)->m_LinkInfo |= bits_LINK_OFF;
							pOldPortal1Node->HasLink(pOldPortal2Node->m_iID)->m_LinkInfo &= ~bits_LINK_PORTAL;
						}
					}
				//}
			}
			else
			{
				//Msg("Found orange portal\n");
				m_hPortal2 = pPortal;
				bFoundPortal2 = true;
				//if (m_hPortal2.ToInt() && hOldPortal2.ToInt() && m_hPortal2.ToInt() != hOldPortal2.ToInt())
				//{
					//new portal, get the new node
				m_pPortal2Node = NearestNodeToPoint(m_hPortal2->GetAbsOrigin(), true, false);
					if (m_pPortal2Node != pOldPortal2Node)//portal has changed place (significantly) or just been found
					{
						m_bCrossingPortal = false;//if we were crossing through a portal, the portal has now moved, so don't try to cross anymore.
						//Warning("Orange portal moved\n");
						//bNewPortal2 = true;
						if (pOldPortal1Node && pOldPortal2Node && pOldPortal1Node->HasLink(pOldPortal2Node->m_iID))//get rid of old connection
						{
							//Warning("Breaking old AI nav link between %0.1f %0.1f %0.1f and %0.1f %0.1f %0.1f\n",
							//	pOldPortal1Node->m_vOrigin[0], pOldPortal1Node->m_vOrigin[1], pOldPortal1Node->m_vOrigin[2],
							//	pOldPortal2Node->m_vOrigin[0], pOldPortal2Node->m_vOrigin[1], pOldPortal2Node->m_vOrigin[2]);
							pOldPortal1Node->HasLink(pOldPortal2Node->m_iID)->m_LinkInfo |= bits_LINK_OFF;
							pOldPortal1Node->HasLink(pOldPortal2Node->m_iID)->m_LinkInfo &= ~bits_LINK_PORTAL;
						}
					}
				//}
			}
		}
		//else
		//{
			//Msg("Portals not found yet\n");
			pPortalEnt = gEntList.FindEntityByClassname(pPortalEnt, "prop_portal");
		//}
	}
	if (bFoundPortal1 && bFoundPortal2)
	{
		//Msg("Both portals are in walkable places\n");
		ForceNodeLink(m_pPortal1Node, m_pPortal2Node);
	}
	if ((!bFoundPortal1 || !bFoundPortal2))// || ((bNewPortal1 || bNewPortal2) && pOldPortal1Node && pOldPortal2Node && pOldPortal1Node->HasLink(pOldPortal2Node->m_iID)))
	{
		//Msg("Didn't find either portal\n");//portals were probably fizzled
		if (pOldPortal1Node && pOldPortal2Node && pOldPortal1Node->HasLink(pOldPortal2Node->m_iID))//get rid of old connection
		{
			//Warning("Breaking old AI nav link between %0.1f %0.1f %0.1f and %0.1f %0.1f %0.1f\n",
			//	pOldPortal1Node->m_vOrigin[0], pOldPortal1Node->m_vOrigin[1], pOldPortal1Node->m_vOrigin[2],
			//	pOldPortal2Node->m_vOrigin[0], pOldPortal2Node->m_vOrigin[1], pOldPortal2Node->m_vOrigin[2]);
			pOldPortal1Node->HasLink(pOldPortal2Node->m_iID)->m_LinkInfo |= bits_LINK_OFF;
		}
	}
	//old code required both portals to be visible and open.
	/*if (bFoundPortals)
	{
		//Msg("Did find our portals\n");
		if (m_hPortal1.ToInt() && hOldPortal1.ToInt() && m_hPortal1.ToInt() != hOldPortal1.ToInt())
		{
			//Msg("New blue portal");
			//Vector m_vecMoveDir = Vector(0, 0, 0);
			//AngleVectors(m_hPortal1.Get()->GetAbsAngles(), &m_vecMoveDir);
			////We want to test from a small distance ahead of the portal or else tracelines will hit the portal plane itself, and fixing it here is surely easier
			//Vector vecPortalAhead = m_hPortal1.Get()->GetAbsOrigin() + (m_vecMoveDir * 4);
			m_pPortal1Node = GetNavigator()->GetNetwork()->GetNode(GetNavigator()->GetNetwork()->NearestNodeToPoint(m_hPortal1.Get()->GetAbsOrigin(), true));
			//Msg(", node %i\n", m_pPortal1Node->m_iID);
		}
		if (m_hPortal2.ToInt() && hOldPortal2.ToInt() && m_hPortal2.ToInt() != hOldPortal2.ToInt())
		{
			//Msg("New orange portal");
			m_pPortal2Node = GetNavigator()->GetNetwork()->GetNode(GetNavigator()->GetNetwork()->NearestNodeToPoint(m_hPortal2.Get()->GetAbsOrigin(), true));
			//Msg(", node %i\n", m_pPortal2Node->m_iID);
		}
		if (m_pPortal1Node != pOldPortal1Node || m_pPortal2Node != pOldPortal2Node)//if either portal has changed places (significantly) or just been found
		{
			//Msg("A portal has changed places\n");
			if (pOldPortal1Node && pOldPortal2Node && pOldPortal1Node->HasLink(pOldPortal2Node->m_iID))//get rid of old connection
			{
				//Msg("Breaking old AI nav link between %0.1f %0.1f %0.1f and %0.1f %0.1f %0.1f\n",
				//	pOldPortal1Node->m_vOrigin[0], pOldPortal1Node->m_vOrigin[1], pOldPortal1Node->m_vOrigin[2],
				//	pOldPortal2Node->m_vOrigin[0], pOldPortal2Node->m_vOrigin[1], pOldPortal2Node->m_vOrigin[2]);
				pOldPortal1Node->HasLink(pOldPortal2Node->m_iID)->m_LinkInfo |= bits_LINK_OFF;
				pOldPortal1Node->HasLink(pOldPortal2Node->m_iID)->m_LinkInfo &= ~bits_LINK_PORTAL;
			}
		}
		if (m_pPortal1Node && m_pPortal2Node)
		{
			//Msg("Both portals are in walkable places\n");
			//CAI_Link *pOldLink = m_pPortal2Node->HasLink(m_pPortal1Node->m_iID);
			//if (!pOldLink)
			//{
			//Msg("Making new AI nav link between %0.1f %0.1f %0.1f and %0.1f %0.1f %0.1f\n",
			//	m_pPortal1Node->m_vOrigin[0], m_pPortal1Node->m_vOrigin[1], m_pPortal1Node->m_vOrigin[2],
			//	m_pPortal2Node->m_vOrigin[0], m_pPortal2Node->m_vOrigin[1], m_pPortal2Node->m_vOrigin[2]);
			ForceNodeLink(m_pPortal1Node, m_pPortal2Node);
			//}
			//else
			//{
			//	Msg("Found an old link. All done. from %0.1f %0.1f %0.1f to %0.1f %0.1f %0.1f\n",
			//		m_pPortal1Node->m_vOrigin[0], m_pPortal1Node->m_vOrigin[1], m_pPortal1Node->m_vOrigin[2],
			//		m_pPortal2Node->m_vOrigin[0], m_pPortal2Node->m_vOrigin[1], m_pPortal2Node->m_vOrigin[2]);//This should not appear early on in debugging
			//}
		}
	}
	else//portals were probably fizzled
	{
		//Msg("Didn't find our portals\n");
		if (pOldPortal1Node && pOldPortal2Node && pOldPortal1Node->HasLink(pOldPortal2Node->m_iID))//get rid of old connection
		{
			//Msg("Breaking old AI nav link between %0.1f %0.1f %0.1f and %0.1f %0.1f %0.1f\n",
			//	pOldPortal1Node->m_vOrigin[0], pOldPortal1Node->m_vOrigin[1], pOldPortal1Node->m_vOrigin[2],
			//	pOldPortal2Node->m_vOrigin[0], pOldPortal2Node->m_vOrigin[1], pOldPortal2Node->m_vOrigin[2]);
			pOldPortal1Node->HasLink(pOldPortal2Node->m_iID)->m_LinkInfo |= bits_LINK_OFF;
		}
	}*/
}

//
//Sometimes we need to jump simply for the speed boost. Normal walking speed is 150. Jumping boosts us to 225.
//
void CNPC_Chell::JumpForSpeed()
{
	//what if, no?
	//return;
	//if (m_pPlayer->GetGroundEntity() && m_bDucked)
	//	PlayerDuckEnd();
	//Likely to hurt more than help
	//if (m_bCrossingPortal)
	//	return;
	//Check current speed because jumping could actually hurt more than help if we're in the air and not accelerating via walking
	if (m_bInCycle && m_pPlayer->GetGroundEntity() && m_pPlayer->GetLocalVelocity().Length() >= 149 && m_flLastCyclePortalTime != 0)
	{
		//Avoid jumping onto a cube. It will only cause problems. Distance from player to cube must be higher than something.
		if (/*m_ChellGoal->GetMoveType() == MOVETYPE_VPHYSICS &&*/ (GetAbsOrigin() - m_ChellGoal->GetAbsOrigin()).Length() < 145)
		{
			//Warning("Going to land on cube, don't jump!\n");
			return;
		}
		//Only jump when going in the direction we're supposed to be going in.
		float flTargetYaw = m_angTargetView[1];
		QAngle angPlayerVelocity;
		VectorAngles(m_pPlayer->GetLocalVelocity(), angPlayerVelocity);
		if (angPlayerVelocity[1] > 180)
			angPlayerVelocity[1] -= 360;
		if (m_bWalkReverse)
		{
			if (flTargetYaw > 0)
				flTargetYaw -= 180;
			else
				flTargetYaw += 180;
		}
		if (abs(angPlayerVelocity[1] - flTargetYaw) >= 5)//This number is how accurate the direction needs to be. Lower means more accurate.
		{
			//Warning("Bad trajectory, don't jump! (%0.1f and %0.1f)\n", angPlayerVelocity[1], flTargetYaw);
			return;
		}
		//Msg("Starting path length: %0.1f\n", GetNavigator()->GetPath()->GetPathLength());
		float flPathLength = GetNavigator()->GetPath()->GetPathLength();
		//We are assuming we're in test 01 at the moment.
		if (strcmp(STRING(m_ChellGoal->GetEntityName()), "chelldest_cyclewaitingplace"))
		{
			//Msg("Still have to go back once reached destination, add on %0.1f to length\n", m_flCurrentPathLength);
			flPathLength += m_flCurrentPathLength;//Add on length of full path including what's already covered.
		}
		//Add on the length of the object we are or will be holding to prevent it from being separated from us by a portal quickly closing.
		bool bWillPickUpCube = false;
		if (GetPlayerHeldEntity(m_pPlayer) || m_ChellGoal->GetMoveType() == MOVETYPE_VPHYSICS)
		{
			flPathLength += 100;
			if (GetPlayerHeldEntity(m_pPlayer) || m_ChellGoal->GetMoveType() == MOVETYPE_VPHYSICS)
				bWillPickUpCube = true;
			//Msg("Account for the cube we have to hold: %0.1f + 100 = %0.1f\n", flPathLength - 100, flPathLength);
		}
		//Msg("Path length %0.1f / walk speed 140 (%0.1f) >= time left %0.1f\n", flPathLength, flPathLength / 140, gpGlobals->curtime - m_flLastCyclePortalTime);
		float flTimeLeft = gpGlobals->curtime - m_flLastCyclePortalTime;
		if (bWillPickUpCube)
			flTimeLeft -= .5;
		if (flPathLength / 150 >= flTimeLeft)
		{
			Msg("Jumping\n");
			PlayerJump();
			//PlayerDuckStart();//Ducking allows for extra distance jumped.
		}
		else
		{
			Msg("Not Jumping\n");
		}
	}
}

void CNPC_Chell::Think()
{
	//Msg("Think started\n");
	BaseClass::Think();//
	//Msg("Think marker 1\n");
	//GetNavigator()->GetNetwork()->NumSubZones();
	if (chell_plrctrl_disable.GetBool())
	{
		//Msg("Think marker 2\n");
		chell_plrctrl_aim.SetValue(0);
		chell_plrctrl_walk.SetValue(0);
		PlayerStopAiming();
		PlayerStopMoving();
		SUB_Remove();
	}
	if (chell_plrctrl_enable.GetBool())
	{
		//Msg("Think marker 2\n");
		chell_plrctrl_aim.SetValue(1);
		chell_plrctrl_walk.SetValue(1);
	}

	//Msg("Chell thinking");

	//if (m_ChellGoal != NULL && m_hGoalEnt == NULL && strlen(m_strChellGoal) != 0)
	//{
	//	ChellNavToEntity(m_strChellGoal);
	//}

	UpdatePortalNodeLinks();//We can't run this just at the moment we get a nav command, we have to run it continously as portals can
	//change places IN ORDER TO navigate, and it will fix a bug with routes improperly simplifying if they start by going through a portal
	//Msg("Think marker 3\n");

	FindObstacles();
	//Msg("Think marker 4\n");

	EvaluateObstacle();
	//Msg("Think marker 5\n");
	
	SteerIncremental();
	//Msg("Think marker 6\n");

	GetWaypointCurChell();
	//Msg("Think marker 7\n");
	
	MaintainPath();
	//Msg("Think marker 8\n");

	JumpForSpeed();
	//Msg("Think marker 9\n");

	PassiveFinalGoalSeek();

	SetNextThink(gpGlobals->curtime + NPC_CHELL_THINKRATE);
	//Msg("Think finished\n");
}

CAI_Link *CNPC_Chell::ForceNodeLink(CAI_Node *pNode1, CAI_Node *pNode2)
{
	CAI_Link *pLink = pNode2->HasLink(pNode1->m_iID);
	if (!pLink)//Sometimes HasLink is acceptable, sometimes not. WHY??? When it's not, we have to manually make the link. this shouldn't be necessary though...
	{
		//Msg("Chell forced node link\n");
		pLink = GetNavigator()->GetNetwork()->CreateLink(pNode1->m_iID, pNode2->m_iID);
	}
	//int acceptedMotions[NUM_HULLS];
	pLink->m_iAcceptedMoveTypes[HULL_HUMAN] = bits_CAP_MOVE_GROUND;
	pLink->m_LinkInfo &= ~bits_LINK_OFF;
	pLink->m_LinkInfo |= bits_LINK_PORTAL;
	return pLink;
}

void CNPC_Chell::PlayerUsePickUp(char *type)
{
	Warning("Picking up: %s\n", type);
	if (!GetPlayerHeldEntity(m_pPlayer))
	{
		PlayerUseImpulse(type);
	}
	else
	{
		Warning("Pick up failed because we are already holding something\n", type);
	}
}

void CNPC_Chell::PlayerUseDrop(char *type)
{
	Warning("Dropping: %s\n", type);
	if (GetPlayerHeldEntity(m_pPlayer))
	{
		PlayerUseImpulse(type);
	}
	else
	{
		Warning("Drop failed because we aren't holding anything\n", type);
	}
}

void CNPC_Chell::PlayerUseImpulse(char *type)
{
	//Warning("USE: %s\n", type);
	SendCommand("+use;wait 10;-use");
}

void CNPC_Chell::PlayerShootBlue()
{
	SendCommand("+attack;wait 10;-attack");
}

void CNPC_Chell::PlayerShootOrange()
{
	SendCommand("+attack2;wait 10;-attack2");
}

void CNPC_Chell::PlayerJump()
{
	SendCommand("+jump;wait 10;-jump");
}

void CNPC_Chell::PlayerDuckStart()
{
	SendCommand("+duck");
	m_bDucked = true;
}

void CNPC_Chell::PlayerDuckEnd()
{
	SendCommand("-duck");
	m_bDucked = false;
}

void CNPC_Chell::PlayerLookUp(float angDif)
{
	if (chell_plrctrl_aim.GetBool())
	{
		GetAimSpeed(angDif, true);
		SendCommand("-lookdown");
		SendCommand("+lookup");
	}
}

void CNPC_Chell::PlayerLookDown(float angDif)
{
	if (chell_plrctrl_aim.GetBool())
	{
		GetAimSpeed(angDif, true);
		SendCommand("-lookup");
		SendCommand("+lookdown");
	}
}

void CNPC_Chell::PlayerLookLeft(float angDif)
{
	if (chell_plrctrl_aim.GetBool())
	{
		GetAimSpeed(angDif, false);
		SendCommand("-right");
		SendCommand("+left");
	}
}

void CNPC_Chell::PlayerLookRight(float angDif)
{
	if (chell_plrctrl_aim.GetBool())
	{
		GetAimSpeed(angDif, false);
		SendCommand("-left");
		SendCommand("+right");
	}
}

void CNPC_Chell::PlayerStopMoving()
{
	if (chell_plrctrl_walk_noletup.GetBool())
		return;
	SendCommand("-forward");
	SendCommand("-back");
	SendCommand("-moveleft");
	SendCommand("-moveright");
}

void CNPC_Chell::PlayerStopAiming()
{
	SendCommand("-left");
	SendCommand("-right");
	SendCommand("-lookup");
	SendCommand("-lookdown");
}

void CNPC_Chell::SendCommand(const char *command)
{
	//Msg("(%0.1f) Command: %s\n", gpGlobals->curtime, command);
	engine->ClientCommand(engine->PEntityOfEntIndex(1), "%s\n", command);
}

//
//Go to the final goal location if we aren't busy or unable.
//
void CNPC_Chell::PassiveFinalGoalSeek()
{
	if (gpGlobals->curtime < m_flWaitTime || m_bPickingUpObject || GetPlayerHeldEntity(m_pPlayer) || m_bCrossingPortal)
	{
		return;
	}
	//last ditch effort to avoid pressing our face against glass like a fucking idiot
	bool bQuickFail = false;
	if (m_pCurWaypoint)
	{
		trace_t tr;
		UTIL_TraceLine(GetAbsOrigin() + Vector(0, 0, 32), m_pCurWaypoint->GetPos() + Vector(0, 0, 1), MASK_PLAYERSOLID, m_pPlayer, COLLISION_GROUP_NONE, &tr);
		if (tr.fraction != 1 && tr.m_pEnt != GetPlayerHeldEntity(m_pPlayer) && tr.m_pEnt != m_ChellGoal)
		{
			//Try fixing me in ANY OTHER WAY before relying on this!
			Warning("Last ditch effort self correction.\n");
			NDebugOverlay::Line(GetAbsOrigin(), m_pCurWaypoint->GetPos() + Vector(0, 0, 1), 255, 0, 0, true, 30);
			bQuickFail = true;
		}
	}
	CBaseEntity *pFinalGoal = gEntList.FindEntityByName(NULL, m_strChellGoal);
	//if we are at our final goal, bail (prevents massive lag from calling ComplexNavTest rapidly)
	if (pFinalGoal && (GetAbsOrigin() - pFinalGoal->GetAbsOrigin()).Length() < 100)
	{
		return;
	}
	//Msg("CNPC_Chell::PassiveFinalGoalSeek marker 1\n");
	if (bQuickFail || (m_flCurrentPathLength == 0 && !m_bShootingPortal))
	{
		//Msg("CNPC_Chell::PassiveFinalGoalSeek marker 2\n");
		if (m_strChellGoal)//In the middle of solving. (But currently standing still.)
		{
			//Msg("CNPC_Chell::PassiveFinalGoalSeek marker 3\n");
			//AI_Waypoint_t *pFirstWaypoint = GetPathfinder()->BuildRoute(GetAbsOrigin(), m_pFinalGoal->GetAbsOrigin(), NULL, NPC_CHELL_NAV_ARRIVE_TOLERANCE, true, GetNavType());
			chellNavQuery_t resToGoal = ComplexNavTest(GetAbsOrigin(), pFinalGoal->GetAbsOrigin(), 0, pFinalGoal);

			//test 00: we will stall after Wait() if the door is closed because it gets found by ScanAhead
			//test 01: we will attempt to go through the chamber door while it's closed if we don't use ScanAhead
			if (resToGoal.result != CNQR_NO)//There is a path to the goal ent, so go to it.
			{
				if (ScanAheadForBlockers(resToGoal.pResultWaypoint))
				{
					EvaluateObstacle();
					return;
				}
				//ScanAheadForBlockers(resToGoal.pResultWaypoint);
				//EvaluateObstacle();
				if (m_flCurrentPathLength != 0)
					return;
				//Msg("m_bInCycle set to false from PassiveFinalGoalSeek, was %s\n", m_bInCycle ? "TRUE" : "FALSE");
				m_bInCycle = false;
				ChellNavToEntity(m_strChellGoal, "PassiveFinalGoalSeek()", 0);
			}
			else
			{
				if (resToGoal.pResultWaypoint)
					ScanAheadForBlockers(resToGoal.pResultWaypoint);
				EvaluateObstacle();
			}
		}
		else
		{
			//Automatically find where to go. This is currently being scrapped cause there's too many different cases to consider and it isn't really needed.

			/*
			//try to path to an elevator.
			if (m_pPlayer->GetGroundEntity() && m_strChellGoal && !strcmp(STRING(m_pPlayer->GetGroundEntity()->GetModelName()), "models/props/round_elevator_body.mdl") && !strcmp(STRING(m_pPlayer->GetGroundEntity()->GetEntityName()), m_strChellGoal))
			{
				//We are already in the goal elevator.
				return;
			}


			CBaseEntity *m_pElevCand = gEntList.FindEntityByClassname(NULL, "prop_dynamic");
			while (m_pElevCand)
			{
				if (!strcmp(STRING(m_pElevCand->GetModelName()), "models/props/round_elevator_body.mdl"))
				{
					chellNavQuery_t navResult = ComplexNavTest(GetAbsOrigin(), m_pElevCand->GetAbsOrigin(), true);
					//come back to this
					if (navResult.result == CNQR_ALLPOS)
					{
						//There are three kinds of elevators that exist at the beginning of a level.
						//Elevators that are completely unreachable (not including the step of riding in another elevator), AKA in a different chamber (CNQR_NO)
						//Elevators that are reachable by simply walking over to them, AKA the starting one (CNQR_SIMPLE)
						//Elevators that could be reached if some extra steps were done, AKA the goal (CNQR_ALLPOS)

						//go to it.
						ExecuteShot(navResult.shot);
						break;
					}
				}
				m_pElevCand = gEntList.FindEntityByClassname(m_pElevCand, "prop_dynamic");
			}
			*/
		}
	}
}

void CNPC_Chell::ExecuteShot(portalShot_t *shot)
{
	if (m_iGunInfo == GUN_NONE)
	{
		Warning("Cannot shoot portal - don't have gun!\n");
		return;
	}
	m_bShootingPortal = true;
	ChellLookAtPos(shot->vecPos, "aiming portal shot");
	//Go to where the shot says to shoot from. Nope, not needed.
	//CBaseEntity *pTarget = CreateEntityByName("info_target");
	//pTarget->SetAbsOrigin(shot->pSrcNode->GetPosition(HULL_HUMAN));
	//pTarget->SetName(MAKE_STRING("chelldest_portalstartpos"));
	//ChellNavToEntity("chelldest_portalstartpos", "moving to position for portal shot", 0);
}

//
//Sometimes when we query for paths, we need to have an "all possibilities" result and a "simple navigation" result.
//ALL POS considers movement techniques such as flings and hypothetical portal placements.
//Basically, all possible states in a finite state machine.
//Simple will work like normal navigation does, basically, only worrying about the current state.
//
chellNavQuery_t CNPC_Chell::ComplexNavTest(Vector vecStart, Vector vecEnd, int flags, CBaseEntity *pTarget, float flGoalTolerance)
{
	//Msg("ComplexNavTest marker 1\n");
	//dont try start cycle again if the flag said not to, or there's already a waitng place entity.
	bool bGoToCycle = !(flags & QF_DONT_DETECT_CYCLE) && !gEntList.FindEntityByName(NULL, "chelldest_cyclewaitingplace") && DetectCycle();
	//First try simple nav.
	AI_Waypoint_t *pFirstWaypoint = GetPathfinder()->BuildRoute(vecStart, vecEnd, pTarget, flGoalTolerance, true, GetNavType());
	if (pFirstWaypoint && !bGoToCycle)
	{
		//Msg("ComplexNavTest marker 2\n");
		chellNavQuery_t result;
		result.pResultWaypoint = pFirstWaypoint;
		result.result = CNQR_SIMPLE;
		result.vecHypoPortalPos = Vector(0, 0, 0);
		result.pGoalEnt = pTarget;
		return result;
	}
	else
	{
		//Msg("ComplexNavTest marker 3\n");
		if (flags & QF_SIMPLE_ONLY)
		{
			//Msg("ComplexNavTest marker 4\n");
			//For whatever reason, we aren't allowed to use complex nav, so fail.
			chellNavQuery_t result;
			result.result = CNQR_NO;
			result.vecHypoPortalPos = Vector(0, 0, 0);
			result.pGoalEnt = pTarget;
			return result;
		}
		//Not a simple path ahead.
		//Could we use a portalgun to reach the goal?
		//Also consider if we will obtain a portal gun on this level.
		if (m_iGunInfo == GUN_NONE)
		{
			//Msg("ComplexNavTest marker 5\n");
			//We have no gun. Will we get one during this level?
			CBaseEntity *pGun = FindUnheldPortalGun();
			if (pGun)
			{
				//Msg("ComplexNavTest marker 6\n");
				chellNavQuery_t resStartToGun = PortalChangeNavTest(vecStart, pGun->GetAbsOrigin(), !(flags & QF_NO_PORTAL_TRAVEL), GUN_NONE, flags);
				if (resStartToGun.result != CNQR_NO)
				{
					//Msg("ComplexNavTest marker 7\n");
					//Will the elevator in question be reachable AFTER getting this portal gun?
					//Remember, there are elevators in completely different test chambers that must be accounted for.

					//This returns the necessary shot to portal up to the orange portal.
					chellNavQuery_t resGunToEnd = PortalChangeNavTest(pGun->GetAbsOrigin(), vecEnd, !(flags & QF_NO_PORTAL_TRAVEL), GUN_BLUE, flags);
					if (resGunToEnd.bHasShot && resGunToEnd.result != CNQR_NO)
					{
						//Msg("ComplexNavTest marker 8\n");
						//hybrid result
						chellNavQuery_t startToEnd;
						startToEnd.bHasShot = true;
						startToEnd.result = CNQR_ALLPOS;
						startToEnd.pResultWaypoint = resStartToGun.pResultWaypoint;
						startToEnd.vecHypoPortalPos = resStartToGun.vecHypoPortalPos;
						startToEnd.pGoalEnt = pTarget;
						return startToEnd;
					}
				}
			}
			else if (!(flags & QF_NO_PORTAL_TRAVEL) && bGoToCycle)//test 1
			{
				//Msg("DetectCycle true\n");
				//I really want to make this more dynamic/adaptive but it also really isn't worth the time and effort.
				CBaseEntity *pEnt = gEntList.FindEntityByName(NULL, "chelldest_cyclewaitingplace");
				if (pEnt == NULL)
				{
					Vector vecWaitingSpot = NearestNodeToPoint(m_pStaticPortal->GetAbsOrigin(), true, false)->GetPosition(HULL_HUMAN) + Vector(0, 0, 1);
					trace_t xMin;
					UTIL_TraceLine(vecWaitingSpot, vecWaitingSpot + Vector(-1000, 0, 0), MASK_SOLID, NULL, COLLISION_GROUP_NONE, &xMin);
					trace_t xMax;
					UTIL_TraceLine(vecWaitingSpot, vecWaitingSpot + Vector(1000, 0, 0), MASK_SOLID, NULL, COLLISION_GROUP_NONE, &xMax);
					vecWaitingSpot.x = (xMin.endpos.x + xMax.endpos.x) / 2;
					trace_t yMin;
					UTIL_TraceLine(vecWaitingSpot, vecWaitingSpot + Vector(0, -1000, 0), MASK_SOLID, NULL, COLLISION_GROUP_NONE, &yMin);
					trace_t yMax;
					UTIL_TraceLine(vecWaitingSpot, vecWaitingSpot + Vector(0, 1000, 0), MASK_SOLID, NULL, COLLISION_GROUP_NONE, &yMax);
					vecWaitingSpot.y = (yMin.endpos.y + yMax.endpos.y) / 2;

					//Msg("PortalChangeNavTest marker 14\n");
					//Make chell go into the starting area for the portal cycle
					CBaseEntity *pTarget = CreateEntityByName("info_target");
					pTarget->SetAbsOrigin(vecWaitingSpot);
					pTarget->SetName(MAKE_STRING("chelldest_cyclewaitingplace"));
				}
				pEnt = gEntList.FindEntityByName(NULL, "chelldest_cyclewaitingplace");
				AI_Waypoint_t *pWPToTrigger = GetPathfinder()->BuildRoute(GetAbsOrigin(), pEnt->GetAbsOrigin(), NULL, NPC_CHELL_NAV_ARRIVE_TOLERANCE, true, GetNavType());
				if (pWPToTrigger)// && !m_pPlayer->GetAbsVelocity().Length() && (GetAbsOrigin() - pEnt->GetAbsOrigin()).Length2D() > 100)
				{
					//Msg("InCycle set to true, was %s\n", m_bInCycle ? "TRUE" : "FALSE");
					m_bInCycle = true;//may want to move this to NavTo...? but it should be okay here.
					chellNavQuery_t result;
					result.pResultWaypoint = pWPToTrigger;
					result.result = CNQR_ALLPOS;//Not sure if this should be allpos or simple tbh
					result.vecHypoPortalPos = Vector(0, 0, 0);
					result.pGoalEnt = pEnt;
					return result;
				}
				else
				{
					//pass a success result... but what should it look like???
				}
			}
			//Just try a straight PortalChangeNavTest (for test 01)
			//chellNavQuery_t startToEnd = PortalChangeNavTest(vecStart, vecEnd, bAllowPortalTravel, GUN_NONE);
		}
		if (m_iGunInfo == GUN_BLUE || m_iGunInfo == GUN_ORANGE)
		{
			Msg("ComplexNavTest marker 9\n");
			chellNavQuery_t navResult = PortalChangeNavTest(vecStart, vecEnd, !(flags & QF_NO_PORTAL_TRAVEL), m_iGunInfo, flags | QF_DONT_DETECT_CYCLE);
			navResult.pGoalEnt = pTarget;
			return navResult;
			/*if (navResult.bHasShot && navResult.result != CNQR_NO)
			{
				Msg("ComplexNavTest marker 10\n");
				//hybrid result
				chellNavQuery_t startToEnd;
				startToEnd.bHasShot = true;
				startToEnd.result = CNQR_ALLPOS;
				startToEnd.pResultWaypoint = startToGun.pResultWaypoint;
				startToEnd.vecHypoPortalPos = startToGun.vecHypoPortalPos;
				return startToEnd;
			}*/
		}
	}
	//Msg("ComplexNavTest gave negative result!\n");
	chellNavQuery_t result;
	result.result = CNQR_NO;
	result.vecHypoPortalPos = Vector(0, 0, 0);
	result.pGoalEnt = pTarget;
	return result;
}

//
//Get a subZoneLinkInfo_t from a start and end subzone.
//
subZoneLink_t *CNPC_Chell::GetSZLink(int iSrcSubZone, int iDestSubZone)
{
	for (int i = 0; i < m_SubZoneLinks.Count(); i++)
	{
		if (m_SubZoneLinks[i]->iSrcszID == iSrcSubZone && m_SubZoneLinks[i]->iDestszID == iDestSubZone)
			return m_SubZoneLinks[i];
	}
	//No link exists for us yet.
	CCopyableUtlVector< portalShot_t *> shots;
	subZoneLink_t *link = new subZoneLink_t(iSrcSubZone, iDestSubZone, SZLINK_UNKNOWN, shots);
	m_SubZoneLinks.AddToTail(link);
	return link;
}

//
//Simple test for if a portal shot would succeed.
//This assumes vecEnd is some point beyond a wall, if it's right on the portalable plane, there may be problems.
//
bool CNPC_Chell::QuickPortalTrace(Vector vecStart, Vector vecEnd, trace_t *tr)
{
	UTIL_TraceLine(vecStart, vecEnd, MASK_SOLID, m_pPlayer, COLLISION_GROUP_DEBRIS, tr);
	if (tr->fraction != 1 && !IsNoPortalMaterial(tr->surface))
	{
		NDebugOverlay::Line(vecStart, tr->endpos, 255, 0, 255, true, 30);
		QAngle angles;
		VectorAngles(tr->plane.normal, angles);
		float flPlacementSuccess = VerifyPortalPlacement(NULL, tr->endpos, -angles, PORTAL_PLACED_BY_PLAYER, true);
		//Msg("flPlacementSuccess < 1 is %s\n", (flPlacementSuccess < 1) ? "TRUE" : "FALSE");
		return flPlacementSuccess < 1;
	}
	//we were supposed to hit a wall...
	NDebugOverlay::Line(vecStart, tr->endpos, 0, 0, 255, true, 30);
	return false;
}

chellNavQuery_t CNPC_Chell::QuickShotCreate(Vector pos, CAI_Node *pSrcNode)
{
	portalShot_t *shot = new portalShot_t;
	//portalShot_t *shot_b = shot;
	shot->vecPos = pos;
	shot->pDestNode = GetNodeNearPortal(pos);
	shot->pSrcNode = pSrcNode;
	shot->bLOS = true;
	shot->bUseYaw = false;
	shot->flYaw = 0;
	shot->flPlacementSuccess = PORTAL_FIZZLE_SUCCESS;
	m_PortalShots.AddToTail(*shot);

	chellNavQuery_t resToShotPos = ComplexNavTest(pSrcNode->GetOrigin(), shot->pDestNode->GetOrigin(), QF_SIMPLE_ONLY | QF_DONT_GET_GUN | QF_DONT_DETECT_CYCLE);

	chellNavQuery_t result;
	result.pResultWaypoint = resToShotPos.pResultWaypoint;
	result.pResultWaypoint->GetLast()->SetShot(shot);
	result.bHasShot = true;
	result.result = CNQR_ALLPOS;
	result.vecHypoPortalPos = Vector(0, 0, 0);
	return result;
}

chellNavQuery_t CNPC_Chell::FindPortalShotsFromAutoGun(CBaseEntity *pGun, Vector vecEnd)
{
	trace_t tr;
	if (QuickPortalTrace(pGun->GetAbsOrigin(), vecEnd, &tr))
	{
		//Can we walk to the hypo-portal right now?
		chellNavQuery_t resToMovingPortal = ComplexNavTest(GetAbsOrigin(), tr.endpos, QF_SIMPLE_ONLY | QF_NO_PORTAL_TRAVEL | QF_DONT_GET_GUN | QF_DONT_DETECT_CYCLE);
		if (resToMovingPortal.result == CNQR_SIMPLE)
		{
			//Go to the hypo-portal, but if the portal is there go to the gun instead
			//ChellNavToEntity(pGun->GetEntityName().ToCStr(), tr.endpos);
			//link the two paths together!?
			chellNavQuery_t result;
			result.result = CNQR_SIMPLE;
			result.pResultWaypoint = resToMovingPortal.pResultWaypoint;
			result.vecHypoPortalPos = tr.endpos;
			return result;
		}
	}
	//Warning("FindPortalShotsFromAutoGun gave negative result!\n");
	chellNavQuery_t result;
	result.result = CNQR_NO;
	result.vecHypoPortalPos = Vector(0, 0, 0);
	return result;
}

//
//Consider if we could get from point A to B if we change the positions of portals.
//We will not worry in this function about the possibility of obtaining a portal gun over the course of the test chamber. That is in ComplexNavTest.
//iGunInfo = hypothetical gun info state which doesn't have to match m_iGunInfo.
//
chellNavQuery_t CNPC_Chell::PortalChangeNavTest(Vector vecStart, Vector vecEnd, bool bAllowPortalTravel, int iGunInfo, int flags, CBaseEntity *pTarget, float flGoalTolerance)
{
	Msg("PortalChangeNavTest marker 1\n");
	//Find nearest nodes to start and end.
	CAI_Node *pStartNode = (vecStart == GetAbsOrigin()) ? MyNearestNode(true) : NearestNodeToPoint(vecStart, true, false);
	CAI_Node *pEndNode = NearestNodeToPoint(vecEnd, true, true);
	//GetNavigator()->GetNetwork()->NumSubZones();
	Assert(pStartNode && pEndNode);

	//In tests 1 and 2, portal positions can change despite us not having a gun.
	if (iGunInfo == GUN_NONE)
	{
		//Msg("PortalChangeNavTest marker 2\n");
		bool bUnheldPortalGun = false;
		CBaseEntity *pGun = gEntList.FindEntityByClassname(NULL, "weapon_portalgun");
		while (pGun)
		{
			//Msg("PortalChangeNavTest marker 3\n");
			CWeaponPortalgun *pPortalGun = static_cast<CWeaponPortalgun *>(pGun);
			if (!pPortalGun->GetOwner())
			{
				//Msg("PortalChangeNavTest marker 4\n");
				bUnheldPortalGun = true;
				break;
			}
			pGun = gEntList.FindEntityByClassname(pGun, "weapon_portalgun");
		}
		if (!bUnheldPortalGun || (flags & QF_DONT_GET_GUN))
		{
			//Msg("PortalChangeNavTest marker 5\n");
			pGun = NULL;
		}
		if (pGun)//test 2, trying to reach portal gun - don't bother checking if we can walk straight to the portal gun. we can if the blue portal is in the right spot, but otherwise, level design would logically not allow that.
		{
			//Msg("PortalChangeNavTest marker 6\n");
			//Can we walk from the orange (static) portal to the gun?
			bool bFoundPortal = false;
			CBaseEntity *pPortalEnt = gEntList.FindEntityByClassname(NULL, "prop_portal");
			chellNavQuery_t resStaticToGun;
			while (pPortalEnt)
			{
				//Msg("UpdatePortalNodeLinks loop\n");
				CProp_Portal *pPortal = static_cast<CProp_Portal*>(pPortalEnt);
				if (pPortal)
				{
					if (pPortal->IsPortal2())
					{
						//using QF_DONT_DETECT_CYCLE here is making assumptions about level design but it should be fine for the portal campaign
						resStaticToGun = ComplexNavTest(pPortal->GetAbsOrigin(), pGun->GetAbsOrigin(), QF_SIMPLE_ONLY | QF_NO_PORTAL_TRAVEL | QF_DONT_GET_GUN | QF_DONT_DETECT_CYCLE, pGun);
						if (resStaticToGun.result == CNQR_SIMPLE)
						{
							if (bFoundPortal)
							{
								Warning("Found two orange portals that can be used to reach the portal gun, confused!\n");
								chellNavQuery_t result;
								result.result = CNQR_NO;
								result.vecHypoPortalPos = Vector(0, 0, 0);
								return result;
							}
							else
							{
								bFoundPortal = true;
							}
						}
					}
				}
				pPortalEnt = gEntList.FindEntityByClassname(pPortalEnt, "prop_portal");
			}
			if (bFoundPortal)
			{
				//Msg("PortalChangeNavTest marker 8a\n");
				//find out about the positions where the automated gun can shoot. We should be able to walk to at least one hypo-portal.
				//We should never hit this code in test 3 or 12 for multiple reasons.
				chellNavQuery_t res1 = FindPortalShotsFromAutoGun(pGun, pGun->GetAbsOrigin() + Vector(1000, 0, 0));
				if (res1.result == CNQR_SIMPLE)
				{
					//Msg("PortalChangeNavTest marker 8\n");
					//Combine path from current position to hypo-portal to gun.
					res1.pResultWaypoint->GetLast()->SetNext(resStaticToGun.pResultWaypoint);
					return res1;
				}
				//Msg("PortalChangeNavTest marker 9a\n");
				chellNavQuery_t res2 = FindPortalShotsFromAutoGun(pGun, pGun->GetAbsOrigin() + Vector(-1000, 0, 0));
				if (res2.result == CNQR_SIMPLE)
				{
					//Msg("PortalChangeNavTest marker 9\n");
					res2.pResultWaypoint->GetLast()->SetNext(resStaticToGun.pResultWaypoint);
					return res2;
				}
				//Msg("PortalChangeNavTest marker 10a\n");
				chellNavQuery_t res3 = FindPortalShotsFromAutoGun(pGun, pGun->GetAbsOrigin() + Vector(0, 1000, 0));
				if (res3.result == CNQR_SIMPLE)
				{
					//Msg("PortalChangeNavTest marker 10\n");
					res3.pResultWaypoint->GetLast()->SetNext(resStaticToGun.pResultWaypoint);
					return res3;
				}
				//Msg("PortalChangeNavTest marker 11a\n");
				chellNavQuery_t res4 = FindPortalShotsFromAutoGun(pGun, pGun->GetAbsOrigin() + Vector(0, -1000, 0));
				if (res4.result == CNQR_SIMPLE)
				{
					//Msg("PortalChangeNavTest marker 11\n");
					res4.pResultWaypoint->GetLast()->SetNext(resStaticToGun.pResultWaypoint);
					return res4;
				}
			}
			else
			{
				//Msg("PortalChangeNavTest marker 12\n");
				//Would this be anything?
			}
		}
	}
	//If using blue-only-gun
	if (iGunInfo == GUN_BLUE)
	{
		CAI_Node *pPortal2SubNode = m_pPortal2Node;
		if (!m_pPortal2Node)
		{
			CBaseEntity *pPortalEnt = gEntList.FindEntityByClassname(NULL, "prop_portal");
			//chellNavQuery_t navResultStaticToGun;
			bool bFoundPortal = false;
			while (pPortalEnt)
			{
				CProp_Portal *pPortal = static_cast<CProp_Portal*>(pPortalEnt);
				if (pPortal)
				{
					if (pPortal->IsPortal2())
					{
						//Using only simple nav means we may get a false negative result, but if we allow complex nav this can cause an infinite loop.
						//current problem: m_chellgoal changes to hypoportalpos after our 4th time through this function, so in the 5th time, we can no longer make a path to it in test 02
						//navResultStaticToGun = ComplexNavTest(pPortal->GetAbsOrigin(), m_ChellGoal->GetAbsOrigin(), QF_SIMPLE_ONLY | QF_DONT_DETECT_CYCLE, m_ChellGoal);
						CBaseEntity *pFinalGoal = gEntList.FindEntityByName(NULL, m_strChellGoal);
						chellNavQuery_t resStaticToFinal = ComplexNavTest(pPortal->GetAbsOrigin(), pFinalGoal->GetAbsOrigin(), QF_SIMPLE_ONLY | QF_DONT_DETECT_CYCLE, pFinalGoal);
						//Msg("navResultStaticToGun.result is %i\n", navResultStaticToGun.result);
						//Msg("navResultStaticToFinal.result is %i\n", navResultStaticToFinal.result);
						if (/*navResultStaticToGun.result != CNQR_NO &&*/ resStaticToFinal.result != CNQR_NO)
						{
							if (bFoundPortal)
							{
								Warning("Found two orange portals that can be used to reach the exit, confused!\n");
							}
							else
							{
								//Msg("Works\n");
								bFoundPortal = true;
								pPortal2SubNode = NearestNodeToPoint(pPortalEnt->GetAbsOrigin(), true, false);
							}
						}
					}
				}
				pPortalEnt = gEntList.FindEntityByClassname(pPortalEnt, "prop_portal");
			}
		}
		//Msg("PortalChangeNavTest marker 16\n");
		//Isn't this kinda dumb?
		//Find if the start is in the same subzone as the orange portal.
		if (pStartNode->GetSubZone() == pEndNode->GetSubZone())
		{
			//Msg("PortalChangeNavTest marker 17\n");
			//We don't even need to change portals in this case.
			chellNavQuery_t result;
			result.result = CNQR_SIMPLE;
			result.vecHypoPortalPos = Vector(0, 0, 0);
			return result;
		}
		else if (pPortal2SubNode && pEndNode->GetSubZone() == pPortal2SubNode->GetSubZone())//if the orange portal is closed, m_pPortal2Node is null!
		{
			//Msg("PortalChangeNavTest marker 18\n");
			//test 2 after getting gun
			//The end goal and the orange portal are in the same subzone, so we don't need to search for where to put a blue portal post crossing.
			//Just find somewhere we can put a blue portal.
			return FindPortalSpotWithinSubZone(pStartNode->GetSubZone());
		}
		else
		{
			//Msg("PortalChangeNavTest marker 27\n");
			//test 3
			//Could we walk or jump to the orange portal?
			chellNavQuery_t resToOrange = ComplexNavTest(vecStart, m_hPortal2->GetAbsOrigin(), QF_SIMPLE_ONLY | QF_DONT_DETECT_CYCLE, m_hPortal2);
			if (resToOrange.result != CNQR_NO)
			{
				//Msg("PortalChangeNavTest marker 28\n");
				//we can walk or jump to the orange portal:
				//Find which subzones we can get to from the orange portal subzone.
				for (int iCurrentSrcSubzone = AI_NODE_FIRST_ZONE; iCurrentSrcSubzone < GetNavigator()->GetNetwork()->NumSubZones(); iCurrentSrcSubzone++)
				{
					//Msg("PortalChangeNavTest marker 29\n");
					subZoneLink_t *szLink = GetSZLink(pPortal2SubNode->GetSubZone(), iCurrentSrcSubzone);
					//Unknown MUST be checked first.
					if (szLink->info == SZLINK_UNKNOWN)
					{
						Msg("Processing subzone link between %i and %i\n", pPortal2SubNode->GetSubZone(), iCurrentSrcSubzone);
						ProcessSubZoneLink(szLink, m_hPortal2->GetAbsOrigin());
					}
					if (szLink->info == SZLINK_YES)
					{
						portalShot_t *shot = szLink->shots[0];
						//Msg("PortalChangeNavTest marker 31\n");
						//now we need:
						//Path from the start to the node that the shot starts from
						chellNavQuery_t resToShot = ComplexNavTest(vecStart, shot->pSrcNode->GetOrigin(), QF_SIMPLE_ONLY | QF_DONT_DETECT_CYCLE);
						if (resToShot.result != CNQR_NO)
						{
							//put the portal shot into the waypoint
							resToShot.pResultWaypoint->GetLast()->SetShot(shot);
							//Path from that node to the orange portal
							chellNavQuery_t resShotToOrange = ComplexNavTest(shot->pSrcNode->GetOrigin(), m_hPortal2->GetAbsOrigin(), QF_SIMPLE_ONLY | QF_DONT_DETECT_CYCLE);
							if (resShotToOrange.result != CNQR_NO)
							{
								//connect paths
								resToShot.pResultWaypoint->GetLast()->SetNext(resShotToOrange.pResultWaypoint);
								//Path from the blue portal to the end
								chellNavQuery_t resBlueToEnd = ComplexNavTest(shot->pDestNode->GetOrigin(), vecEnd, QF_SIMPLE_ONLY | QF_DONT_DETECT_CYCLE);
								if (resBlueToEnd.result != CNQR_NO)
								{
									//connect again
									resToShot.pResultWaypoint->GetLast()->SetNext(resBlueToEnd.pResultWaypoint);
									resToShot.result = CNQR_ALLPOS;
									resToShot.bHasShot = true;
									return resToShot;
								}
							}
						}
					}
				}
			}
			else
			{
				//If we can't walk or jump to the orange portal:
					//Find what subzones can be reached by jumping.
					//Find what portal surfaces can be reached from those subzones.
						//If there's at least one reachable face then we're good, but we will want to prioritize for distance.
					//Run PortalChangeNavTest again, but with vecStart changed to the node nearest the orange portal, as we can now access the orange portal.
			}
		}
	}
		
	//Warning("PortalChangeNavTest gave negative result!\n");
	chellNavQuery_t result;
	result.result = CNQR_NO;
	result.vecHypoPortalPos = Vector(0, 0, 0);
	return result;
}

//
//Find a place to put a portal that is reachable from a given subzone
//
chellNavQuery_t CNPC_Chell::FindPortalSpotWithinSubZone(int iSubZone)
{
	//find portalable walls near relevant nodes.
	for (int iCurrentSrcNode = 0; iCurrentSrcNode < GetNavigator()->GetNetwork()->NumNodes(); iCurrentSrcNode++)
	{
		//Msg("PortalChangeNavTest marker 20 (%i) (msz: %i)\n", iCurrentSrcNode, iMySubZone);
		CAI_Node *pCurrentSrcNode = GetNavigator()->GetNetwork()->GetNode(iCurrentSrcNode);
		if (pCurrentSrcNode->GetSubZone() != iSubZone)
		{
			//Msg("FindPortalFromSubZone marker 1 (%i) (csz: %i)\n", iCurrentSrcNode, pCurrentSrcNode->GetSubZone());
			continue;
		}
		Vector vecNodeCameraPos = pCurrentSrcNode->GetOrigin() + Vector(0, 0, 64);
		trace_t tr1;
		if (QuickPortalTrace(vecNodeCameraPos, vecNodeCameraPos + Vector(100, 0, 0), &tr1))
		{
			//Msg("FindPortalFromSubZone marker 2 - S (%i)\n", iCurrentSrcNode);
			return QuickShotCreate(tr1.endpos, pCurrentSrcNode);
		}
		trace_t tr2;
		if (QuickPortalTrace(vecNodeCameraPos, vecNodeCameraPos + Vector(-100, 0, 0), &tr2))
		{
			//Msg("FindPortalFromSubZone marker 3 - S (%i)\n", iCurrentSrcNode);
			return QuickShotCreate(tr2.endpos, pCurrentSrcNode);
		}
		trace_t tr3;
		if (QuickPortalTrace(vecNodeCameraPos, vecNodeCameraPos + Vector(0, 100, 0), &tr3))
		{
			//Msg("FindPortalFromSubZone marker 4 - S (%i)\n", iCurrentSrcNode);
			return QuickShotCreate(tr3.endpos, pCurrentSrcNode);
		}
		trace_t tr4;
		if (QuickPortalTrace(vecNodeCameraPos, vecNodeCameraPos + Vector(0, -100, 0), &tr4))
		{
			//Msg("FindPortalFromSubZone marker 5 - S (%i)\n", iCurrentSrcNode);
			return QuickShotCreate(tr4.endpos, pCurrentSrcNode);
		}
		trace_t tr5;
		if (QuickPortalTrace(vecNodeCameraPos, vecNodeCameraPos + Vector(0, 0, -100), &tr5))
		{
			//Msg("FindPortalFromSubZone marker 6 - S (%i)\n", iCurrentSrcNode);
			return QuickShotCreate(tr5.endpos, pCurrentSrcNode);
		}
		//Msg("FindPortalFromSubZone marker 7 - FAILED (%i)\n", iCurrentSrcNode);
	}
	//If we get here, it's probaly impossible to make a portal from our current subzone, and we need to find a way to another one, or try something else entirely.
	chellNavQuery_t result;
	result.result = CNQR_NO;
	result.vecHypoPortalPos = Vector(0, 0, 0);
	return result;
}

CAI_Node *CNPC_Chell::MyNearestNode(bool bCheckVisibility)
{
	return NearestNodeToPoint(GetAbsOrigin() + Vector(0, 0, 32), bCheckVisibility, false);//Raise up testing spot from origin, or else we test straight at the feet and can miss nodes slightly below us
}

CAI_Node *CNPC_Chell::NearestNodeToPoint(const Vector &vPosition, bool bCheckVisibility, bool bDebug)
{
	return GetNavigator()->GetNetwork()->GetNode(GetNavigator()->GetNetwork()->NearestNodeToPoint(vPosition, bCheckVisibility, bDebug), false);
}

//
//Test from one subzone to another.
//
void CNPC_Chell::ProcessSubZoneLink(subZoneLink_t *szLink, Vector vecFilterFrom)
{
	model_t *mWorldFaces = gEntList.FindEntityByClassname(NULL, "worldspawn")->GetModel();

	//TESTING NEW PLANE DATA METHOD
	/*int modelIndex = gEntList.FindEntityByClassname(NULL, "worldspawn")->GetCollideable()->GetCollisionModelIndex();
	model_t* pModel = const_cast<model_t*>(modelinfo->GetModel(modelIndex));
	modelinfo->*/
	//TESTING
	const matrix3x4_t& mat = gEntList.FindEntityByClassname(NULL, "worldspawn")->EntityToWorldTransform();
	cplane_t localPlane;
	cplane_t worldPlane;
	Vector vecOrigin, vecWorld, vecDelta;
	//AngleVectors(QAngle(-90, 0, 0), &vecForward, &vecRight, &vecUp);
	//Find what portal surfaces are visible from the starting subzone that would lead to the other subzone.
	for (int iCurrentSrcNode = 0; iCurrentSrcNode < GetNavigator()->GetNetwork()->NumNodes(); iCurrentSrcNode++)
	{
		//Msg("ProcessSubZoneLink iterating on node %i, vec filter is %0.1f %0.1f %0.1f\n", iCurrentSrcNode, vecFilterFrom.x, vecFilterFrom.y, vecFilterFrom.z);
		//I took this plane stuff from c_func_reflective_glass.cpp.
		CAI_Node *pCurrentSrcNode = GetNavigator()->GetNetwork()->GetNode(iCurrentSrcNode);
		Vector vecNodeCameraPos = pCurrentSrcNode->GetOrigin() + Vector(0, 0, 64);

		//DON'T do this as it does not consider jumping.
		/*
		if (pCurrentSrcNode->GetSubZone() != m_pPortal2Node->GetSubZone())
		continue;*/

		//Nodes must be reachable from this vector.
		if (!vecFilterFrom.Length())
		{
			chellNavQuery_t resToI = ComplexNavTest(vecFilterFrom, pCurrentSrcNode->GetOrigin(), QF_SIMPLE_ONLY | QF_DONT_DETECT_CYCLE);
			if (!resToI.pResultWaypoint)
			{
				//Msg("ProcessSubZoneLink, node %i not reachable from %0.1f %0.1f %0.1f\n", iCurrentSrcNode, vecFilterFrom.x, vecFilterFrom.y, vecFilterFrom.z);
				continue;
			}
		}
		//Msg("ProcessSubZoneLink, node %i IS reachable from %0.1f %0.1f %0.1f\n", iCurrentSrcNode, vecFilterFrom.x, vecFilterFrom.y, vecFilterFrom.z);

		//Frustum_t frustum;
		//GeneratePerspectiveFrustum(vecNodeCameraPos, vecForward, vecRight, vecUp, 1, 99999, 360, 360, frustum);
		int iPlaneCount = modelinfo->GetBrushModelPlaneCount(mWorldFaces);
		for (int iCurrentPlane = 0; iCurrentPlane < iPlaneCount; iCurrentPlane++)
		{
			modelinfo->GetBrushModelPlane(mWorldFaces, iCurrentPlane, localPlane, &vecOrigin);
			MatrixTransformPlane(mat, localPlane, worldPlane);//Transform to world space
			VectorTransform(vecOrigin, mat, vecWorld);
			if (vecNodeCameraPos.Dot(worldPlane.normal) <= worldPlane.dist)//Check for view behind plane
				continue;
			VectorSubtract(vecWorld, vecNodeCameraPos, vecDelta);//Backface cull
			if (vecDelta.Dot(worldPlane.normal) >= 0)
				continue;

			//check if plane is in pvs of i node. there will be non-negligible false negatives. don't cull, just deprioritize
			/*static byte		pvs[MAX_MAP_CLUSTERS / 8];
			int clusterIndex = engine->GetClusterForOrigin(vecOrigin);
			engine->GetPVSForCluster(clusterIndex, sizeof(pvs), pvs);
			if (!engine->CheckBoxInPVS(vecNodeCameraPos, vecNodeCameraPos, pvs, sizeof(pvs)))
				continue;*/

			Vector vecCurrentTestPos = vecOrigin;
			trace_t tr;
			UTIL_TraceLine(vecCurrentTestPos + worldPlane.normal, vecCurrentTestPos - worldPlane.normal, MASK_SOLID, NULL, COLLISION_GROUP_NONE, &tr);
			if (tr.fraction != 1 && !IsNoPortalMaterial(tr.surface))
			{
				/*ScanPlane(worldPlane, tr.endpos, pCurrentSrcNode, szLink, 1, 1);
				if (szLink->info == SZLINK_YES)
				{
					return;//bail now to minimize lag
				}
				ScanPlane(worldPlane, tr.endpos, pCurrentSrcNode, szLink, -1, 1);
				if (szLink->info == SZLINK_YES)
				{
					return;//bail now to minimize lag
				}
				ScanPlane(worldPlane, tr.endpos, pCurrentSrcNode, szLink, 1, -1);
				if (szLink->info == SZLINK_YES)
				{
					return;//bail now to minimize lag
				}
				ScanPlane(worldPlane, tr.endpos, pCurrentSrcNode, szLink, -1, -1);
				if (szLink->info == SZLINK_YES)
				{
					return;//bail now to minimize lag
				}*/
				NDebugOverlay::Line(vecCurrentTestPos + worldPlane.normal, vecCurrentTestPos - worldPlane.normal, 255, 255, 255, false, 100);
			}
			else
			{
				NDebugOverlay::Line(vecCurrentTestPos + worldPlane.normal, vecCurrentTestPos - worldPlane.normal, 255, 0, 0, false, 100);
			}
			//If i subzone has a non-portal path to the end, we win.
			/*CUtlVector< subZoneLink_t *> szLinks;
			if (FindPortalRoute(pCurrentSrcNode, pEndNode, &szLinks).flPlacementSuccess == PORTAL_FIZZLE_SUCCESS)
			{

			}*/
		}//iterate on all planes
		//temp for debugging
		break;
	}//iterate on all nodes
}

//
//The dirty deed. Find a route from one subzone to another.
//
subZoneLink_t *CNPC_Chell::FindPortalRoute(CAI_Node *pStartNode, CAI_Node *pEndNode)
{
	//CUtlVector< portalShot_t > shots;
	CUtlVector< subZoneLink_t *> szLinks;
	if (PortalRouteRecursive(pStartNode->GetSubZone(), pEndNode, &szLinks))
	{
		subZoneLink_t *szLink = GetSZLink(pStartNode->GetSubZone(), pEndNode->GetSubZone());
		szLinks.AddToHead(szLink);
		for (int i = 0; i < szLinks.Count(); i++)
		{

		}
	}
	//temp TODO
	//else
	//{
		int linkIndex = m_SubZoneLinks.AddToTail();
		subZoneLink_t *link = m_SubZoneLinks[linkIndex];
		link->iSrcszID = pStartNode->GetSubZone();
		link->iDestszID = pEndNode->GetSubZone();
		link->info = SZLINK_NO;
		//m_SubZoneLinks.AddToTail(link);
		return link;
	//}
}

bool CNPC_Chell::PortalRouteRecursive(int srcSZID, CAI_Node *pEndNode, CUtlVector< subZoneLink_t *> *szLinks)
{
	for (int iCurrentSrcSubzone = AI_NODE_FIRST_ZONE; iCurrentSrcSubzone < GetNavigator()->GetNetwork()->NumSubZones(); iCurrentSrcSubzone++)
	{
		if (srcSZID == iCurrentSrcSubzone)
		{
			continue;
		}
		subZoneLink_t *szLink = GetSZLink(srcSZID, iCurrentSrcSubzone);
		if (szLink->info == SZLINK_YES)
		{
			if (iCurrentSrcSubzone == pEndNode->GetSubZone())
			{
				szLinks->AddToHead(szLink);
				return true;//We hit the end!
			}
			if (PortalRouteRecursive(iCurrentSrcSubzone, pEndNode, szLinks))
			{
				szLinks->AddToHead(szLink);
				return true;
			}
		}
	}
	return false;
}

//
//Grab the first node we can find that has a given subzone.
//I don't like this...
//
CAI_Node *CNPC_Chell::GetNodeInSZ(int id, int startID)
{
	for (int iCurrentSrcNode = startID; iCurrentSrcNode < GetNavigator()->GetNetwork()->NumNodes(); iCurrentSrcNode++)
	{
		CAI_Node *node = GetNavigator()->GetNetwork()->GetNode(iCurrentSrcNode);
		if (node->GetSubZone() == id)
			return node;
	}
	Warning("No nodes with that subzone!");
	return NULL;
}

//
//Scan a plane for portalability (after deducing that it is portalable)
//
void CNPC_Chell::ScanPlane(cplane_t worldPlane, Vector vecTestPos, CAI_Node *pCurrentSrcNode, subZoneLink_t *szLink, int x, int y)
{
	//Get vectors that are "facing" the plane.
	QAngle angPlane;
	Vector vecForward, vecRight, vecUp;
	VectorAngles(worldPlane.normal, angPlane);
	AngleVectors(angPlane, &vecForward, &vecRight, &vecUp);

	//Invert right and up if desired. If we don't do this it's possible to miss some wall segments
	vecRight *= x;
	vecUp *= y;

	//Test down to the left to see if we're on the top right of the wall.
	vecTestPos += (-vecRight * CHELL_PORTAL_FIND_WIDTH / 2) + (-vecUp * CHELL_PORTAL_FIND_HEIGHT / 2);
	float flInitialPlacementSuccess = VerifyPortalPlacement(NULL, vecTestPos, angPlane, PORTAL_PLACED_BY_PLAYER, true);
	if (flInitialPlacementSuccess != PORTAL_FIZZLE_SUCCESS)
	{
		return;//If not, we will eventually iterate on the top right.
	}

	float flPlacementDownSuccess = PORTAL_FIZZLE_SUCCESS;
	float flPlacementLeftSuccess = PORTAL_FIZZLE_SUCCESS;
	Vector vecTestPosRightest = vecTestPos;
	int iColumnLimit = 0;
	int iColumns = 1;
	while (flPlacementDownSuccess == PORTAL_FIZZLE_SUCCESS)//scan down
	{
		while (flPlacementLeftSuccess == PORTAL_FIZZLE_SUCCESS)//scan left
		{
			PortalTest(vecTestPos, pCurrentSrcNode, angPlane, flPlacementLeftSuccess, szLink);
			if (iColumnLimit == iColumns)
			{
				iColumns++;//Consistency with other branch exit.
				break;//Do not go out longer than our set column limit or else our imaginary plane is no longer rectangular.
			}
			//Next try testing one space to the left.
			vecTestPos += -vecRight * CHELL_PORTAL_FIND_WIDTH;
			iColumns++;
		}
		//Go back to the right side and test one row down.
		vecTestPosRightest.z -= CHELL_PORTAL_FIND_HEIGHT;
		vecTestPos = vecTestPosRightest;
		iColumns--;
		if (iColumnLimit == 0)
		{
			iColumnLimit = iColumns;//The column limit is determined by how long the first row is.
		}
		if (iColumnLimit < iColumns)
		{
			//Row shorter than the column limit, purge these results or else our imaginary plane is no longer rectangular.
			m_PortalShots.RemoveMultipleFromTail(iColumns);
		}
		else
		{
			//We had a full row, save results.
			for (int i = 0; i < iColumns; i++)
			{
				if (m_PortalShots.Element(i).pDestNode == NULL || m_PortalShots.Element(i).pDestNode->GetId() == -1)//didn't find a node in GetNodeNearPortal
					continue;
				subZoneLink_t *link = GetSZLink(pCurrentSrcNode->GetSubZone(), m_PortalShots.Element(i).pDestNode->GetSubZone());
				link->info = SZLINK_YES;
				link->shots.AddToTail(&m_PortalShots.Element(i));
			}
		}
		flPlacementLeftSuccess = PORTAL_FIZZLE_SUCCESS;
		iColumns = 1;
		PortalTest(vecTestPos, pCurrentSrcNode, angPlane, flPlacementDownSuccess, szLink);
	}
	//m_PortalShots.RemoveMultipleFromTail(1);//Take off last unsuccessful test for the next row down?
}

//
//Testing a single portal position from a single point, saving its result.
//
void CNPC_Chell::PortalTest(Vector vecTestPos, CAI_Node *pSrcNode, QAngle angPlane, float &flPlacementSuccess, subZoneLink_t *szLink)
{
	bool bFoundPlacementData = false;
	bool bLOS = true;
	//Do we know if a portal can go here?
	for (int iShot = 0; iShot < m_PortalShots.Count(); iShot++)
	{
		portalShot_t pShot = m_PortalShots.Element(iShot);
		if (vecTestPos == pShot.vecPos)
		{
			if (pShot.flPlacementSuccess != PORTAL_FIZZLE_UNKNOWN)
			{
				//we found an existing shot data point with the exact same position with a non-unknown result, so yes, we know it's either a yes or no
				bFoundPlacementData = true;
				flPlacementSuccess = pShot.flPlacementSuccess;
				break;
			}
		}
	}

	if (!bFoundPlacementData)
		flPlacementSuccess = VerifyPortalPlacement(NULL, vecTestPos, angPlane, PORTAL_PLACED_BY_PLAYER, true);

	if (flPlacementSuccess == PORTAL_FIZZLE_SUCCESS)
	{
		//fits, now see if we can actually make the shot from the node we're iterating on
		trace_t traceToNode;

		//In the distant future we will revisit this for the fan in escape 1, both because of crouching and because the shot will not always be perfectly clear.
		//could check bLOS before tracing but it's only one trace, how much could it cost? ten dollars?
		UTIL_TraceLine(vecTestPos, pSrcNode->GetOrigin() + Vector(0, 0, 64), MASK_SHOT_PORTAL, NULL, COLLISION_GROUP_NONE, &traceToNode);
		bLOS = traceToNode.fraction == 1;
		if (bLOS)
		{
			portalShot_t shot;
			shot.pSrcNode = pSrcNode;
			shot.pDestNode = GetNodeNearPortal(vecTestPos);
			shot.vecPos = vecTestPos;
			shot.bLOS = true;
			shot.flPlacementSuccess = flPlacementSuccess;
			if (!bFoundPlacementData)
				m_PortalShots.AddToTail(shot);
			//szLink->shots.AddToTail(shot);//moved to ScanPlane
			return;
		}
	}

	if (!bFoundPlacementData || !bLOS)//don't bloat list with duplicate data!
	{
		portalShot_t shot;
		shot.pSrcNode = pSrcNode;
		shot.pDestNode = GetNodeNearPortal(vecTestPos);
		shot.vecPos = vecTestPos;
		shot.bLOS = false;//technically a lie but it shouldn't matter
		shot.flPlacementSuccess = flPlacementSuccess;
		m_PortalShots.AddToTail(shot);
	}
}

//
//Try to find our top right corner.
//
Vector PortalTraceToTopRight(Vector vecStart, Vector vecRight, Vector vecUp, QAngle angPlane)
{
	int iTraceWidth = CHELL_PORTAL_FIND_WIDTH;
	int iTraceHeight = CHELL_PORTAL_FIND_HEIGHT;
	//Try going up first.
	while (iTraceHeight <= 16)
	{
		vecStart.z += iTraceHeight;
		float flPlacementSuccess = VerifyPortalPlacement(NULL, vecStart, angPlane, PORTAL_PLACED_BY_PLAYER, true);
		if (flPlacementSuccess != PORTAL_FIZZLE_SUCCESS)
		{
			//At the ceiling, start getting snug.
			iTraceHeight /= 2;
		}
	}
	//Now go right.
	while (iTraceWidth <= 16)
	{
		vecStart += vecRight * iTraceWidth;
		float flPlacementSuccess = VerifyPortalPlacement(NULL, vecStart, angPlane, PORTAL_PLACED_BY_PLAYER, true);
		if (flPlacementSuccess != PORTAL_FIZZLE_SUCCESS)
		{
			//At the edge, start getting snug.
			iTraceWidth /= 2;
		}
	}
	return vecStart;
}

//
//We got the coordinates of a portal, where do we end up outside of it?
//
CAI_Node *CNPC_Chell::GetNodeNearPortal(Vector vecPos)
{
	//TODO-12: One day in the far future we will have to account for flings.
	trace_t tr;
	UTIL_TraceLine(vecPos, vecPos - Vector(0, 0, 3000), MASK_PLAYERSOLID, NULL, COLLISION_GROUP_NONE, &tr);
	return NearestNodeToPoint(tr.endpos + Vector(0, 0, 64), true, false);
}

float CNPC_Chell::GetArriveTolerance()
{
	if (m_bPickingUpObject)
		return NPC_CHELL_NAV_ARRIVE_TOLERANCE_BOX;
	return NPC_CHELL_NAV_ARRIVE_TOLERANCE;
}

//
//Aiming is slower when looking closer to target
//
void CNPC_Chell::GetAimSpeed(float angDif, bool pitch)
{
	char szCommand[2048];
	Q_snprintf(szCommand, sizeof(szCommand), "cl_%sspeed %0.1f", pitch ? "pitch" : "yaw", pitch ? 
		angDif / GetAimSpeedFactor() : //pitch speed
		m_iForceCubeOut == DONT_WIGGLE ? angDif / GetAimSpeedFactor() : 200);//yaw speed
	SendCommand(szCommand);
}

CAI_Node *CNPC_Chell::GetWPNode(AI_Waypoint_t *pWaypoint)
{
	return GetNavigator()->GetNetwork()->GetNode(pWaypoint->iNodeID, false);
}

//
//Move mouse faster when doing certain things. e.g. normal speed takes weirdly long to grab a cube
//lower = faster
//
float CNPC_Chell::GetAimSpeedFactor()
{
	if (m_bPickingUpObject || m_bDroppingObject || m_bShootingPortal)
	{
		//Msg("GetAimSpeedFactor 0.15\n");
		return 0.15;
	}
	
	//Msg("1. %s\n", m_pHeldObject != NULL ? "TRUE" : "FALSE");
	//Msg("2. %s\n", m_pCurWaypoint ? "TRUE" : "FALSE");
	//if (m_pCurWaypoint)
	//{
	//	Msg("3. %s\n", m_pCurWaypoint->iNodeID > -10 ? "TRUE" : "FALSE");
	//	//Msg("4. %0.1f %0.1f %0.1f\n", m_pCurWaypoint->GetNext()->GetPos().x, m_pCurWaypoint->GetNext()->GetPos().y, m_pCurWaypoint->GetNext()->GetPos().z);
	//}
	//If we are holding something and are close to a portal, aim faster to ensure the object doesn't get caught on the wall near the portal.
	if (GetPlayerHeldEntity(m_pPlayer))
	{
		//We need to detect if we're going to go into a portal ahead of time. m_bCrossingPortal alone does not tell us soon enough to be reliable.
		bool bCrossingSoon = m_bCrossingPortal;
		bool bFoundPortal1 = false;
		//float flPathLength = 0;
		AI_Waypoint_t *waypoint = GetNavigator()->GetPath()->GetCurWaypoint();
		while (waypoint != NULL && waypoint->GetNext() != NULL && !bCrossingSoon && m_flStopReverseTime < gpGlobals->curtime)
		{
			//Msg("GetAimSpeedFactor loop 1 (%i)\n", waypoint->iNodeID);
			if (!bFoundPortal1)
				bFoundPortal1 = NearestNodeToPoint(waypoint->GetPos(), true, false)->GetId() == m_pPortal1Node->GetId();
			//Look for the node IDs we know are closest to portals
			//We have to check both of these because sometimes there will not be a waypoint on our side of the portal, as weird as that sounds.
			if (CheckCrossFuture(MyNearestNode(true), waypoint, true) || CheckCrossFuture(GetWPNode(waypoint), waypoint, true))
			{
				//We need to actually be fairly close to it.
				float flDistToPortal = (GetAbsOrigin() - (bFoundPortal1 ? m_hPortal1 : m_hPortal2)->GetAbsOrigin()).Length();
				//Msg("flDistToPortal = %0.1f\n", flDistToPortal);
				if (flDistToPortal < 300)
				{
					//Msg("GetAimSpeedFactor loop 3\n");
					bCrossingSoon = true;
				}
			}
			else
			{
				//Msg("GetAimSpeedFactor loop 4\n");
			}
			//flPathLength += (waypoint->GetPos() - waypoint->GetNext()->GetPos()).Length2D();
			waypoint = waypoint->GetNext();
		}
		if (bCrossingSoon)
		{
			//Msg("GetAimSpeedFactor 0.2 (about to go into portal)\n");
			m_bWalkReverse = true;	
			m_flStopReverseTime = gpGlobals->curtime + 0.05;
			return 0.2;
		}
	}
	else
	{
		if (m_flStopReverseTime < gpGlobals->curtime)
		{
			m_bWalkReverse = false;
		}
	}
	//Msg("GetAimSpeedFactor 0.45\n");
	return 0.45;
}

//
//Aiming is slower when looking closer to target
//
float CNPC_Chell::GetAimTolerance(float angDif, bool pitch)
{
	return 1;
}

//
//Polish: Sometimes it's better to look horizontally instead of the exact place we want to look. This will fix cases where chell
//looks up or down for pointlessly minute adjustments, or adjustments that shouldn't actually be made, e.g. small steps, probably jumping
//
float CNPC_Chell::GetLookGoalZ(float fDefaultZ)
{
	//Msg("GetLookGoalZ: %0.1f - %0.1f\n", m_pPlayer->EyePosition()[2], fDefaultZ);
	if (m_bPickingUpObject)
	{
		//Msg("Looking at pickup target\n\n");
		return m_ChellGoal->GetAbsOrigin().z;//look to thing we're picking up
	}
	if (GetPlayerHeldEntity(m_pPlayer) && GetPlayerHeldEntity(m_pPlayer) != m_ChellGoal)//Taking a cube somewhere
	{
		return m_pPlayer->EyePosition()[2];
	}
	if (abs(m_pPlayer->EyePosition()[2] - fDefaultZ) < 18)//18 = regular auto step size
	{
		return m_pPlayer->EyePosition()[2];
	}
	if (m_ChellGoal->GetMoveType() == MOVETYPE_VPHYSICS)//going to pick up a cube
	{
		return m_ChellGoal->GetAbsOrigin().z;
	}
	return fDefaultZ;
}

void CNPC_Chell::InputGoToEntity(inputdata_t &inputdata)
{
	//Msg("InputGoToEntity sending Chell to: %s\n", inputdata.value.String());
	m_strChellGoal = inputdata.value.String();
	//chellNavQuery_t navResult = ComplexNavTest(GetAbsOrigin(), gEntList.FindEntityByName(NULL, m_strChellGoal)->GetAbsOrigin(), true);
	//if (navResult.result == CNQR_ALLPOS)
	//{
	//	ExecuteShot(navResult.shot);
	//}
	ChellNavToEntity(inputdata.value.String(), "sent from input", 0);
}

void CNPC_Chell::ChellNavToEntity(const char *pName, const char *strReason, int flags, Vector vecHypoPortal)
{
	Msg("ChellNavToEntity: %s, %s\n", pName, strReason);
	if (m_ChellGoal)
	{
		Msg("Last goal was '%s'\n", STRING(m_ChellGoal->GetEntityName()));
		if (!(flags & QF_ALLOW_REDUNDANT_GOAL) && !strcmp(pName, STRING(m_ChellGoal->GetEntityName())))
		{
			return;//we are already going there!
		}
		if (GetPlayerHeldEntity(m_pPlayer) && !strcmp(pName, STRING(GetPlayerHeldEntity(m_pPlayer)->GetEntityName())))
		{
			Assert(0);//we are holding the goal! not valid!
			return;
		}
	}
	m_ChellGoal = gEntList.FindEntityByName(NULL, pName);
	if (!m_ChellGoal)
	{
		Warning("ChellNavToEntity didn't find goal!\n");
		return;
	}
	m_bCrossingPortal = false;
	m_ChellGoal->SetNavIgnore();//dont worry about being blocked by goal. duh!
	chellNavQuery_t navResult = ComplexNavTest(GetAbsOrigin(), m_ChellGoal->GetAbsOrigin(), flags, m_ChellGoal);
	if (navResult.result == CNQR_SIMPLE)
	{
		Msg("ChellNavToEntity simple\n");
	}
	if (navResult.result == CNQR_ALLPOS)
	{
		Msg("ChellNavToEntity allpos\n");
	}
	if (navResult.result == CNQR_NO)
	{
		Warning("Chell nav failed!\n");
		return;
	}
	if (navResult.pGoalEnt != m_ChellGoal)
	{
		if (navResult.pGoalEnt)
		{
			Msg("ChellNavToEntity actually going to %s\n", STRING(navResult.pGoalEnt->GetEntityName()));
			//go to this other place instead of original goal
			m_ChellGoal = navResult.pGoalEnt;
		}
		else
		{
			Msg("ChellNavToEntity actually going to chelldest_generic_a\n");
			//Make an info_target at the end of the route so we have somewhere to go
			CBaseEntity *pTarget = CreateEntityByName("info_target");
			pTarget->SetAbsOrigin(navResult.pResultWaypoint->GetLast()->GetPos());
			pTarget->SetName(MAKE_STRING("chelldest_generic_a"));
			m_ChellGoal = pTarget;
		}
	}
	m_vecWalkingStartPos = m_pPlayer->GetAbsOrigin();
	//CheckCross(MyNearestNode(true), GetWPNode(navResult.pResultWaypoint), false);
	CheckCrossFuture(MyNearestNode(true), navResult.pResultWaypoint, false);
	if (vecHypoPortal.Length())
		m_vecHypoPortalPos = vecHypoPortal;
	if (navResult.vecHypoPortalPos.Length())
		m_vecHypoPortalPos = navResult.vecHypoPortalPos;

	m_flCurrentPathLength = GetNavigator()->GetPath()->GetPathLength();
	//m_flEvaluateObstacleTime = gpGlobals->curtime + 0.1;//if we send multiple MoveTo calls in the same tick, everything goes weird

	if (navResult.pResultWaypoint->GetShot() && !m_bShootingPortal)
	{
		ExecuteShot(navResult.pResultWaypoint->GetShot());
	}
	else
	{
		//This tells the navigator to use the waypoints we already constructed. The premade ones have useful information like pShot which will be lost if we allow the pathfinder to make its own waypoints again. (plus it's just efficient!)
		//GetNavigator()->GetPath()->SetWaypoints(navResult.pResultWaypoint);//
		//ScheduledMoveToGoalEntity(SCHED_IDLE_WALK, m_ChellGoal, ACT_WALK);

		//The code below is the most shameful thing I have ever done
		//This code was nabbed from a few base AI functions because at least part of it somehow prevents an issue where chell wouldn't move. Some bits have been replaced with relevant bits from the result of ComplexNavTest
		//I do not know how much of it is necessary. I've tried removing bits, and the behavior becomes inconsistent with no apparent rhyme or reason.

		//CAI_BehaviorHost<BASE_NPC>::ScheduledMoveToGoalEntity

		ChangeBehaviorTo(NULL);

		//CAI_BaseNPC::ScheduledMoveToGoalEntity

		if (m_NPCState == NPC_STATE_NONE)
		{
			// More than likely being grabbed before first think. Set ideal state to prevent schedule stomp
			m_NPCState = m_IdealNPCState;
		}

		SetSchedule(SCHED_IDLE_WALK);

		SetGoalEnt(m_ChellGoal);

		// HACKHACK: Call through TranslateNavGoal to fixup this goal position
		// UNDONE: Remove this and have NPCs that need this functionality fix up paths in the 
		// movement system instead of when they are specified.
		AI_NavGoal_t goal(m_ChellGoal->GetAbsOrigin(), ACT_WALK, AIN_DEF_TOLERANCE, AIN_YAW_TO_DEST);

		TranslateNavGoal(m_ChellGoal, goal.dest);

		//CAI_Navigator::SetGoal

		unsigned flags = 0;
		// Queue this up if we're in the middle of a frame
		if (PostFrameNavigationSystem()->IsGameFrameRunning())
		{
			// Send off the query for queuing
			PostFrameNavigationSystem()->EnqueueEntityNavigationQuery(this, CreateFunctor(GetNavigator(), &CAI_Navigator::SetGoal, RefToVal(goal), flags));

			// Complete immediately if we're waiting on that
			// FIXME: This will probably cause a lot of subtle little nuisances...
			if ((flags & AIN_NO_PATH_TASK_FAIL) == 0 || IsCurTaskContinuousMove())
			{
				TaskComplete();
			}

			// For now, always succeed -- we need to deal with failures on the next frame
			return;// true;
		}

		CAI_Path *pPath = GetNavigator()->GetPath();

		GetNavigator()->OnNewGoal();

		// Clear out previous state
		if (flags & AIN_CLEAR_PREVIOUS_STATE)
			GetNavigator()->ClearPath();

		if (IsCurTaskContinuousMove() || ai_post_frame_navigation.GetBool())
			flags |= AIN_NO_PATH_TASK_FAIL;

		bool result = HACKFindPath(goal, flags, navResult.pResultWaypoint);

		if (result == false)
		{
			//Msg("Failed to pathfind to nav goal:\n" );
			//Msg("   Type:      %s\n", AIGetGoalTypeText( goal.type) );
			//Msg("   Dest:      %s\n", NavVecToString(goal.dest));
			//Msg("   Dest node: %p\n", goal.destNode);
			//Msg("   Target:    %p\n", goal.pTarget);

			if (flags & AIN_DISCARD_IF_FAIL)
				GetNavigator()->ClearPath();
		}
		else
		{
			//Msg("New goal set:\n");
			//Msg("   Type:         %s\n", AIGetGoalTypeText(goal.type));
			//Msg("   Dest:         %s\n", NavVecToString(goal.dest));
			//Msg("   Dest node:    %p\n", goal.destNode);
			//Msg("   Target:       %p\n", goal.pTarget);
			//Msg("   Tolerance:    %.1f\n", GetPath()->GetGoalTolerance());
			//Msg("   Waypoint tol: %.1f\n", GetPath()->GetWaypointTolerance());
			//Msg("   Activity:     %s\n", GetOuter()->GetActivityName(GetPath()->GetMovementActivity()));
			//Msg("   Arrival act:  %s\n", GetOuter()->GetActivityName(GetPath()->GetArrivalActivity()));
			//Msg("   Arrival seq:  %d\n", GetPath()->GetArrivalSequence());
			//Msg("   Goal dir:     %s\n", NavVecToString(GetPath()->GetGoalDirection(GetAbsOrigin())));

			// Set our ideal yaw. This has to be done *after* finding the path, 
			// because the goal position may not be set until then
			if (goal.flags & AIN_YAW_TO_DEST)
			{
				//Msg("   Yaw to dest\n");
				GetMotor()->SetIdealYawToTarget(pPath->ActualGoalPosition());
			}
			//Behave differently when chell, lest we may simplify over portal nodes (bad) at the very start of our path
			GetNavigator()->SimplifyPath(!IsPlayerPilot(), IsPlayerPilot() ? -1 : goal.maxInitialSimplificationDist);
		}
	}
}

bool CNPC_Chell::HACKFindPath(const AI_NavGoal_t &goal, unsigned flags, AI_Waypoint_t *pResultWaypoint)
{
	CAI_Path *pPath = GetNavigator()->GetPath();

	Remember(bits_MEMORY_TASK_EXPENSIVE);

	// Clear out previous state
	if (flags & AIN_CLEAR_PREVIOUS_STATE)
		pPath->Clear();
	else if (flags & AIN_CLEAR_TARGET)
		pPath->ClearTarget();

	// Set the activity
	if (goal.activity != AIN_DEF_ACTIVITY)
		pPath->SetMovementActivity(goal.activity);
	else if (pPath->GetMovementActivity() == ACT_INVALID)
		pPath->SetMovementActivity((GetState() == NPC_STATE_COMBAT) ? ACT_RUN : ACT_WALK);

	// Set the tolerance
	if (goal.tolerance == AIN_HULL_TOLERANCE)
		pPath->SetGoalTolerance(GetHullWidth());
	else if (goal.tolerance != AIN_DEF_TOLERANCE)
		pPath->SetGoalTolerance(goal.tolerance);
	else if (pPath->GetGoalTolerance() == 0)
		pPath->SetGoalTolerance(GetDefaultNavGoalTolerance());

	if (pPath->GetGoalTolerance() < 0.1)
		DevMsg(this, "Suspicious navigation goal tolerance specified\n");

	pPath->SetWaypointTolerance(GetHullWidth() * 0.5);

	pPath->SetGoalType(GOALTYPE_NONE); // avoids a spurious warning about setting the goal type twice
	pPath->SetGoalType(goal.type);
	pPath->SetGoalFlags(goal.flags);

	CBaseEntity *pPathTarget = goal.pTarget;
	if ((goal.type == GOALTYPE_TARGETENT) || (goal.type == GOALTYPE_ENEMY))
	{
		// Guarantee that the path target 
		if (goal.type == GOALTYPE_TARGETENT)
			pPathTarget = GetTarget();
		else
			pPathTarget = GetEnemy();

		Assert(goal.pTarget == AIN_DEF_TARGET || goal.pTarget == pPathTarget);

		// Set the goal offset
		if (goal.dest != AIN_NO_DEST)
			pPath->SetTargetOffset(goal.dest);

		// We're not setting the goal position here because
		// it's going to be computed + set in DoFindPath.
	}
	else
	{
		// When our goaltype is position based, we have to set
		// the goal position here because it won't get set during DoFindPath().
		if (goal.dest != AIN_NO_DEST)
			pPath->ResetGoalPosition(goal.dest);
		else if (goal.destNode != AIN_NO_NODE)
			pPath->ResetGoalPosition(GetNavigator()->GetNodePos(goal.destNode));
	}

	if (pPathTarget > AIN_DEF_TARGET)
	{
		pPath->SetTarget(pPathTarget);
	}

	pPath->ClearWaypoints();
	bool result = HACKFindPath((flags & AIN_NO_PATH_TASK_FAIL) == 0, false, pResultWaypoint);

	if (result == false)
	{
		if (flags & AIN_DISCARD_IF_FAIL)
			pPath->Clear();
		else
			pPath->SetGoalType(GOALTYPE_NONE);
	}
	else
	{
		if (goal.arrivalActivity != AIN_DEF_ACTIVITY && goal.arrivalActivity > ACT_RESET)
		{
			pPath->SetArrivalActivity(goal.arrivalActivity);
		}
		else if (goal.arrivalSequence != -1)
		{
			pPath->SetArrivalSequence(goal.arrivalSequence);
		}

		// Initialize goal facing direction
		// FIXME: This is a poor way to initialize the arrival direction, apps calling SetGoal() 
		//		  should do this themselves, and/or it should be part of AI_NavGoal_t
		// FIXME: A number of calls to SetGoal() building routes to their enemy but don't set the flags!
		if (goal.type == GOALTYPE_ENEMY)
		{
			pPath->SetGoalDirection(pPathTarget);
			pPath->SetGoalSpeed(pPathTarget);
		}
		else
		{
			pPath->SetGoalDirection(pPath->ActualGoalPosition() - GetAbsOrigin());
		}
	}

	return result;
}

bool CNPC_Chell::HACKFindPath(bool fSignalTaskStatus, bool bDontIgnoreBadLinks, AI_Waypoint_t *pResultWaypoint)
{
	// Test to see if we're resolving spiking problems via threading
	/*if (ai_navigator_generate_spikes.GetBool())
	{
		unsigned int nLargeCount = (INT_MAX >> (ai_navigator_generate_spikes_strength.GetInt()));
		while (nLargeCount--) {}
	}*/
	//not relevant?
	/*
	bool bRetrying = (HasMemory(bits_MEMORY_PATH_FAILED) && GetNavigator()->m_timePathRebuildMax != 0);
	if (bRetrying)
	{
		// If I've passed by fail time, fail this task
		if (GetNavigator()->m_timePathRebuildFail < gpGlobals->curtime)
		{
			if (fSignalTaskStatus)
			{
				GetNavigator()->OnNavFailed(FAIL_NO_ROUTE);
			}
			else
			{
				GetNavigator()->OnNavFailed();
			}
			return false;
		}
		else if (GetNavigator()->m_timePathRebuildNext > gpGlobals->curtime)
		{
			return false;
		}
	}*/

	bool bFindResult = HACKDoFindPath(pResultWaypoint);

	if (!bDontIgnoreBadLinks && !bFindResult && IsNavigationUrgent())
	{
		GetPathfinder()->SetIgnoreBadLinks();
		bFindResult = HACKDoFindPath(pResultWaypoint);
	}

	if (bFindResult)
	{
		Forget(bits_MEMORY_PATH_FAILED);

		if (fSignalTaskStatus)
		{
			TaskComplete();
		}
		return true;
	}
	//not relevant?
	/*
	if (GetNavigator()->m_timePathRebuildMax == 0)
	{
		if (fSignalTaskStatus)
			GetNavigator()->OnNavFailed(FAIL_NO_ROUTE);
		else
			GetNavigator()->OnNavFailed();
		return false;
	}
	else
	{

		Remember(bits_MEMORY_PATH_FAILED);
		GetNavigator()->m_timePathRebuildFail = gpGlobals->curtime + GetNavigator()->m_timePathRebuildMax;
		GetNavigator()->m_timePathRebuildNext = gpGlobals->curtime + GetNavigator()->m_timePathRebuildDelay;
		return false;
	}*/
	return true;
}

bool CNPC_Chell::HACKDoFindPath(AI_Waypoint_t *pResultWaypoint)
{
	AI_PROFILE_SCOPE(CAI_Navigator_DoFindPath);

	//Msg("Finding new path\n");

	GetNavigator()->GetPath()->ClearWaypoints();

	bool		returnCode;

	returnCode = false;

	switch (GetNavigator()->GetPath()->GoalType())
	{
	/*case GOALTYPE_PATHCORNER:
	{
		returnCode = GetNavigator()->DoFindPathToPathcorner(GetGoalEnt());
	}
	break;*/

	/*case GOALTYPE_ENEMY:
	{
		// NOTE: This is going to set the goal position, which was *not*
		// set in SetGoal for this movement type
		CBaseEntity *pEnemy = GetNavigator()->GetPath()->GetTarget();
		if (pEnemy)
		{
			Assert(pEnemy == GetEnemy());

			Vector newPos = GetEnemyLKP();

			float tolerance = GetNavigator()->GetPath()->GetGoalTolerance();
			float outerTolerance = GetDefaultNavGoalTolerance();
			if (outerTolerance > tolerance)
			{
				GetNavigator()->GetPath()->SetGoalTolerance(outerTolerance);
				tolerance = outerTolerance;
			}

			TranslateNavGoal(pEnemy, newPos);

			// NOTE: Calling reset here because this can get called
			// any time we have to update our goal position
			GetNavigator()->GetPath()->ResetGoalPosition(newPos);
			GetNavigator()->GetPath()->SetGoalTolerance(tolerance);

			returnCode = GetNavigator()->DoFindPathToPos();
		}
	}
	break;*/

	case GOALTYPE_LOCATION:
	case GOALTYPE_FLANK:
	case GOALTYPE_COVER:
		returnCode = HACKDoFindPathToPos(pResultWaypoint);
		break;

	/*case GOALTYPE_LOCATION_NEAREST_NODE:
	{
		int myNodeID;
		int destNodeID;

		returnCode = false;

		myNodeID = GetPathfinder()->NearestNodeToNPC();
		if (myNodeID != NO_NODE)
		{
			destNodeID = GetPathfinder()->NearestNodeToPoint(GetNavigator()->GetPath()->ActualGoalPosition());
			if (destNodeID != NO_NODE)
			{
				//AI_Waypoint_t *pRoute = GetPathfinder()->FindBestPath(myNodeID, destNodeID, false);//I don't know anything about this DoFindPath function...

				GetNavigator()->GetPath()->SetWaypoints(pResultWaypoint);
				GetNavigator()->GetPath()->SetLastNodeAsGoal(true);
				returnCode = true;
			}
		}
	}
	break;*/

	/*case GOALTYPE_TARGETENT:
	{
		// NOTE: This is going to set the goal position, which was *not*
		// set in SetGoal for this movement type
		CBaseEntity *pTarget = GetNavigator()->GetPath()->GetTarget();

		if (pTarget)
		{
			Assert(pTarget == GetTarget());

			// NOTE: Calling reset here because this can get called
			// any time we have to update our goal position

			Vector	initPos = pTarget->GetAbsOrigin();
			TranslateNavGoal(pTarget, initPos);

			GetNavigator()->GetPath()->ResetGoalPosition(initPos);
			returnCode = GetNavigator()->DoFindPathToPos();
		}
	}
	break;*/
	}
	
	return returnCode;
}

bool CNPC_Chell::HACKDoFindPathToPos(AI_Waypoint_t *pResultWaypoint)
{
	CAI_Path *		pPath = GetNavigator()->GetPath();
	CAI_Pathfinder *pPathfinder = GetPathfinder();
	const Vector &	actualGoalPos = pPath->ActualGoalPosition();
	CBaseEntity *	pTarget = pPath->GetTarget();
	float 			tolerance = pPath->GetGoalTolerance();
	Vector			origin;

	if (gpGlobals->curtime - GetNavigator()->m_flTimeClipped > 0.11 || GetNavigator()->m_bLastNavFailed)
	{
		//GetNavigator()->m_pClippedWaypoints->RemoveAll();
	}

	if (GetNavigator()->m_pClippedWaypoints->IsEmpty() || IsPlayerPilot())
	{
		origin = GetAbsOrigin();//PIN: this used to be GetLocalOrigin. I changed it because it caused chell's pathfinding to start at 0 0 0.
		//Catastrophe probability... low
		//Msg("CAI_Navigator::DoFindPathToPos ClippedWaypoints was empty, set origin to %0.1f %0.1f %0.1f\n", origin[0], origin[1], origin[2]);
	}
	else
	{
		AI_Waypoint_t *pLastClipped = GetNavigator()->m_pClippedWaypoints->GetLast();
		origin = pLastClipped->GetPos();
		//Msg("CAI_Navigator::DoFindPathToPos ClippedWaypoints was NOT empty, set origin to %0.1f %0.1f %0.1f\n", origin[0], origin[1], origin[2]);
	}

	GetNavigator()->m_bLastNavFailed = false;

	pPath->ClearWaypoints();

	AI_Waypoint_t *pFirstWaypoint = pPathfinder->BuildRoute(origin, actualGoalPos, pTarget, tolerance, GetNavigator()->m_bLocalSucceedOnWithinTolerance, GetNavType());

	if (!pFirstWaypoint)
	{
		Msg("FIRST WAYPOINT FAILED\n");
		//  Sorry no path
		return false;
	}

	pPath->SetWaypoints(pFirstWaypoint);

	//always false?
	/*if (ShouldTestFailPath())
	{
		pPath->ClearWaypoints();
		return false;
	}*/

	if (!GetNavigator()->m_pClippedWaypoints->IsEmpty())
	{
		AI_Waypoint_t *pFirstClipped = GetNavigator()->m_pClippedWaypoints->GetFirst();
		GetNavigator()->m_pClippedWaypoints->Set(NULL);
		pFirstClipped->ModifyFlags(bits_WP_DONT_SIMPLIFY, true);
		pPath->PrependWaypoints(pFirstClipped);
		pFirstWaypoint = pFirstClipped;
	}

	if (pFirstWaypoint->IsReducible() && pFirstWaypoint->GetNext() && pFirstWaypoint->GetNext()->NavType() == GetNavType() &&
		GetNavigator()->ShouldOptimizeInitialPathSegment(pFirstWaypoint))
	{
		// If we're seemingly beyond the waypoint, and our hull is over the line, move on
		const float EPS = 0.1;
		Vector vClosest;
		CalcClosestPointOnLineSegment(origin,
			pFirstWaypoint->GetPos(), pFirstWaypoint->GetNext()->GetPos(),
			vClosest);
		if ((pFirstWaypoint->GetPos() - vClosest).Length() > EPS &&
			(origin - vClosest).Length() < GetHullWidth() * 0.5)
		{
			pPath->Advance();
		}
	}

	return true;
}

void CNPC_Chell::InputLookAtEntity(inputdata_t &inputdata)
{
	Msg("InputLookAtEntity turning Chell to: %s\n", inputdata.value.String());
	EHANDLE pEntity = gEntList.FindEntityByName(NULL, MAKE_STRING(inputdata.value.String()));
	if (!pEntity)
	{
		Warning("InputLookAtEntity didn't find look goal!\n");
	}
	else
	{
		ChellLookAtPos(pEntity->GetAbsOrigin(), "sent from input");
	}
}

void CNPC_Chell::ChellLookAtPos(Vector pos, const char *strReason)
{
	//Msg("ChellLookAtPos: %0.1f, %0.1f, %0.1f %s\n", pos[0], pos[1], pos[2], strReason);
	m_LookGoal = pos;
}

void CNPC_Chell::InputWait(inputdata_t &inputdata)
{
	Wait(inputdata.value.Float());
}

void CNPC_Chell::Wait(float flTime)
{
	m_ChellGoal = NULL;
	m_pCurWaypoint = NULL;
	m_vecWaypointEyePos.Zero();
	m_flCurrentPathLength = 0;//or else passivefinalgoalseek will not work
	GetNavigator()->StopMoving();
	PlayerStopMoving();
	m_flWaitTime = gpGlobals->curtime + flTime;
}

//
//When moving to a place, try to move towards it and reorient our view at the same time, meaning we move in the direction that will direct us closest to
//the waypoint, even if that's not forward (it will be forward in a short while though) the alternative is to pause walking to wait for aiming to be ready.
//
void CNPC_Chell::UpdateMoveDirToWayPoint(Vector wayPoint)
{
	if (!wayPoint.Length() || !m_LookGoal.Length())//Questionable!
		return;

	if (m_bWalkReverse)
	{
		Msg("Moving in reverse so we can safely hold item thru portal\n");
		if (m_flCrossTime != 0 && gpGlobals->curtime < m_flCrossTime + 1)
			SendCommand("+back;-moveright;-forward;-moveleft;");
	}

	//HACK: tell player to keep going forward when we're right on top of our waypoint. otherwise, we send +back and it looks terrible
	if (m_angTargetView.y == 0)
	{
		//Msg("CHELL STEER: force forward\n");
		SendCommand(!m_bWalkReverse ? "+forward;-moveleft;-back;-moveright;" : "+back;-moveright;-forward;-moveleft;");
		return;
	}

	if (m_pCurWaypoint)
	{
		//Msg("Chell travelling to: %0.1f, %0.1f, %0.1f ID: %i\n", wayPoint[0], wayPoint[1], wayPoint[2], m_pCurWaypoint->iNodeID);
	}
	else
	{
		//Msg("Chell travelling to: %0.1f, %0.1f, %0.1f\n", wayPoint[0], wayPoint[1], wayPoint[2]);
	}

	//PlayerStopMoving();

	if (gpGlobals->curtime < m_flWaitTime)
	{
		PlayerStopMoving();
		return;
	}

	const char* directions[] = {
		"+forward;",//forward
		"+forward;+moveleft;",//forward and left
		"+moveleft;",//left
		"+moveleft;+back;",//left and back
		"+back;",//back
		"+back;+moveright;",//back and right
		"+moveright;",//right
		"+moveright;+forward;",//right and forward
		"+forward;" };//forward
	const char* directionsReverse[] = {
		"+back;",//forward (back)
		"+back;+moveright;",//forward and left (back and right)
		"+moveright;",//left (right)
		"+moveright;+forward;",//left and back (right and forward)
		"+forward;",//back (forward)
		"+forward;+moveleft;",//back and right (forward and left)
		"+moveleft;",//right (left)
		"+moveleft;+back;",//right and forward (left and forward)
		"+back;" };//forward (back)
	int index = ((((int)m_angTargetView.y - (int)m_pPlayer->EyeAngles().y % 360 + 360) % 360) + 22.5) / 45;
	//Msg("CHELL STEER: ((%0.1f - %0.1f) + 22.5) / 45 = %i %s\n", m_angTargetView.y, m_pPlayer->EyeAngles().y, index, !m_bWalkReverse ? directions[index] : directionsReverse[index]);

	if (!chell_plrctrl_walk.GetBool())
		return;

	SendCommand(!m_bWalkReverse ? directions[index] : directionsReverse[index]);

	const char* antidirections[] = {
		"-moveleft;-back;-moveright;",//forward
		"-back;-moveright;",//forward and left
		"-forward;-back;-moveright;",//left
		"-forward;-moveright;",//left and back
		"-forward;-moveleft;-moveright;",//back
		"-forward;-moveleft;",//back and right
		"-forward;-moveleft;-back;",//right
		"-moveleft;-back;",//right and forward
		"-moveleft;-back;-moveright;" };//forward
	const char* antidirectionsReverse[] = {
		"-moveright;-forward;-moveleft;",//forward (back)
		"-forward;-moveleft;",//forward and left (back and right)
		"-back;-forward;-moveleft;",//left (right)
		"-back;-moveleft;",//left and back (right and forward)
		"-back;-moveright;-moveleft;",//back (forward)
		"-back;-moveright;",//back and right (forward and left)
		"-back;-moveright;-forward;",//right (left)
		"-moveright;-forward;",//right and forward (left and forward)
		"-moveright;-forward;-moveleft;" };//forward (back)

	SendCommand(!m_bWalkReverse ? antidirections[index] : antidirectionsReverse[index]);
}

void CNPC_Chell::UpdatePortalCross()
{
	//We just crossed through a portal. We can't use the regular detection method because it doesn't fire to children of the crosser (the player)
	Warning("Chell found portal crossed\n");

	//Probably going to have to add some kind of context for this line.
	m_vecHypoPortalPos = Vector(0, 0, 0);

	m_bCrossingPortal = false;
	m_flCrossTime = gpGlobals->curtime;

	if (!m_bWalkReverse)
	{
		m_iForceCubeOut = DONT_WIGGLE;
	}

	//Remember the portal we last crossed.
	if (GetPlayerHeldEntity(m_pPlayer))//Helps with carrying objects through portals
	{
		float flBestPortalDist = 1000000;
		CBaseEntity *pPortalCand = gEntList.FindEntityByClassname(NULL, "prop_portal");
		while (pPortalCand)
		{
			float flPortalDist = (GetAbsOrigin() - pPortalCand->GetAbsOrigin()).Length();
			if (flPortalDist < flBestPortalDist)
			{
				m_pLastPortalCrossed = pPortalCand;
			}
			pPortalCand = gEntList.FindEntityByClassname(pPortalCand, "prop_portal");
		}
	}
	//We successfully went to one of our portal nodes... so act like it
	//very hacky imo
	//if (!GetNavigator()->GetPath()->IsEmpty())
	//{
		//Trying this to fix a strange issue where pathfinding tries to start below the floor in test chamber 01
	if (m_ChellGoal)
	{
		ChellNavToEntity(STRING(m_ChellGoal->GetEntityName()), "crossed portal", QF_ALLOW_REDUNDANT_GOAL);
	}
	//GetNavigator()->AdvancePath();//originally we just artificially advanced instead of restarting the path, but the new way is less buggy
	//m_LookGoal.Zero();
	m_vecWaypointEyePos.Zero();
	m_pCurWaypoint = NULL;
	m_pLastWaypoint = NULL;
	//}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
string_t CNPC_Chell::GetModelName() const
{
	string_t iszModelName = AllocPooledString("models/Humans/Group01/female_07.mdl");
	return iszModelName;
}

//-----------------------------------------------------------------------------
// Chell does not partake in such childish games
//-----------------------------------------------------------------------------
Class_T	CNPC_Chell::Classify()
{
	return CLASS_NONE;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CNPC_Chell::SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode )
{
//	switch( failedSchedule )
//	{
//
//	case SCHED_ESTABLISH_LINE_OF_FIRE_FALLBACK:
//
//		break;
//	}

	return BaseClass::SelectFailSchedule( failedSchedule, failedTask, taskFailCode );
}

float CNPC_Chell::GetDefaultNavGoalTolerance()
{
	return NPC_CHELL_NAV_ARRIVE_TOLERANCE;
}

void CNPC_Chell::NotifyNavZoneCross(int srcZone, int destZone, int srcSubZone, int destSubZone, bool bTestOnly)
{
	//Msg("CNPC_Chell::NotifyNavZoneCross\n");
}

//
//Try to recognize things potentially blocking our path! I say potentially because this can include basically any entity, even worldspawn
//This function is called from both ScanAhead() and moveprobe.
//
int CNPC_Chell::NotifyNavBlocker(CBaseEntity *pEnt)
{
	const char *strClassname = pEnt->GetClassname();
	if (FStrEq(strClassname, "player") || FStrEq(strClassname, "worldspawn"))
	{
		return SHOULDHIT_DEFAULT;//dont even bother showing the debug messages!
	}
	//Msg("CNPC_Chell::NotifyNavBlocker\n");
	
	//Msg("Blocker is a '%s' named '%s' with model '%s' at %0.1f, %0.1f, %0.1f.\n", strClassname, pEnt->GetDebugName(), pEnt->GetModelName(),
	//	pEnt->GetAbsOrigin().x, pEnt->GetAbsOrigin().y, pEnt->GetAbsOrigin().z);

	//elevator doors will always open as you approach
	if (FStrEq(STRING(pEnt->GetModelName()), "models/props/round_elevator_doors.mdl"))
	{
		return SHOULDHIT_NO;
	}

	//objects that have hierarchies in them may cause traces to be inconsistent in a way (!) so look for both the func_doors and prop_dynamics that make up test chamber doors
	CBaseEntity *pChild, *pNext;
	for (pChild = pEnt->FirstMoveChild(); pChild; pChild = pNext)
	{
		//Msg("CNPC_Chell::NotifyNavBlocker for loop\n");
		//test chamber door
		if (FStrEq(pChild->GetClassname(), "prop_dynamic") && FStrEq(STRING(pChild->GetModelName()), "models/props/door_01_lftdoor_reference.mdl"))
		{
			CBaseDoor *pFuncDoor = dynamic_cast<CBaseDoor*>(pEnt);
			if (pFuncDoor->m_toggle_state == TS_AT_TOP || pFuncDoor->m_toggle_state == TS_GOING_UP)
				return SHOULDHIT_DEFAULT;//This door is already open, don't worry about it
			CreateObstacle(pChild, "test_chamber_door");
			return SHOULDHIT_YES;
		}
		//wrong side of door, we prefer to look at the "left" door model
		if (FStrEq(pChild->GetClassname(), "prop_dynamic") && FStrEq(STRING(pChild->GetModelName()), "models/props/door_01_rtdoor_reference.mdl"))
		{
			CBaseEntity *pDoorCand = gEntList.FindEntityByClassname(NULL, "prop_dynamic");
			while (pDoorCand)
			{
				//Msg("CNPC_Chell::NotifyNavBlocker while loop\n");
				if (pDoorCand == pChild)
				{
					pDoorCand = gEntList.FindEntityByClassname(pDoorCand, "prop_dynamic");
					continue;
				}
				if (pDoorCand->GetAbsOrigin() == pChild->GetAbsOrigin())
				{
					//Same origin? we're two sides of the same coin... or door
					pChild = pDoorCand;
					break;
				}
				pDoorCand = gEntList.FindEntityByClassname(pDoorCand, "prop_dynamic");
			}
			CBaseDoor *pFuncDoor = dynamic_cast<CBaseDoor*>(pEnt);
			if (pFuncDoor->m_toggle_state == TS_AT_TOP || pFuncDoor->m_toggle_state == TS_GOING_UP)
				return SHOULDHIT_DEFAULT;//This door is already open, don't worry about it
			CreateObstacle(pChild, "test_chamber_door");
			return SHOULDHIT_YES;
		}
		pNext = pChild->NextMovePeer();
	}

	//test chamber door
	if (FStrEq(strClassname, "prop_dynamic") && FStrEq(STRING(pEnt->GetModelName()), "models/props/door_01_lftdoor_reference.mdl"))
	{
		CBaseDoor *pFuncDoor = dynamic_cast<CBaseDoor*>(pEnt->GetParent());
		if (pFuncDoor->m_toggle_state == TS_AT_TOP || pFuncDoor->m_toggle_state == TS_GOING_UP)
			return SHOULDHIT_DEFAULT;//This door is already open, don't worry about it
		CreateObstacle(pEnt, "test_chamber_door");
		return SHOULDHIT_YES;
	}
	//wrong side of door, we prefer to look at the "left" door model
	if (FStrEq(strClassname, "prop_dynamic") && FStrEq(STRING(pEnt->GetModelName()), "models/props/door_01_rtdoor_reference.mdl"))
	{
		CBaseEntity *pDoorCand = gEntList.FindEntityByClassname(NULL, "prop_dynamic");
		while (pDoorCand)
		{
			//Msg("CNPC_Chell::NotifyNavBlocker while loop 2\n");
			if (pDoorCand == pChild)
			{
				pDoorCand = gEntList.FindEntityByClassname(pDoorCand, "prop_dynamic");
				continue;
			}
			if (pDoorCand->GetAbsOrigin() == pEnt->GetAbsOrigin())
			{
				//Same origin? we're two sides of the same coin... or door
				pEnt = pDoorCand;
				break;
			}
			pDoorCand = gEntList.FindEntityByClassname(pDoorCand, "prop_dynamic");
		}
		CBaseDoor *pFuncDoor = dynamic_cast<CBaseDoor*>(pEnt->GetParent());
		if (pFuncDoor->m_toggle_state == TS_AT_TOP || pFuncDoor->m_toggle_state == TS_GOING_UP)
			return SHOULDHIT_DEFAULT;//This door is already open, don't worry about it
		CreateObstacle(pEnt, "test_chamber_door");
		return SHOULDHIT_YES;
	}
	return SHOULDHIT_DEFAULT;
}

void CNPC_Chell::CreateObstacle(CBaseEntity *pEnt, char *type)
{
	//Msg("Obstacle is a '%s' named '%s' with model '%s' at %0.1f, %0.1f, %0.1f.\n", pEnt->GetClassname(), pEnt->GetDebugName(), pEnt->GetModelName(),
	//	pEnt->GetAbsOrigin().x, pEnt->GetAbsOrigin().y, pEnt->GetAbsOrigin().z);
	for (int i = 0; i < m_Obstacles.Count(); i++)
	{
		//Msg("CNPC_Chell::CreateObstacle for loop\n");
		if (pEnt == m_Obstacles[i].pEnt)
		{
			//Msg("Obstacle already in list\n");
			return;//already in obstacle list
		}
	}
	obstacle_t newObst;
	newObst.pEnt = pEnt;
	newObst.type = type;
	newObst.iObstacleState = OBST_STATE_UNSOLVED;
	m_Obstacles.AddToTail(newObst);
}

bool CNPC_Chell::IsPlayerPilot()
{
	//Msg("CNPC_Chell::IsPlayerPilot\n");
	return true;
}

bool CNPC_Chell::IsPlayerPilot() const
{
	//Msg("CNPC_Chell::IsPlayerPilot\n");
	return true;
}

void CNPC_Chell::ChellCollide(int index, gamevcollisionevent_t *pEvent)
{
	//Warning("ChellCollide\n");
	if (pEvent->pEntities[!index] == m_ChellGoal && pEvent->pEntities[!index]->GetMoveType() == MOVETYPE_VPHYSICS && !m_bPickingUpObject)
	{
		Msg("Colliding and task status is %i\n", m_ScheduleState.fTaskStatus);
		m_pCurWaypoint = NULL;
		m_vecWaypointEyePos.Zero();
		m_hGoalEnt = NULL;
		m_flCurrentPathLength = 0;//This may cause problems later down the road, but it fixes a problem in test 00 and didn't affect 01.
		GetNavigator()->StopMoving(true);
		GetNavigator()->ClearGoal();
		GetNavigator()->GetPath()->ClearWaypoints();
		GetNavigator()->OnNavComplete();
		GetNavigator()->GetPath()->ClearWaypoints();
		m_bPickingUpObject = true;
		PlayerDuckEnd();
		ChellLookAtPos(pEvent->pEntities[!index]->GetAbsOrigin(), "picking up object");
	}
}

void CNPC_Chell::PilotJump()
{
	Warning("Jumping because Motor AI said to!\n");
	PlayerJump();
}

void CNPC_Chell::StartTask(const Task_t *pTask)
{
	//switch (pTask->iTask)
	//{
	//case TASK_CHELL_STEER_TO_PORTAL:

		//ChellLookAtPos();
	//	break;
	//}
}

int CNPC_Chell::SelectSchedule()
{
	if (!GetNavigator()->GetPath()->IsEmpty() && m_pCurWaypoint && m_pCurWaypoint->GetPos().Length())
	{
	}

	return BaseClass::SelectSchedule();
}

CON_COMMAND_F(contents_crosshair, "print crosshair content flags", FCVAR_CHEAT)
{
	Vector forward, up;
	UTIL_GetLocalPlayer()->EyeVectors(&forward, NULL, &up);
	trace_t tr;
	UTIL_TraceLine(UTIL_GetLocalPlayer()->EyePosition(), forward * 1000 + UTIL_GetLocalPlayer()->EyePosition(), MASK_SHOT, UTIL_GetLocalPlayer(), COLLISION_GROUP_DEBRIS, &tr);
	Msg("contents of (%0.1f %0.1f %0.1f): %i\n", tr.endpos.x, tr.endpos.y, tr.endpos.z, enginetrace->GetPointContents(tr.endpos + forward));
}

//CON_COMMAND_F(spew_subzones, "print subzone info", FCVAR_CHEAT)
//{
//	Msg("Number of subzones: %i\n", g_pAINetworkManager->GetNetwork()->NumSubZones());
//}

AI_BEGIN_CUSTOM_NPC(npc_chell, CNPC_Chell)

//DECLARE_CONDITION(COND_CHELL_CROSS_PORTAL)
//DECLARE_CONDITION(COND_CHELL_PICKING_UP_OBJECT)

//DEFINE_SCHEDULE
//(
//SCHED_CHELL_CROSS_PORTAL,

//"	Tasks"
//"		TASK_CHELL_STEER_TO_PORTAL		0"
//""
//"	Interrupts"
//)

AI_END_CUSTOM_NPC()