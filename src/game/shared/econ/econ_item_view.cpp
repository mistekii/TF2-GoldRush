//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "econ_item_view.h"
#include "econ_item_system.h"
#include "econ_item_description.h"
#include "econ_item_inventory.h"

#include "rtime.h"
#include "econ_gcmessages.h"
#include "gamestringpool.h"

// For localization
#include "tier3/tier3.h"
#include "vgui/ILocalize.h"

#include "isaverestore.h"
#include "dt_utlvector_send.h"
#include "dt_utlvector_recv.h"
#include <vgui_controls/Panel.h>

#ifdef CLIENT_DLL
#ifndef DEDICATED
#include "vgui/IScheme.h"
#endif
#endif

#if defined(TF_CLIENT_DLL)
#include "tf_duel_summary.h"
#include "econ_contribution.h"
#include "tf_player_info.h"
#include "tf_gcmessages.h"
#include "c_tf_freeaccount.h"
#include "c_tf_player.h"

static ConVar tf_hide_custom_decals( "tf_hide_custom_decals", "0", FCVAR_ARCHIVE );
#endif

#include "materialsystem/itexture.h"
#include "materialsystem/itexturecompositor.h"

#include "activitylist.h"

#ifdef CLIENT_DLL
#include "gc_clientsystem.h"
#endif // CLIENT_DLL


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"




// Networking tables for attributes
BEGIN_NETWORK_TABLE_NOBASE( CEconItemAttribute, DT_ScriptCreatedAttribute )

	// Note: we are networking the value as an int, even though it's a "float", because really it isn't
	// a float.  It's 32 raw bits.

#ifndef CLIENT_DLL
	SendPropInt( SENDINFO(m_iAttributeDefinitionIndex), -1, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO_NAME(m_flValue, m_iRawValue32), 32, SPROP_UNSIGNED ),
#if ENABLE_ATTRIBUTE_CURRENCY_TRACKING
	SendPropInt( SENDINFO(m_nRefundableCurrency), -1, SPROP_UNSIGNED ),
#endif // ENABLE_ATTRIBUTE_CURRENCY_TRACKING
#else
	RecvPropInt( RECVINFO(m_iAttributeDefinitionIndex) ),
	RecvPropInt( RECVINFO_NAME(m_flValue, m_iRawValue32) ),
	RecvPropFloat( RECVINFO(m_flValue), SPROP_NOSCALE ), // for demo compatibility only
#if ENABLE_ATTRIBUTE_CURRENCY_TRACKING
	RecvPropInt( RECVINFO(m_nRefundableCurrency) ),
#endif // ENABLE_ATTRIBUTE_CURRENCY_TRACKING
#endif
END_NETWORK_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEconItemAttribute::CEconItemAttribute( void )
{
 	Init();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEconItemAttribute::CEconItemAttribute( const attrib_definition_index_t iAttributeIndex, float flValue )
{
	Init();

	m_iAttributeDefinitionIndex = iAttributeIndex;
	SetValue( flValue );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEconItemAttribute::CEconItemAttribute( const attrib_definition_index_t iAttributeIndex, uint32 unValue )
{
	Init();

	m_iAttributeDefinitionIndex = iAttributeIndex;
	SetIntValue( unValue );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconItemAttribute::SetValue( float flValue )
{
//	Assert( GetStaticData() && GetStaticData()->IsStoredAsFloat() );
	m_flValue = flValue;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconItemAttribute::SetIntValue( uint32 unValue )
{
	// @note we don't check the storage type here, because this is how it is set from the data file
	// Note that numbers approaching two billion cannot be stored in a float
	// representation because they will map to NaNs. Numbers below 16 million
	// will fail if denormals are disabled.
	m_flValue = *(float*)&unValue;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconItemAttribute::Init( void )
{
	m_iAttributeDefinitionIndex = INVALID_ATTRIB_DEF_INDEX;
	m_flValue = 0.0f;

#if ENABLE_ATTRIBUTE_CURRENCY_TRACKING
	m_nRefundableCurrency = 0;
#endif // ENABLE_ATTRIBUTE_CURRENCY_TRACKING
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconItemAttribute::operator=( const CEconItemAttribute &val )
{
	m_iAttributeDefinitionIndex = val.m_iAttributeDefinitionIndex;
	m_flValue = val.m_flValue;

#if ENABLE_ATTRIBUTE_CURRENCY_TRACKING
	m_nRefundableCurrency = val.m_nRefundableCurrency;
#endif // ENABLE_ATTRIBUTE_CURRENCY_TRACKING
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const CEconItemAttributeDefinition *CEconItemAttribute::GetStaticData( void ) const
{ 
	return GetItemSchema()->GetAttributeDefinition( m_iAttributeDefinitionIndex ); 
}

BEGIN_NETWORK_TABLE_NOBASE( CAttributeList, DT_AttributeList )
#if !defined( CLIENT_DLL )
SendPropUtlVectorDataTable( m_Attributes, MAX_ATTRIBUTES_PER_ITEM, DT_ScriptCreatedAttribute ),
#else
RecvPropUtlVectorDataTable( m_Attributes, MAX_ATTRIBUTES_PER_ITEM, DT_ScriptCreatedAttribute ),
#endif // CLIENT_DLL
END_NETWORK_TABLE()

BEGIN_DATADESC_NO_BASE( CAttributeList )
END_DATADESC()

//===========================================================================================================================
// SCRIPT CREATED ITEMS
//===========================================================================================================================
BEGIN_NETWORK_TABLE_NOBASE( CEconItemView, DT_ScriptCreatedItem )
#if !defined( CLIENT_DLL )
	SendPropInt( SENDINFO( m_iItemDefinitionIndex ), 20, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iEntityLevel ), 8 ),
	//SendPropInt( SENDINFO( m_iItemID ), 64, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iItemIDHigh ), 32, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iItemIDLow ), 32, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iAccountID ), 32, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iEntityQuality ), 5 ),
	SendPropBool( SENDINFO( m_bInitialized ) ),
	SendPropBool( SENDINFO( m_bOnlyIterateItemViewAttributes) ),
	SendPropDataTable(SENDINFO_DT(m_AttributeList), &REFERENCE_SEND_TABLE(DT_AttributeList)),
	SendPropInt( SENDINFO( m_iTeamNumber ) ),
	SendPropDataTable(SENDINFO_DT( m_NetworkedDynamicAttributesForDemos ), &REFERENCE_SEND_TABLE( DT_AttributeList ) ),
#else
	RecvPropInt( RECVINFO( m_iItemDefinitionIndex ) ),
	RecvPropInt( RECVINFO( m_iEntityLevel ) ),
	//RecvPropInt( RECVINFO( m_iItemID ) ),
	RecvPropInt( RECVINFO( m_iItemIDHigh ) ),
	RecvPropInt( RECVINFO( m_iItemIDLow ) ),
	RecvPropInt( RECVINFO( m_iAccountID ) ),
	RecvPropInt( RECVINFO( m_iEntityQuality ) ),
	RecvPropBool( RECVINFO( m_bInitialized ) ),
	RecvPropBool( RECVINFO( m_bOnlyIterateItemViewAttributes ) ),
	RecvPropDataTable(RECVINFO_DT(m_AttributeList), 0, &REFERENCE_RECV_TABLE(DT_AttributeList)),
	RecvPropInt( RECVINFO( m_iTeamNumber ) ),
	RecvPropDataTable( RECVINFO_DT( m_NetworkedDynamicAttributesForDemos ), 0, &REFERENCE_RECV_TABLE( DT_AttributeList ) ),
#endif // CLIENT_DLL
END_NETWORK_TABLE()

BEGIN_DATADESC_NO_BASE( CEconItemView )
	DEFINE_FIELD( m_iItemDefinitionIndex, FIELD_INTEGER ),
	DEFINE_FIELD( m_iEntityQuality, FIELD_INTEGER ),
	DEFINE_FIELD( m_iEntityLevel, FIELD_INTEGER ),
	DEFINE_FIELD( m_iItemID, FIELD_INTEGER ),
	//	DEFINE_FIELD( m_wszItemName, FIELD_STRING ),		Regenerated post-save
	//	DEFINE_FIELD( m_szItemName, FIELD_STRING ),		Regenerated post-save
	//	DEFINE_FIELD( m_szAttributeDescription, FIELD_STRING ),		Regenerated post-save
	//  m_AttributeLineColors	// Regenerated post-save
	//  m_Attributes			// Custom handling in Save()/Restore()
	DEFINE_FIELD( m_bInitialized, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bOnlyIterateItemViewAttributes, FIELD_BOOLEAN ),
	DEFINE_EMBEDDED( m_AttributeList ),
	DEFINE_EMBEDDED( m_NetworkedDynamicAttributesForDemos ),
END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEconItemView::CEconItemView( void )
{
	m_iItemDefinitionIndex = INVALID_ITEM_DEF_INDEX;
	m_iEntityQuality = (int)AE_UNDEFINED;
	m_iEntityLevel = 0;
	SetItemID( INVALID_ITEM_ID );
	m_iInventoryPosition = 0;
	m_bInitialized = false;
	m_bOnlyIterateItemViewAttributes = false;
	m_iAccountID = 0;
	m_pNonSOEconItem = NULL;
	m_bColorInit = false;
	m_flOverrideIndex = 0.f;
#if defined( CLIENT_DLL )
	m_bIsTradeItem = false;
	m_iEntityQuantity = 1;
	m_unClientFlags = 0;
	m_unOverrideStyle = INVALID_STYLE_INDEX;
	m_unOverrideOrigin = kEconItemOrigin_Max;
#endif
#if BUILD_ITEM_NAME_AND_DESC
	m_pDescription = NULL;
	m_pszGrayedOutReason = NULL;
#endif

#ifdef CLIENT_DLL
	m_pWeaponSkinBase = NULL;
	m_pWeaponSkinBaseCompositor = NULL;
	m_iLastGeneratedTeamSkin = TF_TEAM_RED;
	m_bWeaponSkinUseHighRes = false;
	m_bWeaponSkinUseLowRes = false;
#endif // CLIENT_DLL

	m_iTeamNumber = TF_TEAM_RED;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEconItemView::~CEconItemView( void )
{
#ifdef CLIENT_DLL
	SafeRelease( &m_pWeaponSkinBase );
	SafeRelease( &m_pWeaponSkinBaseCompositor );
#endif // CLIENT_DLL

	DestroyAllAttributes();

#if BUILD_ITEM_NAME_AND_DESC
	MarkDescriptionDirty();
	free( m_pszGrayedOutReason );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEconItemView::CEconItemView( const CEconItemView &src )
{
#if BUILD_ITEM_NAME_AND_DESC
	m_pDescription = NULL;
	m_pszGrayedOutReason = NULL;
#endif

#ifdef CLIENT_DLL
	// Need to null these out here for initial behavior.
	m_pWeaponSkinBase = NULL;
	m_pWeaponSkinBaseCompositor = NULL;
	m_nWeaponSkinGeneration = 0;
	m_iLastGeneratedTeamSkin = TF_TEAM_RED;
	m_unWeaponSkinBaseCreateFlags = 0;
	m_bWeaponSkinUseHighRes = src.m_bWeaponSkinUseHighRes;
	m_bWeaponSkinUseLowRes  = src.m_bWeaponSkinUseLowRes;
#endif //CLIENT_DLL

	m_iTeamNumber = src.m_iTeamNumber; // keep the same team from the first creation

	*this = src;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconItemView::Init( int iDefIndex, int iQuality, int iLevel, uint32 iAccountID )
{
	m_AttributeList.Init();
	m_NetworkedDynamicAttributesForDemos.Init();

	m_iItemDefinitionIndex = iDefIndex;
	CEconItemDefinition *pData = GetStaticData();
	if ( !pData )
	{
		// We've got an item that we don't have static data for.
		return;
	}

	SetItemID( INVALID_ITEM_ID );
	m_bInitialized = true;
	m_iAccountID = iAccountID;

	if ( iQuality == AE_USE_SCRIPT_VALUE )
	{
		m_iEntityQuality = pData->GetQuality();

		// Kyle says: this is a horrible hack because AE_UNDEFINED will get stuffed into a uint8 when
		// loaded into the item definition, but then read back out into a regular int here.
		if ( m_iEntityQuality == (uint8)AE_UNDEFINED )
		{
			m_iEntityQuality = (int)AE_NORMAL;
		}
	}
	else if ( iQuality == k_unItemQuality_Any )
	{
		m_iEntityQuality = (int)AE_RARITY1;
	}
	else
	{
		m_iEntityQuality = iQuality;
	}

	if ( iLevel == AE_USE_SCRIPT_VALUE )
	{
		m_iEntityLevel = pData->RollItemLevel();
	}
	else
	{
		m_iEntityLevel = iLevel;
	}

	// We made changes to quality, level, etc. so mark the description as dirty.
	MarkDescriptionDirty();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEconItemView& CEconItemView::operator=( const CEconItemView& src )
{
	m_iItemDefinitionIndex = src.m_iItemDefinitionIndex;
	m_iEntityQuality = src.m_iEntityQuality;
	m_iEntityLevel = src.m_iEntityLevel;
	SetItemID( src.GetItemID() );
	m_iInventoryPosition = src.m_iInventoryPosition;
	m_bInitialized = src.m_bInitialized;
	m_bOnlyIterateItemViewAttributes = src.m_bOnlyIterateItemViewAttributes;
	m_iAccountID = src.m_iAccountID;
	SetNonSOEconItem(src.m_pNonSOEconItem);
	m_bColorInit = false;		// reset Color init
	m_flOverrideIndex = 0.f;
#ifdef CLIENT_DLL
	m_iLastGeneratedTeamSkin = src.m_iLastGeneratedTeamSkin;
	m_bIsTradeItem = src.m_bIsTradeItem;
	m_iEntityQuantity = src.m_iEntityQuantity;
	m_unClientFlags = src.m_unClientFlags;
	m_unOverrideStyle = src.m_unOverrideStyle;
	m_unOverrideOrigin = src.m_unOverrideOrigin;

	SafeAssign( &m_pWeaponSkinBase, src.m_pWeaponSkinBase );
	SafeAssign( &m_pWeaponSkinBaseCompositor, src.m_pWeaponSkinBaseCompositor );

	m_nWeaponSkinGeneration = src.m_nWeaponSkinGeneration;
	m_unWeaponSkinBaseCreateFlags = src.m_unWeaponSkinBaseCreateFlags;

	m_bWeaponSkinUseHighRes = src.m_bWeaponSkinUseHighRes;
	m_bWeaponSkinUseLowRes  = src.m_bWeaponSkinUseLowRes;

#endif // CLIENT_DLL

	m_iTeamNumber = src.m_iTeamNumber; // keep the same team from the first creation

	DestroyAllAttributes();

	m_AttributeList = src.m_AttributeList;
	m_NetworkedDynamicAttributesForDemos = src.m_NetworkedDynamicAttributesForDemos;

	// TODO: Copying the description pointer and refcounting it would work also.
	MarkDescriptionDirty();

	// Clear out any overrides we currently have, they'll get reset up on demand.
	ResetMaterialOverrides();
	return *this;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEconItemView::operator==( const CEconItemView &other ) const
{
	if ( IsValid() != other.IsValid() )
		return false;
	if ( ( GetItemID() != INVALID_ITEM_ID || other.GetItemID() != INVALID_ITEM_ID ) && GetItemID() != other.GetItemID() )
		return false;
	if ( GetItemDefIndex() != other.GetItemDefIndex() )
		return false;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
GameItemDefinition_t *CEconItemView::GetStaticData( void ) const
{ 
	CEconItemDefinition	 *pRet		= GetItemSchema()->GetItemDefinition( m_iItemDefinitionIndex );
	GameItemDefinition_t *pTypedRet = dynamic_cast<GameItemDefinition_t *>( pRet );

	AssertMsg( pRet == pTypedRet, "Item definition of inappropriate type." );

	return pTypedRet;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int32 CEconItemView::GetQuality() const
{
	return GetSOCData()
		 ? GetSOCData()->GetQuality()
#ifdef TF_CLIENT_DLL
		 : GetFlags() & kEconItemFlagClient_StoreItem
		 ? AE_UNIQUE
#endif
		 : GetOrigin() != kEconItemOrigin_Invalid
		 ? GetItemQuality()
		 : AE_NORMAL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
style_index_t CEconItemView::GetStyle() const
{
	return GetItemStyle();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
uint8 CEconItemView::GetFlags() const
{
	uint8 unSOCFlags = GetSOCData() ? GetSOCData()->GetFlags() : 0;

#if !defined( GAME_DLL )
	return unSOCFlags | m_unClientFlags;
#else // defined( GAME_DLL )
	return unSOCFlags;
#endif // !defined( GAME_DLL )
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
eEconItemOrigin CEconItemView::GetOrigin() const
{
#ifdef CLIENT_DLL
	if( m_unOverrideOrigin != kEconItemOrigin_Max )
	{
		return m_unOverrideOrigin;
	}
#endif//CLIENT_DLL

	return GetSOCData() ? GetSOCData()->GetOrigin() : kEconItemOrigin_Invalid;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CEconItemView::GetQuantity() const
{
	return GetItemQuantity();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CEconItemView::GetCustomName() const
{
	return GetSOCData() ? GetSOCData()->GetCustomName() : NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CEconItemView::GetCustomDesc() const
{
	return GetSOCData() ? GetSOCData()->GetCustomDesc() : NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconItemView::IterateAttributes( class IEconItemAttributeIterator *pIterator ) const
{
	Assert( pIterator );

	// Note if we have network attribs, because m_NetworkedDynamicAttributesForDemos might be the iterator
	// which we're about to fill up below.
	const bool bHasNetworkedAttribsForDemos = m_NetworkedDynamicAttributesForDemos.GetNumAttributes() > 0;

	// First, we iterate over the attributes we have local copies of. If we have any attribute
	// values here they'll override whatever values we would otherwise have pulled from our
	// definition/CEconItem.
	const CAttributeList *pAttrList = GetAttributeList();
	if ( pAttrList )
	{
		pAttrList->IterateAttributes( pIterator );
	}

	if ( m_bOnlyIterateItemViewAttributes )
		return;

	// This wraps any other iterator class and will prevent double iteration of any attributes
	// that exist on us.
	class CEconItemAttributeIterator_EconItemViewWrapper : public IEconItemAttributeIterator
	{
	public:
		CEconItemAttributeIterator_EconItemViewWrapper( const CEconItemView *pEconItemView, IEconItemAttributeIterator *pIterator )
			: m_pEconItemView( pEconItemView )
			, m_pIterator( pIterator )
		{
			Assert( m_pEconItemView );
			Assert( m_pIterator );
		}

		virtual bool OnIterateAttributeValue( const CEconItemAttributeDefinition *pAttrDef, attrib_value_t value )
		{
			Assert( pAttrDef );

			return m_pEconItemView->GetAttributeList()->GetAttributeByID( pAttrDef->GetDefinitionIndex() )
				 ? true
				 : m_pIterator->OnIterateAttributeValue( pAttrDef, value );
		}

		virtual bool OnIterateAttributeValue( const CEconItemAttributeDefinition *pAttrDef, float value )
		{
			Assert( pAttrDef );

			return m_pEconItemView->GetAttributeList()->GetAttributeByID( pAttrDef->GetDefinitionIndex() )
				 ? true
				 : m_pIterator->OnIterateAttributeValue( pAttrDef, value );
		}

		virtual bool OnIterateAttributeValue( const CEconItemAttributeDefinition *pAttrDef, const uint64& value )
		{
			Assert( pAttrDef );

			return m_pEconItemView->GetAttributeList()->GetAttributeByID( pAttrDef->GetDefinitionIndex() )
				 ? true
				 : m_pIterator->OnIterateAttributeValue( pAttrDef, value );
		}

		virtual bool OnIterateAttributeValue( const CEconItemAttributeDefinition *pAttrDef, const CAttribute_String& value )
		{
			Assert( pAttrDef );

			return m_pEconItemView->GetAttributeList()->GetAttributeByID( pAttrDef->GetDefinitionIndex() )
				 ? true
				 : m_pIterator->OnIterateAttributeValue( pAttrDef, value );
		}

		virtual bool OnIterateAttributeValue( const CEconItemAttributeDefinition *pAttrDef, const CAttribute_DynamicRecipeComponent& value )
		{
			Assert( pAttrDef );

			return m_pEconItemView->GetAttributeList()->GetAttributeByID( pAttrDef->GetDefinitionIndex() )
				 ? true
				 : m_pIterator->OnIterateAttributeValue( pAttrDef, value );
		}

		virtual bool OnIterateAttributeValue( const CEconItemAttributeDefinition *pAttrDef, const CAttribute_ItemSlotCriteria& value )
		{
			Assert( pAttrDef );

			return m_pEconItemView->GetAttributeList()->GetAttributeByID( pAttrDef->GetDefinitionIndex() )
				 ? true
				 : m_pIterator->OnIterateAttributeValue( pAttrDef, value );
		}

		virtual bool OnIterateAttributeValue( const CEconItemAttributeDefinition *pAttrDef, const CAttribute_WorldItemPlacement& value )
		{
			Assert( pAttrDef );

			return m_pEconItemView->GetAttributeList()->GetAttributeByID( pAttrDef->GetDefinitionIndex() )
				 ? true
				 : m_pIterator->OnIterateAttributeValue( pAttrDef, value );
		}

	private:
		const CEconItemView *m_pEconItemView;
		IEconItemAttributeIterator *m_pIterator;
	};

	CEconItemAttributeIterator_EconItemViewWrapper iteratorWrapper( this, pIterator );

	// Next, iterate over our database-backed item if we have one... if we do have a DB
	// backing for our item here, that will also feed in the definition attributes.
	CEconItem *pEconItem = GetSOCData();
	if ( pEconItem )
	{
		pEconItem->IterateAttributes( &iteratorWrapper );
	}
	else if ( GetItemID() != INVALID_ITEM_ID && bHasNetworkedAttribsForDemos )
	{
		// Since there's no persistent data available, try the networked values!
		// note: only copies the default type and floats
		m_NetworkedDynamicAttributesForDemos.IterateAttributes( &iteratorWrapper );
	}
	// If we didn't have a DB backing, we can still iterate over our item definition
	// attributes ourselves. This can happen if we're previewing an item in the store, etc.
	else if ( GetStaticData() )
	{
		GetStaticData()->IterateAttributes( &iteratorWrapper );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconItemView::EnsureDescriptionIsBuilt() const
{
	tmZone( TELEMETRY_LEVEL1, TMZF_NONE, "%s", __FUNCTION__ );

#if BUILD_ITEM_NAME_AND_DESC
	if ( m_pDescription )
	{
		return;
	}

	m_pDescription = new CEconItemDescription;
#if defined( CLIENT_DLL )
	m_pDescription->SetIsToolTip( m_bIsToolTip );
#endif // CLIENT_DLL

	IEconItemDescription::YieldingFillOutEconItemDescription( m_pDescription, GLocalizationProvider(), this );

	// We use the empty string to mean "grey out but don't specify a user-facing reason".
	if ( m_pszGrayedOutReason && m_pszGrayedOutReason[0] )
	{
		m_pDescription->AddEmptyDescLine();
		m_pDescription->LocalizedAddDescLine( GLocalizationProvider(), m_pszGrayedOutReason, ATTRIB_COL_NEGATIVE, kDescLineFlag_Misc );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconItemView::MarkDescriptionDirty()
{
#if BUILD_ITEM_NAME_AND_DESC
	if ( m_pDescription )
	{
		delete m_pDescription;
		m_pDescription = NULL;
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconItemView::SetGrayedOutReason( const char *pszGrayedOutReason )
{
#if BUILD_ITEM_NAME_AND_DESC
	if ( m_pszGrayedOutReason )
	{
		free( m_pszGrayedOutReason );
		m_pszGrayedOutReason = NULL;
	}

	if ( pszGrayedOutReason )
	{
		m_pszGrayedOutReason = strdup(pszGrayedOutReason);
	}

	MarkDescriptionDirty();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CEconItemView::GetItemQuantity() const
{
	CEconItem *pSOCData = GetSOCData();
	if ( pSOCData )
	{
		return pSOCData->GetQuantity();
	}
#ifdef CLIENT_DLL
	return m_iEntityQuantity;
#else
	return 1;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
style_index_t CEconItemView::GetItemStyle() const
{

#ifdef CLIENT_DLL
	// Are we overriding the backing store style?
	if ( m_unOverrideStyle != INVALID_STYLE_INDEX )
		return m_unOverrideStyle;
#endif // CLIENT_DLL

	static CSchemaAttributeDefHandle pAttrDef_ItemStyleOverride( "item style override" );
	float fStyleOverride = 0.f;
	if ( FindAttribute_UnsafeBitwiseCast<attrib_value_t>( this, pAttrDef_ItemStyleOverride, &fStyleOverride ) )
	{
		return fStyleOverride;
	}

	static CSchemaAttributeDefHandle pAttrDef_ItemStyleStrange( "style changes on strange level" );
	uint32 iMaxStyle = 0;
	if ( pAttrDef_ItemStyleStrange && FindAttribute( pAttrDef_ItemStyleStrange, &iMaxStyle ) )
	{
		// Use the strange prefix if the weapon has one.
		uint32 unScore = 0;
		if ( !FindAttribute( GetKillEaterAttr_Score( 0 ), &unScore ) )
			return 0;

		// What type of event are we tracking and how does it describe itself?
		uint32 unKillEaterEventType = 0;
		// This will overwrite our default 0 value if we have a value set but leave it if not.
		float fKillEaterEventType;
		if ( FindAttribute_UnsafeBitwiseCast<attrib_value_t>( this, GetKillEaterAttr_Type( 0 ), &fKillEaterEventType ) )
		{
			unKillEaterEventType = fKillEaterEventType;
		}

		const char *pszLevelingDataName = GetItemSchema()->GetKillEaterScoreTypeLevelingDataName( unKillEaterEventType );
		if ( !pszLevelingDataName )
		{
			pszLevelingDataName = KILL_EATER_RANK_LEVEL_BLOCK_NAME;
		}

		const CItemLevelingDefinition *pLevelDef = GetItemSchema()->GetItemLevelForScore( pszLevelingDataName, unScore );
		if ( !pLevelDef )
			return 0;

		return Min( pLevelDef->GetLevel(), iMaxStyle );
	}


	CEconItem *pSOCData = GetSOCData();
	if ( pSOCData )
		return pSOCData->GetStyle();

	return INVALID_STYLE_INDEX;
}

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconItemView::SetClientItemFlags( uint8 unFlags )
{
	// Generally speaking, we have two uses for client flags:
	//
	//		- we don't have a backing store (a real CEconItem) but want to pretend we do
	//		  for purposes of generating tooltips, graying out icons, etc.
	//
	//		- we may or may not have a backing store but want to shove client-specific
	//		  information into the structure -- things like "this is the item being
	//		  actively previewed", etc.
	//
	// If neither of these two cases is true, then we're going to get unexpected
	// behavior where the GC and the client disagree about the item flags and then
	// Terrible Things happen. We assert to make sure we're in one of the above cases.
	Assert( !GetSOCData() || (unFlags & kEconItemFlags_CheckFlags_AllGCFlags) == 0 );

	m_unClientFlags = unFlags;
	MarkDescriptionDirty();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconItemView::SetItemStyleOverride( style_index_t unNewStyleOverride )
{
	// We should only ever override the style on items that don't have a real
	// backing store or we'll start getting disagreements about what the client
	// wants to happen and what's being stored on the GC. Unfortunately we can't
	// assert on this because we do it sometimes when previewing items.
	//Assert( !GetSOCData() );

	m_unOverrideStyle = unNewStyleOverride;
	MarkDescriptionDirty();
}


void CEconItemView::SetItemOriginOverride( eEconItemOrigin unNewOriginOverride )
{
	Assert( !GetSOCData() || m_pNonSOEconItem );
	Assert( unNewOriginOverride >= kEconItemOrigin_Invalid );
	Assert( unNewOriginOverride <= kEconItemOrigin_Max );	// Allow max.  We ignore this value if it's max

	m_unOverrideOrigin = unNewOriginOverride;
}
#endif // CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEconItem *CEconItemView::GetSOCData( void ) const
{
	if ( m_pNonSOEconItem )
		return m_pNonSOEconItem;

#ifdef CLIENT_DLL
	// We need to find the inventory that contains this item. If we're not connected 
	// to a server, and the owner is the same as the local player, use the local inventory.
	// We need to do this for trading, since we are subscribed to the other person's cache.
	if ( !engine->IsInGame() && InventoryManager()->GetLocalInventory()->GetOwner().GetAccountID() == m_iAccountID )
		return InventoryManager()->GetLocalInventory()->GetSOCDataForItem( GetItemID() );
#endif // CLIENT_DLL

	// We're in-game. Find the inventory with our account ID.
	CPlayerInventory *pInventory = InventoryManager()->GetInventoryForAccount( m_iAccountID );
	if ( pInventory )
		return pInventory->GetSOCDataForItem( GetItemID() );

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Return the model to use for model panels containing this item
//-----------------------------------------------------------------------------
const char *CEconItemView::GetInventoryModel( void )
{
	if ( !GetStaticData() )
		return NULL;
	return GetStaticData()->GetInventoryModel();
}

//-----------------------------------------------------------------------------
// Purpose: Return the image to use for model panels containing this item
//-----------------------------------------------------------------------------
const char *CEconItemView::GetInventoryImage( void )
{
	if ( !GetStaticData() )
		return NULL;

	// Do we have a style set?
	const char* pStyleImage = NULL;
	if ( GetStaticData()->GetNumStyles() )
		pStyleImage = GetStaticData()->GetStyleInventoryImage( GetItemStyle() );
		
	if ( pStyleImage && *pStyleImage )
		return pStyleImage;

	return GetStaticData()->GetInventoryImage();
}

//-----------------------------------------------------------------------------
// Purpose: Return the drawing data for the image to use for model panels containing this item
//-----------------------------------------------------------------------------
bool CEconItemView::GetInventoryImageData( int *iPosition, int *iSize )
{
	if ( !GetStaticData() )
		return false;
	for ( int i = 0; i < 2; i++ )
	{
		iPosition[i] = GetStaticData()->GetInventoryImagePosition(i);
		iSize[i] = GetStaticData()->GetInventoryImageSize(i);
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Return the image to use for model panels containing this item
//-----------------------------------------------------------------------------
const char *CEconItemView::GetInventoryOverlayImage( int idx )
{
	if ( !GetStaticData() )
		return NULL;
	return GetStaticData()->GetInventoryOverlayImage( idx );
}

int	CEconItemView::GetInventoryOverlayImageCount( void )
{
	if ( !GetStaticData() )
		return 0;
	return GetStaticData()->GetInventoryOverlayImageCount();
}

//-----------------------------------------------------------------------------
// Purpose: Return the model to use when displaying this model on the player character model, if any
//-----------------------------------------------------------------------------
const char *CEconItemView::GetPlayerDisplayModel( int iClass, int iTeam ) const
{
	const CEconItemDefinition *pDef = GetStaticData();
	if ( !pDef )
		return NULL;

	// If we have styles, give the style system a chance to change the mesh used for this
	// player class.
	if ( pDef->GetNumStyles() )
	{
		style_index_t unStyle = GetItemStyle();

		const CEconStyleInfo *pStyle = pDef->GetStyleInfo( unStyle );

		// It's possible to get back a NULL pStyle if GetItemStyle() returns INVALID_STYLE_INDEX.
		if ( pStyle )
		{
#if defined( TF_DLL ) || defined( TF_CLIENT_DLL )
			// TF styles support per-class models.
			const CTFStyleInfo *pTFStyle = assert_cast<const CTFStyleInfo *>( pStyle );
			if ( pTFStyle->GetPlayerDisplayModel( iClass, iTeam ) )
				return pTFStyle->GetPlayerDisplayModel( iClass, iTeam );
#endif // defined( TF_DLL ) || defined( TF_CLIENT_DLL )

			if ( pStyle->GetBasePlayerDisplayModel() )
				return pStyle->GetBasePlayerDisplayModel();
		}
	}

#if defined( TF_DLL ) || defined( TF_CLIENT_DLL )
	// If we don't have a style, we still a couple potential overrides.
	if ( iClass >= 0 && iClass < LOADOUT_COUNT )
	{
		// We don't support overriding meshes in the visuals section, but we might still be overriding 
		// the model for each class at the schema level.
		const CTFItemDefinition *pTFDef = dynamic_cast<const CTFItemDefinition *>( pDef );
		if ( pTFDef )	
		{
			const char *pszModel = pTFDef->GetPlayerDisplayModel(iClass);
			if ( pszModel && pszModel[0] )
				return pszModel;
		}
	}
#endif // defined( TF_DLL ) || defined( TF_CLIENT_DLL )

	return pDef->GetBasePlayerDisplayModel();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CEconItemView::GetSkin( int iTeam, bool bViewmodel /*= false*/ ) const
{
	int iDefaultSkin = -1;
#ifndef CSTRIKE_DLL
	// Immediately abort if we're out of range.
	if ( iTeam < 0 || iTeam >= TEAM_VISUAL_SECTIONS )
		return 0;

	// Do we have a style set?
	if ( GetStaticData()->GetNumStyles() )
		return GetStaticData()->GetStyleSkin( GetItemStyle(), iTeam, bViewmodel );
		
	iTeam = GetStaticData()->GetBestVisualTeamData( iTeam );
	if ( iTeam < 0 || iTeam >= TEAM_VISUAL_SECTIONS )
		return -1;

	// Do we have per-team skins set?
	const perteamvisuals_t *pVisData = GetStaticData()->GetPerTeamVisual( iTeam );
	if ( pVisData )
		return pVisData->iSkin;

	iDefaultSkin = GetItemDefinition()->GetDefaultSkin();
#endif

	// Fallback case.
	return iDefaultSkin;
}

#ifdef CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose: Handle assignment for textures, which involves some reference counting shenanigans.
//-----------------------------------------------------------------------------
void CEconItemView::SetWeaponSkinBase( ITexture* pBaseTex )
{
	SafeAssign( &m_pWeaponSkinBase, pBaseTex );
}

//-----------------------------------------------------------------------------
// Purpose: Handle assignment for compositors, which involves some reference counting shenanigans.
//-----------------------------------------------------------------------------
void CEconItemView::SetWeaponSkinBaseCompositor( ITextureCompositor * pTexCompositor )
{
	SafeAssign( &m_pWeaponSkinBaseCompositor, pTexCompositor );
}

//-----------------------------------------------------------------------------
// Purpose: Cancels a pending composite, if one is currently in process.
//-----------------------------------------------------------------------------
void CEconItemView::CancelWeaponSkinComposite( )
{
	SafeRelease( &m_pWeaponSkinBaseCompositor );
}
#endif // CLIENT_DLL


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CEconItemView::GetWorldDisplayModel() const
{
	CEconItemDefinition *pData = GetStaticData();
	if ( !pData )
		return NULL;

	return pData->GetWorldDisplayModel();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CEconItemView::GetExtraWearableModel() const
{
	CEconItemDefinition *pData = GetStaticData();
	if ( !pData )
		return NULL;

	return pData->GetExtraWearableModel();
}

const char *CEconItemView::GetExtraWearableViewModel() const
{
	CEconItemDefinition *pData = GetStaticData();
	if ( !pData )
		return NULL;

	return pData->GetExtraWearableViewModel();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CEconItemView::GetQualityParticleType() const
{
	static CSchemaParticleHandle pSparkleSystem( "community_sparkle" );
	
	CEconItem* pItem = GetSOCData();
	if ( !pItem )
		return 0;

	if( GetSOCData()->GetQuality() == AE_SELFMADE || GetSOCData()->GetQuality() == AE_COMMUNITY )
		return pSparkleSystem ? pSparkleSystem->nSystemID : 0;
	else
		return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Return the animation set that this item wants the player to use (ie., melee, item1, pda)
//-----------------------------------------------------------------------------
int CEconItemView::GetAnimationSlot( void ) const
{
	if ( !GetStaticData() )
		return -1;

#if defined( CSTRIKE_DLL ) || defined( DOTA_DLL )
	return -1;
#else
	return GetStaticData()->GetAnimSlot();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Return an int that indicates whether the item should be dropped from a dead owner.
//-----------------------------------------------------------------------------
int CEconItemView::GetDropType( void )
{
	if ( !GetStaticData() )
		return 0;
	return GetStaticData()->GetDropType();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconItemView::DestroyAllAttributes( void )
{
	m_AttributeList.DestroyAllAttributes();
	m_NetworkedDynamicAttributesForDemos.DestroyAllAttributes();
	NetworkStateChanged();
	MarkDescriptionDirty();
}

extern const char *g_EffectTypes[NUM_EFFECT_TYPES];

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
#ifdef CLIENT_DLL
const wchar_t *CEconItemView::GetItemName() const
{
	static const wchar_t *pwzDefaultName = L"";

	const CEconItemDescription *pDescription = GetDescription();
	if ( !pDescription )
		return pwzDefaultName;

	const econ_item_description_line_t *pNameDescLine = pDescription->GetFirstLineWithMetaType( kDescLineFlag_Name );
	if ( !pNameDescLine )
		return pwzDefaultName;

	return pNameDescLine->sText.Get();
}

//-----------------------------------------------------------------------------
void CEconItemView::GetRenderBounds( Vector& mins, Vector& maxs ) 
{ 
	const CEconItemDefinition *pDef = GetStaticData();
	if ( !pDef )
		return;

	int iClass = 0;
	int iTeam = 0;

#ifdef TF_CLIENT_DLL
	C_TFPlayer *pTFPlayer = ToTFPlayer( GetPlayerByAccountID( GetAccountID() ) );
	if ( pTFPlayer )
	{
		iClass = pTFPlayer->GetPlayerClass()->GetClassIndex();
		iTeam = pTFPlayer->GetTeamNumber();
	}
#endif // TF_CLIENT_DLL
	
	const char * pszModel = GetPlayerDisplayModel( iClass, iTeam );
	if ( !pszModel )
		return;

	int iIndex = modelinfo->GetModelIndex( pszModel );

	if ( iIndex == -1 )
	{
		// hard load the model to get its bounds
		MDLHandle_t hMDLFindResult = g_pMDLCache->FindMDL( pszModel );
		MDLHandle_t hMDL = pszModel ? hMDLFindResult : MDLHANDLE_INVALID;
		if ( g_pMDLCache->IsErrorModel( hMDL ) )
			return;

		const studiohdr_t * pStudioHdr = g_pMDLCache->GetStudioHdr( hMDL );
		VectorCopy( pStudioHdr->hull_min, mins );
		VectorCopy( pStudioHdr->hull_max, maxs );

		g_pMDLCache->Release( hMDLFindResult );
	}
	else
	{
		const model_t *pModel = modelinfo->GetModel( iIndex );
		modelinfo->GetModelRenderBounds( pModel, mins, maxs );
	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEconItemView::InitNetworkedDynamicAttributesForDemos( void )
{
	if ( !GetSOCData() )
		return;

	class CEconDynamicAttributesForDemosIterator : public CEconItemSpecificAttributeIterator
	{
	public:
		CEconDynamicAttributesForDemosIterator( CAttributeList* out_NetworkedDynamicAttributesForDemos )
			: m_NetworkedDynamicAttributesForDemos( out_NetworkedDynamicAttributesForDemos )
		{
			m_bAdded = false;
		}

		virtual bool OnIterateAttributeValue( const CEconItemAttributeDefinition *pAttrDef, attrib_value_t value ) OVERRIDE
		{
			CEconItemAttribute attribute( pAttrDef->GetDefinitionIndex(), value );
			m_NetworkedDynamicAttributesForDemos->AddAttribute( &attribute );
			m_bAdded = true;
			return true;
		}

		virtual bool OnIterateAttributeValue( const CEconItemAttributeDefinition *pAttrDef, float value ) OVERRIDE
		{
			CEconItemAttribute attribute( pAttrDef->GetDefinitionIndex(), value );
			m_NetworkedDynamicAttributesForDemos->AddAttribute( &attribute );
			m_bAdded = true;
			return true;
		}

		bool BAdded( void ){ return m_bAdded; }

	private:
		bool m_bAdded;
		CAttributeList *m_NetworkedDynamicAttributesForDemos;
	};

	m_NetworkedDynamicAttributesForDemos.DestroyAllAttributes();

	CEconDynamicAttributesForDemosIterator it( &m_NetworkedDynamicAttributesForDemos );
	IterateAttributes( &it );
	
	if ( it.BAdded() )
	{
		NetworkStateChanged();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get RGB modifying attribute value
//-----------------------------------------------------------------------------
int CEconItemView::GetModifiedRGBValue( bool bAltColor )
{
	enum
	{
		kPaintConstant_Default = 0,
		kPaintConstant_OldTeamColor = 1,
	};

	static CSchemaAttributeDefHandle pAttr_Paint( "set item tint rgb" );
	static CSchemaAttributeDefHandle pAttr_Paint2( "set item tint rgb 2" );

	static CSchemaAttributeDefHandle pAttr_PaintOverride( "SPELL: set item tint RGB" );

	if ( !m_bColorInit )
	{
		// See if we also have a secondary paint color.
		uint32 unRGB = kPaintConstant_Default;
		uint32 unRGBAlt = kPaintConstant_Default;
		float fRGB;
		float fRGBAlt;

		// If we have no base paint color we don't do anything special.
		if ( FindAttribute_UnsafeBitwiseCast<attrib_value_t>( this, pAttr_Paint, &fRGB ) )
		{
			unRGB = (uint32)fRGB;
			unRGBAlt = unRGB;
		}

		// Backwards compatibility for old team colored items.
		if ( unRGB == kPaintConstant_OldTeamColor )
		{
			unRGB = RGB_INT_RED;
			unRGBAlt = RGB_INT_BLUE;
		}
		else if ( FindAttribute_UnsafeBitwiseCast<attrib_value_t>( this, pAttr_Paint2, &fRGBAlt ) )
		{
			unRGBAlt = (uint32)fRGBAlt;
		}
		else
		{
			// By default our secondary color will match our primary if we can't find a replacement.
			unRGBAlt = unRGB;
		}

		m_unRGB = unRGB;
		m_unAltRGB = unRGBAlt;

		m_bColorInit = true;
	}

	return bAltColor ? m_unAltRGB : m_unRGB;
}

uint64 CEconItemView::GetCustomUserTextureID()
{
	static CSchemaAttributeDefHandle pAttr_CustomTextureLo( "custom texture lo" );
	static CSchemaAttributeDefHandle pAttr_CustomTextureHi( "custom texture hi" );

#if defined( TF_CLIENT_DLL )
	if ( tf_hide_custom_decals.GetBool() )
	{
		CBasePlayer *pPlayer = GetPlayerByAccountID( m_iAccountID );
		if ( !pPlayer || ( pPlayer != C_BasePlayer::GetLocalPlayer() ) )
			return 0;
	}
#endif // TF_CLIENT_DLL

	uint32 unLowVal, unHighVal;
	const bool bHasLowVal = FindAttribute( pAttr_CustomTextureLo, &unLowVal ),
			   bHasHighVal = FindAttribute( pAttr_CustomTextureHi, &unHighVal );

	// We should have both, or neither.  We should never have just one
	Assert( bHasLowVal == bHasHighVal );

	if ( bHasLowVal && bHasHighVal )
	{
		return ((uint64)unHighVal << 32) | (uint64)unLowVal;
	}

	// No attribute set
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CAttributeList::CAttributeList()
{
	m_pManager = NULL;
	m_Attributes.Purge();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAttributeList::SetManager( CAttributeManager *pManager )
{
	m_pManager = pManager;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAttributeList::Init()
{
	m_Attributes.Purge();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAttributeList::IterateAttributes( class IEconItemAttributeIterator *pIterator ) const
{
	Assert( pIterator );

	FOR_EACH_VEC( m_Attributes, i )
	{
		const CEconItemAttribute *pAttrInst = &m_Attributes[i];

		const CEconItemAttributeDefinition *pAttrDef = pAttrInst->GetStaticData();
		if ( !pAttrDef )
			continue;

		const ISchemaAttributeType *pAttrType = pAttrDef->GetAttributeType();
		Assert( pAttrType );
		Assert( pAttrType->BSupportsGameplayModificationAndNetworking() );

		// We know (and assert) that we only need 32 bits of data to store this attribute
		// data. We don't know anything about the type but we'll let the type handle it
		// below.
		attribute_data_union_t value;
		value.asFloat = pAttrInst->m_flValue;

		if ( !pAttrType->OnIterateAttributeValue( pIterator, pAttrDef, value ) )
			return;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAttributeList::DestroyAllAttributes( void )
{
	if ( m_Attributes.Count() )
	{
		m_Attributes.Purge();
		NotifyManagerOfAttributeValueChanges();
		NetworkStateChanged();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAttributeList::AddAttribute( CEconItemAttribute *pAttribute )
{
	Assert( pAttribute );

	// Only add attributes to the attribute list if they have a definition we can
	// pull data from.
	if ( !pAttribute->GetStaticData() )
		return;

	m_Attributes.AddToTail( *pAttribute );
	NetworkStateChanged();
	NotifyManagerOfAttributeValueChanges();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAttributeList::SetRuntimeAttributeValue( const CEconItemAttributeDefinition *pAttrDef, float flValue )
{
	Assert( pAttrDef );
	
	// Look for an existing attribute.
	const int iAttributes = GetNumAttributes();
	for ( int i = 0; i < iAttributes; i++ )
	{
		CEconItemAttribute *pAttribute = GetAttribute(i);

		if ( pAttribute->GetAttribIndex() == pAttrDef->GetDefinitionIndex() )
		{
			// Found existing attribute -- change value.
			pAttribute->m_flValue = flValue;
			NotifyManagerOfAttributeValueChanges();
			return;
		}
	}

	// Couldn't find an existing attribute for this definition -- make a new one.
	CEconItemAttribute attribute;
	attribute.m_iAttributeDefinitionIndex = pAttrDef->GetDefinitionIndex();
	attribute.m_flValue = flValue;

	m_Attributes.AddToTail( attribute );
	NotifyManagerOfAttributeValueChanges();
}

#if ENABLE_ATTRIBUTE_CURRENCY_TRACKING
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAttributeList::SetRuntimeAttributeRefundableCurrency( const CEconItemAttributeDefinition *pAttrDef, int iRefundableCurrency )
{
	Assert( pAttrDef );

	// Look for an existing attribute.
	const int iAttributes = GetNumAttributes();
	for ( int i = 0; i < iAttributes; i++ )
	{
		CEconItemAttribute *pAttribute = GetAttribute(i);

		if ( pAttribute->GetAttribIndex() == pAttrDef->GetDefinitionIndex() )
		{
			// Found existing attribute -- change value.
			pAttribute->m_nRefundableCurrency = iRefundableCurrency;
			return;
		}
	}

	AssertMsg1( false, "Unable to find attribute '%s' for setting currency!", pAttrDef->GetDefinitionName() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CAttributeList::GetRuntimeAttributeRefundableCurrency( const CEconItemAttributeDefinition *pAttrDef ) const
{
	const int iAttributes = GetNumAttributes();
	for ( int i = 0; i < iAttributes; i++ )
	{
		const CEconItemAttribute *pAttribute = GetAttribute(i);

		if ( pAttribute->GetAttribIndex() == pAttrDef->GetDefinitionIndex() )
			return pAttribute->m_nRefundableCurrency;
	}

	AssertMsg1( false, "Unable to find attribute '%s' for getting currency!", pAttrDef->GetDefinitionName() );
	return 0;
}
#endif // ENABLE_ATTRIBUTE_CURRENCY_TRACKING

//-----------------------------------------------------------------------------
// Purpose: Remove an attribute by name
//-----------------------------------------------------------------------------
void CAttributeList::RemoveAttribute( const CEconItemAttributeDefinition *pAttrDef )
{
	const int iAttributes = m_Attributes.Count();
	for ( int i = 0; i < iAttributes; i++ )
	{
		if ( m_Attributes[i].GetStaticData() == pAttrDef )
		{
			m_Attributes.Remove( i );
			NotifyManagerOfAttributeValueChanges();
			return;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Remove an attribute by index
//-----------------------------------------------------------------------------
void CAttributeList::RemoveAttributeByIndex( int iIndex )
{
	if ( iIndex < 0 || iIndex >= GetNumAttributes() )
		return;

	m_Attributes.Remove( iIndex );
	NotifyManagerOfAttributeValueChanges();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const CEconItemAttribute *CAttributeList::GetAttributeByID( int iAttributeID ) const
{
	int iAttributes = m_Attributes.Count();
	for ( int i = 0; i < iAttributes; i++ )
	{
		const CEconItemAttributeDefinition *pData = m_Attributes[i].GetStaticData();

		if ( pData && ( pData->GetDefinitionIndex() == iAttributeID ) )
			return &m_Attributes[i];
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const CEconItemAttribute *CAttributeList::GetAttributeByName( const char *pszAttribDefName ) const
{
	CEconItemAttributeDefinition *pDef = GetItemSchema()->GetAttributeDefinitionByName( pszAttribDefName );
	if ( !pDef )
		return NULL;

	int iAttributes = m_Attributes.Count();
	for ( int i = 0; i < iAttributes; i++ )
	{
		if ( m_Attributes[i].GetStaticData()->GetDefinitionIndex() == pDef->GetDefinitionIndex() )
			return &m_Attributes[i];
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAttributeList::operator=( const CAttributeList& src )
{
	m_Attributes = src.m_Attributes;

	// HACK: We deliberately don't copy managers, because attributelists are contained inside 
	// CEconItemViews, which we duplicate inside CItemModelPanels all the time. If the manager
	// is copied, copies will mess with the attribute caches of the copied item.
	// Our manager will be setup properly by the CAttributeManager itself if we have an associated entity.
	m_pManager = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAttributeList::NotifyManagerOfAttributeValueChanges( void ) 
{ 
	if ( m_pManager ) 
	{
		m_pManager->OnAttributeValuesChanged();
	}
}

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool DoesItemPassSearchFilter( const IEconItemDescription *pDescription, const wchar_t* wszFilter )
{
	// check if item matches name filter
	if ( wszFilter && *wszFilter )
	{
		if ( !pDescription )
		{
			return false;
		}

		wchar_t wszBuffer[ 4096 ] = L"";
		for ( unsigned int i = 0; i < pDescription->GetLineCount(); i++ )
		{
			const econ_item_description_line_t& line = pDescription->GetLine(i);

			if ( !(line.unMetaType & ( kDescLineFlag_Collection | kDescLineFlag_CollectionCurrentItem ) ) )
			{
				V_wcscat_safe( wszBuffer, line.sText.Get() );
			}
		}

		V_wcslower( wszBuffer );
		if ( !wcsstr( wszBuffer, wszFilter ) )
		{
			return false;
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBasePlayer *GetPlayerByAccountID( uint32 unAccountID )
{
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( pPlayer == NULL )
			continue;

		CSteamID steamIDPlayer;
		if ( !pPlayer->GetSteamID( &steamIDPlayer ) )
			continue;

		// return the player with the matching ID
		if ( steamIDPlayer.GetAccountID() == unAccountID )
		{
			return pPlayer;
		}
	}

	return NULL;
}

#endif // CLIENT_DLL
