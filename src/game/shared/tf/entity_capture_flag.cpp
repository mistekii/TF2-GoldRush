//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: CTF Flag.
//
//=============================================================================//
#include "cbase.h"
#include "entity_capture_flag.h"
#include "tf_gamerules.h"
#include "tf_shareddefs.h"
#include "filesystem.h"

#ifdef CLIENT_DLL
#include <vgui_controls/Panel.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/EditablePanel.h>
#include <vgui/IScheme.h>
#include "hudelement.h"
#include "iclientmode.h"
#include "hud_numericdisplay.h"
#include "tf_imagepanel.h"
#include "c_tf_player.h"
#include "c_tf_team.h"
#include "tf_hud_objectivestatus.h"
#include "view.h"

ConVar cl_flag_return_size( "cl_flag_return_size", "20", FCVAR_CHEAT );

extern ConVar tf_rd_flag_ui_mode;

#else
#include "tf_player.h"
#include "tf_team.h"
#include "tf_objective_resource.h"
#include "tf_gamestats.h"
#include "func_respawnroom.h"
#include "datacache/imdlcache.h"
#include "func_respawnflag.h"
#include "func_capture_zone.h"
#include "nav_mesh/tf_nav_mesh.h"
#include "bot/tf_bot.h"
#include "tf_logic_halloween_2014.h"
extern ConVar tf_flag_caps_per_round;
extern ConVar tf_rd_min_points_to_steal;

ConVar cl_flag_return_height( "cl_flag_return_height", "82", FCVAR_CHEAT );

#endif

ConVar tf_flag_return_on_touch( "tf_flag_return_on_touch", "0", FCVAR_REPLICATED, "If this is set, your flag must be at base in order to capture the enemy flag. Remote friendly flags return to your base instantly when you touch them" );


ConVar tf_flag_return_time_credit_factor( "tf_flag_return_time_credit_factor", "1.0", FCVAR_REPLICATED, "Number of seconds the flag's return time will be credited for each second the flag is being carried.", true, 0.f, false, 0.f );

enum
{
	INVADE_NEUTRAL_TYPE_NONE = 0,	// no neutral time
	INVADE_NEUTRAL_TYPE_DEFAULT,	// current behavior....30 secs (TF_INVADE_NEUTRAL_TIME)
	INVADE_NEUTRAL_TYPE_HALF,		// half the return time
};

enum
{
	INVADE_SCORING_TEAM_SCORE = 0,		
	INVADE_SCORING_TEAM_CAPTURE_COUNT,
};

#define FLAG_EFFECTS_NONE		0
#define FLAG_EFFECTS_ALL		1
#define FLAG_EFFECTS_PAPERONLY	2
#define FLAG_EFFECTS_COLORONLY	3

#ifdef CLIENT_DLL

static void RecvProxy_IsDisabled( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CCaptureFlag *pFlag = (CCaptureFlag *) pStruct;
	bool bIsDisabled = ( pData->m_Value.m_Int > 0 );

	if ( pFlag )
	{
		pFlag->SetDisabled( bIsDisabled ); 
	}
}

static void RecvProxy_IsVisibleWhenDisabled( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CCaptureFlag *pFlag = (CCaptureFlag *) pStruct;
	bool bVisible = ( pData->m_Value.m_Int > 0 );

	if ( pFlag )
	{
		pFlag->SetVisibleWhenDisabled( bVisible ); 
	}
}

static void RecvProxy_FlagStatus( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CCaptureFlag *pFlag = (CCaptureFlag *) pStruct;

	if ( pFlag )
	{
		pFlag->SetFlagStatus( pData->m_Value.m_Int );

		IGameEvent *pEvent = gameeventmanager->CreateEvent( "flagstatus_update" );
		if ( pEvent )
		{
			pEvent->SetInt( "entindex", pFlag->entindex() );
			gameeventmanager->FireEventClientSide( pEvent );
		}
	}
}

#endif

//=============================================================================
//
// CTF Flag tables.
//

IMPLEMENT_NETWORKCLASS_ALIASED( CaptureFlag, DT_CaptureFlag )

BEGIN_NETWORK_TABLE( CCaptureFlag, DT_CaptureFlag )

#ifdef GAME_DLL
	SendPropBool( SENDINFO( m_bDisabled ) ),
	SendPropBool( SENDINFO( m_bVisibleWhenDisabled ) ),
	SendPropInt( SENDINFO( m_nType ), 5, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_nFlagStatus ), 3, SPROP_UNSIGNED ),
	SendPropTime( SENDINFO( m_flResetTime ) ),
	SendPropTime( SENDINFO( m_flNeutralTime ) ),
	SendPropTime( SENDINFO( m_flMaxResetTime ) ),
	SendPropEHandle( SENDINFO( m_hPrevOwner ) ),
	SendPropString( SENDINFO( m_szModel ) ),
	SendPropString( SENDINFO( m_szHudIcon ) ),
	SendPropString( SENDINFO( m_szPaperEffect ) ),
	SendPropString( SENDINFO( m_szTrailEffect ) ),
	SendPropInt( SENDINFO( m_nUseTrailEffect ), 3, SPROP_UNSIGNED ),
	SendPropFloat( SENDINFO( m_flAutoCapTime ) ),

#else
	RecvPropInt( RECVINFO( m_bDisabled ), 0, RecvProxy_IsDisabled ),
	RecvPropInt( RECVINFO( m_bVisibleWhenDisabled ), 0, RecvProxy_IsVisibleWhenDisabled ),
	RecvPropInt( RECVINFO( m_nType ) ),
	RecvPropInt( RECVINFO( m_nFlagStatus ), 0, RecvProxy_FlagStatus ),
	RecvPropTime( RECVINFO( m_flResetTime ) ),
	RecvPropTime( RECVINFO( m_flNeutralTime ) ),
	RecvPropTime( RECVINFO( m_flMaxResetTime ) ),
	RecvPropEHandle( RECVINFO( m_hPrevOwner ) ),
	RecvPropString( RECVINFO( m_szModel ) ),
	RecvPropString( RECVINFO( m_szHudIcon ) ),
	RecvPropString( RECVINFO( m_szPaperEffect ) ),
	RecvPropString( RECVINFO( m_szTrailEffect ) ),
	RecvPropInt( RECVINFO( m_nUseTrailEffect ) ),
	RecvPropFloat( RECVINFO( m_flAutoCapTime ) ),

#endif
END_NETWORK_TABLE()

BEGIN_DATADESC( CCaptureFlag )

	// Keyfields.
	DEFINE_KEYFIELD( m_nType, FIELD_INTEGER, "GameType" ),
	DEFINE_KEYFIELD( m_nReturnTime, FIELD_INTEGER, "ReturnTime" ),
	DEFINE_KEYFIELD( m_nUseTrailEffect, FIELD_INTEGER, "trail_effect" ),
	DEFINE_KEYFIELD( m_nNeutralType, FIELD_INTEGER, "NeutralType" ),
	DEFINE_KEYFIELD( m_nScoringType, FIELD_INTEGER, "ScoringType" ),
	DEFINE_KEYFIELD( m_bVisibleWhenDisabled, FIELD_BOOLEAN, "VisibleWhenDisabled" ),
	DEFINE_KEYFIELD( m_bUseShotClockMode, FIELD_BOOLEAN, "ShotClockMode" ),

#ifdef GAME_DLL
	DEFINE_KEYFIELD( m_iszModel, FIELD_STRING, "flag_model" ),
	DEFINE_KEYFIELD( m_iszHudIcon, FIELD_STRING, "flag_icon" ),
	DEFINE_KEYFIELD( m_iszPaperEffect, FIELD_STRING, "flag_paper" ),
	DEFINE_KEYFIELD( m_iszTrailEffect, FIELD_STRING, "flag_trail" ),
	DEFINE_KEYFIELD( m_iszTags, FIELD_STRING, "tags" ),

	// Inputs.
	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "RoundActivate", InputRoundActivate ),
	DEFINE_INPUTFUNC( FIELD_VOID, "ForceDrop", InputForceDrop ),
	DEFINE_INPUTFUNC( FIELD_VOID, "ForceReset", InputForceReset ),
	DEFINE_INPUTFUNC( FIELD_VOID, "ForceResetSilent", InputForceResetSilent ),
	DEFINE_INPUTFUNC( FIELD_VOID, "ForceResetAndDisableSilent", InputForceResetAndDisableSilent ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "SetReturnTime", InputSetReturnTime ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "ShowTimer", InputShowTimer ),

	// Outputs.
	DEFINE_OUTPUT( m_outputOnReturn, "OnReturn" ),
	DEFINE_OUTPUT( m_outputOnPickUp, "OnPickUp" ),
	DEFINE_OUTPUT( m_outputOnPickUp1, "OnPickup1" ),
	DEFINE_OUTPUT( m_outputOnPickUpTeam1, "OnPickupTeam1" ),
	DEFINE_OUTPUT( m_outputOnPickUpTeam2, "OnPickupTeam2" ),
	DEFINE_OUTPUT( m_outputOnDrop, "OnDrop" ),
	DEFINE_OUTPUT( m_outputOnDrop1, "OnDrop1" ),
	DEFINE_OUTPUT( m_outputOnCapture, "OnCapture" ),
	DEFINE_OUTPUT( m_outputOnCapture1, "OnCapture1" ),
	DEFINE_OUTPUT( m_OnCapTeam1, "OnCapTeam1" ),
	DEFINE_OUTPUT( m_OnCapTeam2, "OnCapTeam2" ),
	DEFINE_OUTPUT( m_OnTouchSameTeam, "OnTouchSameTeam" ),
#endif

END_DATADESC();

LINK_ENTITY_TO_CLASS( item_teamflag, CCaptureFlag );

IMPLEMENT_AUTO_LIST( ICaptureFlagAutoList );

//=============================================================================
//
// CTF Flag functions.
//

CCaptureFlag::CCaptureFlag()
{
#ifdef CLIENT_DLL
	m_pGlowTrailEffect = NULL;
	m_pPaperTrailEffect = NULL;
	m_hOldOwner = NULL;
#else
	m_hReturnIcon = NULL;
	m_nReturnTime = 60;
	m_hInitialPlayer = NULL;
	m_hInitialParent = NULL;
	m_vecOffset.Init( 0, 0, 0 ); 

	m_iszModel = NULL_STRING;
	m_iszHudIcon = NULL_STRING;
	m_iszPaperEffect = NULL_STRING;
	m_iszTrailEffect = NULL_STRING;

	// Team specific sound throttling for Special Delivery
	for ( int i = 0; i < ARRAYSIZE( m_flNextTeamSoundTime ); i++ )
	{
		m_flNextTeamSoundTime[i] = 0.f;
	}
#endif	

	m_nNeutralType = INVADE_NEUTRAL_TYPE_DEFAULT;
	m_nScoringType = INVADE_SCORING_TEAM_SCORE;
	m_bVisibleWhenDisabled = false;
	m_bUseShotClockMode = false;
		
	UseClientSideAnimation();

	m_szModel.GetForModify()[ 0 ] = '\0';
	m_szHudIcon.GetForModify()[ 0 ] = '\0';
	m_szPaperEffect.GetForModify()[ 0 ] = '\0';
	m_szTrailEffect.GetForModify()[ 0 ] = '\0';
	m_nUseTrailEffect.Set( FLAG_EFFECTS_ALL );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CCaptureFlag::~CCaptureFlag()
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
unsigned int CCaptureFlag::GetItemID( void ) const
{
	return TF_ITEM_CAPTURE_FLAG;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const char *CCaptureFlag::GetFlagModel( void )
{
	if ( m_szModel[ 0 ] != '\0' )
	{
		if ( g_pFullFileSystem->FileExists( m_szModel.Get(), "GAME" ) )
		{
			return ( m_szModel.Get() );
		}
	}

	return TF_FLAG_MODEL;
}

void CCaptureFlag::GetHudIcon( int nTeam, char *pchName, int nBuffSize )
{
	V_snprintf( pchName, nBuffSize, "%s_%s", 
				( ( m_szHudIcon[ 0 ] != '\0' ) ? ( m_szHudIcon.Get() ) : ( TF_FLAG_ICON ) ), 
				( ( nTeam == TF_TEAM_BLUE ) ? ( "blue" ) : ( "red" ) ) );	
}

const char *CCaptureFlag::GetPaperEffect( void )
{
	if ( m_szPaperEffect[ 0 ] != '\0' )
	{
		return ( m_szPaperEffect.Get() );
	}

	return TF_FLAG_EFFECT;
}

void CCaptureFlag::GetTrailEffect( int nTeam, char *pchName, int nBuffSize )
{
	V_snprintf( pchName, nBuffSize, "effects/%s_%s.vmt", 
				( ( m_szTrailEffect[ 0 ] != '\0' ) ? ( m_szTrailEffect.Get() ) : ( TF_FLAG_TRAIL ) ), 
				( ( nTeam == TF_TEAM_RED ) ? ( "red" ) : ( "blu" ) ) );
}

//-----------------------------------------------------------------------------
// Purpose: Precache the model and sounds.
//-----------------------------------------------------------------------------
void CCaptureFlag::Precache( void )
{
	PrecacheModel( GetFlagModel() );

	PrecacheScriptSound( TF_CTF_FLAGSPAWN );
	PrecacheScriptSound( TF_CTF_ENEMY_STOLEN );
	PrecacheScriptSound( TF_CTF_ENEMY_DROPPED );
	PrecacheScriptSound( TF_CTF_ENEMY_CAPTURED );
	PrecacheScriptSound( TF_CTF_ENEMY_RETURNED );
	PrecacheScriptSound( TF_CTF_TEAM_STOLEN );
	PrecacheScriptSound( TF_CTF_TEAM_DROPPED );
	PrecacheScriptSound( TF_CTF_TEAM_CAPTURED );
	PrecacheScriptSound( TF_CTF_TEAM_RETURNED );

	PrecacheScriptSound( TF_AD_CAPTURED_SOUND );
	PrecacheScriptSound( TF_AD_ENEMY_STOLEN );
	PrecacheScriptSound( TF_AD_ENEMY_DROPPED );
	PrecacheScriptSound( TF_AD_ENEMY_CAPTURED );
	PrecacheScriptSound( TF_AD_ENEMY_RETURNED );
	PrecacheScriptSound( TF_AD_TEAM_STOLEN );
	PrecacheScriptSound( TF_AD_TEAM_DROPPED );
	PrecacheScriptSound( TF_AD_TEAM_CAPTURED );
	PrecacheScriptSound( TF_AD_TEAM_RETURNED );

	PrecacheScriptSound( TF_INVADE_ENEMY_STOLEN );
	PrecacheScriptSound( TF_INVADE_ENEMY_DROPPED );
	PrecacheScriptSound( TF_INVADE_ENEMY_CAPTURED );
	PrecacheScriptSound( TF_INVADE_TEAM_STOLEN );
	PrecacheScriptSound( TF_INVADE_TEAM_DROPPED );
	PrecacheScriptSound( TF_INVADE_TEAM_CAPTURED );
	PrecacheScriptSound( TF_INVADE_FLAG_RETURNED );

	PrecacheScriptSound( TF_RESOURCE_FLAGSPAWN );
	PrecacheScriptSound( TF_RESOURCE_ENEMY_STOLEN	);
	PrecacheScriptSound( TF_RESOURCE_ENEMY_DROPPED );
	PrecacheScriptSound( TF_RESOURCE_ENEMY_CAPTURED );
	PrecacheScriptSound( TF_RESOURCE_TEAM_STOLEN );
	PrecacheScriptSound( TF_RESOURCE_TEAM_DROPPED );
	PrecacheScriptSound( TF_RESOURCE_TEAM_CAPTURED );
	PrecacheScriptSound( TF_RESOURCE_RETURNED );
	PrecacheScriptSound( TF_RESOURCE_EVENT_ENEMY_STOLEN );
	PrecacheScriptSound( TF_RESOURCE_EVENT_ENEMY_DROPPED );
	PrecacheScriptSound( TF_RESOURCE_EVENT_TEAM_STOLEN );
	PrecacheScriptSound( TF_RESOURCE_EVENT_TEAM_DROPPED );
	PrecacheScriptSound( TF_RESOURCE_EVENT_RETURNED );
	PrecacheScriptSound( TF_RESOURCE_EVENT_NAGS );
	PrecacheScriptSound( TF_RESOURCE_EVENT_RED_CAPPED );
	PrecacheScriptSound( TF_RESOURCE_EVENT_BLUE_CAPPED );

	PrecacheParticleSystem( GetPaperEffect() );

	char szFileName[ MAX_PATH ];
	GetTrailEffect( TF_TEAM_BLUE, szFileName, sizeof( szFileName ) );
	PrecacheModel( szFileName );

	GetTrailEffect( TF_TEAM_RED, szFileName, sizeof( szFileName ) );
	PrecacheModel( szFileName );
}

#ifdef CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCaptureFlag::ShouldDraw()
{
	return BaseClass::ShouldDraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCaptureFlag::OnDataChanged( DataUpdateType_t updateType )
{
	CreateSiren();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCaptureFlag::CreateSiren( void )
{
	if ( m_hSirenEffect )
		return;

	int iAttachment = GetBaseAnimating()->LookupAttachment( "siren" );
	if ( iAttachment != INVALID_PARTICLE_ATTACHMENT )
	{
		const char* flashlightName = "cart_flashinglight";
		if ( GetTeamNumber() == TF_TEAM_RED )
		{
			flashlightName = "cart_flashinglight_red";
		}
		m_hSirenEffect = ParticleProp()->Create( flashlightName, PATTACH_POINT_FOLLOW, iAttachment );
	}
}

void CCaptureFlag::DestroySiren( void )
{
	if ( m_hSirenEffect )
	{
		ParticleProp()->StopEmission( m_hSirenEffect );
		m_hSirenEffect = NULL;
	}
}
#endif //#ifdef CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCaptureFlag::Spawn( void )
{
#ifdef GAME_DLL
	V_strncpy( m_szModel.GetForModify(), STRING( m_iszModel ), MAX_PATH );
	V_strncpy( m_szHudIcon.GetForModify(), STRING( m_iszHudIcon ), MAX_PATH );
	V_strncpy( m_szPaperEffect.GetForModify(), STRING( m_iszPaperEffect ), MAX_PATH );
	V_strncpy( m_szTrailEffect.GetForModify(), STRING( m_iszTrailEffect ), MAX_PATH );
#endif

	// Precache the model and sounds.  Set the flag model.
	Precache();
	SetModel( GetFlagModel() );

	// Set the flag solid and the size for touching.
	SetSolid( SOLID_BBOX );
	SetSolidFlags( FSOLID_NOT_SOLID | FSOLID_TRIGGER );
	SetSize( vec3_origin, vec3_origin );

	// Bloat the box for player pickup
	CollisionProp()->UseTriggerBounds( true, 24 );

	// use the initial dynamic prop "m_bStartDisabled" setting to set our own m_bDisabled flag
#ifdef GAME_DLL
	m_bDisabled = m_bStartDisabled;
	m_bStartDisabled = false;
	m_bInstantTrailRemove = false;

	// Don't allow the intelligence to fade.
	m_flFadeScale = 0.0f;
#else
	m_bDisabled = false;
#endif

	// Base class spawn.
	BaseClass::Spawn();

	// Force specific collision bounds!
	// This is to prevent a case where the flag can fall through the world
	// If the model's bounds reach outside the player's from player center
	SetCollisionBounds( Vector( -19.5f, -22.5f, -6.5f ), Vector( 19.5f, 22.5f, 6.5f ) );

#ifdef GAME_DLL
	// Save the starting position, so we can reset the flag later if need be.
	m_vecResetPos = GetAbsOrigin();
	m_vecResetAng = GetAbsAngles();

	CBaseEntity *pParent = GetParent();
	if ( pParent )
	{
		m_hInitialParent = pParent;
		m_vecOffset = GetAbsOrigin() - pParent->GetAbsOrigin();
	}

	SetFlagStatus( TF_FLAGINFO_HOME );
	ResetFlagReturnTime();
	ResetFlagNeutralTime();

	m_hInitialPlayer = NULL;

	m_bAllowOwnerPickup = true;
	m_hPrevOwner = NULL;

	m_bCaptured = false;

	const char* tags = STRING( m_iszTags );
	CSplitString splitTags( tags, " " );
	for ( int i=0; i<splitTags.Count(); ++i )
	{
		m_tags.CopyAndAddToTail( splitTags[i] );
	}
#endif

	SetDisabled( m_bDisabled );
}

void CCaptureFlag::UpdateOnRemove( void )
{
#ifndef GAME_DLL
	DestroySiren();
#endif

	// This makes the player stop glowing
	CTFPlayer *pOwnerPlayer = dynamic_cast< CTFPlayer * >( GetOwnerEntity() );
	if ( pOwnerPlayer )
	{
		pOwnerPlayer->SetItem( NULL );
	}

	BaseClass::UpdateOnRemove();
}

#ifdef GAME_DLL

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCaptureFlag::PlaySound( IRecipientFilter& filter, const char *pszString, int iTeam /*= TEAM_ANY */ )
{
	// Note:  iTeam parameter is only used for rate-limiting flag sounds based on team, and does not affect
	// who the sound is targetted at; the filter parameter is the only thing that will affect who hears this.

	if ( TFGameRules()->IsPlayingSpecialDeliveryMode() && ( ( V_strcmp( pszString, TF_RESOURCE_TEAM_DROPPED ) == 0 ) || ( V_strcmp( pszString, TF_RESOURCE_EVENT_TEAM_DROPPED ) == 0 ) ) )
	{
		// Rate limit certain flag sounds in Special Delivery
		if ( iTeam == TEAM_ANY || gpGlobals->curtime >= m_flNextTeamSoundTime[iTeam]  )
		{
			EmitSound( filter, entindex(), pszString );

			if ( iTeam != TEAM_ANY )
			{
				m_flNextTeamSoundTime[iTeam] = gpGlobals->curtime + 20.0f;
			}
		}
	}
	else
	{
		EmitSound( filter, entindex(), pszString );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Gets the return time for first down mode, if enabled and supported, otherwise 
// simply returns the passed in nReturnTime.
//-----------------------------------------------------------------------------
int CCaptureFlag::GetReturnTimeShotClockMode(int nStartReturnTime)
{
    int nReturnTime = nStartReturnTime;

    // Only enable this for specific modes.
    if (IsFlagShotClockModePossible())
    {
        if (m_bUseShotClockMode)
        {
			float flCreditTime = (gpGlobals->curtime - m_flLastPickupTime) * tf_flag_return_time_credit_factor.GetFloat()
                               + m_flLastResetDuration;
            int nPossibleCreditTime = RoundFloatToInt(flCreditTime);
            int nActualCreditTime = MAX(0, nPossibleCreditTime);
            nReturnTime = MIN(nStartReturnTime, nActualCreditTime);
        }
    }

    return nReturnTime;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCaptureFlag &CCaptureFlag::operator=( const CCaptureFlag& rhs )
{
	m_bDisabled = rhs.m_bDisabled;
	m_nType = rhs.m_nType;
	m_nReturnTime = rhs.m_nReturnTime;
	m_nUseTrailEffect = rhs.m_nUseTrailEffect;
	m_nNeutralType = rhs.m_nNeutralType;
	m_nScoringType = rhs.m_nScoringType;
	m_bVisibleWhenDisabled = rhs.m_bVisibleWhenDisabled;
	m_iszModel = rhs.m_iszModel;
	m_iszHudIcon = rhs.m_iszHudIcon;
	m_iszPaperEffect = rhs.m_iszPaperEffect;
	m_iszTrailEffect = rhs.m_iszTrailEffect;
	m_iszTags = rhs.m_iszTags;
	m_nSkin = rhs.m_nSkin;
	m_bUseShotClockMode = rhs.m_bUseShotClockMode;

	ChangeTeam( rhs.GetTeamNumber() );

	return *this;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCaptureFlag::Activate( void )
{
	BaseClass::Activate();

	m_iOriginalTeam = GetTeamNumber();
	m_nSkin = ( GetTeamNumber() == TEAM_UNASSIGNED ) ? 2 : (GetTeamNumber() - 2);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCaptureFlag* CCaptureFlag::Create( const Vector& vecOrigin, const char *pszModelName, ETFFlagType type )
{
	CCaptureFlag *pFlag = static_cast< CCaptureFlag* >( CBaseEntity::CreateNoSpawn( "item_teamflag", vecOrigin, vec3_angle, NULL ) );
	pFlag->m_iszModel = MAKE_STRING( pszModelName );
	pFlag->m_nType = type;

	DispatchSpawn( pFlag );

	return pFlag;
}
#endif


//-----------------------------------------------------------------------------
// Purpose: Reset the flag position state.
//-----------------------------------------------------------------------------
void CCaptureFlag::Reset( void )
{
#ifdef GAME_DLL
	m_bInstantTrailRemove = true;
	RemoveFlagTrail();

	// Set the flag position.
	if ( !m_hInitialParent.Get() )
	{
		SetAbsOrigin( m_vecResetPos );
		SetParent( NULL );
	}
	else
	{
		SetAbsOrigin( m_hInitialParent->GetAbsOrigin() + m_vecOffset );
		SetParent( m_hInitialParent.Get() );
	}

	SetAbsAngles( m_vecResetAng );

	// No longer dropped, if it was.
	SetFlagStatus( TF_FLAGINFO_HOME );
	ResetFlagReturnTime();
	ResetFlagNeutralTime();

	m_hInitialPlayer = NULL;

	m_bAllowOwnerPickup = true;
	m_hPrevOwner = NULL;

	if ( m_nType == TF_FLAGTYPE_INVADE || m_nType == TF_FLAGTYPE_RESOURCE_CONTROL )
	{
		ChangeTeam( m_iOriginalTeam );
		m_nSkin = ( GetTeamNumber() == TEAM_UNASSIGNED ) ? 2 : (GetTeamNumber() - 2);
	}

	SetMoveType( MOVETYPE_NONE );
#endif 
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCaptureFlag::ResetMessage( void )
{
#ifdef GAME_DLL
	if ( m_nType == TF_FLAGTYPE_CTF )
	{
		for ( int iTeam = TF_TEAM_RED; iTeam < TF_TEAM_COUNT; ++iTeam )
		{
			if ( iTeam == GetTeamNumber() )
			{
				CTeamRecipientFilter filter( iTeam, true );
				PlaySound( filter, TF_CTF_ENEMY_RETURNED );

				TFGameRules()->SendHudNotification( filter, HUD_NOTIFY_YOUR_FLAG_RETURNED );
			}
			else
			{
				CTeamRecipientFilter filter( iTeam, true );
				PlaySound( filter, TF_CTF_TEAM_RETURNED );

				TFGameRules()->SendHudNotification( filter, HUD_NOTIFY_ENEMY_FLAG_RETURNED );
			}
		}

		IGameEvent *event = gameeventmanager->CreateEvent( "teamplay_flag_event" );
		if ( event )
		{
			event->SetInt( "eventtype", TF_FLAGEVENT_RETURNED );
			event->SetInt( "priority", 8 );
			event->SetInt( "team", GetTeamNumber() );
			gameeventmanager->FireEvent( event );
		}

		// Returned sound
		CPASAttenuationFilter filter( this, TF_CTF_FLAGSPAWN );
		PlaySound( filter, TF_CTF_FLAGSPAWN );
	}
	else if ( m_nType == TF_FLAGTYPE_ATTACK_DEFEND )
	{
		for ( int iTeam = TF_TEAM_RED; iTeam < TF_TEAM_COUNT; ++iTeam )
		{
			if ( iTeam == GetTeamNumber() )
			{
				CTeamRecipientFilter filter( iTeam, true );
				PlaySound( filter, TF_AD_TEAM_RETURNED );
			}
			else
			{
				CTeamRecipientFilter filter( iTeam, true );
				PlaySound( filter, TF_AD_ENEMY_RETURNED );
			}
		}
	}
	else if ( m_nType == TF_FLAGTYPE_INVADE )
	{
		for ( int iTeam = TF_TEAM_RED; iTeam < TF_TEAM_COUNT; ++iTeam )
		{
			CTeamRecipientFilter filter( iTeam, true );
			PlaySound( filter, TF_INVADE_FLAG_RETURNED );
		}
	}
	else if ( m_nType == TF_FLAGTYPE_RESOURCE_CONTROL )
	{
		const char *pszSound = TF_RESOURCE_RETURNED;
		if ( TFGameRules() && TFGameRules()->IsHalloweenScenario( CTFGameRules::HALLOWEEN_SCENARIO_DOOMSDAY ) )
		{
			TFGameRules()->StartDoomsdayTicketsTimer();
			pszSound = TF_RESOURCE_EVENT_RETURNED;
		}

		TFGameRules()->BroadcastSound( 255, pszSound );

		IGameEvent *event = gameeventmanager->CreateEvent( "teamplay_flag_event" );
		if ( event )
		{
			event->SetInt( "eventtype", TF_FLAGEVENT_RETURNED );
			event->SetInt( "priority", 8 );
			event->SetInt( "team", GetTeamNumber() );
			gameeventmanager->FireEvent( event );
		}

		// Returned sound
		CPASAttenuationFilter filter( this, TF_RESOURCE_FLAGSPAWN );
		PlaySound( filter, TF_RESOURCE_FLAGSPAWN );
	}

	// Output.
	m_outputOnReturn.FireOutput( this, this );

	DestroyReturnIcon();
#endif
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCaptureFlag::FlagTouch( CBaseEntity *pOther )
{
	// Is the flag disabled or stolen already?
	if ( IsDisabled() || IsStolen() )
	{
		return;
	}

	// The touch from a live player.
	if ( !pOther->IsPlayer() || !pOther->IsAlive() )
	{
		return;
	}

#ifdef GAME_DLL
	// Don't let the person who threw this flag pick it up until it hits the ground.
	// This way we can throw the flag to people, but not touch it as soon as we throw it ourselves
	if(  m_hPrevOwner.Get() && m_hPrevOwner.Get() == pOther && m_bAllowOwnerPickup == false )
	{
		return;
	}
#endif

	if ( pOther->GetTeamNumber() == GetTeamNumber() )
	{
#ifdef GAME_DLL
		m_OnTouchSameTeam.FireOutput( this, this );
#endif
		
		// Does my team own this flag? If so, no touch.
		if ( m_nType == TF_FLAGTYPE_CTF )
		{
			if ( !tf_flag_return_on_touch.GetBool() )
				return;

			if ( IsHome() || IsStolen() )
				return;
		}
	}

	if ( ( m_nType == TF_FLAGTYPE_ATTACK_DEFEND || m_nType == TF_FLAGTYPE_TERRITORY_CONTROL ) &&
		   pOther->GetTeamNumber() != GetTeamNumber() )
	{
		return;
	}

	if ( ( m_nType == TF_FLAGTYPE_INVADE || m_nType == TF_FLAGTYPE_RESOURCE_CONTROL ) && ( GetTeamNumber() != TEAM_UNASSIGNED ) )
	{
		if ( pOther->GetTeamNumber() != GetTeamNumber() )
		{
			return;
		}
	}

	// Can't pickup flags during WaitingForPlayers
	if ( TFGameRules()->IsInWaitingForPlayers() )
		return;

	// Get the touching player.
	CTFPlayer *pPlayer = ToTFPlayer( pOther );
	if ( !pPlayer )
	{
		return;
	}

	if ( !pPlayer->IsAllowedToPickUpFlag() )
	{
		return;
	}

	// Is the touching player about to teleport?
	if ( pPlayer->m_Shared.InCond( TF_COND_SELECTED_TO_TELEPORT ) )
		return;

	// Don't let invulnerable players pickup flags
	if ( pPlayer->m_Shared.IsInvulnerable() )
		return;

	// Don't let stealthed spies pickup the flag
 	if ( pPlayer->m_Shared.IsStealthed() || pPlayer->m_Shared.InCond( TF_COND_STEALTHED_BLINK ) || pPlayer->m_Shared.GetPercentInvisible() > 0.25f )
 		return;

	// Don't let phased scouts pickup flags
	if ( pPlayer->m_Shared.InCond( TF_COND_PHASE ) )
		return;

	// Don't let players carry multiple flags for user-made maps with >1
	if ( pPlayer->HasTheFlag() && !tf_flag_return_on_touch.GetBool() )
		return;

#ifdef GAME_DLL
	if ( PointInRespawnRoom(pPlayer,pPlayer->WorldSpaceCenter()) )
		return;
#endif

	if ( IsDropped() && ( pOther->GetTeamNumber() == GetTeamNumber() ) && ( m_nType == TF_FLAGTYPE_CTF ) && tf_flag_return_on_touch.GetBool() )
	{
		Reset();
		ResetMessage();
#ifdef GAME_DLL
		CTF_GameStats.Event_PlayerReturnedFlag( pPlayer ); 
#endif
	}
	else
	{
		// Pick up the flag.
		PickUp( pPlayer, true );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCaptureFlag::PickUp( CTFPlayer *pPlayer, bool bInvisible )
{
	// Is the flag enabled?
	if ( IsDisabled() )
		return;

	if ( !TFGameRules()->FlagsMayBeCapped() )
		return;

#ifdef GAME_DLL
	if ( !m_bAllowOwnerPickup )
	{
		if ( m_hPrevOwner.Get() && m_hPrevOwner.Get() == pPlayer ) 
		{
			return;
		}
	}
#endif

	// Call into the base class pickup.
	BaseClass::PickUp( pPlayer, false );

	pPlayer->TeamFortress_SetSpeed();

#ifdef GAME_DLL
	
	// Update the parent to set the correct place on the model to attach the flag.
	int iAttachment = pPlayer->LookupAttachment( "flag" );
	if( iAttachment > 0 )
	{
		SetParent( pPlayer, iAttachment );
		SetLocalOrigin( vec3_origin );
		SetLocalAngles( vec3_angle );
	}

	// Remove the player's disguise if they're a spy
	if ( pPlayer->GetPlayerClass()->GetClassIndex() == TF_CLASS_SPY )
	{
		if ( pPlayer->m_Shared.InCond( TF_COND_DISGUISED ) ||
			pPlayer->m_Shared.InCond( TF_COND_DISGUISING ))
		{
			pPlayer->m_Shared.RemoveDisguise();
		}
	}

	// switch to brighter picked up skin
	m_nSkin = m_nSkin + TF_FLAG_NUMBEROFSKINS;

	// Remove the touch function.
	SetTouch( NULL );

	m_hPrevOwner = pPlayer;
	m_bAllowOwnerPickup = true;

	if ( m_hInitialPlayer.Get() == NULL )
	{
		m_hInitialPlayer = pPlayer;
	}

	if ( m_nType == TF_FLAGTYPE_CTF )
	{
		for ( int iTeam = TF_TEAM_RED; iTeam < TF_TEAM_COUNT; ++iTeam )
		{
			if ( iTeam != pPlayer->GetTeamNumber() )
			{
				CTeamRecipientFilter filter( iTeam, true );
				PlaySound( filter, TF_CTF_ENEMY_STOLEN );

				TFGameRules()->SendHudNotification( filter, HUD_NOTIFY_YOUR_FLAG_TAKEN );
			}
			else
			{
				CTeamRecipientFilter filter( iTeam, true );
				PlaySound( filter, TF_CTF_TEAM_STOLEN );

				// exclude the guy who just picked it up
				filter.RemoveRecipient( pPlayer );

				TFGameRules()->SendHudNotification( filter, HUD_NOTIFY_ENEMY_FLAG_TAKEN );
			}
		}
	}
	else if ( m_nType == TF_FLAGTYPE_ATTACK_DEFEND )
	{
		for ( int iTeam = TF_TEAM_RED; iTeam < TF_TEAM_COUNT; ++iTeam )
		{
			if ( iTeam != pPlayer->GetTeamNumber() )
			{
				CTeamRecipientFilter filter( iTeam, true );
				PlaySound( filter, TF_AD_ENEMY_STOLEN );
			}
			else
			{
				CTeamRecipientFilter filter( iTeam, true );
				PlaySound( filter, TF_AD_TEAM_STOLEN );
			}
		}
	}
	else if ( m_nType == TF_FLAGTYPE_INVADE )
	{
		for ( int iTeam = TF_TEAM_RED; iTeam < TF_TEAM_COUNT; ++iTeam )
		{
			if ( iTeam != pPlayer->GetTeamNumber() )
			{
				CTeamRecipientFilter filter( iTeam, true );
				PlaySound( filter, TF_INVADE_ENEMY_STOLEN );
			}
			else
			{
				CTeamRecipientFilter filter( iTeam, true );
				PlaySound( filter, TF_INVADE_TEAM_STOLEN );
			}
		}

		// set the flag's team to match the player's team
		ChangeTeam( pPlayer->GetTeamNumber() );
		m_nSkin = ( GetTeamNumber() == TEAM_UNASSIGNED ) ? 2 : (GetTeamNumber() - 2);
		m_nSkin = m_nSkin + TF_FLAG_NUMBEROFSKINS;
	}
	else if ( m_nType == TF_FLAGTYPE_RESOURCE_CONTROL )
	{
		// In Special delivery we only tell them about the very first flag pick up from neutral
		if ( GetTeamNumber() == TEAM_UNASSIGNED )
		{
			bool bEventMap = TFGameRules() && TFGameRules()->IsHalloweenScenario( CTFGameRules::HALLOWEEN_SCENARIO_DOOMSDAY );
			if ( bEventMap )
			{
				TFGameRules()->StopDoomsdayTicketsTimer();
			}

			for ( int iTeam = TF_TEAM_RED; iTeam < TF_TEAM_COUNT; ++iTeam )
			{
				const char *pszSound = TF_RESOURCE_ENEMY_STOLEN;

				if ( iTeam != pPlayer->GetTeamNumber() )
				{
					if ( bEventMap )
					{
						pszSound = TF_RESOURCE_EVENT_ENEMY_STOLEN;
					}
				}
				else
				{
					pszSound = TF_RESOURCE_TEAM_STOLEN;
					if ( bEventMap )
					{
						pszSound = TF_RESOURCE_EVENT_TEAM_STOLEN;
					}
				}

				CTeamRecipientFilter filter( iTeam, true );
				PlaySound( filter, pszSound, iTeam );
			}
		}

		// set the flag's team to match the player's team
		ChangeTeam( pPlayer->GetTeamNumber() );
		m_nSkin = ( GetTeamNumber() == TEAM_UNASSIGNED ) ? 2 : (GetTeamNumber() - 2);
		m_nSkin = m_nSkin + TF_FLAG_NUMBEROFSKINS;
	}

    // If the flag was at home, set the initial reset time to the max allowable time, otherwise it's to whatever it was
    // right now so we can persist that until later.
    if ( m_nFlagStatus == TF_FLAGINFO_HOME )
    {
        m_flLastResetDuration = GetMaxReturnTime();
    }
    else
    {
        m_flLastResetDuration = m_flResetTime - gpGlobals->curtime;
    }

    // Remember that this is when the item was picked up.
    m_flLastPickupTime = gpGlobals->curtime;

	int nOldFlagStatus = m_nFlagStatus;
	SetFlagStatus( TF_FLAGINFO_STOLEN, pPlayer );
	ResetFlagReturnTime();
	ResetFlagNeutralTime();

	IGameEvent *event = gameeventmanager->CreateEvent( "teamplay_flag_event" );
	if ( event )
	{
		event->SetInt( "player", pPlayer->entindex() );
		event->SetInt( "eventtype", TF_FLAGEVENT_PICKUP );
		event->SetInt( "priority", 8 );
		event->SetInt( "home", ( nOldFlagStatus == TF_FLAGINFO_HOME ) ? 1 : 0 );
		event->SetInt( "team", GetTeamNumber() );
		gameeventmanager->FireEvent( event );
	}

	pPlayer->SpeakConceptIfAllowed( MP_CONCEPT_FLAGPICKUP );

	// Output.
	m_outputOnPickUp.FireOutput( this, this );
	m_outputOnPickUp1.FireOutput( pPlayer, this );

	switch ( pPlayer->GetTeamNumber() )
	{
	case TF_TEAM_RED: 
		m_outputOnPickUpTeam1.FireOutput( this, this );
		break;
	case TF_TEAM_BLUE: 
		m_outputOnPickUpTeam2.FireOutput( this, this );
		break;
	default:
		break;
	}

	DestroyReturnIcon();

	StartFlagTrail();

	HandleFlagPickedUpInDetectionZone( pPlayer );

#endif
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCaptureFlag::Capture( CTFPlayer *pPlayer, int nCapturePoint )
{
	// Is the flag enabled?
	if ( IsDisabled() )
		return;

#ifdef GAME_DLL

	if ( m_nType == TF_FLAGTYPE_CTF )
	{
		bool bNotify = true;

		// don't play any sounds if this is going to win the round for one of the teams (victory sound will be played instead)
		if ( tf_flag_caps_per_round.GetInt() > 0 )
		{
			int nCaps = TFTeamMgr()->GetFlagCaptures( pPlayer->GetTeamNumber() );

			if ( ( nCaps >= 0 ) && ( tf_flag_caps_per_round.GetInt() - nCaps <= 1 ) )
			{
				// this cap is going to win, so don't play a sound
				bNotify = false;
			}
		}

		if ( bNotify )
		{
			for ( int iTeam = TF_TEAM_RED; iTeam < TF_TEAM_COUNT; ++iTeam )
			{
				if ( iTeam != pPlayer->GetTeamNumber() )
				{
					CTeamRecipientFilter filter( iTeam, true );
					PlaySound( filter, TF_CTF_ENEMY_CAPTURED );

					TFGameRules()->SendHudNotification( filter, HUD_NOTIFY_YOUR_FLAG_CAPTURED );
				}
				else
				{
					CTeamRecipientFilter filter( iTeam, true );
					PlaySound( filter, TF_CTF_TEAM_CAPTURED );

					TFGameRules()->SendHudNotification( filter, HUD_NOTIFY_ENEMY_FLAG_CAPTURED );
				}
			}

			if ( TFGameRules() )
			{
				TFGameRules()->HandleCTFCaptureBonus( pPlayer->GetTeamNumber() );
			}
		}

		// Reward the player
		CTF_GameStats.Event_PlayerCapturedPoint( pPlayer );

		// if someone else stole the flag, give them credit, too
		if ( m_hInitialPlayer.Get() && m_hInitialPlayer.Get() != pPlayer )
		{
			CTF_GameStats.Event_PlayerCapturedPoint( ToTFPlayer( m_hInitialPlayer.Get() ) );
			m_hInitialPlayer = NULL;
		}

		// Reward the team
		if ( tf_flag_caps_per_round.GetInt() > 0 )
		{
			TFTeamMgr()->IncrementFlagCaptures( pPlayer->GetTeamNumber() );
		}
		else
		{
			TFTeamMgr()->AddTeamScore( pPlayer->GetTeamNumber(), TF_CTF_CAPTURED_TEAM_SCORE );
		}
	}
	else if ( m_nType == TF_FLAGTYPE_ATTACK_DEFEND )
	{
		char szNumber[64];
		Q_snprintf( szNumber, sizeof(szNumber), "%d", nCapturePoint );

		for ( int iTeam = TF_TEAM_RED; iTeam < TF_TEAM_COUNT; ++iTeam )
		{
			if ( iTeam != pPlayer->GetTeamNumber() )
			{
				CTeamRecipientFilter filter( iTeam, true );
				PlaySound( filter, TF_AD_ENEMY_CAPTURED );
			}
			else
			{
				CTeamRecipientFilter filter( iTeam, true );
				PlaySound( filter, TF_AD_TEAM_CAPTURED );
			}
		}

		// Capture sound
		CBroadcastRecipientFilter filter;
		PlaySound( filter, TF_AD_CAPTURED_SOUND );

		// Reward the player
		CTF_GameStats.Event_PlayerCapturedPoint( pPlayer );

		// TFTODO:: Reward the team	
	}
	else if ( m_nType == TF_FLAGTYPE_INVADE )
	{
		for ( int iTeam = TF_TEAM_RED; iTeam < TF_TEAM_COUNT; ++iTeam )
		{
			if ( iTeam != pPlayer->GetTeamNumber() )
			{
				CTeamRecipientFilter filter( iTeam, true );
				PlaySound( filter, TF_INVADE_ENEMY_CAPTURED );
			}
			else
			{
				CTeamRecipientFilter filter( iTeam, true );
				PlaySound( filter, TF_INVADE_TEAM_CAPTURED );
			}
		}

		// Reward the player
		CTF_GameStats.Event_PlayerCapturedPoint( pPlayer );

		// Reward the team
		if ( m_nScoringType == INVADE_SCORING_TEAM_CAPTURE_COUNT )
		{
			if ( tf_flag_caps_per_round.GetInt() > 0 )
			{
				TFTeamMgr()->IncrementFlagCaptures( pPlayer->GetTeamNumber() );
			}
			else
			{
				TFTeamMgr()->AddTeamScore( pPlayer->GetTeamNumber(), TF_INVADE_CAPTURED_TEAM_SCORE );
			}
		}
		else
		{
			TFTeamMgr()->AddTeamScore( pPlayer->GetTeamNumber(), TF_INVADE_CAPTURED_TEAM_SCORE );
		}
	}
	else if ( m_nType == TF_FLAGTYPE_RESOURCE_CONTROL )
	{
		if ( TFGameRules() && TFGameRules()->IsHalloweenScenario( CTFGameRules::HALLOWEEN_SCENARIO_DOOMSDAY ) )
		{
			TFGameRules()->BroadcastSound( 255, ( pPlayer->GetTeamNumber() == TF_TEAM_RED ) ? TF_RESOURCE_EVENT_RED_CAPPED : TF_RESOURCE_EVENT_BLUE_CAPPED );
		}
		else
		{
			for ( int iTeam = TF_TEAM_RED; iTeam < TF_TEAM_COUNT; ++iTeam )
			{
				const char *pszSound = TF_RESOURCE_ENEMY_CAPTURED;
				if ( iTeam == pPlayer->GetTeamNumber() )
				{
					pszSound = TF_RESOURCE_TEAM_CAPTURED;
				}
	
				CTeamRecipientFilter filter( iTeam, true );
				PlaySound( filter, pszSound, iTeam );
			}
  		}

		// Reward the player
		CTF_GameStats.Event_PlayerCapturedPoint( pPlayer );

		// if someone else stole the flag, give them credit, too
		if ( m_hInitialPlayer.Get() && m_hInitialPlayer.Get() != pPlayer )
		{
			CTF_GameStats.Event_PlayerCapturedPoint( ToTFPlayer( m_hInitialPlayer.Get() ) );
			m_hInitialPlayer = NULL;
		}

		// Reward the team
		if ( tf_flag_caps_per_round.GetInt() > 0 )
		{
			TFTeamMgr()->IncrementFlagCaptures( pPlayer->GetTeamNumber() );
		}
		else
		{
			TFTeamMgr()->AddTeamScore( pPlayer->GetTeamNumber(), TF_RESOURCE_CAPTURED_TEAM_SCORE );
		}
	}

	IGameEvent *event = gameeventmanager->CreateEvent( "teamplay_flag_event" );
	if ( event )
	{
		event->SetInt( "player", pPlayer->entindex() );
		event->SetInt( "eventtype", TF_FLAGEVENT_CAPTURE );
		event->SetInt( "priority", 9 );
		event->SetInt( "team", GetTeamNumber() );
		gameeventmanager->FireEvent( event );
	}

	SetFlagStatus( TF_FLAGINFO_HOME );
	ResetFlagReturnTime();
	ResetFlagNeutralTime();

	m_bInstantTrailRemove = true;
	RemoveFlagTrail();
	m_nSkin = ( GetTeamNumber() == TEAM_UNASSIGNED ) ? 2 : (GetTeamNumber() - 2);

	HandleFlagCapturedInDetectionZone( pPlayer );
	HandleFlagDroppedInDetectionZone( pPlayer );

	// Reset the flag.
	BaseClass::Drop( pPlayer, true );

	Reset();

	pPlayer->TeamFortress_SetSpeed();

	if ( !TFGameRules() || !TFGameRules()->IsHalloweenScenario( CTFGameRules::HALLOWEEN_SCENARIO_DOOMSDAY ) )
	{
		pPlayer->SpeakConceptIfAllowed( MP_CONCEPT_FLAGCAPTURED );
	}
	
	// Outputs
	m_outputOnCapture.FireOutput( this, this );
	m_outputOnCapture1.FireOutput( pPlayer, this );

	switch ( pPlayer->GetTeamNumber() )
	{
	case TF_TEAM_RED: 
		m_OnCapTeam1.FireOutput( this, this );
		break;
	case TF_TEAM_BLUE: 
		m_OnCapTeam2.FireOutput( this, this );
		break;
	default:
		break;
	}

	m_bCaptured = true;
	SetNextThink( gpGlobals->curtime + TF_FLAG_THINK_TIME );

	if ( TFGameRules()->InStalemate() )
	{
		// whoever capped the flag is the winner, give them enough caps to win
		CTFTeam *pTeam = pPlayer->GetTFTeam();
		if ( !pTeam )
			return;

		// if we still need more caps to trigger a win, give them to us
		if ( pTeam->GetFlagCaptures() < tf_flag_caps_per_round.GetInt() )
		{
			pTeam->SetFlagCaptures( tf_flag_caps_per_round.GetInt() );
		}	
	}

#endif
}

//-----------------------------------------------------------------------------
// Purpose: A player drops the flag.
//-----------------------------------------------------------------------------
void CCaptureFlag::Drop( CTFPlayer *pPlayer, bool bVisible,  bool bThrown /*= false*/, bool bMessage /*= true*/ )
{
	// Is the flag enabled?
	if ( IsDisabled() )
		return;

	// Call into the base class drop.
	BaseClass::Drop( pPlayer, bVisible );

	pPlayer->TeamFortress_SetSpeed();

#ifdef GAME_DLL

	if ( bThrown )
	{
		m_bAllowOwnerPickup = false;
		m_flOwnerPickupTime = gpGlobals->curtime + TF_FLAG_OWNER_PICKUP_TIME;
	}

	// Drop from the player's center so we can guarantee that it is in a valid spot
	Vector vecStart = pPlayer->WorldSpaceCenter();
	Vector vecEnd = vecStart;
	vecEnd.z -= 8000.0f;
	trace_t trace;
	UTIL_TraceHull( vecStart, vecEnd, WorldAlignMins(), WorldAlignMaxs(), MASK_SOLID, this, COLLISION_GROUP_DEBRIS, &trace );

	if ( trace.startsolid )
	{
		DevWarning( "Dropped flag trace started solid!\nWiggle around each axis to find a safer fit!\n" );

		const float fMultipliers[ 3 ] = { 0.0f, 1.0f, -1.0f };

		// Wiggle it around on each axis to find a safe place
		for ( int z = 0; z < ARRAYSIZE( fMultipliers ) && trace.startsolid; z++ )
		{
			for ( int y = 0; y < ARRAYSIZE( fMultipliers ) && trace.startsolid; y++ )
			{
				for ( int x = 0; x < ARRAYSIZE( fMultipliers ) && trace.startsolid; x++ )
				{
					vecStart = pPlayer->WorldSpaceCenter();
					vecStart += Vector( fMultipliers[ x ] * 10.0f, fMultipliers[ y ] * 10.0f, fMultipliers[ z ] * 10.0f );

					vecEnd = vecStart;
					vecEnd.z -= 8000.0f;
					UTIL_TraceHull( vecStart, vecEnd, WorldAlignMins(), WorldAlignMaxs(), MASK_SOLID, this, COLLISION_GROUP_DEBRIS, &trace );	
				}
			}
		}
	}

	if ( trace.startsolid )
	{
		// Couldn't find a good spot... just leave it in the center of where the player died
		AssertMsg( 0, "Couldn't find a safe place to drop the flag!\n" );
		DevWarning( "Couldn't find a safe place to drop the flag!\nDropping at the player's center!\n" );

		SetAbsOrigin( pPlayer->WorldSpaceCenter() );
	}
	else
	{
		// Found a good spot for it
		SetAbsOrigin( trace.endpos );

		// If it lands on an elevator, parent it to the elevator
		if ( trace.m_pEnt && trace.m_pEnt->GetMoveType() == MOVETYPE_PUSH )
		{
			SetParent( trace.m_pEnt );
		}
	}

	if ( m_nType == TF_FLAGTYPE_CTF )
	{
		if ( bMessage  )
		{
			for ( int iTeam = TF_TEAM_RED; iTeam < TF_TEAM_COUNT; ++iTeam )
			{
				if ( iTeam != pPlayer->GetTeamNumber() )
				{
					CTeamRecipientFilter filter( iTeam, true );
					PlaySound( filter, TF_CTF_ENEMY_DROPPED );

					TFGameRules()->SendHudNotification( filter, HUD_NOTIFY_YOUR_FLAG_DROPPED );
				}
				else
				{
					CTeamRecipientFilter filter( iTeam, true );
					PlaySound( filter, TF_CTF_TEAM_DROPPED );

					TFGameRules()->SendHudNotification( filter, HUD_NOTIFY_ENEMY_FLAG_DROPPED );
				}
			}
		}
	}
	else if ( m_nType == TF_FLAGTYPE_INVADE )
	{
		if ( bMessage  )
		{
			for ( int iTeam = TF_TEAM_RED; iTeam < TF_TEAM_COUNT; ++iTeam )
			{
				if ( iTeam != pPlayer->GetTeamNumber() )
				{
					CTeamRecipientFilter filter( iTeam, true );
					PlaySound( filter, TF_INVADE_ENEMY_DROPPED );
				}
				else
				{
					CTeamRecipientFilter filter( iTeam, true );
					PlaySound( filter, TF_INVADE_TEAM_DROPPED );
				}
			}
		}

		if ( m_nNeutralType == INVADE_NEUTRAL_TYPE_HALF )
		{
			SetFlagNeutralIn( (float)GetMaxReturnTime() / 2.0 );
		}
		else if ( m_nNeutralType == INVADE_NEUTRAL_TYPE_DEFAULT )
		{
			// if our return time is less than the neutral time, we don't need a neutral time
			if ( TF_INVADE_NEUTRAL_TIME < GetMaxReturnTime() )
			{
				SetFlagNeutralIn( TF_INVADE_NEUTRAL_TIME ); 
			}
		}
	}
	else if ( m_nType == TF_FLAGTYPE_ATTACK_DEFEND )
	{
		if ( bMessage  )
		{
			for ( int iTeam = TF_TEAM_RED; iTeam < TF_TEAM_COUNT; ++iTeam )
			{
				if ( iTeam != pPlayer->GetTeamNumber() )
				{
					CTeamRecipientFilter filter( iTeam, true );
					PlaySound( filter, TF_AD_ENEMY_DROPPED );
				}
				else
				{
					CTeamRecipientFilter filter( iTeam, true );
					PlaySound( filter, TF_AD_TEAM_DROPPED );
				}
			}
		}
	}
	else if ( m_nType == TF_FLAGTYPE_RESOURCE_CONTROL )
	{
		if ( bMessage  )
		{
			const char *pszSound = TF_RESOURCE_TEAM_DROPPED;
			if ( TFGameRules() && TFGameRules()->IsHalloweenScenario( CTFGameRules::HALLOWEEN_SCENARIO_DOOMSDAY ) )
			{
				pszSound = TF_RESOURCE_EVENT_TEAM_DROPPED;
			}

			// We only care about our own team dropping it in Special Delivery
			int iTeam = pPlayer->GetTeamNumber();

			CTeamRecipientFilter filter( iTeam, true );
			PlaySound( filter, pszSound, iTeam );
		}
	}
	
	m_nSkin = m_nSkin - TF_FLAG_NUMBEROFSKINS;

	RemoveFlagTrail();

    int nMaxReturnTime = GetMaxReturnTime();
	SetFlagReturnIn( GetReturnTime( nMaxReturnTime ), nMaxReturnTime );

	// Reset the flag's angles.
	SetAbsAngles( m_vecResetAng );

	// Reset the touch function.
	SetTouch( &CCaptureFlag::FlagTouch );

	SetFlagStatus( TF_FLAGINFO_DROPPED );

	// Output.
	m_outputOnDrop.FireOutput( this, this );
	m_outputOnDrop1.FireOutput( pPlayer, this );

	CreateReturnIcon();

	// did we get dropped in a func_respawnflag zone?
	if ( PointInRespawnFlagZone( GetAbsOrigin() ) == true )
	{
		Reset();
		ResetMessage();
	}

	HandleFlagDroppedInDetectionZone( pPlayer );
#endif
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CCaptureFlag::IsDropped( void )
{ 
	return ( m_nFlagStatus == TF_FLAGINFO_DROPPED ); 
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CCaptureFlag::IsHome( void )
{ 
	return ( m_nFlagStatus == TF_FLAGINFO_HOME ); 
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CCaptureFlag::IsStolen( void )
{ 
	return ( m_nFlagStatus == TF_FLAGINFO_STOLEN ); 
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CCaptureFlag::IsDisabled( void ) const
{
	return m_bDisabled;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCaptureFlag::SetDisabled( bool bDisabled )
{
	m_bDisabled = bDisabled;

	if ( bDisabled )
	{
		if ( m_bVisibleWhenDisabled )
		{
			SetRenderMode( kRenderTransAlpha );
			SetRenderColorA( 180 );
			RemoveEffects( EF_NODRAW );
		}
		else
		{
			AddEffects( EF_NODRAW );
		}

		SetTouch( NULL );
		SetThink( NULL );
	}
	else
	{
		RemoveEffects( EF_NODRAW );
		SetRenderMode( kRenderNormal );
		SetRenderColorA( 255 );

		// The flag in RD is not actually touched by players when it's home
		SetTouch( &CCaptureFlag::FlagTouch );
		

		SetThink( &CCaptureFlag::Think );
		SetNextThink( gpGlobals->curtime );

#ifdef GAME_DLL
		if ( TFGameRules() && TFGameRules()->IsHalloweenScenario( CTFGameRules::HALLOWEEN_SCENARIO_DOOMSDAY ) && ( GetTeamNumber() == TEAM_UNASSIGNED ) )
		{
			TFGameRules()->StartDoomsdayTicketsTimer();
		}
#endif
	}
}

void CCaptureFlag::SetVisibleWhenDisabled( bool bVisible )
{
	m_bVisibleWhenDisabled = bVisible;
	SetDisabled( IsDisabled() ); 
}

//-----------------------------------------------------------------------------
// Purpose: Sets the flag status
//-----------------------------------------------------------------------------
void CCaptureFlag::SetFlagStatus( int iStatus, CBasePlayer *pNewOwner /*= NULL*/ )
{ 
#ifdef GAME_DLL
	MDLCACHE_CRITICAL_SECTION();
#endif

	if ( m_nFlagStatus != iStatus )
	{
		m_nFlagStatus = iStatus; 

		IGameEvent *pEvent = gameeventmanager->CreateEvent( "flagstatus_update" );
		if ( pEvent )
		{
#ifdef GAME_DLL
			pEvent->SetInt( "userid", pNewOwner ? pNewOwner->GetUserID() : -1 );
			pEvent->SetInt( "entindex", entindex() );
#endif
			gameeventmanager->FireEvent( pEvent );
		}
	}

#ifdef GAME_DLL
	switch ( m_nFlagStatus )
	{
	case TF_FLAGINFO_HOME:
	case TF_FLAGINFO_DROPPED:
		ResetSequence( LookupSequence("spin") );	// set spin animation if it's not being held
		break;
	case TF_FLAGINFO_STOLEN:
		ResetSequence( LookupSequence("idle") );	// set idle animation if it is being held
		break;
	default:
		AssertOnce( false );	// invalid stats
		break;
	}
#endif
}

//-----------------------------------------------------------------------------------------------
// GAME DLL Functions
//-----------------------------------------------------------------------------------------------
#ifdef GAME_DLL
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCaptureFlag::Think( void )
{
	// Is the flag enabled?
	if ( IsDisabled() )
		return;

	if ( !TFGameRules()->FlagsMayBeCapped() )
	{
		SetNextThink( gpGlobals->curtime + TF_FLAG_THINK_TIME );
		return;
	}

	if ( m_bCaptured )
	{
		m_bCaptured = false;
		SetTouch( &CCaptureFlag::FlagTouch );
	}

	if ( IsDropped() )
	{
		if ( !m_bAllowOwnerPickup )
		{
			if ( m_flOwnerPickupTime && gpGlobals->curtime > m_flOwnerPickupTime )
			{
				m_bAllowOwnerPickup = true;
			}
		}
		
		if ( m_nType == TF_FLAGTYPE_INVADE )
		{
			if ( m_flResetTime && gpGlobals->curtime > m_flResetTime )
			{
				Reset();
				ResetMessage();
			}
			else if ( m_flNeutralTime && gpGlobals->curtime > m_flNeutralTime )
			{
				// reset the team to the original team setting (when it spawned)
				ChangeTeam( m_iOriginalTeam );
				m_nSkin = ( GetTeamNumber() == TEAM_UNASSIGNED ) ? 2 : (GetTeamNumber() - 2);

				ResetFlagNeutralTime();
			}
		}
		else
		{
			if ( m_flResetTime && gpGlobals->curtime > m_flResetTime )
			{
				Reset();
				ResetMessage();
			}
		}
	}
	else if ( IsStolen() && m_hPrevOwner )
	{
		CBasePlayer *pPlayer = ToBasePlayer( m_hPrevOwner );
		if ( pPlayer )
		{
			pPlayer->SetLastObjectiveTime( gpGlobals->curtime );
		}
	}

	if ( m_flResetTime && gpGlobals->curtime > m_flResetTime )
	{
		DestroyReturnIcon();
	}

	CTFPlayer *pPlayer = ToTFPlayer( GetPrevOwner() );
	if ( pPlayer )
	{
		bool bRunning;
		float flSpeed = pPlayer->MaxSpeed();
		flSpeed *= flSpeed;
		if ( pPlayer->GetAbsVelocity().LengthSqr() >= (flSpeed* 0.1f) )
		{
			bRunning = true;
		}
		else
		{
			bRunning = false;
		}

		if ( !bRunning && m_pFlagTrail )
		{
			RemoveFlagTrail();
		}
		else if ( bRunning && !m_pFlagTrail )
		{
			StartFlagTrail();
		}
	}

	if ( m_nType == TF_FLAGTYPE_RESOURCE_CONTROL )
	{
		if ( TFGameRules() && TFGameRules()->IsHalloweenScenario( CTFGameRules::HALLOWEEN_SCENARIO_DOOMSDAY ) )
		{
			if ( TFGameRules()->DoomsdayTicketTimerElapsed() )
			{
				if ( CTFMinigameLogic::GetMinigameLogic() && CTFMinigameLogic::GetMinigameLogic()->GetActiveMinigame() )
				{
					// we've started playing a minigame so just cancel the timer
					TFGameRules()->StopDoomsdayTicketsTimer();
				}
				else
				{
					TFGameRules()->StartDoomsdayTicketsTimer(); // start the timer again
					TFGameRules()->BroadcastSound( 255, TF_RESOURCE_EVENT_NAGS );
				}
			}
		}
	}
	
	SetNextThink( gpGlobals->curtime + TF_FLAG_THINK_TIME );
}

void CCaptureFlag::CreateReturnIcon( void )
{
	if ( m_hReturnIcon.Get() )
		return;

	CBaseEntity *pReturnIcon = CBaseEntity::Create( "item_teamflag_return_icon", GetAbsOrigin() + Vector(0,0,cl_flag_return_height.GetFloat()), vec3_angle, this );
	if ( pReturnIcon )
	{
		m_hReturnIcon = pReturnIcon;
		m_hReturnIcon->SetParent( this );
	}
}

void CCaptureFlag::DestroyReturnIcon( void )
{
	if ( !m_hReturnIcon.Get() )
		return;

	UTIL_Remove( m_hReturnIcon );
	m_hReturnIcon = NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCaptureFlag::InputEnable( inputdata_t &inputdata )
{
	SetDisabled( false );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCaptureFlag::InputDisable( inputdata_t &inputdata )
{
	SetDisabled( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCaptureFlag::InputRoundActivate( inputdata_t &inputdata )
{
	CTFPlayer *pPlayer = ToTFPlayer( m_hPrevOwner.Get() );

	// If the player has a capture flag, drop it.
	if ( pPlayer && pPlayer->HasItem() && ( pPlayer->GetItem() == this ) )
	{
		Drop( pPlayer, true, false, false );
	}

	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCaptureFlag::InputForceDrop( inputdata_t &inputdata )
{
	CTFPlayer *pPlayer = ToTFPlayer( m_hPrevOwner.Get() );

	// If the player has a capture flag, drop it.
	if ( pPlayer && pPlayer->HasItem() && ( pPlayer->GetItem() == this ) )
	{
		pPlayer->DropFlag();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCaptureFlag::InternalForceReset( bool bSilent /* = false */ )
{
	if ( IsHome() )
		return;

	CTFPlayer *pPlayer = ToTFPlayer( m_hPrevOwner.Get() );

	// If the player has a capture flag, drop it.
	if ( pPlayer && pPlayer->HasItem() && ( pPlayer->GetItem() == this ) )
	{
		pPlayer->DropFlag( bSilent );
	}

	if ( !bSilent )
	{
		ResetFlag();
	}
	else
	{
		Reset();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCaptureFlag::InputForceReset( inputdata_t &inputdata )
{
	InternalForceReset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCaptureFlag::InputForceResetSilent( inputdata_t &inputdata )
{
	InternalForceReset( true );
}

void CCaptureFlag::InputForceResetAndDisableSilent( inputdata_t &inputdata )
{
	InternalForceReset( true );
	SetDisabled( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCaptureFlag::InputSetReturnTime( inputdata_t &inputdata )
{
	int nReturnTime = inputdata.value.Int();
	m_nReturnTime = ( nReturnTime >= 0 ) ? nReturnTime : 0;

	if ( IsDropped() )
	{
		// do we currently have a neutral time?
		if ( m_flNeutralTime > 0 )
		{
			if ( m_nNeutralType == INVADE_NEUTRAL_TYPE_HALF )
			{
				SetFlagNeutralIn( (float)m_nReturnTime / 2.0 );
			}
			else if ( m_nNeutralType == INVADE_NEUTRAL_TYPE_DEFAULT )
			{
				// if our return time is less than the neutral time, we don't need a neutral time
				if ( TF_INVADE_NEUTRAL_TIME < m_nReturnTime )
				{
					SetFlagNeutralIn( TF_INVADE_NEUTRAL_TIME ); 
				}
			}
		}

		SetFlagReturnIn( m_nReturnTime );
	}
}

void CCaptureFlag::InputShowTimer( inputdata_t &inputdata )
{
	int nReturnTime = inputdata.value.Int();
	m_nReturnTime = ( nReturnTime >= 0 ) ? nReturnTime : 0;

	SetFlagReturnIn( m_nReturnTime );

	CreateReturnIcon();
}

//-----------------------------------------------------------------------------
// Purpose: Always transmitted to clients
//-----------------------------------------------------------------------------
int CCaptureFlag::UpdateTransmitState()
{
	// ALWAYS transmit to all clients.
	return SetTransmitState( FL_EDICT_ALWAYS );
}


#else

float CCaptureFlag::GetReturnProgress()
{
	float flEventTime = MAX( m_flResetTime.m_Value, m_flNeutralTime.m_Value );

	return ( 1.0 - ( ( flEventTime - gpGlobals->curtime ) / m_flMaxResetTime ) );
}


void CCaptureFlag::Simulate( void )
{
	BaseClass::Simulate();

	ManageTrailEffects();

	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();
	if ( m_hPrevOwner && m_hPrevOwner->IsPlayer() && pLocalPlayer && pLocalPlayer->m_Shared.IsFullyInvisible() && !IsEffectActive( EF_NODRAW ) )
	{
		C_TFPlayer *pTFOwner = ToTFPlayer( m_hPrevOwner );
		if ( pTFOwner && pTFOwner != pLocalPlayer )
		{
			AddEffects( EF_NODRAW );
		}
	}
	else if ( IsEffectActive( EF_NODRAW ) && ( IsStolen() || IsDropped() ) )
	{
		RemoveEffects( EF_NODRAW );
	}
}

void CCaptureFlag::ManageTrailEffects( void )
{
	if ( ( m_nUseTrailEffect == FLAG_EFFECTS_NONE ) || ( m_nUseTrailEffect == FLAG_EFFECTS_COLORONLY ) )
		return;

	if ( m_nFlagStatus == TF_FLAGINFO_STOLEN )
	{
		if ( GetPrevOwner() )
		{
			CTFPlayer *pPlayer = ToTFPlayer( GetPrevOwner() );

			if ( pPlayer )
			{
				if ( pPlayer->GetAbsVelocity().Length() >= pPlayer->MaxSpeed() * 0.2f )	
				{
					if ( m_pPaperTrailEffect == NULL )
					{
						m_pPaperTrailEffect = ParticleProp()->Create( GetPaperEffect(), PATTACH_ABSORIGIN_FOLLOW );
					}
				}
				else
				{
					if ( m_pPaperTrailEffect )
					{
						ParticleProp()->StopEmission( m_pPaperTrailEffect );
						m_pPaperTrailEffect = NULL;
					}
				}
			}
		}

 	}

	else
	{
		if ( m_pPaperTrailEffect )
		{
			ParticleProp()->StopEmission( m_pPaperTrailEffect );
			m_pPaperTrailEffect = NULL;
		}
	}
}


#endif


LINK_ENTITY_TO_CLASS( item_teamflag_return_icon, CCaptureFlagReturnIcon );

IMPLEMENT_NETWORKCLASS_ALIASED( CaptureFlagReturnIcon, DT_CaptureFlagReturnIcon )

BEGIN_NETWORK_TABLE( CCaptureFlagReturnIcon, DT_CaptureFlagReturnIcon )
END_NETWORK_TABLE()

CCaptureFlagReturnIcon::CCaptureFlagReturnIcon()
{
#ifdef CLIENT_DLL
	m_pReturnProgressMaterial_Empty = NULL;
	m_pReturnProgressMaterial_Full = NULL;
#endif
}

#ifdef GAME_DLL

void CCaptureFlagReturnIcon::Spawn( void )
{
	BaseClass::Spawn();

	UTIL_SetSize( this, Vector(-8,-8,-8), Vector(8,8,8) );

	CollisionProp()->SetCollisionBounds( Vector( -50, -50, -50 ), Vector( 50, 50, 50 ) );
}

int CCaptureFlagReturnIcon::UpdateTransmitState( void )
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}

//-----------------------------------------------------------------------------
// Purpose: Start the flag trail
//-----------------------------------------------------------------------------
void CCaptureFlag::StartFlagTrail( void )
{
	if ( ( m_nUseTrailEffect == FLAG_EFFECTS_NONE ) || ( m_nUseTrailEffect == FLAG_EFFECTS_PAPERONLY ) )
		return;

	if ( m_pFlagTrail )
		return;

	CTFPlayer *pPlayer = ToTFPlayer( GetPrevOwner() );

	if ( pPlayer )
	{
		if ( !m_pFlagTrail )
		{
			char szTrailTeamName[ MAX_PATH ];
			GetTrailEffect( pPlayer->GetTeamNumber(), szTrailTeamName, sizeof( szTrailTeamName ) );

			CSpriteTrail *pTempTrail = NULL;

			pTempTrail = CSpriteTrail::SpriteTrailCreate( szTrailTeamName, GetAbsOrigin(), true );
			pTempTrail->SetTransmit( false );
			pTempTrail->FollowEntity( this );
			pTempTrail->SetTransparency( kRenderTransAlpha, 255, 255, 255, TF_FLAG_TRAIL_ALPHA, kRenderFxNone );
			pTempTrail->SetStartWidth( 32 );
			pTempTrail->SetTextureResolution( 1.0f / ( 96.0f * 1.0f ) );
			pTempTrail->SetLifeTime( 0.70 );
			pTempTrail->TurnOn();
			pTempTrail->SetAttachment( this, 0 );
			m_pFlagTrail = pTempTrail;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Fade and kill the trail
//-----------------------------------------------------------------------------
void CCaptureFlag::RemoveFlagTrail( void )
{
	if ( !m_pFlagTrail )
		return;

	if (m_pFlagTrail)
	{
		if (m_flFlagTrailLife <= 0 || m_bInstantTrailRemove == true )
		{
			UTIL_Remove( m_pFlagTrail);
			m_flFlagTrailLife = 1.0f;
		}	
		else	
		{
			float fAlpha = TF_FLAG_TRAIL_ALPHA * m_flFlagTrailLife;

			CSpriteTrail *pTempTrail = dynamic_cast< CSpriteTrail*>( m_pFlagTrail.Get() );

			if ( pTempTrail )
			{
				pTempTrail->SetBrightness( int(fAlpha) );
			}

			m_flFlagTrailLife = m_flFlagTrailLife - 0.1f;
			SetContextThink( &CCaptureFlag::RemoveFlagTrail, gpGlobals->curtime + 0.05, "FadeFlagTrail");
		}
	}
	m_bInstantTrailRemove = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCaptureFlag::AddFollower( CTFBot* pBot )
{
	if ( !m_followers.HasElement( pBot ) )
	{
		m_followers.AddToTail( pBot );
		for ( int i=0; i<m_tags.Count(); ++i )
		{
			pBot->AddTag( m_tags[i] );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCaptureFlag::RemoveFollower( CTFBot* pBot )
{
	int index = m_followers.Find( pBot );
	if ( index != m_followers.InvalidIndex() )
	{
		m_followers.Remove( index );
		for ( int i=0; i<m_tags.Count(); ++i )
		{
			pBot->RemoveTag( m_tags[i] );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CCaptureFlag::GetReturnTime( int nMaxReturnTime )
{
    return GetReturnTimeShotClockMode( nMaxReturnTime );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CCaptureFlag::GetMaxReturnTime( void )
{
    return m_nReturnTime;
}
#endif // GAME_DLL

#ifdef CLIENT_DLL

typedef struct
{
	float maxProgress;

	float vert1x;
	float vert1y;
	float vert2x;
	float vert2y;

	int swipe_dir_x;
	int swipe_dir_y;
} progress_segment_t;


// This defines the properties of the 8 circle segments
// in the circular progress bar.
progress_segment_t Segments[8] = 
{
	{ 0.125, 0.5, 0.0, 1.0, 0.0, 1, 0 },
	{ 0.25,	 1.0, 0.0, 1.0, 0.5, 0, 1 },
	{ 0.375, 1.0, 0.5, 1.0, 1.0, 0, 1 },
	{ 0.50,	 1.0, 1.0, 0.5, 1.0, -1, 0 },
	{ 0.625, 0.5, 1.0, 0.0, 1.0, -1, 0 },
	{ 0.75,	 0.0, 1.0, 0.0, 0.5, 0, -1 },
	{ 0.875, 0.0, 0.5, 0.0, 0.0, 0, -1 },
	{ 1.0,	 0.0, 0.0, 0.5, 0.0, 1, 0 },
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
RenderGroup_t CCaptureFlagReturnIcon::GetRenderGroup( void ) 
{	
	return RENDER_GROUP_TRANSLUCENT_ENTITY;	
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCaptureFlagReturnIcon::GetRenderBounds( Vector& theMins, Vector& theMaxs )
{
	theMins.Init( -20, -20, -20 );
	theMaxs.Init(  20,  20,  20 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CCaptureFlagReturnIcon::DrawModel( int flags )
{
	int nRetVal = BaseClass::DrawModel( flags );
	
	DrawReturnProgressBar();

	return nRetVal;
}

//-----------------------------------------------------------------------------
// Purpose: Draw progress bar above the flag indicating when it will return
//-----------------------------------------------------------------------------
void CCaptureFlagReturnIcon::DrawReturnProgressBar( void )
{
	CCaptureFlag *pFlag = dynamic_cast< CCaptureFlag * > ( GetOwnerEntity() );

	if ( !pFlag )
		return;

	// Don't draw if this flag is not going to reset
	if ( pFlag->GetMaxResetTime() <= 0 )
		return;

	if ( !TFGameRules()->FlagsMayBeCapped() )
		return;

	if ( !m_pReturnProgressMaterial_Full )
	{
		m_pReturnProgressMaterial_Full = materials->FindMaterial( "VGUI/flagtime_full", TEXTURE_GROUP_VGUI );
	}

	if ( !m_pReturnProgressMaterial_Empty )
	{
		m_pReturnProgressMaterial_Empty = materials->FindMaterial( "VGUI/flagtime_empty", TEXTURE_GROUP_VGUI );
	}

	if ( !m_pReturnProgressMaterial_Full || !m_pReturnProgressMaterial_Empty )
	{
		return;
	}

	CMatRenderContextPtr pRenderContext( materials );

	Vector vOrigin = GetAbsOrigin();
	QAngle vAngle = vec3_angle;

	// Align it towards the viewer
	Vector vUp = CurrentViewUp();
	Vector vRight = CurrentViewRight();
	if ( fabs( vRight.z ) > 0.95 )	// don't draw it edge-on
		return;

	vRight.z = 0;
	VectorNormalize( vRight );

	float flSize = cl_flag_return_size.GetFloat();

	unsigned char ubColor[4];
	ubColor[3] = 255;

	switch( pFlag->GetTeamNumber() )
	{
	case TF_TEAM_RED:
		ubColor[0] = 255;
		ubColor[1] = 0;
		ubColor[2] = 0;
		break;
	case TF_TEAM_BLUE:
		ubColor[0] = 0;
		ubColor[1] = 0;
		ubColor[2] = 255;
		break;
	default:
		ubColor[0] = 200;
		ubColor[1] = 200;
		ubColor[2] = 200;
		break;
	}

	// First we draw a quad of a complete icon, background
	CMeshBuilder meshBuilder;

	pRenderContext->Bind( m_pReturnProgressMaterial_Empty );
	IMesh *pMesh = pRenderContext->GetDynamicMesh();

	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

	meshBuilder.Color4ubv( ubColor );
	meshBuilder.TexCoord2f( 0,0,0 );
	meshBuilder.Position3fv( (vOrigin + (vRight * -flSize) + (vUp * flSize)).Base() );
	meshBuilder.AdvanceVertex();

	meshBuilder.Color4ubv( ubColor );
	meshBuilder.TexCoord2f( 0,1,0 );
	meshBuilder.Position3fv( (vOrigin + (vRight * flSize) + (vUp * flSize)).Base() );
	meshBuilder.AdvanceVertex();

	meshBuilder.Color4ubv( ubColor );
	meshBuilder.TexCoord2f( 0,1,1 );
	meshBuilder.Position3fv( (vOrigin + (vRight * flSize) + (vUp * -flSize)).Base() );
	meshBuilder.AdvanceVertex();

	meshBuilder.Color4ubv( ubColor );
	meshBuilder.TexCoord2f( 0,0,1 );
	meshBuilder.Position3fv( (vOrigin + (vRight * -flSize) + (vUp * -flSize)).Base() );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();

	pMesh->Draw();

	float flProgress = pFlag->GetReturnProgress();

	pRenderContext->Bind( m_pReturnProgressMaterial_Full );
	pMesh = pRenderContext->GetDynamicMesh();

	vRight *= flSize * 2;
	vUp *= flSize * -2;

	// Next we're drawing the circular progress bar, in 8 segments
	// For each segment, we calculate the vertex position that will draw
	// the slice.
	int i;
	for ( i=0;i<8;i++ )
	{
		if ( flProgress < Segments[i].maxProgress )
		{
			CMeshBuilder meshBuilder_Full;

			meshBuilder_Full.Begin( pMesh, MATERIAL_TRIANGLES, 3 );

			// vert 0 is ( 0.5, 0.5 )
			meshBuilder_Full.Color4ubv( ubColor );
			meshBuilder_Full.TexCoord2f( 0, 0.5, 0.5 );
			meshBuilder_Full.Position3fv( vOrigin.Base() );
			meshBuilder_Full.AdvanceVertex();

			// Internal progress is the progress through this particular slice
			float internalProgress = RemapVal( flProgress, Segments[i].maxProgress - 0.125, Segments[i].maxProgress, 0.0, 1.0 );
			internalProgress = clamp( internalProgress, 0.0f, 1.0f );

			// Calculate the x,y of the moving vertex based on internal progress
			float swipe_x = Segments[i].vert2x - ( 1.0 - internalProgress ) * 0.5 * Segments[i].swipe_dir_x;
			float swipe_y = Segments[i].vert2y - ( 1.0 - internalProgress ) * 0.5 * Segments[i].swipe_dir_y;

			// vert 1 is calculated from progress
			meshBuilder_Full.Color4ubv( ubColor );
			meshBuilder_Full.TexCoord2f( 0, swipe_x, swipe_y );
			meshBuilder_Full.Position3fv( (vOrigin + (vRight * ( swipe_x - 0.5 ) ) + (vUp *( swipe_y - 0.5 ) ) ).Base() );
			meshBuilder_Full.AdvanceVertex();

			// vert 2 is ( Segments[i].vert1x, Segments[i].vert1y )
			meshBuilder_Full.Color4ubv( ubColor );
			meshBuilder_Full.TexCoord2f( 0, Segments[i].vert2x, Segments[i].vert2y );
			meshBuilder_Full.Position3fv( (vOrigin + (vRight * ( Segments[i].vert2x - 0.5 ) ) + (vUp *( Segments[i].vert2y - 0.5 ) ) ).Base() );
			meshBuilder_Full.AdvanceVertex();

			meshBuilder_Full.End();

			pMesh->Draw();
		}
	}
}

#endif
