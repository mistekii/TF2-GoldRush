//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: CTF Halloween Pickup.
//
//=============================================================================//
#ifndef ENTITY_HALLOWEEN_PICKUP_H
#define ENTITY_HALLOWEEN_PICKUP_H

#ifdef _WIN32
#pragma once
#endif

#ifdef GAME_DLL
#include "tf_powerup.h"
#include "tf_gamerules.h"
#include "tf_player.h"
#endif

#include "ehandle.h"

#define TF_HALLOWEEN_PICKUP_MODEL	"models/items/target_duck.mdl"
#define TF_GIFT_MODEL			"models/props_halloween/gargoyle_ghost.mdl"; //"models/props_halloween/halloween_gift.mdl";
#define TF_HALLOWEEN_PICKUP_DEFAULT_SOUND	"AmmoPack.Touch"
											  
#ifdef CLIENT_DLL
#define CHalloweenPickup C_HalloweenPickup
#define CHalloweenGiftPickup C_HalloweenGiftPickup

#include "c_tf_player.h"
#endif

//=============================================================================
//
// CTF Halloween Pickup class.
//

class CHalloweenPickup
#ifdef GAME_DLL
	: public CTFPowerup
#else
	: public C_BaseAnimating
#endif
{
public:
#ifdef GAME_DLL
	DECLARE_CLASS( CHalloweenPickup, CTFPowerup );
#else
	DECLARE_CLASS( CHalloweenPickup, C_BaseAnimating );
#endif

	DECLARE_NETWORKCLASS();

	CHalloweenPickup();
	~CHalloweenPickup();

	virtual void	Precache( void ) OVERRIDE;

#ifdef GAME_DLL
	virtual int		UpdateTransmitState() OVERRIDE;
	virtual int		ShouldTransmit( const CCheckTransmitInfo *pInfo ) OVERRIDE;
	virtual bool	ValidTouch( CBasePlayer *pPlayer ) OVERRIDE;
	virtual bool	MyTouch( CBasePlayer *pPlayer ) OVERRIDE;
	virtual CBaseEntity* Respawn( void );

	virtual const char *GetDefaultPowerupModel( void ) OVERRIDE
	{ 
		return TF_HALLOWEEN_PICKUP_MODEL;
	}

	virtual float	GetRespawnDelay( void ) OVERRIDE;

	virtual bool	ItemCanBeTouchedByPlayer( CBasePlayer *pPlayer );
#endif // GAME_DLL

private:
	string_t		m_iszSound;
	string_t		m_iszParticle;

#ifdef GAME_DLL
	COutputEvent	m_OnRedPickup;
	COutputEvent	m_OnBluePickup;
#endif

	DECLARE_DATADESC();
};



//*************************************************************************************************


#endif // ENTITY_HALLOWEEN_PICKUP_H


