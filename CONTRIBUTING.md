# Contributing

Bug reports and focused pull requests are welcome.

Before opening a pull request:

1. Do not commit account data, API keys, tokens, logs, client binaries, class
   dumps, or generated build output.
2. Build `TagEssentials.vcxproj` in both Debug and Release for x64.
3. Run `npm ci` followed by `npm test`.
4. Describe which Minecraft client and version you tested.

Keep changes scoped and explain any new network requests or runtime hooks in the
pull request. Features intended to evade anti-cheat or bypass access controls are
out of scope.
