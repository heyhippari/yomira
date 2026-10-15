# System-98

Engine scaffold for Four-Nine's System-98 visual novel engine. Detection is
implemented; game playback is not. Detected games are marked unsupported.

## Initial Release

- Game ID: `mugenyasoukyoku`
- Title: Mugen Yasoukyoku (Nocturnal Illusion)
- Developer/publisher: Apricot
- Release: Japanese PC-98, April 22, 1995, four floppies
- Engine version: System-98 3.10

Point ScummVM at a directory containing the extracted floppy contents together.
Detection matches both `DISK_A.CAT` and `DISK_A.LIB` by size and MD5, using
ScummVM's default first-5000-byte hashing (the entire file for shorter files).
It does not depend on the executable, which is shared with other System-98 games.

## References

- [System-98 research by ValleyBell](https://github.com/ValleyBell/PC98VNResearch/tree/master/four-nine_system98)
- [Research game list](https://github.com/ValleyBell/PC98VNResearch/blob/master/four-nine_system98/game-list.md)
- [Game](https://vndb.org/v725)
- [Target PC-98 release](https://vndb.org/r18109)
