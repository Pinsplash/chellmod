//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The downtrodden chells of City 17. Timid when unarmed, they will
//			rise up against their Combine oppressors when given a weapon.
//
//=============================================================================//

#ifndef	NPC_CHELL_H
#define	NPC_CHELL_H

#include "npc_playercompanion.h"
#include "ai_utils.h"
//#include "portal_placement.cpp"
//

//-----------------------------------------------------------------------------
//
// CLASS: CNPC_Chell
// Please dont tell anyone how I live.
//-----------------------------------------------------------------------------
/*
//namespace
//{
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
//}
*/
extern bool IsNoPortalMaterial(const csurface_t &surface);
const float NPC_CHELL_NAV_ARRIVE_TOLERANCE = 16;

//Flags for ComplexNavTest
#define QF_NO_PORTAL_TRAVEL		1
#define QF_SIMPLE_ONLY			2
#define QF_DONT_DETECT_CYCLE	4
#define QF_ALLOW_REDUNDANT_GOAL	8
#define QF_DONT_GET_GUN			16
enum subzoneLinkState_t
{
	SZLINK_UNKNOWN = 0,
	SZLINK_NO = 1,
	SZLINK_YES = 2,
	//SZLINK_FAIL = 3,
};
class subZoneLink_t
{
public:
	int iSrcszID;
	int iDestszID;
	int info = SZLINK_UNKNOWN;
	CCopyableUtlVector< portalShot_t *> shots;
	subZoneLink_t(int iSrcszID, int iDestszID, int info, CCopyableUtlVector< portalShot_t *> initShots);
};
subZoneLink_t::subZoneLink_t(int initSrcszID, int initDestszID, int initInfo, CCopyableUtlVector< portalShot_t *> initShots)
	: shots(initShots)
{
	memset(this, 0, sizeof(*this));
	iSrcszID = initSrcszID;
	iDestszID = initDestszID;
	info = initInfo;
}
struct chellNavQuery_t
{
	AI_Waypoint_t *pResultWaypoint = NULL;//if result is 1 or 2, the waypoint to start at.
	int result;//navQueryResult_t
	//portalShot_t shot;
	bool bHasShot;//smart language
	Vector vecHypoPortalPos = Vector(0, 0, 0);
	CBaseEntity *pGoalEnt = NULL;
};
enum navQueryResult_t
{
	CNQR_NO,//Not possible to reach at all
	CNQR_SIMPLE,//Can be reached by simple nav (which inherently means ALLPOS too)
	CNQR_ALLPOS,//can only be reached by ALLPOS nav
};
struct obstacle_t
{
	CBaseEntity *pEnt;
	const char *type;
	int iObstacleState;
};
enum//wiggle directions god this is so dumb
{
	DONT_WIGGLE,
	WIGGLE_LEFT,
	WIGGLE_RIGHT,
};
class CNPC_Chell : public CNPC_PlayerCompanion
{
	DECLARE_CLASS(CNPC_Chell, CNPC_PlayerCompanion);
public:
	DECLARE_DATADESC();
	//basics
	~CNPC_Chell();
	void			Spawn();
	void			Precache();
	void			PostNPCInit();
	string_t 		GetModelName() const;
	Class_T 		Classify();
	int				SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode );
	int				SelectSchedule();
	void			Think();
	void			StartTask(const Task_t *pTask);

	//regular nav
	void			GetWaypointCurChell();
	void			MaintainPath();
	void			UpdatePortalNodeLinks();
	CAI_Link *		ForceNodeLink(CAI_Node *pNode1, CAI_Node *pNode2);
	CAI_Node *		GetNodeNearPortal(Vector vecPos);
	virtual void	PilotJump();
	float			GetArriveTolerance();
	CAI_Node *		GetWPNode(AI_Waypoint_t *pWaypoint);
	CAI_Node *		MyNearestNode(bool bCheckVisibility);
	CAI_Node *		NearestNodeToPoint(const Vector &vPosition, bool bCheckVisibility, bool bDebug);
	virtual void	NotifyNavZoneCross(int srcZone, int destZone, int srcSubZone, int destSubZone, bool bTestOnly);//if we know how to handle crossing zones, then we don't need node connections
	float			GetDefaultNavGoalTolerance();
	virtual bool	IsPlayerPilot();//Basically explicitly tell base ai we're chell to make exceptions
	virtual bool	IsPlayerPilot() const;//Basically explicitly tell base ai we're chell to make exceptions
	bool			HACKFindPath(const AI_NavGoal_t &goal, unsigned flags, AI_Waypoint_t *pResultWaypoint);
	bool			HACKFindPath(bool fSignalTaskStatus, bool bDontIgnoreBadLinks, AI_Waypoint_t *pResultWaypoint);
	bool			HACKDoFindPath(AI_Waypoint_t *pResultWaypoint);
	virtual bool	HACKDoFindPathToPos(AI_Waypoint_t *pResultWaypoint);
	AI_Waypoint_t *	m_pCurWaypoint;
	AI_Waypoint_t *	m_pLastWaypoint;
	CBaseEntity *	m_SavedGoal;//Temporarily save off m_ChellGoal so we can go to an even more immediate goal. Terrible idea.
	CBaseEntity *	m_ChellGoal;//Nav goal
	const char *	m_strChellGoal;//Name of final goal. We store this separately so that we can remember it after doing immediate goals of any kind. Only set by InputGoToEntity.
	Vector			m_vecWalkingStartPos;//HACK: Where we were when we started walking somewhere. Using this to fix a bug where chell inexplicably tries to go back to the place she was when she began walking
	float			m_flCurrentPathLength;//The length of our current route including what we already covered. This is a bandaid fix for some unknown thing gone awry that makes BuildRoute lead to crashes.

	//portal crossing
	bool			CheckCrossFuture(CAI_Node *pNode1, AI_Waypoint_t *pWaypoint, bool bTest);
	void			CrossPortal();
	CBaseEntity*	m_pLastPortalCrossed;
	float			m_flCrossTime;
	int				m_iForceCubeOut;
	Vector			m_vecHypoPortalPos;
	bool			m_bCrossingPortal;

	//special nav
	void			InputTestComplexNav(inputdata_t &inputdata);
	void			ChellNavToEntity(const char *pName, const char *strReason, int flags, Vector vecHypoPortal = Vector(0, 0, 0));
	void			PortalTest(Vector vecTestPos, CAI_Node *pSrcNode, QAngle angPlane, float &flPlacementSuccess, subZoneLink_t *szLink);
	void			ScanPlane(cplane_t worldPlane, Vector vecTestPos, CAI_Node *pCurrentSrcNode, subZoneLink_t *szLink, int x, int y);
	subZoneLink_t *	FindPortalRoute(CAI_Node *pStartNode, CAI_Node *pEndNode);
	bool			PortalRouteRecursive(int srcSZID, CAI_Node *pEndNode, CUtlVector< subZoneLink_t *> *szLinks);
	CAI_Node *		GetNodeInSZ(int id, int startID = 0);
	subZoneLink_t *	GetSZLink(int iSrcSubZone, int iDestSubZone);
	void			ProcessSubZoneLink(subZoneLink_t *szLink, Vector vecFilterFrom);
	bool			QuickPortalTrace(Vector vecStart, Vector vecEnd, trace_t *tr);
	chellNavQuery_t QuickShotCreate(Vector pos, CAI_Node *pSrcNode = NULL);
	chellNavQuery_t FindPortalShotsFromAutoGun(CBaseEntity *pGun, Vector vecEnd);
	chellNavQuery_t ComplexNavTest(Vector vecStart, Vector vecEnd, int flags, CBaseEntity *pTarget = NULL, float flGoalTolerance = NPC_CHELL_NAV_ARRIVE_TOLERANCE);
	chellNavQuery_t PortalChangeNavTest(Vector vecStart, Vector vecEnd, bool bAllowPortalTravel, int iGunInfo, int flags, CBaseEntity *pTarget = NULL, float flGoalTolerance = NPC_CHELL_NAV_ARRIVE_TOLERANCE);
	CBaseEntity *	FindUnheldPortalGun();
	chellNavQuery_t	FindPortalSpotWithinSubZone(int iSubZone);
	int				m_iGunInfo;//State of portal gun.
	CUtlVector< portalShot_t > m_PortalShots;
	CUtlVector< subZoneLink_t *> m_SubZoneLinks;
	enum GunState_t
	{
		GUN_NONE,
		GUN_BLUE,
		GUN_ORANGE,
		GUN_BOTH,
	};

	//player
	void			UpdateMoveDirToWayPoint(Vector);
	void			SendCommand(const char *command);
	void			PlayerUseImpulse(char *type);
	void			PlayerUsePickUp(char *type);
	void			PlayerUseDrop(char *type);
	virtual void	UpdateGunKnowledge(CBaseCombatWeapon *pWeapon);
	virtual void	ChellCollide(int index, gamevcollisionevent_t *pEvent);
	CBasePlayer	*	m_pPlayer;
	
	//mouse
	void			SteerIncremental();
	void			ChellLookAtPos(Vector pos, const char *strReason);
	void			InputLookAtEntity(inputdata_t &inputdata);
	void			PlayerStopAiming();
	void			PlayerLookUp(float angDif);
	void			PlayerLookDown(float angDif);
	void			PlayerLookLeft(float angDif);
	void			PlayerLookRight(float angDif);
	void			PlayerShootBlue();
	void			PlayerShootOrange();
	void			GetAimSpeed(float angDif, bool pitch);
	float			GetAimSpeedFactor();
	float			GetLookGoalZ(float fDefaultZ);
	float			GetAimTolerance(float angDif, bool pitch);
	Vector			m_LookGoal;//Look in this direction. Might be at an entity or an arbitrary point on a wall.
	Vector			m_vecWaypointEyePos;//Where we should look in order to face forward
	bool			m_bLookedToCurWaypoint;
	bool			m_bPitchOkay;
	bool			m_bYawOkay;
	QAngle			m_angTargetView;//Direction we want to look in as an angle

	//movement
	virtual void	UpdatePortalCross();
	void			InputGoToEntity(inputdata_t &inputdata);
	void			PlayerStopMoving();
	void			PlayerJump();
	void			PlayerDuckStart();
	void			PlayerDuckEnd();
	void			JumpForSpeed();
	bool			m_bWalkReverse;
	float			m_flStopReverseTime;
	bool			m_bDucked;//If we are holding duck key. Includes state of going to ducked position, but not standing back up.

	//puzzle solving logic
	void			EvaluateObstacle();
	void			FindObstacles();
	bool			DetectCycle();
	void			PassiveFinalGoalSeek();
	void			CreateObstacle(CBaseEntity *pEnt, char *type);
	virtual int		NotifyNavBlocker(CBaseEntity *pEnt);//let the NPC intelligently remove the blocker based on AI
	bool			ScanAheadForBlockers(AI_Waypoint_t *waypoint);
	void			SolveDoor(obstacle_t *obst);
	CBaseEntity *	FindButtonForDoor(CBaseDoor *pFuncDoor);
	CBaseEntity *	FindTriggerForDoor(CBaseDoor *pFuncDoor);
	CBaseEntity *	FindWeightForButton();
	void			ApproachCubeForPickup(CBaseEntity *pBox);
	void			ApproachButtonWithCube(CBaseEntity *pBoxTrigger);
	void			ExecuteShot(portalShot_t *shot);
	CBaseEntity *	FindStaticPortal();
	CUtlVector< CBaseEntity* > m_pCyclePortals;
	CUtlVector< obstacle_t > m_Obstacles;
	CBaseEntity*	m_pStaticPortal;
	float			m_flLastCyclePortalTime;
	float			m_flEvaluateObstacleTime;
	bool			m_bLastBoxFindFailed;
	float			m_flCycleTime;
	enum ObstacleState_t
	{
		OBST_STATE_UNSOLVED,
		OBST_STATE_SOLVING,
		OBST_STATE_SOLVED,
	};

	//context
	bool			m_bPickingUpObject;
	bool			m_bDroppingObject;
	bool			m_bInCycle;
	bool			m_bShootingPortal;

	//chell misc
	void			InputWait(inputdata_t &inputdata);
	void			Wait(float flTime);
	float			m_flWaitTime;//When to quit waiting
	enum
	{
		//COND_CHELL_CROSS_PORTAL = BaseClass::NEXT_CONDITION,
		//COND_CHELL_PICKING_UP_OBJECT,
		SCHED_CHELL_SHOOT_PORTAL = BaseClass::NEXT_SCHEDULE,
		//TASK_CHELL_STEER_TO_PORTAL = BaseClass::NEXT_TASK,
	};
	bool m_bHACK_ok;//temporary hack boolean for debugging purposes
	float m_flHACK_time;//temporary hack time float for debugging purposes
	DEFINE_CUSTOM_AI;
};
#endif//NPC_CHELL_H
