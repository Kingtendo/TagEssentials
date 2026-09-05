# Lunar class dump diagnostics

These two Visual Studio projects are maintainer diagnostics for investigating
Lunar Client mapping changes. They are not required to build or run
TagEssentials.

The dumper writes captured runtime class files below the ignored `lunar-dump/`
directory. Never commit those captures: they can be large, are tied to a local
client build, and are not part of this project's source distribution.

Build both projects as Release x64, start Lunar Client 1.8.9 and TagEssentials,
then run `LunarDumpInjector.exe` from `lunar-dump/tools`. Review all captured
material before sharing it and never collect account, session, or access token
data.
