//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "cbase.h"

#include "modelentities.h"
#include "iservervehicle.h"
#include "movevars_shared.h"

#include "ai_moveprobe.h"

#include "ai_basenpc.h"
#include "ai_routedist.h"
#include "props.h"
#include "vphysics/object_hash.h"
#include "prop_portal.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#undef LOCAL_STEP_SIZE
// FIXME: this should be based in their hull width
#define	LOCAL_STEP_SIZE	16.0 // 8 // 16

// If set to 1, results will be drawn for moveprobes done by player-selected NPCs
ConVar	ai_moveprobe_debug( "ai_moveprobe_debug", "0" );
ConVar	ai_moveprobe_jump_debug( "ai_moveprobe_jump_debug", "0" );
ConVar	ai_moveprobe_usetracelist( "ai_moveprobe_usetracelist", "0" );

ConVar	ai_strong_optimizations_no_checkstand( "ai_strong_optimizations_no_checkstand", "0" );

#ifdef DEBUG
ConVar ai_old_check_stand_position( "ai_old_check_stand_position", "0" );
#define UseOldCheckStandPosition() (ai_old_check_stand_position.GetBool())
#else
#define UseOldCheckStandPosition() (false)
#endif

//-----------------------------------------------------------------------------

// We may be able to remove this, but due to certain collision
// problems on displacements, and due to the fact that CheckStep is currently
// being called from code outside motor code, we may need to give it a little 
// room to avoid boundary condition problems. Also note that this will
// cause us to start 2*EPSILON above the ground the next time that this
// function is called, but for now, that appears to not be a problem.
float MOVE_HEIGHT_EPSILON = 0.0625f;

CON_COMMAND( ai_set_move_height_epsilon, "Set how high AI bumps up ground walkers when checking steps" )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	if ( args.ArgC() > 1 )
	{
		float newEps = atof( args[1] );
		if ( newEps >= 0.0  && newEps < 1.0 )
		{
			MOVE_HEIGHT_EPSILON = newEps;
		}
		Msg( "Epsilon now %f\n", MOVE_HEIGHT_EPSILON );
	}
}

//-----------------------------------------------------------------------------

BEGIN_SIMPLE_DATADESC(CAI_MoveProbe)
	//					m_pTraceListData (not saved, a cached item)
	DEFINE_FIELD( m_bIgnoreTransientEntities,		FIELD_BOOLEAN ),
	DEFINE_FIELD( m_hLastBlockingEnt,				FIELD_EHANDLE ),

END_DATADESC();


//-----------------------------------------------------------------------------
// Categorizes the blocker and sets the appropriate bits
//-----------------------------------------------------------------------------
AIMoveResult_t AIComputeBlockerMoveResult( CBaseEntity *pBlocker )
{
	if (pBlocker->MyNPCPointer())
		return AIMR_BLOCKED_NPC;
	else if (pBlocker->entindex() == 0)
		return AIMR_BLOCKED_WORLD;
	return AIMR_BLOCKED_ENTITY;
}

//-----------------------------------------------------------------------------
bool CAI_MoveProbe::ShouldBrushBeIgnored( CBaseEntity *pEntity )
{
	if ( pEntity->m_iClassname == g_iszFuncBrushClassname )
	{
		CFuncBrush *pFuncBrush = assert_cast<CFuncBrush *>(pEntity);

		// this is true if my class or entity name matches the exclusion name on the func brush
#if HL2_EPISODIC
		bool nameMatches = ( pFuncBrush->m_iszExcludedClass == GetOuter()->m_iClassname ) || GetOuter()->NameMatches(pFuncBrush->m_iszExcludedClass);
#else	// do not match against entity name in base HL2 (just in case there is some case somewhere that might be broken by this)
		bool nameMatches = ( pFuncBrush->m_iszExcludedClass == GetOuter()->m_iClassname );
#endif

		// return true (ignore brush) if the name matches, or, if exclusion is inverted, if the name does not match
		return ( pFuncBrush->m_bInvertExclusion ? !nameMatches : nameMatches );
	}

	return false;
}

//-----------------------------------------------------------------------------

void CAI_MoveProbe::TraceLine( const Vector &vecStart, const Vector &vecEnd, unsigned int mask, 
							   bool bUseCollisionGroup, trace_t *pResult ) const
{
	int collisionGroup 	= (bUseCollisionGroup) ? 
							GetCollisionGroup() : 
							COLLISION_GROUP_NONE;

	CTraceFilterNav traceFilter( const_cast<CAI_BaseNPC *>(GetOuter()), m_bIgnoreTransientEntities, GetOuter(), collisionGroup );

	AI_TraceLine( vecStart, vecEnd, mask, &traceFilter, pResult );

#ifdef _DEBUG
	// Just to make sure; I'm not sure that this is always the case but it should be
	if (pResult->allsolid)
	{
		Assert( pResult->startsolid );
	}
#endif
}

//-----------------------------------------------------------------------------

CAI_MoveProbe::CAI_MoveProbe(CAI_BaseNPC *pOuter)
 : 	CAI_Component( pOuter ),
	m_bIgnoreTransientEntities( false ),
	m_pTraceListData( NULL )
{
}

//-----------------------------------------------------------------------------

CAI_MoveProbe::~CAI_MoveProbe()
{
	delete m_pTraceListData;
}

//-----------------------------------------------------------------------------

void CAI_MoveProbe::TraceHull( 
	const Vector &vecStart, const Vector &vecEnd, const Vector &hullMin, 
	const Vector &hullMax, unsigned int mask, trace_t *pResult ) const
{
	AI_PROFILE_SCOPE( CAI_MoveProbe_TraceHull );
	//Assert(0);
	CTraceFilterNav traceFilter( const_cast<CAI_BaseNPC *>(GetOuter()), m_bIgnoreTransientEntities, GetOuter(), GetCollisionGroup() );

	Ray_t ray;
	ray.Init( vecStart, vecEnd, hullMin, hullMax );

	if ( !m_pTraceListData || m_pTraceListData->IsEmpty() )
		enginetrace->TraceRay( ray, mask, &traceFilter, pResult );
	else
	{
		enginetrace->TraceRayAgainstLeafAndEntityList( ray, *(const_cast<CAI_MoveProbe *>(this)->m_pTraceListData), mask, &traceFilter, pResult );
#if 0
		trace_t verificationTrace;
		enginetrace->TraceRay( ray, mask, &traceFilter, &verificationTrace );
		Assert( fabsf(verificationTrace.fraction - pResult->fraction) < 0.01 &&
				VectorsAreEqual( verificationTrace.endpos, pResult->endpos, 0.01 ) &&
				verificationTrace.m_pEnt == pResult->m_pEnt );

#endif
	}

	//if ( r_visualizetraces.GetBool() )
	//DebugDrawLine( pResult->startpos, pResult->endpos, 255, 255, 0, true, 10.0f );

	//NDebugOverlay::SweptBox( vecStart, vecEnd, hullMin, hullMax, vec3_angle, 255, 255, 0, 0, 10 );
	// Just to make sure; I'm not sure that this is always the case but it should be
	Assert( !pResult->allsolid || pResult->startsolid );
}

//-----------------------------------------------------------------------------

void CAI_MoveProbe::TraceHull( const Vector &vecStart, const Vector &vecEnd, 
	unsigned int mask, trace_t *pResult ) const
{
	TraceHull( vecStart, vecEnd, WorldAlignMins(), WorldAlignMaxs(), mask, pResult);
}

//-----------------------------------------------------------------------------

void CAI_MoveProbe::SetupCheckStepTraceListData( const CheckStepArgs_t &args ) const
{
	if ( ai_moveprobe_usetracelist.GetBool() )
	{
		Ray_t ray;
		Vector hullMin = WorldAlignMins();
		Vector hullMax = WorldAlignMaxs();

		hullMax.z += MOVE_HEIGHT_EPSILON;
		hullMin.z -= MOVE_HEIGHT_EPSILON;

		hullMax.z += args.stepHeight;
		hullMin.z -= args.stepHeight;

		if ( args.groundTest != STEP_DONT_CHECK_GROUND )
			hullMin.z -= args.stepHeight;

		hullMax.x += args.minStepLanding;
		hullMin.x -= args.minStepLanding;

		hullMax.y += args.minStepLanding;
		hullMin.y -= args.minStepLanding;

		Vector vecEnd;
		Vector2DMA( args.vecStart.AsVector2D(), args.stepSize, args.vecStepDir.AsVector2D(), vecEnd.AsVector2D() );
		vecEnd.z = args.vecStart.z;

		ray.Init( args.vecStart, vecEnd, hullMin, hullMax );

		if ( !m_pTraceListData )
		{
			const_cast<CAI_MoveProbe *>(this)->m_pTraceListData = new CTraceListData;
		}
		enginetrace->SetupLeafAndEntityListRay( ray, *(const_cast<CAI_MoveProbe *>(this)->m_pTraceListData) );
	}
}

//-----------------------------------------------------------------------------
// CheckStep() is a fundamentally 2D operation!	vecEnd.z is ignored.
// We can step up one StepHeight or down one StepHeight from vecStart
//-----------------------------------------------------------------------------
//bool g_bAIDebugStep = false;

bool CAI_MoveProbe::CheckStep( const CheckStepArgs_t &args, CheckStepResult_t *pResult ) const
{
	//Msg("CAI_MoveProbe::CheckStep marker 1\n");
	AI_PROFILE_SCOPE( CAI_MoveProbe_CheckStep );

	//bool bIsNearAPortal = IsNearAPortal();
	Vector vecBlockedNear;
	Vector vecEnd;
	unsigned collisionMask = args.collisionMask;
	VectorMA( args.vecStart, args.stepSize, args.vecStepDir, vecEnd );
	
	pResult->endPoint = args.vecStart;
	pResult->fStartSolid = false;
	pResult->hitNormal = vec3_origin;
	pResult->pBlocker = NULL;

	// This is fundamentally a 2D operation; we just want the end
	// position in Z to be no more than a step height from the start position
	Vector stepStart( args.vecStart.x, args.vecStart.y, args.vecStart.z + MOVE_HEIGHT_EPSILON );
	Vector stepEnd( vecEnd.x, vecEnd.y, args.vecStart.z + MOVE_HEIGHT_EPSILON );

	if (ai_moveprobe_debug.GetBool())
	{
		NDebugOverlay::Line( stepStart, stepEnd, 255, 255, 255, true, 5 );
		NDebugOverlay::Cross3D( stepEnd, 32, 255, 255, 255, true, 5 );
	}

	trace_t trace;

	AI_PROFILE_SCOPE_BEGIN( CAI_Motor_CheckStep_Forward );

	TraceHull( stepStart, stepEnd, collisionMask, &trace );

	if ((trace.startsolid || (trace.fraction < 1)))//PIN: Chell does not care about blockers
	{
		//Msg("CAI_MoveProbe::CheckStep marker 2\n");
		if (!GetOuter()->IsPlayerPilot())//Avoid assert below
		{
			//Msg("CAI_MoveProbe::CheckStep marker 2a\n");
			// Either the entity is starting embedded in the world, or it hit something.
			// Raise the box by the step height and try again
			trace_t stepTrace;

			if (!trace.startsolid)
			{
				//Msg("CAI_MoveProbe::CheckStep marker 3\n");
				if (ai_moveprobe_debug.GetBool())
				{
					NDebugOverlay::Box(trace.endpos, WorldAlignMins(), WorldAlignMaxs(), 64, 64, 64, 0, 5);
				}

				// Advance to first obstruction point
				stepStart = trace.endpos;

				// Trace up to locate the maximum step up in the space
				Vector stepUp(stepStart);
				stepUp.z += args.stepHeight;
				TraceHull(stepStart, stepUp, collisionMask, &stepTrace);

				if (ai_moveprobe_debug.GetBool())
				{
					NDebugOverlay::Box(stepTrace.endpos, WorldAlignMins(), WorldAlignMaxs(), 96, 96, 96, 0, 5);
				}

				stepStart = stepTrace.endpos;
			}
			else
			{
				//Msg("CAI_MoveProbe::CheckStep marker 4\n");
				stepStart.z += args.stepHeight;
			}

			// Now move forward
			stepEnd.z = stepStart.z;

			TraceHull(stepStart, stepEnd, collisionMask, &stepTrace);
			bool bRejectStep = false;

			// Ok, raising it didn't work; we're obstructed
			if (stepTrace.startsolid || stepTrace.fraction <= 0.01)
			{
				//Msg("CAI_MoveProbe::CheckStep marker 5\n");
				// If started in solid, and never escaped from solid, bail
				if (trace.startsolid)
				{
					//Msg("CAI_MoveProbe::CheckStep marker 6\n");
					pResult->fStartSolid = true;
					pResult->pBlocker = trace.m_pEnt;
					pResult->hitNormal = trace.plane.normal;
					return false;
				}

				bRejectStep = true;
			}
			else
			{
				//Msg("CAI_MoveProbe::CheckStep marker 7\n");
				if (ai_moveprobe_debug.GetBool())
				{
					NDebugOverlay::Box(stepTrace.endpos, WorldAlignMins(), WorldAlignMaxs(), 128, 128, 128, 0, 5);
				}

				// If didn't step forward enough to qualify as a step, try as if stepped forward to
				// confirm there's potentially enough space to "land"
				float landingDistSq = (stepEnd.AsVector2D() - stepStart.AsVector2D()).LengthSqr();
				float requiredLandingDistSq = args.minStepLanding*args.minStepLanding;
				if (landingDistSq < requiredLandingDistSq)
				{
					//Msg("CAI_MoveProbe::CheckStep marker 8\n");
					trace_t landingTrace;
					Vector stepEndWithLanding;

					VectorMA(stepStart, args.minStepLanding, args.vecStepDir, stepEndWithLanding);
					TraceHull(stepStart, stepEndWithLanding, collisionMask, &landingTrace);
					if (landingTrace.fraction < 1)
					{
						//Msg("CAI_MoveProbe::CheckStep marker 9\n");
						if (ai_moveprobe_debug.GetBool())
						{
							NDebugOverlay::Box(landingTrace.endpos, WorldAlignMins() + Vector(0, 0, 0.1), WorldAlignMaxs() + Vector(0, 0, 0.1), 255, 0, 0, 0, 5);
						}

						bRejectStep = true;
						if (landingTrace.m_pEnt)
						{
							//Msg("CAI_MoveProbe::CheckStep marker 10\n");
							pResult->pBlocker = landingTrace.m_pEnt;
							vecBlockedNear = landingTrace.endpos;
						}
					}
				}
				else if ((stepTrace.endpos.AsVector2D() - stepStart.AsVector2D()).LengthSqr() < requiredLandingDistSq)
				{
					//Msg("CAI_MoveProbe::CheckStep marker 11\n");
					if (ai_moveprobe_debug.GetBool())
					{
						NDebugOverlay::Box(stepTrace.endpos, WorldAlignMins() + Vector(0, 0, 0.1), WorldAlignMaxs() + Vector(0, 0, 0.1), 255, 0, 0, 0, 5);
					}

					bRejectStep = true;
				}
			}

			// If trace.fraction == 0, we fall through and check the position
			// we moved up to for suitability. This allows for sub-step
			// traces if the position ends up being suitable
			if (!bRejectStep)
			{
				//Msg("CAI_MoveProbe::CheckStep marker 12\n");
				trace = stepTrace;
			}

			if (trace.fraction < 1.0)
			{
				//Msg("CAI_MoveProbe::CheckStep marker 13\n");
				if (!pResult->pBlocker)
				{
					//Msg("CAI_MoveProbe::CheckStep marker 14\n");
					pResult->pBlocker = trace.m_pEnt;
					vecBlockedNear = trace.endpos;
				}
				pResult->hitNormal = trace.plane.normal;
			}
		}//Avoid assert below
		stepEnd = trace.endpos;//this was always here
	}

	AI_PROFILE_SCOPE_END();

	AI_PROFILE_SCOPE_BEGIN( CAI_Motor_CheckStep_Down );
	// seems okay, now find the ground
	// The ground is only valid if it's within a step height of the original position
	Assert( VectorsAreEqual( trace.endpos, stepEnd, 1e-3 ) );
	stepStart = stepEnd; 
	stepEnd.z = args.vecStart.z - args.stepHeight * args.stepDownMultiplier - MOVE_HEIGHT_EPSILON;

	TraceHull(stepStart, stepEnd, collisionMask, &trace);
	if (ai_moveprobe_debug.GetBool())
	{
		NDebugOverlay::Box(trace.endpos, WorldAlignMins(), WorldAlignMaxs(), 255, 0, 255, 0, 5);
	}

	// in empty space, lie and say we hit the world
	if (trace.fraction == 1.0f && !GetOuter()->IsPlayerPilot())//PIN: Chell dont caare. Somehow, this trace returns empty on the test chamber 01 elevator.
	//This change fixes it. It should never be a problem for Chell if the trace comes back correctly anyway, because long story short, we always know what we're doing.
	{
		//Msg("CAI_MoveProbe::CheckStep marker 15 stepStart %0.1f %0.1f %0.1f stepEnd %0.1f %0.1f %0.1f \n",
		//	stepStart.x, stepStart.y, stepStart.z, stepEnd.x, stepEnd.y, stepEnd.z);
		if (ai_moveprobe_debug.GetBool())
		{
			NDebugOverlay::Box(trace.endpos, WorldAlignMins(), WorldAlignMaxs(), 255, 0, 0, 0, 5);
		}

		Assert( pResult->endPoint == args.vecStart );
		if ( const_cast<CAI_MoveProbe *>(this)->GetOuter()->GetGroundEntity() )
		{
			//Msg("CAI_MoveProbe::CheckStep marker 16\n");
			pResult->pBlocker = const_cast<CAI_MoveProbe *>(this)->GetOuter()->GetGroundEntity();
			vecBlockedNear = trace.endpos;
		}
		else
		{
			//Msg("CAI_MoveProbe::CheckStep marker 17\n");
			pResult->pBlocker = GetContainingEntity( INDEXENT(0) );
			vecBlockedNear = trace.endpos;
		}
		return false;
	}

	if (ai_moveprobe_debug.GetBool())
	{
		NDebugOverlay::Box( trace.endpos, WorldAlignMins(), WorldAlignMaxs(), 160, 160, 160, 0, 5 );
	}

	AI_PROFILE_SCOPE_END();

	// Checks to see if the thing we're on is a *type* of thing we
	// are capable of standing on. Always true for our current ground ent
	// otherwise we'll be stuck forever
	if (trace.m_pEnt)
	{
		//Msg("trace.m_pEnt exists\n");

		if (trace.m_pEnt != GetOuter()->GetGroundEntity())
		{
			//Msg("trace.m_pEnt is not the ground entity\n");
		}
		else
		{
			//Msg("trace.m_pEnt is the ground entity\n");
		}
		if (CanStandOn(trace.m_pEnt))
		{
			//Msg("Can stand on trace.m_pEnt\n");
		}
		else
		{
			//Msg("Cannot stand on trace.m_pEnt\n");
		}
	}
	else
	{
		//Msg("trace.m_pEnt does not exist\n");
	}

	CBaseEntity *pFloor = trace.m_pEnt;
	if (pFloor != GetOuter()->GetGroundEntity() && (!pFloor || !CanStandOn(pFloor) ) )
	{
		//Warning("CAI_MoveProbe::CheckStep marker 18 - trying to go on something unstandable\n");// (%s %s %s)\n", pFloor != GetOuter()->GetGroundEntity() ? "TRUE" : "FALSE", !pFloor ? "TRUE" : "FALSE", !CanStandOn(pFloor) ? "TRUE" : "FALSE");
		if (ai_moveprobe_debug.GetBool())
		{
			NDebugOverlay::Cross3D(trace.endpos, 32, 255, 0, 0, true, 5);
		}

		Assert( pResult->endPoint == args.vecStart );
		pResult->pBlocker = pFloor;
		//Msg("Blocker name: '%s' class: '%s' origin: %0.1f %0.1f %0.1f blocked near: %0.1f %0.1f %0.1f\n",
		//	pResult->pBlocker->GetEntityName(), pResult->pBlocker->GetClassname(), pResult->pBlocker->GetAbsOrigin().x, pResult->pBlocker->GetAbsOrigin().y, pResult->pBlocker->GetAbsOrigin().z, trace.endpos.x, trace.endpos.y, trace.endpos.z);
		//Msg("Blocker name: '%s'\n", pFloor->GetEntityName());
		//Msg("Blocker class: '%s'\n", pFloor->GetClassname());
		//Msg("Blocker origin: %0.1f %0.1f %0.1f\n", pResult->pBlocker->GetAbsOrigin().x, pResult->pBlocker->GetAbsOrigin().y, pResult->pBlocker->GetAbsOrigin().z);
		//Msg("blocked near: %0.1f %0.1f %0.1f\n", trace.endpos.x, trace.endpos.y, trace.endpos.z);
		return false;
	}

	// Don't step up onto an odd slope
	if ( trace.endpos.z - args.vecStart.z > args.stepHeight * 0.5 &&
		 ( ( pFloor->IsWorld() && trace.hitbox > 0 ) ||
		   dynamic_cast<CPhysicsProp *>( pFloor ) ) )
	{
		//Msg("CAI_MoveProbe::CheckStep marker 19\n");
		if ( fabsf( trace.plane.normal.Dot( Vector(1, 0, 0) ) ) > .4 )
		{
			//Msg("CAI_MoveProbe::CheckStep marker 20\n");
			Assert( pResult->endPoint == args.vecStart );
			pResult->pBlocker = pFloor;
			vecBlockedNear = trace.endpos;

			if (ai_moveprobe_debug.GetBool())
			{
				NDebugOverlay::Cross3D(trace.endpos, 32, 0, 0, 255, true, 5);
			}
			return false;
		}
	}

	if (args.groundTest != STEP_DONT_CHECK_GROUND)
	{
		//Msg("CAI_MoveProbe::CheckStep marker 21\n");
		AI_PROFILE_SCOPE( CAI_Motor_CheckStep_Stand );
		// Next, check to see if we can *geometrically* stand on the floor
		bool bIsFloorFlat = CheckStandPosition( trace.endpos, collisionMask );
		if (args.groundTest != STEP_ON_INVALID_GROUND && !bIsFloorFlat)
		{
			//Msg("CAI_MoveProbe::CheckStep marker 22\n");
			pResult->pBlocker = pFloor;
			vecBlockedNear = trace.endpos;

			if (ai_moveprobe_debug.GetBool())
			{
				NDebugOverlay::Cross3D( trace.endpos, 32, 255, 0, 255, true, 5 );
			}
			return false;
		}
		// If we started on shaky ground (namely, it's not geometrically ok),
		// then we can continue going even if we remain on shaky ground.
		// This allows NPCs who have been blown into an invalid area to get out
		// of that invalid area and into a valid area. As soon as we're in
		// a valid area, though, we're not allowed to leave it.
	}

	// Return a point that is *on the ground*
	// We'll raise it by an epsilon in check step again
	pResult->endPoint = trace.endpos;
	pResult->endPoint.z += MOVE_HEIGHT_EPSILON; // always safe because always stepped down at least by epsilon

	if (ai_moveprobe_debug.GetBool())
	{
		NDebugOverlay::Cross3D(trace.endpos, 32, 0, 255, 0, true, 5);
	}

	//Msg("CAI_MoveProbe::CheckStep marker 23 (%s)\n", (pResult->pBlocker == NULL) ? "no blocker" : "blocker found");
	if (pResult->pBlocker)
	{
		//Msg("Blocker name: '%s' class: '%s' origin: %0.1f %0.1f %0.1f blocked near: %0.1f %0.1f %0.1f\n",
		//	pResult->pBlocker->GetEntityName(), pResult->pBlocker->GetClassname(), pResult->pBlocker->GetAbsOrigin().x, pResult->pBlocker->GetAbsOrigin().y, pResult->pBlocker->GetAbsOrigin().z, vecBlockedNear.x, vecBlockedNear.y, vecBlockedNear.z);
	}
	return ( pResult->pBlocker == NULL ); // totally clear if pBlocker is NULL, partial blockage otherwise
}

//-----------------------------------------------------------------------------
// Checks a ground-based movement
// NOTE: The movement will be based on an *actual* start position and
// a *desired* end position; it works this way because the ground-based movement
// is 2 1/2D, and we may end up on a ledge above or below the actual desired endpoint.
//-----------------------------------------------------------------------------
bool CAI_MoveProbe::TestGroundMove( const Vector &vecActualStart, const Vector &vecDesiredEnd, 
	unsigned int collisionMask, float pctToCheckStandPositions, unsigned flags, AIMoveTrace_t *pMoveTrace )
{
	bool bIsNearAPortal = IsNearAPortal();

	//Msg("CAI_MoveProbe::TestGroundMove started from %0.1f %0.1f %0.1f to %0.1f %0.1f %0.1f\n", vecActualStart.x, vecActualStart.y, vecActualStart.z, vecDesiredEnd.x, vecDesiredEnd.y, vecDesiredEnd.z);
	AIMoveTrace_t ignored;
	if (!pMoveTrace)
	{
		//Msg("CAI_MoveProbe::TestGroundMove marker 2 - using default movetrace\n");
		pMoveTrace = &ignored;
	}
	// Set a reasonable default set of values
	pMoveTrace->flDistObstructed = 0.0f;
	pMoveTrace->pObstruction 	 = NULL;
	pMoveTrace->vHitNormal		 = vec3_origin;
	pMoveTrace->fStatus 		 = AIMR_OK;
	pMoveTrace->vEndPosition 	 = vecActualStart;
	pMoveTrace->flStepUpDistance = 0;

	Vector vecMoveDir;
	pMoveTrace->flTotalDist = ComputePathDirection( NAV_GROUND, vecActualStart, vecDesiredEnd, &vecMoveDir );
	if (pMoveTrace->flTotalDist == 0.0f)
	{
		//Msg("CAI_MoveProbe::TestGroundMove marker 3 - success because at goal\n");
		return true;
	}
	
	// If it starts hanging over an edge, tough it out until it's not
	// This allows us to blow an NPC in an invalid region + allow him to walk out
	StepGroundTest_t groundTest;
	if ( (flags & AITGM_IGNORE_FLOOR) || pctToCheckStandPositions < 0.001 )
	{
		//Msg("CAI_MoveProbe::TestGroundMove marker 4 - using Ignore_Floor flag or it's technically fine to ignore floor?\n");
		groundTest = STEP_DONT_CHECK_GROUND;
		pctToCheckStandPositions = 0; // AITGM_IGNORE_FLOOR always overrides pct
	}
	else
	{
		//Msg("CAI_MoveProbe::TestGroundMove marker 5 - not using Ignore_Floor flag and we need to pay attention to stepping?\n");
		if (pctToCheckStandPositions > 99.999)
		{
			//Msg("CAI_MoveProbe::TestGroundMove marker 6 - just a sanity check?\n");
			pctToCheckStandPositions = 100;
		}

		if ((flags & AITGM_IGNORE_INITIAL_STAND_POS) || CheckStandPosition(vecActualStart, collisionMask))
		{
			//Msg("CAI_MoveProbe::TestGroundMove marker 7 - using Ignore_Initial_Stand_Pos flag? or had a good CheckStandPosition?\n");
			groundTest = STEP_ON_VALID_GROUND;
		}
		else
		{
			//Warning("CAI_MoveProbe::TestGroundMove marker 8 - not using Ignore_Initial_Stand_Pos flag? and had a bad CheckStandPosition? which means we're on invalid ground\n");
			groundTest = STEP_ON_INVALID_GROUND;
		}
	}

	if ( ( flags & AITGM_DRAW_RESULTS ) && !CheckStandPosition(vecActualStart, collisionMask) )
	{
		NDebugOverlay::Cross3D( vecActualStart, 16, 128, 0, 0, true, 10 );
	}

	//  Take single steps towards the goal
	float distClear = 0;
	int i;

	CheckStepArgs_t checkStepArgs;
	CheckStepResult_t checkStepResult;

	checkStepArgs.vecStart				= vecActualStart;
	checkStepArgs.vecStepDir			= vecMoveDir;
	checkStepArgs.stepSize				= 0;
	checkStepArgs.stepHeight			= StepHeight();
	checkStepArgs.stepDownMultiplier	= GetOuter()->GetStepDownMultiplier();
	checkStepArgs.minStepLanding		= GetHullWidth() * 0.3333333;
	checkStepArgs.collisionMask			= collisionMask;
	checkStepArgs.groundTest			= groundTest;

	checkStepResult.endPoint = vecActualStart;
	checkStepResult.hitNormal = vec3_origin;
	checkStepResult.pBlocker = NULL;
	
	float distStartToIgnoreGround = (pctToCheckStandPositions == 100) ? pMoveTrace->flTotalDist : pMoveTrace->flTotalDist * ( pctToCheckStandPositions * 0.01);
	bool bTryNavIgnore = ((vecActualStart - GetAbsOrigin()).Length2DSqr() < 0.1 && fabsf(vecActualStart.z - GetAbsOrigin().z) < checkStepArgs.stepHeight * 0.5);

	CUtlVector<CBaseEntity *> ignoredEntities;

	for (;;)
	{
		//Msg("CAI_MoveProbe::TestGroundMove marker 10 - start of for loop\n");
		float flStepSize = MIN( LOCAL_STEP_SIZE, pMoveTrace->flTotalDist - distClear );
		if (flStepSize < 0.001)
		{
			//Msg("CAI_MoveProbe::TestGroundMove marker 11 - step size is minimal so don't bother? %f, %f, %f\n", flStepSize, pMoveTrace->flTotalDist, distClear);
			break;
		}

		checkStepArgs.stepSize = flStepSize;
		if (distClear - distStartToIgnoreGround > 0.001)
		{
			//Msg("CAI_MoveProbe::TestGroundMove marker 12 - We don't need to worry about steps cause we aren't next to the step yet?\n");
			checkStepArgs.groundTest = STEP_DONT_CHECK_GROUND;
		}

		Assert( !m_pTraceListData || m_pTraceListData->IsEmpty() );
		SetupCheckStepTraceListData( checkStepArgs );
		
		for ( i = 0; i < 16; i++ )
		{
			//Msg("CAI_MoveProbe::TestGroundMove marker 13 - inner for loop i = %i\n", i);
			CheckStep( checkStepArgs, &checkStepResult );

			if ((!bTryNavIgnore || !checkStepResult.pBlocker || !checkStepResult.fStartSolid) && !bIsNearAPortal)
			{
				//Msg("CAI_MoveProbe::TestGroundMove marker 14 - Not ignoring nav-ignored objects or we don't have a blocker or we aren't stuck in something, and we're not near a portal, so break for loop, %s, %s, %s\n", !bTryNavIgnore ? "TRUE" : "FALSE", !checkStepResult.pBlocker ? "TRUE" : "FALSE", !checkStepResult.fStartSolid ? "TRUE" : "FALSE");
				break;//PIN: try not breaking here, may help with portal navigation
			}

			if (checkStepResult.pBlocker && checkStepResult.pBlocker->GetMoveType() != MOVETYPE_VPHYSICS && !checkStepResult.pBlocker->IsNPC() && !bIsNearAPortal)
			{
				//Warning("CAI_MoveProbe::TestGroundMove marker 15 - we have a blocker which isn't vphysics or an NPC, and we're not near a portal, so break for loop\n");
				break;//PIN: try not breaking here, may help with portal navigation
			}

			// Only permit pass through of objects initially embedded in
			if ( vecActualStart != checkStepArgs.vecStart )
			{
				//Msg("CAI_MoveProbe::TestGroundMove marker 16 - vector start places differed, stop ignoring nav-ignored objects and break for loop\n");
				bTryNavIgnore = false;
				break;
			}

			// Only allow move away from physics objects
			if (checkStepResult.pBlocker && checkStepResult.pBlocker->GetMoveType() == MOVETYPE_VPHYSICS)
			{
				//Msg("CAI_MoveProbe::TestGroundMove marker 17 - we have a blocker which is vphysics\n");
				Vector vMoveDir = vecDesiredEnd - vecActualStart;
				VectorNormalize( vMoveDir );

				Vector vObstacleDir = (checkStepResult.pBlocker->WorldSpaceCenter() - GetOuter()->WorldSpaceCenter() );
				VectorNormalize( vObstacleDir );

				if (vMoveDir.Dot(vObstacleDir) >= 0)
				{
					//Msg("CAI_MoveProbe::TestGroundMove marker 18 - blocker affects our path? so break for loop\n");
					break;
				}
			}

			if (checkStepResult.pBlocker && (flags & AITGM_DRAW_RESULTS) && checkStepResult.fStartSolid && checkStepResult.pBlocker->IsNPC())
			{
				//NDebugOverlay::EntityBounds( GetOuter(), 0, 0, 255, 0, 10 );
				NDebugOverlay::EntityBounds( checkStepResult.pBlocker, 255, 0, 0, 0, 10 );
			}

			if (checkStepResult.pBlocker)
			{
				//Msg("CAI_MoveProbe::TestGroundMove marker 19 - we have a blocker... add it to our list of nav-ignored objects\n");
				ignoredEntities.AddToTail(checkStepResult.pBlocker);
				checkStepResult.pBlocker->SetNavIgnore();
			}
		}

		//Msg("CAI_MoveProbe::TestGroundMove marker 20 - left inner for loop\n");
		ResetTraceListData();
		
		if ( flags & AITGM_DRAW_RESULTS )
		{
			if ( !CheckStandPosition(checkStepResult.endPoint, collisionMask) )
			{
				NDebugOverlay::Box( checkStepResult.endPoint, WorldAlignMins(), WorldAlignMaxs(), 255, 0, 0, 0, 10 );
				NDebugOverlay::Cross3D( checkStepResult.endPoint, 16, 255, 0, 0, true, 10 );
			}
			else
			{
				NDebugOverlay::Box( checkStepResult.endPoint, WorldAlignMins(), WorldAlignMaxs(), 0, 255, 0, 0, 10 );
				NDebugOverlay::Cross3D( checkStepResult.endPoint, 16, 0, 255, 0, true, 10 );
			}
		}

		// If we're being blocked by something, move as close as we can and stop
		if (checkStepResult.pBlocker && !bIsNearAPortal)
		{
			//Msg("CAI_MoveProbe::TestGroundMove marker 21 - we have a blocker and we aren't near a portal, so get as close as possible and break for loop\n");
			distClear += (checkStepResult.endPoint - checkStepArgs.vecStart).Length2D();
			if (GetOuter()->m_bSolveBlockers)
			{
				GetOuter()->NotifyNavBlocker(checkStepResult.pBlocker);
			}
			break;//PIN: try not breaking here, may help with portal navigation
		}
		
		float dz = checkStepResult.endPoint.z - checkStepArgs.vecStart.z;
		if ( dz < 0 )
		{
			//Msg("CAI_MoveProbe::TestGroundMove marker 22 - not important?\n");
			dz = 0;
		}
		
		pMoveTrace->flStepUpDistance += dz;
		distClear += flStepSize;
		checkStepArgs.vecStart = checkStepResult.endPoint;
	}

	//Msg("CAI_MoveProbe::TestGroundMove marker 23 - left big for loop\n");
	for ( i = 0; i < ignoredEntities.Count(); i++ )
	{
		//Msg("CAI_MoveProbe::TestGroundMove marker 24 - No longer ignoring nav-ignored entities\n");
		ignoredEntities[i]->ClearNavIgnore();
	}

	pMoveTrace->vEndPosition = checkStepResult.endPoint;
	
	if (checkStepResult.pBlocker && !bIsNearAPortal)//PIN: chell does not care about blockers
	{
		//Msg("CAI_MoveProbe::TestGroundMove marker 25 - failed because path obstructed, not near a portal\n");
		pMoveTrace->pObstruction	 = checkStepResult.pBlocker;
		pMoveTrace->vHitNormal		 = checkStepResult.hitNormal;
		pMoveTrace->fStatus			 = AIComputeBlockerMoveResult( checkStepResult.pBlocker );
		pMoveTrace->flDistObstructed = pMoveTrace->flTotalDist - distClear;

		if ( flags & AITGM_DRAW_RESULTS )
		{
			NDebugOverlay::Box( checkStepResult.endPoint, WorldAlignMins(), WorldAlignMaxs(), 255, 0, 0, 0, 10 );
		}

		return false;
	}

	// FIXME: If you started on a ledge and ended on a ledge, 
	// should it return an error condition (that you hit the world)?
	// Certainly not for Step(), but maybe for GroundMoveLimit()?
	
	// Make sure we actually made it to the target position 
	// and not a ledge above or below the target.
	if (!(flags & AITGM_2D))
	{
		//Msg("CAI_MoveProbe::TestGroundMove marker 26 - not using 2d movement flag\n");
		float threshold = MAX(  0.5f * GetHullHeight(), StepHeight() + 0.1 );
		if (fabs(pMoveTrace->vEndPosition.z - vecDesiredEnd.z) > threshold)
		{
			//Msg("CAI_MoveProbe::TestGroundMove marker 27 - failed because we went somewhere above or below our goal (%0.1f %0.1f %0.1f) instead of the actual place (%0.1f %0.1f %0.1f)\n", pMoveTrace->vEndPosition.x, pMoveTrace->vEndPosition.y, pMoveTrace->vEndPosition.z, vecDesiredEnd.x, vecDesiredEnd.y, vecDesiredEnd.z);
			if (ai_moveprobe_debug.GetBool())
			{
				NDebugOverlay::Cross3D(vecDesiredEnd, 8, 0, 255, 0, false, 10);
				NDebugOverlay::Cross3D(pMoveTrace->vEndPosition, 8, 255, 0, 0, false, 10);
			}
			// Ok, we ended up on a ledge above or below the desired destination
			pMoveTrace->pObstruction = GetContainingEntity( INDEXENT(0) );
			pMoveTrace->vHitNormal	 = vec3_origin;
			pMoveTrace->fStatus = AIMR_BLOCKED_WORLD;
			pMoveTrace->flDistObstructed = ComputePathDistance( NAV_GROUND, pMoveTrace->vEndPosition, vecDesiredEnd );
			return false;
		}
	}

	//Msg("CAI_MoveProbe::TestGroundMove marker 28 - success generic\n");
	return true;
}

//-----------------------------------------------------------------------------
// Tries to generate a route from the specified start to end positions
// Will return the results of the attempt in the AIMoveTrace_t structure
//-----------------------------------------------------------------------------
void CAI_MoveProbe::GroundMoveLimit( const Vector &vecStart, const Vector &vecEnd, 
	unsigned int collisionMask, const CBaseEntity *pTarget, unsigned testGroundMoveFlags, float pctToCheckStandPositions, AIMoveTrace_t* pTrace )
{
	//Msg("CAI_MoveProbe::GroundMoveLimit marker 1 from %0.1f %0.1f %0.1f to %0.1f %0.1f %0.1f\n", vecStart.x, vecStart.y, vecStart.z, vecEnd.x, vecEnd.y, vecEnd.z);
	// NOTE: Never call this directly!!! Always use MoveLimit!!
	// This assertion should ensure this happens
	Assert( !IsMoveBlocked( *pTrace ) );

	AI_PROFILE_SCOPE( CAI_Motor_GroundMoveLimit );

	Vector vecActualStart, vecDesiredEnd;

	pTrace->flTotalDist = ComputePathDistance( NAV_GROUND, vecStart, vecEnd );

	if ( !IterativeFloorPoint( vecStart, collisionMask, &vecActualStart ) )
	{
		pTrace->flDistObstructed = pTrace->flTotalDist;
		pTrace->pObstruction	= GetContainingEntity( INDEXENT(0) );
		pTrace->vHitNormal		= vec3_origin;
		pTrace->fStatus			= AIMR_BLOCKED_WORLD;
		pTrace->vEndPosition	= vecStart;

		//Warning("CAI_MoveProbe::GroundMoveLimit marker 2: Attempting to path from/to a point that is in solid space or is too high\n");
		return;
	}
	
	// find out where they (in theory) should have ended up  
	if (!(testGroundMoveFlags & AITGM_2D))
	{
		//Msg("CAI_MoveProbe::GroundMoveLimit marker 3\n");
		IterativeFloorPoint(vecEnd, collisionMask, &vecDesiredEnd);
	}
	else
	{
		//Msg("CAI_MoveProbe::GroundMoveLimit marker 4\n");
		vecDesiredEnd = vecEnd;
	}

	// When checking the route, look for ground geometric validity
	// Let's try to avoid invalid routes
	TestGroundMove( vecActualStart, vecDesiredEnd, collisionMask, pctToCheckStandPositions, testGroundMoveFlags, pTrace );

	// Check to see if the target is in a vehicle and the vehicle is blocking our way
	bool bVehicleMatchesObstruction = false;

	if ( pTarget != NULL )
	{
		//Msg("CAI_MoveProbe::GroundMoveLimit marker 5\n");
		CBaseCombatCharacter *pCCTarget = ((CBaseEntity *)pTarget)->MyCombatCharacterPointer();
		if ( pCCTarget != NULL && pCCTarget->IsInAVehicle() )
		{
			//Msg("CAI_MoveProbe::GroundMoveLimit marker 6\n");
			CBaseEntity *pVehicleEnt = pCCTarget->GetVehicleEntity();
			if (pVehicleEnt == pTrace->pObstruction)
			{
				//Msg("CAI_MoveProbe::GroundMoveLimit marker 7\n");
				bVehicleMatchesObstruction = true;
			}
		}
	}

	if ( (pTarget && (pTarget == pTrace->pObstruction)) || bVehicleMatchesObstruction )
	{
		//Msg("CAI_MoveProbe::GroundMoveLimit marker 8\n");
		// Collided with target entity, return there was no collision!!
		// but leave the end trace position
		pTrace->flDistObstructed = 0.0f;
		pTrace->pObstruction = NULL;
		pTrace->vHitNormal = vec3_origin;
		pTrace->fStatus = AIMR_OK;
	}
}


//-----------------------------------------------------------------------------
// Purpose: returns zero if the caller can walk a straight line from
//			vecStart to vecEnd ignoring collisions with pTarget
//
//			if the move fails, returns the distance remaining to vecEnd
//-----------------------------------------------------------------------------
void CAI_MoveProbe::FlyMoveLimit( const Vector &vecStart, const Vector &vecEnd, 
	unsigned int collisionMask, const CBaseEntity *pTarget, AIMoveTrace_t *pMoveTrace ) const
{
	// NOTE: Never call this directly!!! Always use MoveLimit!!
	// This assertion should ensure this happens
	Assert( !IsMoveBlocked( *pMoveTrace) );

	trace_t tr;
	TraceHull( vecStart, vecEnd, collisionMask, &tr );

	if ( tr.fraction < 1 )
	{
		CBaseEntity *pBlocker = tr.m_pEnt;
		if ( pBlocker )
		{
			if ( pTarget == pBlocker )
			{
				// Colided with target entity, movement is ok
				pMoveTrace->vEndPosition = tr.endpos;
				return;
			}

			// If blocked by an npc remember
			pMoveTrace->pObstruction = pBlocker;
			pMoveTrace->vHitNormal	 = vec3_origin;
			pMoveTrace->fStatus = AIComputeBlockerMoveResult( pBlocker );
		}
		pMoveTrace->flDistObstructed = ComputePathDistance( NAV_FLY, tr.endpos, vecEnd );
		pMoveTrace->vEndPosition = tr.endpos;
		return;
	}

	// no collisions, movement is ok
	pMoveTrace->vEndPosition = vecEnd;
}


//-----------------------------------------------------------------------------
// Purpose: returns zero if the caller can jump from
//			vecStart to vecEnd ignoring collisions with pTarget
//
//			if the jump fails, returns the distance
//			that can be travelled before an obstacle is hit
//-----------------------------------------------------------------------------
void CAI_MoveProbe::JumpMoveLimit( const Vector &vecStart, const Vector &vecEnd, 
	unsigned int collisionMask, const CBaseEntity *pTarget, AIMoveTrace_t *pMoveTrace ) const
{
	pMoveTrace->vJumpVelocity.Init( 0, 0, 0 );

	float flDist = ComputePathDistance( NAV_JUMP, vecStart, vecEnd );

	if (!IsJumpLegal(vecStart, vecEnd, vecEnd))
	{
		pMoveTrace->fStatus = AIMR_ILLEGAL;
		pMoveTrace->flDistObstructed = flDist;
		return;
	}

	// --------------------------------------------------------------------------
	// Drop start and end vectors to the floor and check to see if they're legal
	// --------------------------------------------------------------------------
	Vector vecFrom;
	IterativeFloorPoint( vecStart, collisionMask, &vecFrom );

	Vector vecTo;
	IterativeFloorPoint( vecEnd, collisionMask, StepHeight() * 0.5, &vecTo );
	if (!CheckStandPosition( vecTo, collisionMask))
	{
		pMoveTrace->fStatus = AIMR_ILLEGAL;
		pMoveTrace->flDistObstructed = flDist;
		return;
	}

	if (vecFrom == vecTo)
	{
		pMoveTrace->fStatus = AIMR_ILLEGAL;
		pMoveTrace->flDistObstructed = flDist;
		return;
	}

	if ((vecFrom - vecTo).Length2D() == 0.0)
	{
		pMoveTrace->fStatus = AIMR_ILLEGAL;
		pMoveTrace->flDistObstructed = flDist;
		return;
	}

	// FIXME: add max jump velocity callback?  Look at the velocity in the jump animation?  use ideal running speed?
	float maxHorzVel = GetOuter()->GetMaxJumpSpeed();

	Vector gravity = Vector(0, 0, GetCurrentGravity() * GetOuter()->GetJumpGravity() );

	if ( gravity.z < 0.01 )
	{
		pMoveTrace->fStatus = AIMR_ILLEGAL;
		pMoveTrace->flDistObstructed = flDist;
		return;
	}

	// intialize error state to it being an illegal jump
	CBaseEntity *pObstruction = NULL;
	AIMoveResult_t fStatus = AIMR_ILLEGAL;
	float flDistObstructed = flDist;

	// initialize jump state
	float minSuccessfulJumpHeight = 1024.0;
	float minJumpHeight = 0.0;
	float minJumpStep = 1024.0;

	// initial jump, sets baseline for minJumpHeight
	Vector vecApex;
	Vector rawJumpVel = CalcJumpLaunchVelocity(vecFrom, vecTo, gravity.z, &minJumpHeight, maxHorzVel, &vecApex );
	float baselineJumpHeight = minJumpHeight;

	// FIXME: this is a binary search, which really isn't the right thing to do.  If there's a gap 
	// the npc can jump through, this won't reliably find it.  The only way I can think to do this is a 
	// linear search trying ever higher jumps until the gap is either found or the jump is illegal.
	do
	{
		rawJumpVel = CalcJumpLaunchVelocity(vecFrom, vecTo, gravity.z, &minJumpHeight, maxHorzVel, &vecApex );
		// DevMsg( "%.0f ", minJumpHeight );

		if (!IsJumpLegal(vecFrom, vecApex, vecTo))
		{
			// too high, try lower
			minJumpStep = minJumpStep / 2.0;
			minJumpHeight = minJumpHeight - minJumpStep;
		}
		else
		{
			// Calculate the total time of the jump minus a tiny fraction
			float jumpTime		= (vecFrom - vecTo).Length2D()/rawJumpVel.Length2D();
			float timeStep		= jumpTime / 10.0;	

			Vector vecTest = vecFrom;
			bool bMadeIt = true;

			// this sweeps out a rough approximation of the jump
			// FIXME: this won't reliably hit the apex
			for (float flTime = 0 ; flTime < jumpTime - 0.01; flTime += timeStep )
			{
				trace_t trace;

				// Calculate my position after the time step (average velocity over this time step)
				Vector nextPos = vecTest + (rawJumpVel - 0.5 * gravity * timeStep) * timeStep;

				TraceHull( vecTest, nextPos, collisionMask, &trace );

				if (trace.startsolid || trace.fraction < 0.99) // FIXME: getting inconsistant trace fractions, revisit after Jay resolves collision eplisons
				{
					// NDebugOverlay::Box( trace.endpos, WorldAlignMins(), WorldAlignMaxs(), 255, 255, 0, 0, 10.0 );
					
					// save error state
					pObstruction = trace.m_pEnt;
					fStatus = AIComputeBlockerMoveResult( pObstruction );
					flDistObstructed = ComputePathDistance( NAV_JUMP, vecTest, vecTo );

					if (trace.plane.normal.z < 0.0)
					{
						// hit a ceiling looking thing, too high, try lower
						minJumpStep = minJumpStep / 2.0;
						minJumpHeight = minJumpHeight - minJumpStep;
					}
					else
					{
						// hit wall looking thing, try higher
						minJumpStep = minJumpStep / 2.0;
						minJumpHeight += minJumpStep;
					}
					
					if ( ai_moveprobe_jump_debug.GetBool() )
					{
						NDebugOverlay::Line( vecTest, nextPos, 255, 0, 0, true, 2.0f );
					}

					bMadeIt = false;
					break;
				}
				else
				{
					if ( ai_moveprobe_jump_debug.GetBool() )
					{
						NDebugOverlay::Line( vecTest, nextPos, 0, 255, 0, true, 2.0f );
					}
				}

				rawJumpVel	= rawJumpVel - gravity * timeStep;
				vecTest		= nextPos;
			}

			if (bMadeIt)
			{
				// made it, try lower
				minSuccessfulJumpHeight = minJumpHeight;
				minJumpStep = minJumpStep / 2.0;
				minJumpHeight -= minJumpStep;
			}
		}
	}
	while (minJumpHeight > baselineJumpHeight && minJumpHeight <= 1024.0 && minJumpStep >= 16.0);

	// DevMsg( "(%.0f)\n", minSuccessfulJumpHeight );

	if (minSuccessfulJumpHeight != 1024.0)
	{
		// Get my jump velocity
		pMoveTrace->vJumpVelocity = CalcJumpLaunchVelocity(vecFrom, vecTo, gravity.z, &minSuccessfulJumpHeight, maxHorzVel, &vecApex );
	}
	else
	{
		// ----------------------------------------------------------
		// If blocked by an npc remember
		// ----------------------------------------------------------
		pMoveTrace->pObstruction = pObstruction;
		pMoveTrace->vHitNormal	= vec3_origin;
		pMoveTrace->fStatus = fStatus;
		pMoveTrace->flDistObstructed = flDistObstructed;
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns zero if the caller can climb from
//			vecStart to vecEnd ignoring collisions with pTarget
//
//			if the climb fails, returns the distance remaining 
//			before the obstacle is hit
//-----------------------------------------------------------------------------
void CAI_MoveProbe::ClimbMoveLimit( const Vector &vecStart, const Vector &vecEnd, 
	const CBaseEntity *pTarget, AIMoveTrace_t *pMoveTrace ) const
{
	trace_t tr;
	TraceHull( vecStart, vecEnd, MASK_NPCSOLID, &tr );

	if (tr.fraction < 1.0)
	{
		CBaseEntity *pEntity = tr.m_pEnt;
		if (pEntity == pTarget)
		{
			return;
		}
		else
		{
			// ----------------------------------------------------------
			// If blocked by an npc remember
			// ----------------------------------------------------------
			pMoveTrace->pObstruction = pEntity;
			pMoveTrace->vHitNormal = vec3_origin;
			pMoveTrace->fStatus = AIComputeBlockerMoveResult( pEntity );

			float flDist = (1.0 - tr.fraction) * ComputePathDistance( NAV_CLIMB, vecStart, vecEnd );
			if (flDist <= 0.001) 
			{
				flDist = 0.001;
			}
			pMoveTrace->flDistObstructed = flDist;
			return;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_MoveProbe::MoveLimit( Navigation_t navType, const Vector &vecStart, 
	const Vector &vecEnd, unsigned int collisionMask, const CBaseEntity *pTarget, 
	float pctToCheckStandPositions, unsigned flags, AIMoveTrace_t* pTrace)
{
	//Msg("CAI_MoveProbe::MoveLimit marker 1\n");
	AIMoveTrace_t ignoredTrace;
	if (!pTrace)
	{
		//Msg("CAI_MoveProbe::MoveLimit marker 2\n");
		pTrace = &ignoredTrace;
	}

	// Set a reasonable default set of values
	pTrace->flTotalDist = ComputePathDistance( navType, vecStart, vecEnd );
	pTrace->flDistObstructed = 0.0f;
	pTrace->pObstruction = NULL;
	pTrace->vHitNormal = vec3_origin;
	pTrace->fStatus = AIMR_OK;
	pTrace->vEndPosition = vecStart;

	switch (navType)
	{
	case NAV_GROUND:	
	{
		//Msg("CAI_MoveProbe::MoveLimit marker 3\n");
		unsigned testGroundMoveFlags = AITGM_DEFAULT;
		if (flags & AIMLF_2D)
		{
			//Msg("CAI_MoveProbe::MoveLimit marker 4\n");
			testGroundMoveFlags |= AITGM_2D;//PIN: possibly bad idea, trying for portal navigation
		}
		if (flags & AIMLF_DRAW_RESULTS)
		{
			//Msg("CAI_MoveProbe::MoveLimit marker 5\n");
			testGroundMoveFlags |= AITGM_DRAW_RESULTS;
		}
		if (ai_moveprobe_debug.GetBool() && (GetOuter()->m_debugOverlays & OVERLAY_NPC_SELECTED_BIT))
		{
			//Msg("CAI_MoveProbe::MoveLimit marker 6\n");
			testGroundMoveFlags |= AITGM_DRAW_RESULTS;
		}

		if (flags & AIMLF_IGNORE_TRANSIENTS)
		{
			//Msg("CAI_MoveProbe::MoveLimit marker 7\n");
			const_cast<CAI_MoveProbe *>(this)->m_bIgnoreTransientEntities = true;
		}

		bool bDoIt = true;
		if ( flags & AIMLF_QUICK_REJECT )
		{
			//Warning("CAI_MoveProbe::MoveLimit marker 8\n");
			Assert(vecStart == GetAbsOrigin());
			trace_t tr;
			TraceLine(const_cast<CAI_MoveProbe *>(this)->GetOuter()->EyePosition(), vecEnd, collisionMask, true, &tr);
			bDoIt = ( tr.fraction > 0.99 );
		}

		if (bDoIt)
		{
			//Msg("CAI_MoveProbe::MoveLimit marker 9\n");
			GroundMoveLimit(vecStart, vecEnd, collisionMask, pTarget, testGroundMoveFlags, pctToCheckStandPositions, pTrace);
		}
		else
		{
			//Warning("CAI_MoveProbe::MoveLimit marker 10\n");
			pTrace->pObstruction = GetContainingEntity( INDEXENT(0) );
			pTrace->vHitNormal	 = vec3_origin;
			pTrace->fStatus		= AIMR_BLOCKED_WORLD;
			pTrace->flDistObstructed = ComputePathDistance( NAV_GROUND, vecStart, vecEnd );
		}

		const_cast<CAI_MoveProbe *>(this)->m_bIgnoreTransientEntities = false;

		break;
	}

	case NAV_FLY:
		//Msg("CAI_MoveProbe::MoveLimit marker 11\n");
		FlyMoveLimit(vecStart, vecEnd, collisionMask, pTarget, pTrace);
		break;

	case NAV_JUMP:
		//Msg("CAI_MoveProbe::MoveLimit marker 12\n");
		JumpMoveLimit(vecStart, vecEnd, collisionMask, pTarget, pTrace);
		break;

	case NAV_CLIMB:
		//Msg("CAI_MoveProbe::MoveLimit marker 13\n");
		ClimbMoveLimit(vecStart, vecEnd, pTarget, pTrace);
		break;

	default:
		//Msg("CAI_MoveProbe::MoveLimit marker 14\n");
		pTrace->fStatus = AIMR_ILLEGAL;
		pTrace->flDistObstructed = ComputePathDistance( navType, vecStart, vecEnd );
		break;
	}

	if (IsMoveBlocked(pTrace->fStatus) && pTrace->pObstruction && !pTrace->pObstruction->IsWorld())
	{
		//Msg("CAI_MoveProbe::MoveLimit marker 15\n");
		m_hLastBlockingEnt = pTrace->pObstruction;
	}

	//Warning("CAI_MoveProbe::MoveLimit marker 16 returning %s\n", !IsMoveBlocked(pTrace->fStatus) ? "TRUE" : "FALSE");
	return !IsMoveBlocked(pTrace->fStatus);
}

//-----------------------------------------------------------------------------
// Purpose: Returns a jump lauch velocity for the current target entity
// Input  :
// Output :
//-----------------------------------------------------------------------------
Vector CAI_MoveProbe::CalcJumpLaunchVelocity(const Vector &startPos, const Vector &endPos, float flGravity, float *pminHeight, float maxHorzVelocity, Vector *pvecApex ) const
{
	// Get the height I have to jump to get to the target
	float	stepHeight = endPos.z - startPos.z;

	// get horizontal distance to target
	Vector targetDir2D	= endPos - startPos;
	targetDir2D.z = 0;
	float distance = VectorNormalize(targetDir2D);

	Assert( maxHorzVelocity > 0 );

	// get minimum times and heights to meet ideal horz velocity
	float minHorzTime = distance / maxHorzVelocity;
	float minHorzHeight = 0.5 * flGravity * (minHorzTime * 0.5) * (minHorzTime * 0.5);

	// jump height must be enough to hang in the air
	*pminHeight = MAX( *pminHeight, minHorzHeight );
	// jump height must be enough to cover the step up
	*pminHeight = MAX( *pminHeight, stepHeight );

	// time from start to apex
	float t0 = sqrt( ( 2.0 * *pminHeight) / flGravity );
	// time from apex to end
	float t1 = sqrt( ( 2.0 * fabs( *pminHeight - stepHeight) ) / flGravity );

	float velHorz = distance / (t0 + t1);

	Vector jumpVel = targetDir2D * velHorz;

	jumpVel.z = (float)sqrt(2.0f * flGravity * (*pminHeight));

	if (pvecApex)
	{
		*pvecApex = startPos + targetDir2D * velHorz * t0 + Vector( 0, 0, *pminHeight );
	}

	// -----------------------------------------------------------
	// Make the horizontal jump vector and add vertical component
	// -----------------------------------------------------------

	return jumpVel;
}

//-----------------------------------------------------------------------------

bool CAI_MoveProbe::CheckStandPosition( const Vector &vecStart, unsigned int collisionMask ) const
{
	// If we're not supposed to do ground checks, always say we can stand there
	if ( (GetOuter()->CapabilitiesGet() & bits_CAP_SKIP_NAV_GROUND_CHECK) )
		return true;

	// This is an extra-strong optimization
	if ( ai_strong_optimizations_no_checkstand.GetBool() )
		return true;

	//if ( UseOldCheckStandPosition() )
	//	return OldCheckStandPosition( vecStart, collisionMask );

	AI_PROFILE_SCOPE( CAI_Motor_CheckStandPosition );

	Vector contactMin, contactMax;

	// this should assume the model is already standing
	Vector vecUp	= Vector( vecStart.x, vecStart.y, vecStart.z + 0.1 );
	Vector vecDown	= Vector( vecStart.x, vecStart.y, vecStart.z - StepHeight() * GetOuter()->GetStepDownMultiplier() );

	// check a half sized box centered around the foot
	Vector vHullMins = WorldAlignMins();
	Vector vHullMaxs = WorldAlignMaxs();

	if ( vHullMaxs == vec3_origin && vHullMins == vHullMaxs )
	{
		// "Test hulls" have no collision property
		vHullMins = GetHullMins();
		vHullMaxs = GetHullMaxs();
	}

	contactMin.x = vHullMins.x * 0.75 + vHullMaxs.x * 0.25;
	contactMax.x = vHullMins.x * 0.25 + vHullMaxs.x * 0.75;
	contactMin.y = vHullMins.y * 0.75 + vHullMaxs.y * 0.25;
	contactMax.y = vHullMins.y * 0.25 + vHullMaxs.y * 0.75;
	contactMin.z = vHullMins.z;
	contactMax.z = vHullMins.z;

	trace_t trace1;
	trace_t trace2;
	trace2.m_pEnt = NULL;//PIN: Fixes an issue with garbage pointers!? Why does it happen? Why does this fix it? The same doesn't have to be done with trace1
	
	if ( !GetOuter()->IsFlaggedEfficient() )
	{
		AI_PROFILE_SCOPE( CAI_Motor_CheckStandPosition_Sides );
		
		Vector vHullBottomCenter;
		vHullBottomCenter.Init( 0, 0, vHullMins.z );

		// Try diagonal from lower left to upper right
		TraceHull( vecUp, vecDown, contactMin, vHullBottomCenter, collisionMask, &trace1 );
		if (trace1.fraction != 1.0 && trace1.m_pEnt && CanStandOn(trace1.m_pEnt))
		{
			//NDebugOverlay::BoxAngles(contactMax, vecUp, vecDown, vec3_angle, 255, 0, 0, 0, 10);
			TraceHull(vecUp, vecDown, vHullBottomCenter, contactMax, collisionMask, &trace2);
			if (trace2.fraction != 1.0 && (trace1.m_pEnt == trace2.m_pEnt || (trace2.m_pEnt && CanStandOn(trace2.m_pEnt) ) ) )
			{
				//NDebugOverlay::BoxAngles(contactMax, vecUp, vecDown, vec3_angle, 255, 255, 0, 0, 10);
				return true;//trace 1 and 2 are both standable objects
			}
		}

		// Okay, try the other one
		Vector testMin;
		Vector testMax;
		testMin.Init(contactMin.x, 0, vHullMins.z);
		testMax.Init(0, contactMax.y, vHullMins.z);

		TraceHull(vecUp, vecDown, testMin, testMax, collisionMask, &trace1);
		if (trace1.fraction != 1.0 && trace1.m_pEnt && CanStandOn(trace1.m_pEnt))
		{
			//NDebugOverlay::BoxAngles(testMax, vecUp, vecDown, vec3_angle, 0, 255, 0, 0, 10);
			testMin.Init(0, contactMin.y, vHullMins.z);
			testMax.Init(contactMax.x, 0, vHullMins.z);
			TraceHull(vecUp, vecDown, testMin, testMax, collisionMask, &trace2);
			if (trace2.fraction != 1.0 && (trace1.m_pEnt == trace2.m_pEnt || (trace2.m_pEnt && CanStandOn(trace2.m_pEnt) ) ) )
			{
				//NDebugOverlay::BoxAngles(testMax, vecUp, vecDown, vec3_angle, 0, 255, 255, 0, 10);
				return true;//trace 1 and 2 are both standable objects
			}
		}
	}
	else
	{
		AI_PROFILE_SCOPE( CAI_Motor_CheckStandPosition_Center );
		TraceHull(vecUp, vecDown, contactMin, contactMax, collisionMask, &trace1);
		//NDebugOverlay::BoxAngles(vecUp, vecDown, contactMin, vec3_angle, 0, 0, 255, 0, 10);
		if (trace1.fraction != 1.0 && trace1.m_pEnt && CanStandOn(trace1.m_pEnt))
			return true;//efficient npc, trace 1 is standable object
	}
	//PIN: Chell wants to react to different blockers in different ways in different circumstances
	
		if (trace1.m_pEnt)
		{
			//Msg("Blocker is a '%s' named '%s' with model '%s' at %0.1f, %0.1f, %0.1f.\n", trace1.m_pEnt->GetClassname(), trace1.m_pEnt->GetDebugName(), STRING(trace1.m_pEnt->GetModelName()),
			//	trace1.m_pEnt->GetAbsOrigin().x, trace1.m_pEnt->GetAbsOrigin().y, trace1.m_pEnt->GetAbsOrigin().z);
		}
		if (trace2.m_pEnt)
		{
			//Msg("Blocker is a '%s' named '%s' with model '%s' at %0.1f, %0.1f, %0.1f.\n", trace2.m_pEnt->GetClassname(), trace2.m_pEnt->GetDebugName(), STRING(trace2.m_pEnt->GetModelName()),
			//	trace2.m_pEnt->GetAbsOrigin().x, trace2.m_pEnt->GetAbsOrigin().y, trace2.m_pEnt->GetAbsOrigin().z);
		}


	return false;
}

//-----------------------------------------------------------------------------

bool CAI_MoveProbe::OldCheckStandPosition( const Vector &vecStart, unsigned int collisionMask ) const
{
	AI_PROFILE_SCOPE( CAI_Motor_CheckStandPosition );

	Vector contactMin, contactMax;

	// this should assume the model is already standing
	Vector vecUp	= Vector( vecStart.x, vecStart.y, vecStart.z + 0.1 );
	Vector vecDown	= Vector( vecStart.x, vecStart.y, vecStart.z - StepHeight() * GetOuter()->GetStepDownMultiplier() );

	// check a half sized box centered around the foot
	const Vector &vHullMins = WorldAlignMins();
	const Vector &vHullMaxs = WorldAlignMaxs();

	contactMin.x = vHullMins.x * 0.75 + vHullMaxs.x * 0.25;
	contactMax.x = vHullMins.x * 0.25 + vHullMaxs.x * 0.75;
	contactMin.y = vHullMins.y * 0.75 + vHullMaxs.y * 0.25;
	contactMax.y = vHullMins.y * 0.25 + vHullMaxs.y * 0.75;
	contactMin.z = vHullMins.z;
	contactMax.z = vHullMins.z;

	trace_t trace;
	
	AI_PROFILE_SCOPE_BEGIN( CAI_Motor_CheckStandPosition_Center );
	TraceHull( vecUp, vecDown, contactMin, contactMax, collisionMask, &trace );
	AI_PROFILE_SCOPE_END();

	if (trace.fraction == 1.0 || !CanStandOn( trace.m_pEnt ))
		return false;

	float sumFraction = 0;

	if ( !GetOuter()->IsFlaggedEfficient() )
	{
		AI_PROFILE_SCOPE( CAI_Motor_CheckStandPosition_Sides );
		
		// check a box for each quadrant, allow one failure
		int already_failed = false;
		for	(int x = 0; x <= 1 ;x++)
		{
			for	(int y = 0; y <= 1; y++)
			{
				// create bounding boxes for each quadrant
				contactMin[0] = x ? 0 :vHullMins.x;
				contactMax[0] = x ? vHullMaxs.x : 0;
				contactMin[1] = y ? 0 : vHullMins.y;
				contactMax[1] = y ? vHullMaxs.y : 0;
				
				TraceHull( vecUp, vecDown, contactMin, contactMax, collisionMask, &trace );

				sumFraction += trace.fraction;

				// this should hit something, if it doesn't allow one failure
				if (trace.fraction == 1.0 || !CanStandOn( trace.m_pEnt ))
				{
					if (already_failed)
						return false;
					else
					{
						already_failed = true;
					}
				}
				else
				{
					if ( sumFraction > 2.0 )
						return false;
				}
			}
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Computes a point on the floor below the start point, somewhere
// between vecStart.z + flStartZ and vecStart.z + flEndZ
//-----------------------------------------------------------------------------
bool CAI_MoveProbe::FloorPoint( const Vector &vecStart, unsigned int collisionMask, 
						   float flStartZ, float flEndZ, Vector *pVecResult ) const
{
	bool bIsNearAPortal = IsNearAPortal();
	
	//if (bIsNearAPortal)
	//{
	//	return true;
	//}

	//Msg("CAI_MoveProbe::FloorPoint marker 1\n");
	AI_PROFILE_SCOPE( CAI_Motor_FloorPoint );

	// make a pizzabox shaped bounding hull
	Vector mins = WorldAlignMins();
	Vector maxs( WorldAlignMaxs().x, WorldAlignMaxs().y, mins.z );

	// trace down step height and a bit more
	Vector vecUp( vecStart.x, vecStart.y, vecStart.z + flStartZ + MOVE_HEIGHT_EPSILON );
	Vector vecDown( vecStart.x, vecStart.y, vecStart.z + flEndZ );
	
	trace_t trace;
	TraceHull( vecUp, vecDown, mins, maxs, collisionMask, &trace );
	//Msg("vecStart %0.1f %0.1f %0.1f, flStartZ %0.1f, flEndZ %0.1f\n", vecStart.x, vecStart.y, vecStart.z, flStartZ, flEndZ);
	//NDebugOverlay::Cross3D(vecUp, 32, 255, 0, 255, true, 5);
	//NDebugOverlay::Cross3D(vecDown, 32, 0, 0, 255, true, 5);
	bool fStartedInObject = false;

	if (trace.startsolid)
	{
		//Warning("CAI_MoveProbe::FloorPoint marker 2\n");
		if ( trace.m_pEnt && 
			 ( trace.m_pEnt->GetMoveType() == MOVETYPE_VPHYSICS || trace.m_pEnt->IsNPC() ) &&
			 (vecStart - GetAbsOrigin()).Length() < 0.1)
		{
			//Warning("CAI_MoveProbe::FloorPoint marker 3\n");
			fStartedInObject = true;
		}

		vecUp.z = vecStart.z + MOVE_HEIGHT_EPSILON;
		TraceHull( vecUp, vecDown, mins, maxs, collisionMask, &trace );
	}

	//Warning("CAI_MoveProbe::FloorPoint marker 4, %s, %s, %s\n", trace.fraction == 1 ? "TRUE" : "FALSE", trace.allsolid ? "TRUE" : "FALSE", (fStartedInObject && trace.startsolid) ? "TRUE" : "FALSE");
	// this should have hit a solid surface by now
	if (trace.fraction == 1 || trace.allsolid || ( fStartedInObject && trace.startsolid ) )
	{
		// set result to start position if it doesn't work
		*pVecResult = vecStart;
		if (fStartedInObject)
		{
			//Warning("CAI_MoveProbe::FloorPoint marker 5\n");
			if (!bIsNearAPortal)
				return false;
			return true; // in this case, probably got intruded on by a physics object. Try ignoring it...
		}
		//Warning("NPC STUCK IN SOLID%s near portal!!\n", bIsNearAPortal ? "." : ". not");
		if (!bIsNearAPortal)
			return false;
		//PIN: Chell should be allowed to start "inside a solid". This is an artifact of how portals work.
		//Not caring about this seems like a terrible idea for HL2 NPCs, but I'm hard pressed to come up with a plausible example at the moment.
	}

	*pVecResult = trace.endpos;
	//Msg("CAI_MoveProbe::FloorPoint marker 7\n");
	return true;
}


//-----------------------------------------------------------------------------
// A floorPoint that is useful only in the context of iterative movement
//-----------------------------------------------------------------------------
bool CAI_MoveProbe::IterativeFloorPoint( const Vector &vecStart, unsigned int collisionMask, Vector *pVecResult ) const
{
	//Msg("IterativeFloorPoint B returned %s\n", IterativeFloorPoint(vecStart, collisionMask, 0, pVecResult) ? "TRUE" : "FALSE");
	return IterativeFloorPoint( vecStart, collisionMask, 0, pVecResult );
}

//-----------------------------------------------------------------------------
bool CAI_MoveProbe::IterativeFloorPoint( const Vector &vecStart, unsigned int collisionMask, float flAddedStep, Vector *pVecResult ) const
{
	// Used by the movement code, it guarantees we don't move outside a step
	// height from our current position
	//Msg("IterativeFloorPoint A returned %s\n", FloorPoint(vecStart, collisionMask, StepHeight() * GetOuter()->GetStepDownMultiplier() + flAddedStep, -(12 * 60), pVecResult) ? "TRUE" : "FALSE");
	return FloorPoint( vecStart, collisionMask, StepHeight() * GetOuter()->GetStepDownMultiplier() + flAddedStep, IsNearAPortal() ? 0 : -(12*60), pVecResult );//HACK: Sometimes, being near a portal can cause something to mess up and commands further down the line will try to start pathing 720 units below chell
}

//-----------------------------------------------------------------------------

float CAI_MoveProbe::StepHeight() const
{
	return GetOuter()->StepHeight();
}

//-----------------------------------------------------------------------------

bool CAI_MoveProbe::CanStandOn( CBaseEntity *pSurface ) const
{
	//Msg("CanStandOn\n");
	return GetOuter()->CanStandOn( pSurface );
}

//-----------------------------------------------------------------------------

bool CAI_MoveProbe::IsJumpLegal( const Vector &startPos, const Vector &apex, const Vector &endPos ) const
{
	return GetOuter()->IsJumpLegal( startPos, apex, endPos );
}

//-----------------------------------------------------------------------------

bool CAI_MoveProbe::IsNearAPortal() const
{
	CBaseEntity *pEnt = gEntList.FindEntityByClassname(NULL, "prop_portal");
	bool bFoundPortals = false;
	EHANDLE hPortal1;
	EHANDLE hPortal2;
	while (pEnt)
	{
		CProp_Portal *pPortal = dynamic_cast<CProp_Portal*>(pEnt);
		if (pPortal && !pPortal->IsPortal2() && pPortal->IsActivedAndLinked())
		{
			if (pPortal->m_hLinkedPortal->IsActivedAndLinked())
			{
				hPortal1 = pPortal;
				hPortal2 = pPortal->m_hLinkedPortal;
				bFoundPortals = true;
				break;
			}
		}
		else
		{
			pEnt = gEntList.FindEntityByClassname(pEnt, "prop_portal");
		}
	}
	float fPortalTolerance = 32;
	if (bFoundPortals)
	{
		Vector vecPortal1 = hPortal1.Get()->GetAbsOrigin();
		Vector vecPortal2 = hPortal2.Get()->GetAbsOrigin();
		Vector vecNPC = GetOuter()->WorldSpaceCenter();
		trace_t tr1;
		trace_t tr2;
		UTIL_TraceLine(vecNPC, vecPortal1, MASK_SOLID, GetOuter(), COLLISION_GROUP_DEBRIS, &tr1);
		UTIL_TraceLine(vecNPC, vecPortal2, MASK_SOLID, GetOuter(), COLLISION_GROUP_DEBRIS, &tr2);
		if ((vecPortal1 - vecNPC).Length() < fPortalTolerance && tr1.fraction == 1)
		{
			//ConColorMsg(Color(255, 128, 0, 255), "MOVEPROBE NEAR PORTAL 1 (%0.1f)\n", (vecPortal1 - vecNPC).Length());
			return true;
		}
		if ((vecPortal2 - vecNPC).Length() < fPortalTolerance && tr2.fraction == 1)
		{
			//ConColorMsg(Color(255, 128, 0, 255), "MOVEPROBE NEAR PORTAL 2 (%0.1f)\n", (vecPortal2 - vecNPC).Length());
			return true;
		}
	}
	//ConColorMsg(Color(255, 128, 0, 255), "moveprobe not near portal\n");
	return false;
}