//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: TF Sniper Rifle
//
//=============================================================================//
#ifndef TF_WEAPON_SNIPERRIFLE_H
#define TF_WEAPON_SNIPERRIFLE_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_weaponbase_gun.h"
#include "Sprite.h"

#if defined( CLIENT_DLL )
#define CTFSniperRifle C_TFSniperRifle
#define CSniperDot C_SniperDot
#endif

//=============================================================================
//
// Sniper Rifle Laser Dot class.
//
class CSniperDot : public CBaseEntity
{
public:

	DECLARE_CLASS( CSniperDot, CBaseEntity );
	DECLARE_NETWORKCLASS();
	DECLARE_DATADESC();

	// Creation/Destruction.
	CSniperDot( void );
	~CSniperDot( void );

	static CSniperDot *Create( const Vector &origin, CBaseEntity *pOwner = NULL, bool bVisibleDot = true );
	void		ResetChargeTime( void ) { m_flChargeStartTime = gpGlobals->curtime; }

	// Attributes.
	int			ObjectCaps()							{ return (BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_DONT_SAVE; }

	// Targeting.
	void        Update( CBaseEntity *pTarget, const Vector &vecOrigin, const Vector &vecNormal );
	CBaseEntity	*GetTargetEntity( void )				{ return m_hTargetEnt; }
	Vector		GetChasePosition();

// Client specific.
#ifdef CLIENT_DLL

	bool GetRenderingPositions( C_TFPlayer *pPlayer, Vector &vecAttachment, Vector &vecEndPos, float &flSize );

	// Rendering.
	virtual bool			IsTransparent( void )		{ return true; }
	virtual RenderGroup_t	GetRenderGroup( void )		{ return RENDER_GROUP_TRANSLUCENT_ENTITY; }
	virtual int				DrawModel( int flags );
	virtual bool			ShouldDraw( void );

	virtual void			ClientThink( void );

	virtual void			OnDataChanged( DataUpdateType_t updateType );

	CMaterialReference		m_hSpriteMaterial;
	HPARTICLEFFECT			m_laserBeamEffect;
#endif

protected:

	Vector					m_vecSurfaceNormal;
	EHANDLE					m_hTargetEnt;

	CNetworkVar( float, m_flChargeStartTime );
};

//=============================================================================
//
// Sniper Rifle class.
//
class CTFSniperRifle : public CTFWeaponBaseGun
{
public:

	DECLARE_CLASS( CTFSniperRifle, CTFWeaponBaseGun );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	DECLARE_DATADESC();

	CTFSniperRifle();
	virtual ~CTFSniperRifle();

	virtual int	GetWeaponID( void ) const			{ return TF_WEAPON_SNIPERRIFLE; }

	virtual void Spawn();
	virtual void Precache() OVERRIDE;
	void		 ResetTimers( void );

	virtual bool Reload( void );
	virtual bool CanHolster( void ) const;
	virtual bool Holster( CBaseCombatWeapon *pSwitchingTo );

	virtual void HandleZooms( void );
	virtual void ItemPostFrame( void );
	virtual bool Lower( void );
	virtual float GetProjectileDamage( void );
	virtual int	GetDamageType() const;
	int		GetRifleType( void ) const { int iMode = 0; CALL_ATTRIB_HOOK_INT( iMode, set_weapon_mode ); return iMode; };

	virtual void WeaponReset( void );

	virtual bool CanFireCriticalShot( bool bIsHeadshot = false, CBaseEntity *pTarget = NULL ) OVERRIDE;

	virtual void PlayWeaponShootSound( void );
	virtual bool MustBeZoomedToFire( void );

	virtual ETFDmgCustom GetPenetrateType() const;

#ifdef CLIENT_DLL
	float GetHUDDamagePerc( void );

	virtual bool ShouldEjectBrass();
#endif

	bool IsZoomed( void );
	bool IsFullyCharged( void ) const;			// have we been zoomed in long enough for our shot to do max damage

	virtual void OnControlStunned( void );

	virtual int GetCustomDamageType() const;

#ifdef GAME_DLL
	// Hit tracking for achievements
	virtual void	OnPlayerKill( CTFPlayer *pVictim, const CTakeDamageInfo &info ) OVERRIDE;
	virtual void	OnBulletFire( int iEnemyPlayersHit ) OVERRIDE;

	void			ExplosiveHeadShot( CTFPlayer *pAttacker, CTFPlayer *pVictim );
#endif
	
	void			Detach( void ) OVERRIDE;

	virtual bool	IsJarateRifle( void ) const;
	virtual float	GetJarateTime( void ) const;

	virtual float	SniperRifleChargeRateMod() { return 0.f; }

	virtual int		GetBuffType() { int iBuffType = 0; CALL_ATTRIB_HOOK_INT( iBuffType, set_buff_type ); return iBuffType; }

	float			GetProgress( void );
	bool			IsRageFull( void ); // same as GetProgress() without the division by 100.0f
	const char*		GetEffectLabelText( void ) { return "#TF_SNIPERRAGE"; }
	bool			EffectMeterShouldFlash( void );

#ifdef SIXENSE
	float			GetRezoomTime() const;
#endif
	virtual void	ZoomIn( void );
	virtual void	ZoomOut( void );

	void			ApplyScopeSpeedModifications( float &flBaseRef );
	void			ApplyChargeSpeedModifications( float &flBaseRef );

protected:

	float			GetJarateTimeInternal( void ) const;

	bool	m_bCurrentShotIsHeadshot;

	void CreateSniperDot( void );
	void DestroySniperDot( void );
	void UpdateSniperDot( void );

	void Fire( CTFPlayer *pPlayer );

	// Auto-rezooming handling
	void SetRezoom( bool bRezoom, float flDelay );
	virtual void Zoom( void );
	void ZoomOutIn( void );

	void HandleNoScopeFireDeny( void );

	CNetworkVar( float,	m_flChargedDamage );

#ifdef GAME_DLL
	CHandle<CSniperDot>		m_hSniperDot;
#else
	bool m_bPlayedBell;
#endif

	bool m_bRezoomAfterShot;

	float m_flChargePerSec;
	bool m_bWasAimedAtEnemy;

	void SetInternalUnzoomTime( float flUnzoomTime );

private:
	// Handles rezooming after the post-fire unzoom
	float m_flUnzoomTime;
	float m_flRezoomTime;

	CTFSniperRifle( const CTFSniperRifle & );
};


#endif // TF_WEAPON_SNIPERRIFLE_H
