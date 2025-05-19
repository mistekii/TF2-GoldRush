//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "c_tf_projectile_arrow.h"
#include "particles_new.h"
#include "SpriteTrail.h"
#include "c_tf_player.h"
#include "collisionutils.h"
#include "util_shared.h"
#include "c_rope.h"

//-----------------------------------------------------------------------------
IMPLEMENT_NETWORKCLASS_ALIASED( TFProjectile_Arrow, DT_TFProjectile_Arrow )

BEGIN_NETWORK_TABLE( C_TFProjectile_Arrow, DT_TFProjectile_Arrow )
	RecvPropBool( RECVINFO( m_bArrowAlight ) ),
	RecvPropBool( RECVINFO( m_bCritical ) ),
	RecvPropInt( RECVINFO( m_iProjectileType ) ),
END_NETWORK_TABLE()

//-----------------------------------------------------------------------------
IMPLEMENT_NETWORKCLASS_ALIASED( TFProjectile_HealingBolt, DT_TFProjectile_HealingBolt )

BEGIN_NETWORK_TABLE( C_TFProjectile_HealingBolt, DT_TFProjectile_HealingBolt )
END_NETWORK_TABLE()

#define NEAR_MISS_THRESHOLD 120

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TFProjectile_Arrow::C_TFProjectile_Arrow( void )
{
	m_fAttachTime = 0.f;
	m_nextNearMissCheck = 0.f;
	m_bNearMiss = false;
	m_bArrowAlight = false;
	m_bCritical = true;
	m_pCritEffect = NULL;
	m_iCachedDeflect = false;
	m_flLifeTime = 40.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TFProjectile_Arrow::~C_TFProjectile_Arrow( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFProjectile_Arrow::OnDataChanged( DataUpdateType_t updateType )
{
	if ( updateType == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );

		if ( m_bArrowAlight )
		{
			ParticleProp()->Create( "flying_flaming_arrow", PATTACH_POINT_FOLLOW, "muzzle" );
		}
	}
	if ( m_bCritical )
	{
		if ( updateType == DATA_UPDATE_CREATED || m_iCachedDeflect != GetDeflected() )
		{
			CreateCritTrail();
		}
	}
	m_iCachedDeflect = GetDeflected();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFProjectile_Arrow::NotifyBoneAttached( C_BaseAnimating* attachTarget )
{
	BaseClass::NotifyBoneAttached( attachTarget );

	m_fAttachTime = gpGlobals->curtime;
	SetNextClientThink( CLIENT_THINK_ALWAYS );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFProjectile_Arrow::ClientThink( void )
{
	// Perform a near-miss check.
	if ( !m_bNearMiss && (gpGlobals->curtime > m_nextNearMissCheck) )
	{
		CheckNearMiss();
		m_nextNearMissCheck = gpGlobals->curtime + 0.05f;
	}

	// Remove crit effect if we hit a wall.
	if ( GetMoveType() == MOVETYPE_NONE && m_pCritEffect )
	{
		ParticleProp()->StopEmission( m_pCritEffect );
		m_pCritEffect = NULL;
	}

	BaseClass::ClientThink();

	// DO THIS LAST: Destroy us automatically after a period of time.
	if ( m_pAttachedTo )
	{
		if ( gpGlobals->curtime - m_fAttachTime > m_flLifeTime )
		{
			Release();
			return;
		}
		else if ( m_pAttachedTo->IsEffectActive( EF_NODRAW ) && !IsEffectActive( EF_NODRAW ) )
		{
			AddEffects( EF_NODRAW );
			UpdateVisibility();
		}
		else if ( !m_pAttachedTo->IsEffectActive( EF_NODRAW ) && IsEffectActive( EF_NODRAW ) && (m_pAttachedTo != C_BasePlayer::GetLocalPlayer()) )
		{
			RemoveEffects( EF_NODRAW );
			UpdateVisibility();
		}
	}

	if ( IsDormant() && !IsEffectActive( EF_NODRAW ) )
	{
		AddEffects( EF_NODRAW );
		UpdateVisibility();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFProjectile_Arrow::CheckNearMiss( void )
{
	// If we are attached to something or stationary we don't want to do near miss checks.
	if ( m_pAttachedTo || (GetMoveType() == MOVETYPE_NONE) )
	{
		m_bNearMiss = true;
		return;
	}

	if ( UTIL_BPerformNearMiss( this, "Weapon_Arrow.Nearmiss", NEAR_MISS_THRESHOLD ) )
	{
		SetNextClientThink( CLIENT_THINK_NEVER );
		m_bNearMiss = true;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFProjectile_Arrow::CreateCritTrail( void )
{
	if ( IsDormant() )
		return;

	if ( m_pCritEffect )
	{
		ParticleProp()->StopEmission( m_pCritEffect );
		m_pCritEffect = NULL;
	}

	if ( m_bCritical )
	{
		switch( GetTeamNumber() )
		{
		case TF_TEAM_BLUE:
			m_pCritEffect = ParticleProp()->Create( "critical_rocket_blue", PATTACH_ABSORIGIN_FOLLOW );
			break;
		case TF_TEAM_RED:
			m_pCritEffect = ParticleProp()->Create( "critical_rocket_red", PATTACH_ABSORIGIN_FOLLOW );
			break;
		default:
			break;
		}
	}
}

//-----------------------------------------------------------------------------
void C_TFProjectile_HealingBolt::OnDataChanged( DataUpdateType_t updateType )
{
	if ( updateType == DATA_UPDATE_CREATED )
	{
		switch( GetTeamNumber() )
		{
		case TF_TEAM_BLUE:
			ParticleProp()->Create( "healshot_trail_blue", PATTACH_ABSORIGIN_FOLLOW );
			break;
		case TF_TEAM_RED:
			ParticleProp()->Create( "healshot_trail_red", PATTACH_ABSORIGIN_FOLLOW );
			break;
		}
	}

	BaseClass::OnDataChanged( updateType );
}
