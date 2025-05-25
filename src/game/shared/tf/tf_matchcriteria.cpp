//========= Copyright Valve Corporation, All rights reserved. ============//

#include "cbase.h"
#include "tf_matchcriteria.h"
#include "tf_matchmaking_shared.h"
#include "filesystem.h"
#include "google/protobuf/text_format.h"
#include "protoutils.h"

//-----------------------------------------------------------------------------
bool ITFGroupMatchCriteriaReader::GetLateJoin() const
{
	return Proto().late_join_ok();
}

//-----------------------------------------------------------------------------
uint32_t ITFGroupMatchCriteriaReader::GetCustomPingTolerance() const
{
	return Proto().custom_ping_tolerance();
}

//-----------------------------------------------------------------------------
void ITFGroupMatchCriteriaReader::SaveCasualCriteriaToFile( const char *pszFileName ) const
{
	std::string strOut;
	google::protobuf::TextFormat::PrintToString( Proto().casual_criteria(), &strOut );
	CUtlBuffer bufOut;
	bufOut.SetBufferType( true, true );
	bufOut.PutString( strOut.c_str() );
	g_pFullFileSystem->WriteFile( pszFileName, NULL, bufOut );
}


//-----------------------------------------------------------------------------
bool ITFGroupMatchCriteriaReader::IsCasualMapSelected( uint32 nMapDefIndex ) const
{
	return GetCasualCriteriaHelper().IsMapSelected( nMapDefIndex );
}

//-----------------------------------------------------------------------------
CCasualCriteriaHelper ITFGroupMatchCriteriaReader::GetCasualCriteriaHelper() const
{
	CCasualCriteriaHelper casualHelper( Proto().casual_criteria( ) );
	return casualHelper;
}

//-----------------------------------------------------------------------------
void ITFGroupMatchCriteria::SetLateJoin( bool bLateJoin )
{
	if ( Proto().late_join_ok() != bLateJoin )
	{
		MutProto().set_late_join_ok( bLateJoin );
	}
}

//-----------------------------------------------------------------------------
void ITFGroupMatchCriteria::SetCustomPingTolerance( uint32_t unCustomPingTolerance )
{
	if ( Proto().custom_ping_tolerance() != unCustomPingTolerance )
	{
		MutProto().set_custom_ping_tolerance( unCustomPingTolerance );
	}
}

//-----------------------------------------------------------------------------
void ITFGroupMatchCriteria::SetCasualMapSelected( uint32 nMapDefIndex, bool bSelected )
{
	CCasualCriteriaHelper casualHelper( Proto().casual_criteria() );

	if ( casualHelper.IsMapSelected( nMapDefIndex ) == bSelected )
		{ return; }

	casualHelper.SetMapSelected( nMapDefIndex, bSelected );
	MutProto().mutable_casual_criteria()->CopyFrom( casualHelper.GetCasualCriteria() );

	Assert( casualHelper.IsValid() || !casualHelper.AnySelected() );
}

//-----------------------------------------------------------------------------
void ITFGroupMatchCriteria::SetCasualGroupSelected( EMatchmakingGroupType eGroup, bool bSelected )
{
	auto pGroup = GetItemSchema()->GetMMGroup( eGroup );
	if ( pGroup )
	{
		FOR_EACH_VEC( pGroup->m_vecModes, i )
		{
			SetCasualCategorySelected( pGroup->m_vecModes[ i ]->m_eGameCategory, bSelected );
		}
	}
}

//-----------------------------------------------------------------------------
void ITFGroupMatchCriteria::SetCasualCategorySelected( EGameCategory eCategory, bool bSelected )
{
	auto pCat = GetItemSchema()->GetGameCategory( eCategory );
	if ( pCat )
	{
		FOR_EACH_VEC( pCat->m_vecEnabledMaps, i )
		{
			if ( pCat->m_vecEnabledMaps[ i ] )
			{
				SetCasualMapSelected( pCat->m_vecEnabledMaps[ i ]->m_nDefIndex, bSelected );
			}
		}
	}
}

//-----------------------------------------------------------------------------
void ITFGroupMatchCriteria::LoadCasualCriteriaFromFile( const char *pszFileName )
{
	// Read casual criteria if the file exists
	CUtlBuffer buffer;
	buffer.SetBufferType( true, true );
	if ( g_pFullFileSystem->ReadFile( pszFileName, NULL, buffer ) &&
	     buffer.TellPut() > buffer.TellGet() )
	{
		// Null terminate. Why is buffer this pseudo-text class but has AddNullTerminator private?
		const char zero = '\0';
		buffer.Put( &zero, sizeof( zero ) );

		std::string strIn( (const char *)buffer.PeekGet() );

		CTFCasualMatchCriteria parsedCriteria;
		bool bParsed = google::protobuf::TextFormat::ParseFromString( strIn, &parsedCriteria );

		if ( bParsed )
		{
			// let the CCasualCriteriaHelper validate/cleanup the bits that we've just loaded
			CCasualCriteriaHelper parsedCasualHelper( parsedCriteria );

			// Compare&set
			SetCasualCriteriaFromHelper( parsedCasualHelper );

			DevMsg( "Loaded new casual criteria from \"%s\"\n", pszFileName );
			return;
		}

		// Else fall through to failed path
	}

	// default to the Core maps
	SetCasualGroupSelected( kMatchmakingType_Core, true );
	DevMsg( "No casual criteria in \"%s\", loaded default criteria\n", pszFileName );
}

//-----------------------------------------------------------------------------
void ITFGroupMatchCriteria::ClearCasualCriteria()
{
	CCasualCriteriaHelper casualHelper = GetCasualCriteriaHelper();

	if ( casualHelper.AnySelected() )
	{
		casualHelper.Clear();
		MutProto().mutable_casual_criteria()->CopyFrom( casualHelper.GetCasualCriteria() );
	}
}

//-----------------------------------------------------------------------------
void ITFGroupMatchCriteria::SetCasualCriteriaFromHelper( const CCasualCriteriaHelper &helper )
{
	CCasualCriteriaHelper currentCriteria = GetCasualCriteriaHelper();
	// Same criteria?
	if ( currentCriteria == helper )
		{ return; }

	MutProto().mutable_casual_criteria()->CopyFrom( helper.GetCasualCriteria() );
}

//-----------------------------------------------------------------------------
bool ITFGroupMatchCriteria::MakeDelta( const ITFGroupMatchCriteriaReader& msgBase,
                                       const ITFGroupMatchCriteriaReader& msgFinal )
{
	// Note: These functions are potentially called a lot, and should avoid instantiating e.g. casual criteria helpers,
	//       mvm mission sets, and just look at the two proto objects.  If you need to assume something about the guts
	//       of a proto object add an assert like ---v
	AssertMsgOnce( ValveProtoUtils::MessageHasExactFields( Proto(), { 5, 13, 10, 15, 16, 12 } ),
	               "Audit this if you change the message thx" );

	bool bChanged = false;

	#define DELTA_PROTO_FIELD( field )                            \
		do {                                                      \
			DbgAssert( !Proto().has_ ## field() );                \
			auto x ## field ## Value = msgFinal.Proto().field();  \
			if ( x ## field ## Value != msgBase.Proto().field() ) \
			{                                                     \
				bChanged = true;                                  \
				DbgAssert( msgFinal.Proto().has_ ## field() );    \
				MutProto().set_ ## field( x ## field ## Value );  \
			}                                                     \
		} while ( false );

	// Simple fields
	DELTA_PROTO_FIELD( late_join_ok );
	DELTA_PROTO_FIELD( custom_ping_tolerance );

	#undef DELTA_PROTO_FIELD

	// Repeated/complex fields

	#define MISSION_CHANGE_CHECK( field )																			\
	{																												\
		int nFinalMissions = msgFinal.Proto().field ## _size();														\
		bool bMissionsChanged = ( msgBase.Proto().field ## _size() != nFinalMissions );								\
																													\
		if ( !bMissionsChanged && nFinalMissions )																	\
		{																											\
			/* Deep compare */																						\
			for ( int i = 0; i < nFinalMissions; i++ )																\
			{																										\
				if ( msgFinal.Proto().field( i ) != msgBase.Proto().field( i ) )								\
				{																									\
					bMissionsChanged = true;																		\
					break;																							\
				}																									\
			}																										\
		}																											\
																													\
		if ( bMissionsChanged )																						\
		{																											\
			/* Always just re-send them	*/																			\
			bChanged = true;																						\
			MutProto().mutable_## field()->CopyFrom( msgFinal.Proto().field() );									\
			/* Terrible hack -- so we can properly make a delta that clears this field, always send something. */	\
			if ( MutProto().field ##_size() == 0 )																	\
			{ MutProto().add_## field( "" ); }																		\
		}																											\
	}																												\

	// casual_criteria
	// ---
	// Just cheat and copy innerds, but assert they're what we expect
	AssertMsgOnce( ValveProtoUtils::MessageHasExactFields( *CTFCasualMatchCriteria::descriptor(), { 3 } ),
	               "Audit this if you change the message thx" );
	auto &finalCriteriaProto = msgFinal.Proto().casual_criteria();
	auto &baseCriteriaProto = msgBase.Proto().casual_criteria();
	int nFinalCasualBitsSize = finalCriteriaProto.selected_maps_bits_size();
	bool bCasualChanged = ( nFinalCasualBitsSize != baseCriteriaProto.selected_maps_bits_size() );
	if ( !bCasualChanged )
	{
		// Deep compare
		for ( int i = 0; i < nFinalCasualBitsSize; i++ )
		{
			if ( finalCriteriaProto.selected_maps_bits( i ) != baseCriteriaProto.selected_maps_bits( i ) )
			{
				bCasualChanged = true;
				break;
			}
		}
	}

	// Always re-send
	if ( bCasualChanged )
	{
		bChanged = true;
		MutProto().mutable_casual_criteria()->CopyFrom( msgFinal.Proto().casual_criteria() );
	}

	return bChanged;
}

//-----------------------------------------------------------------------------
bool ITFGroupMatchCriteria::ApplyDelta( const ITFGroupMatchCriteriaReader& msgDelta )
{
	// Note: These functions are potentially called a lot, and should avoid instantiating e.g. casual criteria helpers,
	//       mvm mission sets, and just look at the two proto objects.  If you need to assume something about the guts
	//       of a proto object add an assert like ---v
	AssertMsgOnce( ValveProtoUtils::MessageHasExactFields( Proto(), { 5, 13, 10, 15, 16, 12 } ),
	               "Audit this if you change the message thx" );

	bool bChanged = false;

	#define APPLY_DELTA_FIELD( field )                               \
		do {                                                         \
			if ( msgDelta.Proto().has_ ## field() )                  \
			{                                                        \
				bChanged = true;                                     \
				auto x ## field ## Value = msgDelta.Proto().field(); \
				DbgAssert( Proto().field() != x ## field ## Value ); \
				MutProto().set_ ## field( x ## field ## Value );     \
			}                                                        \
		} while ( false );

	APPLY_DELTA_FIELD( late_join_ok );
	APPLY_DELTA_FIELD( custom_ping_tolerance );

	#undef APPLY_DELTA_FIELD

	// Complex/repeated fields

	// casual_criteria
	// ---
	// Just cheat and copy innerds, but assert they're what we expect
	AssertMsgOnce( ValveProtoUtils::MessageHasExactFields( *CTFCasualMatchCriteria::descriptor(), { 3 } ),
	               "Audit this if you change the message thx" );
	if ( msgDelta.Proto().has_casual_criteria() )
	{
		auto &otherBits = msgDelta.Proto().casual_criteria().selected_maps_bits();
		MutProto().mutable_casual_criteria()->mutable_selected_maps_bits()->CopyFrom( otherBits );
	}

	return bChanged;
}

//-----------------------------------------------------------------------------
// ITFPerPlayerMatchCriteria
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
bool ITFPerPlayerMatchCriteria::MakeDelta( const ITFPerPlayerMatchCriteriaReader& msgBase,
                                           const ITFPerPlayerMatchCriteriaReader& msgFinal )
{
	AssertMsgOnce( ValveProtoUtils::MessageHasExactFields( Proto(), { 1 } ),
	               "Audit this if you change the message thx" );

	bool bChanged = false;

	#define DELTA_PROTO_FIELD( field )                            \
		do {                                                      \
			DbgAssert( !Proto().has_ ## field() );                \
			auto x ## field ## Value = msgFinal.Proto().field();  \
			if ( x ## field ## Value != msgBase.Proto().field() ) \
			{                                                     \
				bChanged = true;                                  \
				DbgAssert( msgFinal.Proto().has_ ## field() );    \
				MutProto().set_ ## field( x ## field ## Value );  \
			}                                                     \
		} while ( false );

	#undef DELTA_PROTO_FIELD

	return bChanged;
}

//-----------------------------------------------------------------------------
bool ITFPerPlayerMatchCriteria::ApplyDelta( const ITFPerPlayerMatchCriteriaReader& msgDelta )
{
	AssertMsgOnce( ValveProtoUtils::MessageHasExactFields( Proto(), { 1 } ),
	               "Audit this if you change the message thx" );

	bool bChanged = false;

	#define APPLY_DELTA_FIELD( field )                               \
		do {                                                         \
			if ( msgDelta.Proto().has_ ## field() )                  \
			{                                                        \
				bChanged = true;                                     \
				auto x ## field ## Value = msgDelta.Proto().field(); \
				DbgAssert( Proto().field() != x ## field ## Value ); \
				MutProto().set_ ## field( x ## field ## Value );     \
			}                                                        \
		} while ( false );

	#undef APPLY_DELTA_FIELD

	return bChanged;
}
