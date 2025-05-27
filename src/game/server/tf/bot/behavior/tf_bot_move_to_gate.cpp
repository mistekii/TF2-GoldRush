//========= Copyright Valve Corporation, All rights reserved. ============//
// tf_bot_move_to_gate.cpp
// Move to gate and wait for round to start.

#include "cbase.h"
#include "nav_mesh.h"
#include "tf_player.h"
#include "tf_gamerules.h"
#include "team_control_point_master.h"
#include "team_train_watcher.h"
#include "trigger_area_capture.h"
#include "bot/tf_bot.h"
#include "bot/behavior/tf_bot_move_to_gate.h"

extern ConVar tf_bot_path_lookahead_range;

//---------------------------------------------------------------------------------------------
ActionResult< CTFBot >	CTFBotMoveToGate::OnStart( CTFBot *me, Action< CTFBot > *priorAction )
{
	m_path.SetMinLookAheadDistance( me->GetDesiredPathLookAheadRange() );
	m_path.Invalidate();

	//m_giveUpTimer.Start( RandomFloat( 12.0f, 18.0f ) );

	return Continue();
}


//---------------------------------------------------------------------------------------------
ActionResult< CTFBot >	CTFBotMoveToGate::Update( CTFBot *me, float interval )
{
	const CKnownEntity *threat = me->GetVisionInterface()->GetPrimaryKnownThreat();
	if ( threat && threat->IsVisibleRecently() )
	{
		// prepare to fight
		me->EquipBestWeaponForThreat( threat );
	}

	if ( !TFGameRules()->InSetup() )
	{
		return Done( "Done moving to gate." );
	}

	// move toward the point, periodically repathing to account for changing situation
	if ( m_repathTimer.IsElapsed() )
	{
		VPROF_BUDGET( "CTFBotMoveToGate::Update( repath )", "NextBot" );

		Vector spotPos;
		bool foundSpot = false;
		CUtlVector< CNavArea* > gateVector;

		CTeamTrainWatcher *trainWatcher = TFGameRules()->GetPayloadToPush( me->GetTeamNumber() );
		CBaseEntity* cart = NULL;

		if ( trainWatcher )
		{
			cart = trainWatcher->GetTrainEntity();
		}

		// Look for valid positions near the gate/exit
		FOR_EACH_VEC( TheNavAreas, it )
		{
			CTFNavArea *area = (CTFNavArea *)TheNavAreas[it];
			bool isCanidate = false;

			// Payload race: Get the gate instead of spawnroom.
			if ( TFGameRules()->HasMultipleTrains() && area->IsBlocked( me->GetTeamNumber() ) && !area->HasAttributeTF( TF_NAV_SPAWN_ROOM_EXIT )  )
			{
				isCanidate = true;
			}

			if ( area->HasAttributeTF( TF_NAV_BLUE_SETUP_GATE ) )
			{
				isCanidate = true;
			}

			// Is this an eligable canidate?
			if ( isCanidate )
			{
				CUtlVector< CNavArea* > adjVector;
				area->CollectAdjacentAreas( &adjVector );

				// Search through any adjacent areas for ones we can use.
				for ( int j = 0; j < adjVector.Count(); ++j )
				{
					CTFNavArea *adj = (CTFNavArea *)adjVector[j];

					if ( adj->IsReachableByTeam( me->GetTeamNumber() ) )
					{
						// We can reach this area, add it.
						gateVector.AddToTail(adjVector[j]);
					}
				}
			}

			// Keep going.
			continue;
		}

		// Did we collect any areas?
		if ( gateVector.Count() > 0 )
		{
			// In Payload race, randomly pick between the cart or gate
			if ( TFGameRules()->HasMultipleTrains() && cart && RandomInt(0,1) )
			{
				spotPos = cart->WorldSpaceCenter();
			}
			else
			{
				// Randomly pick an area from our collection
				spotPos = gateVector[RandomInt(0, gateVector.Count()- 1)]->GetRandomPoint();
			}

			foundSpot = true;
		}

		if ( !foundSpot )
		{
			return Continue();
		}

		CTFBotPathCost cost( me, DEFAULT_ROUTE );
		m_path.Compute( me, spotPos, cost );

		m_repathTimer.Start( RandomFloat( 6.0f, 25.0f ) );
	}

	// Move to 
	m_path.Update( me );

	return Continue();
}


//---------------------------------------------------------------------------------------------
ActionResult< CTFBot > CTFBotMoveToGate::OnResume( CTFBot *me, Action< CTFBot > *interruptingAction )
{
	VPROF_BUDGET( "CTFBotMoveToGate::OnResume", "NextBot" );

	m_repathTimer.Invalidate();

	return Continue();
}


//---------------------------------------------------------------------------------------------
EventDesiredResult< CTFBot > CTFBotMoveToGate::OnStuck( CTFBot *me )
{
	VPROF_BUDGET( "CTFBotMoveToGate::OnStuck", "NextBot" );

	m_repathTimer.Invalidate();
	me->GetLocomotionInterface()->ClearStuckStatus();

	return TryContinue();
}


//---------------------------------------------------------------------------------------------
EventDesiredResult< CTFBot > CTFBotMoveToGate::OnMoveToSuccess( CTFBot *me, const Path *path )
{
	return TryContinue();
}


//---------------------------------------------------------------------------------------------
EventDesiredResult< CTFBot > CTFBotMoveToGate::OnMoveToFailure( CTFBot *me, const Path *path, MoveToFailureType reason )
{
	VPROF_BUDGET( "CTFBotMoveToGate::OnMoveToFailure", "NextBot" );

	m_repathTimer.Invalidate();

	return TryContinue();
}


//---------------------------------------------------------------------------------------------
QueryResultType	CTFBotMoveToGate::ShouldRetreat( const INextBot *bot ) const
{
	return ANSWER_UNDEFINED;
}


//---------------------------------------------------------------------------------------------
QueryResultType CTFBotMoveToGate::ShouldHurry( const INextBot *bot ) const
{
	return ANSWER_UNDEFINED;
}
