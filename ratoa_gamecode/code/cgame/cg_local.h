/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//
#include "../qcommon/q_shared.h"
#include "../renderer/tr_types.h"
#include "../game/bg_public.h"
#include "cg_public.h"

#include "../game/challenges.h"


// The entire cgame module is unloaded and reloaded on each level change,
// so there is NO persistant data between levels on the client side.
// If you absolutely need something stored, it can either be kept
// by the server in the server stored userinfos, or stashed in a cvar.

#ifdef MISSIONPACK
#define CG_FONT_THRESHOLD 0.1
#endif

#define	POWERUP_BLINKS		5

#define	POWERUP_BLINK_TIME	1000
#define	FADE_TIME			200
#define	PULSE_TIME			200
#define	DAMAGE_DEFLECT_TIME	100
#define	DAMAGE_RETURN_TIME	400
#define DAMAGE_TIME			500
#define	LAND_DEFLECT_TIME	150
#define	LAND_RETURN_TIME	300
#define	STEP_TIME			200
#define	DUCK_TIME			100
#define	PAIN_TWITCH_TIME	200
#define	WEAPON_SELECT_TIME	1400
#define	ITEM_SCALEUP_TIME	1000
#define	ZOOM_TIME			150
#define	ITEM_BLOB_TIME		200
#define	MUZZLE_FLASH_TIME	20
#define	SINK_TIME			1000		// time for fragments to sink into ground before going away
#define	ATTACKER_HEAD_TIME	10000
#define	REWARD_TIME		2000
#define	REWARD2_TIME		3000
#define	REWARD2_SOUNDDELAY	1000

#define	PULSE_SCALE			1.5			// amount to scale up the icons when activating

#define	MAX_STEP_CHANGE		32

#define	MAX_VERTS_ON_POLY	10
#define	MAX_MARK_POLYS		4096

#define STAT_MINUS			10	// num frame for '-' stats digit

#define	ICON_SIZE			48
#define	CHAR_WIDTH			32
#define	CHAR_HEIGHT			48
#define	TEXT_ICON_SPACE		4

#define	TEAMCHAT_WIDTH		80
#define TEAMCHAT_HEIGHT		8

// very large characters.pk
#define	GIANT_WIDTH			32
#define	GIANT_HEIGHT		48

#define	NUM_CROSSHAIRS		64

#define TEAM_OVERLAY_MAXNAME_WIDTH	12
#define TEAM_OVERLAY_MAXLOCATION_WIDTH	16

#define	DEFAULT_MODEL			"sarge"
/*
#ifdef MISSIONPACK
#define	DEFAULT_TEAM_MODEL		"sergei"
#define	DEFAULT_TEAM_HEAD		"*sergei"
#else
*/
#define	DEFAULT_TEAM_MODEL		"sarge"
#define	DEFAULT_TEAM_HEAD		"sarge"
// #endif

#define DEFAULT_FORCED_MODEL		"sarge"
#define DEFAULT_FORCED_BRIGHT_SKIN	"pm"

#define MAX(a,b) (((a) > (b)) ? (a) : (b))

#define DEFAULT_REDTEAM_NAME		"Stroggs"
#define DEFAULT_BLUETEAM_NAME		"Pagans"

#define MAX_SPAWNPOINTS 64

#define MAX_PREDICTED_MISSILES	32

#define PLASMABALL_RADIUS 11

#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#define MAX(a,b) (((a) > (b)) ? (a) : (b))

// scoreboard header
//#define RATSB_HEADER   86
#define RATSB_HEADER   90

// for ratstatusbar 4
#define RSB4_NUM_HA_BAR_ELEMENTS 8
#define RSB4_NUM_HA_BAR_DECOR_ELEMENTS 3
#define RSB4_NUM_W_BAR_ELEMENTS 11
#define RSB4_NUM_W_BAR_DECOR_ELEMENTS 3

// DOTTED HEALTH/ARMOR/WEAPON BARS
#define DOTBAR_MAX_ELEMENTS MAX(10,MAX(RSB4_NUM_HA_BAR_ELEMENTS, RSB4_NUM_W_BAR_ELEMENTS))
typedef enum {
	DB_FILLED_EMPTY,
	DB_FILLED_FULL,
	DB_FILLED_EMPTYGLOW,
} dotbar_filled_t;

typedef struct {
	int lastFilledTimes[DOTBAR_MAX_ELEMENTS];
	dotbar_filled_t filled[DOTBAR_MAX_ELEMENTS];
} dotbar_t;

typedef enum {
	FOOTSTEP_NORMAL,
	FOOTSTEP_BOOT,
	FOOTSTEP_FLESH,
	FOOTSTEP_MECH,
	FOOTSTEP_ENERGY,
	FOOTSTEP_METAL,
	FOOTSTEP_SPLASH,

	FOOTSTEP_TOTAL
} footstep_t;

#define CROUCHSLIDE_SOUNDS 4

typedef enum {
	IMPACTSOUND_DEFAULT,
	IMPACTSOUND_METAL,
	IMPACTSOUND_FLESH
} impactSound_t;

typedef struct {
	vec3_t origin;
	vec3_t angle;
	int team;
} spawnpoint_t;

//=================================================

// player entities need to track more information
// than any other type of entity.

// note that not every player entity is a client entity,
// because corpses after respawn are outside the normal
// client numbering range

// when changing animation, set animationTime to frameTime + lerping time
// The current lerp will finish out, then it will lerp to the new animation
typedef struct {
	int			oldFrame;
	int			oldFrameTime;		// time when ->oldFrame was exactly on

	int			frame;
	int			frameTime;			// time when ->frame will be exactly on

	float		backlerp;

	float		yawAngle;
	qboolean	yawing;
	float		pitchAngle;
	qboolean	pitching;

	int			animationNumber;	// may include ANIM_TOGGLEBIT
	animation_t	*animation;
	int			animationTime;		// time when the first frame of the animation will be exact
} lerpFrame_t;


typedef struct {
	lerpFrame_t		legs, torso, flag;
	int				painTime;
	int				painDirection;	// flip from 0 to 1
	int				lightningFiring;

	// railgun trail spawning
	vec3_t			railgunImpact;
	qboolean		railgunFlash;

	// machinegun spinning
	float			barrelAngle;
	int				barrelTime;
	qboolean		barrelSpinning;

	// for crouchslide sound
	int 			crouchSlideSndCounter;	
} playerEntity_t;

//=================================================

#define MF_EXPLODED  		1
#define MF_HITPLAYER 		2
#define MF_HITWALL   		4
#define MF_HITWALLMETAL   	8
#define MF_DISAPPEARED 		16
#define MF_REMOVEDPMISSILE	32
#define MF_TRAILFINISHED	64
#define MF_EXPLOSIONCONFIRMED	128

typedef struct predictedMissileStatus_s {
	int	missileFlags;
	int	explosionTime;
	vec3_t	explosionPos;
	int	hitEntity;

	int 	expLEntityID;
} predictedMissileStatus_t;



// centity_t have a direct corespondence with gentity_t in the game, but
// only the entityState_t is directly communicated to the cgame
typedef struct centity_s {
	entityState_t	currentState;	// from cg.frame
	entityState_t	nextState;		// from cg.nextFrame, if available
	qboolean		interpolate;	// true if next is valid to interpolate to
	qboolean		currentValid;	// true if cg.frame holds this entity

	int				muzzleFlashTime;	// move to playerEntity?
	int				previousEvent;
	int				teleportFlag;

	int				trailTime;		// so missile trails can handle dropped initial packets
	int				dustTrailTime;
	int				miscTime;

	int				snapShotTime;	// last time this entity was found in a snapshot

	playerEntity_t	pe;

	int				errorTime;		// decay the error from this time
	vec3_t			errorOrigin;
	vec3_t			errorAngles;
	
	qboolean		extrapolated;	// false if origin / angles is an interpolation
	vec3_t			rawOrigin;
	vec3_t			rawAngles;

	vec3_t			beamEnd;

	// exact interpolated position of entity on this frame
	vec3_t			lerpOrigin;
	vec3_t			lerpAngles;

	// for cg_projectileNudgeAuto
	int			projectileNudge;

	// set if a player entity should not make sounds
	qboolean		quiet;
	qboolean		missileTeleported;
	predictedMissileStatus_t missileStatus;
} centity_t;


//======================================================================

// local entities are created as a result of events or predicted actions,
// and live independantly from all server transmitted entities

typedef struct markPoly_s {
	struct markPoly_s	*prevMark, *nextMark;
	int			time;
	qhandle_t	markShader;
	qboolean	alphaFade;		// fade alpha instead of rgb
	float		color[4];
	poly_t		poly;
	polyVert_t	verts[MAX_VERTS_ON_POLY];
} markPoly_t;


typedef enum {
	LE_MARK,
	LE_EXPLOSION,
	LE_SPRITE_EXPLOSION,
	LE_FRAGMENT,
	LE_MOVE_SCALE_FADE,
	LE_FALL_SCALE_FADE,
	LE_FADE_RGB,
	LE_FADE_RGB_SIN,
	LE_RAILTUBE,
	LE_SCALE_FADE,
	LE_LOCATIONPING,
	LE_SCOREPLUM,
	LE_DAMAGEPLUM,
/*
	LE_KAMIKAZE,
	LE_INVULIMPACT,
	LE_INVULJUICED,
*/
	LE_SHOWREFENTITY,

	LE_GORE,
} leType_t;

typedef enum {
	LEF_PUFF_DONT_SCALE  = 0x0001,			// do not scale size over time
	LEF_TUMBLE			 = 0x0002,			// tumble over time, used for ejecting shells
	LEF_SOUND1			 = 0x0004,			// sound 1 for kamikaze
	LEF_SOUND2			 = 0x0008,			// sound 2 for kamikaze
	LEF_FADE_RGB			 = 0x0010,			// fade all channels, not just alpha
	LEF_MOVE_OLDORIGIN		 = 0x0020,			// move oldorigin as well 
	LEF_PINGLOC_HUD			 = 0x0040			// draw a HUD direction marker for this location ping
} leFlag_t;

typedef enum {
	LEMT_NONE,
	LEMT_BURN,
	LEMT_BLOOD
} leMarkType_t;			// fragment local entities can leave marks on walls

typedef enum {
	LEBS_NONE,
	LEBS_BLOOD,
	LEBS_BRASS,
	LEBS_SHELL
} leBounceSoundType_t;	// fragment local entities can make sounds on impacts

typedef struct localEntity_s {
	struct localEntity_s	*prev, *next;
	leType_t		leType;
	int				leFlags;

	int				startTime;
	int				endTime;
	int				fadeInTime;

	float			lifeRate;			// 1.0 / (endTime - startTime)

	trajectory_t	pos;
	trajectory_t	pos2; // for oldorigin
	trajectory_t	angles;

	float			bounceFactor;		// 0.0 = no bounce, 1.0 = perfect

	float			color[4];

	float			radius;
	float			radius2;

	float			light;
	vec3_t			lightColor;

	leMarkType_t		leMarkType;		// mark to leave on fragment impact
	leBounceSoundType_t	leBounceSoundType;

	refEntity_t		refEntity;		

	// for entitytype-specific flags
	int generic;

	// to remove wrongfully predicted explosions
	// id = 0 for free/unused entities
	int			id;
} localEntity_t;


typedef struct predictedMissile_s {
	struct predictedMissile_s *prev, *next;

	int			weapon;

	trajectory_t	pos;
	trajectory_t	angles;

	// time at which to remove this missile even if it was not confirmed by the server
	int	removeTime;

	predictedMissileStatus_t status;

	refEntity_t		refEntity;		
} predictedMissile_t;

//======================================================================

typedef struct {
	int				client;
	int				score;
	int				ping;
	int				time;
	int				scoreFlags;
	int				powerUps;
	int				accuracy;
	int				impressiveCount;
	int				excellentCount;
	int				guantletCount;
	int				defendCount;
	int				assistCount;
	// int				eaward_counts[EAWARD_NUM_AWARDS];
	int				captures;
	int			flagrecovery;
	qboolean	perfect;
	int				team;
	int			isDead;
	int			kills;
	int			deaths;
	int			dmgGiven;
	int			dmgTaken;
	spectatorGroup_t	spectatorGroup;
	int			topweapon1;
	int			topweapon2;
	int			topweapon3;
	int			ratclient;
	int			yellow_armors;
	int			red_armors;
	int			mega_healths;
#ifdef WITH_MULTITOURNAMENT
	int			gameId;
#endif

} score_t;

// each client has an associated clientInfo_t
// that contains media references necessary to present the
// client model and other color coded effects
// this is regenerated each time a client's configstring changes,
// usually as a result of a userinfo (name, model, etc) change
#define	MAX_CUSTOM_SOUNDS	32

typedef struct {
	qboolean		infoValid;

	char			name[MAX_QPATH];
	team_t			team;

	int				botSkill;		// 0 = not bot, 1-5 = bot

	vec3_t			color1;
	vec3_t			color2;
	vec3_t			color3;

	int			playerColorIndex;

	int				score;			// updated by score servercmds
	int				location;		// location index for team mode
	int				health;			// you only get this info about your teammates
	int				armor;
	int				curWeapon;
	int				respawnTime;

	int				handicap;
	int				wins, losses;	// in tourney mode

	int				teamTask;		// task in teamplay (offence/defence)
	qboolean		teamLeader;		// true when this is a team leader

	int				powerups;		// so can display quad/flag status

	int				medkitUsageTime;
	int				invulnerabilityStartTime;
	int				invulnerabilityStopTime;

	int				breathPuffTime;

	// when clientinfo is changed, the loading of models/skins/sounds
	// can be deferred until you are dead, to prevent hitches in
	// gameplay
	char			modelName[MAX_QPATH];
	char			skinName[MAX_QPATH];
	char			headModelName[MAX_QPATH];
	char			headSkinName[MAX_QPATH];
	char			redTeam[MAX_TEAMNAME];
	char			blueTeam[MAX_TEAMNAME];
	qboolean		deferred;

	qboolean	forcedModel; 			// true if the model was forced through cg_teamModel or cg_enemyModel
	qboolean	forcedBrightModel; 		// true if it's a forced bright model (e.g. cg_teamModel smarine/bright)

	qboolean		newAnims;		// true if using the new mission pack animations
	qboolean		fixedlegs;		// true if legs yaw is always the same as torso yaw
	qboolean		fixedtorso;		// true if torso never changes yaw

	vec3_t			headOffset;		// move head in icon views
	footstep_t		footsteps;
	gender_t		gender;			// from model

	qhandle_t		legsModel;
	qhandle_t		legsSkin;

	qhandle_t		torsoModel;
	qhandle_t		torsoSkin;

	qhandle_t		headModel;
	qhandle_t		headSkin;

	qhandle_t		modelIcon;

	animation_t		animations[MAX_TOTALANIMATIONS];

	sfxHandle_t		sounds[MAX_CUSTOM_SOUNDS];

	int		isDead;

	int 		lastPinglocationTime;

#ifdef WITH_MULTITOURNAMENT
	// for GT_MULTITOURNAMENT
	int		gameId;
#endif

} clientInfo_t;


// each WP_* weapon enum has an associated weaponInfo_t
// that contains media references necessary to present the
// weapon and its effects
typedef struct weaponInfo_s {
	qboolean		registered;
	gitem_t			*item;

	qhandle_t		handsModel;			// the hands don't actually draw, they just position the weapon
	qhandle_t		weaponModel;
	qhandle_t		barrelModel;
	qhandle_t		flashModel;

	vec3_t			weaponMidpoint;		// so it will rotate centered instead of by tag

	float			flashDlight;
	vec3_t			flashDlightColor;
	sfxHandle_t		flashSound[4];		// fast firing weapons randomly choose

	qhandle_t		weaponIcon;
	qhandle_t		ammoIcon;

	qhandle_t		ammoModel;

	qhandle_t		missileModel;
	sfxHandle_t		missileSound;
	void			(*missileTrailFunc)( centity_t *, const struct weaponInfo_s *wi );
	float			missileDlight;
	vec3_t			missileDlightColor;
	int				missileRenderfx;

	void			(*ejectBrassFunc)( centity_t * );

	float			trailRadius;
	float			wiTrailTime;

	sfxHandle_t		readySound;
	sfxHandle_t		firingSound;
	qboolean		loopFireSound;
} weaponInfo_t;


// each IT_* item has an associated itemInfo_t
// that constains media references necessary to present the
// item and its effects
typedef struct {
	qboolean		registered;
	qhandle_t		models[MAX_ITEM_MODELS];
	qhandle_t		icon;
} itemInfo_t;


typedef struct {
	int				itemNum;
} powerupInfo_t;


#define MAX_SKULLTRAIL		10

typedef struct {
	vec3_t positions[MAX_SKULLTRAIL];
	int numpositions;
} skulltrail_t;

//=====================================SUPERHUD=========================

// hudElements_t
// struct to save all the hudelement data
typedef struct {
	qboolean inuse;
	int xpos;
	int ypos;
	int width;
	int height;
	float color[4];
	float bgcolor[4];
	qboolean fill;
	int fontWidth;
	int fontHeight;
	char *image;
	char *text;
	int textAlign;
	int textstyle;
	int time;
	qhandle_t imageHandle;
	int teamColor;
	int teamBgColor;
	char *cvar;
	int cvarValue;
} hudElements_t;

enum{
	HUD_DEFAULT,
	HUD_AMMOWARNING,
	HUD_ATTACKERICON,
	HUD_ATTACKERNAME,
	HUD_CHAT1,
	HUD_CHAT2,
	HUD_CHAT3,
	HUD_CHAT4,
	HUD_CHAT5,
	HUD_CHAT6,
	HUD_CHAT7,
	HUD_CHAT8,
	HUD_FS_OWN,
	HUD_FS_NME,
	HUD_FOLLOW,
	HUD_FPS,
	HUD_FRAGMSG,
	HUD_GAMETIME,
	HUD_CATIME,
	HUD_GAMETYPE,
	HUD_ITEMPICKUPNAME,
	HUD_ITEMPICKUPTIME,
	HUD_ITEMPICKUPICON,
	HUD_NETGRAPH,
	HUD_NETGRAPHPING,
	HUD_SPEED,
	HUD_ACCEL,
	HUD_PU1,
	HUD_PU2,
	HUD_PU3,
	HUD_PU4,
	HUD_PU1ICON,
	HUD_PU2ICON,
	HUD_PU3ICON,
	HUD_PU4ICON,
	HUD_RANKMSG,
	HUD_SCORELIMIT,
	HUD_SCORENME,
	HUD_SCOREOWN,
	HUD_SPECMESSAGE,
	HUD_ARMORBAR,
	HUD_ARMORCOUNT,
	HUD_ARMORICON,
	HUD_AMMOBAR,
	HUD_AMMOCOUNT,
	HUD_AMMOICON,
	HUD_HEALTHBAR,
	HUD_HEALTHCOUNT,
	HUD_HEALTHICON,
	HUD_TARGETNAME,
	HUD_TARGETSTATUS,
	HUD_TC_NME,
	HUD_TC_OWN,
	HUD_TI_NME,
	HUD_TI_OWN,
	HUD_TEAMCHAT1,
	HUD_TEAMCHAT2,
	HUD_TEAMCHAT3,
	HUD_TEAMCHAT4,
	HUD_TEAMCHAT5,
	HUD_TEAMCHAT6,
	HUD_TEAMCHAT7,
	HUD_TEAMCHAT8,
	HUD_VOTEMSG,
	HUD_WARMUP,
	HUD_WEAPONLIST,
	HUD_READYSTATUS,
	HUD_DEATHNOTICE1,
	HUD_DEATHNOTICE2,
	HUD_DEATHNOTICE3,
	HUD_DEATHNOTICE4,
	HUD_DEATHNOTICE5,
	HUD_COUNTDOWN,
	HUD_RESPAWNTIMER,
	HUD_STATUSBARFLAG,
	HUD_TEAMOVERLAY1,
	HUD_TEAMOVERLAY2,
	HUD_TEAMOVERLAY3,
	HUD_TEAMOVERLAY4,
	HUD_TEAMOVERLAY5,
	HUD_TEAMOVERLAY6,
	HUD_TEAMOVERLAY7,
	HUD_TEAMOVERLAY8,
	HUD_REWARD,
	HUD_REWARDCOUNT,
	HUD_CONSOLE,
	HUD_PREDECORATE1,
	HUD_PREDECORATE2,
	HUD_PREDECORATE3,
	HUD_PREDECORATE4,
	HUD_PREDECORATE5,
	HUD_PREDECORATE6,
	HUD_PREDECORATE7,
	HUD_PREDECORATE8,
	HUD_POSTDECORATE1,
	HUD_POSTDECORATE2,
	HUD_POSTDECORATE3,
	HUD_POSTDECORATE4,
	HUD_POSTDECORATE5,
	HUD_POSTDECORATE6,
	HUD_POSTDECORATE7,
	HUD_POSTDECORATE8,

	HUD_MAX
};

//=========================SUPERHUD END==================================


#define MAX_REWARDSTACK		10
#define MAX_SOUNDBUFFER		30

#define MAX_REWARDROW		10
#define MAX_REWARDSOUNDBUFFER	10


#define NUM_RAILSPIRALSHADERS	10



//======================================================================

// all cg.stepTime, cg.duckTime, cg.landTime, etc are set to cg.time when the action
// occurs, and they will have visible effects for #define STEP_TIME or whatever msec after

#define MAX_PREDICTED_EVENTS	16

//unlagged - optimized prediction
#define NUM_SAVED_STATES (CMD_BACKUP + 2)
//unlagged - optimized prediction
 
typedef struct {
	int			clientFrame;		// incremented each frame

	int			clientNum;
	
	qboolean	demoPlayback;
	qboolean	demoRecording;
	qboolean	levelShot;			// taking a level menu screenshot
	int			deferredPlayerLoading;
	qboolean	loading;			// don't defer players at initial startup
	qboolean	intermissionStarted;	// don't play voice rewards, because game will end shortly

	// there are only one or two snapshot_t that are relevent at a time
	int			latestSnapshotNum;	// the number of snapshots the client system has received
	int			latestSnapshotTime;	// the time from latestSnapshotNum, so we don't need to read the snapshot yet

	snapshot_t	*snap;				// cg.snap->serverTime <= cg.time
	snapshot_t	*nextSnap;			// cg.nextSnap->serverTime > cg.time, or NULL
	snapshot_t	activeSnapshots[2];

	float		frameInterpolation;	// (float)( cg.time - cg.frame->serverTime ) / (cg.nextFrame->serverTime - cg.frame->serverTime)

	qboolean	thisFrameTeleport;
	qboolean	nextFrameTeleport;

	int			frametime;		// cg.time - cg.oldTime

	int			time;			// this is the time value that the client
								// is rendering at.
	int			oldTime;		// time at last frame, used for missile trails and prediction checking

	int			physicsTime;	// either cg.snap->time or cg.nextSnap->time

	int			timelimitWarnings;	// 5 min, 1 min, overtime
	int			fraglimitWarnings;

	qboolean	mapRestart;			// set on a map restart to set back the weapon

	qboolean	renderingThirdPerson;		// during deaths, chasecams, etc

	// prediction state
	qboolean	hyperspace;				// true if prediction has hit a trigger_teleport
	int		predictedTeleports;			// number of predicted teleports

	playerState_t	predictedPlayerState;
	centity_t		predictedPlayerEntity;
	qboolean	validPPS;				// clear until the first call to CG_PredictPlayerState
	int			predictedErrorTime;
	vec3_t		predictedError;

	int			eventSequence;
	int			predictableEvents[MAX_PREDICTED_EVENTS];

	float		stepChange;				// for stair up smoothing
	int			stepTime;

	float		duckChange;				// for duck viewheight smoothing
	int			duckTime;

	float		landChange;				// for landing hard
	int			landTime;

	// input state sent to server
	int			weaponSelect;

	// auto rotating items
	vec3_t		autoAngles;
	vec3_t		autoAxis[3];
	vec3_t		autoAnglesFast;
	vec3_t		autoAxisFast[3];

	// view rendering
	refdef_t	refdef;
	vec3_t		refdefViewAngles;		// will be converted to refdef.viewaxis

	// zoom key
	qboolean	zoomed;
	int			zoomTime;
	float		zoomSensitivity;
	qboolean	specZoomed;

	// information screen text during loading
	char		infoScreenText[MAX_STRING_CHARS];

	// scoreboard
	int			scoresRequestTime;
	int			numScores;
	qboolean		medals_available;
	qboolean		stats_available;
	int			selectedScore;
	int			teamScores[2];
	score_t		scores[MAX_CLIENTS];
	int		numScores_buf;
	int		ratscores_expected;
	int		received_ratscores;
	score_t		scores_buf[MAX_CLIENTS];
	qboolean	showScores;
	int		showScoreboardNum;
	qboolean	scoreBoardShowing;
	int			scoreFadeTime;

	qboolean teamsLocked;
	qboolean teamQueueSystem;

        int		accuracys[WP_NUM_WEAPONS][2];
	int		accRequestTime;
	qboolean	showAcc;
	qboolean	accBoardShowing;
	int		accFadeTime;


	char		killerName[MAX_NAME_LENGTH];
	char			spectatorList[MAX_STRING_CHARS];		// list of names
	int				spectatorLen;												// length of list
	float			spectatorWidth;											// width in device units
	int				spectatorTime;											// next time to offset
	int				spectatorPaintX;										// current paint x
	int				spectatorPaintX2;										// current paint x
	int				spectatorOffset;										// current offset from start
	int				spectatorPaintLen; 									// current offset from start

	// skull trails
	skulltrail_t	skulltrails[MAX_CLIENTS];

	// centerprinting
	int			centerPrintTime;
	int			centerPrintCharWidth;
	int			centerPrintY;
	char		centerPrint[1024];
	int			centerPrintLines;

	// low ammo warning state
	int			lowAmmoWarning;		// 1 = low, 2 = empty

	// kill timers for carnage reward
	int			lastKillTime;

	// crosshair client ID
	int			crosshairClientNum;
	int			crosshairClientTime;

	// powerup active flashing
	int			powerupActive;
	int			powerupTime;

	// attacking player
	int			attackerTime;
	int			voiceTime;

	// reward medals
	int			rewardStack;
	int			rewardTime;
	int			rewardCount[MAX_REWARDSTACK];
	qhandle_t	rewardShader[MAX_REWARDSTACK];
	qhandle_t	rewardSound[MAX_REWARDSTACK];

	// for new, faster reward display row
	qhandle_t	reward2Shader[MAX_REWARDROW];
	int		reward2RowTimes[MAX_REWARDROW];
	int		reward2Count[MAX_REWARDROW];
	int		reward2SoundDelay[MAX_REWARDROW];

	// sound buffer mainly for announcer sounds
	int			soundBufferIn;
	int			soundBufferOut;
	int			soundTime;
	qhandle_t	soundBuffer[MAX_SOUNDBUFFER];

	// sound buffer for reward sounds
	int			rewardSoundBufferIn;
	int			rewardSoundBufferOut;
	int			rewardSoundTime;
	qhandle_t		rewardSoundBuffer[MAX_REWARDSOUNDBUFFER];

	// for voice chat buffer
	int			voiceChatTime;
	int			voiceChatBufferIn;
	int			voiceChatBufferOut;

	// warmup countdown
	int			warmup;
	int			warmupCount;

	//==========================

	int			itemPickup;
	int			itemPickupTime;
	int			itemPickupBlendTime;	// the pulse around the crosshair is timed seperately

	int			weaponSelectTime;
	int			weaponAnimation;
	int			weaponAnimationTime;

	// blend blobs
	float		damageTime;
	float		damageX, damageY, damageValue;

	// status bar head
	float		headYaw;
	float		headEndPitch;
	float		headEndYaw;
	int			headEndTime;
	float		headStartPitch;
	float		headStartYaw;
	int			headStartTime;

	// view movement
	float		v_dmg_time;
	float		v_dmg_pitch;
	float		v_dmg_roll;

	vec3_t		kick_angles;	// weapon kicks
	vec3_t		kick_origin;

	// temp working variables for player view
	float		bobfracsin;
	int			bobcycle;
	float		xyspeed;
	int     nextOrbitTime;

	//qboolean cameraMode;		// if rendering from a loaded camera


	// development tool
	refEntity_t		testModelEntity;
	char			testModelName[MAX_QPATH];
	qboolean		testGun;

//unlagged - optimized prediction
	int			lastPredictedCommand;
	int			lastServerTime;
	playerState_t savedPmoveStates[NUM_SAVED_STATES];
	int			stateHead, stateTail;
	int			cmdMsecDelta;
//unlagged - optimized prediction

        //time that the client will respawn. If 0 = the player is alive.
        int respawnTime;
        
        int redObeliskHealth;
        int blueObeliskHealth;

	spectatorGroup_t spectatorGroup;
	int readyMask;
	int numSpawnpoints;
	spawnpoint_t spawnpoints[MAX_SPAWNPOINTS];
	int spectatorHelpDrawTime;

	int elimLastPlayerTime;

	int lastHitTime[64];
	int lastHitDamage[64];

	dotbar_t healthbar;
	dotbar_t armorbar;
	dotbar_t weaponbar;

	vec3_t		accel;
	vec3_t		lastaccel;
	int		AccelTime;
	int		lastAccelTime;

	int		speed;

	char		fragMessage[512];
	int		fragMessageTime;
	char		rankMessage[512];
	int		rankMessageTime;

	int		quadKills;
	qboolean	forceChat;
} cg_t;


// all of the model, shader, and sound references that are
// loaded at gamestate time are stored in cgMedia_t
// Other media that can be tied to clients, weapons, or items are
// stored in the clientInfo_t, itemInfo_t, weaponInfo_t, and powerupInfo_t
typedef struct {
	qhandle_t	charsetShader;
	qhandle_t	charsetShader64;
	qhandle_t	charsetShader32;
	qhandle_t	charsetShader16;
	qhandle_t	charsetProp;
	qhandle_t	charsetPropGlow;
	qhandle_t	charsetPropB;
	qhandle_t	whiteShader;

	qhandle_t	redCubeModel;
	qhandle_t	blueCubeModel;
	qhandle_t	redCubeIcon;
	qhandle_t	blueCubeIcon;
	qhandle_t	redFlagModel;
	qhandle_t	blueFlagModel;
	qhandle_t	neutralFlagModel;
	qhandle_t	redFlagShader[3];
	qhandle_t	blueFlagShader[3];
	qhandle_t	neutralFlagShader[3];
	qhandle_t	flagShader[4];

// For Treasure Hunter:
	qhandle_t	thToken;
	//qhandle_t	thTokenTeamShader;
	//qhandle_t	thTokenRedShader;
	//qhandle_t	thTokenBlueShader;
	qhandle_t	thTokenRedIShader;
	qhandle_t	thTokenBlueIShader;
	qhandle_t	thTokenRedISolidShader;
	qhandle_t	thTokenBlueISolidShader;

//For Double Domination:
	//qhandle_t	ddPointA;
	//qhandle_t	ddPointB;
	qhandle_t	ddPointSkinA[4]; //white,red,blue,none
        qhandle_t	ddPointSkinB[4]; //white,red,blue,none

	qhandle_t	flagPoleModel;
	qhandle_t	flagFlapModel;

	qhandle_t	redFlagFlapSkin;
	qhandle_t	blueFlagFlapSkin;
	qhandle_t	neutralFlagFlapSkin;

	qhandle_t	redFlagBaseModel;
	qhandle_t	blueFlagBaseModel;
	qhandle_t	neutralFlagBaseModel;

	/*
	qhandle_t	overloadBaseModel;
	qhandle_t	overloadTargetModel;
	qhandle_t	overloadLightsModel;
	qhandle_t	overloadEnergyModel;

	qhandle_t	harvesterModel;
	qhandle_t	harvesterRedSkin;
	qhandle_t	harvesterBlueSkin;
	qhandle_t	harvesterNeutralModel;
	*/

	qhandle_t	armorModel;
	qhandle_t	armorIcon;
	qhandle_t	healthIcon;

	qhandle_t	armorIconBlue;
	qhandle_t	healthIconBlue;
	qhandle_t	armorIconRed;
	qhandle_t	healthIconRed;

	qhandle_t	weaponSelectShader11;
	qhandle_t	weaponSelectShader13;
	qhandle_t	weaponSelectShaderTech;
	qhandle_t	weaponSelectShaderTechBorder;
	qhandle_t	weaponSelectShaderCircle;
	qhandle_t	weaponSelectShaderCircleGlow;
	qhandle_t	noammoCircleShader;

	qhandle_t	powerupFrameShader;
	qhandle_t	bottomFPSShaderDecor;
	qhandle_t	bottomFPSShaderColor;

	qhandle_t	damageIndicatorCenter;
	qhandle_t	damageIndicatorBottom;
	qhandle_t	damageIndicatorTop;
	qhandle_t	damageIndicatorRight;
	qhandle_t	damageIndicatorLeft;
	
	qhandle_t	movementKeyIndicatorJump;
	qhandle_t	movementKeyIndicatorCrouch;
	qhandle_t	movementKeyIndicatorUp;
	qhandle_t	movementKeyIndicatorDown;
	qhandle_t	movementKeyIndicatorLeft;
	qhandle_t	movementKeyIndicatorRight;

	qhandle_t	bardot;
	qhandle_t	bardot_additiveglow;
	qhandle_t	bardot_transparentglow;

	// for Ratstatusbar 4/5
	// 4 for Ratstatusbar 4 shaders, 5 for Ratstatusbar 5 (vertically flipped)
	int		rsb4_shadersLoaded;
	// glowing elements:
	qhandle_t	rsb4_health_shaders[RSB4_NUM_HA_BAR_ELEMENTS];
	qhandle_t	rsb4_health_glowShaders[RSB4_NUM_HA_BAR_ELEMENTS];
	qhandle_t	rsb4_health_additiveGlowShaders[RSB4_NUM_HA_BAR_ELEMENTS];
	qhandle_t	rsb4_armor_shaders[RSB4_NUM_HA_BAR_ELEMENTS];
	qhandle_t	rsb4_armor_glowShaders[RSB4_NUM_HA_BAR_ELEMENTS];
	qhandle_t	rsb4_armor_additiveGlowShaders[RSB4_NUM_HA_BAR_ELEMENTS];

	qhandle_t	rsb4_weapon_shaders[RSB4_NUM_W_BAR_ELEMENTS];
	qhandle_t	rsb4_weapon_glowShaders[RSB4_NUM_W_BAR_ELEMENTS];
	qhandle_t	rsb4_weapon_additiveGlowShaders[RSB4_NUM_W_BAR_ELEMENTS];
	// decor elements
	qhandle_t	rsb4_health_decorShaders[RSB4_NUM_HA_BAR_DECOR_ELEMENTS];
	qhandle_t	rsb4_armor_decorShaders[RSB4_NUM_HA_BAR_DECOR_ELEMENTS];
	qhandle_t	rsb4_weapon_decorShaders[RSB4_NUM_W_BAR_DECOR_ELEMENTS];

	qhandle_t	rsb4_health_bg;
	qhandle_t	rsb4_health_bg_border;
	qhandle_t	rsb4_armor_bg;
	qhandle_t	rsb4_armor_bg_border;

	qhandle_t	teamStatusBar;

	qhandle_t	deferShader;

	// gib explosions
	qhandle_t	gibAbdomen;
	qhandle_t	gibArm;
	qhandle_t	gibChest;
	qhandle_t	gibFist;
	qhandle_t	gibFoot;
	qhandle_t	gibForearm;
	qhandle_t	gibIntestine;
	qhandle_t	gibLeg;
	qhandle_t	gibSkull;
	qhandle_t	gibBrain;

	qhandle_t	smoke2;

	qhandle_t	machinegunBrassModel;
	qhandle_t	shotgunBrassModel;

	qhandle_t	railRingsShader;
	qhandle_t	railCoreShader;
	qhandle_t	ratRailCoreShader;
	qhandle_t	ratRailSpiralShaders[NUM_RAILSPIRALSHADERS];
	qhandle_t	ratRailCoreShaderOverlay;
	qhandle_t	ratRailSpiralModel;
	qhandle_t	ratRailTubeShader100;
	qhandle_t	ratRailTubeShader50;

	qhandle_t	lightningShader;

	//qhandle_t	friendShader;
	//qhandle_t	friendShaderThroughWalls;

	qhandle_t	friendColorShaders[6];
	qhandle_t	friendThroughWallColorShaders[6];

	qhandle_t	friendFlagShaderNeutral;
	qhandle_t	friendFlagShaderBlue;
	qhandle_t	friendFlagShaderRed;
	
	qhandle_t	friendFrozenShader;

	qhandle_t	radarShader;
	qhandle_t	radarDotShader;

	qhandle_t	pingLocation;
	qhandle_t	pingLocationHudMarker;
	qhandle_t	pingLocationEnemyHudMarker;
	qhandle_t	pingLocationBg;
	qhandle_t	pingLocationFg;
	qhandle_t	pingLocationWarn;
	qhandle_t	pingLocationDead;
	qhandle_t	pingLocationEnemyFg;
	qhandle_t	pingLocationEnemyBg;
	qhandle_t	pingLocationBlueFlagBg;
	qhandle_t	pingLocationBlueFlagFg;
	qhandle_t	pingLocationBlueFlagHudMarker;
	qhandle_t	pingLocationRedFlagBg;
	qhandle_t	pingLocationRedFlagFg;
	qhandle_t	pingLocationRedFlagHudMarker;
	qhandle_t	pingLocationNeutralFlagBg;
	qhandle_t	pingLocationNeutralFlagFg;
	qhandle_t	pingLocationNeutralFlagHudMarker;

	qhandle_t	balloonShader;
	qhandle_t	connectionShader;

	qhandle_t	selectShader;
	qhandle_t	viewBloodShader;
	qhandle_t	tracerShader;
	qhandle_t	crosshairShader[NUM_CROSSHAIRS];
	qhandle_t	crosshairOutlineShader[NUM_CROSSHAIRS];
	qhandle_t	lagometerShader;
	qhandle_t	backTileShader;
	qhandle_t	noammoShader;

	qhandle_t	zoomScopeMGShader;
	qhandle_t	zoomScopeRGShader;

	qhandle_t	smokePuffShader;
	qhandle_t	smokePuffRageProShader;
	qhandle_t	plasmaTrailShader;
	qhandle_t	shotgunSmokePuffShader;
	qhandle_t	plasmaBallShader;
	qhandle_t	waterBubbleShader;
	qhandle_t	bloodTrailShader;



	// LEILEI shaders

	qhandle_t	lsmkShader1;
	qhandle_t	lsmkShader2;
	qhandle_t	lsmkShader3;
	qhandle_t	lsmkShader4;
	qhandle_t	lbumShader1;
	qhandle_t	lfblShader1;
	qhandle_t	lsplShader;
	qhandle_t	lspkShader1;
	qhandle_t	lspkShader2;
	qhandle_t	lbldShader1;
	qhandle_t	lbldShader2;
	qhandle_t	grappleShader;	// leilei - grapple hook
	qhandle_t	lmarkmetal1;
	qhandle_t	lmarkmetal2;
	qhandle_t	lmarkmetal3;
	qhandle_t	lmarkmetal4;
	qhandle_t	lmarkbullet1;
	qhandle_t	lmarkbullet2;
	qhandle_t	lmarkbullet3;
	qhandle_t	lmarkbullet4;

/*
//#ifdef MISSIONPACK
	qhandle_t	nailPuffShader;
	qhandle_t	blueProxMine;
//#endif
*/

	qhandle_t	numberShaders[11];

	qhandle_t	shadowMarkShader;

	qhandle_t	botSkillShaders[5];

	// wall mark shaders
	qhandle_t	wakeMarkShader;
	qhandle_t	bloodMarkShader;
	qhandle_t	bulletMarkShader;
	qhandle_t	burnMarkShader;
	qhandle_t	holeMarkShader;
	qhandle_t	energyMarkShader;

	// powerup shaders
	qhandle_t	quadShader;
	qhandle_t	quadShaderBase;
	qhandle_t	quadShaderSpots;
	qhandle_t	redQuadShader;
	qhandle_t	quadWeaponShader;
	qhandle_t	invisShader;
	qhandle_t	regenShader;
	qhandle_t	battleSuitShader;
	qhandle_t	battleWeaponShader;
	qhandle_t	hastePuffShader;
	//qhandle_t	redKamikazeShader;
	//qhandle_t	blueKamikazeShader;

	qhandle_t	frozenShader;
	qhandle_t	thawingShader;

	qhandle_t	spawnPointShader;
        
        // player overlays 
        qhandle_t       neutralOverlay;
        qhandle_t       redOverlay;
        qhandle_t       blueOverlay;

	// bright shell overlay
        qhandle_t       brightShell;
        qhandle_t       brightShellBlend;
        qhandle_t       brightShellFlat;
        qhandle_t       brightOutline;
        qhandle_t       brightOutlineBlend;
        qhandle_t       brightOutlineOpaque;
        qhandle_t       brightOutlineSmall;
        qhandle_t       brightOutlineSmallBlend;

	// weapon effect models
	qhandle_t	bulletFlashModel;
	qhandle_t	ringFlashModel;
	qhandle_t	dishFlashModel;
	qhandle_t	lightningExplosionModel;

	qhandle_t	grenadeBrightSkinShader;
	qhandle_t	grenadeBrightSkinShaderBlue;
	qhandle_t	grenadeBrightSkinShaderRed;
	qhandle_t	grenadeBrightSkinShaderWhite;

	// weapon effect shaders
	qhandle_t	railExplosionShader;
	qhandle_t	plasmaExplosionShader;
	qhandle_t	bulletExplosionShader;
	qhandle_t	rocketExplosionShader;
	qhandle_t	grenadeExplosionShader;
	qhandle_t	bfgExplosionShader;
	qhandle_t	bloodExplosionShader;

	// special effects models
	qhandle_t	teleportEffectModel;
	qhandle_t	teleportEffectShader;
/*
//#ifdef MISSIONPACK
	qhandle_t	kamikazeEffectModel;
	qhandle_t	kamikazeShockWave;
	qhandle_t	kamikazeHeadModel;
	//qhandle_t	kamikazeHeadTrail;
	qhandle_t	guardPowerupModel;
	qhandle_t	scoutPowerupModel;
	qhandle_t	doublerPowerupModel;
	qhandle_t	ammoRegenPowerupModel;
	qhandle_t	invulnerabilityImpactModel;
	qhandle_t	invulnerabilityJuicedModel;
	qhandle_t	medkitUsageModel;
	qhandle_t	dustPuffShader;
	qhandle_t	heartShader;
//#endif
	qhandle_t	invulnerabilityPowerupModel;
*/

	// scoreboard headers
	qhandle_t	scoreboardName;
	qhandle_t	scoreboardPing;
	qhandle_t	scoreboardScore;
	qhandle_t	scoreboardTime;

	// medals shown during gameplay
	qhandle_t	medalImpressive;
	qhandle_t	medalExcellent;
	qhandle_t	medalGauntlet;
	qhandle_t	medalDefend;
	qhandle_t	medalAssist;
	qhandle_t	medalCapture;
	// new extended medals
	// qhandle_t	eaward_medals[EAWARD_NUM_AWARDS];

	// sounds
	sfxHandle_t	quadSound;
	sfxHandle_t	tracerSound;
	sfxHandle_t	selectSound;
	sfxHandle_t	useNothingSound;
	sfxHandle_t	wearOffSound;
	sfxHandle_t	footsteps[FOOTSTEP_TOTAL][4];
	sfxHandle_t	crouchslideSounds[CROUCHSLIDE_SOUNDS];
	sfxHandle_t	sfx_lghit1;
	sfxHandle_t	sfx_lghit2;
	sfxHandle_t	sfx_lghit3;
	sfxHandle_t	sfx_ric1;
	sfxHandle_t	sfx_ric2;
	sfxHandle_t	sfx_ric3;
	sfxHandle_t	sfx_railg;
	sfxHandle_t	sfx_rockexp;
	sfxHandle_t	sfx_plasmaexp;
/*
//#ifdef MISSIONPACK
	sfxHandle_t	sfx_proxexp;
	sfxHandle_t	sfx_nghit;
	sfxHandle_t	sfx_nghitflesh;
	sfxHandle_t	sfx_nghitmetal;
	sfxHandle_t	sfx_chghit;
	sfxHandle_t	sfx_chghitflesh;
	sfxHandle_t	sfx_chghitmetal;
	sfxHandle_t kamikazeExplodeSound;
	sfxHandle_t kamikazeImplodeSound;
	sfxHandle_t kamikazeFarSound;
	sfxHandle_t useInvulnerabilitySound;
	sfxHandle_t invulnerabilityImpactSound1;
	sfxHandle_t invulnerabilityImpactSound2;
	sfxHandle_t invulnerabilityImpactSound3;
	sfxHandle_t invulnerabilityJuicedSound;
	sfxHandle_t obeliskHitSound1;
	sfxHandle_t obeliskHitSound2;
	sfxHandle_t obeliskHitSound3;
	sfxHandle_t	obeliskRespawnSound;
	sfxHandle_t	winnerSound;
	sfxHandle_t	loserSound;
	sfxHandle_t	youSuckSound;
//#endif
*/
	sfxHandle_t	gibSound;
	sfxHandle_t	gibBounce1Sound;
	sfxHandle_t	gibBounce2Sound;
	sfxHandle_t	gibBounce3Sound;
	sfxHandle_t	teleShotSound;
	sfxHandle_t	teleInSound;
	sfxHandle_t	teleOutSound;
	sfxHandle_t	noAmmoSound;
	sfxHandle_t	respawnSound;
	sfxHandle_t talkSound;
	sfxHandle_t teamTalkSound;
	sfxHandle_t landSound;
	sfxHandle_t fallSound;
	sfxHandle_t jumpPadSound;

	sfxHandle_t pingLocationSound;
	sfxHandle_t pingLocationLowSound;
	sfxHandle_t pingLocationWarnSound;
	sfxHandle_t pingLocationWarnLowSound;

	sfxHandle_t queueJoinSound;
/*
// LEILEI
	sfxHandle_t	lspl1Sound;
	sfxHandle_t	lspl2Sound; // Blood Splat Noises
	sfxHandle_t	lspl3Sound;

	sfxHandle_t	lbul1Sound;
	sfxHandle_t	lbul2Sound;	// Bullet Drop Noises
	sfxHandle_t	lbul3Sound;

	sfxHandle_t	lshl1Sound;
	sfxHandle_t	lshl2Sound; // Shell Drop Noises
	sfxHandle_t	lshl3Sound;

// LEILEI END
*/
	
	sfxHandle_t oneMinuteSound;
	sfxHandle_t fiveMinuteSound;
	sfxHandle_t suddenDeathSound;

	sfxHandle_t threeFragSound;
	sfxHandle_t twoFragSound;
	sfxHandle_t oneFragSound;

	sfxHandle_t hitSound;
	/*
	sfxHandle_t hitSound0;
	sfxHandle_t hitSound1;
	sfxHandle_t hitSound2;
	sfxHandle_t hitSound3;
	sfxHandle_t hitSound4;
	sfxHandle_t hitSoundHighArmor;
	sfxHandle_t hitSoundLowArmor;
	*/
	sfxHandle_t hitTeamSound;
	sfxHandle_t accuracySound;
	sfxHandle_t fragsSound;
	sfxHandle_t impressiveSound;
	sfxHandle_t excellentSound;
	sfxHandle_t deniedSound;
	sfxHandle_t humiliationSound;
	sfxHandle_t assistSound;
	sfxHandle_t defendSound;
	sfxHandle_t perfectSound;
	/*
	sfxHandle_t firstImpressiveSound;
	sfxHandle_t firstExcellentSound;
	sfxHandle_t firstHumiliationSound;
	*/
	//sfxHandle_t eaward_sounds[EAWARD_NUM_AWARDS];

	sfxHandle_t takenLeadSound;
	sfxHandle_t tiedLeadSound;
	sfxHandle_t lostLeadSound;

	sfxHandle_t voteNow;
	sfxHandle_t votePassed;
	sfxHandle_t voteFailed;

	sfxHandle_t watrInSound;
	sfxHandle_t watrOutSound;
	sfxHandle_t watrUnSound;

	sfxHandle_t flightSound;
	sfxHandle_t medkitSound;

	//sfxHandle_t weaponHoverSound;

	// teamplay sounds
	sfxHandle_t captureAwardSound;
	sfxHandle_t redScoredSound;
	sfxHandle_t blueScoredSound;
	sfxHandle_t redLeadsSound;
	sfxHandle_t blueLeadsSound;
	sfxHandle_t teamsTiedSound;

	sfxHandle_t	captureYourTeamSound;
	sfxHandle_t	captureOpponentSound;
	sfxHandle_t	returnYourTeamSound;
	sfxHandle_t	returnOpponentSound;
	sfxHandle_t	takenYourTeamSound;
	sfxHandle_t	takenOpponentSound;

	sfxHandle_t	flagDroppedSound;

	sfxHandle_t redFlagReturnedSound;
	sfxHandle_t blueFlagReturnedSound;
	//sfxHandle_t neutralFlagReturnedSound;
	sfxHandle_t	enemyTookYourFlagSound;
	//sfxHandle_t	enemyTookTheFlagSound;
	sfxHandle_t yourTeamTookEnemyFlagSound;
	//sfxHandle_t yourTeamTookTheFlagSound;
	sfxHandle_t	youHaveFlagSound;
	//sfxHandle_t yourBaseIsUnderAttackSound;
	sfxHandle_t holyShitSound;

	// Elimination / CA / Extermination
	//sfxHandle_t oneLeftSound;
	//sfxHandle_t oneFriendLeftSound;
	//sfxHandle_t oneEnemyLeftSound;

	//sfxHandle_t elimPlayerRespawnSound;

	// tournament sounds
	sfxHandle_t	count3Sound;
	sfxHandle_t	count2Sound;
	sfxHandle_t	count1Sound;
	sfxHandle_t	countFightSound;
	sfxHandle_t	countPrepareSound;
/*
#ifdef MISSIONPACK
	// new stuff
	qhandle_t patrolShader;
	qhandle_t assaultShader;
	qhandle_t campShader;
	qhandle_t followShader;
	qhandle_t defendShader;
	qhandle_t teamLeaderShader;
	qhandle_t retrieveShader;
	qhandle_t escortShader;
        qhandle_t deathShader;
	qhandle_t flagShaders[3];
	sfxHandle_t	countPrepareTeamSound;
#endif
*/
	/*
	sfxHandle_t ammoregenSound;
	sfxHandle_t doublerSound;
	sfxHandle_t guardSound;
	sfxHandle_t scoutSound;
	*/
	qhandle_t cursor;
	qhandle_t selectCursor;
	qhandle_t sizeCursor;

	sfxHandle_t	regenSound;
	sfxHandle_t	protectSound;
	sfxHandle_t	n_healthSound;
	sfxHandle_t	hgrenb1aSound;
	sfxHandle_t	hgrenb2aSound;
	sfxHandle_t	wstbimplSound;
	sfxHandle_t	wstbimpmSound;
	sfxHandle_t	wstbimpdSound;
	sfxHandle_t	wstbactvSound;

	/*
	sfxHandle_t	announceQuad;
	sfxHandle_t	announceBattlesuit;
	sfxHandle_t	announceHaste;
	sfxHandle_t	announceInvis;
	sfxHandle_t	announceRegen;
	sfxHandle_t	announceFlight;

	sfxHandle_t	coinbounceSound;
	*/
	sfxHandle_t	freezeSound;

	// new media
	qhandle_t	redMarker;
	qhandle_t	blueMarker;
	qhandle_t	playericon;
	qhandle_t	grenadeMapoverview;
	qhandle_t	rocketMapoverview;
	qhandle_t	plasmaMapoverview;
	qhandle_t	bfgMapoverview;

} cgMedia_t;

#define CONSOLE_MAXHEIGHT 40
#define CONSOLE_WIDTH 80
typedef struct {
	char	msgs[CONSOLE_MAXHEIGHT][CONSOLE_WIDTH*3+1];
	int	msgTimes[CONSOLE_MAXHEIGHT];
	int	insertIdx;
	int	displayIdx;
} console_t;

#define MCIDX_HEAD 0
#define MCIDX_TORSO 1
#define MCIDX_LEGS 2
#define MCIDX_NUM 3
#define MODELCOLOR_DEFAULT 0
#define MODELCOLOR_ENEMY 1
#define MODELCOLOR_TEAM 2
#define MODELCOLOR_RED 1
#define MODELCOLOR_BLUE 2
#define MODELCOLOR_NUM 3

#define HELPMOTDSTATE_RECEIVED 1
#define HELPMOTDSTATE_SHOWN    2
#define HELPMOTDSTATE_HIDDEN   4

#define DEATHNOTICE_HEIGHT	5

#define MAX_RESPAWN_TIMERS	16

// The client game static (cgs) structure hold everything
// loaded or calculated from the gamestate.  It will NOT
// be cleared when a tournement restart is done, allowing
// all clients to begin playing instantly
typedef struct {
	gameState_t		gameState;			// gamestate from server
	glconfig_t		glconfig;			// rendering configuration
	float			screenXScale;		// derived from glconfig
	float			screenYScale;
	float			screenXBias;

	int				serverCommandSequence;	// reliable command stream counter
	int				processedSnapshotNum;// the number of snapshots cgame has requested

	qboolean		localServer;		// detected on startup by checking sv_running

	// parsed from serverinfo
	gametype_t		gametype;
	int				dmflags;
        int                             videoflags;
        int				elimflags;
	int				teamflags;
	int				fraglimit;
	int				capturelimit;
	int				timelimit;
	int				overtime;
	int				maxclients;
	char			mapname[MAX_QPATH];
	char			mapbasename[MAX_QPATH];
	char			redTeam[MAX_QPATH];
	char			blueTeam[MAX_QPATH];

	int				voteTime;
	int				voteYes;
	int				voteNo;
	qboolean		voteModified;			// beep whenever changed
	char			voteString[MAX_STRING_TOKENS];

	int			nextmapVoteEndTime;

	int				teamVoteTime[2];
	int				teamVoteYes[2];
	int				teamVoteNo[2];
	qboolean		teamVoteModified[2];	// beep whenever changed
	char			teamVoteString[2][MAX_STRING_TOKENS];

	int				levelStartTime;

	int			team_gt;

//Elimination
	int				roundStartTime;	
	int				roundtime;

// Treasure Hunter
	treasurehunter_t th_phase;
	int th_roundStart;
	int th_roundDuration;
	int th_redTokens;
	int th_blueTokens;
	int th_oldTokenStyle;
	int th_tokenStyle;

//CTF Elimination
	int				attackingTeam;

//Last Man Standing
	int				lms_mode;

//instantgib + nexuiz style rocket arena:
	int				nopickup;

//Double Domination DD
	int 				timetaken;

//Domination
	int domination_points_count;
	char domination_points_names[MAX_DOMINATION_POINTS][MAX_DOMINATION_POINTS_NAMES];
	int domination_points_status[MAX_DOMINATION_POINTS];


	// from configstrings
	int				scores1, scores2;
#ifdef WITH_MULTITOURNAMENT
	int				scores1Mtrn[MULTITRN_MAX_GAMES];
	int				scores2Mtrn[MULTITRN_MAX_GAMES];		
#endif
	int				redflag, blueflag;		// flag status from configstrings
	int				flagStatus;

	long			mtrnGameFlags;

	// for elimination modes
	int redLivingCount;
	int blueLivingCount;

	int elimNextRespawnTime;
	int elimStartHealth;	// Player's start health
	int elimStartArmor;	// Player's start armor
	
	int pushNotifyTime;
	int pushNotifyClientNum;

	qboolean  newHud;

	//
	// locally derived information from gamestate
	//
	qhandle_t		gameModels[MAX_MODELS];
	sfxHandle_t		gameSounds[MAX_SOUNDS];

	int				numInlineModels;
	qhandle_t		inlineDrawModel[MAX_MODELS];
	vec3_t			inlineModelMidpoints[MAX_MODELS];

	clientInfo_t	clientinfo[MAX_CLIENTS];

/*
	console_t commonConsole;
	console_t console;
	console_t chat;
	console_t teamChat;
	console_t helpMotdConsole;
*/

	int helpMotdState;

	// teamchat width is *3 because of embedded color codes
	char			teamChatMsgs[TEAMCHAT_HEIGHT][TEAMCHAT_WIDTH*3+1];
	int				teamChatMsgTimes[TEAMCHAT_HEIGHT];
	int				teamChatPos;
	int				teamLastChatPos;

	int cursorX;
	int cursorY;
	qboolean eventHandling;
	qboolean mouseCaptured;
	qboolean sizingHud;
	void *capturedItem;
	qhandle_t activeCursor;

	// orders
	int currentOrder;
	qboolean orderPending;
	int orderTime;
	int currentVoiceClient;
	int acceptOrderTime;
	int acceptTask;
	int acceptLeader;
	char acceptVoice[MAX_NAME_LENGTH];


	sfxHandle_t		mySounds[MAX_CUSTOM_SOUNDS];
	sfxHandle_t		teamSounds[MAX_CUSTOM_SOUNDS];
	sfxHandle_t		enemySounds[MAX_CUSTOM_SOUNDS];
	
	// player colors
	byte modelRGBA[MCIDX_NUM][MODELCOLOR_NUM][4];
	byte corpseRGBA[MCIDX_NUM][MODELCOLOR_NUM][4];

	// media
	cgMedia_t		media;

//unlagged - client options
	// this will be set to the server's g_delagHitscan
	int				delagHitscan;
//unlagged - client options
//KK-OAX For storing whether or not the server has multikills enabled. 
	int             altExcellent;

	int		startWhenReady;
	int		rocketSpeed;
	int		delagMissileMaxLatency;
	int		predictedMissileNudge;
	int		ratFlags;
	movement_t	movement;
	float		maxBrightshellAlpha;
	int		timeoutEnd;
	int		timeoutOvertime;
	char		sv_hostname[MAX_QPATH];

	hudElements_t	hud[HUD_MAX];
	int		csStatus;


	char		chatMsgs[TEAMCHAT_HEIGHT][TEAMCHAT_WIDTH * 3 + 1];
	int		chatMsgTimes[TEAMCHAT_HEIGHT];
	int		chatPos;
	int		lastChatPos;

	char		consoleMsgs[TEAMCHAT_HEIGHT][TEAMCHAT_WIDTH * 3 + 1];
	int		consoleMsgTimes[TEAMCHAT_HEIGHT];
	int		consolePos;
	int		lastConsolePos;

	int		deathNoticeTime[ DEATHNOTICE_HEIGHT ];
	char		deathNoticeName1[ DEATHNOTICE_HEIGHT ][ MAX_NAME_LENGTH ];
	char		deathNoticeName2[ DEATHNOTICE_HEIGHT ][ MAX_NAME_LENGTH ];
	int		deathNoticeTeam1[ DEATHNOTICE_HEIGHT ];
	int		deathNoticeTeam2[ DEATHNOTICE_HEIGHT ];
	qhandle_t 	deathNoticeIcon1[ DEATHNOTICE_HEIGHT ];
	qhandle_t 	deathNoticeIcon2[ DEATHNOTICE_HEIGHT ];
	qboolean 	deathNoticeTwoIcons[ DEATHNOTICE_HEIGHT ];

	qboolean	respawnTimerUsed[ MAX_RESPAWN_TIMERS ];
	int		respawnTimerEntitynum[ MAX_RESPAWN_TIMERS ];
	int		respawnTimerType[ MAX_RESPAWN_TIMERS ];
	int		respawnTimerQuantity[ MAX_RESPAWN_TIMERS ];
	int		respawnTimerTime[ MAX_RESPAWN_TIMERS ];
	int		respawnTimerNumber;
	int		respawnTimerNextItem[ MAX_RESPAWN_TIMERS ];
	int		respawnTimerTeam[ MAX_RESPAWN_TIMERS ];
	int		respawnTimerClientNum[MAX_RESPAWN_TIMERS];

	int		timeout;
	int		crosshair[WP_NUM_WEAPONS];
	int		crosshairSize[WP_NUM_WEAPONS];

} cgs_t;

//==============================================================================

extern	cgs_t			cgs;
extern	cg_t			cg;
extern	centity_t		cg_entities[MAX_GENTITIES];
extern	weaponInfo_t	cg_weapons[MAX_WEAPONS];
extern	itemInfo_t		cg_items[MAX_ITEMS];
extern	markPoly_t		cg_markPolys[MAX_MARK_POLYS];

extern	vmCvar_t		cg_centertime;
extern	vmCvar_t		cg_runpitch;
extern	vmCvar_t		cg_runroll;
extern	vmCvar_t		cg_bobup;
extern	vmCvar_t		cg_bobpitch;
extern	vmCvar_t		cg_bobroll;
extern	vmCvar_t		cg_swingSpeed;
extern	vmCvar_t		cg_shadows;
extern	vmCvar_t		cg_gibs;
extern	vmCvar_t		cg_drawTimer;
extern	vmCvar_t		cg_timerPosition;
extern	vmCvar_t		cg_drawFPS;
extern	vmCvar_t		cg_drawSnapshot;
extern	vmCvar_t		cg_draw3dIcons;
extern	vmCvar_t		cg_drawIcons;
extern	vmCvar_t		cg_drawAmmoWarning;
extern	vmCvar_t		cg_drawZoomScope;
extern	vmCvar_t		cg_zoomScopeSize;
extern	vmCvar_t		cg_zoomScopeRGColor;
extern	vmCvar_t		cg_zoomScopeMGColor;
extern	vmCvar_t		cg_drawCrosshair;
extern	vmCvar_t		cg_drawCrosshairNames;
extern	vmCvar_t		cg_drawRewards;
extern	vmCvar_t		cg_drawFollowPosition;
extern	vmCvar_t		cg_drawTeamOverlay;
extern	vmCvar_t		cg_teamOverlayUserinfo;
extern	vmCvar_t		cg_crosshairX;
extern	vmCvar_t		cg_crosshairY;
extern	vmCvar_t		cg_crosshairHit;
extern	vmCvar_t		cg_crosshairHitTime;
extern	vmCvar_t		cg_crosshairHitColor;
extern	vmCvar_t		cg_crosshairHitStyle;
extern	vmCvar_t		cg_crosshairSize;
extern	vmCvar_t		cg_crosshairHealth;
extern	vmCvar_t		cg_drawStatus;
extern	vmCvar_t		cg_draw2D;
extern	vmCvar_t		cg_animSpeed;
extern	vmCvar_t		cg_debugAnim;
extern	vmCvar_t		cg_debugPosition;
extern	vmCvar_t		cg_debugEvents;
extern	vmCvar_t		cg_drawBBox;
extern	vmCvar_t		cg_railTrailTime;
extern	vmCvar_t		cg_errorDecay;
extern	vmCvar_t		cg_nopredict;
extern	vmCvar_t		cg_checkChangedEvents;
extern	vmCvar_t		cg_noPlayerAnims;
extern	vmCvar_t		cg_showmiss;
extern	vmCvar_t		cg_footsteps;
extern	vmCvar_t		cg_addMarks;
extern	vmCvar_t		cg_brassTime;
extern	vmCvar_t		cg_gun_frame;
extern	vmCvar_t		cg_gun_x;
extern	vmCvar_t		cg_gun_y;
extern	vmCvar_t		cg_gun_z;
extern	vmCvar_t		cg_drawGun;
extern	vmCvar_t		cg_viewsize;
extern	vmCvar_t		cg_tracerChance;
extern	vmCvar_t		cg_tracerWidth;
extern	vmCvar_t		cg_tracerLength;
extern	vmCvar_t		cg_autoswitch;
extern	vmCvar_t		cg_ignore;
extern	vmCvar_t		cg_simpleItems;
extern	vmCvar_t		cg_fov;
extern	vmCvar_t		cg_horplus;
extern	vmCvar_t		cg_zoomFov;
extern	vmCvar_t		cg_zoomFovTmp;
extern	vmCvar_t		cg_thirdPersonRange;
extern	vmCvar_t		cg_thirdPersonAngle;
extern	vmCvar_t		cg_thirdPerson;
extern	vmCvar_t		cg_lagometer;
extern	vmCvar_t		cg_drawAttacker;
extern	vmCvar_t		cg_attackerScale;
extern	vmCvar_t		cg_drawPickup;
extern	vmCvar_t		cg_pickupScale;
extern	vmCvar_t		cg_drawSpeed;
extern	vmCvar_t		cg_drawSpeed3D;
extern	vmCvar_t		cg_synchronousClients;
extern	vmCvar_t		cg_teamChatTime;
extern	vmCvar_t		cg_teamChatHeight;
extern 	vmCvar_t 		cg_teamChatY;
extern 	vmCvar_t 		cg_teamChatScaleX;
extern 	vmCvar_t 		cg_teamChatScaleY;
extern	vmCvar_t		cg_stats;
extern	vmCvar_t 		cg_forceModel;
extern	vmCvar_t 		cg_buildScript;
extern	vmCvar_t		cg_paused;
extern	vmCvar_t		cg_blood;
extern	vmCvar_t		cg_predictItems;
extern	vmCvar_t		cg_predictItemsNearPlayers;
extern	vmCvar_t		cg_deferPlayers;
extern	vmCvar_t		cg_drawFriend;
extern  vmCvar_t		cg_friendHudMarker;
extern  vmCvar_t		cg_friendHudMarkerMaxDist;
extern  vmCvar_t		cg_friendHudMarkerSize;
extern  vmCvar_t		cg_friendHudMarkerMaxScale;
extern  vmCvar_t		cg_friendHudMarkerMinScale;
extern	vmCvar_t		cg_teamChatsOnly;
extern	vmCvar_t		cg_chat;
extern	vmCvar_t		cg_noVoiceChats;
extern	vmCvar_t		cg_noVoiceText;
extern  vmCvar_t		cg_scorePlum;
extern  vmCvar_t		cg_damagePlums;
extern  vmCvar_t		cg_damagePlumSize;
extern  vmCvar_t		cg_pushNotifications;
extern  vmCvar_t		cg_pushNotificationTime;

extern vmCvar_t                	cg_altInitialized;

extern vmCvar_t                	cg_predictTeleport;
extern vmCvar_t                	cg_predictWeapons;
extern vmCvar_t                	cg_predictExplosions;
extern vmCvar_t                	cg_predictPlayerExplosions;
extern vmCvar_t                	cg_altPredictMissiles;
extern vmCvar_t                	cg_altScoreboard;
extern vmCvar_t                	cg_altScoreboardAccuracy;
extern vmCvar_t                	cg_altStatusbar;
extern vmCvar_t                	cg_altStatusbarOldNumbers;
extern vmCvar_t                	cg_printDuelStats;
extern vmCvar_t			cg_altPlasmaTrail;
extern vmCvar_t			cg_altPlasmaTrailAlpha;
extern vmCvar_t			cg_altPlasmaTrailStep;
extern vmCvar_t			cg_altPlasmaTrailTime;
extern vmCvar_t			cg_altRail;
extern vmCvar_t			cg_altRailBeefy;
extern vmCvar_t			cg_altRailRadius;
extern vmCvar_t			cg_altLg;
extern vmCvar_t			cg_altLgImpact;
extern vmCvar_t			cg_lgSound;
extern vmCvar_t			cg_rgSound;
extern vmCvar_t			cg_consoleStyle;
extern vmCvar_t			cg_delagProjectileTrail;
extern vmCvar_t			cg_noBubbleTrail;
extern vmCvar_t			cg_specShowZoom;
extern vmCvar_t			cg_zoomToggle;
extern vmCvar_t			cg_zoomAnim;
extern vmCvar_t			cg_zoomAnimScale;
extern vmCvar_t			cg_sensScaleWithFOV;
extern vmCvar_t			cg_drawHabarBackground;
extern vmCvar_t			cg_drawHabarDecor;
extern vmCvar_t			cg_hudDamageIndicator;
extern vmCvar_t			cg_hudDamageIndicatorScale;
extern vmCvar_t			cg_hudDamageIndicatorOffset;
extern vmCvar_t			cg_hudDamageIndicatorAlpha;
extern vmCvar_t			cg_hudMovementKeys;
extern vmCvar_t			cg_hudMovementKeysScale;
extern vmCvar_t			cg_hudMovementKeysColor;
extern vmCvar_t			cg_hudMovementKeysAlpha;
extern vmCvar_t			cg_emptyIndicator;
extern vmCvar_t			cg_reloadIndicator;
extern vmCvar_t			cg_reloadIndicatorY;
extern vmCvar_t			cg_reloadIndicatorWidth;
extern vmCvar_t			cg_reloadIndicatorHeight;
extern vmCvar_t			cg_reloadIndicatorAlpha;
extern vmCvar_t			cg_crosshairNamesY;
extern vmCvar_t			cg_crosshairNamesHealth;
extern vmCvar_t			cg_friendFloatHealth;
extern vmCvar_t			cg_friendFloatHealthSize;
extern vmCvar_t			cg_radar;
extern vmCvar_t			cg_announcer;
extern vmCvar_t			cg_announcerNewAwards;
extern vmCvar_t			cg_soundBufferDelay;
extern vmCvar_t			cg_powerupBlink;
extern vmCvar_t			cg_quadStyle;
extern vmCvar_t			cg_quadAlpha;
extern vmCvar_t			cg_quadHue;
extern vmCvar_t			cg_drawSpawnpoints;
extern vmCvar_t			cg_teamOverlayScale;
extern vmCvar_t			cg_teamOverlayAutoColor;
extern vmCvar_t			cg_drawTeamBackground;

extern vmCvar_t			cg_newFont;
extern vmCvar_t			cg_newConsole;
extern vmCvar_t			cg_chatTime;
extern vmCvar_t			cg_consoleTime;

extern vmCvar_t			cg_helpMotdSeconds;

extern vmCvar_t			cg_fontScale;
extern vmCvar_t			cg_fontShadow;

extern vmCvar_t			cg_consoleSizeX;
extern vmCvar_t			cg_consoleSizeY;
extern vmCvar_t			cg_chatSizeX;
extern vmCvar_t			cg_chatSizeY;
extern vmCvar_t			cg_teamChatSizeX;
extern vmCvar_t			cg_teamChatSizeY;

extern vmCvar_t			cg_consoleLines;
extern vmCvar_t			cg_commonConsoleLines;
extern vmCvar_t			cg_chatLines;
extern vmCvar_t			cg_teamChatLines;

extern vmCvar_t			cg_commonConsole;


extern vmCvar_t			cg_mySound;
extern vmCvar_t			cg_teamSound;
extern vmCvar_t			cg_enemySound;

extern vmCvar_t			cg_myFootsteps;
extern vmCvar_t			cg_teamFootsteps;
extern vmCvar_t			cg_enemyFootsteps;

extern vmCvar_t			cg_brightShells;
extern vmCvar_t			cg_brightShellAlpha;
extern vmCvar_t			cg_brightOutline;


extern vmCvar_t			cg_enemyModel;
extern vmCvar_t			cg_teamModel;

extern vmCvar_t			cg_teamHueBlue;
extern vmCvar_t			cg_teamHueDefault;
extern vmCvar_t			cg_teamHueRed;

extern vmCvar_t			cg_enemyColor;
extern vmCvar_t			cg_teamColor;
extern vmCvar_t			cg_enemyHeadColor;
extern vmCvar_t			cg_teamHeadColor;
extern vmCvar_t			cg_enemyTorsoColor;
extern vmCvar_t			cg_teamTorsoColor;
extern vmCvar_t			cg_enemyLegsColor;
extern vmCvar_t			cg_teamLegsColor;

extern vmCvar_t			cg_teamHeadColorAuto;
extern vmCvar_t			cg_enemyHeadColorAuto;

extern vmCvar_t			cg_teamHeadColorAuto;
extern vmCvar_t			cg_enemyHeadColorAuto;

extern vmCvar_t			cg_enemyCorpseSaturation;
extern vmCvar_t			cg_enemyCorpseValue;
extern vmCvar_t			cg_teamCorpseSaturation;
extern vmCvar_t			cg_teamCorpseValue;

extern vmCvar_t			cg_itemFade;
extern vmCvar_t			cg_itemFadeTime;

extern vmCvar_t			cg_pingLocation;
extern vmCvar_t			cg_pingEnemyStyle;
extern vmCvar_t			cg_pingLocationHud;
extern vmCvar_t			cg_pingLocationHudSize;
extern vmCvar_t			cg_pingLocationTime;
extern vmCvar_t			cg_pingLocationTime2;
extern vmCvar_t			cg_pingLocationSize;
extern vmCvar_t			cg_pingLocationSize2;
extern vmCvar_t			cg_pingLocationBeep;

extern vmCvar_t			cg_bobGun;

extern vmCvar_t			cg_thTokenIndicator;
extern vmCvar_t			cg_thTokenstyle;

extern vmCvar_t			cg_autorecord;

extern vmCvar_t			cg_timerAlpha;
extern vmCvar_t			cg_fpsAlpha;
extern vmCvar_t			cg_speedAlpha;
extern vmCvar_t			cg_timerScale;
extern vmCvar_t			cg_fpsScale;
extern vmCvar_t			cg_speedScale;

//unlagged - smooth clients #2
// this is done server-side now
//extern	vmCvar_t		cg_smoothClients;
//unlagged - smooth clients #2
extern	vmCvar_t		pmove_fixed;
extern	vmCvar_t		pmove_msec;
extern	vmCvar_t		pmove_float;
//extern	vmCvar_t		cg_pmove_fixed;
extern	vmCvar_t		cg_cameraOrbit;
extern	vmCvar_t		cg_cameraOrbitDelay;
extern	vmCvar_t		cg_timescaleFadeEnd;
extern	vmCvar_t		cg_timescaleFadeSpeed;
extern	vmCvar_t		cg_timescale;
extern	vmCvar_t		cg_cameraMode;
extern  vmCvar_t		cg_smallFont;
extern  vmCvar_t		cg_bigFont;
extern	vmCvar_t		cg_taunts;
extern	vmCvar_t		cg_noProjectileTrail;
extern	vmCvar_t		cg_oldRail;
extern	vmCvar_t		cg_oldRocket;
extern	vmCvar_t		cg_oldMachinegun;

extern	vmCvar_t		cg_leiEnhancement;			// LEILEI'S LINE!
extern	vmCvar_t		cg_leiGoreNoise;			// LEILEI'S LINE!
extern	vmCvar_t		cg_leiBrassNoise;			// LEILEI'S LINE!
extern	vmCvar_t		cg_leiSuperGoreyAwesome;	// LEILEI'S LINE!
extern	vmCvar_t		cg_oldPlasma;
extern	vmCvar_t		cg_trueLightning;
extern	vmCvar_t		cg_music;
/*
#ifdef MISSIONPACK
extern	vmCvar_t		cg_redTeamName;
extern	vmCvar_t		cg_blueTeamName;
extern	vmCvar_t		cg_currentSelectedPlayer;
extern	vmCvar_t		cg_currentSelectedPlayerName;
extern	vmCvar_t		cg_singlePlayer;
extern	vmCvar_t		cg_singlePlayerActive;
extern  vmCvar_t		cg_recordSPDemo;
extern  vmCvar_t		cg_recordSPDemoName;
#endif
//Sago: Moved outside
extern	vmCvar_t		cg_obeliskRespawnDelay;
extern	vmCvar_t		cg_enableDust;
extern	vmCvar_t		cg_enableBreath;
*/

//unlagged - client options
extern	vmCvar_t		cg_delag;
//extern	vmCvar_t		cg_debugDelag;
//extern	vmCvar_t		cg_drawBBox;
extern	vmCvar_t		cg_cmdTimeNudge;
extern	vmCvar_t		sv_fps;
extern	vmCvar_t		cg_projectileNudge;
extern	vmCvar_t		cg_projectileNudgeAuto;
extern	vmCvar_t		cg_optimizePrediction;
extern	vmCvar_t		cl_timeNudge;
//extern	vmCvar_t		cg_latentSnaps;
//extern	vmCvar_t		cg_latentCmds;
//extern	vmCvar_t		cg_plOut;
//unlagged - client options
extern	vmCvar_t		com_maxfps;
extern	vmCvar_t		con_notifytime;

//extra CVARS elimination
extern	vmCvar_t		cg_alwaysWeaponBar;
extern	vmCvar_t		cg_hitsound;
extern  vmCvar_t                cg_voip_teamonly;
extern  vmCvar_t                cg_voteflags;
extern  vmCvar_t                cg_cyclegrapple;
extern  vmCvar_t                cg_vote_custom_commands;

extern  vmCvar_t                cg_autovertex;

extern  vmCvar_t                cg_picmipBackup;

//Cvar to adjust the size of the fragmessage
extern	vmCvar_t		cg_fragmsgsize;

extern	vmCvar_t		cg_crosshairPulse;
extern	vmCvar_t		cg_differentCrosshairs;
extern	vmCvar_t		cg_ch1;
extern	vmCvar_t		cg_ch1size;
extern	vmCvar_t		cg_ch2;
extern	vmCvar_t		cg_ch2size;
extern	vmCvar_t		cg_ch3;
extern	vmCvar_t		cg_ch3size;
extern	vmCvar_t		cg_ch4;
extern	vmCvar_t		cg_ch4size;
extern	vmCvar_t		cg_ch5;
extern	vmCvar_t		cg_ch5size;
extern	vmCvar_t		cg_ch6;
extern	vmCvar_t		cg_ch6size;
extern	vmCvar_t		cg_ch7;
extern	vmCvar_t		cg_ch7size;
extern	vmCvar_t		cg_ch8;
extern	vmCvar_t		cg_ch8size;
extern	vmCvar_t		cg_ch9;
extern	vmCvar_t		cg_ch9size;
extern	vmCvar_t		cg_ch10;
extern	vmCvar_t		cg_ch10size;
extern	vmCvar_t		cg_ch11;
extern	vmCvar_t		cg_ch11size;
extern	vmCvar_t		cg_ch12;
extern	vmCvar_t		cg_ch12size;
extern	vmCvar_t		cg_ch13;
extern	vmCvar_t		cg_ch13size;

extern	vmCvar_t                cg_crosshairColorRed;
extern	vmCvar_t                cg_crosshairColorGreen;
extern	vmCvar_t                cg_crosshairColorBlue;

extern vmCvar_t			cg_weaponBarStyle;

extern vmCvar_t                 cg_weaponOrder;
extern vmCvar_t			cg_chatBeep;
extern vmCvar_t			cg_teamChatBeep;

extern	vmCvar_t		cg_lowHealthPercentile;
extern	vmCvar_t		cg_inverseTimer;
extern	vmCvar_t		cg_selfOnTeamOverlay;
extern	vmCvar_t		cg_deathNoticeTime;
extern	vmCvar_t		cg_drawCenterprint;
extern	vmCvar_t		cg_crosshairHitColorStyle;
//extern	vmCvar_t		cg_crosshairColor;
extern	vmCvar_t		cg_crosshairHitColorTime;
extern	vmCvar_t		cg_drawItemPickups;
extern	vmCvar_t		cg_drawAccel;
extern	vmCvar_t		cg_mapoverview;
extern	vmCvar_t		cg_hud;

//unlagged - cg_unlagged.c
void CG_PredictWeaponEffects( centity_t *cent );
int CG_ReliablePing( void );
int CG_ReliablePingFromSnaps(snapshot_t *snap, snapshot_t *nextsnap);
void CG_AddBoundingBox( centity_t *cent );
qboolean CG_Cvar_ClampInt( const char *name, vmCvar_t *vmCvar, int min, int max );

qboolean CG_IsOwnMissile(centity_t *missile);
int CG_MissileOwner(centity_t *missile);
void CG_AddPredictedMissiles(void );
void CG_RemoveExpiredPredictedMissiles(void);
qboolean CG_ShouldPredictExplosion(void);
void CG_PredictedExplosion(trace_t *tr, int weapon, predictedMissile_t *predMissile, centity_t *missileEnt);
qboolean CG_ExplosionPredicted(centity_t *cent, int checkFlags, vec3_t realExpOrigin, int realHitEnt);
void	CG_InitPMissilles( void );
void CG_UpdateMissileStatus(predictedMissileStatus_t *pms, int addedFlags, vec3_t explosionOrigin, int hitEntity);
void CG_RemovePredictedMissile(centity_t *missile);
void CG_RemoveOldMissileExplosion(predictedMissileStatus_t *pms);
void CG_RecoverMissile(centity_t *missile);
//unlagged - cg_unlagged.c

//
// cg_main.c
//
const char *CG_ConfigString( int index );
const char *CG_Argv( int arg );

void QDECL CG_PrintfHelpMotd(const char *msg, ... );
void QDECL CG_PrintfChat( qboolean team, const char *msg, ... );
void QDECL CG_Printf( const char *msg, ... );
void QDECL CG_Error( const char *msg, ... ) __attribute__((noreturn));

void CG_StartMusic( void );

void CG_UpdateCvars( void );

int CG_CrosshairPlayer( void );
int CG_LastAttacker( void );
void CG_LoadMenus(const char *menuFile);
void CG_KeyEvent(int key, qboolean down);
void CG_MouseEvent(int x, int y);
void CG_EventHandling(int type);
void CG_RankRunFrame( void );
void CG_SetScoreSelection(void *menu);
//score_t *CG_GetSelectedScore( void );
void CG_BuildSpectatorString( void );
void CG_ForceModelChange( void );

//unlagged, sagos modfication
void SnapVectorTowards( vec3_t v, vec3_t to );

void CG_FairCvars( void );
void CG_CvarResetDefaults( void );
void CG_Cvar_SetAndUpdate( const char *var_name, const char *value );
void CG_Cvar_ResetToDefault( const char *var_name );
void CG_Cvar_Update( const char *var_name );
void CG_Cvar_PrintUserChanges( qboolean all );
qboolean CG_SupportsOggVorbis(void);
qboolean CG_BrokenEngine(void);
void CG_AutoRecordStart(void);
void CG_AutoRecordStop(void);
qboolean CG_IsTeamGametype(void);

//
// cg_view.c
//
void CG_TestModel_f (void);
void CG_TestGun_f (void);
void CG_TestModelNextFrame_f (void);
void CG_TestModelPrevFrame_f (void);
void CG_TestModelNextSkin_f (void);
void CG_TestModelPrevSkin_f (void);
void CG_ZoomDown_f( void );
void CG_ZoomUp_f( void );
void CG_AddBufferedSound( sfxHandle_t sfx);
int CG_AddBufferedRewardSound( sfxHandle_t sfx );

void CG_DrawActiveFrame( int serverTime, stereoFrame_t stereoView, qboolean demoPlayback );


//
// cg_drawtools.c
//
void CG_AdjustFrom640( float *x, float *y, float *w, float *h );
void CG_FillRect( float x, float y, float width, float height, const float *color );
void CG_DrawPic( float x, float y, float width, float height, qhandle_t hShader );
void CG_DrawString( float x, float y, const char *string, 
				   float charWidth, float charHeight, const float *modulate );


void CG_DrawStringExt( int x, int y, const char *string, const float *setColor, 
		qboolean forceColor, qboolean shadow, int charWidth, int charHeight, int maxChars );
void CG_DrawStringExtFloat( float x, float y, const char *string, const float *setColor, 
		qboolean forceColor, qboolean shadow, float charWidth, float charHeight, int maxChars );
void CG_DrawScoreString( int x, int y, const char *s, float alpha, int maxchars );
void CG_DrawScoreStringColor( int x, int y, const char *s, vec4_t color );
void CG_DrawSmallScoreString( int x, int y, const char *s, float alpha );
void CG_DrawSmallScoreStringColor( int x, int y, const char *s, vec4_t color );
void CG_DrawTinyScoreString( int x, int y, const char *s, float alpha );
void CG_DrawTinyScoreStringColor( int x, int y, const char *s, vec4_t color );
void CG_DrawMediumString( int x, int y, const char *s, float alpha );
void CG_DrawBigString( int x, int y, const char *s, float alpha );
void CG_DrawBigStringAspect( int x, int y, const char *s, float alpha );
void CG_DrawBigStringColor( int x, int y, const char *s, vec4_t color );
void CG_DrawSmallString( int x, int y, const char *s, float alpha );
void CG_DrawSmallStringColor( int x, int y, const char *s, vec4_t color );
void CG_DrawStringFloat( float x, float y,
	       	const char *string, float alpha, float charWidth, float charHeight);

int CG_DrawStrlen( const char *str );

float	*CG_FadeColor( int startMsec, int totalMsec );
float CG_FadeScale( int startMsec, int totalMsec );
float *CG_TeamColor( int team );
void CG_TileClear( void );
void CG_ColorForHealth( vec4_t hcolor );
void CG_ColorForHealth2( vec4_t hcolor );
void CG_GetColorForHealth( int health, int armor, vec4_t hcolor );
void CG_GetColorForHealth2( int health, int armor, vec4_t hcolor );

void UI_DrawProportionalString( int x, int y, const char* str, int style, vec4_t color );
void CG_DrawRect( float x, float y, float width, float height, float size, const float *color );
void CG_DrawSides(float x, float y, float w, float h, float size);
void CG_DrawTopBottom(float x, float y, float w, float h, float size);
void CG_DrawRectAspect( float x, float y, float width, float height, float size, const float *color );
void CG_DrawCorners( float x, float y, float width, float height, float cornerlength, float thickness, const float *color );
float CG_HeightToWidth(float h);
float CG_DrawPicSquareByHeight( float x, float y, float height, qhandle_t hShader );


//
// cg_draw.c, cg_newDraw.c
//
extern	int sortedTeamPlayers[TEAM_MAXOVERLAY];
extern	int	numSortedTeamPlayers;
extern	int drawTeamOverlayModificationCount;
extern  char systemChat[256];
extern  char teamChat1[256];
extern  char teamChat2[256];

void CG_AddLagometerFrameInfo( void );
void CG_AddLagometerSnapshotInfo( snapshot_t *snap );
void CG_CenterPrint( const char *str, int y, int charWidth );
void CG_DrawHead( float x, float y, float w, float h, int clientNum, vec3_t headAngles );
void CG_DrawActive( stereoFrame_t stereoView );
void CG_DrawFlagModel( float x, float y, float w, float h, int team, qboolean force2D );
void CG_DrawTeamBackground( int x, int y, int w, int h, float alpha, int team );
void CG_OwnerDraw(float x, float y, float w, float h, float text_x, float text_y, int ownerDraw, int ownerDrawFlags, int align, float special, float scale, vec4_t color, qhandle_t shader, int textStyle);
void CG_Text_Paint(float x, float y, float scale, vec4_t color, const char *text, float adjust, int limit, int style);
int CG_Text_Width(const char *text, float scale, int limit);
int CG_Text_Height(const char *text, float scale, int limit);
void CG_SelectPrevPlayer( void );
void CG_SelectNextPlayer( void );
float CG_GetValue(int ownerDraw);
qboolean CG_OwnerDrawVisible(int flags);
void CG_RunMenuScript(char **args);
void CG_ShowResponseHead( void );
void CG_SetPrintString(int type, const char *p);
void CG_InitTeamChat( void );
void CG_GetTeamColor(vec4_t *color);
const char *CG_GetGameStatusText( void );
const char *CG_GetKillerText( void );
void CG_Draw3DModel(float x, float y, float w, float h, qhandle_t model, qhandle_t skin, vec3_t origin, vec3_t angles);
void CG_Text_PaintChar(float x, float y, float width, float height, float scale, float s, float t, float s2, float t2, qhandle_t hShader);
void CG_CheckOrderPending( void );
const char *CG_GameTypeString( void );
qboolean CG_YourTeamHasFlag( void );
qboolean CG_OtherTeamHasFlag( void );
qhandle_t CG_StatusHandle(int task);
//void CG_AddToGenericConsole( const char *str, console_t *console );
//void CG_ClearGenericConsole(console_t *console);
void CG_AddToConsole ( const char *str );
void CG_AddToChat ( const char *str );
void CG_AddToTeamChat ( const char *str );
/*
int CG_Reward2Time(int idx);
void CG_ResetStatusbar(void);
void CG_Ratstatusbar4RegisterShaders(void);
void CG_Ratstatusbar3RegisterShaders(void);
*/
#ifdef WITH_MULTITOURNAMENT
int CG_GetScoresMtrn(int scoreNum);
#endif



//
// cg_player.c
//
void CG_Player( centity_t *cent );
void CG_PlayerGetColors(clientInfo_t *ci, qboolean isDead, int bodyPart, byte *outColor);
void CG_ResetPlayerEntity( centity_t *cent );
void CG_AddRefEntityWithPowerups( refEntity_t *ent, entityState_t *state, int team, qboolean isMissile,
	       	clientInfo_t *ci, int orderIndicator, qboolean useBlendBrightshell );
void CG_NewClientInfo( int clientNum );
sfxHandle_t	CG_CustomSound( int clientNum, const char *soundName );
void CG_LoadForcedSounds(void);
int CG_CountPlayers(team_t team);
int CG_GetTotalHitPoints(int health, int armor);
qboolean CG_AllowColoredProjectiles(void);
void CG_ProjectileColor(team_t team, byte *outColor);
void CG_PlayerAutoHeadColor(clientInfo_t *ci, byte *outColor);
void CG_FloatColorToRGBA(float *color, byte *out);
void CG_ParseForcedColors( void );
byte CG_GetBrightShellAlpha(void);
byte CG_GetBrightOutlineAlpha(void);
qboolean CG_THPlayerVisible(centity_t *cent);
void CG_PlayerColorFromString(char *str, float *h, float *s, float *v);
qboolean CG_IsFrozenPlayer( centity_t *cent );
qboolean CG_IsFrozenPlayerState( entityState_t *state );

//
// cg_predict.c
//
void CG_BuildSolidList( void );
int	CG_PointContents( const vec3_t point, int passEntityNum );
void CG_Trace( trace_t *result, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, 
					 int skipNumber, int mask );
void CG_PredictPlayerState( void );
void CG_LoadDeferredPlayers( void );
qboolean CG_MissileTouchedPortal(const vec3_t start, const vec3_t end);
void CG_EncodePlayerBBox( pmove_t *pm, entityState_t *ent);


//
// cg_events.c
//
void CG_CheckEvents( centity_t *cent );
const char	*CG_PlaceString( int rank );
void CG_EntityEvent( centity_t *cent, vec3_t position );
void CG_PainEvent( centity_t *cent, int health );


//
// cg_ents.c
//
void CG_SetEntitySoundPosition( centity_t *cent );
void CG_AddPacketEntities( void );
void CG_Beam( centity_t *cent );
void CG_AdjustPositionForMover( const vec3_t in, int moverNum, int fromTime, int toTime, vec3_t out );

void CG_PositionEntityOnTag( refEntity_t *entity, const refEntity_t *parent, 
							qhandle_t parentModel, char *tagName );
void CG_PositionRotatedEntityOnTag( refEntity_t *entity, const refEntity_t *parent, 
							qhandle_t parentModel, char *tagName );
int CG_ProjectileNudgeTimeshift(centity_t *cent);



//
// cg_weapons.c
//
void CG_NextWeapon_f( void );
void CG_PrevWeapon_f( void );
void CG_Weapon_f( void );

sfxHandle_t CG_RegisterRailFireSound(void);
void CG_RegisterWeapon( int weaponNum );
void CG_RegisterItemVisuals( int itemNum );

void CG_FireWeapon( centity_t *cent );
void CG_MissileHitWall( int weapon, int clientNum, vec3_t origin, vec3_t dir, impactSound_t soundType, predictedMissileStatus_t *missileStatus );
void CG_MissileHitPlayer( int weapon, vec3_t origin, vec3_t dir, int entityNum, predictedMissileStatus_t *missileStatus );
void CG_ShotgunFire( entityState_t *es );
void CG_Bullet( vec3_t origin, int sourceEntityNum, vec3_t normal, qboolean flesh, int fleshEntityNum );

void CG_RailTrail( clientInfo_t *ci, vec3_t start, vec3_t end );
void CG_GrappleTrail( centity_t *ent, const weaponInfo_t *wi );
void CG_AddViewWeapon (playerState_t *ps);
void CG_AddPlayerWeapon( refEntity_t *parent, playerState_t *ps, centity_t *cent, int team );
void CG_DrawWeaponSelect( void );

void CG_DrawWeaponBar0(int count, int bits);
void CG_DrawWeaponBar1(int count, int bits);
void CG_DrawWeaponBar2(int count, int bits, float *color);
void CG_DrawWeaponBar3(int count, int bits, float *color);
void CG_DrawWeaponBar4(int count, int bits, float *color);
void CG_DrawWeaponBar5(int count, int bits, float *color);
void CG_DrawWeaponBar6(int count, int bits, float *color);
void CG_DrawWeaponBar7(int count, int bits, float *color);
void CG_DrawWeaponBar8(int count, int bits, float *color);
void CG_DrawWeaponBar9(int count, int bits, float *color);
void CG_DrawWeaponBar10(int count, int bits, float *color);
void CG_DrawWeaponBar11(int count, int bits);
void CG_DrawWeaponBar12(int count, int bits, float *color);
void CG_DrawWeaponBar13(int count, int bits, float *color);
void CG_DrawWeaponBar14(int count, int bits, float *color);
void CG_DrawWeaponBar15(int count, int bits, float *color);
int CG_GetWeaponSelect( void );

void CG_OutOfAmmoChange( void );	// should this be in pmove?
int CG_FullAmmo(int weapon);

//
// cg_marks.c
//
void	CG_InitMarkPolys( void );
void	CG_AddMarks( void );
void	CG_ImpactMark( qhandle_t markShader, 
				    const vec3_t origin, const vec3_t dir, 
					float orientation, 
				    float r, float g, float b, float a, 
					qboolean alphaFade, 
					float radius, qboolean temporary );
void    CG_LeiSparks (vec3_t org, vec3_t vel, int duration, float x, float y, float speed);
void    CG_LeiSparks2 (vec3_t org, vec3_t vel, int duration, float x, float y, float speed);
void    CG_LeiPuff (vec3_t org, vec3_t vel, int duration, float x, float y, float speed, float size);


//
// cg_localents.c
//
void	CG_InitLocalEntities( void );
localEntity_t	*CG_AllocLocalEntity( void );
void	CG_AddLocalEntities( void );
void CG_FreeLocalEntityById(int id);

//
// cg_effects.c
//
localEntity_t *CG_SmokePuff( const vec3_t p, 
				   const vec3_t vel, 
				   float radius,
				   float r, float g, float b, float a,
				   float duration,
				   int startTime,
				   int fadeInTime,
				   int leFlags,
				   qhandle_t hShader );
void CG_BubbleTrail( vec3_t start, vec3_t end, float spacing );
void CG_SpawnEffect( vec3_t org );
/*
//#ifdef MISSIONPACK
void CG_KamikazeEffect( vec3_t org );
void CG_ObeliskExplode( vec3_t org, int entityNum );
void CG_ObeliskPain( vec3_t org );
void CG_InvulnerabilityImpact( vec3_t org, vec3_t angles );
void CG_InvulnerabilityJuiced( vec3_t org );
void CG_LightningBoltBeam( vec3_t start, vec3_t end );
//#endif
*/
void CG_ScorePlum( int client, vec3_t org, int score );
void CG_DamagePlum( int client, vec3_t org, int damage );

void CG_GibPlayer( vec3_t playerOrigin );
void CG_BigExplode( vec3_t playerOrigin );

void CG_Bleed( vec3_t origin, int entityNum );

localEntity_t *CG_MakeExplosion( vec3_t origin, vec3_t dir,
								qhandle_t hModel, qhandle_t shader, int msec,
								qboolean isSprite );

void CG_SpurtBlood( vec3_t origin, vec3_t velocity, int hard );
void CG_PingLocation( centity_t *cent );
void CG_PingHudMarker ( vec3_t pingOrigin, float alpha, qhandle_t shader );
void CG_HudBorderMarker ( vec3_t origin, float alpha, float radius, qhandle_t shader, int baseAngle );

//
// cg_snapshot.c
//
void CG_ProcessSnapshots( void );
//unlagged - early transitioning
void CG_TransitionEntity( centity_t *cent );
//unlagged - early transitioning

//
// cg_info.c
//
void CG_LoadingString( const char *s );
void CG_LoadingItem( int itemNum );
void CG_LoadingClient( int clientNum );
void CG_DrawInformation( void );

//
// cg_scoreboard.c
//
qboolean CG_DrawOldScoreboard( void );
qboolean CG_DrawRatScoreboard( void );
void CG_DrawOldTourneyScoreboard( void );

//
// cg_challenges.c
//
void challenges_init(void);
void challenges_save(void);
unsigned int getChallenge(int challenge);
void addChallenge(int challenge);

//
// cg_consolecmds.c
//
qboolean CG_ConsoleCommand( void );
void CG_InitConsoleCommands( void );
void CG_Randomcolors_f( void );

//
// cg_servercmds.c
//
void CG_ExecuteNewServerCommands( int latestSequence );
void CG_ParseServerinfo( void );
void CG_SetConfigValues( void );
void CG_LoadVoiceChats( void );
void CG_LoadTaunts( void );
void CG_PlayTaunt(int clientNum, const char *id);
void CG_PrintTaunts( void );
void CG_ShaderStateChanged(void);
void CG_VoiceChatLocal( int mode, qboolean voiceOnly, int clientNum, int color, const char *cmd );
void CG_PlayBufferedVoiceChats( void );
#ifdef WITH_MULTITOURNAMENT
long CG_GetMtrnGameFlags(int gameId);
#endif

//
// cg_playerstate.c
//
void CG_Respawn( void );
void CG_TransitionPlayerState( playerState_t *ps, playerState_t *ops );
void CG_CheckChangedPredictableEvents( playerState_t *ps );
void CG_PushReward(sfxHandle_t sfx, qhandle_t shader, int rewardCount);

//
// cg_superHud.c
//
void CG_HudEdit_f( void );
void CG_ClearHud( void );
void CG_LoadHudFile( const char* hudFile );
void CG_WriteHudFile_f( void );



//===============================================

//
// system traps
// These functions are how the cgame communicates with the main game system
//

// print message on the local console
void		trap_Print( const char *fmt );

// abort the game
void		trap_Error( const char *fmt )  __attribute__((noreturn));

// milliseconds should only be used for performance tuning, never
// for anything game related.  Get time from the CG_DrawActiveFrame parameter
int			trap_Milliseconds( void );

// console variable interaction
void		trap_Cvar_Register( vmCvar_t *vmCvar, const char *varName, const char *defaultValue, int flags );
void		trap_Cvar_Update( vmCvar_t *vmCvar );
void		trap_Cvar_Set( const char *var_name, const char *value );
void		trap_Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize );

// ServerCommand and ConsoleCommand parameter access
int			trap_Argc( void );
void		trap_Argv( int n, char *buffer, int bufferLength );
void		trap_Args( char *buffer, int bufferLength );

// filesystem access
// returns length of file
int			trap_FS_FOpenFile( const char *qpath, fileHandle_t *f, fsMode_t mode );
void		trap_FS_Read( void *buffer, int len, fileHandle_t f );
void		trap_FS_Write( const void *buffer, int len, fileHandle_t f );
void		trap_FS_FCloseFile( fileHandle_t f );
int			trap_FS_Seek( fileHandle_t f, long offset, int origin ); // fsOrigin_t

// add commands to the local console as if they were typed in
// for map changing, etc.  The command is not executed immediately,
// but will be executed in order the next time console commands
// are processed
void		trap_SendConsoleCommand( const char *text );

// register a command name so the console can perform command completion.
// FIXME: replace this with a normal console command "defineCommand"?
void		trap_AddCommand( const char *cmdName );

// send a string to the server over the network
void		trap_SendClientCommand( const char *s );

// force a screen update, only used during gamestate load
void		trap_UpdateScreen( void );

// model collision
void		trap_CM_LoadMap( const char *mapname );
int			trap_CM_NumInlineModels( void );
clipHandle_t trap_CM_InlineModel( int index );		// 0 = world, 1+ = bmodels
clipHandle_t trap_CM_TempBoxModel( const vec3_t mins, const vec3_t maxs );
int			trap_CM_PointContents( const vec3_t p, clipHandle_t model );
int			trap_CM_TransformedPointContents( const vec3_t p, clipHandle_t model, const vec3_t origin, const vec3_t angles );
void		trap_CM_BoxTrace( trace_t *results, const vec3_t start, const vec3_t end,
					  const vec3_t mins, const vec3_t maxs,
					  clipHandle_t model, int brushmask );
void		trap_CM_TransformedBoxTrace( trace_t *results, const vec3_t start, const vec3_t end,
					  const vec3_t mins, const vec3_t maxs,
					  clipHandle_t model, int brushmask,
					  const vec3_t origin, const vec3_t angles );

// Returns the projection of a polygon onto the solid brushes in the world
int			trap_CM_MarkFragments( int numPoints, const vec3_t *points, 
			const vec3_t projection,
			int maxPoints, vec3_t pointBuffer,
			int maxFragments, markFragment_t *fragmentBuffer );

// normal sounds will have their volume dynamically changed as their entity
// moves and the listener moves
void		trap_S_StartSound( vec3_t origin, int entityNum, int entchannel, sfxHandle_t sfx );
void		trap_S_StopLoopingSound(int entnum);

// a local sound is always played full volume
void		trap_S_StartLocalSound( sfxHandle_t sfx, int channelNum );
void		trap_S_ClearLoopingSounds( qboolean killall );
void		trap_S_AddLoopingSound( int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx );
void		trap_S_AddRealLoopingSound( int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx );
void		trap_S_UpdateEntityPosition( int entityNum, const vec3_t origin );

// respatialize recalculates the volumes of sound as they should be heard by the
// given entityNum and position
void		trap_S_Respatialize( int entityNum, const vec3_t origin, vec3_t axis[3], int inwater );
sfxHandle_t	trap_S_RegisterSound( const char *sample, qboolean compressed );		// returns buzz if not found
void		trap_S_StartBackgroundTrack( const char *intro, const char *loop );	// empty name stops music
void	trap_S_StopBackgroundTrack( void );


void		trap_R_LoadWorldMap( const char *mapname );

// all media should be registered during level startup to prevent
// hitches during gameplay
qhandle_t	trap_R_RegisterModel( const char *name );			// returns rgb axis if not found
qhandle_t	trap_R_RegisterSkin( const char *name );			// returns all white if not found
qhandle_t	trap_R_RegisterShader( const char *name );			// returns all white if not found
qhandle_t	trap_R_RegisterShaderNoMip( const char *name );			// returns all white if not found

// a scene is built up by calls to R_ClearScene and the various R_Add functions.
// Nothing is drawn until R_RenderScene is called.
void		trap_R_ClearScene( void );
void		trap_R_AddRefEntityToScene( const refEntity_t *re );

// polys are intended for simple wall marks, not really for doing
// significant construction
void		trap_R_AddPolyToScene( qhandle_t hShader , int numVerts, const polyVert_t *verts );
void		trap_R_AddPolysToScene( qhandle_t hShader , int numVerts, const polyVert_t *verts, int numPolys );
void		trap_R_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b );
int			trap_R_LightForPoint( vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir );
void		trap_R_RenderScene( const refdef_t *fd );
void		trap_R_SetColor( const float *rgba );	// NULL = 1,1,1,1
void		trap_R_DrawStretchPic( float x, float y, float w, float h, 
			float s1, float t1, float s2, float t2, qhandle_t hShader );
void		trap_R_ModelBounds( clipHandle_t model, vec3_t mins, vec3_t maxs );
int			trap_R_LerpTag( orientation_t *tag, clipHandle_t mod, int startFrame, int endFrame, 
					   float frac, const char *tagName );
void		trap_R_RemapShader( const char *oldShader, const char *newShader, const char *timeOffset );

// The glconfig_t will not change during the life of a cgame.
// If it needs to change, the entire cgame will be restarted, because
// all the qhandle_t are then invalid.
void		trap_GetGlconfig( glconfig_t *glconfig );

// the gamestate should be grabbed at startup, and whenever a
// configstring changes
void		trap_GetGameState( gameState_t *gamestate );

// cgame will poll each frame to see if a newer snapshot has arrived
// that it is interested in.  The time is returned seperately so that
// snapshot latency can be calculated.
void		trap_GetCurrentSnapshotNumber( int *snapshotNumber, int *serverTime );

// a snapshot get can fail if the snapshot (or the entties it holds) is so
// old that it has fallen out of the client system queue
qboolean	trap_GetSnapshot( int snapshotNumber, snapshot_t *snapshot );

// retrieve a text command from the server stream
// the current snapshot will hold the number of the most recent command
// qfalse can be returned if the client system handled the command
// argc() / argv() can be used to examine the parameters of the command
qboolean	trap_GetServerCommand( int serverCommandNumber );

// returns the most recent command number that can be passed to GetUserCmd
// this will always be at least one higher than the number in the current
// snapshot, and it may be quite a few higher if it is a fast computer on
// a lagged connection
int			trap_GetCurrentCmdNumber( void );	

qboolean	trap_GetUserCmd( int cmdNumber, usercmd_t *ucmd );

// used for the weapon select and zoom
void		trap_SetUserCmdValue( int stateValue, float sensitivityScale );

// aids for VM testing
void		testPrintInt( char *string, int i );
void		testPrintFloat( char *string, float f );

int			trap_MemoryRemaining( void );
void		trap_R_RegisterFont(const char *fontName, int pointSize, fontInfo_t *font);
qboolean	trap_Key_IsDown( int keynum );
int			trap_Key_GetCatcher( void );
void		trap_Key_SetCatcher( int catcher );
int			trap_Key_GetKey( const char *binding );

int			trap_RealTime( qtime_t *qtime );


typedef enum {
  SYSTEM_PRINT,
  CHAT_PRINT,
  TEAMCHAT_PRINT
} q3print_t; // bk001201 - warning: useless keyword or type name in empty declaration


int trap_CIN_PlayCinematic( const char *arg0, int xpos, int ypos, int width, int height, int bits);
e_status trap_CIN_StopCinematic(int handle);
e_status trap_CIN_RunCinematic (int handle);
void trap_CIN_DrawCinematic (int handle);
void trap_CIN_SetExtents (int handle, int x, int y, int w, int h);

void trap_SnapVector( float *v );

qboolean	trap_loadCamera(const char *name);
void		trap_startCamera(int time);
qboolean	trap_getCameraInfo(int time, vec3_t *origin, vec3_t *angles);

qboolean	trap_GetEntityToken( char *buffer, int bufferSize );

void	CG_ClearParticles (void);
void	CG_AddParticles (void);
void	CG_ParticleSnow (qhandle_t pshader, vec3_t origin, vec3_t origin2, int turb, float range, int snum);
void	CG_ParticleSmoke (qhandle_t pshader, centity_t *cent);
void	CG_AddParticleShrapnel (localEntity_t *le);
void	CG_ParticleSnowFlurry (qhandle_t pshader, centity_t *cent);
void	CG_ParticleBulletDebris (vec3_t	org, vec3_t vel, int duration);
void	CG_ParticleSparks (vec3_t org, vec3_t vel, int duration, float x, float y, float speed);
void	CG_ParticleDust (centity_t *cent, vec3_t origin, vec3_t dir);
void	CG_ParticleMisc (qhandle_t pshader, vec3_t origin, int size, int duration, float alpha);
void	CG_ParticleExplosion (char *animStr, vec3_t origin, vec3_t vel, int duration, int sizeStart, int sizeEnd);
extern qboolean		initparticles;
int CG_NewParticleArea ( int num );


// LEILEI ENHANCEMENT

