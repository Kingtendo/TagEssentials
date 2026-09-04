TAGESSENTIALS - WINDOWS X64
==========================

1. Extract the entire ZIP to a normal folder. Do not run the EXE from the ZIP.
2. Start Badlion Client or Lunar Client with Minecraft 1.8.9 (64-bit).
3. Run TagEssentials.exe. Windows may ask for administrator permission.

The injected module is embedded in the EXE; no separate DLL is required.

OPTIONAL: MUTED VOICE

Install Node.js 22 or newer, open a terminal in this folder, and run:

    npm ci --omit=dev

Keep mutedVoiceBot.js, package.json, package-lock.json, and the generated
node_modules folder beside TagEssentials.exe. Microsoft sign-in data is stored
locally in .mineflayer-auth and is never meant to be committed or shared.

OPTIONAL: PUBLIC HELPERS

The wins-in-username feature requires public_helpers_server.txt beside the EXE.
That file contains the hosted service URL and client token. It is intentionally
not included in public source builds. See public_helpers_server.example.txt and
public-helpers-server/README.md in the source repository.

SECURITY AND COMPATIBILITY

TagEssentials injects a DLL into a running Java process. Antivirus or SmartScreen
may flag unsigned builds because injection is also used by malicious software.
Get maintainer-provided builds only through the PrimeTag Discord, or build the
source yourself from the official GitHub repository:

    https://discord.gg/primetag
    https://github.com/Kingtendo/TagEssentials

Client updates can break runtime mappings. This build targets Minecraft 1.8.9
on current Badlion and Lunar clients. Use at your own risk and follow the rules
of every server and client you use. TagEssentials is not affiliated with Mojang,
Microsoft, Hypixel, Badlion, or Lunar Client.

Press End or close the TagEssentials window to unload the injected module.
