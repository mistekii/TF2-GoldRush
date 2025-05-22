//========= Copyright Valve Corporation, All rights reserved. ============//
//
//
//=============================================================================
#include "cbase.h"
#include "tf_weapon_pistol.h"
#include "tf_fx_shared.h"
#include "in_buttons.h"
#include "tf_gamerules.h"

// Client specific.
#ifdef CLIENT_DLL
#include "c_tf_player.h"
#include "c_tf_gamestats.h"
// Server specific.
#else
#include "tf_player.h"
#include "tf_gamestats.h"
#include "ilagcompensationmanager.h"
#endif

//=============================================================================
//
// Weapon Pistol tables.
//
IMPLEMENT_NETWORKCLASS_ALIASED( TFPistol, DT_WeaponPistol )

BEGIN_NETWORK_TABLE( CTFPistol, DT_WeaponPistol )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CTFPistol )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( tf_weapon_pistol, CTFPistol );
PRECACHE_WEAPON_REGISTER( tf_weapon_pistol );

// Server specific.
#ifndef CLIENT_DLL
BEGIN_DATADESC( CTFPistol )
END_DATADESC()
#endif

//============================

IMPLEMENT_NETWORKCLASS_ALIASED( TFPistol_Scout, DT_WeaponPistol_Scout )

BEGIN_NETWORK_TABLE( CTFPistol_Scout, DT_WeaponPistol_Scout )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CTFPistol_Scout )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( tf_weapon_pistol_scout, CTFPistol_Scout );
PRECACHE_WEAPON_REGISTER( tf_weapon_pistol_scout );

//============================

IMPLEMENT_NETWORKCLASS_ALIASED( TFPistol_ScoutSecondary, DT_WeaponPistol_ScoutSecondary )

BEGIN_NETWORK_TABLE( CTFPistol_ScoutSecondary, DT_WeaponPistol_ScoutSecondary )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CTFPistol_ScoutSecondary )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( tf_weapon_handgun_scout_secondary, CTFPistol_ScoutSecondary );
PRECACHE_WEAPON_REGISTER( tf_weapon_handgun_scout_secondary );

//-----------------------------------------------------------------------------
int	CTFPistol_ScoutSecondary::GetDamageType( void ) const
{
	int iBackheadshot = 0;
	CALL_ATTRIB_HOOK_INT( iBackheadshot, back_headshot );
	if ( iBackheadshot )
	{
		return BaseClass::GetDamageType() | DMG_USE_HITLOCATIONS;	
	}
	return BaseClass::GetDamageType();
}
