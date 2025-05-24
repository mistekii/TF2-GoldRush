//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================
#include "cbase.h"
#include "tf_weapon_grenadelauncher.h"
#include "tf_fx_shared.h"
#include "tf_weapon_grenade_pipebomb.h"
#include "tf_gamerules.h"
#include "in_buttons.h"
#include "tf_weaponbase_gun.h"

// Client specific.
#ifdef CLIENT_DLL
#include "c_tf_player.h"
#include "c_tf_gamestats.h"
#include "bone_setup.h"

// Server specific.
#else
#include "tf_player.h"
#include "tf_gamestats.h"
#include "tf_fx.h"
#endif

#define TF_TUBE_COUNT 6

// X is time as a fraction of cProceduralBarrelRotationTime, which is in seconds.
// Y is rotation in degrees
// Z is slope at Y. 
// These are hermite spline control points that match maya.
const Vector cProceduralBarrelRotationAnimationPoints[] = 
{
	Vector( 0,			0,			0 ),
	Vector( 0.7519f,	63.546f,	0 ),
	Vector( 1.0f,		60,			0 )
};

static_assert( ARRAYSIZE( cProceduralBarrelRotationAnimationPoints ) > 1, "cProceduralBarrelRotationAnimationPoints must have at least two elements." );

const float cProceduralBarrelRotationTime = 0.2666f;

//=============================================================================
//
// Weapon Grenade Launcher tables.
//
IMPLEMENT_NETWORKCLASS_ALIASED( TFGrenadeLauncher, DT_WeaponGrenadeLauncher )

BEGIN_NETWORK_TABLE( CTFGrenadeLauncher, DT_WeaponGrenadeLauncher )
#ifdef CLIENT_DLL
	RecvPropFloat( RECVINFO( m_flDetonateTime ) ),
	RecvPropInt( RECVINFO( m_iCurrentTube ) ),	
	RecvPropInt( RECVINFO( m_iGoalTube ) ), 
#else
	SendPropFloat( SENDINFO( m_flDetonateTime ) ),
	SendPropInt( SENDINFO( m_iCurrentTube ) ),	
	SendPropInt( SENDINFO( m_iGoalTube ) ), 
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CTFGrenadeLauncher )
	DEFINE_FIELD( m_flDetonateTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_iCurrentTube, FIELD_INTEGER ),
	DEFINE_FIELD( m_iGoalTube, FIELD_INTEGER )
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS( tf_weapon_grenadelauncher, CTFGrenadeLauncher );
PRECACHE_WEAPON_REGISTER( tf_weapon_grenadelauncher );

// Server specific.
#ifndef CLIENT_DLL
BEGIN_DATADESC( CTFGrenadeLauncher )
END_DATADESC()
#endif

#define TF_GRENADE_LAUNCER_MIN_VEL 1200

#define TF_DETONATE_MODE_AIR		2

//=============================================================================
//
// Weapon Grenade Launcher functions.
//

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
CTFGrenadeLauncher::CTFGrenadeLauncher()
{
	m_bReloadsSingly = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
CTFGrenadeLauncher::~CTFGrenadeLauncher()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGrenadeLauncher::Spawn( void )
{
	m_iAltFireHint = HINT_ALTFIRE_GRENADELAUNCHER;
	BaseClass::Spawn();

	ResetDetonateTime();
}

//-----------------------------------------------------------------------------
// Purpose: Reset the charge when we holster
//-----------------------------------------------------------------------------
bool CTFGrenadeLauncher::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	ResetDetonateTime();
	return BaseClass::Holster( pSwitchingTo );
}

//-----------------------------------------------------------------------------
// Purpose: Reset the charge when we deploy
//-----------------------------------------------------------------------------
bool CTFGrenadeLauncher::Deploy( void )
{
	ResetDetonateTime();
	return BaseClass::Deploy();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTFGrenadeLauncher::GetMaxClip1( void ) const
{
#ifdef _X360 
	return TF_GRENADE_LAUNCHER_XBOX_CLIP;
#endif

	return BaseClass::GetMaxClip1();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTFGrenadeLauncher::GetDefaultClip1( void ) const
{
#ifdef _X360
	return TF_GRENADE_LAUNCHER_XBOX_CLIP;
#endif

	return BaseClass::GetDefaultClip1();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGrenadeLauncher::PrimaryAttack( void )
{
	// Check for ammunition.
	if ( m_iClip1 <= 0 && m_iClip1 != -1 )
		return;

	// Are we capable of firing again?
	if ( m_flNextPrimaryAttack > gpGlobals->curtime )
		return;

	if ( !CanAttack() )
	{
		ResetDetonateTime();
		return;
	}

	m_iWeaponMode = TF_WEAPON_PRIMARY_MODE;

	LaunchGrenade();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGrenadeLauncher::ItemPostFrame( void )
{
	BaseClass::ItemPostFrame();

	if ( m_flDetonateTime > 0.f )
	{
		if ( m_flDetonateTime > gpGlobals->curtime )
		{
			CTFPlayer *pPlayer = ToTFPlayer( GetPlayerOwner() );
			if ( !pPlayer )
				return;

			// If we're not holding down the attack button, launch our grenade
			if ( m_iClip1 > 0  && !(pPlayer->m_nButtons & IN_ATTACK) )
			{
				LaunchGrenade();
			}
		}
		else
		{
			Misfire();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGrenadeLauncher::Misfire( void )
{
	BaseClass::Misfire();

	LaunchGrenade();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGrenadeLauncher::WeaponIdle( void )
{
	BaseClass::WeaponIdle();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGrenadeLauncher::FireProjectileInternal( CTFPlayer* pTFPlayer )
{
#ifdef GAME_DLL
	CTFGrenadePipebombProjectile *pProjectile = static_cast<CTFGrenadePipebombProjectile*>( FireProjectile( pTFPlayer ) );
	if ( pProjectile )
	{
		if ( GetDetonateMode() == TF_DETONATE_MODE_AIR )
		{
			pProjectile->m_bWallShatter = true;
		}
		
		if ( m_flDetonateTime > 0.f )
		{
			float flDetonateTimeLength = Clamp(m_flDetonateTime - gpGlobals->curtime, 0.f, GetMortarDetonateTimeLength());
			pProjectile->SetDetonateTimerLength( flDetonateTimeLength );
			if ( flDetonateTimeLength == 0.f )
			{
				trace_t tr;
				UTIL_TraceLine( pProjectile->GetAbsOrigin(), pTFPlayer->EyePosition(), MASK_SOLID, pProjectile, COLLISION_GROUP_NONE, &tr );
				pProjectile->Explode( &tr, GetDamageType() );
			}
		}

		float flDetonationPenalty = 1.0f;
		CALL_ATTRIB_HOOK_FLOAT( flDetonationPenalty, grenade_detonation_damage_penalty );
		if ( flDetonationPenalty != 1.0f )
		{
			// Setting the initial damage of a grenade lower will set its fused time damage lower
			// on contact detonations reset the damage to max
			pProjectile->SetDamage( pProjectile->GetDamage() * flDetonationPenalty );
		}
	}
#else
	FireProjectile( pTFPlayer );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGrenadeLauncher::WeaponReset( void )
{
	BaseClass::WeaponReset();

	ResetDetonateTime();

	m_iCurrentTube = 0;
	m_iGoalTube = 0;
	m_bCurrentAndGoalTubeEqual = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFGrenadeLauncher::SendWeaponAnim( int iActivity )
{
	// Client procedurally animates the barrel bone
	if ( iActivity == ACT_VM_PRIMARYATTACK )
	{
		m_iGoalTube = ( m_iCurrentTube + 1 ) % TF_TUBE_COUNT;
		m_flBarrelRotateBeginTime = gpGlobals->curtime;
	} 

	// When we start firing, play the startup firing anim first
	if ( iActivity == ACT_VM_PRIMARYATTACK )
	{
		// If we're already playing the fire anim, let it continue. It loops.
		if ( GetActivity() == ACT_VM_PRIMARYATTACK )
			return true;

		// Otherwise, play the start it
		return BaseClass::SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	}

	return BaseClass::SendWeaponAnim( iActivity );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGrenadeLauncher::PostFire()
{
	// Set next attack times.
	float flFireDelay = ApplyFireDelay( m_pWeaponInfo->GetWeaponData( m_iWeaponMode ).m_flTimeFireDelay );

	m_flNextPrimaryAttack = gpGlobals->curtime + flFireDelay;

	SetWeaponIdleTime( gpGlobals->curtime + SequenceDuration() );

	// Check the reload mode and behave appropriately.
	if ( m_bReloadsSingly )
	{
		m_iReloadMode.Set( TF_RELOAD_START );
	}

	ResetDetonateTime();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGrenadeLauncher::LaunchGrenade( void )
{
	// Get the player owning the weapon.
	CTFPlayer *pPlayer = ToTFPlayer( GetPlayerOwner() );
	if ( !pPlayer )
		return;

	CalcIsAttackCritical();

	SendWeaponAnim( ACT_VM_PRIMARYATTACK );

	pPlayer->SetAnimation( PLAYER_ATTACK1 );
	pPlayer->DoAnimationEvent( PLAYERANIMEVENT_ATTACK_PRIMARY );

	if ( !AutoFiresFullClipAllAtOnce() )
	{
		FireProjectileInternal( pPlayer );
	}
	else
	{
		int nCurrentClipSize = m_iClip1;
		m_nLauncherSlot = 0;
		int iSeed = CBaseEntity::GetPredictionRandomSeed() & 255;
		QAngle punchAngle = pPlayer->GetPunchAngle();
		for ( int i=0; i<nCurrentClipSize; ++i, ++iSeed )
		{
			RandomSeed( iSeed );
			FireProjectileInternal( pPlayer );
			if ( i == 0 )
			{
				punchAngle = pPlayer->GetPunchAngle();
			}
		}
		pPlayer->SetPunchAngle( punchAngle );
	}

#ifdef CLIENT_DLL
	C_CTF_GameStats.Event_PlayerFiredWeapon( pPlayer, IsCurrentAttackACrit() );
#else
	pPlayer->SpeakWeaponFire();
	CTF_GameStats.Event_PlayerFiredWeapon( pPlayer, IsCurrentAttackACrit() );
#endif

	PostFire();
}

float CTFGrenadeLauncher::GetProjectileSpeed( void )
{
	float flLaunchSpeed = TF_GRENADE_LAUNCER_MIN_VEL;
	CALL_ATTRIB_HOOK_FLOAT( flLaunchSpeed, mult_projectile_speed );
	return flLaunchSpeed;
}

int CTFGrenadeLauncher::GetDetonateMode( void ) const
{
	int iMode = 0;
	CALL_ATTRIB_HOOK_INT( iMode, set_detonate_mode );
	return iMode;
}

//-----------------------------------------------------------------------------
// Purpose: Detonate this demoman's pipebombs
//-----------------------------------------------------------------------------
void CTFGrenadeLauncher::SecondaryAttack( void )
{
#ifdef GAME_DLL

	if ( !CanAttack() )
		return;

	CTFPlayer *pOwner = ToTFPlayer( GetOwner() );
	pOwner->DoClassSpecialSkill();

#endif
}

bool CTFGrenadeLauncher::Reload( void )
{
	return BaseClass::Reload();
}


void CTFGrenadeLauncher::FireFullClipAtOnce( void )
{
	m_iWeaponMode = TF_WEAPON_PRIMARY_MODE;

	LaunchGrenade();	
}


void CTFGrenadeLauncher::ResetDetonateTime()
{
	m_flDetonateTime = 0.f;
}


float CTFGrenadeLauncher::GetMortarDetonateTimeLength()
{
	float flMortarDetonateTimeLength = 0.f;
	CALL_ATTRIB_HOOK_FLOAT( flMortarDetonateTimeLength, grenade_launcher_mortar_mode );
	return flMortarDetonateTimeLength;
}


#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CStudioHdr *CTFGrenadeLauncher::OnNewModel( void )
{
	CStudioHdr *hdr = BaseClass::OnNewModel();

	m_iBarrelBone = LookupBone( "procedural_chamber" );

	return hdr;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGrenadeLauncher::StandardBlendingRules( CStudioHdr *hdr, Vector pos[], Quaternion q[], float currentTime, int boneMask )
{
	BaseClass::StandardBlendingRules( hdr, pos, q, currentTime, boneMask );

	if (m_iBarrelBone != -1)
	{
		UpdateBarrelMovement();

		AngleQuaternion( RadianEuler( 0, 0, m_flBarrelAngle ), q[m_iBarrelBone] );
	}

}

//-----------------------------------------------------------------------------
// Purpose: For third person weapons.
//-----------------------------------------------------------------------------
void CTFGrenadeLauncher::OnDataChanged( DataUpdateType_t type )
{
	if ( m_bCurrentAndGoalTubeEqual && m_iCurrentTube != m_iGoalTube )
		m_flBarrelRotateBeginTime = gpGlobals->curtime;
	
	m_bCurrentAndGoalTubeEqual = ( m_iCurrentTube == m_iGoalTube );

	BaseClass::OnDataChanged( type );
}

//-----------------------------------------------------------------------------
// Purpose: Updates the velocity and position of the rotating barrel
//-----------------------------------------------------------------------------
void CTFGrenadeLauncher::UpdateBarrelMovement( void )
{
	if ( m_iGoalTube != m_iCurrentTube )
	{
		float flPartialRotationDeg = 0.0f;

		const float tVal = ( gpGlobals->curtime - m_flBarrelRotateBeginTime ) / cProceduralBarrelRotationTime;

		if ( tVal < 1.0f )
		{
			Assert( cProceduralBarrelRotationAnimationPoints[ 0 ].x == 0.0f );
			Assert( cProceduralBarrelRotationAnimationPoints[ ARRAYSIZE( cProceduralBarrelRotationAnimationPoints ) - 1 ].x == 1.0f );

			const Vector* pFirst = NULL;
			const Vector* pSecond  = NULL;

			for ( int i = 1; i < ARRAYSIZE( cProceduralBarrelRotationAnimationPoints ); ++i ) 
			{
				// Need to be increasing in time, or we won't find the right span. 
				Assert( cProceduralBarrelRotationAnimationPoints[ i - 1 ].x < cProceduralBarrelRotationAnimationPoints[ i ].x );

				if ( tVal <= cProceduralBarrelRotationAnimationPoints[ i ].x )
				{
					pFirst = &cProceduralBarrelRotationAnimationPoints[ i - 1 ];
					pSecond = &cProceduralBarrelRotationAnimationPoints[ i ];
					break;
				}
			}

			Assert( pFirst && pSecond );
			float flPartialT = ( tVal - pFirst->x ) / ( pSecond->x - pFirst->x );
			flPartialRotationDeg = Hermite_Spline( pFirst->y, pSecond->y, pFirst->z, pSecond->z, flPartialT );
		}
		else
		{
			m_iCurrentTube = m_iGoalTube;
			m_bCurrentAndGoalTubeEqual = true;
		}

		const float flBaseDeg = 60.0f * m_iCurrentTube;
		m_flBarrelAngle = DEG2RAD( flBaseDeg + flPartialRotationDeg );
	}
}

void CTFGrenadeLauncher::ViewModelAttachmentBlending( CStudioHdr *hdr, Vector pos[], Quaternion q[], float currentTime, int boneMask )
{
	int iBarrelBone = Studio_BoneIndexByName( hdr, "procedural_chamber" );

	// Assert( iBarrelBone != -1 );

	if ( iBarrelBone != -1 )
	{
		if ( hdr->boneFlags( iBarrelBone ) & boneMask )
		{
			RadianEuler a;
			QuaternionAngles( q[ iBarrelBone ], a );

			a.z = m_flBarrelAngle;

			AngleQuaternion( a, q[ iBarrelBone ] );
		}
	}

}

#endif //CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose: 
// won't be called for w_ version of the model, so this isn't getting updated twice
//-----------------------------------------------------------------------------
void CTFGrenadeLauncher::ItemPreFrame( void )
{
#ifdef CLIENT_DLL
	UpdateBarrelMovement();
#endif

#ifdef GAME_DLL
	if ( gpGlobals->curtime > m_flBarrelRotateBeginTime + cProceduralBarrelRotationTime )
		m_iCurrentTube = m_iGoalTube;
#endif

	BaseClass::ItemPreFrame();
}
