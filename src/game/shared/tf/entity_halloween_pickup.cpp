//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: CTF HealthKit.
//
//=============================================================================//
#include "cbase.h"
#include "entity_halloween_pickup.h"

#ifdef GAME_DLL
#include "items.h"
#include "tf_gamerules.h"
#include "tf_player.h"
#include "tf_team.h"
#include "engine/IEngineSound.h"
#include "entity_halloween_pickup.h"
#include "tf_fx.h"
#include "tf_logic_halloween_2014.h"
#endif // GAME_DLL

#ifdef CLIENT_DLL
#include "c_tf_player.h"
#endif 

#include "tf_shareddefs.h"


#define TF_HALLOWEEN_PICKUP_RETURN_DELAY	10

//=============================================================================
//
// CTF Halloween Pickup defines.

IMPLEMENT_NETWORKCLASS_ALIASED( HalloweenPickup, DT_CHalloweenPickup )

BEGIN_NETWORK_TABLE( CHalloweenPickup, DT_CHalloweenPickup )
END_NETWORK_TABLE()

BEGIN_DATADESC( CHalloweenPickup )
	DEFINE_KEYFIELD( m_iszSound, FIELD_STRING, "pickup_sound" ),
	DEFINE_KEYFIELD( m_iszParticle, FIELD_STRING, "pickup_particle" ),

#ifdef GAME_DLL
	DEFINE_OUTPUT( m_OnRedPickup, "OnRedPickup" ),
	DEFINE_OUTPUT( m_OnBluePickup, "OnBluePickup" ),
#endif
END_DATADESC();
										 
LINK_ENTITY_TO_CLASS( tf_halloween_pickup, CHalloweenPickup );

// ************************************************************************************


ConVar tf_halloween_gift_lifetime( "tf_halloween_gift_lifetime", "240", FCVAR_CHEAT | FCVAR_REPLICATED );


//=============================================================================
//
// CTF Halloween Pickup functions.
//

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHalloweenPickup::CHalloweenPickup()
{
#ifdef GAME_DLL
	ChangeTeam( TEAM_UNASSIGNED );
#endif

	m_iszSound = MAKE_STRING( "Halloween.Quack" );
	m_iszParticle = MAKE_STRING( "halloween_explosion" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHalloweenPickup::~CHalloweenPickup()
{
}

//-----------------------------------------------------------------------------
// Purpose: Precache function for the pickup
//-----------------------------------------------------------------------------
void CHalloweenPickup::Precache( void )
{
	// We deliberately allow late precaches here
	bool bAllowPrecache = CBaseEntity::IsPrecacheAllowed();
	CBaseEntity::SetAllowPrecache( true );
	PrecacheScriptSound( TF_HALLOWEEN_PICKUP_DEFAULT_SOUND );
	if ( m_iszSound != NULL_STRING )
	{
		PrecacheScriptSound( STRING( m_iszSound ) );
	}
	if ( m_iszParticle != NULL_STRING )
	{
		PrecacheParticleSystem( STRING( m_iszParticle ) );
	}
	BaseClass::Precache();
	CBaseEntity::SetAllowPrecache( bAllowPrecache );
}

#ifdef GAME_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CHalloweenPickup::UpdateTransmitState()
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CHalloweenPickup::ShouldTransmit( const CCheckTransmitInfo *pInfo )
{
	return FL_EDICT_ALWAYS;
}

//-----------------------------------------------------------------------------
// Purpose: MyTouch function for the pickup
//-----------------------------------------------------------------------------
bool CHalloweenPickup::MyTouch( CBasePlayer *pPlayer )
{
	bool bSuccess = false;

	if ( ValidTouch( pPlayer ) )
	{
		bSuccess = true;

		switch( pPlayer->GetTeamNumber() )
		{
		case TF_TEAM_BLUE:
			m_OnBluePickup.FireOutput( this, this );
			break;
		case TF_TEAM_RED:
			m_OnRedPickup.FireOutput( this, this );
			break;
		}

		Vector vecOrigin = GetAbsOrigin() + Vector( 0, 0, 32 );
		CPVSFilter filter( vecOrigin );
		if ( m_iszSound != NULL_STRING )
		{
			EmitSound( filter, entindex(), STRING( m_iszSound ) );
		}
		else
		{
			EmitSound( filter, entindex(), TF_HALLOWEEN_PICKUP_DEFAULT_SOUND );
		}

		if ( m_iszParticle != NULL_STRING )
		{
			TE_TFParticleEffect( filter, 0.0, STRING( m_iszParticle ), vecOrigin, vec3_angle );
		}

		// Increment score directly during 2014 halloween
		if ( CTFMinigameLogic::GetMinigameLogic() && CTFMinigameLogic::GetMinigameLogic()->GetActiveMinigame() )
		{
			inputdata_t inputdata;

			inputdata.pActivator = NULL;
			inputdata.pCaller = NULL;
			inputdata.value.SetInt( 1 );
			inputdata.nOutputID = 0;

			if ( pPlayer->GetTeamNumber() == TF_TEAM_RED )
			{
				CTFMinigameLogic::GetMinigameLogic()->GetActiveMinigame()->InputScoreTeamRed( inputdata );
			}
			else
			{
				CTFMinigameLogic::GetMinigameLogic()->GetActiveMinigame()->InputScoreTeamBlue( inputdata );
			}
		}

		if ( TFGameRules() && TFGameRules()->IsHalloweenScenario( CTFGameRules::HALLOWEEN_SCENARIO_DOOMSDAY ) )
		{
			CTFPlayer *pTFPlayer = ToTFPlayer( pPlayer );
			if ( pTFPlayer )
			{
				IGameEvent *pEvent = gameeventmanager->CreateEvent( "halloween_duck_collected" );
				if ( pEvent )
				{
					pEvent->SetInt( "collector", pTFPlayer->GetUserID() );
					gameeventmanager->FireEvent( pEvent, true );
				}
			}
		}
	}

	return bSuccess;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHalloweenPickup::ValidTouch( CBasePlayer *pPlayer )
{
	CTFPlayer *pTFPlayer = ToTFPlayer( pPlayer );
	if ( pTFPlayer && pTFPlayer->m_Shared.InCond( TF_COND_HALLOWEEN_GHOST_MODE ) )
		return false;

	return BaseClass::ValidTouch( pPlayer );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
float CHalloweenPickup::GetRespawnDelay( void )
{
	return TF_HALLOWEEN_PICKUP_RETURN_DELAY;
}

//-----------------------------------------------------------------------------
// Purpose: Do everything that our base does, but don't change our origin
//-----------------------------------------------------------------------------
CBaseEntity* CHalloweenPickup::Respawn( void )
{
	SetTouch( NULL );
	AddEffects( EF_NODRAW );

	VPhysicsDestroyObject();

	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_TRIGGER );

	m_bRespawning = true;

	//UTIL_SetOrigin( this, g_pGameRules->VecItemRespawnSpot( this ) );// blip to whereever you should respawn.
	SetAbsAngles( g_pGameRules->VecItemRespawnAngles( this ) );// set the angles.

#if !defined( TF_DLL )
	UTIL_DropToFloor( this, MASK_SOLID );
#endif

	RemoveAllDecals(); //remove any decals

	SetThink ( &CItem::Materialize );
	SetNextThink( gpGlobals->curtime + GetRespawnDelay() );
	return this;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CHalloweenPickup::ItemCanBeTouchedByPlayer( CBasePlayer *pPlayer )
{
	if ( m_flThrowerTouchTime > 0.f && gpGlobals->curtime < m_flThrowerTouchTime )
	{
		return false;
	}

	return BaseClass::ItemCanBeTouchedByPlayer( pPlayer );
}

#endif // GAME_DLL
