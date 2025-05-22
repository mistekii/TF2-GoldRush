//========= Copyright Valve Corporation, All rights reserved. ============//
//
// TF Rocket Launcher
//
//=============================================================================
#include "cbase.h"
#include "tf_weapon_rocketlauncher.h"
#include "tf_fx_shared.h"
#include "tf_weaponbase_rocket.h"
#include "in_buttons.h"
#include "tf_gamerules.h"

// Client specific.
#ifdef CLIENT_DLL
#include "c_tf_player.h"
#include <vgui_controls/Panel.h>
#include <vgui/ISurface.h>
#include "soundenvelope.h"
#include "particle_property.h"
// Server specific.
#else
#include "tf_player.h"
#include "tf_obj_sentrygun.h"
#include "tf_projectile_arrow.h"

#endif

#define BOMBARDMENT_ROCKET_MODEL "models/buildables/sentry3_rockets.mdl"

//=============================================================================
//
// Weapon Rocket Launcher tables.
//
IMPLEMENT_NETWORKCLASS_ALIASED( TFRocketLauncher, DT_WeaponRocketLauncher )

BEGIN_NETWORK_TABLE( CTFRocketLauncher, DT_WeaponRocketLauncher )
#ifndef CLIENT_DLL
//	SendPropInt( SENDINFO( m_iSecondaryShotsFired ) ),
#else
//	RecvPropInt( RECVINFO( m_iSecondaryShotsFired ) ),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CTFRocketLauncher )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( tf_weapon_rocketlauncher, CTFRocketLauncher );
PRECACHE_WEAPON_REGISTER( tf_weapon_rocketlauncher );

// Server specific.
#ifndef CLIENT_DLL
BEGIN_DATADESC( CTFRocketLauncher )
END_DATADESC()
#endif

//=============================================================================
//
// Direct Hit tables.
//
IMPLEMENT_NETWORKCLASS_ALIASED( TFRocketLauncher_DirectHit, DT_WeaponRocketLauncher_DirectHit )

BEGIN_NETWORK_TABLE( CTFRocketLauncher_DirectHit, DT_WeaponRocketLauncher_DirectHit )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CTFRocketLauncher_DirectHit )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( tf_weapon_rocketlauncher_directhit, CTFRocketLauncher_DirectHit );
PRECACHE_WEAPON_REGISTER( tf_weapon_rocketlauncher_directhit );

// Server specific.
#ifndef CLIENT_DLL
BEGIN_DATADESC( CTFRocketLauncher_DirectHit )
END_DATADESC()
#endif

//CREATE_SIMPLE_WEAPON_TABLE( TFRocketLauncher_Mortar, tf_weapon_rocketlauncher_mortar )
//=============================================================================
//
// Mortar tables.
//
IMPLEMENT_NETWORKCLASS_ALIASED( TFRocketLauncher_Mortar, DT_WeaponRocketLauncher_Mortar )

BEGIN_NETWORK_TABLE( CTFRocketLauncher_Mortar, DT_WeaponRocketLauncher_Mortar )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CTFRocketLauncher_Mortar )
END_PREDICTION_DATA()


// Server specific.
#ifndef CLIENT_DLL
BEGIN_DATADESC( CTFRocketLauncher_Mortar )
END_DATADESC()
#endif

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
CTFRocketLauncher::CTFRocketLauncher()
{
	m_bReloadsSingly = true;
	m_nReloadPitchStep = 0;

#ifdef GAME_DLL
	m_bIsOverloading = false;
#endif //GAME_DLL
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
CTFRocketLauncher::~CTFRocketLauncher()
{
}

#ifndef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRocketLauncher::Precache()
{
	BaseClass::Precache();
	PrecacheParticleSystem( "rocketbackblast" );

	// FIXME: DO WE STILL NEED THESE??
	PrecacheScriptSound( "MVM.GiantSoldierRocketShoot" );
	PrecacheScriptSound( "MVM.GiantSoldierRocketShootCrit" );
	PrecacheScriptSound( "MVM.GiantSoldierRocketExplode" );

	//Building_Sentrygun.FireRocket
}
#endif

void CTFRocketLauncher::ModifyEmitSoundParams( EmitSound_t &params )
{
	bool bBaseReloadSound = V_strcmp( params.m_pSoundName, "Weapon_RPG.Reload" ) == 0;
	if ( AutoFiresFullClip() && ( bBaseReloadSound || V_strcmp( params.m_pSoundName, "Weapon_DumpsterRocket.Reload" ) == 0 ) )
	{
		float fMaxAmmoInClip = GetMaxClip1();
		float fAmmoPercentage = static_cast< float >( m_nReloadPitchStep ) / fMaxAmmoInClip;

		// Play a sound that gets higher pitched as more ammo is added
		if ( bBaseReloadSound )
		{
			params.m_pSoundName = "Weapon_DumpsterRocket.Reload_FP";
		}
		else
		{
			params.m_pSoundName = "Weapon_DumpsterRocket.Reload";
		}

		params.m_nPitch *= RemapVal( fAmmoPercentage, 0.0f, ( fMaxAmmoInClip - 1.0f ) / fMaxAmmoInClip, 0.79f, 1.19f );
		params.m_nFlags |= SND_CHANGE_PITCH;

		m_nReloadPitchStep = MIN( GetMaxClip1() - 1, m_nReloadPitchStep + 1 );

		// The last rocket goes in right when this sound happens so that you can launch it before a misfire
		IncrementAmmo();
		m_bReloadedThroughAnimEvent = true;
	}
	else if ( UsesCenterFireProjectile() && ( bBaseReloadSound || V_strcmp( params.m_pSoundName, "Weapon_QuakeRPG.Reload" ) == 0 ) )
	{
		params.m_pSoundName = "Weapon_QuakeRPG.Reload";
	}
}

void CTFRocketLauncher::Misfire( void )
{
	BaseClass::Misfire();

#ifdef GAME_DLL
	if ( CanOverload() )
	{
		CTFPlayer *pPlayer = ToTFPlayer( GetPlayerOwner() );
		if ( !pPlayer )
			return;

		CTFBaseRocket *pRocket = dynamic_cast< CTFBaseRocket* >( BaseClass::FireProjectile( pPlayer ) );
		if ( pRocket )
		{
			trace_t tr;
			UTIL_TraceLine( pRocket->GetAbsOrigin(), pPlayer->EyePosition(), MASK_SOLID, pRocket, COLLISION_GROUP_NONE, &tr );
			pRocket->Explode( &tr, pPlayer );
		}
	}
#endif
}

//-----------------------------------------------------------------------------
bool CTFRocketLauncher::CheckReloadMisfire( void )
{
	if ( !CanOverload() )
		return false;

#ifdef GAME_DLL
	CTFPlayer *pPlayer = GetTFPlayerOwner();

	if ( m_bIsOverloading )
	{
		if ( Clip1() > 0 )
		{
			Misfire();
			return true;
		}
		else
		{
			m_bIsOverloading = false;
		}
	}
	else if ( Clip1() >= GetMaxClip1() || ( Clip1() > 0 && pPlayer && pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) == 0 ) )
	{
		Misfire();
		m_bIsOverloading = true;
		return true;
	}
#endif // GAME_DLL
	return false;
}


//-----------------------------------------------------------------------------
bool CTFRocketLauncher::ShouldBlockPrimaryFire()
{
	return !AutoFiresFullClip();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseEntity *CTFRocketLauncher::FireProjectile( CTFPlayer *pPlayer )
{
	m_flShowReloadHintAt = gpGlobals->curtime + 30;
	CBaseEntity *pRocket = BaseClass::FireProjectile( pPlayer );

	m_nReloadPitchStep = MAX( 0, m_nReloadPitchStep - 1 );

#ifdef GAME_DLL
	int iProjectile = 0;
	CALL_ATTRIB_HOOK_INT( iProjectile, override_projectile_type );
	if ( iProjectile == 0 )
	{
		iProjectile = GetWeaponProjectileType();
	}
	if ( pPlayer->IsPlayerClass( TF_CLASS_SOLDIER ) && IsCurrentAttackARandomCrit() && ( iProjectile == TF_PROJECTILE_ROCKET ) )
	{
		// Track consecutive crit shots for achievements
		m_iConsecutiveCrits++;
		if ( m_iConsecutiveCrits == 2 )
		{
			pPlayer->AwardAchievement( ACHIEVEMENT_TF_SOLDIER_SHOOT_MULT_CRITS );
		}
	}
	else
	{
		m_iConsecutiveCrits = 0;
	}
	m_bIsOverloading = false;
#endif

	if ( TFGameRules()->GameModeUsesUpgrades() )
	{
		PlayUpgradedShootSound( "Weapon_Upgrade.DamageBonus" );
	}


	return pRocket;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRocketLauncher::ItemPostFrame( void )
{
	CTFPlayer *pOwner = ToTFPlayer( GetOwnerEntity() );
	if ( !pOwner )
		return;

	BaseClass::ItemPostFrame();

#ifdef GAME_DLL

	if ( m_flShowReloadHintAt && m_flShowReloadHintAt < gpGlobals->curtime )
	{
		if ( Clip1() < GetMaxClip1() )
		{
			pOwner->HintMessage( HINT_SOLDIER_RPG_RELOAD );
		}
		m_flShowReloadHintAt = 0;
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFRocketLauncher::DefaultReload( int iClipSize1, int iClipSize2, int iActivity )
{
	m_flShowReloadHintAt = 0;
	return BaseClass::DefaultReload( iClipSize1, iClipSize2, iActivity );
}

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFRocketLauncher::CreateMuzzleFlashEffects( C_BaseEntity *pAttachEnt, int nIndex )
{
	BaseClass::CreateMuzzleFlashEffects( pAttachEnt, nIndex );

	// Don't do backblast effects in first person
	C_TFPlayer *pOwner = ToTFPlayer( GetOwnerEntity() );
	if ( pOwner->IsLocalPlayer() )
		return;

	ParticleProp()->Init( this );
	ParticleProp()->Create( "rocketbackblast", PATTACH_POINT_FOLLOW, "backblast" );
}
#endif


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int	CTFRocketLauncher::GetWeaponProjectileType( void ) const
{
	return BaseClass::GetWeaponProjectileType();
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------
// CTFRocketLauncher_Mortar BEGIN
//----------------------------------------------------------------------------------------------------------------------------------------------------------
//CTFRocketLauncher_Mortar::CTFRocketLauncher_Mortar()
//{
//	
//}
//----------------------------------------------------------------------------------------------------------------------------------------------------------
CBaseEntity *CTFRocketLauncher_Mortar::FireProjectile( CTFPlayer *pPlayer )
{
	// Fire the rocket
	CBaseEntity* pRocket = BaseClass::FireProjectile( pPlayer );
	// Add it to my list
#ifdef GAME_DLL
	m_vecRockets.AddToTail( pRocket );
#endif

	return pRocket;
}
//----------------------------------------------------------------------------------------------------------------------------------------------------------
void CTFRocketLauncher_Mortar::SecondaryAttack( void )
{
	RedirectRockets();
}
//-----------------------------------------------------------------------------
void CTFRocketLauncher_Mortar::ItemPostFrame( void )
{
#ifdef GAME_DLL
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( pOwner && pOwner->m_nButtons & IN_ATTACK2 )
	{
		// If allowed
		RedirectRockets();
	}
#endif
	BaseClass::ItemPostFrame();
}

//-----------------------------------------------------------------------------
void CTFRocketLauncher_Mortar::ItemBusyFrame( void )
{
#ifdef GAME_DLL
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( pOwner && pOwner->m_nButtons & IN_ATTACK2 )
	{
		// If allowed
		RedirectRockets();
	}
#endif
	BaseClass::ItemBusyFrame();
}


//-----------------------------------------------------------------------------
void CTFRocketLauncher_Mortar::RedirectRockets( void )
{
#ifdef GAME_DLL
	if ( m_vecRockets.Count() <= 0 )
		return;

	CTFPlayer *pOwner = ToTFPlayer( GetOwnerEntity() );
	if ( !pOwner )
		return;

	Vector vecEye = pOwner->EyePosition();
	Vector vecForward, vecRight, vecUp;
	AngleVectors( pOwner->EyeAngles(), &vecForward, &vecRight, &vecUp );

	trace_t tr;
	UTIL_TraceLine( vecEye, vecEye + vecForward * MAX_TRACE_LENGTH, MASK_SOLID, pOwner, COLLISION_GROUP_NONE, &tr );
	float flVel = 1100.0f;

	FOR_EACH_VEC_BACK( m_vecRockets, i )
	{
		CBaseEntity* pRocket = m_vecRockets[i].Get();
		// Remove targets that have disappeared
		if ( !pRocket || pRocket->GetOwnerEntity() != GetOwnerEntity() )
		{
			m_vecRockets.Remove( i );
			continue;
		}

		// Give the rocket a new target
		Vector vecDir = pRocket->WorldSpaceCenter() - tr.endpos;
		VectorNormalize( vecDir );

		Vector vecVel = pRocket->GetAbsVelocity();
		vecVel = -flVel * vecDir;
		pRocket->SetAbsVelocity( vecVel );

		QAngle newAngles;
		VectorAngles( -vecDir, newAngles );
		pRocket->SetAbsAngles( newAngles );

		m_vecRockets.Remove( i );
	}
#endif
}