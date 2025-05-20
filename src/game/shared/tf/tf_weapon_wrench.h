//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef TF_WEAPON_WRENCH_H
#define TF_WEAPON_WRENCH_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_weaponbase_melee.h"
#include "tf_item_wearable.h"

#ifdef CLIENT_DLL
#define CTFWrench C_TFWrench
#endif

//=============================================================================
//
// Wrench class.
//
class CTFWrench : public CTFWeaponBaseMelee
{
public:

	DECLARE_CLASS( CTFWrench, CTFWeaponBaseMelee );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	CTFWrench();

	virtual void		Spawn();
	virtual int			GetWeaponID( void ) const			{ return TF_WEAPON_WRENCH; }
	virtual void		Smack( void );

	virtual bool		Holster( CBaseCombatWeapon *pSwitchingTo );

	bool				IsPDQ( void ) { int iMode = 0; CALL_ATTRIB_HOOK_INT( iMode, wrench_builds_minisentry ); return iMode==1; };
	float				GetConstructionValue( void );
	float				GetRepairAmount( void );
#ifdef GAME_DLL
	virtual void		Equip( CBaseCombatCharacter *pOwner );
	virtual void		Detach();

	void				ApplyBuildingHealthUpgrade( void );

	void				OnFriendlyBuildingHit( CBaseObject *pObject, CTFPlayer *pPlayer, Vector hitLoc );
#else
	virtual void		ItemPostFrame();
#endif


private:
	bool				m_bReloadDown;
	CTFWrench( const CTFWrench & ) {}
};

#endif // TF_WEAPON_WRENCH_H
