/*
 *  Copyright (C) 2014 JS <jsdemonsim@gmail.com>
 *
 *  Note: JS (aka John Smythe) is the pseudonym of the author.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * HOW THIS SIMULATOR WORKS
 * ------------------------
 *
 * The basic unit of simulation is the State structure.  The State structure
 * holds every piece of information that is needed to run one simulation
 * from start to end.  Since the program is multithreaded, one State
 * structure is created for each thread.  That way, the program can be
 * run on multiple cores, with each core operating on a separate State.
 * After all simulations are run, the main thread will total the results
 * from each of the States and print the results.
 *
 * The main way that abilities are handled is that each card has an array
 * of "Attributes".  An attribute can either be an ability, such as "Dodge:60",
 * or it can be a temporary buff or debuff applied by another card's ability,
 * such as "Toxic Clouds:200".  Whenever a buff/debuff affects a card, we
 * simply add an attribute to the card.  When the buff/debuff disappears, we
 * remove the attribute from the card.  At many places in the simulation, we
 * check whether the card has a particular attribute with the HasAttr()
 * function.
 *
 * Many times, the demon simulator will reuse the same attribute as both the
 * ability and the debuff.  It can do this because the demon is immune to all
 * debuffs.  So if the demon has "Fire God:200", we know it has the ability
 * and isn't affected by it.  Conversely, the player card will never have the
 * "Fire God" ability because we make sure never to add abilities like this to
 * the player cards in the cards file.  So if a player card has "Fire God", we
 * know that it was put there by the demon and the player card should lose hp
 * every turn.
 *
 * In other cases, we create a separate attribute for an ability to distinguish
 * its buff from the ability itself.  For example, if a card has the
 * "Forest force" ability (ATTR_FOREST_ATK), it will place the
 * "Force force buff" attribute (ATTR_FOREST_ATK_BUFF) on all other cards.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#if defined(_MSC_VER)
  // Compiling for Windows.
  #include <windows.h>
  #define USING_WINDOWS
  // Would you believe that the microsoft compiler doesn't have stdbool?
  // So I have to define my own bool type here.
  #define true		1
  #define false		0
  #define bool		int
  #define strcasecmp	stricmp
#else
  // Not Windows.
  #include <pthread.h>
  #include <stdbool.h>
  #include <stdint.h>
#endif

/*---------------------------------------------------------------------------*/
/* CONSTANTS								     */
/*---------------------------------------------------------------------------*/

#define MAX_LEVEL		150

#define FIRST_DEMON_ROUND	5
#define FIRST_PLAYER_ROUND	6

#define DEFAULT_ITERS		50000
#define DEFAULT_LEVEL		61
#define DEFAULT_MAX_ROUNDS	500
#define DEFAULT_THREADS		8

#define MAX_ATTR		40
#define MAX_RUNES		4
#define MAX_CARDS_IN_SET	20
#define MAX_CARDS_IN_DECK	10
#define MAX_CARD_TYPES		1000
#define MAX_CARDS_IN_HAND	5
#define MAX_THREADS		64

#define SET_HAND	1
#define SET_FIELD	2
#define SET_GRAVE	3

#define MAX_LINE_SIZE	4096

#define dprintf(fmt, ...) \
    do { if (doDebug) {fprintf(output, fmt, ## __VA_ARGS__);} } while (0)

#define vprintf(fmt, ...) \
    do { if (verbose) {fprintf(output, fmt, ## __VA_ARGS__);} } while (0)

/*---------------------------------------------------------------------------*/
/* GLOBALS								     */
/*---------------------------------------------------------------------------*/

static FILE *output;

static bool        doDebug;
static bool        doAppend;
static bool        verbose;
static bool        showDamage;
static bool        avgConcentrate;
static const char *outputFilename;
static const char *deckFile = "deck.txt";
static int         numIters = DEFAULT_ITERS;
static int         numThreads = 8;

// Initial state:
static int initialLevel = DEFAULT_LEVEL;
static int initialHp    = 0;
static int maxRounds    = DEFAULT_MAX_ROUNDS;

static const char *theDeck[MAX_CARDS_IN_DECK];
static int         numDeckCards;
static const char *theRunes[MAX_RUNES];
static int         numRunes;

static const char *theDemon = "DarkTitan";

// This is the big list of attributes that are supported (i.e. abilities).
enum attrTypes {
    ATTR_NONE,
    ATTR_ADVANCED_STRIKE,
    ATTR_BACKSTAB,
    ATTR_BACKSTAB_BUFF,
    ATTR_BITE,
    ATTR_BLOODSUCKER,
    ATTR_BLOODTHIRSTY,
    ATTR_CHAIN_ATTACK,
    ATTR_CONCENTRATE,
    ATTR_COUNTERATTACK,
    ATTR_CRAZE,
    ATTR_CURSE,
    ATTR_D_PRAYER,
    ATTR_D_REANIMATE,
    ATTR_D_REINCARNATE,
    ATTR_DAMNATION,
    ATTR_DEAD,
    ATTR_DESTROY,
    ATTR_DEXTERITY,
    ATTR_DODGE,
    ATTR_EVASION,
    ATTR_EXILE,
    ATTR_FIRE_GOD,
    ATTR_FOREST,
    ATTR_FOREST_ATK,
    ATTR_FOREST_ATK_BUFF,
    ATTR_FOREST_HP,
    ATTR_FOREST_HP_BUFF,
    ATTR_GUARD,
    ATTR_HEALING,
    ATTR_HOT_CHASE,
    ATTR_ICE_SHIELD,
    ATTR_IMMUNITY,
    ATTR_LACERATE,
    ATTR_LACERATE_BUFF,
    ATTR_MANA_CORRUPT,
    ATTR_MANIA,
    ATTR_MTN,
    ATTR_MTN_ATK,
    ATTR_MTN_ATK_BUFF,
    ATTR_MTN_HP,
    ATTR_MTN_HP_BUFF,
    ATTR_OBSTINACY,
    ATTR_PARRY,
    ATTR_PRAYER,
    ATTR_QS_PRAYER,
    ATTR_QS_REGENERATE,
    ATTR_QS_REINCARNATE,
    ATTR_REANIMATE,
    ATTR_REANIM_SICKNESS,
    ATTR_REFLECTION,
    ATTR_REGENERATE,
    ATTR_REINCARNATE,
    ATTR_REJUVENATE,
    ATTR_RESISTANCE,
    ATTR_RESURRECTION,
    ATTR_RETALIATION,
    ATTR_SACRIFICE,
    ATTR_SNIPE,
    ATTR_SWAMP,
    ATTR_SWAMP_ATK,
    ATTR_SWAMP_ATK_BUFF,
    ATTR_SWAMP_HP,
    ATTR_SWAMP_HP_BUFF,
    ATTR_TOXIC_CLOUDS,
    ATTR_TRAP,
    ATTR_TRAP_BUFF,
    ATTR_TUNDRA,
    ATTR_TUNDRA_ATK,
    ATTR_TUNDRA_ATK_BUFF,
    ATTR_TUNDRA_HP,
    ATTR_TUNDRA_HP_BUFF,
    ATTR_VENDETTA,
    ATTR_WARPATH,
    ATTR_WICKED_LEECH,

    // Runes
    ATTR_ARCTIC_FREEZE,
    ATTR_BLOOD_STONE,
    ATTR_CLEAR_SPRING,
    ATTR_FROST_BITE,
    ATTR_RED_VALLEY,
    ATTR_LORE,
    ATTR_LEAF,
    ATTR_REVIVAL,
    ATTR_FIRE_FORGE,
    ATTR_STONEWALL,
    ATTR_SPRING_BREEZE,
    ATTR_THUNDER_SHIELD,
    ATTR_NIMBLE_SOUL,
    ATTR_DIRT,
    ATTR_FLYING_STONE,
    ATTR_TSUNAMI,
};

// An attribute will have a type and an optional "level".  The level will be
// either an amount or percent.  For example, "Dodge:60" will have a type
// of ATTR_DODGE and a level of 60.
typedef struct attribute {
    int		type;
    int		level;
} Attr;

// The dead attribute is used to mark a card as dead so we can identify
// a dead card that way instead of looking at the hit points.  Some cards
// can "die" without losing all their hit points (e.g. exile).
const Attr DeadAttr = { ATTR_DEAD };

// This is the structure for one card.  There are two sections.  The first
// section is set by the type of card and can't change.  The second section
// is the current state of the card and may change over the course of the
// battle.  The first section is used to initialize the second section when
// a card needs to be reset to its original stats (e.g. when first played,
// when reincarnated, etc).
typedef struct card {
    // Not changeable info.
    const char *name;
    int		cost;
    int		timing;
    int		baseAtk;
    int		baseHp;
    Attr	baseAttr[MAX_ATTR];

    // Current state.
    int		curTiming;
    int		atk;
    int		curBaseAtk;
    int		hp;
    int		maxHp;
    int		numAttr;
    Attr	attr[MAX_ATTR];
} Card;

// A card set is basically an array of cards with a count.  There are four
// sets for each simulation: the field, the hand, the graveyard, and the deck.
// See the State structure below.
typedef struct cardSet {
    int		numCards;
    Card	cards[MAX_CARDS_IN_SET];
} CardSet;

// This is the structure for a rune.  Like a Card, it has the constant
// section and the current state section.
typedef struct rune {
    // Not changeable.
    const char *name;
    Attr	attr;
    int		maxCharges;
    // Changeable.
    int		chargesUsed;
    int		usedThisRound;
} Rune;

// The State structure holds the entire state of a simulation.
typedef struct state {
    int			dmgDone;		// Damage done to demon.
    int			hp;			// Player's current hp.
    int			maxHp;			// Player's max hp.
    int			round;			// Current round.
    int			numRunes;		// Number of runes.
    Card		demon;			// Demon state.
    CardSet		deck;			// Cards in deck.
    CardSet		hand;			// Cards in hand.
    CardSet		field;			// Cards on field.
    CardSet		grave;			// Cards in grave.
    Rune		runes[MAX_RUNES];	// Array of runes.
    unsigned int	seedW;			// Random seed part 1.
    unsigned int	seedZ;			// Random seed part 2.
} State;

typedef struct result {
    long long total;
    long long totalRounds;
    int       lowRounds;
    int       highRounds;
    int       lowDamage;
    int       highDamage;
    int       timesRoundX;
} Result;

typedef struct task {
    State   *state;
    int      numIterations;
    Result  *result;
} Task;

#define DIM(a)		(sizeof(a)/sizeof(a[0]))
#define MIN(x,y)	((x) < (y) ? (x) : (y))
#define MAX(x,y)	((x) > (y) ? (x) : (y))

static Card *FindLowestHpCard(State *state, CardSet *cs, bool mostDamaged);
static void CardPlayedToField(State *state, Card *c);
static void SimAdvancedStrike(State *state);
static void SimPrayer(State *state, int heal);
static void SimRegenerate(State *state, const char *name, int heal);
static void SimReincarnate(State *state, const char *attrName, int level);
static void SimReanimate(State *state, const char *attrName);
static int PickAliveCardFromSet(State *state, const CardSet *cs);
static void AddCardToSetRandomly(State *state, CardSet *cs, const Card *c);

static Card cardTypes[MAX_CARD_TYPES];
static int numCardTypes;

typedef struct AttrLookup {
    const char *name;
    int         attrType;
} AttrLookup;

static const AttrLookup allAttrs[] = {
    { "NONE",             ATTR_NONE },
    { "ADVANCED STRIKE",  ATTR_ADVANCED_STRIKE },
    { "BACKSTAB",         ATTR_BACKSTAB },
    { "BITE",             ATTR_BITE },
    { "BLOODSUCKER",      ATTR_BLOODSUCKER },
    { "BLOODTHIRSTY",     ATTR_BLOODTHIRSTY },
    { "CHAIN ATTACK",     ATTR_CHAIN_ATTACK },
    { "CONCENTRATE",      ATTR_CONCENTRATE },
    { "COUNTERATTACK",    ATTR_COUNTERATTACK },
    { "CRAZE",            ATTR_CRAZE },
    { "CURSE",            ATTR_CURSE },
    { "D_PRAYER",      	  ATTR_D_PRAYER },
    { "D_REANIMATE",      ATTR_D_REANIMATE },
    { "D_REINCARNATE",    ATTR_D_REINCARNATE },
    { "DAMNATION",        ATTR_DAMNATION },
    { "DEAD",             ATTR_DEAD },
    { "DEXTERITY",        ATTR_DEXTERITY },
    { "DESTROY",          ATTR_DESTROY },
    { "DODGE",            ATTR_DODGE },
    { "EXILE",            ATTR_EXILE },
    { "EVASION",          ATTR_EVASION },
    { "FIRE GOD",         ATTR_FIRE_GOD },
    { "FOREST",           ATTR_FOREST },
    { "FOREST FORCE",     ATTR_FOREST_ATK },
    { "FOREST GUARD",     ATTR_FOREST_HP },
    { "GUARD",            ATTR_GUARD },
    { "HEALING",          ATTR_HEALING },
    { "HOT CHASE",        ATTR_HOT_CHASE },
    { "ICE SHIELD",       ATTR_ICE_SHIELD },
    { "IMMUNITY",         ATTR_IMMUNITY },
    { "LACERATE",         ATTR_LACERATE },
    { "MANA CORRUPT",     ATTR_MANA_CORRUPT },
    { "MANIA",            ATTR_MANIA },
    { "MTN",              ATTR_MTN },
    { "MTN FORCE",        ATTR_MTN_ATK },
    { "MTN GUARD",        ATTR_MTN_HP },
    { "OBSTINACY",        ATTR_OBSTINACY },
    { "PARRY",            ATTR_PARRY },
    { "PRAYER",           ATTR_PRAYER },
    { "QS_PRAYER",        ATTR_QS_PRAYER },
    { "QS_REGENERATE",    ATTR_QS_REGENERATE },
    { "QS_REINCARNATE",   ATTR_QS_REINCARNATE },
    { "REANIMATE",        ATTR_REANIMATE },
    { "REFLECTION",       ATTR_REFLECTION },
    { "REGENERATE",       ATTR_REGENERATE },
    { "REINCARNATE",      ATTR_REINCARNATE },
    { "REJUVENATE",       ATTR_REJUVENATE },
    { "RESISTANCE",       ATTR_RESISTANCE },
    { "RESURRECTION",     ATTR_RESURRECTION },
    { "RETALIATION",      ATTR_RETALIATION },
    { "SACRIFICE",        ATTR_SACRIFICE },
    { "SNIPE",            ATTR_SNIPE },
    { "SWAMP",            ATTR_SWAMP },
    { "SWAMP FORCE",      ATTR_SWAMP_ATK },
    { "SWAMP GUARD",      ATTR_SWAMP_HP },
    { "TOXIC CLOUDS",     ATTR_TOXIC_CLOUDS },
    { "TRAP",             ATTR_TRAP },
    { "TUNDRA",           ATTR_TUNDRA },
    { "TUNDRA FORCE",     ATTR_TUNDRA_ATK },
    { "TUNDRA GUARD",     ATTR_TUNDRA_HP },
    { "VENDETTA",         ATTR_VENDETTA },
    { "WARPATH",          ATTR_WARPATH },
    { "WICKED LEECH",     ATTR_WICKED_LEECH },
};

static const Rune allRunes[] = {
    { "Arctic Freeze",  { ATTR_ARCTIC_FREEZE,  100 }, 3 },
    { "Blood Stone",    { ATTR_BLOOD_STONE,    270 }, 5 },
    { "Clear Spring",   { ATTR_CLEAR_SPRING,   225 }, 4 },
    { "Frost Bite",     { ATTR_FROST_BITE,     140 }, 3 },
    { "Red Valley",     { ATTR_RED_VALLEY,      90 }, 5 },
    { "Lore",           { ATTR_LORE,           150 }, 4 },
    { "Leaf",           { ATTR_LEAF,           240 }, 4 },
    { "Revival",        { ATTR_REVIVAL,        120 }, 4 },
    { "Fire Forge",     { ATTR_FIRE_FORGE,     210 }, 4 },
    { "Stonewall",      { ATTR_STONEWALL,      180 }, 4 },
    { "Spring Breeze",  { ATTR_SPRING_BREEZE,  240 }, 4 },
    { "Thunder Shield", { ATTR_THUNDER_SHIELD, 200 }, 4 },
    { "Nimble Soul",    { ATTR_NIMBLE_SOUL,     65 }, 3 },
    { "Dirt",           { ATTR_DIRT,            70 }, 4 },
    { "Flying Stone",   { ATTR_FLYING_STONE,   270 }, 4 },
    { "Tsunami",        { ATTR_TSUNAMI,         80 }, 4 },
};

static const Card DeadCard = {
    /* name        = */ "Dead Card",
    /* cost        = */ 0,
    /* timing      = */ 0,
    /* baseAtk     = */ 0,
    /* baseHp      = */ 0,
    /* baseAttr[0] = */ {{ ATTR_DEAD, 0 }, {ATTR_NONE, 0}},
    /* curTiming   = */ 0,
    /* atk         = */ 0,
    /* curBaseAtk  = */ 0,
    /* hp          = */ 0,
    /* maxHp       = */ 0,
    /* numAttr     = */ 1,
    /* attr[0];    = */ {{ ATTR_DEAD, 0 }, {ATTR_NONE, 0}}
};

static const int hpPerLevel[MAX_LEVEL+1] = {
    0, 1000, 1070, 1140, 1210, 1280, 1350, 1420, 1490, 1560, 1630,
    1800, 1880, 1960, 2040, 2120, 2200, 2280, 2360, 2440, 2520,
    2800, 2890, 2980, 3070, 3160, 3250, 3340, 3430, 3520, 3610,
    4000, 4100, 4200, 4300, 4400, 4500, 4600, 4700, 4800, 4900,
    5400, 5510, 5620, 5730, 5840, 5950, 6060, 6170, 6280, 6390,
    7000, 7120, 7240, 7360, 7480, 7600, 7720, 7840, 7960, 8080,
    8800, 8930, 9060, 9190, 9320, 9450, 9580, 9710, 9840, 9970,
    10800, 10940, 11080, 11220, 11360, 11500, 11640, 11780, 11920, 12060,
    13000, 13150, 13300, 13450, 13600, 13750, 13900, 14050, 14200, 14350,
    15400, 15560, 15720, 15880, 16040, 16200, 16360, 16520, 16680, 16840,
    18000, 18170, 18340, 18510, 18680, 18850, 19020, 19190, 19360, 19530,
    20800, 20980, 21160, 21340, 21520, 21700, 21880, 22060, 22240, 22420,
    23800, 23990, 24180, 24370, 24560, 24750, 24940, 25130, 25320, 25510,
    27000, 27200, 27400, 27600, 27800, 28000, 28200, 28400, 28600, 28800,
    30400, 30610, 30820, 31030, 31240, 31450, 31660, 31870, 32080, 32290,
};

// Array of states, one per thread.
static State **states;

// Space for the states.  Instead of malloc'ing each state one by one,
// we allocate a large buffer and then pick page aligned states within the
// buffer.
static char *stateBuffer;

// The one original state.  When we start a new simulation, we copy from
// this state.
static State defaultState;

//static State state;
static int roundX = 50;

/**
 * At initialization time, this function is called to create the specified
 * number of State structures.  The tricky part here is that we want to
 * make sure that the states do not overlap cache lines, so that the cores
 * will not interfere with each other by accessing shared cache lines.  To
 * do this, we make sure that each State is aligned to a 4KB boundary.
 * The created states are put into the states global array.
 *
 * @param	numStates	The number of states to create.
 */
static void AllocateStates(int numStates)
{
    int   stateSize     = sizeof(State);
    int   statePageSize = (stateSize + 0xfff) & ~0xfff;
    int   totalSize     = statePageSize * numStates + 0x1000;
    int   i             = 0;
    char *alignedPtr    = NULL;

    // Allocate the big space.
    stateBuffer = calloc(1, totalSize);

    // Find the first page aligned address.
    alignedPtr  = (char *) (((uintptr_t)(stateBuffer) + 0xfff) & ~0xfff);

    // Allocate array.
    states = calloc(numStates, sizeof(State *));

    for (i=0;i<numStates;i++) {
	// Allocate each state on a page aligned address.
	states[i]   = (State *) alignedPtr;
	alignedPtr += statePageSize;
    }
}

/**
 * Finds an attribute by name from the global array of attributes (allAttrs).
 *
 * @param	name		The name of the attribute to find.
 * @return			The index of the attribute in allAttrs, or
 *				-1 if not found.
 */
static int LookupAttr(const char *name)
{
    int i = 0;

    for (i=0;i<DIM(allAttrs);i++) {
	if (!strcasecmp(name, allAttrs[i].name))
	    return allAttrs[i].attrType;
    }
    return -1;
}

/**
 * Finds a card type by name from the global array of card types (cardTypes).
 *
 * @param	name		The name of the card to find.
 * @return			A pointer to the card type, or NULL if not
 *				found.
 */
static const Card *FindCard(const char *name)
{
    int i = 0;

    for (i=0;i<numCardTypes;i++) {
	if (!strcasecmp(name, cardTypes[i].name))
	    return &cardTypes[i];
    }
    return NULL;
}

/**
 * Finds a rune type by name from the global array of rune types (allRunes).
 *
 * @param	name		The name of the rune to find.
 * @return			A pointer to the rune type, or NULL if not
 *				found.
 */
static const Rune *FindRune(const char *name)
{
    int i = 0;

    for (i=0;i<DIM(allRunes);i++) {
	if (!strcasecmp(name, allRunes[i].name))
	    return &allRunes[i];
    }
    return NULL;
}

/**
 * Initializes a card's current state back to its base state.  This is done
 * at the start of each simulation, and whenever a card is recycled back
 * into play (e.g. reincarnation).  The card's attributes are also reset
 * to its base attributes.
 *
 * @param	card		The card to initialize.
 */
static void InitCard(Card *card)
{
    int i = 0;
    int j = 0;

    card->curTiming  = card->timing;
    card->atk        = card->baseAtk;
    card->curBaseAtk = card->baseAtk;
    card->hp         = card->baseHp;
    card->maxHp      = card->baseHp;
    card->numAttr    = card->baseHp;

    memset(card->attr, 0, sizeof(card->attr));
    card->numAttr = 0;
    for (i=0, j=0;i<MAX_ATTR;i++) {
	if (card->baseAttr[i].type != ATTR_NONE) {
	    card->attr[j++] = card->baseAttr[i];
	    card->numAttr++;
	}
    }
}

/**
 * Returns a random number.  This function is based on the MWC generator,
 * which concatenates two 16-bit multiply with carry generators.  It uses
 * two seeds stored in the state (seedW and seedZ).  The reason why I use
 * this rng is because it is reentrant (it can be run simultaneously by
 * multiple cores).
 *
 * @param	state		The simulator state.  Both seedW and seedZ
 *				are updated to new values on each call.
 * @return			A 32-bit random number.
 */
static unsigned int myRand(State *state)
{
    state->seedW = 18000*(state->seedW & 65535) + (state->seedW >> 16);
    state->seedZ = 36969*(state->seedZ & 65535) + (state->seedZ >> 16);

    return (state->seedZ << 16) + state->seedW;
}

/**
 * Returns a random number in the given range.  Calls the previous
 * function to get the random number.
 * 
 * @param	state		The simulator state.
 * @param	range		Range of random number.
 * @return			A random number in the range [0..range-1].
 */
static unsigned int Rnd(State *state, unsigned int range)
{
    return (((unsigned int) myRand(state)) % range);
}

/**
 * Given a card set, shuffle the cards into a random order.
 *
 * @param	state		The simulator state.
 * @param	cs		The card set to shuffle.
 */
static void ShuffleSet(State *state, CardSet *cs)
{
    int i = 0;

    for (i=0;i<cs->numCards-1;i++) {
	unsigned int r = Rnd(state, cs->numCards - i);

	if (r != 0) {
	    Card tmp = cs->cards[i];
	    cs->cards[i]   = cs->cards[i+r];
	    cs->cards[i+r] = tmp;
	}
    }
}

/**
 * Prints the card state (debug mode only).  The type of printout depends
 * on which set we are printing.
 *
 * @param	c		The card to print.
 * @param	whichSet	Which set the card belongs to.
 */
static void PrintCard(const Card *c, int whichSet)
{
    switch (whichSet) {
	case SET_HAND:
	    dprintf("%-20s (%d)\n", c->name, c->curTiming);
	    break;
	case SET_FIELD:
	    dprintf("%-20s (%d atk) (%4d/%4d hp)\n", c->name,
		    c->atk, c->hp, c->maxHp);
	    break;
	case SET_GRAVE:
	default:
	    dprintf("%-20s\n", c->name);
	    break;
    }
#if 0
    {
	int i = 0;
	dprintf("Attrs = ");
	for (i=0;i<c->numAttr;i++) {
	    dprintf("%d ", c->attr[i].type);
	}
	dprintf("\n");
    }
#endif
}

/**
 * Prints all the cards in a set (debug mode only).
 *
 * @param	cs		The set of cards to print.
 * @param	whichSet	Which set of cards (e.g. SET_GRAVE).
 */
static void PrintCardSet(const CardSet *cs, int whichSet)
{
    int i = 0;
    for (i=0;i<cs->numCards;i++) {
	PrintCard(&cs->cards[i], whichSet);
    }
}

/**
 * Prints the current state (debug mode only).  This is done once per round.
 *
 * @param	state		The simulator state.
 */
void PrintState(const State *state)
{
    dprintf("\nPlayer: Hp = %d, Damage done = %d\n", state->hp, state->dmgDone);
    PrintCard(&state->demon, SET_FIELD);
    if (state->field.numCards != 0) {
	dprintf("\nField:\n");
	PrintCardSet(&state->field, SET_FIELD);
    }
    if (state->hand.numCards != 0) {
	dprintf("\nHand:\n");
	PrintCardSet(&state->hand, SET_HAND);
    }
    if (state->grave.numCards != 0) {
	dprintf("\nGrave:\n");
	PrintCardSet(&state->grave, SET_GRAVE);
    }
}

/**
 * Returns whether a card has a particular attribute, and also what
 * level the attribute is.
 *
 * @param	c		The card.
 * @param	attrType	The attribute type.
 * @param	pLevel		Returns the level of the attribute, if found.
 * @return			True if the attribute was found, false if not.
 */
static bool HasAttr(const Card *c, int attrType, int *pLevel)
{
    int i = 0;

    for (i=0;i<c->numAttr;i++) {
	if (c->attr[i].type == attrType) {
	    if (pLevel != NULL)
		*pLevel = c->attr[i].level;
	    return true;
	}
    }
    return false;
}

/**
 * Adds an attribute to a card.
 *
 * @param	c		The card.
 * @param	attr		The attribute to add to the card.
 */
static void AddAttr(Card *c, const Attr *attr)
{
    if (c->numAttr >= MAX_ATTR) {
	int i = 0;
	fprintf(stderr, "Too many attrs on %s\n", c->name);
	for (i=0;i<c->numAttr;i++) {
	    fprintf(stderr, "%d ", c->attr[i].type);
	}
	fprintf(stderr, "\n");
	exit(1);
    }
    c->attr[c->numAttr++] = *attr;
}

/**
 * Removes an attribute from a card.  Can either remove just one of that
 * attribute or all of that attribute, depending on the level argument.
 *
 * @param	c		The card.
 * @param	attrType	The attribute type to remove from the card.
 * @param	level		If level is -1, then all attributes of the
 *				given type will be removed.  If the level is
 *				not -1, then only one attribute matching the
 *				specified level will be removed.
 */
static void RemoveAttr(Card *c, int attrType, int level)
{
    int i = 0;

    for (i=0;i<c->numAttr;i++) {
	if (c->attr[i].type == attrType &&
		(c->attr[i].level == level || level == -1)) {
	    int j = i;
	    c->numAttr--;
	    for (j=i;j<c->numAttr;j++) {
		c->attr[j] = c->attr[j+1];
	    }
	    i--;
	    if (level != -1)
		return;
	}
    }
}

/**
 * Removes one card from a card set.
 * 
 * @param	cs		Card set.
 * @param	n		Index of card to remove from set.
 */
static void RemoveCardFromSet(CardSet *cs, int n)
{
    int i = 0;

    cs->numCards--;
    for (i=n;i<cs->numCards;i++)
	cs->cards[i] = cs->cards[i+1];
}

/**
 * Adds one card to a set.  Note that a copy will be made of the given card.
 * The card will be added to the end of the set.  This is important because
 * reincarnation currently uses this function to add a card back to the deck.
 * Since the deck is played from the end, the last reincarnated card will
 * be played from the deck as the next card.
 *
 * @param	cs		Card set.
 * @param	c		Card to be added to set (a copy will be made).
 */
static void AddCardToSet(CardSet *cs, const Card *c)
{
    if (cs->numCards >= MAX_CARDS_IN_SET) {
	fprintf(stderr, "Too many cards\n");
	exit(1);
    }
    cs->cards[cs->numCards++] = *c;
}

/**
 * Adds one card to a set in a random position.  This is used when adding
 * a card back to the deck in a random order.  Note that currently this is
 * only used when a card is exiled.  It has been determined that reincarnating
 * a card from the graveyard puts the card at the top of the deck and not
 * randomly.
 *
 * @param	state		The simulator state.
 * @param	cs		Card set.
 * @param	c		Card to be added to set (a copy will be made).
 */
static void AddCardToSetRandomly(State *state, CardSet *cs, const Card *c)
{
    int r = Rnd(state, cs->numCards+1);
    int i = 0;

    if (cs->numCards >= MAX_CARDS_IN_SET) {
	fprintf(stderr, "Too many cards\n");
	exit(1);
    }

    // Shift cards to the right to make room for new card at slot r.
    for (i=cs->numCards;i>r;i--) {
	cs->cards[i] = cs->cards[i-1];
    }

    // Insert card at r.
    cs->cards[r] = *c;
    cs->numCards++;
}

/**
 * Initializes the "default" state.  The default state is the master initial
 * state that will be copied to each state at the beginning of each simulation
 * run.  That way, we only have to look up the demon, cards, and runes once
 * to create the default state.  Then we simply make a copy of the default
 * state when we want to start over a new run.
 *
 * @param	state		The default state to create.
 */
static void InitDefaultState(State *state)
{
    int         i = 0;
    const Card *c = NULL;

    state->dmgDone = 0;
    state->hp      = initialHp;
    state->maxHp   = initialHp;
    state->round   = 1;

    // Look up demon.
    c = FindCard(theDemon);
    if (c == NULL) {
	fprintf(stderr, "Couldn't find demon card: %s\n", theDemon);
	exit(1);
    }
    memcpy(&state->demon, c, sizeof(Card));
    InitCard(&state->demon);

    // Look up cards.
    state->deck.numCards = 0;
    for (i=0;i<numDeckCards;i++) {
	c = FindCard(theDeck[i]);
	AddCardToSet(&state->deck, c);
	InitCard(&state->deck.cards[i]);
    }
    state->hand.numCards = 0;
    state->field.numCards = 0;
    state->grave.numCards = 0;

    // Look up runes.
    for (i=0;i<numRunes;i++) {
	const Rune *rune = FindRune(theRunes[i]);
	state->runes[i] = *rune;
	state->runes[i].chargesUsed   = 0;
	state->runes[i].usedThisRound = 0;
    }
    state->numRunes = i;
}

/**
 * Initializes a state in order to start a new simulation run.  This merely
 * copies the default state, but preserves the current rng seeds so that
 * we have new random numbers on each run.
 *
 * @param	state		The simulator state to initialize.
 */
static void InitState(State *state)
{
    unsigned int seedSaveW = state->seedW;
    unsigned int seedSaveZ = state->seedZ;

    memcpy(state, &defaultState, sizeof(State));
    state->seedW = seedSaveW;
    state->seedZ = seedSaveZ;
}

/**
 * Removes any dead cards from the field.   Note that these cards should
 * already be added to the grave or the deck before calling this function.
 *
 * @param	state		The simulator state.
 */
static void RemoveDeadCards(State *state)
{
    int      i = 0;
    CardSet *f = &state->field;

    for (i=0;i<f->numCards;i++) {
	if (HasAttr(&f->cards[i], ATTR_DEAD, NULL)) {
	    RemoveCardFromSet(f, i);
	    i--;
	}
    }
}

/**
 * Removes a buff from all cards on the field (except the card that originated
 * the buff).  This is called when a card dies and had a "force" or "guard"
 * type ability that added attack or hp to all other cards of a certain type.
 *
 * @param	state		The simulator state.
 * @param	c		The card that created the buff.  We need to
 *				know this so we can skip this card.
 * @param	buff		The attribute type to remove.
 * @param	level		The level of the buff to remove.  We need to
 *				know this because there could be many of the
 *				same type of buff on the card of different
 *				levels.
 */
static void RemoveBuffFromField(State *state, Card *c, int buff, int level)
{
    int      i = 0;
    CardSet *f = &state->field;

    for (i=0;i<f->numCards;i++) {
	Card *c2 = &f->cards[i];
	if (c == c2)
	    continue;
	if (HasAttr(c2, buff, NULL)) {
	    switch (buff) {
		case ATTR_TUNDRA_HP_BUFF:
		case ATTR_FOREST_HP_BUFF:
		case ATTR_MTN_HP_BUFF:
		case ATTR_SWAMP_HP_BUFF:
		{
		    int oldHp = c2->hp;

		    RemoveAttr(c2, buff, level);
		    c2->maxHp -= level;
		    if (c2->hp > c2->maxHp)
			c2->hp = c2->maxHp;
		    dprintf("Hp buff removed: %s loses %d max hp and %d hp "
			    "(now %d)\n", c2->name, level, oldHp - c2->hp,
			    c2->hp);
		    break;
		}
		case ATTR_TUNDRA_ATK_BUFF:
		case ATTR_FOREST_ATK_BUFF:
		case ATTR_MTN_ATK_BUFF:
		case ATTR_SWAMP_ATK_BUFF:
		{
		    RemoveAttr(c2, buff, level);
		    c2->atk        -= level;
		    c2->curBaseAtk -= level;
		    if (c2->atk < 0)
			c2->atk = 0;
		    if (c2->curBaseAtk < 0)
			c2->curBaseAtk = 0;
		    dprintf("Atk buff removed: %s loses %d atk and "
			    "base atk (now %d)\n", c2->name, level, c2->atk);
		    break;
		}
		default:
		    RemoveAttr(c2, buff, level);
		    break;
	    }
	}
    }
}

/**
 * Adds a buff to a card.
 *
 * @param	src		The card that created the buff.  This is
 *				only used for debug printing purposes.
 * @param	target		The card receiving the buff.
 * @param	buff		The attribute type of the buff.
 * @param	level		The level of the buff attribute.
 */
static void AddBuffToCard(Card *src, Card *target, int buff, int level)
{
    Attr attr = { buff, level };

    switch (buff) {
	case ATTR_TUNDRA_HP_BUFF:
	case ATTR_FOREST_HP_BUFF:
	case ATTR_MTN_HP_BUFF:
	case ATTR_SWAMP_HP_BUFF:
	{
	    target->hp    += level;
	    target->maxHp += level;
	    AddAttr(target, &attr);
	    dprintf("%s increases hp of %s by %d.\n", src->name, target->name,
		    level);
	    break;
	}
	case ATTR_TUNDRA_ATK_BUFF:
	case ATTR_FOREST_ATK_BUFF:
	case ATTR_MTN_ATK_BUFF:
	case ATTR_SWAMP_ATK_BUFF:
	{
	    target->atk        += level;
	    target->curBaseAtk += level;
	    AddAttr(target, &attr);
	    dprintf("%s increases atk and base atk of %s by %d "
		    "(now %d).\n", src->name, target->name, level, target->atk);
	    break;
	}
	default:
	    AddAttr(target, &attr);
	    break;
    }
}

/**
 * Adds a buff to all cards on the field of a certain class, except the card
 * that originated the buff.  This is used for abilities like "Forest Guard".
 *
 * @param	state		The simulator state.
 * @param	c		The card originating the buff.
 * @param	class		The class receiving the buff (e.g. ATTR_FOREST).
 *				If the class is ATTR_NONE, all cards will be
 *				affected.
 * @param	buff		The buff attribute type (e.g. ATTR_FOREST_HP).
 * @param	level		The level of the buff attribute.
 */
static void AddBuffToField(State *state, Card *c, int class,
	int buff, int level)
{
    int      i = 0;
    CardSet *f = &state->field;

    for (i=0;i<f->numCards;i++) {
	Card *c2 = &f->cards[i];
	if (c == c2)
	    continue;
	if (class == ATTR_NONE || HasAttr(c2, class, NULL))
	    AddBuffToCard(c, c2, buff, level);
    }
}

/**
 * Removes the given card from the field, and sends it to the graveyard (died)
 * or back to the deck (exiled).  This function will take care of removing any
 * hp/attack buffs caused by the card.  It will also handle any "Desperation"
 * type abilities.
 *
 * Note that the card isn't actually "removed" from the field.  It is replaced
 * with a "dead card" placeholder.  This is so that the cards don't shift
 * position, so that abilities that hit neighboring cards during this round
 * will hit the cards they are supposed to.
 *
 * @param	state		The simulator state.
 * @param	c		The card to remove from the field.
 * @param	sendToGraveyard	True to send the card to the graveyard.  False
 *				to send the card back to the deck.
 */
static void RemoveCard(State *state, Card *c, int sendToGraveyard)
{
    int      i           = 0;
    int      level       = 0;
    Card     copy;

    // Mark the card dead.
    c->hp = 0;
    AddAttr(c, &DeadAttr);

    // Remove all buffs caused by the card and handle Desperation abilities.
    for (i=0;i<c->numAttr;i++) {
	level = c->attr[i].level;
	switch (c->attr[i].type) {
	    case ATTR_TUNDRA_HP:
		RemoveBuffFromField(state, c, ATTR_TUNDRA_HP_BUFF, level);
		break;
	    case ATTR_FOREST_HP:
		RemoveBuffFromField(state, c, ATTR_FOREST_HP_BUFF, level);
		break;
	    case ATTR_MTN_HP:
		RemoveBuffFromField(state, c, ATTR_MTN_HP_BUFF, level);
		break;
	    case ATTR_SWAMP_HP:
		RemoveBuffFromField(state, c, ATTR_SWAMP_HP_BUFF, level);
		break;
	    case ATTR_TUNDRA_ATK:
		RemoveBuffFromField(state, c, ATTR_TUNDRA_ATK_BUFF, level);
		break;
	    case ATTR_FOREST_ATK:
		RemoveBuffFromField(state, c, ATTR_FOREST_ATK_BUFF, level);
		break;
	    case ATTR_MTN_ATK:
		RemoveBuffFromField(state, c, ATTR_MTN_ATK_BUFF, level);
		break;
	    case ATTR_SWAMP_ATK:
		RemoveBuffFromField(state, c, ATTR_SWAMP_ATK_BUFF, level);
		break;

	    case ATTR_D_PRAYER:
		if (sendToGraveyard)
		    SimPrayer(state, level);
		break;

	    case ATTR_D_REANIMATE:
		if (sendToGraveyard)
		    SimReanimate(state, "Desperation: Reanimated");
		break;

	    case ATTR_D_REINCARNATE:
		if (sendToGraveyard)
		    SimReincarnate(state, "Desperation: Reincarnated", level);
		break;

	    default:
		break;
	}
    }

    // Move the card to the graveyard or deck.
    copy = *c;
    InitCard(&copy);
    if (sendToGraveyard) {
	// Died.
	CardSet *destination = &state->grave;
	dprintf("%s died.\n", c->name);
	if (HasAttr(c, ATTR_DIRT, &level)) {
	    int r = Rnd(state, 100);
	    if (r < level) {
		if (state->hand.numCards >= MAX_CARDS_IN_HAND) {
		    dprintf("%s resurrected (Dirt) to deck because "
			    "hand is full.\n", c->name); 
		    destination = &state->deck;
		} else {
		    dprintf("%s resurrected (Dirt).\n", c->name); 
		    destination = &state->hand;
		}
	    }
	}
	if (HasAttr(c, ATTR_RESURRECTION, &level)) {
	    int r = Rnd(state, 100);
	    if (r < level) {
		if (state->hand.numCards >= MAX_CARDS_IN_HAND) {
		    dprintf("%s resurrected to deck because hand is full.\n",
			    c->name); 
		    destination = &state->deck;
		} else {
		    dprintf("%s resurrected.\n", c->name); 
		    destination = &state->hand;
		}
	    }
	}
	// When the resurrecting card goes to the deck because of a
	// full hand, does the card go to the front of the deck?
	AddCardToSet(destination, &copy);
    } else {
	// Exiled.
	CardSet *d = &state->deck;

	// Does an exiled card enter the deck randomly?
	dprintf("%s exiled.\n", c->name);
	AddCardToSetRandomly(state, d, &copy);
    }
    // Replace card on field with dead card.  This keeps all the other cards
    // in their position.  Dead cards are removed at the end of the round.
    *c = DeadCard;
}

/**
 * Picks N random cards out of a set of cards.  This is used currently only
 * for the demon's Trap ability.
 *
 * @param	state		The simulator state.
 * @param	cs		The card set.
 * @param	ret		An array of size MAX_CARDS_IN_SET.  This
 *				array will be filled with one card index
 *				per card picked.
 * @return			The number of cards picked.  This could be
 *				less than the number requested if there were
 *				not enough cards in the set.
 */
static int PickNCards(State *state, CardSet *cs, int n, int *ret)
{
    int   i        = 0;
    int   numAlive = 0;
    Card *c        = NULL;

    // First, count the number of alive cards in the set.
    for (i=0;i<cs->numCards;i++) {
	c = &cs->cards[i];
	if (c->hp > 0) {
	    ret[numAlive++] = i;
	}
    }

    // Limit to the actual number of alive cards.
    n = MIN(n, numAlive);

    // If all cards must be picked, this is simple.
    if (n == numAlive)
	return n;

    // Now randomly pick n cards out of numAlive cards.
    for (i=0;i<n;i++) {
	unsigned int r = Rnd(state, numAlive - 1 - i);

	if (r != 0) {
	    int tmp  = ret[i];
	    ret[i]   = ret[i+r];
	    ret[i+r] = tmp;
	}
    }

    // Sort the return set.
    for (i=0;i<n-1;i++) {
	int j           = 0;
	int lowest      = ret[i];
	int lowestIndex = 0;

	for (j=i+1;j<n;j++) {
	    if (ret[j] < lowest) {
		lowest = ret[j];
		lowestIndex = j;
	    }
	}
	if (ret[i] != lowest) {
	    int tmp           = ret[i];
	    ret[i]            = ret[lowestIndex];
	    ret[lowestIndex]  = tmp;
	}
    }
    
    return n;
}

/**
 * Simulates the demon Trap ability.
 *
 * @param	state		The simulator state.
 * @param	numToTrap	Number of cards to trap.
 */
static void SimDemonTrap(State *state, int numToTrap)
{
    CardSet *f = &state->field;
    int      trapped[MAX_CARDS_IN_SET];
    int      numTrapped = 0;
    int      i = 0;

    numTrapped = PickNCards(state, f, numToTrap, trapped);
    if (numTrapped == 0)
	return;
    for (i=0;i<numTrapped;i++) {
	int r = Rnd(state, 100);
	Card *c = &f->cards[trapped[i]];

	if (HasAttr(c, ATTR_IMMUNITY, NULL)) {
	    dprintf("%s not trapped because of immunity.\n", c->name);
	} else if (HasAttr(c, ATTR_EVASION, NULL)) {
	    dprintf("%s not trapped because of evasion.\n", c->name);
	} else if (r < 65) {
	    Attr trapAttr = { ATTR_TRAP_BUFF, 0 };
	    AddAttr(c, &trapAttr);
	    dprintf("%s trapped.\n", c->name);
	} else {
	    dprintf("%s not trapped.\n", c->name);
	}
    }
}

/**
 * Simulates demon doing damage to the player's hero.  This damage can
 * be absorbed by cards with the Guard ability.
 *
 * @param	state		The simulator state.
 * @param	dmg		Amount of damage.
 */
static void DamagePlayer(State *state, int dmg)
{
    int      i       = 0;
    int      level   = 0;
    CardSet *f       = &state->field;
    bool     newline = false;

    // Find cards with Guard to absorb damage.
    for (i=0;i<f->numCards;i++) {
	Card *c = &f->cards[i];
	if (HasAttr(c, ATTR_GUARD, &level)) {
	    int cardDmg = MIN(dmg, c->hp);

	    if (cardDmg > 0) {
		c->hp -= cardDmg;
		if (newline)
		    dprintf("        ");
		dprintf("%s absorbs %d (%d left).\n", c->name, cardDmg, c->hp);
		newline = true;
		if (c->hp <= 0) {
		    dprintf("        ");
		    RemoveCard(state, c, 1);
		}
		dmg -= cardDmg;
	    }
	}
    }

    // Any damage left over is applied to the player's hero.
    state->hp -= dmg;
    if (dmg > 0) {
	if (newline)
	    dprintf("        ");
	dprintf("Player takes %d dmg (%d left).\n", dmg, state->hp);
    }
}

/**
 * Reduces physical damage by the defending card's parry or ice shield.
 *
 * @param	c	The card taking damage.
 * @param	dmg	The amount of physical damage being taken.
 * @return		The reduced amount of damage (could be as low as 0).
 */
static int ReducePhysDmg(const Card *c, int dmg)
{
    int i = 0;
    for (i=0;i<c->numAttr;i++) {
	switch (c->attr[i].type) {
	    case ATTR_PARRY:
		dmg -= c->attr[i].level;
		if (dmg < 0)
		    dmg = 0;
		break;
	    case ATTR_STONEWALL:
		dmg -= c->attr[i].level;
		if (dmg < 0)
		    dmg = 0;
		break;
	    case ATTR_ICE_SHIELD:
	    case ATTR_ARCTIC_FREEZE:
		if (dmg > c->attr[i].level)
		    dmg = c->attr[i].level;
		break;
	    default:
		break;
	}
    }
    return dmg;
}

/**
 * Simulates demon lacerate ability.
 * 
 * @param	c	The card being lacerated.
 */
static void SimDemonLacerate(Card *c)
{
    if (!HasAttr(c, ATTR_LACERATE_BUFF, NULL)) {
	Attr lacerateAttr = { ATTR_LACERATE_BUFF, 0 };
	AddAttr(c, &lacerateAttr);
	dprintf("%s lacerated.\n", c->name);
    }
}

/**
 * Simulates demon doing damage to a player card.  This will handle any
 * damage mitigating abilities such as dodge/parry/ice shield, and also
 * any damage triggered abilities such craze/retaliatin.
 *
 * @param	state		The simulator state.
 * @param	c		The card taking damage.
 * @param	dmg		The amount of damage.
 * @return			The damage done to the card (used for chain
 *				attack).
 */
static int DamageCard(State *state, Card *c, int dmg)
{
    int i     = 0;
    int level = 0;

    // Apply damage avoidance and mitigation.
    if (HasAttr(c, ATTR_NIMBLE_SOUL, &level)) {
	int r = Rnd(state, 100);

	if (r < level) {
	    dprintf("%s dodged (nimble soul).\n", c->name);
	    return 0;
	}
    }
    if (HasAttr(c, ATTR_DODGE, &level)) {
	int r = Rnd(state, 100);

	if (r < level) {
	    dprintf("%s dodged.\n", c->name);
	    return 0;
	}
    }
    dmg = ReducePhysDmg(c, dmg);

    // If the damage is 0, it's as if nothing happened (no further effects
    // are triggered).  Otherwise, cause damage to the card.
    if (dmg > 0)
	c->hp -= dmg;
    else
	return 0;

    if (c->hp <= 0)
	c->hp = 0;
    dprintf("%s takes %d dmg (%d left).\n", c->name, dmg, c->hp);

    // Abilities triggered by damage.
    for (i=0;i<c->numAttr;i++) {
	level = c->attr[i].level;
	switch (c->attr[i].type) {
	    case ATTR_CRAZE:
		dprintf("Craze: %s +%d dmg\n", c->name, level);
		c->atk        += level;
		c->curBaseAtk += level;
		break;
	    case ATTR_TSUNAMI:
		dprintf("Tsunami: %s +%d dmg\n", c->name, level);
		c->atk        += level;
		c->curBaseAtk += level;
		break;
	    case ATTR_COUNTERATTACK:
	    case ATTR_RETALIATION:
		if (c->attr[i].type == ATTR_COUNTERATTACK)
		    dprintf("Counterattack: %d dmg\n", level);
		else
		    dprintf("Retaliation: %d dmg\n", level);
		state->dmgDone += level;
		state->demon.hp -= level;
		break;
	    case ATTR_THUNDER_SHIELD:
		dprintf("Thunder Shield: %d dmg\n", level);
		state->dmgDone += level;
		state->demon.hp -= level;
		break;
	    case ATTR_FIRE_FORGE:
		dprintf("Fire Forge: %d dmg\n", level);
		state->dmgDone += level;
		state->demon.hp -= level;
		break;
	    case ATTR_WICKED_LEECH:
	    {
		int atkLoss = (state->demon.curBaseAtk * level) / 100;
		state->demon.curBaseAtk -= atkLoss;
		state->demon.atk        -= atkLoss;
		c->atk                  += atkLoss;
		c->curBaseAtk           += atkLoss;
		dprintf("Wicked Leech: Steal %d atk (now %d) (demon now %d)\n",
			atkLoss, c->atk, state->demon.atk);
		break;
	    }
	    default:
		break;
	}
    }
    // Check for card death.
    if (c->hp == 0)
	RemoveCard(state, c, 1);
    // Lacerate the card, if the demon has lacerate.
    if (c->hp > 0 && HasAttr(&state->demon, ATTR_LACERATE, NULL))
	SimDemonLacerate(c);
    return dmg;
}

/**
 * Simulates the demon's physical attack.  This will either hit the leftmost
 * card, or if there is no leftmost card it will hit the player's hero.
 *
 * @param	state		The simulator state.
 * @param	dmg		The amount of damage.
 */
static void SimDemonAttack(State *state, int dmg)
{
    CardSet *f      = &state->field;
    
    dprintf("Attack: %d dmg.  ", dmg);
    if (f->numCards > 0) {
	Card       *c        = &f->cards[0];
	int         level    = 0;
	const char *cardName = c->name;

	// If the leftmost card is not dead, hit the leftmost card.
	if (!HasAttr(c, ATTR_DEAD, NULL)) {
	    // Card hit.
	    int newDmg = DamageCard(state, c, dmg);

	    // If the demon has Chain Attack, it does extra damage to each card
	    // of the same name.
	    if (newDmg > 0 && HasAttr(&state->demon, ATTR_CHAIN_ATTACK, &level)){
		int i = 0;

		// The chain attack damage is normally greater than the
		// initial hit.
		newDmg = (newDmg * level) / 100;

		// Look for cards with the same name.
		for (i=1;i<f->numCards;i++) {
		    Card *c2 = &f->cards[i];
		    if (!HasAttr(c2, ATTR_DEAD, NULL) && c2->hp > 0 &&
			    !strcmp(cardName, c2->name)) {
			// Found a card with the same name.  Apply newDmg.
			dprintf("Chain attack on %s for %d damage.\n",
				c2->name, newDmg);
			DamageCard(state, c2, newDmg);
		    }
		}
	    }
	    return;
	}
    }

    // Player hit.
    DamagePlayer(state, dmg);
}

/**
 * Simulates the demon's round.
 *
 * @param	state		The simulator state.
 */
static void SimDemon(State *state)
{
    int      i = 0;
    Card    *d = &state->demon;
    CardSet *f = &state->field;

    if (state->round < FIRST_DEMON_ROUND)
	return;
    else if (state->round == FIRST_DEMON_ROUND)
	dprintf("%s appears.\n", d->name);

    vprintf("%s's turn:\n", d->name);

    // At round 51, the player starts taking unavoidable damage.
    if (state->round >= 51) {
	int dmg = ((state->round - 51) / 2) * 60 + 80;

	dmg = MIN(dmg, state->hp);
	state->hp -= dmg;
	dprintf("Player takes %d unavoidable damage (%d left)\n",
		dmg, state->hp);
    }

    // Handle demon abilities.
    for (i=0;i<d->numAttr;i++) {
	if (state->hp <= 0)
	    break;
	switch (d->attr[i].type) {
	    case ATTR_CURSE:
	    {
		int dmg = d->attr[i].level;
		dprintf("Curse : %d dmg.  ", dmg);
		DamagePlayer(state, dmg);
		break;
	    }
	    case ATTR_DAMNATION:
	    {
		int dmg = d->attr[i].level * state->field.numCards;

		if (dmg > 0) {
		    dprintf("Damnation: %d dmg.  ", dmg);
		    DamagePlayer(state, dmg);
		}
		break;
	    }
	    case ATTR_EXILE:
		if (f->numCards > 0) {
		    Card *c = &f->cards[0];

		    if (c->hp > 0) {
			dprintf("Exile cast on %s.\n", c->name);
			if (!HasAttr(c, ATTR_RESISTANCE, NULL) &&
				!HasAttr(c, ATTR_IMMUNITY, NULL)) {
			    RemoveCard(state, c, 0);
			}
		    } else {
			dprintf("%s resisted Exile.\n", c->name);
		    }
		}
		break;
	    case ATTR_SNIPE:
	    {
		Card *c   = FindLowestHpCard(state, f, false);
		int   dmg = d->attr[i].level;

		if (c == NULL)
		    break;
		dmg = MIN(dmg, c->hp);
		dprintf("Devil's blade: %d dmg to %s.\n", dmg, c->name);
		c->hp -= dmg;
		if (c->hp == 0)
		    RemoveCard(state, c, 1);
		break;
	    }
	    case ATTR_MANA_CORRUPT:
	    {
		int   r   = 0;
		Card *c   = NULL;
		int   dmg = d->attr[i].level;

		r = PickAliveCardFromSet(state, f);
		if (r == -1)
		    break;

		c = &f->cards[r];
		if (HasAttr(c, ATTR_REFLECTION, NULL) ||
			HasAttr(c, ATTR_IMMUNITY, NULL))
		    dmg *= 3;
		dmg = MIN(dmg, c->hp);
		dprintf("Mana corrupt: %d dmg to %s.\n", dmg, c->name);
		c->hp -= dmg;
		if (c->hp == 0)
		    RemoveCard(state, c, 1);
		break;
	    }
	    case ATTR_DESTROY:
	    {
		int   r = 0;
		Card *c = NULL;

		r = PickAliveCardFromSet(state, f);
		if (r == -1)
		    break;

		c = &f->cards[r];
		dprintf("Destroy cast on %s.\n", c->name);
		if (!HasAttr(c, ATTR_RESISTANCE, NULL) &&
			!HasAttr(c, ATTR_IMMUNITY, NULL)) {
		    c->hp = 0;
		    RemoveCard(state, c, 1);
		} else {
		    dprintf("%s resisted Destroy.\n", c->name);
		}
		break;
	    }
	    case ATTR_FIRE_GOD:
	    {
		int j;
		for (j=0;j<f->numCards;j++) {
		    Card *c = &f->cards[j];
		    if (c->hp <= 0)
			continue;
		    if (HasAttr(c, ATTR_IMMUNITY, NULL)) {
			dprintf("%s immune to Fire God.\n", c->name);
		    } else if (!HasAttr(c, ATTR_FIRE_GOD, NULL)) {
			dprintf("Fire God cast on %s.\n", c->name);
			AddAttr(c, &d->attr[i]);
		    }
		}
		break;
	    }
	    case ATTR_TOXIC_CLOUDS:
	    {
		int j;
		for (j=0;j<f->numCards;j++) {
		    int dmg = d->attr[i].level;
		    Card *c = &f->cards[j];

		    if (c->hp <= 0)
			continue;
		    if (HasAttr(c, ATTR_IMMUNITY, NULL)) {
			dprintf("%s immune to Toxic Clouds.\n", c->name);
			break;
		    }
		    dmg = MIN(dmg, c->hp);
		    c->hp -= dmg;
		    dprintf("Toxic clouds does %d dmg to %s "
			    "(%d hp left).\n", dmg, c->name, c->hp);
		    if (c->hp <= 0)
			RemoveCard(state, c, 1);
		    else if (!HasAttr(c, ATTR_TOXIC_CLOUDS, NULL))
			AddAttr(c, &d->attr[i]);
		}
		break;
	    }
	    case ATTR_TRAP:
		SimDemonTrap(state, d->attr[i].level);
		break;
	    default:
		break;
	}
    }

    if (state->hp > 0) {
	int atk   = d->atk;
	int level = 0;

	// Handle demon attack buffs.
	if (HasAttr(d, ATTR_HOT_CHASE, &level)) {
	    // Hot chase: adds attack for each card in graveyard.
	    int numCards = state->grave.numCards;

	    level *= numCards;
	    if (level > 0) {
		atk += level;
		dprintf("Hot Chase: Demon attack +%d (now %d).\n", level, atk);
	    }
	}

	// Handle demon physical attack.
	SimDemonAttack(state, atk);
    }

#if 0
    // Post attack abilities.
    for (i=0;i<d->numAttr;i++) {
	switch (d->attr[i].type) {
	    default:
		break;
	}
    }
#endif

    RemoveDeadCards(state);
}

/**
 * On each round, we need to decrease the timers on all card in the hand.
 *
 * @param	state		The simulator state.
 */
static void DecreaseTimers(State *state)
{
    CardSet *h = &state->hand;
    int      i = 0;

    for (i=0;i<h->numCards;i++) {
	Card *c = &h->cards[i];

	if (c->curTiming > 0)
	    c->curTiming--;
    }
}

/**
 * On the player's turn, we play one card from the deck to the hand.
 *
 * @param	state		The simulator state.
 */
static void PlayCardsFromDeck(State *state)
{
    CardSet *d = &state->deck;
    CardSet *h = &state->hand;

    if (d->numCards > 0 && h->numCards >= MAX_CARDS_IN_HAND) {
	dprintf("Hand is full.  No card played to hand this turn\n");
	return;
    }

    // Cards are played from the end of the deck because reincarnated cards
    // get put there.
    if (d->numCards > 0) {
	Card *c = &d->cards[d->numCards-1];

	vprintf("%s dealt to hand.\n", c->name);
	AddCardToSet(h, c);
	RemoveCardFromSet(d, d->numCards-1);
    }
}

/**
 * If a card is played from the hand to the field, then handle the buffs
 * that are caused by the new card.
 *
 * @param	state		The simulator state.
 * @param	c		The card just played to the field.
 */
static void HandleBuffsFromCardPlayed(State *state, Card *c)
{
    int i     = 0;
    int level = 0;

    for (i=0;i<c->numAttr;i++) {
	level = c->attr[i].level;
	switch (c->attr[i].type) {
	    case ATTR_TUNDRA_HP:
		AddBuffToField(state, c,ATTR_TUNDRA,ATTR_TUNDRA_HP_BUFF, level);
		break;
	    case ATTR_FOREST_HP:
		AddBuffToField(state, c,ATTR_FOREST,ATTR_FOREST_HP_BUFF, level);
		break;
	    case ATTR_MTN_HP:
		AddBuffToField(state, c,ATTR_MTN,   ATTR_MTN_HP_BUFF,    level);
		break;
	    case ATTR_SWAMP_HP:
		AddBuffToField(state, c,ATTR_SWAMP, ATTR_SWAMP_HP_BUFF,  level);
		break;
	    case ATTR_TUNDRA_ATK:
		AddBuffToField(state, c,ATTR_TUNDRA,ATTR_TUNDRA_ATK_BUFF,level);
		break;
	    case ATTR_FOREST_ATK:
		AddBuffToField(state, c,ATTR_FOREST,ATTR_FOREST_ATK_BUFF,level);
		break;
	    case ATTR_MTN_ATK:
		AddBuffToField(state, c,ATTR_MTN,   ATTR_MTN_ATK_BUFF,   level);
		break;
	    case ATTR_SWAMP_ATK:
		AddBuffToField(state, c,ATTR_SWAMP, ATTR_SWAMP_ATK_BUFF, level);
		break;
	    default:
		break;
	}
    }
}

/**
 * If a card is played from the hand to the field, then handle the abilities
 * that are triggered from that (QuickStrike, buffs, etc).
 *
 * @param	state		The simulator state.
 * @param	c		The card just played to the field.
 */
static void CardPlayedToField(State *state, Card *c)
{
    int      class       = ATTR_NONE;
    int      attrHp      = ATTR_NONE;
    int      attrHpBuff  = ATTR_NONE;
    int      attrAtk     = ATTR_NONE;
    int      attrAtkBuff = ATTR_NONE;
    int      i           = 0;
    int      level       = 0;
    CardSet *f           = &state->field;

    if (HasAttr(c, ATTR_OBSTINACY, &level)) {
	dprintf("Obstinacy: -%d hp\n", level);
	state->hp -= level;
    }

    if (HasAttr(c, ATTR_BACKSTAB, &level)) {
	Attr bsBuff = { ATTR_BACKSTAB_BUFF, level };
	c->atk += level;
	dprintf("%s backstab +%d attack (now %d).\n", c->name, level, c->atk);
	AddAttr(c, &bsBuff);
    }

    if (HasAttr(c, ATTR_QS_PRAYER, &level))
	SimPrayer(state, level);

    if (HasAttr(c, ATTR_QS_REGENERATE, &level))
	SimRegenerate(state, c->name, level);

    if (HasAttr(c, ATTR_QS_REINCARNATE, &level))
	SimReincarnate(state, "QS Reincarnated", level);

    if (HasAttr(c, ATTR_SACRIFICE, &level) && f->numCards > 1) {
	int    r = Rnd(state, f->numCards - 1);
	Card *c2 = &f->cards[r];

	if (HasAttr(c2, ATTR_IMMUNITY, NULL)) {
	    dprintf("%s attempts to sacrifice %s but fails.\n", c->name,
		    c2->name);
	} else {
	    int atkIncrease = (c->atk * level) / 100;
	    int hpIncrease  = (c->hp  * level) / 100;

	    c->atk        += atkIncrease;
	    c->curBaseAtk += atkIncrease;
	    c->hp         += hpIncrease;
	    c->maxHp      += hpIncrease;
	    dprintf("%s sacrifices %s.  Atk +%d (now %d).  Hp +%d (now %d).\n",
		    c->name, c2->name, atkIncrease, c->atk, hpIncrease, c->hp);
	    c2->hp = 0;
	    RemoveCard(state, c2, 1);
	    RemoveDeadCards(state);
	}
    }

    // This part handles the new card receiving buffs from cards already on
    // the field.
    if (HasAttr(c, ATTR_TUNDRA, NULL)) {
	class       = ATTR_TUNDRA;
	attrHp      = ATTR_TUNDRA_HP;
	attrHpBuff  = ATTR_TUNDRA_HP_BUFF;
	attrAtk     = ATTR_TUNDRA_ATK;
	attrAtkBuff = ATTR_TUNDRA_ATK_BUFF;
    } else if (HasAttr(c, ATTR_FOREST, NULL)) {
	class       = ATTR_FOREST;
	attrHp      = ATTR_FOREST_HP;
	attrHpBuff  = ATTR_FOREST_HP_BUFF;
	attrAtk     = ATTR_FOREST_ATK;
	attrAtkBuff = ATTR_FOREST_ATK_BUFF;
    } else if (HasAttr(c, ATTR_MTN, NULL)) {
	class       = ATTR_MTN;
	attrHp      = ATTR_MTN_HP;
	attrHpBuff  = ATTR_MTN_HP_BUFF;
	attrAtk     = ATTR_MTN_ATK;
	attrAtkBuff = ATTR_MTN_ATK_BUFF;
    } else if (HasAttr(c, ATTR_SWAMP, NULL)) {
	class       = ATTR_SWAMP;
	attrHp      = ATTR_SWAMP_HP;
	attrHpBuff  = ATTR_SWAMP_HP_BUFF;
	attrAtk     = ATTR_SWAMP_ATK;
	attrAtkBuff = ATTR_SWAMP_ATK_BUFF;
    }

    if (class != ATTR_NONE) {
	for (i=0;i<f->numCards;i++) {
	    Card *c2 = &f->cards[i];
	    if (c == c2)
		continue;
	    if (HasAttr(c2, attrHp, &level))
		AddBuffToCard(c2, c, attrHpBuff, level);
	    if (HasAttr(c2, attrAtk, &level))
		AddBuffToCard(c2, c, attrAtkBuff, level);
	}
    }

    // This part handles applying buffs from the new card to other
    // cards on the field.
    HandleBuffsFromCardPlayed(state, c);
}

/**
 * Plays all cards from the hand that are at timing 0 to the field.
 *
 * @param	state		The simulator state.
 */
static void PlayCardsFromHand(State *state)
{
    CardSet *h = &state->hand;
    CardSet *f = &state->field;
    int      i = 0;

    for (i=0;i<h->numCards;i++) {
	Card *c = &h->cards[i];
	if (c->curTiming <= 0) {
	    Card c2 = *c;
	    RemoveCardFromSet(h, i);
	    AddCardToSet(f, &c2);
	    CardPlayedToField(state, &f->cards[f->numCards-1]);
	    i--;
	}
    }
}

/**
 * Simulates the advanced strike ability.  This ability reduces the card
 * with the highest timing by 1.
 * 
 * @param	state		The simulator state.
 */
static void SimAdvancedStrike(State *state)
{
    CardSet *h          = &state->hand;
    Card    *c          = NULL;
    int      i          = 0;
    int      highTiming = -1;
    int      highIndex  = -1;

    for (i=0;i<h->numCards;i++) {
	c = &h->cards[i];
	if (c->curTiming > highTiming) {
	    highTiming = c->curTiming;
	    highIndex  = i;
	}
    }
    if (highIndex != -1) {
	c = &h->cards[highIndex];
	if (c->curTiming > 0) {
	    c->curTiming--;
	    dprintf("Advanced strike: %s timing lowered to %d.\n",
		    c->name, c->curTiming);
	}
    }
}

/**
 * Heals one card (due to regenerate or healing).
 *
 * @param	c	The card to heal.
 * @param	name	The name of the card doing the healing.  Used only
 *			for debug printing.
 * @param	heal	The amount to heal.
 */
static void HealOneCard(Card *c, const char *name, int heal)
{
    if (HasAttr(c, ATTR_LACERATE_BUFF, NULL) || HasAttr(c, ATTR_IMMUNITY, NULL))
	return;
    if (c->hp > 0 && c->hp < c->maxHp) {
	int amount = MIN(heal, c->maxHp - c->hp);
	c->hp += amount;
	dprintf("%s healed %s for %d.\n", name, c->name, amount);
    }
}

/**
 * Simulates the regenerate ability.
 *
 * @param	state	The simulator state.
 * @param	name	The name of the card doing the healing.
 * @param	heal	The amount to heal.
 */
static void SimRegenerate(State *state, const char *name, int heal)
{
    CardSet *f = &state->field;
    int      i = 0;

    for (i=0;i<f->numCards;i++) {
	Card *c = &f->cards[i];
	HealOneCard(c, name, heal);
    }
}

/**
 * Simulates the reincarnate ability.
 *
 * @param	state	The simulator state.
 * @param	name	The name of the ability.  This is used for debug
 *			printing to distinguish between normal reincarnate
 *			and the Desperation or QuickStrike one.
 * @param	level	The level (number of cards to reincarnate).
 */
static void SimReincarnate(State *state, const char *attrName, int level)
{
    int      i = 0;
    CardSet *g = &state->grave;
    CardSet *d = &state->deck;
    Card c2;

    for (i=0;i<level;i++) {
	if (g->numCards == 0)
	    break;
	c2 = g->cards[0];
	RemoveCardFromSet(g, 0);
	AddCardToSet(d, &c2);
	dprintf("%s %s.\n", attrName, c2.name);
    }
}

/**
 * Returns the index of a live card from a set, or -1 if there are none.
 *
 * @param	state		The simulator state.
 * @param	cs		Card set to find live card in.
 * @return			Index of live card, or -1 if there are none.
 */
static int PickAliveCardFromSet(State *state, const CardSet *cs)
{
    int         i     = 0;
    int         count = 0;
    int         r     = 0;
    const Card *c     = NULL;

    if (cs->numCards == 0)
	return -1;

    // First, count the number of alive cards.
    for (i=0;i<cs->numCards;i++) {
	c = &cs->cards[i];
	if (c->hp <= 0)
	    continue;
	count++;
    }

    if (count == 0)
	return -1;

    // Now, count holds the number of alive cards on the field.
    // Pick a random one from that many.
    r = Rnd(state, count);

    // Find that card, skipping over the ones that couldn't be reanimated.
    for (i=0;i<cs->numCards;i++) {
	c = &cs->cards[i];
	if (c->hp <= 0)
	    continue;
	if (r == 0) {
	    // Found the card.
	    return i;
	}
	r--;
    }
    return -1;
}

/**
 * Returns the index in the grave of a random reanimatable card, or -1 if
 * there are none.
 *
 * @param	state		The simulator state.
 * @return			The index of a random reanimatable card, or -1
 *				if there are none.
 */
static int PickReanimatableCard(State *state)
{
    CardSet *g     = &state->grave;
    int      i     = 0;
    int      count = 0;
    int      r     = 0;
    Card    *c     = NULL;

    if (g->numCards == 0)
	return -1;

    // First, count the number of reanimatable cards.
    for (i=0;i<g->numCards;i++) {
	c = &g->cards[i];
	if (HasAttr(c, ATTR_REANIMATE, NULL) ||
		HasAttr(c, ATTR_D_REANIMATE, NULL) ||
		HasAttr(c, ATTR_IMMUNITY, NULL)) {
	    // Can't reanimate, do not add to count.
	    continue;
	}
	count++;
    }

    if (count == 0)
	return -1;

    // Now, count holds the number of cards that can be reanimated.
    // Pick a random one from that many.
    r = Rnd(state, count);

    // Find that card, skipping over the ones that couldn't be reanimated.
    for (i=0;i<g->numCards;i++) {
	c = &g->cards[i];
	if (HasAttr(c, ATTR_REANIMATE, NULL) ||
		HasAttr(c, ATTR_D_REANIMATE, NULL) ||
		HasAttr(c, ATTR_IMMUNITY, NULL)) {
	    // Can't reanimate, do not add to count.
	    continue;
	}
	if (r == 0) {
	    // Found the card.
	    return i;
	}
	r--;
    }
    return -1;
}

/**
 * Simulates the reanimate ability.
 *
 * @param	state		The simulator state.
 * @param	attrName	Attribute name (used for debug printing).
 */
static void SimReanimate(State *state, const char *attrName)
{
    CardSet *f        = &state->field;
    CardSet *g        = &state->grave;
    int      r        = 0;
    Card    *c        = NULL;
    Attr     sickAttr = { ATTR_REANIM_SICKNESS, 0 };
    Card     c2;

    r = PickReanimatableCard(state);
    if (r == -1)
	return;

    c2 = g->cards[r];
    RemoveCardFromSet(g, r);
    c2.curTiming = 0;

    // Add card to field, but with reanimation sickness so it won't take a
    // turn this turn.
    AddCardToSet(f, &c2);
    c = &f->cards[f->numCards-1];
    AddAttr(c, &sickAttr);
    dprintf("%s %s.\n", attrName, c->name);
    CardPlayedToField(state, c);
}

/**
 * Finds the lowest hp card or the most damaged card in the given set.
 * If there are more than one card that are equal, a random card is returned.
 * This may not be correct.  The game might always pick the leftmost card.
 *
 * @param	state		The simulator state.
 * @param	cs		The card set.
 * @param	mostDamaged	True to find the most damaged card.  False
 *				to find the lowest hp card.
 * @return			Pointer to the card found, or NULL if there
 *				is no such card.
 */
static Card *FindLowestHpCard(State *state, CardSet *cs, bool mostDamaged)
{
    int   i         = 0;
    int   r         = 0;
    int   lowest    = -1;
    int   numLowest = 0;
    int   value     = 0;
    Card *c         = NULL;
    Card *lowC      = NULL;

    // First find the lowest value and number of cards that share that value.
    // Most Damaged: find card that took the most damage.
    // Otherwise   : find card that has the lowest hp.
    for (i=0;i<cs->numCards;i++) {
	bool isLowest = false;

	c = &cs->cards[i];
	if (mostDamaged) {
	    value = c->maxHp - c->hp;
	    isLowest = (c->hp > 0 && (lowest == -1 || value > lowest));
	} else {
	    value = c->hp;
	    isLowest = (c->hp > 0 && (lowest == -1 || value < lowest));
	}
	if (isLowest) {
	    lowest = value;
	    numLowest = 1;
	    lowC      = c;
	} else if (value == lowest) {
	    numLowest++;
	}
    }

    if (numLowest == 0)
	return NULL;

    if (numLowest == 1)
	return lowC;

    // There are 2 or more cards with the same value.  Pick one at random.
    if (!mostDamaged)
	r = numLowest-1;
    else
	r = Rnd(state, numLowest);
    for (i=0;i<cs->numCards;i++) {
	if (mostDamaged) {
	    value = c->maxHp - c->hp;
	} else {
	    value = c->hp;
	}
	if (value == lowest) {
	    if (r == 0)
		return c;
	    else
		r--;
	}
    }

    // It should never get here.
    return lowC;
}

/**
 * Simulate the healing ability.
 *
 * @param	state		The simulator state.
 * @param	name		Name of card doing the healing.
 * @param	heal		The amount to heal.
 */
static void SimHealing(State *state, const char *name, int heal)
{
    CardSet *f = &state->field;
    Card    *c = NULL;

    c = FindLowestHpCard(state, f, true);
    if (c != NULL)
	HealOneCard(c, name, heal);
}

/**
 * Simulate the prayer ability.
 *
 * @param	state		The simulator state.
 * @param	heal		The amount to heal the hero.
 */
static void SimPrayer(State *state, int heal)
{
    if (state->hp > 0 && state->hp < state->maxHp) {
	int amount = MIN(heal, state->maxHp - state->hp);
	state->hp += amount;
	dprintf("Prayer healed %d.\n", amount);
    }
}

/**
 * Simulate the leftmost card's physical attack on the demon.
 *
 * @param	state		The simulator state.
 */
static void SimPlayerAttack(State *state)
{
    CardSet *f        = &state->field;
    Card    *c        = &f->cards[0];
    int      level    = 0;
    int      dmg      = 0;
    int      baseAtk  = c->curBaseAtk;
    int      i        = 0;
    int      increase = 0;
    
    if (f->numCards == 0)
	return;

    if (state->round < FIRST_PLAYER_ROUND)
	return;

    dmg = c->atk;

    // Find any attributes that can modify base attack first.
    for (i=0;i<c->numAttr;i++) {
	level = c->attr[i].level;
	switch (c->attr[i].type) {
	    case ATTR_REVIVAL:
		dmg += level;
		baseAtk += level;
		dprintf("Revival: Dmg increased by %d to %d.\n", level, dmg);
		dprintf("Revival: Base dmg increased by %d to %d.\n",
			level, baseAtk);
		break;
	    default:
		break;
	}
    }

    // Now apply pre-attack attributes.
    for (i=0;i<c->numAttr;i++) {
	level = c->attr[i].level;
	switch (c->attr[i].type) {
	    case ATTR_VENDETTA:
		increase = state->grave.numCards * level;
		if (increase > 0) {
		    dmg += increase;
		    dprintf("Vendetta: dmg increased by %d to %d.\n",
			    increase, dmg);
		}
		break;
	    case ATTR_WARPATH:
		increase = (baseAtk * level) / 100;
		dmg += increase;
		dprintf("Warpath: dmg increased by %d to %d.\n", increase, dmg);
		break;
	    case ATTR_LORE:
		increase = (baseAtk * level) / 100;
		dmg += increase;
		dprintf("Lore: dmg increased by %d to %d.\n", increase, dmg);
		break;
	    case ATTR_CONCENTRATE:
		if (avgConcentrate) {
		    increase = (baseAtk * level) / 200;
		    dmg += increase;
		    dprintf("Concentrate: dmg increased by %d to %d (AVG).\n",
			    increase, dmg);
		} else if (Rnd(state, 100) < 50) {
		    increase = (baseAtk * level) / 100;
		    dmg += increase;
		    dprintf("Concentrate: dmg increased by %d to %d.\n",
			    increase, dmg);
		}
		break;
	    case ATTR_FROST_BITE:
		if (avgConcentrate) {
		    increase = (baseAtk * level) / 200;
		    dmg += increase;
		    dprintf("Frost bite: dmg increased by %d to %d (AVG).\n",
			    increase, dmg);
		} else if (Rnd(state, 100) < 50) {
		    increase = (baseAtk * level) / 100;
		    dmg += increase;
		    dprintf("Frost bite: dmg increased by %d to %d.\n",
			    increase, dmg);
		}
		break;
	    default:
		break;
	}
    }

    dmg = ReducePhysDmg(&state->demon, dmg);

    dprintf("%s attacks for %d dmg.\n", c->name, dmg);
    state->dmgDone  += dmg;
    state->demon.hp -= dmg;

    // If no damage was done, do not apply any further effects.
    if (dmg <= 0)
	return;

    // Now apply post-attack attributes.
    for (i=0;i<c->numAttr;i++) {
	level = c->attr[i].level;
	switch (c->attr[i].type) {
	    case ATTR_BLOODSUCKER:
		increase = (dmg * level) / 100;
		increase = MIN(increase, c->maxHp - c->hp);
		if (c->hp > 0 && increase > 0) {
		    c->hp += increase;
		    dprintf("Bloodsucker: %s heals %d (%d hp).\n", c->name,
			    increase, c->hp);
		}
		break;
	    case ATTR_RED_VALLEY:
		increase = (dmg * level) / 100;
		increase = MIN(increase, c->maxHp - c->hp);
		if (c->hp > 0 && increase > 0) {
		    c->hp += increase;
		    dprintf("Red valley: %s heals %d (%d hp).\n", c->name,
			    increase, c->hp);
		}
		break;
	    case ATTR_BLOODTHIRSTY:
		c->atk        += level;
		c->curBaseAtk += level;
		dprintf("Bloodthirsty: %s attack increases by %d (now %d).\n",
			c->name, level, c->atk);
		break;
	    default:
		break;
	}
    }

    // Check for demon counterattack or retaliation.
    {
	int numCardsToCounter = 0;
	int i   = 0;
	int dmg = 0;

	if (HasAttr(&state->demon, ATTR_RETALIATION, &level)) {
	    numCardsToCounter = 2;
	} else if (HasAttr(&state->demon, ATTR_COUNTERATTACK, &level)) {
	    numCardsToCounter = 1;
	}
	for (i=0;i<numCardsToCounter;i++) {
	    int attributeLevel = 0;	// for dex
	    Card *c2 = &f->cards[i];
	    if (f->numCards <= i)
		break;
	    if (c2->hp <= 0)
		continue;
	    // start Added_dexterity
	    if (HasAttr(c2, ATTR_DEXTERITY, &attributeLevel)){
		if (Rnd(state, 100) < attributeLevel) {
			dprintf("Dexterity: %s dodges the counter.\n",
			c2->name);
			continue;
		}
	    }
	    // end Added_dexterity		
	    dmg = MIN(level, c2->hp);
	    c2->hp -= dmg;
	    dprintf("Demon counterattack hits %s for %d dmg.\n", c2->name, dmg);
	    if (c2->hp <= 0)
		RemoveCard(state, c2, 1);
	}
    }

    // If the card died, don't continue.
    if (f->cards[0].hp <= 0)
	return;

    // If the demon has wicked leech, handle that now.
    if (HasAttr(&state->demon, ATTR_WICKED_LEECH, &level)) {
	int atkLoss = (c->curBaseAtk * level) / 100;

	c->atk        -= atkLoss;
	c->curBaseAtk -= atkLoss;
	if (c->atk < 0)
	    c->atk = 0;
	state->demon.curBaseAtk += atkLoss;
	state->demon.atk        += atkLoss;
	dprintf("Wicked leech: %s loses %d atk (now %d), "
		"demon gains %d atk (now %d).\n",
		c->name, atkLoss, c->atk, atkLoss, state->demon.atk);
    }
}

/**
 * Simulates a player card.
 *
 * @param	state		The simulator state.
 * @param	cardNum		Index of card on the field.
 */
static void SimPlayerCard(State *state, int cardNum)
{
    CardSet *f       = &state->field;
    Card    *c       = NULL;
    int      i       = 0;
    bool     trapped = false;

    // Handle all attrs before attack.
    c = &f->cards[cardNum];

    // If this is a dead card, skip it.
    if (c->hp <= 0)
	return;

    vprintf("%s's turn:\n", c->name);

    // Cards that have just been reanimated don't get a turn.
    if (HasAttr(c, ATTR_REANIM_SICKNESS, NULL)) {
	RemoveAttr(c, ATTR_REANIM_SICKNESS, -1);
	return;
    }
    // Cards that have been trapped don't get a turn.
    if (HasAttr(c, ATTR_TRAP_BUFF, NULL)) {
	dprintf("Trap removed from %s.\n", c->name);
	RemoveAttr(c, ATTR_TRAP_BUFF, -1);
	trapped = true;
	goto SkipAttack;
    }

    for (i=0;i<c->numAttr;i++) {
	int level = c->attr[i].level;
	switch (c->attr[i].type) {
	    case ATTR_ADVANCED_STRIKE:
		SimAdvancedStrike(state);
		break;

	    case ATTR_REINCARNATE:
		SimReincarnate(state, "Reincarnated", level);
		break;

	    case ATTR_REANIMATE:
		SimReanimate(state, "Reanimated");
		break;

	    case ATTR_REGENERATE:
		SimRegenerate(state, c->name, level);
		break;
	    case ATTR_HEALING:
		SimHealing(state, c->name, level);
		break;
	    case ATTR_PRAYER:
		SimPrayer(state, level);
		break;
	    case ATTR_SNIPE:
	    case ATTR_MANA_CORRUPT:
	    case ATTR_FLYING_STONE:
		if (state->round >= FIRST_PLAYER_ROUND) {
		    if (c->attr[i].type == ATTR_SNIPE) {
			dprintf("Snipe: %d dmg\n", level);
		    } else if (c->attr[i].type == ATTR_MANA_CORRUPT) {
			level *= 3;
			dprintf("Mana Corrupt: %d dmg\n", level);
		    } else {
			dprintf("Flying Stone: %d dmg\n", level);
		    }
		    state->dmgDone += level;
		    state->demon.hp -= level;
		}
		break;
	    case ATTR_BITE:
#if 0
		if (state->round >= FIRST_PLAYER_ROUND) {
		    state->dmgDone += level;
		    state->demon.hp -= level;
		    c->hp += level;
		    if (c->hp > c->maxHp)
			c->hp = c->maxHp;
		    dprintf("Bite: %d dmg, healed to %d hp.\n", level, c->hp);
		}
#else
		dprintf("Bite: Demon is immune.\n");
#endif
		break;
	    case ATTR_MANIA:
		c->hp         -= level;
		c->atk        += level;
		c->curBaseAtk += level;
		if (c->hp < 0)
		    c->hp = 0;
		dprintf("Mania: -%d hp (to %d), +%d atk (to %d).\n",
			level, c->hp, level, c->atk);
		if (c->hp == 0)
		    RemoveCard(state, c, 1);
		break;
	    default:
		break;
	}
    }

    if (cardNum == 0) {
	if (c->hp > 0)
	    SimPlayerAttack(state);
	c = &f->cards[0];
    }

    // If the card died, don't do any more.
    if (c->hp <= 0)
	return;

SkipAttack:
    // Handle damaging statuses after attack.
    for (i=0;i<c->numAttr;i++) {
	int level = c->attr[i].level;
	switch (c->attr[i].type) {
	    case ATTR_FIRE_GOD:
	    case ATTR_TOXIC_CLOUDS:
	    {
		level = MIN(level, c->hp);
		if (level >= 0) {
		    c->hp -= level;
		    if (c->attr[i].type == ATTR_FIRE_GOD) {
			dprintf("Fire God does %d dmg to %s "
				"(%d hp left).\n", level, c->name, c->hp);
		    } else {
			dprintf("Toxic clouds does %d dmg to %s "
				"(%d hp left).\n", level, c->name, c->hp);
			RemoveAttr(c, c->attr[i].type, -1);
		    }
		    if (c->hp <= 0)
			RemoveCard(state, c, 1);
		}
		break;
	    }
	    default:
		break;
	}
    }

    // If card died, don't try to heal it.
    if (c->hp <= 0)
	return;

    // Handle healing attrs after attack.
    for (i=0;i<c->numAttr;i++) {
	int level = c->attr[i].level;
	switch (c->attr[i].type) {
	    case ATTR_REJUVENATE:
	    case ATTR_BLOOD_STONE:
	    {
		if (trapped)
		    break;
		if (HasAttr(c, ATTR_LACERATE_BUFF, NULL))
		    break;
		level = MIN(level, c->maxHp - c->hp);
		if (level > 0) {
		    c->hp += level;
		    if (c->attr[i].type == ATTR_BLOOD_STONE) {
			dprintf("%s rejuvenates %d to %d hp (Blood Stone).\n",
				c->name, level, c->hp);
		    } else {
			dprintf("%s rejuvenates %d to %d hp.\n", c->name, level,
				c->hp);
		    }
		}
		break;
	    }
	    default:
		break;
	}
    }
}

/**
 * Returns the number of cards in the given set with the given attribute.
 * This is mainly used for runes and counting cards of a particular class.
 *
 * @param	cs	The card set.
 * @param	attr	Attribute type.
 * @return		Number of cards with that attribute.
 */
static int countCardsWithAttr(const CardSet *cs, int attr)
{
    int i     = 0;
    int count = 0;

    for (i=0;i<cs->numCards;i++) {
	const Card *c = &cs->cards[i];
	if (HasAttr(c, attr, NULL))
	    count++;
    }
    return count;
}

/**
 * Adds a rune's buff to all cards on the field.
 *
 * @param	state	The simulator state.
 * @param	attr	Pointer to rune's attribute.
 */
static void addRuneBuffToField(State *state, Attr *attr)
{
    CardSet *f = &state->field;
    int      i = 0;

    for (i=0;i<f->numCards;i++) {
	Card *c = &f->cards[i];
	AddAttr(c, attr);
    }
}

/**
 * Removes a rune's buff from all cards on the field.
 *
 * @param	state	The simulator state.
 * @param	attr	Rune's attribute type.
 */
static void removeRuneBuffFromField(State *state, int attr)
{
    CardSet *f = &state->field;
    int      i = 0;

    for (i=0;i<f->numCards;i++) {
	Card *c = &f->cards[i];
	RemoveAttr(c, attr, -1);
    }
}

/**
 * At the start of the player's round, handle rune activations and
 * deactivations.
 *
 * @param	state		The simulator state.
 */
static void HandleRunes(State *state)
{
    int i = 0;

    // Deactivate runes from last round.
    for (i=0;i<state->numRunes;i++) {
	Rune *rune = &state->runes[i];

	if (!rune->usedThisRound)
	    continue;
	rune->usedThisRound = 0;
	switch (rune->attr.type) {
	    case ATTR_ARCTIC_FREEZE:
	    case ATTR_BLOOD_STONE:
	    case ATTR_FROST_BITE:
	    case ATTR_RED_VALLEY:
	    case ATTR_LORE:
	    case ATTR_REVIVAL:
	    case ATTR_FIRE_FORGE:
	    case ATTR_STONEWALL:
	    case ATTR_THUNDER_SHIELD:
	    case ATTR_NIMBLE_SOUL:
	    case ATTR_DIRT:
	    case ATTR_FLYING_STONE:
	    case ATTR_TSUNAMI:
		removeRuneBuffFromField(state, rune->attr.type);
		break;
	    case ATTR_SPRING_BREEZE:
	    {
		int      j     = 0;
		int      level = rune->attr.level;
		CardSet *f     = &state->field;
		
		dprintf("Spring breeze ended.\n");
		for (j=0;j<f->numCards;j++) {
		    Card *c     = &f->cards[j];
		    int   oldHp = c->hp;
		    if (!HasAttr(c, ATTR_SPRING_BREEZE, NULL))
			continue;
		    RemoveAttr(c, ATTR_SPRING_BREEZE, -1);
		    c->maxHp -= level;
		    if (c->hp > c->maxHp)
			c->hp = c->maxHp;
		    if (c->hp != oldHp) {
			dprintf("Spring breeze ended, hp of %s dropped by %d "
				"(to %d).\n", c->name, oldHp - c->hp, c->hp);
		    }
		}
		break;
	    }
	    default:
		break;
	}
    }

    // Handle rune activations.
    for (i=0;i<state->numRunes;i++) {
	Rune    *rune  = &state->runes[i];
	int      count = 0;

	if (rune->chargesUsed >= rune->maxCharges)
	    continue;
	switch (rune->attr.type) {
	    case ATTR_ARCTIC_FREEZE:
		count = countCardsWithAttr(&state->grave, ATTR_TUNDRA);
		if (count > 2) {
		    vprintf("Arctic Freeze activated.\n");
		    addRuneBuffToField(state, &rune->attr);
		    rune->chargesUsed++;
		    rune->usedThisRound = 1;
		}
		break;
	    case ATTR_BLOOD_STONE:
		count = countCardsWithAttr(&state->field, ATTR_MTN);
		if (count > 1) {
		    vprintf("Blood stone activated.\n");
		    addRuneBuffToField(state, &rune->attr);
		    rune->chargesUsed++;
		    rune->usedThisRound = 1;
		}
		break;
	    case ATTR_CLEAR_SPRING:
		count = countCardsWithAttr(&state->field, ATTR_TUNDRA);
		if (count > 1) {
		    // Make sure at least one card is damaged.
		    int  j          = 0;
		    bool hasDamaged = false;

		    for (j=0;j<state->field.numCards;j++) {
			const Card *c = &state->field.cards[j];
			if (c->hp != 0 && c->hp < c->maxHp) {
			    hasDamaged = true;
			    break;
			}
		    }
		    if (!hasDamaged) {
			vprintf("Clear spring skipped because no cards "
				"damaged.\n");
			break;
		    }
		}
		if (count > 1) {
		    vprintf("Clear spring activated.\n");
		    SimRegenerate(state, "Clear spring", rune->attr.level);
		    rune->chargesUsed++;
		}
		break;
	    case ATTR_FROST_BITE:
		count = countCardsWithAttr(&state->grave, ATTR_TUNDRA);
		if (count > 3) {
		    vprintf("Frost bite activated.\n");
		    addRuneBuffToField(state, &rune->attr);
		    rune->chargesUsed++;
		    rune->usedThisRound = 1;
		}
		break;
	    case ATTR_RED_VALLEY:
		count = countCardsWithAttr(&state->field, ATTR_SWAMP);
		if (count > 1) {
		    vprintf("Red valley activated.\n");
		    addRuneBuffToField(state, &rune->attr);
		    rune->chargesUsed++;
		    rune->usedThisRound = 1;
		}
		break;
	    case ATTR_LORE:
		count = countCardsWithAttr(&state->grave, ATTR_MTN);
		if (count > 2) {
		    vprintf("Lore activated.\n");
		    addRuneBuffToField(state, &rune->attr);
		    rune->chargesUsed++;
		    rune->usedThisRound = 1;
		}
		break;
	    case ATTR_LEAF:
		if (state->round > 14) {
		    dprintf("Leaf: %d dmg\n", rune->attr.level);
		    state->dmgDone += rune->attr.level;
		    state->demon.hp -= rune->attr.level;
		    rune->chargesUsed++;
		}
		break;
	    case ATTR_REVIVAL:
		count = countCardsWithAttr(&state->grave, ATTR_FOREST);
		if (count > 1) {
		    vprintf("Revival activated.\n");
		    addRuneBuffToField(state, &rune->attr);
		    rune->chargesUsed++;
		    rune->usedThisRound = 1;
		}
		break;
	    case ATTR_FIRE_FORGE:
		count = countCardsWithAttr(&state->grave, ATTR_MTN);
		if (count > 1) {
		    vprintf("Fire forge activated.\n");
		    addRuneBuffToField(state, &rune->attr);
		    rune->chargesUsed++;
		    rune->usedThisRound = 1;
		}
		break;
	    case ATTR_STONEWALL:
		count = countCardsWithAttr(&state->field, ATTR_SWAMP);
		if (count > 1) {
		    vprintf("Stonewall activated.\n");
		    addRuneBuffToField(state, &rune->attr);
		    rune->chargesUsed++;
		    rune->usedThisRound = 1;
		}
		break;
	    case ATTR_THUNDER_SHIELD:
		count = countCardsWithAttr(&state->field, ATTR_FOREST);
		if (count > 1) {
		    vprintf("Thunder shield activated.\n");
		    addRuneBuffToField(state, &rune->attr);
		    rune->chargesUsed++;
		    rune->usedThisRound = 1;
		}
		break;
	    case ATTR_NIMBLE_SOUL:
		count = countCardsWithAttr(&state->grave, ATTR_FOREST);
		if (count > 2) {
		    vprintf("Nimble soul activated.\n");
		    addRuneBuffToField(state, &rune->attr);
		    rune->chargesUsed++;
		    rune->usedThisRound = 1;
		}
		break;
	    case ATTR_DIRT:
		count = countCardsWithAttr(&state->grave, ATTR_SWAMP);
		if (count > 1) {
		    vprintf("Dirt activated.\n");
		    addRuneBuffToField(state, &rune->attr);
		    rune->chargesUsed++;
		    rune->usedThisRound = 1;
		}
		break;
	    case ATTR_FLYING_STONE:
		count = countCardsWithAttr(&state->grave, ATTR_SWAMP);
		if (count > 2) {
		    vprintf("Flying stone activated.\n");
		    addRuneBuffToField(state, &rune->attr);
		    rune->chargesUsed++;
		    rune->usedThisRound = 1;
		}
		break;
	    case ATTR_TSUNAMI:
		if (state->hp < state->maxHp / 2) {
		    vprintf("Tsunami activated.\n");
		    addRuneBuffToField(state, &rune->attr);
		    rune->chargesUsed++;
		    rune->usedThisRound = 1;
		}
		break;
	    case ATTR_SPRING_BREEZE:
	    {
		int      j = 0;
		CardSet *f = &state->field;

		count = countCardsWithAttr(&state->hand, ATTR_FOREST);
		if (count > 1 && f->numCards > 0) {
		    vprintf("Spring breeze activated.\n");
		    addRuneBuffToField(state, &rune->attr);
		    rune->chargesUsed++;
		    rune->usedThisRound = 1;
		    for (j=0;j<f->numCards;j++) {
			Card *c = &f->cards[j];
			c->hp    += rune->attr.level;
			c->maxHp += rune->attr.level;
			dprintf("Spring breeze increases hp of %s by %d"
				" (to %d).\n", c->name, rune->attr.level,
				c->hp);
		    }
		}
		break;
	    }
	    default:
		break;
	}
    }
}

/**
 * Simulate the player's round.
 *
 * @param	state		The simulator state.
 */
static void SimPlayer(State *state)
{
    CardSet *f = &state->field;
    int      i = 0;
    Card    *c = NULL;

    HandleRunes(state);

    for (i=0;i<f->numCards;i++)
	SimPlayerCard(state, i);

    // Remove backstab buffs from cards.
    for (i=0;i<f->numCards;i++) {
	int level = 0;
	c = &f->cards[i];

	if (HasAttr(c, ATTR_BACKSTAB_BUFF, &level)) {
	    RemoveAttr(c, ATTR_BACKSTAB_BUFF, -1);
	    c->atk -= level;
	}
    }

    // Could be dead from counterattack.
    RemoveDeadCards(state);
}

/**
 * Simulates one complete battle from round 1 to player death.
 *
 * @param	state		The simulator state.
 * @param	localRoundX	This is the number of rounds specified by
 *				the -printround command line option.  If the
 *				battle goes on for this number of rounds, we
 *				set *hitRoundX to true.
 * @param	hitRoundX	Pointer to a bool.  We will set this to true
 *				if the battle reaches round X.
 */
void Simulate(State *state, int localRoundX, bool *hitRoundX)
{
    while (state->hp > 0 && (state->field.numCards > 0 ||
	    state->deck.numCards > 0 || state->hand.numCards > 0) &&
	    state->round <= maxRounds) {
	if (state->round == localRoundX)
	    *hitRoundX = true;
	PrintState(state);
	DecreaseTimers(state);
	if ((state->round & 1) == 0) {
	    dprintf("\nRound %d (player)\n\n", state->round);
	    PlayCardsFromDeck(state);
	    PlayCardsFromHand(state);
	    // Check here because of obstinacy.
	    if (state->hp <= 0)
		break;
	    SimPlayer(state);
	} else {
	    dprintf("\nRound %d (demon)\n\n", state->round);
	    SimDemon(state);
	}
	state->round++;
    }
    state->round--;
    PrintState(state);
}

/**
 * Calculates the cost of the deck.  This affects the deck's cooldown.
 *
 * @return	Deck cost.
 */
int CalcCost(void)
{
    int         i    = 0;
    int         cost = 0;
    const Card *c    = NULL;
    for (i=0;i<numDeckCards;i++) {
	c = FindCard(theDeck[i]);
	cost += c->cost;
    }
    return cost;
}

/**
 * Takes a string and strips spaces from the front and end of the string.
 * This is normally the strtrim() function but not every platform has it
 * so I had to write my own.
 */
static char *trim(char *s)
{
    int len = 0;

    while (isspace((unsigned int) (*s)))
	s++;
    len = strlen(s);
    while (len > 0 && isspace((unsigned int)(s[len-1])))
	len--;
    s[len] = '\0';
    return s;
}

/**
 * Returns an allocated copy of a string.  This is normally the strdup()
 * function but not every platform has it so I had to write my own.
 */
static char *my_strdup(const char *s)
{
    int   len = strlen(s);
    char *ret = (char *) malloc(len+1);

    strcpy(ret, s);
    return ret;
}

/**
 * Reads the cards.txt file to get all the card descriptions from that
 * file.  Fills in the cardTypes array.
 *
 * @param	filename	The name of the cards file (normally cards.txt).
 */
static void readCardTypesFromFile(const char *filename)
{
    static char buffer[MAX_LINE_SIZE];
    FILE *f       = NULL;
    char *trimmed = NULL;
    char *s       = NULL;
    bool  error   = false;
    int   attr    = 0;
    Card *c       = NULL;

    f = fopen(filename, "r");
    if (f == NULL) {
	fprintf(stderr, "Error: Couldn't read file %s.\n", filename);
	exit(1);
    }
    numCardTypes = 0;
    while (fgets(buffer, MAX_LINE_SIZE, f) != NULL) {
	buffer[MAX_LINE_SIZE-1] = '\0';
	trimmed = trim(buffer);
	if (trimmed[0] == '#' || trimmed[0] == '\0')
	    continue;

	c = &cardTypes[numCardTypes];

	// Name
	s = strtok(trimmed, ",");
	if (s == NULL) {
	    fprintf(stderr, "Bad card description: %s\n", trimmed);
	    exit(1);
	}
	c->name = my_strdup(trim(s));

	// Cost
	s = strtok(NULL, ",");
	if (s == NULL) { error = true; break; }
	c->cost = strtoul(trim(s), NULL, 0);
	if (c->cost == 0) {
	    fprintf(stderr, "Bad cost: %s\n", trim(s));
	    error = true;
	    break;
	}

	// Timing
	s = strtok(NULL, ",");
	if (s == NULL) { error = true; break; }
	c->timing = strtoul(trim(s), NULL, 0);
	if (c->timing == 0) {
	    fprintf(stderr, "Bad timing: %s\n", trim(s));
	    error = true;
	    break;
	}

	// Base atk
	s = strtok(NULL, ",");
	if (s == NULL) { error = true; break; }
	c->baseAtk = strtoul(trim(s), NULL, 0);
	if (c->baseAtk == 0) {
	    fprintf(stderr, "Bad attack: %s\n", trim(s));
	    error = true;
	    break;
	}

	// Base hp
	s = strtok(NULL, ",");
	if (s == NULL) { error = true; break; }
	c->baseHp = strtoul(trim(s), NULL, 0);
	if (c->baseHp == 0) {
	    fprintf(stderr, "Bad hp: %s\n", trim(s));
	    error = true;
	    break;
	}

	// Attributes
	attr = 0;
	do {
	    int   attrType = 0;
	    char *colon    = NULL;
	    int   value    = 0;

	    s = strtok(NULL, ",");
	    if (s == NULL)
		break;
	    s = trim(s);
	    colon = strchr(s, ':');
	    if (colon) {
		*colon = '\0';
		value = strtoul(colon+1, NULL, 0);
	    }
	    attrType = LookupAttr(s);
	    if (attrType == -1) {
		fprintf(stderr, "Bad attribute: %s not found\n", s);
		error = true;
		break;
	    }
	    c->baseAttr[attr].type  = attrType;
	    c->baseAttr[attr].level = value;
	    attr++;
	} while (attr < MAX_ATTR-1);
	if (error)
	    break;
	cardTypes[numCardTypes].baseAttr[attr].type = ATTR_NONE;
	numCardTypes++;
    }
    if (error) {
	fprintf(stderr, "Bad card description: %s\n",
		cardTypes[numCardTypes].name);
	exit(1);
    }
    fclose(f);
}

/**
 * Reads the deck description from a file.  Fills in theDeck array and
 * theRunes array.
 *
 * @param	filename	Filename of deck file.
 */
static void readDeckFromFile(const char *filename)
{
    static char buffer[MAX_LINE_SIZE];
    FILE *f       = NULL;
    char *trimmed = NULL;

    f = fopen(filename, "r");
    if (f == NULL) {
	fprintf(stderr, "Error: Couldn't read file %s.\n", filename);
	exit(1);
    }
    numDeckCards = 0;
    numRunes = 0;
    while (fgets(buffer, MAX_LINE_SIZE, f) != NULL) {
	buffer[MAX_LINE_SIZE-1] = '\0';
	trimmed = trim(buffer);
	if (trimmed[0] == '#' || trimmed[0] == '\0')
	    continue;
	if (FindCard(trimmed) != NULL) {
	    // Found a card.
	    if (numDeckCards >= MAX_CARDS_IN_DECK) {
		fprintf(stderr, "Error: Too many cards in deck.\n");
		exit(1);
	    }
	    theDeck[numDeckCards++] = my_strdup(trimmed);
	} else if (FindRune(trimmed) != NULL) {
	    // Found a rune.
	    if (numRunes >= MAX_RUNES) {
		fprintf(stderr, "Error: Too many runes.\n");
		exit(1);
	    }
	    theRunes[numRunes++] = my_strdup(trimmed);
	} else {
	    fprintf(stderr, "Error: Unknown card/rune %s.\n", trimmed);
	    exit(1);
	}
    }
    fclose(f);
}

/**
 * Handles command line arguments.
 */
static void HandleArgs(int argc, char **argv)
{
    int i = 0;

    for (i=1;i<argc;i++) {
	if (!strcasecmp(argv[i], "-level")) {
	    i++;
	    if (i < argc) {
		initialLevel = strtoul(argv[i], NULL, 0);
		if (initialLevel <= 0 || initialLevel > MAX_LEVEL) {
		    fprintf(stderr, "Bad level: %d\n", initialLevel);
		    exit(1);
		}
		initialHp = hpPerLevel[initialLevel];
	    }
	} else if (!strcasecmp(argv[i], "-hp")) {
	    i++;
	    if (i < argc)
		initialHp = strtoul(argv[i], NULL, 0);
	} else if (!strcasecmp(argv[i], "-iter")) {
	    i++;
	    if (i < argc)
		numIters = strtoul(argv[i], NULL, 0);
	} else if (!strcasecmp(argv[i], "-demon")) {
	    i++;
	    if (i < argc)
		theDemon = argv[i];
	} else if (!strcasecmp(argv[i], "-debug")) {
	    doDebug = true;
	    numIters = 10;
	} else if (!strcasecmp(argv[i], "-verbose")) {
	    doDebug = true;
	    verbose = true;
	    numIters = 10;
	} else if (!strcasecmp(argv[i], "-showdamage")) {
	    showDamage = true;
	    numIters = 200;
	} else if (!strcasecmp(argv[i], "-avgconcentrate")) {
	    avgConcentrate = true;
	} else if (!strcasecmp(argv[i], "-numthreads")) {
	    i++;
	    if (i < argc)
		numThreads = strtoul(argv[i], NULL, 0);
	    if (numThreads >= MAX_THREADS)
		numThreads = MAX_THREADS;
	    if (numThreads <= 0)
		numThreads = 1;
	} else if (!strcasecmp(argv[i], "-maxrounds")) {
	    i++;
	    if (i < argc)
		maxRounds = strtoul(argv[i], NULL, 0);
	} else if (!strcasecmp(argv[i], "-printround")) {
	    i++;
	    if (i < argc)
		roundX = strtoul(argv[i], NULL, 0);
	} else if (!strcasecmp(argv[i], "-deck")) {
	    i++;
	    if (i < argc)
		deckFile = argv[i];
	} else if (!strcasecmp(argv[i], "-o") ||
		!strcasecmp(argv[i], "-output")) {
	    i++;
	    if (i < argc) {
		outputFilename = argv[i];
		doAppend = false;
	    }
	} else if (!strcasecmp(argv[i], "-a") ||
		!strcasecmp(argv[i], "-append")) {
	    i++;
	    if (i < argc) {
		outputFilename = argv[i];
		doAppend = true;
	    }
	}
    }
    if (doDebug || showDamage)
	numThreads = 1;
}

#define MAX_DEFAULT_ARGS	50

/**
 * Reads the defaults.txt file and handles command line arguments in that file.
 */
static void HandleDefaultArgs(void)
{
    static char defaultArgs[MAX_LINE_SIZE];
    char       *argArray[MAX_DEFAULT_ARGS];
    FILE       *def     = NULL;
    int         numArgs = 1;
    char       *start   = NULL;
    char       *s       = NULL;

    def = fopen("defaults.txt", "r");
    if (def == NULL)
	return;
    if (fgets(defaultArgs, MAX_LINE_SIZE, def) != NULL) {
	start = trim(defaultArgs);

	do {
	    s = strchr(start, ' ');
	    if (s != NULL) {
		argArray[numArgs++] = start;
		*s = '\0';
		start = trim(s+1);
	    } else {
		argArray[numArgs++] = start;
		break;
	    }
	} while(numArgs < MAX_DEFAULT_ARGS);

	if (numArgs > 1)
	    HandleArgs(numArgs, argArray);
    }
    fclose(def);
}

/**
 * The entrypoint for one thread.  This will run the specified number of
 * simulations, and put the results in the given Task structure.
 *
 * @param	arg		A Task structure holding the State and the
 *				number of iterations to run.  The results
 *				will also be placed in the Task.
 */
#if defined(USING_WINDOWS)
static DWORD WINAPI ThreadSimulate(LPVOID arg)
#else
static void *ThreadSimulate(void *arg)
#endif
{
    Task     *task          = (Task *) arg;
    State    *state         = task->state;
    int       numIterations = task->numIterations;
    Result   *result        = task->result;
    int       i             = 0;
    long long total         = 0;
    long long totalRounds   = 0;
    int       lowRounds     = 0x7fffffff;
    int       highRounds    = 0;
    int       lowDamage     = 0x7fffffff;
    int       highDamage    = 0;
    int       localRoundX   = roundX;
    int       timesRoundX   = 0;
    bool      hitRoundX     = false;

    for (i=0;i<numIterations;i++) {
	InitState(state);
	ShuffleSet(state, &state->deck);
	hitRoundX = false;
	Simulate(state, localRoundX, &hitRoundX);
	if (hitRoundX)
	    timesRoundX++;
	total       += state->dmgDone;
	totalRounds += state->round;
	highDamage = MAX(highDamage, state->dmgDone);
	lowDamage  = MIN(lowDamage,  state->dmgDone);
	highRounds = MAX(highRounds, state->round);
	lowRounds  = MIN(lowRounds,  state->round);
	if (showDamage) {
	    fprintf(output, "Dmg done = %d\n", state->dmgDone);
	}
	dprintf("\n");
    }
    result->total       = total;
    result->totalRounds = totalRounds;
    result->highDamage  = highDamage;
    result->lowDamage   = lowDamage;
    result->highRounds  = highRounds;
    result->lowRounds   = lowRounds;
    result->timesRoundX = timesRoundX;

#if defined(USING_WINDOWS)
    return 0;
#else
    return NULL;
#endif
}

/**
 * Main function.  Reads info from various files, starts up multiple
 * threads, and then runs the simulation on those threads.  Once all the
 * threads are done, this function will gather the results and print them out.
 */
int main(int argc, char *argv[])
{
    int         i           = 0;
    int         cost        = 0;
    int         deckTime    = 0;
    long long   total       = 0;
    double      dTotal      = 0;
    long long   totalRounds = 0;
    int         lowRounds   = 0x7fffffff;
    int         highRounds  = 0;
    int         lowDamage   = 0x7fffffff;
    int         highDamage  = 0;
    int         timesRoundX = 0;
    Result     *results     = NULL;
    Task       *tasks       = NULL;
#if defined(USING_WINDOWS)
    HANDLE     *threads     = NULL;
#else
    pthread_t  *threads     = NULL;
#endif

    output = stdout;
    readCardTypesFromFile("cards.txt");

    initialHp = hpPerLevel[initialLevel];

    HandleDefaultArgs();
    HandleArgs(argc, argv);

    if (outputFilename != NULL) {
	if (doAppend)
	    output = fopen(outputFilename, "a");
	else
	    output = fopen(outputFilename, "w");
	if (output == NULL) {
	    fprintf(stderr, "Couldn't open output file: %s.\n", outputFilename);
	    exit(1);
	}
    }
    readDeckFromFile(deckFile);

    cost     = CalcCost();
    deckTime = 60 + cost*2;

    srand(time(NULL));

    InitDefaultState(&defaultState);

    AllocateStates(numThreads);
    for (i=0;i<numThreads;i++) {
	states[i]->seedW = rand();
	states[i]->seedZ = rand();
    }

    results = (Result *)    calloc(numThreads, sizeof(Result));
    tasks   = (Task *)      calloc(numThreads, sizeof(Task));
#if defined(USING_WINDOWS)
    threads = (HANDLE *)    calloc(numThreads, sizeof(HANDLE));
#else
    threads = (pthread_t *) calloc(numThreads, sizeof(pthread_t));
#endif

    // Start all threads running.
    for (i=0;i<numThreads;i++) {
	int numIterations = numIters / numThreads;

	// Make the first thread also do all the remainder # of iters.
	if (i == 0)
	    numIterations += (numIters - numIterations * numThreads);
	tasks[i].state         = states[i];
	tasks[i].numIterations = numIterations;
	tasks[i].result        = &results[i];
#if defined(USING_WINDOWS)
	threads[i] = CreateThread(NULL, 0, ThreadSimulate, (LPVOID) &tasks[i],
			0, NULL);
#else
	pthread_create(&threads[i], NULL, ThreadSimulate, (void *) &tasks[i]);
#endif
    }

    // Wait for all threads to complete.
    for (i=0;i<numThreads;i++) {
#if defined(USING_WINDOWS)
	WaitForSingleObject(threads[i], INFINITE);
#else
	void *dummyRet;

	pthread_join(threads[i], &dummyRet);
#endif
    }

    // Total the results from all threads.
    for (i=0;i<numThreads;i++) {
	total       += results[i].total;
	totalRounds += results[i].totalRounds;
	timesRoundX += results[i].timesRoundX;
	highDamage   = MAX(highDamage, results[i].highDamage);
	lowDamage    = MIN(lowDamage,  results[i].lowDamage);
	highRounds   = MAX(highRounds, results[i].highRounds);
	lowRounds    = MIN(lowRounds,  results[i].lowRounds);
    }

    fprintf(output, "Demon: %s\n", theDemon);
    fprintf(output,
	    "Deck : (level %d, %d initial hp, %d cost, "
	    "%d:%02d cooldown)\n\n",
	    initialLevel, initialHp, cost,
	    deckTime / 60, deckTime % 60);
    for (i=0;i<defaultState.deck.numCards;i++)
	fprintf(output, "%2d) %s\n", i+1, defaultState.deck.cards[i].name);
    fprintf(output, "\nRunes:\n\n");
    for (i=0;i<defaultState.numRunes;i++)
	fprintf(output, "%s\n", defaultState.runes[i].name);
    fprintf(output, "\nResults of simulation (%d fights):\n\n", numIters);
#if 0
    fprintf(output, "Total dmg over %d runs: %lld\n",
	    numIters, (long long) total);
#endif
    dTotal = (double) total / numIters;
    fprintf(output, "Lowest  number of rounds      : %d\n"
	            "Highest number of rounds      : %d\n"
		    "Average number of rounds      : %4.1lf\n",
		    lowRounds, highRounds, (double) totalRounds / numIters);
    if (timesRoundX > 0) {
	fprintf(output, "Percent time hitting round %d : %4.1lf\n",
		roundX, ((double) timesRoundX * 100.0f / numIters));
    }
    fprintf(output, "\n");
    fprintf(output, "Lowest  damage                : %d\n"
		    "Highest damage                : %d\n"
	            "Average dmg per fight         : %5.1lf\n",
		    lowDamage, highDamage, dTotal);
    fprintf(output, "Average dmg per minute        : %5.1lf\n",
	    (dTotal * 60) / (60 + cost * 2));
    fprintf(output, "\n\n");
    if (output != stdout)
	fclose(output);
    return 0;
}
