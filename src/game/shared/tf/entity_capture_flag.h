//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: CTF Flag.
//
//=============================================================================//
#ifndef ENTITY_CAPTURE_FLAG_H
#define ENTITY_CAPTURE_FLAG_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_item.h"
#include "tf_shareddefs.h"

#ifdef CLIENT_DLL
#define CCaptureFlag C_CaptureFlag
#else
class CTFBot;
#endif

#define TF_FLAG_THINK_TIME			0.25f
#define	TF_FLAG_OWNER_PICKUP_TIME	3.0f

#define TF_FLAG_TRAIL_ALPHA			96
#define TF_FLAG_NUMBEROFSKINS		3

#define TF_FLAG_MODEL				"models/flag/briefcase.mdl"
#define TF_FLAG_ICON				"../hud/objectives_flagpanel_carried"
#define TF_FLAG_EFFECT				"player_intel_papertrail"
#define TF_FLAG_TRAIL				"flagtrail"

//=============================================================================
//
// CTF Flag defines.
//

#define TF_CTF_ENEMY_STOLEN			"CaptureFlag.EnemyStolen"
#define TF_CTF_ENEMY_DROPPED		"CaptureFlag.EnemyDropped"
#define TF_CTF_ENEMY_CAPTURED		"CaptureFlag.EnemyCaptured"
#define TF_CTF_ENEMY_RETURNED		"CaptureFlag.EnemyReturned"

#define TF_CTF_TEAM_STOLEN			"CaptureFlag.TeamStolen"
#define TF_CTF_TEAM_DROPPED			"CaptureFlag.TeamDropped"
#define TF_CTF_TEAM_CAPTURED		"CaptureFlag.TeamCaptured"
#define TF_CTF_TEAM_RETURNED		"CaptureFlag.TeamReturned"

#define TF_CTF_FLAGSPAWN			"CaptureFlag.FlagSpawn"

#define TF_CTF_CAPTURED_TEAM_SCORE	1

//=============================================================================
//
// Attack/Defend Flag defines.
//

#define TF_AD_ENEMY_STOLEN			"AttackDefend.EnemyStolen"
#define TF_AD_ENEMY_DROPPED			"AttackDefend.EnemyDropped"
#define TF_AD_ENEMY_CAPTURED		"AttackDefend.EnemyCaptured"
#define TF_AD_ENEMY_RETURNED		"AttackDefend.EnemyReturned"
   
#define TF_AD_TEAM_STOLEN			"AttackDefend.TeamStolen"
#define TF_AD_TEAM_DROPPED			"AttackDefend.TeamDropped"
#define TF_AD_TEAM_CAPTURED			"AttackDefend.TeamCaptured"
#define TF_AD_TEAM_RETURNED			"AttackDefend.TeamReturned"

#define TF_AD_CAPTURED_SOUND		"AttackDefend.Captured"

//=============================================================================
//
// Invade Flag defines.
//

#define TF_INVADE_ENEMY_STOLEN			"Invade.EnemyStolen"
#define TF_INVADE_ENEMY_DROPPED			"Invade.EnemyDropped"
#define TF_INVADE_ENEMY_CAPTURED		"Invade.EnemyCaptured"

#define TF_INVADE_TEAM_STOLEN			"Invade.TeamStolen"
#define TF_INVADE_TEAM_DROPPED			"Invade.TeamDropped"
#define TF_INVADE_TEAM_CAPTURED			"Invade.TeamCaptured"

#define TF_INVADE_FLAG_RETURNED			"Invade.FlagReturned"

#define TF_INVADE_CAPTURED_TEAM_SCORE	1

#define TF_INVADE_NEUTRAL_TIME			30.0f

//=============================================================================
//
// Resource Flag defines.
//

#define TF_RESOURCE_FLAGSPAWN			"Resource.FlagSpawn"

#define TF_RESOURCE_ENEMY_STOLEN		"Announcer.SD_TheirTeamHasFlag"
#define TF_RESOURCE_ENEMY_DROPPED		"Announcer.SD_TheirTeamDroppedFlag"
#define TF_RESOURCE_ENEMY_CAPTURED		"Announcer.SD_TheirTeamCapped"
#define TF_RESOURCE_TEAM_STOLEN			"Announcer.SD_OurTeamHasFlag"
#define TF_RESOURCE_TEAM_DROPPED		"Announcer.SD_OurTeamDroppedFlag"
#define TF_RESOURCE_TEAM_CAPTURED		"Announcer.SD_OurTeamCapped"
#define TF_RESOURCE_RETURNED			"Announcer.SD_FlagReturned"

// Halloween event strings
#define TF_RESOURCE_EVENT_ENEMY_STOLEN		"Announcer.SD_Event_TheirTeamHasFlag"
#define TF_RESOURCE_EVENT_ENEMY_DROPPED		"Announcer.SD_Event_TheirTeamDroppedFlag"
#define TF_RESOURCE_EVENT_TEAM_STOLEN		"Announcer.SD_Event_OurTeamHasFlag"
#define TF_RESOURCE_EVENT_TEAM_DROPPED		"Announcer.SD_Event_OurTeamDroppedFlag"
#define TF_RESOURCE_EVENT_RETURNED			"Announcer.SD_Event_FlagReturned"
#define TF_RESOURCE_EVENT_NAGS				"Announcer.SD_Event_FlagNags"
#define TF_RESOURCE_EVENT_RED_CAPPED		"Announcer.SD_Event_CappedRed"
#define TF_RESOURCE_EVENT_BLUE_CAPPED		"Announcer.SD_Event_CappedBlu"

#define TF_RESOURCE_CAPTURED_TEAM_SCORE	1

//=============================================================================
//
//	Robot Destruction defines
//

#ifdef CLIENT_DLL
	#define CCaptureFlagReturnIcon C_CaptureFlagReturnIcon
	#define CBaseAnimating C_BaseAnimating
#endif

class CCaptureFlagReturnIcon: public CBaseAnimating
{
public:
	DECLARE_CLASS( CCaptureFlagReturnIcon, CBaseEntity );
	DECLARE_NETWORKCLASS();

	CCaptureFlagReturnIcon();

#ifdef CLIENT_DLL

	virtual int		DrawModel( int flags );
	void			DrawReturnProgressBar( void );

	virtual RenderGroup_t GetRenderGroup( void );
	virtual bool	ShouldDraw( void ) { return true; }

	virtual void GetRenderBounds( Vector& theMins, Vector& theMaxs );

private:

	IMaterial	*m_pReturnProgressMaterial_Empty;		// For labels above players' heads.
	IMaterial	*m_pReturnProgressMaterial_Full;

#else
public:
	virtual void Spawn( void );
	virtual int UpdateTransmitState( void );

#endif

};

//=============================================================================
//
// CTF Flag class.
//
DECLARE_AUTO_LIST( ICaptureFlagAutoList );
class CCaptureFlag : public CTFItem, public ICaptureFlagAutoList
{
public:

	DECLARE_CLASS( CCaptureFlag, CTFItem );
	DECLARE_NETWORKCLASS();

	CCaptureFlag();
	~CCaptureFlag();

	unsigned int	GetItemID( void ) const OVERRIDE;

	void			Precache( void );
	void			Spawn( void );

	virtual void	UpdateOnRemove( void );

	void			FlagTouch( CBaseEntity *pOther );

	bool			IsDisabled( void ) const;
	void			SetDisabled( bool bDisabled );
	void			SetVisibleWhenDisabled( bool bVisible );

	bool			IsVisibleWhenDisabled( void ) { return m_bVisibleWhenDisabled; }

	CBaseEntity		*GetPrevOwner( void ) { return m_hPrevOwner.Get(); }

	//-----------------------------------------------------------------------------
	// Purpose: Sets the flag status
	//-----------------------------------------------------------------------------
	void SetFlagStatus( int iStatus, CBasePlayer *pNewOwner = NULL );

// Game DLL Functions
#ifdef GAME_DLL
	CCaptureFlag &operator=( const CCaptureFlag& rhs );
	virtual void	Activate( void );

	static CCaptureFlag*	Create( const Vector& vecOrigin, const char *pszModelName, ETFFlagType type );

	// Input handlers
	void			InputEnable( inputdata_t &inputdata );
	void			InputDisable( inputdata_t &inputdata );
	void			InputRoundActivate( inputdata_t &inputdata );
	void			InputForceDrop( inputdata_t &inputdata );
	void			InputForceReset( inputdata_t &inputdata );
	void			InputForceResetSilent( inputdata_t &inputdata );
	void			InputForceResetAndDisableSilent( inputdata_t &inputdata );
	void			InputSetReturnTime( inputdata_t &inputdata );
	void			InputShowTimer( inputdata_t &inputdata );

	void			Think( void );

	void			CreateReturnIcon( void );
	void			DestroyReturnIcon( void );
	
	void			ResetFlagReturnTime( void ) { m_flResetTime = 0; }
	void			SetFlagReturnIn( float flTime )
	{
		m_flResetTime = gpGlobals->curtime + flTime;
		m_flMaxResetTime = flTime;
	}

    void			SetFlagReturnIn( float flTime, float flMaxResetTime )
    {
        m_flResetTime = gpGlobals->curtime + flTime;
        m_flMaxResetTime = flMaxResetTime;
    }

	void			ResetFlagNeutralTime( void ) { m_flNeutralTime = 0; }
	void			SetFlagNeutralIn( float flTime )
	{ 
		m_flNeutralTime = gpGlobals->curtime + flTime;
		m_flMaxResetTime = flTime;
	}
	bool			IsCaptured( void ){ return m_bCaptured; }

	int				UpdateTransmitState();

	void			StartFlagTrail ( void );
	void			RemoveFlagTrail ( void );
	EHANDLE			m_pFlagTrail;
	float			m_flFlagTrailLife;
	bool			m_bInstantTrailRemove;

	int				GetNumTags() const { return m_tags.Count(); }
	const char*		GetTag( int i ) const { return m_tags[i]; }
	void			AddFollower( CTFBot* pBot );
	void			RemoveFollower( CTFBot* pBot );
	int				GetNumFollowers() const { return m_followers.Count(); }

	void			AddPointValue( int nPoints );

#else // CLIENT DLL Functions
	virtual bool	ShouldDraw() OVERRIDE;
	virtual const char	*GetIDString( void ) { return "entity_capture_flag"; };

	virtual void	OnDataChanged( DataUpdateType_t updateType );

	void			CreateSiren( void );
	void			DestroySiren( void );

	void			ManageTrailEffects( void );

	CNewParticleEffect	*m_pGlowTrailEffect;
	CNewParticleEffect	*m_pPaperTrailEffect;

	virtual void	Simulate( void );

	float			GetMaxResetTime() { return m_flMaxResetTime; }
	float			GetReturnProgress( void );
	
#endif
    // TODO: Both of these should be updated to work with floats instead of ints.
	int				GetReturnTime( int nMaxReturnTime );
    int             GetMaxReturnTime( void );
	
	void			Capture( CTFPlayer *pPlayer, int nCapturePoint );
	virtual void	PickUp( CTFPlayer *pPlayer, bool bInvisible );
	virtual void	Drop( CTFPlayer *pPlayer, bool bVisible, bool bThrown = false, bool bMessage = true );

	ETFFlagType		GetType( void )	const { return (ETFFlagType)m_nType.Get(); }

	bool			IsDropped( void );
	bool			IsHome( void );
	bool			IsStolen( void );

	void ResetFlag( void )
	{
		Reset();
		ResetMessage();
	}

	const char		*GetFlagModel( void );
	void			GetHudIcon( int nTeam, char *pchName, int nBuffSize );
	const char		*GetPaperEffect( void );
	void			GetTrailEffect( int nTeam, char *pchName, int nBuffSize );
private:

	void			Reset( void );
	void			ResetMessage( void );
	void			InternalForceReset( bool bSilent = false );

#ifdef GAME_DLL
	void			PlaySound( IRecipientFilter& filter, const char *pszString, int iTeam = TEAM_ANY );

	float			m_flNextTeamSoundTime[TF_TEAM_COUNT];
#endif

private:

	CNetworkVar( bool,	m_bDisabled );	// Enabled/Disabled?
	CNetworkVar( bool,	m_bVisibleWhenDisabled );
	CNetworkVar( int,	m_nType );	// Type of game this flag will be used for.

	CNetworkVar( int,	m_nFlagStatus );
	CNetworkVar( float,	m_flResetTime );		// Time until the flag is placed back at spawn.
	CNetworkVar( float, m_flMaxResetTime );		// Time the flag takes to return in the current mode
	CNetworkVar( float, m_flNeutralTime );	// Time until the flag becomes neutral (used for the invade gametype)
	CNetworkHandle( CBaseEntity, m_hPrevOwner );
	CNetworkVar( float, m_flAutoCapTime );

#ifdef GAME_DLL
	string_t m_iszModel;
	string_t m_iszHudIcon;
	string_t m_iszPaperEffect;
	string_t m_iszTrailEffect;

	string_t m_iszTags;
	CUtlStringList m_tags;

	CUtlVector< CHandle< CTFBot > >	m_followers;
#endif

	CNetworkString( m_szModel, MAX_PATH );
	CNetworkString( m_szHudIcon, MAX_PATH );
	CNetworkString( m_szPaperEffect, MAX_PATH );
	CNetworkString( m_szTrailEffect, MAX_PATH );
	CNetworkVar( int, m_nUseTrailEffect );
	

	int				m_iOriginalTeam;
	float			m_flOwnerPickupTime;

    int GetReturnTimeShotClockMode( int nStartReturnTime );
    inline bool IsFlagShotClockModePossible() const
    {
        return m_nType == TF_FLAGTYPE_CTF 
			|| m_nType == TF_FLAGTYPE_RESOURCE_CONTROL;
    }

    float           m_flLastPickupTime; // What the time was of the last pickup by any player.
    float           m_flLastResetDuration; // How long was the last time to reset before being picked up?

	int				m_nReturnTime; // Length of time (in seconds) before dropped flag/intelligence returns to base.
	int				m_nNeutralType; // Type of neutral flag (only used for Invade game type).
	int				m_nScoringType; // Type of scoring for flag capture (only used for Invade game type).

	bool			m_bUseShotClockMode; // Used to determine whether we should be using shot clock mode or not.
	
	EHANDLE		m_hReturnIcon;

#ifdef GAME_DLL
	Vector			m_vecResetPos;		// The position the flag should respawn (reset) at.
	QAngle			m_vecResetAng;		// The angle the flag should respawn (reset) at.

	COutputEvent	m_outputOnReturn;	// Fired when the flag is returned via timer.
	COutputEvent	m_outputOnPickUp;	// Fired when the flag is picked up.
	COutputEvent	m_outputOnPickUp1;	// Fired with the player as the activator when the flag is picked up.
	COutputEvent	m_outputOnPickUpTeam1;	// Fired when the flag is picked up by RED.
	COutputEvent	m_outputOnPickUpTeam2;	// Fired when the flag is picked up by BLU.
	COutputEvent	m_outputOnDrop;		// Fired when the flag is dropped.
	COutputEvent	m_outputOnDrop1;	// Fired with the player as the activator when the flag is dropped.
	COutputEvent	m_outputOnCapture;	// Fired when the flag is captured.
	COutputEvent	m_outputOnCapture1;	// Fired with the player as the activator when the flag is captured.
	COutputEvent	m_OnCapTeam1;
	COutputEvent	m_OnCapTeam2;
	COutputEvent	m_OnTouchSameTeam;

	bool			m_bAllowOwnerPickup;

	bool			m_bCaptured;

	EHANDLE			m_hInitialPlayer;
	
	EHANDLE			m_hInitialParent;
	Vector			m_vecOffset;

#else

	IMaterial	*m_pReturnProgressMaterial_Empty;		// For labels above players' heads.
	IMaterial	*m_pReturnProgressMaterial_Full;		

	EHANDLE		m_hOldOwner;

	HPARTICLEFFECT		m_hSirenEffect;

#endif

	DECLARE_DATADESC();
};

#endif // ENTITY_CAPTURE_FLAG_H
