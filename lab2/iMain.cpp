//==============================================================================
//  Shadow Chase: The Final Escape
//  2D survival / endless runner  -  C++ + iGraphics
//
//  Course : CSE-1200 (Software Development I)
//  Dept. of CSE, Ahsanullah University of Science and Technology
//
//  Build  : Visual Studio 2013, Win32, Platform Toolset v120_xp
//
//  ---------------------------------------------------------------------------
//  CONTENTS
//    1. Tunable game values          6. Level 01 - update (game rules)
//    2. Game states                  7. Level 01 - drawing
//    3. Assets (pictures + sounds)   8. Win / Lose screens
//    4. Level 01 - data              9. iGraphics callbacks
//    5. Level 01 - setup            10. main()
//==============================================================================

#define _CRT_SECURE_NO_WARNINGS

#include "iGraphics.h"
#include "spriteLoader.h"   // must come after iGraphics.h (it uses stb_image)

//------------------------------------------------------------------------------
// Screen
//------------------------------------------------------------------------------
#define SCREEN_W 1280
#define SCREEN_H 720

//==============================================================================
//  1. TUNABLE GAME VALUES
//     Every rule number lives here so the game can be balanced from one place
//     instead of hunting for magic numbers spread through the code.
//==============================================================================

const float MAX_HP               = 200.0f;  // shown in the HUD, no longer damaged
const float SPEED_BOOST_DURATION =   8.0f;  // seconds a Speed Box lasts

// ---------------------------------------------------------------------------
//  Level 01 is an endless run. Nothing is placed by hand any more: the object
//  arrays below are pools, and each entry is re-placed further up the road once
//  it has been used or has scrolled away. These are the random world-pixel gaps
//  left between one object and the next of its kind.
//
//  At the normal 185 px/s a bomb gap of 800..1400 works out at roughly 4.3 to
//  7.6 seconds apart, which is comfortably enough time to read the bomb's
//  height and jump or slide.
// ---------------------------------------------------------------------------
const float BOMB_GAP_MIN  =  800.0f;
const float BOMB_GAP_MAX  = 1400.0f;
const float LIFE_GAP_MIN  = 1400.0f;
const float LIFE_GAP_MAX  = 2400.0f;
const float SPEED_GAP_MIN = 3000.0f;
const float SPEED_GAP_MAX = 5000.0f;
const float FOOD_STEP     =   88.0f;   // spacing inside one trail of food
const float FOOD_GAP_MIN  =  800.0f;   // gap between two trails
const float FOOD_GAP_MAX  = 1400.0f;
#define FOOD_TRAIL_LEN 5               // food boxes per trail

// A pickup is nudged forward if it would land on top of a bomb, so a Life Box
// can never be used as bait to walk the player into an explosion.
const float PICKUP_CLEARANCE = 150.0f;

// There is no level clock. The run ends only by reaching the finish line
// (win) or by health reaching zero (lose).

// How many of each object exist in Level 01. These are hard limits: the
// arrays below are exactly this long, so no extra object can ever appear.
#define LIFE_BOX_COUNT   5
#define SPEED_BOX_COUNT  4
#define BOMB_BOX_COUNT  10
#define FOOD_BOX_COUNT  30

// Backgrounds. Each background covers one screen width of the level, so the
// whole level is BG_COUNT screens long.
#define BG_COUNT        10
#define SEG_W           SCREEN_W
#define LEVEL_DISTANCE  (BG_COUNT * SEG_W)          // 12800 pixels of world

// The finish line stands a little before the very end of the last background.
#define FINISH_WORLD_X  (LEVEL_DISTANCE - 300)

// Running speed of the world, in pixels per second.
// At BASE_SCROLL a clean run takes about 69 seconds. Nothing enforces that
// any more - the run lasts as long as the player survives.
const float BASE_SCROLL     = 185.0f;
const float BOOST_MULT      = 1.80f;   // while a Speed Box is active
const float FAST_KEY_MULT   = 1.30f;   // while RIGHT / D is held
const float SLOW_KEY_MULT   = 0.70f;   // while LEFT  / A is held

// Jumping.
// JUMP_V is set so the jump clears a low bomb with room to spare. At 860 the
// jump peaks about 195 pixels up and the player stays above bomb height for
// roughly 0.7 seconds, which is a comfortable window to aim for. A weaker jump
// cleared the bomb by only a few pixels and demanded frame perfect timing.
const float GRAVITY  = -1900.0f;       // pixels per second, per second
const float JUMP_V   =   860.0f;       // upward launch speed

// Bombs
const float BOMB_APPROACH_SPEED = 130.0f;  // extra leftward speed, on top of scroll
const float BOMB_HOME_RATE      =   1.2f;  // how hard a bomb steers toward the player

// A bomb stops steering once it is this close and simply flies straight on.
// Contact happens at about 300 pixels, so locking at 620 gives the player
// roughly one full second of committed flight to read the bomb and react.
// Without this gap the bomb would follow the player right into contact and no
// jump could ever shake it off.
const float BOMB_LOCK_DISTANCE  = 620.0f;

// A bomb may only drift this far from the height it started at. The drift is
// what makes it look alive and hunting, but the cap is what keeps a HIGH bomb
// high and a LOW bomb low - otherwise every bomb would end up at the player's
// exact height and neither jumping nor sliding would ever save them.
const float BOMB_DRIFT_LIMIT    =  20.0f;

// The bomb picture is drawn at BOMB_SIZE, but its damage area is deliberately
// smaller than the picture. The artwork is an explosion with long spikes, and
// charging the player for clipping a spike feels unfair.
const float BOMB_HIT_SCALE      =   0.66f;

// The two heights a bomb can be launched at, measured from the ground.
// LOW must be jumped over, HIGH must be slid under.
const float BOMB_LOW_Y          =  10.0f;
const float BOMB_HIGH_Y         = 110.0f;

// Scoring
#define SCORE_PER_FOOD   10
#define SCORE_PER_LIFE   25
#define SCORE_PER_SPEED  25

//------------------------------------------------------------------------------
// Player geometry
//------------------------------------------------------------------------------
#define PLAYER_X       170     // the player stays at this screen x (auto runner)
#define PLAYER_W       120
#define PLAYER_H       134
#define GROUND_Y        78     // y of the player's feet

#define PICKUP_SIZE     58     // on screen size of a Life / Speed / Food box
#define BOMB_SIZE       76     // on screen size of a Bomb Box

//==============================================================================
//  2. GAME STATES
//==============================================================================

enum GameState
{
	MAIN_MENU,
	KEYS,
	ABOUT_GAME,
	LEVEL_SELECT,      // the campaign screen NEW GAME now opens
	LEVEL_01,
	LEVEL_01_WIN,
	LEVEL_01_LOSE,
	LEVEL_02,
	LEVEL_02_WIN,
	LEVEL_02_LOSE,
	LEVEL_03,
	LEVEL_03_WIN,
	LEVEL_03_LOSE
};

GameState gameState = MAIN_MENU;

//==============================================================================
//  3. ASSETS
//==============================================================================

#define RUN_FRAMES 8

Sprite sprPoster;
Sprite sprBg[BG_COUNT];
Sprite sprRun[RUN_FRAMES];
Sprite sprLifeBox;
Sprite sprSpeedBox;
Sprite sprFoodBox;
Sprite sprBombBox;

int runFrame    = 0;    // which run_N.png is showing
int animTimerId = -1;   // handle returned by iSetTimer

// Sound aliases used with mciSendString.
// introsong  - introSound.mp3                        (menu)
// lvl1bgm    - level_01_bgsound.mp3                  (Level 01 music)
// pickupsnd  - lifebox_speedbox_collision_sound.mp3  (Life + Speed box)
// bombsnd    - bomb collision sound.mp3              (Bomb hit)
// winsnd     - winsound.mp3
// losesnd    - Loosesound.mp3
int introMusicPlaying = 0;

//==============================================================================
//  4. LEVEL 01 DATA
//==============================================================================

// One collectable box (Life, Speed or Food).
typedef struct
{
	float worldX;   // position along the level, in world pixels
	float y;        // height above the bottom of the screen
	int   active;   // 0 once collected, so it can never be collected twice
} Item;

// One Bomb Box. Bombs move on their own, so they get a separate type.
typedef struct
{
	float worldX;
	float y;
	float baseY;    // the height it was launched at, used to cap the drift
	int   active;   // 0 once it has hit the player or left the screen
	int   hasHit;   // makes sure one bomb can only damage the player once
} Bomb;

Item lifeBox [LIFE_BOX_COUNT];
Item speedBox[SPEED_BOX_COUNT];
Item foodBox [FOOD_BOX_COUNT];
Bomb bombBox [BOMB_BOX_COUNT];

// --- running state of the level ---
float scrollX;            // how far the world has scrolled, in pixels
float hp;                 // shown in the HUD; explosions no longer touch it

// How many Life Boxes are in hand. This is the whole survival system now: one
// Life Box absorbs exactly one explosion, and being hit with none left ends the
// run. Nothing else reads or writes it.
int   lifeBoxCount;

// Where the next object of each kind will be dropped on the road. Each pool
// entry, once used, is re-placed at its cursor and the cursor moves on.
float bombSpawnX;
float lifeSpawnX;
float speedSpawnX;
float foodSpawnX;
int   foodInTrail;        // position within the trail currently being laid

int   score;
int   foodCollected;

float playerY;            // feet height (GROUND_Y when standing)
float playerVY;           // vertical speed, used by the jump
int   isJumping;
int   isSliding;

int   speedBoostActive;   // 1 while a Speed Box effect is running
float speedBoostTimer;    // seconds left on that effect

float damageFlash;        // short red flash after a bomb hit (seconds)

DWORD prevTick;           // used to measure real elapsed time between updates

//==============================================================================
//  MENU DATA (unchanged from the previous step)
//==============================================================================

#define BTN_NEW_GAME  0
#define BTN_KEYS      1
#define BTN_ABOUT     2
#define BTN_EXIT      3
#define BTN_COUNT     4

#define BTN_X       996
#define BTN_W       252
#define BTN_H        58
#define BTN_GAP      18
#define BTN_TOP_Y   392

#define BACK_X       52
#define BACK_Y       44
#define BACK_W      168
#define BACK_H       48

char *btnLabel[BTN_COUNT] = {
	"NEW GAME",
	"KEYS",
	"ABOUT THE GAME",
	"EXIT"
};

int hoveredButton = -1;
int backHovered   = 0;

//==============================================================================
//  LEVEL SELECTION DATA
//==============================================================================

#define LEVEL_COUNT 3

// The ONLY place a level's unlock state is written down. To open Level 03
// later, flip its entry here and nothing else in the file needs touching.
//                                    L01  L02  L03
int levelUnlocked[LEVEL_COUNT] = {      1,   1,   1 };

char *levelTitle[LEVEL_COUNT] = {
	"LEVEL 01",
	"LEVEL 02",
	"LEVEL 03"
};

// The line printed under the title on each card.
char *levelSubtitle[LEVEL_COUNT] = {
	"The Final Escape",
	"Daylight Forest Run",
	"Eagle Chase Run"
};

// Three cards in a row, centred across the screen.
#define CARD_W       280
#define CARD_H       300
#define CARD_GAP      44
#define CARD_Y       200
#define CARD_ROW_W   (LEVEL_COUNT * CARD_W + (LEVEL_COUNT - 1) * CARD_GAP)
#define CARD_X0      ((SCREEN_W - CARD_ROW_W) / 2)

int hoveredCard = -1;   // index of the card under the cursor, -1 = none

// A notice drawn on top of the level select screen. While one is showing the
// cards stop responding, so a click can only dismiss the notice.
#define MSG_NONE         0
#define MSG_COMING_SOON  1
#define MSG_LOCKED       2

int levelMessage = MSG_NONE;

// Left edge of card number i.
int cardX(int index)
{
	return CARD_X0 + index * (CARD_W + CARD_GAP);
}

//==============================================================================
//  SMALL HELPERS
//==============================================================================

// Width of a string in pixels for one of the GLUT bitmap fonts.
// (glutBitmapLength is compiled out of this glut.h, so we add it up ourselves.)
int textWidth(char *str, void *font)
{
	int i, w = 0;
	for (i = 0; str[i]; i++)
		w += glutBitmapWidth(font, str[i]);
	return w;
}

void iTextCentered(double centerX, double y, char *str, void *font)
{
	iText(centerX - textWidth(str, font) / 2.0, y, str, font);
}

// iGraphics only gives us opaque colours, so translucent panels are drawn by hand.
void iFilledRectangleAlpha(double x, double y, double w, double h,
                           double r, double g, double b, double a)
{
	glColor4d(r / 255.0, g / 255.0, b / 255.0, a);
	glBegin(GL_QUADS);
		glVertex2d(x,     y);
		glVertex2d(x + w, y);
		glVertex2d(x + w, y + h);
		glVertex2d(x,     y + h);
	glEnd();
	glColor4d(1.0, 1.0, 1.0, 1.0);      // restore full alpha for later draws
}

void dimScreen(double amount)
{
	iFilledRectangleAlpha(0, 0, SCREEN_W, SCREEN_H, 0, 0, 0, amount);
}

int pointInBox(int px, int py, int x, int y, int w, int h)
{
	return (px >= x && px <= x + w && py >= y && py <= y + h);
}

int buttonY(int index)
{
	return BTN_TOP_Y - index * (BTN_H + BTN_GAP);
}

//------------------------------------------------------------------------------
// Bounding box overlap test (AABB).
// Two rectangles touch when they overlap on the x axis AND on the y axis.
// This is the single collision routine used for every object in Level 01.
//------------------------------------------------------------------------------
int boxesOverlap(double ax, double ay, double aw, double ah,
                 double bx, double by, double bw, double bh)
{
	if (ax + aw < bx) return 0;    // A entirely left of B
	if (bx + bw < ax) return 0;    // B entirely left of A
	if (ay + ah < by) return 0;    // A entirely below B
	if (by + bh < ay) return 0;    // B entirely below A
	return 1;
}

//==============================================================================
//  AUDIO  (MCI - winmm is auto-linked by glut.h)
//
//  Every sound is opened once at start up and given a short alias. Playing a
//  sound afterwards is just "play <alias>", which is cheap enough to call from
//  a collision without any stutter.
//==============================================================================

// Returns 0 on success. MCI failing silently is painful to debug, so say so.
// Defined in the progress-file section further down. Declared here so every
// level's end handler can record its result, whichever order they appear in.
void recordLevelResult(int levelNumber, int completed, int levelScore);
void loadProgress();

int audioOpen(char *file, char *alias)
{
	char cmd[512];
	int  err;

	sprintf(cmd, "open \"%s\" alias %s", file, alias);
	err = mciSendString(cmd, NULL, 0, NULL);

	if (err != 0)
		printf("AUDIO: could not open %s (MCI error %d)\n", file, err);
	else
		printf("AUDIO: opened %s as '%s'\n", file, alias);

	fflush(stdout);
	return err;
}

// Plays a sound once, always from the beginning.
void audioPlayOnce(char *alias)
{
	char cmd[128];
	sprintf(cmd, "play %s from 0", alias);
	mciSendString(cmd, NULL, 0, NULL);
}

// Plays a sound on endless repeat.
void audioPlayLoop(char *alias)
{
	char cmd[128];
	sprintf(cmd, "play %s from 0 repeat", alias);
	mciSendString(cmd, NULL, 0, NULL);
}

void audioStop(char *alias)
{
	char cmd[128];
	sprintf(cmd, "stop %s", alias);
	mciSendString(cmd, NULL, 0, NULL);
}

//==============================================================================
//  ASSET LOADING
//==============================================================================

// The exe can be launched from the solution folder (VS debugger) or from
// Debug\ (double click), and the media folders sit at different depths.
// Walk up until Assets\introPoster.png is visible, then lock the cwd there.
void setupWorkingDirectory()
{
	char *candidates[] = { ".", "..", "..\\..", "..\\..\\.." };
	int i;

	for (i = 0; i < 4; i++)
	{
		char probe[512];
		FILE *fp;

		sprintf(probe, "%s\\Assets\\introPoster.png", candidates[i]);
		fp = fopen(probe, "rb");
		if (fp)
		{
			fclose(fp);
			SetCurrentDirectoryA(candidates[i]);
			return;
		}
	}

	printf("WARNING: could not locate the Assets folder.\n");
	fflush(stdout);
}

// Must run AFTER iInitialize() - textures need a live OpenGL context.
void loadAssets()
{
	char path[256];
	int i;

	sprPoster = loadSpritePlain("Assets\\introPoster.png");

	// Backgrounds are loaded in their numbered order, bg1 first and bg10 last.
	// sprBg[0] is bg1, sprBg[9] is bg10. That order is the level's route and
	// it is never shuffled or randomised anywhere in this file.
	for (i = 0; i < BG_COUNT; i++)
	{
		sprintf(path, "Backround\\bg%d.png", i + 1);
		sprBg[i] = loadSpritePlain(path);
	}

	// The character frames already have real transparency, so they load plainly.
	for (i = 0; i < RUN_FRAMES; i++)
	{
		sprintf(path, "Assets\\run_%d.png", i + 1);
		sprRun[i] = loadSpritePlain(path);
	}

	// The pickup boxes are JPEGs on white sheets - key the white out and trim.
	sprLifeBox  = loadSpriteKeyed("Assets\\LifeBox.jpg");
	sprSpeedBox = loadSpriteKeyed("Assets\\speedbox.jpg");
	sprFoodBox  = loadSpriteKeyed("Assets\\foodbox.jpg");
	sprBombBox  = loadSpriteKeyed("Assets\\explosionbox.jpg");
}

void loadAudio()
{
	audioOpen("introSound.mp3",                        "introsong");
	audioOpen("level_01_bgsound.mp3",                  "lvl1bgm");
	audioOpen("lifebox_speedbox_collision_sound.mp3",  "pickupsnd");
	audioOpen("bomb collision sound.mp3",              "bombsnd");
	audioOpen("winsound.mp3",                          "winsnd");
	audioOpen("Loosesound.mp3",                        "losesnd");

	// Level 02 background music.
	audioOpen("level02\\SOUND.mp3",                    "lvl2bgm");
}

//==============================================================================
//  5. LEVEL 01 SETUP
//
//  Every object is placed here, once, from fixed formulas. Because the arrays
//  have a fixed length and nothing is ever spawned during play, the level can
//  never contain more than 5 Life, 4 Speed, 10 Bomb and 30 Food boxes.
//==============================================================================

//==============================================================================
//  ENDLESS SPAWNING
//
//  Level 01 no longer has a hand-written list of objects. Each array is a pool:
//  when an entry is collected, hit, or scrolls off the left, it is immediately
//  re-placed further up the road at that kind's spawn cursor, and the cursor
//  advances by a random gap. Ten bombs recycled this way produce an unlimited
//  stream of bombs while never putting more than ten on the road at once.
//==============================================================================

float randBetween(float lo, float hi)
{
	return lo + (hi - lo) * ((float)rand() / (float)RAND_MAX);
}

// Keeps a pickup clear of every bomb currently on the road.
float clearOfBombs(float x)
{
	int i, guard;

	for (guard = 0; guard < 8; guard++)
	{
		int moved = 0;

		for (i = 0; i < BOMB_BOX_COUNT; i++)
		{
			if (!bombBox[i].active) continue;

			if (x > bombBox[i].worldX - PICKUP_CLEARANCE &&
			    x < bombBox[i].worldX + PICKUP_CLEARANCE)
			{
				x = bombBox[i].worldX + PICKUP_CLEARANCE + 40.0f;
				moved = 1;
			}
		}

		if (!moved) break;
	}

	return x;
}

// Drops bomb i at the bomb cursor. One bomb in three flies HIGH (slide under
// it); the rest fly LOW (jump over it), exactly as before.
void spawnBomb(int i)
{
	int high = (rand() % 3 == 0);

	bombBox[i].worldX = bombSpawnX;
	bombBox[i].y      = high ? (GROUND_Y + BOMB_HIGH_Y) : (GROUND_Y + BOMB_LOW_Y);
	bombBox[i].baseY  = bombBox[i].y;
	bombBox[i].active = 1;
	bombBox[i].hasHit = 0;

	bombSpawnX += randBetween(BOMB_GAP_MIN, BOMB_GAP_MAX);
}

void spawnLifeBox(int i)
{
	lifeBox[i].worldX = clearOfBombs(lifeSpawnX);
	lifeBox[i].y      = (rand() % 2) ? (GROUND_Y + 6.0f) : (GROUND_Y + 104.0f);
	lifeBox[i].active = 1;

	lifeSpawnX = lifeBox[i].worldX + randBetween(LIFE_GAP_MIN, LIFE_GAP_MAX);
}

void spawnSpeedBox(int i)
{
	speedBox[i].worldX = clearOfBombs(speedSpawnX);
	speedBox[i].y      = (rand() % 2) ? (GROUND_Y + 6.0f) : (GROUND_Y + 100.0f);
	speedBox[i].active = 1;

	speedSpawnX = speedBox[i].worldX + randBetween(SPEED_GAP_MIN, SPEED_GAP_MAX);
}

// Food keeps its trail look: five in a row, then a wider gap before the next.
void spawnFoodBox(int i)
{
	foodBox[i].worldX = foodSpawnX;
	foodBox[i].y      = (foodInTrail < FOOD_TRAIL_LEN) ? (GROUND_Y + 8.0f)
	                                                   : (GROUND_Y + 98.0f);
	foodBox[i].active = 1;

	foodInTrail++;

	if (foodInTrail >= FOOD_TRAIL_LEN)
	{
		foodInTrail = 0;
		foodSpawnX += randBetween(FOOD_GAP_MIN, FOOD_GAP_MAX);
	}
	else
	{
		foodSpawnX += FOOD_STEP;
	}
}

void resetLevel01()
{
	int i;

	// --- player and level counters back to their starting values --------------
	scrollX          = 0.0f;           // back to the first background
	hp               = MAX_HP;         // 200 / 200
	lifeBoxCount     = 0;              // no protection until one is collected
	score            = 0;
	foodCollected    = 0;

	playerY          = (float)GROUND_Y;
	playerVY         = 0.0f;
	isJumping        = 0;
	isSliding        = 0;

	speedBoostActive = 0;
	speedBoostTimer  = 0.0f;
	damageFlash      = 0.0f;

	runFrame         = 0;

	// --- prime the spawn cursors, then fill every pool from them --------------
	// The first bomb is far enough in that the player has time to settle.
	bombSpawnX  = 2400.0f;
	lifeSpawnX  = 1500.0f;
	speedSpawnX =  900.0f;
	foodSpawnX  =  700.0f;
	foodInTrail = 0;

	// Bombs first, so the pickups below can be placed clear of them.
	for (i = 0; i < BOMB_BOX_COUNT; i++)   spawnBomb(i);
	for (i = 0; i < LIFE_BOX_COUNT; i++)   spawnLifeBox(i);
	for (i = 0; i < SPEED_BOX_COUNT; i++)  spawnSpeedBox(i);
	for (i = 0; i < FOOD_BOX_COUNT; i++)   spawnFoodBox(i);

	prevTick = GetTickCount();
}

//==============================================================================
//  LEVEL / SCREEN TRANSITIONS
//==============================================================================

void startIntroMusic()
{
	if (!introMusicPlaying)
	{
		audioPlayLoop("introsong");
		introMusicPlaying = 1;
	}
}

void startLevel02();   // defined in the Level 02 section further down
void startLevel03();   // defined in the Level 03 section further down

// NEW GAME opens this instead of dropping straight into a level.
// The level select screen is still part of the menu shell, so the intro
// narration keeps looping here; it only stops when a level actually starts.
void openLevelSelect()
{
	gameState     = LEVEL_SELECT;
	hoveredCard   = -1;
	backHovered   = 0;
	hoveredButton = -1;
	levelMessage  = MSG_NONE;
}

void startLevel01()
{
	// The intro loop ends here, when Level 01 actually begins.
	audioStop("introsong");
	introMusicPlaying = 0;

	resetLevel01();

	gameState = LEVEL_01;
	iResumeTimer(animTimerId);         // the run cycle only animates in game

	// Level music starts once here, and loops by itself until the level ends.
	audioPlayLoop("lvl1bgm");
}

// Called the moment the player touches the finish line.
void endLevel01Win()
{
	gameState = LEVEL_01_WIN;
	iPauseTimer(animTimerId);

	audioStop("lvl1bgm");              // music stops before the result sound
	audioPlayOnce("winsnd");           // called once, from this transition only
}

// Called when health hits zero.
void endLevel01Lose()
{
	gameState = LEVEL_01_LOSE;
	iPauseTimer(animTimerId);

	audioStop("lvl1bgm");
	audioPlayOnce("losesnd");          // called once, from this transition only

	// Level 01 is an endless run, so there is no "completed" for it - only
	// the distance and score reached before the last explosion.
	recordLevelResult(1, 0, score);
}

void returnToMenu()
{
	audioStop("lvl1bgm");
	audioStop("winsnd");
	audioStop("losesnd");
	iPauseTimer(animTimerId);

	gameState     = MAIN_MENU;
	hoveredButton = -1;
	backHovered   = 0;

	startIntroMusic();
}

//==============================================================================
//  6. LEVEL 01 UPDATE  -  all of the game rules live here
//
//  This runs from fixedUpdate(), roughly every 16 ms. Instead of trusting the
//  timer to be exact we measure the real time that passed (dt) with
//  GetTickCount, so the 8 second speed boost stays accurate even if Windows
//  delivers the timer late.
//==============================================================================

void updateLevel01()
{
	DWORD now;
	float dt;
	float speedMult;
	float scrollSpeed;
	float playerBoxY, playerBoxH;
	int   i;

	// ---- 1. how much real time passed since the last update ------------------
	now = GetTickCount();
	dt  = (now - prevTick) / 1000.0f;
	prevTick = now;

	if (dt > 0.10f) dt = 0.10f;        // ignore long stalls (e.g. window dragged)
	if (dt <= 0.0f) return;

	// ---- 2. the speed boost clock --------------------------------------------
	// The timer is set to 8 seconds once, when the Speed Box is picked up, and
	// only counts down here. It is never refilled while the effect is running.
	if (speedBoostActive)
	{
		speedBoostTimer -= dt;
		if (speedBoostTimer <= 0.0f)
		{
			speedBoostTimer  = 0.0f;
			speedBoostActive = 0;      // back to normal speed after 8 seconds
		}
	}

	if (damageFlash > 0.0f)
		damageFlash -= dt;

	// ---- 3. how fast the world scrolls this frame ----------------------------
	speedMult = 1.0f;
	if (speedBoostActive)
		speedMult *= BOOST_MULT;
	if (isKeyPressed('d') || isSpecialKeyPressed(GLUT_KEY_RIGHT))
		speedMult *= FAST_KEY_MULT;
	if (isKeyPressed('a') || isSpecialKeyPressed(GLUT_KEY_LEFT))
		speedMult *= SLOW_KEY_MULT;

	scrollSpeed = BASE_SCROLL * speedMult;
	scrollX    += scrollSpeed * dt;

	// ---- 4. jumping and sliding ----------------------------------------------
	if (!isJumping &&
	    (isKeyPressed('w') || isKeyPressed(' ') || isSpecialKeyPressed(GLUT_KEY_UP)))
	{
		isJumping = 1;
		playerVY  = JUMP_V;
	}

	isSliding = (!isJumping &&
	             (isKeyPressed('s') || isSpecialKeyPressed(GLUT_KEY_DOWN)));

	if (isJumping)
	{
		playerVY += GRAVITY * dt;
		playerY  += playerVY * dt;

		if (playerY <= GROUND_Y)       // landed
		{
			playerY   = (float)GROUND_Y;
			playerVY  = 0.0f;
			isJumping = 0;
		}
	}

	// The player's collision box. Sliding makes it short, so the player can
	// duck under a bomb that is flying at head height.
	playerBoxY = playerY + 8.0f;
	playerBoxH = isSliding ? (PLAYER_H * 0.45f) : (PLAYER_H - 16.0f);

	// ---- 5. Life Boxes -------------------------------------------------------
	for (i = 0; i < LIFE_BOX_COUNT; i++)
	{
		float sx;
		if (!lifeBox[i].active) continue;

		sx = lifeBox[i].worldX - scrollX;          // world -> screen
		if (sx < -PICKUP_SIZE) { spawnLifeBox(i); continue; }   // missed, recycle

		if (boxesOverlap(PLAYER_X + 18, playerBoxY, PLAYER_W - 40, playerBoxH,
		                 sx - PICKUP_SIZE / 2.0, lifeBox[i].y,
		                 PICKUP_SIZE, PICKUP_SIZE))
		{
			// One more explosion the player can walk away from.
			lifeBoxCount++;

			score += SCORE_PER_LIFE;
			audioPlayOnce("pickupsnd");
			spawnLifeBox(i);                       // taken, so re-place it ahead
		}
	}

	// ---- 6. Speed Boxes ------------------------------------------------------
	for (i = 0; i < SPEED_BOX_COUNT; i++)
	{
		float sx;
		if (!speedBox[i].active) continue;

		sx = speedBox[i].worldX - scrollX;
		if (sx < -PICKUP_SIZE) { spawnSpeedBox(i); continue; }

		if (boxesOverlap(PLAYER_X + 18, playerBoxY, PLAYER_W - 40, playerBoxH,
		                 sx - PICKUP_SIZE / 2.0, speedBox[i].y,
		                 PICKUP_SIZE, PICKUP_SIZE))
		{
			// The 8 second countdown is started HERE, at the moment of pickup,
			// and nowhere else. Picking up a second box refills it to 8 again.
			speedBoostActive = 1;
			speedBoostTimer  = SPEED_BOOST_DURATION;

			score += SCORE_PER_SPEED;
			audioPlayOnce("pickupsnd");
			spawnSpeedBox(i);
		}
	}

	// ---- 7. Food Boxes (collectables, like coins) ----------------------------
	for (i = 0; i < FOOD_BOX_COUNT; i++)
	{
		float sx;
		if (!foodBox[i].active) continue;

		sx = foodBox[i].worldX - scrollX;
		if (sx < -PICKUP_SIZE) { spawnFoodBox(i); continue; }

		if (boxesOverlap(PLAYER_X + 18, playerBoxY, PLAYER_W - 40, playerBoxH,
		                 sx - PICKUP_SIZE / 2.0, foodBox[i].y,
		                 PICKUP_SIZE, PICKUP_SIZE))
		{
			// Food is purely a collectable for now: it is counted and scored,
			// but it deliberately has no health / speed / time effect.
			foodCollected++;
			score += SCORE_PER_FOOD;
			spawnFoodBox(i);
		}
	}

	// ---- 8. Bomb Boxes -------------------------------------------------------
	// A bomb is not a still obstacle. Once it comes near the screen it starts
	// travelling toward the player faster than the ground scrolls, and it also
	// drifts up or down to line itself up with the player.
	for (i = 0; i < BOMB_BOX_COUNT; i++)
	{
		float sx, targetY;
		if (!bombBox[i].active) continue;

		sx = bombBox[i].worldX - scrollX;

		if (sx < SCREEN_W + 220)        // close enough to start chasing
		{
			// always closes in on the player, faster than the ground moves
			bombBox[i].worldX -= BOMB_APPROACH_SPEED * dt;

			// Steer toward the player's height, but only while still far away.
			// Inside BOMB_LOCK_DISTANCE the bomb commits to its current line,
			// which is what makes a last moment jump or slide actually work.
			if (sx > BOMB_LOCK_DISTANCE)
			{
				targetY = playerY + 10.0f;
				bombBox[i].y += (targetY - bombBox[i].y) * BOMB_HOME_RATE * dt;

				// Keep the drift inside the cap so a HIGH bomb stays high and
				// a LOW bomb stays low.
				if (bombBox[i].y > bombBox[i].baseY + BOMB_DRIFT_LIMIT)
					bombBox[i].y = bombBox[i].baseY + BOMB_DRIFT_LIMIT;
				if (bombBox[i].y < bombBox[i].baseY - BOMB_DRIFT_LIMIT)
					bombBox[i].y = bombBox[i].baseY - BOMB_DRIFT_LIMIT;
			}

			sx = bombBox[i].worldX - scrollX;
		}

		if (sx < -BOMB_SIZE) { spawnBomb(i); continue; }   // dodged, send another

		if (!bombBox[i].hasHit &&
		    boxesOverlap(PLAYER_X + 18, playerBoxY, PLAYER_W - 40, playerBoxH,
		                 sx - (BOMB_SIZE * BOMB_HIT_SCALE) / 2.0, bombBox[i].y,
		                 BOMB_SIZE * BOMB_HIT_SCALE, BOMB_SIZE * BOMB_HIT_SCALE))
		{
			// Marking the bomb as used means this one bomb can never be counted
			// twice, and the sound only fires once per bomb.
			bombBox[i].hasHit = 1;
			damageFlash = 0.35f;
			audioPlayOnce("bombsnd");

			// THE SURVIVAL RULE. A Life Box in hand is spent to walk away from
			// this explosion. With none left, the run is over. Health plays no
			// part in it any more.
			if (lifeBoxCount > 0)
			{
				lifeBoxCount--;                    // one box absorbs one blast
				spawnBomb(i);                      // keep the stream going
			}
			else
			{
				bombBox[i].active = 0;
				endLevel01Lose();                  // LOSE: hit with no Life Box
				return;
			}
		}
	}

	// ---- 9. no finish line ---------------------------------------------------
	// The run has no end and no win condition. It carries on until an explosion
	// lands while the player holds no Life Box.
}

//==============================================================================
//  7. LEVEL 01 DRAWING
//==============================================================================

//------------------------------------------------------------------------------
// Backgrounds.
//
// The level is one long strip: background 1 occupies world x 0..1280,
// background 2 occupies 1280..2560, and so on to background 10.
// We work out which background the player is standing on, draw it shifted left
// by how far into it we are, and draw the NEXT one immediately behind it. That
// way the two always meet edge to edge and the picture never jumps back to the
// first background.
//------------------------------------------------------------------------------
void drawBackgroundSequence()
{
	float offset;
	int   i;

	// Level 01 runs through one single scene: Backround/bg1.png, held for the
	// whole endless run. The scroll simply repeats that one image rather than
	// stepping through bg1..bg10.
	offset = (float)fmod((double)scrollX, (double)SEG_W);

	// current copy
	iShowImage((int)(-offset), 0, SEG_W, SCREEN_H, sprBg[0].tex);

	// the same image again right behind it, so the two always meet edge to edge
	// and there is never a bare gap on the right
	iShowImage((int)(SEG_W - offset), 0, SEG_W, SCREEN_H, sprBg[0].tex);

	// The picture's right edge does not match its own left edge, so the point
	// where one copy hands over to the next would show as a hard vertical line.
	// A dark band over the join reads as fog and hides it.
	{
		float joinX = SEG_W - offset;
		float half  = 130.0f;

		for (i = 0; i < 2; i++)
		{
			float x0 = (i == 0) ? (joinX - half) : joinX;
			float a0 = (i == 0) ? 0.0f : 0.75f;
			float a1 = (i == 0) ? 0.75f : 0.0f;

			glBegin(GL_QUADS);
				glColor4d(0.02, 0.02, 0.04, a0);
				glVertex2d(x0, 0);
				glVertex2d(x0, SCREEN_H);
				glColor4d(0.02, 0.02, 0.04, a1);
				glVertex2d(x0 + half, SCREEN_H);
				glVertex2d(x0 + half, 0);
			glEnd();
		}
		glColor4d(1.0, 1.0, 1.0, 1.0);
	}
}

//------------------------------------------------------------------------------
// The finish line: a striped banner on two poles, drawn with plain shapes
// because the project has no finish line picture.
//------------------------------------------------------------------------------
void drawFinishLine()
{
	float fx = FINISH_WORLD_X - scrollX;
	int   row, col;

	if (fx < -140 || fx > SCREEN_W + 140)
		return;

	// poles
	iSetColor(225, 225, 235);
	iFilledRectangle(fx - 62, GROUND_Y - 10, 10, 330);
	iFilledRectangle(fx + 52, GROUND_Y - 10, 10, 330);

	// chequered banner
	for (row = 0; row < 4; row++)
	{
		for (col = 0; col < 6; col++)
		{
			if ((row + col) % 2 == 0)
				iSetColor(250, 250, 250);
			else
				iSetColor(25, 25, 30);

			iFilledRectangle(fx - 52 + col * 17, GROUND_Y + 246 - row * 17, 17, 17);
		}
	}

	iSetColor(255, 220, 90);
	iTextCentered(fx, GROUND_Y + 330, "FINISH", GLUT_BITMAP_HELVETICA_18);
}

//------------------------------------------------------------------------------
// HUD: health, score, food and the speed state.
//------------------------------------------------------------------------------
void drawLevel01Hud()
{
	char buf[128];
	float barW;

	iFilledRectangleAlpha(0, SCREEN_H - 58, SCREEN_W, 58, 8, 10, 16, 0.66);

	// --- health, as a number and as a bar ---
	sprintf(buf, "HP: %d / %d", (int)hp, (int)MAX_HP);
	iSetColor(255, 205, 80);
	iText(24, SCREEN_H - 38, buf, GLUT_BITMAP_HELVETICA_18);

	iFilledRectangleAlpha(24, SCREEN_H - 52, 190, 8, 60, 20, 20, 0.9);
	barW = 190.0f * (hp / MAX_HP);
	if (barW > 0)
	{
		if (hp > MAX_HP * 0.5f)      iFilledRectangleAlpha(24, SCREEN_H - 52, barW, 8,  70, 200,  90, 0.95);
		else if (hp > MAX_HP * 0.25f) iFilledRectangleAlpha(24, SCREEN_H - 52, barW, 8, 235, 190,  60, 0.95);
		else                          iFilledRectangleAlpha(24, SCREEN_H - 52, barW, 8, 225,  60,  55, 0.95);
	}

	// --- Life Boxes in hand: the number that decides survival ---
	// Green while protected, red on zero, because at zero the very next
	// explosion ends the run.
	sprintf(buf, "LIFEBOX: %d", lifeBoxCount);
	if (lifeBoxCount > 0) iSetColor(120, 235, 140);
	else                  iSetColor(255, 90, 80);
	iText(250, SCREEN_H - 38, buf, GLUT_BITMAP_HELVETICA_18);

	iSetColor(225, 225, 235);

	sprintf(buf, "SCORE: %d", score);
	iText(430, SCREEN_H - 38, buf, GLUT_BITMAP_HELVETICA_18);

	sprintf(buf, "FOOD: %d", foodCollected);
	iText(590, SCREEN_H - 38, buf, GLUT_BITMAP_HELVETICA_18);

	// --- speed state ---
	if (speedBoostActive)
	{
		iSetColor(120, 200, 255);
		iText(730, SCREEN_H - 38, "SPEED: FAST", GLUT_BITMAP_HELVETICA_18);

		sprintf(buf, "BOOST: %d", (int)(speedBoostTimer + 0.999f));
		iText(880, SCREEN_H - 38, buf, GLUT_BITMAP_HELVETICA_18);
	}
	else
	{
		iSetColor(180, 180, 195);
		iText(730, SCREEN_H - 38, "SPEED: NORMAL", GLUT_BITMAP_HELVETICA_18);
	}

	// --- distance run ---
	// There is no finish to make progress towards, so the old progress bar is
	// replaced by how far the player has actually got. It sits far enough right
	// to clear the BOOST readout, which only appears while a boost is running.
	sprintf(buf, "DISTANCE: %d m", (int)(scrollX / 10.0f));
	iSetColor(235, 200, 90);
	iText(1030, SCREEN_H - 38, buf, GLUT_BITMAP_HELVETICA_18);
}

void drawLevel01()
{
	int i;
	float sx;

	drawBackgroundSequence();
	// no finish banner: the run is endless

	// --- Food Boxes ---
	for (i = 0; i < FOOD_BOX_COUNT; i++)
	{
		if (!foodBox[i].active) continue;
		sx = foodBox[i].worldX - scrollX;
		if (sx < -PICKUP_SIZE || sx > SCREEN_W + PICKUP_SIZE) continue;
		drawSpriteFit(sprFoodBox, sx, foodBox[i].y + PICKUP_SIZE / 2.0, PICKUP_SIZE);
	}

	// --- Life Boxes ---
	for (i = 0; i < LIFE_BOX_COUNT; i++)
	{
		if (!lifeBox[i].active) continue;
		sx = lifeBox[i].worldX - scrollX;
		if (sx < -PICKUP_SIZE || sx > SCREEN_W + PICKUP_SIZE) continue;
		drawSpriteFit(sprLifeBox, sx, lifeBox[i].y + PICKUP_SIZE / 2.0, PICKUP_SIZE);
	}

	// --- Speed Boxes ---
	for (i = 0; i < SPEED_BOX_COUNT; i++)
	{
		if (!speedBox[i].active) continue;
		sx = speedBox[i].worldX - scrollX;
		if (sx < -PICKUP_SIZE || sx > SCREEN_W + PICKUP_SIZE) continue;
		drawSpriteFit(sprSpeedBox, sx, speedBox[i].y + PICKUP_SIZE / 2.0, PICKUP_SIZE);
	}

	// --- Bomb Boxes ---
	for (i = 0; i < BOMB_BOX_COUNT; i++)
	{
		if (!bombBox[i].active) continue;
		sx = bombBox[i].worldX - scrollX;
		if (sx < -BOMB_SIZE || sx > SCREEN_W + BOMB_SIZE) continue;
		drawSpriteFit(sprBombBox, sx, bombBox[i].y + BOMB_SIZE / 2.0, BOMB_SIZE);
	}

	// --- the player ---
	if (isSliding)
	{
		// squashed down while sliding, to match the shorter collision box
		iShowImage(PLAYER_X, (int)playerY, PLAYER_W, (int)(PLAYER_H * 0.55),
		           sprRun[runFrame].tex);
	}
	else
	{
		iShowImage(PLAYER_X, (int)playerY, PLAYER_W, PLAYER_H,
		           sprRun[runFrame].tex);
	}

	// red flash for a moment after a bomb hit
	if (damageFlash > 0.0f)
		iFilledRectangleAlpha(0, 0, SCREEN_W, SCREEN_H, 220, 40, 40, damageFlash * 0.7);

	drawLevel01Hud();

	iSetColor(150, 150, 165);
	iText(24, 18, "W / UP jump    S / DOWN slide    D faster    A slower    ESC menu",
	      GLUT_BITMAP_HELVETICA_12);
}

//==============================================================================
//  8. WIN AND LOSE SCREENS
//==============================================================================

void drawResultScreen(int didWin)
{
	char buf[128];
	int  y;

	// the frozen level stays visible behind the result panel
	drawLevel01();
	dimScreen(0.78);

	iFilledRectangleAlpha(340, 200, 600, 330, 10, 12, 20, 0.88);

	if (didWin) iSetColor(90, 210, 110);
	else        iSetColor(225, 60, 60);
	iRectangle(340, 200, 600, 330);

	y = 470;

	if (didWin)
	{
		iSetColor(90, 220, 120);
		iTextCentered(SCREEN_W / 2.0, y, "YOU WIN!", GLUT_BITMAP_TIMES_ROMAN_24);
		y -= 34;
		iSetColor(220, 220, 230);
		iTextCentered(SCREEN_W / 2.0, y, "LEVEL 01 COMPLETED", GLUT_BITMAP_HELVETICA_18);
	}
	else
	{
		iSetColor(235, 70, 70);
		iTextCentered(SCREEN_W / 2.0, y, "YOU LOSE", GLUT_BITMAP_TIMES_ROMAN_24);
		y -= 34;
		iSetColor(220, 220, 230);

		// Losing has only one cause now that the level clock is gone.
		iTextCentered(SCREEN_W / 2.0, y, "HIT AN EXPLOSION WITH NO LIFEBOX",
		              GLUT_BITMAP_HELVETICA_18);
	}

	y -= 46;
	iSetColor(90, 90, 105);
	iLine(400, y + 16, 880, y + 16);

	iSetColor(255, 205, 80);
	sprintf(buf, "Score: %d", score);
	iTextCentered(SCREEN_W / 2.0, y, buf, GLUT_BITMAP_HELVETICA_18);

	y -= 30;
	iSetColor(220, 220, 230);
	sprintf(buf, "Distance Run: %d m", (int)(scrollX / 10.0f));
	iTextCentered(SCREEN_W / 2.0, y, buf, GLUT_BITMAP_HELVETICA_18);

	y -= 30;
	sprintf(buf, "Food Collected: %d", foodCollected);
	iTextCentered(SCREEN_W / 2.0, y, buf, GLUT_BITMAP_HELVETICA_18);

	y -= 56;
	iSetColor(170, 170, 185);

	if (didWin)
	{
		iTextCentered(SCREEN_W / 2.0, y, "Press ENTER to continue", GLUT_BITMAP_HELVETICA_12);
		y -= 20;
		iTextCentered(SCREEN_W / 2.0, y, "Press ESC for Main Menu", GLUT_BITMAP_HELVETICA_12);
	}
	else
	{
		iTextCentered(SCREEN_W / 2.0, y, "Press R to Restart", GLUT_BITMAP_HELVETICA_12);
		y -= 20;
		iTextCentered(SCREEN_W / 2.0, y, "Press ESC for Main Menu", GLUT_BITMAP_HELVETICA_12);
	}
}

//==============================================================================
//  MENU SCREENS  (unchanged from the previous step)
//==============================================================================

void drawMenu()
{
	int i;
	int panelX = BTN_X - 22;
	int panelY = buttonY(BTN_COUNT - 1) - 26;
	int panelW = BTN_W + 44;
	int panelH = BTN_COUNT * BTN_H + (BTN_COUNT - 1) * BTN_GAP + 52;

	iShowImage(0, 0, SCREEN_W, SCREEN_H, sprPoster.tex);

	iFilledRectangleAlpha(panelX, panelY, panelW, panelH, 8, 10, 16, 0.62);
	iSetColor(150, 30, 30);
	iRectangle(panelX, panelY, panelW, panelH);

	for (i = 0; i < BTN_COUNT; i++)
	{
		int by  = buttonY(i);
		int hot = (hoveredButton == i);

		if (hot)
		{
			iFilledRectangleAlpha(BTN_X, by, BTN_W, BTN_H, 200, 35, 35, 0.92);
			iSetColor(255, 235, 190);
		}
		else
		{
			iFilledRectangleAlpha(BTN_X, by, BTN_W, BTN_H, 26, 30, 40, 0.88);
			iSetColor(215, 215, 225);
		}

		iRectangle(BTN_X, by, BTN_W, BTN_H);
		iTextCentered(BTN_X + BTN_W / 2.0, by + BTN_H / 2.0 - 6,
		              btnLabel[i], GLUT_BITMAP_HELVETICA_18);
	}

	iSetColor(190, 190, 200);
	iText(20, 16, "Use the mouse to choose an option.", GLUT_BITMAP_HELVETICA_12);
}

void drawBackButton()
{
	if (backHovered)
	{
		iFilledRectangleAlpha(BACK_X, BACK_Y, BACK_W, BACK_H, 200, 35, 35, 0.92);
		iSetColor(255, 235, 190);
	}
	else
	{
		iFilledRectangleAlpha(BACK_X, BACK_Y, BACK_W, BACK_H, 26, 30, 40, 0.88);
		iSetColor(215, 215, 225);
	}

	iRectangle(BACK_X, BACK_Y, BACK_W, BACK_H);
	iTextCentered(BACK_X + BACK_W / 2.0, BACK_Y + BACK_H / 2.0 - 6,
	              "BACK", GLUT_BITMAP_HELVETICA_18);
}

void drawKeyRow(int y, char *key, char *action)
{
	iSetColor(255, 205, 80);
	iText(300, y, key, GLUT_BITMAP_HELVETICA_18);

	iSetColor(225, 225, 235);
	iText(560, y, action, GLUT_BITMAP_HELVETICA_18);
}

void drawKeys()
{
	int y;

	iShowImage(0, 0, SCREEN_W, SCREEN_H, sprPoster.tex);
	dimScreen(0.82);

	iFilledRectangleAlpha(210, 110, 860, 500, 10, 12, 20, 0.75);
	iSetColor(150, 30, 30);
	iRectangle(210, 110, 860, 500);

	iSetColor(235, 60, 60);
	iTextCentered(SCREEN_W / 2.0, 552, "CONTROLS", GLUT_BITMAP_TIMES_ROMAN_24);

	iSetColor(170, 170, 185);
	iTextCentered(SCREEN_W / 2.0, 524,
	              "Keyboard layout for Shadow Chase", GLUT_BITMAP_HELVETICA_12);

	iSetColor(90, 90, 105);
	iLine(300, 508, 980, 508);

	y = 466;
	drawKeyRow(y, "RIGHT  /  D", "Sprint (outrun a falling net)"); y -= 42;
	drawKeyRow(y, "LEFT  /  A",  "Slow down");                     y -= 42;
	drawKeyRow(y, "UP  /  W",    "Jump over a bomb");              y -= 42;
	drawKeyRow(y, "DOWN  /  S",  "Slide under a bomb");            y -= 42;
	drawKeyRow(y, "SPACE",       "Jump (alternate key)");          y -= 42;
	drawKeyRow(y, "F",           "Cut a net (Level 02)");          y -= 42;
	drawKeyRow(y, "R",           "Restart after losing");          y -= 42;
	drawKeyRow(y, "ENTER",       "Continue after winning");        y -= 42;
	drawKeyRow(y, "ESC",         "Back to the main menu");

	drawBackButton();
}

void drawAbout()
{
	int y;

	iShowImage(0, 0, SCREEN_W, SCREEN_H, sprPoster.tex);
	dimScreen(0.85);

	iFilledRectangleAlpha(96, 74, 1088, 580, 10, 12, 20, 0.78);
	iSetColor(150, 30, 30);
	iRectangle(96, 74, 1088, 580);

	iSetColor(235, 60, 60);
	iTextCentered(SCREEN_W / 2.0, 612,
	              "Shadow Chase: The Final Escape", GLUT_BITMAP_TIMES_ROMAN_24);

	iSetColor(170, 170, 185);
	iTextCentered(SCREEN_W / 2.0, 588,
	              "CSE-1200  Software Development I   |   Dept. of CSE, AUST",
	              GLUT_BITMAP_HELVETICA_12);

	iSetColor(90, 90, 105);
	iLine(140, 572, 1140, 572);

	y = 540;

	iSetColor(255, 205, 80);
	iText(140, y, "THE GAME", GLUT_BITMAP_HELVETICA_18);
	y -= 28;

	iSetColor(220, 220, 230);
	iText(140, y, "Shadow Chase: The Final Escape is a 2D survival adventure game built with C++", GLUT_BITMAP_HELVETICA_12);
	y -= 20;
	iText(140, y, "and the iGraphics library. The player controls a character who continuously runs", GLUT_BITMAP_HELVETICA_12);
	y -= 20;
	iText(140, y, "through a dangerous environment while escaping an unknown threat. The goal is", GLUT_BITMAP_HELVETICA_12);
	y -= 20;
	iText(140, y, "to survive for as long as possible.", GLUT_BITMAP_HELVETICA_12);
	y -= 34;

	iSetColor(255, 205, 80);
	iText(140, y, "ALONG THE WAY", GLUT_BITMAP_HELVETICA_18);
	y -= 28;

	iSetColor(220, 220, 230);
	iText(140, y, "Collect life, food, speed, extra time and knife boxes to survive longer.", GLUT_BITMAP_HELVETICA_12);
	y -= 20;
	iText(140, y, "Avoid bomb boxes, rocks and obstacles - a collision costs speed or health.", GLUT_BITMAP_HELVETICA_12);
	y -= 20;
	iText(140, y, "Net traps fall from the trees and hold you; a collected knife cuts you free.", GLUT_BITMAP_HELVETICA_12);
	y -= 20;
	iText(140, y, "Difficulty rises the longer you last. The run ends when your life reaches zero.", GLUT_BITMAP_HELVETICA_12);
	y -= 34;

	iSetColor(255, 205, 80);
	iText(140, y, "CORE FEATURES", GLUT_BITMAP_HELVETICA_18);
	y -= 28;

	iSetColor(220, 220, 230);
	iText(140, y, "Continuous movement   |   Life, Food, Speed, Time and Knife boxes", GLUT_BITMAP_HELVETICA_12);
	y -= 20;
	iText(140, y, "Collision detection   |   Net trap and knife escape   |   Score tracking", GLUT_BITMAP_HELVETICA_12);
	y -= 20;
	iText(140, y, "Increasing difficulty   |   Game Over and Restart system", GLUT_BITMAP_HELVETICA_12);
	y -= 34;

	iSetColor(255, 205, 80);
	iText(140, y, "INSPIRED BY", GLUT_BITMAP_HELVETICA_18);
	y -= 28;

	iSetColor(220, 220, 230);
	iText(140, y, "Endless runners such as Subway Surfers and Temple Run.", GLUT_BITMAP_HELVETICA_12);

	iSetColor(255, 205, 80);
	iText(800, 300, "PROJECT TEAM", GLUT_BITMAP_HELVETICA_18);

	iSetColor(220, 220, 230);
	iText(800, 268, "Mst. Tasnim Binte Ali", GLUT_BITMAP_HELVETICA_12);
	iSetColor(150, 150, 165);
	iText(800, 250, "00725105101003", GLUT_BITMAP_HELVETICA_12);

	iSetColor(220, 220, 230);
	iText(800, 222, "Humayara Tabassum", GLUT_BITMAP_HELVETICA_12);
	iSetColor(150, 150, 165);
	iText(800, 204, "00725105101008", GLUT_BITMAP_HELVETICA_12);

	iSetColor(220, 220, 230);
	iText(800, 176, "Mehjabin Hossain Anchal", GLUT_BITMAP_HELVETICA_12);
	iSetColor(150, 150, 165);
	iText(800, 158, "00725105101029", GLUT_BITMAP_HELVETICA_12);

	drawBackButton();
}

//==============================================================================
//  LEVEL SELECTION SCREEN
//
//  Drawing, hover and clicking are kept in three separate functions so the
//  iGraphics callbacks at the bottom of the file stay one line each.
//==============================================================================

//------------------------------------------------------------------------------
// A padlock, drawn from plain shapes because the project has no lock image.
// The shackle is three thin arcs stacked side by side so it reads as one
// thick bar; iGraphics has no arc call of its own.
//------------------------------------------------------------------------------
void drawPadlock(double cx, double cy, double size)
{
	double bodyW = size * 0.86;
	double bodyH = size * 0.58;
	double r     = size * 0.30;
	double PI    = acos(-1.0);
	double baseY = cy + bodyH * 0.5;
	int    k;

	for (k = 0; k < 3; k++)
	{
		double rr = r + k * 1.7;
		double px = cx + rr;
		double py = baseY;
		double t;

		for (t = 0.0; t <= PI + 0.001; t += PI / 24.0)
		{
			double nx = cx + rr * cos(t);
			double ny = baseY + rr * sin(t);
			iLine(px, py, nx, ny);
			px = nx;
			py = ny;
		}
	}

	iFilledRectangle(cx - bodyW / 2.0, cy - bodyH / 2.0, bodyW, bodyH);
}

//------------------------------------------------------------------------------
// One level card. Everything about how it looks is decided by whether the
// level is unlocked, so the lock state has a single source of truth.
//------------------------------------------------------------------------------
void drawLevelCard(int index)
{
	int  x        = cardX(index);
	int  unlocked = levelUnlocked[index];
	int  hot      = (hoveredCard == index && levelMessage == MSG_NONE);
	double cx     = x + CARD_W / 2.0;

	// --- body ---
	if (!unlocked)
		iFilledRectangleAlpha(x, CARD_Y, CARD_W, CARD_H, 14, 14, 18, 0.86);
	else if (hot)
		iFilledRectangleAlpha(x, CARD_Y, CARD_W, CARD_H, 52, 26, 26, 0.94);
	else
		iFilledRectangleAlpha(x, CARD_Y, CARD_W, CARD_H, 20, 24, 34, 0.90);

	// --- border ---
	if (!unlocked)        iSetColor(80, 80, 92);
	else if (hot)         iSetColor(255, 90, 80);
	else                  iSetColor(150, 30, 30);
	iRectangle(x, CARD_Y, CARD_W, CARD_H);

	// --- big level number ---
	if (!unlocked)        iSetColor(120, 120, 134);
	else if (hot)         iSetColor(255, 235, 190);
	else                  iSetColor(235, 235, 245);
	iTextCentered(cx, CARD_Y + CARD_H - 58, levelTitle[index], GLUT_BITMAP_TIMES_ROMAN_24);

	// --- divider ---
	iSetColor(90, 90, 105);
	iLine(x + 40, CARD_Y + CARD_H - 78, x + CARD_W - 40, CARD_Y + CARD_H - 78);

	// --- middle: padlock for a locked level, status word otherwise ---
	if (!unlocked)
	{
		iSetColor(150, 150, 165);
		drawPadlock(cx, CARD_Y + CARD_H / 2.0 - 6, 74);
	}
	else
	{
		iSetColor(90, 210, 120);
		iTextCentered(cx, CARD_Y + CARD_H / 2.0 + 6, "UNLOCKED", GLUT_BITMAP_HELVETICA_18);
	}

	// --- status strip near the bottom ---
	if (!unlocked)
	{
		iSetColor(225, 70, 70);
		iTextCentered(cx, CARD_Y + 62, "LOCKED", GLUT_BITMAP_HELVETICA_18);
	}
	else
	{
		iSetColor(255, 205, 80);
		iTextCentered(cx, CARD_Y + 62, "READY TO PLAY", GLUT_BITMAP_HELVETICA_18);
	}

	// --- subtitle ---
	iSetColor(150, 150, 165);
	iTextCentered(cx, CARD_Y + 34, levelSubtitle[index], GLUT_BITMAP_HELVETICA_12);
}

//------------------------------------------------------------------------------
// The notice shown after clicking Level 02 or a locked Level 03.
//------------------------------------------------------------------------------
void drawLevelMessage()
{
	dimScreen(0.70);

	iFilledRectangleAlpha(360, 250, 560, 220, 10, 12, 20, 0.94);

	if (levelMessage == MSG_LOCKED) iSetColor(225, 70, 70);
	else                            iSetColor(255, 205, 80);
	iRectangle(360, 250, 560, 220);

	if (levelMessage == MSG_LOCKED)
	{
		iSetColor(235, 70, 70);
		iTextCentered(SCREEN_W / 2.0, 408, "LEVEL 03 IS LOCKED", GLUT_BITMAP_TIMES_ROMAN_24);

		iSetColor(220, 220, 230);
		iTextCentered(SCREEN_W / 2.0, 360,
		              "Complete the previous levels to unlock it.", GLUT_BITMAP_HELVETICA_18);
	}
	else
	{
		iSetColor(255, 205, 80);
		iTextCentered(SCREEN_W / 2.0, 408, "LEVEL 02  -  COMING SOON", GLUT_BITMAP_TIMES_ROMAN_24);

		iSetColor(220, 220, 230);
		iTextCentered(SCREEN_W / 2.0, 360,
		              "This level is not implemented yet.", GLUT_BITMAP_HELVETICA_18);
	}

	iSetColor(150, 150, 165);
	iTextCentered(SCREEN_W / 2.0, 296,
	              "Click anywhere or press ESC to go back", GLUT_BITMAP_HELVETICA_12);
}

void drawLevelSelect()
{
	int i;

	// Same backdrop treatment as the KEYS and ABOUT screens.
	iShowImage(0, 0, SCREEN_W, SCREEN_H, sprPoster.tex);
	dimScreen(0.80);

	iSetColor(235, 60, 60);
	iTextCentered(SCREEN_W / 2.0, 620, "SELECT LEVEL", GLUT_BITMAP_TIMES_ROMAN_24);

	iSetColor(170, 170, 185);
	iTextCentered(SCREEN_W / 2.0, 592,
	              "Choose a level to begin your escape", GLUT_BITMAP_HELVETICA_12);

	iSetColor(90, 90, 105);
	iLine(340, 574, 940, 574);

	for (i = 0; i < LEVEL_COUNT; i++)
		drawLevelCard(i);

	drawBackButton();

	if (levelMessage != MSG_NONE)
		drawLevelMessage();
}

//------------------------------------------------------------------------------
// Hover tracking. Skipped while a notice is up, since the cards are inert then.
//------------------------------------------------------------------------------
void levelSelectHover(int mx, int my)
{
	int i;

	hoveredCard = -1;
	backHovered = 0;

	if (levelMessage != MSG_NONE)
		return;

	for (i = 0; i < LEVEL_COUNT; i++)
	{
		if (pointInBox(mx, my, cardX(i), CARD_Y, CARD_W, CARD_H))
		{
			hoveredCard = i;
			return;                 // cards never overlap, so stop at the first
		}
	}

	backHovered = pointInBox(mx, my, BACK_X, BACK_Y, BACK_W, BACK_H);
}

//------------------------------------------------------------------------------
// Clicking. A level only starts when its unlock flag allows it.
//------------------------------------------------------------------------------
void handleLevelSelectClick(int mx, int my)
{
	int i;

	// A notice swallows the click and closes, returning to the cards.
	if (levelMessage != MSG_NONE)
	{
		levelMessage = MSG_NONE;
		return;
	}

	if (pointInBox(mx, my, BACK_X, BACK_Y, BACK_W, BACK_H))
	{
		gameState   = MAIN_MENU;
		hoveredCard = -1;
		backHovered = 0;
		return;
	}

	for (i = 0; i < LEVEL_COUNT; i++)
	{
		if (!pointInBox(mx, my, cardX(i), CARD_Y, CARD_W, CARD_H))
			continue;

		if (!levelUnlocked[i])
		{
			levelMessage = MSG_LOCKED;       // Level 03: no gameplay is started
			return;
		}

		if (i == 0)
			startLevel01();                  // hands off to the existing level
		else if (i == 1)
			startLevel02();                  // the daylight forest run
		else
			startLevel03();                  // the eagle chase
		return;
	}
}

//==============================================================================
//  PROGRESS FILE  -  info.txt
//
//  A plain text record of how each level went, written next to the executable's
//  working directory. It is read back before every write, so finishing one
//  level never erases what the other two recorded.
//
//  Kept deliberately simple for Visual Studio 2013: fopen / fgets / fprintf,
//  no streams, no external libraries.
//==============================================================================

#define PROGRESS_LEVELS 3
#define PROGRESS_FILE   "info.txt"

int progressDone [PROGRESS_LEVELS] = { 0, 0, 0 };   // 1 once a level is cleared
int progressScore[PROGRESS_LEVELS] = { 0, 0, 0 };   // best score seen so far

// Pulls the existing file into the two arrays above. A missing file simply
// leaves everything at zero, which is the correct "nothing played yet" state.
void loadProgress()
{
	FILE *fp;
	char  line[256];
	int   i;

	for (i = 0; i < PROGRESS_LEVELS; i++)
	{
		progressDone[i]  = 0;
		progressScore[i] = 0;
	}

	fp = fopen(PROGRESS_FILE, "r");
	if (fp == NULL) return;

	while (fgets(line, sizeof(line), fp))
	{
		int n, v;

		// "Level 0X Score: N" has to be tested first - the shorter
		// "Level 0X:" pattern would otherwise swallow it.
		if (sscanf(line, "Level %d Score: %d", &n, &v) == 2)
		{
			if (n >= 1 && n <= PROGRESS_LEVELS) progressScore[n - 1] = v;
		}
		else if (sscanf(line, "Level %d:", &n) == 1)
		{
			if (n >= 1 && n <= PROGRESS_LEVELS)
				progressDone[n - 1] = (strstr(line, "Not Completed") == NULL &&
				                       strstr(line, "Completed")     != NULL);
		}
	}

	fclose(fp);
}

void writeProgress()
{
	FILE *fp;
	int   i, total = 0;

	fp = fopen(PROGRESS_FILE, "w");
	if (fp == NULL)
	{
		printf("PROGRESS: could not write %s\n", PROGRESS_FILE);
		fflush(stdout);
		return;
	}

	fprintf(fp, "PLAYER GAME INFORMATION\n");
	fprintf(fp, "=======================\n\n");

	for (i = 0; i < PROGRESS_LEVELS; i++)
	{
		fprintf(fp, "Level %02d: %s\n", i + 1,
		        progressDone[i] ? "Completed" : "Not Completed");
		fprintf(fp, "Level %02d Score: %d\n\n", i + 1, progressScore[i]);
		total += progressScore[i];
	}

	fprintf(fp, "Total Score: %d\n", total);
	fclose(fp);
}

// The one call every level makes when it ends. Completion is sticky (clearing
// a level once keeps it cleared) and the score only ever moves up, so a weaker
// later attempt cannot wipe out a better earlier one.
void recordLevelResult(int levelNumber, int completed, int levelScore)
{
	int idx = levelNumber - 1;

	if (idx < 0 || idx >= PROGRESS_LEVELS) return;

	loadProgress();                       // keep whatever the other levels wrote

	if (completed)                    progressDone[idx]  = 1;
	if (levelScore > progressScore[idx]) progressScore[idx] = levelScore;

	writeProgress();

	printf("PROGRESS: Level %02d %s, score %d\n", levelNumber,
	       completed ? "Completed" : "Not Completed", levelScore);
	fflush(stdout);
}

//==============================================================================
//  LEVEL 02  -  DAYLIGHT FOREST RUN
//
//  Built on exactly the same skeleton as Level 01: one scrolling world measured
//  in pixels, the character pinned to a fixed screen x, the same run frames,
//  the same jump physics, the same AABB test and the same MCI audio wrapper.
//  What is new here is the gem/magnet economy and the knife-and-net trap.
//==============================================================================

//------------------------------------------------------------------------------
// Tunables. Every Level 02 rule number lives here.
//------------------------------------------------------------------------------
const float L2_BASE_SCROLL   = 185.0f;   // normal running speed, pixels/second
const float L2_SLOW_MULT     =   0.5f;   // speed while slowed by woods/cactus
const float L2_SLOW_DURATION =   5.0f;   // seconds a slowdown lasts

// Holding RIGHT (or D) sprints. This is the escape route from a falling net:
// get to the net's spot before the net has dropped far enough to reach you.
// It applies only while the key is held - nothing is stored anywhere.
const float L2_BOOST_MULT    =   1.5f;

// Level 02's obstacles are far bigger than Level 01's bombs, so the character
// needs a taller hop to clear them. Same gravity, same physics, same code path
// as Level 01 - only the launch speed differs, and only inside Level 02.
// At 1020 the player gets about a third of a second to start the jump, against
// a sixth of a second at Level 01's 860, which was unfairly tight here.
const float L2_JUMP_V        = 1020.0f;

const float MAGNET_DURATION  =   8.0f;   // seconds a magnet stays active
const float MAGNET_RADIUS    = 300.0f;   // how near a gem must be to be pulled
const float MAGNET_PULL      = 620.0f;   // pixels/second a pulled gem travels

// ---------------------------------------------------------------------------
//  The net.
//
//  A net hangs at a fixed spot on the road and drops straight down there. It
//  does NOT follow the character, and it does not care whether a knife is
//  held - it always falls. Whether it catches anyone is pure geometry: the
//  character has to still be underneath it by the time its lower edge has come
//  down to head height.
//
//  These three numbers are what make the sprint escape real. Starting the drop
//  850 pixels ahead at 165 px/s, a character running normally arrives while the
//  net is already at head height and gets caught; a character holding RIGHT
//  gets there roughly half a second sooner, while the net is still well above,
//  and runs straight under it.
// ---------------------------------------------------------------------------
const float NET_TRIGGER_SX    = 850.0f;  // screen x at which the net starts falling
const float NET_FALL_SPEED    = 165.0f;  // pixels/second downward
const float NET_CUT_TIME      =   0.35f; // slash animation before running again

// Only the lower part of the netting can catch anybody, so the damage box is
// a shallow strip along its bottom edge rather than the whole picture.
const float NET_HIT_W = 150.0f;
const float NET_HIT_H =  60.0f;

// How many separate F presses it takes to cut free.
#define NET_REQUIRED_HITS 3

#define GEM_SCORE 100                    // every coloured gem is worth this

// World length. Ten screens of forest, then the finish banner.
#define L2_DISTANCE   (10 * SCREEN_W)
#define L2_FINISH_X   (L2_DISTANCE - 300)

// Fixed object counts - the arrays are exactly this long, so the level can
// never spawn more than these.
#define L2_GEM_COUNT       36
#define L2_GEM_KINDS        4            // blue, red, yellow, colored
#define L2_BLACKGEM_COUNT   6
#define L2_WOODS_COUNT      5
#define L2_CACTUS_COUNT     5
#define L2_KNIFEBOX_COUNT   2
#define L2_MAGNET_COUNT     2
#define L2_NET_COUNT        2

// On-screen sizes
#define GEM_SIZE       54
#define BLACKGEM_SIZE  60
#define MAGNET_SIZE    72
#define KNIFEBOX_SIZE 100
#define GROUND_OB_SIZE 190
#define NET_SIZE      230
#define KNIFE_ICON     34

// Obstacles march along the road at a fixed spacing so the player always has
// room to land between them. 680 pixels is about 3.7 seconds at normal speed.
#define L2_OB_TOTAL   16
#define L2_OB_FIRST 1700.0f
#define L2_OB_STEP   680.0f

// Height a "high" hazard sits at. Standing into it is a hit; sliding clears it.
#define L2_HIGH_Y (GROUND_Y + 92.0f)

// Touching a black gem ends the run outright, so its damage area is kept well
// inside the picture. At the drawn size the player would have had only about a
// sixth of a second to start the jump; trimmed to half, the window is roughly
// a third of a second, which is a fair ask for an instant-loss hazard.
const float BLACKGEM_HIT_SCALE = 0.50f;

// Woods and cactus are drawn big so they read clearly against the forest, but
// their damage box is a modest slab at the base of the picture, not the whole
// image. Given the jump above, these two numbers leave about a third of a
// second to start the hop - the difference between "large obstacle" and
// "impossible obstacle".
const float GROUND_OB_HIT_W = 68.0f;
const float GROUND_OB_HIT_H = 52.0f;

//------------------------------------------------------------------------------
// Sprites
//------------------------------------------------------------------------------
Sprite sprL2Bg;
Sprite sprGem[L2_GEM_KINDS];
Sprite sprBlackGem;
Sprite sprMagnet;
Sprite sprNet;
Sprite sprKnifeBox;
Sprite sprKnife;
Sprite sprWoods;
Sprite sprCactus;

//------------------------------------------------------------------------------
// Entities
//------------------------------------------------------------------------------

// A coloured gem. kind picks which of the four pictures it uses.
typedef struct
{
	float worldX;
	float y;
	int   kind;
	int   active;
} Gem;

// Anything that just sits on the road: black gem, woods, cactus, knife box,
// magnet. "high" marks a hazard you must slide under rather than jump over.
typedef struct
{
	float worldX;
	float y;
	int   active;
	int   high;
} Prop;

// The falling net. It runs through the small state machine below.
#define NET_IDLE     0    // waiting further up the road
#define NET_FALLING  1    // dropping from the top of the screen
#define NET_TRAPPED  2    // landed on the player, waiting for F
#define NET_CUT      3    // being slashed apart
#define NET_INACTIVE 4    // finished with

typedef struct
{
	float worldX;     // the fixed spot on the road where this net hangs
	float y;          // height of the net's LOWER edge while it falls
	int   state;
	int   cutHits;    // how many F presses have landed so far
	float cutTimer;
} Net;

Gem  l2Gem     [L2_GEM_COUNT];
Prop l2BlackGem[L2_BLACKGEM_COUNT];
Prop l2Woods   [L2_WOODS_COUNT];
Prop l2Cactus  [L2_CACTUS_COUNT];
Prop l2KnifeBox[L2_KNIFEBOX_COUNT];
Prop l2Magnet  [L2_MAGNET_COUNT];
Net  l2Net     [L2_NET_COUNT];

//------------------------------------------------------------------------------
// Run state
//------------------------------------------------------------------------------
float l2ScrollX;          // distance travelled along the road
int   l2Score;
int   l2GemsTaken;

int   hasKnife;           // set by a knife box, spent cutting a net
int   playerTrapped;      // held still by a net

int   magnetActive;
float magnetTimer;        // seconds of magnet left

int   isSlowed;
float slowTimer;          // seconds of slowdown left

float l2HitFlash;         // brief tint after a woods/cactus knock
char *l2LoseReason;       // printed on the lose screen

DWORD l2PrevTick;         // real-time delta source, same idea as Level 01

//==============================================================================
//  Level 02 asset loading
//==============================================================================

// Must run AFTER iInitialize() - textures need a live OpenGL context.
//
// The Level 02 artwork comes in two flavours of backdrop, so it needs the two
// keying modes from spriteLoader.h:
//
//   * the gems, magnet and net were exported over the light grey / white
//     transparency chequerboard, so a brightness key lifts them out;
//   * the woods, cactus and knife box are studio photographs on a grey sweep,
//     which only the near-colourless key can remove without eating the object.
void loadLevel02Assets()
{
	sprL2Bg = loadSpritePlain("level02\\level02bg.jpg");

	// Coloured gems, in the order the level cycles through them.
	sprGem[0] = loadSpriteBright("level02\\blue gen.jpg",   225);
	sprGem[1] = loadSpriteBright("level02\\red gem.jpg",    225);
	sprGem[2] = loadSpriteBright("level02\\yellow gwm.png", 193);
	sprGem[3] = loadSpriteBright("level02\\colored.jpg",    225);

	sprBlackGem = loadSpriteBright("level02\\blackgem.jpg", 225);
	sprMagnet   = loadSpriteBright("level02\\magnet.png",   225);
	sprNet      = loadSpriteBright("level02\\net.png",      225);

	// knief.png already carries a real alpha channel, so it loads untouched.
	sprKnife = loadSpritePlain("level02\\knief.png");

	sprWoods    = loadSpriteGrey("level02\\woods.jpg",    10, 80, 200);
	sprCactus   = loadSpriteGrey("level02\\cactus.jpg",   10, 80, 200);
	sprKnifeBox = loadSpriteGrey("level02\\kniefbox.jpg", 10, 80, 200);
}

//==============================================================================
//  Level 02 setup
//
//  Everything is laid out once, from fixed formulas, so a restart always deals
//  the same fair course and the object counts can never drift.
//==============================================================================

void resetLevel02()
{
	// The 16 road hazards, evenly spaced. The pattern is fixed so the run is
	// repeatable: W = woods, C = cactus, B = black gem.
	static const char obKind[L2_OB_TOTAL] =
		{ 'W','C','B','W','B','C','W','C','B','W','B','C','W','C','B','B' };

	int i, w = 0, c = 0, b = 0, trail, k;

	l2ScrollX     = 0.0f;
	l2Score       = 0;
	l2GemsTaken   = 0;

	hasKnife      = 0;
	playerTrapped = 0;

	magnetActive  = 0;
	magnetTimer   = 0.0f;

	isSlowed      = 0;
	slowTimer     = 0.0f;

	l2HitFlash    = 0.0f;
	l2LoseReason  = "";

	// the shared player state, reused exactly as Level 01 leaves it
	playerY   = (float)GROUND_Y;
	playerVY  = 0.0f;
	isJumping = 0;
	isSliding = 0;
	runFrame  = 0;

	// --- road hazards -------------------------------------------------------
	for (i = 0; i < L2_OB_TOTAL; i++)
	{
		float x = L2_OB_FIRST + i * L2_OB_STEP;

		if (obKind[i] == 'W' && w < L2_WOODS_COUNT)
		{
			l2Woods[w].worldX = x;
			l2Woods[w].y      = (float)GROUND_Y;
			l2Woods[w].high   = 0;
			l2Woods[w].active = 1;
			w++;
		}
		else if (obKind[i] == 'C' && c < L2_CACTUS_COUNT)
		{
			l2Cactus[c].worldX = x;
			l2Cactus[c].y      = (float)GROUND_Y;
			l2Cactus[c].high   = 0;
			l2Cactus[c].active = 1;
			c++;
		}
		else if (obKind[i] == 'B' && b < L2_BLACKGEM_COUNT)
		{
			// Every other black gem floats at head height: that one has to be
			// slid under, the rest are jumped over.
			l2BlackGem[b].high   = (b % 2 == 1);
			l2BlackGem[b].worldX = x;
			l2BlackGem[b].y      = l2BlackGem[b].high ? L2_HIGH_Y : (float)GROUND_Y + 4.0f;
			l2BlackGem[b].active = 1;
			b++;
		}
	}

	// --- pickups, dropped into the gaps between hazards ----------------------
	// Each knife box comes well before the net it is needed for.
	l2KnifeBox[0].worldX = 2040.0f;
	l2KnifeBox[1].worldX = 7480.0f;
	for (i = 0; i < L2_KNIFEBOX_COUNT; i++)
	{
		l2KnifeBox[i].y      = (float)GROUND_Y;
		l2KnifeBox[i].high   = 0;
		l2KnifeBox[i].active = 1;
	}

	l2Magnet[0].worldX = 4080.0f;
	l2Magnet[1].worldX = 9520.0f;
	for (i = 0; i < L2_MAGNET_COUNT; i++)
	{
		l2Magnet[i].y      = GROUND_Y + 30.0f;
		l2Magnet[i].high   = 0;
		l2Magnet[i].active = 1;
	}

	// --- nets ---------------------------------------------------------------
	l2Net[0].worldX = 3400.0f;      // after knife box 0 at 2040
	l2Net[1].worldX = 8840.0f;      // after knife box 1 at 7480
	for (i = 0; i < L2_NET_COUNT; i++)
	{
		l2Net[i].y        = (float)SCREEN_H;
		l2Net[i].state    = NET_IDLE;
		l2Net[i].cutHits  = 0;
		l2Net[i].cutTimer = 0.0f;
	}

	// --- gems: six trails of six, sitting in clear gaps ----------------------
	//
	// A trail is 6 gems at 76 pixels apart, so 380 wide. Each start is picked
	// so the trail sits centred in the gap between two hazards: hazard i is at
	// 1700 + i*680, so the midpoints are 2040, 2720, ... and a trail centred
	// there clears the hazards either side by 150 pixels. Getting this wrong
	// once put a gem trail straight on top of a black gem.
	{
		static const float trailAt[6] =
			{ 1000.0f, 2530.0f, 4570.0f, 5930.0f, 7970.0f, 10010.0f };

		for (trail = 0; trail < 6; trail++)
		{
			for (k = 0; k < 6; k++)
			{
				int idx = trail * 6 + k;
				l2Gem[idx].worldX = trailAt[trail] + k * 76.0f;
				// Chest height, so a gem trail is swept up just by running.
				l2Gem[idx].y      = GROUND_Y + 52.0f;
				l2Gem[idx].kind   = idx % L2_GEM_KINDS;
				l2Gem[idx].active = 1;
			}
		}
	}

	l2PrevTick = GetTickCount();
}

//==============================================================================
//  Level 02 transitions and music
//==============================================================================

// SOUND.mp3, started once on entry and stopped on the way out. Because it is
// always stopped before being started again, a restart can never leave two
// copies playing over each other.
void startLevel02Music()
{
	audioStop("lvl2bgm");
	audioPlayLoop("lvl2bgm");
}

void stopLevel02Music()
{
	audioStop("lvl2bgm");
}

void startLevel02()
{
	audioStop("introsong");
	introMusicPlaying = 0;

	resetLevel02();

	gameState = LEVEL_02;
	iResumeTimer(animTimerId);

	startLevel02Music();
}

void endLevel02Win()
{
	gameState = LEVEL_02_WIN;
	iPauseTimer(animTimerId);

	stopLevel02Music();
	audioPlayOnce("winsnd");

	recordLevelResult(2, 1, l2Score);     // Completed
}

void endLevel02Lose(char *reason)
{
	gameState    = LEVEL_02_LOSE;
	l2LoseReason = reason;
	iPauseTimer(animTimerId);

	stopLevel02Music();
	audioPlayOnce("losesnd");

	recordLevelResult(2, 0, l2Score);     // Not Completed
}

// Backing out of Level 02 lands on the level select screen, with the menu
// narration picked back up and every Level 02 sound silenced.
void leaveLevel02()
{
	stopLevel02Music();
	audioStop("winsnd");
	audioStop("losesnd");
	iPauseTimer(animTimerId);

	openLevelSelect();
	startIntroMusic();
}

//==============================================================================
//  Level 02 update
//==============================================================================

// The player's collision box. Narrower than Level 01's, because Level 02's
// obstacles are much wider: the time an obstacle spends overlapping the player
// is (player width + obstacle width) / speed, so trimming the player's own box
// is what buys back enough airtime to make the jump land.
void level02PlayerBox(float *bx, float *by, float *bw, float *bh)
{
	*bx = PLAYER_X + 32.0f;
	*by = playerY + 8.0f;
	*bw = PLAYER_W - 64.0f;
	*bh = isSliding ? (PLAYER_H * 0.45f) : (PLAYER_H - 16.0f);
}

// The top of the character - what a falling net has to come down and touch.
float level02PlayerHeadY()
{
	float bx, by, bw, bh;
	level02PlayerBox(&bx, &by, &bw, &bh);
	return by + bh;
}

// Jump and slide. Same gravity and same code as Level 01, with Level 02's
// taller launch speed.
void updatePlayerLevel02(double dt)
{
	if (!isJumping &&
	    (isKeyPressed('w') || isKeyPressed(' ') || isSpecialKeyPressed(GLUT_KEY_UP)))
	{
		isJumping = 1;
		playerVY  = L2_JUMP_V;
	}

	isSliding = (!isJumping &&
	             (isKeyPressed('s') || isSpecialKeyPressed(GLUT_KEY_DOWN)));

	if (isJumping)
	{
		playerVY += GRAVITY * (float)dt;
		playerY  += playerVY * (float)dt;

		if (playerY <= GROUND_Y)
		{
			playerY   = (float)GROUND_Y;
			playerVY  = 0.0f;
			isJumping = 0;
		}
	}
}

// Coloured gems: collect on touch, and drift toward the player while a magnet
// is running. Only this function ever moves a gem, which is why the magnet can
// never drag a black gem, a net or an obstacle.
void updateGems(double dt)
{
	float bx, by, bw, bh;
	float px, py;
	int   i;

	level02PlayerBox(&bx, &by, &bw, &bh);
	px = bx + bw / 2.0f;
	py = by + bh / 2.0f;

	for (i = 0; i < L2_GEM_COUNT; i++)
	{
		float sx;

		if (!l2Gem[i].active) continue;

		sx = l2Gem[i].worldX - l2ScrollX;

		if (sx < -GEM_SIZE) { l2Gem[i].active = 0; continue; }   // missed

		// --- magnet pull -------------------------------------------------
		if (magnetActive)
		{
			float dx = px - sx;
			float dy = py - (l2Gem[i].y + GEM_SIZE / 2.0f);
			float d  = (float)sqrt(dx * dx + dy * dy);

			if (d <= MAGNET_RADIUS && d > 0.5f)
			{
				// Smooth travel, never a teleport.
				float step = MAGNET_PULL * (float)dt;
				if (step > d) step = d;

				l2Gem[i].worldX += (dx / d) * step;
				l2Gem[i].y      += (dy / d) * step;
				sx = l2Gem[i].worldX - l2ScrollX;
			}
		}

		// --- collection ---------------------------------------------------
		if (boxesOverlap(bx, by, bw, bh,
		                 sx - GEM_SIZE / 2.0, l2Gem[i].y, GEM_SIZE, GEM_SIZE))
		{
			l2Gem[i].active = 0;          // gone for good, cannot re-score
			l2GemsTaken++;
			l2Score += GEM_SCORE;
			audioPlayOnce("pickupsnd");
		}
	}
}

// Black gems are hazards, never collectables, and nothing ever moves them.
void updateBlackGems()
{
	float bx, by, bw, bh;
	int   i;

	level02PlayerBox(&bx, &by, &bw, &bh);

	for (i = 0; i < L2_BLACKGEM_COUNT; i++)
	{
		float sx;

		if (!l2BlackGem[i].active) continue;

		sx = l2BlackGem[i].worldX - l2ScrollX;
		if (sx < -BLACKGEM_SIZE) { l2BlackGem[i].active = 0; continue; }

		if (boxesOverlap(bx, by, bw, bh,
		                 sx - BLACKGEM_SIZE * BLACKGEM_HIT_SCALE / 2.0,
		                 l2BlackGem[i].y,
		                 BLACKGEM_SIZE * BLACKGEM_HIT_SCALE,
		                 BLACKGEM_SIZE * BLACKGEM_HIT_SCALE))
		{
			endLevel02Lose("YOU TOUCHED A BLACK GEM");
			return;
		}
	}
}

// Starts, or refreshes, the five second slowdown. It is a flag plus a timer,
// never a repeated multiply, so two knocks in a row cannot stack into a crawl.
void applySlowdown()
{
	isSlowed   = 1;
	slowTimer  = L2_SLOW_DURATION;    // a fresh knock simply resets the clock
	l2HitFlash = 0.3f;
	audioPlayOnce("bombsnd");
}

// Woods and cactus behave identically, so one routine serves both.
void updateGroundObstacles(Prop *list, int count, int size)
{
	float bx, by, bw, bh;
	int   i;

	level02PlayerBox(&bx, &by, &bw, &bh);

	for (i = 0; i < count; i++)
	{
		float sx;

		if (!list[i].active) continue;

		sx = list[i].worldX - l2ScrollX;
		if (sx < -size) { list[i].active = 0; continue; }

		// A slab at the foot of the picture, not the whole drawn image. Only a
		// real overlap counts - being merely near the obstacle does nothing.
		if (boxesOverlap(bx, by, bw, bh,
		                 sx - GROUND_OB_HIT_W / 2.0, list[i].y,
		                 GROUND_OB_HIT_W, GROUND_OB_HIT_H))
		{
			list[i].active = 0;      // consumed, so it cannot slow you twice
			applySlowdown();
		}
	}
}

void updateWoods()  { updateGroundObstacles(l2Woods,  L2_WOODS_COUNT,  GROUND_OB_SIZE); }
void updateCactus() { updateGroundObstacles(l2Cactus, L2_CACTUS_COUNT, GROUND_OB_SIZE); }

// Knife boxes. One box, one knife, and the box disappears on contact.
void updateKnifeBox()
{
	float bx, by, bw, bh;
	int   i;

	level02PlayerBox(&bx, &by, &bw, &bh);

	for (i = 0; i < L2_KNIFEBOX_COUNT; i++)
	{
		float sx;

		if (!l2KnifeBox[i].active) continue;

		sx = l2KnifeBox[i].worldX - l2ScrollX;
		if (sx < -KNIFEBOX_SIZE) { l2KnifeBox[i].active = 0; continue; }

		if (boxesOverlap(bx, by, bw, bh,
		                 sx - KNIFEBOX_SIZE * 0.32, l2KnifeBox[i].y,
		                 KNIFEBOX_SIZE * 0.64, KNIFEBOX_SIZE * 0.60))
		{
			hasKnife = 1;
			l2KnifeBox[i].active = 0;
			audioPlayOnce("pickupsnd");
		}
	}
}

// Magnet pickups and the countdown that follows.
void updateMagnet(double dt)
{
	float bx, by, bw, bh;
	int   i;

	// countdown first
	if (magnetActive)
	{
		magnetTimer -= (float)dt;
		if (magnetTimer <= 0.0f)
		{
			magnetTimer  = 0.0f;
			magnetActive = 0;            // attraction stops here
		}
	}

	level02PlayerBox(&bx, &by, &bw, &bh);

	for (i = 0; i < L2_MAGNET_COUNT; i++)
	{
		float sx;

		if (!l2Magnet[i].active) continue;

		sx = l2Magnet[i].worldX - l2ScrollX;
		if (sx < -MAGNET_SIZE) { l2Magnet[i].active = 0; continue; }

		if (boxesOverlap(bx, by, bw, bh,
		                 sx - MAGNET_SIZE * 0.34, l2Magnet[i].y,
		                 MAGNET_SIZE * 0.68, MAGNET_SIZE * 0.68))
		{
			// Picking up a second magnet refreshes the eight seconds rather
			// than starting a second overlapping timer.
			magnetActive = 1;
			magnetTimer  = MAGNET_DURATION;

			l2Magnet[i].active = 0;
			audioPlayOnce("pickupsnd");
		}
	}
}

// The falling net, and the trap it creates.
//
//   IDLE -> FALLING -> (knife ? TRAPPED : lose) -> CUT -> INACTIVE
//
// The net never disappears on its own just because a knife is held; the player
// has to press F while trapped, and that press is what spends the knife.
void updateNets(double dt)
{
	float bx, by, bw, bh, sx;
	int   i;

	level02PlayerBox(&bx, &by, &bw, &bh);

	for (i = 0; i < L2_NET_COUNT; i++)
	{
		Net *n = &l2Net[i];

		if (n->state == NET_INACTIVE) continue;

		sx = n->worldX - l2ScrollX;      // the net's own spot on the road

		if (n->state == NET_IDLE)
		{
			// The drop begins once the net's spot comes into view. This does
			// not look at hasKnife at all - a net falls either way.
			if (sx <= NET_TRIGGER_SX)
			{
				n->state = NET_FALLING;
				n->y     = (float)SCREEN_H;
			}
			continue;
		}

		if (n->state == NET_FALLING)
		{
			n->y -= NET_FALL_SPEED * (float)dt;

			// A real box-against-box test between the netting's lower edge and
			// the character. Being lined up horizontally is not enough: the
			// net's bottom has to have actually come down onto the body.
			if (boxesOverlap(bx, by, bw, bh,
			                 sx - NET_HIT_W / 2.0, n->y, NET_HIT_W, NET_HIT_H))
			{
				if (hasKnife)
				{
					n->state      = NET_TRAPPED;
					n->cutHits    = 0;
					playerTrapped = 1;
					audioPlayOnce("bombsnd");
				}
				else
				{
					endLevel02Lose("CAUGHT IN THE NET WITH NO KNIFE");
					return;
				}
				continue;
			}

			// Outrun: the character got past the spot before the net was low
			// enough. It drops harmlessly behind and is finished with.
			if (sx < -NET_SIZE || n->y < -NET_HIT_H)
				n->state = NET_INACTIVE;

			continue;
		}

		if (n->state == NET_CUT)
		{
			n->cutTimer -= (float)dt;
			if (n->cutTimer <= 0.0f)
			{
				n->state      = NET_INACTIVE;
				playerTrapped = 0;       // free to run again
			}
			continue;
		}

		// NET_TRAPPED just waits; the F key handler below releases it.
	}
}

// Called from fixedUpdate() when F goes down. It does nothing at all unless the
// player is actually held by a net and actually owns a knife.
void tryCutNet()
{
	int i;

	if (gameState != LEVEL_02) return;
	if (!playerTrapped)        return;
	if (!hasKnife)             return;

	for (i = 0; i < L2_NET_COUNT; i++)
	{
		if (l2Net[i].state == NET_TRAPPED)
		{
			// One press, one slash. The caller only calls this on the frame F
			// goes down, so holding F does nothing after the first count.
			l2Net[i].cutHits++;
			audioPlayOnce("pickupsnd");

			// The netting only parts once it has been cut through.
			if (l2Net[i].cutHits >= NET_REQUIRED_HITS)
			{
				l2Net[i].state    = NET_CUT;
				l2Net[i].cutTimer = NET_CUT_TIME;

				hasKnife = 0;             // the knife is spent on the cut
			}
			return;
		}
	}
}

void checkLevel02Finish()
{
	float bx, by, bw, bh;
	float fx = L2_FINISH_X - l2ScrollX;

	level02PlayerBox(&bx, &by, &bw, &bh);

	if (boxesOverlap(bx, by, bw, bh, fx - 12, 0, 24, SCREEN_H))
		endLevel02Win();
}

void updateLevel02()
{
	DWORD now;
	double dt;
	float  speed;

	// --- real elapsed time, same approach as Level 01 ---
	now = GetTickCount();
	dt  = (now - l2PrevTick) / 1000.0;
	l2PrevTick = now;

	if (dt > 0.10) dt = 0.10;
	if (dt <= 0.0) return;

	if (l2HitFlash > 0.0f) l2HitFlash -= (float)dt;

	// --- the five second slowdown clock ---
	if (isSlowed)
	{
		slowTimer -= (float)dt;
		if (slowTimer <= 0.0f)
		{
			slowTimer = 0.0f;
			isSlowed  = 0;              // speed returns to normal, never sooner
		}
	}

	// The magnet and the net both keep working while trapped is decided below,
	// so update the net first: it owns whether the world is allowed to move.
	updateNets(dt);
	if (gameState != LEVEL_02) return;   // the net may have ended the level

	if (playerTrapped)
	{
		// Held fast: the road stops, and no pickup or hazard can reach us.
		return;
	}

	updatePlayerLevel02(dt);

	// One speed, chosen fresh every frame from three states. Sprint wins over
	// a slowdown, and neither is ever written back into the base speed, so a
	// boost can never leak past the moment the key is released.
	//
	// Note this is only reached when NOT trapped: the early return above means
	// holding RIGHT cannot pull the character out of a net.
	if (isKeyPressed('d') || isKeyPressed('D') || isSpecialKeyPressed(GLUT_KEY_RIGHT))
		speed = L2_BASE_SCROLL * L2_BOOST_MULT;      // sprint
	else if (isSlowed)
		speed = L2_BASE_SCROLL * L2_SLOW_MULT;       // knocked by woods/cactus
	else
		speed = L2_BASE_SCROLL;                      // normal

	l2ScrollX += speed * (float)dt;

	updateMagnet(dt);
	updateGems(dt);

	updateKnifeBox();
	updateWoods();
	updateCactus();

	updateBlackGems();
	if (gameState != LEVEL_02) return;   // a black gem ends the level instantly

	checkLevel02Finish();
}

//==============================================================================
//  Level 02 drawing
//==============================================================================

// One forest image, scrolled and repeated. The dark band over the join hides
// the seam where the picture's right edge meets its own left edge.
void drawLevel02Background()
{
	float offset = (float)fmod((double)l2ScrollX, (double)SEG_W);
	float joinX  = SEG_W - offset;
	float half   = 155.0f;
	int   i;

	iShowImage((int)(-offset), 0, SEG_W, SCREEN_H, sprL2Bg.tex);
	iShowImage((int)(SEG_W - offset), 0, SEG_W, SCREEN_H, sprL2Bg.tex);

	for (i = 0; i < 2; i++)
	{
		float x0 = (i == 0) ? (joinX - half) : joinX;
		float a0 = (i == 0) ? 0.0f : 0.68f;
		float a1 = (i == 0) ? 0.68f : 0.0f;

		glBegin(GL_QUADS);
			glColor4d(0.05, 0.10, 0.04, a0);
			glVertex2d(x0, 0);
			glVertex2d(x0, SCREEN_H);
			glColor4d(0.05, 0.10, 0.04, a1);
			glVertex2d(x0 + half, SCREEN_H);
			glVertex2d(x0 + half, 0);
		glEnd();
	}
	glColor4d(1.0, 1.0, 1.0, 1.0);
}

void drawLevel02Finish()
{
	float fx = L2_FINISH_X - l2ScrollX;
	int   row, col;

	if (fx < -140 || fx > SCREEN_W + 140) return;

	iSetColor(245, 245, 250);
	iFilledRectangle(fx - 62, GROUND_Y - 10, 10, 330);
	iFilledRectangle(fx + 52, GROUND_Y - 10, 10, 330);

	for (row = 0; row < 4; row++)
	{
		for (col = 0; col < 6; col++)
		{
			if ((row + col) % 2 == 0) iSetColor(250, 250, 250);
			else                      iSetColor(25, 25, 30);
			iFilledRectangle(fx - 52 + col * 17, GROUND_Y + 246 - row * 17, 17, 17);
		}
	}

	iSetColor(40, 90, 40);
	iTextCentered(fx, GROUND_Y + 330, "FINISH", GLUT_BITMAP_HELVETICA_18);
}

// Draws a sprite so its BOTTOM edge lands on bottomY. Props store the bottom
// of their collision box, so this is what makes a big obstacle sit on the
// ground instead of hovering above it.
void drawSpriteBottom(Sprite s, double cx, double bottomY, double boxSize)
{
	double scale, dh;

	if (s.tex == 0 || s.w <= 0 || s.h <= 0) return;

	scale = (s.w >= s.h) ? (boxSize / s.w) : (boxSize / s.h);
	dh    = s.h * scale;

	drawSpriteFit(s, cx, bottomY + dh / 2.0, boxSize);
}

// Draws every member of a Prop list that is on screen.
void drawPropList(Prop *list, int count, Sprite spr, int size)
{
	int i;
	for (i = 0; i < count; i++)
	{
		float sx;
		if (!list[i].active) continue;
		sx = list[i].worldX - l2ScrollX;
		if (sx < -size || sx > SCREEN_W + size) continue;
		drawSpriteBottom(spr, sx, list[i].y, size);
	}
}

void drawLevel02Hud()
{
	char buf[128];

	iFilledRectangleAlpha(0, SCREEN_H - 58, SCREEN_W, 58, 8, 14, 8, 0.62);

	iSetColor(255, 205, 80);
	sprintf(buf, "SCORE: %d", l2Score);
	iText(24, SCREEN_H - 38, buf, GLUT_BITMAP_HELVETICA_18);

	iSetColor(190, 235, 255);
	sprintf(buf, "GEMS: %d", l2GemsTaken);
	iText(210, SCREEN_H - 38, buf, GLUT_BITMAP_HELVETICA_18);

	// --- knife, with the picture beside the word when you own one ---
	if (hasKnife)
	{
		iSetColor(120, 235, 140);
		iText(370, SCREEN_H - 38, "KNIFE: YES", GLUT_BITMAP_HELVETICA_18);
		drawSpriteFit(sprKnife, 510, SCREEN_H - 31, KNIFE_ICON);
	}
	else
	{
		iSetColor(160, 160, 170);
		iText(370, SCREEN_H - 38, "KNIFE: NO", GLUT_BITMAP_HELVETICA_18);
	}

	// --- magnet, only while it is running ---
	if (magnetActive)
	{
		iSetColor(255, 140, 90);
		sprintf(buf, "MAGNET: %ds", (int)(magnetTimer + 0.999f));
		iText(560, SCREEN_H - 38, buf, GLUT_BITMAP_HELVETICA_18);
		drawSpriteFit(sprMagnet, 706, SCREEN_H - 31, 30);
	}

	// --- slowdown warning ---
	if (isSlowed)
	{
		iSetColor(255, 120, 110);
		sprintf(buf, "SLOWED: %ds", (int)(slowTimer + 0.999f));
		iText(760, SCREEN_H - 38, buf, GLUT_BITMAP_HELVETICA_18);
	}

	// --- how far along the road ---
	{
		float pct = l2ScrollX / (float)L2_FINISH_X;
		if (pct < 0) pct = 0;
		if (pct > 1) pct = 1;

		iFilledRectangleAlpha(SCREEN_W - 230, SCREEN_H - 52, 200, 8, 40, 50, 40, 0.9);
		iFilledRectangleAlpha(SCREEN_W - 230, SCREEN_H - 52, 200 * pct, 8, 235, 200, 90, 0.95);

		iSetColor(180, 190, 180);
		iText(SCREEN_W - 230, SCREEN_H - 38, "PROGRESS", GLUT_BITMAP_HELVETICA_12);
	}
}

void drawLevel02()
{
	int i;

	drawLevel02Background();
	drawLevel02Finish();

	// pickups and hazards
	drawPropList(l2Woods,    L2_WOODS_COUNT,    sprWoods,    GROUND_OB_SIZE);
	drawPropList(l2Cactus,   L2_CACTUS_COUNT,   sprCactus,   GROUND_OB_SIZE);
	drawPropList(l2KnifeBox, L2_KNIFEBOX_COUNT, sprKnifeBox, KNIFEBOX_SIZE);
	drawPropList(l2Magnet,   L2_MAGNET_COUNT,   sprMagnet,   MAGNET_SIZE);
	drawPropList(l2BlackGem, L2_BLACKGEM_COUNT, sprBlackGem, BLACKGEM_SIZE);

	// coloured gems
	for (i = 0; i < L2_GEM_COUNT; i++)
	{
		float sx;
		if (!l2Gem[i].active) continue;
		sx = l2Gem[i].worldX - l2ScrollX;
		if (sx < -GEM_SIZE || sx > SCREEN_W + GEM_SIZE) continue;
		drawSpriteFit(sprGem[l2Gem[i].kind], sx, l2Gem[i].y + GEM_SIZE / 2.0, GEM_SIZE);
	}

	// the character
	if (isSliding)
		iShowImage(PLAYER_X, (int)playerY, PLAYER_W, (int)(PLAYER_H * 0.55),
		           sprRun[runFrame].tex);
	else
		iShowImage(PLAYER_X, (int)playerY, PLAYER_W, PLAYER_H, sprRun[runFrame].tex);

	// nets, drawn at their own spot on the road and over the character
	for (i = 0; i < L2_NET_COUNT; i++)
	{
		float nx;

		if (l2Net[i].state == NET_IDLE || l2Net[i].state == NET_INACTIVE)
			continue;

		nx = l2Net[i].worldX - l2ScrollX;
		if (nx < -NET_SIZE || nx > SCREEN_W + NET_SIZE) continue;

		if (l2Net[i].state == NET_CUT)
		{
			// the two halves falling apart
			drawSpriteBottom(sprNet, nx - 60, l2Net[i].y - 20, NET_SIZE * 0.6);
			drawSpriteBottom(sprNet, nx + 60, l2Net[i].y - 20, NET_SIZE * 0.6);
		}
		else
		{
			drawSpriteBottom(sprNet, nx, l2Net[i].y, NET_SIZE);
		}
	}

	// "NET INCOMING" while one is on its way down and still unresolved
	for (i = 0; i < L2_NET_COUNT; i++)
	{
		if (l2Net[i].state != NET_FALLING) continue;
		if (l2Net[i].worldX - l2ScrollX > NET_TRIGGER_SX) continue;

		iFilledRectangleAlpha(SCREEN_W / 2.0 - 190, 596, 380, 46, 40, 12, 8, 0.80);
		iSetColor(255, 170, 70);
		iRectangle(SCREEN_W / 2.0 - 190, 596, 380, 46);
		iTextCentered(SCREEN_W / 2.0, 620, "NET INCOMING!  Hold RIGHT to sprint clear",
		              GLUT_BITMAP_HELVETICA_12);
		break;
	}

	if (l2HitFlash > 0.0f)
		iFilledRectangleAlpha(0, 0, SCREEN_W, SCREEN_H, 220, 90, 40, l2HitFlash * 0.6);

	// The F prompt, shown only while actually held by a net. It carries the
	// cut counter so the player can see the progress each press makes.
	if (playerTrapped)
	{
		char buf[64];
		int  hits = 0;
		int  k;

		for (k = 0; k < L2_NET_COUNT; k++)
			if (l2Net[k].state == NET_TRAPPED) hits = l2Net[k].cutHits;

		iFilledRectangleAlpha(390, 452, 500, 126, 10, 14, 10, 0.90);
		iSetColor(255, 205, 80);
		iRectangle(390, 452, 500, 126);

		iSetColor(255, 205, 80);
		iTextCentered(SCREEN_W / 2.0, 546, "PRESS F TO CUT THE NET",
		              GLUT_BITMAP_HELVETICA_18);

		sprintf(buf, "CUT:  %d / %d", hits, NET_REQUIRED_HITS);
		iSetColor(230, 230, 235);
		iTextCentered(SCREEN_W / 2.0, 512, buf, GLUT_BITMAP_TIMES_ROMAN_24);

		// a small bar of the same progress
		{
			double segW = 90.0, gap = 12.0;
			double total = NET_REQUIRED_HITS * segW + (NET_REQUIRED_HITS - 1) * gap;
			double x0 = SCREEN_W / 2.0 - total / 2.0;

			for (k = 0; k < NET_REQUIRED_HITS; k++)
			{
				if (k < hits) iFilledRectangleAlpha(x0 + k * (segW + gap), 476, segW, 14,
				                                    120, 230, 140, 0.95);
				else          iFilledRectangleAlpha(x0 + k * (segW + gap), 476, segW, 14,
				                                    60, 66, 60, 0.85);
			}
		}
	}

	drawLevel02Hud();

	// The forest floor is busy and mid-toned, so the hint gets its own dark
	// strip to sit on rather than relying on colour alone.
	iFilledRectangleAlpha(0, 0, 560, 30, 8, 14, 8, 0.55);
	iSetColor(215, 230, 210);
	iText(24, 10, "W / UP jump    S / DOWN slide    RIGHT sprint    F cut net    ESC menu",
	      GLUT_BITMAP_HELVETICA_12);
}

//------------------------------------------------------------------------------
// Level 02 result screen, styled to match the Level 01 one.
//------------------------------------------------------------------------------
void drawLevel02Result(int didWin)
{
	char buf[128];
	int  y;

	drawLevel02();
	dimScreen(0.78);

	iFilledRectangleAlpha(340, 200, 600, 330, 10, 14, 10, 0.90);

	if (didWin) iSetColor(90, 210, 110);
	else        iSetColor(225, 60, 60);
	iRectangle(340, 200, 600, 330);

	y = 470;

	if (didWin)
	{
		iSetColor(90, 220, 120);
		iTextCentered(SCREEN_W / 2.0, y, "YOU WIN!", GLUT_BITMAP_TIMES_ROMAN_24);
		y -= 34;
		iSetColor(220, 220, 230);
		iTextCentered(SCREEN_W / 2.0, y, "LEVEL 02 COMPLETED", GLUT_BITMAP_HELVETICA_18);
	}
	else
	{
		iSetColor(235, 70, 70);
		iTextCentered(SCREEN_W / 2.0, y, "YOU LOSE", GLUT_BITMAP_TIMES_ROMAN_24);
		y -= 34;
		iSetColor(220, 220, 230);
		iTextCentered(SCREEN_W / 2.0, y, l2LoseReason, GLUT_BITMAP_HELVETICA_18);
	}

	y -= 46;
	iSetColor(90, 90, 105);
	iLine(400, y + 16, 880, y + 16);

	iSetColor(255, 205, 80);
	sprintf(buf, "Score: %d", l2Score);
	iTextCentered(SCREEN_W / 2.0, y, buf, GLUT_BITMAP_HELVETICA_18);

	y -= 30;
	iSetColor(220, 220, 230);
	sprintf(buf, "Gems Collected: %d / %d", l2GemsTaken, L2_GEM_COUNT);
	iTextCentered(SCREEN_W / 2.0, y, buf, GLUT_BITMAP_HELVETICA_18);

	y -= 56;
	iSetColor(170, 170, 185);

	if (didWin)
		iTextCentered(SCREEN_W / 2.0, y, "Press ENTER to continue", GLUT_BITMAP_HELVETICA_12);
	else
		iTextCentered(SCREEN_W / 2.0, y, "Press R to Restart", GLUT_BITMAP_HELVETICA_12);

	y -= 20;
	iTextCentered(SCREEN_W / 2.0, y, "Press ESC for Level Select", GLUT_BITMAP_HELVETICA_12);
}

//==============================================================================
//  LEVEL 03  -  THE EAGLE CHASE
//
//  Same skeleton as the other two levels: one scrolling world measured in
//  pixels, the character pinned to a fixed screen x, the shared run frames,
//  the shared gravity, the shared AABB test and the shared MCI audio wrapper.
//  What is new is the egg that wakes the eagle and the smoke box that blinds it.
//==============================================================================

//------------------------------------------------------------------------------
// Tunables
//------------------------------------------------------------------------------
const float L3_BASE_SCROLL  = 200.0f;   // normal running speed, pixels/second
const float L3_CHASE_SCROLL = 340.0f;   // the sprint the egg forces on you
const float L3_SPRINT_MULT  =   1.25f;  // extra while RIGHT / D is held
const float L3_STUMBLE_MULT =   0.55f;  // after clipping a wood pile
const float L3_STUMBLE_TIME =   1.2f;   // seconds a stumble lasts

// Level 03's own launch speed. Same gravity and same code as the other levels;
// only the number differs, and only inside Level 03. The wood piles are chunky,
// so 980 is what turns the jump from frame perfect into about a third of a
// second of leeway.
const float L3_JUMP_V       = 980.0f;

// Wood piles
#define L3_WOOD_COUNT      6
#define L3_WOOD_SIZE     130
const float L3_WOOD_HIT_W  = 62.0f;     // damage box, far smaller than the art
const float L3_WOOD_HIT_H  = 50.0f;
const float L3_WOOD_GAP_MIN = 700.0f;
const float L3_WOOD_GAP_MAX = 1200.0f;

// Pickups
#define L3_SMOKEBOX_SIZE  86
#define L3_EGG_SIZE       76
// Each round lays a smoke box on the road first and the egg some way after it,
// measured from wherever the runner happens to be when the round begins.
const float L3_SMOKE_AHEAD     = 1800.0f;   // smoke box, ahead of the runner
const float L3_EGG_AFTER_SMOKE = 2200.0f;   // egg, further on again
const float L3_SMOKE_RETRY     = 1200.0f;   // second chance if the box is missed
const float L3_SMOKE_MIN_LEAD  =  700.0f;   // never re-offer it on top of the egg
const float L3_PICKUP_CLEAR  =  170.0f; // keep pickups off the wood piles

// The chase
// ---------------------------------------------------------------------------
//  The strike.
//
//  Taking an egg brings the eagle down on the runner after a fixed, very short
//  count. There is no outrunning it and no wearing it down: the smoke box is
//  the only answer, and it has to be thrown before the count reaches zero.
//
//  The second egg gives half a second less than the first, so the level tightens
//  as it goes.
// ---------------------------------------------------------------------------
#define L3_EGG_COUNT 2                       // two eggs, two strikes

const float EGG_STRIKE_TIME[L3_EGG_COUNT] = { 2.0f, 1.5f };

// Smoke
const float SMOKE_DURATION    =   4.0f; // seconds the screen stays foggy

// Scoring
#define L3_SCORE_EGG    500
#define L3_SCORE_WOOD    25
#define L3_SCORE_ESCAPE 750

//------------------------------------------------------------------------------
// The background tiles almost seamlessly, so it simply repeats. It is drawn
// oversized and pushed down so the painted grass line lands exactly on
// GROUND_Y, which is what makes the character look planted on the road.
//------------------------------------------------------------------------------
#define L3_BG_W  1600
#define L3_BG_H   900
#define L3_BG_Y  (-150)

//------------------------------------------------------------------------------
// Eagle states
//------------------------------------------------------------------------------
#define EAGLE_NONE         0
#define EAGLE_CHASING      1     // holding station behind the runner
#define EAGLE_ATTACK_BACK  2     // closing in from behind
#define EAGLE_ATTACK_FRONT 3     // swung round and diving from the front
#define EAGLE_BLINDED      4     // lost in the smoke
#define EAGLE_ESCAPED      5     // beaten

//------------------------------------------------------------------------------
// Sprites
//------------------------------------------------------------------------------
Sprite sprL3Bg;
Sprite sprWoodPile;
Sprite sprSmokeBox;
Sprite sprEgg;
Sprite sprEagleBack;
Sprite sprEagleFront;
Sprite sprSmokeFx;

//------------------------------------------------------------------------------
// Run state
//------------------------------------------------------------------------------
Prop  l3Wood[L3_WOOD_COUNT];    // reuses Level 02's simple road-object type
float l3WoodSpawnX;

float l3ScrollX;
int   l3Score;
int   l3WoodCleared;

Prop  l3SmokeBox;               // one at a time
Prop  l3Egg;

int   hasSmokeBox;              // a smoke box is in hand
int   smokeActive;              // the screen is currently full of smoke
float smokeTimer;

int   eggCollected;             // an egg has been taken at least once
int   eggsTaken;                // how many of the two eggs are gone
int   eagleChasing;             // a strike is counting down right now
int   eagleState;
float strikeTimer;              // seconds left before the eagle lands the hit
float strikeTotal;              // what that count started at, for the approach
int   smokeRetryUsed;           // one replacement smoke box per round

int   isStumbling;
float stumbleTimer;

DWORD l3PrevTick;

//==============================================================================
//  Level 03 assets
//==============================================================================

// Must run AFTER iInitialize(). Every sprite here sits on a white sheet or on
// the light transparency chequerboard, so the brightness key lifts them out.
// The back eagle needs a lower cut than the rest: its chequerboard is darker
// and JPEG ringing around the squares survives a higher threshold as a visible
// grid. 200 clears that without eating the bird's white head and tail.
void loadLevel03Assets()
{
	sprL3Bg       = loadSpritePlain("level03\\backround.jpg");
	sprWoodPile   = loadSpriteBright("level03\\woodpile.jpg",         240);
	sprSmokeBox   = loadSpriteBright("level03\\smokebox.jpg",         240);
	sprEgg        = loadSpriteBright("level03\\eagle egg.png",        228);
	sprEagleBack  = loadSpriteBright("level03\\eagle from back.jpg",  200);
	sprEagleFront = loadSpriteBright("level03\\eagle from front.jpg", 240);

	// White smoke on black. It is never keyed: it is blended additively, which
	// makes the black contribute nothing and the smoke glow over the scene.
	sprSmokeFx = loadSpritePlain("level03\\smoke effect.jpg");
}

//==============================================================================
//  Level 03 spawning
//==============================================================================

// Nudges a pickup clear of the wood piles so it can never be collected only by
// running into an obstacle.
float l3ClearOfWood(float x)
{
	int i, guard;

	for (guard = 0; guard < 8; guard++)
	{
		int moved = 0;

		for (i = 0; i < L3_WOOD_COUNT; i++)
		{
			if (!l3Wood[i].active) continue;

			if (x > l3Wood[i].worldX - L3_PICKUP_CLEAR &&
			    x < l3Wood[i].worldX + L3_PICKUP_CLEAR)
			{
				x = l3Wood[i].worldX + L3_PICKUP_CLEAR + 60.0f;
				moved = 1;
			}
		}

		if (!moved) break;
	}

	return x;
}

void spawnWoodPile(int i)
{
	l3Wood[i].worldX = l3WoodSpawnX;
	l3Wood[i].y      = (float)GROUND_Y;
	l3Wood[i].high   = 0;
	l3Wood[i].active = 1;

	l3WoodSpawnX += randBetween(L3_WOOD_GAP_MIN, L3_WOOD_GAP_MAX);
}

//==============================================================================
//  Level 03 setup
//==============================================================================

// Lays out one round: a smoke box on the road ahead, then the egg some way
// past it. Because every strike costs a smoke box, each egg gets its own box
// beforehand - without one the strike cannot be survived.
void beginEggRound()
{
	if (eggsTaken >= L3_EGG_COUNT) return;   // both eggs already taken

	smokeRetryUsed = 0;

	l3SmokeBox.worldX = l3ClearOfWood(l3ScrollX + L3_SMOKE_AHEAD);
	l3SmokeBox.y      = GROUND_Y + 16.0f;
	l3SmokeBox.high   = 0;
	l3SmokeBox.active = 1;

	l3Egg.worldX = l3ClearOfWood(l3SmokeBox.worldX + L3_EGG_AFTER_SMOKE);
	l3Egg.y      = GROUND_Y + 10.0f;
	l3Egg.high   = 0;
	l3Egg.active = 1;
}

void resetLevel03()
{
	int i;

	l3ScrollX     = 0.0f;
	l3Score       = 0;
	l3WoodCleared = 0;

	hasSmokeBox   = 0;
	smokeActive   = 0;
	smokeTimer    = 0.0f;

	eggCollected   = 0;
	eggsTaken      = 0;
	eagleChasing   = 0;
	eagleState     = EAGLE_NONE;
	strikeTimer    = 0.0f;
	strikeTotal    = 0.0f;
	smokeRetryUsed = 0;

	isStumbling   = 0;
	stumbleTimer  = 0.0f;

	// the shared player state, reused exactly as the other levels leave it
	playerY   = (float)GROUND_Y;
	playerVY  = 0.0f;
	isJumping = 0;
	isSliding = 0;
	runFrame  = 0;

	// wood piles first, then the pickups can be placed clear of them
	l3WoodSpawnX = 1500.0f;
	for (i = 0; i < L3_WOOD_COUNT; i++) spawnWoodPile(i);

	beginEggRound();                       // lays the first smoke box and egg

	l3PrevTick = GetTickCount();
}

//==============================================================================
//  Level 03 transitions
//==============================================================================

void startLevel03()
{
	audioStop("introsong");
	introMusicPlaying = 0;

	resetLevel03();

	gameState = LEVEL_03;
	iResumeTimer(animTimerId);

	// Reuses the Level 01 track; Level 03 ships no music of its own.
	audioStop("lvl1bgm");
	audioPlayLoop("lvl1bgm");
}

void stopLevel03Music()
{
	audioStop("lvl1bgm");
}

void endLevel03Win()
{
	gameState = LEVEL_03_WIN;
	iPauseTimer(animTimerId);

	l3Score += L3_SCORE_ESCAPE;

	stopLevel03Music();
	audioPlayOnce("winsnd");

	recordLevelResult(3, 1, l3Score);     // Completed
}

void endLevel03Lose()
{
	gameState = LEVEL_03_LOSE;
	iPauseTimer(animTimerId);

	stopLevel03Music();
	audioPlayOnce("losesnd");

	recordLevelResult(3, 0, l3Score);     // Not Completed, but the score stands
}

void leaveLevel03()
{
	stopLevel03Music();
	audioStop("winsnd");
	audioStop("losesnd");
	iPauseTimer(animTimerId);

	openLevelSelect();
	startIntroMusic();
}

//==============================================================================
//  Level 03 update
//==============================================================================

void level03PlayerBox(float *bx, float *by, float *bw, float *bh)
{
	*bx = PLAYER_X + 30.0f;
	*by = playerY + 8.0f;
	*bw = PLAYER_W - 60.0f;
	*bh = PLAYER_H - 16.0f;
}

// Jump only - there is nothing to slide under on this road.
void updatePlayerLevel03(double dt)
{
	if (!isJumping &&
	    (isKeyPressed('w') || isKeyPressed(' ') || isSpecialKeyPressed(GLUT_KEY_UP)))
	{
		isJumping = 1;
		playerVY  = L3_JUMP_V;
	}

	if (isJumping)
	{
		playerVY += GRAVITY * (float)dt;
		playerY  += playerVY * (float)dt;

		if (playerY <= GROUND_Y)
		{
			playerY   = (float)GROUND_Y;
			playerVY  = 0.0f;
			isJumping = 0;
		}
	}
}

// Wood piles roll toward the runner and are recycled once past, so the road
// never runs out of them.
void updateWoodPiles()
{
	float bx, by, bw, bh;
	int   i;

	level03PlayerBox(&bx, &by, &bw, &bh);

	for (i = 0; i < L3_WOOD_COUNT; i++)
	{
		float sx;

		if (!l3Wood[i].active) continue;

		sx = l3Wood[i].worldX - l3ScrollX;

		if (sx < -L3_WOOD_SIZE)
		{
			l3WoodCleared++;                  // jumped, or at least survived
			l3Score += L3_SCORE_WOOD;
			spawnWoodPile(i);
			continue;
		}

		// Only a genuine overlap counts, and only with the slab at the base of
		// the picture rather than the whole drawn image.
		if (boxesOverlap(bx, by, bw, bh,
		                 sx - L3_WOOD_HIT_W / 2.0, l3Wood[i].y,
		                 L3_WOOD_HIT_W, L3_WOOD_HIT_H))
		{
			l3Wood[i].active = 0;             // consumed, cannot trip you twice

			isStumbling  = 1;
			stumbleTimer = L3_STUMBLE_TIME;   // a fresh knock resets the clock
			audioPlayOnce("bombsnd");
		}
	}
}

// The smoke box. Only ever offered before the egg; once the chase is on, no
// more appear.
void updateSmokeBoxes()
{
	float bx, by, bw, bh, sx;

	if (!l3SmokeBox.active) return;

	level03PlayerBox(&bx, &by, &bw, &bh);
	sx = l3SmokeBox.worldX - l3ScrollX;

	if (sx < -L3_SMOKEBOX_SIZE)
	{
		// Missed. One replacement is put out per round, and only if it still
		// lands far enough before the egg to be worth reaching. Run past that
		// one too and the strike will arrive with nothing to throw.
		float retry = l3ClearOfWood(l3SmokeBox.worldX + L3_SMOKE_RETRY);

		if (!smokeRetryUsed && l3Egg.active &&
		    retry < l3Egg.worldX - L3_SMOKE_MIN_LEAD)
		{
			smokeRetryUsed    = 1;
			l3SmokeBox.worldX = retry;
		}
		else
		{
			l3SmokeBox.active = 0;
		}
		return;
	}

	if (boxesOverlap(bx, by, bw, bh,
	                 sx - L3_SMOKEBOX_SIZE * 0.32, l3SmokeBox.y,
	                 L3_SMOKEBOX_SIZE * 0.64, L3_SMOKEBOX_SIZE * 0.64))
	{
		hasSmokeBox = 1;
		l3SmokeBox.active = 0;                // gone from the road
		audioPlayOnce("pickupsnd");
	}
}

// The egg. Collecting it scores once and wakes the eagle.
void updateEggs()
{
	float bx, by, bw, bh, sx;

	if (!l3Egg.active) return;

	level03PlayerBox(&bx, &by, &bw, &bh);
	sx = l3Egg.worldX - l3ScrollX;

	if (sx < -L3_EGG_SIZE)
	{
		// Missed, so the whole round is laid out again further up the road:
		// a fresh smoke box first, then a fresh egg behind it.
		beginEggRound();
		return;
	}

	if (boxesOverlap(bx, by, bw, bh,
	                 sx - L3_EGG_SIZE * 0.30, l3Egg.y,
	                 L3_EGG_SIZE * 0.60, L3_EGG_SIZE * 0.70))
	{
		// active is cleared first, so this can only ever score once.
		l3Egg.active = 0;

		l3Score     += L3_SCORE_EGG;
		eggCollected = 1;

		// The eagle is now coming, and it arrives on a fixed count. The first
		// egg buys 2 seconds, the second only 1.5. It attacks from behind for
		// the first egg and head-on for the second.
		eagleChasing = 1;
		eagleState   = (eggsTaken == 0) ? EAGLE_ATTACK_BACK : EAGLE_ATTACK_FRONT;
		strikeTotal  = EGG_STRIKE_TIME[eggsTaken];
		strikeTimer  = strikeTotal;

		eggsTaken++;

		audioPlayOnce("pickupsnd");
	}
}

// The smoke, once thrown. While it hangs the eagle cannot close at all.
void updateSmokeEffect(double dt)
{
	if (!smokeActive) return;

	smokeTimer -= (float)dt;

	if (smokeTimer <= 0.0f)
	{
		smokeTimer  = 0.0f;
		smokeActive = 0;

		// The bird never found the runner again.
		if (eagleChasing)
		{
			eagleChasing = 0;
			eagleState   = EAGLE_ESCAPED;

			if (eggsTaken >= L3_EGG_COUNT)
			{
				// That was the last egg, so shaking the eagle ends the level.
				endLevel03Win();
			}
			else
			{
				// Survived the first strike. Back to running, and the next
				// smoke box and egg are laid out ahead.
				eagleState = EAGLE_NONE;
				beginEggRound();
			}
		}
	}
}

// The chase itself. The gap is the whole mechanic: it shrinks on its own,
// shrinks faster during a front swoop, and grows while the player sprints.
// The strike. Once an egg is taken this simply runs the clock down, with the
// eagle diving closer every frame. Nothing the runner does with the movement
// keys affects it - only the smoke box stops it.
void updateEagle(double dt)
{
	if (!eagleChasing) return;

	// Blinded. The smoke is doing the work; the strike is already broken off.
	if (smokeActive)
	{
		eagleState = EAGLE_BLINDED;
		return;
	}

	strikeTimer -= (float)dt;

	if (strikeTimer <= 0.0f)
	{
		// The count ran out with no smoke thrown. It connects.
		strikeTimer = 0.0f;
		endLevel03Lose();
	}
}

// Called from fixedUpdate() on the frame F goes down. Does nothing unless a
// smoke box is actually in hand and the eagle is actually on the hunt.
void throwSmokeBox()
{
	if (gameState != LEVEL_03) return;
	if (!hasSmokeBox)          return;
	if (!eagleChasing)         return;
	if (smokeActive)           return;

	hasSmokeBox = 0;               // consumed: the same box cannot be reused
	smokeActive = 1;
	smokeTimer  = SMOKE_DURATION;

	eagleState  = EAGLE_BLINDED;   // the dive is broken off here and then
	strikeTimer = 0.0f;

	audioPlayOnce("bombsnd");
}

void updateLevel03()
{
	DWORD now;
	double dt;
	float  speed;

	now = GetTickCount();
	dt  = (now - l3PrevTick) / 1000.0;
	l3PrevTick = now;

	if (dt > 0.10) dt = 0.10;
	if (dt <= 0.0) return;

	// the stumble clock
	if (isStumbling)
	{
		stumbleTimer -= (float)dt;
		if (stumbleTimer <= 0.0f)
		{
			stumbleTimer = 0.0f;
			isStumbling  = 0;
		}
	}

	updatePlayerLevel03(dt);

	// One speed, chosen fresh each frame. The chase speed is a Level 03 local:
	// nothing here writes back into any shared movement value, so Level 01 and
	// Level 02 keep their own pace untouched.
	speed = eagleChasing ? L3_CHASE_SCROLL : L3_BASE_SCROLL;

	if (isKeyPressed('d') || isKeyPressed('D') || isSpecialKeyPressed(GLUT_KEY_RIGHT))
		speed *= L3_SPRINT_MULT;

	if (isStumbling)
		speed *= L3_STUMBLE_MULT;

	l3ScrollX += speed * (float)dt;

	updateWoodPiles();
	updateSmokeBoxes();
	updateEggs();

	updateSmokeEffect(dt);
	if (gameState != LEVEL_03) return;      // the smoke clearing can win it

	updateEagle(dt);
}

//==============================================================================
//  Level 03 drawing
//==============================================================================

// Draws a texture into a rectangle, optionally mirrored left-to-right.
void drawTexRect(unsigned int tex, double x, double y, double w, double h, int flip)
{
	float s0 = flip ? 1.0f : 0.0f;
	float s1 = flip ? 0.0f : 1.0f;

	if (tex == 0) return;

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	// iShowImage addresses the vertical axis as 0 .. -1, so the wrap mode
	// has to be REPEAT. The loader stores these textures as CLAMP, which
	// collapses that range and paints a flat edge colour instead.
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	glBegin(GL_QUADS);
		glTexCoord2f(s0, 0);  glVertex2d(x,     y);
		glTexCoord2f(s1, 0);  glVertex2d(x + w, y);
		glTexCoord2f(s1, -1); glVertex2d(x + w, y + h);
		glTexCoord2f(s0, -1); glVertex2d(x,     y + h);
	glEnd();

	glDisable(GL_TEXTURE_2D);
}

// The background, repeated. Every second copy is mirrored, so neighbouring
// tiles always meet on identical pixels and the join disappears. Drawn head to
// tail the picture's right edge met its own left edge and the mismatch showed
// as a hard vertical line through the sky.
void drawLevel03Background()
{
	int   first = (int)(l3ScrollX / L3_BG_W);
	int   k;

	for (k = 0; k <= 1; k++)
	{
		int   idx = first + k;
		double x  = (double)idx * L3_BG_W - l3ScrollX;

		drawTexRect(sprL3Bg.tex, x, L3_BG_Y, L3_BG_W, L3_BG_H, idx & 1);
	}
}

// drawSpriteFit, but mirrored left-to-right. Both eagle pictures face left, so
// the one chasing from behind has to be flipped to face the way it is flying.
void drawSpriteFitFlipped(Sprite s, double cx, double cy, double boxSize)
{
	double scale, dw, dh, x, y;

	if (s.tex == 0 || s.w <= 0 || s.h <= 0) return;

	scale = (s.w >= s.h) ? (boxSize / s.w) : (boxSize / s.h);
	dw = s.w * scale;
	dh = s.h * scale;
	x  = cx - dw / 2.0;
	y  = cy - dh / 2.0;

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, s.tex);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	// iShowImage addresses the vertical axis as 0 .. -1, so the wrap mode
	// has to be REPEAT. The loader stores these textures as CLAMP, which
	// collapses that range and paints a flat edge colour instead.
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	glBegin(GL_QUADS);
		glTexCoord2f(1, 0);  glVertex2d(x,      y);
		glTexCoord2f(0, 0);  glVertex2d(x + dw, y);
		glTexCoord2f(0, -1); glVertex2d(x + dw, y + dh);
		glTexCoord2f(1, -1); glVertex2d(x,      y + dh);
	glEnd();

	glDisable(GL_TEXTURE_2D);
}

// The smoke sheet is white on black, so it is added to the scene rather than
// laid over it: the black contributes nothing and only the smoke shows. alpha
// fades it in and out so it never slams on or off.
void drawSmokeOverlay(double alpha)
{
	int i;

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, sprSmokeFx.tex);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE);          // additive
	glColor4d(1.0, 1.0, 1.0, alpha);

	// Two full-screen layers. The scroll is done in texture space rather than
	// by sliding the quad, so the fog always covers the whole screen; moving
	// the quads instead left their straight edges visible across the picture.
	for (i = 0; i < 2; i++)
	{
		double span = (i == 0) ? 2.2  : 1.6;                    // tiles across
		double u    = (i == 0) ? fmod(l3ScrollX * 0.00060, 1.0)
		                       : fmod(l3ScrollX * 0.00095, 1.0);
		double v    = (i == 0) ? 0.0 : -0.15;                   // offset layers

		// The second layer only thickens the first. Adding two layers at full
		// strength washed the screen to solid white and hid the runner, who
		// still has wood piles to clear while the smoke hangs.
		glColor4d(1.0, 1.0, 1.0, (i == 0) ? alpha : alpha * 0.55);

		glBegin(GL_QUADS);
			glTexCoord2d(u,        v);        glVertex2d(0,        0);
			glTexCoord2d(u + span, v);        glVertex2d(SCREEN_W, 0);
			glTexCoord2d(u + span, v - 1.0);  glVertex2d(SCREEN_W, SCREEN_H);
			glTexCoord2d(u,        v - 1.0);  glVertex2d(0,        SCREEN_H);
		glEnd();
	}

	glDisable(GL_TEXTURE_2D);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);   // back to normal
	glColor4d(1.0, 1.0, 1.0, 1.0);
}

void drawEagle()
{
	double cx, cy, t, size;

	if (!eagleChasing && eagleState != EAGLE_BLINDED) return;

	// While blinded the bird is lost somewhere in the smoke.
	if (smokeActive) return;

	// How far through the strike we are: 0 the instant the egg is taken, 1 at
	// the moment of impact. The eagle is drawn closing along that fraction, so
	// the countdown is something the player can see as well as read.
	t = (strikeTotal > 0.0f) ? (1.0 - (double)strikeTimer / (double)strikeTotal)
	                         : 1.0;
	if (t < 0.0) t = 0.0;
	if (t > 1.0) t = 1.0;

	size = 140.0 + 90.0 * t;          // looms larger as it arrives

	if (eagleState == EAGLE_ATTACK_FRONT)
	{
		// Second egg: head-on. The artwork already faces left, so it flies in
		// from the right exactly as drawn.
		cx = (SCREEN_W + 170.0) + t * ((PLAYER_X + 90.0) - (SCREEN_W + 170.0));
		cy = GROUND_Y + 200.0 - 60.0 * t;
		drawSpriteFit(sprEagleFront, cx, cy, size);
	}
	else
	{
		// First egg: from behind. Flipped so it faces the way it is flying.
		cx = -170.0 + t * ((PLAYER_X + 40.0) - (-170.0));
		cy = GROUND_Y + 200.0 - 60.0 * t;
		drawSpriteFitFlipped(sprEagleBack, cx, cy, size);
	}
}

void drawLevel03Hud()
{
	char buf[128];

	iFilledRectangleAlpha(0, SCREEN_H - 58, SCREEN_W, 58, 8, 14, 20, 0.62);

	iSetColor(255, 205, 80);
	sprintf(buf, "SCORE: %d", l3Score);
	iText(24, SCREEN_H - 38, buf, GLUT_BITMAP_HELVETICA_18);

	if (hasSmokeBox)
	{
		iSetColor(120, 235, 140);
		iText(230, SCREEN_H - 38, "SMOKE BOX: YES", GLUT_BITMAP_HELVETICA_18);
	}
	else
	{
		iSetColor(160, 160, 170);
		iText(230, SCREEN_H - 38, "SMOKE BOX: NO", GLUT_BITMAP_HELVETICA_18);
	}

	if (eagleChasing)
	{
		iSetColor(255, 90, 80);
		iText(470, SCREEN_H - 38, "EAGLE STRIKE!", GLUT_BITMAP_HELVETICA_18);

		// Tenths, because the whole window is only a second or two.
		sprintf(buf, "IMPACT IN: %.1fs", strikeTimer < 0.0f ? 0.0f : strikeTimer);
		iSetColor(235, 200, 90);
		iText(650, SCREEN_H - 38, buf, GLUT_BITMAP_HELVETICA_18);
	}
	else
	{
		iSetColor(180, 190, 200);
		iText(470, SCREEN_H - 38, "FIND THE EAGLE EGG", GLUT_BITMAP_HELVETICA_18);
	}

	sprintf(buf, "EGGS: %d / %d", eggsTaken, L3_EGG_COUNT);
	iSetColor(200, 215, 225);
	iText(860, SCREEN_H - 38, buf, GLUT_BITMAP_HELVETICA_18);

	sprintf(buf, "DISTANCE: %d m", (int)(l3ScrollX / 10.0f));
	iSetColor(200, 215, 225);
	iText(1080, SCREEN_H - 38, buf, GLUT_BITMAP_HELVETICA_12);
}

void drawLevel03()
{
	int i;

	drawLevel03Background();

	// road objects
	for (i = 0; i < L3_WOOD_COUNT; i++)
	{
		float sx;
		if (!l3Wood[i].active) continue;
		sx = l3Wood[i].worldX - l3ScrollX;
		if (sx < -L3_WOOD_SIZE || sx > SCREEN_W + L3_WOOD_SIZE) continue;
		drawSpriteBottom(sprWoodPile, sx, l3Wood[i].y, L3_WOOD_SIZE);
	}

	if (l3SmokeBox.active)
	{
		float sx = l3SmokeBox.worldX - l3ScrollX;
		if (sx > -L3_SMOKEBOX_SIZE && sx < SCREEN_W + L3_SMOKEBOX_SIZE)
			drawSpriteBottom(sprSmokeBox, sx, l3SmokeBox.y, L3_SMOKEBOX_SIZE);
	}

	if (l3Egg.active)
	{
		float sx = l3Egg.worldX - l3ScrollX;
		if (sx > -L3_EGG_SIZE && sx < SCREEN_W + L3_EGG_SIZE)
			drawSpriteBottom(sprEgg, sx, l3Egg.y, L3_EGG_SIZE);
	}

	// the eagle goes behind the runner when it is on his tail
	if (eagleState != EAGLE_ATTACK_FRONT) drawEagle();

	// the runner, on the shared run frames
	iShowImage(PLAYER_X, (int)playerY, PLAYER_W, PLAYER_H, sprRun[runFrame].tex);

	if (eagleState == EAGLE_ATTACK_FRONT) drawEagle();

	// the smoke, over everything, fading in and out at the edges of its life
	if (smokeActive)
	{
		// Thick enough to read as "the eagle has lost you", thin enough that
		// the road and the runner stay playable underneath.
		double a = 0.30;
		if (smokeTimer > SMOKE_DURATION - 0.5) a *= (SMOKE_DURATION - smokeTimer) / 0.5;
		if (smokeTimer < 0.7)                  a *= smokeTimer / 0.7;
		if (a < 0.0) a = 0.0;
		drawSmokeOverlay(a);
	}

	if (isStumbling)
		iFilledRectangleAlpha(0, 0, SCREEN_W, SCREEN_H, 180, 90, 30, stumbleTimer * 0.25);

	// While the eagle is diving: the throw prompt if there is a box to throw,
	// and a plain warning if there is not - with no box, F does nothing and
	// the strike is going to land.
	if (eagleChasing && !smokeActive)
	{
		iFilledRectangleAlpha(SCREEN_W / 2.0 - 230, 556, 460, 46, 10, 14, 10, 0.88);

		if (hasSmokeBox)
		{
			iSetColor(255, 205, 80);
			iRectangle(SCREEN_W / 2.0 - 230, 556, 460, 46);
			iTextCentered(SCREEN_W / 2.0, 572, "PRESS F TO THROW SMOKE",
			              GLUT_BITMAP_HELVETICA_18);
		}
		else
		{
			iSetColor(235, 70, 70);
			iRectangle(SCREEN_W / 2.0 - 230, 556, 460, 46);
			iTextCentered(SCREEN_W / 2.0, 572, "NO SMOKE BOX  -  NOTHING TO THROW!",
			              GLUT_BITMAP_HELVETICA_18);
		}
	}

	drawLevel03Hud();

	iFilledRectangleAlpha(0, 0, 470, 30, 8, 14, 12, 0.55);
	iSetColor(220, 230, 215);
	iText(24, 10, "W / UP jump    RIGHT sprint    F throw smoke    ESC menu",
	      GLUT_BITMAP_HELVETICA_12);
}

//------------------------------------------------------------------------------
// Result screen, styled to match the other two levels.
//------------------------------------------------------------------------------
void drawLevel03Result(int didWin)
{
	char buf[128];
	int  y;

	drawLevel03();
	dimScreen(0.78);

	iFilledRectangleAlpha(340, 200, 600, 330, 10, 14, 20, 0.90);

	if (didWin) iSetColor(90, 210, 110);
	else        iSetColor(225, 60, 60);
	iRectangle(340, 200, 600, 330);

	y = 470;

	if (didWin)
	{
		iSetColor(90, 220, 120);
		iTextCentered(SCREEN_W / 2.0, y, "YOU WIN!", GLUT_BITMAP_TIMES_ROMAN_24);
		y -= 34;
		iSetColor(220, 220, 230);
		iTextCentered(SCREEN_W / 2.0, y, "LEVEL 03 COMPLETE", GLUT_BITMAP_HELVETICA_18);
	}
	else
	{
		iSetColor(235, 70, 70);
		iTextCentered(SCREEN_W / 2.0, y, "YOU LOSE", GLUT_BITMAP_TIMES_ROMAN_24);
		y -= 34;
		iSetColor(220, 220, 230);
		iTextCentered(SCREEN_W / 2.0, y, "THE EAGLE CAUGHT YOU", GLUT_BITMAP_HELVETICA_18);
	}

	y -= 46;
	iSetColor(90, 90, 105);
	iLine(400, y + 16, 880, y + 16);

	iSetColor(255, 205, 80);
	sprintf(buf, "Score: %d", l3Score);
	iTextCentered(SCREEN_W / 2.0, y, buf, GLUT_BITMAP_HELVETICA_18);

	y -= 30;
	iSetColor(220, 220, 230);
	sprintf(buf, "Distance Run: %d m", (int)(l3ScrollX / 10.0f));
	iTextCentered(SCREEN_W / 2.0, y, buf, GLUT_BITMAP_HELVETICA_18);

	y -= 30;
	sprintf(buf, "Wood Piles Passed: %d", l3WoodCleared);
	iTextCentered(SCREEN_W / 2.0, y, buf, GLUT_BITMAP_HELVETICA_18);

	y -= 50;
	iSetColor(170, 170, 185);

	if (didWin) iTextCentered(SCREEN_W / 2.0, y, "Press ENTER to continue", GLUT_BITMAP_HELVETICA_12);
	else        iTextCentered(SCREEN_W / 2.0, y, "Press R to Restart", GLUT_BITMAP_HELVETICA_12);

	y -= 20;
	iTextCentered(SCREEN_W / 2.0, y, "Press ESC for Level Select", GLUT_BITMAP_HELVETICA_12);
}

//==============================================================================
//  9. iGRAPHICS CALLBACKS
//==============================================================================

void iDraw()
{
	iClear();

	switch (gameState)
	{
	case MAIN_MENU:     drawMenu();          break;
	case KEYS:          drawKeys();          break;
	case ABOUT_GAME:    drawAbout();         break;
	case LEVEL_SELECT:  drawLevelSelect();   break;
	case LEVEL_01:      drawLevel01();       break;
	case LEVEL_01_WIN:  drawResultScreen(1); break;
	case LEVEL_01_LOSE: drawResultScreen(0); break;
	case LEVEL_02:      drawLevel02();       break;
	case LEVEL_02_WIN:  drawLevel02Result(1); break;
	case LEVEL_02_LOSE: drawLevel02Result(0); break;
	case LEVEL_03:      drawLevel03();       break;
	case LEVEL_03_WIN:  drawLevel03Result(1); break;
	case LEVEL_03_LOSE: drawLevel03Result(0); break;
	}
}

void iMouseMove(int mx, int my)
{
}

void iPassiveMouseMove(int mx, int my)
{
	int i;

	hoveredButton = -1;
	backHovered   = 0;

	if (gameState == MAIN_MENU)
	{
		for (i = 0; i < BTN_COUNT; i++)
		{
			if (pointInBox(mx, my, BTN_X, buttonY(i), BTN_W, BTN_H))
			{
				hoveredButton = i;
				break;
			}
		}
	}
	else if (gameState == KEYS || gameState == ABOUT_GAME)
	{
		backHovered = pointInBox(mx, my, BACK_X, BACK_Y, BACK_W, BACK_H);
	}
	else if (gameState == LEVEL_SELECT)
	{
		levelSelectHover(mx, my);
	}
}

void iMouse(int button, int state, int mx, int my)
{
	if (button != GLUT_LEFT_BUTTON || state != GLUT_DOWN)
		return;

	if (gameState == MAIN_MENU)
	{
		if (pointInBox(mx, my, BTN_X, buttonY(BTN_NEW_GAME), BTN_W, BTN_H))
		{
			openLevelSelect();     // NEW GAME now opens the campaign screen
		}
		else if (pointInBox(mx, my, BTN_X, buttonY(BTN_KEYS), BTN_W, BTN_H))
		{
			gameState = KEYS;
		}
		else if (pointInBox(mx, my, BTN_X, buttonY(BTN_ABOUT), BTN_W, BTN_H))
		{
			gameState = ABOUT_GAME;
		}
		else if (pointInBox(mx, my, BTN_X, buttonY(BTN_EXIT), BTN_W, BTN_H))
		{
			audioStop("introsong");
			exit(0);
		}
	}
	else if (gameState == KEYS || gameState == ABOUT_GAME)
	{
		if (pointInBox(mx, my, BACK_X, BACK_Y, BACK_W, BACK_H))
		{
			gameState     = MAIN_MENU;
			hoveredButton = -1;
			backHovered   = 0;
		}
	}
	else if (gameState == LEVEL_SELECT)
	{
		handleLevelSelectClick(mx, my);
	}
}

//------------------------------------------------------------------------------
// Runs every 16 ms. Keys are polled here rather than handled as one-off events,
// so held keys stay smooth.
//
// ESC, ENTER and R must only act once per press, so their previous state is
// remembered and the action fires on the change from "up" to "down".
//------------------------------------------------------------------------------
void fixedUpdate()
{
	static int escWasDown   = 0;
	static int enterWasDown = 0;
	static int rWasDown     = 0;
	static int fWasDown     = 0;

	int escDown   = isKeyPressed(27);
	int enterDown = isKeyPressed(13);
	int rDown     = isKeyPressed('r') || isKeyPressed('R');
	int fDown     = isKeyPressed('f') || isKeyPressed('F');

	// --- F: cut a net. tryCutNet() itself checks that the player really is
	//     trapped and really holds a knife, so a stray F does nothing at all.
	if (fDown && !fWasDown)
	{
		tryCutNet();       // Level 02: cuts a net
		throwSmokeBox();   // Level 03: throws the smoke box
	}
	fWasDown = fDown;

	// --- ESC: always goes back one step ---
	if (escDown && !escWasDown)
	{
		if (gameState == KEYS || gameState == ABOUT_GAME)
			gameState = MAIN_MENU;
		else if (gameState == LEVEL_SELECT)
		{
			// A notice closes first; only a second ESC leaves the screen.
			if (levelMessage != MSG_NONE)
				levelMessage = MSG_NONE;
			else
				gameState = MAIN_MENU;
		}
		else if (gameState == LEVEL_01 ||
		         gameState == LEVEL_01_WIN ||
		         gameState == LEVEL_01_LOSE)
			returnToMenu();
		else if (gameState == LEVEL_02 ||
		         gameState == LEVEL_02_WIN ||
		         gameState == LEVEL_02_LOSE)
			leaveLevel02();
		else if (gameState == LEVEL_03 ||
		         gameState == LEVEL_03_WIN ||
		         gameState == LEVEL_03_LOSE)
			leaveLevel03();
	}
	escWasDown = escDown;

	// --- ENTER on the win screen ---
	// Level 02 does not exist yet, so continuing returns to the main menu.
	if (enterDown && !enterWasDown)
	{
		if (gameState == LEVEL_01_WIN)
			returnToMenu();
		else if (gameState == LEVEL_02_WIN)
			leaveLevel02();
		else if (gameState == LEVEL_03_WIN)
			leaveLevel03();
		else if (gameState == LEVEL_SELECT && levelMessage != MSG_NONE)
			levelMessage = MSG_NONE;      // ENTER also dismisses a notice
	}
	enterWasDown = enterDown;

	// --- R on the lose screen: play Level 01 again from the start ---
	if (rDown && !rWasDown)
	{
		if (gameState == LEVEL_01_LOSE)
		{
			audioStop("losesnd");      // make sure the old sound is not still going
			startLevel01();            // resets everything and restarts the music
		}
		else if (gameState == LEVEL_02_LOSE)
		{
			audioStop("losesnd");
			startLevel02();            // full reset, and one fresh music loop
		}
		else if (gameState == LEVEL_03_LOSE)
		{
			audioStop("losesnd");
			startLevel03();
		}
	}
	rWasDown = rDown;

	// --- gameplay only advances while a level is actually being played ---
	if (gameState == LEVEL_01)
		updateLevel01();
	else if (gameState == LEVEL_02)
		updateLevel02();
	else if (gameState == LEVEL_03)
		updateLevel03();
}

// Advances the 8 frame run cycle. Runs faster while a Speed Box is active,
// so the character visibly sprints.
void animatePlayer()
{
	runFrame = (runFrame + 1) % RUN_FRAMES;
}

//==============================================================================
//  10. MAIN
//==============================================================================

int main()
{
	srand((unsigned int)time(NULL));   // endless spawning uses rand()

	loadProgress();                    // pick up any earlier info.txt

	setupWorkingDirectory();

	iInitialize(SCREEN_W, SCREEN_H, "Shadow Chase: The Final Escape");

	// Soft sprite edges. iGraphics only sets up alpha testing by itself, which
	// would leave hard jagged outlines on the character and the pickups.
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	loadAssets();                       // needs the GL context from iInitialize
	loadLevel02Assets();                // ditto
	loadLevel03Assets();                // ditto
	loadAudio();

	resetLevel01();                     // fills the arrays with valid values
	resetLevel02();
	resetLevel03();

	animTimerId = iSetTimer(80, animatePlayer);
	iPauseTimer(animTimerId);           // idle until Level 01 starts

	startIntroMusic();                  // loops until NEW GAME is pressed

	iStart();
	return 0;
}
