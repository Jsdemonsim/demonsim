-----------------------
HOW TO USE THIS PROGRAM
-----------------------
To use this program, you should first open a Windows console window.
To do this, open the start menu, type "cmd" in the search box and press
return.  This should open up a window with a command prompt.  Use the "cd"
command to change the directory to the directory where you put the simulator.
Example:

C:\Users\username> cd desktop\sim

Now, run the simulator by typing "sim" followed by any additional command
line arguments (see below for the allowed arguments).  Example:

C:\Users\username\Desktop\sim> sim -demon Deucalion -deck deck.txt

Alternatively, I have added a batch file called "openwindow.bat".  If you
double click on this, it should open up a console window in the correct
directory.  If double clicking doesn't work, right click and select
"Run as Administrator" instead.

---------------
MACINTOSH USERS
---------------
If you use a mac, first run the "Terminal" program.  Use the cd command to
change the directory to where you put the simulator.  Run ./sim_mac instead of
sim.  Example:

(Run Terminal)
$ cd /Users/username/Downloads/sim_1_6
$ ./sim_mac -demon Deucalion -deck deck.txt

----------------------
COMMAND LINE ARGUMENTS
----------------------
sim [-level #] [-iter #] [-demon name] [-debug] [-verbose]
    [-showdamage] [-avgconcentrate] [-printround #] [-deck filename]
    [-numthreads #] [-o filename] [-a filename]

Options:

-level #
    Sets player level to # (default 61).  Hp will automatically be adjusted.
    The maximum level is 150.

-iter #
    Sets number of iterations to # (default 50000).  Each simulation will
    run this number of fights and then print the results.

-demon name
    Selects demon to fight (default DarkTitan).  Valid names are:
        DarkTitan, Deucalion, Mars, Pandarus, PlagueOgryn, SeaKing

-debug
    Turns on debug output, which prints the fight log.  Setting this mode
    sets the number of iterations to 10.  You can override the number
    of iterations by adding -iter # after -debug.  Default is off.

-verbose
    Same as debug but prints a bit more to the fight log.  Default is off.

-showdamage
    Use this instead of -debug if you only want to see the final damage numbers
    for each fight.  Setting this sets the number of iterations to 200.  You
    can override the number of iterations by adding -iter # after -showdamage.
    Default is off.

-avgconcentrate
    Makes the concentrate ability always add the average amount instead of
    all or nothing.  For example, instead of 50% chance to add 0 and 50%
    chance to add 800, this will always 400 instead.  Default is off.

-printround #
    In the fight summary, it prints the percentage time it reaches a
    particular round #.  You can set that round # with this option (default 50).

-deck filename
    Reads the deck from the given filename (default: deck.txt).

-numthreads #
    Run the simulator using # threads (default 8).  Each thread runs in
    parallel and the work is split amongst the threads.  If you have a
    multicore computer, using threads will speed up the simulation by up
    to N times, where N is the number of cores you have.

-o filename (or -output filname)
    Outputs to the given filename (default: outputs to console).  Note that
    if the file exists, it will be overwritten.

-a filename (or -append filename)
    Appends to the given filename (default: off).  If the file exists, this
    will append to the end of the file instead of overwriting it.  Use only
    one of -o or -a.

If you have a file named defaults.txt in the current directory, options from
the first line in that file will be prepended to your command line options.
This means you can specify default options in defaults.txt and override them
with the command line.  See the sample defaults.txt file.

--------
EXAMPLES
--------
sim -level 71 -iter 20000 -deck hh7wea3.txt -demon deucalion -avgconcentrate
sim -level 71 -deck rk9.txt -demon deucalion -verbose -o dcfight.txt
sim -level 61 -iter 20000 -deck deck.txt -demon deucalion -a dcresult.txt
sim -level 71 -iter 20000 -deck deck.txt -demon deucalion -a dcresult.txt

-----
CARDS
-----
There must be a file in the current directory named cards.txt.  The
program loads the card descriptions from this file.  You can add cards to
the file in the same format as the other cards.  However, you may only
list abilities that are supported (see below).

----
DECK
----
The deck file must be a file with one card name or rune name per line.  It
doesn't matter what order the cards or runes are in, as long as there are not
more than 10 cards and 4 runes.  Each card name must be in the cards.txt file.
Each rune must be one of the runes supported (see below).  Card and rune
names are case insensitive.

ABILITIES SUPPORTED
-------------------
Note that some of these have a different name than in the game, such as
"Tundra Force" instead of "Northern Force".  Hopefully you can figure it out.
The class of the card (e.g. "Tundra") is also considered an ability.  Some
of these abilities are only supported for the demon (e.g. Trap).  Ability
names are case insensitive.

Advanced strike
Backstab
Bite
Bloodsucker
Bloodthirsty
Chain attack
Concentrate
Counterattack
Craze
Curse
D_reanimate (D = Desperation)
D_reincarnate
Damnation
Destroy
Dodge
Evasion
Exile
Fire god
Forest
Forest force
Forest guard
Guard
Healing
Hot chase
Ice shield
Immunity
Lacerate
Mana corrupt
Mania
Mtn
Mtn force
Mtn guard
Obstinacy
Parry
Prayer
QS_regenerate (QS = Quick Strike)
QS_reincarnate
Reanimate
Reflection (only affects demon's mana corruption)
Regenerate
Reincarnate
Rejuvenate
Resistance
Resurrection
Retaliation (treated as counterattack)
Sacrifice
Snipe
Swamp
Swamp force
Swamp guard
Toxic clouds
Trap
Tundra
Tundra force
Tundra guard
Warpath
Vendetta
Wicked leech

RUNES
-----
These are the supported runes.  It is assumed that all runes are max level.

Arctic Freeze
Clear Spring
Dirt
Fire Forge
Flying Stone
Frost Bite
Leaf
Lore
Nimble Soul
Red Valley
Revival
Spring Breeze
Stonewall
Thunder Shield
Tsunami

FEEDBACK
--------
If you find any bugs, or want a new ability or rune added, please post
your feedback at the ek.arcannis.com forums.

VERSION HISTORY
---------------
1.0: Initial Release
1.1: Added/fixed up a few cards in cards.txt.
     Fixed hp and attack buffs
     Fixed starting rounds to match actual demon fights
     Fixed Plague Ogryn trap to be ordered from left to right
     Fixed bite: no longer affects demon (because of immunity) (not sure)
     Added numbering of cards in output
     Added Tsunami rune
     Added lowest/highest/average number of rounds per fight
1.2: Added maximum hand size (5).  This affects resurrect decks.
     Added sacrifice ability.
     Added defaults.txt file for specifying default options.
1.3: Fixed flying stone (was 70 dmg, now 225).
1.4: Fixed flying stone AGAIN (was 225 dmg, now 270).
     Added min/max damage and deck cooldown to results printout.
     Removed floating point operations from percentage calculations in order
           to speed up program.
     Added unavoidable damage under the option -unavoidableDmg (default off).
1.5: Fixed bug with craze/tsunami/bloodthirsty where death did not remove
           the attack increase.
     Fixed demon curse so that if it kills the player, the simulation ends
           instead of having the demon attack (which leads to a possible
           extra counterattack).
     Added d_reanimate (desperation:reanimate) ability.
     Added retaliation ability (treated as counterattack).
     Added evasion ability (only affects Plague Ogryn).
1.6: Added a "how to use this program" section to the readme file.
     Added a mac executable "sim_mac" to the release.
1.7: Fixed resurrection when your hand is full.  Previously resurrection would
           fail and your card would go to the grave.  Now it will resurrect
           your card to your deck instead.
     Fixed Guard to work when the demon attacks the player directly.  That is,
           if the demon exiles or destroys the leftmost card and then attacks
           the player, that damage can now be absorbed by Guard.
     Fixed a bug where healing/regeneration worked on immune cards.
     Added Chain attack, Mana corrupt, Wicked leech, Hot chase, and Damnation
           abilities for the new demons.  Added the new demons to the
           cards.txt file with names such as DarkTitan2, Deucalion2, etc.
           Also added the Reflection ability to cards that have it, because
           it affects the demon's Mana corrupt ability.
1.8: New demons have replaced the old demons in cards.txt.  Old demons have
           been renamed with an "_old" suffix, such as "Mars_old".
     Sea King counterattack now only hits one card.  Counterattack is now
           a separate ability from retaliation (it used to be that both were
           treated as retaliation).
     Wicked leech (on Mars) now affects cards with immunity.  If there is a
           player card with wicked leech, it will not affect the demon.
     Added Vendetta ability, and added Rogue Knight to cards.txt.
     Increased the default max rounds to 500.  Removed the -maxrounds option
           from the help file (although it still exists).  There really
           shouldn't be a need for a maximum number of rounds, but it is
           still there for debugging purposes.
     Changed the way reanimation works.  Previously, it would pick a random
           card from the grave.  If that card had immunity or reanimation,
           the reanimation would fail and nothing would happen.  Now, it
           only picks cards that do not have immunity or reanimation from the
           grave.  So, reanimation can never fail if there is a reanimatable
           card in the grave.
     Added the abilities: QS_regenerate, QS_reincarnate, D_reincarnate.  The
           first one is for Ice Sprite, which is added to cards.txt.  The
           other two are for the upcoming card that has both.
     Added the -printround option to set which round is printed in the
           summary, when it prints the percentage time it reaches round X.
           It used to always use round 50.
1.9: Fixed bug with demon Destroy and Mana corrupt targeting dead cards.
     Added multicore support to the simulator.  By default, the simulator will
           split its work using 8 threads, with each thread running in parallel.
           This means that if you have an N core computer, the simulator will
           run N times as fast (up to 8).  You can control the number of
           threads to use with the new -numthreads option (max 64).  When
           in debug, verbose, or showdamage mode, numthreads is forced to 1
           so that output is not interleaved.
     Fixed cards.txt: Treant Healer (cost 14) and Sea King (chain attack 325).
     Added openwindow.bat for Windows users to quickly open up a console
           window to run the simulator.  You may have to run this as
           Administrator if you can't just run it normally.  I added this
           because so many people were having problems opening up the console
           window.
1.10: Fixed Santa (Tundra).
      Fixed Easter bunny cards (removed Forest).
      Added all 1*, 2*, and 3* cards.
      Fixed bug where if a card did 0 physical damage, it should not trigger
           any effects such as Retaliation or Bloodthirsty.
      Implemented wicked leech ability for player cards.  Added wicked leech
           to Soul Thief in cards.txt.
1.11: Fixed reincarnation.  Testing indicates that the reincarnated card is
           always the oldest card in the grave.  Also, the reincarnated card
	   is placed on the top of the deck, meaning it will be played to the
	   hand next round.
      Made demon snipe (devil's blade) always hit the rightmost card if
           multiple cards have the same lowest hp.
      Added advanced strike ability.
      Added new cards.
1.12: Fixed bloodsucker to occur before demon counterattack.
      Added GPL v3 license to source file and LICENSE file.
