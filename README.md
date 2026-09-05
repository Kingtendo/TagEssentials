# TagEssentials

TagEssentials is a Windows x64 utility mod for Minecraft 1.8.9 on Badlion and
Lunar Client. It embeds a native module in a small launcher and adds TNT Tag
quality of life features without modifying the client installation on disk.

> [!WARNING]
> TagEssentials uses DLL injection and runtime hooks. That technique can trigger
> antivirus or SmartScreen warnings, and some clients or servers may prohibit
> some features. Review the source, follow the rules of the services you use,
> and run it at your own risk.

## Source and downloads

GitHub is the source only distribution for TagEssentials. No prebuilt EXE is
published through GitHub Releases; users can build the project using the steps
below. A Windows build provided by a maintainer and community support are available
through the [PrimeTag Discord](https://discord.gg/primetag).

## Features

* Configurable TNT countdown overlay, crosshair display, nametag timer, and
  scoreboard timer, including prefix placement before or after the `[IT]` tag.
  Hypixel round transitions are measured and conservatively calibrated during
  the current session so the decimal countdown better matches the actual end.
* Sound alerts for Speed III and Slowness transitions.
* Snaplook support on Badlion (Lunar already provides its own implementation).
* TNT Tag visual options such as wheat stage and beacon beam overrides.
* Optional wins labels in nametags and the tab list through the privacy limited
  Public Helpers backend, with confirmed nicked players shown as `[NICKED]`.
* Optional Mineflayer based muted voice helper with Microsoft authentication.
* Multiple animated GUI themes and per number timer colours.
* Clean unload from the window close action or the End key.

## Supported environment

* Windows 10 or 11, x64.
* Badlion Client or Lunar Client running Minecraft 1.8.9.
* Visual Studio 2022 with **Desktop development with C++** to build.
* A JDK with `JAVA_HOME` set (Temurin 21 is used by CI).
* Node.js 22 or newer for the optional muted voice helper and server tests.

Client updates can change obfuscated classes and break compatibility. A build
succeeding does not prove that every hook still works against a newly updated
client.

The current revision has been manually smoke tested on both Badlion and Lunar
Client with Minecraft 1.8.9.

## Build from source

```powershell
git clone https://github.com/Kingtendo/TagEssentials.git
cd TagEssentials
msbuild .\TagEssentials.sln /m /p:Configuration=Release /p:Platform=x64
```

Run the command from a Visual Studio 2022 Developer PowerShell so `msbuild` is
available. Node.js is not required for the native launcher or mod build.

The launcher is written to `x64\Release\TagEssentials.exe`; the injected module
is embedded in it as a resource. The build vendors MinHook source so no opaque
precompiled hooking library is required.

To create the distributable ZIP:

```powershell
.\build-release.ps1
```

The archive is created at `build\TagEssentials-windows-x64.zip`. The optional
Public Helpers client configuration is included only when a valid ignored
`public_helpers_server.txt` exists locally.

## Optional muted voice helper

The build output directory contains `mutedVoiceBot.js`. Install its pinned runtime
dependency in that directory before enabling the feature:

```powershell
npm ci --omit=dev
```

Authentication data is written to `.mineflayer-auth` and configuration to
`mutedVoiceBot.config.json`. Both are ignored by Git and must remain private.

## Optional Public Helpers backend

The wins label never embeds a Hypixel API key in the client. Instead, the client
sends a visible player UUID and name to a narrowly scoped backend that returns
only the TNT Tag wins count after validating the profile name. Deployment and
rate limit guidance is in
[`public-helpers-server/README.md`](public-helpers-server/README.md).

Copy `public_helpers_server.example.txt` to `public_helpers_server.txt` and set
the HTTPS endpoint and client token. The client token ships to users and is an
access control speed bump, not a secret; server side limits are still required.

## Security and privacy

* Never commit API keys, access tokens, `.env` files, Mineflayer authentication
  state, runtime configs, logs, class dumps, or client binaries.
* The Public Helpers server keeps the Hypixel key server side.
* Debug output is stored in `%TEMP%\TagEssentials.log`. Lines containing
  `RoundTiming` include the map, round, scoreboard tick intervals, measured end
  error, and between round timing used by the countdown calibration.
* Report vulnerabilities according to [`SECURITY.md`](SECURITY.md).

## Project layout

* `TagEssentials.cpp`: launcher and embedded module loader.
* `TagEssentialsMod/`: injected native mod and generated Lunar mapping table.
* `assets/`: embedded UI theme images.
* `public-helpers-server/`: optional wins only service.
* `mutedVoiceBot.js`: optional Mineflayer helper.
* `tools/`: mapping and diagnostic utilities.
* `third_party/`: vendored license compliant native dependencies and notices.

## Contributing and license

See [`CONTRIBUTING.md`](CONTRIBUTING.md) before sending a pull request.
TagEssentials is licensed under the [MIT License](LICENSE). Third party and
mapping data terms are listed in
[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).

TagEssentials is an independent project and is not affiliated with or endorsed
by Mojang, Microsoft, Hypixel, Badlion, or Lunar Client.
