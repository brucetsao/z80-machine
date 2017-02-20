/* Filter
 *
 *  2017-02-17 Scott Lawrence
 *
 *   Filter console input to provide a backchannel for data transfer
 */

#include <stdio.h>
#include "mc6850_console.h"

////////////////////////////////////////

/* pass-through */
#define kPS_IDLE	(0)

/* Start command? */
#define kEscKey		(0x1b)
#define kPS_ESC		(1)

/* for REMOTE -> CONSOLE */
#define kStartMsgRC 	('{')	/* esc{  to start from the Remote */
#define kStartMsgCR	('}')	/* esc}  to start from the Remote */
#define kEndMsg 	(0x03)	/* ctrl-c to end */

#define kPS_TCCMD	(2)

/* for CONSOLE -> REMOTE */
#define kPS_TRCMD	(3)

////////////////////////////////////////

/* max space in the buffers */
#define kFCPosMax (1024)

/* TO Console buffers */
static int tcPos = -1;
char ToConsoleBuffer[kFCPosMax]; /* additional stuff we're sending to the console */

static int ProcessStageTC = kPS_IDLE;
static byte FilterTCCommand[ kFCPosMax ];
static size_t FC_TCPos = 0;

/* TO Remote buffers */
static int trPos = -1;
char ToRemoteBuffer[kFCPosMax]; /* additional stuff we're sending to the console */

static int ProcessStageTR = kPS_IDLE;
static byte FilterTRCommand[ kFCPosMax ];
static size_t FC_TRPos = 0;


////////////////////////////////////////
// These are the handlers for the captured streams
void Filter_ProcessTC( byte * buf, size_t len ); // process REMOTE->CONSOLE command
void Filter_ProcessTR( byte * buf, size_t len ); // process CONSOLE->REMOTE command

/* Filters should use these to output text */
void Filter_ToConsolePutByte( byte data );
void Filter_ToConsolePutString( char * str );

void Filter_ToRemotePutByte( byte data );
void Filter_ToRemotePutString( char * str );


// Filter_Init
//  perform all initialization stuff
void Filter_Init( z80info * z80 )
{
#ifdef FILTER_CONSOLE
    printf( "Initializing Filter.\n" );
#endif
}

////////////////////////////////////////////////////////////////////////////////
// to console buffer stuff 


// Filter_ToConsolePutByte
//  add something into the ToConsole buffer
void Filter_ToConsolePutByte( byte data )
{
    tcPos++;
    ToConsoleBuffer[tcPos] = data;
    ToConsoleBuffer[tcPos+1] = '\0';
}

void Filter_ToConsolePutString( char * str )
{
    while( str && *str ) {
	Filter_ToConsolePutByte( *str );
	str++;
    }
}

// Filter_ToConsoleAvailable
//  Is there something in the ToConsole buffer?
int Filter_ToConsoleAvailable()
{
    if( tcPos == -1 ) return 0;
    return 1;
}

// Filter_ToConsoleGet
//  get something from the ToConsole buffer
byte Filter_ToConsoleGet()
{
    size_t i;
    byte r;

    if( tcPos == -1 ) return 0xff;

    r = ToConsoleBuffer[0];

    /* shift everything down */
    for( i=0 ; i<kFCPosMax-1 ; i++ ) {
	ToConsoleBuffer[i] = ToConsoleBuffer[i+1];
    }
    tcPos--;

    return r;
}

////////////////////////////////////////

/* for REMOTE -> CONSOLE (TC) */

void Filter_ReInitTC( void )
{
    int tcPos=0;

    ProcessStageTC = kPS_IDLE;

    for( tcPos=0 ; tcPos<(kFCPosMax-1) ; tcPos++ )
	FilterTCCommand[tcPos] = '\0';
    FC_TCPos = 0;
}

/* filter input going TO the CONSOLE */
void Filter_ToConsole( byte data )
{
    switch( ProcessStageTC ) {

	case( kPS_IDLE ):
	    if( data == kEscKey ) {
		ProcessStageTC = kPS_ESC;
	    } else {
	    	Filter_ToConsolePutByte( data );
	    }
	    break;

	case( kPS_ESC ):
            if( data == kStartMsgRC ) {
		/* yep! it's for us! */
                ProcessStageTC = kPS_TCCMD;	/* Remote->Console command! */
            } else {
		/* nope.  inject both bytes so far. */
		Filter_ToConsolePutByte( kEscKey );
		Filter_ToConsolePutByte( data );
		/* and restore our state... */
		ProcessStageTC = kPS_IDLE;
	    }
	    break;

	case( kPS_TCCMD ):
	    /* process... */
            if( data == kEndMsg ) {
		/* We're done. process it! */
                ProcessStageTC = kPS_IDLE;
		Filter_ProcessTC( FilterTCCommand, FC_TCPos );
		Filter_ReInitTC();
	    } else {
		if( FC_TCPos < kFCPosMax ) {
		    FilterTCCommand[ FC_TCPos ] = data;
		    FC_TCPos++;
		}
	    }
	    break;

	default:
	    ProcessStageTC = kPS_IDLE;
	    Filter_ToConsolePutByte( data );
	    break;
    }
}


////////////////////////////////////////////////////////////////////////////////
// to remote buffer stuff 


// Filter_ToRemotePutByte
//  add something into the ToRemote buffer
void Filter_ToRemotePutByte( byte data )
{
    trPos++;
    ToRemoteBuffer[trPos] = data;
    ToRemoteBuffer[trPos+1] = '\0';
}

void Filter_ToRemotePutString( char * str )
{
    while( str && *str ) {
	Filter_ToRemotePutByte( *str );
	str++;
    }
}

// Filter_ToRemoteAvailable
//  Is there something in the ToRemote buffer?
int Filter_ToRemoteAvailable()
{
    if( trPos == -1 ) return 0;
    return 1;
}

// Filter_ToRemoteGet
//  get something from the ToRemote buffer
byte Filter_ToRemoteGet()
{
    size_t i;
    byte r;

    if( trPos == -1 ) return 0xff;

    r = ToRemoteBuffer[0];

    /* shift everything down */
    for( i=0 ; i<kFCPosMax-1 ; i++ ) {
	ToRemoteBuffer[i] = ToRemoteBuffer[i+1];
    }
    trPos--;

    return r;
}

////////////////////////////////////////////////////////////////////////////////
// TO REMOTE


void Filter_ReInitTR( void )
{
    int trPos=0;

    ProcessStageTR = kPS_IDLE;

    for( trPos=0 ; trPos<(kFCPosMax-1) ; trPos++ )
	FilterTCCommand[trPos] = '\0';
    FC_TRPos = 0;
}

/* filter input going TO the CONSOLE */
void Filter_ToRemote( byte data )
{
    switch( ProcessStageTR ) {

	case( kPS_IDLE ):
	    if( data == kEscKey ) {
		ProcessStageTR = kPS_ESC;
	    } else {
	    	Filter_ToRemotePutByte( data );
	    }
	    break;

	case( kPS_ESC ):
            if( data == kStartMsgCR ) {
		/* yep! it's for us! */
                ProcessStageTR = kPS_TRCMD;	/* Console->Remote command! */
            } else {
		/* nope.  inject both bytes so far. */
		Filter_ToRemotePutByte( kEscKey );
		Filter_ToRemotePutByte( data );
		/* and restore our state... */
		ProcessStageTR = kPS_IDLE;
	    }
	    break;

	case( kPS_TRCMD ):
	    /* process... */
            if( data == kEndMsg ) {
		/* We're done. process it! */
                ProcessStageTR = kPS_IDLE;
		Filter_ProcessTR( FilterTRCommand, FC_TRPos );
		Filter_ReInitTR();
	    } else {
		if( FC_TRPos < kFCPosMax ) {
		    FilterTRCommand[ FC_TRPos ] = data;
		    FC_TRPos++;
		}
	    }
	    break;

	default:
	    ProcessStageTR = kPS_IDLE;
	    Filter_ToRemotePutByte( data );
	    break;
    }
}
