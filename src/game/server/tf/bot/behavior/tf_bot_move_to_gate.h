//========= Copyright Valve Corporation, All rights reserved. ============//
// tf_bot_move_to_gate.cpp
// Move to gate and wait for round to start.

#ifndef TF_BOT_MOVE_TO_GATE_H
#define TF_BOT_MOVE_TO_GATE_H

#include "Path/NextBotPathFollow.h"

class CTFBotMoveToGate : public Action< CTFBot >
{
public:
	virtual ActionResult< CTFBot >	OnStart( CTFBot *me, Action< CTFBot > *priorAction );
	virtual ActionResult< CTFBot >	Update( CTFBot *me, float interval );
	virtual ActionResult< CTFBot >	OnResume( CTFBot *me, Action< CTFBot > *interruptingAction );

	virtual EventDesiredResult< CTFBot > OnStuck( CTFBot *me );
	virtual EventDesiredResult< CTFBot > OnMoveToSuccess( CTFBot *me, const Path *path );
	virtual EventDesiredResult< CTFBot > OnMoveToFailure( CTFBot *me, const Path *path, MoveToFailureType reason );

	virtual QueryResultType	ShouldRetreat( const INextBot *me ) const;					// is it time to retreat?
	virtual QueryResultType ShouldHurry( const INextBot *me ) const;					// are we in a hurry?

	virtual const char *GetName( void ) const	{ return "MoveToGate"; };

private:
	PathFollower m_path;
	CountdownTimer m_repathTimer;

	CountdownTimer m_moveToGateTimer;
	CountdownTimer m_giveUpTimer;
};

#endif // TF_BOT_MOVE_TO_GATE_H