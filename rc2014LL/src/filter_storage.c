/* Filter
 *
 *  2017-02-17 Scott Lawrence
 *
 *   Filter console input to provide a backchannel for data transfer
 */

#include <stdio.h>
#include <string.h>	/* strlen, strcmp */
#include "mc6850_console.h"
#include "filter.h"

////////////////////////////////////////

/* checkAndAdvance
 *
 *  checks if "buf" starts with "key"
 *	if it does, return the pointer to the next thing in the string
 *	if it doesn't, return a NULL
 */
char * checkAndAdvance( char * buf, char * key )
{
    size_t keylen = 0;
    int cmp = 0;

    if( !buf || !key ) return NULL;

    keylen = strlen( key );
    cmp = strncmp( buf, key, keylen );

    /* if they don't match, just return NULL */
    if( cmp ) return NULL;

    /* now, advance the "buf" ptr */
    buf += keylen;

    /* and move it past whitespace */
    while( *buf == ' ' ) { buf++; };
    
    return buf;
}

////////////////////////////////////////////////////////////////////////////////
// path stuff
static char cwbuf[1024];
char fpbuf[1024];

char * cwd = NULL;



////////////////////////////////////////////////////////////////////////////////

/* Handle_init
 *
 *  do one-time inits
 */
void Handle_init( void )
{
    if( cwd != NULL ) return;

    cwd = &cwbuf[0];
    strcpy( cwd, "MASS_DRV/BASIC/" );
    
}


////////////////////////////////////////////////////////////////////////////////
/* command handlers... */


////////////////////////////////////////
// path stuff


void Handle_cd( byte * path )
{
    Handle_init();

    // no path, reset to "home"
    if( !path || path[0] == '\0' )
    {
	cwd = &cwbuf[0];
	strcpy( cwd, "MASS_DRV/BASIC/" );
	return;
    }

    // path set, build the new path, and then test if it exists
    // if it does exist, copy that to 'cwd'

    // TODO
}

////////////////////////////////////////////////////////////////////////////////
/* Handle_more
 *	load in a file and send it to the console directly
 */
void Handle_more( byte * filename )
{
    int c = 0;
    FILE * fp = NULL;
    char prbuf[ 255 ];

    Handle_init();

    /* build the path */
    strcpy( fpbuf, cwbuf );
    strcat( fpbuf, filename );

    fp = fopen( fpbuf, "r" );
    if( fp ) {
	sprintf( prbuf, "------- Begin %s -------\n", fpbuf );
	Filter_ToConsolePutString( prbuf );

	while( (c = fgetc(fp)) != EOF ) {
		Filter_ToConsolePutByte( (byte) c );
	}
	
    	fclose( fp );
	sprintf( prbuf, "------- End %s -------\n", fpbuf );
	Filter_ToConsolePutString( prbuf );

    } else {
	sprintf( prbuf, "%s: Cannot open!/n", fpbuf );
	Filter_ToConsolePutString( prbuf );
    }
}

/* Handle_type
 *	load in a file and send it as though the console was typing it.
 */
void Handle_type( byte * filename )
{
    int crlf = 0;
    int c = 0;
    FILE * fp = NULL;
    char prbuf[ 255 ];

    Handle_init();

    /* build the path */
    strcpy( fpbuf, cwbuf );
    strcat( fpbuf, filename );

    fp = fopen( fpbuf, "r" );
    if( fp ) {
	sprintf( prbuf, "%s: Typing to remote...\n", fpbuf );
	Filter_ToConsolePutString( prbuf );

	while( (c = fgetc(fp)) != EOF ) {
		if( (c == '\r' || c == '\n') && (crlf == 0)) {
		    crlf = 1;
		    Filter_ToRemotePutByte( 0x0a );
		    Filter_ToRemotePutByte( 0x0d );
		} else {
		    crlf = 0;
		    Filter_ToRemotePutByte( (byte) c );
		}
	}
	
    	fclose( fp );
	Filter_ToConsolePutString( "Done!\n" );
    } else {
	sprintf( prbuf, "%s: Cannot open!/n", fpbuf );
	Filter_ToConsolePutString( prbuf );
    }
}


////////////////////////////////////////////////////////////////////////////////

/* Handle_load
 *	New, clear and run a new program
 */
void Handle_load( byte * filename )
{
    Filter_ToConsolePutString( "new\n" );
    Filter_ToConsolePutString( "clear\n" );
    Handle_type( filename );
}

/* Handle_chain
 *	load and run a program
 *	without 'new' and 'clear'
 */
void Handle_chain( byte * filename )
{
    Handle_type( filename );
    Filter_ToConsolePutString( "run\n" );
}


////////////////////////////////////////////////////////////////////////////////
// SAVE routine

/* things i've tried:
 - screen scraping hack -
  1. ^c at the end
  2. ^g at the end
  3. add:  9999 REM SENTINEL
  4. \nOk
  5. manual ^g to end
  6. wait for digits, stop on non-digits, ctrl-g to exit (works, messy)
  7. read in and handle a line at a time
	- if Number, set "got numbers", save line to file
	- if Ok, and gotNumbers, terminate
 */


/* static/globals for the save routine */
#define kLBsz (1024)
static char cfLineBuf[kLBsz];
static size_t cfLinePos = 0;
static int gotNumbers = 0;
static FILE * savefp = NULL;


/* consumeSaveByte
 *	consume a byte for the save function
 */
int consumeSaveByte( byte b )
{
    if( !savefp )
    {
	consumeFcn = NULL;
	return b;
    }


    /* is end of line? */
    if( b == 0x0a || b == 0x0d ) {
	Filter_ToConsolePutByte( '.' );

	if( cfLineBuf[0] == '\0' ) {
	    /* empty string. do nothing */
	    return( -1 );
	}

	if( cfLineBuf[ 0 ] >= '0' && cfLineBuf[ 0 ] <= '9' ) {
	    /* starts with a line number... save it! */
	    fprintf( savefp, "%s\n", cfLineBuf );
	    gotNumbers = 1;

	} else if( gotNumbers ) {
	    /* it's non-digits after digits, so we exit. */
	    gotNumbers = 0;
	    fclose( savefp );
	    savefp = NULL;
	    Filter_ToConsolePutString( "\n\nDone saving.\n" );
	}

	/* clear the buffer */
	cfLinePos = 0;
	cfLineBuf[0] = '\0';

    } else {
	/* save to the line buffer*/
	if( cfLinePos <= (kLBsz-2) )
	{
	    cfLineBuf[ cfLinePos++ ] = b;
	    cfLineBuf[ cfLinePos ] = '\0';
	}
    }

    return( -1 ); /* don't echo anything! */
}

/* Handle_save
 *	Start the mechanism to save out to a file
 *	- Open the file
 *	- trigger the 'list' command
 *	- set up our capture function above
 */
void Handle_save( byte * filename )
{
    Handle_init();

    strcpy( fpbuf, cwd );
    strcat( fpbuf, filename );

    /* attempt to open the file for write */
    savefp = fopen( fpbuf, "w" );
    if( !savefp ) {
	return;
    }

    /* send out the command and the end marker sentinel */
    Filter_ToRemotePutString( "\n\n\nlist\n" );

    /* enable capture */
    cfLineBuf[0] = '\0';
    cfLinePos = 0;
    gotNumbers = 0;

    consumeFcn = consumeSaveByte;

    /* leave the file open... */
}

////////////////////////////////////////////////////////////////////////////////
/* handle_boot
 *	trigger the command to run when the "boot" command is called
 *	basically, chain "boot.bas" (new, load, run )
 */
void Handle_boot( byte * filename )
{
    /* handle the case where the user types 0 for autoboot */
    Handle_chain( "boot.bas" );
}

////////////////////////////////////////////////////////////////////////////////

typedef void (*handleFcn)( byte * );

struct HandlerFuns {
    char * name;
    handleFcn fcn;
};


/* handlers for all of the functions called by the remote */
struct HandlerFuns tcFuncs[] = {
    { "boot", Handle_boot },

    /* file type */
    { "type", Handle_type },
    { "chain", Handle_chain },
    { "load", Handle_load },
    { "save", Handle_save },

    /* directory */
    { "cd", Handle_cd },

    { NULL, NULL }
};

/* handlers for all of the functions called by the console */
struct HandlerFuns trFuncs[] = {
    { "boot", Handle_boot },
    { "more", Handle_more },

    /* file type */
    { "type", Handle_type },
    { "chain", Handle_chain },
    { "load", Handle_load },
    { "save", Handle_save },

    /* directory */
    { "cd", Handle_cd },

    { NULL, NULL }
};


////////////////////////////////////////////////////////////////////////////////

/* Filter_ProcessTC
 *
 *  Process content initiated by the remote computer 
 */
void Filter_ProcessTC( byte * buf, size_t len )
{
    struct HandlerFuns * hf = tcFuncs;
    byte *args = NULL;
    int used = 0;

    printf( "PROCESS To Console: %ld|%s|\n", len, buf );

    /* sanity check the arguments */
    if( len < 1 || buf == NULL || buf[0] == '\0' ) return;

    /* iterate over the function list to find the handler */
    while( hf->name != NULL && !used ) {
    	if( (args = checkAndAdvance( buf, hf->name )) != NULL )
	{
	    /* found it! Call the handler! */
	    hf->fcn( args );
	    used = 1;
	}
	hf++;
    }

    /* output a message if it wasn't handled. */
    if( !used ) {
	Filter_ToConsolePutString( "TC: Command not found: " );
	Filter_ToConsolePutString( buf );
	Filter_ToConsolePutString( "\n" );
    }
}

/* Filter_ProcessTR
 *
 *  Process content initiated by the user at the console
 */
void Filter_ProcessTR( byte * buf, size_t len )
{
    int used = 0;
    byte *args = NULL;

    printf( "PROCESS To Remote: %ld|%s|\n", len, buf );

    /* sanity check the arguments */
    if( len < 1 || buf == NULL || buf[0] == '\0' ) return;

    /* iterate over the function list to find the handler */
    struct HandlerFuns * hf = trFuncs;
    while( hf->name != NULL ) {
    	if( (args = checkAndAdvance( buf, hf->name )) != NULL )
	{
	    /* found it! Call the handler! */
	    hf->fcn( args );
	    used = 1;
	}
	hf++;
    }

    /* output a message if it wasn't handled. */
    if( !used ) {
	Filter_ToConsolePutString( "TR: Command not found: " );
	Filter_ToConsolePutString( buf );
	Filter_ToConsolePutString( "\n" );
    }
}
