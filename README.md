# Yomira

## About Yomira

Yomira is a fork of [ScummVM](https://www.scummvm.org/) that lets you play classic graphic point-and-click adventure games, text adventure games, and RPGs using the original game data files. It retains ScummVM's broad platform and game support while keeping changes focused so upstream improvements can be backported.

Yomira inherits support for many games from ScummVM, including Broken Sword, Myst, and Blade Runner. See the [upstream supported games list](https://wiki.scummvm.org/index.php?title=Category:Supported_Games).

For upstream compatibility information, releases, and project news, visit the [ScummVM website](https://www.scummvm.org/).

## Quickstart

For the impatient among you, here is how to get Yomira running in five simple steps.

1. Install a Yomira build for your platform.

2. Create a directory on your hard drive and copy the game datafiles from the original media to this directory. Repeat this for every game you want to play.

3. Start Yomira, choose 'Add game', select the directory containing the game datafiles (do not try to select the datafiles themselves!) and press Choose.

4. The Game Options dialog opens to allow configuration of various settings for the game. These can be reconfigured at any time, but for now everything should be OK at the default settings.

5. Select the game you want to play in the list, and press Start. To play a game next time, skip to step 5, unless you want to add more games.

>
> Hint:
>
> To add multiple games in one go, click the small arrow on the 'Add game' button and choose 'Mass Add'. Yomira will search the selected directory and its subdirectories for supported games.



## Reporting a bug

For bugs specific to Yomira, use this repository's issue tracker. For issues also present in upstream ScummVM, use the ScummVM [Issue Tracker](https://bugs.scummvm.org/).

Please make sure the bug is reproducible on the latest Yomira build. For upstream reports, check the [ScummVM daily builds](https://buildbot.scummvm.org/#/dailybuilds) and [compatibility list](https://www.scummvm.org/compatibility/) first. Please do not report upstream bugs for games that are not listed as completable on the [Supported Games](https://wiki.scummvm.org/index.php?title=Category:Supported_Games) wiki page or compatibility list.

Please include the following information in the bug report:

- Yomira version
- Bug details, including instructions for how to reproduce the bug. If possible, include log files, screenshots, and any other relevant information.
- Game language
- Game version (for example, talkie or floppy)
- Platform and Compiler (for example, Win32, Linux or FreeBSD)
- An attached saved game, if possible.
- If this bug only occurred recently, include the last version without the bug, and the first version with the bug. That way we can fix it quicker by looking at the changes made.

Finally, please report each issue separately; do not file multiple issues on the same ticket. It is difficult to track the status of each individual bug when they aren't on their own tickets.

## Documentation

### User documentation

For general usage information, see the upstream [ScummVM user documentation](https://docs.scummvm.org/).

### The ScummVM Wiki

[The wiki](https://wiki.scummvm.org/) has information about games supported by ScummVM and development.

### Changelog

Our extensive change log is available [here](NEWS.md).

## SAST Tools

[PVS-Studio](https://pvs-studio.com/en/pvs-studio/?utm_source=github&utm_medium=organic&utm_campaign=open_source) - static analyzer for C, C++, C#, and Java code.

## Credits

A massive thank you to the ScummVM team and all Yomira contributors. See the credits [here](AUTHORS)!

-----

> Good Luck and Happy Adventuring\!
> The Yomira contributors.
