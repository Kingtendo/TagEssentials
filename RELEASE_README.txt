TAGESSENTIALS : WINDOWS X64
==========================

1. Start Badlion Client or Lunar Client with Minecraft 1.8.9 (64 bit).
2. Run TagEssentials.exe. Windows may ask for administrator permission.

The injected module and the optional muted voice runtime are embedded in this
single EXE. No separate DLL, Node.js install, npm install, or runtime folder is
required. The first start unpacks private support files to the local TagEssentials
application data folder.

OPTIONAL MUTED VOICE

The muted voice helper is included in the single EXE. On first use it creates
mutedVoiceBot.config.json and opens it so the Minecraft username can be entered.
Microsoft sign in data is stored locally in the private TagEssentials application
data folder and is never meant to be committed or shared.

OPTIONAL PUBLIC HELPERS

Maintainer builds include the Public Helpers client configuration when a valid
local public_helpers_server.txt is present during the build. Source builds need
that ignored file beside the EXE if the wins in username feature is wanted.

SECURITY AND COMPATIBILITY

TagEssentials injects a DLL into a running Java process. Antivirus or SmartScreen
may flag unsigned builds because injection is also used by malicious software.
Get maintainer provided builds only through the PrimeTag Discord, or build the
source yourself from the official GitHub repository:

    https://discord.gg/primetag
    https://github.com/Kingtendo/TagEssentials

Client updates can break runtime mappings. This build targets Minecraft 1.8.9
on current Badlion and Lunar clients. Use at your own risk and follow the rules
of every server and client you use. TagEssentials is not affiliated with Mojang,
Microsoft, Hypixel, Badlion, or Lunar Client.

Press End or close the TagEssentials window to unload the injected module.
