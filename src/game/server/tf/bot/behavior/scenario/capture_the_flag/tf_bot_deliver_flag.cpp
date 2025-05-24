//========= Copyright Valve Corporation, All rights reserved. ============//
// tf_bot_deliver_flag.cpp
// Take the flag we are holding to its destination
// Michael Booth, May 2011

#include "cbase.h"

#include "tf_player_shared.h"

#include "bot/tf_bot.h"
#include "bot/behavior/scenario/capture_the_flag/tf_bot_deliver_flag.h"
#include "bot/behavior/tf_bot_taunt.h"

#include "tf_objective_resource.h"
#include "econ_item_system.h"
#include "tf_gamestats.h"

#include "bot/behavior/nav_entities/tf_bot_nav_ent_move_to.h"
#include "bot/behavior/nav_entities/tf_bot_nav_ent_wait.h"

#include "particle_parse.h"


//---------------------------------------------------------------------------------------------
ActionResult< CTFBot >	CTFBotDeliverFlag::OnStart( CTFBot *me, Action< CTFBot > *priorAction )
{
	m_flTotalTravelDistance = -1.0f;

	m_path.SetMinLookAheadDistance( me->GetDesiredPathLookAheadRange() );

	return Continue();
}


//---------------------------------------------------------------------------------------------
// In Mann Vs Machine, the flag carrier gets stronger the longer he carries the flag
bool CTFBotDeliverFlag::UpgradeOverTime( CTFBot *me )
{
	return false;
}


//---------------------------------------------------------------------------------------------
ActionResult< CTFBot > CTFBotDeliverFlag::Update( CTFBot *me, float interval )
{
	CCaptureFlag *flag = me->GetFlagToFetch();

	if ( !flag )
	{
		return Done( "No flag" );
	}

	CTFPlayer *carrier = ToTFPlayer( flag->GetOwnerEntity() );
	if ( !carrier || !me->IsSelf( carrier ) )
	{
		return Done( "I'm no longer carrying the flag" );
	}

	const CKnownEntity *threat = me->GetVisionInterface()->GetPrimaryKnownThreat();
	if ( threat && threat->IsVisibleRecently() )
	{
		// prepare to fight
		me->EquipBestWeaponForThreat( threat );
	}

	// deliver the flag
	if ( m_repathTimer.IsElapsed() )
	{
		CCaptureZone *zone = me->GetFlagCaptureZone();

		if ( !zone )
		{
			return Done( "No flag capture zone exists!" );
		}

		CTFBotPathCost cost( me, FASTEST_ROUTE );
		m_path.Compute( me, zone->WorldSpaceCenter(), cost );

		float flOldTravelDistance = m_flTotalTravelDistance;

		m_flTotalTravelDistance = NavAreaTravelDistance( me->GetLastKnownArea(), TheNavMesh->GetNavArea( zone->WorldSpaceCenter() ), cost );

		if ( TFGameRules()->IsMannVsMachineMode() && flOldTravelDistance != -1.0f && m_flTotalTravelDistance - flOldTravelDistance > 2000.0f )
		{
			TFGameRules()->BroadcastSound( 255, "Announcer.MVM_Bomb_Reset" );

			// Look for players that helped with the reset and send an event
			CUtlVector<CTFPlayer *> playerVector;
			CollectPlayers( &playerVector, TF_TEAM_PVE_DEFENDERS );
			FOR_EACH_VEC( playerVector, i )
			{
				CTFPlayer *pPlayer = playerVector[i];
				if ( !pPlayer )
					continue;

				if ( me->m_AchievementData.IsPusherInHistory( pPlayer, 3.f ) )
				{
					IGameEvent *event = gameeventmanager->CreateEvent( "mvm_bomb_reset_by_player" );
					if ( event )
					{
						event->SetInt( "player", pPlayer->entindex() );
						gameeventmanager->FireEvent( event );
					}

					CTF_GameStats.Event_PlayerAwardBonusPoints( pPlayer, me, 100 );
				}
			}
		}

		m_repathTimer.Start( RandomFloat( 1.0f, 2.0f ) );
	}

	m_path.Update( me );

	if ( UpgradeOverTime( me ) )
	{
		return SuspendFor( new CTFBotTaunt, "Taunting for our new upgrade" );
	}

	return Continue();
}


//---------------------------------------------------------------------------------------------
void CTFBotDeliverFlag::OnEnd( CTFBot *me, Action< CTFBot > *nextAction )
{
	me->ClearAttribute( CTFBot::SUPPRESS_FIRE );

	if ( TFGameRules() && TFGameRules()->IsMannVsMachineMode() )
	{
		me->m_Shared.ResetRageBuffs();
	}
}


//---------------------------------------------------------------------------------------------
QueryResultType CTFBotDeliverFlag::ShouldAttack( const INextBot *me, const CKnownEntity *them ) const
{
	return ANSWER_NO;
}


//---------------------------------------------------------------------------------------------
// are we in a hurry?
QueryResultType CTFBotDeliverFlag::ShouldHurry( const INextBot *me ) const
{
	return ANSWER_YES;
}


//---------------------------------------------------------------------------------------------
// is it time to retreat?
QueryResultType	CTFBotDeliverFlag::ShouldRetreat( const INextBot *me ) const
{
	return ANSWER_NO;
}


//---------------------------------------------------------------------------------------------
EventDesiredResult< CTFBot > CTFBotDeliverFlag::OnContact( CTFBot *me, CBaseEntity *other, CGameTrace *result )
{
	return TryContinue();
}


//---------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------
CTFBotPushToCapturePoint::CTFBotPushToCapturePoint( Action< CTFBot > *nextAction )
{
	m_nextAction = nextAction;
}


//---------------------------------------------------------------------------------------------
ActionResult< CTFBot > CTFBotPushToCapturePoint::Update( CTFBot *me, float interval )
{
	// flag collection and delivery is handled by our parent behavior, ScenarioMonitor

	CCaptureZone *zone = me->GetFlagCaptureZone();

	if ( !zone )
	{
		if ( m_nextAction )
		{
			return ChangeTo( m_nextAction, "No flag capture zone exists!" );
		}

		return Done( "No flag capture zone exists!" );
	}

	Vector toZone = zone->WorldSpaceCenter() - me->GetAbsOrigin();
	if ( toZone.AsVector2D().IsLengthLessThan( 50.0f ) )
	{
		if ( m_nextAction )
		{
			return ChangeTo( m_nextAction, "At destination" );
		}

		return Done( "At destination" );
	}

	const CKnownEntity *threat = me->GetVisionInterface()->GetPrimaryKnownThreat();
	if ( threat && threat->IsVisibleRecently() )
	{
		// prepare to fight
		me->EquipBestWeaponForThreat( threat );
	}

	if ( m_repathTimer.IsElapsed() )
	{
		CTFBotPathCost cost( me, FASTEST_ROUTE );
		m_path.Compute( me, zone->WorldSpaceCenter(), cost );

		m_repathTimer.Start( RandomFloat( 1.0f, 2.0f ) );
	}

	m_path.Update( me );

	return Continue();
}

//-----------------------------------------------------------------------------------------
EventDesiredResult< CTFBot > CTFBotPushToCapturePoint::OnNavAreaChanged( CTFBot *me, CNavArea *newArea, CNavArea *oldArea )
{
	// does the area we are entering have a prerequisite?
	if ( newArea && newArea->HasPrerequisite( me ) )
	{
		const CUtlVector< CHandle< CFuncNavPrerequisite > > &prereqVector = newArea->GetPrerequisiteVector();

		for( int i=0; i<prereqVector.Count(); ++i )
		{
			const CFuncNavPrerequisite *prereq = prereqVector[i];
			if ( prereq && prereq->IsEnabled() && const_cast< CFuncNavPrerequisite * >( prereq )->PassesTriggerFilters( me ) )
			{
				// this prerequisite applies to me
				if ( prereq->IsTask( CFuncNavPrerequisite::TASK_WAIT ) )
				{
					return TrySuspendFor( new CTFBotNavEntWait( prereq ), RESULT_IMPORTANT, "Prerequisite commands me to wait" );
				}
				else if ( prereq->IsTask( CFuncNavPrerequisite::TASK_MOVE_TO_ENTITY ) )
				{
					return TrySuspendFor( new CTFBotNavEntMoveTo( prereq ), RESULT_IMPORTANT, "Prerequisite commands me to move to an entity" );
				}
			}
		}
	}

	return TryContinue();
}
