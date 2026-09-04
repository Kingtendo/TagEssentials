#define _CRT_SECURE_NO_WARNINGS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <winhttp.h>
#include <Windows.h>
#include <GL/gl.h>
#include <jni.h>
#include <jvmti.h>
#include <Psapi.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <propkey.h>
#include <propvarutil.h>
#include <gdiplus.h>
#include <MinHook.h>
#include <string>
#include <fstream>
#include <mutex>
#include <cmath>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <algorithm>
#include <array>
#include <cstdint>
#include <climits>
#include <iterator>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <deque>
#include "LunarMappings.generated.h"
#include "resource.h"

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "Psapi.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Propsys.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "gdiplus.lib")

#ifndef WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2
#define WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 0x00000800
#endif

// =============================================================
// Config
// =============================================================
struct OverlayConfig {
    float xPct = 0.05f;
    float yPct = 0.05f;
    float scale = 2.0f;
    bool dragging = false;
    float dragOffsetX = 0.0f;
    float dragOffsetY = 0.0f;
    bool visible = true;

    float GetX(int screenW) { return xPct * screenW; }
    float GetY(int screenH) { return yPct * screenH; }
    void SetPixelPos(float px, float py, int screenW, int screenH) {
        if (screenW > 0) xPct = px / screenW;
        if (screenH > 0) yPct = py / screenH;
    }
    void Save(const std::string& path) {
        std::ofstream f(path);
        if (f.is_open()) f << xPct << " " << yPct << " " << scale;
    }
    void Load(const std::string& path) {
        std::ifstream f(path);
        if (f.is_open()) f >> xPct >> yPct >> scale;
    }
};

OverlayConfig g_config;
int g_explosionSeconds = -1;
char g_explosionColorCode = 'a';
std::mutex g_settingsMutex;
SRWLOCK g_debugLogLock = SRWLOCK_INIT;
char g_debugLogPath[MAX_PATH] = {};
LPTOP_LEVEL_EXCEPTION_FILTER g_prevUnhandledExceptionFilter = nullptr;
HMODULE g_moduleHandle = nullptr;
ULONG_PTR g_cosmicGdiplusToken = 0;
Gdiplus::Bitmap* g_themeBackgroundImages[6] = {};
Gdiplus::Bitmap* g_themeMotionSprites[6] = {};
Gdiplus::Bitmap* g_neonFlyingCarSprite = nullptr;
Gdiplus::Bitmap* g_cosmicPlanetSheetImage = nullptr;
HDC g_cosmicBackgroundCacheDC = nullptr;
HBITMAP g_cosmicBackgroundCacheBitmap = nullptr;
HGDIOBJ g_cosmicBackgroundCacheOldBitmap = nullptr;
std::uint32_t* g_cosmicBackgroundCacheBits = nullptr;
int g_cosmicBackgroundCacheWidth = 0;
int g_cosmicBackgroundCacheHeight = 0;
int g_cosmicBackgroundCacheTheme = -1;

struct GuiThemeEffectSample {
    int x;
    int y;
    COLORREF color;
    unsigned char kind;
};

std::vector<GuiThemeEffectSample> g_themeEffectSamples;

constexpr const char* kTimerOverlayConfigPath = "timer_overlay.cfg";
constexpr const char* kToolSettingsPath = "tool_settings.cfg";
constexpr const char* kTimerFallbackTeamName = "TagEssentials";
constexpr jsize kScoreboardDisplaySlotCount = 19;
constexpr jsize kScoreboardSidebarDisplaySlot = 1;
constexpr jsize kScoreboardTeamSidebarDisplaySlotStart = 3;
constexpr jsize kScoreboardTeamSidebarDisplaySlotEnd = 18;
constexpr int kCameraBack = 1;
constexpr int kCameraFront = 2;
constexpr float kTimerScaleMin = 0.75f;
constexpr float kTimerScaleMax = 8.00f;
constexpr int kTimerNumberMin = 0;
constexpr int kTimerNumberMax = 55;
constexpr int kTimerNumberCount = kTimerNumberMax - kTimerNumberMin + 1;
constexpr double kBetweenRoundTimerSeconds = 10.5;
constexpr float kAlertVolumeMin = 0.00f;
constexpr float kAlertVolumeMax = 2.00f;
constexpr ULONGLONG kChatAlertPollIntervalMs = 75;
constexpr int kChatAlertSnapshotLimit = 12;
constexpr ULONGLONG kSpeedTransitionDiagnosticPollIntervalMs = 10;
constexpr ULONGLONG kSpeedTransitionDiagnosticSampleIntervalMs = 50;
constexpr ULONGLONG kSpeedTransitionDiagnosticCaptureMs = 1200;
constexpr int kSeeBarriersRangeMin = 5;
constexpr int kSeeBarriersRangeMax = 30;
constexpr int kSeeBarriersRangeInfinite = 31;
constexpr int kSeeBarriersRangeDefault = 16;
constexpr int kSeeBarriersDirtBlockId = 3;
constexpr unsigned short kMutedVoiceControlPort = 49623;
constexpr const char* kMutedVoiceScriptName = "mutedVoiceBot.js";
constexpr const char* kMutedVoiceRuntimeDirFile = "muted_voice_runtime_dir.txt";
constexpr const char* kPublicHelpersServerConfigFile = "public_helpers_server.txt";
constexpr const char* kHypixelWinsCacheFile = "hypixel_tnttag_wins_cache.cfg";
constexpr ULONGLONG kHypixelWinsCacheTtlMs = 60ULL * 60ULL * 1000ULL;
constexpr ULONGLONG kHypixelWinsFailureRetryMs = 2ULL * 60ULL * 1000ULL;
constexpr ULONGLONG kTntTagContextGraceMs = 2000ULL;

std::uint32_t PackTimerColour(int red, int green, int blue) {
    return ((std::uint32_t)(red & 0xFF) << 16) |
        ((std::uint32_t)(green & 0xFF) << 8) |
        (std::uint32_t)(blue & 0xFF);
}

int ClampTimerNumber(int number) {
    if (number < kTimerNumberMin) return kTimerNumberMin;
    if (number > kTimerNumberMax) return kTimerNumberMax;
    return number;
}

std::uint32_t GetDefaultTimerNumberColour(int number) {
    number = ClampTimerNumber(number);
    if (number >= 16) return PackTimerColour(85, 255, 85);
    if (number >= 6) return PackTimerColour(255, 170, 0);
    if (number >= 3) return PackTimerColour(255, 85, 85);
    if (number >= 2) return PackTimerColour(0, 0, 170);
    if (number >= 1) return PackTimerColour(0, 170, 170);
    return PackTimerColour(170, 0, 170);
}

std::array<std::uint32_t, kTimerNumberCount> g_timerNumberColours = []() {
    std::array<std::uint32_t, kTimerNumberCount> colours = {};
    for (int number = kTimerNumberMin; number <= kTimerNumberMax; ++number) {
        colours[number] = GetDefaultTimerNumberColour(number);
    }
    return colours;
}();

struct MinecraftColourOption {
    char code;
    const char* name;
    int red;
    int green;
    int blue;
};

static const std::array<MinecraftColourOption, 16> kMinecraftColourOptions = { {
    { '0', "Black",         0,   0,   0 },
    { '1', "Dark Blue",     0,   0, 170 },
    { '2', "Dark Green",    0, 170,   0 },
    { '3', "Dark Aqua",     0, 170, 170 },
    { '4', "Dark Red",    170,   0,   0 },
    { '5', "Dark Purple", 170,   0, 170 },
    { '6', "Gold",        255, 170,   0 },
    { '7', "Gray",        170, 170, 170 },
    { '8', "Dark Gray",    85,  85,  85 },
    { '9', "Blue",         85,  85, 255 },
    { 'a', "Green",        85, 255,  85 },
    { 'b', "Aqua",         85, 255, 255 },
    { 'c', "Red",         255,  85,  85 },
    { 'd', "Light Purple",255,  85, 255 },
    { 'e', "Yellow",      255, 255,  85 },
    { 'f', "White",       255, 255, 255 }
} };

enum SeeBarriersRenderStyle {
    SEE_BARRIERS_STYLE_BOX_OUTLINE = 0,
    SEE_BARRIERS_STYLE_OUTLINE = 1
};

enum AlertSoundId {
    ALERT_SOUND_NOTE_PLING = 0,
    ALERT_SOUND_NOTE_HARP,
    ALERT_SOUND_NOTE_BASS,
    ALERT_SOUND_NOTE_HAT,
    ALERT_SOUND_NOTE_SNARE,
    ALERT_SOUND_NOTE_BASSATTACK,
    ALERT_SOUND_NOTE_BD,
    ALERT_SOUND_RANDOM_ORB,
    ALERT_SOUND_RANDOM_CLICK,
    ALERT_SOUND_RANDOM_LEVELUP,
    ALERT_SOUND_RANDOM_ANVIL_LAND,
    ALERT_SOUND_RANDOM_POP,
    ALERT_SOUND_RANDOM_SUCCESSFUL_HIT,
    ALERT_SOUND_MOB_BLAZE_HIT,
    ALERT_SOUND_MOB_ENDERMEN_PORTAL,
    ALERT_SOUND_MOB_GUARDIAN_ELDER_IDLE,
    ALERT_SOUND_MOB_BAT_IDLE,
    ALERT_SOUND_MOB_CHICKEN_PLOP,
    ALERT_SOUND_MOB_VILLAGER_YES,
    ALERT_SOUND_MOB_VILLAGER_NO,
    ALERT_SOUND_MOB_WOLF_BARK,
    ALERT_SOUND_MOB_CAT_MEOW,
    ALERT_SOUND_MOB_GHAST_SCREAM,
    ALERT_SOUND_MOB_ZOMBIE_REMEDY,
    ALERT_SOUND_FIREWORKS_BLAST,
    ALERT_SOUND_FIREWORKS_TWINKLE,
    ALERT_SOUND_RANDOM_GLASS,
    ALERT_SOUND_RANDOM_BOW,
    ALERT_SOUND_RANDOM_BOWHIT,
    ALERT_SOUND_FIRE_IGNITE
};

struct AlertSoundOption {
    const char* name;
    const char* label;
};

static const AlertSoundOption kAlertSoundOptions[] = {
    { "note.pling", "Pling" },
    { "note.harp", "Harp" },
    { "note.bass", "Bass" },
    { "note.hat", "Hat" },
    { "note.snare", "Snare" },
    { "note.bassattack", "Bass Attack" },
    { "note.bd", "Bass Drum" },
    { "random.orb", "Orb Pickup" },
    { "random.click", "Click" },
    { "random.levelup", "Level Up" },
    { "random.anvil_land", "Anvil" },
    { "random.pop", "Pop" },
    { "random.successful_hit", "Hit" },
    { "mob.blaze.hit", "Blaze Hit" },
    { "mob.endermen.portal", "Enderman Portal" },
    { "mob.guardian.elder.idle", "Elder Guardian" },
    { "mob.bat.idle", "Bat" },
    { "mob.chicken.plop", "Chicken Plop" },
    { "mob.villager.yes", "Villager Yes" },
    { "mob.villager.no", "Villager No" },
    { "mob.wolf.bark", "Wolf Bark" },
    { "mob.cat.meow", "Cat Meow" },
    { "mob.ghast.scream", "Ghast Scream" },
    { "mob.zombie.remedy", "Zombie Cure" },
    { "fireworks.blast", "Firework Blast" },
    { "fireworks.twinkle", "Firework Twinkle" },
    { "dig.glass", "Glass Break" },
    { "random.bow", "Bow Shoot" },
    { "random.bowhit", "Arrow Hit" },
    { "fire.ignite", "Fire Ignite" },
    { "mob.cow.say", "Cow Moo" },
    { "mob.cow.hurt", "Cow Hurt" },
    { "gui.button.press", "Button Press" },
    { "random.anvil_break", "Anvil Break" },
    { "random.anvil_use", "Anvil Use" },
    { "random.burp", "Burp" },
    { "random.chestopen", "Chest Open" },
    { "random.chestclosed", "Chest Close" },
    { "random.door_open", "Door Open" },
    { "random.door_close", "Door Close" },
    { "random.drink", "Drink" },
    { "random.eat", "Eat" },
    { "random.explode", "Explosion" },
    { "random.fizz", "Fizz" },
    { "random.splash", "Splash" },
    { "random.wood_click", "Wood Click" },
    { "game.tnt.primed", "TNT Primed" },
    { "creeper.primed", "Creeper Primed" },
    { "game.potion.smash", "Potion Smash" },
    { "portal.portal", "Portal Ambient" },
    { "portal.travel", "Portal Travel" },
    { "portal.trigger", "Portal Trigger" },
    { "ambient.cave.cave", "Cave Ambience" },
    { "ambient.weather.rain", "Rain" },
    { "ambient.weather.thunder", "Thunder" },
    { "liquid.lava", "Lava" },
    { "liquid.lavapop", "Lava Pop" },
    { "liquid.water", "Water" },
    { "tile.piston.in", "Piston Retract" },
    { "tile.piston.out", "Piston Extend" },
    { "fireworks.launch", "Firework Launch" },
    { "fireworks.largeBlast", "Large Firework" },
    { "fireworks.blast_far", "Distant Firework" },
    { "fireworks.twinkle_far", "Distant Twinkle" },
    { "mob.bat.takeoff", "Bat Takeoff" },
    { "mob.bat.death", "Bat Death" },
    { "mob.blaze.breathe", "Blaze Breathe" },
    { "mob.blaze.death", "Blaze Death" },
    { "mob.cat.hiss", "Cat Hiss" },
    { "mob.cat.purr", "Cat Purr" },
    { "mob.cat.purreow", "Cat Purreow" },
    { "mob.cat.hitt", "Cat Hurt" },
    { "mob.chicken.say", "Chicken Cluck" },
    { "mob.chicken.hurt", "Chicken Hurt" },
    { "mob.pig.say", "Pig Oink" },
    { "mob.pig.death", "Pig Death" },
    { "mob.sheep.say", "Sheep Baa" },
    { "mob.sheep.shear", "Sheep Shear" },
    { "mob.wolf.growl", "Wolf Growl" },
    { "mob.wolf.howl", "Wolf Howl" },
    { "mob.wolf.whine", "Wolf Whine" },
    { "mob.wolf.hurt", "Wolf Hurt" },
    { "mob.wolf.death", "Wolf Death" },
    { "mob.villager.idle", "Villager Hmm" },
    { "mob.villager.haggle", "Villager Haggle" },
    { "mob.villager.hit", "Villager Hurt" },
    { "mob.villager.death", "Villager Death" },
    { "mob.endermen.idle", "Enderman Idle" },
    { "mob.endermen.scream", "Enderman Scream" },
    { "mob.endermen.stare", "Enderman Stare" },
    { "mob.endermen.hit", "Enderman Hurt" },
    { "mob.endermen.death", "Enderman Death" },
    { "mob.ghast.charge", "Ghast Charge" },
    { "mob.ghast.fireball", "Ghast Fireball" },
    { "mob.ghast.moan", "Ghast Moan" },
    { "mob.ghast.death", "Ghast Death" },
    { "mob.ghast.affectionate_scream", "Ghast Affectionate" },
    { "mob.creeper.say", "Creeper Hurt" },
    { "mob.creeper.death", "Creeper Death" },
    { "mob.enderdragon.growl", "Dragon Growl" },
    { "mob.enderdragon.wings", "Dragon Wings" },
    { "mob.enderdragon.hit", "Dragon Hurt" },
    { "mob.enderdragon.end", "Dragon Death" },
    { "mob.guardian.curse", "Guardian Curse" },
    { "mob.guardian.attack", "Guardian Attack" },
    { "mob.guardian.flop", "Guardian Flop" },
    { "mob.guardian.death", "Guardian Death" },
    { "mob.guardian.elder.death", "Elder Guardian Death" },
    { "mob.horse.idle", "Horse Idle" },
    { "mob.horse.angry", "Horse Angry" },
    { "mob.horse.jump", "Horse Jump" },
    { "mob.horse.gallop", "Horse Gallop" },
    { "mob.horse.death", "Horse Death" },
    { "mob.horse.donkey.idle", "Donkey Hee-Haw" },
    { "mob.irongolem.throw", "Iron Golem Throw" },
    { "mob.irongolem.walk", "Iron Golem Walk" },
    { "mob.irongolem.hit", "Iron Golem Hurt" },
    { "mob.irongolem.death", "Iron Golem Death" },
    { "mob.rabbit.idle", "Rabbit Idle" },
    { "mob.rabbit.hop", "Rabbit Hop" },
    { "mob.rabbit.hurt", "Rabbit Hurt" },
    { "mob.rabbit.death", "Rabbit Death" },
    { "mob.slime.attack", "Slime Attack" },
    { "mob.slime.big", "Big Slime" },
    { "mob.slime.small", "Small Slime" },
    { "mob.magmacube.jump", "Magma Cube Jump" },
    { "mob.magmacube.big", "Big Magma Cube" },
    { "mob.magmacube.small", "Small Magma Cube" },
    { "mob.silverfish.say", "Silverfish Idle" },
    { "mob.silverfish.step", "Silverfish Step" },
    { "mob.silverfish.hit", "Silverfish Hurt" },
    { "mob.silverfish.kill", "Silverfish Death" },
    { "mob.skeleton.say", "Skeleton Rattle" },
    { "mob.skeleton.hurt", "Skeleton Hurt" },
    { "mob.skeleton.death", "Skeleton Death" },
    { "mob.spider.say", "Spider Idle" },
    { "mob.spider.step", "Spider Step" },
    { "mob.spider.death", "Spider Death" },
    { "mob.zombie.say", "Zombie Groan" },
    { "mob.zombie.hurt", "Zombie Hurt" },
    { "mob.zombie.death", "Zombie Death" },
    { "mob.zombie.infect", "Zombie Infect" },
    { "mob.zombie.woodbreak", "Zombie Break Door" },
    { "mob.zombiepig.zpig", "Zombie Pigman Idle" },
    { "mob.zombiepig.zpigangry", "Zombie Pigman Angry" },
    { "mob.zombiepig.zpighurt", "Zombie Pigman Hurt" },
    { "mob.zombiepig.zpigdeath", "Zombie Pigman Death" },
    { "mob.wither.idle", "Wither Idle" },
    { "mob.wither.hurt", "Wither Hurt" },
    { "mob.wither.shoot", "Wither Shoot" },
    { "mob.wither.spawn", "Wither Spawn" },
    { "mob.wither.death", "Wither Death" },
    { "dig.cloth", "Break Cloth" },
    { "dig.grass", "Break Grass" },
    { "dig.gravel", "Break Gravel" },
    { "dig.sand", "Break Sand" },
    { "dig.snow", "Break Snow" },
    { "dig.stone", "Break Stone" },
    { "dig.wood", "Break Wood" },
    { "step.cloth", "Step Cloth" },
    { "step.grass", "Step Grass" },
    { "step.gravel", "Step Gravel" },
    { "step.ladder", "Step Ladder" },
    { "step.sand", "Step Sand" },
    { "step.snow", "Step Snow" },
    { "step.stone", "Step Stone" },
    { "step.wood", "Step Wood" },
    { "records.11", "Music Disc 11" },
    { "records.13", "Music Disc 13" },
    { "records.blocks", "Music Disc Blocks" },
    { "records.cat", "Music Disc Cat" },
    { "records.chirp", "Music Disc Chirp" },
    { "records.far", "Music Disc Far" },
    { "records.mall", "Music Disc Mall" },
    { "records.mellohi", "Music Disc Mellohi" },
    { "records.stal", "Music Disc Stal" },
    { "records.strad", "Music Disc Strad" },
    { "records.wait", "Music Disc Wait" },
    { "records.ward", "Music Disc Ward" }
};

constexpr int ALERT_SOUND_COUNT = (int)(sizeof(kAlertSoundOptions) / sizeof(kAlertSoundOptions[0]));

enum GuiTheme {
    GUI_THEME_CLASSIC = 0,
    GUI_THEME_COSMIC,
    GUI_THEME_NEON_CITY,
    GUI_THEME_ENCHANTED_FOREST,
    GUI_THEME_INFERNO,
    GUI_THEME_ARCTIC_AURORA,
    GUI_THEME_COUNT
};

enum PublicWinsPosition {
    PUBLIC_WINS_POSITION_PREFIX = 0,
    PUBLIC_WINS_POSITION_SUFFIX = 1
};

enum TimerNametagPosition {
    TIMER_NAMETAG_POSITION_PREFIX = 0,
    TIMER_NAMETAG_POSITION_SUFFIX = 1
};

int g_guiTheme = GUI_THEME_COSMIC;

int NormalizeGuiTheme(int value) {
    return value >= GUI_THEME_CLASSIC && value < GUI_THEME_COUNT ? value : GUI_THEME_COSMIC;
}

bool IsClassicGuiTheme() { return NormalizeGuiTheme(g_guiTheme) == GUI_THEME_CLASSIC; }
bool IsCosmicGuiTheme() { return NormalizeGuiTheme(g_guiTheme) == GUI_THEME_COSMIC; }
bool IsAnimatedGuiTheme() { return !IsClassicGuiTheme(); }

const char* GetGuiThemeName(int theme) {
    switch (NormalizeGuiTheme(theme)) {
    case GUI_THEME_CLASSIC: return "Classic";
    case GUI_THEME_COSMIC: return "Cosmic";
    case GUI_THEME_NEON_CITY: return "Neon City";
    case GUI_THEME_ENCHANTED_FOREST: return "Enchanted Forest";
    case GUI_THEME_INFERNO: return "Inferno";
    case GUI_THEME_ARCTIC_AURORA: return "Arctic Aurora";
    default: return "Cosmic";
    }
}

const char* GetGuiThemeSubtitle(int theme) {
    switch (NormalizeGuiTheme(theme)) {
    case GUI_THEME_CLASSIC: return "Original dark interface";
    case GUI_THEME_COSMIC: return "Galaxy and orbiting planets";
    case GUI_THEME_NEON_CITY: return "Rainy cyan-magenta skyline";
    case GUI_THEME_ENCHANTED_FOREST: return "Fireflies in moonlit woodland";
    case GUI_THEME_INFERNO: return "Lava flows and rising embers";
    case GUI_THEME_ARCTIC_AURORA: return "Aurora light and layered blizzards";
    default: return "Galaxy and orbiting planets";
    }
}

// GUI toggles
bool g_guiSnaplookEnabled = false;
bool g_guiTimerEnabled = true;
bool g_guiTimerLocked = false;
bool g_guiTimerNametagEnabled = true;
int  g_guiTimerNametagPosition = TIMER_NAMETAG_POSITION_SUFFIX;
bool g_guiTimerEditDefaultScoreboard = false;
bool g_guiTimerCrosshairMode = false;
bool g_guiTimerObsScreenshotsEnabled = false;
int  g_snaplookKeybind = VK_MENU;
int  g_snaplookCameraMode = kCameraFront;
int  g_guiTimerDecimalPlaces = 1;
bool g_guiSpeedSlownessEnabled = true;
int  g_speed3Sound = ALERT_SOUND_RANDOM_LEVELUP;
int  g_slownessSound = ALERT_SOUND_RANDOM_ANVIL_LAND;
float g_speed3Volume = 1.0f;
float g_slownessVolume = 1.0f;
bool g_guiPublicWinsEnabled = false;
int  g_guiPublicWinsPosition = PUBLIC_WINS_POSITION_PREFIX;
bool g_guiPublicWinsSpaceBetweenUsername = false;


// Add these:
bool g_guiExtrasForceWheatStage1 = false;
bool g_guiExtrasHideBeaconBeams = false;
bool g_guiExtrasDisableTagScoreboard = false;
bool g_guiExtrasSeeBarriers = false;
bool g_guiExtrasMutedVoice = false;
bool g_guiExtrasMutedVoiceHideMuteReminder = false;
std::string g_guiMutedVoicePartyOwner;
int  g_guiSeeBarriersRange = kSeeBarriersRangeDefault;
int  g_guiSeeBarriersStyle = SEE_BARRIERS_STYLE_BOX_OUTLINE;

LARGE_INTEGER g_perfFreq;
LARGE_INTEGER g_explosionSetAt;
bool g_timerActive = false;
double g_timerStartSeconds = -1.0;
bool g_betweenRoundsTimerActive = false;
bool g_roundTimerObserved = false;
ULONGLONG g_betweenRoundsStartedAtMs = 0;
ULONGLONG g_lastRoundTimerSeenMs = 0;
volatile LONG g_tntTagGameActive = 0;
volatile LONG g_hypixelTntTagGameActive = 0;
ULONGLONG g_lastTntTagContextSeenMs = 0;
ULONGLONG g_lastHypixelTntTagContextSeenMs = 0;
ULONGLONG g_lastPublicWinsPrefetchMs = 0;
ULONGLONG g_lastTimerNametagUpdateMs = 0;
ULONGLONG g_lastDefaultScoreboardTimerUpdateMs = 0;
bool g_lastDefaultScoreboardBetweenRounds = false;

struct TeamSuffixState {
    std::string baseSuffix;
    std::string appliedSuffix;
};

std::unordered_map<std::string, TeamSuffixState> g_teamSuffixCache;

struct ScoreboardTimerLineState {
    std::string basePrefix;
    std::string baseSuffix;
    std::string appliedPrefix;
    std::string appliedSuffix;
};

std::unordered_map<std::string, ScoreboardTimerLineState> g_scoreboardTimerLineCache;

struct PublicWinsTeamFormatState {
    std::string playerName;
    std::string playerUuid;
    std::string sourceTeamName;
    std::string localTeamName;
    bool sourceHadTeam = false;
    std::string basePrefix;
    std::string baseSuffix;
    std::string appliedPrefix;
    std::string appliedSuffix;
};

std::unordered_map<std::string, PublicWinsTeamFormatState> g_publicWinsTeamFormatCache;
ULONGLONG g_lastPublicWinsTeamUpdateMs = 0;

// Snaplook state
bool g_snaplookActive = false;
int  g_snaplookSavedPerspective = 0;
volatile LONG g_perspectiveRefreshPending = 0;
volatile LONG g_perspectiveRenderingSyncedValue = -1;

int g_screenW = 0;
int g_screenH = 0;
float g_mouseX = 0.0f;
float g_mouseY = 0.0f;
HWND g_hookedHwnd = nullptr;
HWND g_gameRenderHwnd = nullptr;
HWND g_guiHwnd = nullptr;
HANDLE g_guiThreadHandle = nullptr;
DWORD g_guiThreadId = 0;
volatile LONG g_shutdownRequested = 0;
volatile LONG g_unloadRequested = 0;

float ClampFloat(float value, float minValue, float maxValue) {
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

int NormalizePerspectiveMode(int value) {
    return value == kCameraFront ? kCameraFront : kCameraBack;
}

int NormalizeTimerDecimalPlaces(int value) {
    if (value <= 0) return 0;
    return value == 2 ? 2 : 1;
}

int NormalizeTimerNametagPosition(int value) {
    return value == TIMER_NAMETAG_POSITION_PREFIX
        ? TIMER_NAMETAG_POSITION_PREFIX
        : TIMER_NAMETAG_POSITION_SUFFIX;
}

std::uint32_t GetTimerNumberColour(int number) {
    return g_timerNumberColours[ClampTimerNumber(number)] & 0x00FFFFFFu;
}

int GetTimerColourRed(std::uint32_t colour) {
    return (int)((colour >> 16) & 0xFFu);
}

int GetTimerColourGreen(std::uint32_t colour) {
    return (int)((colour >> 8) & 0xFFu);
}

int GetTimerColourBlue(std::uint32_t colour) {
    return (int)(colour & 0xFFu);
}

void SetTimerNumberColour(int number, int red, int green, int blue) {
    if (number < kTimerNumberMin || number > kTimerNumberMax) return;
    red = max(0, min(255, red));
    green = max(0, min(255, green));
    blue = max(0, min(255, blue));
    g_timerNumberColours[number] = PackTimerColour(red, green, blue);
}

int NormalizeAlertSoundId(int value, int fallback) {
    return (value >= 0 && value < ALERT_SOUND_COUNT) ? value : fallback;
}

const char* GetAlertSoundName(int soundId);
const char* GetAlertSoundLabel(int soundId);
std::string ToLowerAscii(const std::string& value);
RECT MakeRectWH(int left, int top, int width, int height);
void FillRoundedRect(HDC hdc, const RECT& rect, COLORREF fill, COLORREF border, int radius);
void DrawTextLine(HDC hdc, const RECT& rect, const std::string& text, COLORREF color, UINT format);
void DrawButtonChip(HDC hdc, const RECT& rect, const std::string& text, bool active, bool accent);
void DrawTextInputField(HDC hdc, const RECT& rect, const std::string& text, const std::string& placeholder, bool active);
void ClampSoundPickerScroll();
void RequestGuiRepaint();
void RequestModuleUnload(const char* source);
bool IsModuleUnloadRequested();
void SaveToolSettings();
void SetMutedVoiceModuleEnabled(bool enabled);
bool QueueMutedVoiceChatMessage(const std::string& message);
bool QueueMutedVoiceControlCommand(const std::string& command);
std::string GetMutedVoiceStatusText();
void BeginMutedVoiceSignOut();
void OpenMutedVoiceAuthUrl();
void CopyMutedVoiceAuthCode();
void SetMutedVoicePartyOwnerFromGui(const std::string& value);
std::string NormalizeMutedVoicePartyOwner(const std::string& value);
bool EnsureNameTagHook(JNIEnv* env);
void RestoreNameTagHook(JNIEnv* env);
void SetPublicWinsEnabled(bool enabled);
void StartPublicWinsWorker();
void StopPublicWinsWorker();
bool EnsurePublicWinsTabNameHook(JNIEnv* env);
bool EnsurePublicWinsApiTabHook(JNIEnv* env);
bool EnsurePublicWinsScoreboardFormatHook(JNIEnv* env);
bool EnsurePublicWinsRenderedNameHook(JNIEnv* env);
void RestorePublicWinsTabNameHook(JNIEnv* env);
bool CaptureRuntimeClassBytes(JNIEnv* env, jclass targetClass, std::vector<unsigned char>& outBytes, const char* label);
void QueuePublicWinsForWorldPlayers();
void ApplyPublicWinsToPlayerTeams(bool enablePublicWins);
bool IsLunarNamedClient();

bool IsPerspectiveModuleEnabled() {
    // Lunar already provides Snaplook. Keep the user's saved preference intact
    // so it becomes effective again when they inject into Badlion.
    return g_guiSnaplookEnabled && !IsLunarNamedClient();
}

void SetPerspectiveModuleEnabled(bool enabled) {
    if (IsLunarNamedClient()) return;
    g_guiSnaplookEnabled = enabled;
}

void DebugLog(const char* fmt, ...) {
    AcquireSRWLockExclusive(&g_debugLogLock);

    if (g_debugLogPath[0] == '\0') {
        char tempPath[MAX_PATH] = {};
        DWORD length = GetTempPathA(MAX_PATH, tempPath);
        if (length == 0 || length >= MAX_PATH) {
            strcpy_s(tempPath, ".");
        }
        snprintf(g_debugLogPath, sizeof(g_debugLogPath), "%s\\TagEssentials.log", tempPath);
    }

    HANDLE file = CreateFileA(
        g_debugLogPath,
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (file != INVALID_HANDLE_VALUE) {
        char buffer[2048] = {};
        SYSTEMTIME st = {};
        GetLocalTime(&st);
        int prefix = snprintf(
            buffer,
            sizeof(buffer),
            "[%02u:%02u:%02u.%03u pid=%lu tid=%lu] ",
            st.wHour,
            st.wMinute,
            st.wSecond,
            st.wMilliseconds,
            GetCurrentProcessId(),
            GetCurrentThreadId());

        if (prefix < 0) prefix = 0;
        if (prefix >= (int)sizeof(buffer)) prefix = (int)sizeof(buffer) - 1;

        va_list args;
        va_start(args, fmt);
        int body = vsnprintf(buffer + prefix, sizeof(buffer) - prefix, fmt, args);
        va_end(args);

        int total = prefix;
        if (body > 0) total += body;
        if (total >= (int)sizeof(buffer) - 3) total = (int)sizeof(buffer) - 3;
        buffer[total++] = '\r';
        buffer[total++] = '\n';
        buffer[total] = '\0';

        DWORD written = 0;
        WriteFile(file, buffer, (DWORD)total, &written, nullptr);
        FlushFileBuffers(file);
        CloseHandle(file);
    }

    ReleaseSRWLockExclusive(&g_debugLogLock);
}

void RequestModuleUnload(const char* source) {
    if (InterlockedCompareExchange(&g_unloadRequested, 1, 0) == 0) {
        DebugLog("Module unload requested source=%s", source ? source : "unknown");
    }
}

bool IsModuleUnloadRequested() {
    return InterlockedCompareExchange(&g_unloadRequested, 0, 0) != 0;
}

void ResetDebugLogFile() {
    AcquireSRWLockExclusive(&g_debugLogLock);

    if (g_debugLogPath[0] == '\0') {
        char tempPath[MAX_PATH] = {};
        DWORD length = GetTempPathA(MAX_PATH, tempPath);
        if (length == 0 || length >= MAX_PATH) {
            strcpy_s(tempPath, ".");
        }
        snprintf(g_debugLogPath, sizeof(g_debugLogPath), "%s\\TagEssentials.log", tempPath);
    }

    HANDLE file = CreateFileA(
        g_debugLogPath,
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);

    ReleaseSRWLockExclusive(&g_debugLogLock);
}

enum MutedVoiceBotStatus {
    MUTED_VOICE_STATUS_OFFLINE = 0,
    MUTED_VOICE_STATUS_CONNECTING,
    MUTED_VOICE_STATUS_ONLINE,
    MUTED_VOICE_STATUS_IN_PARTY,
    MUTED_VOICE_STATUS_ERROR
};

std::mutex g_mutedVoiceMutex;
MutedVoiceBotStatus g_mutedVoiceStatus = MUTED_VOICE_STATUS_OFFLINE;
std::string g_mutedVoiceStatusDetail;
std::string g_mutedVoiceAuthCode;
std::string g_mutedVoiceAuthUrl;
std::string g_mutedVoiceAuthMessage;
HANDLE g_mutedVoiceThreadHandle = nullptr;
HANDLE g_mutedVoiceStopEvent = nullptr;
HANDLE g_mutedVoiceQueueEvent = nullptr;
HANDLE g_mutedVoiceProcessHandle = nullptr;
DWORD g_mutedVoiceProcessId = 0;
std::deque<std::string> g_mutedVoiceChatQueue;
std::deque<std::string> g_mutedVoiceControlQueue;
std::mutex g_mutedVoiceLocalChatMutex;
std::deque<std::string> g_mutedVoiceLocalChatJsonQueue;
volatile LONG g_mutedVoiceWorkerRunning = 0;
bool g_mutedVoiceSignOutInProgress = false;

const char* MutedVoiceStatusName(MutedVoiceBotStatus status) {
    switch (status) {
    case MUTED_VOICE_STATUS_CONNECTING: return "connecting";
    case MUTED_VOICE_STATUS_ONLINE: return "online";
    case MUTED_VOICE_STATUS_IN_PARTY: return "inParty";
    case MUTED_VOICE_STATUS_ERROR: return "error";
    case MUTED_VOICE_STATUS_OFFLINE:
    default:
        return "offline";
    }
}

MutedVoiceBotStatus ParseMutedVoiceStatus(const std::string& status) {
    if (status == "connecting") return MUTED_VOICE_STATUS_CONNECTING;
    if (status == "online") return MUTED_VOICE_STATUS_ONLINE;
    if (status == "inParty") return MUTED_VOICE_STATUS_IN_PARTY;
    if (status == "error") return MUTED_VOICE_STATUS_ERROR;
    return MUTED_VOICE_STATUS_OFFLINE;
}

void ClearMutedVoiceAuthPromptLocked();

void SetMutedVoiceStatus(MutedVoiceBotStatus status, const std::string& detail = "") {
    bool changed = false;
    {
        std::lock_guard<std::mutex> lock(g_mutedVoiceMutex);
        changed = g_mutedVoiceStatus != status || g_mutedVoiceStatusDetail != detail;
        g_mutedVoiceStatus = status;
        g_mutedVoiceStatusDetail = detail;
        if (status != MUTED_VOICE_STATUS_CONNECTING) {
            ClearMutedVoiceAuthPromptLocked();
        }
    }

    if (changed) {
        DebugLog("Muted Voice status=%s detailPresent=%d", MutedVoiceStatusName(status), detail.empty() ? 0 : 1);
        RequestGuiRepaint();
    }
}

std::string GetMutedVoiceStatusText() {
    std::lock_guard<std::mutex> lock(g_mutedVoiceMutex);
    return MutedVoiceStatusName(g_mutedVoiceStatus);
}

struct MutedVoiceGuiSnapshot {
    std::string status;
    std::string detail;
    std::string authCode;
    std::string authUrl;
    std::string authMessage;
    bool signOutInProgress = false;
};

MutedVoiceGuiSnapshot GetMutedVoiceGuiSnapshot() {
    std::lock_guard<std::mutex> lock(g_mutedVoiceMutex);
    MutedVoiceGuiSnapshot snapshot;
    snapshot.status = MutedVoiceStatusName(g_mutedVoiceStatus);
    snapshot.detail = g_mutedVoiceStatusDetail;
    snapshot.authCode = g_mutedVoiceAuthCode;
    snapshot.authUrl = g_mutedVoiceAuthUrl;
    snapshot.authMessage = g_mutedVoiceAuthMessage;
    snapshot.signOutInProgress = g_mutedVoiceSignOutInProgress;
    return snapshot;
}

void ClearMutedVoiceAuthPromptLocked() {
    g_mutedVoiceAuthCode.clear();
    g_mutedVoiceAuthUrl.clear();
    g_mutedVoiceAuthMessage.clear();
}

void ClearMutedVoiceAuthPrompt() {
    {
        std::lock_guard<std::mutex> lock(g_mutedVoiceMutex);
        ClearMutedVoiceAuthPromptLocked();
    }
    RequestGuiRepaint();
}

void SetMutedVoiceAuthPrompt(const std::string& code, const std::string& url, const std::string& message) {
    {
        std::lock_guard<std::mutex> lock(g_mutedVoiceMutex);
        g_mutedVoiceAuthCode = code;
        g_mutedVoiceAuthUrl = url;
        g_mutedVoiceAuthMessage = message;
    }

    DebugLog("Muted Voice auth prompt updated codePresent=%d urlPresent=%d", code.empty() ? 0 : 1, url.empty() ? 0 : 1);
    RequestGuiRepaint();
}

std::string TrimAscii(std::string value) {
    while (!value.empty() && (value.back() == '\r' || value.back() == '\n' || value.back() == ' ' || value.back() == '\t')) {
        value.pop_back();
    }
    size_t first = 0;
    while (first < value.size() && (value[first] == ' ' || value[first] == '\t' || value[first] == '"')) ++first;
    size_t last = value.size();
    while (last > first && value[last - 1] == '"') --last;
    return value.substr(first, last - first);
}

bool IsMutedVoicePartyOwnerChar(char ch) {
    return (ch >= 'A' && ch <= 'Z') ||
        (ch >= 'a' && ch <= 'z') ||
        (ch >= '0' && ch <= '9') ||
        ch == '_';
}

std::string NormalizeMutedVoicePartyOwner(const std::string& value) {
    std::string result;
    result.reserve(16);

    for (char ch : value) {
        if (result.empty() && ch == '@') continue;
        if (!IsMutedVoicePartyOwnerChar(ch)) continue;

        result.push_back(ch);
        if (result.size() >= 16) break;
    }

    return result;
}

std::string JoinPathA(const std::string& dir, const std::string& file) {
    if (dir.empty()) return file;
    char last = dir.back();
    if (last == '\\' || last == '/') return dir + file;
    return dir + "\\" + file;
}

bool FileExistsA(const std::string& path) {
    DWORD attrs = GetFileAttributesA(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

std::string DirectoryFromPathA(const std::string& path) {
    size_t slash = path.find_last_of("\\/");
    if (slash == std::string::npos) return "";
    return path.substr(0, slash);
}

std::string GetModuleDirectoryA(HMODULE module) {
    char path[MAX_PATH] = {};
    DWORD length = GetModuleFileNameA(module, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return "";
    return DirectoryFromPathA(path);
}

void AddMutedVoiceCandidateDir(std::vector<std::string>& dirs, const std::string& dir) {
    if (dir.empty()) return;
    for (const std::string& existing : dirs) {
        if (_stricmp(existing.c_str(), dir.c_str()) == 0) return;
    }
    dirs.push_back(dir);
}

bool ReadMutedVoiceRuntimeDir(const std::string& moduleDir, std::string& runtimeDir) {
    std::ifstream f(JoinPathA(moduleDir, kMutedVoiceRuntimeDirFile), std::ios::binary);
    if (!f.is_open()) return false;

    std::string line;
    std::getline(f, line);
    runtimeDir = TrimAscii(line);
    return !runtimeDir.empty();
}

bool ResolveMutedVoiceRuntime(std::string& workingDir, std::string& scriptPath) {
    std::vector<std::string> candidates;

    std::string moduleDir = GetModuleDirectoryA(g_moduleHandle);
    std::string stagedRuntimeDir;
    if (ReadMutedVoiceRuntimeDir(moduleDir, stagedRuntimeDir)) {
        AddMutedVoiceCandidateDir(candidates, stagedRuntimeDir);
    }

    AddMutedVoiceCandidateDir(candidates, GetModuleDirectoryA(nullptr));
    AddMutedVoiceCandidateDir(candidates, moduleDir);

    char cwd[MAX_PATH] = {};
    DWORD cwdLength = GetCurrentDirectoryA(MAX_PATH, cwd);
    if (cwdLength > 0 && cwdLength < MAX_PATH) {
        AddMutedVoiceCandidateDir(candidates, cwd);
    }

    for (const std::string& dir : candidates) {
        std::string candidateScript = JoinPathA(dir, kMutedVoiceScriptName);
        if (FileExistsA(candidateScript)) {
            workingDir = dir;
            scriptPath = candidateScript;
            return true;
        }
    }

    return false;
}

bool IsHexAscii(char ch) {
    return (ch >= '0' && ch <= '9') ||
        (ch >= 'a' && ch <= 'f') ||
        (ch >= 'A' && ch <= 'F');
}

bool IsUuidLookupId(const std::string& value) {
    std::string id = TrimAscii(value);
    if (id.size() != 32 && id.size() != 36) return false;

    for (size_t i = 0; i < id.size(); ++i) {
        if (id.size() == 36 && (i == 8 || i == 13 || i == 18 || i == 23)) {
            if (id[i] != '-') return false;
            continue;
        }
        if (!IsHexAscii(id[i])) return false;
    }
    return true;
}

std::string NormalizePlayerKey(const std::string& player) {
    return ToLowerAscii(TrimAscii(player));
}

bool IsSafeSingleLineConfigValue(const std::string& value) {
    if (value.empty()) return false;
    for (char ch : value) {
        if (ch == '\r' || ch == '\n') return false;
    }
    return true;
}

void AddRuntimeCandidateDir(std::vector<std::string>& dirs, const std::string& dir) {
    if (dir.empty()) return;
    for (const std::string& existing : dirs) {
        if (_stricmp(existing.c_str(), dir.c_str()) == 0) return;
    }
    dirs.push_back(dir);
}

std::vector<std::string> GetRuntimeConfigCandidateDirs() {
    std::vector<std::string> dirs;
    std::string moduleDir = GetModuleDirectoryA(g_moduleHandle);
    std::string stagedRuntimeDir;
    if (ReadMutedVoiceRuntimeDir(moduleDir, stagedRuntimeDir)) {
        AddRuntimeCandidateDir(dirs, stagedRuntimeDir);
    }

    AddRuntimeCandidateDir(dirs, moduleDir);
    AddRuntimeCandidateDir(dirs, GetModuleDirectoryA(nullptr));

    char cwd[MAX_PATH] = {};
    DWORD cwdLength = GetCurrentDirectoryA(MAX_PATH, cwd);
    if (cwdLength > 0 && cwdLength < MAX_PATH) AddRuntimeCandidateDir(dirs, cwd);
    return dirs;
}


std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) return L"";
    int required = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    if (required <= 1) return L"";

    std::wstring result;
    result.resize((size_t)required);
    int written = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, &result[0], required);
    if (written <= 1) return L"";
    result.resize((size_t)written - 1);
    return result;
}

std::wstring UrlEncodePathSegment(const std::string& value) {
    static const wchar_t kHex[] = L"0123456789ABCDEF";
    std::wstring result;
    for (unsigned char ch : value) {
        bool safe = (ch >= 'A' && ch <= 'Z') ||
            (ch >= 'a' && ch <= 'z') ||
            (ch >= '0' && ch <= '9') ||
            ch == '_' || ch == '-' || ch == '.';
        if (safe) {
            result.push_back((wchar_t)ch);
        }
        else {
            result.push_back(L'%');
            result.push_back(kHex[(ch >> 4) & 0x0F]);
            result.push_back(kHex[ch & 0x0F]);
        }
    }
    return result;
}

bool FindJsonKeyValueStart(const std::string& json, const char* key, size_t start, size_t end, size_t& valueStart) {
    std::string quotedKey = "\"";
    quotedKey += key;
    quotedKey += "\"";
    if (end == std::string::npos || end > json.size()) end = json.size();

    size_t keyPos = start;
    while ((keyPos = json.find(quotedKey, keyPos)) != std::string::npos && keyPos < end) {
        size_t colon = json.find(':', keyPos + quotedKey.size());
        if (colon == std::string::npos || colon >= end) return false;
        valueStart = colon + 1;
        while (valueStart < end && (json[valueStart] == ' ' || json[valueStart] == '\t' || json[valueStart] == '\r' || json[valueStart] == '\n')) {
            ++valueStart;
        }
        return valueStart < end;
    }
    return false;
}

bool FindJsonObjectRange(const std::string& json, const char* key, size_t start, size_t end, size_t& objStart, size_t& objEnd) {
    size_t valueStart = 0;
    if (!FindJsonKeyValueStart(json, key, start, end, valueStart)) return false;
    if (valueStart >= json.size() || json[valueStart] != '{') return false;

    bool inString = false;
    bool escaped = false;
    int depth = 0;
    for (size_t i = valueStart; i < json.size(); ++i) {
        char ch = json[i];
        if (inString) {
            if (escaped) escaped = false;
            else if (ch == '\\') escaped = true;
            else if (ch == '"') inString = false;
            continue;
        }

        if (ch == '"') inString = true;
        else if (ch == '{') ++depth;
        else if (ch == '}') {
            --depth;
            if (depth == 0) {
                objStart = valueStart;
                objEnd = i + 1;
                return true;
            }
        }
    }

    return false;
}

bool JsonBoolInRange(const std::string& json, size_t start, size_t end, const char* key, bool& value) {
    size_t valueStart = 0;
    if (!FindJsonKeyValueStart(json, key, start, end, valueStart)) return false;
    if (valueStart + 4 <= json.size() && json.compare(valueStart, 4, "true") == 0) {
        value = true;
        return true;
    }
    if (valueStart + 5 <= json.size() && json.compare(valueStart, 5, "false") == 0) {
        value = false;
        return true;
    }
    return false;
}

bool JsonStringInRange(const std::string& json, size_t start, size_t end, const char* key, std::string& value) {
    size_t valueStart = 0;
    if (!FindJsonKeyValueStart(json, key, start, end, valueStart)) return false;
    if (valueStart >= end || valueStart >= json.size() || json[valueStart] != '"') return false;

    std::string result;
    bool escaped = false;
    for (size_t i = valueStart + 1; i < json.size() && i < end; ++i) {
        char ch = json[i];
        if (escaped) {
            switch (ch) {
            case 'n': result.push_back('\n'); break;
            case 'r': result.push_back('\r'); break;
            case 't': result.push_back('\t'); break;
            case 'b': result.push_back('\b'); break;
            case 'f': result.push_back('\f'); break;
            case '"': result.push_back('"'); break;
            case '\\': result.push_back('\\'); break;
            case '/': result.push_back('/'); break;
            default: result.push_back(ch); break;
            }
            escaped = false;
            continue;
        }
        if (ch == '\\') {
            escaped = true;
            continue;
        }
        if (ch == '"') {
            value = result;
            return true;
        }
        result.push_back(ch);
    }

    return false;
}

bool JsonUnsignedLongLongInRange(const std::string& json, size_t start, size_t end, const char* key, unsigned long long& value) {
    size_t valueStart = 0;
    if (!FindJsonKeyValueStart(json, key, start, end, valueStart)) return false;
    if (valueStart >= json.size() || valueStart >= end || json[valueStart] < '0' || json[valueStart] > '9') return false;

    unsigned long long result = 0;
    size_t i = valueStart;
    while (i < json.size() && i < end && json[i] >= '0' && json[i] <= '9') {
        unsigned int digit = (unsigned int)(json[i] - '0');
        if (result > (ULLONG_MAX - digit) / 10ULL) return false;
        result = result * 10ULL + digit;
        ++i;
    }
    value = result;
    return true;
}

bool WinHttpReadAll(HINTERNET request, std::string& response) {
    response.clear();

    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request, &available)) return false;
        if (available == 0) return true;

        std::vector<char> buffer(available);
        DWORD read = 0;
        if (!WinHttpReadData(request, buffer.data(), available, &read)) return false;
        if (read == 0) return true;
        response.append(buffer.data(), buffer.data() + read);
        if (response.size() > 1024 * 1024) return false;
    }
}


// =============================================================
// Public Helpers - Hypixel TNT Tag wins
// =============================================================
enum PublicWinsFetchResult {
    PUBLIC_WINS_FETCH_TRANSIENT = 0,
    PUBLIC_WINS_FETCH_AVAILABLE,
    PUBLIC_WINS_FETCH_UNAVAILABLE,
    PUBLIC_WINS_FETCH_FORBIDDEN
};

struct PublicWinsCacheEntry {
    bool fetching = false;
    bool hasDefinitiveResult = false;
    bool available = false;
    unsigned long long wins = 0;
    unsigned long long fetchedEpochMs = 0;
    ULONGLONG nextFetchTickMs = 0;
    std::string apiDisplayName;
};

struct PublicWinsLookupRequest {
    std::string uuid;
    std::string playerName;
};

std::mutex g_publicWinsMutex;
std::unordered_map<std::string, PublicWinsCacheEntry> g_publicWinsCache;
std::deque<PublicWinsLookupRequest> g_publicWinsQueue;
std::unordered_set<std::string> g_publicWinsQueuedUuids;
HANDLE g_publicWinsThreadHandle = nullptr;
HANDLE g_publicWinsStopEvent = nullptr;
HANDLE g_publicWinsQueueEvent = nullptr;
volatile LONG g_publicWinsWorkerRunning = 0;
volatile LONG g_publicWinsRuntimeEnabled = 0;
volatile LONG g_publicWinsForbidden = 0;
volatile LONG g_publicWinsMissingConfigLogged = 0;
volatile LONG g_publicWinsPrefetchLogged = 0;
volatile LONG g_publicWinsTabDispatchCount = 0;
volatile LONG g_publicWinsApiTabDispatchCount = 0;
volatile LONG g_publicWinsNameTagDispatchCount = 0;
volatile LONG g_publicWinsScoreFormatDispatchCount = 0;
volatile LONG g_publicWinsRenderedNameDispatchCount = 0;
volatile LONG g_lunarNametagTimerDispatchCount = 0;
volatile LONG g_lunarAdventureNametagDispatchCount = 0;
volatile LONG g_publicWinsDecorationCount = 0;
volatile LONG g_publicWinsFetchLogCount = 0;
std::string g_publicWinsServerUrl;
std::string g_publicWinsClientToken;
std::string g_publicWinsCachePath;
bool g_publicWinsCacheLoaded = false;

unsigned long long GetUnixEpochMilliseconds() {
    FILETIME fileTime = {};
    GetSystemTimeAsFileTime(&fileTime);
    ULARGE_INTEGER value = {};
    value.LowPart = fileTime.dwLowDateTime;
    value.HighPart = fileTime.dwHighDateTime;
    constexpr unsigned long long kWindowsToUnixEpoch100Ns = 116444736000000000ULL;
    if (value.QuadPart <= kWindowsToUnixEpoch100Ns) return 0;
    return (value.QuadPart - kWindowsToUnixEpoch100Ns) / 10000ULL;
}

std::string NormalizePublicWinsUuid(const std::string& uuid) {
    std::string normalized;
    normalized.reserve(32);
    for (char ch : TrimAscii(uuid)) {
        if (ch == '-') continue;
        if (!IsHexAscii(ch)) return "";
        normalized.push_back((ch >= 'A' && ch <= 'F') ? (char)(ch - 'A' + 'a') : ch);
    }
    return normalized.size() == 32 ? normalized : "";
}

bool IsSafeMinecraftUsername(const std::string& name) {
    if (name.empty() || name.size() > 16) return false;
    for (char ch : name) {
        if (!((ch >= 'A' && ch <= 'Z') ||
            (ch >= 'a' && ch <= 'z') ||
            (ch >= '0' && ch <= '9') || ch == '_')) return false;
    }
    return true;
}

bool LoadPublicWinsServerConfig(std::string& serverUrl, std::string& clientToken, std::string& runtimeDir) {
    serverUrl.clear();
    clientToken.clear();
    runtimeDir.clear();
    std::vector<std::string> dirs = GetRuntimeConfigCandidateDirs();
    for (const std::string& dir : dirs) {
        std::ifstream f(JoinPathA(dir, kPublicHelpersServerConfigFile), std::ios::binary);
        if (!f.is_open()) continue;
        std::string urlCandidate;
        std::string tokenCandidate;
        std::getline(f, urlCandidate);
        std::getline(f, tokenCandidate);
        urlCandidate = TrimAscii(urlCandidate);
        tokenCandidate = TrimAscii(tokenCandidate);
        if (urlCandidate.size() > 8 && _strnicmp(urlCandidate.c_str(), "https://", 8) == 0 &&
            urlCandidate.find('\r') == std::string::npos && urlCandidate.find('\n') == std::string::npos &&
            tokenCandidate.size() >= 24 && IsSafeSingleLineConfigValue(tokenCandidate)) {
            while (urlCandidate.size() > 8 && urlCandidate.back() == '/') urlCandidate.pop_back();
            serverUrl = urlCandidate;
            clientToken = tokenCandidate;
            runtimeDir = dir;
            return true;
        }
    }
    if (!dirs.empty()) runtimeDir = dirs.front();
    return false;
}

void LoadPublicWinsCacheIfNeeded(const std::string& runtimeDir) {
    {
        std::lock_guard<std::mutex> lock(g_publicWinsMutex);
        if (g_publicWinsCacheLoaded) return;
        g_publicWinsCacheLoaded = true;
        g_publicWinsCachePath = JoinPathA(runtimeDir, kHypixelWinsCacheFile);
    }

    std::ifstream f(JoinPathA(runtimeDir, kHypixelWinsCacheFile), std::ios::binary);
    if (!f.is_open()) return;

    std::string header;
    std::getline(f, header);
    if (TrimAscii(header) != "public_wins_cache_version 1") return;

    std::unordered_map<std::string, PublicWinsCacheEntry> loaded;
    std::string uuid;
    unsigned long long fetchedEpochMs = 0;
    int available = 0;
    unsigned long long wins = 0;
    std::string displayName;
    unsigned long long nowEpochMs = GetUnixEpochMilliseconds();
    while (f >> uuid >> fetchedEpochMs >> available >> wins >> displayName) {
        uuid = NormalizePublicWinsUuid(uuid);
        if (uuid.empty() || fetchedEpochMs == 0 || fetchedEpochMs > nowEpochMs + 5ULL * 60ULL * 1000ULL) continue;
        if (displayName == "-") displayName.clear();
        if (available != 0 && !IsSafeMinecraftUsername(displayName)) continue;

        PublicWinsCacheEntry entry;
        entry.hasDefinitiveResult = true;
        entry.available = available != 0;
        entry.wins = wins;
        entry.fetchedEpochMs = fetchedEpochMs;
        entry.apiDisplayName = displayName;
        loaded[uuid] = entry;
    }

    std::lock_guard<std::mutex> lock(g_publicWinsMutex);
    for (const auto& item : loaded) g_publicWinsCache[item.first] = item.second;
}

void SavePublicWinsCache() {
    std::string cachePath;
    std::unordered_map<std::string, PublicWinsCacheEntry> snapshot;
    {
        std::lock_guard<std::mutex> lock(g_publicWinsMutex);
        cachePath = g_publicWinsCachePath;
        snapshot = g_publicWinsCache;
    }
    if (cachePath.empty()) return;

    std::string tempPath = cachePath + ".tmp";
    std::ofstream f(tempPath, std::ios::binary | std::ios::trunc);
    if (!f.is_open()) return;
    f << "public_wins_cache_version 1\n";
    for (const auto& item : snapshot) {
        const PublicWinsCacheEntry& entry = item.second;
        if (!entry.hasDefinitiveResult || entry.fetchedEpochMs == 0) continue;
        f << item.first << " " << entry.fetchedEpochMs << " "
            << (entry.available ? 1 : 0) << " " << entry.wins << " "
            << (entry.apiDisplayName.empty() ? "-" : entry.apiDisplayName) << "\n";
    }
    f.close();
    if (!f) return;
    MoveFileExA(tempPath.c_str(), cachePath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
}

DWORD QueryPublicWinsRetryDelayMs(HINTERNET request) {
    wchar_t value[64] = {};
    DWORD valueBytes = sizeof(value);
    if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM, L"RateLimit-Reset", value, &valueBytes, WINHTTP_NO_HEADER_INDEX)) {
        valueBytes = sizeof(value);
        if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM, L"Retry-After", value, &valueBytes, WINHTTP_NO_HEADER_INDEX)) {
            return (DWORD)kHypixelWinsFailureRetryMs;
        }
    }
    unsigned long seconds = wcstoul(value, nullptr, 10);
    if (seconds == 0) return (DWORD)kHypixelWinsFailureRetryMs;
    unsigned long long delayMs = (unsigned long long)seconds * 1000ULL;
    if (delayMs > 24ULL * 60ULL * 60ULL * 1000ULL) delayMs = 24ULL * 60ULL * 60ULL * 1000ULL;
    return (DWORD)delayMs;
}

struct PublicWinsServerEndpoint {
    std::wstring host;
    std::wstring basePath;
    INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
};

bool ParsePublicWinsServerEndpoint(const std::string& serverUrl, PublicWinsServerEndpoint& endpoint) {
    endpoint = PublicWinsServerEndpoint{};
    std::wstring wideUrl = Utf8ToWide(serverUrl);
    if (wideUrl.empty()) return false;

    URL_COMPONENTS components = {};
    components.dwStructSize = sizeof(components);
    components.dwSchemeLength = (DWORD)-1;
    components.dwHostNameLength = (DWORD)-1;
    components.dwUrlPathLength = (DWORD)-1;
    components.dwExtraInfoLength = (DWORD)-1;
    if (!WinHttpCrackUrl(wideUrl.c_str(), 0, 0, &components) ||
        components.nScheme != INTERNET_SCHEME_HTTPS || !components.lpszHostName ||
        components.dwHostNameLength == 0 || components.dwExtraInfoLength != 0) return false;

    endpoint.host.assign(components.lpszHostName, components.dwHostNameLength);
    if (components.lpszUrlPath && components.dwUrlPathLength > 0) {
        endpoint.basePath.assign(components.lpszUrlPath, components.dwUrlPathLength);
        while (endpoint.basePath.size() > 1 && endpoint.basePath.back() == L'/') endpoint.basePath.pop_back();
        if (endpoint.basePath == L"/") endpoint.basePath.clear();
    }
    endpoint.port = components.nPort != 0 ? components.nPort : INTERNET_DEFAULT_HTTPS_PORT;
    return !endpoint.host.empty();
}

PublicWinsFetchResult FetchPublicWins(
    const PublicWinsLookupRequest& lookup,
    const std::string& serverUrl,
    const std::string& clientToken,
    unsigned long long& wins,
    std::string& apiDisplayName,
    unsigned long long& fetchedEpochMs,
    DWORD& retryDelayMs,
    DWORD& statusCode,
    DWORD& lastError) {
    wins = 0;
    apiDisplayName.clear();
    fetchedEpochMs = 0;
    retryDelayMs = (DWORD)kHypixelWinsFailureRetryMs;
    statusCode = 0;
    lastError = ERROR_SUCCESS;

    PublicWinsServerEndpoint endpoint;
    if (!ParsePublicWinsServerEndpoint(serverUrl, endpoint) || clientToken.empty()) {
        lastError = ERROR_INVALID_PARAMETER;
        return PUBLIC_WINS_FETCH_FORBIDDEN;
    }

    HINTERNET session = WinHttpOpen(L"TagEssentials Public Helpers/2.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        lastError = GetLastError();
        return PUBLIC_WINS_FETCH_TRANSIENT;
    }
    WinHttpSetTimeouts(session, 5000, 5000, 5000, 8000);
    DWORD secureProtocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
    WinHttpSetOption(session, WINHTTP_OPTION_SECURE_PROTOCOLS, &secureProtocols, sizeof(secureProtocols));

    HINTERNET connect = WinHttpConnect(session, endpoint.host.c_str(), endpoint.port, 0);
    if (!connect) {
        lastError = GetLastError();
        WinHttpCloseHandle(session);
        return PUBLIC_WINS_FETCH_TRANSIENT;
    }

    std::wstring path = endpoint.basePath + L"/v1/tnttag/wins?uuid=" + UrlEncodePathSegment(lookup.uuid);
    path += L"&name=" + UrlEncodePathSegment(lookup.playerName);
    HINTERNET request = WinHttpOpenRequest(connect, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!request) {
        lastError = GetLastError();
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return PUBLIC_WINS_FETCH_TRANSIENT;
    }

    std::wstring headers = L"Accept: application/json\r\nX-Public-Helpers-Token: " + Utf8ToWide(clientToken) + L"\r\n";
    bool sent = WinHttpAddRequestHeaders(request, headers.c_str(), (DWORD)-1L, WINHTTP_ADDREQ_FLAG_ADD) &&
        WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(request, nullptr);
    std::string response;
    if (!sent) {
        lastError = GetLastError();
    }
    else {
        DWORD statusSize = sizeof(statusCode);
        if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX)) {
            lastError = GetLastError();
        }
        else if (statusCode == 429 || statusCode == 503) {
            retryDelayMs = QueryPublicWinsRetryDelayMs(request);
        }
        else {
            WinHttpReadAll(request, response);
        }
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);

    if (!sent || statusCode == 0 || statusCode == 429 || statusCode >= 500) return PUBLIC_WINS_FETCH_TRANSIENT;
    if (statusCode == 401 || statusCode == 403) return PUBLIC_WINS_FETCH_FORBIDDEN;
    if (statusCode < 200 || statusCode >= 300) return PUBLIC_WINS_FETCH_UNAVAILABLE;

    bool ok = false;
    bool available = false;
    if (!JsonBoolInRange(response, 0, response.size(), "ok", ok) || !ok ||
        !JsonBoolInRange(response, 0, response.size(), "available", available) ||
        !JsonUnsignedLongLongInRange(response, 0, response.size(), "fetched_at", fetchedEpochMs)) {
        return PUBLIC_WINS_FETCH_TRANSIENT;
    }
    unsigned long long retrySeconds = 0;
    if (JsonUnsignedLongLongInRange(response, 0, response.size(), "retry_after_seconds", retrySeconds) && retrySeconds > 0) {
        unsigned long long delayMs = retrySeconds * 1000ULL;
        retryDelayMs = (DWORD)std::min<unsigned long long>(delayMs, 24ULL * 60ULL * 60ULL * 1000ULL);
    }
    else {
        retryDelayMs = (DWORD)kHypixelWinsCacheTtlMs;
    }
    if (!available) return PUBLIC_WINS_FETCH_UNAVAILABLE;
    if (!JsonUnsignedLongLongInRange(response, 0, response.size(), "wins", wins)) return PUBLIC_WINS_FETCH_TRANSIENT;
    apiDisplayName = lookup.playerName;
    return PUBLIC_WINS_FETCH_AVAILABLE;
}

bool PopPublicWinsRequest(PublicWinsLookupRequest& request) {
    std::lock_guard<std::mutex> lock(g_publicWinsMutex);
    if (g_publicWinsQueue.empty()) return false;
    request = g_publicWinsQueue.front();
    g_publicWinsQueue.pop_front();
    g_publicWinsQueuedUuids.erase(request.uuid);
    if (g_publicWinsQueue.empty() && g_publicWinsQueueEvent) ResetEvent(g_publicWinsQueueEvent);
    return true;
}

DWORD WINAPI PublicWinsWorkerThread(LPVOID) {
    InterlockedExchange(&g_publicWinsWorkerRunning, 1);
    while (g_publicWinsStopEvent && WaitForSingleObject(g_publicWinsStopEvent, 0) == WAIT_TIMEOUT) {
        PublicWinsLookupRequest lookup;
        if (!PopPublicWinsRequest(lookup)) {
            if (g_publicWinsQueueEvent) WaitForSingleObject(g_publicWinsQueueEvent, 1000);
            continue;
        }

        std::string serverUrl;
        std::string clientToken;
        {
            std::lock_guard<std::mutex> lock(g_publicWinsMutex);
            serverUrl = g_publicWinsServerUrl;
            clientToken = g_publicWinsClientToken;
        }
        unsigned long long wins = 0;
        std::string apiDisplayName;
        unsigned long long fetchedEpochMs = 0;
        DWORD retryDelayMs = (DWORD)kHypixelWinsFailureRetryMs;
        DWORD statusCode = 0;
        DWORD lastError = ERROR_SUCCESS;
        PublicWinsFetchResult result = FetchPublicWins(lookup, serverUrl, clientToken, wins, apiDisplayName,
            fetchedEpochMs, retryDelayMs, statusCode, lastError);

        ULONGLONG nowTickMs = GetTickCount64();
        bool persist = false;
        {
            std::lock_guard<std::mutex> lock(g_publicWinsMutex);
            PublicWinsCacheEntry& entry = g_publicWinsCache[lookup.uuid];
            entry.fetching = false;
            if (result == PUBLIC_WINS_FETCH_AVAILABLE || result == PUBLIC_WINS_FETCH_UNAVAILABLE) {
                entry.hasDefinitiveResult = true;
                entry.available = result == PUBLIC_WINS_FETCH_AVAILABLE;
                entry.wins = wins;
                entry.apiDisplayName = apiDisplayName;
                entry.fetchedEpochMs = fetchedEpochMs != 0 ? fetchedEpochMs : GetUnixEpochMilliseconds();
                entry.nextFetchTickMs = nowTickMs + retryDelayMs;
                persist = true;
            }
            else {
                entry.nextFetchTickMs = nowTickMs + retryDelayMs;
            }
        }
        if (result == PUBLIC_WINS_FETCH_FORBIDDEN) InterlockedExchange(&g_publicWinsForbidden, 1);
        if (persist) SavePublicWinsCache();
        if ((result == PUBLIC_WINS_FETCH_AVAILABLE || result == PUBLIC_WINS_FETCH_UNAVAILABLE) &&
            InterlockedIncrement(&g_publicWinsFetchLogCount) <= 5) {
            DebugLog("Public wins fetch result player=%s available=%d wins=%llu status=%lu",
                lookup.playerName.c_str(),
                result == PUBLIC_WINS_FETCH_AVAILABLE ? 1 : 0,
                wins,
                (unsigned long)statusCode);
        }
        if (result != PUBLIC_WINS_FETCH_AVAILABLE && result != PUBLIC_WINS_FETCH_UNAVAILABLE) {
            DebugLog("Public wins fetch failed uuid=%s status=%lu error=%lu%s",
                lookup.uuid.c_str(), (unsigned long)statusCode, (unsigned long)lastError,
                result == PUBLIC_WINS_FETCH_FORBIDDEN ? " forbidden" : "");
        }
    }
    InterlockedExchange(&g_publicWinsWorkerRunning, 0);
    return 0;
}

void StartPublicWinsWorker() {
    std::string serverUrl;
    std::string clientToken;
    std::string runtimeDir;
    if (!LoadPublicWinsServerConfig(serverUrl, clientToken, runtimeDir)) {
        if (InterlockedExchange(&g_publicWinsMissingConfigLogged, 1) == 0) {
            DebugLog("Public Helpers wins unavailable: missing or invalid public_helpers_server.txt");
        }
        return;
    }
    LoadPublicWinsCacheIfNeeded(runtimeDir);
    DebugLog("Public wins config loaded runtimeDir=%s cachePath=%s",
        runtimeDir.c_str(), g_publicWinsCachePath.c_str());

    std::lock_guard<std::mutex> lock(g_publicWinsMutex);
    g_publicWinsServerUrl = serverUrl;
    g_publicWinsClientToken = clientToken;
    if (g_publicWinsThreadHandle && WaitForSingleObject(g_publicWinsThreadHandle, 0) == WAIT_TIMEOUT) return;
    if (g_publicWinsThreadHandle) CloseHandle(g_publicWinsThreadHandle);
    if (g_publicWinsStopEvent) CloseHandle(g_publicWinsStopEvent);
    if (g_publicWinsQueueEvent) CloseHandle(g_publicWinsQueueEvent);
    g_publicWinsThreadHandle = nullptr;
    g_publicWinsStopEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    g_publicWinsQueueEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    if (!g_publicWinsStopEvent || !g_publicWinsQueueEvent) {
        if (g_publicWinsStopEvent) CloseHandle(g_publicWinsStopEvent);
        if (g_publicWinsQueueEvent) CloseHandle(g_publicWinsQueueEvent);
        g_publicWinsStopEvent = nullptr;
        g_publicWinsQueueEvent = nullptr;
        DebugLog("Public wins worker failed to create events");
        return;
    }

    DWORD threadId = 0;
    g_publicWinsThreadHandle = CreateThread(nullptr, 0, PublicWinsWorkerThread, nullptr, 0, &threadId);
    if (!g_publicWinsThreadHandle) {
        CloseHandle(g_publicWinsStopEvent);
        CloseHandle(g_publicWinsQueueEvent);
        g_publicWinsStopEvent = nullptr;
        g_publicWinsQueueEvent = nullptr;
    }
}

void StopPublicWinsWorker() {
    HANDLE threadHandle = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_publicWinsMutex);
        threadHandle = g_publicWinsThreadHandle;
        if (g_publicWinsStopEvent) SetEvent(g_publicWinsStopEvent);
        if (g_publicWinsQueueEvent) SetEvent(g_publicWinsQueueEvent);
    }
    if (threadHandle && WaitForSingleObject(threadHandle, 5000) == WAIT_TIMEOUT) {
        DebugLog("Public wins worker did not stop within timeout");
        return;
    }

    std::lock_guard<std::mutex> lock(g_publicWinsMutex);
    if (g_publicWinsThreadHandle) CloseHandle(g_publicWinsThreadHandle);
    if (g_publicWinsStopEvent) CloseHandle(g_publicWinsStopEvent);
    if (g_publicWinsQueueEvent) CloseHandle(g_publicWinsQueueEvent);
    g_publicWinsThreadHandle = nullptr;
    g_publicWinsStopEvent = nullptr;
    g_publicWinsQueueEvent = nullptr;
    g_publicWinsQueue.clear();
    g_publicWinsQueuedUuids.clear();
    for (auto& item : g_publicWinsCache) item.second.fetching = false;
}

void SetPublicWinsEnabled(bool enabled) {
    g_guiPublicWinsEnabled = enabled;
    InterlockedExchange(&g_publicWinsRuntimeEnabled, enabled ? 1 : 0);
    if (enabled) {
        InterlockedExchange(&g_publicWinsForbidden, 0);
        InterlockedExchange(&g_publicWinsMissingConfigLogged, 0);
        StartPublicWinsWorker();
    }
    else {
        StopPublicWinsWorker();
    }
}

void QueuePublicWinsLookup(const std::string& uuidValue, const std::string& playerName) {
    if (InterlockedCompareExchange(&g_publicWinsRuntimeEnabled, 0, 0) == 0 ||
        InterlockedCompareExchange(&g_hypixelTntTagGameActive, 0, 0) == 0 ||
        InterlockedCompareExchange(&g_publicWinsWorkerRunning, 0, 0) == 0 ||
        InterlockedCompareExchange(&g_publicWinsForbidden, 0, 0) != 0 ||
        !IsSafeMinecraftUsername(playerName)) return;

    std::string uuid = NormalizePublicWinsUuid(uuidValue);
    if (uuid.empty()) return;
    ULONGLONG nowTickMs = GetTickCount64();
    unsigned long long nowEpochMs = GetUnixEpochMilliseconds();
    HANDLE queueEvent = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_publicWinsMutex);
        PublicWinsCacheEntry& entry = g_publicWinsCache[uuid];
        bool fresh = entry.hasDefinitiveResult && nowEpochMs >= entry.fetchedEpochMs &&
            nowEpochMs - entry.fetchedEpochMs < kHypixelWinsCacheTtlMs;
        if (entry.fetching || fresh || nowTickMs < entry.nextFetchTickMs) return;
        entry.fetching = true;
        entry.nextFetchTickMs = nowTickMs + 30000ULL;
        if (g_publicWinsQueuedUuids.insert(uuid).second) g_publicWinsQueue.push_back({ uuid, playerName });
        queueEvent = g_publicWinsQueueEvent;
    }
    if (queueEvent) SetEvent(queueEvent);
}

bool GetPublicWinsCached(const std::string& uuidValue, const std::string& playerName, unsigned long long& wins) {
    wins = 0;
    if (InterlockedCompareExchange(&g_publicWinsRuntimeEnabled, 0, 0) == 0 ||
        InterlockedCompareExchange(&g_hypixelTntTagGameActive, 0, 0) == 0) return false;
    std::string uuid = NormalizePublicWinsUuid(uuidValue);
    if (uuid.empty()) return false;

    std::lock_guard<std::mutex> lock(g_publicWinsMutex);
    auto it = g_publicWinsCache.find(uuid);
    if (it == g_publicWinsCache.end() || !it->second.hasDefinitiveResult || !it->second.available ||
        _stricmp(it->second.apiDisplayName.c_str(), playerName.c_str()) != 0) return false;
    wins = it->second.wins;
    return true;
}

bool GetPublicWinsCachedByPlayerName(const std::string& playerName, unsigned long long& wins) {
    wins = 0;
    if (playerName.empty() ||
        InterlockedCompareExchange(&g_publicWinsRuntimeEnabled, 0, 0) == 0 ||
        InterlockedCompareExchange(&g_hypixelTntTagGameActive, 0, 0) == 0) return false;

    std::lock_guard<std::mutex> lock(g_publicWinsMutex);
    const PublicWinsCacheEntry* newest = nullptr;
    for (const auto& item : g_publicWinsCache) {
        const PublicWinsCacheEntry& entry = item.second;
        if (!entry.hasDefinitiveResult || !entry.available ||
            _stricmp(entry.apiDisplayName.c_str(), playerName.c_str()) != 0 ||
            (newest && newest->fetchedEpochMs >= entry.fetchedEpochMs)) continue;
        newest = &entry;
    }
    if (!newest) return false;
    wins = newest->wins;
    return true;
}

bool ResolveNodeExecutable(std::string& nodePath) {
    char found[MAX_PATH] = {};
    DWORD length = SearchPathA(nullptr, "node.exe", nullptr, MAX_PATH, found, nullptr);
    if (length == 0 || length >= MAX_PATH) return false;
    nodePath = found;
    return true;
}

bool EnsureMutedVoiceWinsock() {
    static std::mutex s_winsockMutex;
    static bool s_winsockInitialized = false;
    static bool s_winsockFailed = false;

    std::lock_guard<std::mutex> lock(s_winsockMutex);
    if (s_winsockInitialized) return true;
    if (s_winsockFailed) return false;

    WSADATA data = {};
    int result = WSAStartup(MAKEWORD(2, 2), &data);
    if (result != 0) {
        s_winsockFailed = true;
        DebugLog("Muted Voice WSAStartup failed err=%d", result);
        return false;
    }

    s_winsockInitialized = true;
    return true;
}

SOCKET ConnectMutedVoiceControl(DWORD timeoutMs, HANDLE stopEvent) {
    if (!EnsureMutedVoiceWinsock()) return INVALID_SOCKET;

    ULONGLONG deadline = GetTickCount64() + timeoutMs;
    do {
        if (stopEvent && WaitForSingleObject(stopEvent, 0) == WAIT_OBJECT_0) return INVALID_SOCKET;

        SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) return INVALID_SOCKET;

        sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(kMutedVoiceControlPort);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

        if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0) {
            return sock;
        }

        closesocket(sock);
        Sleep(100);
    } while (GetTickCount64() < deadline);

    return INVALID_SOCKET;
}

bool SendMutedVoiceLine(SOCKET sock, const std::string& line) {
    if (sock == INVALID_SOCKET) return false;

    std::string payload = line;
    payload.push_back('\n');

    const char* data = payload.c_str();
    int remaining = (int)payload.size();
    while (remaining > 0) {
        int sent = send(sock, data, remaining, 0);
        if (sent == SOCKET_ERROR || sent == 0) return false;
        data += sent;
        remaining -= sent;
    }

    return true;
}

std::string JsonEscape(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size() + 8);
    for (unsigned char ch : value) {
        switch (ch) {
        case '\\': escaped += "\\\\"; break;
        case '"': escaped += "\\\""; break;
        case '\b': escaped += "\\b"; break;
        case '\f': escaped += "\\f"; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        case '\t': escaped += "\\t"; break;
        default:
            if (ch < 0x20) {
                char buf[8] = {};
                sprintf_s(buf, "\\u%04X", (unsigned int)ch);
                escaped += buf;
            }
            else {
                escaped.push_back((char)ch);
            }
            break;
        }
    }
    return escaped;
}

std::string MakeMutedVoiceDefaultConfigJson(const std::string& partyOwner) {
    std::string json;
    json += "{\n";
    json += "  \"username\": \"\",\n";
    json += "  \"auth\": \"microsoft\",\n";
    json += "  \"flow\": \"live\",\n";
    json += "  \"host\": \"mc.hypixel.net\",\n";
    json += "  \"partyOwnerUsername\": \"";
    json += JsonEscape(partyOwner);
    json += "\",\n";
    json += "  \"minecraftPort\": 25565,\n";
    json += "  \"version\": \"1.8.9\",\n";
    json += "  \"chatDelayMs\": 900,\n";
    json += "  \"partyAcceptDelayMs\": 250,\n";
    json += "  \"maxChatQueue\": 32\n";
    json += "}\n";
    return json;
}

bool ReplaceJsonStringField(std::string& json, const char* key, const std::string& value) {
    if (!key || !*key) return false;

    size_t valueStart = 0;
    std::string replacement = "\"";
    replacement += JsonEscape(value);
    replacement += "\"";

    if (FindJsonKeyValueStart(json, key, 0, json.size(), valueStart) &&
        valueStart < json.size() &&
        json[valueStart] == '"') {
        bool escaped = false;
        for (size_t i = valueStart + 1; i < json.size(); ++i) {
            char ch = json[i];
            if (escaped) {
                escaped = false;
                continue;
            }
            if (ch == '\\') {
                escaped = true;
                continue;
            }
            if (ch == '"') {
                json.replace(valueStart, (i + 1) - valueStart, replacement);
                return true;
            }
        }
    }

    size_t objectStart = json.find('{');
    if (objectStart == std::string::npos) return false;

    std::string entry = "\n  \"";
    entry += key;
    entry += "\": ";
    entry += replacement;
    entry += ",";
    json.insert(objectStart + 1, entry);
    return true;
}

bool WriteMutedVoicePartyOwnerConfig(const std::string& partyOwner) {
    std::string workingDir;
    std::string scriptPath;
    if (!ResolveMutedVoiceRuntime(workingDir, scriptPath)) return false;

    std::string configPath = JoinPathA(workingDir, "mutedVoiceBot.config.json");
    std::string json;
    if (FileExistsA(configPath)) {
        std::ifstream configFile(configPath, std::ios::binary);
        if (configFile.is_open()) {
            json.assign(std::istreambuf_iterator<char>(configFile), std::istreambuf_iterator<char>());
        }
    }

    if (json.empty()) {
        json = MakeMutedVoiceDefaultConfigJson(partyOwner);
    }
    else if (!ReplaceJsonStringField(json, "partyOwnerUsername", partyOwner)) {
        json = MakeMutedVoiceDefaultConfigJson(partyOwner);
    }

    std::ofstream out(configPath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) return false;
    out << json;
    DebugLog("Muted Voice party owner config saved ownerPresent=%d", partyOwner.empty() ? 0 : 1);
    return true;
}

void NotifyMutedVoicePartyOwner() {
    if (!g_guiExtrasMutedVoice) return;

    std::string command = "{\"cmd\":\"setPartyOwner\",\"partyOwnerUsername\":\"";
    command += JsonEscape(g_guiMutedVoicePartyOwner);
    command += "\"}";
    QueueMutedVoiceControlCommand(command);
}

void SetMutedVoicePartyOwnerFromGui(const std::string& value) {
    std::string normalized = NormalizeMutedVoicePartyOwner(value);
    if (g_guiMutedVoicePartyOwner == normalized) return;

    g_guiMutedVoicePartyOwner = normalized;
    SaveToolSettings();
    WriteMutedVoicePartyOwnerConfig(g_guiMutedVoicePartyOwner);
    NotifyMutedVoicePartyOwner();
}

void QueueMutedVoiceLocalChatJson(const std::string& json) {
    if (json.empty()) return;

    std::lock_guard<std::mutex> lock(g_mutedVoiceLocalChatMutex);
    if (g_mutedVoiceLocalChatJsonQueue.size() >= 64) {
        g_mutedVoiceLocalChatJsonQueue.pop_front();
    }
    g_mutedVoiceLocalChatJsonQueue.push_back(json);
}

std::string MutedVoiceRankColor(const std::string& rank) {
    std::string lower = ToLowerAscii(rank);
    if (lower.find("youtube") != std::string::npos || lower.find("admin") != std::string::npos) return "red";
    if (lower.find("mod") != std::string::npos) return "dark_green";
    if (lower.find("helper") != std::string::npos) return "blue";
    if (lower.find("mvp++") != std::string::npos) return "gold";
    if (lower.find("mvp") != std::string::npos) return "aqua";
    if (lower.find("vip") != std::string::npos) return "green";
    return "aqua";
}

void AppendJsonChatPart(std::string& json, const std::string& text, const char* color, bool& first) {
    if (!first) json += ",";
    first = false;

    json += "{\"text\":\"";
    json += JsonEscape(text);
    json += "\"";
    if (color && color[0] != '\0') {
        json += ",\"color\":\"";
        json += color;
        json += "\"";
    }
    json += "}";
}

std::string BuildMutedVoicePrivateMessageJson(
    bool inbound,
    const std::string& rank,
    const std::string& player,
    const std::string& message) {
    std::string json = "{\"text\":\"\",\"extra\":[";
    bool first = true;

    AppendJsonChatPart(json, inbound ? "From " : "To ", "light_purple", first);
    if (!rank.empty()) {
        std::string rankWithSpace = rank;
        rankWithSpace.push_back(' ');
        std::string rankColor = MutedVoiceRankColor(rank);
        AppendJsonChatPart(json, rankWithSpace, rankColor.c_str(), first);
    }
    AppendJsonChatPart(json, player, inbound ? "yellow" : "aqua", first);
    AppendJsonChatPart(json, ": ", "white", first);
    AppendJsonChatPart(json, message, "gray", first);

    json += "]}";
    return json;
}

std::string BuildMutedVoicePrivateMessageFailureJson(
    const std::string& player,
    const std::string& reason) {
    std::string json = "{\"text\":\"\",\"extra\":[";
    bool first = true;

    AppendJsonChatPart(json, "Message to ", "red", first);
    AppendJsonChatPart(json, player, "yellow", first);
    AppendJsonChatPart(json, " failed", "red", first);
    if (!reason.empty()) {
        AppendJsonChatPart(json, ": ", "red", first);
        AppendJsonChatPart(json, reason, "gray", first);
    }

    json += "]}";
    return json;
}

bool ExtractJsonStringValue(const std::string& json, const char* key, std::string& value) {
    std::string quotedKey = "\"";
    quotedKey += key;
    quotedKey += "\"";

    size_t keyPos = json.find(quotedKey);
    if (keyPos == std::string::npos) return false;

    size_t colon = json.find(':', keyPos + quotedKey.size());
    if (colon == std::string::npos) return false;

    size_t quote = json.find('"', colon + 1);
    if (quote == std::string::npos) return false;

    std::string result;
    bool escaped = false;
    for (size_t i = quote + 1; i < json.size(); ++i) {
        char ch = json[i];
        if (escaped) {
            switch (ch) {
            case 'n': result.push_back('\n'); break;
            case 'r': result.push_back('\r'); break;
            case 't': result.push_back('\t'); break;
            case 'b': result.push_back('\b'); break;
            case 'f': result.push_back('\f'); break;
            default: result.push_back(ch); break;
            }
            escaped = false;
            continue;
        }
        if (ch == '\\') {
            escaped = true;
            continue;
        }
        if (ch == '"') {
            value = result;
            return true;
        }
        result.push_back(ch);
    }

    return false;
}

std::string LeafNameA(const std::string& path) {
    size_t end = path.find_last_not_of("\\/");
    if (end == std::string::npos) return "";

    size_t slash = path.find_last_of("\\/", end);
    if (slash == std::string::npos) return path.substr(0, end + 1);
    return path.substr(slash + 1, end - slash);
}

bool IsRootedPathA(const std::string& path) {
    if (path.size() >= 2 && path[1] == ':') return true;
    return path.size() >= 2 && path[0] == '\\' && path[1] == '\\';
}

bool DirectoryExistsA(const std::string& path) {
    DWORD attrs = GetFileAttributesA(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool IsSafeMutedVoiceAuthCachePath(const std::string& path) {
    std::string leaf = ToLowerAscii(LeafNameA(path));
    return leaf == ".mineflayer-auth";
}

bool DeleteDirectoryTreeUncheckedA(const std::string& path) {
    DWORD attrs = GetFileAttributesA(path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) return true;

    if ((attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        SetFileAttributesA(path.c_str(), FILE_ATTRIBUTE_NORMAL);
        return DeleteFileA(path.c_str()) || GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES;
    }

    std::string pattern = JoinPathA(path, "*");
    WIN32_FIND_DATAA findData = {};
    HANDLE findHandle = FindFirstFileA(pattern.c_str(), &findData);
    if (findHandle != INVALID_HANDLE_VALUE) {
        do {
            const char* name = findData.cFileName;
            if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;

            std::string child = JoinPathA(path, name);
            if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                if (!DeleteDirectoryTreeUncheckedA(child)) {
                    FindClose(findHandle);
                    return false;
                }
            }
            else {
                SetFileAttributesA(child.c_str(), FILE_ATTRIBUTE_NORMAL);
                if (!DeleteFileA(child.c_str()) && GetFileAttributesA(child.c_str()) != INVALID_FILE_ATTRIBUTES) {
                    FindClose(findHandle);
                    return false;
                }
            }
        } while (FindNextFileA(findHandle, &findData));

        FindClose(findHandle);
    }

    SetFileAttributesA(path.c_str(), FILE_ATTRIBUTE_NORMAL);
    return RemoveDirectoryA(path.c_str()) || GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES;
}

bool DeleteMutedVoiceAuthCache(const std::string& path) {
    if (!IsSafeMutedVoiceAuthCachePath(path)) {
        DebugLog("Muted Voice sign out refused unsafe auth cache path=%s", path.c_str());
        return false;
    }

    if (!DirectoryExistsA(path)) return true;
    return DeleteDirectoryTreeUncheckedA(path);
}

bool ResolveMutedVoiceAuthCacheDir(std::string& authDir) {
    std::string workingDir;
    std::string scriptPath;
    if (!ResolveMutedVoiceRuntime(workingDir, scriptPath)) return false;

    std::string configuredDir;
    std::string configPath = JoinPathA(workingDir, "mutedVoiceBot.config.json");
    if (FileExistsA(configPath)) {
        std::ifstream configFile(configPath, std::ios::binary);
        if (configFile.is_open()) {
            std::string json((std::istreambuf_iterator<char>(configFile)), std::istreambuf_iterator<char>());
            ExtractJsonStringValue(json, "profilesFolder", configuredDir);
        }
    }

    if (configuredDir.empty()) {
        authDir = JoinPathA(workingDir, ".mineflayer-auth");
    }
    else {
        authDir = IsRootedPathA(configuredDir) ? configuredDir : JoinPathA(workingDir, configuredDir);
    }

    return !authDir.empty();
}

void HandleMutedVoiceControlLine(const std::string& line) {
    std::string type;
    if (ExtractJsonStringValue(line, "type", type)) {
        if (type == "auth") {
            std::string code;
            std::string url;
            std::string message;
            ExtractJsonStringValue(line, "code", code);
            ExtractJsonStringValue(line, "url", url);
            ExtractJsonStringValue(line, "message", message);
            SetMutedVoiceAuthPrompt(code, url, message);
            return;
        }
        if (type == "authClear") {
            ClearMutedVoiceAuthPrompt();
            return;
        }
        if (type == "privateMessage") {
            std::string direction;
            std::string player;
            std::string message;
            std::string rank;
            ExtractJsonStringValue(line, "direction", direction);
            ExtractJsonStringValue(line, "player", player);
            ExtractJsonStringValue(line, "message", message);
            ExtractJsonStringValue(line, "rank", rank);
            if (direction == "from" && !player.empty() && !message.empty()) {
                QueueMutedVoiceLocalChatJson(BuildMutedVoicePrivateMessageJson(true, rank, player, message));
            }
            else if (direction == "to" && !player.empty() && !message.empty()) {
                QueueMutedVoiceLocalChatJson(BuildMutedVoicePrivateMessageJson(false, rank, player, message));
            }
            else if (direction == "failed" && !player.empty()) {
                QueueMutedVoiceLocalChatJson(BuildMutedVoicePrivateMessageFailureJson(player, message));
            }
            return;
        }
        if (type == "error") {
            std::string detail;
            ExtractJsonStringValue(line, "message", detail);
            SetMutedVoiceStatus(MUTED_VOICE_STATUS_ERROR, detail);
            return;
        }
    }

    std::string status;
    if (ExtractJsonStringValue(line, "status", status)) {
        std::string detail;
        ExtractJsonStringValue(line, "message", detail);
        if (detail.empty()) ExtractJsonStringValue(line, "error", detail);
        SetMutedVoiceStatus(ParseMutedVoiceStatus(status), detail);
        return;
    }
}

bool LaunchMutedVoiceProcess(
    const std::string& nodePath,
    const std::string& scriptPath,
    const std::string& workingDir,
    HANDLE& processHandle,
    DWORD& processId) {
    std::string commandLine = "\"";
    commandLine += nodePath;
    commandLine += "\" \"";
    commandLine += scriptPath;
    commandLine += "\" --control-port ";
    commandLine += std::to_string((unsigned int)kMutedVoiceControlPort);

    std::vector<char> mutableCommand(commandLine.begin(), commandLine.end());
    mutableCommand.push_back('\0');

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};
    BOOL ok = CreateProcessA(
        nullptr,
        mutableCommand.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NO_WINDOW,
        nullptr,
        workingDir.c_str(),
        &si,
        &pi);

    if (!ok) {
        DWORD error = GetLastError();
        DebugLog("Muted Voice CreateProcess failed err=%lu node=%s script=%s cwd=%s",
            error, nodePath.c_str(), scriptPath.c_str(), workingDir.c_str());
        return false;
    }

    CloseHandle(pi.hThread);
    processHandle = pi.hProcess;
    processId = pi.dwProcessId;
    DebugLog("Muted Voice launched pid=%lu script=%s cwd=%s", processId, scriptPath.c_str(), workingDir.c_str());
    return true;
}

void CloseMutedVoiceProcessHandle(HANDLE processHandle, DWORD processId) {
    if (!processHandle) return;

    DWORD waitResult = WaitForSingleObject(processHandle, 4000);
    if (waitResult == WAIT_TIMEOUT) {
        DebugLog("Muted Voice process pid=%lu did not exit after shutdown; terminating", processId);
        TerminateProcess(processHandle, 0);
        WaitForSingleObject(processHandle, 1000);
    }

    CloseHandle(processHandle);
}

DWORD WINAPI MutedVoiceWorkerThread(LPVOID) {
    InterlockedExchange(&g_mutedVoiceWorkerRunning, 1);
    SetMutedVoiceStatus(MUTED_VOICE_STATUS_CONNECTING);

    HANDLE stopEvent = nullptr;
    HANDLE queueEvent = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_mutedVoiceMutex);
        stopEvent = g_mutedVoiceStopEvent;
        queueEvent = g_mutedVoiceQueueEvent;
    }

    bool stoppedByRequest = false;
    bool keepErrorStatus = false;
    HANDLE processHandle = nullptr;
    DWORD processId = 0;
    SOCKET sock = INVALID_SOCKET;

    std::string workingDir;
    std::string scriptPath;
    std::string nodePath;

    if (!ResolveNodeExecutable(nodePath)) {
        SetMutedVoiceStatus(MUTED_VOICE_STATUS_ERROR, "Node.js not found");
        keepErrorStatus = true;
        goto cleanup;
    }

    if (!ResolveMutedVoiceRuntime(workingDir, scriptPath)) {
        SetMutedVoiceStatus(MUTED_VOICE_STATUS_ERROR, "mutedVoiceBot.js not found");
        keepErrorStatus = true;
        goto cleanup;
    }

    sock = ConnectMutedVoiceControl(500, stopEvent);
    if (sock == INVALID_SOCKET) {
        if (!LaunchMutedVoiceProcess(nodePath, scriptPath, workingDir, processHandle, processId)) {
            SetMutedVoiceStatus(MUTED_VOICE_STATUS_ERROR, "failed to launch Node.js");
            keepErrorStatus = true;
            goto cleanup;
        }

        {
            std::lock_guard<std::mutex> lock(g_mutedVoiceMutex);
            g_mutedVoiceProcessHandle = processHandle;
            g_mutedVoiceProcessId = processId;
        }

        sock = ConnectMutedVoiceControl(10000, stopEvent);
    }

    if (sock == INVALID_SOCKET) {
        SetMutedVoiceStatus(MUTED_VOICE_STATUS_ERROR, "bot control server unavailable");
        keepErrorStatus = true;
        goto cleanup;
    }

    SendMutedVoiceLine(sock, "{\"cmd\":\"start\"}");
    SendMutedVoiceLine(sock, "{\"cmd\":\"status\"}");

    {
        char recvBuffer[4096] = {};
        std::string pendingInput;
        ULONGLONG lastStatusRequestMs = 0;

        while (WaitForSingleObject(stopEvent, 0) == WAIT_TIMEOUT) {
            if (processHandle && WaitForSingleObject(processHandle, 0) != WAIT_TIMEOUT) {
                SetMutedVoiceStatus(MUTED_VOICE_STATUS_ERROR, "bot process exited");
                keepErrorStatus = true;
                break;
            }

            std::deque<std::string> pendingControls;
            std::deque<std::string> pendingChats;
            {
                std::lock_guard<std::mutex> lock(g_mutedVoiceMutex);
                pendingControls.swap(g_mutedVoiceControlQueue);
                pendingChats.swap(g_mutedVoiceChatQueue);
            }

            for (const std::string& command : pendingControls) {
                if (!SendMutedVoiceLine(sock, command)) {
                    SetMutedVoiceStatus(MUTED_VOICE_STATUS_ERROR, "bot control send failed");
                    keepErrorStatus = true;
                    goto cleanup;
                }
            }

            for (const std::string& message : pendingChats) {
                std::string command = "{\"cmd\":\"sendChat\",\"message\":\"";
                command += JsonEscape(message);
                command += "\"}";
                if (!SendMutedVoiceLine(sock, command)) {
                    SetMutedVoiceStatus(MUTED_VOICE_STATUS_ERROR, "bot control send failed");
                    keepErrorStatus = true;
                    goto cleanup;
                }
            }

            ULONGLONG nowMs = GetTickCount64();
            if (lastStatusRequestMs == 0 || nowMs - lastStatusRequestMs >= 2000) {
                if (!SendMutedVoiceLine(sock, "{\"cmd\":\"status\"}")) {
                    SetMutedVoiceStatus(MUTED_VOICE_STATUS_ERROR, "bot control send failed");
                    keepErrorStatus = true;
                    goto cleanup;
                }
                lastStatusRequestMs = nowMs;
            }

            fd_set readSet;
            FD_ZERO(&readSet);
            FD_SET(sock, &readSet);
            timeval timeout = {};
            timeout.tv_sec = 0;
            timeout.tv_usec = 100000;

            int selected = select(0, &readSet, nullptr, nullptr, &timeout);
            if (selected == SOCKET_ERROR) {
                SetMutedVoiceStatus(MUTED_VOICE_STATUS_ERROR, "bot control socket failed");
                keepErrorStatus = true;
                break;
            }
            if (selected > 0 && FD_ISSET(sock, &readSet)) {
                int received = recv(sock, recvBuffer, (int)sizeof(recvBuffer), 0);
                if (received <= 0) {
                    SetMutedVoiceStatus(MUTED_VOICE_STATUS_OFFLINE, "bot disconnected");
                    break;
                }

                pendingInput.append(recvBuffer, received);
                size_t newline = std::string::npos;
                while ((newline = pendingInput.find('\n')) != std::string::npos) {
                    std::string line = pendingInput.substr(0, newline);
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    pendingInput.erase(0, newline + 1);
                    if (!line.empty()) HandleMutedVoiceControlLine(line);
                }
            }

            if (queueEvent) ResetEvent(queueEvent);
        }
    }

cleanup:
    stoppedByRequest = stopEvent && WaitForSingleObject(stopEvent, 0) == WAIT_OBJECT_0;

    if (sock != INVALID_SOCKET) {
        if (stoppedByRequest) {
            SendMutedVoiceLine(sock, "{\"cmd\":\"stop\"}");
            SendMutedVoiceLine(sock, "{\"cmd\":\"shutdown\"}");
        }
        shutdown(sock, SD_BOTH);
        closesocket(sock);
        sock = INVALID_SOCKET;
    }

    if (processHandle) {
        CloseMutedVoiceProcessHandle(processHandle, processId);
        std::lock_guard<std::mutex> lock(g_mutedVoiceMutex);
        if (g_mutedVoiceProcessHandle == processHandle) {
            g_mutedVoiceProcessHandle = nullptr;
            g_mutedVoiceProcessId = 0;
        }
    }

    {
        std::lock_guard<std::mutex> lock(g_mutedVoiceMutex);
        g_mutedVoiceControlQueue.clear();
        g_mutedVoiceChatQueue.clear();
    }

    InterlockedExchange(&g_mutedVoiceWorkerRunning, 0);
    if (stoppedByRequest || !keepErrorStatus) SetMutedVoiceStatus(MUTED_VOICE_STATUS_OFFLINE);
    return 0;
}

void StartMutedVoiceWorker() {
    std::string startError;

    std::lock_guard<std::mutex> lock(g_mutedVoiceMutex);

    if (g_mutedVoiceThreadHandle) {
        DWORD waitResult = WaitForSingleObject(g_mutedVoiceThreadHandle, 0);
        if (waitResult == WAIT_TIMEOUT) return;

        CloseHandle(g_mutedVoiceThreadHandle);
        g_mutedVoiceThreadHandle = nullptr;
        if (g_mutedVoiceStopEvent) {
            CloseHandle(g_mutedVoiceStopEvent);
            g_mutedVoiceStopEvent = nullptr;
        }
        if (g_mutedVoiceQueueEvent) {
            CloseHandle(g_mutedVoiceQueueEvent);
            g_mutedVoiceQueueEvent = nullptr;
        }
    }

    g_mutedVoiceStopEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    g_mutedVoiceQueueEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    if (!g_mutedVoiceStopEvent || !g_mutedVoiceQueueEvent) {
        if (g_mutedVoiceStopEvent) CloseHandle(g_mutedVoiceStopEvent);
        if (g_mutedVoiceQueueEvent) CloseHandle(g_mutedVoiceQueueEvent);
        g_mutedVoiceStopEvent = nullptr;
        g_mutedVoiceQueueEvent = nullptr;
        startError = "failed to create control events";
    }

    if (startError.empty()) {
        DWORD threadId = 0;
        g_mutedVoiceThreadHandle = CreateThread(nullptr, 0, MutedVoiceWorkerThread, nullptr, 0, &threadId);
        if (!g_mutedVoiceThreadHandle) {
            CloseHandle(g_mutedVoiceStopEvent);
            CloseHandle(g_mutedVoiceQueueEvent);
            g_mutedVoiceStopEvent = nullptr;
            g_mutedVoiceQueueEvent = nullptr;
            startError = "failed to create worker thread";
        }
    }

    if (!startError.empty()) {
        g_mutedVoiceStatus = MUTED_VOICE_STATUS_ERROR;
        g_mutedVoiceStatusDetail = startError;
        DebugLog("Muted Voice status=error detail=%s", startError.c_str());
        RequestGuiRepaint();
    }
}

void StopMutedVoiceWorker() {
    HANDLE threadHandle = nullptr;
    HANDLE stopEvent = nullptr;

    {
        std::lock_guard<std::mutex> lock(g_mutedVoiceMutex);
        threadHandle = g_mutedVoiceThreadHandle;
        stopEvent = g_mutedVoiceStopEvent;
        if (stopEvent) SetEvent(stopEvent);
    }

    if (threadHandle) {
        DWORD waitResult = WaitForSingleObject(threadHandle, 7000);
        if (waitResult == WAIT_TIMEOUT) {
            DebugLog("Muted Voice worker did not stop within timeout");
            SetMutedVoiceStatus(MUTED_VOICE_STATUS_ERROR, "worker stop timed out");
            return;
        }
    }

    {
        std::lock_guard<std::mutex> lock(g_mutedVoiceMutex);
        if (g_mutedVoiceThreadHandle) {
            CloseHandle(g_mutedVoiceThreadHandle);
            g_mutedVoiceThreadHandle = nullptr;
        }
        if (g_mutedVoiceStopEvent) {
            CloseHandle(g_mutedVoiceStopEvent);
            g_mutedVoiceStopEvent = nullptr;
        }
        if (g_mutedVoiceQueueEvent) {
            CloseHandle(g_mutedVoiceQueueEvent);
            g_mutedVoiceQueueEvent = nullptr;
        }
        g_mutedVoiceControlQueue.clear();
        g_mutedVoiceChatQueue.clear();
    }

    SetMutedVoiceStatus(MUTED_VOICE_STATUS_OFFLINE);
}

void SetMutedVoiceModuleEnabled(bool enabled) {
    g_guiExtrasMutedVoice = enabled;
    if (enabled) {
        WriteMutedVoicePartyOwnerConfig(g_guiMutedVoicePartyOwner);
        StartMutedVoiceWorker();
    }
    else StopMutedVoiceWorker();
}

bool QueueMutedVoiceControlCommand(const std::string& command) {
    if (command.empty()) return false;

    HANDLE queueEvent = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_mutedVoiceMutex);
        if (g_mutedVoiceControlQueue.size() >= 64) {
            g_mutedVoiceControlQueue.pop_front();
        }
        g_mutedVoiceControlQueue.push_back(command);
        queueEvent = g_mutedVoiceQueueEvent;
    }

    if (queueEvent) SetEvent(queueEvent);
    return true;
}

bool QueueMutedVoiceChatMessage(const std::string& message) {
    if (!g_guiExtrasMutedVoice || message.empty()) return false;

    HANDLE queueEvent = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_mutedVoiceMutex);
        if (g_mutedVoiceChatQueue.size() >= 64) {
            DebugLog("Muted Voice chat queue full; dropping message length=%u", (unsigned int)message.size());
            return false;
        }
        g_mutedVoiceChatQueue.push_back(message);
        queueEvent = g_mutedVoiceQueueEvent;
    }

    if (queueEvent) SetEvent(queueEvent);
    DebugLog("Muted Voice queued chat message length=%u", (unsigned int)message.size());
    return true;
}

DWORD WINAPI MutedVoiceSignOutThread(LPVOID) {
    StopMutedVoiceWorker();

    std::string authDir;
    bool ok = ResolveMutedVoiceAuthCacheDir(authDir) && DeleteMutedVoiceAuthCache(authDir);

    {
        std::lock_guard<std::mutex> lock(g_mutedVoiceMutex);
        ClearMutedVoiceAuthPromptLocked();
        g_mutedVoiceSignOutInProgress = false;
    }

    if (ok) {
        SetMutedVoiceStatus(MUTED_VOICE_STATUS_OFFLINE, "signed out");
    }
    else {
        SetMutedVoiceStatus(MUTED_VOICE_STATUS_ERROR, "sign out failed");
    }
    RequestGuiRepaint();
    return 0;
}

void BeginMutedVoiceSignOut() {
    bool shouldStart = false;
    {
        std::lock_guard<std::mutex> lock(g_mutedVoiceMutex);
        if (!g_mutedVoiceSignOutInProgress) {
            g_mutedVoiceSignOutInProgress = true;
            ClearMutedVoiceAuthPromptLocked();
            shouldStart = true;
        }
    }

    if (!shouldStart) return;

    g_guiExtrasMutedVoice = false;
    SaveToolSettings();
    SetMutedVoiceStatus(MUTED_VOICE_STATUS_OFFLINE, "signing out");

    DWORD threadId = 0;
    HANDLE threadHandle = CreateThread(nullptr, 0, MutedVoiceSignOutThread, nullptr, 0, &threadId);
    if (!threadHandle) {
        {
            std::lock_guard<std::mutex> lock(g_mutedVoiceMutex);
            g_mutedVoiceSignOutInProgress = false;
        }
        SetMutedVoiceStatus(MUTED_VOICE_STATUS_ERROR, "sign out thread failed");
        return;
    }

    CloseHandle(threadHandle);
    RequestGuiRepaint();
}

void OpenMutedVoiceAuthUrl() {
    std::string url;
    {
        std::lock_guard<std::mutex> lock(g_mutedVoiceMutex);
        url = g_mutedVoiceAuthUrl;
    }
    if (url.empty()) return;

    HINSTANCE result = ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if ((INT_PTR)result <= 32) {
        DebugLog("Muted Voice auth URL open failed result=%ld", (long)(INT_PTR)result);
    }
}

bool CopyTextToClipboard(const std::string& text) {
    if (text.empty()) return false;
    if (!OpenClipboard(g_guiHwnd)) return false;

    bool copied = false;
    if (EmptyClipboard()) {
        HGLOBAL dataHandle = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
        if (dataHandle) {
            void* data = GlobalLock(dataHandle);
            if (data) {
                memcpy(data, text.c_str(), text.size() + 1);
                GlobalUnlock(dataHandle);
                if (SetClipboardData(CF_TEXT, dataHandle)) {
                    copied = true;
                    dataHandle = nullptr;
                }
            }
            if (dataHandle) GlobalFree(dataHandle);
        }
    }

    CloseClipboard();
    return copied;
}

void CopyMutedVoiceAuthCode() {
    std::string code;
    {
        std::lock_guard<std::mutex> lock(g_mutedVoiceMutex);
        code = g_mutedVoiceAuthCode;
    }
    if (code.empty()) return;

    bool copied = CopyTextToClipboard(code);
    {
        std::lock_guard<std::mutex> lock(g_mutedVoiceMutex);
        g_mutedVoiceAuthMessage = copied ? "Code copied" : "Copy failed";
    }
    RequestGuiRepaint();
}

const char* WindowMessageName(UINT msg) {
    switch (msg) {
    case WM_KEYDOWN: return "WM_KEYDOWN";
    case WM_KEYUP: return "WM_KEYUP";
    case WM_SYSKEYDOWN: return "WM_SYSKEYDOWN";
    case WM_SYSKEYUP: return "WM_SYSKEYUP";
    case WM_SIZE: return "WM_SIZE";
    case WM_DISPLAYCHANGE: return "WM_DISPLAYCHANGE";
    case WM_WINDOWPOSCHANGING: return "WM_WINDOWPOSCHANGING";
    case WM_WINDOWPOSCHANGED: return "WM_WINDOWPOSCHANGED";
    case WM_STYLECHANGING: return "WM_STYLECHANGING";
    case WM_STYLECHANGED: return "WM_STYLECHANGED";
    case WM_ACTIVATEAPP: return "WM_ACTIVATEAPP";
    case WM_SETFOCUS: return "WM_SETFOCUS";
    case WM_KILLFOCUS: return "WM_KILLFOCUS";
    case WM_NCDESTROY: return "WM_NCDESTROY";
    default: return nullptr;
    }
}

bool ShouldLogWindowMessage(UINT msg, WPARAM wParam) {
    if ((msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP) && wParam == VK_F11) return true;
    switch (msg) {
    case WM_SIZE:
    case WM_DISPLAYCHANGE:
    case WM_WINDOWPOSCHANGING:
    case WM_WINDOWPOSCHANGED:
    case WM_STYLECHANGING:
    case WM_STYLECHANGED:
    case WM_ACTIVATEAPP:
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
    case WM_NCDESTROY:
        return true;
    default:
        return false;
    }
}

void LogWindowMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    const char* name = WindowMessageName(msg);
    if (!name) name = "WM_UNKNOWN";

    if (msg == WM_SIZE) {
        DebugLog("%s hwnd=%p size=%ux%u wParam=0x%Ix", name, hwnd, LOWORD(lParam), HIWORD(lParam), (size_t)wParam);
        return;
    }

    if (msg == WM_WINDOWPOSCHANGING || msg == WM_WINDOWPOSCHANGED) {
        WINDOWPOS* wp = reinterpret_cast<WINDOWPOS*>(lParam);
        if (wp) {
            DebugLog("%s hwnd=%p x=%d y=%d cx=%d cy=%d flags=0x%X after=%p",
                name, hwnd, wp->x, wp->y, wp->cx, wp->cy, wp->flags, wp->hwndInsertAfter);
            return;
        }
    }

    if (msg == WM_STYLECHANGING || msg == WM_STYLECHANGED) {
        STYLESTRUCT* style = reinterpret_cast<STYLESTRUCT*>(lParam);
        if (style) {
            DebugLog("%s hwnd=%p styleType=%llu old=0x%08lX new=0x%08lX",
                name, hwnd, (unsigned long long)wParam, style->styleOld, style->styleNew);
            return;
        }
    }

    DebugLog("%s hwnd=%p wParam=0x%Ix lParam=0x%Ix", name, hwnd, (size_t)wParam, (size_t)lParam);
}

LONG WINAPI FullscreenCrashUnhandledExceptionFilter(EXCEPTION_POINTERS* exceptionInfo) {
    DWORD code = 0;
    void* address = nullptr;
    void* accessAddress = nullptr;
    ULONG_PTR accessType = 0;
    void* ip = nullptr;

    if (exceptionInfo && exceptionInfo->ExceptionRecord) {
        code = exceptionInfo->ExceptionRecord->ExceptionCode;
        address = exceptionInfo->ExceptionRecord->ExceptionAddress;
        if (exceptionInfo->ExceptionRecord->NumberParameters >= 2 &&
            code == EXCEPTION_ACCESS_VIOLATION) {
            accessType = exceptionInfo->ExceptionRecord->ExceptionInformation[0];
            accessAddress = reinterpret_cast<void*>(exceptionInfo->ExceptionRecord->ExceptionInformation[1]);
        }
    }

    if (exceptionInfo && exceptionInfo->ContextRecord) {
#if defined(_M_X64)
        ip = reinterpret_cast<void*>(exceptionInfo->ContextRecord->Rip);
#elif defined(_M_IX86)
        ip = reinterpret_cast<void*>(exceptionInfo->ContextRecord->Eip);
#endif
    }

    DebugLog("Unhandled exception code=0x%08lX address=%p ip=%p accessType=%llu accessAddress=%p currentCtx=%p currentDC=%p",
        code,
        address,
        ip,
        (unsigned long long)accessType,
        accessAddress,
        wglGetCurrentContext(),
        wglGetCurrentDC());

    if (g_prevUnhandledExceptionFilter) return g_prevUnhandledExceptionFilter(exceptionInfo);
    return EXCEPTION_CONTINUE_SEARCH;
}

int NormalizeKeybind(int value) {
    return (value >= 0 && value <= 255) ? value : 0;
}

int NormalizeSeeBarriersRange(int value) {
    if (value >= kSeeBarriersRangeInfinite) return kSeeBarriersRangeInfinite;
    if (value < kSeeBarriersRangeMin) return kSeeBarriersRangeMin;
    if (value > kSeeBarriersRangeMax) return kSeeBarriersRangeMax;
    return value;
}

bool IsSeeBarriersRangeInfinite(int value) {
    return NormalizeSeeBarriersRange(value) == kSeeBarriersRangeInfinite;
}

std::string FormatSeeBarriersRangeLabel(int value) {
    if (IsSeeBarriersRangeInfinite(value)) return "Infinite";

    char buf[32];
    sprintf(buf, "%d blocks", NormalizeSeeBarriersRange(value));
    return buf;
}

int NormalizeSeeBarriersStyle(int value) {
    return value == SEE_BARRIERS_STYLE_OUTLINE ? SEE_BARRIERS_STYLE_OUTLINE : SEE_BARRIERS_STYLE_BOX_OUTLINE;
}

bool IsKeybindDown(int keybind) {
    return keybind != 0 && (GetAsyncKeyState(keybind) & 0x8000) != 0;
}

void SaveToolSettings() {
    std::lock_guard<std::mutex> lock(g_settingsMutex);
    std::ofstream f(kToolSettingsPath, std::ios::trunc);
    if (!f.is_open()) return;

    f << "snaplook_enabled " << (g_guiSnaplookEnabled ? 1 : 0) << "\n";
    f << "timer_enabled " << (g_guiTimerEnabled ? 1 : 0) << "\n";
    f << "timer_locked " << (g_guiTimerLocked ? 1 : 0) << "\n";
    f << "timer_nametag_enabled " << (g_guiTimerNametagEnabled ? 1 : 0) << "\n";
    f << "timer_nametag_position " << NormalizeTimerNametagPosition(g_guiTimerNametagPosition) << "\n";
    f << "timer_edit_default_scoreboard " << (g_guiTimerEditDefaultScoreboard ? 1 : 0) << "\n";
    f << "timer_obs_screenshots " << (g_guiTimerObsScreenshotsEnabled ? 1 : 0) << "\n";
    f << "timer_crosshair_mode " << (g_guiTimerCrosshairMode ? 1 : 0) << "\n";
    f << "timer_decimal_places " << g_guiTimerDecimalPlaces << "\n";
    for (int number = kTimerNumberMax; number >= kTimerNumberMin; --number) {
        std::uint32_t colour = GetTimerNumberColour(number);
        f << "timer_number_colour " << number << " "
            << GetTimerColourRed(colour) << " "
            << GetTimerColourGreen(colour) << " "
            << GetTimerColourBlue(colour) << "\n";
    }
    f << "gui_theme " << NormalizeGuiTheme(g_guiTheme) << "\n";
    f << "speed_slowness_enabled " << (g_guiSpeedSlownessEnabled ? 1 : 0) << "\n";
    f << "sound_settings_version 2" << "\n";
    f << "speed3_sound " << g_speed3Sound << "\n";
    f << "speed3_volume " << g_speed3Volume << "\n";
    f << "slowness_sound " << g_slownessSound << "\n";
    f << "slowness_volume " << g_slownessVolume << "\n";
    f << "public_helpers_wins_enabled " << (g_guiPublicWinsEnabled ? 1 : 0) << "\n";
    f << "public_helpers_wins_position " << g_guiPublicWinsPosition << "\n";
    f << "public_helpers_wins_space_between_username " << (g_guiPublicWinsSpaceBetweenUsername ? 1 : 0) << "\n";
    f << "extras_force_wheat_stage1 " << (g_guiExtrasForceWheatStage1 ? 1 : 0) << "\n";
    f << "extras_hide_beacon_beams " << (g_guiExtrasHideBeaconBeams ? 1 : 0) << "\n";
    f << "extras_disable_tag_scoreboard " << (g_guiExtrasDisableTagScoreboard ? 1 : 0) << "\n";
    f << "extras_muted_voice " << (g_guiExtrasMutedVoice ? 1 : 0) << "\n";
    f << "extras_muted_voice_hide_mute_reminder " << (g_guiExtrasMutedVoiceHideMuteReminder ? 1 : 0) << "\n";
    f << "extras_muted_voice_party_owner " << (g_guiMutedVoicePartyOwner.empty() ? "-" : g_guiMutedVoicePartyOwner) << "\n";
    f << "snaplook_keybind " << g_snaplookKeybind << "\n";
    f << "snaplook_camera_mode " << g_snaplookCameraMode << "\n";
}

void LoadToolSettings() {
    std::lock_guard<std::mutex> lock(g_settingsMutex);
    std::ifstream f(kToolSettingsPath);
    if (!f.is_open()) return;

    std::string key;
    bool guiThemeLoaded = false;
    int soundSettingsVersion = 1;
    while (f >> key) {
        if (key == "sound_settings_version") {
            f >> soundSettingsVersion;
        }
        else if (key == "snaplook_enabled") {
            int value = 0;
            if (f >> value) g_guiSnaplookEnabled = value != 0;
        }
        else if (key == "timer_enabled") {
            int value = 0;
            if (f >> value) g_guiTimerEnabled = value != 0;
        }
        else if (key == "timer_locked") {
            int value = 0;
            if (f >> value) g_guiTimerLocked = value != 0;
        }
        else if (key == "timer_nametag_enabled") {
            int value = 0;
            if (f >> value) g_guiTimerNametagEnabled = value != 0;
        }
        else if (key == "timer_nametag_position") {
            int value = TIMER_NAMETAG_POSITION_SUFFIX;
            if (f >> value) g_guiTimerNametagPosition = NormalizeTimerNametagPosition(value);
        }
        else if (key == "timer_edit_default_scoreboard") {
            int value = 0;
            if (f >> value) g_guiTimerEditDefaultScoreboard = value != 0;
        }
        else if (key == "timer_obs_screenshots") {
            int value = 0;
            if (f >> value) g_guiTimerObsScreenshotsEnabled = value != 0;
        }
        else if (key == "timer_crosshair_mode") {
            int value = 0;
            if (f >> value) g_guiTimerCrosshairMode = value != 0;
        }
        else if (key == "timer_decimal_places") {
            f >> g_guiTimerDecimalPlaces;
        }
        else if (key == "timer_number_colour" || key == "timer_number_color") {
            int number = -1;
            int red = 0;
            int green = 0;
            int blue = 0;
            if (f >> number >> red >> green >> blue &&
                number >= kTimerNumberMin && number <= kTimerNumberMax &&
                red >= 0 && red <= 255 &&
                green >= 0 && green <= 255 &&
                blue >= 0 && blue <= 255) {
                SetTimerNumberColour(number, red, green, blue);
            }
        }
        else if (key == "gui_theme") {
            int value = GUI_THEME_COSMIC;
            if (f >> value) {
                g_guiTheme = NormalizeGuiTheme(value);
                guiThemeLoaded = true;
            }
        }
        else if (key == "gui_cosmic_theme") {
            int value = 1;
            if (f >> value && !guiThemeLoaded) {
                g_guiTheme = value != 0 ? GUI_THEME_COSMIC : GUI_THEME_CLASSIC;
            }
        }
        else if (key == "speed_slowness_enabled") {
            int value = 0;
            if (f >> value) g_guiSpeedSlownessEnabled = value != 0;
        }
        else if (key == "speed3_sound") {
            f >> g_speed3Sound;
        }
        else if (key == "speed3_volume") {
            f >> g_speed3Volume;
        }
        else if (key == "slowness_sound") {
            f >> g_slownessSound;
        }
        else if (key == "slowness_volume") {
            f >> g_slownessVolume;
        }
        else if (key == "public_helpers_wins_enabled") {
            int value = 0;
            if (f >> value) g_guiPublicWinsEnabled = value != 0;
        }
        else if (key == "public_helpers_wins_position") {
            int value = PUBLIC_WINS_POSITION_PREFIX;
            if (f >> value) g_guiPublicWinsPosition = value == PUBLIC_WINS_POSITION_SUFFIX
                ? PUBLIC_WINS_POSITION_SUFFIX : PUBLIC_WINS_POSITION_PREFIX;
        }
        else if (key == "public_helpers_wins_space_between_username") {
            int value = 0;
            if (f >> value) g_guiPublicWinsSpaceBetweenUsername = value != 0;
        }
        else if (key == "extras_force_wheat_stage1") {
            int value = 0;
            if (f >> value) g_guiExtrasForceWheatStage1 = value != 0;
        }
        else if (key == "extras_hide_beacon_beams") {
            int value = 0;
            if (f >> value) g_guiExtrasHideBeaconBeams = value != 0;
        }
        else if (key == "extras_disable_tag_scoreboard") {
            int value = 0;
            if (f >> value) g_guiExtrasDisableTagScoreboard = value != 0;
        }
        else if (key == "extras_muted_voice") {
            int value = 0;
            if (f >> value) g_guiExtrasMutedVoice = value != 0;
        }
        else if (key == "extras_muted_voice_hide_mute_reminder" || key == "extras_muted_voice_block_local_send") {
            int value = 0;
            if (f >> value) g_guiExtrasMutedVoiceHideMuteReminder = value != 0;
        }
        else if (key == "extras_muted_voice_party_owner") {
            std::string value;
            if (f >> value) g_guiMutedVoicePartyOwner = (value == "-") ? "" : NormalizeMutedVoicePartyOwner(value);
        }
        else if (key == "snaplook_keybind") {
            f >> g_snaplookKeybind;
        }
        else if (key == "snaplook_camera_mode") {
            f >> g_snaplookCameraMode;
        }
        else {
            std::string ignored;
            std::getline(f, ignored);
        }
    }

    g_snaplookKeybind = NormalizeKeybind(g_snaplookKeybind);
    g_snaplookCameraMode = NormalizePerspectiveMode(g_snaplookCameraMode);
    g_guiTimerDecimalPlaces = NormalizeTimerDecimalPlaces(g_guiTimerDecimalPlaces);
    if (g_guiTimerDecimalPlaces == 0) g_guiTimerEditDefaultScoreboard = false;
    if (soundSettingsVersion < 2) {
        if (g_speed3Sound == 0) g_speed3Sound = ALERT_SOUND_RANDOM_ANVIL_LAND;
        else if (g_speed3Sound == 1) g_speed3Sound = ALERT_SOUND_RANDOM_LEVELUP;

        if (g_slownessSound == 0) g_slownessSound = ALERT_SOUND_RANDOM_ANVIL_LAND;
        else if (g_slownessSound == 1) g_slownessSound = ALERT_SOUND_RANDOM_LEVELUP;
    }
    g_speed3Sound = NormalizeAlertSoundId(g_speed3Sound, ALERT_SOUND_RANDOM_LEVELUP);
    g_slownessSound = NormalizeAlertSoundId(g_slownessSound, ALERT_SOUND_RANDOM_ANVIL_LAND);
    g_speed3Volume = ClampFloat(g_speed3Volume, kAlertVolumeMin, kAlertVolumeMax);
    g_slownessVolume = ClampFloat(g_slownessVolume, kAlertVolumeMin, kAlertVolumeMax);
    InterlockedExchange(&g_publicWinsRuntimeEnabled, g_guiPublicWinsEnabled ? 1 : 0);

}

// =============================================================
// Decimal timer
// =============================================================
double GetDecimalSeconds() {
    if (!g_timerActive || g_timerStartSeconds < 0.0) return -1.0;
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    double elapsed = (double)(now.QuadPart - g_explosionSetAt.QuadPart) / (double)g_perfFreq.QuadPart;
    double result = g_timerStartSeconds - elapsed;
    return result < 0.0 ? 0.0 : result;
}

void GetTimerColor(int whole, float& r, float& g, float& b) {
    std::uint32_t colour = GetTimerNumberColour(whole);
    r = (float)GetTimerColourRed(colour) / 255.0f;
    g = (float)GetTimerColourGreen(colour) / 255.0f;
    b = (float)GetTimerColourBlue(colour) / 255.0f;
}

char GetTimerColorCode(int whole) {
    std::uint32_t selected = GetTimerNumberColour(whole);
    int red = GetTimerColourRed(selected);
    int green = GetTimerColourGreen(selected);
    int blue = GetTimerColourBlue(selected);
    char nearestCode = kMinecraftColourOptions[0].code;
    int nearestDistance = (255 * 255 * 3) + 1;

    for (const MinecraftColourOption& colour : kMinecraftColourOptions) {
        int redDelta = red - colour.red;
        int greenDelta = green - colour.green;
        int blueDelta = blue - colour.blue;
        int distance = redDelta * redDelta + greenDelta * greenDelta + blueDelta * blueDelta;
        if (distance < nearestDistance) {
            nearestDistance = distance;
            nearestCode = colour.code;
        }
    }
    return nearestCode;
}

std::string FormatTimerText(double secondsRemaining) {
    double clampedSeconds = secondsRemaining < 0.0 ? 0.0 : secondsRemaining;
    char buf[24] = {};
    int decimals = NormalizeTimerDecimalPlaces(g_guiTimerDecimalPlaces);
    if (decimals <= 0) {
        sprintf_s(buf, "%d", (int)std::ceil(clampedSeconds));
    }
    else {
        sprintf_s(buf, "%.*f", decimals, clampedSeconds);
    }
    return buf;
}

// =============================================================
// JNI (main loop thread � timer)
// =============================================================
int GetDisplayedTimerNumber(double secondsRemaining) {
    double clampedSeconds = secondsRemaining < 0.0 ? 0.0 : secondsRemaining;
    int number = NormalizeTimerDecimalPlaces(g_guiTimerDecimalPlaces) <= 0
        ? (int)std::ceil(clampedSeconds)
        : (int)clampedSeconds;
    return ClampTimerNumber(number);
}

JavaVM* g_jvm = nullptr;
JNIEnv* g_env = nullptr;
jvmtiEnv* g_jvmti = nullptr;
bool g_jvmtiFailed = false;
bool g_seeBarriersMinecraftApplied = false;
bool g_seeBarriersMinecraftFailed = false;
volatile LONG g_seeBarriersMinecraftRefreshPending = 0;
bool g_sharedClassFileHookInstalled = false;
volatile LONG g_runtimeClassCaptureEnabled = 0;
jclass g_runtimeClassCaptureTarget = nullptr;
std::vector<unsigned char> g_runtimeCapturedClassBytes;
jclass g_seeBarriersBarrierClass = nullptr;
std::vector<unsigned char> g_seeBarriersOriginalClassBytes;
std::vector<unsigned char> g_seeBarriersPatchedClassBytes;
std::vector<unsigned char> g_seeBarriersOriginalDamageDispatcherBytes;
std::vector<unsigned char> g_seeBarriersPatchedDamageDispatcherBytes;
bool g_seeBarriersDamageDispatcherPatched = false;
std::vector<unsigned char> g_seeBarriersOriginalRenderChunkBytes;
std::vector<unsigned char> g_seeBarriersPatchedRenderChunkBytes;
bool g_seeBarriersRenderChunkPatched = false;
jclass g_seeBarriersBlockClass = nullptr;
jclass g_seeBarriersBlockRendererDispatcherClass = nullptr;
jclass g_seeBarriersRenderChunkClass = nullptr;
jclass g_seeBarriersBlockStateContainerClass = nullptr;
jclass g_seeBarriersBlockModelShapesClass = nullptr;
jclass g_seeBarriersMapClass = nullptr;
jclass g_seeBarriersBakedModelClass = nullptr;
jclass g_seeBarriersListClass = nullptr;
jclass g_seeBarriersDirectionClass = nullptr;
jclass g_seeBarriersObjectClass = nullptr;
jclass g_seeBarriersEnumClass = nullptr;
jmethodID g_seeBarriersMinecraftGetBlockRendererDispatcher = nullptr;
jmethodID g_seeBarriersBlockRendererGetModelShapes = nullptr;
jmethodID g_seeBarriersBlockModelShapesGetModel = nullptr;
jmethodID g_seeBarriersBlockGetById = nullptr;
jmethodID g_seeBarriersBlockGetStateContainer = nullptr;
jmethodID g_seeBarriersBlockGetDefaultState = nullptr;
jmethodID g_seeBarriersBlockStateContainerGetValidStates = nullptr;
jmethodID g_seeBarriersBlockGetRenderType = nullptr;
jmethodID g_seeBarriersBlockGetRenderLayer = nullptr;
jmethodID g_seeBarriersBlockGetActualState = nullptr;
jmethodID g_seeBarriersBlockShouldSideBeRendered = nullptr;
jmethodID g_seeBarriersBlockRendererGetModelForWorldState = nullptr;
jmethodID g_seeBarriersBakedModelGetGeneralQuads = nullptr;
jmethodID g_seeBarriersBakedModelGetFaceQuads = nullptr;
jmethodID g_seeBarriersListSize = nullptr;
jmethodID g_seeBarriersListGet = nullptr;
jmethodID g_seeBarriersDirectionValues = nullptr;
jmethodID g_seeBarriersObjectToString = nullptr;
jmethodID g_seeBarriersEnumOrdinal = nullptr;
jmethodID g_seeBarriersMapGet = nullptr;
jmethodID g_seeBarriersMapPut = nullptr;
jmethodID g_seeBarriersMapRemove = nullptr;
jfieldID g_seeBarriersBlockModelShapesModelMap = nullptr;
bool g_seeBarriersModelOverrideInited = false;
bool g_seeBarriersBarrierModelOverridden = false;
jobject g_seeBarriersOriginalBarrierModel = nullptr;

struct SeeBarriersStoredModelOverride {
    jobject state = nullptr;
    jobject originalModel = nullptr;
    bool hadOriginalModel = false;
};

std::vector<SeeBarriersStoredModelOverride> g_seeBarriersStoredModelOverrides;

bool EnsureSeeBarriersModelOverrideJNI(JNIEnv* env);
bool InitSeeBarriersJNI();
void ClearSeeBarriersDamageMarkers(JNIEnv* env);
void ResetSeeBarriersCache(JNIEnv* env);

struct ScoreboardJNIContext {
    jclass mcClass = nullptr;
    jclass objectiveClass = nullptr;
    jclass scoreClass = nullptr;
    jclass hashMapClass = nullptr;
    jclass arrayListClass = nullptr;
    jfieldID fWorldField = nullptr;
    jfieldID fSbField = nullptr;
    jfieldID fPlayerList = nullptr;
    jfieldID fObjectivesByName = nullptr;
    jfieldID fObjectivesByCriteria = nullptr;
    jfieldID fScoresByEntity = nullptr;
    jfieldID fMapE = nullptr;
    jfieldID fDisplayObjectives = nullptr;
    jfieldID fObjectiveName = nullptr;
    jfieldID fObjectiveCriteria = nullptr;
    jfieldID fObjectiveDisplayName = nullptr;
    jfieldID fScoreObjective = nullptr;
    jfieldID fScoreEntityName = nullptr;
    jfieldID fTeamName = nullptr;
    jfieldID fTeamMembers = nullptr;
    jfieldID fTeamDisplayName = nullptr;
    jfieldID fTeamE = nullptr;
    jfieldID fTeamF = nullptr;
    jfieldID fTeamFriendlyFire = nullptr;
    jfieldID fTeamSeeFriendlyInvisibles = nullptr;
    jfieldID fTeamNameTagVisibility = nullptr;
    jfieldID fTeamDeathMessageVisibility = nullptr;
    jfieldID fTeamColor = nullptr;
    jmethodID mGetMC = nullptr;
    jmethodID mMinecraftGetNetHandler = nullptr;
    jmethodID mNetHandlerGetPlayerInfoCollection = nullptr;
    jmethodID mNetworkInfoGetProfile = nullptr;
    jmethodID mScoreboardRemoveObjective = nullptr;
    jmethodID mScoreboardOnObjectiveAdded = nullptr;
    jmethodID mScoreboardOnScoreUpdated = nullptr;
    jmethodID mScoreboardSetObjectiveInDisplaySlot = nullptr;
    jmethodID mScoreboardGetTeam = nullptr;
    jmethodID mScoreboardCreateTeam = nullptr;
    jmethodID mScoreboardGetPlayersTeam = nullptr;
    jmethodID mScoreboardAddPlayerToTeam = nullptr;
    jmethodID mScoreboardRemovePlayerFromTeams = nullptr;
    jmethodID mScoreboardRemoveTeam = nullptr;
    jmethodID mMapGet = nullptr;
    jmethodID mMapPut = nullptr;
    jmethodID mMapValues = nullptr;
    jmethodID mToArray = nullptr;
    jmethodID mCollectionContains = nullptr;
    jmethodID mListAdd = nullptr;
    jmethodID mHashMapCtor = nullptr;
    jmethodID mArrayListCtor = nullptr;
    jmethodID mPlayerGetProfile = nullptr;
    jmethodID mGameProfileGetName = nullptr;
    jmethodID mGameProfileGetId = nullptr;
    jmethodID mUuidToString = nullptr;
    jobject hiddenSidebarScoreboard = nullptr;
    jobject hiddenSidebarObjectives[kScoreboardDisplaySlotCount] = {};
    std::vector<jobject> hiddenSidebarScores[kScoreboardDisplaySlotCount];
    bool inited = false;
    bool failed = false;
};

ScoreboardJNIContext g_scoreboardJNI;

enum class MinecraftClientProfile {
    Unknown = 0,
    BadlionObfuscated,
    LunarNamed
};

MinecraftClientProfile g_minecraftClientProfile = MinecraftClientProfile::Unknown;

bool IsLunarNamedClient() {
    return g_minecraftClientProfile == MinecraftClientProfile::LunarNamed;
}

bool AttachToJVM();

jclass FindLoadedClassBySignature(JNIEnv* env, const char* name) {
    if (!env || !name || !g_jvm) return nullptr;

    jvmtiEnv* jvmti = nullptr;
    jint res = g_jvm->GetEnv(reinterpret_cast<void**>(&jvmti), JVMTI_VERSION_1_0);
    if (res != JNI_OK || !jvmti) return nullptr;

    std::string wantedSignature;
    if (name[0] == '[') {
        wantedSignature = name;
    }
    else {
        wantedSignature = "L";
        wantedSignature += name;
        wantedSignature += ";";
    }

    jint classCount = 0;
    jclass* classes = nullptr;
    jvmtiError err = jvmti->GetLoadedClasses(&classCount, &classes);
    if (err != JVMTI_ERROR_NONE || !classes) return nullptr;

    jclass match = nullptr;
    for (jint i = 0; i < classCount; ++i) {
        char* signature = nullptr;
        err = jvmti->GetClassSignature(classes[i], &signature, nullptr);
        if (err == JVMTI_ERROR_NONE && signature) {
            bool same = wantedSignature == signature;
            jvmti->Deallocate(reinterpret_cast<unsigned char*>(signature));
            if (same) {
                match = static_cast<jclass>(env->NewLocalRef(classes[i]));
                if (env->ExceptionCheck()) {
                    env->ExceptionClear();
                    match = nullptr;
                }
                break;
            }
        }
        else if (signature) {
            jvmti->Deallocate(reinterpret_cast<unsigned char*>(signature));
        }
    }

    jvmti->Deallocate(reinterpret_cast<unsigned char*>(classes));
    return match;
}

const char* FindLunarNamedClass(const char* obfuscatedName) {
    if (!obfuscatedName || !*obfuscatedName) return nullptr;
    for (const LunarClassNameMapping& entry : kLunarClassNameMappings) {
        if (strcmp(entry.obfuscated, obfuscatedName) == 0) return entry.named;
    }
    return nullptr;
}

std::string TranslateLunarDescriptor(const char* descriptor) {
    if (!descriptor) return "";
    if (!IsLunarNamedClient()) return descriptor;

    std::string translated;
    const size_t length = strlen(descriptor);
    translated.reserve(length + 64);
    for (size_t index = 0; index < length;) {
        if (descriptor[index] != 'L') {
            translated.push_back(descriptor[index++]);
            continue;
        }

        const size_t end = std::string(descriptor).find(';', index + 1);
        if (end == std::string::npos) {
            translated.append(descriptor + index);
            break;
        }

        const std::string className(descriptor + index + 1, end - index - 1);
        const char* namedClass = FindLunarNamedClass(className.c_str());
        translated.push_back('L');
        translated += namedClass ? namedClass : className;
        translated.push_back(';');
        index = end + 1;
    }
    return translated;
}

std::string GetRuntimeClassName(jclass targetClass) {
    if (!targetClass || !g_jvm) return "";

    jvmtiEnv* jvmti = nullptr;
    if (g_jvm->GetEnv(reinterpret_cast<void**>(&jvmti), JVMTI_VERSION_1_0) != JNI_OK || !jvmti) {
        return "";
    }

    char* signature = nullptr;
    if (jvmti->GetClassSignature(targetClass, &signature, nullptr) != JVMTI_ERROR_NONE || !signature) {
        if (signature) jvmti->Deallocate(reinterpret_cast<unsigned char*>(signature));
        return "";
    }

    std::string result;
    const size_t length = strlen(signature);
    if (length >= 3 && signature[0] == 'L' && signature[length - 1] == ';') {
        result.assign(signature + 1, length - 2);
    }
    jvmti->Deallocate(reinterpret_cast<unsigned char*>(signature));
    return result;
}

const char* FindLunarFieldName(const std::string& runtimeOwner, const char* obfuscatedName) {
    if (runtimeOwner.empty() || !obfuscatedName) return nullptr;
    for (const LunarFieldNameMapping& entry : kLunarFieldNameMappings) {
        if (runtimeOwner == entry.owner && strcmp(entry.obfuscated, obfuscatedName) == 0) {
            return entry.named;
        }
    }
    return nullptr;
}

const char* FindLunarMethodName(
    const std::string& runtimeOwner,
    const char* obfuscatedName,
    const char* obfuscatedDescriptor) {
    if (runtimeOwner.empty() || !obfuscatedName || !obfuscatedDescriptor) return nullptr;
    for (const LunarMethodNameMapping& entry : kLunarMethodNameMappings) {
        if (runtimeOwner == entry.owner &&
            strcmp(entry.obfuscated, obfuscatedName) == 0 &&
            strcmp(entry.descriptor, obfuscatedDescriptor) == 0) {
            return entry.named;
        }
    }
    return nullptr;
}

template <typename Lookup>
const char* ResolveLunarMemberName(JNIEnv* env, jclass targetClass, Lookup&& lookup) {
    if (!env || !targetClass || !IsLunarNamedClient()) return nullptr;

    jclass currentClass = targetClass;
    bool ownsCurrentRef = false;
    const char* mappedName = nullptr;
    while (currentClass) {
        mappedName = lookup(GetRuntimeClassName(currentClass));
        if (mappedName) break;

        jclass parentClass = env->GetSuperclass(currentClass);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            parentClass = nullptr;
        }
        if (ownsCurrentRef) env->DeleteLocalRef(currentClass);
        currentClass = parentClass;
        ownsCurrentRef = true;
    }

    if (ownsCurrentRef && currentClass) env->DeleteLocalRef(currentClass);
    return mappedName;
}

void ClearLookupException(JNIEnv* env) {
    if (env && env->ExceptionCheck()) env->ExceptionClear();
}

jfieldID GetFieldIDCompat(JNIEnv* env, jclass targetClass, const char* name, const char* descriptor) {
    if (!env || !targetClass || !name || !descriptor) return nullptr;
    if (!IsLunarNamedClient()) return env->GetFieldID(targetClass, name, descriptor);

    const std::string namedDescriptor = TranslateLunarDescriptor(descriptor);
    const char* namedMember = ResolveLunarMemberName(
        env,
        targetClass,
        [&](const std::string& owner) { return FindLunarFieldName(owner, name); });
    const char* effectiveName = namedMember ? namedMember : name;
    jfieldID result = env->GetFieldID(targetClass, effectiveName, namedDescriptor.c_str());
    if (result && !env->ExceptionCheck()) return result;
    ClearLookupException(env);

    if (namedMember && strcmp(namedMember, name) != 0) {
        result = env->GetFieldID(targetClass, name, namedDescriptor.c_str());
        if (result && !env->ExceptionCheck()) return result;
        ClearLookupException(env);
    }
    return nullptr;
}

jfieldID GetStaticFieldIDCompat(JNIEnv* env, jclass targetClass, const char* name, const char* descriptor) {
    if (!env || !targetClass || !name || !descriptor) return nullptr;
    if (!IsLunarNamedClient()) return env->GetStaticFieldID(targetClass, name, descriptor);

    const std::string namedDescriptor = TranslateLunarDescriptor(descriptor);
    const char* namedMember = ResolveLunarMemberName(
        env,
        targetClass,
        [&](const std::string& owner) { return FindLunarFieldName(owner, name); });
    const char* effectiveName = namedMember ? namedMember : name;
    jfieldID result = env->GetStaticFieldID(targetClass, effectiveName, namedDescriptor.c_str());
    if (result && !env->ExceptionCheck()) return result;
    ClearLookupException(env);

    if (namedMember && strcmp(namedMember, name) != 0) {
        result = env->GetStaticFieldID(targetClass, name, namedDescriptor.c_str());
        if (result && !env->ExceptionCheck()) return result;
        ClearLookupException(env);
    }
    return nullptr;
}

jmethodID GetMethodIDCompat(JNIEnv* env, jclass targetClass, const char* name, const char* descriptor) {
    if (!env || !targetClass || !name || !descriptor) return nullptr;
    if (!IsLunarNamedClient()) return env->GetMethodID(targetClass, name, descriptor);

    const std::string namedDescriptor = TranslateLunarDescriptor(descriptor);
    const char* namedMember = ResolveLunarMemberName(
        env,
        targetClass,
        [&](const std::string& owner) { return FindLunarMethodName(owner, name, descriptor); });
    const char* effectiveName = namedMember ? namedMember : name;
    jmethodID result = env->GetMethodID(targetClass, effectiveName, namedDescriptor.c_str());
    if (result && !env->ExceptionCheck()) return result;
    ClearLookupException(env);

    if (namedMember && strcmp(namedMember, name) != 0) {
        result = env->GetMethodID(targetClass, name, namedDescriptor.c_str());
        if (result && !env->ExceptionCheck()) return result;
        ClearLookupException(env);
    }
    return nullptr;
}

jmethodID GetStaticMethodIDCompat(JNIEnv* env, jclass targetClass, const char* name, const char* descriptor) {
    if (!env || !targetClass || !name || !descriptor) return nullptr;
    if (!IsLunarNamedClient()) return env->GetStaticMethodID(targetClass, name, descriptor);

    const std::string namedDescriptor = TranslateLunarDescriptor(descriptor);
    const char* namedMember = ResolveLunarMemberName(
        env,
        targetClass,
        [&](const std::string& owner) { return FindLunarMethodName(owner, name, descriptor); });
    const char* effectiveName = namedMember ? namedMember : name;
    jmethodID result = env->GetStaticMethodID(targetClass, effectiveName, namedDescriptor.c_str());
    if (result && !env->ExceptionCheck()) return result;
    ClearLookupException(env);

    if (namedMember && strcmp(namedMember, name) != 0) {
        result = env->GetStaticMethodID(targetClass, name, namedDescriptor.c_str());
        if (result && !env->ExceptionCheck()) return result;
        ClearLookupException(env);
    }
    return nullptr;
}

bool AttachToJVM() {
    jsize vmCount = 0;
    jint result = JNI_GetCreatedJavaVMs(&g_jvm, 1, &vmCount);
    if (result != JNI_OK || vmCount == 0) return false;
    result = g_jvm->AttachCurrentThread(reinterpret_cast<void**>(&g_env), nullptr);
    if (result != JNI_OK || !g_env) return false;

    jclass lunarMinecraft = FindLoadedClassBySignature(g_env, "net/minecraft/client/Minecraft");
    if (lunarMinecraft) {
        g_minecraftClientProfile = MinecraftClientProfile::LunarNamed;
        g_env->DeleteLocalRef(lunarMinecraft);
    }
    else {
        g_minecraftClientProfile = MinecraftClientProfile::BadlionObfuscated;
    }
    DebugLog("Minecraft client profile=%s",
        IsLunarNamedClient() ? "lunar-named-1.8.9" : "badlion-obfuscated-1.8.9");
    return true;
}

std::string ToClassLoaderBinaryName(const char* name) {
    if (!name || name[0] == '[') return "";

    std::string binaryName(name);
    if (binaryName.empty() || binaryName.find(';') != std::string::npos) return "";

    std::replace(binaryName.begin(), binaryName.end(), '/', '.');
    return binaryName;
}

jclass LoadClassWithClassLoader(JNIEnv* env, jobject classLoader, jmethodID loadClassMethod, const std::string& binaryName) {
    if (!env || !classLoader || !loadClassMethod || binaryName.empty()) return nullptr;

    jstring className = env->NewStringUTF(binaryName.c_str());
    if (!className || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (className) env->DeleteLocalRef(className);
        return nullptr;
    }

    jobject loadedClass = env->CallObjectMethod(classLoader, loadClassMethod, className);
    env->DeleteLocalRef(className);
    if (!loadedClass || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (loadedClass) env->DeleteLocalRef(loadedClass);
        return nullptr;
    }

    return static_cast<jclass>(loadedClass);
}

jclass TryClassLoader(JNIEnv* env, jobject classLoader, jmethodID loadClassMethod, const std::string& binaryName, jobject& outWinningLoader) {
    outWinningLoader = nullptr;
    jclass loadedClass = LoadClassWithClassLoader(env, classLoader, loadClassMethod, binaryName);
    if (!loadedClass) return nullptr;

    outWinningLoader = env->NewLocalRef(classLoader);
    if (!outWinningLoader || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (outWinningLoader) env->DeleteLocalRef(outWinningLoader);
        outWinningLoader = nullptr;
    }

    return loadedClass;
}

jclass FindClassViaThreadClassLoaders(JNIEnv* env, const char* name, jobject& outWinningLoader, int& outLoaderCount, int& outCheckedCount) {
    outWinningLoader = nullptr;
    outLoaderCount = 0;
    outCheckedCount = 0;

    std::string binaryName = ToClassLoaderBinaryName(name);
    if (binaryName.empty()) return nullptr;

    jclass classLoaderClass = env->FindClass("java/lang/ClassLoader");
    jclass threadClass = env->FindClass("java/lang/Thread");
    jclass mapClass = env->FindClass("java/util/Map");
    jclass collectionClass = env->FindClass("java/util/Collection");
    if (!classLoaderClass || !threadClass || !mapClass || !collectionClass || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (collectionClass) env->DeleteLocalRef(collectionClass);
        if (mapClass) env->DeleteLocalRef(mapClass);
        if (threadClass) env->DeleteLocalRef(threadClass);
        if (classLoaderClass) env->DeleteLocalRef(classLoaderClass);
        return nullptr;
    }

    jmethodID loadClassMethod = GetMethodIDCompat(env, classLoaderClass, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    jmethodID getSystemClassLoaderMethod = GetStaticMethodIDCompat(env, classLoaderClass, "getSystemClassLoader", "()Ljava/lang/ClassLoader;");
    jmethodID currentThreadMethod = GetStaticMethodIDCompat(env, threadClass, "currentThread", "()Ljava/lang/Thread;");
    jmethodID getAllStackTracesMethod = GetStaticMethodIDCompat(env, threadClass, "getAllStackTraces", "()Ljava/util/Map;");
    jmethodID getContextClassLoaderMethod = GetMethodIDCompat(env, threadClass, "getContextClassLoader", "()Ljava/lang/ClassLoader;");
    jmethodID keySetMethod = GetMethodIDCompat(env, mapClass, "keySet", "()Ljava/util/Set;");
    jmethodID toArrayMethod = GetMethodIDCompat(env, collectionClass, "toArray", "()[Ljava/lang/Object;");
    if (!loadClassMethod || !currentThreadMethod || !getAllStackTracesMethod ||
        !getContextClassLoaderMethod || !keySetMethod || !toArrayMethod || env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(collectionClass);
        env->DeleteLocalRef(mapClass);
        env->DeleteLocalRef(threadClass);
        env->DeleteLocalRef(classLoaderClass);
        return nullptr;
    }

    auto tryLoaderObject = [&](jobject loader) -> jclass {
        if (!loader || env->ExceptionCheck()) {
            env->ExceptionClear();
            return nullptr;
        }

        ++outLoaderCount;
        ++outCheckedCount;
        return TryClassLoader(env, loader, loadClassMethod, binaryName, outWinningLoader);
    };

    jobject currentThread = env->CallStaticObjectMethod(threadClass, currentThreadMethod);
    if (currentThread && !env->ExceptionCheck()) {
        jobject loader = env->CallObjectMethod(currentThread, getContextClassLoaderMethod);
        jclass loadedClass = tryLoaderObject(loader);
        if (loader) env->DeleteLocalRef(loader);
        env->DeleteLocalRef(currentThread);
        if (loadedClass) {
            env->DeleteLocalRef(collectionClass);
            env->DeleteLocalRef(mapClass);
            env->DeleteLocalRef(threadClass);
            env->DeleteLocalRef(classLoaderClass);
            return loadedClass;
        }
    }
    else {
        env->ExceptionClear();
        if (currentThread) env->DeleteLocalRef(currentThread);
    }

    if (getSystemClassLoaderMethod) {
        jobject systemLoader = env->CallStaticObjectMethod(classLoaderClass, getSystemClassLoaderMethod);
        jclass loadedClass = tryLoaderObject(systemLoader);
        if (systemLoader) env->DeleteLocalRef(systemLoader);
        if (loadedClass) {
            env->DeleteLocalRef(collectionClass);
            env->DeleteLocalRef(mapClass);
            env->DeleteLocalRef(threadClass);
            env->DeleteLocalRef(classLoaderClass);
            return loadedClass;
        }
    }
    else if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }

    jobject stackTraces = env->CallStaticObjectMethod(threadClass, getAllStackTracesMethod);
    jobject threadSet = nullptr;
    jobjectArray threadArray = nullptr;
    if (stackTraces && !env->ExceptionCheck()) {
        threadSet = env->CallObjectMethod(stackTraces, keySetMethod);
        if (threadSet && !env->ExceptionCheck()) {
            threadArray = static_cast<jobjectArray>(env->CallObjectMethod(threadSet, toArrayMethod));
        }
    }

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }

    if (threadArray) {
        jsize threadCount = env->GetArrayLength(threadArray);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            threadCount = 0;
        }

        for (jsize i = 0; i < threadCount; ++i) {
            jobject thread = env->GetObjectArrayElement(threadArray, i);
            if (!thread || env->ExceptionCheck()) {
                env->ExceptionClear();
                if (thread) env->DeleteLocalRef(thread);
                continue;
            }

            jobject loader = env->CallObjectMethod(thread, getContextClassLoaderMethod);
            jclass loadedClass = tryLoaderObject(loader);
            if (loader) env->DeleteLocalRef(loader);
            env->DeleteLocalRef(thread);
            if (loadedClass) {
                env->DeleteLocalRef(threadArray);
                if (threadSet) env->DeleteLocalRef(threadSet);
                if (stackTraces) env->DeleteLocalRef(stackTraces);
                env->DeleteLocalRef(collectionClass);
                env->DeleteLocalRef(mapClass);
                env->DeleteLocalRef(threadClass);
                env->DeleteLocalRef(classLoaderClass);
                return loadedClass;
            }
        }
    }

    if (threadArray) env->DeleteLocalRef(threadArray);
    if (threadSet) env->DeleteLocalRef(threadSet);
    if (stackTraces) env->DeleteLocalRef(stackTraces);
    env->DeleteLocalRef(collectionClass);
    env->DeleteLocalRef(mapClass);
    env->DeleteLocalRef(threadClass);
    env->DeleteLocalRef(classLoaderClass);
    return nullptr;
}

jclass FindClassLoose(JNIEnv* env, const char* name) {
    if (!env || !name) return nullptr;

    if (IsLunarNamedClient()) {
        const char* namedClass = FindLunarNamedClass(name);
        if (namedClass) name = namedClass;
    }

    jclass localClass = env->FindClass(name);
    if (localClass && !env->ExceptionCheck()) return localClass;

    if (env->ExceptionCheck()) env->ExceptionClear();
    if (localClass) env->DeleteLocalRef(localClass);

    static std::mutex s_cacheMutex;
    static std::unordered_map<std::string, jclass> s_globalClassCache;
    static jobject s_cachedMinecraftClassLoader = nullptr;

    auto cacheClass = [&](jclass cls, const char* logPrefix) {
        if (!cls) return;

        jclass globalClass = static_cast<jclass>(env->NewGlobalRef(cls));
        if (!globalClass || env->ExceptionCheck()) {
            env->ExceptionClear();
            if (globalClass) env->DeleteGlobalRef(globalClass);
            return;
        }

        std::lock_guard<std::mutex> lock(s_cacheMutex);
        auto inserted = s_globalClassCache.emplace(name, globalClass);
        if (!inserted.second) {
            env->DeleteGlobalRef(globalClass);
        }
        else {
            DebugLog("%s %s", logPrefix, name);
        }
    };

    {
        std::lock_guard<std::mutex> lock(s_cacheMutex);
        auto it = s_globalClassCache.find(name);
        if (it != s_globalClassCache.end() && it->second) {
            localClass = static_cast<jclass>(env->NewLocalRef(it->second));
            if (localClass && !env->ExceptionCheck()) return localClass;
            if (env->ExceptionCheck()) env->ExceptionClear();
        }
    }

    std::string binaryName = ToClassLoaderBinaryName(name);
    jobject cachedLoaderLocal = nullptr;
    if (!binaryName.empty()) {
        {
            std::lock_guard<std::mutex> lock(s_cacheMutex);
            if (s_cachedMinecraftClassLoader) cachedLoaderLocal = env->NewLocalRef(s_cachedMinecraftClassLoader);
        }

        if (cachedLoaderLocal && !env->ExceptionCheck()) {
            jclass classLoaderClass = env->FindClass("java/lang/ClassLoader");
            jmethodID loadClassMethod = nullptr;
            if (classLoaderClass && !env->ExceptionCheck()) {
                loadClassMethod = GetMethodIDCompat(env, classLoaderClass, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
            }

            if (loadClassMethod && !env->ExceptionCheck()) {
                localClass = LoadClassWithClassLoader(env, cachedLoaderLocal, loadClassMethod, binaryName);
            }
            else {
                env->ExceptionClear();
            }

            if (classLoaderClass) env->DeleteLocalRef(classLoaderClass);
            env->DeleteLocalRef(cachedLoaderLocal);
            if (localClass) {
                cacheClass(localClass, "FindClass cached classloader resolved class");
                return localClass;
            }
        }
        else if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }

        jobject discoveredLoader = nullptr;
        int loaderCount = 0;
        int checkedCount = 0;
        localClass = FindClassViaThreadClassLoaders(env, name, discoveredLoader, loaderCount, checkedCount);
        if (localClass) {
            if (discoveredLoader) {
                std::lock_guard<std::mutex> lock(s_cacheMutex);
                if (!s_cachedMinecraftClassLoader) {
                    s_cachedMinecraftClassLoader = env->NewGlobalRef(discoveredLoader);
                    if (!s_cachedMinecraftClassLoader || env->ExceptionCheck()) {
                        env->ExceptionClear();
                        if (s_cachedMinecraftClassLoader) env->DeleteGlobalRef(s_cachedMinecraftClassLoader);
                        s_cachedMinecraftClassLoader = nullptr;
                    }
                    else {
                        DebugLog("FindClass classloader cached from class %s loaders=%d checked=%d", name, loaderCount, checkedCount);
                    }
                }
                env->DeleteLocalRef(discoveredLoader);
            }
            cacheClass(localClass, "FindClass classloader resolved class");
            return localClass;
        }

        if (std::string(name) == "ave") {
            DebugLog("FindClass classloader failed for ave loaders=%d checked=%d", loaderCount, checkedCount);
        }
    }

    localClass = FindLoadedClassBySignature(env, name);
    if (!localClass) return nullptr;

    cacheClass(localClass, "FindClass JVMTI loaded-class resolved class");
    return localClass;
}

std::string ExtractSeconds(const std::string& line, char& colorCode) {
    char lastColor = 'a';
    for (size_t i = 0; i < line.size(); i++) {
        if (line[i] == '\xC2' && i + 2 < line.size() && line[i + 1] == '\xA7') {
            lastColor = line[i + 2]; i += 2; continue;
        }
        if (line[i] == '\xA7' && i + 1 < line.size()) {
            lastColor = line[i + 1]; i++; continue;
        }

        if (line[i] >= '0' && line[i] <= '9') {
            std::string num;
            colorCode = lastColor;
            do {
                num.push_back(line[i]);
                ++i;
            } while (i < line.size() && line[i] >= '0' && line[i] <= '9');
            return num;
        }
    }

    return "";
}

std::string StripMinecraftFormattingCodes(const std::string& value) {
    std::string clean;
    clean.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '\xC2' && i + 2 < value.size() && value[i + 1] == '\xA7') {
            i += 2;
            continue;
        }
        if (value[i] == '\xA7' && i + 1 < value.size()) {
            ++i;
            continue;
        }
        clean.push_back(value[i]);
    }
    return clean;
}

std::string JStringToUtf8(JNIEnv* env, jstring value) {
    if (!env || !value) return "";
    const char* chars = env->GetStringUTFChars(value, nullptr);
    if (!chars) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return "";
    }
    std::string result(chars);
    env->ReleaseStringUTFChars(value, chars);
    return result;
}

std::string JStringToUtf8(jstring value) {
    return JStringToUtf8(g_env, value);
}

struct MutedVoiceLocalChatJNIContext {
    jclass mcClass = nullptr;
    jclass chatSerializerClass = nullptr;
    jfieldID fGuiIngame = nullptr;
    jmethodID mGetMinecraft = nullptr;
    jmethodID mGetChatGui = nullptr;
    jmethodID mAddChatMessage = nullptr;
    jmethodID mParseJson = nullptr;
    bool inited = false;
    bool failed = false;
};

MutedVoiceLocalChatJNIContext g_mutedVoiceLocalChatJNI;

bool InitMutedVoiceLocalChatJNI(JNIEnv* env) {
    if (!env) return false;
    if (g_mutedVoiceLocalChatJNI.inited) return true;
    if (g_mutedVoiceLocalChatJNI.failed) return false;

    auto fail = [&](const char* label) -> bool {
        if (env->ExceptionCheck()) env->ExceptionClear();
        DebugLog("Muted Voice local chat JNI init failed: %s", label);
        g_mutedVoiceLocalChatJNI.failed = true;
        return false;
    };

    jclass mcClassLocal = FindClassLoose(env, "ave");
    if (!mcClassLocal) return fail("FindClass ave");
    g_mutedVoiceLocalChatJNI.mcClass = static_cast<jclass>(env->NewGlobalRef(mcClassLocal));
    env->DeleteLocalRef(mcClassLocal);
    if (!g_mutedVoiceLocalChatJNI.mcClass || env->ExceptionCheck()) return fail("NewGlobalRef ave");

    g_mutedVoiceLocalChatJNI.mGetMinecraft = GetStaticMethodIDCompat(env, g_mutedVoiceLocalChatJNI.mcClass, "A", "()Lave;");
    if (!g_mutedVoiceLocalChatJNI.mGetMinecraft || env->ExceptionCheck()) return fail("ave.A()Lave;");

    g_mutedVoiceLocalChatJNI.fGuiIngame = GetFieldIDCompat(env, g_mutedVoiceLocalChatJNI.mcClass, "q", "Lavo;");
    if (!g_mutedVoiceLocalChatJNI.fGuiIngame || env->ExceptionCheck()) return fail("ave.q Lavo;");

    jclass guiIngameClass = FindClassLoose(env, "avo");
    if (!guiIngameClass) return fail("FindClass avo");
    g_mutedVoiceLocalChatJNI.mGetChatGui = GetMethodIDCompat(env, guiIngameClass, "d", "()Lavt;");
    env->DeleteLocalRef(guiIngameClass);
    if (!g_mutedVoiceLocalChatJNI.mGetChatGui || env->ExceptionCheck()) return fail("avo.d()Lavt;");

    jclass chatGuiClass = FindClassLoose(env, "avt");
    if (!chatGuiClass) return fail("FindClass avt");
    g_mutedVoiceLocalChatJNI.mAddChatMessage = GetMethodIDCompat(env, chatGuiClass, "a", "(Leu;)V");
    env->DeleteLocalRef(chatGuiClass);
    if (!g_mutedVoiceLocalChatJNI.mAddChatMessage || env->ExceptionCheck()) return fail("avt.a(Leu;)V");

    jclass serializerClassLocal = FindClassLoose(env, "eu$a");
    if (!serializerClassLocal) return fail("FindClass eu$a");
    g_mutedVoiceLocalChatJNI.chatSerializerClass = static_cast<jclass>(env->NewGlobalRef(serializerClassLocal));
    env->DeleteLocalRef(serializerClassLocal);
    if (!g_mutedVoiceLocalChatJNI.chatSerializerClass || env->ExceptionCheck()) return fail("NewGlobalRef eu$a");

    g_mutedVoiceLocalChatJNI.mParseJson = GetStaticMethodIDCompat(env,
        g_mutedVoiceLocalChatJNI.chatSerializerClass,
        "a",
        "(Ljava/lang/String;)Leu;");
    if (!g_mutedVoiceLocalChatJNI.mParseJson || env->ExceptionCheck()) return fail("eu$a.a(Ljava/lang/String;)Leu;");

    g_mutedVoiceLocalChatJNI.inited = true;
    DebugLog("Muted Voice local chat JNI ready");
    return true;
}

bool InjectLocalChatJson(JNIEnv* env, const std::string& json) {
    if (!env || json.empty() || !InitMutedVoiceLocalChatJNI(env)) return false;

    jobject mc = env->CallStaticObjectMethod(
        g_mutedVoiceLocalChatJNI.mcClass,
        g_mutedVoiceLocalChatJNI.mGetMinecraft);
    if (!mc || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (mc) env->DeleteLocalRef(mc);
        return false;
    }

    jobject guiIngame = env->GetObjectField(mc, g_mutedVoiceLocalChatJNI.fGuiIngame);
    if (!guiIngame || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (guiIngame) env->DeleteLocalRef(guiIngame);
        env->DeleteLocalRef(mc);
        return false;
    }

    jobject chatGui = env->CallObjectMethod(guiIngame, g_mutedVoiceLocalChatJNI.mGetChatGui);
    if (!chatGui || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (chatGui) env->DeleteLocalRef(chatGui);
        env->DeleteLocalRef(guiIngame);
        env->DeleteLocalRef(mc);
        return false;
    }

    jstring jsonString = env->NewStringUTF(json.c_str());
    if (!jsonString || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (jsonString) env->DeleteLocalRef(jsonString);
        env->DeleteLocalRef(chatGui);
        env->DeleteLocalRef(guiIngame);
        env->DeleteLocalRef(mc);
        return false;
    }

    jobject component = env->CallStaticObjectMethod(
        g_mutedVoiceLocalChatJNI.chatSerializerClass,
        g_mutedVoiceLocalChatJNI.mParseJson,
        jsonString);
    env->DeleteLocalRef(jsonString);
    if (!component || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (component) env->DeleteLocalRef(component);
        env->DeleteLocalRef(chatGui);
        env->DeleteLocalRef(guiIngame);
        env->DeleteLocalRef(mc);
        return false;
    }

    env->CallVoidMethod(chatGui, g_mutedVoiceLocalChatJNI.mAddChatMessage, component);
    bool ok = !env->ExceptionCheck();
    if (!ok) env->ExceptionClear();

    env->DeleteLocalRef(component);
    env->DeleteLocalRef(chatGui);
    env->DeleteLocalRef(guiIngame);
    env->DeleteLocalRef(mc);
    return ok;
}

bool InjectLocalChatJson(const std::string& json) {
    return InjectLocalChatJson(g_env, json);
}

void FlushMutedVoiceLocalChatQueue(JNIEnv* env) {
    if (!env) return;

    std::deque<std::string> pending;
    {
        std::lock_guard<std::mutex> lock(g_mutedVoiceLocalChatMutex);
        pending.swap(g_mutedVoiceLocalChatJsonQueue);
    }

    for (const std::string& json : pending) {
        if (!InjectLocalChatJson(env, json)) {
            DebugLog("Muted Voice local chat inject failed jsonLength=%u", (unsigned int)json.size());
        }
    }
}

JNIEnv* GetJNIEnvForCurrentThread();
extern volatile LONG64 g_mutedVoiceLastLocalChatAttemptMs;

struct MutedVoiceChatJNIContext {
    jclass mcClass = nullptr;
    jclass guiChatClass = nullptr;
    jfieldID fCurrentScreen = nullptr;
    jfieldID fChatInputField = nullptr;
    jmethodID mGetMinecraft = nullptr;
    jmethodID mDisplayGuiScreen = nullptr;
    jmethodID mTextFieldGetText = nullptr;
    bool inited = false;
    bool failed = false;
};

MutedVoiceChatJNIContext g_mutedVoiceChatJNI;

bool InitMutedVoiceChatJNI(JNIEnv* env) {
    if (!env) return false;
    if (g_mutedVoiceChatJNI.inited) return true;
    if (g_mutedVoiceChatJNI.failed) return false;

    auto fail = [&](const char* label) -> bool {
        if (env->ExceptionCheck()) env->ExceptionClear();
        DebugLog("Muted Voice chat JNI init failed: %s", label);
        g_mutedVoiceChatJNI.failed = true;
        return false;
    };

    jclass mcClassLocal = FindClassLoose(env, "ave");
    if (!mcClassLocal) return fail("FindClass ave");
    g_mutedVoiceChatJNI.mcClass = static_cast<jclass>(env->NewGlobalRef(mcClassLocal));
    env->DeleteLocalRef(mcClassLocal);
    if (!g_mutedVoiceChatJNI.mcClass || env->ExceptionCheck()) return fail("NewGlobalRef ave");

    g_mutedVoiceChatJNI.mGetMinecraft = GetStaticMethodIDCompat(env, g_mutedVoiceChatJNI.mcClass, "A", "()Lave;");
    if (!g_mutedVoiceChatJNI.mGetMinecraft || env->ExceptionCheck()) return fail("ave.A()Lave;");

    g_mutedVoiceChatJNI.mDisplayGuiScreen = GetMethodIDCompat(env, g_mutedVoiceChatJNI.mcClass, "a", "(Laxu;)V");
    if (!g_mutedVoiceChatJNI.mDisplayGuiScreen || env->ExceptionCheck()) return fail("ave.a(Laxu;)V");

    g_mutedVoiceChatJNI.fCurrentScreen = GetFieldIDCompat(env, g_mutedVoiceChatJNI.mcClass, "m", "Laxu;");
    if (!g_mutedVoiceChatJNI.fCurrentScreen || env->ExceptionCheck()) return fail("ave.m Laxu;");

    jclass guiChatClassLocal = FindClassLoose(env, "awv");
    if (!guiChatClassLocal) return fail("FindClass awv");
    g_mutedVoiceChatJNI.guiChatClass = static_cast<jclass>(env->NewGlobalRef(guiChatClassLocal));

    g_mutedVoiceChatJNI.fChatInputField = GetFieldIDCompat(env, guiChatClassLocal, "a", "Lavw;");
    env->DeleteLocalRef(guiChatClassLocal);
    if (!g_mutedVoiceChatJNI.guiChatClass || env->ExceptionCheck()) return fail("GuiChat input field");
    if (!g_mutedVoiceChatJNI.fChatInputField || env->ExceptionCheck()) return fail("awv.a Lavw;");

    jclass textFieldClass = FindClassLoose(env, "avw");
    if (!textFieldClass) return fail("FindClass avw");
    g_mutedVoiceChatJNI.mTextFieldGetText = GetMethodIDCompat(env, textFieldClass, "b", "()Ljava/lang/String;");
    env->DeleteLocalRef(textFieldClass);
    if (!g_mutedVoiceChatJNI.mTextFieldGetText || env->ExceptionCheck()) return fail("avw.b()Ljava/lang/String;");

    g_mutedVoiceChatJNI.inited = true;
    return true;
}

bool ReadCurrentMutedVoiceChatInput(JNIEnv* env, std::string& message) {
    message.clear();
    if (!InitMutedVoiceChatJNI(env)) return false;

    jobject mc = env->CallStaticObjectMethod(
        g_mutedVoiceChatJNI.mcClass,
        g_mutedVoiceChatJNI.mGetMinecraft);
    if (!mc || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (mc) env->DeleteLocalRef(mc);
        return false;
    }

    jobject screen = env->GetObjectField(mc, g_mutedVoiceChatJNI.fCurrentScreen);
    if (!screen || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (screen) env->DeleteLocalRef(screen);
        env->DeleteLocalRef(mc);
        return false;
    }

    bool isChat = env->IsInstanceOf(screen, g_mutedVoiceChatJNI.guiChatClass) == JNI_TRUE;
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        isChat = false;
    }
    if (!isChat) {
        env->DeleteLocalRef(screen);
        env->DeleteLocalRef(mc);
        return false;
    }

    jobject inputField = env->GetObjectField(screen, g_mutedVoiceChatJNI.fChatInputField);
    if (!inputField || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (inputField) env->DeleteLocalRef(inputField);
        env->DeleteLocalRef(screen);
        env->DeleteLocalRef(mc);
        return false;
    }

    jstring text = static_cast<jstring>(env->CallObjectMethod(inputField, g_mutedVoiceChatJNI.mTextFieldGetText));
    if (!text || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (text) env->DeleteLocalRef(text);
        env->DeleteLocalRef(inputField);
        env->DeleteLocalRef(screen);
        env->DeleteLocalRef(mc);
        return false;
    }

    message = JStringToUtf8(env, text);
    env->DeleteLocalRef(text);
    env->DeleteLocalRef(inputField);
    env->DeleteLocalRef(screen);
    env->DeleteLocalRef(mc);
    return !message.empty();
}

std::string TrimChatCommandWhitespace(const std::string& value) {
    size_t first = 0;
    while (first < value.size() && (value[first] == ' ' || value[first] == '\t' ||
        value[first] == '\r' || value[first] == '\n')) {
        ++first;
    }

    size_t last = value.size();
    while (last > first && (value[last - 1] == ' ' || value[last - 1] == '\t' ||
        value[last - 1] == '\r' || value[last - 1] == '\n')) {
        --last;
    }

    return value.substr(first, last - first);
}

bool HasChatCommandArgument(const std::string& text, size_t offset) {
    for (size_t i = offset; i < text.size(); ++i) {
        char c = text[i];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') return true;
    }
    return false;
}

bool IsMutedVoiceBotReroutedChatMessage(const std::string& message) {
    std::string text = TrimChatCommandWhitespace(message);
    if (text.empty()) return false;
    if (text[0] != '/') return true;

    size_t commandStart = 1;
    size_t commandEnd = commandStart;
    while (commandEnd < text.size()) {
        char c = text[commandEnd];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') break;
        ++commandEnd;
    }
    if (commandEnd == commandStart) return false;

    std::string command = ToLowerAscii(text.substr(commandStart, commandEnd - commandStart));
    bool reroutedCommand =
        command == "pc" ||
        command == "ac" ||
        command == "gc" ||
        command == "oc" ||
        command == "msg" ||
        command == "w";
    return reroutedCommand && HasChatCommandArgument(text, commandEnd);
}

bool ParseMutedVoicePrivateMessageCommand(
    const std::string& message,
    std::string& player,
    std::string& privateMessage) {
    player.clear();
    privateMessage.clear();

    std::string text = TrimChatCommandWhitespace(message);
    if (text.empty() || text[0] != '/') return false;

    size_t commandStart = 1;
    size_t commandEnd = commandStart;
    while (commandEnd < text.size()) {
        char c = text[commandEnd];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') break;
        ++commandEnd;
    }
    if (commandEnd == commandStart) return false;

    std::string command = ToLowerAscii(text.substr(commandStart, commandEnd - commandStart));
    if (command != "msg") return false;

    size_t playerStart = commandEnd;
    while (playerStart < text.size() && (text[playerStart] == ' ' || text[playerStart] == '\t' ||
        text[playerStart] == '\r' || text[playerStart] == '\n')) {
        ++playerStart;
    }
    if (playerStart >= text.size()) return false;

    size_t playerEnd = playerStart;
    while (playerEnd < text.size()) {
        char c = text[playerEnd];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') break;
        ++playerEnd;
    }
    if (playerEnd == playerStart) return false;

    size_t messageStart = playerEnd;
    while (messageStart < text.size() && (text[messageStart] == ' ' || text[messageStart] == '\t' ||
        text[messageStart] == '\r' || text[messageStart] == '\n')) {
        ++messageStart;
    }
    if (messageStart >= text.size()) return false;

    player = text.substr(playerStart, playerEnd - playerStart);
    privateMessage = TrimChatCommandWhitespace(text.substr(messageStart));
    return !player.empty() && !privateMessage.empty();
}

bool CloseMutedVoiceChatScreen(JNIEnv* env) {
    if (!env || !InitMutedVoiceChatJNI(env)) return false;

    jobject mc = env->CallStaticObjectMethod(
        g_mutedVoiceChatJNI.mcClass,
        g_mutedVoiceChatJNI.mGetMinecraft);
    if (!mc || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (mc) env->DeleteLocalRef(mc);
        return false;
    }

    env->CallVoidMethod(mc, g_mutedVoiceChatJNI.mDisplayGuiScreen, nullptr);
    bool ok = !env->ExceptionCheck();
    if (!ok) env->ExceptionClear();
    env->DeleteLocalRef(mc);
    return ok;
}

bool CaptureMutedVoiceChatOnEnter(WPARAM wParam, LPARAM lParam) {
    if (!g_guiExtrasMutedVoice || wParam != VK_RETURN) return false;
    if ((lParam & 0x40000000) != 0) return false;

    JNIEnv* env = GetJNIEnvForCurrentThread();
    if (!env) return false;

    std::string message;
    if (!ReadCurrentMutedVoiceChatInput(env, message)) return false;
    if (!IsMutedVoiceBotReroutedChatMessage(message)) return false;

    bool queued = QueueMutedVoiceChatMessage(message);
    if (!queued) return false;
    InterlockedExchange64(&g_mutedVoiceLastLocalChatAttemptMs, (LONG64)GetTickCount64());

    CloseMutedVoiceChatScreen(env);
    return true;
}

bool InitScoreboardJNI();
jobject GetWorldObject();
jobject GetScoreboardObjectFromWorld(jobject world);

bool IsSidebarDisplaySlot(jsize slot) {
    return slot == kScoreboardSidebarDisplaySlot ||
        (slot >= kScoreboardTeamSidebarDisplaySlotStart && slot <= kScoreboardTeamSidebarDisplaySlotEnd);
}

bool IsSameJavaObject(JNIEnv* env, jobject left, jobject right) {
    return env && left && right && env->IsSameObject(left, right) == JNI_TRUE;
}

bool ContainsJavaObject(JNIEnv* env, const std::vector<jobject>& objects, jobject candidate) {
    if (!env || !candidate) return false;
    for (jobject object : objects) {
        if (object && env->IsSameObject(object, candidate) == JNI_TRUE) return true;
    }
    return false;
}

void ClearHiddenSidebarSlotState(JNIEnv* env, jsize slot) {
    if (!env || slot < 0 || slot >= kScoreboardDisplaySlotCount) return;
    if (g_scoreboardJNI.hiddenSidebarObjectives[slot]) {
        env->DeleteGlobalRef(g_scoreboardJNI.hiddenSidebarObjectives[slot]);
        g_scoreboardJNI.hiddenSidebarObjectives[slot] = nullptr;
    }
    for (jobject hiddenScore : g_scoreboardJNI.hiddenSidebarScores[slot]) {
        if (hiddenScore) env->DeleteGlobalRef(hiddenScore);
    }
    g_scoreboardJNI.hiddenSidebarScores[slot].clear();
}

void ClearHiddenSidebarObjectiveState(JNIEnv* env) {
    if (!env) return;
    for (jsize slot = 0; slot < kScoreboardDisplaySlotCount; ++slot) {
        ClearHiddenSidebarSlotState(env, slot);
    }
    if (g_scoreboardJNI.hiddenSidebarScoreboard) {
        env->DeleteGlobalRef(g_scoreboardJNI.hiddenSidebarScoreboard);
        g_scoreboardJNI.hiddenSidebarScoreboard = nullptr;
    }
}

bool CaptureHiddenSidebarScoresForObjective(jobject scoreboard, jobject objective, std::vector<jobject>& storedScores) {
    if (!g_env || !scoreboard || !objective) return false;

    jobject scoresByEntity = g_env->GetObjectField(scoreboard, g_scoreboardJNI.fScoresByEntity);
    if (!scoresByEntity || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (scoresByEntity) g_env->DeleteLocalRef(scoresByEntity);
        return false;
    }

    jobject scoreMaps = g_env->CallObjectMethod(scoresByEntity, g_scoreboardJNI.mMapValues);
    if (!scoreMaps || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (scoreMaps) g_env->DeleteLocalRef(scoreMaps);
        g_env->DeleteLocalRef(scoresByEntity);
        return false;
    }

    jobjectArray entityScoreMaps = (jobjectArray)g_env->CallObjectMethod(scoreMaps, g_scoreboardJNI.mToArray);
    if (!entityScoreMaps || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (entityScoreMaps) g_env->DeleteLocalRef(entityScoreMaps);
        g_env->DeleteLocalRef(scoreMaps);
        g_env->DeleteLocalRef(scoresByEntity);
        return false;
    }

    jsize scoreMapCount = g_env->GetArrayLength(entityScoreMaps);
    for (jsize i = 0; i < scoreMapCount; ++i) {
        jobject entityScoreMap = g_env->GetObjectArrayElement(entityScoreMaps, i);
        if (!entityScoreMap || g_env->ExceptionCheck()) {
            g_env->ExceptionClear();
            if (entityScoreMap) g_env->DeleteLocalRef(entityScoreMap);
            continue;
        }

        jobject score = g_env->CallObjectMethod(entityScoreMap, g_scoreboardJNI.mMapGet, objective);
        if (g_env->ExceptionCheck()) {
            g_env->ExceptionClear();
            if (score) g_env->DeleteLocalRef(score);
            g_env->DeleteLocalRef(entityScoreMap);
            continue;
        }

        if (score) {
            jobject hiddenScore = g_env->NewGlobalRef(score);
            if (hiddenScore && !g_env->ExceptionCheck()) {
                storedScores.push_back(hiddenScore);
            } else {
                g_env->ExceptionClear();
                if (hiddenScore) g_env->DeleteGlobalRef(hiddenScore);
            }
            g_env->DeleteLocalRef(score);
        }

        g_env->DeleteLocalRef(entityScoreMap);
    }

    g_env->DeleteLocalRef(entityScoreMaps);
    g_env->DeleteLocalRef(scoreMaps);
    g_env->DeleteLocalRef(scoresByEntity);
    return true;
}

bool StoreHiddenSidebarObjectiveForSlot(jobject scoreboard, jsize slot, jobject objective) {
    if (!g_env || !scoreboard || !objective || slot < 0 || slot >= kScoreboardDisplaySlotCount) return false;

    if (IsSameJavaObject(g_env, g_scoreboardJNI.hiddenSidebarObjectives[slot], objective)) {
        return true;
    }

    ClearHiddenSidebarSlotState(g_env, slot);

    g_scoreboardJNI.hiddenSidebarObjectives[slot] = g_env->NewGlobalRef(objective);
    if (!g_scoreboardJNI.hiddenSidebarObjectives[slot] || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (g_scoreboardJNI.hiddenSidebarObjectives[slot]) {
            g_env->DeleteGlobalRef(g_scoreboardJNI.hiddenSidebarObjectives[slot]);
            g_scoreboardJNI.hiddenSidebarObjectives[slot] = nullptr;
        }
        return false;
    }

    return true;
}

bool RegisterStoredScoreboardObjective(jobject scoreboard, jobject objective) {
    if (!g_env || !scoreboard || !objective) return false;

    jobject objectivesByName = g_env->GetObjectField(scoreboard, g_scoreboardJNI.fObjectivesByName);
    jobject objectivesByCriteria = g_env->GetObjectField(scoreboard, g_scoreboardJNI.fObjectivesByCriteria);
    if (!objectivesByName || !objectivesByCriteria || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (objectivesByCriteria) g_env->DeleteLocalRef(objectivesByCriteria);
        if (objectivesByName) g_env->DeleteLocalRef(objectivesByName);
        return false;
    }

    jstring objectiveName = (jstring)g_env->GetObjectField(objective, g_scoreboardJNI.fObjectiveName);
    jobject objectiveCriteria = g_env->GetObjectField(objective, g_scoreboardJNI.fObjectiveCriteria);
    if (!objectiveName || !objectiveCriteria || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (objectiveCriteria) g_env->DeleteLocalRef(objectiveCriteria);
        if (objectiveName) g_env->DeleteLocalRef(objectiveName);
        g_env->DeleteLocalRef(objectivesByCriteria);
        g_env->DeleteLocalRef(objectivesByName);
        return false;
    }

    jobject existingObjective = g_env->CallObjectMethod(objectivesByName, g_scoreboardJNI.mMapGet, objectiveName);
    if (g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (existingObjective) g_env->DeleteLocalRef(existingObjective);
        g_env->DeleteLocalRef(objectiveCriteria);
        g_env->DeleteLocalRef(objectiveName);
        g_env->DeleteLocalRef(objectivesByCriteria);
        g_env->DeleteLocalRef(objectivesByName);
        return false;
    }

    if (existingObjective && !IsSameJavaObject(g_env, existingObjective, objective)) {
        g_env->CallVoidMethod(scoreboard, g_scoreboardJNI.mScoreboardRemoveObjective, existingObjective);
        if (g_env->ExceptionCheck()) g_env->ExceptionClear();
    }
    if (existingObjective) g_env->DeleteLocalRef(existingObjective);

    jobject previousByName = g_env->CallObjectMethod(objectivesByName, g_scoreboardJNI.mMapPut, objectiveName, objective);
    if (g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (previousByName) g_env->DeleteLocalRef(previousByName);
        g_env->DeleteLocalRef(objectiveCriteria);
        g_env->DeleteLocalRef(objectiveName);
        g_env->DeleteLocalRef(objectivesByCriteria);
        g_env->DeleteLocalRef(objectivesByName);
        return false;
    }
    if (previousByName) g_env->DeleteLocalRef(previousByName);

    jobject criteriaObjectives = g_env->CallObjectMethod(objectivesByCriteria, g_scoreboardJNI.mMapGet, objectiveCriteria);
    if (g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (criteriaObjectives) g_env->DeleteLocalRef(criteriaObjectives);
        g_env->DeleteLocalRef(objectiveCriteria);
        g_env->DeleteLocalRef(objectiveName);
        g_env->DeleteLocalRef(objectivesByCriteria);
        g_env->DeleteLocalRef(objectivesByName);
        return false;
    }

    if (!criteriaObjectives) {
        criteriaObjectives = g_env->NewObject(g_scoreboardJNI.arrayListClass, g_scoreboardJNI.mArrayListCtor);
        if (!criteriaObjectives || g_env->ExceptionCheck()) {
            g_env->ExceptionClear();
            if (criteriaObjectives) g_env->DeleteLocalRef(criteriaObjectives);
            g_env->DeleteLocalRef(objectiveCriteria);
            g_env->DeleteLocalRef(objectiveName);
            g_env->DeleteLocalRef(objectivesByCriteria);
            g_env->DeleteLocalRef(objectivesByName);
            return false;
        }

        jobject previousByCriteria = g_env->CallObjectMethod(objectivesByCriteria, g_scoreboardJNI.mMapPut, objectiveCriteria, criteriaObjectives);
        if (g_env->ExceptionCheck()) g_env->ExceptionClear();
        if (previousByCriteria) g_env->DeleteLocalRef(previousByCriteria);
    }

    jboolean containsObjective = g_env->CallBooleanMethod(criteriaObjectives, g_scoreboardJNI.mCollectionContains, objective);
    if (g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
    } else if (containsObjective != JNI_TRUE) {
        g_env->CallBooleanMethod(criteriaObjectives, g_scoreboardJNI.mListAdd, objective);
        if (g_env->ExceptionCheck()) g_env->ExceptionClear();
    }

    g_env->CallVoidMethod(scoreboard, g_scoreboardJNI.mScoreboardOnObjectiveAdded, objective);
    if (g_env->ExceptionCheck()) g_env->ExceptionClear();

    g_env->DeleteLocalRef(criteriaObjectives);
    g_env->DeleteLocalRef(objectiveCriteria);
    g_env->DeleteLocalRef(objectiveName);
    g_env->DeleteLocalRef(objectivesByCriteria);
    g_env->DeleteLocalRef(objectivesByName);
    return true;
}

bool RestoreStoredSidebarScores(jobject scoreboard, const std::vector<jobject>& storedScores) {
    if (!g_env || !scoreboard) return false;

    jobject scoresByEntity = g_env->GetObjectField(scoreboard, g_scoreboardJNI.fScoresByEntity);
    if (!scoresByEntity || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (scoresByEntity) g_env->DeleteLocalRef(scoresByEntity);
        return false;
    }

    for (jobject score : storedScores) {
        if (!score) continue;

        jstring entityName = (jstring)g_env->GetObjectField(score, g_scoreboardJNI.fScoreEntityName);
        jobject objective = g_env->GetObjectField(score, g_scoreboardJNI.fScoreObjective);
        if (!entityName || !objective || g_env->ExceptionCheck()) {
            g_env->ExceptionClear();
            if (objective) g_env->DeleteLocalRef(objective);
            if (entityName) g_env->DeleteLocalRef(entityName);
            continue;
        }

        jobject entityScores = g_env->CallObjectMethod(scoresByEntity, g_scoreboardJNI.mMapGet, entityName);
        if (g_env->ExceptionCheck()) {
            g_env->ExceptionClear();
            if (entityScores) g_env->DeleteLocalRef(entityScores);
            g_env->DeleteLocalRef(objective);
            g_env->DeleteLocalRef(entityName);
            continue;
        }

        if (!entityScores) {
            entityScores = g_env->NewObject(g_scoreboardJNI.hashMapClass, g_scoreboardJNI.mHashMapCtor);
            if (!entityScores || g_env->ExceptionCheck()) {
                g_env->ExceptionClear();
                if (entityScores) g_env->DeleteLocalRef(entityScores);
                g_env->DeleteLocalRef(objective);
                g_env->DeleteLocalRef(entityName);
                continue;
            }

            jobject previousEntityScores = g_env->CallObjectMethod(scoresByEntity, g_scoreboardJNI.mMapPut, entityName, entityScores);
            if (g_env->ExceptionCheck()) g_env->ExceptionClear();
            if (previousEntityScores) g_env->DeleteLocalRef(previousEntityScores);
        }

        jobject previousScore = g_env->CallObjectMethod(entityScores, g_scoreboardJNI.mMapPut, objective, score);
        if (g_env->ExceptionCheck()) g_env->ExceptionClear();
        if (previousScore) g_env->DeleteLocalRef(previousScore);

        g_env->CallVoidMethod(scoreboard, g_scoreboardJNI.mScoreboardOnScoreUpdated, score);
        if (g_env->ExceptionCheck()) g_env->ExceptionClear();

        g_env->DeleteLocalRef(entityScores);
        g_env->DeleteLocalRef(objective);
        g_env->DeleteLocalRef(entityName);
    }

    g_env->DeleteLocalRef(scoresByEntity);
    return true;
}

bool ResolveScoreboardDisplayObjectivesField(jobject scoreboard) {
    if (!g_env || !scoreboard) return false;
    if (g_scoreboardJNI.fDisplayObjectives) return true;

    jclass scoreboardClass = g_env->GetObjectClass(scoreboard);
    jclass classClass = FindClassLoose(g_env, "java/lang/Class");
    jclass fieldClass = FindClassLoose(g_env, "java/lang/reflect/Field");
    jclass modifierClass = FindClassLoose(g_env, "java/lang/reflect/Modifier");
    if (!scoreboardClass || !classClass || !fieldClass || !modifierClass || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (modifierClass) g_env->DeleteLocalRef(modifierClass);
        if (fieldClass) g_env->DeleteLocalRef(fieldClass);
        if (classClass) g_env->DeleteLocalRef(classClass);
        if (scoreboardClass) g_env->DeleteLocalRef(scoreboardClass);
        return false;
    }

    jmethodID mClassGetDeclaredFields = GetMethodIDCompat(g_env, classClass, "getDeclaredFields", "()[Ljava/lang/reflect/Field;");
    jmethodID mClassIsArray = GetMethodIDCompat(g_env, classClass, "isArray", "()Z");
    jmethodID mClassGetComponentType = GetMethodIDCompat(g_env, classClass, "getComponentType", "()Ljava/lang/Class;");
    jmethodID mClassIsPrimitive = GetMethodIDCompat(g_env, classClass, "isPrimitive", "()Z");
    jmethodID mFieldGetType = GetMethodIDCompat(g_env, fieldClass, "getType", "()Ljava/lang/Class;");
    jmethodID mFieldGetModifiers = GetMethodIDCompat(g_env, fieldClass, "getModifiers", "()I");
    jmethodID mFieldSetAccessible = GetMethodIDCompat(g_env, fieldClass, "setAccessible", "(Z)V");
    jmethodID mFieldGet = GetMethodIDCompat(g_env, fieldClass, "get", "(Ljava/lang/Object;)Ljava/lang/Object;");
    jmethodID mModifierIsStatic = GetStaticMethodIDCompat(g_env, modifierClass, "isStatic", "(I)Z");
    bool methodsReady =
        mClassGetDeclaredFields && mClassIsArray && mClassGetComponentType && mClassIsPrimitive &&
        mFieldGetType && mFieldGetModifiers && mFieldSetAccessible && mFieldGet && mModifierIsStatic;
    if (!methodsReady || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        g_env->DeleteLocalRef(modifierClass);
        g_env->DeleteLocalRef(fieldClass);
        g_env->DeleteLocalRef(classClass);
        g_env->DeleteLocalRef(scoreboardClass);
        return false;
    }

    jobjectArray fields = (jobjectArray)g_env->CallObjectMethod(scoreboardClass, mClassGetDeclaredFields);
    if (!fields || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (fields) g_env->DeleteLocalRef(fields);
        g_env->DeleteLocalRef(modifierClass);
        g_env->DeleteLocalRef(fieldClass);
        g_env->DeleteLocalRef(classClass);
        g_env->DeleteLocalRef(scoreboardClass);
        return false;
    }

    jsize fieldCount = g_env->GetArrayLength(fields);
    for (jsize i = 0; i < fieldCount; ++i) {
        jobject field = g_env->GetObjectArrayElement(fields, i);
        if (!field) continue;

        jint modifiers = g_env->CallIntMethod(field, mFieldGetModifiers);
        if (g_env->ExceptionCheck()) {
            g_env->ExceptionClear();
            g_env->DeleteLocalRef(field);
            continue;
        }

        jboolean isStatic = g_env->CallStaticBooleanMethod(modifierClass, mModifierIsStatic, modifiers);
        if (g_env->ExceptionCheck()) {
            g_env->ExceptionClear();
            g_env->DeleteLocalRef(field);
            continue;
        }
        if (isStatic == JNI_TRUE) {
            g_env->DeleteLocalRef(field);
            continue;
        }

        g_env->CallVoidMethod(field, mFieldSetAccessible, JNI_TRUE);
        if (g_env->ExceptionCheck()) {
            g_env->ExceptionClear();
            g_env->DeleteLocalRef(field);
            continue;
        }

        jobject fieldType = g_env->CallObjectMethod(field, mFieldGetType);
        if (!fieldType || g_env->ExceptionCheck()) {
            g_env->ExceptionClear();
            if (fieldType) g_env->DeleteLocalRef(fieldType);
            g_env->DeleteLocalRef(field);
            continue;
        }

        jboolean isArray = g_env->CallBooleanMethod(fieldType, mClassIsArray);
        if (g_env->ExceptionCheck()) {
            g_env->ExceptionClear();
            g_env->DeleteLocalRef(fieldType);
            g_env->DeleteLocalRef(field);
            continue;
        }
        if (isArray != JNI_TRUE) {
            g_env->DeleteLocalRef(fieldType);
            g_env->DeleteLocalRef(field);
            continue;
        }

        jobject componentType = g_env->CallObjectMethod(fieldType, mClassGetComponentType);
        if (!componentType || g_env->ExceptionCheck()) {
            g_env->ExceptionClear();
            if (componentType) g_env->DeleteLocalRef(componentType);
            g_env->DeleteLocalRef(fieldType);
            g_env->DeleteLocalRef(field);
            continue;
        }

        jboolean isPrimitive = g_env->CallBooleanMethod(componentType, mClassIsPrimitive);
        if (g_env->ExceptionCheck()) {
            g_env->ExceptionClear();
            g_env->DeleteLocalRef(componentType);
            g_env->DeleteLocalRef(fieldType);
            g_env->DeleteLocalRef(field);
            continue;
        }
        if (isPrimitive == JNI_TRUE) {
            g_env->DeleteLocalRef(componentType);
            g_env->DeleteLocalRef(fieldType);
            g_env->DeleteLocalRef(field);
            continue;
        }

        jobject arrayValue = g_env->CallObjectMethod(field, mFieldGet, scoreboard);
        if (!arrayValue || g_env->ExceptionCheck()) {
            g_env->ExceptionClear();
            if (arrayValue) g_env->DeleteLocalRef(arrayValue);
            g_env->DeleteLocalRef(componentType);
            g_env->DeleteLocalRef(fieldType);
            g_env->DeleteLocalRef(field);
            continue;
        }

        jobjectArray objectiveArray = (jobjectArray)arrayValue;
        jsize arrayLength = g_env->GetArrayLength(objectiveArray);
        if (g_env->ExceptionCheck()) {
            g_env->ExceptionClear();
            g_env->DeleteLocalRef(arrayValue);
            g_env->DeleteLocalRef(componentType);
            g_env->DeleteLocalRef(fieldType);
            g_env->DeleteLocalRef(field);
            continue;
        }

        if (arrayLength >= kScoreboardDisplaySlotCount) {
            if (!g_scoreboardJNI.objectiveClass) {
                g_scoreboardJNI.objectiveClass = (jclass)g_env->NewGlobalRef(componentType);
                if (!g_scoreboardJNI.objectiveClass && g_env->ExceptionCheck()) g_env->ExceptionClear();
            }
            g_scoreboardJNI.fDisplayObjectives = g_env->FromReflectedField(field);
            g_env->DeleteLocalRef(arrayValue);
            g_env->DeleteLocalRef(componentType);
            g_env->DeleteLocalRef(fieldType);
            g_env->DeleteLocalRef(field);
            break;
        }

        g_env->DeleteLocalRef(arrayValue);
        g_env->DeleteLocalRef(componentType);
        g_env->DeleteLocalRef(fieldType);
        g_env->DeleteLocalRef(field);
    }

    g_env->DeleteLocalRef(fields);
    g_env->DeleteLocalRef(modifierClass);
    g_env->DeleteLocalRef(fieldClass);
    g_env->DeleteLocalRef(classClass);
    g_env->DeleteLocalRef(scoreboardClass);
    return g_scoreboardJNI.fDisplayObjectives != nullptr;
}

jobjectArray GetScoreboardDisplayObjectiveArray(jobject scoreboard) {
    if (!g_env || !scoreboard) return nullptr;
    if (!ResolveScoreboardDisplayObjectivesField(scoreboard)) return nullptr;

    jobjectArray displayObjectives = (jobjectArray)g_env->GetObjectField(scoreboard, g_scoreboardJNI.fDisplayObjectives);
    if (!displayObjectives || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (displayObjectives) g_env->DeleteLocalRef(displayObjectives);
        return nullptr;
    }
    return displayObjectives;
}

bool RestoreStoredTagScoreboardVisibility() {
    if (!g_env || !InitScoreboardJNI() || !g_scoreboardJNI.hiddenSidebarScoreboard) return false;

    jobjectArray displayObjectives = GetScoreboardDisplayObjectiveArray(g_scoreboardJNI.hiddenSidebarScoreboard);
    if (!displayObjectives) {
        DebugLog("Tag scoreboard restore skipped: no display objective array");
        return false;
    }

    int restoredSlots = 0;
    int skippedSlots = 0;
    for (jsize slot = 0; slot < kScoreboardDisplaySlotCount; ++slot) {
        if (!IsSidebarDisplaySlot(slot)) continue;

        jobject storedObjective = g_scoreboardJNI.hiddenSidebarObjectives[slot];
        if (!storedObjective) continue;

        jobject currentObjective = g_env->GetObjectArrayElement(displayObjectives, slot);
        if (g_env->ExceptionCheck()) {
            g_env->ExceptionClear();
            if (currentObjective) g_env->DeleteLocalRef(currentObjective);
            skippedSlots++;
            continue;
        }

        if (currentObjective) {
            g_env->DeleteLocalRef(currentObjective);
            skippedSlots++;
            continue;
        }

        g_env->CallVoidMethod(
            g_scoreboardJNI.hiddenSidebarScoreboard,
            g_scoreboardJNI.mScoreboardSetObjectiveInDisplaySlot,
            (jint)slot,
            storedObjective);
        if (g_env->ExceptionCheck()) {
            g_env->ExceptionClear();
            skippedSlots++;
        }
        else {
            restoredSlots++;
        }
    }

    g_env->DeleteLocalRef(displayObjectives);
    DebugLog("Tag scoreboard restore restoredSlots=%d skippedSlots=%d", restoredSlots, skippedSlots);
    ClearHiddenSidebarObjectiveState(g_env);
    return true;
}

void UpdateTagScoreboardVisibility(bool shouldHide) {
    if (!g_env || !InitScoreboardJNI()) return;

    if (!shouldHide) {
        RestoreStoredTagScoreboardVisibility();
        return;
    }

    jobject world = GetWorldObject();
    if (!world) return;

    jobject scoreboard = GetScoreboardObjectFromWorld(world);
    g_env->DeleteLocalRef(world);
    if (!scoreboard) return;

    if (g_scoreboardJNI.hiddenSidebarScoreboard &&
        !g_env->IsSameObject(scoreboard, g_scoreboardJNI.hiddenSidebarScoreboard)) {
        ClearHiddenSidebarObjectiveState(g_env);
    }

    jobjectArray displayObjectives = GetScoreboardDisplayObjectiveArray(scoreboard);
    if (!displayObjectives) {
        g_env->DeleteLocalRef(scoreboard);
        return;
    }

    if (!g_scoreboardJNI.hiddenSidebarScoreboard) {
        g_scoreboardJNI.hiddenSidebarScoreboard = g_env->NewGlobalRef(scoreboard);
    }

    int storedSlots = 0;
    int hiddenSlots = 0;
    for (jsize slot = 0; slot < kScoreboardDisplaySlotCount; ++slot) {
        if (!IsSidebarDisplaySlot(slot)) continue;

        jobject currentSidebarObjective = g_env->GetObjectArrayElement(displayObjectives, slot);
        if (g_env->ExceptionCheck()) {
            g_env->ExceptionClear();
            if (currentSidebarObjective) g_env->DeleteLocalRef(currentSidebarObjective);
            continue;
        }

        if (currentSidebarObjective) {
            if (StoreHiddenSidebarObjectiveForSlot(scoreboard, slot, currentSidebarObjective)) {
                storedSlots++;
            }
            g_env->CallVoidMethod(
                scoreboard,
                g_scoreboardJNI.mScoreboardSetObjectiveInDisplaySlot,
                (jint)slot,
                nullptr);
            if (g_env->ExceptionCheck()) {
                g_env->ExceptionClear();
            }
            else {
                hiddenSlots++;
            }
            g_env->DeleteLocalRef(currentSidebarObjective);
        }
    }

    if (hiddenSlots > 0) {
        DebugLog("Tag scoreboard hide hiddenSlots=%d storedSlots=%d", hiddenSlots, storedSlots);
    }
    g_env->DeleteLocalRef(displayObjectives);
    g_env->DeleteLocalRef(scoreboard);
}

bool InitScoreboardJNI() {
    if (g_scoreboardJNI.failed) {
        g_scoreboardJNI.failed = false;
        g_scoreboardJNI.inited = false;
    }
    if (g_scoreboardJNI.inited) return true;
    if (!g_env) return false;

    g_scoreboardJNI.inited = true;

    jclass mcClassLocal = FindClassLoose(g_env, "ave");
    if (!mcClassLocal) { g_env->ExceptionClear(); g_scoreboardJNI.failed = true; return false; }
    g_scoreboardJNI.mcClass = (jclass)g_env->NewGlobalRef(mcClassLocal);
    g_env->DeleteLocalRef(mcClassLocal);
    if (!g_scoreboardJNI.mcClass) { g_env->ExceptionClear(); g_scoreboardJNI.failed = true; return false; }

    g_scoreboardJNI.mGetMC = GetStaticMethodIDCompat(g_env, g_scoreboardJNI.mcClass, "A", "()Lave;");
    if (!g_scoreboardJNI.mGetMC) { g_env->ExceptionClear(); g_scoreboardJNI.failed = true; return false; }
    g_scoreboardJNI.mMinecraftGetNetHandler = GetMethodIDCompat(g_env, g_scoreboardJNI.mcClass, "u", "()Lbcy;");
    if (!g_scoreboardJNI.mMinecraftGetNetHandler) { g_env->ExceptionClear(); g_scoreboardJNI.failed = true; return false; }

    g_scoreboardJNI.fWorldField = GetFieldIDCompat(g_env, g_scoreboardJNI.mcClass, "f", "Lbdb;");
    if (!g_scoreboardJNI.fWorldField) { g_env->ExceptionClear(); g_scoreboardJNI.failed = true; return false; }

    jclass bdbClass = FindClassLoose(g_env, "bdb");
    if (!bdbClass) { g_env->ExceptionClear(); g_scoreboardJNI.failed = true; return false; }
    jclass classClass = FindClassLoose(g_env, "java/lang/Class");
    if (!classClass) {
        g_env->ExceptionClear();
        g_env->DeleteLocalRef(bdbClass);
        g_scoreboardJNI.failed = true;
        return false;
    }

    jmethodID getSuperclass = GetMethodIDCompat(g_env, classClass, "getSuperclass", "()Ljava/lang/Class;");
    if (!getSuperclass) {
        g_env->ExceptionClear();
        g_env->DeleteLocalRef(classClass);
        g_env->DeleteLocalRef(bdbClass);
        g_scoreboardJNI.failed = true;
        return false;
    }

    jclass admClass = (jclass)g_env->CallObjectMethod(bdbClass, getSuperclass);
    g_env->DeleteLocalRef(classClass);
    g_env->DeleteLocalRef(bdbClass);
    if (!admClass || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        g_scoreboardJNI.failed = true;
        return false;
    }

    g_scoreboardJNI.fSbField = GetFieldIDCompat(g_env, admClass, "C", "Lauo;");
    if (!g_scoreboardJNI.fSbField) { g_env->ExceptionClear(); g_env->DeleteLocalRef(admClass); g_scoreboardJNI.failed = true; return false; }
    g_scoreboardJNI.fPlayerList = GetFieldIDCompat(g_env, admClass, "j", "Ljava/util/List;");
    g_env->DeleteLocalRef(admClass);
    if (!g_scoreboardJNI.fPlayerList) { g_env->ExceptionClear(); g_scoreboardJNI.failed = true; return false; }

    jclass auoClass = FindClassLoose(g_env, "auo");
    if (!auoClass) { g_env->ExceptionClear(); g_scoreboardJNI.failed = true; return false; }
    g_scoreboardJNI.fObjectivesByName = GetFieldIDCompat(g_env, auoClass, "a", "Ljava/util/Map;");
    if (!g_scoreboardJNI.fObjectivesByName) { g_env->ExceptionClear(); g_env->DeleteLocalRef(auoClass); g_scoreboardJNI.failed = true; return false; }
    g_scoreboardJNI.fObjectivesByCriteria = GetFieldIDCompat(g_env, auoClass, "b", "Ljava/util/Map;");
    if (!g_scoreboardJNI.fObjectivesByCriteria) { g_env->ExceptionClear(); g_env->DeleteLocalRef(auoClass); g_scoreboardJNI.failed = true; return false; }
    g_scoreboardJNI.fScoresByEntity = GetFieldIDCompat(g_env, auoClass, "c", "Ljava/util/Map;");
    if (!g_scoreboardJNI.fScoresByEntity) { g_env->ExceptionClear(); g_env->DeleteLocalRef(auoClass); g_scoreboardJNI.failed = true; return false; }
    g_scoreboardJNI.fDisplayObjectives = GetFieldIDCompat(g_env, auoClass, "d", "[Lauk;");
    if (!g_scoreboardJNI.fDisplayObjectives) { g_env->ExceptionClear(); g_env->DeleteLocalRef(auoClass); g_scoreboardJNI.failed = true; return false; }
    g_scoreboardJNI.fMapE = GetFieldIDCompat(g_env, auoClass, "e", "Ljava/util/Map;");
    if (!g_scoreboardJNI.fMapE) { g_env->ExceptionClear(); g_env->DeleteLocalRef(auoClass); g_scoreboardJNI.failed = true; return false; }
    g_scoreboardJNI.mScoreboardRemoveObjective = GetMethodIDCompat(g_env, auoClass, "k", "(Lauk;)V");
    if (!g_scoreboardJNI.mScoreboardRemoveObjective) {
        g_env->ExceptionClear();
        DebugLog("Scoreboard remove-objective method unavailable; using display-slot hide only");
    }
    g_scoreboardJNI.mScoreboardOnObjectiveAdded = GetMethodIDCompat(g_env, auoClass, "a", "(Lauk;)V");
    if (!g_scoreboardJNI.mScoreboardOnObjectiveAdded) { g_env->ExceptionClear(); g_env->DeleteLocalRef(auoClass); g_scoreboardJNI.failed = true; return false; }
    g_scoreboardJNI.mScoreboardOnScoreUpdated = GetMethodIDCompat(g_env, auoClass, "a", "(Laum;)V");
    if (!g_scoreboardJNI.mScoreboardOnScoreUpdated) { g_env->ExceptionClear(); g_env->DeleteLocalRef(auoClass); g_scoreboardJNI.failed = true; return false; }
    g_scoreboardJNI.mScoreboardSetObjectiveInDisplaySlot = GetMethodIDCompat(g_env, auoClass, "a", "(ILauk;)V");
    if (!g_scoreboardJNI.mScoreboardSetObjectiveInDisplaySlot) { g_env->ExceptionClear(); g_env->DeleteLocalRef(auoClass); g_scoreboardJNI.failed = true; return false; }
    g_scoreboardJNI.mScoreboardGetTeam = GetMethodIDCompat(g_env, auoClass, "d", "(Ljava/lang/String;)Laul;");
    if (!g_scoreboardJNI.mScoreboardGetTeam) { g_env->ExceptionClear(); g_env->DeleteLocalRef(auoClass); g_scoreboardJNI.failed = true; return false; }
    g_scoreboardJNI.mScoreboardCreateTeam = GetMethodIDCompat(g_env, auoClass, "e", "(Ljava/lang/String;)Laul;");
    if (!g_scoreboardJNI.mScoreboardCreateTeam) { g_env->ExceptionClear(); g_env->DeleteLocalRef(auoClass); g_scoreboardJNI.failed = true; return false; }
    g_scoreboardJNI.mScoreboardGetPlayersTeam = GetMethodIDCompat(g_env, auoClass, "h", "(Ljava/lang/String;)Laul;");
    if (!g_scoreboardJNI.mScoreboardGetPlayersTeam) { g_env->ExceptionClear(); g_env->DeleteLocalRef(auoClass); g_scoreboardJNI.failed = true; return false; }
    g_scoreboardJNI.mScoreboardAddPlayerToTeam = GetMethodIDCompat(g_env, auoClass, "a", "(Ljava/lang/String;Ljava/lang/String;)Z");
    if (!g_scoreboardJNI.mScoreboardAddPlayerToTeam) { g_env->ExceptionClear(); g_env->DeleteLocalRef(auoClass); g_scoreboardJNI.failed = true; return false; }
    g_scoreboardJNI.mScoreboardRemovePlayerFromTeams = GetMethodIDCompat(g_env, auoClass, "f", "(Ljava/lang/String;)Z");
    if (!g_scoreboardJNI.mScoreboardRemovePlayerFromTeams) { g_env->ExceptionClear(); g_env->DeleteLocalRef(auoClass); g_scoreboardJNI.failed = true; return false; }
    g_scoreboardJNI.mScoreboardRemoveTeam = GetMethodIDCompat(g_env, auoClass, "d", "(Laul;)V");
    g_env->DeleteLocalRef(auoClass);
    if (!g_scoreboardJNI.mScoreboardRemoveTeam) { g_env->ExceptionClear(); g_scoreboardJNI.failed = true; return false; }

    jclass aukClass = FindClassLoose(g_env, "auk");
    if (!aukClass) { g_env->ExceptionClear(); g_scoreboardJNI.failed = true; return false; }
    g_scoreboardJNI.objectiveClass = (jclass)g_env->NewGlobalRef(aukClass);
    if (!g_scoreboardJNI.objectiveClass || g_env->ExceptionCheck()) { g_env->ExceptionClear(); g_env->DeleteLocalRef(aukClass); g_scoreboardJNI.failed = true; return false; }
    g_scoreboardJNI.fObjectiveName = GetFieldIDCompat(g_env, aukClass, "b", "Ljava/lang/String;");
    if (!g_scoreboardJNI.fObjectiveName) { g_env->ExceptionClear(); g_env->DeleteLocalRef(aukClass); g_scoreboardJNI.failed = true; return false; }
    g_scoreboardJNI.fObjectiveCriteria = GetFieldIDCompat(g_env, aukClass, "c", "Lauu;");
    if (!g_scoreboardJNI.fObjectiveCriteria) { g_env->ExceptionClear(); g_env->DeleteLocalRef(aukClass); g_scoreboardJNI.failed = true; return false; }
    // 1.8.9 ScoreObjective: d is the render-type enum; e is the visible title.
    g_scoreboardJNI.fObjectiveDisplayName = GetFieldIDCompat(g_env, aukClass, "e", "Ljava/lang/String;");
    if (!g_scoreboardJNI.fObjectiveDisplayName) g_env->ExceptionClear();
    g_env->DeleteLocalRef(aukClass);

    jclass aumClass = FindClassLoose(g_env, "aum");
    if (!aumClass) { g_env->ExceptionClear(); g_scoreboardJNI.failed = true; return false; }
    g_scoreboardJNI.scoreClass = (jclass)g_env->NewGlobalRef(aumClass);
    if (!g_scoreboardJNI.scoreClass || g_env->ExceptionCheck()) { g_env->ExceptionClear(); g_env->DeleteLocalRef(aumClass); g_scoreboardJNI.failed = true; return false; }
    g_scoreboardJNI.fScoreObjective = GetFieldIDCompat(g_env, aumClass, "c", "Lauk;");
    if (!g_scoreboardJNI.fScoreObjective) { g_env->ExceptionClear(); g_env->DeleteLocalRef(aumClass); g_scoreboardJNI.failed = true; return false; }
    g_scoreboardJNI.fScoreEntityName = GetFieldIDCompat(g_env, aumClass, "d", "Ljava/lang/String;");
    g_env->DeleteLocalRef(aumClass);
    if (!g_scoreboardJNI.fScoreEntityName) { g_env->ExceptionClear(); g_scoreboardJNI.failed = true; return false; }

    jclass aulClass = FindClassLoose(g_env, "aul");
    if (!aulClass) { g_env->ExceptionClear(); g_scoreboardJNI.failed = true; return false; }
    g_scoreboardJNI.fTeamName = GetFieldIDCompat(g_env, aulClass, "b", "Ljava/lang/String;");
    if (!g_scoreboardJNI.fTeamName) { g_env->ExceptionClear(); g_env->DeleteLocalRef(aulClass); g_scoreboardJNI.failed = true; return false; }
    g_scoreboardJNI.fTeamMembers = GetFieldIDCompat(g_env, aulClass, "c", "Ljava/util/Set;");
    if (!g_scoreboardJNI.fTeamMembers) { g_env->ExceptionClear(); g_env->DeleteLocalRef(aulClass); g_scoreboardJNI.failed = true; return false; }
    g_scoreboardJNI.fTeamDisplayName = GetFieldIDCompat(g_env, aulClass, "d", "Ljava/lang/String;");
    if (!g_scoreboardJNI.fTeamDisplayName) g_env->ExceptionClear();
    g_scoreboardJNI.fTeamE = GetFieldIDCompat(g_env, aulClass, "e", "Ljava/lang/String;");
    if (!g_scoreboardJNI.fTeamE) { g_env->ExceptionClear(); g_env->DeleteLocalRef(aulClass); g_scoreboardJNI.failed = true; return false; }
    g_scoreboardJNI.fTeamF = GetFieldIDCompat(g_env, aulClass, "f", "Ljava/lang/String;");
    if (!g_scoreboardJNI.fTeamF) { g_env->ExceptionClear(); g_scoreboardJNI.failed = true; return false; }
    g_scoreboardJNI.fTeamFriendlyFire = GetFieldIDCompat(g_env, aulClass, "g", "Z");
    if (!g_scoreboardJNI.fTeamFriendlyFire) g_env->ExceptionClear();
    g_scoreboardJNI.fTeamSeeFriendlyInvisibles = GetFieldIDCompat(g_env, aulClass, "h", "Z");
    if (!g_scoreboardJNI.fTeamSeeFriendlyInvisibles) g_env->ExceptionClear();
    g_scoreboardJNI.fTeamNameTagVisibility = GetFieldIDCompat(g_env, aulClass, "i", "Lauq$a;");
    if (!g_scoreboardJNI.fTeamNameTagVisibility) g_env->ExceptionClear();
    g_scoreboardJNI.fTeamDeathMessageVisibility = GetFieldIDCompat(g_env, aulClass, "j", "Lauq$a;");
    if (!g_scoreboardJNI.fTeamDeathMessageVisibility) g_env->ExceptionClear();
    g_scoreboardJNI.fTeamColor = GetFieldIDCompat(g_env, aulClass, "k", "La;");
    if (!g_scoreboardJNI.fTeamColor) g_env->ExceptionClear();
    g_env->DeleteLocalRef(aulClass);

    jclass mapClass = FindClassLoose(g_env, "java/util/Map");
    if (!mapClass) { g_env->ExceptionClear(); g_scoreboardJNI.failed = true; return false; }
    g_scoreboardJNI.mMapGet = GetMethodIDCompat(g_env, mapClass, "get", "(Ljava/lang/Object;)Ljava/lang/Object;");
    if (!g_scoreboardJNI.mMapGet) { g_env->ExceptionClear(); g_env->DeleteLocalRef(mapClass); g_scoreboardJNI.failed = true; return false; }
    g_scoreboardJNI.mMapPut = GetMethodIDCompat(g_env, mapClass, "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    if (!g_scoreboardJNI.mMapPut) { g_env->ExceptionClear(); g_env->DeleteLocalRef(mapClass); g_scoreboardJNI.failed = true; return false; }
    g_scoreboardJNI.mMapValues = GetMethodIDCompat(g_env, mapClass, "values", "()Ljava/util/Collection;");
    g_env->DeleteLocalRef(mapClass);
    if (!g_scoreboardJNI.mMapValues) { g_env->ExceptionClear(); g_scoreboardJNI.failed = true; return false; }

    jclass collClass = FindClassLoose(g_env, "java/util/Collection");
    if (!collClass) { g_env->ExceptionClear(); g_scoreboardJNI.failed = true; return false; }
    g_scoreboardJNI.mCollectionContains = GetMethodIDCompat(g_env, collClass, "contains", "(Ljava/lang/Object;)Z");
    if (!g_scoreboardJNI.mCollectionContains) { g_env->ExceptionClear(); g_env->DeleteLocalRef(collClass); g_scoreboardJNI.failed = true; return false; }
    g_scoreboardJNI.mToArray = GetMethodIDCompat(g_env, collClass, "toArray", "()[Ljava/lang/Object;");
    g_env->DeleteLocalRef(collClass);
    if (!g_scoreboardJNI.mToArray) { g_env->ExceptionClear(); g_scoreboardJNI.failed = true; return false; }

    jclass listClass = FindClassLoose(g_env, "java/util/List");
    if (!listClass) { g_env->ExceptionClear(); g_scoreboardJNI.failed = true; return false; }
    g_scoreboardJNI.mListAdd = GetMethodIDCompat(g_env, listClass, "add", "(Ljava/lang/Object;)Z");
    g_env->DeleteLocalRef(listClass);
    if (!g_scoreboardJNI.mListAdd) { g_env->ExceptionClear(); g_scoreboardJNI.failed = true; return false; }

    jclass hashMapClass = FindClassLoose(g_env, "java/util/HashMap");
    if (!hashMapClass) { g_env->ExceptionClear(); g_scoreboardJNI.failed = true; return false; }
    g_scoreboardJNI.hashMapClass = (jclass)g_env->NewGlobalRef(hashMapClass);
    if (!g_scoreboardJNI.hashMapClass || g_env->ExceptionCheck()) { g_env->ExceptionClear(); g_env->DeleteLocalRef(hashMapClass); g_scoreboardJNI.failed = true; return false; }
    g_scoreboardJNI.mHashMapCtor = GetMethodIDCompat(g_env, hashMapClass, "<init>", "()V");
    g_env->DeleteLocalRef(hashMapClass);
    if (!g_scoreboardJNI.mHashMapCtor) { g_env->ExceptionClear(); g_scoreboardJNI.failed = true; return false; }

    jclass arrayListClass = FindClassLoose(g_env, "java/util/ArrayList");
    if (!arrayListClass) { g_env->ExceptionClear(); g_scoreboardJNI.failed = true; return false; }
    g_scoreboardJNI.arrayListClass = (jclass)g_env->NewGlobalRef(arrayListClass);
    if (!g_scoreboardJNI.arrayListClass || g_env->ExceptionCheck()) { g_env->ExceptionClear(); g_env->DeleteLocalRef(arrayListClass); g_scoreboardJNI.failed = true; return false; }
    g_scoreboardJNI.mArrayListCtor = GetMethodIDCompat(g_env, arrayListClass, "<init>", "()V");
    g_env->DeleteLocalRef(arrayListClass);
    if (!g_scoreboardJNI.mArrayListCtor) { g_env->ExceptionClear(); g_scoreboardJNI.failed = true; return false; }

    jclass wnClass = FindClassLoose(g_env, "wn");
    if (!wnClass) { g_env->ExceptionClear(); g_scoreboardJNI.failed = true; return false; }
    g_scoreboardJNI.mPlayerGetProfile = GetMethodIDCompat(g_env, wnClass, "cd", "()Lcom/mojang/authlib/GameProfile;");
    g_env->DeleteLocalRef(wnClass);
    if (!g_scoreboardJNI.mPlayerGetProfile) { g_env->ExceptionClear(); g_scoreboardJNI.failed = true; return false; }

    jclass netHandlerClass = FindClassLoose(g_env, "bcy");
    if (!netHandlerClass) { g_env->ExceptionClear(); g_scoreboardJNI.failed = true; return false; }
    g_scoreboardJNI.mNetHandlerGetPlayerInfoCollection =
        GetMethodIDCompat(g_env, netHandlerClass, "d", "()Ljava/util/Collection;");
    g_env->DeleteLocalRef(netHandlerClass);
    if (!g_scoreboardJNI.mNetHandlerGetPlayerInfoCollection) { g_env->ExceptionClear(); g_scoreboardJNI.failed = true; return false; }

    jclass networkInfoClass = FindClassLoose(g_env, "bdc");
    if (!networkInfoClass) { g_env->ExceptionClear(); g_scoreboardJNI.failed = true; return false; }
    g_scoreboardJNI.mNetworkInfoGetProfile =
        GetMethodIDCompat(g_env, networkInfoClass, "a", "()Lcom/mojang/authlib/GameProfile;");
    g_env->DeleteLocalRef(networkInfoClass);
    if (!g_scoreboardJNI.mNetworkInfoGetProfile) { g_env->ExceptionClear(); g_scoreboardJNI.failed = true; return false; }

    jclass profileClass = FindClassLoose(g_env, "com/mojang/authlib/GameProfile");
    if (!profileClass) { g_env->ExceptionClear(); g_scoreboardJNI.failed = true; return false; }
    g_scoreboardJNI.mGameProfileGetName = GetMethodIDCompat(g_env, profileClass, "getName", "()Ljava/lang/String;");
    if (!g_scoreboardJNI.mGameProfileGetName) { g_env->ExceptionClear(); g_env->DeleteLocalRef(profileClass); g_scoreboardJNI.failed = true; return false; }
    g_scoreboardJNI.mGameProfileGetId = GetMethodIDCompat(g_env, profileClass, "getId", "()Ljava/util/UUID;");
    g_env->DeleteLocalRef(profileClass);
    if (!g_scoreboardJNI.mGameProfileGetId) { g_env->ExceptionClear(); g_scoreboardJNI.failed = true; return false; }

    jclass uuidClass = FindClassLoose(g_env, "java/util/UUID");
    if (!uuidClass) { g_env->ExceptionClear(); g_scoreboardJNI.failed = true; return false; }
    g_scoreboardJNI.mUuidToString = GetMethodIDCompat(g_env, uuidClass, "toString", "()Ljava/lang/String;");
    g_env->DeleteLocalRef(uuidClass);
    if (!g_scoreboardJNI.mUuidToString) { g_env->ExceptionClear(); g_scoreboardJNI.failed = true; return false; }

    return true;
}

jobject GetWorldObject() {
    if (!g_env || !InitScoreboardJNI()) return nullptr;

    jobject mc = g_env->CallStaticObjectMethod(g_scoreboardJNI.mcClass, g_scoreboardJNI.mGetMC);
    if (!mc || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (mc) g_env->DeleteLocalRef(mc);
        return nullptr;
    }

    jobject world = g_env->GetObjectField(mc, g_scoreboardJNI.fWorldField);
    if (!world || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (world) g_env->DeleteLocalRef(world);
        g_env->DeleteLocalRef(mc);
        return nullptr;
    }

    g_env->DeleteLocalRef(mc);
    return world;
}

jobjectArray GetNetworkPlayerInfoArray() {
    if (!g_env || !InitScoreboardJNI()) return nullptr;

    jobject mc = g_env->CallStaticObjectMethod(g_scoreboardJNI.mcClass, g_scoreboardJNI.mGetMC);
    if (!mc || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (mc) g_env->DeleteLocalRef(mc);
        return nullptr;
    }
    jobject netHandler = g_env->CallObjectMethod(mc, g_scoreboardJNI.mMinecraftGetNetHandler);
    g_env->DeleteLocalRef(mc);
    if (!netHandler || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (netHandler) g_env->DeleteLocalRef(netHandler);
        return nullptr;
    }
    jobject playerInfos = g_env->CallObjectMethod(
        netHandler, g_scoreboardJNI.mNetHandlerGetPlayerInfoCollection);
    g_env->DeleteLocalRef(netHandler);
    if (!playerInfos || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (playerInfos) g_env->DeleteLocalRef(playerInfos);
        return nullptr;
    }
    jobjectArray result = (jobjectArray)g_env->CallObjectMethod(playerInfos, g_scoreboardJNI.mToArray);
    g_env->DeleteLocalRef(playerInfos);
    if (!result || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (result) g_env->DeleteLocalRef(result);
        return nullptr;
    }
    return result;
}

bool GetNetworkPlayerInfoNameAndUuid(
    jobject networkInfo,
    std::string& name,
    std::string& uuid) {
    name.clear();
    uuid.clear();
    if (!g_env || !networkInfo) return false;

    jobject profile = g_env->CallObjectMethod(networkInfo, g_scoreboardJNI.mNetworkInfoGetProfile);
    if (!profile || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (profile) g_env->DeleteLocalRef(profile);
        return false;
    }
    jstring nameString = (jstring)g_env->CallObjectMethod(profile, g_scoreboardJNI.mGameProfileGetName);
    if (g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (nameString) g_env->DeleteLocalRef(nameString);
        g_env->DeleteLocalRef(profile);
        return false;
    }
    name = JStringToUtf8(nameString);
    if (nameString) g_env->DeleteLocalRef(nameString);

    jobject uuidObject = g_env->CallObjectMethod(profile, g_scoreboardJNI.mGameProfileGetId);
    if (g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        uuidObject = nullptr;
    }
    if (uuidObject) {
        jstring uuidString = (jstring)g_env->CallObjectMethod(uuidObject, g_scoreboardJNI.mUuidToString);
        if (!g_env->ExceptionCheck()) uuid = JStringToUtf8(uuidString);
        else g_env->ExceptionClear();
        if (uuidString) g_env->DeleteLocalRef(uuidString);
        g_env->DeleteLocalRef(uuidObject);
    }
    g_env->DeleteLocalRef(profile);
    return !name.empty() && !uuid.empty();
}

jobject GetScoreboardObjectFromWorld(jobject world) {
    if (!g_env || !world || !InitScoreboardJNI()) return nullptr;

    jobject scoreboard = g_env->GetObjectField(world, g_scoreboardJNI.fSbField);
    if (!scoreboard || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (scoreboard) g_env->DeleteLocalRef(scoreboard);
        return nullptr;
    }
    return scoreboard;
}

jobjectArray GetScoreboardTeamArrayFromScoreboard(jobject scoreboard) {
    if (!g_env || !scoreboard || !InitScoreboardJNI()) return nullptr;
    jobject teamMap = g_env->GetObjectField(scoreboard, g_scoreboardJNI.fMapE);
    if (!teamMap || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (teamMap) g_env->DeleteLocalRef(teamMap);
        return nullptr;
    }

    jobject values = g_env->CallObjectMethod(teamMap, g_scoreboardJNI.mMapValues);
    if (!values || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (values) g_env->DeleteLocalRef(values);
        g_env->DeleteLocalRef(teamMap);
        return nullptr;
    }

    jobjectArray arr = (jobjectArray)g_env->CallObjectMethod(values, g_scoreboardJNI.mToArray);
    if (g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        arr = nullptr;
    }

    g_env->DeleteLocalRef(values);
    g_env->DeleteLocalRef(teamMap);
    return arr;
}

jobjectArray GetScoreboardTeamArray() {
    jobject world = GetWorldObject();
    if (!world) return nullptr;
    jobject scoreboard = GetScoreboardObjectFromWorld(world);
    g_env->DeleteLocalRef(world);
    if (!scoreboard) return nullptr;
    jobjectArray arr = GetScoreboardTeamArrayFromScoreboard(scoreboard);
    g_env->DeleteLocalRef(scoreboard);
    return arr;
}

bool IsLikelyPlayerUsername(const std::string& value) {
    if (value.empty() || value.size() > 16) return false;
    for (char c : value) {
        bool valid = (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '_';
        if (!valid) return false;
    }
    return true;
}

bool TeamEntriesArePlayerUsernames(jobject team) {
    if (!g_env || !team) return false;

    jobject members = g_env->GetObjectField(team, g_scoreboardJNI.fTeamMembers);
    if (!members || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (members) g_env->DeleteLocalRef(members);
        return false;
    }

    jobjectArray memberArray = (jobjectArray)g_env->CallObjectMethod(members, g_scoreboardJNI.mToArray);
    g_env->DeleteLocalRef(members);
    if (!memberArray || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (memberArray) g_env->DeleteLocalRef(memberArray);
        return false;
    }

    jsize len = g_env->GetArrayLength(memberArray);
    bool hasEntries = false;
    for (jsize i = 0; i < len; i++) {
        jstring entry = (jstring)g_env->GetObjectArrayElement(memberArray, i);
        std::string value = JStringToUtf8(entry);
        if (entry) g_env->DeleteLocalRef(entry);
        if (!IsLikelyPlayerUsername(value)) {
            g_env->DeleteLocalRef(memberArray);
            return false;
        }
        hasEntries = true;
    }

    g_env->DeleteLocalRef(memberArray);
    return hasEntries;
}

std::string GetSingleTeamPlayerName(jobject team) {
    if (!g_env || !team) return "";

    jobject members = g_env->GetObjectField(team, g_scoreboardJNI.fTeamMembers);
    if (!members || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (members) g_env->DeleteLocalRef(members);
        return "";
    }

    jobjectArray memberArray = (jobjectArray)g_env->CallObjectMethod(members, g_scoreboardJNI.mToArray);
    g_env->DeleteLocalRef(members);
    if (!memberArray || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (memberArray) g_env->DeleteLocalRef(memberArray);
        return "";
    }

    jsize len = g_env->GetArrayLength(memberArray);
    std::string playerName;
    for (jsize i = 0; i < len; i++) {
        jstring entry = (jstring)g_env->GetObjectArrayElement(memberArray, i);
        std::string value = JStringToUtf8(entry);
        if (entry) g_env->DeleteLocalRef(entry);

        if (!IsLikelyPlayerUsername(value)) {
            g_env->DeleteLocalRef(memberArray);
            return "";
        }
        if (!playerName.empty()) {
            g_env->DeleteLocalRef(memberArray);
            return "";
        }
        playerName = value;
    }

    g_env->DeleteLocalRef(memberArray);
    return playerName;
}

bool GetPlayerNameAndUuid(jobject player, std::string& name, std::string& uuid) {
    name.clear();
    uuid.clear();
    if (!g_env || !player) return false;

    jobject profile = g_env->CallObjectMethod(player, g_scoreboardJNI.mPlayerGetProfile);
    if (!profile || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (profile) g_env->DeleteLocalRef(profile);
        return false;
    }

    jstring nameStr = (jstring)g_env->CallObjectMethod(profile, g_scoreboardJNI.mGameProfileGetName);
    if (g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (nameStr) g_env->DeleteLocalRef(nameStr);
        g_env->DeleteLocalRef(profile);
        return false;
    }

    name = JStringToUtf8(nameStr);
    if (nameStr) g_env->DeleteLocalRef(nameStr);

    jobject uuidObj = g_env->CallObjectMethod(profile, g_scoreboardJNI.mGameProfileGetId);
    if (g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        uuidObj = nullptr;
    }

    if (uuidObj) {
        jstring uuidStr = (jstring)g_env->CallObjectMethod(uuidObj, g_scoreboardJNI.mUuidToString);
        if (g_env->ExceptionCheck()) {
            g_env->ExceptionClear();
            if (uuidStr) g_env->DeleteLocalRef(uuidStr);
        }
        else {
            uuid = JStringToUtf8(uuidStr);
            if (uuidStr) g_env->DeleteLocalRef(uuidStr);
        }
        g_env->DeleteLocalRef(uuidObj);
    }

    g_env->DeleteLocalRef(profile);
    return !name.empty();
}

std::string GetPlayerName(jobject player) {
    std::string name;
    std::string uuid;
    GetPlayerNameAndUuid(player, name, uuid);
    return name;
}

void BuildWorldPlayerUuidByName(jobject world, std::unordered_map<std::string, std::string>& uuidByName) {
    if (!g_env || !world) return;

    jobject players = g_env->GetObjectField(world, g_scoreboardJNI.fPlayerList);
    if (!players || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (players) g_env->DeleteLocalRef(players);
        return;
    }

    jobjectArray playerArray = (jobjectArray)g_env->CallObjectMethod(players, g_scoreboardJNI.mToArray);
    g_env->DeleteLocalRef(players);
    if (!playerArray || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (playerArray) g_env->DeleteLocalRef(playerArray);
        return;
    }

    jsize len = g_env->GetArrayLength(playerArray);
    for (jsize i = 0; i < len; i++) {
        jobject player = g_env->GetObjectArrayElement(playerArray, i);
        if (!player) continue;

        std::string name;
        std::string uuid;
        if (GetPlayerNameAndUuid(player, name, uuid) && IsLikelyPlayerUsername(name) && IsUuidLookupId(uuid)) {
            uuidByName[NormalizePlayerKey(name)] = uuid;
        }

        g_env->DeleteLocalRef(player);
    }

    g_env->DeleteLocalRef(playerArray);
}

void QueuePublicWinsForWorldPlayers() {
    if (!g_env || !InitScoreboardJNI() ||
        InterlockedCompareExchange(&g_publicWinsRuntimeEnabled, 0, 0) == 0 ||
        InterlockedCompareExchange(&g_hypixelTntTagGameActive, 0, 0) == 0) return;

    jobjectArray playerArray = GetNetworkPlayerInfoArray();
    if (!playerArray || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (playerArray) g_env->DeleteLocalRef(playerArray);
        return;
    }

    jsize len = g_env->GetArrayLength(playerArray);
    int resolved = 0;
    for (jsize i = 0; i < len; ++i) {
        jobject networkInfo = g_env->GetObjectArrayElement(playerArray, i);
        if (!networkInfo) continue;
        std::string name;
        std::string uuid;
        if (GetNetworkPlayerInfoNameAndUuid(networkInfo, name, uuid)) {
            ++resolved;
            QueuePublicWinsLookup(uuid, name);
        }
        g_env->DeleteLocalRef(networkInfo);
    }
    g_env->DeleteLocalRef(playerArray);
    if (InterlockedCompareExchange(&g_publicWinsPrefetchLogged, 1, 0) == 0) {
        DebugLog("Public wins prefetch tabPlayers=%d resolved=%d worker=%ld",
            (int)len,
            resolved,
            InterlockedCompareExchange(&g_publicWinsWorkerRunning, 0, 0));
    }
}

jobject FindOrCreateFallbackTeam(jobject scoreboard) {
    if (!g_env || !scoreboard) return nullptr;

    jstring teamName = g_env->NewStringUTF(kTimerFallbackTeamName);
    if (!teamName || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (teamName) g_env->DeleteLocalRef(teamName);
        return nullptr;
    }

    jobject team = g_env->CallObjectMethod(scoreboard, g_scoreboardJNI.mScoreboardGetTeam, teamName);
    if (g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (team) g_env->DeleteLocalRef(team);
        team = nullptr;
    }

    if (!team) {
        team = g_env->CallObjectMethod(scoreboard, g_scoreboardJNI.mScoreboardCreateTeam, teamName);
        if (g_env->ExceptionCheck()) {
            g_env->ExceptionClear();
            if (team) g_env->DeleteLocalRef(team);
            team = nullptr;
        }
    }

    g_env->DeleteLocalRef(teamName);
    return team;
}

void HealFallbackCoverage(jobject scoreboard, jobject world) {
    if (!g_env || !scoreboard || !world) return;

    jobject players = g_env->GetObjectField(world, g_scoreboardJNI.fPlayerList);
    if (!players || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (players) g_env->DeleteLocalRef(players);
        return;
    }

    jobjectArray playerArray = (jobjectArray)g_env->CallObjectMethod(players, g_scoreboardJNI.mToArray);
    g_env->DeleteLocalRef(players);
    if (!playerArray || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (playerArray) g_env->DeleteLocalRef(playerArray);
        return;
    }

    jstring fallbackTeamName = g_env->NewStringUTF(kTimerFallbackTeamName);
    if (!fallbackTeamName || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (fallbackTeamName) g_env->DeleteLocalRef(fallbackTeamName);
        g_env->DeleteLocalRef(playerArray);
        return;
    }

    jsize len = g_env->GetArrayLength(playerArray);
    for (jsize i = 0; i < len; i++) {
        jobject player = g_env->GetObjectArrayElement(playerArray, i);
        if (!player) continue;

        std::string playerName = GetPlayerName(player);
        if (playerName.empty()) {
            g_env->DeleteLocalRef(player);
            continue;
        }

        jstring playerNameStr = g_env->NewStringUTF(playerName.c_str());
        if (!playerNameStr || g_env->ExceptionCheck()) {
            g_env->ExceptionClear();
            if (playerNameStr) g_env->DeleteLocalRef(playerNameStr);
            g_env->DeleteLocalRef(player);
            continue;
        }

        jobject existingTeam = g_env->CallObjectMethod(scoreboard, g_scoreboardJNI.mScoreboardGetPlayersTeam, playerNameStr);
        if (g_env->ExceptionCheck()) {
            g_env->ExceptionClear();
            if (existingTeam) g_env->DeleteLocalRef(existingTeam);
            existingTeam = nullptr;
        }

        if (!existingTeam) {
            g_env->CallBooleanMethod(scoreboard, g_scoreboardJNI.mScoreboardAddPlayerToTeam, playerNameStr, fallbackTeamName);
            if (g_env->ExceptionCheck()) g_env->ExceptionClear();
        }
        else {
            g_env->DeleteLocalRef(existingTeam);
        }

        g_env->DeleteLocalRef(playerNameStr);
        g_env->DeleteLocalRef(player);
    }

    g_env->DeleteLocalRef(fallbackTeamName);
    g_env->DeleteLocalRef(playerArray);
}

void RemoveFallbackTeam(jobject scoreboard) {
    if (!g_env || !scoreboard) return;

    jstring teamName = g_env->NewStringUTF(kTimerFallbackTeamName);
    if (!teamName || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (teamName) g_env->DeleteLocalRef(teamName);
        return;
    }

    jobject team = g_env->CallObjectMethod(scoreboard, g_scoreboardJNI.mScoreboardGetTeam, teamName);
    g_env->DeleteLocalRef(teamName);
    if (!team || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (team) g_env->DeleteLocalRef(team);
        return;
    }

    g_env->CallVoidMethod(scoreboard, g_scoreboardJNI.mScoreboardRemoveTeam, team);
    if (g_env->ExceptionCheck()) g_env->ExceptionClear();
    g_env->DeleteLocalRef(team);
}

size_t Utf8CharLen(unsigned char lead) {
    if ((lead & 0x80) == 0) return 1;
    if ((lead & 0xE0) == 0xC0) return 2;
    if ((lead & 0xF0) == 0xE0) return 3;
    if ((lead & 0xF8) == 0xF0) return 4;
    return 1;
}

bool IsSectionSignAt(const std::string& value, size_t index) {
    return index + 1 < value.size() &&
        (unsigned char)value[index] == 0xC2 &&
        (unsigned char)value[index + 1] == 0xA7;
}

size_t CountUtf8Chars(const std::string& value) {
    size_t count = 0;
    for (size_t i = 0; i < value.size();) {
        size_t charLen = Utf8CharLen((unsigned char)value[i]);
        if (charLen == 0 || i + charLen > value.size()) charLen = 1;
        i += charLen;
        count++;
    }
    return count;
}

std::string TruncateSuffixBase(const std::string& value, size_t maxChars) {
    std::string result;
    size_t used = 0;
    for (size_t i = 0; i < value.size() && used < maxChars;) {
        if (IsSectionSignAt(value, i)) {
            if (used + 2 > maxChars || i + 2 >= value.size()) break;
            size_t codeLen = Utf8CharLen((unsigned char)value[i + 2]);
            if (i + 2 + codeLen > value.size()) break;
            result.append(value, i, 2 + codeLen);
            i += 2 + codeLen;
            used += 2;
            continue;
        }

        size_t charLen = Utf8CharLen((unsigned char)value[i]);
        if (charLen == 0 || i + charLen > value.size()) charLen = 1;
        result.append(value, i, charLen);
        i += charLen;
        used++;
    }
    return result;
}

bool EndsWith(const std::string& value, const std::string& suffix) {
    return value.size() >= suffix.size() &&
        value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string MakeTimerSuffix(double secondsRemaining) {
    double clampedSeconds = secondsRemaining < 0.0 ? 0.0 : secondsRemaining;
    int whole = GetDisplayedTimerNumber(clampedSeconds);
    std::string timerText = FormatTimerText(clampedSeconds);

    std::string result = " ";
    result += "\xC2\xA7";
    result += "7[";
    result += "\xC2\xA7";
    result.push_back(GetTimerColorCode(whole));
    result += timerText;
    result += "\xC2\xA7";
    result += "7s]";
    return result;
}

std::string MakeTimerPrefix(double secondsRemaining) {
    double clampedSeconds = secondsRemaining < 0.0 ? 0.0 : secondsRemaining;
    int whole = GetDisplayedTimerNumber(clampedSeconds);
    std::string result = "\xC2\xA7" "7[\xC2\xA7";
    result.push_back(GetTimerColorCode(whole));
    result += FormatTimerText(clampedSeconds);
    result += "\xC2\xA7" "7s]\xC2\xA7" "r ";
    return result;
}

std::string BuildTimerTeamSuffix(const std::string& baseSuffix, const std::string& timerSuffix) {
    size_t timerChars = CountUtf8Chars(timerSuffix);
    size_t maxBaseChars = (timerChars >= 16) ? 0 : (16 - timerChars);
    return TruncateSuffixBase(baseSuffix, maxBaseChars) + timerSuffix;
}

bool SetTeamSuffix(jobject team, const std::string& suffix) {
    if (!g_env || !team) return false;
    jstring newSuffix = g_env->NewStringUTF(suffix.c_str());
    if (!newSuffix || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (newSuffix) g_env->DeleteLocalRef(newSuffix);
        return false;
    }

    g_env->SetObjectField(team, g_scoreboardJNI.fTeamF, newSuffix);
    g_env->DeleteLocalRef(newSuffix);
    if (g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        return false;
    }
    return true;
}

bool SetTeamStringField(jobject team, jfieldID field, const std::string& value) {
    if (!g_env || !team || !field) return false;
    jstring newValue = g_env->NewStringUTF(value.c_str());
    if (!newValue || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (newValue) g_env->DeleteLocalRef(newValue);
        return false;
    }

    g_env->SetObjectField(team, field, newValue);
    g_env->DeleteLocalRef(newValue);
    if (g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        return false;
    }
    return true;
}

bool SetTeamPrefixAndSuffix(jobject team, const std::string& prefix, const std::string& suffix) {
    bool prefixOk = SetTeamStringField(team, g_scoreboardJNI.fTeamE, prefix);
    bool suffixOk = SetTeamStringField(team, g_scoreboardJNI.fTeamF, suffix);
    return prefixOk && suffixOk;
}


bool IsMinecraftFormatCodeAt(const std::string& value, size_t index, size_t& codeLength) {
    if (index < value.size() && value[index] == '\xC2' && index + 2 < value.size() && value[index + 1] == '\xA7') {
        codeLength = 3;
        return true;
    }
    if (index < value.size() && value[index] == '\xA7' && index + 1 < value.size()) {
        codeLength = 2;
        return true;
    }
    return false;
}

char GetActiveMinecraftColourBefore(const std::string& value, size_t endIndex) {
    char activeColour = '\0';
    for (size_t i = 0; i < endIndex && i < value.size();) {
        size_t codeLength = 0;
        if (!IsMinecraftFormatCodeAt(value, i, codeLength)) {
            ++i;
            continue;
        }

        size_t codeIndex = i + codeLength - 1;
        char code = codeIndex < value.size() ? value[codeIndex] : '\0';
        if (code >= 'A' && code <= 'Z') code = (char)(code - 'A' + 'a');
        if ((code >= '0' && code <= '9') || (code >= 'a' && code <= 'f')) activeColour = code;
        else if (code == 'r') activeColour = '\0';
        i += codeLength;
    }
    return activeColour;
}

std::string ReplaceFirstTimerNumber(const std::string& line, double secondsRemaining) {
    double clampedSeconds = secondsRemaining < 0.0 ? 0.0 : secondsRemaining;
    std::string timerText = FormatTimerText(clampedSeconds);

    for (size_t i = 0; i < line.size(); ++i) {
        size_t codeLength = 0;
        if (IsMinecraftFormatCodeAt(line, i, codeLength)) {
            i += codeLength - 1;
            continue;
        }

        if (line[i] < '0' || line[i] > '9') continue;

        size_t start = i;
        while (i < line.size() && line[i] >= '0' && line[i] <= '9') ++i;
        if (i < line.size() && line[i] == '.') {
            ++i;
            while (i < line.size() && line[i] >= '0' && line[i] <= '9') ++i;
        }

        char restoreColour = GetActiveMinecraftColourBefore(line, start);
        std::string colouredTimer = "\xC2\xA7";
        colouredTimer.push_back(GetTimerColorCode(GetDisplayedTimerNumber(clampedSeconds)));
        colouredTimer += timerText;
        colouredTimer += "\xC2\xA7";
        colouredTimer.push_back(restoreColour != '\0' ? restoreColour : 'r');

        std::string result = line;
        result.replace(start, i - start, colouredTimer);
        return result;
    }

    std::string result = line + " \xC2\xA7";
    result.push_back(GetTimerColorCode(GetDisplayedTimerNumber(clampedSeconds)));
    result += timerText;
    result += "\xC2\xA7rs";
    return result;
}

std::string FormatBetweenRoundScoreboardLine(double secondsRemaining) {
    double clampedSeconds = secondsRemaining < 0.0 ? 0.0 : secondsRemaining;
    int whole = GetDisplayedTimerNumber(clampedSeconds);
    std::string result = "\xC2\xA7";
    result += "eNext: ";
    result += "\xC2\xA7";
    result.push_back(GetTimerColorCode(whole));
    result += FormatTimerText(clampedSeconds);
    result += "\xC2\xA7";
    result += "es";
    return result;
}

void GetReadableScoreboardAffixes(
    const std::string& teamName,
    const std::string& prefix,
    const std::string& suffix,
    std::string& readablePrefix,
    std::string& readableSuffix) {
    readablePrefix = prefix;
    readableSuffix = suffix;
    auto it = g_scoreboardTimerLineCache.find(teamName);
    if (it != g_scoreboardTimerLineCache.end() &&
        prefix == it->second.appliedPrefix &&
        suffix == it->second.appliedSuffix) {
        readablePrefix = it->second.basePrefix;
        readableSuffix = it->second.baseSuffix;
    }
}

std::string GetReadableScoreboardLine(const std::string& teamName, const std::string& prefix, const std::string& suffix) {
    std::string readablePrefix;
    std::string readableSuffix;
    GetReadableScoreboardAffixes(teamName, prefix, suffix, readablePrefix, readableSuffix);
    return readablePrefix + readableSuffix;
}

void AppendUniqueString(std::vector<std::string>& values, const std::string& value) {
    if (std::find(values.begin(), values.end(), value) == values.end()) values.push_back(value);
}

std::vector<std::string> GetScoreboardTeamEntries(jobject team) {
    std::vector<std::string> entries;
    if (!g_env || !team || !g_scoreboardJNI.fTeamMembers || !g_scoreboardJNI.mToArray) return entries;

    jobject members = g_env->GetObjectField(team, g_scoreboardJNI.fTeamMembers);
    if (!members || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (members) g_env->DeleteLocalRef(members);
        return entries;
    }

    jobjectArray memberArray = (jobjectArray)g_env->CallObjectMethod(members, g_scoreboardJNI.mToArray);
    g_env->DeleteLocalRef(members);
    if (!memberArray || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (memberArray) g_env->DeleteLocalRef(memberArray);
        return entries;
    }

    jsize count = g_env->GetArrayLength(memberArray);
    for (jsize i = 0; i < count; ++i) {
        jstring entryString = (jstring)g_env->GetObjectArrayElement(memberArray, i);
        std::string entry = JStringToUtf8(entryString);
        if (entryString) g_env->DeleteLocalRef(entryString);
        AppendUniqueString(entries, entry);
    }
    g_env->DeleteLocalRef(memberArray);
    return entries;
}

std::vector<std::string> GetReadableScoreboardLineCandidates(
    jobject team,
    const std::string& teamName,
    const std::string& prefix,
    const std::string& suffix) {
    std::string readablePrefix;
    std::string readableSuffix;
    GetReadableScoreboardAffixes(teamName, prefix, suffix, readablePrefix, readableSuffix);

    std::vector<std::string> candidates;
    AppendUniqueString(candidates, readablePrefix + readableSuffix);
    for (const std::string& entry : GetScoreboardTeamEntries(team)) {
        // Vanilla renders a scoreboard entry as team prefix + entry + suffix.
        // Hypixel normally keeps the useful text in the affixes, while Void Tag
        // can put the coloured countdown itself in the entry.
        AppendUniqueString(candidates, readablePrefix + entry + readableSuffix);
    }
    return candidates;
}

std::vector<std::string> GetSidebarObjectiveDisplayNames() {
    std::vector<std::string> names;
    if (!g_env || !InitScoreboardJNI() || !g_scoreboardJNI.fObjectiveDisplayName) return names;

    jobject world = GetWorldObject();
    if (!world) return names;
    jobject scoreboard = GetScoreboardObjectFromWorld(world);
    g_env->DeleteLocalRef(world);
    if (!scoreboard) return names;

    jobjectArray objectives = GetScoreboardDisplayObjectiveArray(scoreboard);
    g_env->DeleteLocalRef(scoreboard);
    if (!objectives) return names;

    jsize count = g_env->GetArrayLength(objectives);
    for (jsize slot = 1; slot < count; ++slot) {
        if (!IsSidebarDisplaySlot(slot)) continue;
        jobject objective = g_env->GetObjectArrayElement(objectives, slot);
        if (!objective) continue;
        jstring displayName = (jstring)g_env->GetObjectField(objective, g_scoreboardJNI.fObjectiveDisplayName);
        if (!g_env->ExceptionCheck()) AppendUniqueString(names, JStringToUtf8(displayName));
        else g_env->ExceptionClear();
        if (displayName) g_env->DeleteLocalRef(displayName);
        g_env->DeleteLocalRef(objective);
    }
    g_env->DeleteLocalRef(objectives);
    return names;
}

std::vector<std::string> GetSidebarScoreboardLines() {
    std::vector<std::string> lines;
    if (!g_env || !InitScoreboardJNI()) return lines;

    jobject world = GetWorldObject();
    if (!world) return lines;
    jobject scoreboard = GetScoreboardObjectFromWorld(world);
    g_env->DeleteLocalRef(world);
    if (!scoreboard) return lines;

    jobjectArray displayObjectives = GetScoreboardDisplayObjectiveArray(scoreboard);
    if (!displayObjectives) {
        g_env->DeleteLocalRef(scoreboard);
        return lines;
    }

    std::vector<jobject> sidebarObjectives;
    jsize displaySlotCount = g_env->GetArrayLength(displayObjectives);
    for (jsize slot = 1; slot < displaySlotCount; ++slot) {
        if (!IsSidebarDisplaySlot(slot)) continue;
        jobject objective = g_env->GetObjectArrayElement(displayObjectives, slot);
        if (!objective) continue;

        bool duplicate = false;
        for (jobject existing : sidebarObjectives) {
            if (g_env->IsSameObject(existing, objective)) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) g_env->DeleteLocalRef(objective);
        else sidebarObjectives.push_back(objective);
    }
    g_env->DeleteLocalRef(displayObjectives);

    jobject scoresByEntity = g_env->GetObjectField(scoreboard, g_scoreboardJNI.fScoresByEntity);
    if (!scoresByEntity || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (scoresByEntity) g_env->DeleteLocalRef(scoresByEntity);
        for (jobject objective : sidebarObjectives) g_env->DeleteLocalRef(objective);
        g_env->DeleteLocalRef(scoreboard);
        return lines;
    }

    jobject scoreMaps = g_env->CallObjectMethod(scoresByEntity, g_scoreboardJNI.mMapValues);
    g_env->DeleteLocalRef(scoresByEntity);
    if (!scoreMaps || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (scoreMaps) g_env->DeleteLocalRef(scoreMaps);
        for (jobject objective : sidebarObjectives) g_env->DeleteLocalRef(objective);
        g_env->DeleteLocalRef(scoreboard);
        return lines;
    }

    jobjectArray entityScoreMaps = (jobjectArray)g_env->CallObjectMethod(scoreMaps, g_scoreboardJNI.mToArray);
    g_env->DeleteLocalRef(scoreMaps);
    if (!entityScoreMaps || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (entityScoreMaps) g_env->DeleteLocalRef(entityScoreMaps);
        for (jobject objective : sidebarObjectives) g_env->DeleteLocalRef(objective);
        g_env->DeleteLocalRef(scoreboard);
        return lines;
    }

    jsize scoreMapCount = g_env->GetArrayLength(entityScoreMaps);
    for (jobject objective : sidebarObjectives) {
        for (jsize i = 0; i < scoreMapCount; ++i) {
            jobject entityScoreMap = g_env->GetObjectArrayElement(entityScoreMaps, i);
            if (!entityScoreMap || g_env->ExceptionCheck()) {
                g_env->ExceptionClear();
                if (entityScoreMap) g_env->DeleteLocalRef(entityScoreMap);
                continue;
            }

            jobject score = g_env->CallObjectMethod(entityScoreMap, g_scoreboardJNI.mMapGet, objective);
            g_env->DeleteLocalRef(entityScoreMap);
            if (g_env->ExceptionCheck()) {
                g_env->ExceptionClear();
                if (score) g_env->DeleteLocalRef(score);
                continue;
            }
            if (!score) continue;

            jstring entityNameString =
                (jstring)g_env->GetObjectField(score, g_scoreboardJNI.fScoreEntityName);
            g_env->DeleteLocalRef(score);
            if (!entityNameString || g_env->ExceptionCheck()) {
                g_env->ExceptionClear();
                if (entityNameString) g_env->DeleteLocalRef(entityNameString);
                continue;
            }

            std::string entityName = JStringToUtf8(entityNameString);
            jobject team = g_env->CallObjectMethod(
                scoreboard,
                g_scoreboardJNI.mScoreboardGetPlayersTeam,
                entityNameString);
            g_env->DeleteLocalRef(entityNameString);
            if (g_env->ExceptionCheck()) {
                g_env->ExceptionClear();
                if (team) g_env->DeleteLocalRef(team);
                team = nullptr;
            }

            if (!team) {
                AppendUniqueString(lines, entityName);
                continue;
            }

            jstring teamNameString = (jstring)g_env->GetObjectField(team, g_scoreboardJNI.fTeamName);
            jstring prefixString = (jstring)g_env->GetObjectField(team, g_scoreboardJNI.fTeamE);
            jstring suffixString = (jstring)g_env->GetObjectField(team, g_scoreboardJNI.fTeamF);
            if (g_env->ExceptionCheck()) g_env->ExceptionClear();
            std::string teamName = JStringToUtf8(teamNameString);
            std::string prefix = JStringToUtf8(prefixString);
            std::string suffix = JStringToUtf8(suffixString);
            if (teamNameString) g_env->DeleteLocalRef(teamNameString);
            if (prefixString) g_env->DeleteLocalRef(prefixString);
            if (suffixString) g_env->DeleteLocalRef(suffixString);
            g_env->DeleteLocalRef(team);

            std::string readablePrefix;
            std::string readableSuffix;
            GetReadableScoreboardAffixes(
                teamName,
                prefix,
                suffix,
                readablePrefix,
                readableSuffix);
            AppendUniqueString(lines, readablePrefix + entityName + readableSuffix);
        }
    }

    g_env->DeleteLocalRef(entityScoreMaps);
    for (jobject objective : sidebarObjectives) g_env->DeleteLocalRef(objective);
    g_env->DeleteLocalRef(scoreboard);
    return lines;
}

bool ScoreboardLineHasTimerNumber(const std::string& line) {
    char ignoredColour = 'a';
    return !ExtractSeconds(line, ignoredColour).empty();
}

void ApplyDefaultScoreboardTimerEdit(
    bool enableTimerEdit,
    double secondsRemaining = 0.0,
    bool betweenRounds = false) {
    if (!enableTimerEdit) {
        if (g_env && InitScoreboardJNI() && !g_scoreboardTimerLineCache.empty()) {
            jobjectArray restoreTeams = GetScoreboardTeamArray();
            if (restoreTeams) {
                jsize restoreCount = g_env->GetArrayLength(restoreTeams);
                for (jsize i = 0; i < restoreCount; ++i) {
                    jobject team = g_env->GetObjectArrayElement(restoreTeams, i);
                    if (!team) continue;

                    jstring teamNameStr = (jstring)g_env->GetObjectField(team, g_scoreboardJNI.fTeamName);
                    std::string teamName = JStringToUtf8(teamNameStr);
                    if (teamNameStr) g_env->DeleteLocalRef(teamNameStr);

                    auto cached = g_scoreboardTimerLineCache.find(teamName);
                    if (cached != g_scoreboardTimerLineCache.end()) {
                        jstring prefixStr = (jstring)g_env->GetObjectField(team, g_scoreboardJNI.fTeamE);
                        std::string prefix = JStringToUtf8(prefixStr);
                        if (prefixStr) g_env->DeleteLocalRef(prefixStr);
                        jstring suffixStr = (jstring)g_env->GetObjectField(team, g_scoreboardJNI.fTeamF);
                        std::string suffix = JStringToUtf8(suffixStr);
                        if (suffixStr) g_env->DeleteLocalRef(suffixStr);

                        if (prefix == cached->second.appliedPrefix &&
                            suffix == cached->second.appliedSuffix) {
                            SetTeamPrefixAndSuffix(
                                team,
                                cached->second.basePrefix,
                                cached->second.baseSuffix);
                        }
                    }
                    g_env->DeleteLocalRef(team);
                }
                g_env->DeleteLocalRef(restoreTeams);
            }
        }
        g_scoreboardTimerLineCache.clear();
        return;
    }

    if (!g_env || !InitScoreboardJNI()) {
        return;
    }

    jobjectArray arr = GetScoreboardTeamArray();
    if (!arr) {
        return;
    }

    bool hasExplosionLine = false;
    if (betweenRounds) {
        jsize scanCount = g_env->GetArrayLength(arr);
        for (jsize i = 0; i < scanCount; ++i) {
            jobject team = g_env->GetObjectArrayElement(arr, i);
            if (!team) continue;

            jstring teamNameStr = (jstring)g_env->GetObjectField(team, g_scoreboardJNI.fTeamName);
            std::string teamName = JStringToUtf8(teamNameStr);
            if (teamNameStr) g_env->DeleteLocalRef(teamNameStr);
            jstring prefixStr = (jstring)g_env->GetObjectField(team, g_scoreboardJNI.fTeamE);
            std::string prefix = JStringToUtf8(prefixStr);
            if (prefixStr) g_env->DeleteLocalRef(prefixStr);
            jstring suffixStr = (jstring)g_env->GetObjectField(team, g_scoreboardJNI.fTeamF);
            std::string suffix = JStringToUtf8(suffixStr);
            if (suffixStr) g_env->DeleteLocalRef(suffixStr);

            std::string readableLine = GetReadableScoreboardLine(teamName, prefix, suffix);
            std::string cleanLower = ToLowerAscii(StripMinecraftFormattingCodes(readableLine));
            if (cleanLower.find("explosion") != std::string::npos &&
                ScoreboardLineHasTimerNumber(readableLine)) hasExplosionLine = true;
            g_env->DeleteLocalRef(team);
            if (hasExplosionLine) break;
        }
    }

    jsize len = g_env->GetArrayLength(arr);
    bool editedLine = false;
    for (jsize i = 0; i < len; i++) {
        jobject team = g_env->GetObjectArrayElement(arr, i);
        if (!team) continue;

        jstring teamNameStr = (jstring)g_env->GetObjectField(team, g_scoreboardJNI.fTeamName);
        std::string teamName = JStringToUtf8(teamNameStr);
        if (teamNameStr) g_env->DeleteLocalRef(teamNameStr);

        jstring prefixStr = (jstring)g_env->GetObjectField(team, g_scoreboardJNI.fTeamE);
        std::string prefix = JStringToUtf8(prefixStr);
        if (prefixStr) g_env->DeleteLocalRef(prefixStr);

        jstring suffixStr = (jstring)g_env->GetObjectField(team, g_scoreboardJNI.fTeamF);
        std::string suffix = JStringToUtf8(suffixStr);
        if (suffixStr) g_env->DeleteLocalRef(suffixStr);

        auto it = g_scoreboardTimerLineCache.find(teamName);

        std::string readableLine = GetReadableScoreboardLine(teamName, prefix, suffix);
        std::string cleanLower = ToLowerAscii(StripMinecraftFormattingCodes(readableLine));
        bool isExplosionLine = cleanLower.find("explosion") != std::string::npos &&
            ScoreboardLineHasTimerNumber(readableLine);
        bool isRoundFallbackLine = betweenRounds && !hasExplosionLine &&
            cleanLower.find("round") != std::string::npos;
        if (editedLine || (!isExplosionLine && !isRoundFallbackLine)) {
            g_env->DeleteLocalRef(team);
            continue;
        }

        if (it == g_scoreboardTimerLineCache.end()) {
            ScoreboardTimerLineState state;
            state.basePrefix = prefix;
            state.baseSuffix = suffix;
            it = g_scoreboardTimerLineCache.emplace(teamName, state).first;
        }
        else if (prefix != it->second.appliedPrefix || suffix != it->second.appliedSuffix) {
            it->second.basePrefix = prefix;
            it->second.baseSuffix = suffix;
        }

        std::string desiredLine = betweenRounds
            ? FormatBetweenRoundScoreboardLine(secondsRemaining)
            : ReplaceFirstTimerNumber(it->second.basePrefix + it->second.baseSuffix, secondsRemaining);
        std::string desiredPrefix = desiredLine;
        std::string desiredSuffix;
        if (prefix != desiredPrefix || suffix != desiredSuffix) {
            SetTeamPrefixAndSuffix(team, desiredPrefix, desiredSuffix);
        }
        it->second.appliedPrefix = desiredPrefix;
        it->second.appliedSuffix = desiredSuffix;
        editedLine = true;

        g_env->DeleteLocalRef(team);
    }

    g_env->DeleteLocalRef(arr);
}

void ApplyTimerToPlayerTeams(bool enableTimerSuffix) {
    if (!g_env || !InitScoreboardJNI()) {
        if (!enableTimerSuffix) g_teamSuffixCache.clear();
        return;
    }

    jobject world = GetWorldObject();
    if (!world) {
        if (!enableTimerSuffix) g_teamSuffixCache.clear();
        return;
    }

    jobject scoreboard = GetScoreboardObjectFromWorld(world);
    if (!scoreboard) {
        g_env->DeleteLocalRef(world);
        if (!enableTimerSuffix) g_teamSuffixCache.clear();
        return;
    }

    if (enableTimerSuffix) {
        jobject fallbackTeam = FindOrCreateFallbackTeam(scoreboard);
        if (fallbackTeam) g_env->DeleteLocalRef(fallbackTeam);
        HealFallbackCoverage(scoreboard, world);
    }

    jobjectArray arr = GetScoreboardTeamArrayFromScoreboard(scoreboard);
    if (!arr) {
        if (!enableTimerSuffix) RemoveFallbackTeam(scoreboard);
        g_env->DeleteLocalRef(scoreboard);
        g_env->DeleteLocalRef(world);
        if (!enableTimerSuffix) g_teamSuffixCache.clear();
        return;
    }

    std::string timerSuffix;
    if (enableTimerSuffix) timerSuffix = MakeTimerSuffix(GetDecimalSeconds());

    jsize len = g_env->GetArrayLength(arr);
    for (jsize i = 0; i < len; i++) {
        jobject team = g_env->GetObjectArrayElement(arr, i);
        if (!team) continue;

        jstring teamNameStr = (jstring)g_env->GetObjectField(team, g_scoreboardJNI.fTeamName);
        std::string teamName = JStringToUtf8(teamNameStr);
        if (teamNameStr) g_env->DeleteLocalRef(teamNameStr);

        if (teamName.empty() || !TeamEntriesArePlayerUsernames(team)) {
            g_env->DeleteLocalRef(team);
            continue;
        }

        jstring suffixStr = (jstring)g_env->GetObjectField(team, g_scoreboardJNI.fTeamF);
        std::string currentSuffix = JStringToUtf8(suffixStr);
        if (suffixStr) g_env->DeleteLocalRef(suffixStr);

        auto it = g_teamSuffixCache.find(teamName);
        if (enableTimerSuffix) {
            if (it == g_teamSuffixCache.end()) {
                TeamSuffixState state;
                state.baseSuffix = currentSuffix;
                it = g_teamSuffixCache.emplace(teamName, state).first;
            }
            else if (!it->second.appliedSuffix.empty() && !EndsWith(currentSuffix, it->second.appliedSuffix)) {
                it->second.baseSuffix = currentSuffix;
            }

            std::string desiredSuffix = BuildTimerTeamSuffix(it->second.baseSuffix, timerSuffix);
            if (currentSuffix != desiredSuffix) SetTeamSuffix(team, desiredSuffix);
            it->second.appliedSuffix = timerSuffix;
        }
        else if (it != g_teamSuffixCache.end()) {
            if (!it->second.appliedSuffix.empty() && EndsWith(currentSuffix, it->second.appliedSuffix) && currentSuffix != it->second.baseSuffix) {
                SetTeamSuffix(team, it->second.baseSuffix);
            }
        }

        g_env->DeleteLocalRef(team);
    }

    g_env->DeleteLocalRef(arr);
    if (!enableTimerSuffix) {
        RemoveFallbackTeam(scoreboard);
        g_teamSuffixCache.clear();
    }

    g_env->DeleteLocalRef(scoreboard);
    g_env->DeleteLocalRef(world);
}

std::string ReadExplosionTimer(
    bool* outIsLikelyTntTagGame = nullptr,
    bool* outIsHypixelTntTagGame = nullptr) {
    if (outIsLikelyTntTagGame) *outIsLikelyTntTagGame = false;
    if (outIsHypixelTntTagGame) *outIsHypixelTntTagGame = false;
    if (!g_env || !InitScoreboardJNI()) return "";
    try {
        bool foundVoidTagMarker = false;
        bool foundHypixelTitle = false;
        std::vector<std::string> sidebarTitles = GetSidebarObjectiveDisplayNames();
        for (const std::string& title : sidebarTitles) {
            std::string cleanTitle = TrimAscii(ToLowerAscii(StripMinecraftFormattingCodes(title)));
            if (cleanTitle.find("voidtag") != std::string::npos ||
                cleanTitle.find("void tag") != std::string::npos) foundVoidTagMarker = true;
            if (cleanTitle.find("tnt tag") != std::string::npos) foundHypixelTitle = true;
        }

        bool foundExplosionLine = false;
        bool foundTntMarker = false;
        bool foundRoundLine = false;
        bool foundMapLine = false;
        bool foundPlayerLine = false;
        bool foundExactGameMarker = false;
        std::string firstExplosionLine;
        std::string timerLine;

        auto observeLine = [&](const std::string& line) {
            std::string cleanLower = TrimAscii(ToLowerAscii(StripMinecraftFormattingCodes(line)));
            bool hasExplosion = cleanLower.find("explosion") != std::string::npos;
            if (hasExplosion) {
                foundExplosionLine = true;
                if (firstExplosionLine.empty()) firstExplosionLine = line;
                if (timerLine.empty() && ScoreboardLineHasTimerNumber(line)) timerLine = line;
            }
            if (cleanLower.find("voidtag") != std::string::npos ||
                cleanLower.find("void tag") != std::string::npos) foundVoidTagMarker = true;
            if (cleanLower.find("tnt:") != std::string::npos) foundTntMarker = true;
            if (cleanLower.find("round") != std::string::npos) foundRoundLine = true;
            if (cleanLower.find("map") != std::string::npos) foundMapLine = true;
            if (cleanLower.find("players") != std::string::npos ||
                cleanLower.find("alive") != std::string::npos) foundPlayerLine = true;
            if (cleanLower == "game: tnt tag") foundExactGameMarker = true;
        };

        // Read the actual score entries attached to the active sidebar
        // objective. Void Tag can use raw score names without creating teams,
        // so the team-map fallback below cannot see those lines by itself.
        std::vector<std::string> sidebarLines = GetSidebarScoreboardLines();
        for (const std::string& line : sidebarLines) observeLine(line);

        jobjectArray arr = GetScoreboardTeamArray();
        if (arr) {
            jsize len = g_env->GetArrayLength(arr);
            for (jsize i = 0; i < len; i++) {
                jobject team = g_env->GetObjectArrayElement(arr, i);
                if (!team) continue;
                jstring teamNameStr = (jstring)g_env->GetObjectField(team, g_scoreboardJNI.fTeamName);
                std::string teamName = JStringToUtf8(teamNameStr);
                if (teamNameStr) g_env->DeleteLocalRef(teamNameStr);

                jstring prefixStr = (jstring)g_env->GetObjectField(team, g_scoreboardJNI.fTeamE);
                std::string prefix = JStringToUtf8(prefixStr);
                if (prefixStr) g_env->DeleteLocalRef(prefixStr);
                jstring suffixStr = (jstring)g_env->GetObjectField(team, g_scoreboardJNI.fTeamF);
                std::string suffix = JStringToUtf8(suffixStr);
                if (suffixStr) g_env->DeleteLocalRef(suffixStr);

                for (const std::string& line :
                    GetReadableScoreboardLineCandidates(team, teamName, prefix, suffix)) {
                    observeLine(line);
                }
                g_env->DeleteLocalRef(team);
            }
            g_env->DeleteLocalRef(arr);
        }

        bool foundHypixelMarker = foundHypixelTitle || foundExactGameMarker || foundTntMarker ||
            (foundRoundLine && foundMapLine && foundPlayerLine);

        static std::string lastLoggedMissingTimerTitle;
        std::string titleSignature;
        for (const std::string& title : sidebarTitles) {
            if (!titleSignature.empty()) titleSignature += " | ";
            titleSignature += TrimAscii(StripMinecraftFormattingCodes(title));
        }
        if (timerLine.empty() && !titleSignature.empty() &&
            titleSignature != lastLoggedMissingTimerTitle) {
            lastLoggedMissingTimerTitle = titleSignature;
            DebugLog("Timer scoreboard snapshot title=%s lines=%u teamsFallback=%d",
                titleSignature.c_str(),
                (unsigned int)sidebarLines.size(),
                arr ? 1 : 0);
            size_t loggedLineCount = (std::min)(sidebarLines.size(), (size_t)32);
            for (size_t i = 0; i < loggedLineCount; ++i) {
                std::string cleanLine = TrimAscii(StripMinecraftFormattingCodes(sidebarLines[i]));
                DebugLog("Timer scoreboard line[%u]=%s", (unsigned int)i, cleanLine.c_str());
            }
        }
        else if (!timerLine.empty()) {
            lastLoggedMissingTimerTitle.clear();
        }

        if (outIsLikelyTntTagGame) {
            *outIsLikelyTntTagGame = foundVoidTagMarker || foundHypixelMarker || foundExplosionLine;
        }
        if (outIsHypixelTntTagGame) *outIsHypixelTntTagGame = foundHypixelMarker;
        return timerLine.empty() ? firstExplosionLine : timerLine;
    }
    catch (...) { g_env->ExceptionClear(); }
    return "";
}

struct SpeedSlownessJNIContext {
    jclass mcClass = nullptr;
    jmethodID mGetMC = nullptr;
    jfieldID fGuiIngame = nullptr;
    jfieldID fThePlayer = nullptr;
    jmethodID mGetChatGui = nullptr;
    jfieldID fChatStoredLines = nullptr;
    jfieldID fChatVisibleLines = nullptr;
    jmethodID mChatLineGetComponent = nullptr;
    jmethodID mChatLineGetUpdateCounter = nullptr;
    jmethodID mChatLineGetLineId = nullptr;
    jmethodID mChatComponentGetUnformattedText = nullptr;
    jmethodID mChatComponentGetFormattedText = nullptr;
    jmethodID mListSize = nullptr;
    jmethodID mListGet = nullptr;
    jmethodID mListRemoveIndex = nullptr;
    jmethodID mEntityPlaySound = nullptr;
    bool inited = false;
    bool failed = false;
};

struct SpeedTransitionDiagnosticJNIContext {
    jclass mcClass = nullptr;
    jmethodID mGetMC = nullptr;
    jfieldID fThePlayer = nullptr;
    jfieldID fGameSettings = nullptr;
    jfieldID fMovementInput = nullptr;
    jfieldID fServerSprintState = nullptr;
    jfieldID fMoveForward = nullptr;
    jfieldID fKeyForward = nullptr;
    jfieldID fKeySprint = nullptr;
    jmethodID mKeyBindingIsDown = nullptr;
    jclass potionClass = nullptr;
    jfieldID fMoveSpeedPotion = nullptr;
    jmethodID mGetActivePotionEffect = nullptr;
    jmethodID mGetAmplifier = nullptr;
    jmethodID mGetDuration = nullptr;
    jmethodID mIsSprinting = nullptr;
    jfieldID fMotionX = nullptr;
    jfieldID fMotionZ = nullptr;
    jclass sharedAttributesClass = nullptr;
    jfieldID fMovementSpeedAttribute = nullptr;
    jmethodID mGetEntityAttribute = nullptr;
    jmethodID mGetAttributeValue = nullptr;
    bool inited = false;
};

struct ChatAlertLine {
    int updateCounter = 0;
    int lineId = 0;
    std::string text;
};

bool operator==(const ChatAlertLine& lhs, const ChatAlertLine& rhs) {
    return lhs.updateCounter == rhs.updateCounter &&
        lhs.lineId == rhs.lineId &&
        lhs.text == rhs.text;
}

SpeedSlownessJNIContext g_speedSlownessJNI;
SpeedTransitionDiagnosticJNIContext g_speedTransitionDiagnosticJNI;
int g_speedTransitionDiagnosticLastAmplifier = -2;
ULONGLONG g_speedTransitionDiagnosticCaptureUntilMs = 0;
ULONGLONG g_speedTransitionDiagnosticLastPollMs = 0;
ULONGLONG g_speedTransitionDiagnosticLastSampleMs = 0;
ULONGLONG g_speedTransitionDiagnosticLastInitAttemptMs = 0;
std::vector<ChatAlertLine> g_chatAlertSnapshot;
bool g_chatAlertSnapshotPrimed = false;
ULONGLONG g_lastChatAlertPollMs = 0;
ULONGLONG g_lastMutedVoiceMuteReminderFilterMs = 0;
std::mutex g_chatListAccessMutex;
jmethodID g_mutedVoiceS02PacketProcessMethod = nullptr;
jmethodID g_mutedVoiceS02PacketGetComponentMethod = nullptr;
jmethodID g_mutedVoicePlayClientHandleChatMethod = nullptr;
jmethodID g_mutedVoicePacketComponentGetUnformattedText = nullptr;
jmethodID g_mutedVoicePacketComponentGetFormattedText = nullptr;
jclass g_mutedVoiceS02PacketClass = nullptr;
jclass g_mutedVoicePacketFilterHelperClass = nullptr;
jlocation g_mutedVoiceS02PacketBreakpointLocation = 0;
volatile LONG g_mutedVoicePacketFilterInstalled = 0;
volatile LONG g_mutedVoicePacketFilterFailed = 0;
volatile LONG64 g_mutedVoiceLastBlockedMutePacketMs = 0;
volatile LONG64 g_mutedVoiceLastLocalChatAttemptMs = 0;
volatile LONG64 g_mutedVoiceLastForwardedSeparatorPacketMs = 0;
volatile LONG64 g_mutedVoicePendingSeparatorCleanupMs = 0;
constexpr ULONGLONG kMutedVoiceSeparatorPacketWindowMs = 1000;
constexpr ULONGLONG kMutedVoiceLeadingSeparatorConfirmWindowMs = 1500;
constexpr ULONGLONG kMutedVoicePendingSeparatorCleanupWindowMs = 1500;
constexpr int kMutedVoiceRecentSeparatorCleanupScanLimit = 8;
std::vector<unsigned char> g_mutedVoiceOriginalS02PacketBytes;
std::vector<unsigned char> g_mutedVoicePatchedS02PacketBytes;

void JNICALL SharedClassFileLoadHook(
    jvmtiEnv* jvmtiEnv,
    JNIEnv* env,
    jclass classBeingRedefined,
    jobject loader,
    const char* name,
    jobject protectionDomain,
    jint classDataLen,
    const unsigned char* classData,
    jint* newClassDataLen,
    unsigned char** newClassData);

void JNICALL MutedVoiceChatPacketBreakpoint(
    jvmtiEnv* jvmtiEnv,
    JNIEnv* env,
    jthread thread,
    jmethodID method,
    jlocation location);

void JNICALL MutedVoicePacketFilterDispatch(JNIEnv* env, jclass klass, jobject component, jobject handler, jobject packet);
bool InitMutedVoicePacketFilter(JNIEnv* env);
void ShutdownMutedVoicePacketFilter();
bool EnsureMutedVoicePacketFilterHelper(JNIEnv* env);
bool EnsureMutedVoiceS02PacketChatBytecode(JNIEnv* env);
bool RedefineMutedVoiceS02PacketChat(JNIEnv* env, const std::vector<unsigned char>& classBytes, const char* label);

bool InitSpeedSlownessJNI() {
    if (g_speedSlownessJNI.failed) {
        g_speedSlownessJNI.failed = false;
        g_speedSlownessJNI.inited = false;
    }
    if (g_speedSlownessJNI.inited) return true;
    if (!g_env) return false;

    g_speedSlownessJNI.inited = true;

    jclass mcClassLocal = FindClassLoose(g_env, "ave");
    if (!mcClassLocal) { g_env->ExceptionClear(); g_speedSlownessJNI.failed = true; return false; }
    g_speedSlownessJNI.mcClass = (jclass)g_env->NewGlobalRef(mcClassLocal);
    g_env->DeleteLocalRef(mcClassLocal);
    if (!g_speedSlownessJNI.mcClass) { g_env->ExceptionClear(); g_speedSlownessJNI.failed = true; return false; }

    g_speedSlownessJNI.mGetMC = GetStaticMethodIDCompat(g_env, g_speedSlownessJNI.mcClass, "A", "()Lave;");
    if (!g_speedSlownessJNI.mGetMC) { g_env->ExceptionClear(); g_speedSlownessJNI.failed = true; return false; }

    g_speedSlownessJNI.fGuiIngame = GetFieldIDCompat(g_env, g_speedSlownessJNI.mcClass, "q", "Lavo;");
    if (!g_speedSlownessJNI.fGuiIngame) { g_env->ExceptionClear(); g_speedSlownessJNI.failed = true; return false; }

    g_speedSlownessJNI.fThePlayer = GetFieldIDCompat(g_env, g_speedSlownessJNI.mcClass, "h", "Lbew;");
    if (!g_speedSlownessJNI.fThePlayer) { g_env->ExceptionClear(); g_speedSlownessJNI.failed = true; return false; }

    jclass guiIngameClass = FindClassLoose(g_env, "avo");
    if (!guiIngameClass) { g_env->ExceptionClear(); g_speedSlownessJNI.failed = true; return false; }
    g_speedSlownessJNI.mGetChatGui = GetMethodIDCompat(g_env, guiIngameClass, "d", "()Lavt;");
    g_env->DeleteLocalRef(guiIngameClass);
    if (!g_speedSlownessJNI.mGetChatGui) { g_env->ExceptionClear(); g_speedSlownessJNI.failed = true; return false; }

    jclass guiNewChatClass = FindClassLoose(g_env, "avt");
    if (!guiNewChatClass) {
        bool hadException = g_env->ExceptionCheck();
        if (hadException) g_env->ExceptionClear();
        DebugLog("Muted Voice hide init failed finding avt chat GUI class%s", hadException ? " (exception cleared)" : "");
        g_speedSlownessJNI.failed = true;
        return false;
    }
    g_speedSlownessJNI.fChatStoredLines = GetFieldIDCompat(g_env, guiNewChatClass, "h", "Ljava/util/List;");
    if (!g_speedSlownessJNI.fChatStoredLines || g_env->ExceptionCheck()) {
        bool hadException = g_env->ExceptionCheck();
        if (hadException) g_env->ExceptionClear();
        DebugLog("Muted Voice hide init failed resolving avt.h stored chat list%s", hadException ? " (exception cleared)" : "");
        g_env->DeleteLocalRef(guiNewChatClass);
        g_speedSlownessJNI.failed = true;
        return false;
    }
    DebugLog("Muted Voice hide init found avt.h stored chat list");

    g_speedSlownessJNI.fChatVisibleLines = GetFieldIDCompat(g_env, guiNewChatClass, "i", "Ljava/util/List;");
    if (!g_speedSlownessJNI.fChatVisibleLines || g_env->ExceptionCheck()) {
        bool hadException = g_env->ExceptionCheck();
        if (hadException) g_env->ExceptionClear();
        DebugLog("Muted Voice hide init failed resolving avt.i visible chat list%s", hadException ? " (exception cleared)" : "");
        g_env->DeleteLocalRef(guiNewChatClass);
        g_speedSlownessJNI.failed = true;
        return false;
    }
    DebugLog("Muted Voice hide init found avt.i visible chat list");
    g_env->DeleteLocalRef(guiNewChatClass);

    jclass chatLineClass = FindClassLoose(g_env, "ava");
    if (!chatLineClass) {
        bool hadException = g_env->ExceptionCheck();
        if (hadException) g_env->ExceptionClear();
        DebugLog("Muted Voice hide init failed finding ava chat line class%s", hadException ? " (exception cleared)" : "");
        g_speedSlownessJNI.failed = true;
        return false;
    }
    g_speedSlownessJNI.mChatLineGetComponent = GetMethodIDCompat(g_env, chatLineClass, "a", "()Leu;");
    g_speedSlownessJNI.mChatLineGetUpdateCounter = GetMethodIDCompat(g_env, chatLineClass, "b", "()I");
    g_speedSlownessJNI.mChatLineGetLineId = GetMethodIDCompat(g_env, chatLineClass, "c", "()I");
    g_env->DeleteLocalRef(chatLineClass);
    if (!g_speedSlownessJNI.mChatLineGetComponent ||
        !g_speedSlownessJNI.mChatLineGetUpdateCounter ||
        !g_speedSlownessJNI.mChatLineGetLineId) {
        bool hadException = g_env->ExceptionCheck();
        if (hadException) g_env->ExceptionClear();
        DebugLog("Muted Voice hide init failed resolving ava methods%s", hadException ? " (exception cleared)" : "");
        g_speedSlownessJNI.failed = true;
        return false;
    }
    DebugLog("Muted Voice hide init found ava.a() chat component getter");

    jclass componentClass = FindClassLoose(g_env, "eu");
    if (!componentClass) {
        bool hadException = g_env->ExceptionCheck();
        if (hadException) g_env->ExceptionClear();
        DebugLog("Muted Voice hide init failed finding eu chat component class%s", hadException ? " (exception cleared)" : "");
        g_speedSlownessJNI.failed = true;
        return false;
    }
    g_speedSlownessJNI.mChatComponentGetUnformattedText = GetMethodIDCompat(g_env, componentClass, "c", "()Ljava/lang/String;");
    if (!g_speedSlownessJNI.mChatComponentGetUnformattedText || g_env->ExceptionCheck()) {
        bool hadException = g_env->ExceptionCheck();
        if (hadException) g_env->ExceptionClear();
        DebugLog("Muted Voice hide init failed resolving eu.c() text%s", hadException ? " (exception cleared)" : "");
        g_env->DeleteLocalRef(componentClass);
        g_speedSlownessJNI.failed = true;
        return false;
    }
    DebugLog("Muted Voice hide init found eu.c() text");

    g_speedSlownessJNI.mChatComponentGetFormattedText = GetMethodIDCompat(g_env, componentClass, "d", "()Ljava/lang/String;");
    if (!g_speedSlownessJNI.mChatComponentGetFormattedText || g_env->ExceptionCheck()) {
        bool hadException = g_env->ExceptionCheck();
        if (hadException) g_env->ExceptionClear();
        DebugLog("Muted Voice hide init failed resolving eu.d() formatted text%s", hadException ? " (exception cleared)" : "");
        g_env->DeleteLocalRef(componentClass);
        g_speedSlownessJNI.failed = true;
        return false;
    }
    DebugLog("Muted Voice hide init found eu.d() formatted text");
    g_env->DeleteLocalRef(componentClass);

    jclass listClass = FindClassLoose(g_env, "java/util/List");
    if (!listClass) {
        bool hadException = g_env->ExceptionCheck();
        if (hadException) g_env->ExceptionClear();
        DebugLog("Muted Voice hide init failed finding java/util/List class%s", hadException ? " (exception cleared)" : "");
        g_speedSlownessJNI.failed = true;
        return false;
    }
    g_speedSlownessJNI.mListSize = GetMethodIDCompat(g_env, listClass, "size", "()I");
    g_speedSlownessJNI.mListGet = GetMethodIDCompat(g_env, listClass, "get", "(I)Ljava/lang/Object;");
    g_speedSlownessJNI.mListRemoveIndex = GetMethodIDCompat(g_env, listClass, "remove", "(I)Ljava/lang/Object;");
    g_env->DeleteLocalRef(listClass);
    if (!g_speedSlownessJNI.mListSize || !g_speedSlownessJNI.mListGet || !g_speedSlownessJNI.mListRemoveIndex) {
        bool hadException = g_env->ExceptionCheck();
        if (hadException) g_env->ExceptionClear();
        DebugLog("Muted Voice hide init failed resolving java/util/List methods%s", hadException ? " (exception cleared)" : "");
        g_speedSlownessJNI.failed = true;
        return false;
    }
    DebugLog("Muted Voice hide init found java/util/List.remove(int)");

    jclass entityClass = FindClassLoose(g_env, "pk");
    if (!entityClass) { g_env->ExceptionClear(); g_speedSlownessJNI.failed = true; return false; }
    g_speedSlownessJNI.mEntityPlaySound = GetMethodIDCompat(g_env, entityClass, "a", "(Ljava/lang/String;FF)V");
    g_env->DeleteLocalRef(entityClass);
    if (!g_speedSlownessJNI.mEntityPlaySound) { g_env->ExceptionClear(); g_speedSlownessJNI.failed = true; return false; }

    return true;
}

void ReleaseSpeedTransitionDiagnosticJNI() {
    if (g_env) {
        if (g_speedTransitionDiagnosticJNI.mcClass) {
            g_env->DeleteGlobalRef(g_speedTransitionDiagnosticJNI.mcClass);
        }
        if (g_speedTransitionDiagnosticJNI.potionClass) {
            g_env->DeleteGlobalRef(g_speedTransitionDiagnosticJNI.potionClass);
        }
        if (g_speedTransitionDiagnosticJNI.sharedAttributesClass) {
            g_env->DeleteGlobalRef(g_speedTransitionDiagnosticJNI.sharedAttributesClass);
        }
    }
    g_speedTransitionDiagnosticJNI = SpeedTransitionDiagnosticJNIContext{};
}

bool InitSpeedTransitionDiagnosticJNI() {
    if (g_speedTransitionDiagnosticJNI.inited) return true;
    if (!g_env) return false;

    ReleaseSpeedTransitionDiagnosticJNI();
    auto fail = [](const char* step) {
        if (g_env && g_env->ExceptionCheck()) g_env->ExceptionClear();
        DebugLog("SpeedDiag init failed at %s", step);
        ReleaseSpeedTransitionDiagnosticJNI();
        return false;
    };

    jclass localClass = FindClassLoose(g_env, "ave");
    if (!localClass) return fail("Minecraft class");
    g_speedTransitionDiagnosticJNI.mcClass = (jclass)g_env->NewGlobalRef(localClass);
    g_env->DeleteLocalRef(localClass);
    if (!g_speedTransitionDiagnosticJNI.mcClass) return fail("Minecraft global ref");

    g_speedTransitionDiagnosticJNI.mGetMC =
        GetStaticMethodIDCompat(g_env, g_speedTransitionDiagnosticJNI.mcClass, "A", "()Lave;");
    g_speedTransitionDiagnosticJNI.fThePlayer =
        GetFieldIDCompat(g_env, g_speedTransitionDiagnosticJNI.mcClass, "h", "Lbew;");
    g_speedTransitionDiagnosticJNI.fGameSettings =
        GetFieldIDCompat(g_env, g_speedTransitionDiagnosticJNI.mcClass, "t", "Lavh;");
    if (!g_speedTransitionDiagnosticJNI.mGetMC ||
        !g_speedTransitionDiagnosticJNI.fThePlayer ||
        !g_speedTransitionDiagnosticJNI.fGameSettings) {
        return fail("Minecraft members");
    }

    localClass = FindClassLoose(g_env, "bew");
    if (!localClass) return fail("EntityPlayerSP class");
    g_speedTransitionDiagnosticJNI.fMovementInput =
        GetFieldIDCompat(g_env, localClass, "b", "Lbeu;");
    g_speedTransitionDiagnosticJNI.fServerSprintState =
        GetFieldIDCompat(g_env, localClass, "bQ", "Z");
    g_env->DeleteLocalRef(localClass);
    if (!g_speedTransitionDiagnosticJNI.fMovementInput ||
        !g_speedTransitionDiagnosticJNI.fServerSprintState) {
        return fail("EntityPlayerSP fields");
    }

    localClass = FindClassLoose(g_env, "beu");
    if (!localClass) return fail("MovementInput class");
    g_speedTransitionDiagnosticJNI.fMoveForward = GetFieldIDCompat(g_env, localClass, "b", "F");
    g_env->DeleteLocalRef(localClass);
    if (!g_speedTransitionDiagnosticJNI.fMoveForward) return fail("MovementInput.moveForward");

    localClass = FindClassLoose(g_env, "avh");
    if (!localClass) return fail("GameSettings class");
    g_speedTransitionDiagnosticJNI.fKeyForward = GetFieldIDCompat(g_env, localClass, "Y", "Lavb;");
    g_speedTransitionDiagnosticJNI.fKeySprint = GetFieldIDCompat(g_env, localClass, "ae", "Lavb;");
    g_env->DeleteLocalRef(localClass);
    if (!g_speedTransitionDiagnosticJNI.fKeyForward ||
        !g_speedTransitionDiagnosticJNI.fKeySprint) {
        return fail("GameSettings key bindings");
    }

    localClass = FindClassLoose(g_env, "avb");
    if (!localClass) return fail("KeyBinding class");
    g_speedTransitionDiagnosticJNI.mKeyBindingIsDown =
        GetMethodIDCompat(g_env, localClass, "d", "()Z");
    g_env->DeleteLocalRef(localClass);
    if (!g_speedTransitionDiagnosticJNI.mKeyBindingIsDown) return fail("KeyBinding.isKeyDown");

    localClass = FindClassLoose(g_env, "pe");
    if (!localClass) return fail("Potion class");
    g_speedTransitionDiagnosticJNI.potionClass = (jclass)g_env->NewGlobalRef(localClass);
    g_env->DeleteLocalRef(localClass);
    if (!g_speedTransitionDiagnosticJNI.potionClass) return fail("Potion global ref");
    g_speedTransitionDiagnosticJNI.fMoveSpeedPotion =
        GetStaticFieldIDCompat(g_env, g_speedTransitionDiagnosticJNI.potionClass, "c", "Lpe;");
    if (!g_speedTransitionDiagnosticJNI.fMoveSpeedPotion) return fail("Potion.moveSpeed");

    localClass = FindClassLoose(g_env, "pr");
    if (!localClass) return fail("EntityLivingBase class");
    g_speedTransitionDiagnosticJNI.mGetActivePotionEffect =
        GetMethodIDCompat(g_env, localClass, "b", "(Lpe;)Lpf;");
    g_speedTransitionDiagnosticJNI.mGetEntityAttribute =
        GetMethodIDCompat(g_env, localClass, "a", "(Lqb;)Lqc;");
    g_env->DeleteLocalRef(localClass);
    if (!g_speedTransitionDiagnosticJNI.mGetActivePotionEffect ||
        !g_speedTransitionDiagnosticJNI.mGetEntityAttribute) {
        return fail("EntityLivingBase methods");
    }

    localClass = FindClassLoose(g_env, "pf");
    if (!localClass) return fail("PotionEffect class");
    g_speedTransitionDiagnosticJNI.mGetDuration = GetMethodIDCompat(g_env, localClass, "b", "()I");
    g_speedTransitionDiagnosticJNI.mGetAmplifier = GetMethodIDCompat(g_env, localClass, "c", "()I");
    g_env->DeleteLocalRef(localClass);
    if (!g_speedTransitionDiagnosticJNI.mGetDuration ||
        !g_speedTransitionDiagnosticJNI.mGetAmplifier) {
        return fail("PotionEffect methods");
    }

    localClass = FindClassLoose(g_env, "pk");
    if (!localClass) return fail("Entity class");
    g_speedTransitionDiagnosticJNI.mIsSprinting = GetMethodIDCompat(g_env, localClass, "aw", "()Z");
    g_speedTransitionDiagnosticJNI.fMotionX = GetFieldIDCompat(g_env, localClass, "v", "D");
    g_speedTransitionDiagnosticJNI.fMotionZ = GetFieldIDCompat(g_env, localClass, "x", "D");
    g_env->DeleteLocalRef(localClass);
    if (!g_speedTransitionDiagnosticJNI.mIsSprinting ||
        !g_speedTransitionDiagnosticJNI.fMotionX ||
        !g_speedTransitionDiagnosticJNI.fMotionZ) {
        return fail("Entity sprint/motion members");
    }

    localClass = FindClassLoose(g_env, "vy");
    if (!localClass) return fail("SharedMonsterAttributes class");
    g_speedTransitionDiagnosticJNI.sharedAttributesClass = (jclass)g_env->NewGlobalRef(localClass);
    g_env->DeleteLocalRef(localClass);
    if (!g_speedTransitionDiagnosticJNI.sharedAttributesClass) {
        return fail("SharedMonsterAttributes global ref");
    }
    g_speedTransitionDiagnosticJNI.fMovementSpeedAttribute =
        GetStaticFieldIDCompat(g_env, g_speedTransitionDiagnosticJNI.sharedAttributesClass, "d", "Lqb;");
    if (!g_speedTransitionDiagnosticJNI.fMovementSpeedAttribute) {
        return fail("SharedMonsterAttributes.movementSpeed");
    }

    localClass = FindClassLoose(g_env, "qc");
    if (!localClass) return fail("IAttributeInstance class");
    g_speedTransitionDiagnosticJNI.mGetAttributeValue = GetMethodIDCompat(g_env, localClass, "e", "()D");
    g_env->DeleteLocalRef(localClass);
    if (!g_speedTransitionDiagnosticJNI.mGetAttributeValue) return fail("IAttributeInstance.getAttributeValue");

    if (g_env->ExceptionCheck()) return fail("final exception check");
    g_speedTransitionDiagnosticJNI.inited = true;
    DebugLog("SpeedDiag read-only transition trace ready");
    return true;
}

void ResetSpeedTransitionDiagnosticCapture() {
    g_speedTransitionDiagnosticLastAmplifier = -2;
    g_speedTransitionDiagnosticCaptureUntilMs = 0;
    g_speedTransitionDiagnosticLastSampleMs = 0;
}

void PollSpeedTransitionDiagnostic(bool inTntTagGame) {
    ULONGLONG now = GetTickCount64();
    if (!inTntTagGame) {
        ResetSpeedTransitionDiagnosticCapture();
        return;
    }
    if (g_speedTransitionDiagnosticLastPollMs != 0 &&
        (now - g_speedTransitionDiagnosticLastPollMs) < kSpeedTransitionDiagnosticPollIntervalMs) {
        return;
    }
    g_speedTransitionDiagnosticLastPollMs = now;

    if (!g_speedTransitionDiagnosticJNI.inited) {
        if (g_speedTransitionDiagnosticLastInitAttemptMs != 0 &&
            (now - g_speedTransitionDiagnosticLastInitAttemptMs) < 1000) {
            return;
        }
        g_speedTransitionDiagnosticLastInitAttemptMs = now;
        if (!InitSpeedTransitionDiagnosticJNI()) return;
    }

    jobject mc = nullptr;
    jobject player = nullptr;
    jobject speedPotion = nullptr;
    jobject speedEffect = nullptr;
    jobject movementInput = nullptr;
    jobject gameSettings = nullptr;
    jobject keyForward = nullptr;
    jobject keySprint = nullptr;
    jobject movementSpeedAttribute = nullptr;
    jobject movementSpeedInstance = nullptr;
    auto cleanup = [&]() {
        if (movementSpeedInstance) g_env->DeleteLocalRef(movementSpeedInstance);
        if (movementSpeedAttribute) g_env->DeleteLocalRef(movementSpeedAttribute);
        if (keySprint) g_env->DeleteLocalRef(keySprint);
        if (keyForward) g_env->DeleteLocalRef(keyForward);
        if (gameSettings) g_env->DeleteLocalRef(gameSettings);
        if (movementInput) g_env->DeleteLocalRef(movementInput);
        if (speedEffect) g_env->DeleteLocalRef(speedEffect);
        if (speedPotion) g_env->DeleteLocalRef(speedPotion);
        if (player) g_env->DeleteLocalRef(player);
        if (mc) g_env->DeleteLocalRef(mc);
    };
    auto failed = [&]() {
        if (g_env->ExceptionCheck()) {
            g_env->ExceptionClear();
            return true;
        }
        return false;
    };

    mc = g_env->CallStaticObjectMethod(
        g_speedTransitionDiagnosticJNI.mcClass,
        g_speedTransitionDiagnosticJNI.mGetMC);
    if (failed() || !mc) { cleanup(); return; }
    player = g_env->GetObjectField(mc, g_speedTransitionDiagnosticJNI.fThePlayer);
    if (failed() || !player) { cleanup(); return; }
    speedPotion = g_env->GetStaticObjectField(
        g_speedTransitionDiagnosticJNI.potionClass,
        g_speedTransitionDiagnosticJNI.fMoveSpeedPotion);
    if (failed() || !speedPotion) { cleanup(); return; }
    speedEffect = g_env->CallObjectMethod(
        player,
        g_speedTransitionDiagnosticJNI.mGetActivePotionEffect,
        speedPotion);
    if (failed()) { cleanup(); return; }

    int amplifier = -1;
    int duration = -1;
    if (speedEffect) {
        amplifier = (int)g_env->CallIntMethod(speedEffect, g_speedTransitionDiagnosticJNI.mGetAmplifier);
        duration = (int)g_env->CallIntMethod(speedEffect, g_speedTransitionDiagnosticJNI.mGetDuration);
        if (failed()) { cleanup(); return; }
    }

    bool transitioned = false;
    if (g_speedTransitionDiagnosticLastAmplifier == -2) {
        g_speedTransitionDiagnosticLastAmplifier = amplifier;
    }
    else if (amplifier != g_speedTransitionDiagnosticLastAmplifier) {
        DebugLog(
            "SpeedDiag transition amp=%d->%d duration=%d captureMs=%llu",
            g_speedTransitionDiagnosticLastAmplifier,
            amplifier,
            duration,
            (unsigned long long)kSpeedTransitionDiagnosticCaptureMs);
        g_speedTransitionDiagnosticLastAmplifier = amplifier;
        g_speedTransitionDiagnosticCaptureUntilMs = now + kSpeedTransitionDiagnosticCaptureMs;
        g_speedTransitionDiagnosticLastSampleMs = 0;
        transitioned = true;
    }

    if (!transitioned && now > g_speedTransitionDiagnosticCaptureUntilMs) {
        cleanup();
        return;
    }
    if (!transitioned && g_speedTransitionDiagnosticLastSampleMs != 0 &&
        (now - g_speedTransitionDiagnosticLastSampleMs) < kSpeedTransitionDiagnosticSampleIntervalMs) {
        cleanup();
        return;
    }
    g_speedTransitionDiagnosticLastSampleMs = now;

    movementInput = g_env->GetObjectField(player, g_speedTransitionDiagnosticJNI.fMovementInput);
    gameSettings = g_env->GetObjectField(mc, g_speedTransitionDiagnosticJNI.fGameSettings);
    if (failed() || !movementInput || !gameSettings) { cleanup(); return; }
    keyForward = g_env->GetObjectField(gameSettings, g_speedTransitionDiagnosticJNI.fKeyForward);
    keySprint = g_env->GetObjectField(gameSettings, g_speedTransitionDiagnosticJNI.fKeySprint);
    movementSpeedAttribute = g_env->GetStaticObjectField(
        g_speedTransitionDiagnosticJNI.sharedAttributesClass,
        g_speedTransitionDiagnosticJNI.fMovementSpeedAttribute);
    if (failed() || !keyForward || !keySprint || !movementSpeedAttribute) { cleanup(); return; }
    movementSpeedInstance = g_env->CallObjectMethod(
        player,
        g_speedTransitionDiagnosticJNI.mGetEntityAttribute,
        movementSpeedAttribute);
    if (failed() || !movementSpeedInstance) { cleanup(); return; }

    jboolean sprinting = g_env->CallBooleanMethod(player, g_speedTransitionDiagnosticJNI.mIsSprinting);
    jboolean reportedSprinting = g_env->GetBooleanField(
        player,
        g_speedTransitionDiagnosticJNI.fServerSprintState);
    jfloat moveForward = g_env->GetFloatField(
        movementInput,
        g_speedTransitionDiagnosticJNI.fMoveForward);
    jboolean forwardDown = g_env->CallBooleanMethod(
        keyForward,
        g_speedTransitionDiagnosticJNI.mKeyBindingIsDown);
    jboolean sprintKeyDown = g_env->CallBooleanMethod(
        keySprint,
        g_speedTransitionDiagnosticJNI.mKeyBindingIsDown);
    jdouble motionX = g_env->GetDoubleField(player, g_speedTransitionDiagnosticJNI.fMotionX);
    jdouble motionZ = g_env->GetDoubleField(player, g_speedTransitionDiagnosticJNI.fMotionZ);
    jdouble movementAttribute = g_env->CallDoubleMethod(
        movementSpeedInstance,
        g_speedTransitionDiagnosticJNI.mGetAttributeValue);
    if (failed()) { cleanup(); return; }

    double horizontalMotion = std::sqrt((double)(motionX * motionX + motionZ * motionZ));
    ULONGLONG captureRemaining = now < g_speedTransitionDiagnosticCaptureUntilMs
        ? g_speedTransitionDiagnosticCaptureUntilMs - now
        : 0;
    DebugLog(
        "SpeedDiag sample amp=%d duration=%d moveAttr=%.6f sprint=%d reportedSprint=%d "
        "moveForward=%.3f W=%d sprintKey=%d motion=%.6f remainingMs=%llu",
        amplifier,
        duration,
        (double)movementAttribute,
        sprinting == JNI_TRUE ? 1 : 0,
        reportedSprinting == JNI_TRUE ? 1 : 0,
        (double)moveForward,
        forwardDown == JNI_TRUE ? 1 : 0,
        sprintKeyDown == JNI_TRUE ? 1 : 0,
        horizontalMotion,
        (unsigned long long)captureRemaining);
    cleanup();
}

jobject GetMinecraftClientForAlerts() {
    if (!g_env || !InitSpeedSlownessJNI()) return nullptr;

    jobject mc = g_env->CallStaticObjectMethod(g_speedSlownessJNI.mcClass, g_speedSlownessJNI.mGetMC);
    if (!mc || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (mc) g_env->DeleteLocalRef(mc);
        return nullptr;
    }
    return mc;
}

jobject GetMinecraftClientForAlerts(JNIEnv* env) {
    if (!env || !g_speedSlownessJNI.inited || g_speedSlownessJNI.failed ||
        !g_speedSlownessJNI.mcClass || !g_speedSlownessJNI.mGetMC) {
        return nullptr;
    }

    jobject mc = env->CallStaticObjectMethod(g_speedSlownessJNI.mcClass, g_speedSlownessJNI.mGetMC);
    if (!mc || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (mc) env->DeleteLocalRef(mc);
        return nullptr;
    }
    return mc;
}

std::string ToLowerAscii(const std::string& value) {
    std::string result = value;
    for (size_t i = 0; i < result.size(); ++i) {
        char c = result[i];
        if (c >= 'A' && c <= 'Z') result[i] = (char)(c - 'A' + 'a');
    }
    return result;
}

bool IsSpeed3AlertMessage(const std::string& text) {
    return ToLowerAscii(text).find("you got lucky and received speed 3") != std::string::npos;
}

bool IsSlownessAlertMessage(const std::string& text) {
    return ToLowerAscii(text).find("you got lucky and applied slowness") != std::string::npos;
}

bool PlayConfiguredAlertSound(int soundId, float volume) {
    if (!g_env || !InitSpeedSlownessJNI()) return false;

    jobject mc = GetMinecraftClientForAlerts();
    if (!mc) return false;

    jobject player = g_env->GetObjectField(mc, g_speedSlownessJNI.fThePlayer);
    g_env->DeleteLocalRef(mc);
    if (!player || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (player) g_env->DeleteLocalRef(player);
        return false;
    }

    jstring soundName = g_env->NewStringUTF(GetAlertSoundName(soundId));
    if (!soundName || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (soundName) g_env->DeleteLocalRef(soundName);
        g_env->DeleteLocalRef(player);
        return false;
    }

    g_env->CallVoidMethod(
        player,
        g_speedSlownessJNI.mEntityPlaySound,
        soundName,
        (jfloat)ClampFloat(volume, kAlertVolumeMin, kAlertVolumeMax),
        1.0f);

    bool ok = !g_env->ExceptionCheck();
    if (!ok) g_env->ExceptionClear();

    g_env->DeleteLocalRef(soundName);
    g_env->DeleteLocalRef(player);
    return ok;
}

void HandleChatAlertMessage(const std::string& text, bool allowAlerts) {
    if (!allowAlerts || text.empty()) return;

    if (IsSpeed3AlertMessage(text)) {
        PlayConfiguredAlertSound(g_speed3Sound, g_speed3Volume);
    }
    if (IsSlownessAlertMessage(text)) {
        PlayConfiguredAlertSound(g_slownessSound, g_slownessVolume);
    }
}

bool ReadChatLineTexts(JNIEnv* env, jobject chatLine, std::string& unformattedText, std::string& formattedText) {
    unformattedText.clear();
    formattedText.clear();
    if (!env || !chatLine || !g_speedSlownessJNI.mChatLineGetComponent ||
        !g_speedSlownessJNI.mChatComponentGetUnformattedText ||
        !g_speedSlownessJNI.mChatComponentGetFormattedText) {
        return false;
    }

    jobject component = env->CallObjectMethod(chatLine, g_speedSlownessJNI.mChatLineGetComponent);
    if (!component || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            DebugLog("Muted Voice hide read failed calling ava.a() (exception cleared)");
        }
        if (component) env->DeleteLocalRef(component);
        return false;
    }

    jstring unformattedString = (jstring)env->CallObjectMethod(component, g_speedSlownessJNI.mChatComponentGetUnformattedText);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        DebugLog("Muted Voice hide read failed calling eu.c() (exception cleared)");
        if (unformattedString) env->DeleteLocalRef(unformattedString);
        unformattedString = nullptr;
    }

    jstring formattedString = (jstring)env->CallObjectMethod(component, g_speedSlownessJNI.mChatComponentGetFormattedText);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        DebugLog("Muted Voice hide read failed calling eu.d() (exception cleared)");
        if (formattedString) env->DeleteLocalRef(formattedString);
        formattedString = nullptr;
    }

    unformattedText = JStringToUtf8(env, unformattedString);
    formattedText = JStringToUtf8(env, formattedString);
    if (unformattedString) env->DeleteLocalRef(unformattedString);
    if (formattedString) env->DeleteLocalRef(formattedString);
    env->DeleteLocalRef(component);
    return !unformattedText.empty() || !formattedText.empty();
}

bool IsMutedVoiceMuteReminderText(const std::string& text) {
    std::string cleanLower = ToLowerAscii(TrimAscii(StripMinecraftFormattingCodes(text)));
    return cleanLower.find("you are currently muted") != std::string::npos ||
        cleanLower.find("your mute will expire") != std::string::npos ||
        cleanLower.find("hypixel.net/mutes") != std::string::npos ||
        cleanLower.find("mute id:") != std::string::npos;
}

bool IsMutedVoiceMuteReminderCoreLine(const std::string& unformattedText, const std::string& formattedText) {
    return IsMutedVoiceMuteReminderText(unformattedText) || IsMutedVoiceMuteReminderText(formattedText);
}

bool IsMutedVoiceMuteReminderContinuationText(const std::string& text) {
    std::string cleanLower = ToLowerAscii(TrimAscii(StripMinecraftFormattingCodes(text)));
    return cleanLower.find("major chat infraction") != std::string::npos;
}

bool IsMutedVoiceMuteReminderContinuationLine(const std::string& unformattedText, const std::string& formattedText) {
    return IsMutedVoiceMuteReminderContinuationText(unformattedText) ||
        IsMutedVoiceMuteReminderContinuationText(formattedText);
}

bool IsMutedVoiceMuteReminderSeparatorText(const std::string& text) {
    std::string clean = TrimAscii(StripMinecraftFormattingCodes(text));
    if (clean.size() < 8) return false;

    int dashChars = 0;
    for (char c : clean) {
        if (c == '-') {
            ++dashChars;
            continue;
        }
        if (c == ' ') continue;
        return false;
    }
    return dashChars >= 8;
}

bool IsMutedVoiceMuteReminderSeparatorLine(const std::string& unformattedText, const std::string& formattedText) {
    return IsMutedVoiceMuteReminderSeparatorText(unformattedText) ||
        IsMutedVoiceMuteReminderSeparatorText(formattedText);
}

int RemoveMutedVoiceMuteReminderFromList(JNIEnv* env, jobject chatLines, const char* listLabel) {
    if (!env || !chatLines || !g_speedSlownessJNI.mListSize ||
        !g_speedSlownessJNI.mListGet || !g_speedSlownessJNI.mListRemoveIndex) {
        return 0;
    }

    jint listSize = env->CallIntMethod(chatLines, g_speedSlownessJNI.mListSize);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        DebugLog("Muted Voice hide failed reading %s size (exception cleared)", listLabel ? listLabel : "chat list");
        return 0;
    }

    int scanCount = (int)listSize;
    if (scanCount < 0) scanCount = 0;
    if (scanCount == 0) return 0;

    std::vector<std::string> unformattedLines(scanCount);
    std::vector<std::string> formattedLines(scanCount);
    std::vector<bool> core(scanCount, false);
    std::vector<bool> continuation(scanCount, false);
    std::vector<bool> separator(scanCount, false);
    std::vector<bool> remove(scanCount, false);

    for (int i = 0; i < scanCount; ++i) {
        jobject chatLine = env->CallObjectMethod(chatLines, g_speedSlownessJNI.mListGet, (jint)i);
        if (!chatLine || env->ExceptionCheck()) {
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                DebugLog("Muted Voice hide failed reading %s index %d (exception cleared)", listLabel ? listLabel : "chat list", i);
            }
            if (chatLine) env->DeleteLocalRef(chatLine);
            continue;
        }

        if (ReadChatLineTexts(env, chatLine, unformattedLines[i], formattedLines[i])) {
            core[i] = IsMutedVoiceMuteReminderCoreLine(unformattedLines[i], formattedLines[i]);
            continuation[i] = IsMutedVoiceMuteReminderContinuationLine(unformattedLines[i], formattedLines[i]);
            separator[i] = IsMutedVoiceMuteReminderSeparatorLine(unformattedLines[i], formattedLines[i]);
            if (core[i]) remove[i] = true;
        }
        env->DeleteLocalRef(chatLine);
    }

    for (int i = 0; i < scanCount; ++i) {
        if (!core[i]) continue;
        int first = i - 3;
        int last = i + 3;
        if (first < 0) first = 0;
        if (last >= scanCount) last = scanCount - 1;
        for (int j = first; j <= last; ++j) {
            if (core[j] || continuation[j] || separator[j]) remove[j] = true;
        }
    }

    int removedCount = 0;
    for (int i = scanCount - 1; i >= 0; --i) {
        if (!remove[i]) continue;

        jobject removed = env->CallObjectMethod(chatLines, g_speedSlownessJNI.mListRemoveIndex, (jint)i);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            DebugLog("Muted Voice hide failed removing %s index %d (exception cleared)", listLabel ? listLabel : "chat list", i);
            if (removed) env->DeleteLocalRef(removed);
            continue;
        }
        if (removed) env->DeleteLocalRef(removed);
        ++removedCount;
    }

    return removedCount;
}

int RemoveRecentMutedVoiceMuteReminderSeparatorFromList(JNIEnv* env, jobject chatLines, const char* listLabel) {
    if (!env || !chatLines || !g_speedSlownessJNI.mListSize ||
        !g_speedSlownessJNI.mListGet || !g_speedSlownessJNI.mListRemoveIndex) {
        return 0;
    }

    jint listSize = env->CallIntMethod(chatLines, g_speedSlownessJNI.mListSize);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        DebugLog("Muted Voice hide failed reading recent %s size (exception cleared)", listLabel ? listLabel : "chat list");
        return 0;
    }

    int scanCount = (int)listSize;
    if (scanCount < 0) scanCount = 0;
    if (scanCount > kMutedVoiceRecentSeparatorCleanupScanLimit) {
        scanCount = kMutedVoiceRecentSeparatorCleanupScanLimit;
    }
    if (scanCount == 0) return 0;

    std::vector<bool> remove(scanCount, false);
    for (int i = 0; i < scanCount; ++i) {
        jobject chatLine = env->CallObjectMethod(chatLines, g_speedSlownessJNI.mListGet, (jint)i);
        if (!chatLine || env->ExceptionCheck()) {
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                DebugLog("Muted Voice hide failed reading recent %s index %d (exception cleared)", listLabel ? listLabel : "chat list", i);
            }
            if (chatLine) env->DeleteLocalRef(chatLine);
            continue;
        }

        std::string unformattedText;
        std::string formattedText;
        if (ReadChatLineTexts(env, chatLine, unformattedText, formattedText) &&
            IsMutedVoiceMuteReminderSeparatorLine(unformattedText, formattedText)) {
            remove[i] = true;
            env->DeleteLocalRef(chatLine);
            break;
        }
        env->DeleteLocalRef(chatLine);
    }

    int removedCount = 0;
    for (int i = scanCount - 1; i >= 0; --i) {
        if (!remove[i]) continue;

        jobject removed = env->CallObjectMethod(chatLines, g_speedSlownessJNI.mListRemoveIndex, (jint)i);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            DebugLog("Muted Voice hide failed removing recent %s index %d (exception cleared)", listLabel ? listLabel : "chat list", i);
            if (removed) env->DeleteLocalRef(removed);
            continue;
        }
        if (removed) env->DeleteLocalRef(removed);
        ++removedCount;
    }

    return removedCount;
}

int RemoveMutedVoiceMuteReminderFromChatGui(JNIEnv* env, jobject chatGui, jfieldID listField, const char* listLabel) {
    if (!env || !chatGui || !listField) return 0;

    jobject chatLines = env->GetObjectField(chatGui, listField);
    if (!chatLines || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            DebugLog("Muted Voice hide failed reading %s field (exception cleared)", listLabel ? listLabel : "chat list");
        }
        if (chatLines) env->DeleteLocalRef(chatLines);
        return 0;
    }

    int removedCount = RemoveMutedVoiceMuteReminderFromList(env, chatLines, listLabel);
    env->DeleteLocalRef(chatLines);
    return removedCount;
}

int RemoveRecentMutedVoiceMuteReminderSeparatorFromChatGui(JNIEnv* env, jobject chatGui, jfieldID listField, const char* listLabel) {
    if (!env || !chatGui || !listField) return 0;

    jobject chatLines = env->GetObjectField(chatGui, listField);
    if (!chatLines || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            DebugLog("Muted Voice hide failed reading recent %s field (exception cleared)", listLabel ? listLabel : "chat list");
        }
        if (chatLines) env->DeleteLocalRef(chatLines);
        return 0;
    }

    int removedCount = RemoveRecentMutedVoiceMuteReminderSeparatorFromList(env, chatLines, listLabel);
    env->DeleteLocalRef(chatLines);
    return removedCount;
}

std::string ReadChatComponentText(JNIEnv* env, jobject component, jmethodID method, const char* label) {
    if (!env || !component || !method) return "";

    jstring textString = (jstring)env->CallObjectMethod(component, method);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        DebugLog("Muted Voice packet filter failed calling %s (exception cleared)", label ? label : "chat component text");
        if (textString) env->DeleteLocalRef(textString);
        return "";
    }

    std::string text = JStringToUtf8(env, textString);
    if (textString) env->DeleteLocalRef(textString);
    return text;
}

bool ShouldBlockMutedVoiceChatComponent(JNIEnv* env, jobject component, bool* outCoreLine, bool* outSeparatorLine) {
    if (outCoreLine) *outCoreLine = false;
    if (outSeparatorLine) *outSeparatorLine = false;

    if (!env || !component ||
        !g_mutedVoicePacketComponentGetUnformattedText ||
        !g_mutedVoicePacketComponentGetFormattedText) {
        return false;
    }

    std::string unformattedText = ReadChatComponentText(
        env,
        component,
        g_mutedVoicePacketComponentGetUnformattedText,
        "eu.c()");
    std::string formattedText = ReadChatComponentText(
        env,
        component,
        g_mutedVoicePacketComponentGetFormattedText,
        "eu.d()");

    bool coreLine = IsMutedVoiceMuteReminderCoreLine(unformattedText, formattedText);
    bool continuationLine = IsMutedVoiceMuteReminderContinuationLine(unformattedText, formattedText);
    bool separatorLine = IsMutedVoiceMuteReminderSeparatorLine(unformattedText, formattedText);
    if (outCoreLine) *outCoreLine = coreLine;
    if (outSeparatorLine) *outSeparatorLine = separatorLine;
    if (coreLine) return true;

    if (continuationLine || separatorLine) {
        LONG64 lastBlockedMs = InterlockedCompareExchange64(&g_mutedVoiceLastBlockedMutePacketMs, 0, 0);
        ULONGLONG now = GetTickCount64();
        if (lastBlockedMs > 0 && now >= (ULONGLONG)lastBlockedMs &&
            (now - (ULONGLONG)lastBlockedMs) <= kMutedVoiceSeparatorPacketWindowMs) {
            return true;
        }
        if (separatorLine) {
            InterlockedExchange64(&g_mutedVoiceLastForwardedSeparatorPacketMs, (LONG64)now);
        }
    }

    return false;
}

bool ShouldBlockMutedVoiceChatPacket(JNIEnv* env, jobject packet, bool* outCoreLine, bool* outSeparatorLine) {
    if (outCoreLine) *outCoreLine = false;
    if (outSeparatorLine) *outSeparatorLine = false;

    if (!env || !packet || !g_mutedVoiceS02PacketGetComponentMethod) {
        return false;
    }

    jobject component = env->CallObjectMethod(packet, g_mutedVoiceS02PacketGetComponentMethod);
    if (!component || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            DebugLog("Muted Voice packet filter failed calling fy.a() component getter (exception cleared)");
        }
        if (component) env->DeleteLocalRef(component);
        return false;
    }

    bool shouldBlock = ShouldBlockMutedVoiceChatComponent(env, component, outCoreLine, outSeparatorLine);
    env->DeleteLocalRef(component);
    return shouldBlock;
}

void NoteMutedVoiceCorePacketBlocked() {
    ULONGLONG now = GetTickCount64();
    LONG64 lastForwardedSeparatorMs = InterlockedCompareExchange64(&g_mutedVoiceLastForwardedSeparatorPacketMs, 0, 0);

    InterlockedExchange64(&g_mutedVoiceLastBlockedMutePacketMs, (LONG64)now);
    if (lastForwardedSeparatorMs > 0 && now >= (ULONGLONG)lastForwardedSeparatorMs &&
        (now - (ULONGLONG)lastForwardedSeparatorMs) <= kMutedVoiceLeadingSeparatorConfirmWindowMs) {
        InterlockedExchange64(&g_mutedVoicePendingSeparatorCleanupMs, (LONG64)now);
    }
}

void JNICALL MutedVoicePacketFilterDispatch(JNIEnv* env, jclass klass, jobject component, jobject handler, jobject packet) {
    (void)klass;
    bool shouldBlock = false;
    bool coreLine = false;
    bool separatorLine = false;

    if (env && g_guiExtrasMutedVoice && g_guiExtrasMutedVoiceHideMuteReminder) {
        shouldBlock = ShouldBlockMutedVoiceChatComponent(env, component, &coreLine, &separatorLine);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            DebugLog("Muted Voice packet filter native dispatch cleared exception after packet inspection");
            shouldBlock = false;
        }
        if (shouldBlock && coreLine) {
            NoteMutedVoiceCorePacketBlocked();
        }
    }

    if (shouldBlock) {
        DebugLog("Muted Voice packet filter blocked inbound S02PacketChat core=%d separator=%d",
            coreLine ? 1 : 0,
            separatorLine ? 1 : 0);
        return;
    }

    if (!env || !handler || !packet || !g_mutedVoicePlayClientHandleChatMethod) {
        DebugLog("Muted Voice packet filter native dispatch missing handler=%p packet=%p method=%p",
            handler,
            packet,
            g_mutedVoicePlayClientHandleChatMethod);
        return;
    }

    env->CallVoidMethod(handler, g_mutedVoicePlayClientHandleChatMethod, packet);
}

void FilterMutedVoiceMuteReminderChat(JNIEnv* env, bool enabled) {
    if (!enabled || !env || !g_speedSlownessJNI.inited || g_speedSlownessJNI.failed) return;

    ULONGLONG now = GetTickCount64();
    if (g_lastMutedVoiceMuteReminderFilterMs != 0 && (now - g_lastMutedVoiceMuteReminderFilterMs) < 50) return;
    g_lastMutedVoiceMuteReminderFilterMs = now;

    std::lock_guard<std::mutex> lock(g_chatListAccessMutex);

    jobject mc = GetMinecraftClientForAlerts(env);
    if (!mc) return;

    jobject guiIngame = env->GetObjectField(mc, g_speedSlownessJNI.fGuiIngame);
    if (!guiIngame || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            DebugLog("Muted Voice hide failed reading Minecraft.guiIngame (exception cleared)");
        }
        if (guiIngame) env->DeleteLocalRef(guiIngame);
        env->DeleteLocalRef(mc);
        return;
    }

    jobject chatGui = env->CallObjectMethod(guiIngame, g_speedSlownessJNI.mGetChatGui);
    if (!chatGui || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            DebugLog("Muted Voice hide failed calling GuiIngame.getChatGUI (exception cleared)");
        }
        if (chatGui) env->DeleteLocalRef(chatGui);
        env->DeleteLocalRef(guiIngame);
        env->DeleteLocalRef(mc);
        return;
    }

    LONG64 pendingSeparatorCleanupMs = InterlockedCompareExchange64(&g_mutedVoicePendingSeparatorCleanupMs, 0, 0);
    bool cleanupRecentSeparator = pendingSeparatorCleanupMs > 0 &&
        now >= (ULONGLONG)pendingSeparatorCleanupMs &&
        (now - (ULONGLONG)pendingSeparatorCleanupMs) <= kMutedVoicePendingSeparatorCleanupWindowMs;
    int removedRecentVisible = 0;
    int removedRecentStored = 0;
    if (cleanupRecentSeparator) {
        removedRecentVisible = RemoveRecentMutedVoiceMuteReminderSeparatorFromChatGui(
            env,
            chatGui,
            g_speedSlownessJNI.fChatVisibleLines,
            "visible chat avt.i");
        removedRecentStored = RemoveRecentMutedVoiceMuteReminderSeparatorFromChatGui(
            env,
            chatGui,
            g_speedSlownessJNI.fChatStoredLines,
            "stored chat avt.h");
        InterlockedCompareExchange64(&g_mutedVoicePendingSeparatorCleanupMs, 0, pendingSeparatorCleanupMs);
    }
    else if (pendingSeparatorCleanupMs > 0) {
        InterlockedCompareExchange64(&g_mutedVoicePendingSeparatorCleanupMs, 0, pendingSeparatorCleanupMs);
    }

    int removedVisible = RemoveMutedVoiceMuteReminderFromChatGui(
        env,
        chatGui,
        g_speedSlownessJNI.fChatVisibleLines,
        "visible chat avt.i");
    int removedStored = RemoveMutedVoiceMuteReminderFromChatGui(
        env,
        chatGui,
        g_speedSlownessJNI.fChatStoredLines,
        "stored chat avt.h");
    if (removedVisible > 0) {
        DebugLog("Muted Voice hide removed %d lines from visible chat avt.i", removedVisible);
    }
    if (removedStored > 0) {
        DebugLog("Muted Voice hide removed %d lines from stored chat avt.h", removedStored);
    }
    if (removedRecentVisible > 0) {
        DebugLog("Muted Voice hide removed %d recent separator lines from visible chat avt.i", removedRecentVisible);
    }
    if (removedRecentStored > 0) {
        DebugLog("Muted Voice hide removed %d recent separator lines from stored chat avt.h", removedRecentStored);
    }
    if (removedVisible > 0 || removedStored > 0 || removedRecentVisible > 0 || removedRecentStored > 0) {
        g_chatAlertSnapshotPrimed = false;
    }

    env->DeleteLocalRef(chatGui);
    env->DeleteLocalRef(guiIngame);
    env->DeleteLocalRef(mc);
}

bool ReadCurrentChatSnapshot(std::vector<ChatAlertLine>& snapshot) {
    snapshot.clear();
    if (!g_env || !InitSpeedSlownessJNI()) return false;

    std::lock_guard<std::mutex> lock(g_chatListAccessMutex);

    jobject mc = GetMinecraftClientForAlerts();
    if (!mc) return false;

    jobject guiIngame = g_env->GetObjectField(mc, g_speedSlownessJNI.fGuiIngame);
    if (!guiIngame || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (guiIngame) g_env->DeleteLocalRef(guiIngame);
        g_env->DeleteLocalRef(mc);
        return false;
    }

    jobject chatGui = g_env->CallObjectMethod(guiIngame, g_speedSlownessJNI.mGetChatGui);
    if (!chatGui || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (chatGui) g_env->DeleteLocalRef(chatGui);
        g_env->DeleteLocalRef(guiIngame);
        g_env->DeleteLocalRef(mc);
        return false;
    }

    jobject chatLines = g_env->GetObjectField(chatGui, g_speedSlownessJNI.fChatStoredLines);
    if (!chatLines || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (chatLines) g_env->DeleteLocalRef(chatLines);
        g_env->DeleteLocalRef(chatGui);
        g_env->DeleteLocalRef(guiIngame);
        g_env->DeleteLocalRef(mc);
        return false;
    }

    jint listSize = g_env->CallIntMethod(chatLines, g_speedSlownessJNI.mListSize);
    if (g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        g_env->DeleteLocalRef(chatLines);
        g_env->DeleteLocalRef(chatGui);
        g_env->DeleteLocalRef(guiIngame);
        g_env->DeleteLocalRef(mc);
        return false;
    }

    int readCount = listSize;
    if (readCount < 0) readCount = 0;
    if (readCount > kChatAlertSnapshotLimit) readCount = kChatAlertSnapshotLimit;
    snapshot.reserve(readCount);

    bool ok = true;
    for (int i = 0; i < readCount; ++i) {
        jobject chatLine = g_env->CallObjectMethod(chatLines, g_speedSlownessJNI.mListGet, (jint)i);
        if (!chatLine || g_env->ExceptionCheck()) {
            g_env->ExceptionClear();
            if (chatLine) g_env->DeleteLocalRef(chatLine);
            ok = false;
            break;
        }

        jobject component = g_env->CallObjectMethod(chatLine, g_speedSlownessJNI.mChatLineGetComponent);
        if (!component || g_env->ExceptionCheck()) {
            g_env->ExceptionClear();
            if (component) g_env->DeleteLocalRef(component);
            g_env->DeleteLocalRef(chatLine);
            ok = false;
            break;
        }

        jint updateCounter = g_env->CallIntMethod(chatLine, g_speedSlownessJNI.mChatLineGetUpdateCounter);
        if (g_env->ExceptionCheck()) {
            g_env->ExceptionClear();
            g_env->DeleteLocalRef(component);
            g_env->DeleteLocalRef(chatLine);
            ok = false;
            break;
        }

        jint lineId = g_env->CallIntMethod(chatLine, g_speedSlownessJNI.mChatLineGetLineId);
        if (g_env->ExceptionCheck()) {
            g_env->ExceptionClear();
            g_env->DeleteLocalRef(component);
            g_env->DeleteLocalRef(chatLine);
            ok = false;
            break;
        }

        jstring text = (jstring)g_env->CallObjectMethod(component, g_speedSlownessJNI.mChatComponentGetUnformattedText);
        if (g_env->ExceptionCheck()) {
            g_env->ExceptionClear();
            if (text) g_env->DeleteLocalRef(text);
            g_env->DeleteLocalRef(component);
            g_env->DeleteLocalRef(chatLine);
            ok = false;
            break;
        }

        ChatAlertLine entry;
        entry.updateCounter = (int)updateCounter;
        entry.lineId = (int)lineId;
        entry.text = JStringToUtf8(text);
        snapshot.push_back(entry);
        if (text) g_env->DeleteLocalRef(text);
        g_env->DeleteLocalRef(component);
        g_env->DeleteLocalRef(chatLine);
    }

    g_env->DeleteLocalRef(chatLines);
    g_env->DeleteLocalRef(chatGui);
    g_env->DeleteLocalRef(guiIngame);
    g_env->DeleteLocalRef(mc);

    if (!ok) snapshot.clear();
    return ok;
}

bool SnapshotContainsChatAlertLine(const std::vector<ChatAlertLine>& snapshot, const ChatAlertLine& line) {
    for (size_t i = 0; i < snapshot.size(); ++i) {
        if (snapshot[i] == line) return true;
    }
    return false;
}

void PollSpeedSlownessChatAlerts(bool allowAlerts) {
    ULONGLONG now = GetTickCount64();
    if (g_lastChatAlertPollMs != 0 && (now - g_lastChatAlertPollMs) < kChatAlertPollIntervalMs) return;
    g_lastChatAlertPollMs = now;

    std::vector<ChatAlertLine> currentSnapshot;
    if (!ReadCurrentChatSnapshot(currentSnapshot)) return;

    if (!g_chatAlertSnapshotPrimed) {
        g_chatAlertSnapshot = currentSnapshot;
        g_chatAlertSnapshotPrimed = true;
        return;
    }

    if (currentSnapshot == g_chatAlertSnapshot) return;

    for (size_t i = currentSnapshot.size(); i > 0; --i) {
        const ChatAlertLine& line = currentSnapshot[i - 1];
        if (!SnapshotContainsChatAlertLine(g_chatAlertSnapshot, line)) {
            HandleChatAlertMessage(line.text, allowAlerts);
        }
    }

    g_chatAlertSnapshot = currentSnapshot;
}

// =============================================================
// Snaplook (JNI perspective control)
// =============================================================
// ave.t (avh GameSettings) -> avh.aB (int thirdPersonView: 0/1/2)
static jclass g_slMcClass = nullptr;
static jmethodID g_slGetMC = nullptr;
static jfieldID g_slGameSettings = nullptr;
static jfieldID g_slThirdPersonView = nullptr;
static jfieldID g_slRenderGlobal = nullptr;
static jfieldID g_slEntityRenderer = nullptr;
static jmethodID g_slGetRenderViewEntity = nullptr;
static jmethodID g_slReloadRenderers = nullptr;
static jmethodID g_slMarkDisplayListEntitiesDirty = nullptr;
static jmethodID g_slLoadEntityShader = nullptr;
static bool g_slInited = false;
static bool g_slFailed = false;
JNIEnv* GetJNIEnvForCurrentThread();

bool InitSnaplookJNI() {
    if (g_slFailed) {
        g_slFailed = false;
        g_slInited = false;
    }
    if (g_slInited) return true;
    if (!g_env) return false;

    auto fail = [](const char* label) -> bool {
        if (g_env && g_env->ExceptionCheck()) g_env->ExceptionClear();
        DebugLog("Snaplook JNI init failed: %s", label);
        g_slFailed = true;
        return false;
    };

    jclass mcClassLocal = FindClassLoose(g_env, "ave");
    if (!mcClassLocal) return fail("FindClass ave");
    g_slMcClass = (jclass)g_env->NewGlobalRef(mcClassLocal);
    g_env->DeleteLocalRef(mcClassLocal);
    if (!g_slMcClass || g_env->ExceptionCheck()) return fail("NewGlobalRef ave");

    g_slGetMC = GetStaticMethodIDCompat(g_env, g_slMcClass, "A", "()Lave;");
    if (!g_slGetMC || g_env->ExceptionCheck()) return fail("ave.A()Lave;");

    g_slGameSettings = GetFieldIDCompat(g_env, g_slMcClass, "t", "Lavh;");
    if (!g_slGameSettings || g_env->ExceptionCheck()) return fail("ave.t Lavh;");

    jclass gsClass = FindClassLoose(g_env, "avh");
    if (!gsClass) return fail("FindClass avh");

    g_slThirdPersonView = GetFieldIDCompat(g_env, gsClass, "aB", "I");
    g_env->DeleteLocalRef(gsClass);
    if (!g_slThirdPersonView || g_env->ExceptionCheck()) return fail("avh.aB I");

    g_slRenderGlobal = GetFieldIDCompat(g_env, g_slMcClass, "g", "Lbfr;");
    if (g_env->ExceptionCheck()) { g_env->ExceptionClear(); g_slRenderGlobal = nullptr; }

    g_slEntityRenderer = GetFieldIDCompat(g_env, g_slMcClass, "o", "Lbfk;");
    if (g_env->ExceptionCheck()) { g_env->ExceptionClear(); g_slEntityRenderer = nullptr; }

    g_slGetRenderViewEntity = GetMethodIDCompat(g_env, g_slMcClass, "ac", "()Lpk;");
    if (g_env->ExceptionCheck()) { g_env->ExceptionClear(); g_slGetRenderViewEntity = nullptr; }

    jclass renderGlobalClass = FindClassLoose(g_env, "bfr");
    if (renderGlobalClass && !g_env->ExceptionCheck()) {
        g_slReloadRenderers = GetMethodIDCompat(g_env, renderGlobalClass, "a", "()V");
        if (g_env->ExceptionCheck()) { g_env->ExceptionClear(); g_slReloadRenderers = nullptr; }
        g_slMarkDisplayListEntitiesDirty = GetMethodIDCompat(g_env, renderGlobalClass, "m", "()V");
        if (g_env->ExceptionCheck()) { g_env->ExceptionClear(); g_slMarkDisplayListEntitiesDirty = nullptr; }
        g_env->DeleteLocalRef(renderGlobalClass);
    }
    else {
        g_env->ExceptionClear();
    }

    jclass entityRendererClass = FindClassLoose(g_env, "bfk");
    if (entityRendererClass && !g_env->ExceptionCheck()) {
        g_slLoadEntityShader = GetMethodIDCompat(g_env, entityRendererClass, "a", "(Lpk;)V");
        if (g_env->ExceptionCheck()) { g_env->ExceptionClear(); g_slLoadEntityShader = nullptr; }
        g_env->DeleteLocalRef(entityRendererClass);
    }
    else {
        g_env->ExceptionClear();
    }

    g_slInited = true;
    return true;
}

int GetPerspective() {
    if (!g_env || !g_slGameSettings || !g_slThirdPersonView) return -1;
    jobject mc = g_env->CallStaticObjectMethod(g_slMcClass, g_slGetMC);
    if (!mc || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (mc) g_env->DeleteLocalRef(mc);
        return -1;
    }
    jobject gs = g_env->GetObjectField(mc, g_slGameSettings);
    if (!gs || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (gs) g_env->DeleteLocalRef(gs);
        g_env->DeleteLocalRef(mc);
        return -1;
    }
    jint val = g_env->GetIntField(gs, g_slThirdPersonView);
    if (g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        g_env->DeleteLocalRef(gs);
        g_env->DeleteLocalRef(mc);
        return -1;
    }
    g_env->DeleteLocalRef(gs);
    g_env->DeleteLocalRef(mc);
    return (int)val;
}

bool RefreshPerspectiveRenderingWithEnv(JNIEnv* env, bool reloadWorldRenderers = false) {
    if (!env || !g_slMcClass || !g_slGetMC || !g_slGameSettings || !g_slThirdPersonView) return false;

    jobject mc = env->CallStaticObjectMethod(g_slMcClass, g_slGetMC);
    if (!mc || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (mc) env->DeleteLocalRef(mc);
        return false;
    }

    jobject gs = env->GetObjectField(mc, g_slGameSettings);
    if (!gs || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (gs) env->DeleteLocalRef(gs);
        env->DeleteLocalRef(mc);
        return false;
    }

    jint perspective = env->GetIntField(gs, g_slThirdPersonView);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(gs);
        env->DeleteLocalRef(mc);
        return false;
    }

    if (g_slEntityRenderer && g_slLoadEntityShader) {
        jobject entityRenderer = env->GetObjectField(mc, g_slEntityRenderer);
        if (!env->ExceptionCheck() && entityRenderer) {
            jobject renderViewEntity = nullptr;
            if (perspective == 0 && g_slGetRenderViewEntity) {
                renderViewEntity = env->CallObjectMethod(mc, g_slGetRenderViewEntity);
                if (env->ExceptionCheck()) {
                    env->ExceptionClear();
                    renderViewEntity = nullptr;
                }
            }

            env->CallVoidMethod(entityRenderer, g_slLoadEntityShader, renderViewEntity);
            if (env->ExceptionCheck()) env->ExceptionClear();

            if (renderViewEntity) env->DeleteLocalRef(renderViewEntity);
            env->DeleteLocalRef(entityRenderer);
        }
        else if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
    }

    // F5 only marks entity display lists dirty. Full renderer reloads rebuild chunks
    // and cause visible frame spikes on short snaplook taps.
    jmethodID renderGlobalRefresh = reloadWorldRenderers ? g_slReloadRenderers : g_slMarkDisplayListEntitiesDirty;
    if (g_slRenderGlobal && renderGlobalRefresh) {
        jobject renderGlobal = env->GetObjectField(mc, g_slRenderGlobal);
        if (!env->ExceptionCheck() && renderGlobal) {
            env->CallVoidMethod(renderGlobal, renderGlobalRefresh);
            if (env->ExceptionCheck()) env->ExceptionClear();
            env->DeleteLocalRef(renderGlobal);
        }
        else if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
    }

    env->DeleteLocalRef(gs);
    env->DeleteLocalRef(mc);
    InterlockedExchange(&g_perspectiveRenderingSyncedValue, (LONG)perspective);
    return true;
}

void ProcessPendingPerspectiveRefresh() {
    if (InterlockedCompareExchange(&g_perspectiveRefreshPending, 0, 0) == 0) return;

    JNIEnv* env = GetJNIEnvForCurrentThread();
    if (!env) return;
    if (!RefreshPerspectiveRenderingWithEnv(env)) return;

    InterlockedExchange(&g_perspectiveRefreshPending, 0);
}

bool SetPerspective(int value) {
    if (!g_env || !g_slGameSettings || !g_slThirdPersonView) return false;
    jobject mc = g_env->CallStaticObjectMethod(g_slMcClass, g_slGetMC);
    if (!mc || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (mc) g_env->DeleteLocalRef(mc);
        return false;
    }
    jobject gs = g_env->GetObjectField(mc, g_slGameSettings);
    if (!gs || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (gs) g_env->DeleteLocalRef(gs);
        g_env->DeleteLocalRef(mc);
        return false;
    }
    jint current = g_env->GetIntField(gs, g_slThirdPersonView);
    if (g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        g_env->DeleteLocalRef(gs);
        g_env->DeleteLocalRef(mc);
        return false;
    }
    if (current == (jint)value) {
        g_env->DeleteLocalRef(gs);
        g_env->DeleteLocalRef(mc);
        return true;
    }
    LONG syncedPerspective = InterlockedCompareExchange(&g_perspectiveRenderingSyncedValue, 0, 0);
    if (InterlockedCompareExchange(&g_perspectiveRefreshPending, 0, 0) == 0 &&
        (syncedPerspective < 0 || syncedPerspective != (LONG)current)) {
        syncedPerspective = (LONG)current;
        InterlockedExchange(&g_perspectiveRenderingSyncedValue, syncedPerspective);
    }
    g_env->SetIntField(gs, g_slThirdPersonView, (jint)value);
    if (g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        g_env->DeleteLocalRef(gs);
        g_env->DeleteLocalRef(mc);
        return false;
    }
    g_env->DeleteLocalRef(gs);
    g_env->DeleteLocalRef(mc);
    InterlockedExchange(&g_perspectiveRefreshPending, syncedPerspective == (LONG)value ? 0 : 1);
    return true;
}

// Lunar's own perspective modules conflict with this DLL's Snaplook key handling.
// Keep both of Lunar's camera-hold modules off while the DLL is loaded without
// changing the user's persisted Lunar configuration.
struct LunarPerspectiveGuardJNI {
    jclass managerClass = nullptr;
    jclass moduleBaseClass = nullptr;
    jclass mapClass = nullptr;
    jclass collectionClass = nullptr;
    jfieldID fModuleRegistry = nullptr;
    jfieldID fEnabled = nullptr;
    jmethodID mMapValues = nullptr;
    jmethodID mCollectionToArray = nullptr;
    bool inited = false;
};

LunarPerspectiveGuardJNI g_lunarPerspectiveGuardJNI;

constexpr const char* kLunarModuleManagerClass =
    "com/moonsworth/lunar/client/HIOHRIRIOIHOICCCCOHCOOIICIICOH/"
    "OHOORCOHRORRIHROOCROOIIHHOOHRI/RROCOHOCHIHOIOIOROCORHRHRCIRHI";
constexpr const char* kLunarModuleBaseClass =
    "com/moonsworth/lunar/client/RIHHCHHHROOHOOIOIIRIOOCRHHOOOR/"
    "COOCHIRORIICRCIIRIROHIIRIRICCH";
constexpr const char* kLunarModuleRegistryField = "RRCRIRHOORRCORHCCOCHIRCHOROORC";

void ResetLunarPerspectiveGuardJNI(JNIEnv* env) {
    if (env) {
        if (g_lunarPerspectiveGuardJNI.managerClass) env->DeleteGlobalRef(g_lunarPerspectiveGuardJNI.managerClass);
        if (g_lunarPerspectiveGuardJNI.moduleBaseClass) env->DeleteGlobalRef(g_lunarPerspectiveGuardJNI.moduleBaseClass);
        if (g_lunarPerspectiveGuardJNI.mapClass) env->DeleteGlobalRef(g_lunarPerspectiveGuardJNI.mapClass);
        if (g_lunarPerspectiveGuardJNI.collectionClass) env->DeleteGlobalRef(g_lunarPerspectiveGuardJNI.collectionClass);
        if (env->ExceptionCheck()) env->ExceptionClear();
    }
    g_lunarPerspectiveGuardJNI = {};
}

bool InitLunarPerspectiveGuardJNI(JNIEnv* env) {
    if (!IsLunarNamedClient()) return true;
    if (g_lunarPerspectiveGuardJNI.inited) return true;
    if (!env) return false;

    auto fail = [&](const char* label) -> bool {
        if (env->ExceptionCheck()) env->ExceptionClear();
        ResetLunarPerspectiveGuardJNI(env);
        static std::string lastFailure;
        if (lastFailure != label) {
            lastFailure = label;
            DebugLog("Lunar perspective guard init waiting: %s", label);
        }
        return false;
    };

    jclass managerLocal = FindClassLoose(env, kLunarModuleManagerClass);
    if (!managerLocal) return fail("module manager class unavailable");
    g_lunarPerspectiveGuardJNI.managerClass = (jclass)env->NewGlobalRef(managerLocal);
    env->DeleteLocalRef(managerLocal);
    if (!g_lunarPerspectiveGuardJNI.managerClass || env->ExceptionCheck()) return fail("module manager global ref");

    jclass moduleBaseLocal = FindClassLoose(env, kLunarModuleBaseClass);
    if (!moduleBaseLocal) return fail("module base class unavailable");
    g_lunarPerspectiveGuardJNI.moduleBaseClass = (jclass)env->NewGlobalRef(moduleBaseLocal);
    env->DeleteLocalRef(moduleBaseLocal);
    if (!g_lunarPerspectiveGuardJNI.moduleBaseClass || env->ExceptionCheck()) return fail("module base global ref");

    jclass mapLocal = FindClassLoose(env, "java/util/Map");
    if (!mapLocal) return fail("java.util.Map unavailable");
    g_lunarPerspectiveGuardJNI.mapClass = (jclass)env->NewGlobalRef(mapLocal);
    env->DeleteLocalRef(mapLocal);
    if (!g_lunarPerspectiveGuardJNI.mapClass || env->ExceptionCheck()) return fail("Map global ref");

    jclass collectionLocal = FindClassLoose(env, "java/util/Collection");
    if (!collectionLocal) return fail("java.util.Collection unavailable");
    g_lunarPerspectiveGuardJNI.collectionClass = (jclass)env->NewGlobalRef(collectionLocal);
    env->DeleteLocalRef(collectionLocal);
    if (!g_lunarPerspectiveGuardJNI.collectionClass || env->ExceptionCheck()) return fail("Collection global ref");

    g_lunarPerspectiveGuardJNI.fModuleRegistry = GetStaticFieldIDCompat(env,
        g_lunarPerspectiveGuardJNI.managerClass,
        kLunarModuleRegistryField,
        "Ljava/util/Map;");
    if (!g_lunarPerspectiveGuardJNI.fModuleRegistry || env->ExceptionCheck()) return fail("module registry field unavailable");

    g_lunarPerspectiveGuardJNI.fEnabled = GetFieldIDCompat(env,
        g_lunarPerspectiveGuardJNI.moduleBaseClass,
        "enabled",
        "Z");
    if (!g_lunarPerspectiveGuardJNI.fEnabled || env->ExceptionCheck()) return fail("module enabled field unavailable");

    g_lunarPerspectiveGuardJNI.mMapValues = GetMethodIDCompat(env,
        g_lunarPerspectiveGuardJNI.mapClass,
        "values",
        "()Ljava/util/Collection;");
    if (!g_lunarPerspectiveGuardJNI.mMapValues || env->ExceptionCheck()) return fail("Map.values unavailable");

    g_lunarPerspectiveGuardJNI.mCollectionToArray = GetMethodIDCompat(env,
        g_lunarPerspectiveGuardJNI.collectionClass,
        "toArray",
        "()[Ljava/lang/Object;");
    if (!g_lunarPerspectiveGuardJNI.mCollectionToArray || env->ExceptionCheck()) return fail("Collection.toArray unavailable");

    g_lunarPerspectiveGuardJNI.inited = true;
    DebugLog("Lunar perspective guard initialized");
    return true;
}

bool EnforceLunarPerspectiveModulesDisabled(JNIEnv* env) {
    if (!IsLunarNamedClient()) return true;
    if (!InitLunarPerspectiveGuardJNI(env)) return false;

    jobject registry = env->GetStaticObjectField(
        g_lunarPerspectiveGuardJNI.managerClass,
        g_lunarPerspectiveGuardJNI.fModuleRegistry);
    if (!registry || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (registry) env->DeleteLocalRef(registry);
        return false;
    }

    jobject values = env->CallObjectMethod(registry, g_lunarPerspectiveGuardJNI.mMapValues);
    jobjectArray modules = values && !env->ExceptionCheck()
        ? (jobjectArray)env->CallObjectMethod(values, g_lunarPerspectiveGuardJNI.mCollectionToArray)
        : nullptr;
    if (!values || !modules || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (modules) env->DeleteLocalRef(modules);
        if (values) env->DeleteLocalRef(values);
        env->DeleteLocalRef(registry);
        return false;
    }

    int foundCount = 0;
    int disabledCount = 0;
    jsize moduleCount = env->GetArrayLength(modules);
    for (jsize index = 0; index < moduleCount; ++index) {
        jobject module = env->GetObjectArrayElement(modules, index);
        if (!module || env->ExceptionCheck()) {
            if (env->ExceptionCheck()) env->ExceptionClear();
            if (module) env->DeleteLocalRef(module);
            continue;
        }

        jclass moduleClass = env->GetObjectClass(module);
        jmethodID getId = moduleClass
            ? GetMethodIDCompat(env, moduleClass, "getId", "()Ljava/lang/String;")
            : nullptr;
        jstring idString = getId && !env->ExceptionCheck()
            ? (jstring)env->CallObjectMethod(module, getId)
            : nullptr;
        if (env->ExceptionCheck()) env->ExceptionClear();

        std::string moduleId = idString ? ToLowerAscii(JStringToUtf8(env, idString)) : "";
        bool isConflictingModule = moduleId == "snaplook" || moduleId == "freelook";
        if (isConflictingModule) {
            ++foundCount;
            jboolean wasEnabled = env->GetBooleanField(module, g_lunarPerspectiveGuardJNI.fEnabled);
            if (!env->ExceptionCheck() && wasEnabled == JNI_TRUE) {
                env->SetBooleanField(module, g_lunarPerspectiveGuardJNI.fEnabled, JNI_FALSE);
                if (!env->ExceptionCheck()) ++disabledCount;
            }
            if (env->ExceptionCheck()) env->ExceptionClear();
        }

        if (idString) env->DeleteLocalRef(idString);
        if (moduleClass) env->DeleteLocalRef(moduleClass);
        env->DeleteLocalRef(module);
    }

    env->DeleteLocalRef(modules);
    env->DeleteLocalRef(values);
    env->DeleteLocalRef(registry);

    static int lastFoundCount = -1;
    if (disabledCount > 0 || foundCount != lastFoundCount) {
        lastFoundCount = foundCount;
        DebugLog("Lunar perspective guard modulesFound=%d modulesDisabled=%d", foundCount, disabledCount);
    }
    return foundCount >= 2;
}

static jclass g_glContextClass = nullptr;
static jmethodID g_glGetCapabilities = nullptr;
static jfieldID g_glBlendFuncSeparateField = nullptr;
static bool g_glBlendHookInited = false;
static bool g_glBlendHookFailed = false;
static HGLRC g_glBlendPatchedContext = nullptr;

JNIEnv* GetJNIEnvForCurrentThread() {
    if (!g_jvm) return nullptr;
    JNIEnv* env = nullptr;
    if (g_jvm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        if (g_jvm->AttachCurrentThread((void**)&env, nullptr) != JNI_OK) return nullptr;
    }
    return env;
}

bool InitBlendHookJNI(JNIEnv* env) {
    if (g_glBlendHookFailed) return false;
    if (g_glBlendHookInited) return true;
    if (!env) return false;

    jclass glContextClass = FindClassLoose(env, "org/lwjgl/opengl/GLContext");
    if (!glContextClass) { env->ExceptionClear(); g_glBlendHookFailed = true; return false; }
    g_glContextClass = (jclass)env->NewGlobalRef(glContextClass);
    env->DeleteLocalRef(glContextClass);
    if (!g_glContextClass) { env->ExceptionClear(); g_glBlendHookFailed = true; return false; }

    g_glGetCapabilities = GetStaticMethodIDCompat(env, g_glContextClass, "getCapabilities", "()Lorg/lwjgl/opengl/ContextCapabilities;");
    if (!g_glGetCapabilities) { env->ExceptionClear(); g_glBlendHookFailed = true; return false; }

    jclass capsClass = FindClassLoose(env, "org/lwjgl/opengl/ContextCapabilities");
    if (!capsClass) { env->ExceptionClear(); g_glBlendHookFailed = true; return false; }

    g_glBlendFuncSeparateField = GetFieldIDCompat(env, capsClass, "glBlendFuncSeparate", "J");
    if (!g_glBlendFuncSeparateField) {
        env->ExceptionClear();
        g_glBlendFuncSeparateField = GetFieldIDCompat(env, capsClass, "glBlendFuncSeparateEXT", "J");
    }
    env->DeleteLocalRef(capsClass);
    if (!g_glBlendFuncSeparateField) { env->ExceptionClear(); g_glBlendHookFailed = true; return false; }

    g_glBlendHookInited = true;
    return true;
}

bool InitJvmtiRedefineSupport() {
    if (g_jvmtiFailed) return false;
    if (g_jvmti) return true;
    if (!g_jvm) return false;

    jint res = g_jvm->GetEnv(reinterpret_cast<void**>(&g_jvmti), JVMTI_VERSION_1_0);
    if (res != JNI_OK || !g_jvmti) {
        DebugLog("JVMTI unavailable res=%d", (int)res);
        g_jvmtiFailed = true;
        g_jvmti = nullptr;
        return false;
    }

    jvmtiCapabilities caps = {};
    jvmtiError err = g_jvmti->GetCapabilities(&caps);
    if (err != JVMTI_ERROR_NONE) {
        DebugLog("JVMTI GetCapabilities failed err=%d", (int)err);
        g_jvmtiFailed = true;
        g_jvmti = nullptr;
        return false;
    }

    if (!caps.can_redefine_classes) {
        jvmtiCapabilities requested = {};
        requested.can_redefine_classes = 1;
        err = g_jvmti->AddCapabilities(&requested);
        if (err != JVMTI_ERROR_NONE) {
            DebugLog("JVMTI AddCapabilities failed err=%d", (int)err);
            g_jvmtiFailed = true;
            g_jvmti = nullptr;
            return false;
        }

        caps = {};
        err = g_jvmti->GetCapabilities(&caps);
        if (err != JVMTI_ERROR_NONE || !caps.can_redefine_classes) {
            DebugLog("JVMTI redefine capability missing err=%d canRedefine=%d",
                (int)err,
                caps.can_redefine_classes ? 1 : 0);
            g_jvmtiFailed = true;
            g_jvmti = nullptr;
            return false;
        }
    }

    return true;
}

bool SetSharedJvmtiCallbacks(const char* label) {
    if (!g_jvmti) return false;

    jvmtiEventCallbacks callbacks = {};
    callbacks.ClassFileLoadHook = &SharedClassFileLoadHook;
    callbacks.Breakpoint = &MutedVoiceChatPacketBreakpoint;

    jvmtiError err = g_jvmti->SetEventCallbacks(&callbacks, sizeof(callbacks));
    if (err != JVMTI_ERROR_NONE) {
        DebugLog("%s JVMTI SetEventCallbacks failed err=%d", label ? label : "Shared", (int)err);
        return false;
    }

    return true;
}

bool EnsureMutedVoicePacketFilterCapabilities() {
    if (!g_jvmti) return false;

    jvmtiCapabilities caps = {};
    jvmtiError err = g_jvmti->GetCapabilities(&caps);
    if (err != JVMTI_ERROR_NONE) {
        DebugLog("Muted Voice packet filter JVMTI GetCapabilities failed err=%d", (int)err);
        InterlockedExchange(&g_mutedVoicePacketFilterFailed, 1);
        return false;
    }

    bool needsAdd =
        !caps.can_generate_breakpoint_events ||
        !caps.can_force_early_return ||
        !caps.can_access_local_variables;
    if (needsAdd) {
        jvmtiCapabilities requested = {};
        requested.can_generate_breakpoint_events = caps.can_generate_breakpoint_events ? 0 : 1;
        requested.can_force_early_return = caps.can_force_early_return ? 0 : 1;
        requested.can_access_local_variables = caps.can_access_local_variables ? 0 : 1;

        err = g_jvmti->AddCapabilities(&requested);
        if (err != JVMTI_ERROR_NONE) {
            DebugLog("Muted Voice packet filter JVMTI AddCapabilities failed err=%d breakpoint=%d earlyReturn=%d locals=%d",
                (int)err,
                requested.can_generate_breakpoint_events ? 1 : 0,
                requested.can_force_early_return ? 1 : 0,
                requested.can_access_local_variables ? 1 : 0);
            InterlockedExchange(&g_mutedVoicePacketFilterFailed, 1);
            return false;
        }
    }

    caps = {};
    err = g_jvmti->GetCapabilities(&caps);
    if (err != JVMTI_ERROR_NONE ||
        !caps.can_generate_breakpoint_events ||
        !caps.can_force_early_return ||
        !caps.can_access_local_variables) {
        DebugLog("Muted Voice packet filter missing JVMTI caps err=%d breakpoint=%d earlyReturn=%d locals=%d",
            (int)err,
            caps.can_generate_breakpoint_events ? 1 : 0,
            caps.can_force_early_return ? 1 : 0,
            caps.can_access_local_variables ? 1 : 0);
        InterlockedExchange(&g_mutedVoicePacketFilterFailed, 1);
        return false;
    }

    return true;
}

bool ResolveMutedVoicePacketFilterMethods(JNIEnv* env) {
    if (!env) return false;

    jclass packetClass = FindClassLoose(env, "fy");
    if (!packetClass || env->ExceptionCheck()) {
        bool hadException = env->ExceptionCheck();
        if (hadException) env->ExceptionClear();
        DebugLog("Muted Voice packet filter init pending: failed finding fy S02PacketChat%s",
            hadException ? " (exception cleared)" : "");
        if (packetClass) env->DeleteLocalRef(packetClass);
        return false;
    }

    if (!g_mutedVoiceS02PacketClass) {
        g_mutedVoiceS02PacketClass = (jclass)env->NewGlobalRef(packetClass);
        if (!g_mutedVoiceS02PacketClass || env->ExceptionCheck()) {
            bool hadException = env->ExceptionCheck();
            if (hadException) env->ExceptionClear();
            DebugLog("Muted Voice packet filter init failed global-ref fy S02PacketChat%s",
                hadException ? " (exception cleared)" : "");
            env->DeleteLocalRef(packetClass);
            InterlockedExchange(&g_mutedVoicePacketFilterFailed, 1);
            return false;
        }
    }

    g_mutedVoiceS02PacketProcessMethod = GetMethodIDCompat(env, packetClass, "a", "(Lfj;)V");
    if (!g_mutedVoiceS02PacketProcessMethod || env->ExceptionCheck()) {
        bool hadException = env->ExceptionCheck();
        if (hadException) env->ExceptionClear();
        DebugLog("Muted Voice packet filter init failed resolving fy.a(Lfj;)V%s",
            hadException ? " (exception cleared)" : "");
        env->DeleteLocalRef(packetClass);
        InterlockedExchange(&g_mutedVoicePacketFilterFailed, 1);
        return false;
    }
    DebugLog("Muted Voice packet filter found fy.a(Lfj;)V packet process method");

    g_mutedVoiceS02PacketGetComponentMethod = GetMethodIDCompat(env, packetClass, "a", "()Leu;");
    if (!g_mutedVoiceS02PacketGetComponentMethod || env->ExceptionCheck()) {
        bool hadException = env->ExceptionCheck();
        if (hadException) env->ExceptionClear();
        DebugLog("Muted Voice packet filter init failed resolving fy.a()Leu;%s",
            hadException ? " (exception cleared)" : "");
        env->DeleteLocalRef(packetClass);
        InterlockedExchange(&g_mutedVoicePacketFilterFailed, 1);
        return false;
    }
    DebugLog("Muted Voice packet filter found fy.a()Leu; chat component getter");
    env->DeleteLocalRef(packetClass);

    jclass handlerClass = FindClassLoose(env, "fj");
    if (!handlerClass || env->ExceptionCheck()) {
        bool hadException = env->ExceptionCheck();
        if (hadException) env->ExceptionClear();
        DebugLog("Muted Voice packet filter init pending: failed finding fj INetHandlerPlayClient%s",
            hadException ? " (exception cleared)" : "");
        if (handlerClass) env->DeleteLocalRef(handlerClass);
        return false;
    }

    g_mutedVoicePlayClientHandleChatMethod = GetMethodIDCompat(env, handlerClass, "a", "(Lfy;)V");
    if (!g_mutedVoicePlayClientHandleChatMethod || env->ExceptionCheck()) {
        bool hadException = env->ExceptionCheck();
        if (hadException) env->ExceptionClear();
        DebugLog("Muted Voice packet filter init failed resolving fj.a(Lfy;)V%s",
            hadException ? " (exception cleared)" : "");
        env->DeleteLocalRef(handlerClass);
        InterlockedExchange(&g_mutedVoicePacketFilterFailed, 1);
        return false;
    }
    DebugLog("Muted Voice packet filter found fj.a(Lfy;)V chat handler method");
    env->DeleteLocalRef(handlerClass);

    jclass componentClass = FindClassLoose(env, "eu");
    if (!componentClass || env->ExceptionCheck()) {
        bool hadException = env->ExceptionCheck();
        if (hadException) env->ExceptionClear();
        DebugLog("Muted Voice packet filter init pending: failed finding eu chat component%s",
            hadException ? " (exception cleared)" : "");
        if (componentClass) env->DeleteLocalRef(componentClass);
        return false;
    }

    g_mutedVoicePacketComponentGetUnformattedText = GetMethodIDCompat(env, componentClass, "c", "()Ljava/lang/String;");
    if (!g_mutedVoicePacketComponentGetUnformattedText || env->ExceptionCheck()) {
        bool hadException = env->ExceptionCheck();
        if (hadException) env->ExceptionClear();
        DebugLog("Muted Voice packet filter init failed resolving eu.c()%s",
            hadException ? " (exception cleared)" : "");
        env->DeleteLocalRef(componentClass);
        InterlockedExchange(&g_mutedVoicePacketFilterFailed, 1);
        return false;
    }
    DebugLog("Muted Voice packet filter found eu.c() text");

    g_mutedVoicePacketComponentGetFormattedText = GetMethodIDCompat(env, componentClass, "d", "()Ljava/lang/String;");
    if (!g_mutedVoicePacketComponentGetFormattedText || env->ExceptionCheck()) {
        bool hadException = env->ExceptionCheck();
        if (hadException) env->ExceptionClear();
        DebugLog("Muted Voice packet filter init failed resolving eu.d()%s",
            hadException ? " (exception cleared)" : "");
        env->DeleteLocalRef(componentClass);
        InterlockedExchange(&g_mutedVoicePacketFilterFailed, 1);
        return false;
    }
    DebugLog("Muted Voice packet filter found eu.d() formatted text");
    env->DeleteLocalRef(componentClass);
    return true;
}

bool InitMutedVoicePacketFilter(JNIEnv* env) {
    if (InterlockedCompareExchange(&g_mutedVoicePacketFilterInstalled, 0, 0) != 0) return true;
    if (InterlockedCompareExchange(&g_mutedVoicePacketFilterFailed, 0, 0) != 0) return false;
    if (!env) return false;

    if (!InitJvmtiRedefineSupport() || !g_jvmti) {
        DebugLog("Muted Voice packet filter init failed: JVMTI unavailable");
        InterlockedExchange(&g_mutedVoicePacketFilterFailed, 1);
        return false;
    }

    if (!ResolveMutedVoicePacketFilterMethods(env)) return false;
    if (!EnsureMutedVoicePacketFilterHelper(env)) {
        InterlockedExchange(&g_mutedVoicePacketFilterFailed, 1);
        return false;
    }
    if (!EnsureMutedVoiceS02PacketChatBytecode(env)) {
        InterlockedExchange(&g_mutedVoicePacketFilterFailed, 1);
        return false;
    }
    if (!RedefineMutedVoiceS02PacketChat(env, g_mutedVoicePatchedS02PacketBytes, "S02PacketChat muted voice packet filter")) {
        InterlockedExchange(&g_mutedVoicePacketFilterFailed, 1);
        return false;
    }

    InterlockedExchange(&g_mutedVoicePacketFilterInstalled, 1);
    DebugLog("Muted Voice packet filter installed by redefining fy.a(Lfj;)V");
    return true;
}

void JNICALL MutedVoiceChatPacketBreakpoint(
    jvmtiEnv* jvmtiEnv,
    JNIEnv* env,
    jthread thread,
    jmethodID method,
    jlocation location) {
    if (!jvmtiEnv || !env || !thread || method != g_mutedVoiceS02PacketProcessMethod ||
        location != g_mutedVoiceS02PacketBreakpointLocation) {
        return;
    }
    if (!g_guiExtrasMutedVoice || !g_guiExtrasMutedVoiceHideMuteReminder) return;

    jobject packet = nullptr;
    jvmtiError err = jvmtiEnv->GetLocalInstance(thread, 0, &packet);
    if (err != JVMTI_ERROR_NONE || !packet) {
        if (packet) env->DeleteLocalRef(packet);
        packet = nullptr;
        err = jvmtiEnv->GetLocalObject(thread, 0, 0, &packet);
    }
    if (err != JVMTI_ERROR_NONE || !packet) {
        DebugLog("Muted Voice packet filter GetLocalInstance/GetLocalObject failed err=%d", (int)err);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            DebugLog("Muted Voice packet filter cleared exception after local packet lookup");
        }
        if (packet) env->DeleteLocalRef(packet);
        return;
    }

    bool coreLine = false;
    bool separatorLine = false;
    bool shouldBlock = ShouldBlockMutedVoiceChatPacket(env, packet, &coreLine, &separatorLine);
    env->DeleteLocalRef(packet);

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        DebugLog("Muted Voice packet filter cleared exception after packet inspection");
        return;
    }
    if (!shouldBlock) return;

    if (coreLine) {
        NoteMutedVoiceCorePacketBlocked();
    }

    err = jvmtiEnv->ForceEarlyReturnVoid(thread);
    if (err != JVMTI_ERROR_NONE) {
        DebugLog("Muted Voice packet filter ForceEarlyReturnVoid failed err=%d core=%d separator=%d",
            (int)err,
            coreLine ? 1 : 0,
            separatorLine ? 1 : 0);
        return;
    }

    DebugLog("Muted Voice packet filter blocked inbound S02PacketChat core=%d separator=%d",
        coreLine ? 1 : 0,
        separatorLine ? 1 : 0);
}

void ShutdownMutedVoicePacketFilter() {
    if (!g_jvmti || InterlockedCompareExchange(&g_mutedVoicePacketFilterInstalled, 0, 0) == 0 ||
        !g_mutedVoiceS02PacketClass || g_mutedVoiceOriginalS02PacketBytes.empty()) {
        return;
    }

    if (!RedefineMutedVoiceS02PacketChat(g_env, g_mutedVoiceOriginalS02PacketBytes, "S02PacketChat muted voice packet filter restore")) {
        DebugLog("Muted Voice packet filter restore failed during shutdown");
    }

    InterlockedExchange(&g_mutedVoicePacketFilterInstalled, 0);
}

bool GetSeeBarriersBarrierClass(JNIEnv* env, jclass& outClass) {
    outClass = nullptr;
    if (!env) return false;
    if (g_seeBarriersBarrierClass) {
        outClass = g_seeBarriersBarrierClass;
        return true;
    }

    if (g_seeBarriersBlockClass && g_seeBarriersBlockGetById) {
        jobject barrierBlock = env->CallStaticObjectMethod(g_seeBarriersBlockClass, g_seeBarriersBlockGetById, (jint)166);
        if (barrierBlock && !env->ExceptionCheck()) {
            jclass runtimeClass = env->GetObjectClass(barrierBlock);
            env->DeleteLocalRef(barrierBlock);
            if (runtimeClass && !env->ExceptionCheck()) {
                g_seeBarriersBarrierClass = (jclass)env->NewGlobalRef(runtimeClass);
                env->DeleteLocalRef(runtimeClass);
                if (!g_seeBarriersBarrierClass || env->ExceptionCheck()) {
                    env->ExceptionClear();
                    g_seeBarriersBarrierClass = nullptr;
                    DebugLog("See Barriers failed to global-ref runtime barrier class");
                    return false;
                }

                outClass = g_seeBarriersBarrierClass;
                DebugLog("See Barriers resolved BlockBarrier class from runtime block id=166");
                return true;
            }

            if (runtimeClass) env->DeleteLocalRef(runtimeClass);
        }
        else {
            if (env->ExceptionCheck()) env->ExceptionClear();
            if (barrierBlock) env->DeleteLocalRef(barrierBlock);
        }
    }

    jclass localClass = FindClassLoose(env, "afb");
    if (!localClass || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (localClass) env->DeleteLocalRef(localClass);
        DebugLog("See Barriers failed to find BlockBarrier class afb");
        return false;
    }

    g_seeBarriersBarrierClass = (jclass)env->NewGlobalRef(localClass);
    env->DeleteLocalRef(localClass);
    if (!g_seeBarriersBarrierClass || env->ExceptionCheck()) {
        env->ExceptionClear();
        g_seeBarriersBarrierClass = nullptr;
        DebugLog("See Barriers failed to global-ref BlockBarrier class");
        return false;
    }

    outClass = g_seeBarriersBarrierClass;
    DebugLog("See Barriers resolved BlockBarrier class by class lookup fallback");
    return true;
}

bool ReadJavaInputStream(JNIEnv* env, jobject stream, std::vector<unsigned char>& outBytes) {
    outBytes.clear();
    if (!env || !stream) return false;

    jclass inputStreamClass = FindClassLoose(env, "java/io/InputStream");
    if (!inputStreamClass || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (inputStreamClass) env->DeleteLocalRef(inputStreamClass);
        return false;
    }

    jmethodID mRead = GetMethodIDCompat(env, inputStreamClass, "read", "([B)I");
    jmethodID mClose = GetMethodIDCompat(env, inputStreamClass, "close", "()V");
    env->DeleteLocalRef(inputStreamClass);
    if (!mRead || !mClose || env->ExceptionCheck()) {
        env->ExceptionClear();
        return false;
    }

    const jsize bufferSize = 4096;
    jbyteArray buffer = env->NewByteArray(bufferSize);
    if (!buffer || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (buffer) env->DeleteLocalRef(buffer);
        return false;
    }

    std::vector<jbyte> temp((size_t)bufferSize);
    bool ok = true;
    for (;;) {
        jint readCount = env->CallIntMethod(stream, mRead, buffer);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            ok = false;
            break;
        }
        if (readCount < 0) break;
        if (readCount == 0) continue;
        if (readCount > bufferSize) {
            ok = false;
            break;
        }

        env->GetByteArrayRegion(buffer, 0, readCount, temp.data());
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            ok = false;
            break;
        }
        for (jint i = 0; i < readCount; ++i) {
            outBytes.push_back((unsigned char)temp[(size_t)i]);
        }
    }

    env->CallVoidMethod(stream, mClose);
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(buffer);
    return ok && !outBytes.empty();
}

jobject OpenClassResourceStream(JNIEnv* env, jclass targetClass, const char* resourceName) {
    if (!env || !targetClass || !resourceName) return nullptr;

    jclass classClass = FindClassLoose(env, "java/lang/Class");
    if (!classClass || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (classClass) env->DeleteLocalRef(classClass);
        return nullptr;
    }

    jmethodID mGetResourceAsStream = GetMethodIDCompat(env,
        classClass,
        "getResourceAsStream",
        "(Ljava/lang/String;)Ljava/io/InputStream;");
    if (!mGetResourceAsStream || env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(classClass);
        return nullptr;
    }

    jstring resourceString = env->NewStringUTF(resourceName);
    if (!resourceString || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (resourceString) env->DeleteLocalRef(resourceString);
        env->DeleteLocalRef(classClass);
        return nullptr;
    }

    jobject stream = env->CallObjectMethod(targetClass, mGetResourceAsStream, resourceString);
    env->DeleteLocalRef(resourceString);
    env->DeleteLocalRef(classClass);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        if (stream) env->DeleteLocalRef(stream);
        return nullptr;
    }

    return stream;
}

jobject OpenClassLoaderResourceStream(JNIEnv* env, jclass targetClass, const char* resourceName) {
    if (!env || !targetClass || !resourceName) return nullptr;

    jclass classClass = FindClassLoose(env, "java/lang/Class");
    jclass classLoaderClass = FindClassLoose(env, "java/lang/ClassLoader");
    if (!classClass || !classLoaderClass || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (classClass) env->DeleteLocalRef(classClass);
        if (classLoaderClass) env->DeleteLocalRef(classLoaderClass);
        return nullptr;
    }

    jmethodID mGetClassLoader = GetMethodIDCompat(env, classClass, "getClassLoader", "()Ljava/lang/ClassLoader;");
    jmethodID mGetResourceAsStream = GetMethodIDCompat(env,
        classLoaderClass,
        "getResourceAsStream",
        "(Ljava/lang/String;)Ljava/io/InputStream;");
    if (!mGetClassLoader || !mGetResourceAsStream || env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(classClass);
        env->DeleteLocalRef(classLoaderClass);
        return nullptr;
    }

    jobject classLoader = env->CallObjectMethod(targetClass, mGetClassLoader);
    if (!classLoader || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (classLoader) env->DeleteLocalRef(classLoader);
        env->DeleteLocalRef(classClass);
        env->DeleteLocalRef(classLoaderClass);
        return nullptr;
    }

    jstring resourceString = env->NewStringUTF(resourceName);
    if (!resourceString || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (resourceString) env->DeleteLocalRef(resourceString);
        env->DeleteLocalRef(classLoader);
        env->DeleteLocalRef(classClass);
        env->DeleteLocalRef(classLoaderClass);
        return nullptr;
    }

    jobject stream = env->CallObjectMethod(classLoader, mGetResourceAsStream, resourceString);
    env->DeleteLocalRef(resourceString);
    env->DeleteLocalRef(classLoader);
    env->DeleteLocalRef(classClass);
    env->DeleteLocalRef(classLoaderClass);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        if (stream) env->DeleteLocalRef(stream);
        return nullptr;
    }

    return stream;
}

bool LoadBarrierClassBytes(JNIEnv* env, std::vector<unsigned char>& outBytes) {
    outBytes.clear();
    jclass barrierClass = nullptr;
    if (!GetSeeBarriersBarrierClass(env, barrierClass)) return false;

    jobject stream = OpenClassResourceStream(env, barrierClass, "afb.class");
    if (!stream) stream = OpenClassResourceStream(env, barrierClass, "/afb.class");
    if (!stream) stream = OpenClassLoaderResourceStream(env, barrierClass, "afb.class");
    if (!stream) {
        DebugLog("See Barriers failed to open afb.class resource");
        return false;
    }

    bool ok = ReadJavaInputStream(env, stream, outBytes);
    env->DeleteLocalRef(stream);
    if (!ok) DebugLog("See Barriers failed to read afb.class bytes");
    return ok;
}

bool ReadClassU1(const std::vector<unsigned char>& bytes, size_t& offset, unsigned char& value) {
    if (offset >= bytes.size()) return false;
    value = bytes[offset++];
    return true;
}

bool ReadClassU2(const std::vector<unsigned char>& bytes, size_t& offset, uint16_t& value) {
    if (offset + 2 > bytes.size()) return false;
    value = ((uint16_t)bytes[offset] << 8) | (uint16_t)bytes[offset + 1];
    offset += 2;
    return true;
}

bool ReadClassU4(const std::vector<unsigned char>& bytes, size_t& offset, uint32_t& value) {
    if (offset + 4 > bytes.size()) return false;
    value = ((uint32_t)bytes[offset] << 24) |
        ((uint32_t)bytes[offset + 1] << 16) |
        ((uint32_t)bytes[offset + 2] << 8) |
        (uint32_t)bytes[offset + 3];
    offset += 4;
    return true;
}

bool AdvanceClassBytes(const std::vector<unsigned char>& bytes, size_t& offset, size_t count) {
    if (offset > bytes.size() || count > (bytes.size() - offset)) return false;
    offset += count;
    return true;
}

bool SkipClassMemberInfo(const std::vector<unsigned char>& bytes, size_t& offset) {
    uint16_t accessFlags = 0;
    uint16_t nameIndex = 0;
    uint16_t descriptorIndex = 0;
    uint16_t attributesCount = 0;
    if (!ReadClassU2(bytes, offset, accessFlags) ||
        !ReadClassU2(bytes, offset, nameIndex) ||
        !ReadClassU2(bytes, offset, descriptorIndex) ||
        !ReadClassU2(bytes, offset, attributesCount)) {
        return false;
    }

    for (uint16_t i = 0; i < attributesCount; ++i) {
        uint16_t attributeNameIndex = 0;
        uint32_t attributeLength = 0;
        if (!ReadClassU2(bytes, offset, attributeNameIndex) ||
            !ReadClassU4(bytes, offset, attributeLength) ||
            !AdvanceClassBytes(bytes, offset, (size_t)attributeLength)) {
            return false;
        }
    }

    return true;
}

void AppendClassU1(std::vector<unsigned char>& bytes, unsigned char value) {
    bytes.push_back(value);
}

void AppendClassU2(std::vector<unsigned char>& bytes, uint16_t value) {
    bytes.push_back((unsigned char)((value >> 8) & 0xFF));
    bytes.push_back((unsigned char)(value & 0xFF));
}

void AppendClassU4(std::vector<unsigned char>& bytes, uint32_t value) {
    bytes.push_back((unsigned char)((value >> 24) & 0xFF));
    bytes.push_back((unsigned char)((value >> 16) & 0xFF));
    bytes.push_back((unsigned char)((value >> 8) & 0xFF));
    bytes.push_back((unsigned char)(value & 0xFF));
}

bool WriteClassU2At(std::vector<unsigned char>& bytes, size_t offset, uint16_t value) {
    if (offset + 2 > bytes.size()) return false;
    bytes[offset] = (unsigned char)((value >> 8) & 0xFF);
    bytes[offset + 1] = (unsigned char)(value & 0xFF);
    return true;
}

bool WriteClassU4At(std::vector<unsigned char>& bytes, size_t offset, uint32_t value) {
    if (offset + 4 > bytes.size()) return false;
    bytes[offset] = (unsigned char)((value >> 24) & 0xFF);
    bytes[offset + 1] = (unsigned char)((value >> 16) & 0xFF);
    bytes[offset + 2] = (unsigned char)((value >> 8) & 0xFF);
    bytes[offset + 3] = (unsigned char)(value & 0xFF);
    return true;
}

bool ReadClassConstantPoolUtf8(
    const std::vector<unsigned char>& bytes,
    size_t& offset,
    uint16_t& constantPoolCount,
    std::vector<std::string>& utf8Constants) {
    uint32_t magic = 0;
    uint16_t minorVersion = 0;
    uint16_t majorVersion = 0;
    if (!ReadClassU4(bytes, offset, magic) ||
        magic != 0xCAFEBABE ||
        !ReadClassU2(bytes, offset, minorVersion) ||
        !ReadClassU2(bytes, offset, majorVersion) ||
        !ReadClassU2(bytes, offset, constantPoolCount)) {
        return false;
    }

    utf8Constants.assign((size_t)constantPoolCount, std::string());
    for (uint16_t i = 1; i < constantPoolCount; ++i) {
        unsigned char tag = 0;
        if (!ReadClassU1(bytes, offset, tag)) return false;

        switch (tag) {
        case 1: {
            uint16_t length = 0;
            if (!ReadClassU2(bytes, offset, length) || offset + length > bytes.size()) return false;
            utf8Constants[(size_t)i] = std::string(
                reinterpret_cast<const char*>(&bytes[offset]),
                (size_t)length);
            offset += length;
            break;
        }
        case 3:
        case 4:
            if (!AdvanceClassBytes(bytes, offset, 4)) return false;
            break;
        case 5:
        case 6:
            if (!AdvanceClassBytes(bytes, offset, 8)) return false;
            ++i;
            break;
        case 7:
        case 8:
        case 16:
            if (!AdvanceClassBytes(bytes, offset, 2)) return false;
            break;
        case 9:
        case 10:
        case 11:
        case 12:
        case 18:
            if (!AdvanceClassBytes(bytes, offset, 4)) return false;
            break;
        case 15:
            if (!AdvanceClassBytes(bytes, offset, 3)) return false;
            break;
        default:
            DebugLog("Muted Voice packet filter unsupported classfile constant tag=%u", (unsigned int)tag);
            return false;
        }
    }

    return true;
}

bool FindClassMethodRefs(
    const std::vector<unsigned char>& bytes,
    unsigned char referenceTag,
    const char* targetName,
    const char* targetDescriptor,
    std::vector<uint16_t>& refs) {
    refs.clear();
    size_t offset = 0;
    uint32_t magic = 0;
    uint16_t minorVersion = 0;
    uint16_t majorVersion = 0;
    uint16_t constantPoolCount = 0;
    if (!ReadClassU4(bytes, offset, magic) || magic != 0xCAFEBABE ||
        !ReadClassU2(bytes, offset, minorVersion) ||
        !ReadClassU2(bytes, offset, majorVersion) ||
        !ReadClassU2(bytes, offset, constantPoolCount)) {
        return false;
    }

    std::vector<unsigned char> tags((size_t)constantPoolCount, 0);
    std::vector<uint16_t> first((size_t)constantPoolCount, 0);
    std::vector<uint16_t> second((size_t)constantPoolCount, 0);
    std::vector<std::string> utf8((size_t)constantPoolCount);
    for (uint16_t i = 1; i < constantPoolCount; ++i) {
        unsigned char tag = 0;
        if (!ReadClassU1(bytes, offset, tag)) return false;
        tags[(size_t)i] = tag;
        switch (tag) {
        case 1: {
            uint16_t length = 0;
            if (!ReadClassU2(bytes, offset, length) || offset + length > bytes.size()) return false;
            utf8[(size_t)i] = std::string(
                reinterpret_cast<const char*>(&bytes[offset]), (size_t)length);
            offset += length;
            break;
        }
        case 3:
        case 4:
            if (!AdvanceClassBytes(bytes, offset, 4)) return false;
            break;
        case 5:
        case 6:
            if (!AdvanceClassBytes(bytes, offset, 8)) return false;
            ++i;
            break;
        case 7:
        case 8:
        case 16:
        case 19:
        case 20:
            if (!ReadClassU2(bytes, offset, first[(size_t)i])) return false;
            break;
        case 9:
        case 10:
        case 11:
        case 12:
        case 17:
        case 18:
            if (!ReadClassU2(bytes, offset, first[(size_t)i]) ||
                !ReadClassU2(bytes, offset, second[(size_t)i])) return false;
            break;
        case 15: {
            unsigned char referenceKind = 0;
            if (!ReadClassU1(bytes, offset, referenceKind) ||
                !ReadClassU2(bytes, offset, first[(size_t)i])) return false;
            break;
        }
        default:
            return false;
        }
    }

    for (uint16_t i = 1; i < constantPoolCount; ++i) {
        if (tags[(size_t)i] != referenceTag) continue;
        uint16_t nameAndType = second[(size_t)i];
        if (nameAndType == 0 || nameAndType >= constantPoolCount ||
            tags[(size_t)nameAndType] != 12) continue;
        uint16_t nameIndex = first[(size_t)nameAndType];
        uint16_t descriptorIndex = second[(size_t)nameAndType];
        if (nameIndex >= constantPoolCount || descriptorIndex >= constantPoolCount) continue;
        if (utf8[(size_t)nameIndex] == targetName &&
            utf8[(size_t)descriptorIndex] == targetDescriptor) {
            refs.push_back(i);
        }
    }
    return !refs.empty();
}

bool FindClassInterfaceMethodRefs(
    const std::vector<unsigned char>& bytes,
    const char* targetName,
    const char* targetDescriptor,
    std::vector<uint16_t>& refs) {
    return FindClassMethodRefs(bytes, 11, targetName, targetDescriptor, refs);
}

std::vector<unsigned char> BuildMutedVoicePacketFilterHelperClassBytes() {
    std::vector<unsigned char> bytes;
    const std::string dispatchDescriptor = TranslateLunarDescriptor("(Leu;Lfj;Lfy;)V");
    AppendClassU4(bytes, 0xCAFEBABE);
    AppendClassU2(bytes, 0);
    AppendClassU2(bytes, 52);
    AppendClassU2(bytes, 14); // constant_pool_count

    auto addUtf8 = [&bytes](const char* value) {
        AppendClassU1(bytes, 1);
        size_t len = strlen(value);
        AppendClassU2(bytes, (uint16_t)len);
        bytes.insert(bytes.end(), value, value + len);
    };

    addUtf8("MutedVoicePacketFilter"); // #1
    AppendClassU1(bytes, 7); AppendClassU2(bytes, 1); // #2 Class
    addUtf8("java/lang/Object"); // #3
    AppendClassU1(bytes, 7); AppendClassU2(bytes, 3); // #4 Class
    addUtf8("<init>"); // #5
    addUtf8("()V"); // #6
    addUtf8("Code"); // #7
    AppendClassU1(bytes, 10); AppendClassU2(bytes, 4); AppendClassU2(bytes, 9); // #8 Object.<init>
    AppendClassU1(bytes, 12); AppendClassU2(bytes, 5); AppendClassU2(bytes, 6); // #9 NameAndType
    addUtf8("a"); // #10
    addUtf8(dispatchDescriptor.c_str()); // #11
    addUtf8("SourceFile"); // #12
    addUtf8("MutedVoicePacketFilter.java"); // #13

    AppendClassU2(bytes, 0x0021); // public super
    AppendClassU2(bytes, 2); // this_class
    AppendClassU2(bytes, 4); // super_class
    AppendClassU2(bytes, 0); // interfaces
    AppendClassU2(bytes, 0); // fields
    AppendClassU2(bytes, 2); // methods

    AppendClassU2(bytes, 0x0001); // public <init>
    AppendClassU2(bytes, 5);
    AppendClassU2(bytes, 6);
    AppendClassU2(bytes, 1);
    AppendClassU2(bytes, 7);
    AppendClassU4(bytes, 17);
    AppendClassU2(bytes, 1); // max_stack
    AppendClassU2(bytes, 1); // max_locals
    AppendClassU4(bytes, 5); // code_length
    AppendClassU1(bytes, 0x2A); // aload_0
    AppendClassU1(bytes, 0xB7); AppendClassU2(bytes, 8); // invokespecial Object.<init>
    AppendClassU1(bytes, 0xB1); // return
    AppendClassU2(bytes, 0); // exceptions
    AppendClassU2(bytes, 0); // code attrs

    AppendClassU2(bytes, 0x0109); // public static native
    AppendClassU2(bytes, 10);
    AppendClassU2(bytes, 11);
    AppendClassU2(bytes, 0);

    AppendClassU2(bytes, 1); // class attrs
    AppendClassU2(bytes, 12);
    AppendClassU4(bytes, 2);
    AppendClassU2(bytes, 13);

    return bytes;
}

jobject GetClassLoaderForClass(JNIEnv* env, jclass targetClass) {
    if (!env || !targetClass) return nullptr;

    jclass classClass = FindClassLoose(env, "java/lang/Class");
    if (!classClass || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (classClass) env->DeleteLocalRef(classClass);
        return nullptr;
    }

    jmethodID getClassLoader = GetMethodIDCompat(env, classClass, "getClassLoader", "()Ljava/lang/ClassLoader;");
    env->DeleteLocalRef(classClass);
    if (!getClassLoader || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return nullptr;
    }

    jobject loader = env->CallObjectMethod(targetClass, getClassLoader);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        if (loader) env->DeleteLocalRef(loader);
        return nullptr;
    }
    return loader;
}

bool EnsureMutedVoicePacketFilterHelper(JNIEnv* env) {
    if (!env) return false;
    if (g_mutedVoicePacketFilterHelperClass) return true;

    jclass helperClass = FindClassLoose(env, "MutedVoicePacketFilter");
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        if (helperClass) env->DeleteLocalRef(helperClass);
        helperClass = nullptr;
    }

    if (!helperClass) {
        jobject loader = GetClassLoaderForClass(env, g_mutedVoiceS02PacketClass);
        std::vector<unsigned char> helperBytes = BuildMutedVoicePacketFilterHelperClassBytes();
        helperClass = env->DefineClass(
            "MutedVoicePacketFilter",
            loader,
            reinterpret_cast<const jbyte*>(helperBytes.data()),
            (jsize)helperBytes.size());
        if (loader) env->DeleteLocalRef(loader);

        if (!helperClass || env->ExceptionCheck()) {
            bool hadException = env->ExceptionCheck();
            if (hadException) env->ExceptionClear();
            if (helperClass) env->DeleteLocalRef(helperClass);
            helperClass = FindClassLoose(env, "MutedVoicePacketFilter");
            if (!helperClass || env->ExceptionCheck()) {
                if (env->ExceptionCheck()) env->ExceptionClear();
                if (helperClass) env->DeleteLocalRef(helperClass);
                DebugLog("Muted Voice packet filter failed defining helper class%s",
                    hadException ? " (exception cleared)" : "");
                return false;
            }
        }

        DebugLog("Muted Voice packet filter defined helper class MutedVoicePacketFilter");
    }

    JNINativeMethod methods[1] = {};
    const std::string dispatchDescriptor = TranslateLunarDescriptor("(Leu;Lfj;Lfy;)V");
    methods[0].name = const_cast<char*>("a");
    methods[0].signature = const_cast<char*>(dispatchDescriptor.c_str());
    methods[0].fnPtr = reinterpret_cast<void*>(&MutedVoicePacketFilterDispatch);
    if (env->RegisterNatives(helperClass, methods, 1) != JNI_OK || env->ExceptionCheck()) {
        bool hadException = env->ExceptionCheck();
        if (hadException) env->ExceptionClear();
        DebugLog("Muted Voice packet filter RegisterNatives failed for helper%s",
            hadException ? " (exception cleared)" : "");
        env->DeleteLocalRef(helperClass);
        return false;
    }
    DebugLog("Muted Voice packet filter registered helper native a(Leu;Lfj;Lfy;)V");

    g_mutedVoicePacketFilterHelperClass = (jclass)env->NewGlobalRef(helperClass);
    env->DeleteLocalRef(helperClass);
    if (!g_mutedVoicePacketFilterHelperClass || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (g_mutedVoicePacketFilterHelperClass) env->DeleteGlobalRef(g_mutedVoicePacketFilterHelperClass);
        g_mutedVoicePacketFilterHelperClass = nullptr;
        DebugLog("Muted Voice packet filter failed global-ref helper class");
        return false;
    }

    return true;
}

bool LoadS02PacketChatClassBytes(JNIEnv* env, std::vector<unsigned char>& outBytes) {
    outBytes.clear();
    if (!env || !g_mutedVoiceS02PacketClass) return false;

    if (IsLunarNamedClient()) {
        return CaptureRuntimeClassBytes(
            env,
            g_mutedVoiceS02PacketClass,
            outBytes,
            "S02PacketChat named runtime");
    }

    jobject stream = OpenClassResourceStream(env, g_mutedVoiceS02PacketClass, "fy.class");
    if (!stream) stream = OpenClassResourceStream(env, g_mutedVoiceS02PacketClass, "/fy.class");
    if (!stream) stream = OpenClassLoaderResourceStream(env, g_mutedVoiceS02PacketClass, "fy.class");
    if (!stream) {
        DebugLog("Muted Voice packet filter failed to open fy.class resource");
        return false;
    }

    bool ok = ReadJavaInputStream(env, stream, outBytes);
    env->DeleteLocalRef(stream);
    if (!ok) DebugLog("Muted Voice packet filter failed to read fy.class bytes");
    return ok;
}

uint16_t AppendClassUtf8Cp(std::vector<unsigned char>& bytes, uint16_t& nextIndex, const char* value) {
    uint16_t index = nextIndex++;
    AppendClassU1(bytes, 1);
    size_t len = strlen(value);
    AppendClassU2(bytes, (uint16_t)len);
    bytes.insert(bytes.end(), value, value + len);
    return index;
}

uint16_t AppendClassClassCp(std::vector<unsigned char>& bytes, uint16_t& nextIndex, uint16_t nameIndex) {
    uint16_t index = nextIndex++;
    AppendClassU1(bytes, 7);
    AppendClassU2(bytes, nameIndex);
    return index;
}

uint16_t AppendClassNameAndTypeCp(std::vector<unsigned char>& bytes, uint16_t& nextIndex, uint16_t nameIndex, uint16_t descriptorIndex) {
    uint16_t index = nextIndex++;
    AppendClassU1(bytes, 12);
    AppendClassU2(bytes, nameIndex);
    AppendClassU2(bytes, descriptorIndex);
    return index;
}

uint16_t AppendClassMethodRefCp(std::vector<unsigned char>& bytes, uint16_t& nextIndex, uint16_t classIndex, uint16_t nameAndTypeIndex) {
    uint16_t index = nextIndex++;
    AppendClassU1(bytes, 10);
    AppendClassU2(bytes, classIndex);
    AppendClassU2(bytes, nameAndTypeIndex);
    return index;
}

bool PatchS02PacketChatProcessMethod(const std::vector<unsigned char>& originalBytes, std::vector<unsigned char>& patchedBytes) {
    patchedBytes = originalBytes;
    const char* packetOwner = "net/minecraft/network/play/server/S02PacketChat";
    const char* mappedProcessName = IsLunarNamedClient()
        ? FindLunarMethodName(packetOwner, "a", "(Lfj;)V")
        : nullptr;
    const char* mappedGetterName = IsLunarNamedClient()
        ? FindLunarMethodName(packetOwner, "a", "()Leu;")
        : nullptr;
    const std::string processName = mappedProcessName ? mappedProcessName : "a";
    const std::string getterName = mappedGetterName ? mappedGetterName : "a";
    const std::string processDescriptor = TranslateLunarDescriptor("(Lfj;)V");
    const std::string getterDescriptor = TranslateLunarDescriptor("()Leu;");
    const std::string dispatchDescriptor = TranslateLunarDescriptor("(Leu;Lfj;Lfy;)V");
    const char* mappedPacketClass = IsLunarNamedClient() ? FindLunarNamedClass("fy") : nullptr;
    const std::string packetClassName = mappedPacketClass ? mappedPacketClass : "fy";

    size_t cpOffset = 0;
    uint16_t constantPoolCount = 0;
    std::vector<std::string> utf8Constants;
    if (!ReadClassConstantPoolUtf8(patchedBytes, cpOffset, constantPoolCount, utf8Constants)) {
        DebugLog("Muted Voice packet filter invalid fy.class constant pool");
        return false;
    }

    if (constantPoolCount > 65520) {
        DebugLog("Muted Voice packet filter fy.class constant pool too large count=%u", (unsigned int)constantPoolCount);
        return false;
    }

    std::vector<unsigned char> cpAdditions;
    uint16_t nextIndex = constantPoolCount;

    uint16_t helperNameUtf8 = AppendClassUtf8Cp(cpAdditions, nextIndex, "MutedVoicePacketFilter");
    uint16_t helperClass = AppendClassClassCp(cpAdditions, nextIndex, helperNameUtf8);
    uint16_t dispatchNameUtf8 = AppendClassUtf8Cp(cpAdditions, nextIndex, "a");
    uint16_t dispatchDescUtf8 = AppendClassUtf8Cp(cpAdditions, nextIndex, dispatchDescriptor.c_str());
    uint16_t dispatchNameAndType = AppendClassNameAndTypeCp(cpAdditions, nextIndex, dispatchNameUtf8, dispatchDescUtf8);
    uint16_t dispatchMethodRef = AppendClassMethodRefCp(cpAdditions, nextIndex, helperClass, dispatchNameAndType);

    uint16_t packetNameUtf8 = AppendClassUtf8Cp(cpAdditions, nextIndex, packetClassName.c_str());
    uint16_t packetClass = AppendClassClassCp(cpAdditions, nextIndex, packetNameUtf8);
    uint16_t getterNameUtf8 = AppendClassUtf8Cp(cpAdditions, nextIndex, getterName.c_str());
    uint16_t getterDescUtf8 = AppendClassUtf8Cp(cpAdditions, nextIndex, getterDescriptor.c_str());
    uint16_t getterNameAndType = AppendClassNameAndTypeCp(cpAdditions, nextIndex, getterNameUtf8, getterDescUtf8);
    uint16_t getterMethodRef = AppendClassMethodRefCp(cpAdditions, nextIndex, packetClass, getterNameAndType);

    patchedBytes.insert(patchedBytes.begin() + (ptrdiff_t)cpOffset, cpAdditions.begin(), cpAdditions.end());
    if (!WriteClassU2At(patchedBytes, 8, nextIndex)) return false;

    size_t offset = 0;
    if (!ReadClassConstantPoolUtf8(patchedBytes, offset, constantPoolCount, utf8Constants)) {
        DebugLog("Muted Voice packet filter invalid patched fy.class constant pool");
        return false;
    }

    if (!AdvanceClassBytes(patchedBytes, offset, 6)) return false;

    uint16_t interfacesCount = 0;
    if (!ReadClassU2(patchedBytes, offset, interfacesCount) ||
        !AdvanceClassBytes(patchedBytes, offset, (size_t)interfacesCount * 2)) {
        return false;
    }

    uint16_t fieldsCount = 0;
    if (!ReadClassU2(patchedBytes, offset, fieldsCount)) return false;
    for (uint16_t i = 0; i < fieldsCount; ++i) {
        if (!SkipClassMemberInfo(patchedBytes, offset)) return false;
    }

    uint16_t methodsCount = 0;
    if (!ReadClassU2(patchedBytes, offset, methodsCount)) return false;
    for (uint16_t i = 0; i < methodsCount; ++i) {
        uint16_t accessFlags = 0;
        uint16_t nameIndex = 0;
        uint16_t descriptorIndex = 0;
        uint16_t attributesCount = 0;
        if (!ReadClassU2(patchedBytes, offset, accessFlags) ||
            !ReadClassU2(patchedBytes, offset, nameIndex) ||
            !ReadClassU2(patchedBytes, offset, descriptorIndex) ||
            !ReadClassU2(patchedBytes, offset, attributesCount)) {
            return false;
        }

        const std::string methodName =
            nameIndex < utf8Constants.size() ? utf8Constants[(size_t)nameIndex] : std::string();
        const std::string descriptor =
            descriptorIndex < utf8Constants.size() ? utf8Constants[(size_t)descriptorIndex] : std::string();

        for (uint16_t attr = 0; attr < attributesCount; ++attr) {
            uint16_t attributeNameIndex = 0;
            uint32_t attributeLength = 0;
            if (!ReadClassU2(patchedBytes, offset, attributeNameIndex)) return false;
            size_t attributeLengthOffset = offset;
            if (!ReadClassU4(patchedBytes, offset, attributeLength)) return false;

            size_t attributeStart = offset;
            if (!AdvanceClassBytes(patchedBytes, offset, (size_t)attributeLength)) return false;
            size_t attributeEnd = offset;
            const std::string attributeName =
                attributeNameIndex < utf8Constants.size() ? utf8Constants[(size_t)attributeNameIndex] : std::string();

            if (methodName == processName && descriptor == processDescriptor && attributeName == "Code") {
                size_t codeOffset = attributeStart;
                uint16_t oldMaxStack = 0;
                uint16_t oldMaxLocals = 0;
                uint32_t oldCodeLength = 0;
                if (!ReadClassU2(patchedBytes, codeOffset, oldMaxStack) ||
                    !ReadClassU2(patchedBytes, codeOffset, oldMaxLocals) ||
                    !ReadClassU4(patchedBytes, codeOffset, oldCodeLength) ||
                    codeOffset + oldCodeLength > attributeEnd) {
                    DebugLog("Muted Voice packet filter unsupported fy.a(Lfj;)V Code attribute");
                    return false;
                }

                std::vector<unsigned char> code;
                AppendClassU1(code, 0x2A); // aload_0
                AppendClassU1(code, 0xB6); AppendClassU2(code, getterMethodRef); // invokevirtual fy.a()Leu;
                AppendClassU1(code, 0x2B); // aload_1
                AppendClassU1(code, 0x2A); // aload_0
                AppendClassU1(code, 0xB8); AppendClassU2(code, dispatchMethodRef); // invokestatic helper.a(Leu;Lfj;Lfy;)V
                AppendClassU1(code, 0xB1); // return

                std::vector<unsigned char> codeAttribute;
                AppendClassU2(codeAttribute, oldMaxStack < 3 ? 3 : oldMaxStack);
                AppendClassU2(codeAttribute, oldMaxLocals < 2 ? 2 : oldMaxLocals);
                AppendClassU4(codeAttribute, (uint32_t)code.size());
                codeAttribute.insert(codeAttribute.end(), code.begin(), code.end());
                AppendClassU2(codeAttribute, 0); // exception table
                AppendClassU2(codeAttribute, 0); // code attributes

                if (!WriteClassU4At(patchedBytes, attributeLengthOffset, (uint32_t)codeAttribute.size())) return false;
                patchedBytes.erase(patchedBytes.begin() + (ptrdiff_t)attributeStart, patchedBytes.begin() + (ptrdiff_t)attributeEnd);
                patchedBytes.insert(patchedBytes.begin() + (ptrdiff_t)attributeStart, codeAttribute.begin(), codeAttribute.end());

                DebugLog("Muted Voice packet filter patched fy.a(Lfj;)V oldCodeLength=%u newCodeLength=%u",
                    (unsigned int)oldCodeLength,
                    (unsigned int)code.size());
                return true;
            }
        }
    }

    DebugLog("Muted Voice packet filter failed to find fy.a(Lfj;)V Code attribute");
    return false;
}

bool EnsureMutedVoiceS02PacketChatBytecode(JNIEnv* env) {
    if (!g_mutedVoiceOriginalS02PacketBytes.empty() && !g_mutedVoicePatchedS02PacketBytes.empty()) return true;

    if (!LoadS02PacketChatClassBytes(env, g_mutedVoiceOriginalS02PacketBytes)) {
        g_mutedVoiceOriginalS02PacketBytes.clear();
        g_mutedVoicePatchedS02PacketBytes.clear();
        return false;
    }

    if (!PatchS02PacketChatProcessMethod(g_mutedVoiceOriginalS02PacketBytes, g_mutedVoicePatchedS02PacketBytes)) {
        g_mutedVoiceOriginalS02PacketBytes.clear();
        g_mutedVoicePatchedS02PacketBytes.clear();
        return false;
    }

    DebugLog("Muted Voice packet filter loaded fy.class bytes=%u patchedBytes=%u",
        (unsigned int)g_mutedVoiceOriginalS02PacketBytes.size(),
        (unsigned int)g_mutedVoicePatchedS02PacketBytes.size());
    return true;
}

bool PatchBarrierClassGetRenderType(const std::vector<unsigned char>& originalBytes, std::vector<unsigned char>& patchedBytes) {
    patchedBytes = originalBytes;
    size_t offset = 0;

    uint32_t magic = 0;
    uint16_t minorVersion = 0;
    uint16_t majorVersion = 0;
    uint16_t constantPoolCount = 0;
    if (!ReadClassU4(patchedBytes, offset, magic) ||
        magic != 0xCAFEBABE ||
        !ReadClassU2(patchedBytes, offset, minorVersion) ||
        !ReadClassU2(patchedBytes, offset, majorVersion) ||
        !ReadClassU2(patchedBytes, offset, constantPoolCount)) {
        DebugLog("See Barriers invalid afb.class header");
        return false;
    }

    std::vector<std::string> utf8Constants((size_t)constantPoolCount);
    for (uint16_t i = 1; i < constantPoolCount; ++i) {
        unsigned char tag = 0;
        if (!ReadClassU1(patchedBytes, offset, tag)) return false;

        switch (tag) {
        case 1: {
            uint16_t length = 0;
            if (!ReadClassU2(patchedBytes, offset, length) ||
                offset + length > patchedBytes.size()) {
                return false;
            }
            utf8Constants[(size_t)i] = std::string(
                reinterpret_cast<const char*>(&patchedBytes[offset]),
                (size_t)length);
            offset += length;
            break;
        }
        case 3:
        case 4:
            if (!AdvanceClassBytes(patchedBytes, offset, 4)) return false;
            break;
        case 5:
        case 6:
            if (!AdvanceClassBytes(patchedBytes, offset, 8)) return false;
            ++i;
            break;
        case 7:
        case 8:
        case 16:
            if (!AdvanceClassBytes(patchedBytes, offset, 2)) return false;
            break;
        case 9:
        case 10:
        case 11:
        case 12:
        case 18:
            if (!AdvanceClassBytes(patchedBytes, offset, 4)) return false;
            break;
        case 15:
            if (!AdvanceClassBytes(patchedBytes, offset, 3)) return false;
            break;
        default:
            DebugLog("See Barriers unsupported classfile constant tag=%u", (unsigned int)tag);
            return false;
        }
    }

    if (!AdvanceClassBytes(patchedBytes, offset, 6)) return false; // access_flags, this_class, super_class

    uint16_t interfacesCount = 0;
    if (!ReadClassU2(patchedBytes, offset, interfacesCount) ||
        !AdvanceClassBytes(patchedBytes, offset, (size_t)interfacesCount * 2)) {
        return false;
    }

    uint16_t fieldsCount = 0;
    if (!ReadClassU2(patchedBytes, offset, fieldsCount)) return false;
    for (uint16_t i = 0; i < fieldsCount; ++i) {
        if (!SkipClassMemberInfo(patchedBytes, offset)) return false;
    }

    uint16_t methodsCount = 0;
    if (!ReadClassU2(patchedBytes, offset, methodsCount)) return false;
    for (uint16_t i = 0; i < methodsCount; ++i) {
        uint16_t accessFlags = 0;
        uint16_t nameIndex = 0;
        uint16_t descriptorIndex = 0;
        uint16_t attributesCount = 0;
        if (!ReadClassU2(patchedBytes, offset, accessFlags) ||
            !ReadClassU2(patchedBytes, offset, nameIndex) ||
            !ReadClassU2(patchedBytes, offset, descriptorIndex) ||
            !ReadClassU2(patchedBytes, offset, attributesCount)) {
            return false;
        }

        const std::string methodName =
            nameIndex < utf8Constants.size() ? utf8Constants[(size_t)nameIndex] : std::string();
        const std::string descriptor =
            descriptorIndex < utf8Constants.size() ? utf8Constants[(size_t)descriptorIndex] : std::string();

        for (uint16_t attr = 0; attr < attributesCount; ++attr) {
            uint16_t attributeNameIndex = 0;
            uint32_t attributeLength = 0;
            if (!ReadClassU2(patchedBytes, offset, attributeNameIndex) ||
                !ReadClassU4(patchedBytes, offset, attributeLength)) {
                return false;
            }

            size_t attributeStart = offset;
            if (!AdvanceClassBytes(patchedBytes, offset, (size_t)attributeLength)) return false;
            size_t attributeEnd = offset;
            const std::string attributeName =
                attributeNameIndex < utf8Constants.size() ? utf8Constants[(size_t)attributeNameIndex] : std::string();

            if (methodName == "b" && descriptor == "()I" && attributeName == "Code") {
                size_t codeOffset = attributeStart;
                uint16_t maxStack = 0;
                uint16_t maxLocals = 0;
                uint32_t codeLength = 0;
                if (!ReadClassU2(patchedBytes, codeOffset, maxStack) ||
                    !ReadClassU2(patchedBytes, codeOffset, maxLocals) ||
                    !ReadClassU4(patchedBytes, codeOffset, codeLength) ||
                    codeLength < 2 ||
                    codeOffset + codeLength > attributeEnd ||
                    maxStack < 1 ||
                    maxLocals < 1) {
                    DebugLog("See Barriers unsupported BlockBarrier.getRenderType Code attribute");
                    return false;
                }

                patchedBytes[codeOffset] = 0x06; // iconst_3
                patchedBytes[codeOffset + 1] = 0xAC; // ireturn
                for (uint32_t n = 2; n < codeLength; ++n) {
                    patchedBytes[codeOffset + (size_t)n] = 0x00; // nop
                }

                DebugLog("See Barriers patched afb.b()I codeLength=%u version=%u.%u",
                    (unsigned int)codeLength,
                    (unsigned int)majorVersion,
                    (unsigned int)minorVersion);
                return true;
            }
        }
    }

    DebugLog("See Barriers failed to find afb.b()I Code attribute");
    return false;
}

bool EnsureBarrierClassBytecode(JNIEnv* env) {
    if (!g_seeBarriersOriginalClassBytes.empty() && !g_seeBarriersPatchedClassBytes.empty()) return true;

    if (!LoadBarrierClassBytes(env, g_seeBarriersOriginalClassBytes)) {
        g_seeBarriersOriginalClassBytes.clear();
        g_seeBarriersPatchedClassBytes.clear();
        return false;
    }

    if (!PatchBarrierClassGetRenderType(g_seeBarriersOriginalClassBytes, g_seeBarriersPatchedClassBytes)) {
        g_seeBarriersOriginalClassBytes.clear();
        g_seeBarriersPatchedClassBytes.clear();
        return false;
    }

    DebugLog("See Barriers loaded afb.class bytes=%u", (unsigned int)g_seeBarriersOriginalClassBytes.size());
    return true;
}

bool LoadBlockDamageDispatcherClassBytes(JNIEnv* env, std::vector<unsigned char>& outBytes) {
    outBytes.clear();
    if (!EnsureSeeBarriersModelOverrideJNI(env) || !g_seeBarriersBlockRendererDispatcherClass) return false;

    jobject stream = OpenClassResourceStream(env, g_seeBarriersBlockRendererDispatcherClass, "bgd.class");
    if (!stream) stream = OpenClassResourceStream(env, g_seeBarriersBlockRendererDispatcherClass, "/bgd.class");
    if (!stream) stream = OpenClassLoaderResourceStream(env, g_seeBarriersBlockRendererDispatcherClass, "bgd.class");
    if (!stream) {
        DebugLog("See Barriers failed to open bgd.class resource");
        return false;
    }

    bool ok = ReadJavaInputStream(env, stream, outBytes);
    env->DeleteLocalRef(stream);
    if (!ok) DebugLog("See Barriers failed to read bgd.class bytes");
    return ok;
}

bool GetSeeBarriersRenderChunkClass(JNIEnv* env, jclass& outClass) {
    outClass = nullptr;
    if (!env) return false;
    if (g_seeBarriersRenderChunkClass) {
        outClass = g_seeBarriersRenderChunkClass;
        return true;
    }

    jclass localClass = FindClassLoose(env, "bht");
    if (!localClass || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (localClass) env->DeleteLocalRef(localClass);
        DebugLog("See Barriers failed to find RenderChunk class bht");
        return false;
    }

    g_seeBarriersRenderChunkClass = (jclass)env->NewGlobalRef(localClass);
    env->DeleteLocalRef(localClass);
    if (!g_seeBarriersRenderChunkClass || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (g_seeBarriersRenderChunkClass) env->DeleteGlobalRef(g_seeBarriersRenderChunkClass);
        g_seeBarriersRenderChunkClass = nullptr;
        DebugLog("See Barriers failed to global-ref RenderChunk class");
        return false;
    }

    outClass = g_seeBarriersRenderChunkClass;
    DebugLog("See Barriers resolved RenderChunk class bht");
    return true;
}

bool LoadRenderChunkClassBytes(JNIEnv* env, std::vector<unsigned char>& outBytes) {
    outBytes.clear();
    jclass renderChunkClass = nullptr;
    if (!GetSeeBarriersRenderChunkClass(env, renderChunkClass)) return false;

    jobject stream = OpenClassResourceStream(env, renderChunkClass, "bht.class");
    if (!stream) stream = OpenClassResourceStream(env, renderChunkClass, "/bht.class");
    if (!stream) stream = OpenClassLoaderResourceStream(env, renderChunkClass, "bht.class");
    if (!stream) {
        DebugLog("See Barriers failed to open bht.class resource");
        return false;
    }

    bool ok = ReadJavaInputStream(env, stream, outBytes);
    env->DeleteLocalRef(stream);
    if (!ok) DebugLog("See Barriers failed to read bht.class bytes");
    return ok;
}

bool PatchBlockRendererDispatcherSeeBarriers(const std::vector<unsigned char>& originalBytes, std::vector<unsigned char>& patchedBytes) {
    patchedBytes = originalBytes;
    size_t offset = 0;

    uint32_t magic = 0;
    uint16_t minorVersion = 0;
    uint16_t majorVersion = 0;
    uint16_t constantPoolCount = 0;
    if (!ReadClassU4(patchedBytes, offset, magic) ||
        magic != 0xCAFEBABE ||
        !ReadClassU2(patchedBytes, offset, minorVersion) ||
        !ReadClassU2(patchedBytes, offset, majorVersion) ||
        !ReadClassU2(patchedBytes, offset, constantPoolCount)) {
        DebugLog("See Barriers invalid bgd.class header");
        return false;
    }

    std::vector<std::string> utf8Constants((size_t)constantPoolCount);
    for (uint16_t i = 1; i < constantPoolCount; ++i) {
        unsigned char tag = 0;
        if (!ReadClassU1(patchedBytes, offset, tag)) return false;

        switch (tag) {
        case 1: {
            uint16_t length = 0;
            if (!ReadClassU2(patchedBytes, offset, length) ||
                offset + length > patchedBytes.size()) {
                return false;
            }
            utf8Constants[(size_t)i] = std::string(
                reinterpret_cast<const char*>(&patchedBytes[offset]),
                (size_t)length);
            offset += length;
            break;
        }
        case 3:
        case 4:
            if (!AdvanceClassBytes(patchedBytes, offset, 4)) return false;
            break;
        case 5:
        case 6:
            if (!AdvanceClassBytes(patchedBytes, offset, 8)) return false;
            ++i;
            break;
        case 7:
        case 8:
        case 16:
            if (!AdvanceClassBytes(patchedBytes, offset, 2)) return false;
            break;
        case 9:
        case 10:
        case 11:
        case 12:
        case 18:
            if (!AdvanceClassBytes(patchedBytes, offset, 4)) return false;
            break;
        case 15:
            if (!AdvanceClassBytes(patchedBytes, offset, 3)) return false;
            break;
        default:
            DebugLog("See Barriers unsupported bgd.class constant tag=%u", (unsigned int)tag);
            return false;
        }
    }

    if (!AdvanceClassBytes(patchedBytes, offset, 6)) return false; // access_flags, this_class, super_class

    uint16_t interfacesCount = 0;
    if (!ReadClassU2(patchedBytes, offset, interfacesCount) ||
        !AdvanceClassBytes(patchedBytes, offset, (size_t)interfacesCount * 2)) {
        return false;
    }

    uint16_t fieldsCount = 0;
    if (!ReadClassU2(patchedBytes, offset, fieldsCount)) return false;
    for (uint16_t i = 0; i < fieldsCount; ++i) {
        if (!SkipClassMemberInfo(patchedBytes, offset)) return false;
    }

    auto readCodeS4 = [&patchedBytes](size_t at, int32_t& value) -> bool {
        if (at + 4 > patchedBytes.size()) return false;
        value =
            ((int32_t)patchedBytes[at] << 24) |
            ((int32_t)patchedBytes[at + 1] << 16) |
            ((int32_t)patchedBytes[at + 2] << 8) |
            (int32_t)patchedBytes[at + 3];
        return true;
    };

    auto writeCodeS4 = [&patchedBytes](size_t at, int32_t value) -> bool {
        if (at + 4 > patchedBytes.size()) return false;
        patchedBytes[at] = (unsigned char)((value >> 24) & 0xFF);
        patchedBytes[at + 1] = (unsigned char)((value >> 16) & 0xFF);
        patchedBytes[at + 2] = (unsigned char)((value >> 8) & 0xFF);
        patchedBytes[at + 3] = (unsigned char)(value & 0xFF);
        return true;
    };

    bool damageOverlayPatched = false;
    bool worldRenderPatched = false;

    uint16_t methodsCount = 0;
    if (!ReadClassU2(patchedBytes, offset, methodsCount)) return false;
    for (uint16_t i = 0; i < methodsCount; ++i) {
        uint16_t accessFlags = 0;
        uint16_t nameIndex = 0;
        uint16_t descriptorIndex = 0;
        uint16_t attributesCount = 0;
        if (!ReadClassU2(patchedBytes, offset, accessFlags) ||
            !ReadClassU2(patchedBytes, offset, nameIndex) ||
            !ReadClassU2(patchedBytes, offset, descriptorIndex) ||
            !ReadClassU2(patchedBytes, offset, attributesCount)) {
            return false;
        }

        const std::string methodName =
            nameIndex < utf8Constants.size() ? utf8Constants[(size_t)nameIndex] : std::string();
        const std::string descriptor =
            descriptorIndex < utf8Constants.size() ? utf8Constants[(size_t)descriptorIndex] : std::string();

        for (uint16_t attr = 0; attr < attributesCount; ++attr) {
            uint16_t attributeNameIndex = 0;
            uint32_t attributeLength = 0;
            if (!ReadClassU2(patchedBytes, offset, attributeNameIndex) ||
                !ReadClassU4(patchedBytes, offset, attributeLength)) {
                return false;
            }

            size_t attributeStart = offset;
            if (!AdvanceClassBytes(patchedBytes, offset, (size_t)attributeLength)) return false;
            size_t attributeEnd = offset;
            const std::string attributeName =
                attributeNameIndex < utf8Constants.size() ? utf8Constants[(size_t)attributeNameIndex] : std::string();

            if (methodName == "a" && descriptor == "(Lalz;Lcj;Lbmi;Ladq;)V" && attributeName == "Code") {
                size_t codeOffset = attributeStart;
                uint16_t maxStack = 0;
                uint16_t maxLocals = 0;
                uint32_t codeLength = 0;
                if (!ReadClassU2(patchedBytes, codeOffset, maxStack) ||
                    !ReadClassU2(patchedBytes, codeOffset, maxLocals) ||
                    !ReadClassU4(patchedBytes, codeOffset, codeLength) ||
                    codeOffset + codeLength > attributeEnd ||
                    maxStack < 1 ||
                    maxLocals < 7) {
                    DebugLog("See Barriers unsupported bgd damage-overlay Code attribute");
                    return false;
                }

                for (uint32_t codeIndex = 0; codeIndex + 6 < codeLength; ++codeIndex) {
                    size_t p = codeOffset + (size_t)codeIndex;
                    if (patchedBytes[p] == 0x15 && patchedBytes[p + 1] == 0x06 &&
                        patchedBytes[p + 2] == 0x06 &&
                        patchedBytes[p + 3] == 0x9F &&
                        patchedBytes[p + 6] == 0xB1) {
                        for (size_t n = 0; n < 7; ++n) patchedBytes[p + n] = 0x00;
                        DebugLog("See Barriers patched bgd damage overlay gate codeLength=%u offset=%u version=%u.%u",
                            (unsigned int)codeLength,
                            (unsigned int)codeIndex,
                            (unsigned int)majorVersion,
                            (unsigned int)minorVersion);
                        damageOverlayPatched = true;
                        break;
                    }
                }

                if (!damageOverlayPatched) {
                    DebugLog("See Barriers failed to find bgd damage overlay gate pattern");
                    return false;
                }
            }

            if (methodName == "a" && descriptor == "(Lalz;Lcj;Ladq;Lbfd;)Z" && attributeName == "Code") {
                size_t codeOffset = attributeStart;
                uint16_t maxStack = 0;
                uint16_t maxLocals = 0;
                uint32_t codeLength = 0;
                if (!ReadClassU2(patchedBytes, codeOffset, maxStack) ||
                    !ReadClassU2(patchedBytes, codeOffset, maxLocals) ||
                    !ReadClassU4(patchedBytes, codeOffset, codeLength) ||
                    codeOffset + codeLength > attributeEnd ||
                    maxStack < 1 ||
                    maxLocals < 6) {
                    DebugLog("See Barriers unsupported bgd world-render Code attribute");
                    return false;
                }

                size_t codeStart = codeOffset;
                bool earlyReturnPatched = false;
                for (uint32_t codeIndex = 0; codeIndex + 7 < codeLength; ++codeIndex) {
                    size_t p = codeStart + (size_t)codeIndex;
                    if (patchedBytes[p] == 0x15 &&
                        patchedBytes[p + 2] == 0x02 &&
                        patchedBytes[p + 3] == 0xA0 &&
                        patchedBytes[p + 6] == 0x03 &&
                        patchedBytes[p + 7] == 0xAC) {
                        for (size_t n = 0; n < 8; ++n) patchedBytes[p + n] = 0x00;
                        earlyReturnPatched = true;
                        DebugLog("See Barriers patched bgd world-render early return codeLength=%u offset=%u version=%u.%u",
                            (unsigned int)codeLength,
                            (unsigned int)codeIndex,
                            (unsigned int)majorVersion,
                            (unsigned int)minorVersion);
                        break;
                    }
                }

                if (!earlyReturnPatched) {
                    DebugLog("See Barriers failed to find bgd world-render early return pattern");
                    return false;
                }

                bool switchPatched = false;
                for (uint32_t codeIndex = 0; codeIndex < codeLength; ++codeIndex) {
                    size_t opcodeAt = codeStart + (size_t)codeIndex;
                    if (patchedBytes[opcodeAt] != 0xAA) continue;

                    size_t alignedAt = opcodeAt + 1;
                    while (((alignedAt - codeStart) & 3u) != 0u) ++alignedAt;
                    if (alignedAt + 24 > codeStart + codeLength) continue;

                    int32_t defaultOffset = 0;
                    int32_t low = 0;
                    int32_t high = 0;
                    if (!readCodeS4(alignedAt, defaultOffset) ||
                        !readCodeS4(alignedAt + 4, low) ||
                        !readCodeS4(alignedAt + 8, high)) {
                        continue;
                    }

                    if (low != 1 || high != 3) continue;

                    int32_t caseThreeOffset = 0;
                    size_t jumpTableAt = alignedAt + 12;
                    if (!readCodeS4(jumpTableAt + 8, caseThreeOffset)) continue;
                    if (!writeCodeS4(alignedAt, caseThreeOffset)) continue;

                    switchPatched = true;
                    DebugLog("See Barriers patched bgd world-render default->case3 codeLength=%u offset=%u version=%u.%u",
                        (unsigned int)codeLength,
                        (unsigned int)codeIndex,
                        (unsigned int)majorVersion,
                        (unsigned int)minorVersion);
                    break;
                }

                if (!switchPatched) {
                    DebugLog("See Barriers failed to find bgd world-render tableswitch");
                    return false;
                }

                worldRenderPatched = true;
            }
        }
    }

    if (!damageOverlayPatched) {
        DebugLog("See Barriers failed to find bgd.a(alz,cj,bmi,adq) Code attribute");
        return false;
    }
    if (!worldRenderPatched) {
        DebugLog("See Barriers failed to find bgd.a(alz,cj,adq,bfd) Code attribute");
        return false;
    }

    return true;
}

bool PatchRenderChunkSeeBarriers(const std::vector<unsigned char>& originalBytes, std::vector<unsigned char>& patchedBytes) {
    patchedBytes = originalBytes;
    size_t offset = 0;

    uint32_t magic = 0;
    uint16_t minorVersion = 0;
    uint16_t majorVersion = 0;
    uint16_t constantPoolCount = 0;
    if (!ReadClassU4(patchedBytes, offset, magic) ||
        magic != 0xCAFEBABE ||
        !ReadClassU2(patchedBytes, offset, minorVersion) ||
        !ReadClassU2(patchedBytes, offset, majorVersion) ||
        !ReadClassU2(patchedBytes, offset, constantPoolCount)) {
        DebugLog("See Barriers invalid bht.class header");
        return false;
    }

    std::vector<std::string> utf8Constants((size_t)constantPoolCount);
    for (uint16_t i = 1; i < constantPoolCount; ++i) {
        unsigned char tag = 0;
        if (!ReadClassU1(patchedBytes, offset, tag)) return false;

        switch (tag) {
        case 1: {
            uint16_t length = 0;
            if (!ReadClassU2(patchedBytes, offset, length) ||
                offset + length > patchedBytes.size()) {
                return false;
            }
            utf8Constants[(size_t)i] = std::string(
                reinterpret_cast<const char*>(&patchedBytes[offset]),
                (size_t)length);
            offset += length;
            break;
        }
        case 3:
        case 4:
            if (!AdvanceClassBytes(patchedBytes, offset, 4)) return false;
            break;
        case 5:
        case 6:
            if (!AdvanceClassBytes(patchedBytes, offset, 8)) return false;
            ++i;
            break;
        case 7:
        case 8:
        case 16:
            if (!AdvanceClassBytes(patchedBytes, offset, 2)) return false;
            break;
        case 9:
        case 10:
        case 11:
        case 12:
        case 18:
            if (!AdvanceClassBytes(patchedBytes, offset, 4)) return false;
            break;
        case 15:
            if (!AdvanceClassBytes(patchedBytes, offset, 3)) return false;
            break;
        default:
            DebugLog("See Barriers unsupported bht.class constant tag=%u", (unsigned int)tag);
            return false;
        }
    }

    if (!AdvanceClassBytes(patchedBytes, offset, 6)) return false;

    uint16_t interfacesCount = 0;
    if (!ReadClassU2(patchedBytes, offset, interfacesCount) ||
        !AdvanceClassBytes(patchedBytes, offset, (size_t)interfacesCount * 2)) {
        return false;
    }

    uint16_t fieldsCount = 0;
    if (!ReadClassU2(patchedBytes, offset, fieldsCount)) return false;
    for (uint16_t i = 0; i < fieldsCount; ++i) {
        if (!SkipClassMemberInfo(patchedBytes, offset)) return false;
    }

    bool rebuildPatched = false;

    uint16_t methodsCount = 0;
    if (!ReadClassU2(patchedBytes, offset, methodsCount)) return false;
    for (uint16_t i = 0; i < methodsCount; ++i) {
        uint16_t accessFlags = 0;
        uint16_t nameIndex = 0;
        uint16_t descriptorIndex = 0;
        uint16_t attributesCount = 0;
        if (!ReadClassU2(patchedBytes, offset, accessFlags) ||
            !ReadClassU2(patchedBytes, offset, nameIndex) ||
            !ReadClassU2(patchedBytes, offset, descriptorIndex) ||
            !ReadClassU2(patchedBytes, offset, attributesCount)) {
            return false;
        }

        const std::string methodName =
            nameIndex < utf8Constants.size() ? utf8Constants[(size_t)nameIndex] : std::string();
        const std::string descriptor =
            descriptorIndex < utf8Constants.size() ? utf8Constants[(size_t)descriptorIndex] : std::string();

        for (uint16_t attr = 0; attr < attributesCount; ++attr) {
            uint16_t attributeNameIndex = 0;
            uint32_t attributeLength = 0;
            if (!ReadClassU2(patchedBytes, offset, attributeNameIndex) ||
                !ReadClassU4(patchedBytes, offset, attributeLength)) {
                return false;
            }

            size_t attributeStart = offset;
            if (!AdvanceClassBytes(patchedBytes, offset, (size_t)attributeLength)) return false;
            size_t attributeEnd = offset;
            const std::string attributeName =
                attributeNameIndex < utf8Constants.size() ? utf8Constants[(size_t)attributeNameIndex] : std::string();

            if (methodName == "b" && descriptor == "(FFFLbhn;)V" && attributeName == "Code") {
                size_t codeOffset = attributeStart;
                uint16_t maxStack = 0;
                uint16_t maxLocals = 0;
                uint32_t codeLength = 0;
                if (!ReadClassU2(patchedBytes, codeOffset, maxStack) ||
                    !ReadClassU2(patchedBytes, codeOffset, maxLocals) ||
                    !ReadClassU4(patchedBytes, codeOffset, codeLength) ||
                    codeLength < 8 ||
                    codeOffset + codeLength > attributeEnd) {
                    DebugLog("See Barriers unsupported bht.b(FFFLbhn;)V Code attribute");
                    return false;
                }

                size_t codeStart = codeOffset;
                for (uint32_t codeIndex = 3; codeIndex + 6 < codeLength; ++codeIndex) {
                    size_t opcodeAt = codeStart + (size_t)codeIndex;
                    if (patchedBytes[opcodeAt] != 0x02 ||
                        patchedBytes[opcodeAt + 1] != 0x9F ||
                        patchedBytes[opcodeAt + 4] != 0x19 ||
                        patchedBytes[opcodeAt + 5] != 0x04 ||
                        patchedBytes[opcodeAt + 6] != 0xB6 ||
                        patchedBytes[opcodeAt - 3] != 0xB6) {
                        continue;
                    }

                    int branchOffset =
                        ((int)(signed char)patchedBytes[opcodeAt + 2] << 8) |
                        (int)patchedBytes[opcodeAt + 3];
                    if (branchOffset <= 0) continue;

                    patchedBytes[opcodeAt] = 0x57; // pop
                    patchedBytes[opcodeAt + 1] = 0x00; // nop
                    patchedBytes[opcodeAt + 2] = 0x00; // nop
                    patchedBytes[opcodeAt + 3] = 0x00; // nop

                    rebuildPatched = true;
                    DebugLog("See Barriers patched bht rebuild renderType gate codeLength=%u offset=%u version=%u.%u",
                        (unsigned int)codeLength,
                        (unsigned int)codeIndex,
                        (unsigned int)majorVersion,
                        (unsigned int)minorVersion);
                    break;
                }

                if (!rebuildPatched) {
                    DebugLog("See Barriers failed to find bht rebuild renderType gate");
                    return false;
                }
            }
        }
    }

    if (!rebuildPatched) {
        DebugLog("See Barriers failed to find bht.b(FFFLbhn;)V Code attribute");
        return false;
    }

    return true;
}

bool EnsureBlockDamageDispatcherBytecode(JNIEnv* env) {
    if (!g_seeBarriersOriginalDamageDispatcherBytes.empty() &&
        !g_seeBarriersPatchedDamageDispatcherBytes.empty()) {
        return true;
    }

    if (!LoadBlockDamageDispatcherClassBytes(env, g_seeBarriersOriginalDamageDispatcherBytes)) {
        g_seeBarriersOriginalDamageDispatcherBytes.clear();
        g_seeBarriersPatchedDamageDispatcherBytes.clear();
        return false;
    }

    if (!PatchBlockRendererDispatcherSeeBarriers(
        g_seeBarriersOriginalDamageDispatcherBytes,
        g_seeBarriersPatchedDamageDispatcherBytes)) {
        g_seeBarriersOriginalDamageDispatcherBytes.clear();
        g_seeBarriersPatchedDamageDispatcherBytes.clear();
        return false;
    }

    DebugLog("See Barriers loaded bgd.class bytes=%u", (unsigned int)g_seeBarriersOriginalDamageDispatcherBytes.size());
    return true;
}

bool EnsureRenderChunkBytecode(JNIEnv* env) {
    if (!g_seeBarriersOriginalRenderChunkBytes.empty() &&
        !g_seeBarriersPatchedRenderChunkBytes.empty()) {
        return true;
    }

    if (!LoadRenderChunkClassBytes(env, g_seeBarriersOriginalRenderChunkBytes)) {
        g_seeBarriersOriginalRenderChunkBytes.clear();
        g_seeBarriersPatchedRenderChunkBytes.clear();
        return false;
    }

    if (!PatchRenderChunkSeeBarriers(
        g_seeBarriersOriginalRenderChunkBytes,
        g_seeBarriersPatchedRenderChunkBytes)) {
        g_seeBarriersOriginalRenderChunkBytes.clear();
        g_seeBarriersPatchedRenderChunkBytes.clear();
        return false;
    }

    DebugLog("See Barriers loaded bht.class bytes=%u", (unsigned int)g_seeBarriersOriginalRenderChunkBytes.size());
    return true;
}

void JNICALL SharedClassFileLoadHook(
    jvmtiEnv* jvmtiEnv,
    JNIEnv* env,
    jclass classBeingRedefined,
    jobject loader,
    const char* name,
    jobject protectionDomain,
    jint classDataLen,
    const unsigned char* classData,
    jint* newClassDataLen,
    unsigned char** newClassData) {
    (void)jvmtiEnv;
    (void)loader;
    (void)name;
    (void)protectionDomain;
    (void)newClassDataLen;
    (void)newClassData;

    if (InterlockedCompareExchange(&g_runtimeClassCaptureEnabled, 0, 0) != 0 &&
        env && classBeingRedefined && g_runtimeClassCaptureTarget &&
        classDataLen > 0 && classData &&
        env->IsSameObject(classBeingRedefined, g_runtimeClassCaptureTarget) == JNI_TRUE) {
        g_runtimeCapturedClassBytes.assign(classData, classData + (size_t)classDataLen);
    }

}

bool InitJvmtiRetransformSupport() {
    if (!InitJvmtiRedefineSupport() || !g_jvmti) return false;

    jvmtiCapabilities caps = {};
    jvmtiError err = g_jvmti->GetCapabilities(&caps);
    if (err != JVMTI_ERROR_NONE) {
        DebugLog("JVMTI GetCapabilities for retransform failed err=%d", (int)err);
        return false;
    }

    if (!caps.can_retransform_classes) {
        jvmtiCapabilities requested = {};
        requested.can_retransform_classes = 1;
        err = g_jvmti->AddCapabilities(&requested);
        if (err != JVMTI_ERROR_NONE) {
            DebugLog("JVMTI AddCapabilities retransform failed err=%d", (int)err);
            return false;
        }

        caps = {};
        err = g_jvmti->GetCapabilities(&caps);
        if (err != JVMTI_ERROR_NONE || !caps.can_retransform_classes) {
            DebugLog("JVMTI retransform capability missing err=%d canRetransform=%d",
                (int)err,
                caps.can_retransform_classes ? 1 : 0);
            return false;
        }
    }

    if (!g_sharedClassFileHookInstalled) {
        if (!SetSharedJvmtiCallbacks("Shared")) return false;

        err = g_jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, nullptr);
        if (err != JVMTI_ERROR_NONE) {
            DebugLog("JVMTI enable ClassFileLoadHook failed err=%d", (int)err);
            return false;
        }

        g_sharedClassFileHookInstalled = true;
        DebugLog("JVMTI ClassFileLoadHook installed");
    }

    return true;
}

bool RetransformBarrierClass(JNIEnv* env, bool enabled) {
    (void)env;
    (void)enabled;
    return false;
}

bool CaptureRuntimeClassBytes(JNIEnv* env, jclass targetClass, std::vector<unsigned char>& outBytes, const char* label) {
    outBytes.clear();
    if (!env || !targetClass || !InitJvmtiRetransformSupport() || !g_jvmti) return false;

    jboolean modifiable = JNI_FALSE;
    jvmtiError err = g_jvmti->IsModifiableClass(targetClass, &modifiable);
    if (err != JVMTI_ERROR_NONE || modifiable != JNI_TRUE) {
        DebugLog("Runtime class capture unavailable class=%s err=%d modifiable=%d",
            label ? label : "target", (int)err, modifiable == JNI_TRUE ? 1 : 0);
        return false;
    }

    g_runtimeCapturedClassBytes.clear();
    g_runtimeClassCaptureTarget = targetClass;
    InterlockedExchange(&g_runtimeClassCaptureEnabled, 1);
    err = g_jvmti->RetransformClasses(1, &targetClass);
    InterlockedExchange(&g_runtimeClassCaptureEnabled, 0);
    g_runtimeClassCaptureTarget = nullptr;

    if (err != JVMTI_ERROR_NONE || g_runtimeCapturedClassBytes.empty()) {
        DebugLog("Runtime class capture failed class=%s err=%d bytes=%u",
            label ? label : "target", (int)err, (unsigned int)g_runtimeCapturedClassBytes.size());
        g_runtimeCapturedClassBytes.clear();
        return false;
    }

    outBytes.swap(g_runtimeCapturedClassBytes);
    DebugLog("Runtime class capture succeeded class=%s bytes=%u",
        label ? label : "target", (unsigned int)outBytes.size());
    return true;
}

bool RedefineJavaClass(JNIEnv* env, jclass targetClass, const std::vector<unsigned char>& classBytes, const char* label) {
    if (!env || !targetClass || classBytes.empty()) return false;
    if (!InitJvmtiRedefineSupport()) return false;

    jboolean modifiable = JNI_FALSE;
    jvmtiError err = g_jvmti->IsModifiableClass(targetClass, &modifiable);
    if (err == JVMTI_ERROR_NONE && modifiable != JNI_TRUE) {
        DebugLog("%s class is not modifiable", label ? label : "target");
        return false;
    }
    if (err != JVMTI_ERROR_NONE) {
        DebugLog("IsModifiableClass failed class=%s err=%d", label ? label : "target", (int)err);
        return false;
    }

    jvmtiClassDefinition definition = {};
    definition.klass = targetClass;
    definition.class_byte_count = (jint)classBytes.size();
    definition.class_bytes = classBytes.data();

    err = g_jvmti->RedefineClasses(1, &definition);
    if (err != JVMTI_ERROR_NONE) {
        DebugLog("RedefineClasses failed class=%s err=%d bytes=%u",
            label ? label : "target",
            (int)err,
            (unsigned int)classBytes.size());
        return false;
    }

    DebugLog("RedefineClasses succeeded class=%s bytes=%u",
        label ? label : "target",
        (unsigned int)classBytes.size());
    return true;
}

bool RedefineMutedVoiceS02PacketChat(JNIEnv* env, const std::vector<unsigned char>& classBytes, const char* label) {
    if (!env || !g_mutedVoiceS02PacketClass || classBytes.empty()) return false;
    return RedefineJavaClass(env, g_mutedVoiceS02PacketClass, classBytes, label ? label : "S02PacketChat");
}

bool RedefineBarrierClass(JNIEnv* env, const std::vector<unsigned char>& classBytes) {
    jclass barrierClass = nullptr;
    if (!GetSeeBarriersBarrierClass(env, barrierClass)) return false;
    return RedefineJavaClass(env, barrierClass, classBytes, "BlockBarrier");
}

jclass g_nameTagRenderPlayerClass = nullptr;
jclass g_nameTagHelperClass = nullptr;
jclass g_publicWinsScoreFormatHelperClass = nullptr;
jclass g_publicWinsRenderedNameHelperClass = nullptr;
jclass g_publicWinsRenderedComponentHelperClass = nullptr;
jclass g_lunarAdventureNameHelperClass = nullptr;
jclass g_publicWinsTabOverlayClass = nullptr;
jclass g_publicWinsApiTabRendererClass = nullptr;
jclass g_publicWinsScorePlayerTeamClass = nullptr;
jclass g_publicWinsLivingRendererClass = nullptr;
jclass g_lunarNametagEntityPlayerClass = nullptr;
jclass g_lunarAdventureComponentClass = nullptr;
jclass g_lunarAdventureTextColorClass = nullptr;
std::vector<unsigned char> g_nameTagOriginalRenderPlayerBytes;
std::vector<unsigned char> g_nameTagPatchedRenderPlayerBytes;
std::vector<unsigned char> g_publicWinsOriginalTabOverlayBytes;
std::vector<unsigned char> g_publicWinsPatchedTabOverlayBytes;
std::vector<unsigned char> g_publicWinsOriginalApiTabRendererBytes;
std::vector<unsigned char> g_publicWinsPatchedApiTabRendererBytes;
std::vector<unsigned char> g_publicWinsOriginalScorePlayerTeamBytes;
std::vector<unsigned char> g_publicWinsPatchedScorePlayerTeamBytes;
std::vector<unsigned char> g_publicWinsOriginalLivingRendererBytes;
std::vector<unsigned char> g_publicWinsPatchedLivingRendererBytes;
bool g_nameTagHookPatched = false;
bool g_nameTagHookFailed = false;
bool g_publicWinsTabHookPatched = false;
bool g_publicWinsTabHookFailed = false;
bool g_publicWinsApiTabHookPatched = false;
bool g_publicWinsApiTabHookFailed = false;
bool g_publicWinsScoreboardFormatHookPatched = false;
bool g_publicWinsScoreboardFormatHookFailed = false;
bool g_publicWinsRenderedNameHookPatched = false;
bool g_publicWinsRenderedNameHookFailed = false;
std::mutex g_nameTagJniMutex;
jmethodID g_nameTagPlayerGetProfile = nullptr;
jmethodID g_publicWinsNetworkInfoGetProfile = nullptr;
jmethodID g_publicWinsNetworkInfoGetPlayerName = nullptr;
jmethodID g_publicWinsNetworkInfoGetDisplayName = nullptr;
jmethodID g_publicWinsNetworkInfoGetTeam = nullptr;
jmethodID g_publicWinsComponentGetFormattedText = nullptr;
jmethodID g_publicWinsTeamFormatPlayerName = nullptr;
jmethodID g_publicWinsBaseTeamFormatName = nullptr;
jmethodID g_publicWinsEntityGetName = nullptr;
jmethodID g_publicWinsEntityGetDisplayName = nullptr;
jmethodID g_publicWinsChatComponentGetFormattedText = nullptr;
jmethodID g_lunarAdventureGetDisplayNameComponent = nullptr;
jmethodID g_lunarAdventureTextContentGet = nullptr;
jmethodID g_lunarAdventureTextContentSet = nullptr;
jmethodID g_lunarAdventureComponentTextColored = nullptr;
jmethodID g_lunarAdventureTextColorFromRgb = nullptr;
jmethodID g_lunarAdventureComponentAppend = nullptr;
jmethodID g_nameTagProfileGetName = nullptr;
jmethodID g_nameTagProfileGetId = nullptr;
jmethodID g_nameTagUuidToString = nullptr;

struct PlayerIdentity {
    std::string name;
    std::string uuid;
};

std::string MakePublicWinsLabel(unsigned long long wins) {
    std::string digits = std::to_string(wins);
    if (wins >= 10000ULL && digits.size() >= 3) {
        std::string label = "\xC2\xA7" "c[";
        label += digits.substr(0, 1);
        label += "\xC2\xA7" "6";
        label += digits.substr(1, digits.size() - 2);
        label += "\xC2\xA7" "a";
        label += digits.substr(digits.size() - 1);
        label += "]";
        return label;
    }

    char colour = '8';
    if (wins >= 5000ULL) colour = '0';
    else if (wins >= 2500ULL) colour = 'c';
    else if (wins >= 1500ULL) colour = '6';
    else if (wins >= 1000ULL) colour = '5';
    else if (wins >= 500ULL) colour = '9';
    else if (wins >= 250ULL) colour = 'a';
    else if (wins >= 100ULL) colour = '2';
    else if (wins >= 50ULL) colour = 'f';
    else if (wins >= 15ULL) colour = '7';

    std::string label = "\xC2\xA7";
    label.push_back(colour);
    label += "[" + digits + "]";
    return label;
}

struct MinecraftFormatState {
    char colour = '\0';
    bool styles[5] = {};
};

MinecraftFormatState GetMinecraftFormatStateBefore(const std::string& value, size_t end) {
    MinecraftFormatState state;
    for (size_t i = 0; i < value.size() && i < end; ++i) {
        size_t codeLength = 0;
        if (!IsMinecraftFormatCodeAt(value, i, codeLength)) continue;
        size_t codeIndex = i + codeLength - 1;
        if (codeIndex >= value.size()) break;
        char code = value[codeIndex];
        if (code >= 'A' && code <= 'Z') code = (char)(code - 'A' + 'a');
        if ((code >= '0' && code <= '9') || (code >= 'a' && code <= 'f')) {
            state.colour = code;
            for (bool& style : state.styles) style = false;
        }
        else if (code == 'r') {
            state = MinecraftFormatState{};
        }
        else if (code >= 'k' && code <= 'o') {
            state.styles[code - 'k'] = true;
        }
        i += codeLength - 1;
    }
    return state;
}

std::string RestoreMinecraftFormatState(const MinecraftFormatState& state) {
    std::string result = "\xC2\xA7" "r";
    if (state.colour != '\0') {
        result += "\xC2\xA7";
        result.push_back(state.colour);
    }
    for (int i = 0; i < 5; ++i) {
        if (!state.styles[i]) continue;
        result += "\xC2\xA7";
        result.push_back((char)('k' + i));
    }
    return result;
}

bool FindFormattedUsernameRange(const std::string& formatted, const std::string& username, size_t& start, size_t& end) {
    start = end = std::string::npos;
    if (!IsSafeMinecraftUsername(username)) return false;

    std::string visible;
    std::vector<size_t> sourceOffsets;
    for (size_t i = 0; i < formatted.size();) {
        size_t codeLength = 0;
        if (IsMinecraftFormatCodeAt(formatted, i, codeLength)) {
            i += codeLength;
            continue;
        }
        char ch = formatted[i];
        visible.push_back((ch >= 'A' && ch <= 'Z') ? (char)(ch - 'A' + 'a') : ch);
        sourceOffsets.push_back(i);
        ++i;
    }

    std::string needle = ToLowerAscii(username);
    size_t found = 0;
    while ((found = visible.find(needle, found)) != std::string::npos) {
        auto isNameChar = [](char ch) {
            return (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_';
        };
        bool leftBoundary = found == 0 || !isNameChar(visible[found - 1]);
        bool rightBoundary = found + needle.size() >= visible.size() || !isNameChar(visible[found + needle.size()]);
        if (leftBoundary && rightBoundary) {
            start = sourceOffsets[found];
            size_t lastVisible = found + needle.size() - 1;
            end = sourceOffsets[lastVisible] + 1;
            return true;
        }
        ++found;
    }
    return false;
}

std::string DecorateNameWithPublicWins(
    const std::string& formatted,
    const std::string& username,
    unsigned long long wins) {
    size_t start = 0;
    size_t end = 0;
    if (!FindFormattedUsernameRange(formatted, username, start, end)) return formatted;
    std::string label = MakePublicWinsLabel(wins);
    // The scoreboard-team fallback may already have composed this label into
    // the name before either render hook sees it. Keep both paths safe to use
    // together without rendering the same wins value twice.
    if (formatted.find(label) != std::string::npos) return formatted;
    const bool suffix = g_guiPublicWinsPosition == PUBLIC_WINS_POSITION_SUFFIX;
    size_t insertAt = suffix ? end : start;
    MinecraftFormatState state = GetMinecraftFormatStateBefore(formatted, insertAt);
    const std::string separator = g_guiPublicWinsSpaceBetweenUsername ? " " : "";
    std::string insertion = suffix
        ? separator + label + RestoreMinecraftFormatState(state)
        : label + RestoreMinecraftFormatState(state) + separator;
    std::string result = formatted;
    result.insert(insertAt, insertion);
    return result;
}

std::string DecorateApiTabTextWithPublicWins(const std::string& formatted, bool& matchedPlayer) {
    matchedPlayer = false;
    if (InterlockedCompareExchange(&g_publicWinsRuntimeEnabled, 0, 0) == 0 ||
        InterlockedCompareExchange(&g_hypixelTntTagGameActive, 0, 0) == 0) {
        return formatted;
    }
    std::string bestName;
    unsigned long long bestWins = 0;
    unsigned long long bestFetchedEpochMs = 0;

    {
        std::lock_guard<std::mutex> lock(g_publicWinsMutex);
        for (const auto& item : g_publicWinsCache) {
            const PublicWinsCacheEntry& entry = item.second;
            if (!entry.hasDefinitiveResult || !entry.available ||
                !IsSafeMinecraftUsername(entry.apiDisplayName)) {
                continue;
            }

            size_t nameStart = 0;
            size_t nameEnd = 0;
            if (!FindFormattedUsernameRange(formatted, entry.apiDisplayName, nameStart, nameEnd) ||
                entry.fetchedEpochMs < bestFetchedEpochMs) {
                continue;
            }

            bestName = entry.apiDisplayName;
            bestWins = entry.wins;
            bestFetchedEpochMs = entry.fetchedEpochMs;
        }
    }

    if (bestName.empty()) return formatted;
    matchedPlayer = true;
    return DecorateNameWithPublicWins(formatted, bestName, bestWins);
}

std::string GetScoreboardTeamName(jobject team) {
    if (!g_env || !team) return "";
    jstring value = (jstring)g_env->GetObjectField(team, g_scoreboardJNI.fTeamName);
    if (g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (value) g_env->DeleteLocalRef(value);
        return "";
    }
    std::string result = JStringToUtf8(value);
    if (value) g_env->DeleteLocalRef(value);
    return result;
}

std::string GetScoreboardTeamString(jobject team, jfieldID field) {
    if (!g_env || !team || !field) return "";
    jstring value = (jstring)g_env->GetObjectField(team, field);
    if (g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (value) g_env->DeleteLocalRef(value);
        return "";
    }
    std::string result = JStringToUtf8(value);
    if (value) g_env->DeleteLocalRef(value);
    return result;
}

jobject GetScoreboardTeamByName(jobject scoreboard, const std::string& teamName) {
    if (!g_env || !scoreboard || teamName.empty()) return nullptr;
    jstring name = g_env->NewStringUTF(teamName.c_str());
    if (!name || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (name) g_env->DeleteLocalRef(name);
        return nullptr;
    }
    jobject team = g_env->CallObjectMethod(scoreboard, g_scoreboardJNI.mScoreboardGetTeam, name);
    g_env->DeleteLocalRef(name);
    if (g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (team) g_env->DeleteLocalRef(team);
        return nullptr;
    }
    return team;
}

jobject GetPlayerScoreboardTeam(jobject scoreboard, const std::string& playerName) {
    if (!g_env || !scoreboard || playerName.empty()) return nullptr;
    jstring name = g_env->NewStringUTF(playerName.c_str());
    if (!name || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (name) g_env->DeleteLocalRef(name);
        return nullptr;
    }
    jobject team = g_env->CallObjectMethod(scoreboard, g_scoreboardJNI.mScoreboardGetPlayersTeam, name);
    g_env->DeleteLocalRef(name);
    if (g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (team) g_env->DeleteLocalRef(team);
        return nullptr;
    }
    return team;
}

jobject FindOrCreateScoreboardTeam(jobject scoreboard, const std::string& teamName) {
    jobject team = GetScoreboardTeamByName(scoreboard, teamName);
    if (team || !g_env || !scoreboard || teamName.empty()) return team;

    jstring name = g_env->NewStringUTF(teamName.c_str());
    if (!name || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (name) g_env->DeleteLocalRef(name);
        return nullptr;
    }
    team = g_env->CallObjectMethod(scoreboard, g_scoreboardJNI.mScoreboardCreateTeam, name);
    g_env->DeleteLocalRef(name);
    if (g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (team) g_env->DeleteLocalRef(team);
        return nullptr;
    }
    return team;
}

void CopyScoreboardTeamPresentation(jobject source, jobject destination) {
    if (!g_env || !source || !destination) return;

    auto copyObject = [&](jfieldID field) {
        if (!field) return;
        jobject value = g_env->GetObjectField(source, field);
        if (g_env->ExceptionCheck()) {
            g_env->ExceptionClear();
            if (value) g_env->DeleteLocalRef(value);
            return;
        }
        g_env->SetObjectField(destination, field, value);
        if (value) g_env->DeleteLocalRef(value);
        if (g_env->ExceptionCheck()) g_env->ExceptionClear();
    };
    auto copyBoolean = [&](jfieldID field) {
        if (!field) return;
        jboolean value = g_env->GetBooleanField(source, field);
        if (g_env->ExceptionCheck()) {
            g_env->ExceptionClear();
            return;
        }
        g_env->SetBooleanField(destination, field, value);
        if (g_env->ExceptionCheck()) g_env->ExceptionClear();
    };

    copyObject(g_scoreboardJNI.fTeamDisplayName);
    copyBoolean(g_scoreboardJNI.fTeamFriendlyFire);
    copyBoolean(g_scoreboardJNI.fTeamSeeFriendlyInvisibles);
    copyObject(g_scoreboardJNI.fTeamNameTagVisibility);
    copyObject(g_scoreboardJNI.fTeamDeathMessageVisibility);
    copyObject(g_scoreboardJNI.fTeamColor);
}

std::string MakePublicWinsLocalTeamName(const std::string& uuidValue) {
    std::string uuid = NormalizePublicWinsUuid(uuidValue);
    if (uuid.size() < 13) return "";
    return "phw" + uuid.substr(0, 13);
}

std::string GetTeamSuffixWithoutTimer(const std::string& teamName, const std::string& currentSuffix) {
    auto timerIt = g_teamSuffixCache.find(teamName);
    if (timerIt != g_teamSuffixCache.end() && !timerIt->second.appliedSuffix.empty() &&
        EndsWith(currentSuffix, timerIt->second.appliedSuffix)) {
        return timerIt->second.baseSuffix;
    }
    return currentSuffix;
}

bool AddPlayerToScoreboardTeam(
    jobject scoreboard,
    const std::string& playerName,
    const std::string& teamName) {
    if (!g_env || !scoreboard || playerName.empty() || teamName.empty()) return false;
    jstring player = g_env->NewStringUTF(playerName.c_str());
    jstring team = g_env->NewStringUTF(teamName.c_str());
    if (!player || !team || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (player) g_env->DeleteLocalRef(player);
        if (team) g_env->DeleteLocalRef(team);
        return false;
    }
    jboolean added = g_env->CallBooleanMethod(
        scoreboard, g_scoreboardJNI.mScoreboardAddPlayerToTeam, player, team);
    g_env->DeleteLocalRef(player);
    g_env->DeleteLocalRef(team);
    if (g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        return false;
    }
    return added == JNI_TRUE;
}

void RemovePlayerFromScoreboardTeams(jobject scoreboard, const std::string& playerName) {
    if (!g_env || !scoreboard || playerName.empty()) return;
    jstring player = g_env->NewStringUTF(playerName.c_str());
    if (!player || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (player) g_env->DeleteLocalRef(player);
        return;
    }
    g_env->CallBooleanMethod(scoreboard, g_scoreboardJNI.mScoreboardRemovePlayerFromTeams, player);
    g_env->DeleteLocalRef(player);
    if (g_env->ExceptionCheck()) g_env->ExceptionClear();
}

void RemoveScoreboardTeamByName(jobject scoreboard, const std::string& teamName) {
    jobject team = GetScoreboardTeamByName(scoreboard, teamName);
    if (!team) return;
    g_env->CallVoidMethod(scoreboard, g_scoreboardJNI.mScoreboardRemoveTeam, team);
    if (g_env->ExceptionCheck()) g_env->ExceptionClear();
    g_env->DeleteLocalRef(team);
}

void ReleasePublicWinsLocalTeam(
    jobject scoreboard,
    const PublicWinsTeamFormatState& state,
    bool restoreSourceTeam) {
    if (!scoreboard) return;

    bool restored = false;
    if (restoreSourceTeam && state.sourceHadTeam && !state.sourceTeamName.empty() &&
        state.sourceTeamName != state.localTeamName) {
        jobject sourceTeam = GetScoreboardTeamByName(scoreboard, state.sourceTeamName);
        if (sourceTeam) {
            g_env->DeleteLocalRef(sourceTeam);
            restored = AddPlayerToScoreboardTeam(scoreboard, state.playerName, state.sourceTeamName);
        }
    }
    if (!restored) {
        jobject currentTeam = GetPlayerScoreboardTeam(scoreboard, state.playerName);
        std::string currentTeamName = GetScoreboardTeamName(currentTeam);
        if (currentTeam) g_env->DeleteLocalRef(currentTeam);
        if (currentTeamName == state.localTeamName) {
            RemovePlayerFromScoreboardTeams(scoreboard, state.playerName);
        }
    }

    RemoveScoreboardTeamByName(scoreboard, state.localTeamName);
    g_teamSuffixCache.erase(state.localTeamName);
}

void ApplyPublicWinsToPlayerTeams(bool enablePublicWins) {
    if (!g_env || !InitScoreboardJNI()) {
        if (!enablePublicWins) g_publicWinsTeamFormatCache.clear();
        return;
    }

    jobject world = GetWorldObject();
    if (!world) {
        if (!enablePublicWins) g_publicWinsTeamFormatCache.clear();
        return;
    }
    jobject scoreboard = GetScoreboardObjectFromWorld(world);
    if (!scoreboard) {
        g_env->DeleteLocalRef(world);
        if (!enablePublicWins) g_publicWinsTeamFormatCache.clear();
        return;
    }

    int changedCount = 0;
    int labelledCount = 0;
    if (!enablePublicWins) {
        for (const auto& item : g_publicWinsTeamFormatCache) {
            ReleasePublicWinsLocalTeam(scoreboard, item.second, true);
            ++changedCount;
        }
        g_publicWinsTeamFormatCache.clear();
        g_env->DeleteLocalRef(scoreboard);
        g_env->DeleteLocalRef(world);
        if (changedCount > 0) {
            DebugLog("Public wins local teams restored count=%d", changedCount);
        }
        return;
    }

    jobjectArray playerArray = GetNetworkPlayerInfoArray();
    if (!playerArray || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (playerArray) g_env->DeleteLocalRef(playerArray);
        g_env->DeleteLocalRef(scoreboard);
        g_env->DeleteLocalRef(world);
        return;
    }

    std::unordered_set<std::string> seenLocalTeams;
    jsize playerCount = g_env->GetArrayLength(playerArray);
    for (jsize i = 0; i < playerCount; ++i) {
        jobject networkInfo = g_env->GetObjectArrayElement(playerArray, i);
        if (!networkInfo) continue;

        std::string playerName;
        std::string playerUuid;
        bool hasIdentity = GetNetworkPlayerInfoNameAndUuid(networkInfo, playerName, playerUuid);
        g_env->DeleteLocalRef(networkInfo);
        if (!hasIdentity || !IsLikelyPlayerUsername(playerName) || !IsUuidLookupId(playerUuid)) continue;

        std::string localTeamName = MakePublicWinsLocalTeamName(playerUuid);
        if (localTeamName.empty()) continue;
        seenLocalTeams.insert(localTeamName);

        unsigned long long wins = 0;
        bool hasWins = GetPublicWinsCached(playerUuid, playerName, wins);
        auto stateIt = g_publicWinsTeamFormatCache.find(localTeamName);
        if (!hasWins) {
            if (stateIt != g_publicWinsTeamFormatCache.end()) {
                ReleasePublicWinsLocalTeam(scoreboard, stateIt->second, true);
                g_publicWinsTeamFormatCache.erase(stateIt);
                ++changedCount;
            }
            continue;
        }

        jobject currentTeam = GetPlayerScoreboardTeam(scoreboard, playerName);
        std::string currentTeamName = GetScoreboardTeamName(currentTeam);
        bool ownsCurrentTeam = stateIt != g_publicWinsTeamFormatCache.end() &&
            currentTeamName == stateIt->second.localTeamName;

        if (!ownsCurrentTeam) {
            if (stateIt != g_publicWinsTeamFormatCache.end()) {
                ReleasePublicWinsLocalTeam(scoreboard, stateIt->second, false);
                g_publicWinsTeamFormatCache.erase(stateIt);
            }

            PublicWinsTeamFormatState state;
            state.playerName = playerName;
            state.playerUuid = NormalizePublicWinsUuid(playerUuid);
            state.sourceTeamName = currentTeamName;
            state.localTeamName = localTeamName;
            state.sourceHadTeam = currentTeam != nullptr && currentTeamName != localTeamName;
            if (state.sourceHadTeam) {
                state.basePrefix = GetScoreboardTeamString(currentTeam, g_scoreboardJNI.fTeamE);
                std::string sourceSuffix = GetScoreboardTeamString(currentTeam, g_scoreboardJNI.fTeamF);
                state.baseSuffix = GetTeamSuffixWithoutTimer(currentTeamName, sourceSuffix);
            }

            jobject localTeam = FindOrCreateScoreboardTeam(scoreboard, localTeamName);
            if (localTeam) {
                if (state.sourceHadTeam) CopyScoreboardTeamPresentation(currentTeam, localTeam);
                SetTeamPrefixAndSuffix(localTeam, state.basePrefix, state.baseSuffix);
                if (AddPlayerToScoreboardTeam(scoreboard, playerName, localTeamName)) {
                    state.appliedPrefix = state.basePrefix;
                    state.appliedSuffix = state.baseSuffix;
                    stateIt = g_publicWinsTeamFormatCache.emplace(localTeamName, std::move(state)).first;
                    ++changedCount;
                }
                else {
                    RemoveScoreboardTeamByName(scoreboard, localTeamName);
                }
                g_env->DeleteLocalRef(localTeam);
            }
        }
        if (currentTeam) g_env->DeleteLocalRef(currentTeam);
        if (stateIt == g_publicWinsTeamFormatCache.end()) continue;

        jobject localTeam = GetScoreboardTeamByName(scoreboard, localTeamName);
        if (!localTeam) continue;

        std::string currentPrefix = GetScoreboardTeamString(localTeam, g_scoreboardJNI.fTeamE);
        std::string currentSuffix = GetScoreboardTeamString(localTeam, g_scoreboardJNI.fTeamF);
        std::string compositionPrefix = stateIt->second.basePrefix;
        if (!stateIt->second.appliedPrefix.empty() && currentPrefix != stateIt->second.appliedPrefix) {
            compositionPrefix = currentPrefix;
        }

        std::string desiredPrefix = compositionPrefix;
        std::string desiredBaseSuffix = stateIt->second.baseSuffix;
        std::string label = MakePublicWinsLabel(wins);
        if (g_guiPublicWinsPosition == PUBLIC_WINS_POSITION_SUFFIX) {
            desiredBaseSuffix = (g_guiPublicWinsSpaceBetweenUsername ? " " : "") + label + desiredBaseSuffix;
        }
        else {
            MinecraftFormatState formatState = GetMinecraftFormatStateBefore(
                compositionPrefix, compositionPrefix.size());
            desiredPrefix = compositionPrefix + label + RestoreMinecraftFormatState(formatState) +
                (g_guiPublicWinsSpaceBetweenUsername ? " " : "");
        }

        bool timerOnNametag = g_guiTimerNametagEnabled && g_timerActive && !g_betweenRoundsTimerActive;
        std::string desiredSuffix = desiredBaseSuffix;
        if (timerOnNametag) {
            std::string timerSuffix = MakeTimerSuffix(GetDecimalSeconds());
            desiredSuffix += timerSuffix;
            TeamSuffixState& timerState = g_teamSuffixCache[localTeamName];
            timerState.baseSuffix = desiredBaseSuffix;
            timerState.appliedSuffix = timerSuffix;
        }
        else {
            g_teamSuffixCache.erase(localTeamName);
        }

        if ((currentPrefix != desiredPrefix || currentSuffix != desiredSuffix) &&
            SetTeamPrefixAndSuffix(localTeam, desiredPrefix, desiredSuffix)) {
            ++changedCount;
        }
        stateIt->second.appliedPrefix = desiredPrefix;
        stateIt->second.appliedSuffix = desiredSuffix;
        ++labelledCount;

        g_env->DeleteLocalRef(localTeam);
    }

    g_env->DeleteLocalRef(playerArray);

    for (auto it = g_publicWinsTeamFormatCache.begin(); it != g_publicWinsTeamFormatCache.end();) {
        if (seenLocalTeams.find(it->first) != seenLocalTeams.end()) {
            ++it;
            continue;
        }
        ReleasePublicWinsLocalTeam(scoreboard, it->second, false);
        it = g_publicWinsTeamFormatCache.erase(it);
        ++changedCount;
    }

    g_env->DeleteLocalRef(scoreboard);
    g_env->DeleteLocalRef(world);
    if (changedCount > 0) {
        DebugLog("Public wins team formatting updated changed=%d labelled=%d players=%d position=%s",
            changedCount,
            labelledCount,
            (int)playerCount,
            g_guiPublicWinsPosition == PUBLIC_WINS_POSITION_SUFFIX ? "suffix" : "prefix");
    }
}

PlayerIdentity GetPlayerIdentityForHook(JNIEnv* env, jobject player) {
    PlayerIdentity identity;
    if (!env || !player) return identity;

    std::lock_guard<std::mutex> lock(g_nameTagJniMutex);
    if (!g_nameTagPlayerGetProfile) {
        jclass playerClass = env->GetObjectClass(player);
        if (!playerClass || env->ExceptionCheck()) {
            env->ExceptionClear();
            if (playerClass) env->DeleteLocalRef(playerClass);
            return identity;
        }

        g_nameTagPlayerGetProfile = GetMethodIDCompat(env, playerClass, "cd", "()Lcom/mojang/authlib/GameProfile;");
        env->DeleteLocalRef(playerClass);
        if (!g_nameTagPlayerGetProfile || env->ExceptionCheck()) {
            env->ExceptionClear();
            g_nameTagPlayerGetProfile = nullptr;
            return identity;
        }
    }

    jobject profile = env->CallObjectMethod(player, g_nameTagPlayerGetProfile);
    if (!profile || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (profile) env->DeleteLocalRef(profile);
        return identity;
    }

    if (!g_nameTagProfileGetName) {
        jclass profileClass = env->GetObjectClass(profile);
        if (!profileClass || env->ExceptionCheck()) {
            env->ExceptionClear();
            if (profileClass) env->DeleteLocalRef(profileClass);
            env->DeleteLocalRef(profile);
            return identity;
        }

        g_nameTagProfileGetName = GetMethodIDCompat(env, profileClass, "getName", "()Ljava/lang/String;");
        if (!g_nameTagProfileGetName || env->ExceptionCheck()) {
            env->ExceptionClear();
            g_nameTagProfileGetName = nullptr;
            env->DeleteLocalRef(profileClass);
            env->DeleteLocalRef(profile);
            return identity;
        }

        g_nameTagProfileGetId = GetMethodIDCompat(env, profileClass, "getId", "()Ljava/util/UUID;");
        env->DeleteLocalRef(profileClass);
        if (!g_nameTagProfileGetId || env->ExceptionCheck()) {
            env->ExceptionClear();
            g_nameTagProfileGetId = nullptr;
        }
    }

    jstring nameString = (jstring)env->CallObjectMethod(profile, g_nameTagProfileGetName);
    if (!nameString || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (nameString) env->DeleteLocalRef(nameString);
        env->DeleteLocalRef(profile);
        return identity;
    }

    identity.name = JStringToUtf8(env, nameString);
    env->DeleteLocalRef(nameString);

    if (g_nameTagProfileGetId) {
        jobject uuidObj = env->CallObjectMethod(profile, g_nameTagProfileGetId);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            uuidObj = nullptr;
        }

        if (uuidObj) {
            if (!g_nameTagUuidToString) {
                jclass uuidClass = env->GetObjectClass(uuidObj);
                if (!uuidClass || env->ExceptionCheck()) {
                    env->ExceptionClear();
                    if (uuidClass) env->DeleteLocalRef(uuidClass);
                }
                else {
                    g_nameTagUuidToString = GetMethodIDCompat(env, uuidClass, "toString", "()Ljava/lang/String;");
                    env->DeleteLocalRef(uuidClass);
                    if (!g_nameTagUuidToString || env->ExceptionCheck()) {
                        env->ExceptionClear();
                        g_nameTagUuidToString = nullptr;
                    }
                }
            }

            if (g_nameTagUuidToString) {
                jstring uuidString = (jstring)env->CallObjectMethod(uuidObj, g_nameTagUuidToString);
                if (env->ExceptionCheck()) {
                    env->ExceptionClear();
                    if (uuidString) env->DeleteLocalRef(uuidString);
                }
                else {
                    identity.uuid = JStringToUtf8(env, uuidString);
                    if (uuidString) env->DeleteLocalRef(uuidString);
                }
            }

            env->DeleteLocalRef(uuidObj);
        }
    }

    env->DeleteLocalRef(profile);
    return identity;
}

PlayerIdentity GetNetworkInfoIdentityForHook(JNIEnv* env, jobject networkInfo) {
    PlayerIdentity identity;
    if (!env || !networkInfo) return identity;

    std::lock_guard<std::mutex> lock(g_nameTagJniMutex);
    if (!g_publicWinsNetworkInfoGetProfile) {
        jclass infoClass = env->GetObjectClass(networkInfo);
        if (!infoClass || env->ExceptionCheck()) {
            env->ExceptionClear();
            if (infoClass) env->DeleteLocalRef(infoClass);
            return identity;
        }
        g_publicWinsNetworkInfoGetProfile = GetMethodIDCompat(env, infoClass, "a", "()Lcom/mojang/authlib/GameProfile;");
        env->DeleteLocalRef(infoClass);
        if (!g_publicWinsNetworkInfoGetProfile || env->ExceptionCheck()) {
            env->ExceptionClear();
            g_publicWinsNetworkInfoGetProfile = nullptr;
            return identity;
        }
    }

    jobject profile = env->CallObjectMethod(networkInfo, g_publicWinsNetworkInfoGetProfile);
    if (!profile || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (profile) env->DeleteLocalRef(profile);
        return identity;
    }

    jclass profileClass = nullptr;
    if (!g_nameTagProfileGetName || !g_nameTagProfileGetId) {
        profileClass = env->GetObjectClass(profile);
        if (!profileClass || env->ExceptionCheck()) {
            env->ExceptionClear();
            if (profileClass) env->DeleteLocalRef(profileClass);
            env->DeleteLocalRef(profile);
            return identity;
        }
        if (!g_nameTagProfileGetName) g_nameTagProfileGetName = GetMethodIDCompat(env, profileClass, "getName", "()Ljava/lang/String;");
        if (!g_nameTagProfileGetId) g_nameTagProfileGetId = GetMethodIDCompat(env, profileClass, "getId", "()Ljava/util/UUID;");
        env->DeleteLocalRef(profileClass);
        if (!g_nameTagProfileGetName || env->ExceptionCheck()) {
            env->ExceptionClear();
            env->DeleteLocalRef(profile);
            return identity;
        }
    }

    jstring nameString = (jstring)env->CallObjectMethod(profile, g_nameTagProfileGetName);
    if (!nameString || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (nameString) env->DeleteLocalRef(nameString);
        env->DeleteLocalRef(profile);
        return identity;
    }
    identity.name = JStringToUtf8(env, nameString);
    env->DeleteLocalRef(nameString);

    if (g_nameTagProfileGetId) {
        jobject uuidObj = env->CallObjectMethod(profile, g_nameTagProfileGetId);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            uuidObj = nullptr;
        }
        if (uuidObj) {
            if (!g_nameTagUuidToString) {
                jclass uuidClass = env->GetObjectClass(uuidObj);
                if (uuidClass && !env->ExceptionCheck()) {
                    g_nameTagUuidToString = GetMethodIDCompat(env, uuidClass, "toString", "()Ljava/lang/String;");
                    env->DeleteLocalRef(uuidClass);
                }
                if (env->ExceptionCheck()) env->ExceptionClear();
            }
            if (g_nameTagUuidToString) {
                jstring uuidString = (jstring)env->CallObjectMethod(uuidObj, g_nameTagUuidToString);
                if (!env->ExceptionCheck()) identity.uuid = JStringToUtf8(env, uuidString);
                else env->ExceptionClear();
                if (uuidString) env->DeleteLocalRef(uuidString);
            }
            env->DeleteLocalRef(uuidObj);
        }
    }
    env->DeleteLocalRef(profile);
    return identity;
}

std::string GetVanillaTabDisplayName(JNIEnv* env, jobject networkInfo, const std::string& profileName) {
    if (!env || !networkInfo) return profileName;
    std::lock_guard<std::mutex> lock(g_nameTagJniMutex);

    jclass infoClass = env->GetObjectClass(networkInfo);
    if (!infoClass || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (infoClass) env->DeleteLocalRef(infoClass);
        return profileName;
    }
    if (!g_publicWinsNetworkInfoGetDisplayName) {
        g_publicWinsNetworkInfoGetDisplayName = GetMethodIDCompat(env, infoClass, "k", "()Leu;");
        if (env->ExceptionCheck()) env->ExceptionClear();
    }
    if (!g_publicWinsNetworkInfoGetPlayerName) {
        g_publicWinsNetworkInfoGetPlayerName = GetMethodIDCompat(env, infoClass, "getPlayerName", "()Ljava/lang/String;");
        if (env->ExceptionCheck()) env->ExceptionClear();
    }
    if (!g_publicWinsNetworkInfoGetTeam) {
        g_publicWinsNetworkInfoGetTeam = GetMethodIDCompat(env, infoClass, "i", "()Laul;");
        if (env->ExceptionCheck()) env->ExceptionClear();
    }
    env->DeleteLocalRef(infoClass);

    if (g_publicWinsNetworkInfoGetPlayerName) {
        jstring badlionName = (jstring)env->CallObjectMethod(networkInfo, g_publicWinsNetworkInfoGetPlayerName);
        if (!env->ExceptionCheck()) {
            std::string value = JStringToUtf8(env, badlionName);
            if (badlionName) env->DeleteLocalRef(badlionName);
            if (!value.empty()) return value;
        }
        else {
            env->ExceptionClear();
            if (badlionName) env->DeleteLocalRef(badlionName);
        }
    }

    if (g_publicWinsNetworkInfoGetDisplayName) {
        jobject component = env->CallObjectMethod(networkInfo, g_publicWinsNetworkInfoGetDisplayName);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            component = nullptr;
        }
        if (component) {
            if (!g_publicWinsComponentGetFormattedText) {
                jclass componentClass = env->GetObjectClass(component);
                if (componentClass && !env->ExceptionCheck()) {
                    g_publicWinsComponentGetFormattedText = GetMethodIDCompat(env, componentClass, "d", "()Ljava/lang/String;");
                    env->DeleteLocalRef(componentClass);
                }
                if (env->ExceptionCheck()) env->ExceptionClear();
            }
            if (g_publicWinsComponentGetFormattedText) {
                jstring formatted = (jstring)env->CallObjectMethod(component, g_publicWinsComponentGetFormattedText);
                if (!env->ExceptionCheck()) {
                    std::string value = JStringToUtf8(env, formatted);
                    if (!value.empty()) {
                        if (formatted) env->DeleteLocalRef(formatted);
                        env->DeleteLocalRef(component);
                        return value;
                    }
                }
                else env->ExceptionClear();
                if (formatted) env->DeleteLocalRef(formatted);
            }
            env->DeleteLocalRef(component);
        }
    }

    if (!g_publicWinsNetworkInfoGetTeam) return profileName;
    jobject team = env->CallObjectMethod(networkInfo, g_publicWinsNetworkInfoGetTeam);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        team = nullptr;
    }
    jclass teamClass = FindClassLoose(env, "aul");
    if (!teamClass || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (teamClass) env->DeleteLocalRef(teamClass);
        if (team) env->DeleteLocalRef(team);
        return profileName;
    }
    if (!g_publicWinsTeamFormatPlayerName) {
        g_publicWinsTeamFormatPlayerName = GetStaticMethodIDCompat(env, teamClass, "a", "(Laul;Ljava/lang/String;)Ljava/lang/String;");
        if (env->ExceptionCheck()) env->ExceptionClear();
    }
    std::string result = profileName;
    if (g_publicWinsTeamFormatPlayerName) {
        jstring nameString = env->NewStringUTF(profileName.c_str());
        jstring formatted = (jstring)env->CallStaticObjectMethod(teamClass, g_publicWinsTeamFormatPlayerName, team, nameString);
        if (!env->ExceptionCheck()) result = JStringToUtf8(env, formatted);
        else env->ExceptionClear();
        if (formatted) env->DeleteLocalRef(formatted);
        if (nameString) env->DeleteLocalRef(nameString);
    }
    env->DeleteLocalRef(teamClass);
    if (team) env->DeleteLocalRef(team);
    return result.empty() ? profileName : result;
}

jstring MakePatchedDisplayString(JNIEnv* env, jstring original, const std::string& result) {
    jstring patchedName = env->NewStringUTF(result.c_str());
    if (!patchedName || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (patchedName) env->DeleteLocalRef(patchedName);
        return original;
    }
    return patchedName;
}

jstring JNICALL NameTagHookDispatch(JNIEnv* env, jclass, jobject player, jstring displayName) {
    if (!env || !displayName) return displayName;
    LONG dispatchNumber = InterlockedIncrement(&g_publicWinsNameTagDispatchCount);
    bool publicWinsEnabled = InterlockedCompareExchange(&g_publicWinsRuntimeEnabled, 0, 0) != 0 &&
        InterlockedCompareExchange(&g_hypixelTntTagGameActive, 0, 0) != 0;
    if (!publicWinsEnabled) {
        if (dispatchNumber <= 5) DebugLog("Public wins nametag dispatch=%ld inactive", dispatchNumber);
        return displayName;
    }

    std::string currentName = JStringToUtf8(env, displayName);
    if (currentName.empty()) {
        if (dispatchNumber <= 5) DebugLog("Public wins nametag dispatch=%ld emptyDisplay", dispatchNumber);
        return displayName;
    }
    std::string result = currentName;
    bool publicCacheHit = false;
    PlayerIdentity identity;
    bool hasIdentity = false;

    identity = GetPlayerIdentityForHook(env, player);
    hasIdentity = !identity.name.empty() && IsUuidLookupId(identity.uuid);
    if (!hasIdentity && dispatchNumber <= 5) {
        DebugLog("Public wins nametag dispatch=%ld invalidIdentity namePresent=%d uuidLength=%u",
            dispatchNumber, identity.name.empty() ? 0 : 1, (unsigned int)identity.uuid.size());
    }

    if (publicWinsEnabled && hasIdentity) {
        QueuePublicWinsLookup(identity.uuid, identity.name);
        unsigned long long wins = 0;
        if (GetPublicWinsCached(identity.uuid, identity.name, wins)) {
            publicCacheHit = true;
            result = DecorateNameWithPublicWins(result, identity.name, wins);
        }
    }

    bool changed = result != currentName;
    if (changed) InterlockedIncrement(&g_publicWinsDecorationCount);
    if (dispatchNumber <= 5 || (changed && dispatchNumber <= 20)) {
        DebugLog("Public wins nametag dispatch=%ld player=%s cacheHit=%d changed=%d displayLength=%u",
            dispatchNumber, identity.name.c_str(), publicCacheHit ? 1 : 0, changed ? 1 : 0,
            (unsigned int)currentName.size());
    }
    return changed ? MakePatchedDisplayString(env, displayName, result) : displayName;
}

jstring JNICALL PublicWinsTabNameHookDispatch(JNIEnv* env, jclass, jobject networkInfo) {
    if (!env || !networkInfo) return nullptr;
    LONG dispatchNumber = InterlockedIncrement(&g_publicWinsTabDispatchCount);
    PlayerIdentity identity = GetNetworkInfoIdentityForHook(env, networkInfo);
    if (identity.name.empty()) {
        if (dispatchNumber <= 5) DebugLog("Public wins tab dispatch=%ld emptyIdentity", dispatchNumber);
        return env->NewStringUTF("");
    }
    std::string result = GetVanillaTabDisplayName(env, networkInfo, identity.name);
    std::string originalResult = result;
    bool cacheHit = false;

    if (InterlockedCompareExchange(&g_publicWinsRuntimeEnabled, 0, 0) != 0 &&
        InterlockedCompareExchange(&g_hypixelTntTagGameActive, 0, 0) != 0 && IsUuidLookupId(identity.uuid)) {
        QueuePublicWinsLookup(identity.uuid, identity.name);
        unsigned long long wins = 0;
        if (GetPublicWinsCached(identity.uuid, identity.name, wins)) {
            cacheHit = true;
            result = DecorateNameWithPublicWins(result, identity.name, wins);
        }
    }

    bool changed = result != originalResult;
    if (changed) InterlockedIncrement(&g_publicWinsDecorationCount);
    if (dispatchNumber <= 5 || (changed && dispatchNumber <= 20)) {
        DebugLog("Public wins tab dispatch=%ld player=%s uuidLength=%u cacheHit=%d changed=%d displayLength=%u",
            dispatchNumber, identity.name.c_str(), (unsigned int)identity.uuid.size(),
            cacheHit ? 1 : 0, changed ? 1 : 0, (unsigned int)originalResult.size());
    }

    return MakePatchedDisplayString(env, nullptr, result);
}

jstring JNICALL PublicWinsApiTabTextHookDispatch(JNIEnv* env, jclass, jstring displayText) {
    if (!env || !displayText) return displayText;
    LONG dispatchNumber = InterlockedIncrement(&g_publicWinsApiTabDispatchCount);
    std::string currentText = JStringToUtf8(env, displayText);
    if (currentText.empty() ||
        InterlockedCompareExchange(&g_publicWinsRuntimeEnabled, 0, 0) == 0 ||
        InterlockedCompareExchange(&g_hypixelTntTagGameActive, 0, 0) == 0) {
        if (dispatchNumber <= 5) {
            DebugLog("Public wins API tab dispatch=%ld inactiveOrEmpty=%d",
                dispatchNumber, currentText.empty() ? 1 : 0);
        }
        return displayText;
    }

    bool matchedPlayer = false;
    std::string result = DecorateApiTabTextWithPublicWins(currentText, matchedPlayer);
    bool changed = result != currentText;
    if (changed) InterlockedIncrement(&g_publicWinsDecorationCount);
    if (dispatchNumber <= 10 || (changed && dispatchNumber <= 40)) {
        DebugLog("Public wins API tab dispatch=%ld matched=%d changed=%d textLength=%u",
            dispatchNumber, matchedPlayer ? 1 : 0, changed ? 1 : 0,
            (unsigned int)currentText.size());
    }
    return changed ? MakePatchedDisplayString(env, displayText, result) : displayText;
}

jstring JNICALL PublicWinsScoreFormatHookDispatch(
    JNIEnv* env,
    jclass,
    jobject team,
    jstring playerNameString) {
    if (!env || !playerNameString) return playerNameString;
    LONG dispatchNumber = InterlockedIncrement(&g_publicWinsScoreFormatDispatchCount);

    jmethodID formatMethod = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_nameTagJniMutex);
        if (!g_publicWinsBaseTeamFormatName) {
            jclass baseTeamClass = FindClassLoose(env, "auq");
            if (baseTeamClass && !env->ExceptionCheck()) {
                g_publicWinsBaseTeamFormatName = GetMethodIDCompat(env,
                    baseTeamClass, "d", "(Ljava/lang/String;)Ljava/lang/String;");
                env->DeleteLocalRef(baseTeamClass);
            }
            if (env->ExceptionCheck()) env->ExceptionClear();
        }
        formatMethod = g_publicWinsBaseTeamFormatName;
    }

    jstring formattedName = playerNameString;
    bool ownsFormattedName = false;
    if (team && formatMethod) {
        formattedName = (jstring)env->CallObjectMethod(team, formatMethod, playerNameString);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            formattedName = playerNameString;
        }
        else if (formattedName) {
            ownsFormattedName = true;
        }
        else {
            formattedName = playerNameString;
        }
    }

    std::string playerName = JStringToUtf8(env, playerNameString);
    std::string formatted = JStringToUtf8(env, formattedName);
    unsigned long long wins = 0;
    bool cacheHit = GetPublicWinsCachedByPlayerName(playerName, wins);
    std::string result = cacheHit
        ? DecorateNameWithPublicWins(formatted, playerName, wins)
        : formatted;
    bool changed = result != formatted;
    if (changed) InterlockedIncrement(&g_publicWinsDecorationCount);
    if (dispatchNumber <= 10 || (changed && dispatchNumber <= 40)) {
        DebugLog("Public wins scoreboard dispatch=%ld player=%s cacheHit=%d changed=%d team=%d",
            dispatchNumber,
            playerName.c_str(),
            cacheHit ? 1 : 0,
            changed ? 1 : 0,
            team ? 1 : 0);
    }

    if (!changed) return formattedName;
    jstring patchedName = env->NewStringUTF(result.c_str());
    if (!patchedName || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (patchedName) env->DeleteLocalRef(patchedName);
        return formattedName;
    }
    if (ownsFormattedName) env->DeleteLocalRef(formattedName);
    return patchedName;
}

jstring JNICALL PublicWinsRenderedNameHookDispatch(JNIEnv* env, jclass, jobject player) {
    if (!env || !player) return nullptr;
    LONG dispatchNumber = InterlockedIncrement(&g_publicWinsRenderedNameDispatchCount);

    jmethodID getNameMethod = nullptr;
    jmethodID getDisplayNameMethod = nullptr;
    jmethodID getFormattedTextMethod = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_nameTagJniMutex);
        if (!g_publicWinsEntityGetName || !g_publicWinsEntityGetDisplayName) {
            jclass entityClass = FindClassLoose(env, "pr");
            if (entityClass && !env->ExceptionCheck()) {
                if (!g_publicWinsEntityGetName) {
                    g_publicWinsEntityGetName = GetMethodIDCompat(env,
                        entityClass, "e_", "()Ljava/lang/String;");
                }
                if (!g_publicWinsEntityGetDisplayName) {
                    g_publicWinsEntityGetDisplayName = GetMethodIDCompat(env,
                        entityClass, "f_", "()Leu;");
                }
                env->DeleteLocalRef(entityClass);
            }
            if (env->ExceptionCheck()) env->ExceptionClear();
        }
        if (!g_publicWinsChatComponentGetFormattedText) {
            jclass componentClass = FindClassLoose(env, "eu");
            if (componentClass && !env->ExceptionCheck()) {
                g_publicWinsChatComponentGetFormattedText = GetMethodIDCompat(env,
                    componentClass, "d", "()Ljava/lang/String;");
                env->DeleteLocalRef(componentClass);
            }
            if (env->ExceptionCheck()) env->ExceptionClear();
        }
        getNameMethod = g_publicWinsEntityGetName;
        getDisplayNameMethod = g_publicWinsEntityGetDisplayName;
        getFormattedTextMethod = g_publicWinsChatComponentGetFormattedText;
    }

    if (!getNameMethod || !getDisplayNameMethod || !getFormattedTextMethod) return nullptr;
    jstring playerNameString = (jstring)env->CallObjectMethod(player, getNameMethod);
    if (!playerNameString || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (playerNameString) env->DeleteLocalRef(playerNameString);
        return nullptr;
    }
    std::string playerName = JStringToUtf8(env, playerNameString);

    jobject displayName = env->CallObjectMethod(player, getDisplayNameMethod);
    if (!displayName || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (displayName) env->DeleteLocalRef(displayName);
        return playerNameString;
    }
    jstring formattedName = (jstring)env->CallObjectMethod(displayName, getFormattedTextMethod);
    env->DeleteLocalRef(displayName);
    if (!formattedName || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (formattedName) env->DeleteLocalRef(formattedName);
        return playerNameString;
    }
    env->DeleteLocalRef(playerNameString);

    std::string formatted = JStringToUtf8(env, formattedName);
    unsigned long long wins = 0;
    bool cacheHit = GetPublicWinsCachedByPlayerName(playerName, wins);
    std::string result = cacheHit
        ? DecorateNameWithPublicWins(formatted, playerName, wins)
        : formatted;
    bool changed = result != formatted;
    if (changed) InterlockedIncrement(&g_publicWinsDecorationCount);
    if (dispatchNumber <= 10 || (changed && dispatchNumber <= 40)) {
        DebugLog("Public wins rendered-name dispatch=%ld player=%s cacheHit=%d changed=%d",
            dispatchNumber,
            playerName.c_str(),
            cacheHit ? 1 : 0,
            changed ? 1 : 0);
    }
    if (!changed) return formattedName;

    jstring patchedName = env->NewStringUTF(result.c_str());
    if (!patchedName || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (patchedName) env->DeleteLocalRef(patchedName);
        return formattedName;
    }
    env->DeleteLocalRef(formattedName);
    return patchedName;
}

jstring JNICALL PublicWinsRenderedComponentHookDispatch(JNIEnv* env, jclass, jobject component) {
    if (!env || !component) return nullptr;
    LONG dispatchNumber = InterlockedIncrement(&g_publicWinsRenderedNameDispatchCount);

    jmethodID getFormattedTextMethod = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_nameTagJniMutex);
        if (!g_publicWinsChatComponentGetFormattedText) {
            jclass componentClass = FindClassLoose(env, "eu");
            if (componentClass && !env->ExceptionCheck()) {
                g_publicWinsChatComponentGetFormattedText = GetMethodIDCompat(env,
                    componentClass, "d", "()Ljava/lang/String;");
                env->DeleteLocalRef(componentClass);
            }
            if (env->ExceptionCheck()) env->ExceptionClear();
        }
        getFormattedTextMethod = g_publicWinsChatComponentGetFormattedText;
    }
    if (!getFormattedTextMethod) return nullptr;

    jstring formattedName = (jstring)env->CallObjectMethod(component, getFormattedTextMethod);
    if (!formattedName || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (formattedName) env->DeleteLocalRef(formattedName);
        return nullptr;
    }
    std::string formatted = JStringToUtf8(env, formattedName);
    bool matchedPlayer = false;
    std::string result = DecorateApiTabTextWithPublicWins(formatted, matchedPlayer);
    bool changed = result != formatted;
    if (changed) InterlockedIncrement(&g_publicWinsDecorationCount);
    if (dispatchNumber <= 10 || (changed && dispatchNumber <= 40)) {
        DebugLog("Public wins rendered-component dispatch=%ld matched=%d changed=%d",
            dispatchNumber,
            matchedPlayer ? 1 : 0,
            changed ? 1 : 0);
    }
    if (!changed) return formattedName;

    jstring patchedName = env->NewStringUTF(result.c_str());
    if (!patchedName || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (patchedName) env->DeleteLocalRef(patchedName);
        return formattedName;
    }
    env->DeleteLocalRef(formattedName);
    return patchedName;
}

jstring JNICALL LunarRenderedComponentHookDispatch(
    JNIEnv* env,
    jclass helperClass,
    jobject entity,
    jobject component) {
    jstring formattedName = PublicWinsRenderedComponentHookDispatch(env, helperClass, component);
    if (!env || !entity || !formattedName ||
        !g_guiTimerNametagEnabled || !g_timerActive || g_betweenRoundsTimerActive) {
        return formattedName;
    }

    jclass entityPlayerClass = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_nameTagJniMutex);
        if (!g_lunarNametagEntityPlayerClass) {
            jclass localClass = FindClassLoose(env, "wn");
            if (localClass && !env->ExceptionCheck()) {
                g_lunarNametagEntityPlayerClass = (jclass)env->NewGlobalRef(localClass);
                env->DeleteLocalRef(localClass);
            }
            if (env->ExceptionCheck()) env->ExceptionClear();
        }
        entityPlayerClass = g_lunarNametagEntityPlayerClass;
    }
    if (!entityPlayerClass || !env->IsInstanceOf(entity, entityPlayerClass)) return formattedName;

    const double remaining = GetDecimalSeconds();
    std::string result = JStringToUtf8(env, formattedName);
    if (NormalizeTimerNametagPosition(g_guiTimerNametagPosition) ==
        TIMER_NAMETAG_POSITION_PREFIX) {
        result = MakeTimerPrefix(remaining) + result;
    }
    else {
        result += MakeTimerSuffix(remaining);
    }
    jstring patchedName = env->NewStringUTF(result.c_str());
    if (!patchedName || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (patchedName) env->DeleteLocalRef(patchedName);
        return formattedName;
    }

    const LONG dispatchNumber = InterlockedIncrement(&g_lunarNametagTimerDispatchCount);
    if (dispatchNumber <= 30) {
        DebugLog("Lunar nametag timer render dispatch=%ld remaining=%.3f", dispatchNumber, remaining);
    }
    env->DeleteLocalRef(formattedName);
    return patchedName;
}

bool EnsureLunarAdventureFormattingJni(JNIEnv* env) {
    if (!env) return false;
    std::lock_guard<std::mutex> lock(g_nameTagJniMutex);
    if (g_lunarAdventureComponentClass && g_lunarAdventureTextColorClass &&
        g_lunarAdventureComponentTextColored && g_lunarAdventureTextColorFromRgb &&
        g_lunarAdventureComponentAppend) {
        return true;
    }

    if (!g_lunarAdventureComponentClass) {
        jclass localClass = FindClassLoose(env, "net/kyori/adventure/text/Component");
        if (localClass && !env->ExceptionCheck()) {
            g_lunarAdventureComponentClass = (jclass)env->NewGlobalRef(localClass);
            env->DeleteLocalRef(localClass);
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
    }
    if (!g_lunarAdventureTextColorClass) {
        jclass localClass = FindClassLoose(env, "net/kyori/adventure/text/format/TextColor");
        if (localClass && !env->ExceptionCheck()) {
            g_lunarAdventureTextColorClass = (jclass)env->NewGlobalRef(localClass);
            env->DeleteLocalRef(localClass);
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
    }
    if (!g_lunarAdventureComponentClass || !g_lunarAdventureTextColorClass) return false;

    if (!g_lunarAdventureComponentTextColored) {
        g_lunarAdventureComponentTextColored = GetStaticMethodIDCompat(env,
            g_lunarAdventureComponentClass,
            "text",
            "(Ljava/lang/String;Lnet/kyori/adventure/text/format/TextColor;)Lnet/kyori/adventure/text/TextComponent;");
    }
    if (!g_lunarAdventureTextColorFromRgb) {
        g_lunarAdventureTextColorFromRgb = GetStaticMethodIDCompat(env,
            g_lunarAdventureTextColorClass,
            "color",
            "(I)Lnet/kyori/adventure/text/format/TextColor;");
    }
    if (!g_lunarAdventureComponentAppend) {
        g_lunarAdventureComponentAppend = GetMethodIDCompat(env,
            g_lunarAdventureComponentClass,
            "append",
            "(Lnet/kyori/adventure/text/Component;)Lnet/kyori/adventure/text/Component;");
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    return g_lunarAdventureComponentTextColored && g_lunarAdventureTextColorFromRgb &&
        g_lunarAdventureComponentAppend;
}

jobject MakeLunarAdventureColoredText(
    JNIEnv* env,
    const std::string& text,
    std::uint32_t rgb) {
    if (!EnsureLunarAdventureFormattingJni(env)) return nullptr;
    jobject color = env->CallStaticObjectMethod(
        g_lunarAdventureTextColorClass,
        g_lunarAdventureTextColorFromRgb,
        (jint)(rgb & 0x00FFFFFFu));
    jstring textString = env->NewStringUTF(text.c_str());
    if (!color || !textString || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (color) env->DeleteLocalRef(color);
        if (textString) env->DeleteLocalRef(textString);
        return nullptr;
    }
    jobject component = env->CallStaticObjectMethod(
        g_lunarAdventureComponentClass,
        g_lunarAdventureComponentTextColored,
        textString,
        color);
    env->DeleteLocalRef(textString);
    env->DeleteLocalRef(color);
    if (!component || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (component) env->DeleteLocalRef(component);
        return nullptr;
    }
    return component;
}

jobject AppendLunarAdventureComponent(JNIEnv* env, jobject base, jobject child) {
    if (!env || !base || !child || !EnsureLunarAdventureFormattingJni(env)) return nullptr;
    jobject result = env->CallObjectMethod(base, g_lunarAdventureComponentAppend, child);
    if (!result || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (result) env->DeleteLocalRef(result);
        return nullptr;
    }
    return result;
}

jobject JNICALL LunarAdventureNameHookDispatch(
    JNIEnv* env,
    jclass,
    jobject entity) {
    if (!env || !entity) return nullptr;

    jmethodID getDisplayNameComponent = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_nameTagJniMutex);
        if (!g_lunarAdventureGetDisplayNameComponent) {
            jclass entityRuntimeClass = env->GetObjectClass(entity);
            if (entityRuntimeClass && !env->ExceptionCheck()) {
                g_lunarAdventureGetDisplayNameComponent = GetMethodIDCompat(env,
                    entityRuntimeClass,
                    "bridge$getDisplayNameComponent",
                    "()Lnet/kyori/adventure/text/Component;");
                env->DeleteLocalRef(entityRuntimeClass);
            }
            if (env->ExceptionCheck()) env->ExceptionClear();
        }
        getDisplayNameComponent = g_lunarAdventureGetDisplayNameComponent;
    }
    if (!getDisplayNameComponent) return nullptr;

    jobject component = env->CallObjectMethod(entity, getDisplayNameComponent);
    if (!component || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (component) env->DeleteLocalRef(component);
        return nullptr;
    }

    jclass entityPlayerClass = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_nameTagJniMutex);
        if (!g_lunarNametagEntityPlayerClass) {
            jclass localClass = FindClassLoose(env, "wn");
            if (localClass && !env->ExceptionCheck()) {
                g_lunarNametagEntityPlayerClass = (jclass)env->NewGlobalRef(localClass);
                env->DeleteLocalRef(localClass);
            }
            if (env->ExceptionCheck()) env->ExceptionClear();
        }
        entityPlayerClass = g_lunarNametagEntityPlayerClass;
    }
    const bool isPlayer = entityPlayerClass && env->IsInstanceOf(entity, entityPlayerClass);
    const bool timerActive = g_guiTimerNametagEnabled && g_timerActive &&
        !g_betweenRoundsTimerActive;

    jmethodID contentGet = nullptr;
    jmethodID contentSet = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_nameTagJniMutex);
        if (!g_lunarAdventureTextContentGet || !g_lunarAdventureTextContentSet) {
            jclass componentRuntimeClass = env->GetObjectClass(component);
            if (componentRuntimeClass && !env->ExceptionCheck()) {
                if (!g_lunarAdventureTextContentGet) {
                    g_lunarAdventureTextContentGet = GetMethodIDCompat(env,
                        componentRuntimeClass, "content", "()Ljava/lang/String;");
                }
                if (!g_lunarAdventureTextContentSet) {
                    g_lunarAdventureTextContentSet = GetMethodIDCompat(env,
                        componentRuntimeClass,
                        "content",
                        "(Ljava/lang/String;)Lnet/kyori/adventure/text/TextComponent;");
                }
                env->DeleteLocalRef(componentRuntimeClass);
            }
            if (env->ExceptionCheck()) env->ExceptionClear();
        }
        contentGet = g_lunarAdventureTextContentGet;
        contentSet = g_lunarAdventureTextContentSet;
    }

    std::string contentText;
    if (contentGet) {
        jstring contentString = (jstring)env->CallObjectMethod(component, contentGet);
        if (contentString && !env->ExceptionCheck()) {
            contentText = JStringToUtf8(env, contentString);
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (contentString) env->DeleteLocalRef(contentString);
    }

    const LONG dispatchNumber = InterlockedIncrement(&g_lunarAdventureNametagDispatchCount);
    if (dispatchNumber <= 30) {
        DebugLog("Lunar Adventure nametag dispatch=%ld player=%d active=%d content=%s",
            dispatchNumber,
            isPlayer ? 1 : 0,
            timerActive ? 1 : 0,
            contentText.c_str());
    }
    if (!isPlayer || !timerActive || !EnsureLunarAdventureFormattingJni(env)) return component;

    const double remaining = GetDecimalSeconds();
    const int whole = GetDisplayedTimerNumber(remaining);
    const bool prefix = NormalizeTimerNametagPosition(g_guiTimerNametagPosition) ==
        TIMER_NAMETAG_POSITION_PREFIX;
    const std::string openText = prefix ? "[" : " [";
    const std::string numberText = FormatTimerText(remaining);
    const std::string closeText = prefix ? "s] " : "s]";
    constexpr std::uint32_t bracketColour = 0x00AAAAAAu;
    jobject openComponent = MakeLunarAdventureColoredText(env, openText, bracketColour);
    jobject numberComponent = MakeLunarAdventureColoredText(
        env, numberText, GetTimerNumberColour(whole));
    jobject closeComponent = MakeLunarAdventureColoredText(env, closeText, bracketColour);
    if (!openComponent || !numberComponent || !closeComponent) {
        if (openComponent) env->DeleteLocalRef(openComponent);
        if (numberComponent) env->DeleteLocalRef(numberComponent);
        if (closeComponent) env->DeleteLocalRef(closeComponent);
        return component;
    }

    jobject first = prefix
        ? AppendLunarAdventureComponent(env, openComponent, numberComponent)
        : AppendLunarAdventureComponent(env, component, openComponent);
    jobject second = first
        ? AppendLunarAdventureComponent(env, first, prefix ? closeComponent : numberComponent)
        : nullptr;
    jobject patchedComponent = second
        ? AppendLunarAdventureComponent(env, second, prefix ? component : closeComponent)
        : nullptr;
    env->DeleteLocalRef(openComponent);
    env->DeleteLocalRef(numberComponent);
    env->DeleteLocalRef(closeComponent);
    if (first) env->DeleteLocalRef(first);
    if (second) env->DeleteLocalRef(second);
    if (!patchedComponent) return component;

    if (dispatchNumber <= 30) {
        DebugLog("Lunar Adventure nametag timer applied dispatch=%ld remaining=%.3f position=%s rgb=%06X text=%s%s%s",
            dispatchNumber,
            remaining,
            prefix ? "prefix" : "suffix",
            (unsigned int)GetTimerNumberColour(whole),
            openText.c_str(),
            numberText.c_str(),
            closeText.c_str());
    }
    env->DeleteLocalRef(component);
    return patchedComponent;
}

std::vector<unsigned char> BuildNameTagHelperClassBytes() {
    std::vector<unsigned char> bytes;
    const std::string nameTagDescriptor = TranslateLunarDescriptor("(Lbet;Ljava/lang/String;)Ljava/lang/String;");
    const std::string tabNameDescriptor = TranslateLunarDescriptor("(Lbdc;)Ljava/lang/String;");
    AppendClassU4(bytes, 0xCAFEBABE);
    AppendClassU2(bytes, 0);
    AppendClassU2(bytes, 52);
    AppendClassU2(bytes, 18);

    auto addUtf8 = [&bytes](const char* value) {
        AppendClassU1(bytes, 1);
        size_t len = strlen(value);
        AppendClassU2(bytes, (uint16_t)len);
        bytes.insert(bytes.end(), value, value + len);
    };

    addUtf8("TntTagNameDisplayHookV2"); // #1
    AppendClassU1(bytes, 7); AppendClassU2(bytes, 1); // #2 Class
    addUtf8("java/lang/Object"); // #3
    AppendClassU1(bytes, 7); AppendClassU2(bytes, 3); // #4 Class
    addUtf8("<init>"); // #5
    addUtf8("()V"); // #6
    addUtf8("Code"); // #7
    AppendClassU1(bytes, 10); AppendClassU2(bytes, 4); AppendClassU2(bytes, 9); // #8 Object.<init>
    AppendClassU1(bytes, 12); AppendClassU2(bytes, 5); AppendClassU2(bytes, 6); // #9 NameAndType
    addUtf8("a"); // #10
    addUtf8(nameTagDescriptor.c_str()); // #11
    addUtf8("SourceFile"); // #12
    addUtf8("TntTagNameDisplayHookV2.java"); // #13
    addUtf8("b"); // #14
    addUtf8(tabNameDescriptor.c_str()); // #15
    addUtf8("c"); // #16
    addUtf8("(Ljava/lang/String;)Ljava/lang/String;"); // #17

    AppendClassU2(bytes, 0x0021);
    AppendClassU2(bytes, 2);
    AppendClassU2(bytes, 4);
    AppendClassU2(bytes, 0);
    AppendClassU2(bytes, 0);
    AppendClassU2(bytes, 4);

    AppendClassU2(bytes, 0x0001);
    AppendClassU2(bytes, 5);
    AppendClassU2(bytes, 6);
    AppendClassU2(bytes, 1);
    AppendClassU2(bytes, 7);
    AppendClassU4(bytes, 17);
    AppendClassU2(bytes, 1);
    AppendClassU2(bytes, 1);
    AppendClassU4(bytes, 5);
    AppendClassU1(bytes, 0x2A);
    AppendClassU1(bytes, 0xB7); AppendClassU2(bytes, 8);
    AppendClassU1(bytes, 0xB1);
    AppendClassU2(bytes, 0);
    AppendClassU2(bytes, 0);

    AppendClassU2(bytes, 0x0109);
    AppendClassU2(bytes, 10);
    AppendClassU2(bytes, 11);
    AppendClassU2(bytes, 0);

    AppendClassU2(bytes, 0x0109);
    AppendClassU2(bytes, 14);
    AppendClassU2(bytes, 15);
    AppendClassU2(bytes, 0);

    AppendClassU2(bytes, 0x0109);
    AppendClassU2(bytes, 16);
    AppendClassU2(bytes, 17);
    AppendClassU2(bytes, 0);

    AppendClassU2(bytes, 1);
    AppendClassU2(bytes, 12);
    AppendClassU4(bytes, 2);
    AppendClassU2(bytes, 13);
    return bytes;
}

std::vector<unsigned char> BuildPublicWinsScoreFormatHelperClassBytes() {
    std::vector<unsigned char> bytes;
    const std::string formatDescriptor = TranslateLunarDescriptor("(Lauq;Ljava/lang/String;)Ljava/lang/String;");
    AppendClassU4(bytes, 0xCAFEBABE);
    AppendClassU2(bytes, 0);
    AppendClassU2(bytes, 52);
    AppendClassU2(bytes, 14);

    auto addUtf8 = [&bytes](const char* value) {
        AppendClassU1(bytes, 1);
        size_t len = strlen(value);
        AppendClassU2(bytes, (uint16_t)len);
        bytes.insert(bytes.end(), value, value + len);
    };

    addUtf8("TntTagScoreFormatHookV3"); // #1
    AppendClassU1(bytes, 7); AppendClassU2(bytes, 1); // #2 Class
    addUtf8("java/lang/Object"); // #3
    AppendClassU1(bytes, 7); AppendClassU2(bytes, 3); // #4 Class
    addUtf8("<init>"); // #5
    addUtf8("()V"); // #6
    addUtf8("Code"); // #7
    AppendClassU1(bytes, 10); AppendClassU2(bytes, 4); AppendClassU2(bytes, 9); // #8 Object.<init>
    AppendClassU1(bytes, 12); AppendClassU2(bytes, 5); AppendClassU2(bytes, 6); // #9 NameAndType
    addUtf8("d"); // #10
    addUtf8(formatDescriptor.c_str()); // #11
    addUtf8("SourceFile"); // #12
    addUtf8("TntTagScoreFormatHookV3.java"); // #13

    AppendClassU2(bytes, 0x0021);
    AppendClassU2(bytes, 2);
    AppendClassU2(bytes, 4);
    AppendClassU2(bytes, 0);
    AppendClassU2(bytes, 0);
    AppendClassU2(bytes, 2);

    AppendClassU2(bytes, 0x0001);
    AppendClassU2(bytes, 5);
    AppendClassU2(bytes, 6);
    AppendClassU2(bytes, 1);
    AppendClassU2(bytes, 7);
    AppendClassU4(bytes, 17);
    AppendClassU2(bytes, 1);
    AppendClassU2(bytes, 1);
    AppendClassU4(bytes, 5);
    AppendClassU1(bytes, 0x2A);
    AppendClassU1(bytes, 0xB7); AppendClassU2(bytes, 8);
    AppendClassU1(bytes, 0xB1);
    AppendClassU2(bytes, 0);
    AppendClassU2(bytes, 0);

    AppendClassU2(bytes, 0x0109);
    AppendClassU2(bytes, 10);
    AppendClassU2(bytes, 11);
    AppendClassU2(bytes, 0);

    AppendClassU2(bytes, 1);
    AppendClassU2(bytes, 12);
    AppendClassU4(bytes, 2);
    AppendClassU2(bytes, 13);
    return bytes;
}

bool EnsurePublicWinsScoreFormatHelper(JNIEnv* env) {
    if (!env) return false;
    if (g_publicWinsScoreFormatHelperClass) return true;

    jclass helperClass = FindClassLoose(env, "TntTagScoreFormatHookV3");
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        if (helperClass) env->DeleteLocalRef(helperClass);
        helperClass = nullptr;
    }

    if (!helperClass) {
        jobject loader = GetClassLoaderForClass(env, g_publicWinsScorePlayerTeamClass);
        std::vector<unsigned char> helperBytes = BuildPublicWinsScoreFormatHelperClassBytes();
        helperClass = env->DefineClass(
            "TntTagScoreFormatHookV3",
            loader,
            reinterpret_cast<const jbyte*>(helperBytes.data()),
            (jsize)helperBytes.size());
        if (loader) env->DeleteLocalRef(loader);
        if (!helperClass || env->ExceptionCheck()) {
            bool hadException = env->ExceptionCheck();
            if (hadException) env->ExceptionClear();
            if (helperClass) env->DeleteLocalRef(helperClass);
            DebugLog("Public wins score-format helper define failed%s",
                hadException ? " (exception cleared)" : "");
            return false;
        }
        DebugLog("Public wins score-format helper defined TntTagScoreFormatHookV3");
    }

    JNINativeMethod method = {};
    const std::string formatDescriptor = TranslateLunarDescriptor("(Lauq;Ljava/lang/String;)Ljava/lang/String;");
    method.name = const_cast<char*>("d");
    method.signature = const_cast<char*>(formatDescriptor.c_str());
    method.fnPtr = reinterpret_cast<void*>(&PublicWinsScoreFormatHookDispatch);
    if (env->RegisterNatives(helperClass, &method, 1) != JNI_OK || env->ExceptionCheck()) {
        bool hadException = env->ExceptionCheck();
        if (hadException) env->ExceptionClear();
        env->DeleteLocalRef(helperClass);
        DebugLog("Public wins score-format helper RegisterNatives failed%s",
            hadException ? " (exception cleared)" : "");
        return false;
    }

    g_publicWinsScoreFormatHelperClass = (jclass)env->NewGlobalRef(helperClass);
    env->DeleteLocalRef(helperClass);
    if (!g_publicWinsScoreFormatHelperClass || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        g_publicWinsScoreFormatHelperClass = nullptr;
        return false;
    }
    return true;
}

std::vector<unsigned char> BuildPublicWinsRenderedNameHelperClassBytes() {
    std::vector<unsigned char> bytes;
    const std::string renderedNameDescriptor = TranslateLunarDescriptor("(Lpr;)Ljava/lang/String;");
    AppendClassU4(bytes, 0xCAFEBABE);
    AppendClassU2(bytes, 0);
    AppendClassU2(bytes, 52);
    AppendClassU2(bytes, 14);

    auto addUtf8 = [&bytes](const char* value) {
        AppendClassU1(bytes, 1);
        size_t len = strlen(value);
        AppendClassU2(bytes, (uint16_t)len);
        bytes.insert(bytes.end(), value, value + len);
    };

    addUtf8("TntTagRenderedNameHookV4"); // #1
    AppendClassU1(bytes, 7); AppendClassU2(bytes, 1); // #2 Class
    addUtf8("java/lang/Object"); // #3
    AppendClassU1(bytes, 7); AppendClassU2(bytes, 3); // #4 Class
    addUtf8("<init>"); // #5
    addUtf8("()V"); // #6
    addUtf8("Code"); // #7
    AppendClassU1(bytes, 10); AppendClassU2(bytes, 4); AppendClassU2(bytes, 9); // #8 Object.<init>
    AppendClassU1(bytes, 12); AppendClassU2(bytes, 5); AppendClassU2(bytes, 6); // #9 NameAndType
    addUtf8("e"); // #10
    addUtf8(renderedNameDescriptor.c_str()); // #11
    addUtf8("SourceFile"); // #12
    addUtf8("TntTagRenderedNameHookV4.java"); // #13

    AppendClassU2(bytes, 0x0021);
    AppendClassU2(bytes, 2);
    AppendClassU2(bytes, 4);
    AppendClassU2(bytes, 0);
    AppendClassU2(bytes, 0);
    AppendClassU2(bytes, 2);

    AppendClassU2(bytes, 0x0001);
    AppendClassU2(bytes, 5);
    AppendClassU2(bytes, 6);
    AppendClassU2(bytes, 1);
    AppendClassU2(bytes, 7);
    AppendClassU4(bytes, 17);
    AppendClassU2(bytes, 1);
    AppendClassU2(bytes, 1);
    AppendClassU4(bytes, 5);
    AppendClassU1(bytes, 0x2A);
    AppendClassU1(bytes, 0xB7); AppendClassU2(bytes, 8);
    AppendClassU1(bytes, 0xB1);
    AppendClassU2(bytes, 0);
    AppendClassU2(bytes, 0);

    AppendClassU2(bytes, 0x0109);
    AppendClassU2(bytes, 10);
    AppendClassU2(bytes, 11);
    AppendClassU2(bytes, 0);

    AppendClassU2(bytes, 1);
    AppendClassU2(bytes, 12);
    AppendClassU4(bytes, 2);
    AppendClassU2(bytes, 13);
    return bytes;
}

bool EnsurePublicWinsRenderedNameHelper(JNIEnv* env) {
    if (!env) return false;
    if (g_publicWinsRenderedNameHelperClass) return true;

    jclass helperClass = FindClassLoose(env, "TntTagRenderedNameHookV4");
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        if (helperClass) env->DeleteLocalRef(helperClass);
        helperClass = nullptr;
    }
    if (!helperClass) {
        jobject loader = GetClassLoaderForClass(env, g_publicWinsLivingRendererClass);
        std::vector<unsigned char> helperBytes = BuildPublicWinsRenderedNameHelperClassBytes();
        helperClass = env->DefineClass(
            "TntTagRenderedNameHookV4",
            loader,
            reinterpret_cast<const jbyte*>(helperBytes.data()),
            (jsize)helperBytes.size());
        if (loader) env->DeleteLocalRef(loader);
        if (!helperClass || env->ExceptionCheck()) {
            bool hadException = env->ExceptionCheck();
            if (hadException) env->ExceptionClear();
            if (helperClass) env->DeleteLocalRef(helperClass);
            DebugLog("Public wins rendered-name helper define failed%s",
                hadException ? " (exception cleared)" : "");
            return false;
        }
        DebugLog("Public wins rendered-name helper defined TntTagRenderedNameHookV4");
    }

    JNINativeMethod method = {};
    const std::string renderedNameDescriptor = TranslateLunarDescriptor("(Lpr;)Ljava/lang/String;");
    method.name = const_cast<char*>("e");
    method.signature = const_cast<char*>(renderedNameDescriptor.c_str());
    method.fnPtr = reinterpret_cast<void*>(&PublicWinsRenderedNameHookDispatch);
    if (env->RegisterNatives(helperClass, &method, 1) != JNI_OK || env->ExceptionCheck()) {
        bool hadException = env->ExceptionCheck();
        if (hadException) env->ExceptionClear();
        env->DeleteLocalRef(helperClass);
        DebugLog("Public wins rendered-name helper RegisterNatives failed%s",
            hadException ? " (exception cleared)" : "");
        return false;
    }

    g_publicWinsRenderedNameHelperClass = (jclass)env->NewGlobalRef(helperClass);
    env->DeleteLocalRef(helperClass);
    if (!g_publicWinsRenderedNameHelperClass || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        g_publicWinsRenderedNameHelperClass = nullptr;
        return false;
    }
    return true;
}

std::vector<unsigned char> BuildPublicWinsRenderedComponentHelperClassBytes() {
    std::vector<unsigned char> bytes;
    const std::string componentDescriptor = TranslateLunarDescriptor(
        IsLunarNamedClient() ? "(Lpr;Leu;)Ljava/lang/String;" : "(Leu;)Ljava/lang/String;");
    AppendClassU4(bytes, 0xCAFEBABE);
    AppendClassU2(bytes, 0);
    AppendClassU2(bytes, 52);
    AppendClassU2(bytes, 14);

    auto addUtf8 = [&bytes](const char* value) {
        AppendClassU1(bytes, 1);
        size_t len = strlen(value);
        AppendClassU2(bytes, (uint16_t)len);
        bytes.insert(bytes.end(), value, value + len);
    };

    addUtf8("TntTagRenderedComponentHookV6"); // #1
    AppendClassU1(bytes, 7); AppendClassU2(bytes, 1); // #2 Class
    addUtf8("java/lang/Object"); // #3
    AppendClassU1(bytes, 7); AppendClassU2(bytes, 3); // #4 Class
    addUtf8("<init>"); // #5
    addUtf8("()V"); // #6
    addUtf8("Code"); // #7
    AppendClassU1(bytes, 10); AppendClassU2(bytes, 4); AppendClassU2(bytes, 9); // #8 Object.<init>
    AppendClassU1(bytes, 12); AppendClassU2(bytes, 5); AppendClassU2(bytes, 6); // #9 NameAndType
    addUtf8("f"); // #10
    addUtf8(componentDescriptor.c_str()); // #11
    addUtf8("SourceFile"); // #12
    addUtf8("TntTagRenderedComponentHookV6.java"); // #13

    AppendClassU2(bytes, 0x0021);
    AppendClassU2(bytes, 2);
    AppendClassU2(bytes, 4);
    AppendClassU2(bytes, 0);
    AppendClassU2(bytes, 0);
    AppendClassU2(bytes, 2);

    AppendClassU2(bytes, 0x0001);
    AppendClassU2(bytes, 5);
    AppendClassU2(bytes, 6);
    AppendClassU2(bytes, 1);
    AppendClassU2(bytes, 7);
    AppendClassU4(bytes, 17);
    AppendClassU2(bytes, 1);
    AppendClassU2(bytes, 1);
    AppendClassU4(bytes, 5);
    AppendClassU1(bytes, 0x2A);
    AppendClassU1(bytes, 0xB7); AppendClassU2(bytes, 8);
    AppendClassU1(bytes, 0xB1);
    AppendClassU2(bytes, 0);
    AppendClassU2(bytes, 0);

    AppendClassU2(bytes, 0x0109);
    AppendClassU2(bytes, 10);
    AppendClassU2(bytes, 11);
    AppendClassU2(bytes, 0);

    AppendClassU2(bytes, 1);
    AppendClassU2(bytes, 12);
    AppendClassU4(bytes, 2);
    AppendClassU2(bytes, 13);
    return bytes;
}

std::vector<unsigned char> BuildLunarAdventureNameHelperClassBytes() {
    std::vector<unsigned char> bytes;
    const char* dispatchDescriptor =
        "(Lnet/minecraft/entity/EntityLivingBase;)Lnet/kyori/adventure/text/Component;";
    AppendClassU4(bytes, 0xCAFEBABE);
    AppendClassU2(bytes, 0);
    AppendClassU2(bytes, 52);
    AppendClassU2(bytes, 14);

    auto addUtf8 = [&bytes](const char* value) {
        AppendClassU1(bytes, 1);
        size_t len = strlen(value);
        AppendClassU2(bytes, (uint16_t)len);
        bytes.insert(bytes.end(), value, value + len);
    };

    addUtf8("TntTagLunarAdventureNameHookV1"); // #1
    AppendClassU1(bytes, 7); AppendClassU2(bytes, 1); // #2 Class
    addUtf8("java/lang/Object"); // #3
    AppendClassU1(bytes, 7); AppendClassU2(bytes, 3); // #4 Class
    addUtf8("<init>"); // #5
    addUtf8("()V"); // #6
    addUtf8("Code"); // #7
    AppendClassU1(bytes, 10); AppendClassU2(bytes, 4); AppendClassU2(bytes, 9); // #8 Object.<init>
    AppendClassU1(bytes, 12); AppendClassU2(bytes, 5); AppendClassU2(bytes, 6); // #9 NameAndType
    addUtf8("f"); // #10
    addUtf8(dispatchDescriptor); // #11
    addUtf8("SourceFile"); // #12
    addUtf8("TntTagLunarAdventureNameHookV1.java"); // #13

    AppendClassU2(bytes, 0x0021);
    AppendClassU2(bytes, 2);
    AppendClassU2(bytes, 4);
    AppendClassU2(bytes, 0);
    AppendClassU2(bytes, 0);
    AppendClassU2(bytes, 2);

    AppendClassU2(bytes, 0x0001);
    AppendClassU2(bytes, 5);
    AppendClassU2(bytes, 6);
    AppendClassU2(bytes, 1);
    AppendClassU2(bytes, 7);
    AppendClassU4(bytes, 17);
    AppendClassU2(bytes, 1);
    AppendClassU2(bytes, 1);
    AppendClassU4(bytes, 5);
    AppendClassU1(bytes, 0x2A);
    AppendClassU1(bytes, 0xB7); AppendClassU2(bytes, 8);
    AppendClassU1(bytes, 0xB1);
    AppendClassU2(bytes, 0);
    AppendClassU2(bytes, 0);

    AppendClassU2(bytes, 0x0109);
    AppendClassU2(bytes, 10);
    AppendClassU2(bytes, 11);
    AppendClassU2(bytes, 0);

    AppendClassU2(bytes, 1);
    AppendClassU2(bytes, 12);
    AppendClassU4(bytes, 2);
    AppendClassU2(bytes, 13);
    return bytes;
}

bool EnsureLunarAdventureNameHelper(JNIEnv* env) {
    if (!env || !IsLunarNamedClient()) return false;
    if (g_lunarAdventureNameHelperClass) return true;

    jclass helperClass = FindClassLoose(env, "TntTagLunarAdventureNameHookV1");
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        if (helperClass) env->DeleteLocalRef(helperClass);
        helperClass = nullptr;
    }
    if (!helperClass) {
        jobject loader = GetClassLoaderForClass(env, g_publicWinsLivingRendererClass);
        std::vector<unsigned char> helperBytes = BuildLunarAdventureNameHelperClassBytes();
        helperClass = env->DefineClass(
            "TntTagLunarAdventureNameHookV1",
            loader,
            reinterpret_cast<const jbyte*>(helperBytes.data()),
            (jsize)helperBytes.size());
        if (loader) env->DeleteLocalRef(loader);
        if (!helperClass || env->ExceptionCheck()) {
            bool hadException = env->ExceptionCheck();
            if (hadException) env->ExceptionClear();
            if (helperClass) env->DeleteLocalRef(helperClass);
            DebugLog("Lunar Adventure nametag helper define failed%s",
                hadException ? " (exception cleared)" : "");
            return false;
        }
        DebugLog("Lunar Adventure nametag helper defined TntTagLunarAdventureNameHookV1");
    }

    JNINativeMethod method = {};
    method.name = const_cast<char*>("f");
    method.signature = const_cast<char*>(
        "(Lnet/minecraft/entity/EntityLivingBase;)Lnet/kyori/adventure/text/Component;");
    method.fnPtr = reinterpret_cast<void*>(&LunarAdventureNameHookDispatch);
    if (env->RegisterNatives(helperClass, &method, 1) != JNI_OK || env->ExceptionCheck()) {
        bool hadException = env->ExceptionCheck();
        if (hadException) env->ExceptionClear();
        env->DeleteLocalRef(helperClass);
        DebugLog("Lunar Adventure nametag helper RegisterNatives failed%s",
            hadException ? " (exception cleared)" : "");
        return false;
    }

    g_lunarAdventureNameHelperClass = (jclass)env->NewGlobalRef(helperClass);
    env->DeleteLocalRef(helperClass);
    if (!g_lunarAdventureNameHelperClass || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        g_lunarAdventureNameHelperClass = nullptr;
        return false;
    }
    return true;
}

bool EnsurePublicWinsRenderedComponentHelper(JNIEnv* env) {
    if (!env) return false;
    if (g_publicWinsRenderedComponentHelperClass) return true;

    jclass helperClass = FindClassLoose(env, "TntTagRenderedComponentHookV6");
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        if (helperClass) env->DeleteLocalRef(helperClass);
        helperClass = nullptr;
    }
    if (!helperClass) {
        jobject loader = GetClassLoaderForClass(env, g_publicWinsLivingRendererClass);
        std::vector<unsigned char> helperBytes = BuildPublicWinsRenderedComponentHelperClassBytes();
        helperClass = env->DefineClass(
            "TntTagRenderedComponentHookV6",
            loader,
            reinterpret_cast<const jbyte*>(helperBytes.data()),
            (jsize)helperBytes.size());
        if (loader) env->DeleteLocalRef(loader);
        if (!helperClass || env->ExceptionCheck()) {
            bool hadException = env->ExceptionCheck();
            if (hadException) env->ExceptionClear();
            if (helperClass) env->DeleteLocalRef(helperClass);
            DebugLog("Public wins rendered-component helper define failed%s",
                hadException ? " (exception cleared)" : "");
            return false;
        }
        DebugLog("Public wins rendered-component helper defined TntTagRenderedComponentHookV6");
    }

    JNINativeMethod method = {};
    const std::string componentDescriptor = TranslateLunarDescriptor(
        IsLunarNamedClient() ? "(Lpr;Leu;)Ljava/lang/String;" : "(Leu;)Ljava/lang/String;");
    method.name = const_cast<char*>("f");
    method.signature = const_cast<char*>(componentDescriptor.c_str());
    method.fnPtr = IsLunarNamedClient()
        ? reinterpret_cast<void*>(&LunarRenderedComponentHookDispatch)
        : reinterpret_cast<void*>(&PublicWinsRenderedComponentHookDispatch);
    if (env->RegisterNatives(helperClass, &method, 1) != JNI_OK || env->ExceptionCheck()) {
        bool hadException = env->ExceptionCheck();
        if (hadException) env->ExceptionClear();
        env->DeleteLocalRef(helperClass);
        DebugLog("Public wins rendered-component helper RegisterNatives failed%s",
            hadException ? " (exception cleared)" : "");
        return false;
    }

    g_publicWinsRenderedComponentHelperClass = (jclass)env->NewGlobalRef(helperClass);
    env->DeleteLocalRef(helperClass);
    if (!g_publicWinsRenderedComponentHelperClass || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        g_publicWinsRenderedComponentHelperClass = nullptr;
        return false;
    }
    return true;
}

bool EnsureNameTagHelper(JNIEnv* env) {
    if (!env) return false;
    if (g_nameTagHelperClass) return true;

    jclass helperClass = FindClassLoose(env, "TntTagNameDisplayHookV2");
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        if (helperClass) env->DeleteLocalRef(helperClass);
        helperClass = nullptr;
    }

    if (!helperClass) {
        jclass loaderSource = g_nameTagRenderPlayerClass
            ? g_nameTagRenderPlayerClass
            : g_publicWinsScorePlayerTeamClass;
        jobject loader = GetClassLoaderForClass(env, loaderSource);
        std::vector<unsigned char> helperBytes = BuildNameTagHelperClassBytes();
        helperClass = env->DefineClass(
            "TntTagNameDisplayHookV2",
            loader,
            reinterpret_cast<const jbyte*>(helperBytes.data()),
            (jsize)helperBytes.size());
        if (loader) env->DeleteLocalRef(loader);

        if (!helperClass || env->ExceptionCheck()) {
            bool hadException = env->ExceptionCheck();
            if (hadException) env->ExceptionClear();
            if (helperClass) env->DeleteLocalRef(helperClass);
            DebugLog("Name display hook failed defining helper%s", hadException ? " (exception cleared)" : "");
            return false;
        }

        DebugLog("Name display hook defined helper TntTagNameDisplayHookV2");
    }

    JNINativeMethod methods[3] = {};
    const std::string nameTagDescriptor = TranslateLunarDescriptor("(Lbet;Ljava/lang/String;)Ljava/lang/String;");
    const std::string tabNameDescriptor = TranslateLunarDescriptor("(Lbdc;)Ljava/lang/String;");
    methods[0].name = const_cast<char*>("a");
    methods[0].signature = const_cast<char*>(nameTagDescriptor.c_str());
    methods[0].fnPtr = reinterpret_cast<void*>(&NameTagHookDispatch);
    methods[1].name = const_cast<char*>("b");
    methods[1].signature = const_cast<char*>(tabNameDescriptor.c_str());
    methods[1].fnPtr = reinterpret_cast<void*>(&PublicWinsTabNameHookDispatch);
    methods[2].name = const_cast<char*>("c");
    methods[2].signature = const_cast<char*>("(Ljava/lang/String;)Ljava/lang/String;");
    methods[2].fnPtr = reinterpret_cast<void*>(&PublicWinsApiTabTextHookDispatch);
    if (env->RegisterNatives(helperClass, methods, 3) != JNI_OK || env->ExceptionCheck()) {
        bool hadException = env->ExceptionCheck();
        if (hadException) env->ExceptionClear();
        DebugLog("Name display hook RegisterNatives failed%s", hadException ? " (exception cleared)" : "");
        env->DeleteLocalRef(helperClass);
        return false;
    }

    g_nameTagHelperClass = (jclass)env->NewGlobalRef(helperClass);
    env->DeleteLocalRef(helperClass);
    if (!g_nameTagHelperClass || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (g_nameTagHelperClass) env->DeleteGlobalRef(g_nameTagHelperClass);
        g_nameTagHelperClass = nullptr;
        DebugLog("Name display hook failed global-ref helper");
        return false;
    }

    return true;
}

bool LoadNameTagRenderPlayerClassBytes(JNIEnv* env, std::vector<unsigned char>& outBytes) {
    return CaptureRuntimeClassBytes(env, g_nameTagRenderPlayerClass, outBytes, "RenderPlayer bln");
}

bool ReadPayloadU2(const std::vector<unsigned char>& bytes, size_t offset, uint16_t& value) {
    if (offset + 2 > bytes.size()) return false;
    value = ((uint16_t)bytes[offset] << 8) | (uint16_t)bytes[offset + 1];
    return true;
}

bool WritePayloadU2(std::vector<unsigned char>& bytes, size_t offset, uint16_t value) {
    if (offset + 2 > bytes.size()) return false;
    bytes[offset] = (unsigned char)((value >> 8) & 0xFF);
    bytes[offset + 1] = (unsigned char)(value & 0xFF);
    return true;
}

bool AddPayloadU2(std::vector<unsigned char>& bytes, size_t offset, uint16_t delta) {
    uint16_t value = 0;
    if (!ReadPayloadU2(bytes, offset, value)) return false;
    return WritePayloadU2(bytes, offset, (uint16_t)(value + delta));
}

bool AdjustNameTagStackMapTable(std::vector<unsigned char>& payload, uint16_t insertLen) {
    uint16_t entries = 0;
    if (!ReadPayloadU2(payload, 0, entries) || entries == 0 || payload.size() < 3) return true;

    size_t frameOffset = 2;
    unsigned char frameType = payload[frameOffset];
    if (frameType <= 63) {
        uint16_t newDelta = (uint16_t)(frameType + insertLen);
        if (newDelta <= 63) {
            payload[frameOffset] = (unsigned char)newDelta;
        }
        else {
            payload[frameOffset] = 251;
            payload.insert(payload.begin() + (ptrdiff_t)frameOffset + 1, {
                (unsigned char)((newDelta >> 8) & 0xFF),
                (unsigned char)(newDelta & 0xFF)
            });
        }
        return true;
    }

    if (frameType >= 64 && frameType <= 127) {
        uint16_t newDelta = (uint16_t)((frameType - 64) + insertLen);
        if (newDelta <= 63) {
            payload[frameOffset] = (unsigned char)(64 + newDelta);
        }
        else {
            payload[frameOffset] = 247;
            payload.insert(payload.begin() + (ptrdiff_t)frameOffset + 1, {
                (unsigned char)((newDelta >> 8) & 0xFF),
                (unsigned char)(newDelta & 0xFF)
            });
        }
        return true;
    }

    if (frameType == 247 || (frameType >= 248 && frameType <= 255)) {
        return AddPayloadU2(payload, frameOffset + 1, insertLen);
    }

    return true;
}

bool AdjustNameTagLineNumberTable(std::vector<unsigned char>& payload, uint16_t insertLen) {
    uint16_t count = 0;
    if (!ReadPayloadU2(payload, 0, count)) return false;
    size_t offset = 2;
    for (uint16_t i = 0; i < count; ++i) {
        if (!AddPayloadU2(payload, offset, insertLen)) return false;
        offset += 4;
        if (offset > payload.size()) return false;
    }
    return true;
}

bool AdjustNameTagLocalVariableTable(std::vector<unsigned char>& payload, uint16_t insertLen) {
    uint16_t count = 0;
    if (!ReadPayloadU2(payload, 0, count)) return false;
    size_t offset = 2;
    for (uint16_t i = 0; i < count; ++i) {
        if (!AddPayloadU2(payload, offset, insertLen)) return false;
        offset += 10;
        if (offset > payload.size()) return false;
    }
    return true;
}

bool PatchPublicWinsApiTabRendererMethod(
    const std::vector<unsigned char>& originalBytes,
    std::vector<unsigned char>& patchedBytes) {
    patchedBytes = originalBytes;

    size_t cpOffset = 0;
    uint16_t constantPoolCount = 0;
    std::vector<std::string> utf8Constants;
    if (!ReadClassConstantPoolUtf8(patchedBytes, cpOffset, constantPoolCount, utf8Constants) ||
        constantPoolCount > 65520) {
        DebugLog("Public wins API tab hook invalid apj.class constant pool");
        return false;
    }

    std::vector<unsigned char> cpAdditions;
    uint16_t nextIndex = constantPoolCount;
    uint16_t helperNameUtf8 = AppendClassUtf8Cp(cpAdditions, nextIndex, "TntTagNameDisplayHookV2");
    uint16_t helperClass = AppendClassClassCp(cpAdditions, nextIndex, helperNameUtf8);
    uint16_t dispatchNameUtf8 = AppendClassUtf8Cp(cpAdditions, nextIndex, "c");
    uint16_t dispatchDescUtf8 = AppendClassUtf8Cp(
        cpAdditions, nextIndex, "(Ljava/lang/String;)Ljava/lang/String;");
    uint16_t dispatchNameAndType = AppendClassNameAndTypeCp(
        cpAdditions, nextIndex, dispatchNameUtf8, dispatchDescUtf8);
    uint16_t dispatchMethodRef = AppendClassMethodRefCp(
        cpAdditions, nextIndex, helperClass, dispatchNameAndType);

    patchedBytes.insert(patchedBytes.begin() + (ptrdiff_t)cpOffset, cpAdditions.begin(), cpAdditions.end());
    if (!WriteClassU2At(patchedBytes, 8, nextIndex)) return false;

    size_t offset = 0;
    if (!ReadClassConstantPoolUtf8(patchedBytes, offset, constantPoolCount, utf8Constants) ||
        !AdvanceClassBytes(patchedBytes, offset, 6)) {
        return false;
    }

    uint16_t interfacesCount = 0;
    if (!ReadClassU2(patchedBytes, offset, interfacesCount) ||
        !AdvanceClassBytes(patchedBytes, offset, (size_t)interfacesCount * 2)) {
        return false;
    }

    uint16_t fieldsCount = 0;
    if (!ReadClassU2(patchedBytes, offset, fieldsCount)) return false;
    for (uint16_t i = 0; i < fieldsCount; ++i) {
        if (!SkipClassMemberInfo(patchedBytes, offset)) return false;
    }

    uint16_t methodsCount = 0;
    if (!ReadClassU2(patchedBytes, offset, methodsCount)) return false;
    for (uint16_t i = 0; i < methodsCount; ++i) {
        uint16_t accessFlags = 0;
        uint16_t nameIndex = 0;
        uint16_t descriptorIndex = 0;
        uint16_t attributesCount = 0;
        if (!ReadClassU2(patchedBytes, offset, accessFlags) ||
            !ReadClassU2(patchedBytes, offset, nameIndex) ||
            !ReadClassU2(patchedBytes, offset, descriptorIndex) ||
            !ReadClassU2(patchedBytes, offset, attributesCount)) {
            return false;
        }

        const std::string methodName =
            nameIndex < utf8Constants.size() ? utf8Constants[(size_t)nameIndex] : std::string();
        const std::string descriptor =
            descriptorIndex < utf8Constants.size() ? utf8Constants[(size_t)descriptorIndex] : std::string();

        for (uint16_t attr = 0; attr < attributesCount; ++attr) {
            uint16_t attributeNameIndex = 0;
            uint32_t attributeLength = 0;
            if (!ReadClassU2(patchedBytes, offset, attributeNameIndex)) return false;
            size_t attributeLengthOffset = offset;
            if (!ReadClassU4(patchedBytes, offset, attributeLength)) return false;

            size_t attributeStart = offset;
            if (!AdvanceClassBytes(patchedBytes, offset, (size_t)attributeLength)) return false;
            size_t attributeEnd = offset;
            const std::string attributeName = attributeNameIndex < utf8Constants.size()
                ? utf8Constants[(size_t)attributeNameIndex] : std::string();

            if (methodName != "a" || descriptor != "(Ljava/lang/String;FFZZ)V" ||
                attributeName != "Code") {
                continue;
            }

            size_t codeOffset = attributeStart;
            uint16_t oldMaxStack = 0;
            uint16_t oldMaxLocals = 0;
            uint32_t oldCodeLength = 0;
            if (!ReadClassU2(patchedBytes, codeOffset, oldMaxStack) ||
                !ReadClassU2(patchedBytes, codeOffset, oldMaxLocals) ||
                !ReadClassU4(patchedBytes, codeOffset, oldCodeLength) ||
                codeOffset + oldCodeLength > attributeEnd) {
                DebugLog("Public wins API tab hook unsupported apj draw Code attribute");
                return false;
            }

            size_t oldCodeStart = codeOffset;
            size_t oldCodeEnd = oldCodeStart + oldCodeLength;
            size_t exceptionOffset = oldCodeEnd;
            uint16_t exceptionCount = 0;
            if (!ReadClassU2(patchedBytes, exceptionOffset, exceptionCount) ||
                exceptionOffset + ((size_t)exceptionCount * 8) > attributeEnd) {
                return false;
            }
            size_t exceptionTableStart = exceptionOffset;
            exceptionOffset += (size_t)exceptionCount * 8;
            uint16_t codeAttributeCount = 0;
            if (!ReadClassU2(patchedBytes, exceptionOffset, codeAttributeCount)) return false;
            size_t nestedAttributeStart = exceptionOffset;

            std::vector<unsigned char> injected;
            AppendClassU1(injected, 0x2B); // aload_1 display string
            AppendClassU1(injected, 0xB8); AppendClassU2(injected, dispatchMethodRef); // invokestatic helper.c
            AppendClassU1(injected, 0x4C); // astore_1
            const uint16_t insertLen = (uint16_t)injected.size();

            std::vector<unsigned char> codeAttribute;
            AppendClassU2(codeAttribute, oldMaxStack < 1 ? 1 : oldMaxStack);
            AppendClassU2(codeAttribute, oldMaxLocals);
            AppendClassU4(codeAttribute, oldCodeLength + (uint32_t)injected.size());
            codeAttribute.insert(codeAttribute.end(), injected.begin(), injected.end());
            codeAttribute.insert(codeAttribute.end(),
                patchedBytes.begin() + (ptrdiff_t)oldCodeStart,
                patchedBytes.begin() + (ptrdiff_t)oldCodeEnd);

            AppendClassU2(codeAttribute, exceptionCount);
            size_t exceptionRead = exceptionTableStart;
            for (uint16_t ex = 0; ex < exceptionCount; ++ex) {
                uint16_t startPc = 0;
                uint16_t endPc = 0;
                uint16_t handlerPc = 0;
                uint16_t catchType = 0;
                if (!ReadClassU2(patchedBytes, exceptionRead, startPc) ||
                    !ReadClassU2(patchedBytes, exceptionRead, endPc) ||
                    !ReadClassU2(patchedBytes, exceptionRead, handlerPc) ||
                    !ReadClassU2(patchedBytes, exceptionRead, catchType)) {
                    return false;
                }
                AppendClassU2(codeAttribute, (uint16_t)(startPc + insertLen));
                AppendClassU2(codeAttribute, (uint16_t)(endPc + insertLen));
                AppendClassU2(codeAttribute, (uint16_t)(handlerPc + insertLen));
                AppendClassU2(codeAttribute, catchType);
            }

            AppendClassU2(codeAttribute, codeAttributeCount);
            size_t nestedOffset = nestedAttributeStart;
            for (uint16_t nested = 0; nested < codeAttributeCount; ++nested) {
                uint16_t nestedNameIndex = 0;
                uint32_t nestedLength = 0;
                if (!ReadClassU2(patchedBytes, nestedOffset, nestedNameIndex) ||
                    !ReadClassU4(patchedBytes, nestedOffset, nestedLength) ||
                    nestedOffset + nestedLength > attributeEnd) {
                    return false;
                }

                std::vector<unsigned char> payload(
                    patchedBytes.begin() + (ptrdiff_t)nestedOffset,
                    patchedBytes.begin() + (ptrdiff_t)nestedOffset + (ptrdiff_t)nestedLength);
                nestedOffset += nestedLength;
                const std::string nestedName = nestedNameIndex < utf8Constants.size()
                    ? utf8Constants[(size_t)nestedNameIndex] : std::string();
                if (nestedName == "StackMapTable") {
                    if (!AdjustNameTagStackMapTable(payload, insertLen)) return false;
                }
                else if (nestedName == "LineNumberTable") {
                    if (!AdjustNameTagLineNumberTable(payload, insertLen)) return false;
                }
                else if (nestedName == "LocalVariableTable" || nestedName == "LocalVariableTypeTable") {
                    if (!AdjustNameTagLocalVariableTable(payload, insertLen)) return false;
                }

                AppendClassU2(codeAttribute, nestedNameIndex);
                AppendClassU4(codeAttribute, (uint32_t)payload.size());
                codeAttribute.insert(codeAttribute.end(), payload.begin(), payload.end());
            }

            if (!WriteClassU4At(patchedBytes, attributeLengthOffset, (uint32_t)codeAttribute.size())) {
                return false;
            }
            patchedBytes.erase(
                patchedBytes.begin() + (ptrdiff_t)attributeStart,
                patchedBytes.begin() + (ptrdiff_t)attributeEnd);
            patchedBytes.insert(
                patchedBytes.begin() + (ptrdiff_t)attributeStart,
                codeAttribute.begin(), codeAttribute.end());

            DebugLog("Public wins API tab hook patched apj draw oldCodeLength=%u insert=%u",
                (unsigned int)oldCodeLength, (unsigned int)insertLen);
            return true;
        }
    }

    DebugLog("Public wins API tab hook failed to find apj.a(Ljava/lang/String;FFZZ)V");
    return false;
}

bool PatchRenderPlayerNameTagMethod(const std::vector<unsigned char>& originalBytes, std::vector<unsigned char>& patchedBytes) {
    patchedBytes = originalBytes;
    const char* owner = "net/minecraft/client/renderer/entity/RenderPlayer";
    const char* mappedMethodName = IsLunarNamedClient()
        ? FindLunarMethodName(owner, "a", "(Lbet;DDDLjava/lang/String;FD)V")
        : nullptr;
    const std::string targetMethodName = mappedMethodName ? mappedMethodName : "a";
    const std::string targetDescriptor = TranslateLunarDescriptor("(Lbet;DDDLjava/lang/String;FD)V");
    const std::string dispatchDescriptor = TranslateLunarDescriptor("(Lbet;Ljava/lang/String;)Ljava/lang/String;");

    size_t cpOffset = 0;
    uint16_t constantPoolCount = 0;
    std::vector<std::string> utf8Constants;
    if (!ReadClassConstantPoolUtf8(patchedBytes, cpOffset, constantPoolCount, utf8Constants)) {
        DebugLog("Name display hook invalid bln.class constant pool");
        return false;
    }

    if (constantPoolCount > 65520) {
        DebugLog("Name display hook bln.class constant pool too large count=%u", (unsigned int)constantPoolCount);
        return false;
    }

    std::vector<unsigned char> cpAdditions;
    uint16_t nextIndex = constantPoolCount;
    uint16_t helperNameUtf8 = AppendClassUtf8Cp(cpAdditions, nextIndex, "TntTagNameDisplayHookV2");
    uint16_t helperClass = AppendClassClassCp(cpAdditions, nextIndex, helperNameUtf8);
    uint16_t dispatchNameUtf8 = AppendClassUtf8Cp(cpAdditions, nextIndex, "a");
    uint16_t dispatchDescUtf8 = AppendClassUtf8Cp(cpAdditions, nextIndex, dispatchDescriptor.c_str());
    uint16_t dispatchNameAndType = AppendClassNameAndTypeCp(cpAdditions, nextIndex, dispatchNameUtf8, dispatchDescUtf8);
    uint16_t dispatchMethodRef = AppendClassMethodRefCp(cpAdditions, nextIndex, helperClass, dispatchNameAndType);

    patchedBytes.insert(patchedBytes.begin() + (ptrdiff_t)cpOffset, cpAdditions.begin(), cpAdditions.end());
    if (!WriteClassU2At(patchedBytes, 8, nextIndex)) return false;

    size_t offset = 0;
    if (!ReadClassConstantPoolUtf8(patchedBytes, offset, constantPoolCount, utf8Constants)) {
        DebugLog("Name display hook invalid patched bln.class constant pool");
        return false;
    }

    if (!AdvanceClassBytes(patchedBytes, offset, 6)) return false;

    uint16_t interfacesCount = 0;
    if (!ReadClassU2(patchedBytes, offset, interfacesCount) ||
        !AdvanceClassBytes(patchedBytes, offset, (size_t)interfacesCount * 2)) {
        return false;
    }

    uint16_t fieldsCount = 0;
    if (!ReadClassU2(patchedBytes, offset, fieldsCount)) return false;
    for (uint16_t i = 0; i < fieldsCount; ++i) {
        if (!SkipClassMemberInfo(patchedBytes, offset)) return false;
    }

    uint16_t methodsCount = 0;
    if (!ReadClassU2(patchedBytes, offset, methodsCount)) return false;
    for (uint16_t i = 0; i < methodsCount; ++i) {
        uint16_t accessFlags = 0;
        uint16_t nameIndex = 0;
        uint16_t descriptorIndex = 0;
        uint16_t attributesCount = 0;
        if (!ReadClassU2(patchedBytes, offset, accessFlags) ||
            !ReadClassU2(patchedBytes, offset, nameIndex) ||
            !ReadClassU2(patchedBytes, offset, descriptorIndex) ||
            !ReadClassU2(patchedBytes, offset, attributesCount)) {
            return false;
        }

        const std::string methodName =
            nameIndex < utf8Constants.size() ? utf8Constants[(size_t)nameIndex] : std::string();
        const std::string descriptor =
            descriptorIndex < utf8Constants.size() ? utf8Constants[(size_t)descriptorIndex] : std::string();

        for (uint16_t attr = 0; attr < attributesCount; ++attr) {
            uint16_t attributeNameIndex = 0;
            uint32_t attributeLength = 0;
            if (!ReadClassU2(patchedBytes, offset, attributeNameIndex)) return false;
            size_t attributeLengthOffset = offset;
            if (!ReadClassU4(patchedBytes, offset, attributeLength)) return false;

            size_t attributeStart = offset;
            if (!AdvanceClassBytes(patchedBytes, offset, (size_t)attributeLength)) return false;
            size_t attributeEnd = offset;
            const std::string attributeName =
                attributeNameIndex < utf8Constants.size() ? utf8Constants[(size_t)attributeNameIndex] : std::string();

            if (methodName == targetMethodName && descriptor == targetDescriptor && attributeName == "Code") {
                size_t codeOffset = attributeStart;
                uint16_t oldMaxStack = 0;
                uint16_t oldMaxLocals = 0;
                uint32_t oldCodeLength = 0;
                if (!ReadClassU2(patchedBytes, codeOffset, oldMaxStack) ||
                    !ReadClassU2(patchedBytes, codeOffset, oldMaxLocals) ||
                    !ReadClassU4(patchedBytes, codeOffset, oldCodeLength) ||
                    codeOffset + oldCodeLength > attributeEnd) {
                    DebugLog("Name display hook unsupported bln nametag Code attribute");
                    return false;
                }

                size_t oldCodeStart = codeOffset;
                size_t oldCodeEnd = oldCodeStart + oldCodeLength;
                size_t exceptionOffset = oldCodeEnd;
                uint16_t exceptionCount = 0;
                if (!ReadClassU2(patchedBytes, exceptionOffset, exceptionCount) ||
                    exceptionOffset + ((size_t)exceptionCount * 8) > attributeEnd) {
                    return false;
                }
                size_t exceptionTableStart = exceptionOffset;
                exceptionOffset += (size_t)exceptionCount * 8;
                uint16_t codeAttributeCount = 0;
                if (!ReadClassU2(patchedBytes, exceptionOffset, codeAttributeCount)) return false;
                size_t nestedAttributeStart = exceptionOffset;

                std::vector<unsigned char> injected;
                AppendClassU1(injected, 0x2B); // aload_1 player
                AppendClassU1(injected, 0x19); AppendClassU1(injected, 0x08); // aload 8 display string
                AppendClassU1(injected, 0xB8); AppendClassU2(injected, dispatchMethodRef); // invokestatic helper.a
                AppendClassU1(injected, 0x3A); AppendClassU1(injected, 0x08); // astore 8
                const uint16_t insertLen = (uint16_t)injected.size();

                std::vector<unsigned char> codeAttribute;
                AppendClassU2(codeAttribute, oldMaxStack < 2 ? 2 : oldMaxStack);
                AppendClassU2(codeAttribute, oldMaxLocals < 9 ? 9 : oldMaxLocals);
                AppendClassU4(codeAttribute, oldCodeLength + (uint32_t)injected.size());
                codeAttribute.insert(codeAttribute.end(), injected.begin(), injected.end());
                codeAttribute.insert(codeAttribute.end(), patchedBytes.begin() + (ptrdiff_t)oldCodeStart, patchedBytes.begin() + (ptrdiff_t)oldCodeEnd);

                AppendClassU2(codeAttribute, exceptionCount);
                size_t exceptionRead = exceptionTableStart;
                for (uint16_t ex = 0; ex < exceptionCount; ++ex) {
                    uint16_t startPc = 0;
                    uint16_t endPc = 0;
                    uint16_t handlerPc = 0;
                    uint16_t catchType = 0;
                    ReadClassU2(patchedBytes, exceptionRead, startPc);
                    ReadClassU2(patchedBytes, exceptionRead, endPc);
                    ReadClassU2(patchedBytes, exceptionRead, handlerPc);
                    ReadClassU2(patchedBytes, exceptionRead, catchType);
                    AppendClassU2(codeAttribute, (uint16_t)(startPc + insertLen));
                    AppendClassU2(codeAttribute, (uint16_t)(endPc + insertLen));
                    AppendClassU2(codeAttribute, (uint16_t)(handlerPc + insertLen));
                    AppendClassU2(codeAttribute, catchType);
                }

                AppendClassU2(codeAttribute, codeAttributeCount);
                size_t nestedOffset = nestedAttributeStart;
                for (uint16_t nested = 0; nested < codeAttributeCount; ++nested) {
                    uint16_t nestedNameIndex = 0;
                    uint32_t nestedLength = 0;
                    if (!ReadClassU2(patchedBytes, nestedOffset, nestedNameIndex) ||
                        !ReadClassU4(patchedBytes, nestedOffset, nestedLength) ||
                        nestedOffset + nestedLength > attributeEnd) {
                        return false;
                    }

                    std::vector<unsigned char> payload(
                        patchedBytes.begin() + (ptrdiff_t)nestedOffset,
                        patchedBytes.begin() + (ptrdiff_t)nestedOffset + (ptrdiff_t)nestedLength);
                    nestedOffset += nestedLength;

                    std::string nestedName =
                        nestedNameIndex < utf8Constants.size() ? utf8Constants[(size_t)nestedNameIndex] : std::string();
                    if (nestedName == "StackMapTable") {
                        if (!AdjustNameTagStackMapTable(payload, insertLen)) return false;
                    }
                    else if (nestedName == "LineNumberTable") {
                        if (!AdjustNameTagLineNumberTable(payload, insertLen)) return false;
                    }
                    else if (nestedName == "LocalVariableTable" || nestedName == "LocalVariableTypeTable") {
                        if (!AdjustNameTagLocalVariableTable(payload, insertLen)) return false;
                    }

                    AppendClassU2(codeAttribute, nestedNameIndex);
                    AppendClassU4(codeAttribute, (uint32_t)payload.size());
                    codeAttribute.insert(codeAttribute.end(), payload.begin(), payload.end());
                }

                if (!WriteClassU4At(patchedBytes, attributeLengthOffset, (uint32_t)codeAttribute.size())) return false;
                patchedBytes.erase(patchedBytes.begin() + (ptrdiff_t)attributeStart, patchedBytes.begin() + (ptrdiff_t)attributeEnd);
                patchedBytes.insert(patchedBytes.begin() + (ptrdiff_t)attributeStart, codeAttribute.begin(), codeAttribute.end());

                DebugLog("Name display hook patched RenderPlayer nametag oldCodeLength=%u insert=%u",
                    (unsigned int)oldCodeLength,
                    (unsigned int)insertLen);
                return true;
            }
        }
    }

    DebugLog("Name display hook failed to find bln.a(Lbet;DDDLjava/lang/String;FD)V");
    return false;
}

bool EnsureNameTagHook(JNIEnv* env) {
    if (!env) return false;
    if (g_nameTagHookPatched) return true;
    if (g_nameTagHookFailed) return false;

    jclass renderPlayerClass = FindClassLoose(env, "bln");
    if (!renderPlayerClass || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (renderPlayerClass) env->DeleteLocalRef(renderPlayerClass);
        g_nameTagHookFailed = true;
        DebugLog("Name display hook failed finding RenderPlayer bln");
        return false;
    }

    g_nameTagRenderPlayerClass = (jclass)env->NewGlobalRef(renderPlayerClass);
    env->DeleteLocalRef(renderPlayerClass);
    if (!g_nameTagRenderPlayerClass || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        g_nameTagHookFailed = true;
        DebugLog("Name display hook failed global-ref RenderPlayer");
        return false;
    }

    if (!EnsureNameTagHelper(env) ||
        !LoadNameTagRenderPlayerClassBytes(env, g_nameTagOriginalRenderPlayerBytes) ||
        !PatchRenderPlayerNameTagMethod(g_nameTagOriginalRenderPlayerBytes, g_nameTagPatchedRenderPlayerBytes) ||
        !RedefineJavaClass(env, g_nameTagRenderPlayerClass, g_nameTagPatchedRenderPlayerBytes, "RenderPlayer name display")) {
        g_nameTagOriginalRenderPlayerBytes.clear();
        g_nameTagPatchedRenderPlayerBytes.clear();
        g_nameTagHookFailed = true;
        return false;
    }

    g_nameTagHookPatched = true;
    DebugLog("Name display hook installed");
    return true;
}

void RestoreNameTagHook(JNIEnv* env) {
    if (!env || !g_nameTagHookPatched || !g_nameTagRenderPlayerClass || g_nameTagOriginalRenderPlayerBytes.empty()) return;

    if (RedefineJavaClass(env, g_nameTagRenderPlayerClass, g_nameTagOriginalRenderPlayerBytes, "RenderPlayer name display restore")) {
        g_nameTagHookPatched = false;
        DebugLog("Name display hook restored");
    }
}

bool EnsurePublicWinsApiTabHook(JNIEnv* env) {
    if (!env) return false;
    if (IsLunarNamedClient()) return true;
    if (g_publicWinsApiTabHookPatched) return true;
    if (g_publicWinsApiTabHookFailed) return false;
    if (!EnsureNameTagHook(env) || !EnsureNameTagHelper(env)) {
        g_publicWinsApiTabHookFailed = true;
        return false;
    }

    // Badlion's Hypixel Server API renderer replaces the vanilla awh player
    // list and draws its parsed strings through net.badlion.a.apj instead.
    jclass rendererClass = FindClassLoose(env, "net/badlion/a/apj");
    if (!rendererClass || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (rendererClass) env->DeleteLocalRef(rendererClass);
        g_publicWinsApiTabHookFailed = true;
        DebugLog("Public wins API tab hook failed finding net.badlion.a.apj");
        return false;
    }

    g_publicWinsApiTabRendererClass = (jclass)env->NewGlobalRef(rendererClass);
    env->DeleteLocalRef(rendererClass);
    if (!g_publicWinsApiTabRendererClass || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        g_publicWinsApiTabHookFailed = true;
        DebugLog("Public wins API tab hook failed global-ref apj");
        return false;
    }

    if (!CaptureRuntimeClassBytes(env, g_publicWinsApiTabRendererClass,
            g_publicWinsOriginalApiTabRendererBytes, "Badlion API tab renderer apj") ||
        !PatchPublicWinsApiTabRendererMethod(
            g_publicWinsOriginalApiTabRendererBytes, g_publicWinsPatchedApiTabRendererBytes) ||
        !RedefineJavaClass(env, g_publicWinsApiTabRendererClass,
            g_publicWinsPatchedApiTabRendererBytes, "Badlion API tab renderer Public wins")) {
        g_publicWinsOriginalApiTabRendererBytes.clear();
        g_publicWinsPatchedApiTabRendererBytes.clear();
        g_publicWinsApiTabHookFailed = true;
        return false;
    }

    g_publicWinsApiTabHookPatched = true;
    DebugLog("Public wins API tab hook installed");
    return true;
}

bool LoadPublicWinsTabOverlayClassBytes(JNIEnv* env, std::vector<unsigned char>& outBytes) {
    return CaptureRuntimeClassBytes(env, g_publicWinsTabOverlayClass, outBytes, "GuiPlayerTabOverlay awh");
}

bool PatchPublicWinsTabNameMethod(const std::vector<unsigned char>& originalBytes, std::vector<unsigned char>& patchedBytes) {
    patchedBytes = originalBytes;
    const char* owner = "net/minecraft/client/gui/GuiPlayerTabOverlay";
    const char* mappedMethodName = IsLunarNamedClient()
        ? FindLunarMethodName(owner, "a", "(Lbdc;)Ljava/lang/String;")
        : nullptr;
    const std::string targetMethodName = mappedMethodName ? mappedMethodName : "a";
    const std::string targetDescriptor = TranslateLunarDescriptor("(Lbdc;)Ljava/lang/String;");
    const std::string dispatchDescriptor = TranslateLunarDescriptor("(Lbdc;)Ljava/lang/String;");
    size_t cpOffset = 0;
    uint16_t constantPoolCount = 0;
    std::vector<std::string> utf8Constants;
    if (!ReadClassConstantPoolUtf8(patchedBytes, cpOffset, constantPoolCount, utf8Constants) ||
        constantPoolCount > 65520) return false;

    std::vector<unsigned char> cpAdditions;
    uint16_t nextIndex = constantPoolCount;
    uint16_t helperNameUtf8 = AppendClassUtf8Cp(cpAdditions, nextIndex, "TntTagNameDisplayHookV2");
    uint16_t helperClass = AppendClassClassCp(cpAdditions, nextIndex, helperNameUtf8);
    uint16_t dispatchNameUtf8 = AppendClassUtf8Cp(cpAdditions, nextIndex, "b");
    uint16_t dispatchDescUtf8 = AppendClassUtf8Cp(cpAdditions, nextIndex, dispatchDescriptor.c_str());
    uint16_t dispatchNameAndType = AppendClassNameAndTypeCp(cpAdditions, nextIndex, dispatchNameUtf8, dispatchDescUtf8);
    uint16_t dispatchMethodRef = AppendClassMethodRefCp(cpAdditions, nextIndex, helperClass, dispatchNameAndType);
    patchedBytes.insert(patchedBytes.begin() + (ptrdiff_t)cpOffset, cpAdditions.begin(), cpAdditions.end());
    if (!WriteClassU2At(patchedBytes, 8, nextIndex)) return false;

    size_t offset = 0;
    if (!ReadClassConstantPoolUtf8(patchedBytes, offset, constantPoolCount, utf8Constants) ||
        !AdvanceClassBytes(patchedBytes, offset, 6)) return false;
    uint16_t interfacesCount = 0;
    if (!ReadClassU2(patchedBytes, offset, interfacesCount) ||
        !AdvanceClassBytes(patchedBytes, offset, (size_t)interfacesCount * 2)) return false;
    uint16_t fieldsCount = 0;
    if (!ReadClassU2(patchedBytes, offset, fieldsCount)) return false;
    for (uint16_t i = 0; i < fieldsCount; ++i) if (!SkipClassMemberInfo(patchedBytes, offset)) return false;

    uint16_t methodsCount = 0;
    if (!ReadClassU2(patchedBytes, offset, methodsCount)) return false;
    for (uint16_t i = 0; i < methodsCount; ++i) {
        uint16_t accessFlags = 0;
        uint16_t nameIndex = 0;
        uint16_t descriptorIndex = 0;
        uint16_t attributesCount = 0;
        if (!ReadClassU2(patchedBytes, offset, accessFlags) ||
            !ReadClassU2(patchedBytes, offset, nameIndex) ||
            !ReadClassU2(patchedBytes, offset, descriptorIndex) ||
            !ReadClassU2(patchedBytes, offset, attributesCount)) return false;
        std::string methodName = nameIndex < utf8Constants.size() ? utf8Constants[nameIndex] : std::string();
        std::string descriptor = descriptorIndex < utf8Constants.size() ? utf8Constants[descriptorIndex] : std::string();

        for (uint16_t attr = 0; attr < attributesCount; ++attr) {
            uint16_t attributeNameIndex = 0;
            uint32_t attributeLength = 0;
            if (!ReadClassU2(patchedBytes, offset, attributeNameIndex)) return false;
            size_t attributeLengthOffset = offset;
            if (!ReadClassU4(patchedBytes, offset, attributeLength)) return false;
            size_t attributeStart = offset;
            if (!AdvanceClassBytes(patchedBytes, offset, attributeLength)) return false;
            size_t attributeEnd = offset;
            std::string attributeName = attributeNameIndex < utf8Constants.size()
                ? utf8Constants[attributeNameIndex] : std::string();

            if (methodName == targetMethodName && descriptor == targetDescriptor && attributeName == "Code") {
                size_t codeOffset = attributeStart;
                uint16_t oldMaxStack = 0;
                uint16_t oldMaxLocals = 0;
                uint32_t oldCodeLength = 0;
                if (!ReadClassU2(patchedBytes, codeOffset, oldMaxStack) ||
                    !ReadClassU2(patchedBytes, codeOffset, oldMaxLocals) ||
                    !ReadClassU4(patchedBytes, codeOffset, oldCodeLength)) return false;

                std::vector<unsigned char> codeAttribute;
                AppendClassU2(codeAttribute, 1);
                AppendClassU2(codeAttribute, oldMaxLocals < 2 ? 2 : oldMaxLocals);
                AppendClassU4(codeAttribute, 5);
                AppendClassU1(codeAttribute, 0x2B); // aload_1 NetworkPlayerInfo
                AppendClassU1(codeAttribute, 0xB8); AppendClassU2(codeAttribute, dispatchMethodRef);
                AppendClassU1(codeAttribute, 0xB0); // areturn
                AppendClassU2(codeAttribute, 0); // exception table
                AppendClassU2(codeAttribute, 0); // nested attributes

                if (!WriteClassU4At(patchedBytes, attributeLengthOffset, (uint32_t)codeAttribute.size())) return false;
                patchedBytes.erase(patchedBytes.begin() + (ptrdiff_t)attributeStart,
                    patchedBytes.begin() + (ptrdiff_t)attributeEnd);
                patchedBytes.insert(patchedBytes.begin() + (ptrdiff_t)attributeStart,
                    codeAttribute.begin(), codeAttribute.end());
                return true;
            }
        }
    }
    DebugLog("Public wins tab hook failed to find awh.a(Lbdc;)Ljava/lang/String;");
    return false;
}

bool PatchPublicWinsScorePlayerTeamFormatMethod(
    const std::vector<unsigned char>& originalBytes,
    std::vector<unsigned char>& patchedBytes) {
    patchedBytes = originalBytes;
    const char* owner = "net/minecraft/scoreboard/ScorePlayerTeam";
    const char* mappedMethodName = IsLunarNamedClient()
        ? FindLunarMethodName(owner, "a", "(Lauq;Ljava/lang/String;)Ljava/lang/String;")
        : nullptr;
    const std::string targetMethodName = mappedMethodName ? mappedMethodName : "a";
    const std::string targetDescriptor = TranslateLunarDescriptor(
        "(Lauq;Ljava/lang/String;)Ljava/lang/String;");
    const std::string dispatchDescriptor = TranslateLunarDescriptor(
        "(Lauq;Ljava/lang/String;)Ljava/lang/String;");
    size_t cpOffset = 0;
    uint16_t constantPoolCount = 0;
    std::vector<std::string> utf8Constants;
    if (!ReadClassConstantPoolUtf8(patchedBytes, cpOffset, constantPoolCount, utf8Constants) ||
        constantPoolCount > 65520) return false;

    std::vector<unsigned char> cpAdditions;
    uint16_t nextIndex = constantPoolCount;
    uint16_t helperNameUtf8 = AppendClassUtf8Cp(cpAdditions, nextIndex, "TntTagScoreFormatHookV3");
    uint16_t helperClass = AppendClassClassCp(cpAdditions, nextIndex, helperNameUtf8);
    uint16_t dispatchNameUtf8 = AppendClassUtf8Cp(cpAdditions, nextIndex, "d");
    uint16_t dispatchDescUtf8 = AppendClassUtf8Cp(
        cpAdditions, nextIndex, dispatchDescriptor.c_str());
    uint16_t dispatchNameAndType = AppendClassNameAndTypeCp(
        cpAdditions, nextIndex, dispatchNameUtf8, dispatchDescUtf8);
    uint16_t dispatchMethodRef = AppendClassMethodRefCp(
        cpAdditions, nextIndex, helperClass, dispatchNameAndType);
    patchedBytes.insert(
        patchedBytes.begin() + (ptrdiff_t)cpOffset, cpAdditions.begin(), cpAdditions.end());
    if (!WriteClassU2At(patchedBytes, 8, nextIndex)) return false;

    size_t offset = 0;
    if (!ReadClassConstantPoolUtf8(patchedBytes, offset, constantPoolCount, utf8Constants) ||
        !AdvanceClassBytes(patchedBytes, offset, 6)) return false;
    uint16_t interfacesCount = 0;
    if (!ReadClassU2(patchedBytes, offset, interfacesCount) ||
        !AdvanceClassBytes(patchedBytes, offset, (size_t)interfacesCount * 2)) return false;
    uint16_t fieldsCount = 0;
    if (!ReadClassU2(patchedBytes, offset, fieldsCount)) return false;
    for (uint16_t i = 0; i < fieldsCount; ++i) {
        if (!SkipClassMemberInfo(patchedBytes, offset)) return false;
    }

    uint16_t methodsCount = 0;
    if (!ReadClassU2(patchedBytes, offset, methodsCount)) return false;
    for (uint16_t i = 0; i < methodsCount; ++i) {
        uint16_t accessFlags = 0;
        uint16_t nameIndex = 0;
        uint16_t descriptorIndex = 0;
        uint16_t attributesCount = 0;
        if (!ReadClassU2(patchedBytes, offset, accessFlags) ||
            !ReadClassU2(patchedBytes, offset, nameIndex) ||
            !ReadClassU2(patchedBytes, offset, descriptorIndex) ||
            !ReadClassU2(patchedBytes, offset, attributesCount)) return false;
        std::string methodName = nameIndex < utf8Constants.size()
            ? utf8Constants[nameIndex] : std::string();
        std::string descriptor = descriptorIndex < utf8Constants.size()
            ? utf8Constants[descriptorIndex] : std::string();

        for (uint16_t attr = 0; attr < attributesCount; ++attr) {
            uint16_t attributeNameIndex = 0;
            uint32_t attributeLength = 0;
            if (!ReadClassU2(patchedBytes, offset, attributeNameIndex)) return false;
            size_t attributeLengthOffset = offset;
            if (!ReadClassU4(patchedBytes, offset, attributeLength)) return false;
            size_t attributeStart = offset;
            if (!AdvanceClassBytes(patchedBytes, offset, attributeLength)) return false;
            std::string attributeName = attributeNameIndex < utf8Constants.size()
                ? utf8Constants[attributeNameIndex] : std::string();

            if (methodName == targetMethodName &&
                descriptor == targetDescriptor &&
                attributeName == "Code") {
                size_t codeOffset = attributeStart;
                uint16_t maxStack = 0;
                uint16_t maxLocals = 0;
                uint32_t oldCodeLength = 0;
                if (!ReadClassU2(patchedBytes, codeOffset, maxStack) ||
                    !ReadClassU2(patchedBytes, codeOffset, maxLocals) ||
                    !ReadClassU4(patchedBytes, codeOffset, oldCodeLength)) return false;

                std::vector<unsigned char> codeAttribute;
                AppendClassU2(codeAttribute, 2);
                AppendClassU2(codeAttribute, maxLocals < 2 ? 2 : maxLocals);
                AppendClassU4(codeAttribute, 6);
                AppendClassU1(codeAttribute, 0x2A); // aload_0 ScorePlayerTeam/base team
                AppendClassU1(codeAttribute, 0x2B); // aload_1 username
                AppendClassU1(codeAttribute, 0xB8); AppendClassU2(codeAttribute, dispatchMethodRef);
                AppendClassU1(codeAttribute, 0xB0); // areturn
                AppendClassU2(codeAttribute, 0); // exception table
                AppendClassU2(codeAttribute, 0); // nested attributes

                if (!WriteClassU4At(patchedBytes, attributeLengthOffset,
                        (uint32_t)codeAttribute.size())) return false;
                size_t attributeEnd = attributeStart + attributeLength;
                patchedBytes.erase(
                    patchedBytes.begin() + (ptrdiff_t)attributeStart,
                    patchedBytes.begin() + (ptrdiff_t)attributeEnd);
                patchedBytes.insert(
                    patchedBytes.begin() + (ptrdiff_t)attributeStart,
                    codeAttribute.begin(), codeAttribute.end());
                DebugLog("Public wins scoreboard format hook replaced aul.a(auq,String) oldCodeLength=%u",
                    (unsigned int)oldCodeLength);
                return true;
            }
        }
    }

    DebugLog("Public wins scoreboard format hook failed to find aul.a(auq,String)");
    return false;
}

bool PatchPublicWinsLivingRendererNameRead(
    const std::vector<unsigned char>& originalBytes,
    std::vector<unsigned char>& patchedBytes) {
    patchedBytes = originalBytes;
    const char* owner = "net/minecraft/client/renderer/entity/RendererLivingEntity";
    const char* mappedMethodName = IsLunarNamedClient()
        ? FindLunarMethodName(owner, "b", "(Lpr;DDD)V")
        : nullptr;
    const std::string targetMethodName = mappedMethodName ? mappedMethodName : "b";
    const std::string targetDescriptor = TranslateLunarDescriptor("(Lpr;DDD)V");
    const bool lunarClient = IsLunarNamedClient();
    const std::string dispatchDescriptor = TranslateLunarDescriptor(
        lunarClient ? "(Lpr;Leu;)Ljava/lang/String;" : "(Lpr;)Ljava/lang/String;");
    std::vector<uint16_t> cachedFormattedNameRefs;
    if (!lunarClient && !FindClassMethodRefs(
            originalBytes, 10, "getCachedFormattedName", "()Ljava/lang/String;",
            cachedFormattedNameRefs)) {
        DebugLog("Public wins rendered-name hook could not resolve pr.getCachedFormattedName call");
        return false;
    }
    size_t cpOffset = 0;
    uint16_t constantPoolCount = 0;
    std::vector<std::string> utf8Constants;
    if (!ReadClassConstantPoolUtf8(patchedBytes, cpOffset, constantPoolCount, utf8Constants) ||
        constantPoolCount > 65520) return false;

    std::vector<unsigned char> cpAdditions;
    uint16_t nextIndex = constantPoolCount;
    uint16_t helperNameUtf8 = AppendClassUtf8Cp(
        cpAdditions, nextIndex,
        lunarClient ? "TntTagRenderedComponentHookV6" : "TntTagRenderedNameHookV4");
    uint16_t helperClass = AppendClassClassCp(cpAdditions, nextIndex, helperNameUtf8);
    uint16_t dispatchNameUtf8 = AppendClassUtf8Cp(
        cpAdditions, nextIndex, lunarClient ? "f" : "e");
    uint16_t dispatchDescUtf8 = AppendClassUtf8Cp(
        cpAdditions, nextIndex, dispatchDescriptor.c_str());
    uint16_t dispatchNameAndType = AppendClassNameAndTypeCp(
        cpAdditions, nextIndex, dispatchNameUtf8, dispatchDescUtf8);
    uint16_t dispatchMethodRef = AppendClassMethodRefCp(
        cpAdditions, nextIndex, helperClass, dispatchNameAndType);
    patchedBytes.insert(
        patchedBytes.begin() + (ptrdiff_t)cpOffset, cpAdditions.begin(), cpAdditions.end());
    if (!WriteClassU2At(patchedBytes, 8, nextIndex)) return false;

    size_t offset = 0;
    if (!ReadClassConstantPoolUtf8(patchedBytes, offset, constantPoolCount, utf8Constants) ||
        !AdvanceClassBytes(patchedBytes, offset, 6)) return false;
    uint16_t interfacesCount = 0;
    if (!ReadClassU2(patchedBytes, offset, interfacesCount) ||
        !AdvanceClassBytes(patchedBytes, offset, (size_t)interfacesCount * 2)) return false;
    uint16_t fieldsCount = 0;
    if (!ReadClassU2(patchedBytes, offset, fieldsCount)) return false;
    for (uint16_t i = 0; i < fieldsCount; ++i) {
        if (!SkipClassMemberInfo(patchedBytes, offset)) return false;
    }

    uint16_t methodsCount = 0;
    bool foundCandidateMethod = false;
    if (!ReadClassU2(patchedBytes, offset, methodsCount)) return false;
    for (uint16_t i = 0; i < methodsCount; ++i) {
        uint16_t accessFlags = 0;
        uint16_t nameIndex = 0;
        uint16_t descriptorIndex = 0;
        uint16_t attributesCount = 0;
        if (!ReadClassU2(patchedBytes, offset, accessFlags) ||
            !ReadClassU2(patchedBytes, offset, nameIndex) ||
            !ReadClassU2(patchedBytes, offset, descriptorIndex) ||
            !ReadClassU2(patchedBytes, offset, attributesCount)) return false;
        std::string methodName = nameIndex < utf8Constants.size()
            ? utf8Constants[nameIndex] : std::string();
        std::string descriptor = descriptorIndex < utf8Constants.size()
            ? utf8Constants[descriptorIndex] : std::string();

        for (uint16_t attr = 0; attr < attributesCount; ++attr) {
            uint16_t attributeNameIndex = 0;
            uint32_t attributeLength = 0;
            if (!ReadClassU2(patchedBytes, offset, attributeNameIndex) ||
                !ReadClassU4(patchedBytes, offset, attributeLength)) return false;
            size_t attributeStart = offset;
            if (!AdvanceClassBytes(patchedBytes, offset, attributeLength)) return false;
            std::string attributeName = attributeNameIndex < utf8Constants.size()
                ? utf8Constants[attributeNameIndex] : std::string();

            const bool isLunarWrappedRenderName = IsLunarNamedClient() &&
                methodName.rfind("renderName$mixinextras$wrapped$", 0) == 0;
            if ((methodName != targetMethodName && !isLunarWrappedRenderName) ||
                descriptor != targetDescriptor || attributeName != "Code") {
                continue;
            }
            foundCandidateMethod = true;
            size_t codeOffset = attributeStart;
            uint16_t maxStack = 0;
            uint16_t maxLocals = 0;
            uint32_t codeLength = 0;
            if (!ReadClassU2(patchedBytes, codeOffset, maxStack) ||
                !ReadClassU2(patchedBytes, codeOffset, maxLocals) ||
                !ReadClassU4(patchedBytes, codeOffset, codeLength) ||
                codeOffset + codeLength > patchedBytes.size()) return false;

            for (uint32_t codeIndex = 0; codeIndex + 4 < codeLength; ++codeIndex) {
                size_t p = codeOffset + codeIndex;
                if (!lunarClient && patchedBytes[p] == 0xB6) {
                    uint16_t methodRef = (uint16_t)(
                        ((uint16_t)patchedBytes[p + 1] << 8) | patchedBytes[p + 2]);
                    if (std::find(cachedFormattedNameRefs.begin(), cachedFormattedNameRefs.end(), methodRef) ==
                        cachedFormattedNameRefs.end()) {
                        continue;
                    }

                    // Badlion already has the player on the stack for
                    // getCachedFormattedName(). Redirect that call to the
                    // player-aware native decorator and preserve the following
                    // nickname-filter call.
                    patchedBytes[p] = 0xB8;
                    patchedBytes[p + 1] = (unsigned char)((dispatchMethodRef >> 8) & 0xFF);
                    patchedBytes[p + 2] = (unsigned char)(dispatchMethodRef & 0xFF);
                    DebugLog("Public wins rendered-name hook patched Badlion cached name offset=%u",
                        (unsigned int)codeIndex);
                    return true;
                }

                if (!lunarClient) continue;
                if (patchedBytes[p] != 0xB9 ||
                    patchedBytes[p + 3] != 0x01 ||
                    patchedBytes[p + 4] != 0x00) continue;

                // Stack starts with the IChatComponent. Add method-local entity
                // argument 1, swap to (entity, component), then call the helper.
                patchedBytes[p] = 0x2B; // aload_1
                patchedBytes[p + 1] = 0x5F; // swap
                patchedBytes[p + 2] = 0xB8; // invokestatic helper.f(entity, component)
                patchedBytes[p + 3] = (unsigned char)((dispatchMethodRef >> 8) & 0xFF);
                patchedBytes[p + 4] = (unsigned char)(dispatchMethodRef & 0xFF);
                DebugLog("Public wins rendered-component hook patched bjl.b invokeinterface offset=%u",
                    (unsigned int)codeIndex);
                return true;
            }
            DebugLog("Public wins rendered-name hook candidate %s had no supported name call",
                methodName.c_str());
        }
    }
    DebugLog(foundCandidateMethod
        ? "Public wins rendered-name hook exhausted candidates without supported name call"
        : "Public wins rendered-name hook failed to find bjl.b(Lpr;DDD)V");
    return false;
}

bool PatchLunarAdventureRenderName(
    const std::vector<unsigned char>& originalBytes,
    std::vector<unsigned char>& patchedBytes) {
    patchedBytes = originalBytes;
    if (!IsLunarNamedClient()) return true;

    std::vector<uint16_t> displayComponentRefs;
    if (!FindClassInterfaceMethodRefs(
            originalBytes,
            "bridge$getDisplayNameComponent",
            "()Lnet/kyori/adventure/text/Component;",
            displayComponentRefs)) {
        DebugLog("Lunar Adventure nametag hook could not resolve display-component interface call");
        return false;
    }

    size_t cpOffset = 0;
    uint16_t constantPoolCount = 0;
    std::vector<std::string> utf8Constants;
    if (!ReadClassConstantPoolUtf8(patchedBytes, cpOffset, constantPoolCount, utf8Constants) ||
        constantPoolCount > 65520) return false;

    std::vector<unsigned char> cpAdditions;
    uint16_t nextIndex = constantPoolCount;
    uint16_t helperNameUtf8 = AppendClassUtf8Cp(
        cpAdditions, nextIndex, "TntTagLunarAdventureNameHookV1");
    uint16_t helperClass = AppendClassClassCp(cpAdditions, nextIndex, helperNameUtf8);
    uint16_t dispatchNameUtf8 = AppendClassUtf8Cp(cpAdditions, nextIndex, "f");
    uint16_t dispatchDescUtf8 = AppendClassUtf8Cp(
        cpAdditions,
        nextIndex,
        "(Lnet/minecraft/entity/EntityLivingBase;)Lnet/kyori/adventure/text/Component;");
    uint16_t dispatchNameAndType = AppendClassNameAndTypeCp(
        cpAdditions, nextIndex, dispatchNameUtf8, dispatchDescUtf8);
    uint16_t dispatchMethodRef = AppendClassMethodRefCp(
        cpAdditions, nextIndex, helperClass, dispatchNameAndType);
    patchedBytes.insert(
        patchedBytes.begin() + (ptrdiff_t)cpOffset, cpAdditions.begin(), cpAdditions.end());
    if (!WriteClassU2At(patchedBytes, 8, nextIndex)) return false;

    size_t offset = 0;
    if (!ReadClassConstantPoolUtf8(patchedBytes, offset, constantPoolCount, utf8Constants) ||
        !AdvanceClassBytes(patchedBytes, offset, 6)) return false;
    uint16_t interfacesCount = 0;
    if (!ReadClassU2(patchedBytes, offset, interfacesCount) ||
        !AdvanceClassBytes(patchedBytes, offset, (size_t)interfacesCount * 2)) return false;
    uint16_t fieldsCount = 0;
    if (!ReadClassU2(patchedBytes, offset, fieldsCount)) return false;
    for (uint16_t i = 0; i < fieldsCount; ++i) {
        if (!SkipClassMemberInfo(patchedBytes, offset)) return false;
    }

    uint16_t methodsCount = 0;
    if (!ReadClassU2(patchedBytes, offset, methodsCount)) return false;
    bool foundWrapper = false;
    int patchedCalls = 0;
    for (uint16_t i = 0; i < methodsCount; ++i) {
        uint16_t accessFlags = 0;
        uint16_t nameIndex = 0;
        uint16_t descriptorIndex = 0;
        uint16_t attributesCount = 0;
        if (!ReadClassU2(patchedBytes, offset, accessFlags) ||
            !ReadClassU2(patchedBytes, offset, nameIndex) ||
            !ReadClassU2(patchedBytes, offset, descriptorIndex) ||
            !ReadClassU2(patchedBytes, offset, attributesCount)) return false;
        std::string methodName = nameIndex < utf8Constants.size()
            ? utf8Constants[nameIndex] : std::string();

        for (uint16_t attr = 0; attr < attributesCount; ++attr) {
            uint16_t attributeNameIndex = 0;
            uint32_t attributeLength = 0;
            if (!ReadClassU2(patchedBytes, offset, attributeNameIndex) ||
                !ReadClassU4(patchedBytes, offset, attributeLength)) return false;
            size_t attributeStart = offset;
            if (!AdvanceClassBytes(patchedBytes, offset, attributeLength)) return false;
            std::string attributeName = attributeNameIndex < utf8Constants.size()
                ? utf8Constants[attributeNameIndex] : std::string();
            const bool isAdventureRenderWrapper =
                methodName.rfind("wrapMethod$", 0) == 0 &&
                methodName.find("$renderName") != std::string::npos;
            if (!isAdventureRenderWrapper || attributeName != "Code") continue;
            foundWrapper = true;

            size_t codeOffset = attributeStart;
            uint16_t maxStack = 0;
            uint16_t maxLocals = 0;
            uint32_t codeLength = 0;
            if (!ReadClassU2(patchedBytes, codeOffset, maxStack) ||
                !ReadClassU2(patchedBytes, codeOffset, maxLocals) ||
                !ReadClassU4(patchedBytes, codeOffset, codeLength) ||
                codeOffset + codeLength > patchedBytes.size()) return false;

            for (uint32_t codeIndex = 0; codeIndex + 4 < codeLength; ++codeIndex) {
                size_t p = codeOffset + codeIndex;
                if (patchedBytes[p] != 0xB9 || patchedBytes[p + 3] != 0x01 ||
                    patchedBytes[p + 4] != 0x00) continue;
                uint16_t interfaceRef = (uint16_t)(
                    ((uint16_t)patchedBytes[p + 1] << 8) | patchedBytes[p + 2]);
                if (std::find(displayComponentRefs.begin(), displayComponentRefs.end(), interfaceRef) ==
                    displayComponentRefs.end()) continue;

                // Lunar casts EntityLivingBase to its private bridge interface
                // immediately before this call. The native helper consumes the
                // original EntityLivingBase, so remove that now-redundant cast
                // to keep the verifier's stack type compatible with the helper.
                if (codeIndex < 3 || patchedBytes[p - 3] != 0xC0) {
                    DebugLog("Lunar Adventure nametag hook missing bridge checkcast offset=%u",
                        (unsigned int)codeIndex);
                    return false;
                }
                patchedBytes[p - 3] = 0x00;
                patchedBytes[p - 2] = 0x00;
                patchedBytes[p - 1] = 0x00;

                patchedBytes[p] = 0xB8;
                patchedBytes[p + 1] = (unsigned char)((dispatchMethodRef >> 8) & 0xFF);
                patchedBytes[p + 2] = (unsigned char)(dispatchMethodRef & 0xFF);
                patchedBytes[p + 3] = 0x00;
                patchedBytes[p + 4] = 0x00;
                ++patchedCalls;
                DebugLog("Lunar Adventure nametag hook patched display component offset=%u",
                    (unsigned int)codeIndex);
            }
        }
    }

    if (!foundWrapper || patchedCalls == 0) {
        DebugLog("Lunar Adventure nametag hook failed wrapper=%d calls=%d",
            foundWrapper ? 1 : 0, patchedCalls);
        return false;
    }
    DebugLog("Lunar Adventure nametag hook patched calls=%d", patchedCalls);
    return true;
}

bool EnsurePublicWinsRenderedNameHook(JNIEnv* env) {
    if (!env) return false;
    if (g_publicWinsRenderedNameHookPatched) return true;
    if (g_publicWinsRenderedNameHookFailed) return false;

    jclass rendererClass = FindClassLoose(env, "bjl");
    if (!rendererClass || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (rendererClass) env->DeleteLocalRef(rendererClass);
        g_publicWinsRenderedNameHookFailed = true;
        DebugLog("Public wins rendered-name hook failed finding bjl");
        return false;
    }
    g_publicWinsLivingRendererClass = (jclass)env->NewGlobalRef(rendererClass);
    env->DeleteLocalRef(rendererClass);
    if (!g_publicWinsLivingRendererClass || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        g_publicWinsRenderedNameHookFailed = true;
        return false;
    }

    bool hookReady = IsLunarNamedClient()
        ? EnsurePublicWinsRenderedComponentHelper(env)
        : EnsurePublicWinsRenderedNameHelper(env);
    if (hookReady && IsLunarNamedClient()) {
        hookReady = EnsureLunarAdventureNameHelper(env);
    }
    if (hookReady) {
        hookReady = CaptureRuntimeClassBytes(env, g_publicWinsLivingRendererClass,
            g_publicWinsOriginalLivingRendererBytes, "RendererLivingEntity bjl");
    }
    if (hookReady) {
        hookReady = PatchPublicWinsLivingRendererNameRead(
            g_publicWinsOriginalLivingRendererBytes, g_publicWinsPatchedLivingRendererBytes);
    }
    if (hookReady && IsLunarNamedClient()) {
        std::vector<unsigned char> adventurePatchedBytes;
        hookReady = PatchLunarAdventureRenderName(
            g_publicWinsPatchedLivingRendererBytes, adventurePatchedBytes);
        if (hookReady) g_publicWinsPatchedLivingRendererBytes.swap(adventurePatchedBytes);
    }
    if (hookReady) {
        hookReady = RedefineJavaClass(env, g_publicWinsLivingRendererClass,
            g_publicWinsPatchedLivingRendererBytes, "RendererLivingEntity Public wins");
    }
    if (!hookReady) {
        g_publicWinsOriginalLivingRendererBytes.clear();
        g_publicWinsPatchedLivingRendererBytes.clear();
        g_publicWinsRenderedNameHookFailed = true;
        return false;
    }
    g_publicWinsRenderedNameHookPatched = true;
    DebugLog("Public wins rendered-name hook installed");
    return true;
}

bool EnsurePublicWinsScoreboardFormatHook(JNIEnv* env) {
    if (!env) return false;
    if (g_publicWinsScoreboardFormatHookPatched) return true;
    if (g_publicWinsScoreboardFormatHookFailed) return false;

    jclass teamClass = FindClassLoose(env, "aul");
    if (!teamClass || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (teamClass) env->DeleteLocalRef(teamClass);
        g_publicWinsScoreboardFormatHookFailed = true;
        DebugLog("Public wins scoreboard format hook failed finding aul");
        return false;
    }
    g_publicWinsScorePlayerTeamClass = (jclass)env->NewGlobalRef(teamClass);
    env->DeleteLocalRef(teamClass);
    if (!g_publicWinsScorePlayerTeamClass || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        g_publicWinsScoreboardFormatHookFailed = true;
        DebugLog("Public wins scoreboard format hook failed global-ref aul");
        return false;
    }

    if (!EnsurePublicWinsScoreFormatHelper(env) ||
        !CaptureRuntimeClassBytes(env, g_publicWinsScorePlayerTeamClass,
            g_publicWinsOriginalScorePlayerTeamBytes, "ScorePlayerTeam aul") ||
        !PatchPublicWinsScorePlayerTeamFormatMethod(
            g_publicWinsOriginalScorePlayerTeamBytes, g_publicWinsPatchedScorePlayerTeamBytes) ||
        !RedefineJavaClass(env, g_publicWinsScorePlayerTeamClass,
            g_publicWinsPatchedScorePlayerTeamBytes, "ScorePlayerTeam Public wins")) {
        g_publicWinsOriginalScorePlayerTeamBytes.clear();
        g_publicWinsPatchedScorePlayerTeamBytes.clear();
        g_publicWinsScoreboardFormatHookFailed = true;
        return false;
    }

    g_publicWinsScoreboardFormatHookPatched = true;
    DebugLog("Public wins scoreboard format hook installed");
    return true;
}

bool EnsurePublicWinsTabNameHook(JNIEnv* env) {
    if (!env) return false;
    if (g_publicWinsTabHookPatched) return true;
    if (g_publicWinsTabHookFailed) return false;
    if (!EnsureNameTagHook(env) || !EnsureNameTagHelper(env)) {
        g_publicWinsTabHookFailed = true;
        return false;
    }

    jclass tabClass = FindClassLoose(env, "awh");
    if (!tabClass || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (tabClass) env->DeleteLocalRef(tabClass);
        g_publicWinsTabHookFailed = true;
        DebugLog("Public wins tab hook failed finding GuiPlayerTabOverlay awh");
        return false;
    }
    g_publicWinsTabOverlayClass = (jclass)env->NewGlobalRef(tabClass);
    env->DeleteLocalRef(tabClass);
    if (!g_publicWinsTabOverlayClass || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        g_publicWinsTabHookFailed = true;
        return false;
    }

    if (!LoadPublicWinsTabOverlayClassBytes(env, g_publicWinsOriginalTabOverlayBytes) ||
        !PatchPublicWinsTabNameMethod(g_publicWinsOriginalTabOverlayBytes, g_publicWinsPatchedTabOverlayBytes) ||
        !RedefineJavaClass(env, g_publicWinsTabOverlayClass, g_publicWinsPatchedTabOverlayBytes, "GuiPlayerTabOverlay Public wins")) {
        g_publicWinsOriginalTabOverlayBytes.clear();
        g_publicWinsPatchedTabOverlayBytes.clear();
        g_publicWinsTabHookFailed = true;
        return false;
    }
    g_publicWinsTabHookPatched = true;
    DebugLog("Public wins tab hook installed");
    return true;
}

void RestorePublicWinsTabNameHook(JNIEnv* env) {
    if (!env) return;

    if (g_publicWinsRenderedNameHookPatched && g_publicWinsLivingRendererClass &&
        !g_publicWinsOriginalLivingRendererBytes.empty() &&
        RedefineJavaClass(env, g_publicWinsLivingRendererClass,
            g_publicWinsOriginalLivingRendererBytes, "RendererLivingEntity Public wins restore")) {
        g_publicWinsRenderedNameHookPatched = false;
        DebugLog("Public wins rendered-name hook restored");
    }

    if (g_publicWinsScoreboardFormatHookPatched && g_publicWinsScorePlayerTeamClass &&
        !g_publicWinsOriginalScorePlayerTeamBytes.empty() &&
        RedefineJavaClass(env, g_publicWinsScorePlayerTeamClass,
            g_publicWinsOriginalScorePlayerTeamBytes, "ScorePlayerTeam Public wins restore")) {
        g_publicWinsScoreboardFormatHookPatched = false;
        DebugLog("Public wins scoreboard format hook restored");
    }

    if (g_publicWinsApiTabHookPatched && g_publicWinsApiTabRendererClass &&
        !g_publicWinsOriginalApiTabRendererBytes.empty() &&
        RedefineJavaClass(env, g_publicWinsApiTabRendererClass,
            g_publicWinsOriginalApiTabRendererBytes, "Badlion API tab renderer Public wins restore")) {
        g_publicWinsApiTabHookPatched = false;
        DebugLog("Public wins API tab hook restored");
    }

    if (g_publicWinsTabHookPatched && g_publicWinsTabOverlayClass &&
        !g_publicWinsOriginalTabOverlayBytes.empty() &&
        RedefineJavaClass(env, g_publicWinsTabOverlayClass, g_publicWinsOriginalTabOverlayBytes,
            "GuiPlayerTabOverlay Public wins restore")) {
        g_publicWinsTabHookPatched = false;
        DebugLog("Public wins tab hook restored");
    }

    // Public Wins shares the RenderPlayer name-display hook.
    RestoreNameTagHook(env);
}

bool SetBlockDamageDispatcherPatch(JNIEnv* env, bool enabled) {
    if (enabled == g_seeBarriersDamageDispatcherPatched) return true;
    if (!EnsureBlockDamageDispatcherBytecode(env)) return false;
    if (!g_seeBarriersBlockRendererDispatcherClass) return false;

    const std::vector<unsigned char>& classBytes = enabled ?
        g_seeBarriersPatchedDamageDispatcherBytes :
        g_seeBarriersOriginalDamageDispatcherBytes;
    if (!RedefineJavaClass(env, g_seeBarriersBlockRendererDispatcherClass, classBytes, "BlockRendererDispatcher")) {
        return false;
    }

    g_seeBarriersDamageDispatcherPatched = enabled;
    DebugLog("See Barriers damage overlay dispatcher patch applied=%d", enabled ? 1 : 0);
    return true;
}

bool SetRenderChunkPatch(JNIEnv* env, bool enabled) {
    if (enabled == g_seeBarriersRenderChunkPatched) return true;
    if (!EnsureRenderChunkBytecode(env)) return false;

    jclass renderChunkClass = nullptr;
    if (!GetSeeBarriersRenderChunkClass(env, renderChunkClass)) return false;

    const std::vector<unsigned char>& classBytes = enabled ?
        g_seeBarriersPatchedRenderChunkBytes :
        g_seeBarriersOriginalRenderChunkBytes;
    if (!RedefineJavaClass(env, renderChunkClass, classBytes, "RenderChunk")) {
        return false;
    }

    g_seeBarriersRenderChunkPatched = enabled;
    DebugLog("See Barriers render chunk patch applied=%d", enabled ? 1 : 0);
    return true;
}

bool EnsureSeeBarriersModelOverrideJNI(JNIEnv* env) {
    if (g_seeBarriersModelOverrideInited) return true;
    if (!env) return false;

    if ((!g_slInited || g_slFailed) && !InitSnaplookJNI()) {
        DebugLog("See Barriers model override missing Minecraft JNI");
        return false;
    }

    jclass blockClassLocal = FindClassLoose(env, "afh");
    jclass dispatcherClassLocal = FindClassLoose(env, "bgd");
    jclass blockStateContainerClassLocal = FindClassLoose(env, "ama");
    jclass modelShapesClassLocal = FindClassLoose(env, "bgc");
    jclass mapClassLocal = FindClassLoose(env, "java/util/Map");
    jclass bakedModelClassLocal = FindClassLoose(env, "boq");
    jclass listClassLocal = FindClassLoose(env, "java/util/List");
    if (!blockClassLocal || !dispatcherClassLocal || !blockStateContainerClassLocal ||
        !modelShapesClassLocal || !mapClassLocal || !bakedModelClassLocal || !listClassLocal ||
        env->ExceptionCheck()) {
        env->ExceptionClear();
        if (blockClassLocal) env->DeleteLocalRef(blockClassLocal);
        if (dispatcherClassLocal) env->DeleteLocalRef(dispatcherClassLocal);
        if (blockStateContainerClassLocal) env->DeleteLocalRef(blockStateContainerClassLocal);
        if (modelShapesClassLocal) env->DeleteLocalRef(modelShapesClassLocal);
        if (mapClassLocal) env->DeleteLocalRef(mapClassLocal);
        if (bakedModelClassLocal) env->DeleteLocalRef(bakedModelClassLocal);
        if (listClassLocal) env->DeleteLocalRef(listClassLocal);
        DebugLog("See Barriers model override class lookup failed");
        return false;
    }

    g_seeBarriersBlockClass = (jclass)env->NewGlobalRef(blockClassLocal);
    g_seeBarriersBlockRendererDispatcherClass = (jclass)env->NewGlobalRef(dispatcherClassLocal);
    g_seeBarriersBlockStateContainerClass = (jclass)env->NewGlobalRef(blockStateContainerClassLocal);
    g_seeBarriersBlockModelShapesClass = (jclass)env->NewGlobalRef(modelShapesClassLocal);
    g_seeBarriersMapClass = (jclass)env->NewGlobalRef(mapClassLocal);
    g_seeBarriersBakedModelClass = (jclass)env->NewGlobalRef(bakedModelClassLocal);
    g_seeBarriersListClass = (jclass)env->NewGlobalRef(listClassLocal);

    env->DeleteLocalRef(blockClassLocal);
    env->DeleteLocalRef(dispatcherClassLocal);
    env->DeleteLocalRef(blockStateContainerClassLocal);
    env->DeleteLocalRef(modelShapesClassLocal);
    env->DeleteLocalRef(mapClassLocal);
    env->DeleteLocalRef(bakedModelClassLocal);
    env->DeleteLocalRef(listClassLocal);

    if (!g_seeBarriersBlockClass || !g_seeBarriersBlockRendererDispatcherClass ||
        !g_seeBarriersBlockStateContainerClass || !g_seeBarriersBlockModelShapesClass ||
        !g_seeBarriersMapClass || !g_seeBarriersBakedModelClass || !g_seeBarriersListClass ||
        env->ExceptionCheck()) {
        env->ExceptionClear();
        DebugLog("See Barriers model override global-ref failed");
        return false;
    }

    auto lookupMethod = [env](jclass cls, const char* name, const char* sig, const char* label) -> jmethodID {
        jmethodID id = GetMethodIDCompat(env, cls, name, sig);
        if (!id || env->ExceptionCheck()) {
            env->ExceptionClear();
            DebugLog("See Barriers model override member lookup failed: %s", label);
            return nullptr;
        }
        return id;
    };

    auto lookupStaticMethod = [env](jclass cls, const char* name, const char* sig, const char* label) -> jmethodID {
        jmethodID id = GetStaticMethodIDCompat(env, cls, name, sig);
        if (!id || env->ExceptionCheck()) {
            env->ExceptionClear();
            DebugLog("See Barriers model override member lookup failed: %s", label);
            return nullptr;
        }
        return id;
    };

    auto lookupField = [env](jclass cls, const char* name, const char* sig, const char* label) -> jfieldID {
        jfieldID id = GetFieldIDCompat(env, cls, name, sig);
        if (!id || env->ExceptionCheck()) {
            env->ExceptionClear();
            DebugLog("See Barriers model override member lookup failed: %s", label);
            return nullptr;
        }
        return id;
    };

    g_seeBarriersMinecraftGetBlockRendererDispatcher = lookupMethod(g_slMcClass, "ae", "()Lbgd;", "ave.ae()Lbgd;");
    if (!g_seeBarriersMinecraftGetBlockRendererDispatcher) return false;

    g_seeBarriersBlockRendererGetModelShapes = lookupMethod(g_seeBarriersBlockRendererDispatcherClass, "a", "()Lbgc;", "bgd.a()Lbgc;");
    if (!g_seeBarriersBlockRendererGetModelShapes) return false;

    g_seeBarriersBlockModelShapesGetModel = lookupMethod(g_seeBarriersBlockModelShapesClass, "b", "(Lalz;)Lboq;", "bgc.b(Lalz;)Lboq;");
    if (!g_seeBarriersBlockModelShapesGetModel) return false;

    g_seeBarriersBlockModelShapesModelMap = lookupField(g_seeBarriersBlockModelShapesClass, "a", "Ljava/util/Map;", "bgc.a Ljava/util/Map;");
    if (!g_seeBarriersBlockModelShapesModelMap) return false;

    g_seeBarriersBlockGetById = lookupStaticMethod(g_seeBarriersBlockClass, "c", "(I)Lafh;", "afh.c(I)Lafh;");
    if (!g_seeBarriersBlockGetById) return false;

    g_seeBarriersBlockGetStateContainer = lookupMethod(g_seeBarriersBlockClass, "P", "()Lama;", "afh.P()Lama;");
    if (!g_seeBarriersBlockGetStateContainer) return false;

    g_seeBarriersBlockGetDefaultState = lookupMethod(g_seeBarriersBlockClass, "Q", "()Lalz;", "afh.Q()Lalz;");
    if (!g_seeBarriersBlockGetDefaultState) return false;

    g_seeBarriersBlockStateContainerGetValidStates = lookupMethod(g_seeBarriersBlockStateContainerClass, "a", "()Lcom/google/common/collect/ImmutableList;", "ama.a()LImmutableList;");
    if (!g_seeBarriersBlockStateContainerGetValidStates) return false;

    g_seeBarriersBakedModelGetGeneralQuads = lookupMethod(g_seeBarriersBakedModelClass, "a", "()Ljava/util/List;", "boq.a()Ljava/util/List;");
    if (!g_seeBarriersBakedModelGetGeneralQuads) return false;

    g_seeBarriersBakedModelGetFaceQuads = lookupMethod(g_seeBarriersBakedModelClass, "a", "(Lcq;)Ljava/util/List;", "boq.a(Lcq;)Ljava/util/List;");
    if (!g_seeBarriersBakedModelGetFaceQuads) return false;

    g_seeBarriersListSize = lookupMethod(g_seeBarriersListClass, "size", "()I", "List.size()");
    if (!g_seeBarriersListSize) return false;

    g_seeBarriersListGet = lookupMethod(g_seeBarriersListClass, "get", "(I)Ljava/lang/Object;", "List.get(I)");
    if (!g_seeBarriersListGet) return false;

    g_seeBarriersMapPut = lookupMethod(g_seeBarriersMapClass, "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;", "Map.put(Object,Object)");
    if (!g_seeBarriersMapPut) return false;

    g_seeBarriersMapRemove = lookupMethod(g_seeBarriersMapClass, "remove", "(Ljava/lang/Object;)Ljava/lang/Object;", "Map.remove(Object)");
    if (!g_seeBarriersMapRemove) return false;

    g_seeBarriersModelOverrideInited = true;
    return true;
}

jobject GetSeeBarriersBlockById(JNIEnv* env, int blockId) {
    if (!env || !g_seeBarriersBlockGetById) return nullptr;
    jobject block = env->CallStaticObjectMethod(g_seeBarriersBlockClass, g_seeBarriersBlockGetById, (jint)blockId);
    if (!block || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (block) env->DeleteLocalRef(block);
        return nullptr;
    }
    return block;
}

jobject GetSeeBarriersDefaultState(JNIEnv* env, jobject block) {
    if (!env || !block || !g_seeBarriersBlockGetDefaultState) return nullptr;
    jobject state = env->CallObjectMethod(block, g_seeBarriersBlockGetDefaultState);
    if (!state || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (state) env->DeleteLocalRef(state);
        return nullptr;
    }
    return state;
}

jobject GetSeeBarriersModelShapes(JNIEnv* env) {
    if (!env || !g_slMcClass || !g_slGetMC || !g_seeBarriersMinecraftGetBlockRendererDispatcher ||
        !g_seeBarriersBlockRendererGetModelShapes) {
        return nullptr;
    }

    jobject mc = env->CallStaticObjectMethod(g_slMcClass, g_slGetMC);
    if (!mc || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (mc) env->DeleteLocalRef(mc);
        return nullptr;
    }

    jobject dispatcher = env->CallObjectMethod(mc, g_seeBarriersMinecraftGetBlockRendererDispatcher);
    env->DeleteLocalRef(mc);
    if (!dispatcher || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (dispatcher) env->DeleteLocalRef(dispatcher);
        return nullptr;
    }

    jobject modelShapes = env->CallObjectMethod(dispatcher, g_seeBarriersBlockRendererGetModelShapes);
    env->DeleteLocalRef(dispatcher);
    if (!modelShapes || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (modelShapes) env->DeleteLocalRef(modelShapes);
        return nullptr;
    }

    return modelShapes;
}

jobject GetSeeBarriersBakedModelForBlockId(JNIEnv* env, jobject modelShapes, int blockId) {
    jobject block = GetSeeBarriersBlockById(env, blockId);
    if (!block) return nullptr;

    jobject state = GetSeeBarriersDefaultState(env, block);
    env->DeleteLocalRef(block);
    if (!state) return nullptr;

    jobject model = env->CallObjectMethod(modelShapes, g_seeBarriersBlockModelShapesGetModel, state);
    env->DeleteLocalRef(state);
    if (!model || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (model) env->DeleteLocalRef(model);
        return nullptr;
    }

    return model;
}

struct SeeBarriersModelQuadCounts {
    int generalQuads = -1;
    int faceQuads = -1;
};

int GetSeeBarriersListSize(JNIEnv* env, jobject list) {
    if (!env || !list || !g_seeBarriersListSize) return -1;

    jint size = env->CallIntMethod(list, g_seeBarriersListSize);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return -1;
    }

    return (int)size;
}

void ClearSeeBarriersStoredModelOverrides(JNIEnv* env) {
    if (!env) return;

    for (auto& entry : g_seeBarriersStoredModelOverrides) {
        if (entry.state) {
            env->DeleteGlobalRef(entry.state);
            entry.state = nullptr;
        }
        if (entry.originalModel) {
            env->DeleteGlobalRef(entry.originalModel);
            entry.originalModel = nullptr;
        }
    }
    g_seeBarriersStoredModelOverrides.clear();
}

bool CollectSeeBarriersBarrierStates(JNIEnv* env, jobject barrierBlock, std::vector<jobject>& outStates) {
    outStates.clear();
    if (!env || !barrierBlock) return false;

    if (g_seeBarriersBlockGetStateContainer && g_seeBarriersBlockStateContainerGetValidStates &&
        g_seeBarriersListSize && g_seeBarriersListGet) {
        jobject stateContainer = env->CallObjectMethod(barrierBlock, g_seeBarriersBlockGetStateContainer);
        if (stateContainer && !env->ExceptionCheck()) {
            jobject validStates = env->CallObjectMethod(stateContainer, g_seeBarriersBlockStateContainerGetValidStates);
            if (validStates && !env->ExceptionCheck()) {
                int stateCount = GetSeeBarriersListSize(env, validStates);
                if (stateCount > 0) {
                    outStates.reserve((size_t)stateCount);
                    for (int i = 0; i < stateCount; ++i) {
                        jobject state = env->CallObjectMethod(validStates, g_seeBarriersListGet, (jint)i);
                        if (!state || env->ExceptionCheck()) {
                            env->ExceptionClear();
                            if (state) env->DeleteLocalRef(state);
                            continue;
                        }
                        outStates.push_back(state);
                    }
                }
            }
            else if (env->ExceptionCheck()) {
                env->ExceptionClear();
            }

            if (validStates) env->DeleteLocalRef(validStates);
            env->DeleteLocalRef(stateContainer);
        }
        else {
            env->ExceptionClear();
            if (stateContainer) env->DeleteLocalRef(stateContainer);
        }
    }

    if (!outStates.empty()) return true;

    jobject defaultState = GetSeeBarriersDefaultState(env, barrierBlock);
    if (!defaultState) return false;
    outStates.push_back(defaultState);
    return true;
}

bool RestoreSeeBarriersStoredModelOverrides(JNIEnv* env, jobject modelMap) {
    if (!env || !modelMap) return false;

    bool ok = true;
    for (const auto& entry : g_seeBarriersStoredModelOverrides) {
        jobject previousModel = nullptr;
        if (entry.hadOriginalModel && entry.originalModel) {
            previousModel = env->CallObjectMethod(modelMap, g_seeBarriersMapPut, entry.state, entry.originalModel);
        }
        else {
            previousModel = env->CallObjectMethod(modelMap, g_seeBarriersMapRemove, entry.state);
        }

        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            ok = false;
        }

        if (previousModel) env->DeleteLocalRef(previousModel);
    }

    return ok;
}

bool GetSeeBarriersModelQuadCounts(JNIEnv* env, jobject model, SeeBarriersModelQuadCounts& outCounts) {
    outCounts = {};
    if (!env || !model || !g_seeBarriersBakedModelGetGeneralQuads ||
        !g_seeBarriersBakedModelGetFaceQuads || !g_seeBarriersDirectionValues) {
        return false;
    }

    jobject generalQuads = env->CallObjectMethod(model, g_seeBarriersBakedModelGetGeneralQuads);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        if (generalQuads) env->DeleteLocalRef(generalQuads);
        return false;
    }

    outCounts.generalQuads = generalQuads ? GetSeeBarriersListSize(env, generalQuads) : 0;
    if (generalQuads) env->DeleteLocalRef(generalQuads);
    if (outCounts.generalQuads < 0) return false;

    jobjectArray directions = (jobjectArray)env->CallStaticObjectMethod(g_seeBarriersDirectionClass, g_seeBarriersDirectionValues);
    if (!directions || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (directions) env->DeleteLocalRef(directions);
        return false;
    }

    jsize directionCount = env->GetArrayLength(directions);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(directions);
        return false;
    }

    int faceQuads = 0;
    for (jsize i = 0; i < directionCount; ++i) {
        jobject direction = env->GetObjectArrayElement(directions, i);
        if (!direction || env->ExceptionCheck()) {
            env->ExceptionClear();
            if (direction) env->DeleteLocalRef(direction);
            continue;
        }

        jobject quads = env->CallObjectMethod(model, g_seeBarriersBakedModelGetFaceQuads, direction);
        env->DeleteLocalRef(direction);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            if (quads) env->DeleteLocalRef(quads);
            env->DeleteLocalRef(directions);
            return false;
        }

        int size = quads ? GetSeeBarriersListSize(env, quads) : 0;
        if (quads) env->DeleteLocalRef(quads);
        if (size < 0) {
            env->DeleteLocalRef(directions);
            return false;
        }
        faceQuads += size;
    }

    env->DeleteLocalRef(directions);
    outCounts.faceQuads = faceQuads;
    return true;
}

std::string DescribeSeeBarriersJavaObject(JNIEnv* env, jobject value) {
    if (!env || !value || !g_seeBarriersObjectToString) return "null";

    jstring text = (jstring)env->CallObjectMethod(value, g_seeBarriersObjectToString);
    if (!text || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (text) env->DeleteLocalRef(text);
        return "unknown";
    }

    std::string result = JStringToUtf8(env, text);
    env->DeleteLocalRef(text);
    return result.empty() ? "unknown" : result;
}

int QuerySeeBarriersRuntimeRenderType(JNIEnv* env) {
    if (!env) return -999;

    jobject barrierBlock = GetSeeBarriersBlockById(env, 166);
    if (!barrierBlock) return -999;

    jmethodID renderTypeMethod = nullptr;
    jclass barrierClass = nullptr;
    if (GetSeeBarriersBarrierClass(env, barrierClass)) {
        renderTypeMethod = GetMethodIDCompat(env, barrierClass, "b", "()I");
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            renderTypeMethod = nullptr;
        }
    }
    if (!renderTypeMethod) renderTypeMethod = g_seeBarriersBlockGetRenderType;

    if (!renderTypeMethod) {
        env->DeleteLocalRef(barrierBlock);
        return -999;
    }

    jint renderType = env->CallIntMethod(barrierBlock, renderTypeMethod);
    env->DeleteLocalRef(barrierBlock);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return -999;
    }

    return (int)renderType;
}

void LogSeeBarriersRuntimeState(JNIEnv* env, const char* phase) {
    if (!env || !phase) return;
    if (!EnsureSeeBarriersModelOverrideJNI(env)) {
        DebugLog("See Barriers runtime verify skipped phase=%s reason=model JNI unavailable", phase);
        return;
    }

    jobject barrierBlock = GetSeeBarriersBlockById(env, 166);
    if (!barrierBlock) {
        DebugLog("See Barriers runtime verify failed phase=%s reason=no barrier block", phase);
        return;
    }

    int renderType = QuerySeeBarriersRuntimeRenderType(env);

    jobject renderLayer = env->CallObjectMethod(barrierBlock, g_seeBarriersBlockGetRenderLayer);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        if (renderLayer) env->DeleteLocalRef(renderLayer);
        renderLayer = nullptr;
    }

    int layerOrdinal = -1;
    std::string layerName = "null";
    if (renderLayer) {
        layerName = DescribeSeeBarriersJavaObject(env, renderLayer);
        if (g_seeBarriersEnumOrdinal) {
            jint ordinal = env->CallIntMethod(renderLayer, g_seeBarriersEnumOrdinal);
            if (!env->ExceptionCheck()) layerOrdinal = (int)ordinal;
            else env->ExceptionClear();
        }
        env->DeleteLocalRef(renderLayer);
    }

    env->DeleteLocalRef(barrierBlock);
    DebugLog("See Barriers runtime renderType=%d layerOrdinal=%d layer=%s phase=%s",
        renderType,
        layerOrdinal,
        layerName.c_str(),
        phase);
}

jobject ResolveSeeBarriersVisibleModel(JNIEnv* env, jobject modelShapes) {
    if (!env || !modelShapes) return nullptr;

    jobject model = GetSeeBarriersBakedModelForBlockId(env, modelShapes, kSeeBarriersDirtBlockId);
    if (!model) {
        DebugLog("See Barriers dirt model lookup failed id=%d", kSeeBarriersDirtBlockId);
        return nullptr;
    }

    SeeBarriersModelQuadCounts counts = {};
    if (GetSeeBarriersModelQuadCounts(env, model, counts)) {
        DebugLog("See Barriers selected dirt model id=%d generalQuads=%d faceQuads=%d",
            kSeeBarriersDirtBlockId,
            counts.generalQuads,
            counts.faceQuads);
    }
    else {
        DebugLog("See Barriers selected dirt model id=%d quad count failed", kSeeBarriersDirtBlockId);
    }

    return model;
}

bool SetSeeBarriersModelOverride(JNIEnv* env, bool enabled) {
    if (!env) return false;
    if (enabled == g_seeBarriersBarrierModelOverridden) return true;
    if (!EnsureSeeBarriersModelOverrideJNI(env)) return false;

    jobject modelShapes = GetSeeBarriersModelShapes(env);
    if (!modelShapes) {
        DebugLog("See Barriers model override failed: no BlockModelShapes");
        return false;
    }

    jobject modelMap = env->GetObjectField(modelShapes, g_seeBarriersBlockModelShapesModelMap);
    if (!modelMap || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (modelMap) env->DeleteLocalRef(modelMap);
        env->DeleteLocalRef(modelShapes);
        DebugLog("See Barriers model override failed: no baked model map");
        return false;
    }

    jobject barrierBlock = GetSeeBarriersBlockById(env, 166);
    if (!barrierBlock) {
        env->DeleteLocalRef(modelMap);
        env->DeleteLocalRef(modelShapes);
        DebugLog("See Barriers model override failed: no barrier block");
        return false;
    }

    bool ok = true;
    if (enabled) {
        jobject visibleModel = ResolveSeeBarriersVisibleModel(env, modelShapes);
        if (!visibleModel) {
            ok = false;
            DebugLog("See Barriers model override failed: no dirt model");
        }
        else {
            std::vector<jobject> barrierStates;
            if (!CollectSeeBarriersBarrierStates(env, barrierBlock, barrierStates)) {
                ok = false;
                DebugLog("See Barriers model override failed: no barrier states");
            }
            else {
                ClearSeeBarriersStoredModelOverrides(env);
                g_seeBarriersStoredModelOverrides.reserve(barrierStates.size());

                for (jobject barrierState : barrierStates) {
                    SeeBarriersStoredModelOverride stored = {};
                    stored.state = env->NewGlobalRef(barrierState);
                    if (!stored.state || env->ExceptionCheck()) {
                        env->ExceptionClear();
                        if (stored.state) env->DeleteGlobalRef(stored.state);
                        stored.state = nullptr;
                        ok = false;
                        break;
                    }

                    jobject previousModel = env->CallObjectMethod(modelMap, g_seeBarriersMapPut, barrierState, visibleModel);
                    if (env->ExceptionCheck()) {
                        env->ExceptionClear();
                        env->DeleteGlobalRef(stored.state);
                        stored.state = nullptr;
                        ok = false;
                        break;
                    }

                    stored.hadOriginalModel = previousModel != nullptr;
                    if (previousModel) {
                        stored.originalModel = env->NewGlobalRef(previousModel);
                        if (!stored.originalModel || env->ExceptionCheck()) {
                            env->ExceptionClear();
                            if (stored.originalModel) env->DeleteGlobalRef(stored.originalModel);
                            stored.originalModel = nullptr;
                            env->DeleteLocalRef(previousModel);
                            env->DeleteGlobalRef(stored.state);
                            stored.state = nullptr;
                            ok = false;
                            break;
                        }
                        env->DeleteLocalRef(previousModel);
                    }

                    g_seeBarriersStoredModelOverrides.push_back(stored);
                }

                if (!ok) {
                    RestoreSeeBarriersStoredModelOverrides(env, modelMap);
                    ClearSeeBarriersStoredModelOverrides(env);
                }
                else {
                    g_seeBarriersBarrierModelOverridden = true;
                    DebugLog("See Barriers model override applied states=%u", (unsigned int)g_seeBarriersStoredModelOverrides.size());
                }

                for (jobject barrierState : barrierStates) {
                    env->DeleteLocalRef(barrierState);
                }
            }

            env->DeleteLocalRef(visibleModel);
        }
    }
    else {
        ok = RestoreSeeBarriersStoredModelOverrides(env, modelMap);
        if (!ok) {
            DebugLog("See Barriers model override restore failed");
        }
        else {
            g_seeBarriersBarrierModelOverridden = false;
            DebugLog("See Barriers model override restored states=%u", (unsigned int)g_seeBarriersStoredModelOverrides.size());
        }
        ClearSeeBarriersStoredModelOverrides(env);
    }

    env->DeleteLocalRef(barrierBlock);
    env->DeleteLocalRef(modelMap);
    env->DeleteLocalRef(modelShapes);
    return ok;
}

bool SetSeeBarriersMinecraftRendering(JNIEnv* env, bool enabled) {
    if (enabled == g_seeBarriersMinecraftApplied) return true;
    if (!EnsureSeeBarriersModelOverrideJNI(env)) return false;

    if (!SetSeeBarriersModelOverride(env, enabled)) return false;

    g_seeBarriersMinecraftApplied = enabled;
    DebugLog("See Barriers Minecraft renderer state applied=%d", enabled ? 1 : 0);
    return true;
}

void RequestSeeBarriersMinecraftRefresh() {
    g_seeBarriersMinecraftFailed = false;
    InterlockedExchange(&g_seeBarriersMinecraftRefreshPending, 1);
}

void ProcessSeeBarriersMinecraftRendering() {
    bool requested = g_guiExtrasSeeBarriers && !g_seeBarriersMinecraftFailed;
    bool pendingReload = InterlockedCompareExchange(&g_seeBarriersMinecraftRefreshPending, 0, 0) != 0;
    if (requested == g_seeBarriersMinecraftApplied && !pendingReload) return;

    JNIEnv* env = GetJNIEnvForCurrentThread();
    if (!env) return;

    if (pendingReload && requested && g_seeBarriersMinecraftApplied) {
        if (!SetSeeBarriersMinecraftRendering(env, false)) {
            DebugLog("See Barriers model override reset failed before reapply");
            g_seeBarriersMinecraftFailed = true;
            g_guiExtrasSeeBarriers = false;
            InterlockedExchange(&g_seeBarriersMinecraftRefreshPending, 0);
            return;
        }
    }

    if (!SetSeeBarriersMinecraftRendering(env, requested)) {
        DebugLog("See Barriers Minecraft renderer update failed requested=%d", requested ? 1 : 0);
        if (requested) {
            g_seeBarriersMinecraftFailed = true;
            g_guiExtrasSeeBarriers = false;
        }
        InterlockedExchange(&g_seeBarriersMinecraftRefreshPending, 0);
        return;
    }

    if (!g_slInited || g_slFailed) InitSnaplookJNI();
    bool reloaded = RefreshPerspectiveRenderingWithEnv(env, true);
    DebugLog("See Barriers renderer reload requested=%d applied=%d reloaded=%d",
        requested ? 1 : 0,
        g_seeBarriersMinecraftApplied ? 1 : 0,
        reloaded ? 1 : 0);
    InterlockedExchange(&g_seeBarriersMinecraftRefreshPending, 0);
}

struct TntVisualJNIContext {
    jclass mcClass = nullptr;
    jclass textureManagerClass = nullptr;
    jclass textureMapClass = nullptr;
    jclass spriteClass = nullptr;
    jclass mapClass = nullptr;
    jclass collectionClass = nullptr;
    jclass iteratorClass = nullptr;
    jclass textureUtilClass = nullptr;
    jclass resourceLocationClass = nullptr;
    jclass dynamicTextureClass = nullptr;
    jclass simpleTextureClass = nullptr;
    jmethodID mGetMC = nullptr;
    jmethodID mMinecraftGetTextureManager = nullptr;
    jmethodID mMinecraftGetResourceManager = nullptr;
    jmethodID mMinecraftGetBlockTextureMap = nullptr;
    jmethodID mTextureManagerBindTexture = nullptr;
    jmethodID mTextureManagerLoadTexture = nullptr;
    jmethodID mTextureManagerGetTexture = nullptr;
    jmethodID mTextureMapGetSprite = nullptr;
    jmethodID mMapValues = nullptr;
    jmethodID mCollectionIterator = nullptr;
    jmethodID mIteratorHasNext = nullptr;
    jmethodID mIteratorNext = nullptr;
    jmethodID mSpriteGetFrameData = nullptr;
    jmethodID mSpriteGetX = nullptr;
    jmethodID mSpriteGetY = nullptr;
    jmethodID mSpriteGetWidth = nullptr;
    jmethodID mSpriteGetHeight = nullptr;
    jmethodID mSpriteGetName = nullptr;
    jmethodID mUploadTextureRegion = nullptr;
    jmethodID mTextureUtilLoadTexturePixels = nullptr;
    jmethodID mTextureUtilGenerateMipmapData = nullptr;
    jmethodID mResourceLocationCtor = nullptr;
    jmethodID mDynamicTextureCtor = nullptr;
    jmethodID mDynamicTextureGetPixels = nullptr;
    jmethodID mDynamicTextureUpload = nullptr;
    jmethodID mSimpleTextureCtor = nullptr;
    jfieldID fTextureMapUploadedSprites = nullptr;
    jfieldID fTextureMapMipmapLevels = nullptr;
    jobject beaconBeamResource = nullptr;
    jobject blocksAtlasResource = nullptr;
    jobject blankBeaconTexture = nullptr;
    bool wheatApplied = false;
    bool beaconApplied = false;
    ULONGLONG lastWheatRefreshMs = 0;
    ULONGLONG lastBeaconRefreshMs = 0;
    bool inited = false;
    bool failed = false;
};

TntVisualJNIContext g_tntVisualJNI;
volatile LONG g_tntVisualRestoreRequested = 0;
volatile LONG g_tntVisualRestoreCompleted = 1;
constexpr ULONGLONG kTntVisualRefreshIntervalMs = 500;
constexpr const char* kBeaconBeamResourcePath = "textures/entity/beacon_beam.png";
constexpr const char* kBlocksAtlasResourcePath = "textures/atlas/blocks.png";
constexpr int kForcedWheatRenderStage = 0;
constexpr int kBarrierBlockId = 166;
constexpr int kSeeBarriersSectionsPerChunk = 16;
constexpr int kSeeBarriersSectionBlockCount = 16 * 16 * 16;
constexpr int kSeeBarriersScanSectionsPerFrame = 64;
constexpr double kSeeBarriersScanBudgetMs = 0.45;
constexpr ULONGLONG kSeeBarriersLoadedChunkSyncIntervalMs = 500;
constexpr ULONGLONG kSeeBarriersChunkRescanIntervalMs = 30000;
constexpr ULONGLONG kSeeBarriersDamageMarkerRefreshIntervalMs = 1000;
constexpr int kSeeBarriersDamageMarkerStage = 9;
constexpr int kSeeBarriersDamageMarkerMax = 256;
constexpr int kSeeBarriersOverlayMaxBlocks = 512;
const char* const kWheatStageSpriteNames[] = {
    "minecraft:blocks/wheat_stage_0",
    "minecraft:blocks/wheat_stage_1",
    "minecraft:blocks/wheat_stage_2",
    "minecraft:blocks/wheat_stage_3",
    "minecraft:blocks/wheat_stage_4",
    "minecraft:blocks/wheat_stage_5",
    "minecraft:blocks/wheat_stage_6",
    "minecraft:blocks/wheat_stage_7"
};
const char* const kWheatStageTextureResourcePaths[] = {
    "minecraft:textures/blocks/wheat_stage_0.png",
    "minecraft:textures/blocks/wheat_stage_1.png",
    "minecraft:textures/blocks/wheat_stage_2.png",
    "minecraft:textures/blocks/wheat_stage_3.png",
    "minecraft:textures/blocks/wheat_stage_4.png",
    "minecraft:textures/blocks/wheat_stage_5.png",
    "minecraft:textures/blocks/wheat_stage_6.png",
    "minecraft:textures/blocks/wheat_stage_7.png"
};

struct BarrierBlockPos {
    int x = 0;
    int y = 0;
    int z = 0;
};

struct BarrierChunkCache {
    int chunkX = 0;
    int chunkZ = 0;
    int nextSectionIndex = 0;
    bool complete = false;
    bool rescanning = false;
    ULONGLONG completedAtMs = 0;
    jobject chunkRef = nullptr;
    std::vector<BarrierBlockPos> barriers;
    std::unordered_set<int> barrierLocalKeys;
    std::vector<BarrierBlockPos> pendingBarriers;
    std::unordered_set<int> pendingLocalKeys;
};

struct SeeBarriersJNIContext {
    jclass mcClass = nullptr;
    jclass entityClass = nullptr;
    jclass worldClass = nullptr;
    jclass worldClientClass = nullptr;
    jclass chunkProviderClass = nullptr;
    jclass chunkClass = nullptr;
    jclass extendedBlockStorageClass = nullptr;
    jclass renderManagerClass = nullptr;
    jclass renderGlobalClass = nullptr;
    jclass listClass = nullptr;
    jclass blockPosClass = nullptr;
    jclass blockStateClass = nullptr;
    jclass blockClass = nullptr;
    jmethodID mGetMC = nullptr;
    jmethodID mGetRenderManager = nullptr;
    jmethodID mRenderGlobalSetBlockDamage = nullptr;
    jmethodID mWorldGetBlockState = nullptr;
    jmethodID mBlockStateGetBlock = nullptr;
    jmethodID mBlockGetIdFromBlock = nullptr;
    jmethodID mBlockPosCtor = nullptr;
    jmethodID mBlockPosOffset = nullptr;
    jmethodID mStorageGetData = nullptr;
    jmethodID mListSize = nullptr;
    jmethodID mListGet = nullptr;
    jfieldID fWorld = nullptr;
    jfieldID fThePlayer = nullptr;
    jfieldID fRenderGlobal = nullptr;
    jfieldID fEntityPosX = nullptr;
    jfieldID fEntityPosY = nullptr;
    jfieldID fEntityPosZ = nullptr;
    jfieldID fWorldClientChunkProvider = nullptr;
    jfieldID fLoadedChunks = nullptr;
    jfieldID fChunkX = nullptr;
    jfieldID fChunkZ = nullptr;
    jfieldID fChunkStorageArrays = nullptr;
    jfieldID fRenderPosX = nullptr;
    jfieldID fRenderPosY = nullptr;
    jfieldID fRenderPosZ = nullptr;
    jobject cachedWorld = nullptr;
    std::unordered_map<long long, BarrierChunkCache> chunkCaches;
    std::unordered_map<int, BarrierBlockPos> activeDamageMarkers;
    ULONGLONG lastChunkSyncMs = 0;
    ULONGLONG lastDamageMarkerRefreshMs = 0;
    double renderCameraX = 0.0;
    double renderCameraY = 0.0;
    double renderCameraZ = 0.0;
    bool frameOverlayDrawn = false;
    bool inited = false;
    bool failed = false;
    bool chunkCacheReady = false;
};

SeeBarriersJNIContext g_seeBarriersJNI;

bool InitTntVisualJNI() {
    if (g_tntVisualJNI.failed) {
        g_tntVisualJNI.failed = false;
        g_tntVisualJNI.inited = false;
    }
    if (g_tntVisualJNI.inited) return true;
    if (!g_env) return false;

    g_tntVisualJNI.inited = true;

    jclass mcClassLocal = FindClassLoose(g_env, "ave");
    if (!mcClassLocal) { g_env->ExceptionClear(); g_tntVisualJNI.failed = true; return false; }
    g_tntVisualJNI.mcClass = (jclass)g_env->NewGlobalRef(mcClassLocal);
    g_env->DeleteLocalRef(mcClassLocal);
    if (!g_tntVisualJNI.mcClass) { g_env->ExceptionClear(); g_tntVisualJNI.failed = true; return false; }

    g_tntVisualJNI.mGetMC = GetStaticMethodIDCompat(g_env, g_tntVisualJNI.mcClass, "A", "()Lave;");
    g_tntVisualJNI.mMinecraftGetTextureManager = GetMethodIDCompat(g_env, g_tntVisualJNI.mcClass, "P", "()Lbmj;");
    g_tntVisualJNI.mMinecraftGetResourceManager = GetMethodIDCompat(g_env, g_tntVisualJNI.mcClass, "Q", "()Lbni;");
    g_tntVisualJNI.mMinecraftGetBlockTextureMap = GetMethodIDCompat(g_env, g_tntVisualJNI.mcClass, "T", "()Lbmh;");
    if (!g_tntVisualJNI.mGetMC || !g_tntVisualJNI.mMinecraftGetTextureManager ||
        !g_tntVisualJNI.mMinecraftGetResourceManager || !g_tntVisualJNI.mMinecraftGetBlockTextureMap) {
        g_env->ExceptionClear();
        g_tntVisualJNI.failed = true;
        return false;
    }

    jclass textureManagerClassLocal = FindClassLoose(g_env, "bmj");
    if (!textureManagerClassLocal) { g_env->ExceptionClear(); g_tntVisualJNI.failed = true; return false; }
    g_tntVisualJNI.textureManagerClass = (jclass)g_env->NewGlobalRef(textureManagerClassLocal);
    g_env->DeleteLocalRef(textureManagerClassLocal);
    if (!g_tntVisualJNI.textureManagerClass) { g_env->ExceptionClear(); g_tntVisualJNI.failed = true; return false; }

    g_tntVisualJNI.mTextureManagerBindTexture = GetMethodIDCompat(g_env, g_tntVisualJNI.textureManagerClass, "a", "(Ljy;)V");
    g_tntVisualJNI.mTextureManagerLoadTexture = GetMethodIDCompat(g_env, g_tntVisualJNI.textureManagerClass, "a", "(Ljy;Lbmk;)Z");
    g_tntVisualJNI.mTextureManagerGetTexture = GetMethodIDCompat(g_env, g_tntVisualJNI.textureManagerClass, "b", "(Ljy;)Lbmk;");
    if (!g_tntVisualJNI.mTextureManagerBindTexture || !g_tntVisualJNI.mTextureManagerLoadTexture || !g_tntVisualJNI.mTextureManagerGetTexture) {
        g_env->ExceptionClear();
        g_tntVisualJNI.failed = true;
        return false;
    }

    jclass textureMapClassLocal = FindClassLoose(g_env, "bmh");
    if (!textureMapClassLocal) { g_env->ExceptionClear(); g_tntVisualJNI.failed = true; return false; }
    g_tntVisualJNI.textureMapClass = (jclass)g_env->NewGlobalRef(textureMapClassLocal);
    g_env->DeleteLocalRef(textureMapClassLocal);
    if (!g_tntVisualJNI.textureMapClass) { g_env->ExceptionClear(); g_tntVisualJNI.failed = true; return false; }

    g_tntVisualJNI.mTextureMapGetSprite = GetMethodIDCompat(g_env, g_tntVisualJNI.textureMapClass, "a", "(Ljava/lang/String;)Lbmi;");
    g_tntVisualJNI.fTextureMapUploadedSprites = GetFieldIDCompat(g_env, g_tntVisualJNI.textureMapClass, "k", "Ljava/util/Map;");
    g_tntVisualJNI.fTextureMapMipmapLevels = GetFieldIDCompat(g_env, g_tntVisualJNI.textureMapClass, "n", "I");
    if (!g_tntVisualJNI.mTextureMapGetSprite || !g_tntVisualJNI.fTextureMapUploadedSprites || !g_tntVisualJNI.fTextureMapMipmapLevels) {
        g_env->ExceptionClear();
        g_tntVisualJNI.failed = true;
        return false;
    }

    jclass spriteClassLocal = FindClassLoose(g_env, "bmi");
    if (!spriteClassLocal) { g_env->ExceptionClear(); g_tntVisualJNI.failed = true; return false; }
    g_tntVisualJNI.spriteClass = (jclass)g_env->NewGlobalRef(spriteClassLocal);
    g_env->DeleteLocalRef(spriteClassLocal);
    if (!g_tntVisualJNI.spriteClass) { g_env->ExceptionClear(); g_tntVisualJNI.failed = true; return false; }

    g_tntVisualJNI.mSpriteGetFrameData = GetMethodIDCompat(g_env, g_tntVisualJNI.spriteClass, "a", "(I)[[I");
    g_tntVisualJNI.mSpriteGetX = GetMethodIDCompat(g_env, g_tntVisualJNI.spriteClass, "a", "()I");
    g_tntVisualJNI.mSpriteGetY = GetMethodIDCompat(g_env, g_tntVisualJNI.spriteClass, "b", "()I");
    g_tntVisualJNI.mSpriteGetWidth = GetMethodIDCompat(g_env, g_tntVisualJNI.spriteClass, "c", "()I");
    g_tntVisualJNI.mSpriteGetHeight = GetMethodIDCompat(g_env, g_tntVisualJNI.spriteClass, "d", "()I");
    g_tntVisualJNI.mSpriteGetName = GetMethodIDCompat(g_env, g_tntVisualJNI.spriteClass, "i", "()Ljava/lang/String;");
    if (!g_tntVisualJNI.mSpriteGetFrameData || !g_tntVisualJNI.mSpriteGetX || !g_tntVisualJNI.mSpriteGetY ||
        !g_tntVisualJNI.mSpriteGetWidth || !g_tntVisualJNI.mSpriteGetHeight || !g_tntVisualJNI.mSpriteGetName) {
        g_env->ExceptionClear();
        g_tntVisualJNI.failed = true;
        return false;
    }

    jclass mapClassLocal = FindClassLoose(g_env, "java/util/Map");
    if (!mapClassLocal) { g_env->ExceptionClear(); g_tntVisualJNI.failed = true; return false; }
    g_tntVisualJNI.mapClass = (jclass)g_env->NewGlobalRef(mapClassLocal);
    g_env->DeleteLocalRef(mapClassLocal);
    if (!g_tntVisualJNI.mapClass) { g_env->ExceptionClear(); g_tntVisualJNI.failed = true; return false; }

    g_tntVisualJNI.mMapValues = GetMethodIDCompat(g_env, g_tntVisualJNI.mapClass, "values", "()Ljava/util/Collection;");
    if (!g_tntVisualJNI.mMapValues) {
        g_env->ExceptionClear();
        g_tntVisualJNI.failed = true;
        return false;
    }

    jclass collectionClassLocal = FindClassLoose(g_env, "java/util/Collection");
    if (!collectionClassLocal) { g_env->ExceptionClear(); g_tntVisualJNI.failed = true; return false; }
    g_tntVisualJNI.collectionClass = (jclass)g_env->NewGlobalRef(collectionClassLocal);
    g_env->DeleteLocalRef(collectionClassLocal);
    if (!g_tntVisualJNI.collectionClass) { g_env->ExceptionClear(); g_tntVisualJNI.failed = true; return false; }

    g_tntVisualJNI.mCollectionIterator = GetMethodIDCompat(g_env, g_tntVisualJNI.collectionClass, "iterator", "()Ljava/util/Iterator;");
    if (!g_tntVisualJNI.mCollectionIterator) {
        g_env->ExceptionClear();
        g_tntVisualJNI.failed = true;
        return false;
    }

    jclass iteratorClassLocal = FindClassLoose(g_env, "java/util/Iterator");
    if (!iteratorClassLocal) { g_env->ExceptionClear(); g_tntVisualJNI.failed = true; return false; }
    g_tntVisualJNI.iteratorClass = (jclass)g_env->NewGlobalRef(iteratorClassLocal);
    g_env->DeleteLocalRef(iteratorClassLocal);
    if (!g_tntVisualJNI.iteratorClass) { g_env->ExceptionClear(); g_tntVisualJNI.failed = true; return false; }

    g_tntVisualJNI.mIteratorHasNext = GetMethodIDCompat(g_env, g_tntVisualJNI.iteratorClass, "hasNext", "()Z");
    g_tntVisualJNI.mIteratorNext = GetMethodIDCompat(g_env, g_tntVisualJNI.iteratorClass, "next", "()Ljava/lang/Object;");
    if (!g_tntVisualJNI.mIteratorHasNext || !g_tntVisualJNI.mIteratorNext) {
        g_env->ExceptionClear();
        g_tntVisualJNI.failed = true;
        return false;
    }

    jclass textureUtilClassLocal = FindClassLoose(g_env, "bml");
    if (!textureUtilClassLocal) { g_env->ExceptionClear(); g_tntVisualJNI.failed = true; return false; }
    g_tntVisualJNI.textureUtilClass = (jclass)g_env->NewGlobalRef(textureUtilClassLocal);
    g_env->DeleteLocalRef(textureUtilClassLocal);
    if (!g_tntVisualJNI.textureUtilClass) { g_env->ExceptionClear(); g_tntVisualJNI.failed = true; return false; }

    g_tntVisualJNI.mUploadTextureRegion = GetStaticMethodIDCompat(g_env, g_tntVisualJNI.textureUtilClass, "a", "([[IIIIIZZ)V");
    g_tntVisualJNI.mTextureUtilLoadTexturePixels = GetStaticMethodIDCompat(g_env, g_tntVisualJNI.textureUtilClass, "a", "(Lbni;Ljy;)[I");
    g_tntVisualJNI.mTextureUtilGenerateMipmapData = GetStaticMethodIDCompat(g_env, g_tntVisualJNI.textureUtilClass, "a", "(II[[I)[[I");
    if (!g_tntVisualJNI.mUploadTextureRegion || !g_tntVisualJNI.mTextureUtilLoadTexturePixels || !g_tntVisualJNI.mTextureUtilGenerateMipmapData) {
        g_env->ExceptionClear();
        g_tntVisualJNI.failed = true;
        return false;
    }

    jclass resourceLocationClassLocal = FindClassLoose(g_env, "jy");
    if (!resourceLocationClassLocal) { g_env->ExceptionClear(); g_tntVisualJNI.failed = true; return false; }
    g_tntVisualJNI.resourceLocationClass = (jclass)g_env->NewGlobalRef(resourceLocationClassLocal);
    g_env->DeleteLocalRef(resourceLocationClassLocal);
    if (!g_tntVisualJNI.resourceLocationClass) { g_env->ExceptionClear(); g_tntVisualJNI.failed = true; return false; }

    g_tntVisualJNI.mResourceLocationCtor = GetMethodIDCompat(g_env, g_tntVisualJNI.resourceLocationClass, "<init>", "(Ljava/lang/String;)V");
    if (!g_tntVisualJNI.mResourceLocationCtor) {
        g_env->ExceptionClear();
        g_tntVisualJNI.failed = true;
        return false;
    }

    jclass dynamicTextureClassLocal = FindClassLoose(g_env, "blz");
    if (!dynamicTextureClassLocal) { g_env->ExceptionClear(); g_tntVisualJNI.failed = true; return false; }
    g_tntVisualJNI.dynamicTextureClass = (jclass)g_env->NewGlobalRef(dynamicTextureClassLocal);
    g_env->DeleteLocalRef(dynamicTextureClassLocal);
    if (!g_tntVisualJNI.dynamicTextureClass) { g_env->ExceptionClear(); g_tntVisualJNI.failed = true; return false; }

    g_tntVisualJNI.mDynamicTextureCtor = GetMethodIDCompat(g_env, g_tntVisualJNI.dynamicTextureClass, "<init>", "(II)V");
    g_tntVisualJNI.mDynamicTextureGetPixels = GetMethodIDCompat(g_env, g_tntVisualJNI.dynamicTextureClass, "e", "()[I");
    g_tntVisualJNI.mDynamicTextureUpload = GetMethodIDCompat(g_env, g_tntVisualJNI.dynamicTextureClass, "d", "()V");
    if (!g_tntVisualJNI.mDynamicTextureCtor || !g_tntVisualJNI.mDynamicTextureGetPixels || !g_tntVisualJNI.mDynamicTextureUpload) {
        g_env->ExceptionClear();
        g_tntVisualJNI.failed = true;
        return false;
    }

    jclass simpleTextureClassLocal = FindClassLoose(g_env, "bme");
    if (!simpleTextureClassLocal) { g_env->ExceptionClear(); g_tntVisualJNI.failed = true; return false; }
    g_tntVisualJNI.simpleTextureClass = (jclass)g_env->NewGlobalRef(simpleTextureClassLocal);
    g_env->DeleteLocalRef(simpleTextureClassLocal);
    if (!g_tntVisualJNI.simpleTextureClass) { g_env->ExceptionClear(); g_tntVisualJNI.failed = true; return false; }

    g_tntVisualJNI.mSimpleTextureCtor = GetMethodIDCompat(g_env, g_tntVisualJNI.simpleTextureClass, "<init>", "(Ljy;)V");
    if (!g_tntVisualJNI.mSimpleTextureCtor) {
        g_env->ExceptionClear();
        g_tntVisualJNI.failed = true;
        return false;
    }

    jstring beamPath = g_env->NewStringUTF(kBeaconBeamResourcePath);
    if (!beamPath || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (beamPath) g_env->DeleteLocalRef(beamPath);
        g_tntVisualJNI.failed = true;
        return false;
    }

    jobject beaconResourceLocal = g_env->NewObject(g_tntVisualJNI.resourceLocationClass, g_tntVisualJNI.mResourceLocationCtor, beamPath);
    g_env->DeleteLocalRef(beamPath);
    if (!beaconResourceLocal || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (beaconResourceLocal) g_env->DeleteLocalRef(beaconResourceLocal);
        g_tntVisualJNI.failed = true;
        return false;
    }

    g_tntVisualJNI.beaconBeamResource = g_env->NewGlobalRef(beaconResourceLocal);
    g_env->DeleteLocalRef(beaconResourceLocal);
    if (!g_tntVisualJNI.beaconBeamResource) {
        g_env->ExceptionClear();
        g_tntVisualJNI.failed = true;
        return false;
    }

    jstring blocksAtlasPath = g_env->NewStringUTF(kBlocksAtlasResourcePath);
    if (!blocksAtlasPath || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (blocksAtlasPath) g_env->DeleteLocalRef(blocksAtlasPath);
        g_tntVisualJNI.failed = true;
        return false;
    }

    jobject blocksAtlasLocal = g_env->NewObject(g_tntVisualJNI.resourceLocationClass, g_tntVisualJNI.mResourceLocationCtor, blocksAtlasPath);
    g_env->DeleteLocalRef(blocksAtlasPath);
    if (!blocksAtlasLocal || g_env->ExceptionCheck()) {
        g_env->ExceptionClear();
        if (blocksAtlasLocal) g_env->DeleteLocalRef(blocksAtlasLocal);
        g_tntVisualJNI.failed = true;
        return false;
    }

    g_tntVisualJNI.blocksAtlasResource = g_env->NewGlobalRef(blocksAtlasLocal);
    g_env->DeleteLocalRef(blocksAtlasLocal);
    if (!g_tntVisualJNI.blocksAtlasResource) {
        g_env->ExceptionClear();
        g_tntVisualJNI.failed = true;
        return false;
    }

    return true;
}

bool InitSeeBarriersJNI() {
    if (g_seeBarriersJNI.failed) {
        g_seeBarriersJNI.failed = false;
        g_seeBarriersJNI.inited = false;
    }
    if (g_seeBarriersJNI.inited) return true;
    if (!g_env) return false;

    bool ok = EnsureSeeBarriersModelOverrideJNI(g_env);
    g_seeBarriersJNI.inited = ok;
    g_seeBarriersJNI.failed = !ok;
    DebugLog("See Barriers JNI ready modelSwap=%d", ok ? 1 : 0);
    return ok;
}

bool IsValidAtlasSprite(JNIEnv* env, jobject sprite) {
    if (!env || !sprite) return false;
    jstring name = (jstring)env->CallObjectMethod(sprite, g_tntVisualJNI.mSpriteGetName);
    if (!name || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (name) env->DeleteLocalRef(name);
        return false;
    }
    std::string spriteName = JStringToUtf8(env, name);
    env->DeleteLocalRef(name);
    return !spriteName.empty() && spriteName != "missingno";
}

jobject GetTextureManagerWithEnv(JNIEnv* env) {
    if (!env || !g_tntVisualJNI.mcClass) return nullptr;
    jobject mc = env->CallStaticObjectMethod(g_tntVisualJNI.mcClass, g_tntVisualJNI.mGetMC);
    if (!mc || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (mc) env->DeleteLocalRef(mc);
        return nullptr;
    }

    jobject textureManager = env->CallObjectMethod(mc, g_tntVisualJNI.mMinecraftGetTextureManager);
    env->DeleteLocalRef(mc);
    if (!textureManager || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (textureManager) env->DeleteLocalRef(textureManager);
        return nullptr;
    }
    return textureManager;
}

jobject GetBlockTextureMapWithEnv(JNIEnv* env) {
    if (!env || !g_tntVisualJNI.mcClass) return nullptr;
    jobject mc = env->CallStaticObjectMethod(g_tntVisualJNI.mcClass, g_tntVisualJNI.mGetMC);
    if (!mc || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (mc) env->DeleteLocalRef(mc);
        return nullptr;
    }

    jobject textureMap = env->CallObjectMethod(mc, g_tntVisualJNI.mMinecraftGetBlockTextureMap);
    env->DeleteLocalRef(mc);
    if (!textureMap || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (textureMap) env->DeleteLocalRef(textureMap);
        return nullptr;
    }
    return textureMap;
}

jobject GetResourceManagerWithEnv(JNIEnv* env) {
    if (!env || !g_tntVisualJNI.mcClass) return nullptr;
    jobject mc = env->CallStaticObjectMethod(g_tntVisualJNI.mcClass, g_tntVisualJNI.mGetMC);
    if (!mc || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (mc) env->DeleteLocalRef(mc);
        return nullptr;
    }

    jobject resourceManager = env->CallObjectMethod(mc, g_tntVisualJNI.mMinecraftGetResourceManager);
    env->DeleteLocalRef(mc);
    if (!resourceManager || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (resourceManager) env->DeleteLocalRef(resourceManager);
        return nullptr;
    }
    return resourceManager;
}

jobject GetAtlasSpriteByName(JNIEnv* env, jobject textureMap, const char* spriteName) {
    if (!env || !textureMap || !spriteName) return nullptr;

    const char* candidates[2] = { spriteName, nullptr };
    if (strncmp(spriteName, "minecraft:", 10) == 0) candidates[1] = spriteName + 10;

    for (int i = 0; i < 2; ++i) {
        if (!candidates[i]) continue;

        jstring name = env->NewStringUTF(candidates[i]);
        if (!name || env->ExceptionCheck()) {
            env->ExceptionClear();
            if (name) env->DeleteLocalRef(name);
            return nullptr;
        }

        jobject sprite = env->CallObjectMethod(textureMap, g_tntVisualJNI.mTextureMapGetSprite, name);
        env->DeleteLocalRef(name);
        if (!sprite || env->ExceptionCheck()) {
            env->ExceptionClear();
            if (sprite) env->DeleteLocalRef(sprite);
            continue;
        }

        if (IsValidAtlasSprite(env, sprite)) return sprite;
        env->DeleteLocalRef(sprite);
    }

    return nullptr;
}

bool TryGetAtlasSpriteName(JNIEnv* env, jobject sprite, std::string& spriteName) {
    spriteName.clear();
    if (!env || !sprite) return false;

    jstring name = (jstring)env->CallObjectMethod(sprite, g_tntVisualJNI.mSpriteGetName);
    if (!name || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (name) env->DeleteLocalRef(name);
        return false;
    }

    spriteName = JStringToUtf8(env, name);
    env->DeleteLocalRef(name);
    return !spriteName.empty();
}

int TryParseWheatStageFromName(const std::string& spriteName) {
    static const char* const patterns[] = {
        "wheat_stage_",
        "wheat_stage",
        "wheatstage_",
        "wheatstage"
    };

    for (const char* pattern : patterns) {
        size_t index = spriteName.find(pattern);
        if (index == std::string::npos) continue;

        index += strlen(pattern);
        if (index >= spriteName.size()) continue;

        char ch = spriteName[index];
        if (ch < '0' || ch > '9') continue;

        int stage = 0;
        while (index < spriteName.size()) {
            char digit = spriteName[index];
            if (digit < '0' || digit > '9') break;
            stage = stage * 10 + (digit - '0');
            ++index;
        }

        if (stage >= 0 && stage <= 7) return stage;
    }

    return -1;
}

void ReleaseResolvedSprites(JNIEnv* env, jobject stageSprites[], int count) {
    if (!env || !stageSprites) return;
    for (int i = 0; i < count; ++i) {
        if (stageSprites[i]) {
            env->DeleteLocalRef(stageSprites[i]);
            stageSprites[i] = nullptr;
        }
    }
}

void AppendResolvedWheatStageSummary(JNIEnv* env, jobject stageSprites[], int count, std::string& summary) {
    summary.clear();
    if (!env || !stageSprites || count <= 0) return;

    for (int i = 0; i < count; ++i) {
        if (!summary.empty()) summary += " ";
        summary += "[";
        summary += std::to_string(i);
        summary += "=";

        if (!stageSprites[i]) {
            summary += "missing]";
            continue;
        }

        std::string spriteName;
        if (TryGetAtlasSpriteName(env, stageSprites[i], spriteName)) summary += spriteName;
        else summary += "unknown";
        summary += "]";
    }
}

void ResolveWheatStageSprites(JNIEnv* env, jobject textureMap, jobject stageSprites[], int count) {
    if (!env || !textureMap || !stageSprites || count <= 0) return;

    for (int i = 0; i < count; ++i) {
        stageSprites[i] = GetAtlasSpriteByName(env, textureMap, kWheatStageSpriteNames[i]);
    }

    bool missingStages = false;
    for (int i = 0; i < count; ++i) {
        if (!stageSprites[i]) {
            missingStages = true;
            break;
        }
    }

    if (!missingStages || !g_tntVisualJNI.fTextureMapUploadedSprites || !g_tntVisualJNI.mMapValues ||
        !g_tntVisualJNI.mCollectionIterator || !g_tntVisualJNI.mIteratorHasNext || !g_tntVisualJNI.mIteratorNext) {
        return;
    }

    jobject uploadedSpriteMap = env->GetObjectField(textureMap, g_tntVisualJNI.fTextureMapUploadedSprites);
    if (!uploadedSpriteMap || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (uploadedSpriteMap) env->DeleteLocalRef(uploadedSpriteMap);
        return;
    }

    jobject spriteValues = env->CallObjectMethod(uploadedSpriteMap, g_tntVisualJNI.mMapValues);
    env->DeleteLocalRef(uploadedSpriteMap);
    if (!spriteValues || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (spriteValues) env->DeleteLocalRef(spriteValues);
        return;
    }

    jobject iterator = env->CallObjectMethod(spriteValues, g_tntVisualJNI.mCollectionIterator);
    env->DeleteLocalRef(spriteValues);
    if (!iterator || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (iterator) env->DeleteLocalRef(iterator);
        return;
    }

    while (env->CallBooleanMethod(iterator, g_tntVisualJNI.mIteratorHasNext) == JNI_TRUE) {
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            break;
        }

        jobject sprite = env->CallObjectMethod(iterator, g_tntVisualJNI.mIteratorNext);
        if (!sprite || env->ExceptionCheck()) {
            env->ExceptionClear();
            if (sprite) env->DeleteLocalRef(sprite);
            continue;
        }

        std::string spriteName;
        int stage = TryGetAtlasSpriteName(env, sprite, spriteName)
            ? TryParseWheatStageFromName(spriteName)
            : -1;

        if (stage >= 0 && stage < count && !stageSprites[stage]) {
            stageSprites[stage] = env->NewLocalRef(sprite);
        }

        env->DeleteLocalRef(sprite);
    }

    env->DeleteLocalRef(iterator);
}

bool UploadFrameToSpriteRegion(JNIEnv* env, jobject sprite, jobject frameData) {
    static ULONGLONG s_lastUploadDebugLogMs = 0;
    if (!env || !sprite || !frameData) return false;

    jint x = env->CallIntMethod(sprite, g_tntVisualJNI.mSpriteGetX);
    jint y = env->CallIntMethod(sprite, g_tntVisualJNI.mSpriteGetY);
    jint width = env->CallIntMethod(sprite, g_tntVisualJNI.mSpriteGetWidth);
    jint height = env->CallIntMethod(sprite, g_tntVisualJNI.mSpriteGetHeight);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        ULONGLONG now = GetTickCount64();
        if ((now - s_lastUploadDebugLogMs) >= 2000) {
            DebugLog("Wheat upload failed: sprite dimension query failed");
            s_lastUploadDebugLogMs = now;
        }
        return false;
    }

    if (width <= 0 || height <= 0) {
        ULONGLONG now = GetTickCount64();
        if ((now - s_lastUploadDebugLogMs) >= 2000) {
            DebugLog("Wheat upload failed: invalid sprite region width=%d height=%d", width, height);
            s_lastUploadDebugLogMs = now;
        }
        return false;
    }

    env->CallStaticVoidMethod(
        g_tntVisualJNI.textureUtilClass,
        g_tntVisualJNI.mUploadTextureRegion,
        frameData,
        width,
        height,
        x,
        y,
        JNI_FALSE,
        JNI_FALSE);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        ULONGLONG now = GetTickCount64();
        if ((now - s_lastUploadDebugLogMs) >= 2000) {
            DebugLog("Wheat upload failed: uploadTextureRegion x=%d y=%d width=%d height=%d", x, y, width, height);
            s_lastUploadDebugLogMs = now;
        }
        return false;
    }

    return true;
}

bool BuildMipFrameDataWithEnv(JNIEnv* env, const std::vector<jint>& pixels, jint width, jint height, jint mipmapLevels, jobjectArray& frameData) {
    static ULONGLONG s_lastMipBuildDebugLogMs = 0;
    frameData = nullptr;
    if (!env || width <= 0 || height <= 0) return false;

    jsize pixelCount = (jsize)pixels.size();
    if (pixelCount != width * height) {
        ULONGLONG now = GetTickCount64();
        if ((now - s_lastMipBuildDebugLogMs) >= 2000) {
            DebugLog("Wheat mip build failed: pixelCount=%d expected=%d width=%d height=%d", (int)pixelCount, (int)(width * height), width, height);
            s_lastMipBuildDebugLogMs = now;
        }
        return false;
    }

    jclass intArrayClass = FindClassLoose(env, "[I");
    if (!intArrayClass || env->ExceptionCheck()) {
        env->ExceptionClear();
        ULONGLONG now = GetTickCount64();
        if ((now - s_lastMipBuildDebugLogMs) >= 2000) {
            DebugLog("Wheat mip build failed: FindClass([I) failed");
            s_lastMipBuildDebugLogMs = now;
        }
        if (intArrayClass) env->DeleteLocalRef(intArrayClass);
        return false;
    }

    jintArray basePixels = env->NewIntArray(pixelCount);
    if (!basePixels || env->ExceptionCheck()) {
        env->ExceptionClear();
        ULONGLONG now = GetTickCount64();
        if ((now - s_lastMipBuildDebugLogMs) >= 2000) {
            DebugLog("Wheat mip build failed: NewIntArray failed count=%d", (int)pixelCount);
            s_lastMipBuildDebugLogMs = now;
        }
        env->DeleteLocalRef(intArrayClass);
        if (basePixels) env->DeleteLocalRef(basePixels);
        return false;
    }

    env->SetIntArrayRegion(basePixels, 0, pixelCount, pixels.data());
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        ULONGLONG now = GetTickCount64();
        if ((now - s_lastMipBuildDebugLogMs) >= 2000) {
            DebugLog("Wheat mip build failed: SetIntArrayRegion failed count=%d", (int)pixelCount);
            s_lastMipBuildDebugLogMs = now;
        }
        env->DeleteLocalRef(basePixels);
        env->DeleteLocalRef(intArrayClass);
        return false;
    }

    jsize frameLevels = mipmapLevels > 0 ? (mipmapLevels + 1) : 1;
    jobjectArray baseFrameData = env->NewObjectArray(frameLevels, intArrayClass, nullptr);
    if (!baseFrameData || env->ExceptionCheck()) {
        env->ExceptionClear();
        ULONGLONG now = GetTickCount64();
        if ((now - s_lastMipBuildDebugLogMs) >= 2000) {
            DebugLog("Wheat mip build failed: NewObjectArray failed");
            s_lastMipBuildDebugLogMs = now;
        }
        if (baseFrameData) env->DeleteLocalRef(baseFrameData);
        env->DeleteLocalRef(basePixels);
        env->DeleteLocalRef(intArrayClass);
        return false;
    }

    env->SetObjectArrayElement(baseFrameData, 0, basePixels);
    bool ok = !env->ExceptionCheck();
    if (!ok || mipmapLevels <= 0) {
        if (!ok) {
            env->ExceptionClear();
            env->DeleteLocalRef(baseFrameData);
            baseFrameData = nullptr;
        }
        frameData = baseFrameData;
    }
    else {
        frameData = (jobjectArray)env->CallStaticObjectMethod(
            g_tntVisualJNI.textureUtilClass,
            g_tntVisualJNI.mTextureUtilGenerateMipmapData,
            mipmapLevels,
            width,
            baseFrameData);
        ok = frameData && !env->ExceptionCheck();
        if (!ok) {
            env->ExceptionClear();
            ULONGLONG now = GetTickCount64();
            if ((now - s_lastMipBuildDebugLogMs) >= 2000) {
                DebugLog("Wheat mip build failed: generateMipmapData failed mipLevels=%d width=%d", mipmapLevels, width);
                s_lastMipBuildDebugLogMs = now;
            }
            if (frameData) env->DeleteLocalRef(frameData);
            frameData = nullptr;
        }
        env->DeleteLocalRef(baseFrameData);
    }

    if (!ok) {
        env->ExceptionClear();
    }

    env->DeleteLocalRef(basePixels);
    env->DeleteLocalRef(intArrayClass);
    return ok;
}

bool LoadTexturePixelsWithEnv(JNIEnv* env, const char* resourcePath, std::vector<jint>& pixels) {
    pixels.clear();
    if (!env || !resourcePath || !g_tntVisualJNI.textureUtilClass || !g_tntVisualJNI.mTextureUtilLoadTexturePixels) return false;

    jobject resourceManager = GetResourceManagerWithEnv(env);
    if (!resourceManager) return false;

    jstring path = env->NewStringUTF(resourcePath);
    if (!path || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (path) env->DeleteLocalRef(path);
        env->DeleteLocalRef(resourceManager);
        return false;
    }

    jobject resourceLocation = env->NewObject(g_tntVisualJNI.resourceLocationClass, g_tntVisualJNI.mResourceLocationCtor, path);
    env->DeleteLocalRef(path);
    if (!resourceLocation || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (resourceLocation) env->DeleteLocalRef(resourceLocation);
        env->DeleteLocalRef(resourceManager);
        return false;
    }

    jintArray pixelArray = (jintArray)env->CallStaticObjectMethod(
        g_tntVisualJNI.textureUtilClass,
        g_tntVisualJNI.mTextureUtilLoadTexturePixels,
        resourceManager,
        resourceLocation);

    env->DeleteLocalRef(resourceLocation);
    env->DeleteLocalRef(resourceManager);
    if (!pixelArray || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (pixelArray) env->DeleteLocalRef(pixelArray);
        return false;
    }

    jsize pixelCount = env->GetArrayLength(pixelArray);
    if (pixelCount <= 0 || env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(pixelArray);
        return false;
    }

    pixels.resize((size_t)pixelCount);
    env->GetIntArrayRegion(pixelArray, 0, pixelCount, pixels.data());
    bool ok = !env->ExceptionCheck();
    if (!ok) {
        env->ExceptionClear();
        pixels.clear();
    }

    env->DeleteLocalRef(pixelArray);
    return ok;
}

bool BindBlocksAtlasWithEnv(JNIEnv* env) {
    if (!env || !g_tntVisualJNI.blocksAtlasResource) return false;

    jobject textureManager = GetTextureManagerWithEnv(env);
    if (!textureManager) return false;

    env->CallVoidMethod(textureManager, g_tntVisualJNI.mTextureManagerBindTexture, g_tntVisualJNI.blocksAtlasResource);
    bool ok = !env->ExceptionCheck();
    if (!ok) env->ExceptionClear();
    env->DeleteLocalRef(textureManager);
    return ok;
}

bool ApplyWheatStage1OverrideWithEnv(JNIEnv* env) {
    if (!env || !g_tntVisualJNI.inited || g_tntVisualJNI.failed) return false;
    static ULONGLONG s_lastWheatDebugLogMs = 0;

    if (!BindBlocksAtlasWithEnv(env)) {
        ULONGLONG now = GetTickCount64();
        if ((now - s_lastWheatDebugLogMs) >= 2000) {
            DebugLog("Wheat override aborted: BindBlocksAtlasWithEnv failed");
            s_lastWheatDebugLogMs = now;
        }
        return false;
    }

    jobject textureMap = GetBlockTextureMapWithEnv(env);
    if (!textureMap) {
        ULONGLONG now = GetTickCount64();
        if ((now - s_lastWheatDebugLogMs) >= 2000) {
            DebugLog("Wheat override aborted: GetBlockTextureMapWithEnv failed");
            s_lastWheatDebugLogMs = now;
        }
        return false;
    }

    jobject stageSprites[sizeof(kWheatStageSpriteNames) / sizeof(kWheatStageSpriteNames[0])] = {};
    ResolveWheatStageSprites(env, textureMap, stageSprites, (int)(sizeof(stageSprites) / sizeof(stageSprites[0])));

    jobject sourceSprite = stageSprites[kForcedWheatRenderStage];
    if (!sourceSprite) {
        ULONGLONG now = GetTickCount64();
        if ((now - s_lastWheatDebugLogMs) >= 2000) {
            std::string summary;
            AppendResolvedWheatStageSummary(env, stageSprites, (int)(sizeof(stageSprites) / sizeof(stageSprites[0])), summary);
            DebugLog("Wheat override could not resolve stage %d sprite %s", kForcedWheatRenderStage, summary.c_str());
            s_lastWheatDebugLogMs = now;
        }
        ReleaseResolvedSprites(env, stageSprites, (int)(sizeof(stageSprites) / sizeof(stageSprites[0])));
        env->DeleteLocalRef(textureMap);
        return false;
    }

    jint sourceWidth = env->CallIntMethod(sourceSprite, g_tntVisualJNI.mSpriteGetWidth);
    jint sourceHeight = env->CallIntMethod(sourceSprite, g_tntVisualJNI.mSpriteGetHeight);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        ULONGLONG now = GetTickCount64();
        if ((now - s_lastWheatDebugLogMs) >= 2000) {
            DebugLog("Wheat override aborted: failed to read stage %d sprite dimensions", kForcedWheatRenderStage);
            s_lastWheatDebugLogMs = now;
        }
        ReleaseResolvedSprites(env, stageSprites, (int)(sizeof(stageSprites) / sizeof(stageSprites[0])));
        env->DeleteLocalRef(textureMap);
        return false;
    }

    std::vector<jint> sourcePixels;
    if (!LoadTexturePixelsWithEnv(env, kWheatStageTextureResourcePaths[kForcedWheatRenderStage], sourcePixels)) {
        ULONGLONG now = GetTickCount64();
        if ((now - s_lastWheatDebugLogMs) >= 2000) {
            DebugLog("Wheat override aborted: failed to load stage %d texture resource", kForcedWheatRenderStage);
            s_lastWheatDebugLogMs = now;
        }
        ReleaseResolvedSprites(env, stageSprites, (int)(sizeof(stageSprites) / sizeof(stageSprites[0])));
        env->DeleteLocalRef(textureMap);
        return false;
    }

    jint mipmapLevels = env->GetIntField(textureMap, g_tntVisualJNI.fTextureMapMipmapLevels);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        mipmapLevels = 0;
    }

    bool success = true;
    for (int i = 0; i < (int)(sizeof(stageSprites) / sizeof(stageSprites[0])); ++i) {
        jobject targetSprite = stageSprites[i];
        if (!targetSprite) {
            success = false;
            continue;
        }

        jint targetWidth = env->CallIntMethod(targetSprite, g_tntVisualJNI.mSpriteGetWidth);
        jint targetHeight = env->CallIntMethod(targetSprite, g_tntVisualJNI.mSpriteGetHeight);
        jobjectArray frameData = nullptr;
        bool dimsOk = !env->ExceptionCheck() && targetWidth == sourceWidth && targetHeight == sourceHeight;
        bool frameBuilt = dimsOk && BuildMipFrameDataWithEnv(env, sourcePixels, targetWidth, targetHeight, mipmapLevels, frameData);
        bool uploadOk = frameBuilt && UploadFrameToSpriteRegion(env, targetSprite, frameData);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            uploadOk = false;
        }
        if (frameData) env->DeleteLocalRef(frameData);
        if (!uploadOk) {
            success = false;
            ULONGLONG failNow = GetTickCount64();
            if ((failNow - s_lastWheatDebugLogMs) >= 2000) {
                std::string spriteName;
                TryGetAtlasSpriteName(env, targetSprite, spriteName);
                DebugLog("Wheat override stage=%d failed dimsOk=%d frameBuilt=%d uploadOk=%d target=%dx%d source=%dx%d mipLevels=%d sprite=%s",
                    i,
                    dimsOk ? 1 : 0,
                    frameBuilt ? 1 : 0,
                    uploadOk ? 1 : 0,
                    targetWidth,
                    targetHeight,
                    sourceWidth,
                    sourceHeight,
                    mipmapLevels,
                    spriteName.empty() ? "unknown" : spriteName.c_str());
                s_lastWheatDebugLogMs = failNow;
            }
        }
    }

    ULONGLONG now = GetTickCount64();
    if ((!success || (now - s_lastWheatDebugLogMs) >= 2000) && (success || s_lastWheatDebugLogMs == 0)) {
        std::string summary;
        AppendResolvedWheatStageSummary(env, stageSprites, (int)(sizeof(stageSprites) / sizeof(stageSprites[0])), summary);
        DebugLog("Wheat override apply success=%d source=%dx%d %s", success ? 1 : 0, sourceWidth, sourceHeight, summary.c_str());
        s_lastWheatDebugLogMs = now;
    }

    ReleaseResolvedSprites(env, stageSprites, (int)(sizeof(stageSprites) / sizeof(stageSprites[0])));
    env->DeleteLocalRef(textureMap);
    return success;
}

bool RestoreWheatStageOverrideWithEnv(JNIEnv* env) {
    if (!env || !g_tntVisualJNI.inited || g_tntVisualJNI.failed) return false;
    if (!BindBlocksAtlasWithEnv(env)) return false;

    jobject textureMap = GetBlockTextureMapWithEnv(env);
    if (!textureMap) return false;

    jobject stageSprites[sizeof(kWheatStageSpriteNames) / sizeof(kWheatStageSpriteNames[0])] = {};
    ResolveWheatStageSprites(env, textureMap, stageSprites, (int)(sizeof(stageSprites) / sizeof(stageSprites[0])));

    jint mipmapLevels = env->GetIntField(textureMap, g_tntVisualJNI.fTextureMapMipmapLevels);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        mipmapLevels = 0;
    }

    bool success = true;
    for (int i = 0; i < (int)(sizeof(stageSprites) / sizeof(stageSprites[0])); ++i) {
        jobject targetSprite = stageSprites[i];
        if (!targetSprite) {
            success = false;
            continue;
        }

        jint targetWidth = env->CallIntMethod(targetSprite, g_tntVisualJNI.mSpriteGetWidth);
        jint targetHeight = env->CallIntMethod(targetSprite, g_tntVisualJNI.mSpriteGetHeight);
        std::vector<jint> stagePixels;
        jobjectArray frameData = nullptr;
        bool uploadOk = !env->ExceptionCheck() &&
            LoadTexturePixelsWithEnv(env, kWheatStageTextureResourcePaths[i], stagePixels) &&
            BuildMipFrameDataWithEnv(env, stagePixels, targetWidth, targetHeight, mipmapLevels, frameData) &&
            UploadFrameToSpriteRegion(env, targetSprite, frameData);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            uploadOk = false;
        }
        if (!uploadOk) success = false;
        if (frameData) env->DeleteLocalRef(frameData);
    }

    ReleaseResolvedSprites(env, stageSprites, (int)(sizeof(stageSprites) / sizeof(stageSprites[0])));
    env->DeleteLocalRef(textureMap);
    return success;
}

bool EnsureBlankBeaconTextureWithEnv(JNIEnv* env) {
    if (!env) return false;
    if (g_tntVisualJNI.blankBeaconTexture) return true;

    jobject blankTexture = env->NewObject(g_tntVisualJNI.dynamicTextureClass, g_tntVisualJNI.mDynamicTextureCtor, 1, 1);
    if (!blankTexture || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (blankTexture) env->DeleteLocalRef(blankTexture);
        return false;
    }

    jintArray pixels = (jintArray)env->CallObjectMethod(blankTexture, g_tntVisualJNI.mDynamicTextureGetPixels);
    if (!pixels || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (pixels) env->DeleteLocalRef(pixels);
        env->DeleteLocalRef(blankTexture);
        return false;
    }

    jint transparent = 0;
    env->SetIntArrayRegion(pixels, 0, 1, &transparent);
    env->CallVoidMethod(blankTexture, g_tntVisualJNI.mDynamicTextureUpload);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(pixels);
        env->DeleteLocalRef(blankTexture);
        return false;
    }

    g_tntVisualJNI.blankBeaconTexture = env->NewGlobalRef(blankTexture);
    env->DeleteLocalRef(pixels);
    env->DeleteLocalRef(blankTexture);
    if (!g_tntVisualJNI.blankBeaconTexture) {
        env->ExceptionClear();
        return false;
    }

    return true;
}

bool ApplyBeaconBeamOverrideWithEnv(JNIEnv* env) {
    if (!env || !g_tntVisualJNI.beaconBeamResource || !EnsureBlankBeaconTextureWithEnv(env)) return false;

    jobject textureManager = GetTextureManagerWithEnv(env);
    if (!textureManager) return false;

    jobject currentTexture = env->CallObjectMethod(textureManager, g_tntVisualJNI.mTextureManagerGetTexture, g_tntVisualJNI.beaconBeamResource);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        if (currentTexture) env->DeleteLocalRef(currentTexture);
        env->DeleteLocalRef(textureManager);
        return false;
    }

    bool alreadyApplied = currentTexture && env->IsSameObject(currentTexture, g_tntVisualJNI.blankBeaconTexture);
    bool ok = alreadyApplied;
    if (!alreadyApplied) {
        jboolean loaded = env->CallBooleanMethod(
            textureManager,
            g_tntVisualJNI.mTextureManagerLoadTexture,
            g_tntVisualJNI.beaconBeamResource,
            g_tntVisualJNI.blankBeaconTexture);
        ok = !env->ExceptionCheck() && loaded == JNI_TRUE;
        if (env->ExceptionCheck()) env->ExceptionClear();
    }

    if (currentTexture) env->DeleteLocalRef(currentTexture);
    env->DeleteLocalRef(textureManager);
    return ok;
}

bool RestoreBeaconBeamOverrideWithEnv(JNIEnv* env) {
    if (!env || !g_tntVisualJNI.beaconBeamResource) return false;

    jobject textureManager = GetTextureManagerWithEnv(env);
    if (!textureManager) return false;

    jobject simpleTexture = env->NewObject(g_tntVisualJNI.simpleTextureClass, g_tntVisualJNI.mSimpleTextureCtor, g_tntVisualJNI.beaconBeamResource);
    if (!simpleTexture || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (simpleTexture) env->DeleteLocalRef(simpleTexture);
        env->DeleteLocalRef(textureManager);
        return false;
    }

    jboolean loaded = env->CallBooleanMethod(
        textureManager,
        g_tntVisualJNI.mTextureManagerLoadTexture,
        g_tntVisualJNI.beaconBeamResource,
        simpleTexture);
    bool ok = !env->ExceptionCheck() && loaded == JNI_TRUE;
    if (env->ExceptionCheck()) env->ExceptionClear();

    env->DeleteLocalRef(simpleTexture);
    env->DeleteLocalRef(textureManager);
    return ok;
}

void UpdateTntVisualOverridesOnRenderThread() {
    static int s_lastLoggedWheatDesired = -1;
    static int s_lastLoggedBeaconDesired = -1;
    static int s_lastLoggedRestoreOnly = -1;
    static int s_lastLoggedTntTagActive = -1;

    bool restoreOnly = InterlockedCompareExchange(&g_tntVisualRestoreRequested, 0, 0) != 0;
    bool tntTagActive = InterlockedCompareExchange(&g_tntTagGameActive, 0, 0) != 0;
    if (!g_tntVisualJNI.inited || g_tntVisualJNI.failed) {
        if (restoreOnly) InterlockedExchange(&g_tntVisualRestoreCompleted, 1);
        return;
    }

    ULONGLONG now = GetTickCount64();
    JNIEnv* env = GetJNIEnvForCurrentThread();
    if (!env) return;

    bool shouldApplyWheatOverride = !restoreOnly && tntTagActive && g_guiExtrasForceWheatStage1;
    bool shouldApplyBeaconOverride = !restoreOnly && tntTagActive && g_guiExtrasHideBeaconBeams;

    if (s_lastLoggedWheatDesired != (shouldApplyWheatOverride ? 1 : 0) ||
        s_lastLoggedBeaconDesired != (shouldApplyBeaconOverride ? 1 : 0) ||
        s_lastLoggedRestoreOnly != (restoreOnly ? 1 : 0) ||
        s_lastLoggedTntTagActive != (tntTagActive ? 1 : 0)) {
        DebugLog("Visual override state restoreOnly=%d tntTagActive=%d wheatDesired=%d beaconDesired=%d wheatApplied=%d beaconApplied=%d",
            restoreOnly ? 1 : 0,
            tntTagActive ? 1 : 0,
            shouldApplyWheatOverride ? 1 : 0,
            shouldApplyBeaconOverride ? 1 : 0,
            g_tntVisualJNI.wheatApplied ? 1 : 0,
            g_tntVisualJNI.beaconApplied ? 1 : 0);
        s_lastLoggedWheatDesired = shouldApplyWheatOverride ? 1 : 0;
        s_lastLoggedBeaconDesired = shouldApplyBeaconOverride ? 1 : 0;
        s_lastLoggedRestoreOnly = restoreOnly ? 1 : 0;
        s_lastLoggedTntTagActive = tntTagActive ? 1 : 0;
    }

    if (shouldApplyWheatOverride) {
        if (!g_tntVisualJNI.wheatApplied || (now - g_tntVisualJNI.lastWheatRefreshMs) >= kTntVisualRefreshIntervalMs) {
            if (ApplyWheatStage1OverrideWithEnv(env)) {
                g_tntVisualJNI.wheatApplied = true;
                g_tntVisualJNI.lastWheatRefreshMs = now;
            }
        }
    }
    else if (g_tntVisualJNI.wheatApplied && RestoreWheatStageOverrideWithEnv(env)) {
        g_tntVisualJNI.wheatApplied = false;
        g_tntVisualJNI.lastWheatRefreshMs = 0;
    }

    if (shouldApplyBeaconOverride) {
        if (!g_tntVisualJNI.beaconApplied || (now - g_tntVisualJNI.lastBeaconRefreshMs) >= kTntVisualRefreshIntervalMs) {
            if (ApplyBeaconBeamOverrideWithEnv(env)) {
                g_tntVisualJNI.beaconApplied = true;
                g_tntVisualJNI.lastBeaconRefreshMs = now;
            }
        }
    }
    else if (g_tntVisualJNI.beaconApplied && RestoreBeaconBeamOverrideWithEnv(env)) {
        g_tntVisualJNI.beaconApplied = false;
        g_tntVisualJNI.lastBeaconRefreshMs = 0;
    }

    if (restoreOnly && !g_tntVisualJNI.wheatApplied && !g_tntVisualJNI.beaconApplied) {
        InterlockedExchange(&g_tntVisualRestoreCompleted, 1);
    }
}

bool GetSeeBarriersWorldAndPlayer(JNIEnv* env, jobject& world, jobject& player) {
    world = nullptr;
    player = nullptr;
    if (!env || !g_seeBarriersJNI.mcClass || !g_seeBarriersJNI.mGetMC) return false;

    jobject mc = env->CallStaticObjectMethod(g_seeBarriersJNI.mcClass, g_seeBarriersJNI.mGetMC);
    if (!mc || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (mc) env->DeleteLocalRef(mc);
        return false;
    }

    world = env->GetObjectField(mc, g_seeBarriersJNI.fWorld);
    player = env->GetObjectField(mc, g_seeBarriersJNI.fThePlayer);
    env->DeleteLocalRef(mc);

    if (!world || !player || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (world) env->DeleteLocalRef(world);
        if (player) env->DeleteLocalRef(player);
        world = nullptr;
        player = nullptr;
        return false;
    }

    return true;
}

bool UpdateSeeBarriersRenderCameraFromRenderManager(JNIEnv* env) {
    if (!env || !g_seeBarriersJNI.mcClass || !g_seeBarriersJNI.mGetMC ||
        !g_seeBarriersJNI.mGetRenderManager ||
        !g_seeBarriersJNI.fRenderPosX || !g_seeBarriersJNI.fRenderPosY || !g_seeBarriersJNI.fRenderPosZ) {
        return false;
    }

    jobject mc = env->CallStaticObjectMethod(g_seeBarriersJNI.mcClass, g_seeBarriersJNI.mGetMC);
    if (!mc || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (mc) env->DeleteLocalRef(mc);
        return false;
    }

    jobject renderManager = env->CallObjectMethod(mc, g_seeBarriersJNI.mGetRenderManager);
    env->DeleteLocalRef(mc);
    if (!renderManager || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (renderManager) env->DeleteLocalRef(renderManager);
        return false;
    }

    double renderX = env->GetDoubleField(renderManager, g_seeBarriersJNI.fRenderPosX);
    double renderY = env->GetDoubleField(renderManager, g_seeBarriersJNI.fRenderPosY);
    double renderZ = env->GetDoubleField(renderManager, g_seeBarriersJNI.fRenderPosZ);
    env->DeleteLocalRef(renderManager);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return false;
    }

    g_seeBarriersJNI.renderCameraX = renderX;
    g_seeBarriersJNI.renderCameraY = renderY;
    g_seeBarriersJNI.renderCameraZ = renderZ;
    return true;
}

bool IsBarrierBlockAt(JNIEnv* env, jobject world, int x, int y, int z) {
    if (!env || !world) return false;

    jobject pos = env->NewObject(g_seeBarriersJNI.blockPosClass, g_seeBarriersJNI.mBlockPosCtor, x, y, z);
    if (!pos || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (pos) env->DeleteLocalRef(pos);
        return false;
    }

    jobject state = env->CallObjectMethod(world, g_seeBarriersJNI.mWorldGetBlockState, pos);
    env->DeleteLocalRef(pos);
    if (!state || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (state) env->DeleteLocalRef(state);
        return false;
    }

    jobject block = env->CallObjectMethod(state, g_seeBarriersJNI.mBlockStateGetBlock);
    env->DeleteLocalRef(state);
    if (!block || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (block) env->DeleteLocalRef(block);
        return false;
    }

    jint blockId = env->CallStaticIntMethod(g_seeBarriersJNI.blockClass, g_seeBarriersJNI.mBlockGetIdFromBlock, block);
    env->DeleteLocalRef(block);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return false;
    }

    return blockId == kBarrierBlockId;
}

void ResetSeeBarriersCache(JNIEnv* env) {
    ClearSeeBarriersDamageMarkers(env);
    if (env && g_seeBarriersJNI.cachedWorld) {
        env->DeleteGlobalRef(g_seeBarriersJNI.cachedWorld);
    }

    if (env) {
        for (auto& entry : g_seeBarriersJNI.chunkCaches) {
            if (entry.second.chunkRef) {
                env->DeleteGlobalRef(entry.second.chunkRef);
                entry.second.chunkRef = nullptr;
            }
        }
    }

    g_seeBarriersJNI.cachedWorld = nullptr;
    g_seeBarriersJNI.chunkCaches.clear();
    g_seeBarriersJNI.lastChunkSyncMs = 0;
    g_seeBarriersJNI.lastDamageMarkerRefreshMs = 0;
    g_seeBarriersJNI.frameOverlayDrawn = false;
}

void EnsureSeeBarriersWorldCache(JNIEnv* env, jobject world) {
    if (!env || !world) return;

    if (g_seeBarriersJNI.cachedWorld && !env->IsSameObject(g_seeBarriersJNI.cachedWorld, world)) {
        ResetSeeBarriersCache(env);
    }

    if (!g_seeBarriersJNI.cachedWorld) {
        g_seeBarriersJNI.cachedWorld = env->NewGlobalRef(world);
        if (!g_seeBarriersJNI.cachedWorld || env->ExceptionCheck()) {
            env->ExceptionClear();
            g_seeBarriersJNI.cachedWorld = nullptr;
        }
    }
}

long long MakeBarrierChunkKey(int chunkX, int chunkZ) {
    return ((long long)chunkX << 32) ^ (unsigned int)chunkZ;
}

int MakeBarrierLocalKey(int localX, int y, int localZ) {
    return ((y & 0xff) << 8) | ((localZ & 0x0f) << 4) | (localX & 0x0f);
}

int FloorDiv16(int value) {
    return value >= 0 ? (value >> 4) : -(((-value) + 15) >> 4);
}

long long BarrierChunkDistanceScore(const BarrierChunkCache& chunk, int centerChunkX, int centerChunkZ) {
    long long dx = (long long)chunk.chunkX - (long long)centerChunkX;
    long long dz = (long long)chunk.chunkZ - (long long)centerChunkZ;
    return (dx * dx) + (dz * dz);
}

void ReleaseBarrierChunkCache(JNIEnv* env, BarrierChunkCache& chunk) {
    if (env && chunk.chunkRef) {
        env->DeleteGlobalRef(chunk.chunkRef);
        chunk.chunkRef = nullptr;
    }
}

void ResetBarrierChunkScan(BarrierChunkCache& chunk) {
    chunk.nextSectionIndex = 0;
    chunk.complete = false;
    chunk.rescanning = false;
    chunk.completedAtMs = 0;
    chunk.barriers.clear();
    chunk.barrierLocalKeys.clear();
    chunk.pendingBarriers.clear();
    chunk.pendingLocalKeys.clear();
}

void StartBarrierChunkRescan(BarrierChunkCache& chunk) {
    if (chunk.rescanning) return;
    chunk.rescanning = true;
    chunk.complete = false;
    chunk.nextSectionIndex = 0;
    chunk.pendingBarriers.clear();
    chunk.pendingLocalKeys.clear();
}

void FinishBarrierChunkScan(BarrierChunkCache& chunk, ULONGLONG now) {
    chunk.complete = true;
    chunk.nextSectionIndex = kSeeBarriersSectionsPerChunk;
    chunk.completedAtMs = now;
    if (chunk.rescanning) {
        chunk.barriers.swap(chunk.pendingBarriers);
        chunk.barrierLocalKeys.swap(chunk.pendingLocalKeys);
        chunk.pendingBarriers.clear();
        chunk.pendingLocalKeys.clear();
        chunk.rescanning = false;
    }
}

void AddBarrierToChunk(BarrierChunkCache& chunk, int localX, int y, int localZ) {
    int localKey = MakeBarrierLocalKey(localX, y, localZ);
    std::unordered_set<int>& keys = chunk.rescanning ? chunk.pendingLocalKeys : chunk.barrierLocalKeys;
    if (!keys.insert(localKey).second) return;

    BarrierBlockPos pos;
    pos.x = (chunk.chunkX << 4) + localX;
    pos.y = y;
    pos.z = (chunk.chunkZ << 4) + localZ;
    std::vector<BarrierBlockPos>& target = chunk.rescanning ? chunk.pendingBarriers : chunk.barriers;
    target.push_back(pos);
}

jobject GetSeeBarriersLoadedChunkList(JNIEnv* env, jobject world) {
    if (!env || !world || !g_seeBarriersJNI.chunkCacheReady) return nullptr;

    jobject provider = env->GetObjectField(world, g_seeBarriersJNI.fWorldClientChunkProvider);
    if (!provider || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (provider) env->DeleteLocalRef(provider);
        return nullptr;
    }

    jobject chunks = env->GetObjectField(provider, g_seeBarriersJNI.fLoadedChunks);
    env->DeleteLocalRef(provider);
    if (!chunks || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (chunks) env->DeleteLocalRef(chunks);
        return nullptr;
    }

    return chunks;
}

bool SyncLoadedBarrierChunks(JNIEnv* env, jobject world, ULONGLONG now) {
    if (!env || !world || !g_seeBarriersJNI.chunkCacheReady) return false;

    jobject chunks = GetSeeBarriersLoadedChunkList(env, world);
    if (!chunks) return false;

    jint chunkCount = env->CallIntMethod(chunks, g_seeBarriersJNI.mListSize);
    if (env->ExceptionCheck() || chunkCount <= 0) {
        env->ExceptionClear();
        for (auto& entry : g_seeBarriersJNI.chunkCaches) {
            ReleaseBarrierChunkCache(env, entry.second);
        }
        g_seeBarriersJNI.chunkCaches.clear();
        g_seeBarriersJNI.lastChunkSyncMs = now;
        env->DeleteLocalRef(chunks);
        return false;
    }

    std::unordered_set<long long> loadedKeys;
    loadedKeys.reserve((size_t)chunkCount);
    for (jint chunkIndex = 0; chunkIndex < chunkCount; ++chunkIndex) {
        jobject chunk = env->CallObjectMethod(chunks, g_seeBarriersJNI.mListGet, chunkIndex);
        if (!chunk || env->ExceptionCheck()) {
            env->ExceptionClear();
            if (chunk) env->DeleteLocalRef(chunk);
            continue;
        }

        jint chunkX = env->GetIntField(chunk, g_seeBarriersJNI.fChunkX);
        jint chunkZ = env->GetIntField(chunk, g_seeBarriersJNI.fChunkZ);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            env->DeleteLocalRef(chunk);
            continue;
        }

        long long key = MakeBarrierChunkKey((int)chunkX, (int)chunkZ);
        loadedKeys.insert(key);

        auto cacheIt = g_seeBarriersJNI.chunkCaches.find(key);
        if (cacheIt == g_seeBarriersJNI.chunkCaches.end()) {
            BarrierChunkCache cache;
            cache.chunkX = (int)chunkX;
            cache.chunkZ = (int)chunkZ;
            cacheIt = g_seeBarriersJNI.chunkCaches.emplace(key, std::move(cache)).first;
        }

        BarrierChunkCache& cache = cacheIt->second;
        bool sameChunkObject = cache.chunkRef && env->IsSameObject(cache.chunkRef, chunk);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            sameChunkObject = false;
        }

        if (!sameChunkObject) {
            bool hadPreviousChunkRef = cache.chunkRef != nullptr;
            ReleaseBarrierChunkCache(env, cache);
            cache.chunkRef = env->NewGlobalRef(chunk);
            if (!cache.chunkRef || env->ExceptionCheck()) {
                env->ExceptionClear();
                cache.chunkRef = nullptr;
            }
            else if (hadPreviousChunkRef) {
                ResetBarrierChunkScan(cache);
            }
        }

        env->DeleteLocalRef(chunk);
    }

    for (auto it = g_seeBarriersJNI.chunkCaches.begin(); it != g_seeBarriersJNI.chunkCaches.end();) {
        if (loadedKeys.find(it->first) == loadedKeys.end()) {
            ReleaseBarrierChunkCache(env, it->second);
            it = g_seeBarriersJNI.chunkCaches.erase(it);
        }
        else {
            ++it;
        }
    }

    g_seeBarriersJNI.lastChunkSyncMs = now;
    env->DeleteLocalRef(chunks);
    return true;
}

bool IsSeeBarriersScanBudgetExpired(const LARGE_INTEGER& startCounter) {
    if (g_perfFreq.QuadPart <= 0) return false;

    LARGE_INTEGER nowCounter = {};
    QueryPerformanceCounter(&nowCounter);
    double elapsedMs = ((double)(nowCounter.QuadPart - startCounter.QuadPart) * 1000.0) / (double)g_perfFreq.QuadPart;
    return elapsedMs >= kSeeBarriersScanBudgetMs;
}

bool ScanBarrierChunkSection(JNIEnv* env, BarrierChunkCache& chunk, int sectionIndex) {
    if (!env || !chunk.chunkRef || sectionIndex < 0 || sectionIndex >= kSeeBarriersSectionsPerChunk) return false;

    jobjectArray sections = (jobjectArray)env->GetObjectField(chunk.chunkRef, g_seeBarriersJNI.fChunkStorageArrays);
    if (!sections || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (sections) env->DeleteLocalRef(sections);
        return false;
    }

    jsize sectionCount = env->GetArrayLength(sections);
    if (env->ExceptionCheck() || sectionIndex >= sectionCount) {
        env->ExceptionClear();
        env->DeleteLocalRef(sections);
        return true;
    }

    jobject section = env->GetObjectArrayElement(sections, sectionIndex);
    env->DeleteLocalRef(sections);
    if (!section || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (section) env->DeleteLocalRef(section);
        return true;
    }

    jcharArray dataArray = (jcharArray)env->CallObjectMethod(section, g_seeBarriersJNI.mStorageGetData);
    env->DeleteLocalRef(section);
    if (!dataArray || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (dataArray) env->DeleteLocalRef(dataArray);
        return true;
    }

    jsize dataLength = env->GetArrayLength(dataArray);
    if (env->ExceptionCheck() || dataLength < kSeeBarriersSectionBlockCount) {
        env->ExceptionClear();
        env->DeleteLocalRef(dataArray);
        return true;
    }

    std::array<jchar, kSeeBarriersSectionBlockCount> stateIds = {};
    env->GetCharArrayRegion(dataArray, 0, kSeeBarriersSectionBlockCount, stateIds.data());
    env->DeleteLocalRef(dataArray);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return true;
    }

    int sectionY = sectionIndex << 4;
    for (int index = 0; index < kSeeBarriersSectionBlockCount; ++index) {
        int stateId = (int)stateIds[(size_t)index];
        if ((stateId >> 4) != kBarrierBlockId) continue;

        int localX = index & 15;
        int localZ = (index >> 4) & 15;
        int y = sectionY + ((index >> 8) & 15);
        AddBarrierToChunk(chunk, localX, y, localZ);
    }

    return true;
}

void ScanBarrierChunksBudgeted(JNIEnv* env, jobject world, int centerChunkX, int centerChunkZ, ULONGLONG now) {
    (void)world;
    if (!env || !world || !g_seeBarriersJNI.chunkCacheReady || g_seeBarriersJNI.chunkCaches.empty()) return;

    struct ChunkWorkItem {
        long long key = 0;
        long long distance = 0;
    };

    std::vector<ChunkWorkItem> workItems;
    workItems.reserve(g_seeBarriersJNI.chunkCaches.size());
    for (auto& entry : g_seeBarriersJNI.chunkCaches) {
        BarrierChunkCache& chunk = entry.second;
        if (chunk.complete && chunk.completedAtMs != 0 && (now - chunk.completedAtMs) >= kSeeBarriersChunkRescanIntervalMs) {
            StartBarrierChunkRescan(chunk);
        }
        if (!chunk.complete) {
            ChunkWorkItem item;
            item.key = entry.first;
            item.distance = BarrierChunkDistanceScore(chunk, centerChunkX, centerChunkZ);
            workItems.push_back(item);
        }
    }

    if (workItems.empty()) return;
    std::sort(workItems.begin(), workItems.end(), [](const ChunkWorkItem& a, const ChunkWorkItem& b) {
        return a.distance < b.distance;
    });

    LARGE_INTEGER startCounter = {};
    QueryPerformanceCounter(&startCounter);

    int remainingSections = kSeeBarriersScanSectionsPerFrame;
    int sectionsSinceTimePoll = 0;
    for (const ChunkWorkItem& item : workItems) {
        auto chunkIt = g_seeBarriersJNI.chunkCaches.find(item.key);
        if (chunkIt == g_seeBarriersJNI.chunkCaches.end()) continue;

        BarrierChunkCache& chunk = chunkIt->second;
        if (!chunk.chunkRef) continue;

        while (!chunk.complete && remainingSections > 0) {
            int sectionIndex = chunk.nextSectionIndex++;
            if (sectionIndex >= kSeeBarriersSectionsPerChunk) {
                FinishBarrierChunkScan(chunk, now);
                break;
            }

            ScanBarrierChunkSection(env, chunk, sectionIndex);

            --remainingSections;
            ++sectionsSinceTimePoll;
            if (sectionsSinceTimePoll >= 4) {
                sectionsSinceTimePoll = 0;
                if (IsSeeBarriersScanBudgetExpired(startCounter)) return;
            }
        }

        if (!chunk.complete && chunk.nextSectionIndex >= kSeeBarriersSectionsPerChunk) {
            FinishBarrierChunkScan(chunk, now);
        }

        if (remainingSections <= 0 || IsSeeBarriersScanBudgetExpired(startCounter)) return;
    }
}

int MakeSeeBarriersDamageMarkerId(const BarrierBlockPos& pos) {
    uint32_t h = 2166136261u;
    h = (h ^ (uint32_t)pos.x) * 16777619u;
    h = (h ^ (uint32_t)pos.y) * 16777619u;
    h = (h ^ (uint32_t)pos.z) * 16777619u;
    return (int)(0x5E000000u | (h & 0x00FFFFFFu));
}

jobject GetSeeBarriersRenderGlobal(JNIEnv* env) {
    if (!env || !g_seeBarriersJNI.mcClass || !g_seeBarriersJNI.mGetMC ||
        !g_seeBarriersJNI.fRenderGlobal) {
        return nullptr;
    }

    jobject mc = env->CallStaticObjectMethod(g_seeBarriersJNI.mcClass, g_seeBarriersJNI.mGetMC);
    if (!mc || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (mc) env->DeleteLocalRef(mc);
        return nullptr;
    }

    jobject renderGlobal = env->GetObjectField(mc, g_seeBarriersJNI.fRenderGlobal);
    env->DeleteLocalRef(mc);
    if (!renderGlobal || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (renderGlobal) env->DeleteLocalRef(renderGlobal);
        return nullptr;
    }

    return renderGlobal;
}

bool SetSeeBarriersDamageMarker(JNIEnv* env, jobject renderGlobal, const BarrierBlockPos& pos, int stage) {
    if (!env || !renderGlobal || !g_seeBarriersJNI.blockPosClass ||
        !g_seeBarriersJNI.mBlockPosCtor || !g_seeBarriersJNI.mRenderGlobalSetBlockDamage) {
        return false;
    }

    jobject blockPos = env->NewObject(
        g_seeBarriersJNI.blockPosClass,
        g_seeBarriersJNI.mBlockPosCtor,
        (jint)pos.x,
        (jint)pos.y,
        (jint)pos.z);
    if (!blockPos || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (blockPos) env->DeleteLocalRef(blockPos);
        return false;
    }

    env->CallVoidMethod(
        renderGlobal,
        g_seeBarriersJNI.mRenderGlobalSetBlockDamage,
        (jint)MakeSeeBarriersDamageMarkerId(pos),
        blockPos,
        (jint)stage);
    env->DeleteLocalRef(blockPos);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return false;
    }

    return true;
}

void ClearSeeBarriersDamageMarkers(JNIEnv* env) {
    if (!env || g_seeBarriersJNI.activeDamageMarkers.empty()) return;

    jobject renderGlobal = GetSeeBarriersRenderGlobal(env);
    if (!renderGlobal) {
        g_seeBarriersJNI.activeDamageMarkers.clear();
        return;
    }

    for (const auto& entry : g_seeBarriersJNI.activeDamageMarkers) {
        SetSeeBarriersDamageMarker(env, renderGlobal, entry.second, -1);
    }
    g_seeBarriersJNI.activeDamageMarkers.clear();
    g_seeBarriersJNI.lastDamageMarkerRefreshMs = 0;
    env->DeleteLocalRef(renderGlobal);
}

bool IsSeeBarriersMarkerInRange(const BarrierBlockPos& pos, int range, bool infiniteRange, double playerX, double playerZ, double& distanceSq) {
    double dx = ((double)pos.x + 0.5) - playerX;
    double dz = ((double)pos.z + 0.5) - playerZ;
    distanceSq = (dx * dx) + (dz * dz);
    if (infiniteRange) return true;

    double radius = (double)range + 0.5;
    return distanceSq <= radius * radius;
}

jobject GetSeeBarriersWorldStateAt(JNIEnv* env, jobject world, const BarrierBlockPos& pos) {
    if (!env || !world || !g_seeBarriersJNI.blockPosClass ||
        !g_seeBarriersJNI.mBlockPosCtor || !g_seeBarriersJNI.mWorldGetBlockState) {
        return nullptr;
    }

    jobject blockPos = env->NewObject(
        g_seeBarriersJNI.blockPosClass,
        g_seeBarriersJNI.mBlockPosCtor,
        (jint)pos.x,
        (jint)pos.y,
        (jint)pos.z);
    if (!blockPos || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (blockPos) env->DeleteLocalRef(blockPos);
        return nullptr;
    }

    jobject state = env->CallObjectMethod(world, g_seeBarriersJNI.mWorldGetBlockState, blockPos);
    env->DeleteLocalRef(blockPos);
    if (!state || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (state) env->DeleteLocalRef(state);
        return nullptr;
    }

    return state;
}

jobject GetSeeBarriersActualWorldStateAt(JNIEnv* env, jobject world, const BarrierBlockPos& pos, jobject state) {
    if (!env || !world || !state || !g_seeBarriersJNI.blockPosClass ||
        !g_seeBarriersJNI.mBlockPosCtor || !g_seeBarriersJNI.mBlockStateGetBlock ||
        !g_seeBarriersBlockGetActualState) {
        return nullptr;
    }

    jobject block = env->CallObjectMethod(state, g_seeBarriersJNI.mBlockStateGetBlock);
    if (!block || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (block) env->DeleteLocalRef(block);
        return nullptr;
    }

    jobject blockPos = env->NewObject(
        g_seeBarriersJNI.blockPosClass,
        g_seeBarriersJNI.mBlockPosCtor,
        (jint)pos.x,
        (jint)pos.y,
        (jint)pos.z);
    if (!blockPos || env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(block);
        if (blockPos) env->DeleteLocalRef(blockPos);
        return nullptr;
    }

    jobject actualState = env->CallObjectMethod(block, g_seeBarriersBlockGetActualState, state, world, blockPos);
    env->DeleteLocalRef(blockPos);
    env->DeleteLocalRef(block);
    if (!actualState || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (actualState) env->DeleteLocalRef(actualState);
        return nullptr;
    }

    return actualState;
}

void LogSeeBarriersDispatcherSample(JNIEnv* env, jobject world, const BarrierBlockPos& pos, jobject rawState, jobject visibleModel, jobject modelShapes) {
    if (!env || !world || !rawState || !visibleModel || !modelShapes ||
        !g_slGetMC || !g_seeBarriersMinecraftGetBlockRendererDispatcher ||
        !g_seeBarriersBlockRendererGetModelForWorldState || !g_seeBarriersBlockShouldSideBeRendered ||
        !g_seeBarriersDirectionValues || !g_seeBarriersJNI.blockPosClass || !g_seeBarriersJNI.mBlockPosCtor ||
        !g_seeBarriersJNI.mBlockStateGetBlock) {
        return;
    }

    static ULONGLONG s_lastSampleLogMs = 0;
    ULONGLONG now = GetTickCount64();
    if ((now - s_lastSampleLogMs) < 1500) return;

    jobject mc = env->CallStaticObjectMethod(g_slMcClass, g_slGetMC);
    if (!mc || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (mc) env->DeleteLocalRef(mc);
        return;
    }

    jobject dispatcher = env->CallObjectMethod(mc, g_seeBarriersMinecraftGetBlockRendererDispatcher);
    env->DeleteLocalRef(mc);
    if (!dispatcher || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (dispatcher) env->DeleteLocalRef(dispatcher);
        return;
    }

    jobject blockPos = env->NewObject(
        g_seeBarriersJNI.blockPosClass,
        g_seeBarriersJNI.mBlockPosCtor,
        (jint)pos.x,
        (jint)pos.y,
        (jint)pos.z);
    if (!blockPos || env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(dispatcher);
        if (blockPos) env->DeleteLocalRef(blockPos);
        return;
    }

    jobject dispatcherModel = env->CallObjectMethod(dispatcher, g_seeBarriersBlockRendererGetModelForWorldState, rawState, world, blockPos);
    if (!dispatcherModel || env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(blockPos);
        env->DeleteLocalRef(dispatcher);
        if (dispatcherModel) env->DeleteLocalRef(dispatcherModel);
        return;
    }

    jobject rawBlock = env->CallObjectMethod(rawState, g_seeBarriersJNI.mBlockStateGetBlock);
    if (!rawBlock || env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(dispatcherModel);
        env->DeleteLocalRef(blockPos);
        env->DeleteLocalRef(dispatcher);
        if (rawBlock) env->DeleteLocalRef(rawBlock);
        return;
    }

    jboolean modelMatchesVisible = env->IsSameObject(dispatcherModel, visibleModel);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        modelMatchesVisible = JNI_FALSE;
    }

    jobject actualState = GetSeeBarriersActualWorldStateAt(env, world, pos, rawState);
    jboolean actualSameAsRaw = JNI_FALSE;
    if (actualState) {
        actualSameAsRaw = env->IsSameObject(actualState, rawState);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            actualSameAsRaw = JNI_FALSE;
        }
    }

    std::string sideBits;
    sideBits.reserve(6);
    jobjectArray directions = (jobjectArray)env->CallStaticObjectMethod(g_seeBarriersDirectionClass, g_seeBarriersDirectionValues);
    if (directions && !env->ExceptionCheck()) {
        jsize count = env->GetArrayLength(directions);
        for (jsize i = 0; i < count; ++i) {
            jobject direction = env->GetObjectArrayElement(directions, i);
            if (!direction || env->ExceptionCheck()) {
                env->ExceptionClear();
                if (direction) env->DeleteLocalRef(direction);
                sideBits.push_back('?');
                continue;
            }

            jobject sidePos = env->CallObjectMethod(blockPos, g_seeBarriersJNI.mBlockPosOffset, direction);
            if (!sidePos || env->ExceptionCheck()) {
                env->ExceptionClear();
                if (sidePos) env->DeleteLocalRef(sidePos);
                env->DeleteLocalRef(direction);
                sideBits.push_back('0');
                continue;
            }

            jboolean renderSide = env->CallBooleanMethod(rawBlock, g_seeBarriersBlockShouldSideBeRendered, world, sidePos, direction);
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                renderSide = JNI_FALSE;
            }

            sideBits.push_back(renderSide == JNI_TRUE ? '1' : '0');
            env->DeleteLocalRef(sidePos);
            env->DeleteLocalRef(direction);
        }
        env->DeleteLocalRef(directions);
    }
    else {
        if (env->ExceptionCheck()) env->ExceptionClear();
    }

    DebugLog("See Barriers dispatcher sample pos=%d,%d,%d modelMatchesVisible=%d actualSameAsRaw=%d sides=%s",
        pos.x,
        pos.y,
        pos.z,
        modelMatchesVisible == JNI_TRUE ? 1 : 0,
        actualSameAsRaw == JNI_TRUE ? 1 : 0,
        sideBits.empty() ? "-" : sideBits.c_str());
    s_lastSampleLogMs = now;

    if (actualState) env->DeleteLocalRef(actualState);
    env->DeleteLocalRef(rawBlock);
    env->DeleteLocalRef(dispatcherModel);
    env->DeleteLocalRef(blockPos);
    env->DeleteLocalRef(dispatcher);
}

void RefreshSeeBarriersObservedStateModelOverrides(JNIEnv* env, jobject world, ULONGLONG now) {
    if (!env || !world || !g_guiExtrasSeeBarriers || !g_seeBarriersMinecraftApplied ||
        !g_seeBarriersJNI.chunkCacheReady || g_seeBarriersJNI.chunkCaches.empty()) {
        return;
    }

    static ULONGLONG s_lastRefreshMs = 0;
    if ((now - s_lastRefreshMs) < 750) return;
    s_lastRefreshMs = now;

    if (!EnsureSeeBarriersModelOverrideJNI(env)) return;

    jobject modelShapes = GetSeeBarriersModelShapes(env);
    if (!modelShapes) return;

    jobject modelMap = env->GetObjectField(modelShapes, g_seeBarriersBlockModelShapesModelMap);
    if (!modelMap || env->ExceptionCheck()) {
        env->ExceptionClear();
        if (modelMap) env->DeleteLocalRef(modelMap);
        env->DeleteLocalRef(modelShapes);
        return;
    }

    jobject visibleModel = ResolveSeeBarriersVisibleModel(env, modelShapes);
    if (!visibleModel) {
        env->DeleteLocalRef(modelMap);
        env->DeleteLocalRef(modelShapes);
        return;
    }

    int examinedStates = 0;
    int appliedStates = 0;
    int newStates = 0;
    int replacedStates = 0;
    const int kMaxStatesPerRefresh = 192;
    bool sampled = false;

    for (const auto& entry : g_seeBarriersJNI.chunkCaches) {
        const BarrierChunkCache& chunk = entry.second;
        for (const BarrierBlockPos& pos : chunk.barriers) {
            if (examinedStates >= kMaxStatesPerRefresh) break;
            ++examinedStates;

            jobject state = GetSeeBarriersWorldStateAt(env, world, pos);
            if (!state) continue;

            jobject previousModel = env->CallObjectMethod(modelMap, g_seeBarriersMapPut, state, visibleModel);
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                if (previousModel) env->DeleteLocalRef(previousModel);
                env->DeleteLocalRef(state);
                continue;
            }

            ++appliedStates;
            if (!previousModel) {
                ++newStates;
            }
            else {
                jboolean sameAsVisible = env->IsSameObject(previousModel, visibleModel);
                if (env->ExceptionCheck()) {
                    env->ExceptionClear();
                }
                else if (sameAsVisible != JNI_TRUE) {
                    ++replacedStates;
                }
                env->DeleteLocalRef(previousModel);
            }

            jobject actualState = GetSeeBarriersActualWorldStateAt(env, world, pos, state);
            if (actualState) {
                jobject actualPreviousModel = env->CallObjectMethod(modelMap, g_seeBarriersMapPut, actualState, visibleModel);
                if (env->ExceptionCheck()) {
                    env->ExceptionClear();
                }
                else if (actualPreviousModel) {
                    env->DeleteLocalRef(actualPreviousModel);
                }
            }

            if (!sampled) {
                LogSeeBarriersDispatcherSample(env, world, pos, state, visibleModel, modelShapes);
                sampled = true;
            }

            if (actualState) env->DeleteLocalRef(actualState);
            env->DeleteLocalRef(state);
        }
        if (examinedStates >= kMaxStatesPerRefresh) break;
    }

    env->DeleteLocalRef(visibleModel);
    env->DeleteLocalRef(modelMap);
    env->DeleteLocalRef(modelShapes);

    static ULONGLONG s_lastLogMs = 0;
    if ((appliedStates > 0 || newStates > 0 || replacedStates > 0) &&
        (now - s_lastLogMs) >= 1500) {
        DebugLog("See Barriers observed-state model refresh examined=%d applied=%d new=%d replaced=%d",
            examinedStates,
            appliedStates,
            newStates,
            replacedStates);
        s_lastLogMs = now;
    }
}

void RefreshSeeBarriersDamageMarkers(JNIEnv* env, double playerX, double playerZ, ULONGLONG now) {
    if (!env || !g_guiExtrasSeeBarriers || !g_seeBarriersMinecraftApplied ||
        !g_seeBarriersJNI.chunkCacheReady || !g_seeBarriersJNI.mRenderGlobalSetBlockDamage) {
        return;
    }
    if ((now - g_seeBarriersJNI.lastDamageMarkerRefreshMs) < kSeeBarriersDamageMarkerRefreshIntervalMs) {
        return;
    }

    struct MarkerWorkItem {
        BarrierBlockPos pos;
        double distanceSq = 0.0;
    };

    int range = NormalizeSeeBarriersRange(g_guiSeeBarriersRange);
    bool infiniteRange = IsSeeBarriersRangeInfinite(range);
    std::vector<MarkerWorkItem> markers;

    for (const auto& entry : g_seeBarriersJNI.chunkCaches) {
        const BarrierChunkCache& chunk = entry.second;
        for (const BarrierBlockPos& pos : chunk.barriers) {
            double distanceSq = 0.0;
            if (!IsSeeBarriersMarkerInRange(pos, range, infiniteRange, playerX, playerZ, distanceSq)) continue;

            MarkerWorkItem item;
            item.pos = pos;
            item.distanceSq = distanceSq;
            markers.push_back(item);
        }
    }

    if (markers.empty() && g_seeBarriersJNI.activeDamageMarkers.empty()) {
        g_seeBarriersJNI.lastDamageMarkerRefreshMs = now;
        return;
    }

    if (markers.size() > (size_t)kSeeBarriersDamageMarkerMax) {
        std::nth_element(
            markers.begin(),
            markers.begin() + kSeeBarriersDamageMarkerMax,
            markers.end(),
            [](const MarkerWorkItem& a, const MarkerWorkItem& b) {
                return a.distanceSq < b.distanceSq;
            });
        markers.resize((size_t)kSeeBarriersDamageMarkerMax);
    }

    jobject renderGlobal = GetSeeBarriersRenderGlobal(env);
    if (!renderGlobal) return;

    std::unordered_set<int> refreshedIds;
    refreshedIds.reserve(markers.size() * 2 + 1);

    int refreshedCount = 0;
    for (const MarkerWorkItem& marker : markers) {
        int markerId = MakeSeeBarriersDamageMarkerId(marker.pos);
        refreshedIds.insert(markerId);
        if (SetSeeBarriersDamageMarker(env, renderGlobal, marker.pos, kSeeBarriersDamageMarkerStage)) {
            g_seeBarriersJNI.activeDamageMarkers[markerId] = marker.pos;
            ++refreshedCount;
        }
    }

    for (auto it = g_seeBarriersJNI.activeDamageMarkers.begin(); it != g_seeBarriersJNI.activeDamageMarkers.end();) {
        if (refreshedIds.find(it->first) == refreshedIds.end()) {
            SetSeeBarriersDamageMarker(env, renderGlobal, it->second, -1);
            it = g_seeBarriersJNI.activeDamageMarkers.erase(it);
        }
        else {
            ++it;
        }
    }

    g_seeBarriersJNI.lastDamageMarkerRefreshMs = now;
    env->DeleteLocalRef(renderGlobal);

    static ULONGLONG s_lastMarkerLogMs = 0;
    if ((now - s_lastMarkerLogMs) >= 2000) {
        DebugLog("See Barriers damage markers refreshed=%d active=%u candidates=%u cap=%d",
            refreshedCount,
            (unsigned int)g_seeBarriersJNI.activeDamageMarkers.size(),
            (unsigned int)markers.size(),
            kSeeBarriersDamageMarkerMax);
        s_lastMarkerLogMs = now;
    }
}

void UpdateSeeBarriersOnRenderThread() {
    static int s_lastLoggedEnabled = -1;

    g_seeBarriersJNI.frameOverlayDrawn = false;

    bool enabled = g_guiExtrasSeeBarriers;
    if (s_lastLoggedEnabled != (enabled ? 1 : 0)) {
        DebugLog("See Barriers state enabled=%d inited=%d failed=%d",
            enabled ? 1 : 0,
            g_seeBarriersJNI.inited ? 1 : 0,
            g_seeBarriersJNI.failed ? 1 : 0);
        s_lastLoggedEnabled = enabled ? 1 : 0;
    }
    if (!enabled) {
        if (!g_seeBarriersJNI.activeDamageMarkers.empty() || !g_seeBarriersJNI.chunkCaches.empty()) {
            JNIEnv* env = GetJNIEnvForCurrentThread();
            if (env) ResetSeeBarriersCache(env);
        }
        return;
    }

    if (!g_seeBarriersJNI.inited || g_seeBarriersJNI.failed || !g_seeBarriersJNI.chunkCacheReady) {
        return;
    }

    JNIEnv* env = GetJNIEnvForCurrentThread();
    if (!env) return;

    jobject world = nullptr;
    jobject player = nullptr;
    if (!GetSeeBarriersWorldAndPlayer(env, world, player)) return;

    EnsureSeeBarriersWorldCache(env, world);

    ULONGLONG now = GetTickCount64();
    if (g_seeBarriersJNI.lastChunkSyncMs == 0 ||
        (now - g_seeBarriersJNI.lastChunkSyncMs) >= kSeeBarriersLoadedChunkSyncIntervalMs) {
        SyncLoadedBarrierChunks(env, world, now);
    }

    double playerX = env->GetDoubleField(player, g_seeBarriersJNI.fEntityPosX);
    double playerZ = env->GetDoubleField(player, g_seeBarriersJNI.fEntityPosZ);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(player);
        env->DeleteLocalRef(world);
        return;
    }

    int centerChunkX = FloorDiv16((int)std::floor(playerX));
    int centerChunkZ = FloorDiv16((int)std::floor(playerZ));
    ScanBarrierChunksBudgeted(env, world, centerChunkX, centerChunkZ, now);
    RefreshSeeBarriersObservedStateModelOverrides(env, world, now);
    RefreshSeeBarriersDamageMarkers(env, playerX, playerZ, now);

    env->DeleteLocalRef(player);
    env->DeleteLocalRef(world);
}

// =============================================================
// OpenGL rendering
// =============================================================
static const unsigned char FONT_5x7[][7] = {
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},
    {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
    {0x0E,0x11,0x01,0x06,0x08,0x10,0x1F},
    {0x0E,0x11,0x01,0x06,0x01,0x11,0x0E},
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},
    {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},
    {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E},
    {0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},
    {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C},
};

typedef void (APIENTRY* glOrtho_t)(GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble);
extern glOrtho_t o_glOrtho;

void BeginOrtho(int w, int h) {
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glPushMatrix();
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    if (o_glOrtho) o_glOrtho(0, w, h, 0, -1, 1);
    else glOrtho(0, w, h, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glDisable(GL_DEPTH_TEST); glDisable(GL_TEXTURE_2D); glDisable(GL_LIGHTING);
    glDisable(GL_ALPHA_TEST); glDisable(GL_FOG); glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void EndOrtho() {
    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW); glPopMatrix();
    glPopAttrib();
}

void DrawRect(float x, float y, float w, float h, float r, float g, float b, float a) {
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h);
    glEnd();
}

float DrawBitmapChar(float x, float y, float ps, char c, float r, float g, float b) {
    int idx = -1;
    if (c >= '0' && c <= '9') idx = c - '0';
    else if (c == '.') idx = 10;
    else return ps * 3.0f;
    const unsigned char* rows = FONT_5x7[idx];
    float sr = r * 0.25f, sg = g * 0.25f, sb = b * 0.25f;
    for (int row = 0; row < 7; row++)
        for (int col = 0; col < 5; col++)
            if (rows[row] & (0x10 >> col))
                DrawRect(x + (col + 1) * ps, y + (row + 1) * ps, ps, ps, sr, sg, sb, 1);
    for (int row = 0; row < 7; row++)
        for (int col = 0; col < 5; col++)
            if (rows[row] & (0x10 >> col))
                DrawRect(x + col * ps, y + row * ps, ps, ps, r, g, b, 1);
    return (c == '.') ? ps * 4.0f : ps * 6.0f;
}

float DrawBitmapString(float x, float y, float ps, const std::string& text, float r, float g, float b) {
    float cx = x;
    for (char c : text) cx += DrawBitmapChar(cx, y, ps, c, r, g, b);
    return cx - x;
}

float MeasureString(float ps, const std::string& text) {
    float w = 0;
    for (char c : text) w += (c == '.') ? ps * 4.0f : (c >= '0' && c <= '9') ? ps * 6.0f : ps * 3.0f;
    return w;
}

void GetTimerDrawMetrics(int w, int h, const std::string& display, float& posX, float& posY, float& textW, float& textH, float& ps) {
    ps = 2.0f * g_config.scale;
    textW = MeasureString(ps, display);
    textH = ps * 8.0f;

    if (g_guiTimerCrosshairMode) {
        posX = (w - textW) * 0.5f;
        posY = (h - textH) * 0.5f;
    }
    else {
        posX = g_config.GetX(w);
        posY = g_config.GetY(h);
    }
}

void DrawOverlay(int w, int h) {
    if (!g_config.visible || !g_guiTimerEnabled || w <= 0 || h <= 0) return;
    g_screenW = w; g_screenH = h;
    double decSec = GetDecimalSeconds();
    if (decSec < 0.0) return;
    int whole = GetDisplayedTimerNumber(decSec);
    std::string display = FormatTimerText(decSec);
    float posX = 0.0f;
    float posY = 0.0f;
    float textW = 0.0f;
    float textH = 0.0f;
    float ps = 0.0f;
    GetTimerDrawMetrics(w, h, display, posX, posY, textW, textH, ps);

    BeginOrtho(w, h);
    float r, gc, b;
    GetTimerColor(whole, r, gc, b);
    DrawBitmapString(posX, posY, ps, display, r, gc, b);
    if (!g_guiTimerCrosshairMode && !g_guiTimerLocked) {
        float hoverMargin = 10.0f * g_config.scale;
        bool mouseNear = g_mouseX >= posX - hoverMargin && g_mouseX <= posX + textW + hoverMargin &&
            g_mouseY >= posY - hoverMargin && g_mouseY <= posY + textH + hoverMargin;
        if (mouseNear) {
            float hx = posX + textW, hy = posY + textH, hs = 6.0f * g_config.scale;
            glColor4f(1, 1, 1, 0.35f);
            glBegin(GL_TRIANGLES);
            glVertex2f(hx, hy); glVertex2f(hx - hs, hy); glVertex2f(hx, hy - hs);
            glEnd();
        }
    }
    EndOrtho();
    glFlush();
}

bool IsBarrierWithinConfiguredRange(const BarrierBlockPos& pos, int range, bool infiniteRange, double cameraX, double cameraZ) {
    if (infiniteRange) return true;

    double dx = ((double)pos.x + 0.5) - cameraX;
    double dz = ((double)pos.z + 0.5) - cameraZ;
    double radius = (double)range + 0.5;
    return (dx * dx) + (dz * dz) <= radius * radius;
}

void EmitBarrierCubeQuads(float x, float y, float z) {
    float x2 = x + 1.0f;
    float y2 = y + 1.0f;
    float z2 = z + 1.0f;

    glVertex3f(x, y, z); glVertex3f(x2, y, z); glVertex3f(x2, y2, z); glVertex3f(x, y2, z);
    glVertex3f(x2, y, z2); glVertex3f(x, y, z2); glVertex3f(x, y2, z2); glVertex3f(x2, y2, z2);
    glVertex3f(x, y, z2); glVertex3f(x, y, z); glVertex3f(x, y2, z); glVertex3f(x, y2, z2);
    glVertex3f(x2, y, z); glVertex3f(x2, y, z2); glVertex3f(x2, y2, z2); glVertex3f(x2, y2, z);
    glVertex3f(x, y2, z); glVertex3f(x2, y2, z); glVertex3f(x2, y2, z2); glVertex3f(x, y2, z2);
    glVertex3f(x, y, z2); glVertex3f(x2, y, z2); glVertex3f(x2, y, z); glVertex3f(x, y, z);
}

void EmitBarrierCubeLines(float x, float y, float z) {
    float x2 = x + 1.0f;
    float y2 = y + 1.0f;
    float z2 = z + 1.0f;

    glVertex3f(x, y, z); glVertex3f(x2, y, z);
    glVertex3f(x2, y, z); glVertex3f(x2, y, z2);
    glVertex3f(x2, y, z2); glVertex3f(x, y, z2);
    glVertex3f(x, y, z2); glVertex3f(x, y, z);

    glVertex3f(x, y2, z); glVertex3f(x2, y2, z);
    glVertex3f(x2, y2, z); glVertex3f(x2, y2, z2);
    glVertex3f(x2, y2, z2); glVertex3f(x, y2, z2);
    glVertex3f(x, y2, z2); glVertex3f(x, y2, z);

    glVertex3f(x, y, z); glVertex3f(x, y2, z);
    glVertex3f(x2, y, z); glVertex3f(x2, y2, z);
    glVertex3f(x2, y, z2); glVertex3f(x2, y2, z2);
    glVertex3f(x, y, z2); glVertex3f(x, y2, z2);
}

volatile LONG g_seeBarriersCameraMatricesReady = 0;
GLdouble g_seeBarriersProjectionMatrix[16] = {};
GLdouble g_seeBarriersModelViewMatrix[16] = {};
GLint g_seeBarriersViewport[4] = {};

bool ProjectSeeBarriersWorldPointToScreen(double worldX, double worldY, double worldZ, float& screenX, float& screenY, double& depth) {
    double eyeX =
        (g_seeBarriersModelViewMatrix[0] * worldX) +
        (g_seeBarriersModelViewMatrix[4] * worldY) +
        (g_seeBarriersModelViewMatrix[8] * worldZ) +
        g_seeBarriersModelViewMatrix[12];
    double eyeY =
        (g_seeBarriersModelViewMatrix[1] * worldX) +
        (g_seeBarriersModelViewMatrix[5] * worldY) +
        (g_seeBarriersModelViewMatrix[9] * worldZ) +
        g_seeBarriersModelViewMatrix[13];
    double eyeZ =
        (g_seeBarriersModelViewMatrix[2] * worldX) +
        (g_seeBarriersModelViewMatrix[6] * worldY) +
        (g_seeBarriersModelViewMatrix[10] * worldZ) +
        g_seeBarriersModelViewMatrix[14];
    double eyeW =
        (g_seeBarriersModelViewMatrix[3] * worldX) +
        (g_seeBarriersModelViewMatrix[7] * worldY) +
        (g_seeBarriersModelViewMatrix[11] * worldZ) +
        g_seeBarriersModelViewMatrix[15];

    double clipX =
        (g_seeBarriersProjectionMatrix[0] * eyeX) +
        (g_seeBarriersProjectionMatrix[4] * eyeY) +
        (g_seeBarriersProjectionMatrix[8] * eyeZ) +
        (g_seeBarriersProjectionMatrix[12] * eyeW);
    double clipY =
        (g_seeBarriersProjectionMatrix[1] * eyeX) +
        (g_seeBarriersProjectionMatrix[5] * eyeY) +
        (g_seeBarriersProjectionMatrix[9] * eyeZ) +
        (g_seeBarriersProjectionMatrix[13] * eyeW);
    double clipZ =
        (g_seeBarriersProjectionMatrix[2] * eyeX) +
        (g_seeBarriersProjectionMatrix[6] * eyeY) +
        (g_seeBarriersProjectionMatrix[10] * eyeZ) +
        (g_seeBarriersProjectionMatrix[14] * eyeW);
    double clipW =
        (g_seeBarriersProjectionMatrix[3] * eyeX) +
        (g_seeBarriersProjectionMatrix[7] * eyeY) +
        (g_seeBarriersProjectionMatrix[11] * eyeZ) +
        (g_seeBarriersProjectionMatrix[15] * eyeW);
    if (fabs(clipW) < 1e-6) return false;

    double ndcX = clipX / clipW;
    double ndcY = clipY / clipW;
    double ndcZ = clipZ / clipW;
    depth = ndcZ;

    screenX = (float)(((ndcX * 0.5) + 0.5) * (double)g_seeBarriersViewport[2]);
    screenY = (float)((1.0 - ((ndcY * 0.5) + 0.5)) * (double)g_seeBarriersViewport[3]);
    return clipW > 0.0 &&
        ndcX >= -1.0 && ndcX <= 1.0 &&
        ndcY >= -1.0 && ndcY <= 1.0 &&
        ndcZ >= -1.0 && ndcZ <= 1.0;
}

void DrawSeeBarriersProjectedMarker(float centerX, float centerY, float size) {
    float half = size * 0.5f;
    float left = centerX - half;
    float top = centerY - half;
    float thickness = size >= 10.0f ? 2.0f : 1.0f;

    DrawRect(left, top, size, thickness, 0.43f, 0.29f, 0.14f, 0.92f);
    DrawRect(left, top + size - thickness, size, thickness, 0.43f, 0.29f, 0.14f, 0.92f);
    DrawRect(left, top, thickness, size, 0.43f, 0.29f, 0.14f, 0.92f);
    DrawRect(left + size - thickness, top, thickness, size, 0.43f, 0.29f, 0.14f, 0.92f);

    float inner = thickness + 1.0f;
    if (size > (inner * 2.0f)) {
        DrawRect(left + inner, top + inner, size - (inner * 2.0f), size - (inner * 2.0f), 0.62f, 0.43f, 0.22f, 0.24f);
    }
}

void DrawSeeBarriersWorldOverlay() {
    if (g_seeBarriersJNI.frameOverlayDrawn) return;

    if (!g_guiExtrasSeeBarriers || !g_seeBarriersJNI.inited || g_seeBarriersJNI.failed ||
        !g_seeBarriersJNI.chunkCacheReady || g_seeBarriersJNI.chunkCaches.empty()) {
        return;
    }
    if (InterlockedCompareExchange(&g_seeBarriersCameraMatricesReady, 0, 0) == 0) return;
    g_seeBarriersJNI.frameOverlayDrawn = true;

    JNIEnv* env = GetJNIEnvForCurrentThread();
    if (env) {
        UpdateSeeBarriersRenderCameraFromRenderManager(env);
    }

    int range = NormalizeSeeBarriersRange(g_guiSeeBarriersRange);
    bool infiniteRange = IsSeeBarriersRangeInfinite(range);
    int style = NormalizeSeeBarriersStyle(g_guiSeeBarriersStyle);
    bool drawSolid = style == SEE_BARRIERS_STYLE_BOX_OUTLINE;

    struct BarrierOverlayItem {
        BarrierBlockPos pos;
        double distanceSq = 0.0;
    };
    struct BarrierProjectedMarker {
        float x = 0.0f;
        float y = 0.0f;
        float size = 0.0f;
    };

    std::vector<BarrierOverlayItem> items;
    std::vector<BarrierProjectedMarker> projectedMarkers;
    size_t candidateCount = 0;
    for (const auto& entry : g_seeBarriersJNI.chunkCaches) {
        for (const BarrierBlockPos& pos : entry.second.barriers) {
            double dx = ((double)pos.x + 0.5) - g_seeBarriersJNI.renderCameraX;
            double dy = ((double)pos.y + 0.5) - g_seeBarriersJNI.renderCameraY;
            double dz = ((double)pos.z + 0.5) - g_seeBarriersJNI.renderCameraZ;
            double horizontalDistanceSq = (dx * dx) + (dz * dz);
            if (!infiniteRange) {
                double radius = (double)range + 0.5;
                if (horizontalDistanceSq > radius * radius) continue;
            }

            ++candidateCount;
            BarrierOverlayItem item;
            item.pos = pos;
            item.distanceSq = horizontalDistanceSq + (dy * dy);
            items.push_back(item);
        }
    }

    bool capped = items.size() > (size_t)kSeeBarriersOverlayMaxBlocks;
    if (capped) {
        std::nth_element(
            items.begin(),
            items.begin() + kSeeBarriersOverlayMaxBlocks,
            items.end(),
            [](const BarrierOverlayItem& a, const BarrierOverlayItem& b) {
                return a.distanceSq < b.distanceSq;
            });
        items.resize((size_t)kSeeBarriersOverlayMaxBlocks);
    }

    if (items.empty()) return;

    size_t projectedMarkerReserve = items.size();
    if (projectedMarkerReserve > 128) projectedMarkerReserve = 128;
    projectedMarkers.reserve(projectedMarkerReserve);
    unsigned int projectedVisibleCount = 0;
    bool sampleProjectedValid = false;
    bool sampleProjectedVisible = false;
    float sampleProjectedX = 0.0f;
    float sampleProjectedY = 0.0f;
    double sampleProjectedDepth = 0.0;
    for (const BarrierOverlayItem& item : items) {
        float screenX = 0.0f;
        float screenY = 0.0f;
        double depth = 0.0;
        bool projectedVisible = ProjectSeeBarriersWorldPointToScreen(
            (double)item.pos.x + 0.5,
            (double)item.pos.y + 0.5,
            (double)item.pos.z + 0.5,
            screenX,
            screenY,
            depth);
        if (!sampleProjectedValid) {
            sampleProjectedValid = true;
            sampleProjectedVisible = projectedVisible;
            sampleProjectedX = screenX;
            sampleProjectedY = screenY;
            sampleProjectedDepth = depth;
        }
        if (!projectedVisible) continue;

        ++projectedVisibleCount;
        if (projectedMarkers.size() >= 128) continue;

        double distanceSq = item.distanceSq;
        if (distanceSq < 1.0) distanceSq = 1.0;
        double distance = sqrt(distanceSq);
        float size = (float)(18.0 / distance);
        if (size < 5.0f) size = 5.0f;
        if (size > 14.0f) size = 14.0f;

        BarrierProjectedMarker marker = {};
        marker.x = screenX;
        marker.y = screenY;
        marker.size = size;
        projectedMarkers.push_back(marker);
    }

    GLint oldMatrixMode = GL_MODELVIEW;
    glGetIntegerv(GL_MATRIX_MODE, &oldMatrixMode);

    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT | GL_LINE_BIT | GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT | GL_VIEWPORT_BIT | GL_SCISSOR_BIT | GL_STENCIL_BUFFER_BIT | GL_POLYGON_BIT);
    glViewport(g_seeBarriersViewport[0], g_seeBarriersViewport[1], g_seeBarriersViewport[2], g_seeBarriersViewport[3]);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadMatrixd(g_seeBarriersProjectionMatrix);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadMatrixd(g_seeBarriersModelViewMatrix);

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_FOG);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    if (drawSolid) {
        glColor4f(0.43f, 0.29f, 0.14f, 0.34f);
        glBegin(GL_QUADS);
        for (const BarrierOverlayItem& item : items) {
            const BarrierBlockPos& pos = item.pos;
            EmitBarrierCubeQuads(
                (float)pos.x,
                (float)pos.y,
                (float)pos.z);
        }
        glEnd();
    }

    glLineWidth(1.8f);
    glColor4f(0.28f, 0.18f, 0.08f, 0.95f);
    glBegin(GL_LINES);
    for (const BarrierOverlayItem& item : items) {
        const BarrierBlockPos& pos = item.pos;
        EmitBarrierCubeLines(
            (float)pos.x,
            (float)pos.y,
            (float)pos.z);
    }
    glEnd();

    static ULONGLONG s_lastOverlayLogMs = 0;
    ULONGLONG now = GetTickCount64();
    if ((now - s_lastOverlayLogMs) >= 2000) {
        DebugLog("See Barriers overlay drawn=%u candidates=%u capped=%d range=%s matricesReady=%d projected=%u sampleVisible=%d sampleScreen=%.1f,%.1f sampleDepth=%.3f",
            (unsigned int)items.size(),
            (unsigned int)candidateCount,
            capped ? 1 : 0,
            FormatSeeBarriersRangeLabel(g_guiSeeBarriersRange).c_str(),
            InterlockedCompareExchange(&g_seeBarriersCameraMatricesReady, 0, 0) != 0 ? 1 : 0,
            projectedVisibleCount,
            sampleProjectedVisible ? 1 : 0,
            sampleProjectedX,
            sampleProjectedY,
            sampleProjectedDepth);
        s_lastOverlayLogMs = now;
    }

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(oldMatrixMode);
    glPopAttrib();

    if (!projectedMarkers.empty() && g_seeBarriersViewport[2] > 0 && g_seeBarriersViewport[3] > 0) {
        BeginOrtho(g_seeBarriersViewport[2], g_seeBarriersViewport[3]);
        for (const BarrierProjectedMarker& marker : projectedMarkers) {
            DrawSeeBarriersProjectedMarker(marker.x, marker.y, marker.size);
        }
        EndOrtho();
    }
}

bool IsLikelyHudOrthoProjection(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar) {
    const GLdouble epsilon = 0.01;
    return fabs(left) <= epsilon &&
        fabs(top) <= epsilon &&
        right > 0.0 &&
        bottom > 0.0 &&
        zFar > zNear &&
        zNear >= 100.0 &&
        zFar >= 1000.0;
}

// =============================================================
// Hooks
// =============================================================
typedef BOOL(WINAPI* SwapBuffers_t)(HDC);
SwapBuffers_t o_SwapBuffers = nullptr;
typedef void (APIENTRY* glClear_t)(GLbitfield);
glClear_t o_glClear = nullptr;
glOrtho_t o_glOrtho = nullptr;
typedef void (APIENTRY* glTranslatef_t)(GLfloat, GLfloat, GLfloat);
glTranslatef_t o_glTranslatef = nullptr;
typedef void (APIENTRY* glBlendFuncSeparate_t)(GLenum, GLenum, GLenum, GLenum);
glBlendFuncSeparate_t g_realGlBlendFuncSeparateProc = nullptr;
WNDPROC o_wndProc = nullptr;
volatile LONG g_timerCaptureFrameDrawn = 0;
volatile LONG g_seeBarriersCameraPitchSeen = 0;
volatile LONG g_seeBarriersCameraYawSeen = 0;

LRESULT CALLBACK HookedWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void EnsureWndProcHooked(HWND candidateHwnd = nullptr);

void CaptureSeeBarriersCameraMatrices() {
    if (!g_guiExtrasSeeBarriers || !g_seeBarriersJNI.inited || g_seeBarriersJNI.failed) return;
    if (InterlockedCompareExchange(&g_seeBarriersCameraMatricesReady, 0, 0) != 0) return;

    GLint viewport[4] = {};
    glGetIntegerv(GL_VIEWPORT, viewport);
    if (viewport[2] <= 0 || viewport[3] <= 0) return;

    glGetDoublev(GL_PROJECTION_MATRIX, g_seeBarriersProjectionMatrix);
    glGetDoublev(GL_MODELVIEW_MATRIX, g_seeBarriersModelViewMatrix);
    g_seeBarriersViewport[0] = viewport[0];
    g_seeBarriersViewport[1] = viewport[1];
    g_seeBarriersViewport[2] = viewport[2];
    g_seeBarriersViewport[3] = viewport[3];
    InterlockedExchange(&g_seeBarriersCameraMatricesReady, 1);
}

void ResetBlendFuncSeparatePatchState() {
    if (g_realGlBlendFuncSeparateProc || g_glBlendPatchedContext) {
        DebugLog("ResetBlendFuncSeparatePatchState oldRealProc=%p oldContext=%p", g_realGlBlendFuncSeparateProc, g_glBlendPatchedContext);
    }
    g_realGlBlendFuncSeparateProc = nullptr;
    g_glBlendPatchedContext = nullptr;
}

void APIENTRY hk_glBlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha) {
    glBlendFuncSeparate_t realProc = g_realGlBlendFuncSeparateProc;
    if (!realProc) return;
    if (InterlockedCompareExchange(&g_shutdownRequested, 0, 0) != 0) {
        realProc(sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);
        return;
    }

    if (g_guiTimerCrosshairMode && g_timerActive &&
        sfactorRGB == 0x0307 && dfactorRGB == 0x0301) {
        realProc(GL_ZERO, GL_ONE, sfactorAlpha, dfactorAlpha);
        return;
    }

    realProc(sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);
}

void APIENTRY hk_glOrtho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar) {
    bool isHudProjection = IsLikelyHudOrthoProjection(left, right, bottom, top, zNear, zFar);
    o_glOrtho(left, right, bottom, top, zNear, zFar);

    if (InterlockedCompareExchange(&g_shutdownRequested, 0, 0) != 0) return;
    if (!g_guiTimerObsScreenshotsEnabled) return;
    if (!isHudProjection) return;
    if (InterlockedCompareExchange(&g_timerCaptureFrameDrawn, 1, 0) != 0) return;

    GLint viewport[4] = {};
    glGetIntegerv(GL_VIEWPORT, viewport);
    if (viewport[2] <= 0 || viewport[3] <= 0) return;

    DrawOverlay(viewport[2], viewport[3]);
}

void EnsureBlendFuncSeparatePatched() {
    static DWORD s_lastMakeCurrentError = ERROR_SUCCESS;
    static bool s_loggedMissingContext = false;
    HGLRC currentContext = wglGetCurrentContext();
    if (!currentContext) {
        if (!s_loggedMissingContext) {
            DebugLog("EnsureBlendFuncSeparatePatched skip: no current context currentDC=%p", wglGetCurrentDC());
            s_loggedMissingContext = true;
        }
        ResetBlendFuncSeparatePatchState();
        return;
    }
    s_loggedMissingContext = false;
    if (currentContext != g_glBlendPatchedContext) {
        DebugLog("EnsureBlendFuncSeparatePatched context change old=%p new=%p", g_glBlendPatchedContext, currentContext);
        ResetBlendFuncSeparatePatchState();
    }
    if (g_glBlendPatchedContext == currentContext && g_realGlBlendFuncSeparateProc) return;

    HDC currentDC = wglGetCurrentDC();
    if (!currentDC) {
        DebugLog("EnsureBlendFuncSeparatePatched skip: no current DC currentContext=%p", currentContext);
        ResetBlendFuncSeparatePatchState();
        return;
    }
    if (!wglMakeCurrent(currentDC, currentContext)) {
        DWORD error = GetLastError();
        if (error != s_lastMakeCurrentError || g_glBlendPatchedContext != currentContext) {
            DebugLog("EnsureBlendFuncSeparatePatched wglMakeCurrent failed currentDC=%p currentContext=%p error=%lu", currentDC, currentContext, error);
            s_lastMakeCurrentError = error;
        }
        ResetBlendFuncSeparatePatchState();
        return;
    }
    s_lastMakeCurrentError = ERROR_SUCCESS;

    __try {
        JNIEnv* env = GetJNIEnvForCurrentThread();
        if (!env) {
            DebugLog("EnsureBlendFuncSeparatePatched skip: no JNIEnv currentContext=%p", currentContext);
            return;
        }
        if (!InitBlendHookJNI(env)) {
            DebugLog("EnsureBlendFuncSeparatePatched skip: InitBlendHookJNI failed currentContext=%p", currentContext);
            return;
        }

        jobject capabilities = env->CallStaticObjectMethod(g_glContextClass, g_glGetCapabilities);
        if (!capabilities || env->ExceptionCheck()) {
            env->ExceptionClear();
            if (capabilities) env->DeleteLocalRef(capabilities);
            DebugLog("EnsureBlendFuncSeparatePatched skip: getCapabilities failed currentContext=%p", currentContext);
            return;
        }

        jlong procValue = env->GetLongField(capabilities, g_glBlendFuncSeparateField);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            env->DeleteLocalRef(capabilities);
            DebugLog("EnsureBlendFuncSeparatePatched skip: GetLongField failed currentContext=%p", currentContext);
            return;
        }

        void* hookProc = reinterpret_cast<void*>(&hk_glBlendFuncSeparate);
        void* currentProc = reinterpret_cast<void*>((intptr_t)procValue);
        if (currentProc != hookProc) {
            if (currentProc) g_realGlBlendFuncSeparateProc = reinterpret_cast<glBlendFuncSeparate_t>((intptr_t)procValue);
            env->SetLongField(capabilities, g_glBlendFuncSeparateField, (jlong)(intptr_t)hookProc);
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                env->DeleteLocalRef(capabilities);
                DebugLog("EnsureBlendFuncSeparatePatched skip: SetLongField failed currentContext=%p currentProc=%p", currentContext, currentProc);
                return;
            }
            DebugLog("EnsureBlendFuncSeparatePatched patched currentContext=%p currentProc=%p realProc=%p", currentContext, currentProc, g_realGlBlendFuncSeparateProc);
        }
        else if (!g_realGlBlendFuncSeparateProc) {
            DebugLog("EnsureBlendFuncSeparatePatched found hook already installed but real proc missing currentContext=%p", currentContext);
        }

        g_glBlendPatchedContext = currentContext;
        env->DeleteLocalRef(capabilities);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        DebugLog("EnsureBlendFuncSeparatePatched SEH exception currentContext=%p currentDC=%p", currentContext, currentDC);
        ResetBlendFuncSeparatePatchState();
    }
}

void UnhookWndProc() {
    if (g_hookedHwnd && o_wndProc && IsWindow(g_hookedHwnd)) {
        WNDPROC cur = (WNDPROC)GetWindowLongPtrA(g_hookedHwnd, GWLP_WNDPROC);
        if (cur == HookedWndProc) {
            SetWindowLongPtrA(g_hookedHwnd, GWLP_WNDPROC, (LONG_PTR)o_wndProc);
        }
    }
    if (g_hookedHwnd || o_wndProc) {
        DebugLog("UnhookWndProc hwnd=%p wndProc=%p", g_hookedHwnd, o_wndProc);
    }
    g_hookedHwnd = nullptr;
    o_wndProc = nullptr;
}

void ShutdownGuiThread() {
    HWND guiHwnd = g_guiHwnd;
    if (guiHwnd) PostMessageA(guiHwnd, WM_CLOSE, 0, 0);
    else if (g_guiThreadId != 0) PostThreadMessageA(g_guiThreadId, WM_QUIT, 0, 0);

    if (g_guiThreadHandle) {
        WaitForSingleObject(g_guiThreadHandle, INFINITE);
        CloseHandle(g_guiThreadHandle);
        g_guiThreadHandle = nullptr;
    }
    g_guiThreadId = 0;
    g_guiHwnd = nullptr;
}

void SetOverlayWindowAppId(HWND hwnd) {
    if (!hwnd) return;

    IPropertyStore* propertyStore = nullptr;
    HRESULT hr = SHGetPropertyStoreForWindow(hwnd, IID_PPV_ARGS(&propertyStore));
    if (FAILED(hr) || !propertyStore) {
        DebugLog("SetOverlayWindowAppId SHGetPropertyStoreForWindow failed hr=0x%08lX hwnd=%p", hr, hwnd);
        return;
    }

    PROPVARIANT appId = {};
    hr = InitPropVariantFromString(L"TagEssentials", &appId);
    if (SUCCEEDED(hr)) {
        HRESULT setHr = propertyStore->SetValue(PKEY_AppUserModel_ID, appId);
        HRESULT commitHr = SUCCEEDED(setHr) ? propertyStore->Commit() : setHr;
        DebugLog("SetOverlayWindowAppId setHr=0x%08lX commitHr=0x%08lX hwnd=%p", setHr, commitHr, hwnd);
        PropVariantClear(&appId);
    }
    else {
        DebugLog("SetOverlayWindowAppId InitPropVariantFromString failed hr=0x%08lX", hr);
    }

    propertyStore->Release();
}

void APIENTRY hk_glTranslatef(GLfloat x, GLfloat y, GLfloat z) {
    o_glTranslatef(x, y, z);
}

void APIENTRY hk_glClear(GLbitfield mask) {
    if (InterlockedCompareExchange(&g_shutdownRequested, 0, 0) != 0) {
        o_glClear(mask);
        return;
    }
    // Run render-thread maintenance once on the full-buffer clear at
    // the start of the world render. Later partial clears are passthrough.
    if ((mask & GL_COLOR_BUFFER_BIT) == 0 || (mask & GL_DEPTH_BUFFER_BIT) == 0) {
        o_glClear(mask);
        return;
    }

    ProcessPendingPerspectiveRefresh();
    UpdateTntVisualOverridesOnRenderThread();
    if (g_guiExtrasMutedVoice && g_guiExtrasMutedVoiceHideMuteReminder) {
        bool packetFilterInstalled = InterlockedCompareExchange(&g_mutedVoicePacketFilterInstalled, 0, 0) != 0;
        bool hasPendingSeparatorCleanup =
            InterlockedCompareExchange64(&g_mutedVoicePendingSeparatorCleanupMs, 0, 0) != 0;
        if (!packetFilterInstalled || hasPendingSeparatorCleanup) {
            JNIEnv* env = GetJNIEnvForCurrentThread();
            if (env) FilterMutedVoiceMuteReminderChat(env, true);
        }
    }
    InterlockedExchange(&g_timerCaptureFrameDrawn, 0);
    o_glClear(mask);
}

BOOL WINAPI hk_SwapBuffers(HDC hdc) {
    static bool s_lastSwapWasInvalid = false;
    static HDC s_lastInvalidSwapDC = nullptr;
    static HDC s_lastInvalidCurrentDC = nullptr;
    static HGLRC s_lastInvalidContext = nullptr;
    static HWND s_lastLoggedWindow = nullptr;
    static LONG s_lastLoggedWidth = -1;
    static LONG s_lastLoggedHeight = -1;

    if (InterlockedCompareExchange(&g_shutdownRequested, 0, 0) != 0) {
        return o_SwapBuffers(hdc);
    }

    HGLRC currentContext = wglGetCurrentContext();
    HDC currentDC = wglGetCurrentDC();
    if (!currentContext || !currentDC || currentDC != hdc) {
        if (!s_lastSwapWasInvalid ||
            s_lastInvalidSwapDC != hdc ||
            s_lastInvalidCurrentDC != currentDC ||
            s_lastInvalidContext != currentContext) {
            DebugLog("hk_SwapBuffers bypass invalid state swapDC=%p currentDC=%p currentContext=%p", hdc, currentDC, currentContext);
            s_lastSwapWasInvalid = true;
            s_lastInvalidSwapDC = hdc;
            s_lastInvalidCurrentDC = currentDC;
            s_lastInvalidContext = currentContext;
        }
        ResetBlendFuncSeparatePatchState();
        return o_SwapBuffers(hdc);
    }
    if (s_lastSwapWasInvalid) {
        DebugLog("hk_SwapBuffers recovered swapDC=%p currentDC=%p currentContext=%p", hdc, currentDC, currentContext);
        s_lastSwapWasInvalid = false;
        s_lastInvalidSwapDC = nullptr;
        s_lastInvalidCurrentDC = nullptr;
        s_lastInvalidContext = nullptr;
    }

    __try {
        if (g_guiTimerCrosshairMode) EnsureBlendFuncSeparatePatched();

        HWND hwnd = WindowFromDC(currentDC);
        if (hwnd) {
            InterlockedExchangePointer(reinterpret_cast<PVOID volatile*>(&g_gameRenderHwnd), hwnd);
        }
        RECT rect = {};
        if (hwnd && GetClientRect(hwnd, &rect)) {
            if (hwnd != s_lastLoggedWindow || rect.right != s_lastLoggedWidth || rect.bottom != s_lastLoggedHeight) {
                DebugLog("hk_SwapBuffers target hwnd=%p size=%ldx%ld swapDC=%p currentContext=%p", hwnd, rect.right, rect.bottom, hdc, currentContext);
                s_lastLoggedWindow = hwnd;
                s_lastLoggedWidth = rect.right;
                s_lastLoggedHeight = rect.bottom;
            }
        }
        if (hwnd && rect.right > 0 && rect.bottom > 0) {
            if (!g_guiTimerObsScreenshotsEnabled ||
                InterlockedCompareExchange(&g_timerCaptureFrameDrawn, 0, 0) == 0) {
                DrawOverlay(rect.right, rect.bottom);
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        DebugLog("hk_SwapBuffers SEH exception swapDC=%p currentDC=%p currentContext=%p", hdc, currentDC, currentContext);
        ResetBlendFuncSeparatePatchState();
    }

    BOOL result = o_SwapBuffers(hdc);
    InterlockedExchange(&g_timerCaptureFrameDrawn, 0);
    return result;
}

static bool s_resizing = false;
static float s_resizeStartDist = 0.0f;
static float s_resizeStartScale = 0.0f;

LRESULT CALLBACK HookedWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static thread_local int s_hookDepth = 0;
    struct HookDepthGuard {
        int* depth = nullptr;
        explicit HookDepthGuard(int* value) : depth(value) {}
        ~HookDepthGuard() { if (depth) --(*depth); }
    };

    ++s_hookDepth;
    HookDepthGuard depthGuard(&s_hookDepth);
    if (s_hookDepth > 32) {
        DebugLog("HookedWndProc recursion guard hwnd=%p msg=0x%X depth=%d", hwnd, msg, s_hookDepth);
        return DefWindowProcA(hwnd, msg, wParam, lParam);
    }

    if (InterlockedCompareExchange(&g_shutdownRequested, 0, 0) != 0) {
        return o_wndProc ? CallWindowProcA(o_wndProc, hwnd, msg, wParam, lParam) : DefWindowProcA(hwnd, msg, wParam, lParam);
    }

    if (ShouldLogWindowMessage(msg, wParam)) {
        LogWindowMessage(hwnd, msg, wParam, lParam);
    }

    if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
        if (CaptureMutedVoiceChatOnEnter(wParam, lParam)) {
            return 0;
        }
    }

    float mx = (float)LOWORD(lParam);
    float my = (float)HIWORD(lParam);
    if (msg == WM_MOUSEMOVE) { g_mouseX = mx; g_mouseY = my; }

    std::string timerDisplay;
    if (g_timerActive && g_screenW > 0 && g_screenH > 0) timerDisplay = FormatTimerText(GetDecimalSeconds());

    float posX = 0.0f;
    float posY = 0.0f;
    float textW = 0.0f;
    float textH = 0.0f;
    float ps = 0.0f;
    if (!timerDisplay.empty()) GetTimerDrawMetrics(g_screenW, g_screenH, timerDisplay, posX, posY, textW, textH, ps);
    float handleSize = ps * 6.0f;
    float hx = posX + textW, hy = posY + textH;

    switch (msg) {
    case WM_NCDESTROY: {
        LRESULT result = o_wndProc ? CallWindowProcA(o_wndProc, hwnd, msg, wParam, lParam) : DefWindowProcA(hwnd, msg, wParam, lParam);
        if (hwnd == g_hookedHwnd) {
            DebugLog("HookedWndProc WM_NCDESTROY clearing hooked hwnd=%p", hwnd);
            g_hookedHwnd = nullptr;
            o_wndProc = nullptr;
        }
        return result;
    }
    case WM_LBUTTONDOWN:
        if (g_config.visible && g_guiTimerEnabled && !g_guiTimerLocked && !g_guiTimerCrosshairMode &&
            g_timerActive && g_screenW > 0 && !timerDisplay.empty()) {
            if (mx >= hx - handleSize && mx <= hx + handleSize &&
                my >= hy - handleSize && my <= hy + handleSize) {
                s_resizing = true;
                float dx = mx - posX, dy = my - posY;
                s_resizeStartDist = sqrtf(dx * dx + dy * dy);
                s_resizeStartScale = g_config.scale;
                return 0;
            }
            if (mx >= posX - 5 && mx <= posX + textW + 5 &&
                my >= posY - 5 && my <= posY + textH + 5) {
                g_config.dragging = true;
                g_config.dragOffsetX = mx - posX;
                g_config.dragOffsetY = my - posY;
                return 0;
            }
        }
        break;
    case WM_MOUSEMOVE:
        if (s_resizing) {
            float dx = mx - g_config.GetX(g_screenW), dy = my - g_config.GetY(g_screenH);
            float newDist = sqrtf(dx * dx + dy * dy);
            if (s_resizeStartDist > 0) {
                g_config.scale = ClampFloat(s_resizeStartScale * (newDist / s_resizeStartDist), kTimerScaleMin, kTimerScaleMax);
            }
            return 0;
        }
        if (g_config.dragging && g_screenW > 0 && g_screenH > 0) {
            g_config.SetPixelPos(mx - g_config.dragOffsetX, my - g_config.dragOffsetY, g_screenW, g_screenH);
            return 0;
        }
        break;
    case WM_LBUTTONUP:
        if (s_resizing || g_config.dragging) {
            s_resizing = false; g_config.dragging = false;
            g_config.Save(kTimerOverlayConfigPath);
        }
        break;
    }
    return o_wndProc ? CallWindowProcA(o_wndProc, hwnd, msg, wParam, lParam) : DefWindowProcA(hwnd, msg, wParam, lParam);
}

void EnsureWndProcHooked(HWND candidateHwnd) {
    if (InterlockedCompareExchange(&g_shutdownRequested, 0, 0) != 0) return;

    HWND hwnd = candidateHwnd;
    if (!hwnd) hwnd = (HWND)InterlockedCompareExchangePointer(reinterpret_cast<PVOID volatile*>(&g_gameRenderHwnd), nullptr, nullptr);
    if (!hwnd || !IsWindow(hwnd)) return;

    DWORD windowPid = 0;
    GetWindowThreadProcessId(hwnd, &windowPid);
    if (windowPid != GetCurrentProcessId()) {
        DebugLog("EnsureWndProcHooked rejected foreign hwnd=%p pid=%lu currentPid=%lu", hwnd, windowPid, GetCurrentProcessId());
        return;
    }

    if (g_hookedHwnd && (!IsWindow(g_hookedHwnd) || (hwnd && hwnd != g_hookedHwnd))) {
        DebugLog("EnsureWndProcHooked observed hwnd transition old=%p new=%p", g_hookedHwnd, hwnd);
        UnhookWndProc();
    }

    if (hwnd != g_hookedHwnd) {
        WNDPROC previousProc = (WNDPROC)GetWindowLongPtrA(hwnd, GWLP_WNDPROC);
        if (previousProc != HookedWndProc) {
            SetLastError(ERROR_SUCCESS);
            LONG_PTR replaced = SetWindowLongPtrA(hwnd, GWLP_WNDPROC, (LONG_PTR)HookedWndProc);
            DWORD error = GetLastError();
            if (replaced == 0 && error != ERROR_SUCCESS) {
                DebugLog("EnsureWndProcHooked attach failed hwnd=%p error=%lu previousWndProc=%p", hwnd, error, previousProc);
                return;
            }

            WNDPROC replacedProc = reinterpret_cast<WNDPROC>(replaced);
            if (replacedProc && replacedProc != HookedWndProc) {
                o_wndProc = replacedProc;
            }
            else if (!o_wndProc) {
                o_wndProc = previousProc;
            }
        }

        DebugLog("EnsureWndProcHooked attached hwnd=%p previousWndProc=%p", hwnd, o_wndProc);
        g_hookedHwnd = hwnd;
    }
    else {
        WNDPROC cur = (WNDPROC)GetWindowLongPtrA(hwnd, GWLP_WNDPROC);
        if (cur != HookedWndProc) {
            DebugLog("EnsureWndProcHooked skipping same-hwnd reattach hwnd=%p currentWndProc=%p savedWndProc=%p", hwnd, cur, o_wndProc);
        }
    }

}

void TryRecoverRuntimeJNI() {
    static ULONGLONG s_lastRecoveryAttemptMs = 0;
    static int s_lastLoggedMask = -1;

    bool needMutedVoicePacketFilter =
        g_guiExtrasMutedVoice &&
        g_guiExtrasMutedVoiceHideMuteReminder &&
        InterlockedCompareExchange(&g_mutedVoicePacketFilterInstalled, 0, 0) == 0 &&
        InterlockedCompareExchange(&g_mutedVoicePacketFilterFailed, 0, 0) == 0;
    bool needRecovery =
        !g_scoreboardJNI.inited || g_scoreboardJNI.failed ||
        !g_slInited || g_slFailed ||
        !g_speedSlownessJNI.inited || g_speedSlownessJNI.failed ||
        !g_tntVisualJNI.inited || g_tntVisualJNI.failed ||
        needMutedVoicePacketFilter;
    if (!needRecovery) return;

    ULONGLONG now = GetTickCount64();
    if (s_lastRecoveryAttemptMs != 0 && (now - s_lastRecoveryAttemptMs) < 1000) return;
    s_lastRecoveryAttemptMs = now;

    if (!g_env && !AttachToJVM()) return;

    InitScoreboardJNI();
    InitSnaplookJNI();
    InitSpeedSlownessJNI();
    InitTntVisualJNI();
    if (needMutedVoicePacketFilter) InitMutedVoicePacketFilter(g_env);

    int statusMask =
        (g_scoreboardJNI.inited && !g_scoreboardJNI.failed ? 1 : 0) |
        (g_slInited && !g_slFailed ? 2 : 0) |
        (g_speedSlownessJNI.inited && !g_speedSlownessJNI.failed ? 4 : 0) |
        (g_tntVisualJNI.inited && !g_tntVisualJNI.failed ? 8 : 0) |
        (InterlockedCompareExchange(&g_mutedVoicePacketFilterInstalled, 0, 0) != 0 ? 16 : 0);
    if (statusMask != s_lastLoggedMask) {
        s_lastLoggedMask = statusMask;
        DebugLog("JNI recovery status scoreboard=%d snaplook=%d speedSlowness=%d tntVisual=%d mutedVoicePacket=%d",
            (statusMask & 1) ? 1 : 0,
            (statusMask & 2) ? 1 : 0,
            (statusMask & 4) ? 1 : 0,
            (statusMask & 8) ? 1 : 0,
            (statusMask & 16) ? 1 : 0);
    }
}

// =============================================================
// External GUI (Win32 control panel)
// =============================================================
enum GuiBindCaptureTarget {
    GUI_BIND_NONE = 0,
    GUI_BIND_PERSPECTIVE
};

enum GuiPage {
    GUI_PAGE_MAIN = 0,
    GUI_PAGE_SOUND_PICKER,
    GUI_PAGE_NUMBER_COLOUR_PICKER,
    GUI_PAGE_THEME_PICKER
};

enum GuiSoundPickerField {
    GUI_SOUND_FIELD_NONE = 0,
    GUI_SOUND_FIELD_SPEED3,
    GUI_SOUND_FIELD_SLOWNESS
};

enum GuiNumberColourMode {
    GUI_NUMBER_COLOUR_RGB = 0,
    GUI_NUMBER_COLOUR_MINECRAFT
};

static GuiBindCaptureTarget g_guiBindCapture = GUI_BIND_NONE;
static GuiPage g_guiCurrentPage = GUI_PAGE_MAIN;
static GuiSoundPickerField g_guiSoundPickerField = GUI_SOUND_FIELD_NONE;
static GuiNumberColourMode g_guiNumberColourMode = GUI_NUMBER_COLOUR_RGB;

enum GuiSliderTarget {
    GUI_SLIDER_NONE = 0,
    GUI_SLIDER_TIMER_SCALE,
    GUI_SLIDER_TIMER_COLOUR_RED,
    GUI_SLIDER_TIMER_COLOUR_GREEN,
    GUI_SLIDER_TIMER_COLOUR_BLUE,
    GUI_SLIDER_SPEED3_VOLUME,
    GUI_SLIDER_SLOWNESS_VOLUME
};

static GuiSliderTarget g_guiSliderTarget = GUI_SLIDER_NONE;
static bool g_guiSliderDirty = false;
static bool g_guiMutedVoicePartyOwnerEditing = false;
static ULONGLONG g_guiLastAnimationTickMs = 0;
static ULONGLONG g_guiLastCosmicRepaintMs = 0;
static int g_guiSoundPickerScroll = 0;
static std::string g_guiSoundSearch;
static bool g_guiSoundSearchEditing = false;
static int g_guiMainScroll = 0;
static int g_guiMainContentHeight = 0;
static int g_guiMainViewportHeight = 0;
static std::array<bool, kTimerNumberCount> g_guiNumberColourSelection = {};
static bool g_guiNumberColourSelectionDragging = false;
static int g_guiNumberColourSelectionAnchor = kTimerNumberMax;
static int g_guiNumberColourSelectionEnd = kTimerNumberMax;
static int g_guiNumberColourRed = 85;
static int g_guiNumberColourGreen = 255;
static int g_guiNumberColourBlue = 85;

struct GuiCardAnimState {
    bool expanded = false;
    float currentHeight = 0.0f;
};

static GuiCardAnimState g_perspectiveCardAnim = {};
static GuiCardAnimState g_timerCardAnim = {};
static GuiCardAnimState g_speedSlownessCardAnim = {};
static GuiCardAnimState g_publicHelpersCardAnim = {};
static GuiCardAnimState g_extrasCardAnim = {};
static GuiCardAnimState g_mutedUtilitiesCardAnim = {};

static RECT g_perspectiveCard = {};
static RECT g_perspectiveToggleRect = {};
static RECT g_perspectiveExpandRect = {};
static RECT g_perspectiveBindRect = {};
static RECT g_perspectiveBackRect = {};
static RECT g_perspectiveFrontRect = {};
static RECT g_timerCard = {};
static RECT g_timerShowToggleRect = {};
static RECT g_timerExpandRect = {};
static RECT g_timerLockToggleRect = {};
static RECT g_timerNametagToggleRect = {};
static RECT g_timerNametagPrefixRect = {};
static RECT g_timerNametagSuffixRect = {};
static RECT g_timerCaptureToggleRect = {};
static RECT g_timerModeOverlayRect = {};
static RECT g_timerModeCrosshairRect = {};
static RECT g_timerDecimal0Rect = {};
static RECT g_timerDecimal1Rect = {};
static RECT g_timerDecimal2Rect = {};
static RECT g_timerNumberColourButtonRect = {};
static RECT g_timerDefaultScoreboardToggleRect = {};
static RECT g_timerScaleTrackRect = {};
static RECT g_timerScaleHitRect = {};
static RECT g_speedSlownessCard = {};
static RECT g_speedSlownessToggleRect = {};
static RECT g_speedSlownessExpandRect = {};
static RECT g_speed3SoundButtonRect = {};
static RECT g_speed3VolumeTrackRect = {};
static RECT g_speed3VolumeHitRect = {};
static RECT g_slownessSoundButtonRect = {};
static RECT g_slownessVolumeTrackRect = {};
static RECT g_slownessVolumeHitRect = {};
static RECT g_publicHelpersCard = {};
static RECT g_publicHelpersExpandRect = {};
static RECT g_publicWinsToggleRect = {};
static RECT g_publicWinsPrefixRect = {};
static RECT g_publicWinsSuffixRect = {};
static RECT g_publicWinsSpaceRect = {};
static RECT g_extrasCard = {};
static RECT g_extrasExpandRect = {};
static RECT g_extrasWheatToggleRect = {};
static RECT g_extrasBeaconToggleRect = {};
static RECT g_extrasScoreboardToggleRect = {};
static RECT g_mutedUtilitiesCard = {};
static RECT g_mutedUtilitiesExpandRect = {};
static RECT g_mutedVoiceToggleRect = {};
static RECT g_mutedVoiceHideMuteReminderToggleRect = {};
static RECT g_mutedVoicePartyOwnerFieldRect = {};
static RECT g_mutedVoiceAuthCodeRect = {};
static RECT g_mutedVoiceCopyCodeButtonRect = {};
static RECT g_mutedVoiceOpenAuthButtonRect = {};
static RECT g_mutedVoiceSignOutButtonRect = {};
static RECT g_soundPickerCard = {};
static RECT g_soundPickerBackRect = {};
static RECT g_soundPickerSearchRect = {};
static RECT g_soundPickerListRect = {};
static RECT g_numberColourPickerCard = {};
static RECT g_numberColourPickerBackRect = {};
static RECT g_numberColourRgbModeRect = {};
static RECT g_numberColourMinecraftModeRect = {};
static RECT g_numberColourGridRect = {};
static std::array<RECT, kTimerNumberCount> g_numberColourNumberRects = {};
static RECT g_numberColourPreviewRect = {};
static RECT g_numberColourRedTrackRect = {};
static RECT g_numberColourRedHitRect = {};
static RECT g_numberColourGreenTrackRect = {};
static RECT g_numberColourGreenHitRect = {};
static RECT g_numberColourBlueTrackRect = {};
static RECT g_numberColourBlueHitRect = {};
static std::array<RECT, 16> g_minecraftColourOptionRects = {};
static RECT g_themePickerCard = {};
static RECT g_themePickerBackRect = {};
static RECT g_themeOptionRects[GUI_THEME_COUNT] = {};
static RECT g_footerYouTubeRect = {};
static RECT g_footerDiscordRect = {};
static RECT g_headerThemeRect = {};
static RECT g_headerMinimizeRect = {};
static RECT g_headerCloseRect = {};

struct GuiPalette {
    COLORREF bg;
    COLORREF bgBottom;
    COLORREF header;
    COLORREF headerBottom;
    COLORREF card;
    COLORREF cardBorder;
    COLORREF cardInset;
    COLORREF accent;
    COLORREF accentAlt;
    COLORREF accentSoft;
    COLORREF accentGlow;
    COLORREF divider;
    COLORREF muted;
    COLORREF text;
    COLORREF button;
    COLORREF buttonBorder;
    COLORREF buttonIdleText;
    COLORREF track;
    COLORREF trackBorder;
    COLORREF activeBorder;
    COLORREF activeAltFill;
    COLORREF activeAltBorder;
    COLORREF toggleOff;
    COLORREF toggleOffBorder;
    COLORREF knob;
    COLORREF knobBorder;
    COLORREF activeMeta;
    COLORREF bodyTexture;
    COLORREF scrollTrack;
    COLORREF scrollTrackBorder;
    COLORREF scrollThumbBorder;
    COLORREF starPrimary;
    COLORREF starBlue;
    COLORREF starPink;
};

// The original design remains byte-for-byte equivalent at the palette level.
static const GuiPalette kGuiClassicPalette = {
    RGB(9, 10, 16),       // bg
    RGB(9, 10, 16),       // bgBottom
    RGB(14, 15, 22),      // header
    RGB(14, 15, 22),      // headerBottom
    RGB(17, 18, 27),      // card
    RGB(35, 36, 48),      // cardBorder
    RGB(13, 14, 21),      // cardInset
    RGB(142, 102, 220),   // accent
    RGB(142, 102, 220),   // accentAlt
    RGB(64, 49, 96),      // accentSoft
    RGB(18, 18, 26),      // accentGlow
    RGB(53, 48, 71),      // divider
    RGB(158, 160, 182),   // muted
    RGB(239, 240, 247),   // text
    RGB(22, 23, 33),      // button
    RGB(43, 44, 58),      // buttonBorder
    RGB(220, 221, 232),   // buttonIdleText
    RGB(24, 25, 36),      // track
    RGB(45, 46, 61),      // trackBorder
    RGB(124, 96, 188),    // activeBorder
    RGB(40, 34, 56),      // activeAltFill
    RGB(78, 69, 104),     // activeAltBorder
    RGB(31, 32, 44),      // toggleOff
    RGB(50, 51, 66),      // toggleOffBorder
    RGB(242, 241, 248),   // knob
    RGB(214, 215, 228),   // knobBorder
    RGB(226, 220, 244),   // activeMeta
    RGB(15, 16, 24),      // bodyTexture
    RGB(19, 20, 29),      // scrollTrack
    RGB(31, 32, 43),      // scrollTrackBorder
    RGB(100, 77, 151),    // scrollThumbBorder
    RGB(239, 240, 247),   // starPrimary
    RGB(158, 160, 182),   // starBlue
    RGB(142, 102, 220)    // starPink
};

static const GuiPalette kGuiCosmicPalette = {
    RGB(5, 7, 26),        // bg
    RGB(17, 7, 42),       // bgBottom
    RGB(12, 12, 47),      // header
    RGB(28, 16, 76),      // headerBottom
    RGB(25, 21, 69),      // card
    RGB(91, 77, 184),     // cardBorder
    RGB(14, 15, 48),      // cardInset
    RGB(143, 93, 255),    // accent
    RGB(65, 202, 255),    // accentAlt
    RGB(68, 49, 137),     // accentSoft
    RGB(39, 27, 96),      // accentGlow
    RGB(119, 84, 234),    // divider
    RGB(174, 182, 224),   // muted
    RGB(247, 247, 255),   // text
    RGB(29, 28, 77),      // button
    RGB(75, 70, 148),     // buttonBorder
    RGB(228, 231, 252),   // buttonIdleText
    RGB(18, 20, 58),      // track
    RGB(67, 70, 142),     // trackBorder
    RGB(169, 130, 255),   // activeBorder
    RGB(29, 69, 108),     // activeAltFill
    RGB(78, 199, 239),    // activeAltBorder
    RGB(35, 36, 76),      // toggleOff
    RGB(74, 76, 139),     // toggleOffBorder
    RGB(248, 247, 255),   // knob
    RGB(203, 197, 247),   // knobBorder
    RGB(222, 225, 255),   // activeMeta
    RGB(29, 19, 68),      // bodyTexture
    RGB(13, 15, 44),      // scrollTrack
    RGB(49, 49, 99),      // scrollTrackBorder
    RGB(125, 103, 228),   // scrollThumbBorder
    RGB(248, 246, 255),   // starPrimary
    RGB(126, 206, 255),   // starBlue
    RGB(235, 139, 255)    // starPink
};

static const GuiPalette kGuiNeonCityPalette = {
    RGB(3, 9, 24), RGB(19, 3, 31), RGB(5, 17, 37), RGB(31, 6, 50),
    RGB(8, 23, 45), RGB(18, 151, 190), RGB(5, 14, 31),
    RGB(255, 55, 210), RGB(38, 222, 255), RGB(83, 35, 118), RGB(41, 12, 83),
    RGB(23, 192, 225), RGB(158, 192, 214), RGB(244, 252, 255),
    RGB(10, 29, 53), RGB(29, 115, 151), RGB(215, 242, 251),
    RGB(7, 23, 43), RGB(26, 107, 139), RGB(255, 103, 224),
    RGB(14, 67, 91), RGB(50, 211, 241), RGB(14, 38, 60), RGB(35, 101, 128),
    RGB(246, 254, 255), RGB(167, 232, 243), RGB(223, 251, 255),
    RGB(9, 22, 43), RGB(5, 20, 39), RGB(17, 66, 91), RGB(215, 66, 190),
    RGB(248, 252, 255), RGB(53, 225, 255), RGB(255, 91, 221)
};

static const GuiPalette kGuiEnchantedForestPalette = {
    RGB(3, 16, 18), RGB(4, 30, 23), RGB(5, 25, 28), RGB(10, 49, 37),
    RGB(8, 36, 32), RGB(48, 137, 111), RGB(4, 24, 23),
    RGB(247, 194, 77), RGB(55, 224, 166), RGB(65, 95, 61), RGB(23, 73, 55),
    RGB(59, 158, 116), RGB(168, 205, 185), RGB(244, 255, 246),
    RGB(12, 47, 39), RGB(42, 119, 94), RGB(222, 244, 228),
    RGB(7, 34, 30), RGB(37, 104, 84), RGB(255, 211, 105),
    RGB(17, 77, 59), RGB(78, 205, 149), RGB(15, 49, 41), RGB(45, 110, 89),
    RGB(248, 255, 238), RGB(191, 226, 194), RGB(239, 251, 233),
    RGB(7, 31, 26), RGB(5, 27, 25), RGB(26, 78, 63), RGB(182, 143, 63),
    RGB(251, 241, 182), RGB(88, 230, 182), RGB(239, 122, 203)
};

static const GuiPalette kGuiInfernoPalette = {
    RGB(20, 5, 3), RGB(52, 9, 2), RGB(36, 8, 4), RGB(83, 18, 4),
    RGB(49, 15, 9), RGB(163, 58, 23), RGB(29, 8, 5),
    RGB(255, 146, 24), RGB(255, 55, 20), RGB(111, 43, 17), RGB(90, 24, 7),
    RGB(210, 69, 18), RGB(218, 176, 149), RGB(255, 248, 235),
    RGB(58, 19, 12), RGB(133, 49, 24), RGB(246, 220, 198),
    RGB(35, 10, 7), RGB(109, 38, 18), RGB(255, 185, 55),
    RGB(105, 31, 11), RGB(255, 93, 22), RGB(62, 21, 13), RGB(134, 46, 22),
    RGB(255, 251, 224), RGB(239, 190, 131), RGB(255, 229, 188),
    RGB(39, 9, 4), RGB(29, 8, 6), RGB(77, 25, 14), RGB(210, 83, 23),
    RGB(255, 239, 193), RGB(255, 158, 35), RGB(255, 67, 23)
};

static const GuiPalette kGuiArcticAuroraPalette = {
    RGB(4, 15, 36), RGB(5, 35, 58), RGB(7, 25, 53), RGB(13, 64, 84),
    RGB(11, 39, 68), RGB(78, 170, 197), RGB(6, 27, 51),
    RGB(82, 239, 196), RGB(98, 206, 255), RGB(48, 100, 125), RGB(29, 78, 111),
    RGB(83, 191, 204), RGB(174, 210, 228), RGB(247, 253, 255),
    RGB(15, 48, 79), RGB(54, 132, 164), RGB(224, 245, 252),
    RGB(8, 34, 62), RGB(47, 118, 150), RGB(119, 255, 220),
    RGB(20, 80, 105), RGB(97, 223, 221), RGB(18, 53, 80), RGB(58, 126, 154),
    RGB(249, 255, 255), RGB(188, 231, 240), RGB(231, 250, 251),
    RGB(8, 32, 59), RGB(6, 28, 53), RGB(30, 88, 118), RGB(77, 181, 196),
    RGB(246, 254, 255), RGB(108, 220, 255), RGB(200, 144, 255)
};

const GuiPalette& GetGuiPaletteForTheme(int theme) {
    switch (NormalizeGuiTheme(theme)) {
    case GUI_THEME_CLASSIC: return kGuiClassicPalette;
    case GUI_THEME_NEON_CITY: return kGuiNeonCityPalette;
    case GUI_THEME_ENCHANTED_FOREST: return kGuiEnchantedForestPalette;
    case GUI_THEME_INFERNO: return kGuiInfernoPalette;
    case GUI_THEME_ARCTIC_AURORA: return kGuiArcticAuroraPalette;
    case GUI_THEME_COSMIC:
    default: return kGuiCosmicPalette;
    }
}

const GuiPalette& GetGuiPalette() {
    return GetGuiPaletteForTheme(g_guiTheme);
}

#define kGuiBg (GetGuiPalette().bg)
#define kGuiHeader (GetGuiPalette().header)
#define kGuiCard (GetGuiPalette().card)
#define kGuiCardBorder (GetGuiPalette().cardBorder)
#define kGuiCardInset (GetGuiPalette().cardInset)
#define kGuiAccent (GetGuiPalette().accent)
#define kGuiAccentSoft (GetGuiPalette().accentSoft)
#define kGuiAccentGlow (GetGuiPalette().accentGlow)
#define kGuiDivider (GetGuiPalette().divider)
#define kGuiMuted (GetGuiPalette().muted)
#define kGuiText (GetGuiPalette().text)
#define kGuiButton (GetGuiPalette().button)
#define kGuiButtonBorder (GetGuiPalette().buttonBorder)
#define kGuiButtonIdleText (GetGuiPalette().buttonIdleText)
#define kGuiTrack (GetGuiPalette().track)
#define kGuiTrackBorder (GetGuiPalette().trackBorder)

constexpr int kGuiCardHeaderHeight = 56;
constexpr int kGuiCardGap = 14;
constexpr int kGuiPerspectiveExpandedHeight = 128;
constexpr int kGuiTimerExpandedHeight = 364;
constexpr int kGuiSpeedExpandedHeight = 206;
constexpr int kGuiPublicHelpersExpandedHeight = 180;
constexpr int kGuiExtrasExpandedHeight = 174;
constexpr int kGuiMutedUtilitiesExpandedHeight = 380;
constexpr int kGuiSoundPickerRowHeight = 48;
constexpr int kGuiSoundPickerRowGap = 8;
constexpr int kGuiFooterIconSize = 38;
constexpr int kGuiFooterIconGap = 10;
constexpr int kGuiFooterTopGap = 14;
constexpr const char* kGuiYouTubeUrl = "https://www.youtube.com/@WhoJustGotAnL/videos";
constexpr const char* kGuiDiscordUrl = "https://discord.gg/primetag";
constexpr UINT_PTR kGuiAnimationTimerId = 1;
constexpr UINT kGuiAnimationIntervalMs = 16;
constexpr ULONGLONG kGuiCosmicFrameIntervalMs = 16;
constexpr int kGuiWindowHeaderHeight = 60;
constexpr int kGuiWindowButtonW = 32;
constexpr int kGuiWindowButtonH = 26;

void RequestGuiRepaint() {
    if (g_guiHwnd) InvalidateRect(g_guiHwnd, NULL, FALSE);
}

RECT MakeRectWH(int left, int top, int width, int height) {
    RECT rect = { left, top, left + width, top + height };
    return rect;
}

COLORREF BlendGuiColor(COLORREF from, COLORREF to, float amount) {
    float t = ClampFloat(amount, 0.0f, 1.0f);
    int r = (int)((float)GetRValue(from) + ((float)GetRValue(to) - (float)GetRValue(from)) * t);
    int g = (int)((float)GetGValue(from) + ((float)GetGValue(to) - (float)GetGValue(from)) * t);
    int b = (int)((float)GetBValue(from) + ((float)GetBValue(to) - (float)GetBValue(from)) * t);
    return RGB(r, g, b);
}

void FillVerticalGradient(HDC hdc, const RECT& rect, COLORREF top, COLORREF bottom, int bandCount = 48) {
    int height = rect.bottom - rect.top;
    if (height <= 0 || rect.right <= rect.left) return;

    int bands = bandCount;
    if (bands < 1) bands = 1;
    if (bands > height) bands = height;

    HBRUSH dcBrush = static_cast<HBRUSH>(GetStockObject(DC_BRUSH));
    COLORREF oldColor = SetDCBrushColor(hdc, top);
    for (int i = 0; i < bands; ++i) {
        int topY = rect.top + (height * i) / bands;
        int bottomY = rect.top + (height * (i + 1)) / bands;
        float t = bands <= 1 ? 0.0f : (float)i / (float)(bands - 1);
        SetDCBrushColor(hdc, BlendGuiColor(top, bottom, t));
        RECT band = { rect.left, topY, rect.right, bottomY };
        FillRect(hdc, &band, dcBrush);
    }
    SetDCBrushColor(hdc, oldColor);
}

void FillHorizontalGradient(HDC hdc, const RECT& rect, COLORREF left, COLORREF right, int bandCount = 48) {
    int width = rect.right - rect.left;
    if (width <= 0 || rect.bottom <= rect.top) return;

    int bands = bandCount;
    if (bands < 1) bands = 1;
    if (bands > width) bands = width;

    HBRUSH dcBrush = static_cast<HBRUSH>(GetStockObject(DC_BRUSH));
    COLORREF oldColor = SetDCBrushColor(hdc, left);
    for (int i = 0; i < bands; ++i) {
        int leftX = rect.left + (width * i) / bands;
        int rightX = rect.left + (width * (i + 1)) / bands;
        float t = bands <= 1 ? 0.0f : (float)i / (float)(bands - 1);
        SetDCBrushColor(hdc, BlendGuiColor(left, right, t));
        RECT band = { leftX, rect.top, rightX, rect.bottom };
        FillRect(hdc, &band, dcBrush);
    }
    SetDCBrushColor(hdc, oldColor);
}

float GetGuiCosmicAnimationSeconds() {
    static ULONGLONG animationStartedAtMs = GetTickCount64();
    constexpr float kCosmicAnimationSpeed = 0.55f;
    return ((float)(GetTickCount64() - animationStartedAtMs) / 1000.0f) * kCosmicAnimationSpeed;
}

int WrapCosmicCoordinate(int value, int span) {
    if (span <= 0) return 0;
    int wrapped = value % span;
    return wrapped < 0 ? wrapped + span : wrapped;
}

unsigned int HashCosmicPixel(int x, int y, unsigned int salt) {
    unsigned int value = (unsigned int)x * 0x8DA6B343u;
    value ^= (unsigned int)y * 0xD8163841u;
    value ^= salt * 0xCB1AB31Fu;
    value ^= value >> 16;
    value *= 0x7FEB352Du;
    value ^= value >> 15;
    value *= 0x846CA68Bu;
    return value ^ (value >> 16);
}

COLORREF GetPixelNebulaColor(int index) {
    static const COLORREF colors[] = {
        RGB(91, 72, 255),
        RGB(166, 68, 255),
        RGB(239, 67, 214),
        RGB(43, 151, 255),
        RGB(55, 226, 255),
        RGB(255, 126, 171),
        RGB(199, 158, 255)
    };
    return colors[index % (sizeof(colors) / sizeof(colors[0]))];
}

void DrawPixelNebula(HDC hdc, const RECT& bounds, float seconds) {
    int width = bounds.right - bounds.left;
    int height = bounds.bottom - bounds.top;
    if (!IsCosmicGuiTheme() || width <= 0 || height <= 0) return;

    RECT dcClip = {};
    if (GetClipBox(hdc, &dcClip) == ERROR) return;
    RECT clipped = {};
    if (!IntersectRect(&clipped, &bounds, &dcClip)) return;

    const GuiPalette& palette = GetGuiPalette();
    const int pixelSize = 3;
    int startX = clipped.left - WrapCosmicCoordinate(clipped.left - bounds.left, pixelSize);
    int startY = clipped.top - WrapCosmicCoordinate(clipped.top - bounds.top, pixelSize);
    HBRUSH dcBrush = static_cast<HBRUSH>(GetStockObject(DC_BRUSH));
    COLORREF oldColor = SetDCBrushColor(hdc, palette.bg);

    // Work column-first so the expensive ribbon curves are evaluated once per
    // three-pixel column instead of once per individual nebula sample.
    for (int x = startX; x < clipped.right; x += pixelSize) {
        float nx = (float)(x + pixelSize / 2 - bounds.left) / (float)width;
        float ribbonA = 0.26f + 0.12f * std::sin(nx * 7.4f + seconds * 0.22f);
        float ribbonB = 0.70f + 0.15f * std::cos(nx * 5.1f - seconds * 0.17f);
        float colorFlow = std::sin(nx * 12.0f + seconds * 0.33f);

        for (int y = startY; y < clipped.bottom; y += pixelSize) {
            float ny = (float)(y + pixelSize / 2 - bounds.top) / (float)height;
            float distanceA = (float)std::fabs(ny - ribbonA);
            float distanceB = (float)std::fabs(ny - ribbonB);
            float distance = distanceA < distanceB ? distanceA : distanceB;
            float ridge = ClampFloat(1.0f - (distance / 0.24f), 0.0f, 1.0f);

            int cellX = (x - bounds.left) / pixelSize;
            int cellY = (y - bounds.top) / pixelSize;
            unsigned int hash = HashCosmicPixel(cellX, cellY, 11u);
            float grain = (float)(hash & 255u) / 255.0f;
            if (ridge < 0.15f || grain > ridge * 0.48f) continue;

            int colorIndex = ((int)(nx * 9.0f + ny * 7.0f + seconds * 0.10f) + (int)(hash % 7u)) % 7;
            if (colorFlow > 0.60f) colorIndex = 4;
            else if (colorFlow < -0.72f) colorIndex = 6;

            COLORREF baseColor = BlendGuiColor(palette.bg, palette.bgBottom, ClampFloat(ny, 0.0f, 1.0f));
            float intensity = ridge * (0.45f + grain * 0.55f);
            float blend = 0.045f + intensity * 0.28f;
            COLORREF tileColor = BlendGuiColor(baseColor, GetPixelNebulaColor(colorIndex), blend);

            int inset = (int)((hash >> 8) % 2u);
            int trim = (int)((hash >> 12) % 2u);
            RECT pixel = {
                x + inset,
                y + inset,
                x + pixelSize - trim,
                y + pixelSize - trim
            };
            SetDCBrushColor(hdc, tileColor);
            FillRect(hdc, &pixel, dcBrush);
        }
    }

    SetDCBrushColor(hdc, oldColor);
}

void DrawPixelGalaxyCore(HDC hdc, const RECT& rect, float seconds) {
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    if (!IsCosmicGuiTheme() || width <= 0 || height <= 0) return;

    const GuiPalette& palette = GetGuiPalette();
    float centerX = (float)rect.left + (float)width * 0.58f + std::sin(seconds * 0.17f) * 14.0f;
    float centerY = (float)rect.top + (float)height * 0.72f + std::cos(seconds * 0.14f) * 10.0f;
    HBRUSH dcBrush = static_cast<HBRUSH>(GetStockObject(DC_BRUSH));
    COLORREF oldColor = SetDCBrushColor(hdc, palette.starPrimary);

    // Fine one-pixel dust disk behind the brighter spiral arms.
    for (int dustIndex = 0; dustIndex < 280; ++dustIndex) {
        unsigned int hash = HashCosmicPixel(dustIndex, 73, 101u);
        float normalizedRadius = (float)(hash & 65535u) / 65535.0f;
        float radius = 10.0f + normalizedRadius * normalizedRadius * 190.0f;
        float baseAngle = ((float)((hash >> 16) & 65535u) / 65535.0f) * 6.2831853f;
        float angle = baseAngle - seconds * (0.025f + normalizedRadius * 0.018f);
        int x = (int)(centerX + std::cos(angle) * radius * 1.30f);
        int y = (int)(centerY + std::sin(angle) * radius * 0.46f);
        if (x < rect.left || x >= rect.right || y < rect.top || y >= rect.bottom) continue;

        COLORREF base = BlendGuiColor(palette.bg, palette.bgBottom,
            ClampFloat((float)(y - rect.top) / (float)height, 0.0f, 1.0f));
        COLORREF dustColor = BlendGuiColor(base, GetPixelNebulaColor((int)((hash >> 10) % 7u)),
            0.16f + (1.0f - normalizedRadius) * 0.22f);
        SetDCBrushColor(hdc, dustColor);
        RECT dust = { x, y, x + 1, y + 1 };
        FillRect(hdc, &dust, dcBrush);
    }

    for (int arm = 0; arm < 4; ++arm) {
        for (int step = 0; step < 150; ++step) {
            float radius = 4.0f + (float)step * 1.25f;
            float angle = (float)arm * 1.5707963f + (float)step * 0.105f - seconds * 0.10f;
            int x = (int)(centerX + std::cos(angle) * radius * 1.32f);
            int y = (int)(centerY + std::sin(angle) * radius * 0.50f);
            if (x < rect.left || x >= rect.right || y < rect.top || y >= rect.bottom) continue;

            unsigned int hash = HashCosmicPixel(step, arm, 47u);
            int size = step < 12 ? 3 : ((hash & 15u) == 0u ? 3 : ((hash & 3u) == 0u ? 2 : 1));
            int colorIndex = (arm * 2 + step / 22) % 7;
            float fade = 1.0f - (float)step / 175.0f;
            COLORREF base = BlendGuiColor(palette.bg, palette.bgBottom,
                ClampFloat((float)(y - rect.top) / (float)height, 0.0f, 1.0f));
            COLORREF color = BlendGuiColor(base, GetPixelNebulaColor(colorIndex), 0.16f + fade * 0.58f);
            SetDCBrushColor(hdc, color);
            RECT pixel = { x - size / 2, y - size / 2, x - size / 2 + size, y - size / 2 + size };
            FillRect(hdc, &pixel, dcBrush);

            if ((hash & 1u) == 0u) {
                int dustX = x + (int)((hash >> 8) % 9u) - 4;
                int dustY = y + (int)((hash >> 13) % 7u) - 3;
                SetDCBrushColor(hdc, BlendGuiColor(base, GetPixelNebulaColor((colorIndex + 2) % 7), 0.34f + fade * 0.24f));
                RECT dust = { dustX, dustY, dustX + 1, dustY + 1 };
                FillRect(hdc, &dust, dcBrush);
            }
        }
    }

    for (int glowIndex = 0; glowIndex < 48; ++glowIndex) {
        unsigned int hash = HashCosmicPixel(glowIndex, 19, 131u);
        float angle = ((float)(hash & 65535u) / 65535.0f) * 6.2831853f - seconds * 0.08f;
        float radius = 2.0f + (float)((hash >> 16) % 18u);
        int glowX = (int)(centerX + std::cos(angle) * radius);
        int glowY = (int)(centerY + std::sin(angle) * radius * 0.42f);
        COLORREF glowColor = BlendGuiColor(palette.bg, GetPixelNebulaColor(glowIndex % 7), 0.52f);
        SetDCBrushColor(hdc, glowColor);
        RECT glow = { glowX, glowY, glowX + 1, glowY + 1 };
        FillRect(hdc, &glow, dcBrush);
    }

    COLORREF coreColor = BlendGuiColor(palette.starPrimary, RGB(213, 190, 255), 0.38f);
    SetDCBrushColor(hdc, coreColor);
    RECT core = { (int)centerX - 3, (int)centerY - 2, (int)centerX + 4, (int)centerY + 3 };
    FillRect(hdc, &core, dcBrush);
    SetDCBrushColor(hdc, oldColor);
}

void DrawPixelShootingStars(HDC hdc, const RECT& rect, float seconds) {
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    if (!IsCosmicGuiTheme() || width <= 0 || height <= 0) return;

    const GuiPalette& palette = GetGuiPalette();
    HBRUSH dcBrush = static_cast<HBRUSH>(GetStockObject(DC_BRUSH));
    COLORREF oldColor = SetDCBrushColor(hdc, palette.starPrimary);

    for (int meteor = 0; meteor < 2; ++meteor) {
        float local = (float)std::fmod(seconds + (float)meteor * 4.65f, 10.5f);
        if (local > 3.25f) continue;
        float progress = local / 3.25f;
        int headX = rect.right + 70 - (int)(progress * (float)(width + 160));
        int headY = rect.top + height / 7 + meteor * (height / 3) + (int)(progress * (float)height * 0.24f);
        COLORREF meteorColor = meteor == 0 ? palette.starBlue : palette.starPink;

        for (int tail = 15; tail >= 0; --tail) {
            int x = headX + tail * 5;
            int y = headY - tail * 3;
            float brightness = 1.0f - (float)tail / 18.0f;
            COLORREF color = BlendGuiColor(palette.bg, meteorColor, brightness);
            int size = tail == 0 ? 3 : (tail < 5 ? 2 : 1);
            SetDCBrushColor(hdc, color);
            RECT pixel = { x, y, x + size, y + size };
            FillRect(hdc, &pixel, dcBrush);
        }
    }

    SetDCBrushColor(hdc, oldColor);
}

void DrawCosmicStars(HDC hdc, const RECT& rect, int starCount) {
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    if (!IsCosmicGuiTheme() || width <= 0 || height <= 0 || starCount <= 0) return;

    float seconds = GetGuiCosmicAnimationSeconds();
    const GuiPalette& palette = GetGuiPalette();
    HBRUSH dcBrush = static_cast<HBRUSH>(GetStockObject(DC_BRUSH));
    COLORREF oldColor = SetDCBrushColor(hdc, palette.starPrimary);
    unsigned int state = 0x7F4A7C15u ^ (unsigned int)(width * 131 + height * 17 + rect.top * 7);

    for (int i = 0; i < starCount; ++i) {
        state = state * 1664525u + 1013904223u;
        int baseX = (int)(state % (unsigned int)width);
        state = state * 1664525u + 1013904223u;
        int baseY = (int)(state % (unsigned int)height);
        state = state * 1664525u + 1013904223u;

        int layer = (int)(state % 3u);
        int x = rect.left + WrapCosmicCoordinate(baseX - (int)(seconds * (float)(3 + layer * 4)), width);
        int y = rect.top + baseY;
        int twinkle = ((int)(seconds * 9.0f) + i * 7) % 28;
        float brightness = twinkle < 4 ? 1.0f : (twinkle < 10 ? 0.72f : 0.48f);

        int colorChoice = (int)(state % 10u);
        COLORREF rawColor = colorChoice < 5 ? palette.starPrimary : (colorChoice < 8 ? palette.starBlue : palette.starPink);
        COLORREF color = BlendGuiColor(palette.bg, rawColor, brightness);
        int size = layer == 2 && twinkle < 8 ? 2 : 1;
        SetDCBrushColor(hdc, color);
        RECT star = { x, y, x + size, y + size };
        FillRect(hdc, &star, dcBrush);

        if (layer == 2 && twinkle < 3) {
            RECT horizontal = { x - 3, y + 1, x + 5, y + 2 };
            RECT vertical = { x + 1, y - 3, x + 2, y + 5 };
            FillRect(hdc, &horizontal, dcBrush);
            FillRect(hdc, &vertical, dcBrush);
        }
    }

    SetDCBrushColor(hdc, oldColor);
}

void ReleaseCosmicBackgroundCache() {
    if (g_cosmicBackgroundCacheDC && g_cosmicBackgroundCacheOldBitmap) {
        SelectObject(g_cosmicBackgroundCacheDC, g_cosmicBackgroundCacheOldBitmap);
    }
    if (g_cosmicBackgroundCacheBitmap) DeleteObject(g_cosmicBackgroundCacheBitmap);
    if (g_cosmicBackgroundCacheDC) DeleteDC(g_cosmicBackgroundCacheDC);
    g_cosmicBackgroundCacheDC = nullptr;
    g_cosmicBackgroundCacheBitmap = nullptr;
    g_cosmicBackgroundCacheOldBitmap = nullptr;
    g_cosmicBackgroundCacheBits = nullptr;
    g_cosmicBackgroundCacheWidth = 0;
    g_cosmicBackgroundCacheHeight = 0;
    g_cosmicBackgroundCacheTheme = -1;
    g_themeEffectSamples.clear();
}

Gdiplus::Bitmap* LoadPngResourceFromModule(int resourceId) {
    HMODULE module = g_moduleHandle ? g_moduleHandle : GetModuleHandleW(nullptr);
    if (!module) return nullptr;

    HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
    if (!resource) return nullptr;
    DWORD resourceSize = SizeofResource(module, resource);
    HGLOBAL loadedResource = LoadResource(module, resource);
    const void* resourceBytes = loadedResource ? LockResource(loadedResource) : nullptr;
    if (!resourceBytes || resourceSize == 0) return nullptr;

    HGLOBAL streamBuffer = GlobalAlloc(GMEM_MOVEABLE, resourceSize);
    if (!streamBuffer) return nullptr;
    void* streamBytes = GlobalLock(streamBuffer);
    if (!streamBytes) {
        GlobalFree(streamBuffer);
        return nullptr;
    }
    memcpy(streamBytes, resourceBytes, resourceSize);
    GlobalUnlock(streamBuffer);

    IStream* stream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(streamBuffer, TRUE, &stream)) || !stream) {
        GlobalFree(streamBuffer);
        return nullptr;
    }

    Gdiplus::Bitmap* decoded = Gdiplus::Bitmap::FromStream(stream, FALSE);
    Gdiplus::Bitmap* result = nullptr;
    if (decoded && decoded->GetLastStatus() == Gdiplus::Ok &&
        decoded->GetWidth() > 0 && decoded->GetHeight() > 0) {
        result = decoded->Clone(0, 0, (INT)decoded->GetWidth(), (INT)decoded->GetHeight(), PixelFormat32bppARGB);
        if (result && result->GetLastStatus() != Gdiplus::Ok) {
            delete result;
            result = nullptr;
        }
    }

    delete decoded;
    stream->Release();
    return result;
}

bool InitializeCosmicImageAssets() {
    if (g_cosmicGdiplusToken != 0) return true;

    Gdiplus::GdiplusStartupInput startupInput;
    if (Gdiplus::GdiplusStartup(&g_cosmicGdiplusToken, &startupInput, nullptr) != Gdiplus::Ok) {
        g_cosmicGdiplusToken = 0;
        return false;
    }

    g_themeBackgroundImages[GUI_THEME_COSMIC] = LoadPngResourceFromModule(IDR_COSMIC_BACKGROUND);
    g_themeBackgroundImages[GUI_THEME_NEON_CITY] = LoadPngResourceFromModule(IDR_THEME_NEON_CITY);
    g_themeBackgroundImages[GUI_THEME_ENCHANTED_FOREST] = LoadPngResourceFromModule(IDR_THEME_ENCHANTED_FOREST);
    g_themeBackgroundImages[GUI_THEME_INFERNO] = LoadPngResourceFromModule(IDR_THEME_INFERNO);
    g_themeBackgroundImages[GUI_THEME_ARCTIC_AURORA] = LoadPngResourceFromModule(IDR_THEME_ARCTIC_AURORA);
    g_themeMotionSprites[GUI_THEME_NEON_CITY] = LoadPngResourceFromModule(IDR_THEME_NEON_TRAIN_SPRITE);
    g_themeMotionSprites[GUI_THEME_ENCHANTED_FOREST] = LoadPngResourceFromModule(IDR_THEME_FOREST_MOTH_SPRITE);
    g_themeMotionSprites[GUI_THEME_INFERNO] = LoadPngResourceFromModule(IDR_THEME_INFERNO_FLAME_SPRITE);
    g_themeMotionSprites[GUI_THEME_ARCTIC_AURORA] = LoadPngResourceFromModule(IDR_THEME_ARCTIC_SNOW_SPRITE);
    g_neonFlyingCarSprite = LoadPngResourceFromModule(IDR_THEME_NEON_FLYING_CAR_SPRITE);
    if (g_themeMotionSprites[GUI_THEME_NEON_CITY]) {
        g_themeMotionSprites[GUI_THEME_NEON_CITY]->RotateFlip(Gdiplus::RotateNoneFlipX);
    }
    g_cosmicPlanetSheetImage = LoadPngResourceFromModule(IDR_COSMIC_PLANETS);
    return g_themeBackgroundImages[GUI_THEME_COSMIC] != nullptr;
}

void ReleaseCosmicImageAssets() {
    ReleaseCosmicBackgroundCache();
    delete g_cosmicPlanetSheetImage;
    delete g_neonFlyingCarSprite;
    g_cosmicPlanetSheetImage = nullptr;
    g_neonFlyingCarSprite = nullptr;
    for (int theme = 0; theme < GUI_THEME_COUNT; ++theme) {
        delete g_themeBackgroundImages[theme];
        g_themeBackgroundImages[theme] = nullptr;
        delete g_themeMotionSprites[theme];
        g_themeMotionSprites[theme] = nullptr;
    }
    if (g_cosmicGdiplusToken != 0) {
        Gdiplus::GdiplusShutdown(g_cosmicGdiplusToken);
        g_cosmicGdiplusToken = 0;
    }
}

void BuildThemeEffectSamples(int theme, int width, int height) {
    g_themeEffectSamples.clear();
    if (!g_cosmicBackgroundCacheBits || width <= 0 || height <= 0 ||
        theme == GUI_THEME_CLASSIC || theme == GUI_THEME_COSMIC) return;

    g_themeEffectSamples.reserve(520);
    const size_t sampleLimit = 520;
    size_t infernoAcceptedCount = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            std::uint32_t pixel = g_cosmicBackgroundCacheBits[(size_t)y * (size_t)width + (size_t)x];
            int b = (int)(pixel & 0xFFu);
            int g = (int)((pixel >> 8) & 0xFFu);
            int r = (int)((pixel >> 16) & 0xFFu);
            unsigned int hash = HashCosmicPixel(x, y, (unsigned int)(theme * 41 + 701));
            bool accepted = false;
            unsigned char kind = 0;

            if (theme == GUI_THEME_NEON_CITY) {
                bool cyan = b > 135 && g > 82 && r < 145 && b - r > 35;
                bool magenta = r > 140 && b > 110 && g < 155 && (r + b) - g * 2 > 70;
                bool amber = r > 175 && g > 75 && g < 180 && b < 110;
                accepted = (cyan || magenta || amber) && (hash % 11u == 0u);
                kind = cyan ? 0 : (magenta ? 1 : 2);
            }
            else if (theme == GUI_THEME_ENCHANTED_FOREST) {
                bool firefly = r > 145 && g > 120 && b < 145 && r + g > 300;
                bool magicPlant = y > height / 2 && g > 115 && b > 105 && r < 175 && g + b > 275;
                accepted = (firefly && hash % 2u == 0u) || (magicPlant && hash % 17u == 0u);
                kind = firefly ? 0 : 1;
            }
            else if (theme == GUI_THEME_INFERNO) {
                bool lava = r > 150 && g > 28 && b < 105 && r > g + 55;
                accepted = lava && hash % 14u == 0u;
                kind = g > 115 ? 1 : 0;
            }
            else if (theme == GUI_THEME_ARCTIC_AURORA) {
                bool aurora = y < height * 7 / 10 && b > 125 && g > 105 && r < 190 && (b - r > 25 || g - r > 20);
                bool star = y < height * 3 / 5 && r > 185 && g > 185 && b > 195;
                accepted = (aurora && hash % 13u == 0u) || (star && hash % 4u == 0u);
                kind = star ? 1 : 0;
            }

            if (!accepted) continue;
            if (theme == GUI_THEME_INFERNO) {
                // Reservoir-sample the complete painted scene. Stopping after
                // the first 520 hits only animated the ceiling lava, leaving
                // the large river and foreground falls completely static.
                ++infernoAcceptedCount;
                GuiThemeEffectSample sample = { x, y, RGB(r, g, b), kind };
                if (g_themeEffectSamples.size() < sampleLimit) {
                    g_themeEffectSamples.push_back(sample);
                }
                else {
                    unsigned int replacementHash = HashCosmicPixel(x, y, 2027u);
                    size_t replacementIndex = (size_t)replacementHash % infernoAcceptedCount;
                    if (replacementIndex < sampleLimit) {
                        g_themeEffectSamples[replacementIndex] = sample;
                    }
                }
                continue;
            }
            g_themeEffectSamples.push_back({ x, y, RGB(r, g, b), kind });
            if (g_themeEffectSamples.size() >= sampleLimit) return;
        }
    }
}

bool EnsureCosmicBackgroundCache(HDC targetDC, int clientW, int clientH) {
    int theme = NormalizeGuiTheme(g_guiTheme);
    Gdiplus::Bitmap* backgroundImage = g_themeBackgroundImages[theme];
    if (!backgroundImage || !targetDC || clientW <= 0 || clientH <= 0) return false;

    int wantedW = clientW + 96;
    int wantedH = clientH + 128;
    if (g_cosmicBackgroundCacheDC && g_cosmicBackgroundCacheBitmap &&
        g_cosmicBackgroundCacheWidth == wantedW && g_cosmicBackgroundCacheHeight == wantedH &&
        g_cosmicBackgroundCacheTheme == theme) {
        return true;
    }

    ReleaseCosmicBackgroundCache();
    g_cosmicBackgroundCacheDC = CreateCompatibleDC(targetDC);
    BITMAPINFO bitmapInfo = {};
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = wantedW;
    bitmapInfo.bmiHeader.biHeight = -wantedH;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;
    void* bitmapBits = nullptr;
    g_cosmicBackgroundCacheBitmap = CreateDIBSection(
        targetDC, &bitmapInfo, DIB_RGB_COLORS, &bitmapBits, nullptr, 0);
    g_cosmicBackgroundCacheBits = static_cast<std::uint32_t*>(bitmapBits);
    if (!g_cosmicBackgroundCacheDC || !g_cosmicBackgroundCacheBitmap) {
        ReleaseCosmicBackgroundCache();
        return false;
    }

    g_cosmicBackgroundCacheOldBitmap = SelectObject(g_cosmicBackgroundCacheDC, g_cosmicBackgroundCacheBitmap);
    g_cosmicBackgroundCacheWidth = wantedW;
    g_cosmicBackgroundCacheHeight = wantedH;
    g_cosmicBackgroundCacheTheme = theme;

    Gdiplus::Status status = Gdiplus::GenericError;
    {
        Gdiplus::Graphics graphics(g_cosmicBackgroundCacheDC);
        graphics.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
        graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
        Gdiplus::Rect destination(0, 0, wantedW, wantedH);
        status = graphics.DrawImage(
            backgroundImage,
            destination,
            0, 0,
            (INT)backgroundImage->GetWidth(),
            (INT)backgroundImage->GetHeight(),
            Gdiplus::UnitPixel);
    }
    if (status != Gdiplus::Ok) {
        ReleaseCosmicBackgroundCache();
        return false;
    }
    BuildThemeEffectSamples(theme, wantedW, wantedH);
    return true;
}

bool DrawCosmicImageBackground(HDC hdc, const RECT& rect, float seconds) {
    (void)seconds;
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    if (!EnsureCosmicBackgroundCache(hdc, width, height)) return false;

    int travelX = g_cosmicBackgroundCacheWidth - width;
    int travelY = g_cosmicBackgroundCacheHeight - height;
    int sourceX = travelX > 0 ? travelX / 2 : 0;
    int sourceY = travelY > 0 ? travelY / 2 : 0;
    BitBlt(hdc, rect.left, rect.top, width, height,
        g_cosmicBackgroundCacheDC, sourceX, sourceY, SRCCOPY);
    return true;
}

void DrawCosmicPlanetFrame(
    Gdiplus::Graphics& graphics,
    int row,
    int frame,
    const Gdiplus::RectF& destination,
    float opacity) {
    if (!g_cosmicPlanetSheetImage) return;

    struct SpriteBounds {
        int left;
        int top;
        int width;
        int height;
    };

    // The generated sprite sheet is not registered inside each cell: later
    // frames sit progressively farther left. Crop every frame to its actual
    // artwork so all frames share one stable centre and visible size.
    static const SpriteBounds kMoonBounds[8] = {
        { 58, 106, 185, 185 }, { 50, 106, 185, 185 },
        { 46, 106, 185, 185 }, { 38, 106, 181, 185 },
        { 30, 106, 181, 185 }, { 26, 106, 181, 185 },
        { 18, 106, 183, 185 }, {  8, 106, 183, 185 }
    };
    static const SpriteBounds kRingedBounds[8] = {
        { 50, 100, 197, 153 }, { 42, 102, 197, 151 },
        { 36, 100, 197, 153 }, { 28, 100, 197, 153 },
        { 22, 100, 197, 153 }, { 14, 100, 197, 153 },
        {  8, 100, 197, 153 }, {  0, 100, 197, 153 }
    };

    int sheetW = (int)g_cosmicPlanetSheetImage->GetWidth();
    int sheetH = (int)g_cosmicPlanetSheetImage->GetHeight();
    frame = frame < 0 ? 0 : (frame > 7 ? 7 : frame);
    int cellLeft = (sheetW * frame) / 8;
    int rowTop = (sheetH * row) / 2;
    const SpriteBounds& bounds = row == 0 ? kMoonBounds[frame] : kRingedBounds[frame];
    int sourceLeft = cellLeft + bounds.left;
    int sourceTop = rowTop + bounds.top;

    Gdiplus::ImageAttributes imageAttributes;
    imageAttributes.SetColorKey(
        Gdiplus::Color(0, 238, 0),
        Gdiplus::Color(32, 255, 32),
        Gdiplus::ColorAdjustTypeBitmap);
    if (opacity < 0.999f) {
        Gdiplus::ColorMatrix opacityMatrix = {
            1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, ClampFloat(opacity, 0.0f, 1.0f), 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f, 1.0f
        };
        imageAttributes.SetColorMatrix(
            &opacityMatrix,
            Gdiplus::ColorMatrixFlagsDefault,
            Gdiplus::ColorAdjustTypeBitmap);
    }

    graphics.DrawImage(
        g_cosmicPlanetSheetImage,
        destination,
        (Gdiplus::REAL)sourceLeft,
        (Gdiplus::REAL)sourceTop,
        (Gdiplus::REAL)bounds.width,
        (Gdiplus::REAL)bounds.height,
        Gdiplus::UnitPixel,
        &imageAttributes);
}

void DrawSmoothCosmicPlanet(
    Gdiplus::Graphics& graphics,
    int row,
    float framePosition,
    const Gdiplus::RectF& destination) {
    float wrappedPosition = (float)std::fmod(framePosition, 8.0f);
    if (wrappedPosition < 0.0f) wrappedPosition += 8.0f;
    int currentFrame = (int)std::floor(wrappedPosition);
    int nextFrame = (currentFrame + 1) % 8;
    float frameFraction = wrappedPosition - (float)currentFrame;
    float easedBlend = frameFraction * frameFraction * (3.0f - 2.0f * frameFraction);

    DrawCosmicPlanetFrame(graphics, row, currentFrame, destination, 1.0f);
    if (easedBlend > 0.001f) {
        DrawCosmicPlanetFrame(graphics, row, nextFrame, destination, easedBlend);
    }
}

void DrawCosmicPlanets(HDC hdc, const RECT& rect, float seconds) {
    if (!g_cosmicPlanetSheetImage) return;

    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    float framePosition = seconds * 3.0f;

    Gdiplus::Graphics graphics(hdc);
    graphics.SetCompositingMode(Gdiplus::CompositingModeSourceOver);
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    graphics.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);

    const float moonSize = 154.0f;
    const float moonCenterX = (float)(rect.left + width) - 115.0f;
    const float moonCenterY = (float)rect.top + (float)height * 0.59f;
    Gdiplus::RectF moonDestination(
        moonCenterX - moonSize * 0.5f,
        moonCenterY - moonSize * 0.5f,
        moonSize,
        moonSize);
    DrawSmoothCosmicPlanet(graphics, 0, framePosition, moonDestination);

    const float ringedWidth = 148.0f;
    const float ringedHeight = 115.0f;
    const float ringedCenterX = (float)rect.left + 100.0f;
    const float ringedCenterY = (float)rect.top + (float)height * 0.83f;
    Gdiplus::RectF ringedDestination(
        ringedCenterX - ringedWidth * 0.5f,
        ringedCenterY - ringedHeight * 0.5f,
        ringedWidth,
        ringedHeight);
    DrawSmoothCosmicPlanet(graphics, 1, framePosition + 4.0f, ringedDestination);
}

void DrawNeonCityAnimation(HDC hdc, const RECT& rect, float seconds) {
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0) return;

    const GuiPalette& palette = GetGuiPalette();
    HBRUSH brush = static_cast<HBRUSH>(GetStockObject(DC_BRUSH));
    COLORREF oldColor = SetDCBrushColor(hdc, palette.accentAlt);
    for (int i = 0; i < 44; ++i) {
        unsigned int hash = HashCosmicPixel(i, 91, 211u);
        int x = rect.left + (int)(hash % (unsigned int)width);
        float speed = 7.0f + (float)((hash >> 9) % 7u);
        int y = rect.top + WrapCosmicCoordinate((int)((hash >> 16) % (unsigned int)height) + (int)(seconds * speed), height);
        int length = 4 + (int)((hash >> 25) % 7u);
        COLORREF raw = (hash & 1u) ? palette.accentAlt : palette.accent;
        SetDCBrushColor(hdc, BlendGuiColor(palette.bg, raw, 0.44f + (float)(hash & 3u) * 0.10f));
        RECT drop = { x, y, x + 1, y + length };
        FillRect(hdc, &drop, brush);
    }

    // Slow hover traffic gives the skyline depth without moving the artwork.
    for (int i = 0; i < 7; ++i) {
        unsigned int hash = HashCosmicPixel(i, 317, 719u);
        int laneY = rect.top + height / 5 + (int)((hash >> 11) % (unsigned int)max(1, height * 3 / 5));
        float speed = 10.0f + (float)((hash >> 23) % 9u);
        int travel = width + 80;
        int localX = WrapCosmicCoordinate((int)((hash & 2047u) + seconds * speed), travel) - 40;
        bool reverse = (hash & 1u) != 0u;
        int x = reverse ? rect.right - localX : rect.left + localX;
        COLORREF carColor = (hash & 2u) ? palette.accent : palette.accentAlt;
        SetDCBrushColor(hdc, BlendGuiColor(palette.bg, carColor, 0.82f));
        RECT trail = reverse ? RECT{ x, laneY, x + 22, laneY + 1 } : RECT{ x - 22, laneY, x, laneY + 1 };
        FillRect(hdc, &trail, brush);
        SetDCBrushColor(hdc, carColor);
        RECT car = reverse ? RECT{ x - 8, laneY - 1, x, laneY + 3 } : RECT{ x, laneY - 1, x + 8, laneY + 3 };
        FillRect(hdc, &car, brush);
    }

    for (int i = 0; i < 8; ++i) {
        unsigned int hash = HashCosmicPixel(i, 349, 733u);
        float pulse = 0.35f + 0.55f * (0.5f + 0.5f * std::sin(seconds * 0.72f + (float)i * 1.31f));
        int x = rect.left + 28 + (int)(hash % (unsigned int)max(1, width - 56));
        int y = rect.top + height / 4 + (int)((hash >> 18) % (unsigned int)max(1, height / 2));
        SetDCBrushColor(hdc, BlendGuiColor(palette.bg, (i & 1) ? palette.accent : palette.accentAlt, pulse));
        RECT sign = { x, y, x + 3 + (int)(hash & 7u), y + 2 };
        FillRect(hdc, &sign, brush);
    }
    SetDCBrushColor(hdc, oldColor);
}

void DrawEnchantedForestAnimation(HDC hdc, const RECT& rect, float seconds) {
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0) return;

    const GuiPalette& palette = GetGuiPalette();
    HBRUSH brush = static_cast<HBRUSH>(GetStockObject(DC_BRUSH));
    COLORREF oldColor = SetDCBrushColor(hdc, palette.accent);
    for (int i = 0; i < 30; ++i) {
        unsigned int hash = HashCosmicPixel(i, 137, 307u);
        float phase = (float)(hash & 1023u) * 0.0061359f;
        int baseX = (int)((hash >> 10) % (unsigned int)width);
        int baseY = (int)((hash >> 20) % (unsigned int)height);
        int x = rect.left + WrapCosmicCoordinate(baseX + (int)(std::sin(seconds * 0.22f + phase) * 12.0f), width);
        int y = rect.top + WrapCosmicCoordinate(baseY + (int)(std::cos(seconds * 0.17f + phase * 1.7f) * 8.0f), height);
        float pulse = 0.40f + 0.50f * (0.5f + 0.5f * std::sin(seconds * 0.9f + phase * 2.0f));
        COLORREF color = BlendGuiColor(palette.bg, (hash & 3u) ? palette.accent : palette.accentAlt, pulse);
        SetDCBrushColor(hdc, color);
        int size = pulse > 0.72f ? 2 : 1;
        RECT glow = { x, y, x + size, y + size };
        FillRect(hdc, &glow, brush);
        if (pulse > 0.83f) {
            RECT cross = { x - 2, y, x + 4, y + 1 };
            FillRect(hdc, &cross, brush);
        }
    }

    // Leaves drift across the scene on separate paths while the fireflies hover.
    for (int i = 0; i < 18; ++i) {
        unsigned int hash = HashCosmicPixel(i, 383, 809u);
        float phase = (float)(hash & 1023u) * 0.0061359f;
        float speed = 3.0f + (float)((hash >> 12) % 6u);
        int x = rect.left + WrapCosmicCoordinate((int)((hash >> 19) % (unsigned int)width) + (int)(seconds * speed), width);
        int baseY = rect.top + (int)((hash >> 4) % (unsigned int)height);
        int y = baseY + (int)(std::sin(seconds * 0.28f + phase) * 13.0f);
        COLORREF leafColor = (hash & 1u) ? palette.accent : ((hash & 2u) ? palette.accentAlt : palette.starPink);
        SetDCBrushColor(hdc, BlendGuiColor(palette.bg, leafColor, 0.58f));
        RECT leaf = { x, y, x + ((hash & 4u) ? 3 : 2), y + 2 };
        FillRect(hdc, &leaf, brush);
    }

    // Broken pixel wisps read as moonlit mist but stay extremely cheap to draw.
    for (int band = 0; band < 3; ++band) {
        int bandY = rect.top + height * (52 + band * 13) / 100;
        int offset = WrapCosmicCoordinate((int)(seconds * (3.0f + band)) + band * 97, width + 150) - 150;
        SetDCBrushColor(hdc, BlendGuiColor(palette.bg, palette.accentAlt, 0.24f + band * 0.05f));
        for (int segment = 0; segment < 12; ++segment) {
            int x = rect.left + offset + segment * 42;
            RECT mist = { x, bandY + (segment & 1), x + 24, bandY + 2 + (segment & 1) };
            FillRect(hdc, &mist, brush);
        }
    }
    SetDCBrushColor(hdc, oldColor);
}

void DrawInfernoAnimation(HDC hdc, const RECT& rect, float seconds) {
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0) return;

    const GuiPalette& palette = GetGuiPalette();
    HBRUSH brush = static_cast<HBRUSH>(GetStockObject(DC_BRUSH));
    COLORREF oldColor = SetDCBrushColor(hdc, palette.accent);
    // Ember sources sit over the painted lava falls, river and volcano. Three
    // depth layers create tiny distant cinders, mid-sized embers and a few
    // bright foreground sparks with short upward-motion trails.
    static const float kSourceX[8] = { 0.06f, 0.16f, 0.31f, 0.49f, 0.66f, 0.72f, 0.86f, 0.95f };
    static const float kSourceY[8] = { 0.71f, 0.89f, 0.95f, 0.98f, 0.77f, 0.55f, 0.88f, 0.69f };
    static const int kLayerCount[3] = { 18, 22, 14 };
    static const float kLayerSpeed[3] = { 6.0f, 10.0f, 15.0f };
    static const float kLayerRise[3] = { 105.0f, 165.0f, 225.0f };
    static const float kLayerBrightness[3] = { 0.44f, 0.61f, 0.80f };

    for (int layer = 0; layer < 3; ++layer) {
        for (int i = 0; i < kLayerCount[layer]; ++i) {
            unsigned int hash = HashCosmicPixel(i, layer * 149 + 173, 401u + (unsigned int)layer * 131u);
            int source = (int)((hash >> 3) % 8u);
            float riseDistance = kLayerRise[layer] + (float)((hash >> 12) % 75u);
            float speed = kLayerSpeed[layer] + (float)((hash >> 20) % 8u);
            float seed = (float)(hash & 4095u) / 4096.0f;
            float progress = (float)std::fmod(seed + seconds * speed / riseDistance, 1.0f);
            if (progress < 0.0f) progress += 1.0f;

            float sourceX = (float)rect.left + kSourceX[source] * (float)width;
            float sourceY = (float)rect.top + kSourceY[source] * (float)height;
            float phase = (float)((hash >> 7) & 511u) * 0.012295f;
            float drift = std::sin(progress * 5.4f + seconds * 0.18f + phase) *
                (4.0f + (float)layer * 4.5f);
            drift += ((float)((int)((hash >> 25) & 15u) - 7) * 0.22f) * progress * 8.0f;
            int x = (int)std::round(sourceX + drift);
            int y = (int)std::round(sourceY - progress * riseDistance);
            if (x < rect.left || x >= rect.right || y < rect.top || y >= rect.bottom) continue;

            float fadeIn = ClampFloat(progress / 0.08f, 0.0f, 1.0f);
            float fadeOut = ClampFloat((1.0f - progress) / 0.22f, 0.0f, 1.0f);
            float flicker = 0.78f + 0.22f * std::sin(seconds * 0.72f + phase + progress * 8.0f);
            float brightness = kLayerBrightness[layer] * min(fadeIn, fadeOut) * flicker;
            if (brightness < 0.10f) continue;
            COLORREF emberTint = (hash & 3u) == 0u ? palette.starPrimary :
                ((hash & 1u) ? palette.accent : palette.accentAlt);

            int size = layer == 0 ? 1 : (layer == 1 ? ((hash & 7u) == 0u ? 2 : 1) : 2);
            int trailLength = layer == 0 ? 0 : 2 + (int)((hash >> 16) % (unsigned int)(3 + layer * 2));
            if (trailLength > 0) {
                SetDCBrushColor(hdc, BlendGuiColor(
                    palette.bg,
                    emberTint,
                    ClampFloat(brightness * 0.38f, 0.10f, 0.52f)));
                RECT trail = {
                    x,
                    y + size,
                    min(x + max(1, size - 1), rect.right),
                    min(y + size + trailLength, rect.bottom)
                };
                if (trail.left < trail.right && trail.top < trail.bottom) FillRect(hdc, &trail, brush);
            }

            SetDCBrushColor(hdc, BlendGuiColor(
                palette.bg,
                emberTint,
                ClampFloat(brightness, 0.18f, 0.92f)));
            RECT ember = { x, y, min(x + size, rect.right), min(y + size, rect.bottom) };
            FillRect(hdc, &ember, brush);

            if (layer == 2 && (hash & 15u) == 0u && x + 2 < rect.right && y + 2 < rect.bottom) {
                SetDCBrushColor(hdc, BlendGuiColor(palette.bg, palette.starPrimary, brightness * 0.72f));
                RECT hotCore = { x + 1, y, x + 2, y + 1 };
                FillRect(hdc, &hotCore, brush);
            }
        }
    }
    SetDCBrushColor(hdc, oldColor);
}

void DrawArcticAuroraAnimation(HDC hdc, const RECT& rect, float seconds) {
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0) return;

    const GuiPalette& palette = GetGuiPalette();
    HBRUSH brush = static_cast<HBRUSH>(GetStockObject(DC_BRUSH));
    COLORREF oldColor = SetDCBrushColor(hdc, palette.starPrimary);

    // Two slowly flowing pixel ribbons brighten the existing painted aurora.
    for (int ribbon = 0; ribbon < 2; ++ribbon) {
        COLORREF auroraColor = ribbon == 0 ? palette.accent : palette.starPink;
        SetDCBrushColor(hdc, BlendGuiColor(palette.bg, auroraColor, 0.42f));
        for (int x = rect.left; x < rect.right; x += 10) {
            float nx = (float)(x - rect.left) / (float)max(1, width);
            float wave = std::sin(nx * (7.0f + ribbon * 1.7f) + seconds * (0.19f + ribbon * 0.04f));
            int y = rect.top + height * (18 + ribbon * 10) / 100 + (int)(wave * (12.0f + ribbon * 5.0f));
            RECT ribbonPixel = { x, y, min(x + 7, rect.right), y + 2 };
            FillRect(hdc, &ribbonPixel, brush);
        }
    }

    // Fine ambient flakes give the storm depth without moving the artwork.
    const int ambientSpanX = width + 32;
    const int ambientSpanY = height + 24;
    for (int i = 0; i < 42; ++i) {
        unsigned int hash = HashCosmicPixel(i, 211, 503u);
        float speedX = 9.0f + (float)((hash >> 9) % 9u);
        float speedY = 2.0f + (float)((hash >> 17) % 5u);
        int phaseX = (int)((hash >> 2) % (unsigned int)ambientSpanX);
        int phaseY = (int)((hash >> 19) % (unsigned int)ambientSpanY);
        int x = rect.left - 16 + WrapCosmicCoordinate(phaseX + (int)(seconds * speedX), ambientSpanX);
        int y = rect.top - 12 + WrapCosmicCoordinate(phaseY + (int)(seconds * speedY), ambientSpanY);
        if (x < rect.left || x >= rect.right || y < rect.top || y >= rect.bottom) continue;

        COLORREF flakeColor = (hash & 4u) ? palette.starPrimary : palette.accentAlt;
        float brightness = 0.42f + (float)((hash >> 27) & 3u) * 0.075f;
        SetDCBrushColor(hdc, BlendGuiColor(palette.bg, flakeColor, brightness));
        int size = (hash % 17u) == 0u ? 2 : 1;
        RECT flake = { x, y, min(x + size, rect.right), min(y + size, rect.bottom) };
        FillRect(hdc, &flake, brush);
    }

    // Four independent blizzard bands range from a thin high-altitude gust to
    // a broad foreground whiteout. Their density, streak size and speed all
    // increase separately, avoiding a repeated sheet-of-snow appearance.
    static const int kGustParticleCount[4] = { 16, 24, 32, 40 };
    static const float kGustCentreY[4] = { 0.17f, 0.38f, 0.61f, 0.79f };
    static const float kGustBandHeight[4] = { 0.045f, 0.085f, 0.14f, 0.22f };
    static const float kGustSpeed[4] = { 22.0f, 30.0f, 40.0f, 52.0f };
    static const int kGustBaseLength[4] = { 3, 5, 8, 12 };
    static const int kGustLengthRange[4] = { 3, 5, 8, 11 };
    static const int kGustThickness[4] = { 1, 1, 2, 2 };
    static const float kGustBrightness[4] = { 0.44f, 0.54f, 0.66f, 0.78f };

    for (int gust = 0; gust < 4; ++gust) {
        float breathe = 0.72f + 0.28f * std::sin(
            seconds * (0.12f - (float)gust * 0.012f) + (float)gust * 1.73f);
        float centreY = (float)rect.top + kGustCentreY[gust] * (float)height +
            std::sin(seconds * (0.075f + (float)gust * 0.009f) + (float)gust * 2.1f) *
            (8.0f + (float)gust * 5.0f);
        float bandHeight = max(12.0f, kGustBandHeight[gust] * (float)height);
        int travelMargin = 34 + gust * 8;
        int travelSpan = width + travelMargin * 2;

        COLORREF gustTint = gust < 2 ? palette.accentAlt : palette.starPrimary;
        SetDCBrushColor(hdc, BlendGuiColor(
            palette.bg,
            gustTint,
            ClampFloat(kGustBrightness[gust] * breathe, 0.30f, 0.86f)));

        for (int i = 0; i < kGustParticleCount[gust]; ++i) {
            unsigned int hash = HashCosmicPixel(i, gust * 137 + 457, 1009u + (unsigned int)gust * 83u);
            float speed = kGustSpeed[gust] + (float)((hash >> 11) % 13u);
            int baseX = (int)((hash >> 18) % (unsigned int)travelSpan);
            int headX = rect.left - travelMargin +
                WrapCosmicCoordinate(baseX + (int)(seconds * speed), travelSpan);

            float offsetUnit = (float)(hash & 1023u) / 1023.0f - 0.5f;
            float diagonal = ((float)headX - ((float)rect.left + (float)width * 0.5f)) * 0.14f;
            float flutter = std::sin(seconds * 0.19f + (float)(hash & 255u) * 0.031f) *
                (1.5f + (float)gust);
            int headY = (int)std::round(centreY + offsetUnit * bandHeight + diagonal + flutter);
            if (headX < rect.left - 24 || headX >= rect.right + 2 ||
                headY < rect.top - 8 || headY >= rect.bottom + 2) continue;

            int length = kGustBaseLength[gust] +
                (int)((hash >> 5) % (unsigned int)kGustLengthRange[gust]);
            int thickness = kGustThickness[gust] +
                ((gust >= 2 && (hash & 31u) == 0u) ? 1 : 0);
            int stepSize = gust < 2 ? 2 : 3;
            for (int tail = 0; tail < length; tail += stepSize) {
                int x = headX - tail;
                int y = headY - (tail * 2) / 7;
                int blockW = min(stepSize + (gust >= 2 ? 1 : 0), length - tail);
                RECT streak = {
                    max(x, rect.left),
                    max(y, rect.top),
                    min(x + blockW, rect.right),
                    min(y + thickness, rect.bottom)
                };
                if (streak.left < streak.right && streak.top < streak.bottom) {
                    FillRect(hdc, &streak, brush);
                }
            }
        }
    }
    SetDCBrushColor(hdc, oldColor);
}

void DrawCosmicOrbitalDust(HDC hdc, const RECT& rect, float seconds) {
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0) return;

    const GuiPalette& palette = GetGuiPalette();
    HBRUSH brush = static_cast<HBRUSH>(GetStockObject(DC_BRUSH));
    COLORREF oldColor = SetDCBrushColor(hdc, palette.starBlue);
    float centerX = (float)rect.left + (float)width * 0.57f;
    float centerY = (float)rect.top + (float)height * 0.70f;
    for (int i = 0; i < 52; ++i) {
        unsigned int hash = HashCosmicPixel(i, 491, 1103u);
        float radius = 34.0f + (float)(hash % 190u);
        float angle = (float)i * 0.4833f + seconds * (0.025f + (float)(hash & 7u) * 0.002f);
        int x = (int)(centerX + std::cos(angle) * radius * 1.28f);
        int y = (int)(centerY + std::sin(angle) * radius * 0.38f);
        if (x < rect.left || x >= rect.right || y < rect.top || y >= rect.bottom) continue;
        COLORREF raw = (hash & 3u) ? palette.starBlue : palette.starPink;
        SetDCBrushColor(hdc, BlendGuiColor(palette.bg, raw, 0.38f + (float)(hash & 3u) * 0.12f));
        int size = (hash & 15u) == 0u ? 2 : 1;
        RECT dust = { x, y, x + size, y + size };
        FillRect(hdc, &dust, brush);
    }
    SetDCBrushColor(hdc, oldColor);
}

void DrawArtworkMatchedThemeAnimation(HDC hdc, const RECT& rect, float seconds) {
    if (g_themeEffectSamples.empty()) return;

    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0) return;
    int sourceX = max(0, (g_cosmicBackgroundCacheWidth - width) / 2);
    int sourceY = max(0, (g_cosmicBackgroundCacheHeight - height) / 2);
    int theme = NormalizeGuiTheme(g_guiTheme);
    const GuiPalette& palette = GetGuiPalette();
    HBRUSH brush = static_cast<HBRUSH>(GetStockObject(DC_BRUSH));
    COLORREF oldColor = SetDCBrushColor(hdc, palette.accent);

    for (size_t i = 0; i < g_themeEffectSamples.size(); ++i) {
        const GuiThemeEffectSample& sample = g_themeEffectSamples[i];
        int x = rect.left + sample.x - sourceX;
        int y = rect.top + sample.y - sourceY;
        COLORREF color = sample.color;
        int drawX = x;
        int drawY = y;
        int size = 1;

        if (theme == GUI_THEME_NEON_CITY) {
            float chase = 0.5f + 0.5f * std::sin(
                seconds * 0.92f - (float)sample.y * 0.042f + (float)sample.x * 0.018f);
            if (chase < 0.38f) continue;
            COLORREF glow = sample.kind == 0 ? palette.accentAlt :
                (sample.kind == 1 ? palette.accent : RGB(255, 170, 72));
            color = BlendGuiColor(sample.color, glow, 0.16f + chase * 0.48f);
            size = chase > 0.88f && (i % 9u == 0u) ? 2 : 1;
        }
        else if (theme == GUI_THEME_ENCHANTED_FOREST) {
            float phase = seconds * 0.34f + (float)i * 1.731f;
            float pulse = 0.5f + 0.5f * std::sin(seconds * 0.78f + (float)i * 0.91f);
            if (sample.kind == 0) {
                drawX += (int)std::round(std::sin(phase) * 4.0f);
                drawY += (int)std::round(std::cos(phase * 0.77f) * 3.0f);
                color = BlendGuiColor(sample.color, palette.starPrimary, 0.18f + pulse * 0.55f);
                size = pulse > 0.78f && (i % 5u == 0u) ? 2 : 1;
            }
            else {
                if (pulse < 0.46f) continue;
                color = BlendGuiColor(sample.color, palette.accentAlt, 0.10f + pulse * 0.24f);
            }
        }
        else if (theme == GUI_THEME_INFERNO) {
            // A descending phase makes bright crests travel down the sampled
            // lava falls and toward the foreground along the river. A slower
            // cross-current breaks up the wave so it reads as molten flow.
            float downstream = std::sin(
                seconds * 0.94f - (float)sample.y * 0.056f + (float)sample.x * 0.004f);
            float crossCurrent = std::sin(
                seconds * 0.41f - (float)sample.y * 0.021f - (float)sample.x * 0.013f +
                (float)(i % 17u) * 0.11f);
            float flow = downstream * 0.76f + crossCurrent * 0.24f;
            float brightness = 0.5f + 0.5f * flow;
            if (brightness < 0.18f) continue;
            COLORREF hotColor = sample.kind == 1 ? RGB(255, 239, 126) : palette.accent;
            color = BlendGuiColor(sample.color, hotColor, 0.16f + brightness * 0.57f);
            size = brightness > 0.84f && (i % 7u == 0u) ? 2 : 1;
            if (downstream > 0.72f && sample.kind == 1) drawY += 1;
        }
        else if (theme == GUI_THEME_ARCTIC_AURORA) {
            float ribbon = std::sin(seconds * 0.31f + (float)sample.y * 0.029f + (float)sample.x * 0.006f);
            float pulse = 0.5f + 0.5f * std::sin(seconds * 0.53f + (float)i * 0.47f);
            if (sample.kind == 1) {
                if (pulse < 0.58f) continue;
                color = BlendGuiColor(sample.color, palette.starPrimary, 0.24f + pulse * 0.44f);
                size = pulse > 0.93f && (i % 7u == 0u) ? 2 : 1;
            }
            else {
                drawX += ribbon > 0.42f ? 1 : (ribbon < -0.42f ? -1 : 0);
                COLORREF auroraColor = (i & 1u) ? palette.accent : palette.starPink;
                color = BlendGuiColor(sample.color, auroraColor, 0.10f + pulse * 0.28f);
            }
        }

        if (drawX < rect.left || drawX >= rect.right || drawY < rect.top || drawY >= rect.bottom) continue;
        SetDCBrushColor(hdc, color);
        RECT pixel = { drawX, drawY, min(drawX + size, rect.right), min(drawY + size, rect.bottom) };
        FillRect(hdc, &pixel, brush);
    }

    // These cinders begin on real lava pixels, then rise and cool. They tie the
    // free-floating ember field directly back to the painted molten channels.
    if (theme == GUI_THEME_INFERNO) {
        size_t emberCount = min((size_t)30, g_themeEffectSamples.size());
        for (size_t i = 0; i < emberCount; ++i) {
            const GuiThemeEffectSample& sample = g_themeEffectSamples[(i * 19u) % g_themeEffectSamples.size()];
            unsigned int hash = HashCosmicPixel(sample.x, sample.y, 1871u);
            int riseSpan = 84 + (int)((hash >> 9) % 118u);
            float riseSpeed = 5.0f + (float)((hash >> 21) % 10u);
            int rise = WrapCosmicCoordinate(
                (int)(seconds * riseSpeed) + (int)(hash % (unsigned int)riseSpan),
                riseSpan);
            float phase = (float)(hash & 511u) * 0.012295f;
            int x = rect.left + sample.x - sourceX +
                (int)std::round(std::sin(seconds * 0.22f + phase + (float)rise * 0.028f) *
                    (3.0f + (float)((hash >> 16) & 3u)));
            int y = rect.top + sample.y - sourceY - rise;
            if (x < rect.left || x >= rect.right || y < rect.top || y >= rect.bottom) continue;

            float life = 1.0f - (float)rise / (float)riseSpan;
            float flicker = 0.72f + 0.28f * std::sin(seconds * 0.66f + phase);
            COLORREF emberColor = BlendGuiColor(
                palette.bg,
                (hash & 3u) == 0u ? palette.starPrimary : sample.color,
                ClampFloat(0.30f + life * flicker * 0.55f, 0.22f, 0.86f));
            SetDCBrushColor(hdc, emberColor);
            int emberW = (hash & 15u) == 0u ? 2 : 1;
            int emberH = emberW + ((hash & 7u) == 0u ? 2 : 1);
            RECT ember = { x, y, min(x + emberW, rect.right), min(y + emberH, rect.bottom) };
            FillRect(hdc, &ember, brush);
        }
    }

    SetDCBrushColor(hdc, oldColor);
}

void DrawThemeMotionSprite(
    HDC hdc,
    int theme,
    const Gdiplus::RectF& destination,
    float opacity,
    float rotationDegrees,
    Gdiplus::Bitmap* overrideSprite = nullptr,
    const Gdiplus::GraphicsPath* clipPath = nullptr) {
    Gdiplus::Bitmap* sprite = overrideSprite ? overrideSprite : g_themeMotionSprites[NormalizeGuiTheme(theme)];
    if (!sprite || destination.Width <= 0.0f || destination.Height <= 0.0f) return;

    Gdiplus::ImageAttributes attributes;
    if (theme == GUI_THEME_ENCHANTED_FOREST) {
        attributes.SetColorKey(
            Gdiplus::Color(180, 0, 180),
            Gdiplus::Color(255, 115, 255),
            Gdiplus::ColorAdjustTypeBitmap);
    }
    else {
        attributes.SetColorKey(
            Gdiplus::Color(0, 180, 0),
            Gdiplus::Color(95, 255, 105),
            Gdiplus::ColorAdjustTypeBitmap);
    }

    Gdiplus::ColorMatrix opacityMatrix = {
        1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, ClampFloat(opacity, 0.0f, 1.0f), 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 1.0f
    };
    attributes.SetColorMatrix(
        &opacityMatrix,
        Gdiplus::ColorMatrixFlagsDefault,
        Gdiplus::ColorAdjustTypeBitmap);

    Gdiplus::Graphics graphics(hdc);
    graphics.SetCompositingMode(Gdiplus::CompositingModeSourceOver);
    graphics.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    if (clipPath) {
        graphics.SetClip(clipPath, Gdiplus::CombineModeIntersect);
    }

    float centerX = destination.X + destination.Width * 0.5f;
    float centerY = destination.Y + destination.Height * 0.5f;
    graphics.TranslateTransform(centerX, centerY);
    if (std::fabs(rotationDegrees) > 0.01f) graphics.RotateTransform(rotationDegrees);
    Gdiplus::RectF centredDestination(
        -destination.Width * 0.5f,
        -destination.Height * 0.5f,
        destination.Width,
        destination.Height);
    graphics.DrawImage(
        sprite,
        centredDestination,
        0.0f, 0.0f,
        (Gdiplus::REAL)sprite->GetWidth(),
        (Gdiplus::REAL)sprite->GetHeight(),
        Gdiplus::UnitPixel,
        &attributes);
}

void DrawSceneMatchedSpriteAnimation(HDC hdc, const RECT& rect, float seconds) {
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0) return;

    int theme = NormalizeGuiTheme(g_guiTheme);
    if (theme == GUI_THEME_NEON_CITY) {
        float progress = (float)std::fmod(seconds * 0.11f, 1.0f);
        if (progress < 0.0f) progress += 1.0f;
        // Measured from the rendered Neon City railway: the portal is near
        // (0.50, 0.64), with the centre continuing fully past (1.24, 1.035).
        // At the 620x900 GUI aspect this path is 37.75 degrees.
        float portalX = (float)rect.left + 0.50f * (float)width;
        float portalY = (float)rect.top + 0.64f * (float)height;
        float pathDx = 0.74f * (float)width;
        float pathDy = 0.395f * (float)height;
        float pathLength = std::sqrt(pathDx * pathDx + pathDy * pathDy);
        float dirX = pathDx / pathLength;
        float dirY = pathDy / pathLength;
        const float startBehindPortal = 42.0f;
        float centreX = portalX - dirX * startBehindPortal +
            progress * (pathDx + dirX * startBehindPortal);
        float centreY = portalY - dirY * startBehindPortal +
            progress * (pathDy + dirY * startBehindPortal);
        float spriteW = 92.0f + progress * 96.0f;
        float spriteH = spriteW * 0.50f;
        Gdiplus::RectF train(
            centreX - spriteW * 0.5f,
            centreY - spriteH * 0.5f,
            spriteW,
            spriteH);

        // Hide the train behind the portal/building until it crosses the
        // railway entrance. This makes the nose emerge before the carriages.
        float normalX = -dirY;
        float normalY = dirX;
        float clipExtent = (float)max(width, height) * 2.5f;
        Gdiplus::PointF revealPoints[4] = {
            Gdiplus::PointF(portalX - normalX * clipExtent, portalY - normalY * clipExtent),
            Gdiplus::PointF(portalX + normalX * clipExtent, portalY + normalY * clipExtent),
            Gdiplus::PointF(
                portalX + normalX * clipExtent + dirX * clipExtent * 2.0f,
                portalY + normalY * clipExtent + dirY * clipExtent * 2.0f),
            Gdiplus::PointF(
                portalX - normalX * clipExtent + dirX * clipExtent * 2.0f,
                portalY - normalY * clipExtent + dirY * clipExtent * 2.0f)
        };
        Gdiplus::GraphicsPath revealPath;
        revealPath.AddPolygon(revealPoints, 4);
        DrawThemeMotionSprite(hdc, theme, train, 0.92f, 37.75f, nullptr, &revealPath);

        if (g_neonFlyingCarSprite) {
            for (int i = 0; i < 3; ++i) {
                float cycle = (float)std::fmod(
                    seconds * (0.060f + (float)i * 0.016f) + (float)i * 0.43f,
                    1.24f) - 0.12f;
                if (cycle < -0.12f) cycle += 1.24f;
                float centreXCar = (float)rect.left - 48.0f + cycle * ((float)width + 96.0f);
                float laneY = 0.52f + (float)i * 0.095f;
                float centreYCar = (float)rect.top + laneY * (float)height +
                    std::sin(seconds * 0.22f + (float)i * 2.1f) * 3.0f;
                float spriteWCar = 66.0f - (float)i * 9.0f;
                float spriteHCar = spriteWCar * (2.0f / 3.0f);
                Gdiplus::RectF flyingCar(
                    centreXCar - spriteWCar * 0.5f,
                    centreYCar - spriteHCar * 0.5f,
                    spriteWCar,
                    spriteHCar);
                DrawThemeMotionSprite(
                    hdc,
                    theme,
                    flyingCar,
                    0.84f - (float)i * 0.08f,
                    -1.5f + (float)i * 0.7f,
                    g_neonFlyingCarSprite);
            }
        }
    }
    else if (theme == GUI_THEME_ENCHANTED_FOREST) {
        for (int i = 0; i < 2; ++i) {
            float phase = seconds * (0.16f + (float)i * 0.025f) + (float)i * 3.4f;
            float baseX = i == 0 ? 0.20f : 0.78f;
            float baseY = i == 0 ? 0.63f : 0.79f;
            float centreX = (float)rect.left + (baseX + std::sin(phase) * 0.075f) * (float)width;
            float centreY = (float)rect.top + (baseY + std::cos(phase * 0.73f) * 0.045f) * (float)height;
            float spriteSize = i == 0 ? 72.0f : 54.0f;
            Gdiplus::RectF moth(
                centreX - spriteSize * 0.5f,
                centreY - spriteSize * 0.5f,
                spriteSize,
                spriteSize);
            DrawThemeMotionSprite(hdc, theme, moth, i == 0 ? 0.88f : 0.72f, std::sin(phase * 0.61f) * 5.0f);
        }
    }
    else if (theme == GUI_THEME_INFERNO) {
        static const float kFlameX[3] = { 0.16f, 0.53f, 0.85f };
        static const float kFlameY[3] = { 0.84f, 0.91f, 0.77f };
        static const float kFlameScale[3] = { 0.88f, 1.0f, 0.72f };
        for (int i = 0; i < 3; ++i) {
            float phase = seconds * (0.48f + (float)i * 0.06f) + (float)i * 2.2f;
            float breathe = 1.0f + std::sin(phase) * 0.045f;
            float spriteW = 64.0f * kFlameScale[i] * breathe;
            float spriteH = 102.0f * kFlameScale[i] * breathe;
            float baseX = (float)rect.left + kFlameX[i] * (float)width;
            float baseY = (float)rect.top + kFlameY[i] * (float)height;
            Gdiplus::RectF flame(
                baseX - spriteW * 0.5f,
                baseY - spriteH * 0.86f,
                spriteW,
                spriteH);
            DrawThemeMotionSprite(hdc, theme, flame, 0.78f + 0.10f * std::sin(phase + 0.8f), std::sin(phase * 0.67f) * 1.4f);
        }
    }
    else if (theme == GUI_THEME_ARCTIC_AURORA) {
        // Detailed flurry sprites form four different-depth squalls. Each pass
        // has its own width, route, speed and opacity so the storm alternates
        // naturally between thin wisps and thick foreground blizzards.
        static const float kPassRate[4] = { 0.030f, 0.043f, 0.052f, 0.036f };
        static const float kPassPhase[4] = { 0.02f, 0.57f, 0.28f, 0.78f };
        static const float kPassBaseY[4] = { 0.12f, 0.31f, 0.51f, 0.69f };
        static const float kPassWidth[4] = { 68.0f, 96.0f, 132.0f, 174.0f };
        static const float kPassOpacity[4] = { 0.38f, 0.50f, 0.62f, 0.72f };
        static const float kPassRotation[4] = { -23.0f, -21.0f, -19.0f, -17.0f };

        for (int i = 0; i < 4; ++i) {
            float cycle = (float)std::fmod(seconds * kPassRate[i] + kPassPhase[i], 1.0f);
            if (cycle < 0.0f) cycle += 1.0f;

            float breathe = 1.0f + std::sin(seconds * (0.16f + (float)i * 0.013f) +
                (float)i * 1.9f) * 0.035f;
            float spriteW = kPassWidth[i] * breathe;
            float spriteH = spriteW * 1.477f;
            float travelPadding = spriteW * 0.82f + 22.0f;
            float centreX = (float)rect.left - travelPadding +
                cycle * ((float)width + travelPadding * 2.0f);
            float centreY = (float)rect.top +
                (kPassBaseY[i] + cycle * (0.19f + (float)i * 0.015f)) * (float)height +
                std::sin(seconds * 0.10f + (float)i * 2.35f) * (4.0f + (float)i * 2.5f);

            float edgeFade = min(
                ClampFloat(cycle / 0.12f, 0.0f, 1.0f),
                ClampFloat((1.0f - cycle) / 0.12f, 0.0f, 1.0f));
            Gdiplus::RectF snow(
                centreX - spriteW * 0.5f,
                centreY - spriteH * 0.5f,
                spriteW,
                spriteH);
            DrawThemeMotionSprite(
                hdc,
                theme,
                snow,
                kPassOpacity[i] * edgeFade,
                kPassRotation[i]);
        }
    }
}

void DrawSelectedThemeAnimation(HDC hdc, const RECT& rect, float seconds) {
    switch (NormalizeGuiTheme(g_guiTheme)) {
    case GUI_THEME_COSMIC:
        DrawCosmicPlanets(hdc, rect, seconds);
        DrawPixelShootingStars(hdc, rect, seconds);
        DrawCosmicStars(hdc, rect, 76);
        DrawCosmicOrbitalDust(hdc, rect, seconds);
        break;
    case GUI_THEME_NEON_CITY:
    case GUI_THEME_ENCHANTED_FOREST:
        DrawArtworkMatchedThemeAnimation(hdc, rect, seconds);
        DrawSceneMatchedSpriteAnimation(hdc, rect, seconds);
        break;
    case GUI_THEME_INFERNO:
        DrawArtworkMatchedThemeAnimation(hdc, rect, seconds);
        DrawInfernoAnimation(hdc, rect, seconds);
        DrawSceneMatchedSpriteAnimation(hdc, rect, seconds);
        break;
    case GUI_THEME_ARCTIC_AURORA:
        DrawArtworkMatchedThemeAnimation(hdc, rect, seconds);
        DrawArcticAuroraAnimation(hdc, rect, seconds);
        DrawSceneMatchedSpriteAnimation(hdc, rect, seconds);
        break;
    default: break;
    }
}

void DrawGuiBackground(HDC hdc, const RECT& rect) {
    if (IsClassicGuiTheme()) {
        HBRUSH bg = CreateSolidBrush(kGuiBg);
        FillRect(hdc, &rect, bg);
        DeleteObject(bg);
        return;
    }

    float seconds = GetGuiCosmicAnimationSeconds();
    if (DrawCosmicImageBackground(hdc, rect, seconds)) {
        DrawSelectedThemeAnimation(hdc, rect, seconds);
        return;
    }

    const GuiPalette& palette = GetGuiPalette();
    FillVerticalGradient(hdc, rect, palette.bg, palette.bgBottom, 180);
    if (IsCosmicGuiTheme()) {
        DrawPixelNebula(hdc, rect, seconds);
        DrawPixelGalaxyCore(hdc, rect, seconds);
    }
    DrawSelectedThemeAnimation(hdc, rect, seconds);
}

bool PointInRectEx(const RECT& rect, int x, int y) {
    return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
}

int GetMainPageMaxScroll() {
    int maxScroll = g_guiMainContentHeight - g_guiMainViewportHeight;
    return maxScroll > 0 ? maxScroll : 0;
}

void ClampMainPageScroll(int clientH) {
    g_guiMainViewportHeight = clientH - 76 - 20;
    if (g_guiMainViewportHeight < 0) g_guiMainViewportHeight = 0;

    int maxScroll = GetMainPageMaxScroll();
    if (g_guiMainScroll < 0) g_guiMainScroll = 0;
    if (g_guiMainScroll > maxScroll) g_guiMainScroll = maxScroll;
}

int GetPerspectiveExpandedHeight() {
    return kGuiPerspectiveExpandedHeight;
}

void EnsureGuiCardAnimationInitialized(GuiCardAnimState& state, int expandedHeight) {
    if (expandedHeight < kGuiCardHeaderHeight) expandedHeight = kGuiCardHeaderHeight;
    if (state.currentHeight <= 0.0f) state.currentHeight = (float)kGuiCardHeaderHeight;
    if (state.currentHeight < (float)kGuiCardHeaderHeight) state.currentHeight = (float)kGuiCardHeaderHeight;
    if (state.currentHeight > (float)expandedHeight) state.currentHeight = (float)expandedHeight;
}

int GetAnimatedCardHeight(const GuiCardAnimState& state, int expandedHeight) {
    float currentHeight = state.currentHeight <= 0.0f ? (float)kGuiCardHeaderHeight : state.currentHeight;
    if (currentHeight < (float)kGuiCardHeaderHeight) currentHeight = (float)kGuiCardHeaderHeight;
    if (currentHeight > (float)expandedHeight) currentHeight = (float)expandedHeight;
    return (int)(currentHeight + 0.5f);
}

bool UpdateGuiCardAnimation(GuiCardAnimState& state, int expandedHeight, float deltaSeconds) {
    EnsureGuiCardAnimationInitialized(state, expandedHeight);

    float targetHeight = state.expanded ? (float)expandedHeight : (float)kGuiCardHeaderHeight;
    float delta = targetHeight - state.currentHeight;
    if (fabsf(delta) <= 0.5f) {
        if (state.currentHeight == targetHeight) return false;
        state.currentHeight = targetHeight;
        return true;
    }

    float smoothing = ClampFloat(deltaSeconds * 14.0f, 0.0f, 1.0f);
    state.currentHeight += delta * smoothing;
    return true;
}

bool UpdateGuiAnimations() {
    ULONGLONG nowMs = GetTickCount64();
    if (g_guiLastAnimationTickMs == 0) {
        g_guiLastAnimationTickMs = nowMs;
        return false;
    }

    float deltaSeconds = ClampFloat((float)(nowMs - g_guiLastAnimationTickMs) / 1000.0f, 0.0f, 0.10f);
    g_guiLastAnimationTickMs = nowMs;

    bool dirty = false;
    dirty |= UpdateGuiCardAnimation(g_perspectiveCardAnim, GetPerspectiveExpandedHeight(), deltaSeconds);
    dirty |= UpdateGuiCardAnimation(g_timerCardAnim, kGuiTimerExpandedHeight, deltaSeconds);
    dirty |= UpdateGuiCardAnimation(g_speedSlownessCardAnim, kGuiSpeedExpandedHeight, deltaSeconds);
    dirty |= UpdateGuiCardAnimation(g_publicHelpersCardAnim, kGuiPublicHelpersExpandedHeight, deltaSeconds);
    dirty |= UpdateGuiCardAnimation(g_extrasCardAnim, kGuiExtrasExpandedHeight, deltaSeconds);
    dirty |= UpdateGuiCardAnimation(g_mutedUtilitiesCardAnim, kGuiMutedUtilitiesExpandedHeight, deltaSeconds);
    return dirty;
}

void LayoutGuiControls(int clientW, int clientH) {
    const int margin = 20;
    int cardW = clientW - (margin * 2);
    if (cardW < 0) cardW = 0;
    const int bindW = 142;
    const int segW = 84;
    const int soundButtonW = 172;
    const int segGap = 10;
    const int timerModeW = 98;
    const int timerDpW = 72;
    const int sliderValueReserveW = 84;
    const int toggleW = 46;
    const int toggleH = 24;
    const int expandW = 28;
    const int expandH = 28;
    const int headerRightPad = 18;
    const int headerControlGap = 10;
    const int toggleTop = 16;
    const int expandTop = 14;
    const int legacyBodyShift = kGuiCardHeaderHeight - 48;

    g_headerCloseRect = MakeRectWH(clientW - 18 - kGuiWindowButtonW, 16, kGuiWindowButtonW, kGuiWindowButtonH);
    g_headerMinimizeRect = MakeRectWH(g_headerCloseRect.left - 8 - kGuiWindowButtonW, 16, kGuiWindowButtonW, kGuiWindowButtonH);
    g_headerThemeRect = MakeRectWH(g_headerMinimizeRect.left - 10 - 88, 16, 88, kGuiWindowButtonH);

    EnsureGuiCardAnimationInitialized(g_perspectiveCardAnim, GetPerspectiveExpandedHeight());
    EnsureGuiCardAnimationInitialized(g_timerCardAnim, kGuiTimerExpandedHeight);
    EnsureGuiCardAnimationInitialized(g_speedSlownessCardAnim, kGuiSpeedExpandedHeight);
    EnsureGuiCardAnimationInitialized(g_publicHelpersCardAnim, kGuiPublicHelpersExpandedHeight);
    EnsureGuiCardAnimationInitialized(g_extrasCardAnim, kGuiExtrasExpandedHeight);
    EnsureGuiCardAnimationInitialized(g_mutedUtilitiesCardAnim, kGuiMutedUtilitiesExpandedHeight);

    int y = 76 - g_guiMainScroll;

    int perspectiveHeight = GetAnimatedCardHeight(g_perspectiveCardAnim, GetPerspectiveExpandedHeight());
    g_perspectiveCard = MakeRectWH(margin, y, cardW, perspectiveHeight);
    g_perspectiveExpandRect = MakeRectWH(g_perspectiveCard.right - headerRightPad - expandW, g_perspectiveCard.top + expandTop, expandW, expandH);
    g_perspectiveToggleRect = MakeRectWH(g_perspectiveExpandRect.left - headerControlGap - toggleW, g_perspectiveCard.top + toggleTop, toggleW, toggleH);
    {
        int bodyTop = g_perspectiveCard.top + kGuiCardHeaderHeight;
        g_perspectiveBindRect = MakeRectWH(g_perspectiveCard.left + 18, bodyTop + 30, bindW, 30);

        int camX = g_perspectiveCard.right - 18 - (segW * 2) - segGap;
        g_perspectiveBackRect = MakeRectWH(camX, bodyTop + 30, segW, 30);
        g_perspectiveFrontRect = MakeRectWH(camX + segW + segGap, bodyTop + 30, segW, 30);
    }

    y += perspectiveHeight + kGuiCardGap;

    int timerHeight = GetAnimatedCardHeight(g_timerCardAnim, kGuiTimerExpandedHeight);
    g_timerCard = MakeRectWH(margin, y, cardW, timerHeight);
    g_timerExpandRect = MakeRectWH(g_timerCard.right - headerRightPad - expandW, g_timerCard.top + expandTop, expandW, expandH);
    g_timerShowToggleRect = MakeRectWH(g_timerExpandRect.left - headerControlGap - toggleW, g_timerCard.top + toggleTop, toggleW, toggleH);
    g_timerModeOverlayRect = MakeRectWH(g_timerCard.right - 18 - (timerModeW * 2) - segGap, g_timerCard.top + 52 + legacyBodyShift, timerModeW, 30);
    g_timerModeCrosshairRect = MakeRectWH(g_timerModeOverlayRect.right + segGap, g_timerCard.top + 52 + legacyBodyShift, timerModeW, 30);
    g_timerDecimal0Rect = MakeRectWH(g_timerCard.right - 18 - (timerDpW * 3) - (segGap * 2), g_timerCard.top + 94 + legacyBodyShift, timerDpW, 30);
    g_timerDecimal1Rect = MakeRectWH(g_timerDecimal0Rect.right + segGap, g_timerCard.top + 94 + legacyBodyShift, timerDpW, 30);
    g_timerDecimal2Rect = MakeRectWH(g_timerDecimal1Rect.right + segGap, g_timerCard.top + 94 + legacyBodyShift, timerDpW, 30);
    g_timerNumberColourButtonRect = MakeRectWH(g_timerCard.left + 18, g_timerCard.top + 136 + legacyBodyShift, cardW - 36, 30);
    g_timerNametagToggleRect = MakeRectWH(g_timerCard.right - 64, g_timerCard.top + 172 + legacyBodyShift, 46, 24);
    g_timerNametagSuffixRect = MakeRectWH(g_timerNametagToggleRect.left - 10 - 64, g_timerNametagToggleRect.top - 3, 64, 30);
    g_timerNametagPrefixRect = MakeRectWH(g_timerNametagSuffixRect.left - 6 - 64, g_timerNametagToggleRect.top - 3, 64, 30);
    g_timerDefaultScoreboardToggleRect = MakeRectWH(g_timerCard.right - 64, g_timerCard.top + 206 + legacyBodyShift, 46, 24);
    g_timerCaptureToggleRect = MakeRectWH(g_timerCard.right - 64, g_timerCard.top + 240 + legacyBodyShift, 46, 24);
    g_timerLockToggleRect = MakeRectWH(g_timerCard.right - 64, g_timerCard.top + 274 + legacyBodyShift, 46, 24);
    g_timerScaleTrackRect = MakeRectWH(g_timerCard.left + 18, g_timerCard.top + 324 + legacyBodyShift, cardW - 36, 8);
    g_timerScaleHitRect = MakeRectWH(g_timerCard.left + 18, g_timerCard.top + 312 + legacyBodyShift, cardW - 36, 28);

    y += timerHeight + kGuiCardGap;

    int speedHeight = GetAnimatedCardHeight(g_speedSlownessCardAnim, kGuiSpeedExpandedHeight);
    g_speedSlownessCard = MakeRectWH(margin, y, cardW, speedHeight);
    g_speedSlownessExpandRect = MakeRectWH(g_speedSlownessCard.right - headerRightPad - expandW, g_speedSlownessCard.top + expandTop, expandW, expandH);
    g_speedSlownessToggleRect = MakeRectWH(g_speedSlownessExpandRect.left - headerControlGap - toggleW, g_speedSlownessCard.top + toggleTop, toggleW, toggleH);
    {
        const int soundValueGap = 12;
        int soundRight = g_speedSlownessCard.right - 18 - sliderValueReserveW - soundValueGap;
        int soundX = soundRight - soundButtonW;
        g_speed3SoundButtonRect = MakeRectWH(soundX, g_speedSlownessCard.top + 48 + legacyBodyShift, soundButtonW, 30);
        g_slownessSoundButtonRect = MakeRectWH(soundX, g_speedSlownessCard.top + 120 + legacyBodyShift, soundButtonW, 30);
    }
    g_speed3VolumeTrackRect = MakeRectWH(g_speedSlownessCard.left + 18, g_speedSlownessCard.top + 94 + legacyBodyShift, cardW - 36, 8);
    g_speed3VolumeHitRect = MakeRectWH(g_speedSlownessCard.left + 18, g_speedSlownessCard.top + 82 + legacyBodyShift, cardW - 36, 28);
    g_slownessVolumeTrackRect = MakeRectWH(g_speedSlownessCard.left + 18, g_speedSlownessCard.top + 166 + legacyBodyShift, cardW - 36, 8);
    g_slownessVolumeHitRect = MakeRectWH(g_speedSlownessCard.left + 18, g_speedSlownessCard.top + 154 + legacyBodyShift, cardW - 36, 28);

    y += speedHeight + kGuiCardGap;

    int publicHelpersHeight = GetAnimatedCardHeight(g_publicHelpersCardAnim, kGuiPublicHelpersExpandedHeight);
    g_publicHelpersCard = MakeRectWH(margin, y, cardW, publicHelpersHeight);
    g_publicHelpersExpandRect = MakeRectWH(g_publicHelpersCard.right - headerRightPad - expandW, g_publicHelpersCard.top + expandTop, expandW, expandH);
    g_publicWinsToggleRect = MakeRectWH(g_publicHelpersCard.right - 64, g_publicHelpersCard.top + 68, 46, 24);
    {
        const int positionW = 92;
        int positionX = g_publicHelpersCard.right - 18 - (positionW * 2) - segGap;
        g_publicWinsPrefixRect = MakeRectWH(positionX, g_publicHelpersCard.top + 102, positionW, 30);
        g_publicWinsSuffixRect = MakeRectWH(g_publicWinsPrefixRect.right + segGap, g_publicHelpersCard.top + 102, positionW, 30);
    }
    g_publicWinsSpaceRect = MakeRectWH(g_publicHelpersCard.right - 40, g_publicHelpersCard.top + 145, 22, 22);

    y += publicHelpersHeight + kGuiCardGap;

    int mutedUtilitiesHeight = GetAnimatedCardHeight(g_mutedUtilitiesCardAnim, kGuiMutedUtilitiesExpandedHeight);
    g_mutedUtilitiesCard = MakeRectWH(margin, y, cardW, mutedUtilitiesHeight);
    g_mutedUtilitiesExpandRect = MakeRectWH(g_mutedUtilitiesCard.right - headerRightPad - expandW, g_mutedUtilitiesCard.top + expandTop, expandW, expandH);
    g_mutedVoiceToggleRect = MakeRectWH(g_mutedUtilitiesCard.right - 64, g_mutedUtilitiesCard.top + 68, 46, 24);
    g_mutedVoiceHideMuteReminderToggleRect = MakeRectWH(g_mutedUtilitiesCard.right - 64, g_mutedUtilitiesCard.top + 100, 46, 24);
    g_mutedVoicePartyOwnerFieldRect = MakeRectWH(g_mutedUtilitiesCard.left + 18, g_mutedUtilitiesCard.top + 150, cardW - 36, 32);
    {
        const int copyButtonW = 78;
        int authCodeW = cardW - 36 - copyButtonW - segGap;
        if (authCodeW < 0) authCodeW = 0;
        g_mutedVoiceCopyCodeButtonRect = MakeRectWH(g_mutedUtilitiesCard.right - 18 - copyButtonW, g_mutedUtilitiesCard.top + 238, copyButtonW, 34);
        g_mutedVoiceAuthCodeRect = MakeRectWH(g_mutedUtilitiesCard.left + 18, g_mutedUtilitiesCard.top + 238, authCodeW, 34);
    }
    {
        int buttonW = (cardW - 36 - segGap) / 2;
        if (buttonW < 0) buttonW = 0;
        g_mutedVoiceOpenAuthButtonRect = MakeRectWH(g_mutedUtilitiesCard.left + 18, g_mutedUtilitiesCard.top + 332, buttonW, 30);
        g_mutedVoiceSignOutButtonRect = MakeRectWH(g_mutedVoiceOpenAuthButtonRect.right + segGap, g_mutedUtilitiesCard.top + 332, buttonW, 30);
    }

    y += mutedUtilitiesHeight + kGuiCardGap;

    int extrasHeight = GetAnimatedCardHeight(g_extrasCardAnim, kGuiExtrasExpandedHeight);
    g_extrasCard = MakeRectWH(margin, y, cardW, extrasHeight);
    g_extrasExpandRect = MakeRectWH(g_extrasCard.right - headerRightPad - expandW, g_extrasCard.top + expandTop, expandW, expandH);
    g_extrasWheatToggleRect = MakeRectWH(g_extrasCard.right - 64, g_extrasCard.top + 68, 46, 24);
    g_extrasBeaconToggleRect = MakeRectWH(g_extrasCard.right - 64, g_extrasCard.top + 100, 46, 24);
    g_extrasScoreboardToggleRect = MakeRectWH(g_extrasCard.right - 64, g_extrasCard.top + 132, 46, 24);

    y += extrasHeight;

    // Keep the social links at the lower-right edge while the cards fit in the
    // viewport. Once expanded cards run past that edge, the links follow them
    // as the final scrollable content instead of overlapping a card.
    int footerContentTop = max(
        y + g_guiMainScroll + kGuiFooterTopGap,
        clientH - 20 - kGuiFooterIconSize);
    int footerTop = footerContentTop - g_guiMainScroll;
    int footerRight = clientW - margin;
    g_footerDiscordRect = MakeRectWH(
        footerRight - kGuiFooterIconSize,
        footerTop,
        kGuiFooterIconSize,
        kGuiFooterIconSize);
    g_footerYouTubeRect = MakeRectWH(
        g_footerDiscordRect.left - kGuiFooterIconGap - kGuiFooterIconSize,
        footerTop,
        kGuiFooterIconSize,
        kGuiFooterIconSize);
    g_guiMainContentHeight = footerContentTop + kGuiFooterIconSize - 76;

    int oldMainScroll = g_guiMainScroll;
    ClampMainPageScroll(clientH);
    if (g_guiMainScroll != oldMainScroll) {
        LayoutGuiControls(clientW, clientH);
        return;
    }

    int pickerHeight = clientH - 96;
    if (pickerHeight < 0) pickerHeight = 0;
    g_soundPickerCard = MakeRectWH(margin, 76, cardW, pickerHeight);
    g_soundPickerBackRect = MakeRectWH(g_soundPickerCard.left + 18, g_soundPickerCard.top + 18, 34, 30);
    g_soundPickerSearchRect = MakeRectWH(g_soundPickerCard.left + 18, g_soundPickerCard.top + 72,
        (g_soundPickerCard.right - g_soundPickerCard.left) - 36, 34);
    g_soundPickerListRect = MakeRectWH(g_soundPickerCard.left + 18, g_soundPickerCard.top + 116,
        (g_soundPickerCard.right - g_soundPickerCard.left) - 36,
        (g_soundPickerCard.bottom - g_soundPickerCard.top) - 134);
    ClampSoundPickerScroll();

    g_numberColourPickerCard = MakeRectWH(margin, 76, cardW, pickerHeight);
    g_numberColourPickerBackRect = MakeRectWH(g_numberColourPickerCard.left + 18, g_numberColourPickerCard.top + 18, 34, 30);
    const int numberGridLeft = g_numberColourPickerCard.left + 18;
    const int numberGridTop = g_numberColourPickerCard.top + 104;
    const int numberGridGap = 6;
    const int numberGridColumns = 8;
    const int numberGridRows = 7;
    const int numberGridWidth = (g_numberColourPickerCard.right - g_numberColourPickerCard.left) - 36;
    const int numberGridHeight = 276;
    const int colourModeGap = 8;
    const int colourModeWidth = (numberGridWidth - colourModeGap) / 2;
    g_numberColourRgbModeRect = MakeRectWH(numberGridLeft, g_numberColourPickerCard.top + 70, colourModeWidth, 26);
    g_numberColourMinecraftModeRect = MakeRectWH(
        g_numberColourRgbModeRect.right + colourModeGap,
        g_numberColourRgbModeRect.top,
        colourModeWidth,
        26);
    const int numberCellWidth = (numberGridWidth - (numberGridColumns - 1) * numberGridGap) / numberGridColumns;
    const int numberCellHeight = (numberGridHeight - (numberGridRows - 1) * numberGridGap) / numberGridRows;
    g_numberColourGridRect = MakeRectWH(numberGridLeft, numberGridTop, numberGridWidth, numberGridHeight);
    for (int number = kTimerNumberMax; number >= kTimerNumberMin; --number) {
        int displayIndex = kTimerNumberMax - number;
        int column = displayIndex % numberGridColumns;
        int row = displayIndex / numberGridColumns;
        g_numberColourNumberRects[number] = MakeRectWH(
            numberGridLeft + column * (numberCellWidth + numberGridGap),
            numberGridTop + row * (numberCellHeight + numberGridGap),
            numberCellWidth,
            numberCellHeight);
    }
    g_numberColourPreviewRect = MakeRectWH(numberGridLeft, g_numberColourGridRect.bottom + 16, numberGridWidth, 42);
    g_numberColourRedTrackRect = MakeRectWH(numberGridLeft, g_numberColourPreviewRect.bottom + 38, numberGridWidth, 10);
    g_numberColourRedHitRect = MakeRectWH(numberGridLeft, g_numberColourRedTrackRect.top - 13, numberGridWidth, 34);
    g_numberColourGreenTrackRect = MakeRectWH(numberGridLeft, g_numberColourRedTrackRect.top + 50, numberGridWidth, 10);
    g_numberColourGreenHitRect = MakeRectWH(numberGridLeft, g_numberColourGreenTrackRect.top - 13, numberGridWidth, 34);
    g_numberColourBlueTrackRect = MakeRectWH(numberGridLeft, g_numberColourGreenTrackRect.top + 50, numberGridWidth, 10);
    g_numberColourBlueHitRect = MakeRectWH(numberGridLeft, g_numberColourBlueTrackRect.top - 13, numberGridWidth, 34);
    const int minecraftPaletteColumns = 4;
    const int minecraftPaletteGap = 6;
    const int minecraftOptionWidth = (numberGridWidth - (minecraftPaletteColumns - 1) * minecraftPaletteGap) / minecraftPaletteColumns;
    const int minecraftOptionHeight = 32;
    const int minecraftPaletteTop = g_numberColourPreviewRect.bottom + 28;
    for (int option = 0; option < (int)kMinecraftColourOptions.size(); ++option) {
        int column = option % minecraftPaletteColumns;
        int row = option / minecraftPaletteColumns;
        g_minecraftColourOptionRects[option] = MakeRectWH(
            numberGridLeft + column * (minecraftOptionWidth + minecraftPaletteGap),
            minecraftPaletteTop + row * (minecraftOptionHeight + minecraftPaletteGap),
            minecraftOptionWidth,
            minecraftOptionHeight);
    }

    g_themePickerCard = MakeRectWH(margin, 76, cardW, pickerHeight);
    g_themePickerBackRect = MakeRectWH(g_themePickerCard.left + 18, g_themePickerCard.top + 18, 34, 30);
    const int themeGridLeft = g_themePickerCard.left + 18;
    const int themeGridTop = g_themePickerCard.top + 78;
    const int themeGridGap = 12;
    int themeGridWidth = (g_themePickerCard.right - g_themePickerCard.left) - 36;
    int themeOptionWidth = (themeGridWidth - themeGridGap) / 2;
    int themeGridHeight = g_themePickerCard.bottom - themeGridTop - 18;
    int themeOptionHeight = (themeGridHeight - themeGridGap * 2) / 3;
    if (themeOptionWidth < 0) themeOptionWidth = 0;
    if (themeOptionHeight < 0) themeOptionHeight = 0;
    for (int theme = 0; theme < GUI_THEME_COUNT; ++theme) {
        int column = theme % 2;
        int row = theme / 2;
        g_themeOptionRects[theme] = MakeRectWH(
            themeGridLeft + column * (themeOptionWidth + themeGridGap),
            themeGridTop + row * (themeOptionHeight + themeGridGap),
            themeOptionWidth,
            themeOptionHeight);
    }
}

bool IsExtendedVirtualKey(UINT vk) {
    switch (vk) {
    case VK_RMENU:
    case VK_RCONTROL:
    case VK_INSERT:
    case VK_DELETE:
    case VK_HOME:
    case VK_END:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_LEFT:
    case VK_RIGHT:
    case VK_UP:
    case VK_DOWN:
    case VK_NUMLOCK:
    case VK_DIVIDE:
    case VK_SNAPSHOT:
        return true;
    default:
        return false;
    }
}

std::string GetKeybindLabel(int keybind) {
    if (keybind == 0) return "None";

    switch (keybind) {
    case VK_MENU: return "Alt";
    case VK_CONTROL: return "Ctrl";
    case VK_SHIFT: return "Shift";
    case VK_SPACE: return "Space";
    case VK_RETURN: return "Enter";
    case VK_TAB: return "Tab";
    case VK_CAPITAL: return "Caps Lock";
    case VK_BACK: return "Backspace";
    case VK_ESCAPE: return "Escape";
    case VK_LMENU: return "L Alt";
    case VK_RMENU: return "R Alt";
    case VK_LCONTROL: return "L Ctrl";
    case VK_RCONTROL: return "R Ctrl";
    case VK_LSHIFT: return "L Shift";
    case VK_RSHIFT: return "R Shift";
    default:
        break;
    }

    UINT scanCode = MapVirtualKeyA((UINT)keybind, MAPVK_VK_TO_VSC);
    LONG keyData = (LONG)(scanCode << 16);
    if (IsExtendedVirtualKey((UINT)keybind)) keyData |= 1 << 24;

    char name[64] = {};
    if (GetKeyNameTextA(keyData, name, (int)sizeof(name)) > 0) return name;

    if ((keybind >= 'A' && keybind <= 'Z') || (keybind >= '0' && keybind <= '9')) {
        return std::string(1, (char)keybind);
    }

    char fallback[24];
    sprintf(fallback, "VK %d", keybind);
    return fallback;
}

std::string GetCaptureLabel(GuiBindCaptureTarget target, int keybind) {
    if (g_guiBindCapture == target) return "Press a key...";
    return GetKeybindLabel(keybind);
}

std::string FormatTimerScaleLabel(float value) {
    char buf[32];
    sprintf(buf, "%.2fx", value);
    return buf;
}

std::string FormatAlertVolumeLabel(float value) {
    char buf[32];
    sprintf(buf, "%.0f%%", ClampFloat(value, kAlertVolumeMin, kAlertVolumeMax) * 100.0f);
    return buf;
}

const char* GetAlertSoundName(int soundId) {
    return kAlertSoundOptions[NormalizeAlertSoundId(soundId, ALERT_SOUND_RANDOM_LEVELUP)].name;
}

const char* GetAlertSoundLabel(int soundId) {
    return kAlertSoundOptions[NormalizeAlertSoundId(soundId, ALERT_SOUND_RANDOM_LEVELUP)].label;
}

std::string FormatAlertSoundButtonLabel(int soundId) {
    return std::string(GetAlertSoundLabel(soundId)) + " >";
}

const char* GetSoundPickerFieldLabel(GuiSoundPickerField field) {
    if (field == GUI_SOUND_FIELD_SPEED3) return "Speed 3 sound";
    if (field == GUI_SOUND_FIELD_SLOWNESS) return "Slowness sound";
    return "";
}

int GetEditedSoundValue() {
    if (g_guiSoundPickerField == GUI_SOUND_FIELD_SPEED3) return g_speed3Sound;
    if (g_guiSoundPickerField == GUI_SOUND_FIELD_SLOWNESS) return g_slownessSound;
    return ALERT_SOUND_RANDOM_LEVELUP;
}

void SetEditedSoundValue(int soundId) {
    int normalized = NormalizeAlertSoundId(soundId, ALERT_SOUND_RANDOM_LEVELUP);
    if (g_guiSoundPickerField == GUI_SOUND_FIELD_SPEED3) g_speed3Sound = normalized;
    if (g_guiSoundPickerField == GUI_SOUND_FIELD_SLOWNESS) g_slownessSound = normalized;
}

void CancelGuiInteractions() {
    g_guiBindCapture = GUI_BIND_NONE;
    g_guiSliderTarget = GUI_SLIDER_NONE;
    g_guiSliderDirty = false;
    g_guiNumberColourSelectionDragging = false;
    g_guiMutedVoicePartyOwnerEditing = false;
    if (GetCapture()) ReleaseCapture();
}

std::vector<int> GetFilteredSoundIds() {
    std::vector<int> result;
    std::string query = ToLowerAscii(g_guiSoundSearch);
    result.reserve(ALERT_SOUND_COUNT);
    for (int soundId = 0; soundId < ALERT_SOUND_COUNT; ++soundId) {
        const AlertSoundOption& option = kAlertSoundOptions[soundId];
        if (query.empty() ||
            ToLowerAscii(option.label).find(query) != std::string::npos ||
            ToLowerAscii(option.name).find(query) != std::string::npos) {
            result.push_back(soundId);
        }
    }
    return result;
}

int GetSoundPickerContentHeight() {
    int resultCount = (int)GetFilteredSoundIds().size();
    return 24 + (resultCount * (kGuiSoundPickerRowHeight + kGuiSoundPickerRowGap));
}

int GetSoundPickerMaxScroll() {
    int listHeight = g_soundPickerListRect.bottom - g_soundPickerListRect.top;
    int maxScroll = GetSoundPickerContentHeight() - listHeight;
    return maxScroll > 0 ? maxScroll : 0;
}

void ClampSoundPickerScroll() {
    int maxScroll = GetSoundPickerMaxScroll();
    if (g_guiSoundPickerScroll < 0) g_guiSoundPickerScroll = 0;
    if (g_guiSoundPickerScroll > maxScroll) g_guiSoundPickerScroll = maxScroll;
}

bool GetSoundPickerRowRect(int rowIndex, int rowCount, RECT& rect) {
    if (rowIndex < 0 || rowIndex >= rowCount) return false;

    int listWidth = g_soundPickerListRect.right - g_soundPickerListRect.left;
    int rowWidth = listWidth - 24;
    if (rowWidth < 0) rowWidth = 0;

    int rowTop = g_soundPickerListRect.top + 12 - g_guiSoundPickerScroll +
        (rowIndex * (kGuiSoundPickerRowHeight + kGuiSoundPickerRowGap));
    rect = MakeRectWH(g_soundPickerListRect.left + 12, rowTop, rowWidth, kGuiSoundPickerRowHeight);
    return true;
}

void OpenSoundPicker(GuiSoundPickerField field) {
    g_guiCurrentPage = GUI_PAGE_SOUND_PICKER;
    g_guiSoundPickerField = field;
    g_guiSoundSearch.clear();
    g_guiSoundSearchEditing = true;
    g_guiSoundPickerScroll = 0;

    std::vector<int> filteredSounds = GetFilteredSoundIds();
    int selectedRow = 0;
    int selectedSound = GetEditedSoundValue();
    for (int row = 0; row < (int)filteredSounds.size(); ++row) {
        if (filteredSounds[row] == selectedSound) {
            selectedRow = row;
            break;
        }
    }
    int listHeight = g_soundPickerListRect.bottom - g_soundPickerListRect.top;
    int rowSpan = kGuiSoundPickerRowHeight + kGuiSoundPickerRowGap;
    g_guiSoundPickerScroll = (selectedRow * rowSpan) - ((listHeight - kGuiSoundPickerRowHeight) / 2);
    ClampSoundPickerScroll();
}

void CloseSoundPicker() {
    g_guiCurrentPage = GUI_PAGE_MAIN;
    g_guiSoundPickerField = GUI_SOUND_FIELD_NONE;
    g_guiSoundSearch.clear();
    g_guiSoundSearchEditing = false;
    g_guiSoundPickerScroll = 0;
}

void OpenThemePicker() {
    CancelGuiInteractions();
    g_guiCurrentPage = GUI_PAGE_THEME_PICKER;
}

void CloseThemePicker() {
    g_guiCurrentPage = GUI_PAGE_MAIN;
}

void LoadGuiNumberColour(int number) {
    std::uint32_t colour = GetTimerNumberColour(number);
    g_guiNumberColourRed = GetTimerColourRed(colour);
    g_guiNumberColourGreen = GetTimerColourGreen(colour);
    g_guiNumberColourBlue = GetTimerColourBlue(colour);
}

void SetGuiNumberColourSelectionRange(int firstNumber, int lastNumber) {
    firstNumber = ClampTimerNumber(firstNumber);
    lastNumber = ClampTimerNumber(lastNumber);
    int low = min(firstNumber, lastNumber);
    int high = max(firstNumber, lastNumber);
    g_guiNumberColourSelection.fill(false);
    for (int number = low; number <= high; ++number) {
        g_guiNumberColourSelection[number] = true;
    }
    g_guiNumberColourSelectionAnchor = firstNumber;
    g_guiNumberColourSelectionEnd = lastNumber;
}

void ApplyGuiNumberColourToSelection() {
    for (int number = kTimerNumberMin; number <= kTimerNumberMax; ++number) {
        if (g_guiNumberColourSelection[number]) {
            SetTimerNumberColour(number, g_guiNumberColourRed, g_guiNumberColourGreen, g_guiNumberColourBlue);
        }
    }
}

int GetNumberColourAtPoint(int x, int y) {
    for (int number = kTimerNumberMax; number >= kTimerNumberMin; --number) {
        if (PointInRectEx(g_numberColourNumberRects[number], x, y)) return number;
    }
    return -1;
}

void OpenNumberColourPicker() {
    CancelGuiInteractions();
    g_guiCurrentPage = GUI_PAGE_NUMBER_COLOUR_PICKER;
    int initialNumber = kTimerNumberMax;
    if (g_timerActive) initialNumber = GetDisplayedTimerNumber(GetDecimalSeconds());
    SetGuiNumberColourSelectionRange(initialNumber, initialNumber);
    LoadGuiNumberColour(initialNumber);
}

void CloseNumberColourPicker() {
    CancelGuiInteractions();
    g_guiCurrentPage = GUI_PAGE_MAIN;
}

std::string FormatNumberColourSelectionLabel() {
    int high = -1;
    int low = -1;
    int count = 0;
    for (int number = kTimerNumberMax; number >= kTimerNumberMin; --number) {
        if (!g_guiNumberColourSelection[number]) continue;
        if (high < 0) high = number;
        low = number;
        ++count;
    }

    char buf[96] = {};
    if (count <= 0) {
        sprintf_s(buf, "No numbers selected");
    }
    else if (high == low) {
        sprintf_s(buf, "%d selected  -  RGB %d, %d, %d", high,
            g_guiNumberColourRed, g_guiNumberColourGreen, g_guiNumberColourBlue);
    }
    else {
        sprintf_s(buf, "%d-%d selected  -  RGB %d, %d, %d", high, low,
            g_guiNumberColourRed, g_guiNumberColourGreen, g_guiNumberColourBlue);
    }
    return buf;
}

void ScrollSoundPicker(int wheelDelta) {
    int steps = wheelDelta / WHEEL_DELTA;
    if (steps == 0) return;
    g_guiSoundPickerScroll -= steps * (kGuiSoundPickerRowHeight + kGuiSoundPickerRowGap);
    ClampSoundPickerScroll();
}

void ScrollMainPage(int wheelDelta) {
    int steps = wheelDelta / WHEEL_DELTA;
    if (steps == 0) return;
    g_guiMainScroll -= steps * 64;

    int maxScroll = GetMainPageMaxScroll();
    if (g_guiMainScroll < 0) g_guiMainScroll = 0;
    if (g_guiMainScroll > maxScroll) g_guiMainScroll = maxScroll;
}

void DrawSoundPickerRow(HDC hdc, const RECT& rect, int index, HFONT bodyFont, HFONT metaFont) {
    bool active = index == NormalizeAlertSoundId(GetEditedSoundValue(), ALERT_SOUND_RANDOM_LEVELUP);
    COLORREF fill = active ? kGuiAccentSoft : kGuiButton;
    COLORREF border = active ? GetGuiPalette().activeBorder : kGuiButtonBorder;
    COLORREF titleColor = active ? kGuiText : kGuiButtonIdleText;
    COLORREF metaColor = active ? GetGuiPalette().activeMeta : kGuiMuted;
    const AlertSoundOption& option = kAlertSoundOptions[index];

    FillRoundedRect(hdc, rect, fill, border, 12);

    RECT titleRect = MakeRectWH(rect.left + 14, rect.top + 7, (rect.right - rect.left) - 28, 18);
    RECT metaRect = MakeRectWH(rect.left + 14, rect.top + 25, (rect.right - rect.left) - 28, 16);

    SelectObject(hdc, bodyFont);
    DrawTextLine(hdc, titleRect, option.label, titleColor, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    SelectObject(hdc, metaFont);
    DrawTextLine(hdc, metaRect, option.name, metaColor, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

void DrawSoundPickerPage(HDC hdc, const RECT& clientRect, HFONT sectionFont, HFONT bodyFont, HFONT metaFont) {
    (void)clientRect;
    FillRoundedRect(hdc, g_soundPickerCard, kGuiCard, kGuiCardBorder, 18);
    RECT inset = MakeRectWH(g_soundPickerCard.left + 1, g_soundPickerCard.top + 1,
        (g_soundPickerCard.right - g_soundPickerCard.left) - 2,
        (g_soundPickerCard.bottom - g_soundPickerCard.top) - 2);
    FillRoundedRect(hdc, inset, kGuiCardInset, kGuiCardInset, 18);

    SelectObject(hdc, bodyFont);
    DrawButtonChip(hdc, g_soundPickerBackRect, "<", false, false);

    SelectObject(hdc, sectionFont);
    DrawTextLine(hdc, MakeRectWH(g_soundPickerBackRect.right + 14, g_soundPickerCard.top + 18,
        g_soundPickerCard.right - g_soundPickerBackRect.right - 32, 24),
        "Select Sound", kGuiText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    SelectObject(hdc, metaFont);
    DrawTextLine(hdc, MakeRectWH(g_soundPickerBackRect.right + 14, g_soundPickerCard.top + 44,
        g_soundPickerCard.right - g_soundPickerBackRect.right - 32, 18),
        GetSoundPickerFieldLabel(g_guiSoundPickerField), kGuiMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    SelectObject(hdc, bodyFont);
    DrawTextInputField(hdc, g_soundPickerSearchRect, g_guiSoundSearch, "Search sounds...", g_guiSoundSearchEditing);

    FillRoundedRect(hdc, g_soundPickerListRect, kGuiTrack, kGuiTrackBorder, 16);

    int savedDc = SaveDC(hdc);
    IntersectClipRect(hdc, g_soundPickerListRect.left + 2, g_soundPickerListRect.top + 2,
        g_soundPickerListRect.right - 2, g_soundPickerListRect.bottom - 2);

    std::vector<int> filteredSounds = GetFilteredSoundIds();
    for (int row = 0; row < (int)filteredSounds.size(); ++row) {
        RECT rowRect = {};
        if (!GetSoundPickerRowRect(row, (int)filteredSounds.size(), rowRect)) continue;
        if (rowRect.bottom < g_soundPickerListRect.top || rowRect.top > g_soundPickerListRect.bottom) continue;
        DrawSoundPickerRow(hdc, rowRect, filteredSounds[row], bodyFont, metaFont);
    }
    if (filteredSounds.empty()) {
        SelectObject(hdc, bodyFont);
        DrawTextLine(hdc, g_soundPickerListRect, "No matching sounds", kGuiMuted,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    RestoreDC(hdc, savedDc);
}

void DrawThemePreview(HDC hdc, const RECT& rect, int theme) {
    const GuiPalette& palette = GetGuiPaletteForTheme(theme);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0) return;

    int normalizedTheme = NormalizeGuiTheme(theme);
    Gdiplus::Bitmap* artwork = g_themeBackgroundImages[normalizedTheme];
    if (artwork) {
        int imageW = (int)artwork->GetWidth();
        int imageH = (int)artwork->GetHeight();
        float destinationAspect = (float)width / (float)height;
        int sourceX = 0;
        int sourceY = 0;
        int sourceW = imageW;
        int sourceH = imageH;

        if ((float)imageW / (float)imageH < destinationAspect) {
            sourceH = max(1, (int)((float)imageW / destinationAspect));
            float focus = 0.50f;
            if (normalizedTheme == GUI_THEME_NEON_CITY) focus = 0.46f;
            if (normalizedTheme == GUI_THEME_ENCHANTED_FOREST) focus = 0.52f;
            if (normalizedTheme == GUI_THEME_INFERNO) focus = 0.58f;
            if (normalizedTheme == GUI_THEME_ARCTIC_AURORA) focus = 0.34f;
            sourceY = (int)((float)(imageH - sourceH) * focus);
        }
        else {
            sourceW = max(1, (int)((float)imageH * destinationAspect));
            sourceX = (imageW - sourceW) / 2;
        }

        Gdiplus::Graphics graphics(hdc);
        graphics.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
        graphics.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
        graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        graphics.DrawImage(
            artwork,
            Gdiplus::Rect(rect.left, rect.top, width, height),
            sourceX, sourceY, sourceW, sourceH,
            Gdiplus::UnitPixel);

        // Cosmic uses separate planet sprites at runtime, so show them here too.
        if (normalizedTheme == GUI_THEME_COSMIC && g_cosmicPlanetSheetImage) {
            graphics.SetCompositingMode(Gdiplus::CompositingModeSourceOver);
            DrawCosmicPlanetFrame(graphics, 0, 1,
                Gdiplus::RectF((Gdiplus::REAL)(rect.right - 61), (Gdiplus::REAL)(rect.top + 9), 47.0f, 47.0f), 1.0f);
            DrawCosmicPlanetFrame(graphics, 1, 5,
                Gdiplus::RectF((Gdiplus::REAL)(rect.left + 12), (Gdiplus::REAL)(rect.bottom - 45), 51.0f, 39.0f), 1.0f);
        }
    }
    else {
        // Classic intentionally previews the original clean interface rather than fake scenery.
        FillVerticalGradient(hdc, rect, palette.bg, palette.bgBottom, 28);
        HBRUSH brush = static_cast<HBRUSH>(GetStockObject(DC_BRUSH));
        COLORREF oldColor = SetDCBrushColor(hdc, palette.header);
        RECT header = { rect.left, rect.top, rect.right, rect.top + max(15, height / 5) };
        FillRect(hdc, &header, brush);
        SetDCBrushColor(hdc, palette.accent);
        RECT accent = { rect.left + 8, header.bottom - 2, rect.right - 8, header.bottom };
        FillRect(hdc, &accent, brush);
        int cardTop = header.bottom + 7;
        int cardGap = 5;
        int cardH = max(10, (rect.bottom - cardTop - 8 - cardGap * 2) / 3);
        for (int i = 0; i < 3; ++i) {
            RECT card = { rect.left + 9, cardTop + i * (cardH + cardGap), rect.right - 9, cardTop + i * (cardH + cardGap) + cardH };
            FillRoundedRect(hdc, card, palette.card, palette.cardBorder, 5);
            SetDCBrushColor(hdc, palette.muted);
            RECT label = { card.left + 7, card.top + cardH / 2 - 1, card.left + width / 3, card.top + cardH / 2 + 1 };
            FillRect(hdc, &label, brush);
            SetDCBrushColor(hdc, i == 0 ? palette.accent : palette.toggleOff);
            RECT toggle = { card.right - 20, card.top + cardH / 2 - 4, card.right - 7, card.top + cardH / 2 + 4 };
            FillRect(hdc, &toggle, brush);
        }
        SetDCBrushColor(hdc, oldColor);
    }

    HPEN borderPen = CreatePen(PS_SOLID, 1, palette.buttonBorder);
    HGDIOBJ oldPen = SelectObject(hdc, borderPen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);
}

void DrawThemePickerPage(HDC hdc, const RECT& clientRect, HFONT sectionFont, HFONT bodyFont, HFONT metaFont) {
    (void)clientRect;
    FillRoundedRect(hdc, g_themePickerCard, kGuiCard, kGuiCardBorder, 18);
    RECT inset = MakeRectWH(g_themePickerCard.left + 1, g_themePickerCard.top + 1,
        (g_themePickerCard.right - g_themePickerCard.left) - 2,
        (g_themePickerCard.bottom - g_themePickerCard.top) - 2);
    FillRoundedRect(hdc, inset, kGuiCardInset, kGuiCardInset, 18);

    SelectObject(hdc, bodyFont);
    DrawButtonChip(hdc, g_themePickerBackRect, "<", false, false);
    SelectObject(hdc, sectionFont);
    DrawTextLine(hdc, MakeRectWH(g_themePickerBackRect.right + 14, g_themePickerCard.top + 18,
        g_themePickerCard.right - g_themePickerBackRect.right - 32, 24),
        "Themes", kGuiText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    SelectObject(hdc, metaFont);
    DrawTextLine(hdc, MakeRectWH(g_themePickerBackRect.right + 14, g_themePickerCard.top + 43,
        g_themePickerCard.right - g_themePickerBackRect.right - 32, 18),
        "Choose a pixel-art style", kGuiMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    for (int theme = 0; theme < GUI_THEME_COUNT; ++theme) {
        const RECT& option = g_themeOptionRects[theme];
        bool active = NormalizeGuiTheme(g_guiTheme) == theme;
        const GuiPalette& previewPalette = GetGuiPaletteForTheme(theme);
        COLORREF fill = active ? kGuiAccentSoft : kGuiButton;
        COLORREF border = active ? GetGuiPalette().activeBorder : kGuiButtonBorder;
        FillRoundedRect(hdc, option, fill, border, 14);

        RECT preview = MakeRectWH(option.left + 10, option.top + 10,
            (option.right - option.left) - 20, max(30, (option.bottom - option.top) - 72));
        DrawThemePreview(hdc, preview, theme);

        SelectObject(hdc, bodyFont);
        RECT title = MakeRectWH(option.left + 12, option.bottom - 55, (option.right - option.left) - 24, 20);
        DrawTextLine(hdc, title, GetGuiThemeName(theme), active ? kGuiText : kGuiButtonIdleText,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        SelectObject(hdc, metaFont);
        RECT subtitle = MakeRectWH(option.left + 12, option.bottom - 33, (option.right - option.left) - 24, 17);
        DrawTextLine(hdc, subtitle, GetGuiThemeSubtitle(theme), active ? GetGuiPalette().activeMeta : kGuiMuted,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        if (active) {
            SetDCBrushColor(hdc, previewPalette.accent);
            RECT selected = { option.right - 19, option.top + 7, option.right - 9, option.top + 10 };
            FillRect(hdc, &selected, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
        }
    }
}

float GetTimerScaleRatio(float value) {
    return (ClampFloat(value, kTimerScaleMin, kTimerScaleMax) - kTimerScaleMin) /
        (kTimerScaleMax - kTimerScaleMin);
}

float TimerScaleFromRatio(float ratio) {
    float clamped = ClampFloat(ratio, 0.0f, 1.0f);
    return kTimerScaleMin + ((kTimerScaleMax - kTimerScaleMin) * clamped);
}

void SetTimerScaleFromMouseX(int x) {
    int width = g_timerScaleTrackRect.right - g_timerScaleTrackRect.left;
    if (width <= 0) width = 1;
    float ratio = (float)(x - g_timerScaleTrackRect.left) / (float)width;
    g_config.scale = TimerScaleFromRatio(ratio);
}

void SetTimerColourChannelFromMouseX(GuiSliderTarget target, int x) {
    const RECT* track = nullptr;
    if (target == GUI_SLIDER_TIMER_COLOUR_RED) track = &g_numberColourRedTrackRect;
    else if (target == GUI_SLIDER_TIMER_COLOUR_GREEN) track = &g_numberColourGreenTrackRect;
    else if (target == GUI_SLIDER_TIMER_COLOUR_BLUE) track = &g_numberColourBlueTrackRect;
    if (!track) return;

    int width = track->right - track->left;
    if (width <= 0) width = 1;
    float ratio = ClampFloat((float)(x - track->left) / (float)width, 0.0f, 1.0f);
    int value = (int)std::floor(ratio * 255.0f + 0.5f);
    if (target == GUI_SLIDER_TIMER_COLOUR_RED) g_guiNumberColourRed = value;
    else if (target == GUI_SLIDER_TIMER_COLOUR_GREEN) g_guiNumberColourGreen = value;
    else g_guiNumberColourBlue = value;
    ApplyGuiNumberColourToSelection();
}

float GetAlertVolumeRatio(float value) {
    return (ClampFloat(value, kAlertVolumeMin, kAlertVolumeMax) - kAlertVolumeMin) /
        (kAlertVolumeMax - kAlertVolumeMin);
}

float AlertVolumeFromRatio(float ratio) {
    float clamped = ClampFloat(ratio, 0.0f, 1.0f);
    return kAlertVolumeMin + ((kAlertVolumeMax - kAlertVolumeMin) * clamped);
}

void SetSpeed3VolumeFromMouseX(int x) {
    int width = g_speed3VolumeTrackRect.right - g_speed3VolumeTrackRect.left;
    if (width <= 0) width = 1;
    float ratio = (float)(x - g_speed3VolumeTrackRect.left) / (float)width;
    g_speed3Volume = AlertVolumeFromRatio(ratio);
}

void SetSlownessVolumeFromMouseX(int x) {
    int width = g_slownessVolumeTrackRect.right - g_slownessVolumeTrackRect.left;
    if (width <= 0) width = 1;
    float ratio = (float)(x - g_slownessVolumeTrackRect.left) / (float)width;
    g_slownessVolume = AlertVolumeFromRatio(ratio);
}

void FillRoundedRect(HDC hdc, const RECT& rect, COLORREF fill, COLORREF border, int radius) {
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HBRUSH brush = CreateSolidBrush(fill);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBrush = SelectObject(hdc, brush);
    RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void DrawTextLine(HDC hdc, const RECT& rect, const std::string& text, COLORREF color, UINT format) {
    RECT drawRect = rect;
    SetTextColor(hdc, color);
    DrawTextA(hdc, text.c_str(), -1, &drawRect, format | DT_NOPREFIX);
}

void DrawToggleSwitch(HDC hdc, const RECT& rect, bool enabled, bool available = true) {
    const GuiPalette& palette = GetGuiPalette();
    COLORREF trackFill = !available
        ? BlendGuiColor(palette.toggleOff, palette.bg, 0.48f)
        : (enabled ? kGuiAccent : palette.toggleOff);
    COLORREF trackBorder = !available
        ? BlendGuiColor(palette.toggleOffBorder, palette.bg, 0.58f)
        : (enabled ? kGuiAccentSoft : palette.toggleOffBorder);
    FillRoundedRect(hdc, rect, trackFill, trackBorder, rect.bottom - rect.top);

    int knobSize = (rect.bottom - rect.top) - 6;
    int knobLeft = enabled ? (rect.right - knobSize - 3) : (rect.left + 3);
    RECT knob = { knobLeft, rect.top + 3, knobLeft + knobSize, rect.top + 3 + knobSize };
    COLORREF knobFill = available ? palette.knob : BlendGuiColor(palette.knob, palette.bg, 0.62f);
    COLORREF knobBorder = available ? palette.knobBorder : trackBorder;
    HBRUSH knobBrush = CreateSolidBrush(knobFill);
    HPEN knobPen = CreatePen(PS_SOLID, 1, knobBorder);
    HGDIOBJ oldBrush = SelectObject(hdc, knobBrush);
    HGDIOBJ oldPen = SelectObject(hdc, knobPen);
    Ellipse(hdc, knob.left, knob.top, knob.right, knob.bottom);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(knobPen);
    DeleteObject(knobBrush);
}

void DrawCheckbox(HDC hdc, const RECT& rect, bool checked) {
    const GuiPalette& palette = GetGuiPalette();
    FillRoundedRect(hdc, rect,
        checked ? kGuiAccent : kGuiButton,
        checked ? palette.activeBorder : kGuiButtonBorder,
        6);
    if (!checked) return;

    HPEN checkPen = CreatePen(PS_SOLID, 2, palette.knob);
    HGDIOBJ oldPen = SelectObject(hdc, checkPen);
    MoveToEx(hdc, rect.left + 5, rect.top + 11, nullptr);
    LineTo(hdc, rect.left + 9, rect.top + 15);
    LineTo(hdc, rect.right - 4, rect.top + 6);
    SelectObject(hdc, oldPen);
    DeleteObject(checkPen);
}

void DrawButtonChip(HDC hdc, const RECT& rect, const std::string& text, bool active, bool accent) {
    const GuiPalette& palette = GetGuiPalette();
    COLORREF fill = active
        ? (accent ? kGuiAccentSoft : palette.activeAltFill)
        : kGuiButton;
    COLORREF border = active
        ? (accent ? palette.activeBorder : palette.activeAltBorder)
        : kGuiButtonBorder;
    COLORREF textColor = active ? kGuiText : kGuiButtonIdleText;

    FillRoundedRect(hdc, rect, fill, border, 12);
    DrawTextLine(hdc, rect, text, textColor, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

void DrawTextInputField(HDC hdc, const RECT& rect, const std::string& text, const std::string& placeholder, bool active) {
    COLORREF border = active ? GetGuiPalette().activeBorder : kGuiButtonBorder;
    FillRoundedRect(hdc, rect, kGuiButton, border, 10);

    RECT textRect = MakeRectWH(rect.left + 12, rect.top, (rect.right - rect.left) - 24, rect.bottom - rect.top);
    std::string displayText = text.empty() ? placeholder : text;
    COLORREF textColor = text.empty() ? kGuiMuted : kGuiText;
    DrawTextLine(hdc, textRect, displayText, textColor, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    if (active) {
        SIZE textSize = {};
        GetTextExtentPoint32A(hdc, text.c_str(), (int)text.size(), &textSize);
        int caretX = textRect.left + textSize.cx + 2;
        if (caretX > textRect.right - 2) caretX = textRect.right - 2;
        HPEN caretPen = CreatePen(PS_SOLID, 1, kGuiText);
        HGDIOBJ oldPen = SelectObject(hdc, caretPen);
        MoveToEx(hdc, caretX, rect.top + 8, nullptr);
        LineTo(hdc, caretX, rect.bottom - 8);
        SelectObject(hdc, oldPen);
        DeleteObject(caretPen);
    }
}

void DrawSlider(HDC hdc, const RECT& trackRect, float ratio) {
    const GuiPalette& palette = GetGuiPalette();
    RECT track = trackRect;
    FillRoundedRect(hdc, track, kGuiTrack, kGuiTrackBorder, track.bottom - track.top);

    int trackWidth = track.right - track.left;
    if (trackWidth <= 0) trackWidth = 1;
    float clampedRatio = ClampFloat(ratio, 0.0f, 1.0f);
    int fillRight = track.left + (int)(clampedRatio * (float)trackWidth);
    if (fillRight < track.left + 4) fillRight = track.left + 4;
    RECT fill = track;
    fill.right = fillRight;
    FillRoundedRect(hdc, fill, kGuiAccent, kGuiAccent, fill.bottom - fill.top);
    if (IsAnimatedGuiTheme() && fill.right - fill.left > 4) {
        RECT gradientFill = { fill.left + 2, fill.top + 2, fill.right - 2, fill.bottom - 2 };
        FillHorizontalGradient(hdc, gradientFill, palette.accentAlt, palette.accent, 28);
    }

    int knobCenterX = track.left + (int)(clampedRatio * (float)trackWidth);
    RECT knob = { knobCenterX - 8, track.top - 6, knobCenterX + 8, track.bottom + 6 };
    HBRUSH knobBrush = CreateSolidBrush(palette.knob);
    HPEN knobPen = CreatePen(PS_SOLID, 1, palette.knobBorder);
    HGDIOBJ oldBrush = SelectObject(hdc, knobBrush);
    HGDIOBJ oldPen = SelectObject(hdc, knobPen);
    Ellipse(hdc, knob.left, knob.top, knob.right, knob.bottom);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(knobPen);
    DeleteObject(knobBrush);
}

COLORREF TimerColourToColorRef(std::uint32_t colour) {
    return RGB(GetTimerColourRed(colour), GetTimerColourGreen(colour), GetTimerColourBlue(colour));
}

COLORREF GetTimerColourTextColor(std::uint32_t colour) {
    int luminance = GetTimerColourRed(colour) * 299 +
        GetTimerColourGreen(colour) * 587 +
        GetTimerColourBlue(colour) * 114;
    return luminance >= 150000 ? RGB(12, 12, 20) : RGB(255, 255, 255);
}

void DrawEnchantedNumberGlint(HDC hdc, const RECT& rect) {
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    if (width <= 6 || height <= 6) return;

    ULONGLONG nowMs = GetTickCount64();
    const int stripeSpacing = 28;
    int phase = (int)((nowMs / 22ULL) % stripeSpacing);

    int savedDc = SaveDC(hdc);
    IntersectClipRect(hdc, rect.left + 3, rect.top + 3, rect.right - 3, rect.bottom - 3);

    HPEN purplePen = CreatePen(PS_SOLID, 3, RGB(185, 72, 255));
    HGDIOBJ oldPen = SelectObject(hdc, purplePen);
    for (int x = rect.left - height - stripeSpacing + phase;
        x < rect.right + stripeSpacing; x += stripeSpacing) {
        MoveToEx(hdc, x, rect.bottom - 3, nullptr);
        LineTo(hdc, x + height, rect.top + 3);
    }

    HPEN shinePen = CreatePen(PS_SOLID, 1, RGB(105, 245, 255));
    SelectObject(hdc, shinePen);
    for (int x = rect.left - height - stripeSpacing + phase + 3;
        x < rect.right + stripeSpacing; x += stripeSpacing) {
        MoveToEx(hdc, x, rect.bottom - 3, nullptr);
        LineTo(hdc, x + height, rect.top + 3);
    }
    SelectObject(hdc, oldPen);
    DeleteObject(shinePen);
    DeleteObject(purplePen);
    RestoreDC(hdc, savedDc);

    COLORREF borderColour = ((nowMs / 180ULL) % 2ULL) == 0
        ? RGB(220, 135, 255)
        : RGB(105, 245, 255);
    HPEN borderPen = CreatePen(PS_SOLID, 2, borderColour);
    HGDIOBJ oldBorderPen = SelectObject(hdc, borderPen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    RoundRect(hdc, rect.left + 1, rect.top + 1, rect.right - 1, rect.bottom - 1, 10, 10);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldBorderPen);
    DeleteObject(borderPen);
}

void DrawTimerColourSlider(HDC hdc, const RECT& track, GuiSliderTarget target, int value) {
    COLORREF leftColour = RGB(g_guiNumberColourRed, g_guiNumberColourGreen, g_guiNumberColourBlue);
    COLORREF rightColour = leftColour;
    if (target == GUI_SLIDER_TIMER_COLOUR_RED) {
        leftColour = RGB(0, g_guiNumberColourGreen, g_guiNumberColourBlue);
        rightColour = RGB(255, g_guiNumberColourGreen, g_guiNumberColourBlue);
    }
    else if (target == GUI_SLIDER_TIMER_COLOUR_GREEN) {
        leftColour = RGB(g_guiNumberColourRed, 0, g_guiNumberColourBlue);
        rightColour = RGB(g_guiNumberColourRed, 255, g_guiNumberColourBlue);
    }
    else if (target == GUI_SLIDER_TIMER_COLOUR_BLUE) {
        leftColour = RGB(g_guiNumberColourRed, g_guiNumberColourGreen, 0);
        rightColour = RGB(g_guiNumberColourRed, g_guiNumberColourGreen, 255);
    }

    FillRoundedRect(hdc, track, kGuiTrack, kGuiTrackBorder, track.bottom - track.top);
    RECT gradient = { track.left + 2, track.top + 2, track.right - 2, track.bottom - 2 };
    FillHorizontalGradient(hdc, gradient, leftColour, rightColour, 96);

    int width = max(1, track.right - track.left);
    int knobX = track.left + (int)((float)max(0, min(255, value)) / 255.0f * (float)width);
    RECT knob = { knobX - 8, track.top - 6, knobX + 8, track.bottom + 6 };
    HBRUSH knobBrush = CreateSolidBrush(GetGuiPalette().knob);
    HPEN knobPen = CreatePen(PS_SOLID, 1, GetGuiPalette().knobBorder);
    HGDIOBJ oldBrush = SelectObject(hdc, knobBrush);
    HGDIOBJ oldPen = SelectObject(hdc, knobPen);
    Ellipse(hdc, knob.left, knob.top, knob.right, knob.bottom);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(knobPen);
    DeleteObject(knobBrush);
}

void DrawNumberColourPickerPage(HDC hdc, const RECT& clientRect, HFONT sectionFont, HFONT bodyFont, HFONT metaFont) {
    (void)clientRect;
    FillRoundedRect(hdc, g_numberColourPickerCard, kGuiCard, kGuiCardBorder, 18);
    RECT inset = MakeRectWH(g_numberColourPickerCard.left + 1, g_numberColourPickerCard.top + 1,
        (g_numberColourPickerCard.right - g_numberColourPickerCard.left) - 2,
        (g_numberColourPickerCard.bottom - g_numberColourPickerCard.top) - 2);
    FillRoundedRect(hdc, inset, kGuiCardInset, kGuiCardInset, 18);

    SelectObject(hdc, bodyFont);
    DrawButtonChip(hdc, g_numberColourPickerBackRect, "<", false, false);
    SelectObject(hdc, sectionFont);
    DrawTextLine(hdc, MakeRectWH(g_numberColourPickerBackRect.right + 14, g_numberColourPickerCard.top + 18,
        g_numberColourPickerCard.right - g_numberColourPickerBackRect.right - 32, 24),
        "Number Colour", kGuiText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    SelectObject(hdc, metaFont);
    DrawTextLine(hdc, MakeRectWH(g_numberColourPickerBackRect.right + 14, g_numberColourPickerCard.top + 44,
        g_numberColourPickerCard.right - g_numberColourPickerBackRect.right - 32, 18),
        "Drag across numbers to select a range", kGuiMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    SelectObject(hdc, bodyFont);
    DrawButtonChip(hdc, g_numberColourRgbModeRect, "RGB Colours",
        g_guiNumberColourMode == GUI_NUMBER_COLOUR_RGB, true);
    DrawButtonChip(hdc, g_numberColourMinecraftModeRect, "Minecraft Colours",
        g_guiNumberColourMode == GUI_NUMBER_COLOUR_MINECRAFT, true);

    for (int number = kTimerNumberMax; number >= kTimerNumberMin; --number) {
        const RECT& numberRect = g_numberColourNumberRects[number];
        std::uint32_t colour = GetTimerNumberColour(number);
        bool selected = g_guiNumberColourSelection[number];
        FillRoundedRect(hdc, numberRect, TimerColourToColorRef(colour),
            selected ? GetGuiPalette().activeBorder : kGuiButtonBorder, 10);
        if (selected) DrawEnchantedNumberGlint(hdc, numberRect);

        char numberLabel[8] = {};
        sprintf_s(numberLabel, "%d", number);
        SelectObject(hdc, bodyFont);
        DrawTextLine(hdc, numberRect, numberLabel, GetTimerColourTextColor(colour),
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    std::uint32_t selectedColour = PackTimerColour(
        g_guiNumberColourRed, g_guiNumberColourGreen, g_guiNumberColourBlue);
    FillRoundedRect(hdc, g_numberColourPreviewRect, TimerColourToColorRef(selectedColour),
        GetGuiPalette().activeBorder, 12);
    SelectObject(hdc, bodyFont);
    DrawTextLine(hdc, g_numberColourPreviewRect, FormatNumberColourSelectionLabel(),
        GetTimerColourTextColor(selectedColour), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    if (g_guiNumberColourMode == GUI_NUMBER_COLOUR_RGB) {
        char redLabel[24] = {};
        char greenLabel[24] = {};
        char blueLabel[24] = {};
        sprintf_s(redLabel, "Red  %d", g_guiNumberColourRed);
        sprintf_s(greenLabel, "Green  %d", g_guiNumberColourGreen);
        sprintf_s(blueLabel, "Blue  %d", g_guiNumberColourBlue);
        SelectObject(hdc, metaFont);
        DrawTextLine(hdc, MakeRectWH(g_numberColourRedTrackRect.left, g_numberColourRedTrackRect.top - 26, 160, 18),
            redLabel, kGuiText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        DrawTextLine(hdc, MakeRectWH(g_numberColourGreenTrackRect.left, g_numberColourGreenTrackRect.top - 26, 160, 18),
            greenLabel, kGuiText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        DrawTextLine(hdc, MakeRectWH(g_numberColourBlueTrackRect.left, g_numberColourBlueTrackRect.top - 26, 160, 18),
            blueLabel, kGuiText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        DrawTimerColourSlider(hdc, g_numberColourRedTrackRect, GUI_SLIDER_TIMER_COLOUR_RED, g_guiNumberColourRed);
        DrawTimerColourSlider(hdc, g_numberColourGreenTrackRect, GUI_SLIDER_TIMER_COLOUR_GREEN, g_guiNumberColourGreen);
        DrawTimerColourSlider(hdc, g_numberColourBlueTrackRect, GUI_SLIDER_TIMER_COLOUR_BLUE, g_guiNumberColourBlue);
    }
    else {
        SelectObject(hdc, metaFont);
        DrawTextLine(hdc, MakeRectWH(g_numberColourPreviewRect.left, g_numberColourPreviewRect.bottom + 7,
            g_numberColourPreviewRect.right - g_numberColourPreviewRect.left, 18),
            "Choose a Minecraft colour", kGuiMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        for (int optionIndex = 0; optionIndex < (int)kMinecraftColourOptions.size(); ++optionIndex) {
            const MinecraftColourOption& option = kMinecraftColourOptions[optionIndex];
            const RECT& optionRect = g_minecraftColourOptionRects[optionIndex];
            std::uint32_t optionColour = PackTimerColour(option.red, option.green, option.blue);
            bool active = optionColour == selectedColour;
            FillRoundedRect(hdc, optionRect, TimerColourToColorRef(optionColour),
                active ? GetGuiPalette().activeBorder : kGuiButtonBorder, 9);

            char code = option.code >= 'a' && option.code <= 'f'
                ? (char)(option.code - 'a' + 'A')
                : option.code;
            char label[32] = {};
            sprintf_s(label, "%c  %s", code, option.name);
            SelectObject(hdc, metaFont);
            DrawTextLine(hdc, optionRect, label, GetTimerColourTextColor(optionColour),
                DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
    }
}

void ApplyMinecraftColourToSelection(int optionIndex) {
    if (optionIndex < 0 || optionIndex >= (int)kMinecraftColourOptions.size()) return;
    const MinecraftColourOption& option = kMinecraftColourOptions[optionIndex];
    g_guiNumberColourRed = option.red;
    g_guiNumberColourGreen = option.green;
    g_guiNumberColourBlue = option.blue;
    ApplyGuiNumberColourToSelection();
    SaveToolSettings();
}

bool PointInVisibleCardRect(const RECT& card, const RECT& rect, int x, int y) {
    return PointInRectEx(card, x, y) && PointInRectEx(rect, x, y);
}

bool IsCardBodyVisible(const RECT& card) {
    return (card.bottom - card.top) > (kGuiCardHeaderHeight + 4);
}

void DrawCardHeader(HDC hdc, const RECT& card, const std::string& title, const RECT* toggleRect, bool enabled, const RECT& expandRect, bool expanded, bool toggleAvailable = true) {
    FillRoundedRect(hdc, card, kGuiCard, kGuiCardBorder, 18);
    RECT inset = MakeRectWH(card.left + 1, card.top + 1, (card.right - card.left) - 2, (card.bottom - card.top) - 2);
    FillRoundedRect(hdc, inset, kGuiCardInset, kGuiCardInset, 18);

    if (IsAnimatedGuiTheme()) {
        RECT glint = { card.left + 20, card.top + 1, card.left + 174, card.top + 3 };
        FillHorizontalGradient(hdc, glint, GetGuiPalette().accentAlt, GetGuiPalette().accent, 36);
    }

    int reservedWidth = (expandRect.right - expandRect.left) + 36;
    if (toggleRect) reservedWidth += (toggleRect->right - toggleRect->left) + 12;
    RECT titleRect = MakeRectWH(card.left + 18, card.top + 14, (card.right - card.left) - reservedWidth - 18, 22);
    DrawTextLine(hdc, titleRect, title, toggleAvailable ? kGuiText : kGuiMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    if (toggleRect) DrawToggleSwitch(hdc, *toggleRect, enabled, toggleAvailable);
    DrawButtonChip(hdc, expandRect, expanded ? "-" : "+", expanded, true);
}

void DrawGuiTooltip(HDC hdc, const RECT& clientRect, POINT cursor, const std::string& text) {
    SIZE textSize = {};
    GetTextExtentPoint32A(hdc, text.c_str(), (int)text.size(), &textSize);

    const int paddingX = 12;
    const int paddingY = 8;
    int width = textSize.cx + (paddingX * 2);
    int height = textSize.cy + (paddingY * 2);
    int left = cursor.x + 14;
    int top = cursor.y + 18;

    if (left + width > clientRect.right - 8) left = cursor.x - width - 14;
    if (top + height > clientRect.bottom - 8) top = cursor.y - height - 14;
    left = max(clientRect.left + 8, left);
    top = max(clientRect.top + 8, top);

    RECT tooltipRect = MakeRectWH(left, top, width, height);
    const GuiPalette& palette = GetGuiPalette();
    FillRoundedRect(
        hdc,
        tooltipRect,
        BlendGuiColor(palette.card, palette.bg, 0.16f),
        BlendGuiColor(palette.accent, palette.cardBorder, 0.38f),
        10);
    DrawTextLine(
        hdc,
        MakeRectWH(left + paddingX, top + paddingY, textSize.cx, textSize.cy),
        text,
        kGuiText,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

void DrawSocialFooterButton(HDC hdc, const RECT& rect, bool youtube) {
    const GuiPalette& palette = GetGuiPalette();
    COLORREF circleFill = BlendGuiColor(palette.card, palette.bg, 0.34f);
    COLORREF circleBorder = BlendGuiColor(palette.accent, palette.cardBorder, 0.20f);

    HBRUSH circleBrush = CreateSolidBrush(circleFill);
    HPEN circlePen = CreatePen(PS_SOLID, 2, circleBorder);
    HGDIOBJ oldBrush = SelectObject(hdc, circleBrush);
    HGDIOBJ oldPen = SelectObject(hdc, circlePen);
    Ellipse(hdc, rect.left + 1, rect.top + 1, rect.right - 1, rect.bottom - 1);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(circlePen);
    DeleteObject(circleBrush);

    int centerX = (rect.left + rect.right) / 2;
    int centerY = (rect.top + rect.bottom) / 2;
    if (youtube) {
        RECT mark = MakeRectWH(centerX - 11, centerY - 8, 22, 16);
        FillRoundedRect(hdc, mark, RGB(255, 0, 0), RGB(255, 76, 76), 7);

        POINT play[3] = {
            { centerX - 3, centerY - 5 },
            { centerX - 3, centerY + 5 },
            { centerX + 6, centerY }
        };
        HBRUSH playBrush = CreateSolidBrush(RGB(255, 255, 255));
        HPEN playPen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
        oldBrush = SelectObject(hdc, playBrush);
        oldPen = SelectObject(hdc, playPen);
        Polygon(hdc, play, 3);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(playPen);
        DeleteObject(playBrush);
        return;
    }

    RECT discordMark = MakeRectWH(centerX - 11, centerY - 9, 22, 18);
    FillRoundedRect(hdc, discordMark, RGB(88, 101, 242), RGB(126, 137, 255), 8);
    POINT controller[10] = {
        { centerX - 7, centerY + 5 },
        { centerX - 9, centerY + 2 },
        { centerX - 7, centerY - 4 },
        { centerX - 4, centerY - 6 },
        { centerX + 4, centerY - 6 },
        { centerX + 7, centerY - 4 },
        { centerX + 9, centerY + 2 },
        { centerX + 7, centerY + 5 },
        { centerX + 4, centerY + 2 },
        { centerX - 4, centerY + 2 }
    };
    HBRUSH controllerBrush = CreateSolidBrush(RGB(255, 255, 255));
    HPEN controllerPen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
    oldBrush = SelectObject(hdc, controllerBrush);
    oldPen = SelectObject(hdc, controllerPen);
    Polygon(hdc, controller, 10);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(controllerPen);
    DeleteObject(controllerBrush);

    HBRUSH eyeBrush = CreateSolidBrush(RGB(88, 101, 242));
    oldBrush = SelectObject(hdc, eyeBrush);
    oldPen = SelectObject(hdc, GetStockObject(NULL_PEN));
    Ellipse(hdc, centerX - 6, centerY - 2, centerX - 2, centerY + 2);
    Ellipse(hdc, centerX + 2, centerY - 2, centerX + 6, centerY + 2);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(eyeBrush);
}

void OpenGuiSocialLink(const char* label, const char* url) {
    HINSTANCE result = ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
    if ((INT_PTR)result <= 32) {
        DebugLog("GUI %s URL open failed result=%ld", label, (long)(INT_PTR)result);
    }
}

void DrawMainPageScrollbar(HDC hdc, const RECT& clientRect) {
    int maxScroll = GetMainPageMaxScroll();
    if (maxScroll <= 0 || g_guiMainViewportHeight <= 0 || g_guiMainContentHeight <= 0) return;

    RECT track = { clientRect.right - 12, 76, clientRect.right - 6, clientRect.bottom - 20 };
    int trackHeight = track.bottom - track.top;
    if (trackHeight <= 0) return;

    int thumbHeight = (g_guiMainViewportHeight * trackHeight) / g_guiMainContentHeight;
    if (thumbHeight < 34) thumbHeight = 34;
    if (thumbHeight > trackHeight) thumbHeight = trackHeight;

    int travel = trackHeight - thumbHeight;
    int thumbTop = track.top;
    if (travel > 0) thumbTop += (g_guiMainScroll * travel) / maxScroll;

    RECT thumb = { track.left, thumbTop, track.right, thumbTop + thumbHeight };
    const GuiPalette& palette = GetGuiPalette();
    FillRoundedRect(hdc, track, palette.scrollTrack, palette.scrollTrackBorder, 6);
    FillRoundedRect(hdc, thumb, kGuiAccentSoft, palette.scrollThumbBorder, 6);
}

LRESULT CALLBACK GuiWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;

    case WM_CAPTURECHANGED:
        g_guiSliderTarget = GUI_SLIDER_NONE;
        g_guiSliderDirty = false;
        g_guiNumberColourSelectionDragging = false;
        return 0;

    case WM_TIMER:
        if (wParam == kGuiAnimationTimerId) {
            bool repaint = UpdateGuiAnimations();
            ULONGLONG nowMs = GetTickCount64();
            if (IsAnimatedGuiTheme()) {
                if (g_guiLastCosmicRepaintMs == 0 ||
                    nowMs - g_guiLastCosmicRepaintMs >= kGuiCosmicFrameIntervalMs) {
                    g_guiLastCosmicRepaintMs = nowMs;
                    repaint = true;
                }
            }
            else {
                g_guiLastCosmicRepaintMs = 0;
            }
            if (g_guiCurrentPage == GUI_PAGE_NUMBER_COLOUR_PICKER) repaint = true;
            if (repaint) RequestGuiRepaint();
            return 0;
        }
        break;

    case WM_SIZE:
        RequestGuiRepaint();
        return 0;

    case WM_NCHITTEST: {
        LRESULT hit = DefWindowProcA(hwnd, msg, wParam, lParam);
        if (hit != HTCLIENT) return hit;

        RECT rc; GetClientRect(hwnd, &rc);
        LayoutGuiControls(rc.right, rc.bottom);

        POINT pt = { (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam) };
        ScreenToClient(hwnd, &pt);
        if (pt.x < 0 || pt.y < 0 || pt.x >= rc.right || pt.y >= rc.bottom) return HTNOWHERE;
        if (PointInRectEx(g_headerThemeRect, pt.x, pt.y) ||
            PointInRectEx(g_headerMinimizeRect, pt.x, pt.y) ||
            PointInRectEx(g_headerCloseRect, pt.x, pt.y)) return HTCLIENT;
        if (pt.y < kGuiWindowHeaderHeight) return HTCAPTION;
        return HTCLIENT;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        LayoutGuiControls(rc.right, rc.bottom);
        const int legacyBodyShift = kGuiCardHeaderHeight - 48;

        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

        DrawGuiBackground(memDC, rc);

        RECT header = { 0, 0, rc.right, kGuiWindowHeaderHeight };
        if (IsAnimatedGuiTheme()) {
            FillHorizontalGradient(memDC, header, GetGuiPalette().header, GetGuiPalette().headerBottom, 72);
            RECT subtleGlow = { rc.right - 220, 0, rc.right, kGuiWindowHeaderHeight };
            FillHorizontalGradient(memDC, subtleGlow, GetGuiPalette().headerBottom, kGuiAccentGlow, 42);
            if (IsCosmicGuiTheme()) DrawCosmicStars(memDC, header, 18);
        }
        else {
            HBRUSH headerBrush = CreateSolidBrush(kGuiHeader);
            FillRect(memDC, &header, headerBrush);
            DeleteObject(headerBrush);

            RECT subtleGlow = { rc.right - 180, 0, rc.right, 72 };
            HBRUSH subtleGlowBrush = CreateSolidBrush(kGuiAccentGlow);
            FillRect(memDC, &subtleGlow, subtleGlowBrush);
            DeleteObject(subtleGlowBrush);
        }

        RECT accent = { 20, kGuiWindowHeaderHeight - 2, rc.right - 20, kGuiWindowHeaderHeight + 1 };
        if (IsAnimatedGuiTheme()) {
            FillHorizontalGradient(memDC, accent, GetGuiPalette().accentAlt, kGuiAccent, 72);
        }
        else {
            HBRUSH accentBrush = CreateSolidBrush(kGuiDivider);
            FillRect(memDC, &accent, accentBrush);
            DeleteObject(accentBrush);
        }

        RECT bodyTexture = { 0, 112, rc.right, 113 };
        HBRUSH bodyTextureBrush = CreateSolidBrush(GetGuiPalette().bodyTexture);
        FillRect(memDC, &bodyTexture, bodyTextureBrush);
        DeleteObject(bodyTextureBrush);

        SetBkMode(memDC, TRANSPARENT);

        HFONT titleFont = CreateFontA(-28, 0, 0, 0, 700, 0, 0, 0, ANSI_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Bahnschrift SemiBold");
        HFONT sectionFont = CreateFontA(-18, 0, 0, 0, 700, 0, 0, 0, ANSI_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Bahnschrift SemiBold");
        HFONT bodyFont = CreateFontA(-15, 0, 0, 0, 500, 0, 0, 0, ANSI_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI Semibold");
        HFONT metaFont = CreateFontA(-13, 0, 0, 0, FW_NORMAL, 0, 0, 0, ANSI_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");

        HGDIOBJ oldFont = SelectObject(memDC, titleFont);
        DrawTextLine(memDC, MakeRectWH(20, 12, g_headerThemeRect.left - 40, 36), "TagEssentials", kGuiText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        SelectObject(memDC, bodyFont);
        DrawButtonChip(memDC, g_headerThemeRect, "THEMES", g_guiCurrentPage == GUI_PAGE_THEME_PICKER, true);
        DrawButtonChip(memDC, g_headerMinimizeRect, "-", false, false);
        DrawButtonChip(memDC, g_headerCloseRect, "X", false, false);

        if (g_guiCurrentPage == GUI_PAGE_SOUND_PICKER) {
            DrawSoundPickerPage(memDC, rc, sectionFont, bodyFont, metaFont);
        }
        else if (g_guiCurrentPage == GUI_PAGE_NUMBER_COLOUR_PICKER) {
            DrawNumberColourPickerPage(memDC, rc, sectionFont, bodyFont, metaFont);
        }
        else if (g_guiCurrentPage == GUI_PAGE_THEME_PICKER) {
            DrawThemePickerPage(memDC, rc, sectionFont, bodyFont, metaFont);
        }
        else {
            int mainSavedDc = SaveDC(memDC);
            IntersectClipRect(memDC, 0, kGuiWindowHeaderHeight + 1, rc.right, rc.bottom);

            SelectObject(memDC, sectionFont);
            const bool snaplookAvailable = !IsLunarNamedClient();
            DrawCardHeader(memDC, g_perspectiveCard, "Snaplook", &g_perspectiveToggleRect, IsPerspectiveModuleEnabled(), g_perspectiveExpandRect, g_perspectiveCardAnim.expanded, snaplookAvailable);
            if (IsCardBodyVisible(g_perspectiveCard)) {
                int savedDc = SaveDC(memDC);
                IntersectClipRect(memDC, g_perspectiveCard.left + 2, g_perspectiveCard.top + kGuiCardHeaderHeight, g_perspectiveCard.right - 2, g_perspectiveCard.bottom - 2);

                SelectObject(memDC, metaFont);
                DrawTextLine(memDC, MakeRectWH(g_perspectiveBindRect.left, g_perspectiveBindRect.top - 18, 90, 16), "Keybind", kGuiMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                DrawTextLine(memDC, MakeRectWH(g_perspectiveBackRect.left, g_perspectiveBackRect.top - 18, 100, 16), "Camera", kGuiMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

                SelectObject(memDC, bodyFont);
                DrawButtonChip(memDC, g_perspectiveBindRect, GetCaptureLabel(GUI_BIND_PERSPECTIVE, g_snaplookKeybind), g_guiBindCapture == GUI_BIND_PERSPECTIVE, true);
                DrawButtonChip(memDC, g_perspectiveBackRect, "Back", g_snaplookCameraMode == kCameraBack, false);
                DrawButtonChip(memDC, g_perspectiveFrontRect, "Front", g_snaplookCameraMode == kCameraFront, false);

                RestoreDC(memDC, savedDc);
            }

            SelectObject(memDC, sectionFont);
            DrawCardHeader(memDC, g_timerCard, "Timer Display", &g_timerShowToggleRect, g_guiTimerEnabled, g_timerExpandRect, g_timerCardAnim.expanded);
            if (IsCardBodyVisible(g_timerCard)) {
                int savedDc = SaveDC(memDC);
                IntersectClipRect(memDC, g_timerCard.left + 2, g_timerCard.top + kGuiCardHeaderHeight, g_timerCard.right - 2, g_timerCard.bottom - 2);

                SelectObject(memDC, metaFont);
                DrawTextLine(memDC, MakeRectWH(g_timerCard.left + 18, g_timerCard.top + 59 + legacyBodyShift, 140, 16), "Placement", kGuiMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                DrawTextLine(memDC, MakeRectWH(g_timerCard.left + 18, g_timerCard.top + 101 + legacyBodyShift, 140, 16), "Decimals", kGuiMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                DrawTextLine(memDC, MakeRectWH(g_timerCard.left + 18, g_timerNametagToggleRect.top - 2, 180, 16), "Show in nametags", kGuiText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                DrawTextLine(memDC, MakeRectWH(g_timerCard.left + 18, g_timerDefaultScoreboardToggleRect.top - 2, 210, 16), "Edit Default Scoreboard", NormalizeTimerDecimalPlaces(g_guiTimerDecimalPlaces) > 0 ? kGuiText : kGuiMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                DrawTextLine(memDC, MakeRectWH(g_timerCard.left + 18, g_timerCaptureToggleRect.top - 2, 180, 16), "OBS and Screenshots", kGuiText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                DrawTextLine(memDC, MakeRectWH(g_timerCard.left + 18, g_timerLockToggleRect.top - 2, 180, 16), "Lock free placement", kGuiText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                DrawTextLine(memDC, MakeRectWH(g_timerScaleTrackRect.left, g_timerScaleTrackRect.top - 22, 120, 16), "Timer size", kGuiMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                DrawTextLine(memDC, MakeRectWH(g_timerScaleTrackRect.right - 80, g_timerScaleTrackRect.top - 22, 80, 16), FormatTimerScaleLabel(g_config.scale), kGuiText, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

                SelectObject(memDC, bodyFont);
                DrawButtonChip(memDC, g_timerModeOverlayRect, "Overlay", !g_guiTimerCrosshairMode, false);
                DrawButtonChip(memDC, g_timerModeCrosshairRect, "Crosshair", g_guiTimerCrosshairMode, true);
                DrawButtonChip(memDC, g_timerDecimal0Rect, "0dp", g_guiTimerDecimalPlaces == 0, false);
                DrawButtonChip(memDC, g_timerDecimal1Rect, "1dp", g_guiTimerDecimalPlaces == 1, false);
                DrawButtonChip(memDC, g_timerDecimal2Rect, "2dp", g_guiTimerDecimalPlaces == 2, false);
                DrawButtonChip(memDC, g_timerNumberColourButtonRect, "Number Colour >", false, true);
                DrawButtonChip(memDC, g_timerNametagPrefixRect, "Prefix", NormalizeTimerNametagPosition(g_guiTimerNametagPosition) == TIMER_NAMETAG_POSITION_PREFIX, false);
                DrawButtonChip(memDC, g_timerNametagSuffixRect, "Suffix", NormalizeTimerNametagPosition(g_guiTimerNametagPosition) == TIMER_NAMETAG_POSITION_SUFFIX, true);
                DrawToggleSwitch(memDC, g_timerNametagToggleRect, g_guiTimerNametagEnabled);
                DrawToggleSwitch(memDC, g_timerDefaultScoreboardToggleRect, g_guiTimerEditDefaultScoreboard && NormalizeTimerDecimalPlaces(g_guiTimerDecimalPlaces) > 0);
                DrawToggleSwitch(memDC, g_timerCaptureToggleRect, g_guiTimerObsScreenshotsEnabled);
                DrawToggleSwitch(memDC, g_timerLockToggleRect, g_guiTimerLocked);
                DrawSlider(memDC, g_timerScaleTrackRect, GetTimerScaleRatio(g_config.scale));

                RestoreDC(memDC, savedDc);
            }

            SelectObject(memDC, sectionFont);
            DrawCardHeader(memDC, g_speedSlownessCard, "Speed & Slowness", &g_speedSlownessToggleRect, g_guiSpeedSlownessEnabled, g_speedSlownessExpandRect, g_speedSlownessCardAnim.expanded);
            if (IsCardBodyVisible(g_speedSlownessCard)) {
                int savedDc = SaveDC(memDC);
                IntersectClipRect(memDC, g_speedSlownessCard.left + 2, g_speedSlownessCard.top + kGuiCardHeaderHeight, g_speedSlownessCard.right - 2, g_speedSlownessCard.bottom - 2);

                SelectObject(memDC, metaFont);
                DrawTextLine(memDC, MakeRectWH(g_speedSlownessCard.left + 18, g_speedSlownessCard.top + 59 + legacyBodyShift, 150, 16), "Speed 3 sound", kGuiMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                DrawTextLine(memDC, MakeRectWH(g_speed3VolumeTrackRect.left, g_speed3VolumeTrackRect.top - 22, 150, 16), "Speed 3 volume", kGuiMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                DrawTextLine(memDC, MakeRectWH(g_speed3VolumeTrackRect.right - 80, g_speed3VolumeTrackRect.top - 22, 80, 16), FormatAlertVolumeLabel(g_speed3Volume), kGuiText, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
                DrawTextLine(memDC, MakeRectWH(g_speedSlownessCard.left + 18, g_speedSlownessCard.top + 131 + legacyBodyShift, 150, 16), "Slowness sound", kGuiMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                DrawTextLine(memDC, MakeRectWH(g_slownessVolumeTrackRect.left, g_slownessVolumeTrackRect.top - 22, 150, 16), "Slowness volume", kGuiMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                DrawTextLine(memDC, MakeRectWH(g_slownessVolumeTrackRect.right - 80, g_slownessVolumeTrackRect.top - 22, 80, 16), FormatAlertVolumeLabel(g_slownessVolume), kGuiText, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

                SelectObject(memDC, bodyFont);
                DrawButtonChip(memDC, g_speed3SoundButtonRect, FormatAlertSoundButtonLabel(g_speed3Sound), false, false);
                DrawButtonChip(memDC, g_slownessSoundButtonRect, FormatAlertSoundButtonLabel(g_slownessSound), false, false);
                DrawSlider(memDC, g_speed3VolumeTrackRect, GetAlertVolumeRatio(g_speed3Volume));
                DrawSlider(memDC, g_slownessVolumeTrackRect, GetAlertVolumeRatio(g_slownessVolume));

                RestoreDC(memDC, savedDc);
            }

            SelectObject(memDC, sectionFont);
            DrawCardHeader(memDC, g_publicHelpersCard, "Public Helpers", nullptr, false, g_publicHelpersExpandRect, g_publicHelpersCardAnim.expanded);
            if (IsCardBodyVisible(g_publicHelpersCard)) {
                int savedDc = SaveDC(memDC);
                IntersectClipRect(memDC, g_publicHelpersCard.left + 2, g_publicHelpersCard.top + kGuiCardHeaderHeight, g_publicHelpersCard.right - 2, g_publicHelpersCard.bottom - 2);

                SelectObject(memDC, bodyFont);
                DrawTextLine(memDC, MakeRectWH(g_publicHelpersCard.left + 18, g_publicWinsToggleRect.top - 2,
                    g_publicHelpersCard.right - g_publicHelpersCard.left - 100, 20),
                    "Wins in username", kGuiText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                DrawToggleSwitch(memDC, g_publicWinsToggleRect, g_guiPublicWinsEnabled);

                SelectObject(memDC, metaFont);
                DrawTextLine(memDC, MakeRectWH(g_publicHelpersCard.left + 18, g_publicWinsPrefixRect.top + 7, 100, 16),
                    "Position", kGuiMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                SelectObject(memDC, bodyFont);
                DrawButtonChip(memDC, g_publicWinsPrefixRect, "Prefix",
                    g_guiPublicWinsPosition == PUBLIC_WINS_POSITION_PREFIX, true);
                DrawButtonChip(memDC, g_publicWinsSuffixRect, "Suffix",
                    g_guiPublicWinsPosition == PUBLIC_WINS_POSITION_SUFFIX, true);

                SelectObject(memDC, bodyFont);
                DrawTextLine(memDC, MakeRectWH(g_publicHelpersCard.left + 18, g_publicWinsSpaceRect.top + 1,
                    g_publicHelpersCard.right - g_publicHelpersCard.left - 82, 20),
                    "Space between username", kGuiText,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                DrawCheckbox(memDC, g_publicWinsSpaceRect, g_guiPublicWinsSpaceBetweenUsername);

                RestoreDC(memDC, savedDc);
            }

            SelectObject(memDC, sectionFont);
            DrawCardHeader(memDC, g_extrasCard, "Extras", nullptr, false, g_extrasExpandRect, g_extrasCardAnim.expanded);
            if (IsCardBodyVisible(g_extrasCard)) {
                int savedDc = SaveDC(memDC);
                IntersectClipRect(memDC, g_extrasCard.left + 2, g_extrasCard.top + kGuiCardHeaderHeight, g_extrasCard.right - 2, g_extrasCard.bottom - 2);

                SelectObject(memDC, bodyFont);
                DrawTextLine(memDC, MakeRectWH(g_extrasCard.left + 18, g_extrasWheatToggleRect.top - 2, g_extrasCard.right - g_extrasCard.left - 100, 20), "Force wheat stage 0", kGuiText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                DrawTextLine(memDC, MakeRectWH(g_extrasCard.left + 18, g_extrasBeaconToggleRect.top - 2, g_extrasCard.right - g_extrasCard.left - 100, 20), "Hide beacon beams", kGuiText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                DrawTextLine(memDC, MakeRectWH(g_extrasCard.left + 18, g_extrasScoreboardToggleRect.top - 2, g_extrasCard.right - g_extrasCard.left - 100, 20), "Hide Tag Scoreboard", kGuiText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                DrawToggleSwitch(memDC, g_extrasWheatToggleRect, g_guiExtrasForceWheatStage1);
                DrawToggleSwitch(memDC, g_extrasBeaconToggleRect, g_guiExtrasHideBeaconBeams);
                DrawToggleSwitch(memDC, g_extrasScoreboardToggleRect, g_guiExtrasDisableTagScoreboard);

                RestoreDC(memDC, savedDc);
            }

            MutedVoiceGuiSnapshot mutedVoice = GetMutedVoiceGuiSnapshot();
            SelectObject(memDC, sectionFont);
            DrawCardHeader(memDC, g_mutedUtilitiesCard, "Muted Utilities", nullptr, false, g_mutedUtilitiesExpandRect, g_mutedUtilitiesCardAnim.expanded);
            if (IsCardBodyVisible(g_mutedUtilitiesCard)) {
                int savedDc = SaveDC(memDC);
                IntersectClipRect(memDC, g_mutedUtilitiesCard.left + 2, g_mutedUtilitiesCard.top + kGuiCardHeaderHeight, g_mutedUtilitiesCard.right - 2, g_mutedUtilitiesCard.bottom - 2);

                SelectObject(memDC, bodyFont);
                DrawTextLine(memDC, MakeRectWH(g_mutedUtilitiesCard.left + 18, g_mutedVoiceToggleRect.top - 2, g_mutedUtilitiesCard.right - g_mutedUtilitiesCard.left - 100, 20), "Muted Voice - " + mutedVoice.status, kGuiText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                DrawTextLine(memDC, MakeRectWH(g_mutedUtilitiesCard.left + 18, g_mutedVoiceHideMuteReminderToggleRect.top - 2, g_mutedUtilitiesCard.right - g_mutedUtilitiesCard.left - 100, 20), "Hide mute reminders", g_guiExtrasMutedVoice ? kGuiText : kGuiMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                DrawToggleSwitch(memDC, g_mutedVoiceToggleRect, g_guiExtrasMutedVoice);
                DrawToggleSwitch(memDC, g_mutedVoiceHideMuteReminderToggleRect, g_guiExtrasMutedVoice && g_guiExtrasMutedVoiceHideMuteReminder);

                SelectObject(memDC, metaFont);
                DrawTextLine(memDC, MakeRectWH(g_mutedUtilitiesCard.left + 18, g_mutedUtilitiesCard.top + 130, 180, 16), "Party owner", kGuiMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                SelectObject(memDC, bodyFont);
                DrawTextInputField(memDC, g_mutedVoicePartyOwnerFieldRect, g_guiMutedVoicePartyOwner, "Any account", g_guiMutedVoicePartyOwnerEditing);

                SelectObject(memDC, metaFont);
                std::string mutedDetail = mutedVoice.detail.empty() ? "No active sign-in prompt" : mutedVoice.detail;
                DrawTextLine(memDC, MakeRectWH(g_mutedUtilitiesCard.left + 18, g_mutedUtilitiesCard.top + 196, g_mutedUtilitiesCard.right - g_mutedUtilitiesCard.left - 36, 18), mutedDetail, mutedVoice.detail.empty() ? kGuiMuted : kGuiText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                DrawTextLine(memDC, MakeRectWH(g_mutedUtilitiesCard.left + 18, g_mutedUtilitiesCard.top + 216, 180, 16), "Microsoft sign-in code", kGuiMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

                SelectObject(memDC, sectionFont);
                std::string authCodeText = mutedVoice.authCode.empty() ? "No Code" : mutedVoice.authCode;
                DrawButtonChip(memDC, g_mutedVoiceAuthCodeRect, authCodeText, !mutedVoice.authCode.empty(), true);
                DrawButtonChip(memDC, g_mutedVoiceCopyCodeButtonRect, "Copy", !mutedVoice.authCode.empty(), false);

                SelectObject(memDC, metaFont);
                std::string authMessage = mutedVoice.authMessage.empty()
                    ? (mutedVoice.authUrl.empty() ? "No sign-in link" : mutedVoice.authUrl)
                    : mutedVoice.authMessage;
                DrawTextLine(memDC, MakeRectWH(g_mutedUtilitiesCard.left + 18, g_mutedUtilitiesCard.top + 280, g_mutedUtilitiesCard.right - g_mutedUtilitiesCard.left - 36, 18), authMessage, mutedVoice.authUrl.empty() ? kGuiMuted : kGuiText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                if (!mutedVoice.authUrl.empty()) {
                    DrawTextLine(memDC, MakeRectWH(g_mutedUtilitiesCard.left + 18, g_mutedUtilitiesCard.top + 296, g_mutedUtilitiesCard.right - g_mutedUtilitiesCard.left - 36, 16), mutedVoice.authUrl, kGuiMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                }

                SelectObject(memDC, bodyFont);
                DrawButtonChip(memDC, g_mutedVoiceOpenAuthButtonRect, "Open Link", !mutedVoice.authUrl.empty(), false);
                DrawButtonChip(memDC, g_mutedVoiceSignOutButtonRect, mutedVoice.signOutInProgress ? "Signing Out" : "Sign Out", mutedVoice.signOutInProgress, false);

                RestoreDC(memDC, savedDc);
            }

            DrawSocialFooterButton(memDC, g_footerYouTubeRect, true);
            DrawSocialFooterButton(memDC, g_footerDiscordRect, false);

            if (g_guiBindCapture != GUI_BIND_NONE) {
                SelectObject(memDC, metaFont);
                DrawTextLine(memDC, MakeRectWH(18, rc.bottom - 24, rc.right - 36, 16), "Press Esc to cancel or Backspace to clear the bind.", kGuiMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            }

            RestoreDC(memDC, mainSavedDc);
            DrawMainPageScrollbar(memDC, rc);

            if (!snaplookAvailable) {
                POINT cursor = {};
                if (GetCursorPos(&cursor)) {
                    ScreenToClient(hwnd, &cursor);
                    RECT visibleSnaplookCard = g_perspectiveCard;
                    visibleSnaplookCard.top = max(visibleSnaplookCard.top, kGuiWindowHeaderHeight + 1);
                    visibleSnaplookCard.bottom = min(visibleSnaplookCard.bottom, rc.bottom);
                    if (visibleSnaplookCard.bottom > visibleSnaplookCard.top &&
                        PointInRectEx(visibleSnaplookCard, cursor.x, cursor.y)) {
                        SelectObject(memDC, metaFont);
                        DrawGuiTooltip(memDC, rc, cursor, "This mod already exists in Lunar Client.");
                    }
                }
            }
        }

        SelectObject(memDC, oldFont);
        DeleteObject(metaFont);
        DeleteObject(bodyFont);
        DeleteObject(sectionFont);
        DeleteObject(titleFont);

        BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDC);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        RECT rc; GetClientRect(hwnd, &rc);
        LayoutGuiControls(rc.right, rc.bottom);

        int x = LOWORD(lParam);
        int y = HIWORD(lParam);
        SetFocus(hwnd);

        if (PointInRectEx(g_headerCloseRect, x, y)) {
            PostMessageA(hwnd, WM_CLOSE, 0, 0);
            return 0;
        }
        if (PointInRectEx(g_headerMinimizeRect, x, y)) {
            ShowWindow(hwnd, SW_MINIMIZE);
            return 0;
        }
        if (PointInRectEx(g_headerThemeRect, x, y)) {
            if (g_guiCurrentPage == GUI_PAGE_THEME_PICKER) CloseThemePicker();
            else OpenThemePicker();
            RequestGuiRepaint();
            return 0;
        }

        if (g_guiCurrentPage == GUI_PAGE_SOUND_PICKER) {
            if (PointInRectEx(g_soundPickerBackRect, x, y)) {
                CancelGuiInteractions();
                CloseSoundPicker();
                RequestGuiRepaint();
                return 0;
            }
            if (PointInRectEx(g_soundPickerSearchRect, x, y)) {
                g_guiSoundSearchEditing = true;
                SetFocus(hwnd);
                RequestGuiRepaint();
                return 0;
            }

            if (PointInRectEx(g_soundPickerListRect, x, y)) {
                std::vector<int> filteredSounds = GetFilteredSoundIds();
                for (int row = 0; row < (int)filteredSounds.size(); ++row) {
                    RECT rowRect = {};
                    if (!GetSoundPickerRowRect(row, (int)filteredSounds.size(), rowRect)) continue;
                    if (!PointInRectEx(rowRect, x, y)) continue;

                    CancelGuiInteractions();
                    SetEditedSoundValue(filteredSounds[row]);
                    SaveToolSettings();
                    CloseSoundPicker();
                    RequestGuiRepaint();
                    return 0;
                }
            }

            return 0;
        }

        if (g_guiCurrentPage == GUI_PAGE_THEME_PICKER) {
            if (PointInRectEx(g_themePickerBackRect, x, y)) {
                CloseThemePicker();
                RequestGuiRepaint();
                return 0;
            }
            for (int theme = 0; theme < GUI_THEME_COUNT; ++theme) {
                if (!PointInRectEx(g_themeOptionRects[theme], x, y)) continue;
                if (NormalizeGuiTheme(g_guiTheme) != theme) {
                    g_guiTheme = theme;
                    ReleaseCosmicBackgroundCache();
                    SaveToolSettings();
                }
                RequestGuiRepaint();
                return 0;
            }
            return 0;
        }

        if (g_guiCurrentPage == GUI_PAGE_NUMBER_COLOUR_PICKER) {
            if (PointInRectEx(g_numberColourPickerBackRect, x, y)) {
                CloseNumberColourPicker();
                RequestGuiRepaint();
                return 0;
            }
            if (PointInRectEx(g_numberColourRgbModeRect, x, y)) {
                CancelGuiInteractions();
                g_guiNumberColourMode = GUI_NUMBER_COLOUR_RGB;
                RequestGuiRepaint();
                return 0;
            }
            if (PointInRectEx(g_numberColourMinecraftModeRect, x, y)) {
                CancelGuiInteractions();
                g_guiNumberColourMode = GUI_NUMBER_COLOUR_MINECRAFT;
                RequestGuiRepaint();
                return 0;
            }

            int number = GetNumberColourAtPoint(x, y);
            if (number >= kTimerNumberMin) {
                CancelGuiInteractions();
                LoadGuiNumberColour(number);
                SetGuiNumberColourSelectionRange(number, number);
                g_guiNumberColourSelectionDragging = true;
                SetCapture(hwnd);
                RequestGuiRepaint();
                return 0;
            }

            if (g_guiNumberColourMode == GUI_NUMBER_COLOUR_MINECRAFT) {
                for (int option = 0; option < (int)kMinecraftColourOptions.size(); ++option) {
                    if (!PointInRectEx(g_minecraftColourOptionRects[option], x, y)) continue;
                    ApplyMinecraftColourToSelection(option);
                    RequestGuiRepaint();
                    return 0;
                }
                return 0;
            }

            GuiSliderTarget colourTarget = GUI_SLIDER_NONE;
            if (PointInRectEx(g_numberColourRedHitRect, x, y)) colourTarget = GUI_SLIDER_TIMER_COLOUR_RED;
            else if (PointInRectEx(g_numberColourGreenHitRect, x, y)) colourTarget = GUI_SLIDER_TIMER_COLOUR_GREEN;
            else if (PointInRectEx(g_numberColourBlueHitRect, x, y)) colourTarget = GUI_SLIDER_TIMER_COLOUR_BLUE;
            if (colourTarget != GUI_SLIDER_NONE) {
                CancelGuiInteractions();
                g_guiSliderTarget = colourTarget;
                g_guiSliderDirty = true;
                SetCapture(hwnd);
                SetTimerColourChannelFromMouseX(colourTarget, x);
                RequestGuiRepaint();
                return 0;
            }
            return 0;
        }

        if (PointInRectEx(g_footerYouTubeRect, x, y)) {
            OpenGuiSocialLink("YouTube", kGuiYouTubeUrl);
            return 0;
        }
        if (PointInRectEx(g_footerDiscordRect, x, y)) {
            OpenGuiSocialLink("Discord", kGuiDiscordUrl);
            return 0;
        }

        bool clickedPartyOwnerField = PointInVisibleCardRect(g_mutedUtilitiesCard, g_mutedVoicePartyOwnerFieldRect, x, y);
        if (!clickedPartyOwnerField) g_guiMutedVoicePartyOwnerEditing = false;

        if (clickedPartyOwnerField) {
            g_guiBindCapture = GUI_BIND_NONE;
            g_guiSliderTarget = GUI_SLIDER_NONE;
            g_guiSliderDirty = false;
            g_guiMutedVoicePartyOwnerEditing = true;
            RequestGuiRepaint();
            return 0;
        }

        if (PointInVisibleCardRect(g_perspectiveCard, g_perspectiveExpandRect, x, y)) {
            g_guiBindCapture = GUI_BIND_NONE;
            g_perspectiveCardAnim.expanded = !g_perspectiveCardAnim.expanded;
            RequestGuiRepaint();
            return 0;
        }
        if (PointInVisibleCardRect(g_timerCard, g_timerExpandRect, x, y)) {
            g_guiBindCapture = GUI_BIND_NONE;
            g_timerCardAnim.expanded = !g_timerCardAnim.expanded;
            RequestGuiRepaint();
            return 0;
        }
        if (PointInVisibleCardRect(g_speedSlownessCard, g_speedSlownessExpandRect, x, y)) {
            g_guiBindCapture = GUI_BIND_NONE;
            g_speedSlownessCardAnim.expanded = !g_speedSlownessCardAnim.expanded;
            RequestGuiRepaint();
            return 0;
        }
        if (PointInVisibleCardRect(g_publicHelpersCard, g_publicHelpersExpandRect, x, y)) {
            g_guiBindCapture = GUI_BIND_NONE;
            g_publicHelpersCardAnim.expanded = !g_publicHelpersCardAnim.expanded;
            RequestGuiRepaint();
            return 0;
        }
        if (PointInVisibleCardRect(g_mutedUtilitiesCard, g_mutedUtilitiesExpandRect, x, y)) {
            g_guiBindCapture = GUI_BIND_NONE;
            g_mutedUtilitiesCardAnim.expanded = !g_mutedUtilitiesCardAnim.expanded;
            RequestGuiRepaint();
            return 0;
        }
        if (PointInVisibleCardRect(g_extrasCard, g_extrasExpandRect, x, y)) {
            g_guiBindCapture = GUI_BIND_NONE;
            g_extrasCardAnim.expanded = !g_extrasCardAnim.expanded;
            RequestGuiRepaint();
            return 0;
        }

        if (PointInVisibleCardRect(g_perspectiveCard, g_perspectiveToggleRect, x, y)) {
            g_guiBindCapture = GUI_BIND_NONE;
            if (IsLunarNamedClient()) {
                RequestGuiRepaint();
                return 0;
            }
            SetPerspectiveModuleEnabled(!IsPerspectiveModuleEnabled());
            SaveToolSettings();
            RequestGuiRepaint();
            return 0;
        }
        if (PointInVisibleCardRect(g_timerCard, g_timerShowToggleRect, x, y)) {
            g_guiBindCapture = GUI_BIND_NONE;
            g_guiTimerEnabled = !g_guiTimerEnabled;
            SaveToolSettings();
            RequestGuiRepaint();
            return 0;
        }
        if (PointInVisibleCardRect(g_speedSlownessCard, g_speedSlownessToggleRect, x, y)) {
            g_guiBindCapture = GUI_BIND_NONE;
            g_guiSpeedSlownessEnabled = !g_guiSpeedSlownessEnabled;
            SaveToolSettings();
            RequestGuiRepaint();
            return 0;
        }
        if (PointInVisibleCardRect(g_publicHelpersCard, g_publicWinsToggleRect, x, y)) {
            g_guiBindCapture = GUI_BIND_NONE;
            SetPublicWinsEnabled(!g_guiPublicWinsEnabled);
            SaveToolSettings();
            RequestGuiRepaint();
            return 0;
        }
        if (PointInVisibleCardRect(g_publicHelpersCard, g_publicWinsPrefixRect, x, y)) {
            g_guiBindCapture = GUI_BIND_NONE;
            g_guiPublicWinsPosition = PUBLIC_WINS_POSITION_PREFIX;
            SaveToolSettings();
            RequestGuiRepaint();
            return 0;
        }
        if (PointInVisibleCardRect(g_publicHelpersCard, g_publicWinsSuffixRect, x, y)) {
            g_guiBindCapture = GUI_BIND_NONE;
            g_guiPublicWinsPosition = PUBLIC_WINS_POSITION_SUFFIX;
            SaveToolSettings();
            RequestGuiRepaint();
            return 0;
        }
        if (PointInVisibleCardRect(g_publicHelpersCard, g_publicWinsSpaceRect, x, y)) {
            g_guiBindCapture = GUI_BIND_NONE;
            g_guiPublicWinsSpaceBetweenUsername = !g_guiPublicWinsSpaceBetweenUsername;
            SaveToolSettings();
            RequestGuiRepaint();
            return 0;
        }
        if (PointInVisibleCardRect(g_extrasCard, g_extrasWheatToggleRect, x, y)) {
            g_guiBindCapture = GUI_BIND_NONE;
            g_guiExtrasForceWheatStage1 = !g_guiExtrasForceWheatStage1;
            DebugLog("GUI extras wheat toggled=%d", g_guiExtrasForceWheatStage1 ? 1 : 0);
            SaveToolSettings();
            RequestGuiRepaint();
            return 0;
        }
        if (PointInVisibleCardRect(g_extrasCard, g_extrasBeaconToggleRect, x, y)) {
            g_guiBindCapture = GUI_BIND_NONE;
            g_guiExtrasHideBeaconBeams = !g_guiExtrasHideBeaconBeams;
            DebugLog("GUI extras beacon toggled=%d", g_guiExtrasHideBeaconBeams ? 1 : 0);
            SaveToolSettings();
            RequestGuiRepaint();
            return 0;
        }
        if (PointInVisibleCardRect(g_extrasCard, g_extrasScoreboardToggleRect, x, y)) {
            g_guiBindCapture = GUI_BIND_NONE;
            g_guiExtrasDisableTagScoreboard = !g_guiExtrasDisableTagScoreboard;
            DebugLog("GUI extras disable_tag_scoreboard toggled=%d", g_guiExtrasDisableTagScoreboard ? 1 : 0);
            SaveToolSettings();
            RequestGuiRepaint();
            return 0;
        }
        if (PointInVisibleCardRect(g_mutedUtilitiesCard, g_mutedVoiceToggleRect, x, y)) {
            g_guiBindCapture = GUI_BIND_NONE;
            bool enabled = !g_guiExtrasMutedVoice;
            DebugLog("GUI muted utilities muted_voice toggled=%d", enabled ? 1 : 0);
            SetMutedVoiceModuleEnabled(enabled);
            SaveToolSettings();
            RequestGuiRepaint();
            return 0;
        }
        if (PointInVisibleCardRect(g_mutedUtilitiesCard, g_mutedVoiceHideMuteReminderToggleRect, x, y)) {
            g_guiBindCapture = GUI_BIND_NONE;
            if (g_guiExtrasMutedVoice) {
                g_guiExtrasMutedVoiceHideMuteReminder = !g_guiExtrasMutedVoiceHideMuteReminder;
                DebugLog("GUI muted utilities muted_voice_hide_mute_reminder toggled=%d", g_guiExtrasMutedVoiceHideMuteReminder ? 1 : 0);
                SaveToolSettings();
            }
            RequestGuiRepaint();
            return 0;
        }
        if (PointInVisibleCardRect(g_mutedUtilitiesCard, g_mutedVoiceOpenAuthButtonRect, x, y)) {
            g_guiBindCapture = GUI_BIND_NONE;
            OpenMutedVoiceAuthUrl();
            RequestGuiRepaint();
            return 0;
        }
        if (PointInVisibleCardRect(g_mutedUtilitiesCard, g_mutedVoiceCopyCodeButtonRect, x, y)) {
            g_guiBindCapture = GUI_BIND_NONE;
            CopyMutedVoiceAuthCode();
            return 0;
        }
        if (PointInVisibleCardRect(g_mutedUtilitiesCard, g_mutedVoiceSignOutButtonRect, x, y)) {
            g_guiBindCapture = GUI_BIND_NONE;
            BeginMutedVoiceSignOut();
            RequestGuiRepaint();
            return 0;
        }
        if (PointInVisibleCardRect(g_perspectiveCard, g_perspectiveBindRect, x, y)) {
            g_guiBindCapture = (g_guiBindCapture == GUI_BIND_PERSPECTIVE) ? GUI_BIND_NONE : GUI_BIND_PERSPECTIVE;
            RequestGuiRepaint();
            return 0;
        }
        if (PointInVisibleCardRect(g_perspectiveCard, g_perspectiveBackRect, x, y)) {
            g_guiBindCapture = GUI_BIND_NONE;
            g_snaplookCameraMode = kCameraBack;
            SaveToolSettings();
            RequestGuiRepaint();
            return 0;
        }
        if (PointInVisibleCardRect(g_perspectiveCard, g_perspectiveFrontRect, x, y)) {
            g_guiBindCapture = GUI_BIND_NONE;
            g_snaplookCameraMode = kCameraFront;
            SaveToolSettings();
            RequestGuiRepaint();
            return 0;
        }

        if (PointInVisibleCardRect(g_timerCard, g_timerNametagToggleRect, x, y)) {
            g_guiTimerNametagEnabled = !g_guiTimerNametagEnabled;
            SaveToolSettings();
            RequestGuiRepaint();
            return 0;
        }
        if (PointInVisibleCardRect(g_timerCard, g_timerNametagPrefixRect, x, y)) {
            g_guiTimerNametagPosition = TIMER_NAMETAG_POSITION_PREFIX;
            SaveToolSettings();
            RequestGuiRepaint();
            return 0;
        }
        if (PointInVisibleCardRect(g_timerCard, g_timerNametagSuffixRect, x, y)) {
            g_guiTimerNametagPosition = TIMER_NAMETAG_POSITION_SUFFIX;
            SaveToolSettings();
            RequestGuiRepaint();
            return 0;
        }
        if (PointInVisibleCardRect(g_timerCard, g_timerDefaultScoreboardToggleRect, x, y)) {
            if (NormalizeTimerDecimalPlaces(g_guiTimerDecimalPlaces) == 0) {
                g_guiTimerEditDefaultScoreboard = false;
            }
            else {
                g_guiTimerEditDefaultScoreboard = !g_guiTimerEditDefaultScoreboard;
            }
            SaveToolSettings();
            RequestGuiRepaint();
            return 0;
        }
        if (PointInVisibleCardRect(g_timerCard, g_timerCaptureToggleRect, x, y)) {
            g_guiTimerObsScreenshotsEnabled = !g_guiTimerObsScreenshotsEnabled;
            SaveToolSettings();
            RequestGuiRepaint();
            return 0;
        }
        if (PointInVisibleCardRect(g_timerCard, g_timerLockToggleRect, x, y)) {
            g_guiTimerLocked = !g_guiTimerLocked;
            SaveToolSettings();
            RequestGuiRepaint();
            return 0;
        }
        if (PointInVisibleCardRect(g_timerCard, g_timerModeOverlayRect, x, y)) {
            g_guiTimerCrosshairMode = false;
            SaveToolSettings();
            RequestGuiRepaint();
            return 0;
        }
        if (PointInVisibleCardRect(g_timerCard, g_timerModeCrosshairRect, x, y)) {
            g_guiTimerCrosshairMode = true;
            SaveToolSettings();
            RequestGuiRepaint();
            return 0;
        }
        if (PointInVisibleCardRect(g_timerCard, g_timerDecimal0Rect, x, y)) {
            g_guiTimerDecimalPlaces = 0;
            g_guiTimerEditDefaultScoreboard = false;
            SaveToolSettings();
            RequestGuiRepaint();
            return 0;
        }
        if (PointInVisibleCardRect(g_timerCard, g_timerDecimal1Rect, x, y)) {
            g_guiTimerDecimalPlaces = 1;
            SaveToolSettings();
            RequestGuiRepaint();
            return 0;
        }
        if (PointInVisibleCardRect(g_timerCard, g_timerDecimal2Rect, x, y)) {
            g_guiTimerDecimalPlaces = 2;
            SaveToolSettings();
            RequestGuiRepaint();
            return 0;
        }
        if (PointInVisibleCardRect(g_timerCard, g_timerNumberColourButtonRect, x, y)) {
            OpenNumberColourPicker();
            RequestGuiRepaint();
            return 0;
        }

        if (PointInVisibleCardRect(g_speedSlownessCard, g_speed3SoundButtonRect, x, y)) {
            CancelGuiInteractions();
            OpenSoundPicker(GUI_SOUND_FIELD_SPEED3);
            RequestGuiRepaint();
            return 0;
        }
        if (PointInVisibleCardRect(g_speedSlownessCard, g_slownessSoundButtonRect, x, y)) {
            CancelGuiInteractions();
            OpenSoundPicker(GUI_SOUND_FIELD_SLOWNESS);
            RequestGuiRepaint();
            return 0;
        }

        if (PointInVisibleCardRect(g_timerCard, g_timerScaleHitRect, x, y)) {
            g_guiBindCapture = GUI_BIND_NONE;
            g_guiSliderTarget = GUI_SLIDER_TIMER_SCALE;
            g_guiSliderDirty = true;
            SetCapture(hwnd);
            SetTimerScaleFromMouseX(x);
            RequestGuiRepaint();
            return 0;
        }
        if (PointInVisibleCardRect(g_speedSlownessCard, g_speed3VolumeHitRect, x, y)) {
            g_guiBindCapture = GUI_BIND_NONE;
            g_guiSliderTarget = GUI_SLIDER_SPEED3_VOLUME;
            g_guiSliderDirty = true;
            SetCapture(hwnd);
            SetSpeed3VolumeFromMouseX(x);
            RequestGuiRepaint();
            return 0;
        }
        if (PointInVisibleCardRect(g_speedSlownessCard, g_slownessVolumeHitRect, x, y)) {
            g_guiBindCapture = GUI_BIND_NONE;
            g_guiSliderTarget = GUI_SLIDER_SLOWNESS_VOLUME;
            g_guiSliderDirty = true;
            SetCapture(hwnd);
            SetSlownessVolumeFromMouseX(x);
            RequestGuiRepaint();
            return 0;
        }
        break;
    }

    case WM_MOUSEMOVE: {
        if (IsLunarNamedClient() && g_guiCurrentPage == GUI_PAGE_MAIN) {
            TRACKMOUSEEVENT tracking = { sizeof(tracking), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tracking);
            RequestGuiRepaint();
        }
        if (g_guiNumberColourSelectionDragging) {
            RECT rc; GetClientRect(hwnd, &rc);
            LayoutGuiControls(rc.right, rc.bottom);
            int number = GetNumberColourAtPoint(LOWORD(lParam), HIWORD(lParam));
            if (number >= kTimerNumberMin && number != g_guiNumberColourSelectionEnd) {
                SetGuiNumberColourSelectionRange(g_guiNumberColourSelectionAnchor, number);
                RequestGuiRepaint();
            }
            return 0;
        }
        if (g_guiSliderTarget == GUI_SLIDER_NONE) break;

        RECT rc; GetClientRect(hwnd, &rc);
        LayoutGuiControls(rc.right, rc.bottom);

        if (g_guiSliderTarget == GUI_SLIDER_TIMER_SCALE) {
            SetTimerScaleFromMouseX(LOWORD(lParam));
            RequestGuiRepaint();
            return 0;
        }
        if (g_guiSliderTarget == GUI_SLIDER_TIMER_COLOUR_RED ||
            g_guiSliderTarget == GUI_SLIDER_TIMER_COLOUR_GREEN ||
            g_guiSliderTarget == GUI_SLIDER_TIMER_COLOUR_BLUE) {
            SetTimerColourChannelFromMouseX(g_guiSliderTarget, LOWORD(lParam));
            RequestGuiRepaint();
            return 0;
        }
        if (g_guiSliderTarget == GUI_SLIDER_SPEED3_VOLUME) {
            SetSpeed3VolumeFromMouseX(LOWORD(lParam));
            RequestGuiRepaint();
            return 0;
        }
        if (g_guiSliderTarget == GUI_SLIDER_SLOWNESS_VOLUME) {
            SetSlownessVolumeFromMouseX(LOWORD(lParam));
            RequestGuiRepaint();
            return 0;
        }
        break;
    }

    case WM_MOUSELEAVE:
        if (IsLunarNamedClient() && g_guiCurrentPage == GUI_PAGE_MAIN) {
            RequestGuiRepaint();
        }
        return 0;

    case WM_MOUSEWHEEL:
        if (g_guiCurrentPage == GUI_PAGE_SOUND_PICKER) {
            ScrollSoundPicker(GET_WHEEL_DELTA_WPARAM(wParam));
            RequestGuiRepaint();
            return 0;
        }
        if (g_guiCurrentPage == GUI_PAGE_THEME_PICKER) return 0;
        if (g_guiCurrentPage == GUI_PAGE_NUMBER_COLOUR_PICKER) return 0;
        ScrollMainPage(GET_WHEEL_DELTA_WPARAM(wParam));
        RequestGuiRepaint();
        return 0;
        break;

    case WM_LBUTTONUP:
        if (g_guiNumberColourSelectionDragging) {
            g_guiNumberColourSelectionDragging = false;
            ReleaseCapture();
            RequestGuiRepaint();
            return 0;
        }
        if (g_guiSliderTarget != GUI_SLIDER_NONE) {
            GuiSliderTarget releasedTarget = g_guiSliderTarget;
            g_guiSliderTarget = GUI_SLIDER_NONE;
            ReleaseCapture();
            if (g_guiSliderDirty) {
                if (releasedTarget == GUI_SLIDER_TIMER_SCALE) g_config.Save(kTimerOverlayConfigPath);
                if (releasedTarget == GUI_SLIDER_TIMER_COLOUR_RED ||
                    releasedTarget == GUI_SLIDER_TIMER_COLOUR_GREEN ||
                    releasedTarget == GUI_SLIDER_TIMER_COLOUR_BLUE) SaveToolSettings();
                if (releasedTarget == GUI_SLIDER_SPEED3_VOLUME || releasedTarget == GUI_SLIDER_SLOWNESS_VOLUME) SaveToolSettings();
                g_guiSliderDirty = false;
            }
            RequestGuiRepaint();
            return 0;
        }
        break;

    case WM_CHAR:
        if (g_guiCurrentPage == GUI_PAGE_SOUND_PICKER && g_guiSoundSearchEditing) {
            char ch = (char)wParam;
            if (ch >= 32 && ch <= 126 && g_guiSoundSearch.size() < 64) {
                g_guiSoundSearch.push_back(ch);
                g_guiSoundPickerScroll = 0;
                ClampSoundPickerScroll();
                RequestGuiRepaint();
            }
            return 0;
        }
        if (g_guiMutedVoicePartyOwnerEditing) {
            char ch = (char)wParam;
            if (IsMutedVoicePartyOwnerChar(ch) && g_guiMutedVoicePartyOwner.size() < 16) {
                std::string nextValue = g_guiMutedVoicePartyOwner;
                nextValue.push_back(ch);
                SetMutedVoicePartyOwnerFromGui(nextValue);
                RequestGuiRepaint();
            }
            return 0;
        }
        break;

    case WM_SYSCHAR:
        if (g_guiBindCapture != GUI_BIND_NONE || g_guiMutedVoicePartyOwnerEditing ||
            (g_guiCurrentPage == GUI_PAGE_SOUND_PICKER && g_guiSoundSearchEditing)) return 0;
        break;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (g_guiCurrentPage == GUI_PAGE_SOUND_PICKER && g_guiSoundSearchEditing) {
            if (wParam == VK_BACK) {
                if (!g_guiSoundSearch.empty()) g_guiSoundSearch.pop_back();
                g_guiSoundPickerScroll = 0;
                ClampSoundPickerScroll();
                RequestGuiRepaint();
                return 0;
            }
            if (wParam == VK_DELETE) {
                g_guiSoundSearch.clear();
                g_guiSoundPickerScroll = 0;
                ClampSoundPickerScroll();
                RequestGuiRepaint();
                return 0;
            }
            if (wParam == VK_ESCAPE) {
                if (!g_guiSoundSearch.empty()) {
                    g_guiSoundSearch.clear();
                    g_guiSoundPickerScroll = 0;
                    ClampSoundPickerScroll();
                }
                else {
                    CloseSoundPicker();
                }
                RequestGuiRepaint();
                return 0;
            }
            if (wParam == VK_RETURN) {
                std::vector<int> filteredSounds = GetFilteredSoundIds();
                if (filteredSounds.size() == 1) {
                    SetEditedSoundValue(filteredSounds[0]);
                    SaveToolSettings();
                    CloseSoundPicker();
                }
                RequestGuiRepaint();
                return 0;
            }
        }
        if (g_guiMutedVoicePartyOwnerEditing) {
            if (wParam == VK_ESCAPE || wParam == VK_RETURN) {
                g_guiMutedVoicePartyOwnerEditing = false;
                RequestGuiRepaint();
                return 0;
            }
            if (wParam == VK_BACK) {
                if (!g_guiMutedVoicePartyOwner.empty()) {
                    std::string nextValue = g_guiMutedVoicePartyOwner;
                    nextValue.pop_back();
                    SetMutedVoicePartyOwnerFromGui(nextValue);
                    RequestGuiRepaint();
                }
                return 0;
            }
            if (wParam == VK_DELETE) {
                if (!g_guiMutedVoicePartyOwner.empty()) {
                    SetMutedVoicePartyOwnerFromGui("");
                    RequestGuiRepaint();
                }
                return 0;
            }
            return 0;
        }
        if (g_guiCurrentPage == GUI_PAGE_SOUND_PICKER && g_guiBindCapture == GUI_BIND_NONE && wParam == VK_ESCAPE) {
            CloseSoundPicker();
            RequestGuiRepaint();
            return 0;
        }
        if (g_guiCurrentPage == GUI_PAGE_NUMBER_COLOUR_PICKER && g_guiBindCapture == GUI_BIND_NONE && wParam == VK_ESCAPE) {
            CloseNumberColourPicker();
            RequestGuiRepaint();
            return 0;
        }
        if (g_guiCurrentPage == GUI_PAGE_THEME_PICKER && g_guiBindCapture == GUI_BIND_NONE && wParam == VK_ESCAPE) {
            CloseThemePicker();
            RequestGuiRepaint();
            return 0;
        }
        if (g_guiBindCapture != GUI_BIND_NONE) {
            if (wParam == VK_ESCAPE) {
                g_guiBindCapture = GUI_BIND_NONE;
            }
            else {
                int captured = (wParam == VK_BACK || wParam == VK_DELETE) ? 0 : NormalizeKeybind((int)wParam);
                if (g_guiBindCapture == GUI_BIND_PERSPECTIVE) g_snaplookKeybind = captured;
                g_guiBindCapture = GUI_BIND_NONE;
                SaveToolSettings();
            }
            RequestGuiRepaint();
            return 0;
        }
        break;

    case WM_CLOSE:
        if (InterlockedCompareExchange(&g_shutdownRequested, 0, 0) == 0) {
            RequestModuleUnload("gui_close");
        }
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        if (InterlockedCompareExchange(&g_shutdownRequested, 0, 0) == 0) {
            RequestModuleUnload("gui_destroy");
        }
        KillTimer(hwnd, kGuiAnimationTimerId);
        g_guiHwnd = nullptr;
        g_guiLastAnimationTickMs = 0;
        g_guiLastCosmicRepaintMs = 0;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

DWORD WINAPI GuiThread(LPVOID) {
    HINSTANCE guiModuleHandle = g_moduleHandle ? g_moduleHandle : GetModuleHandleA(NULL);
    HICON largeIcon = static_cast<HICON>(LoadImageA(
        guiModuleHandle,
        MAKEINTRESOURCEA(IDI_APPICON),
        IMAGE_ICON,
        GetSystemMetrics(SM_CXICON),
        GetSystemMetrics(SM_CYICON),
        LR_DEFAULTCOLOR | LR_SHARED));
    HICON smallIcon = static_cast<HICON>(LoadImageA(
        guiModuleHandle,
        MAKEINTRESOURCEA(IDI_APPICON),
        IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON),
        GetSystemMetrics(SM_CYSMICON),
        LR_DEFAULTCOLOR | LR_SHARED));

    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = GuiWndProc;
    wc.hInstance = guiModuleHandle;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = largeIcon;
    wc.hIconSm = smallIcon ? smallIcon : largeIcon;
    wc.lpszClassName = "TagEssentialsGuiWnd";
    RegisterClassExA(&wc);

    HRESULT coInitHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool shouldUninitializeCom = SUCCEEDED(coInitHr);
    InitializeCosmicImageAssets();

    DWORD exStyle = WS_EX_APPWINDOW;
    DWORD style = WS_POPUP | WS_VISIBLE;
    const int windowW = 620;
    const int windowH = 900;

    g_guiHwnd = CreateWindowExA(exStyle, "TagEssentialsGuiWnd", "TagEssentials",
        style, CW_USEDEFAULT, CW_USEDEFAULT,
        windowW, windowH,
        NULL, NULL, wc.hInstance, NULL);

    if (!g_guiHwnd) {
        ReleaseCosmicImageAssets();
        if (shouldUninitializeCom) CoUninitialize();
        return 0;
    }

    if (wc.hIcon) {
        SendMessageA(g_guiHwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(wc.hIcon));
    }
    if (wc.hIconSm) {
        SendMessageA(g_guiHwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(wc.hIconSm));
    }

    SetWindowPos(g_guiHwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    SetOverlayWindowAppId(g_guiHwnd);

    g_guiLastAnimationTickMs = GetTickCount64();
    g_guiLastCosmicRepaintMs = 0;
    SetTimer(g_guiHwnd, kGuiAnimationTimerId, kGuiAnimationIntervalMs, NULL);

    ShowWindow(g_guiHwnd, SW_SHOW);
    UpdateWindow(g_guiHwnd);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    ReleaseCosmicImageAssets();
    if (shouldUninitializeCom) CoUninitialize();
    return 0;
}

// =============================================================
// Main thread
// =============================================================
bool ConsumeLunarCompatibilitySmokeTestMarker() {
    if (!IsLunarNamedClient()) return false;

    wchar_t tempDirectory[MAX_PATH] = {};
    DWORD length = GetTempPathW(MAX_PATH, tempDirectory);
    if (length == 0 || length >= MAX_PATH) return false;

    std::wstring markerPath(tempDirectory, length);
    markerPath += L"TagEssentials_lunar_smoketest.flag";
    if (GetFileAttributesW(markerPath.c_str()) == INVALID_FILE_ATTRIBUTES) return false;
    DeleteFileW(markerPath.c_str());
    return true;
}

bool ConsumeLunarNametagTimerTestMarker() {
    if (!IsLunarNamedClient()) return false;

    wchar_t tempDirectory[MAX_PATH] = {};
    DWORD length = GetTempPathW(MAX_PATH, tempDirectory);
    if (length == 0 || length >= MAX_PATH) return false;

    std::wstring markerPath(tempDirectory, length);
    markerPath += L"TagEssentials_lunar_nametag_timer_test.flag";
    if (GetFileAttributesW(markerPath.c_str()) == INVALID_FILE_ATTRIBUTES) return false;
    DeleteFileW(markerPath.c_str());
    return true;
}

void RunLunarCompatibilitySmokeTest() {
    if (!ConsumeLunarCompatibilitySmokeTestMarker()) return;

    DebugLog("Lunar compatibility smoke test begin");
    const bool primaryJniReady =
        g_slInited && !g_slFailed &&
        g_speedSlownessJNI.inited && !g_speedSlownessJNI.failed &&
        g_tntVisualJNI.inited && !g_tntVisualJNI.failed;
    const bool publicTabReady = EnsurePublicWinsTabNameHook(g_env);
    const bool publicScoreReady = EnsurePublicWinsScoreboardFormatHook(g_env);
    const bool publicRenderedNameReady = EnsurePublicWinsRenderedNameHook(g_env);
    const bool mutedVoicePacketReady = InitMutedVoicePacketFilter(g_env);

    DebugLog(
        "Lunar compatibility smoke test result primaryJni=%d publicTab=%d publicScore=%d publicRenderedName=%d mutedVoicePacket=%d all=%d",
        primaryJniReady ? 1 : 0,
        publicTabReady ? 1 : 0,
        publicScoreReady ? 1 : 0,
        publicRenderedNameReady ? 1 : 0,
        mutedVoicePacketReady ? 1 : 0,
        primaryJniReady && publicTabReady && publicScoreReady &&
            publicRenderedNameReady && mutedVoicePacketReady ? 1 : 0);

    RestorePublicWinsTabNameHook(g_env);
    ShutdownMutedVoicePacketFilter();
    DebugLog("Lunar compatibility smoke test cleanup complete");
}

bool LogLunarNametagTimerSmokeSample(const char* label) {
    if (!g_env || !g_publicWinsRenderedComponentHelperClass) return false;

    jobject mc = GetMinecraftClientForAlerts();
    jobject player = mc && g_speedSlownessJNI.fThePlayer
        ? g_env->GetObjectField(mc, g_speedSlownessJNI.fThePlayer)
        : nullptr;
    if (mc) g_env->DeleteLocalRef(mc);
    if (!player) {
        if (g_env->ExceptionCheck()) g_env->ExceptionClear();
        DebugLog("Lunar nametag timer smoke sample %s failed: local player unavailable", label);
        return false;
    }

    bool adventureReady = false;
    if (g_lunarAdventureNameHelperClass) {
        jmethodID adventureMethod = GetStaticMethodIDCompat(g_env,
            g_lunarAdventureNameHelperClass,
            "f",
            "(Lnet/minecraft/entity/EntityLivingBase;)Lnet/kyori/adventure/text/Component;");
        jobject adventureComponent = adventureMethod
            ? g_env->CallStaticObjectMethod(
                g_lunarAdventureNameHelperClass, adventureMethod, player)
            : nullptr;
        if (adventureComponent && !g_env->ExceptionCheck()) {
            adventureReady = true;
            g_env->DeleteLocalRef(adventureComponent);
            DebugLog("Lunar nametag timer smoke sample %s Adventure helper succeeded", label);
        }
        else {
            if (g_env->ExceptionCheck()) g_env->ExceptionClear();
            if (adventureComponent) g_env->DeleteLocalRef(adventureComponent);
            DebugLog("Lunar nametag timer smoke sample %s Adventure helper failed", label);
        }
    }

    jclass entityClass = FindClassLoose(g_env, "pr");
    jmethodID getDisplayName = entityClass
        ? GetMethodIDCompat(g_env, entityClass, "f_", "()Leu;")
        : nullptr;
    if (entityClass) g_env->DeleteLocalRef(entityClass);
    if (!getDisplayName || g_env->ExceptionCheck()) {
        if (g_env->ExceptionCheck()) g_env->ExceptionClear();
        g_env->DeleteLocalRef(player);
        DebugLog("Lunar nametag timer smoke sample %s failed: getDisplayName unavailable", label);
        return false;
    }

    const std::string helperDescriptor =
        TranslateLunarDescriptor("(Lpr;Leu;)Ljava/lang/String;");
    jmethodID helperMethod = GetStaticMethodIDCompat(g_env,
        g_publicWinsRenderedComponentHelperClass, "f", helperDescriptor.c_str());
    jobject component = g_env->CallObjectMethod(player, getDisplayName);
    if (!helperMethod || !component || g_env->ExceptionCheck()) {
        if (g_env->ExceptionCheck()) g_env->ExceptionClear();
        if (component) g_env->DeleteLocalRef(component);
        g_env->DeleteLocalRef(player);
        DebugLog("Lunar nametag timer smoke sample %s failed: helper invocation unavailable", label);
        return false;
    }

    jstring rendered = (jstring)g_env->CallStaticObjectMethod(
        g_publicWinsRenderedComponentHelperClass, helperMethod, player, component);
    g_env->DeleteLocalRef(component);
    g_env->DeleteLocalRef(player);
    if (!rendered || g_env->ExceptionCheck()) {
        if (g_env->ExceptionCheck()) g_env->ExceptionClear();
        if (rendered) g_env->DeleteLocalRef(rendered);
        DebugLog("Lunar nametag timer smoke sample %s failed: helper returned no text", label);
        return false;
    }

    std::string text = JStringToUtf8(g_env, rendered);
    g_env->DeleteLocalRef(rendered);
    DebugLog("Lunar nametag timer smoke sample %s text=%s", label, text.c_str());
    return adventureReady;
}

void RunLunarNametagTimerSmokeTest() {
    if (!ConsumeLunarNametagTimerTestMarker()) return;

    bool renderWorldReady = false;
    for (int attempt = 0; attempt < 150 && !renderWorldReady; ++attempt) {
        jobject world = GetWorldObject();
        jobjectArray players = GetNetworkPlayerInfoArray();
        if (world && players && g_env->GetArrayLength(players) > 0) renderWorldReady = true;
        if (players) g_env->DeleteLocalRef(players);
        if (world) g_env->DeleteLocalRef(world);
        if (!renderWorldReady) Sleep(100);
    }
    DebugLog("Lunar nametag timer smoke test render-world-ready=%d", renderWorldReady ? 1 : 0);

    if (!EnsurePublicWinsRenderedNameHook(g_env)) {
        DebugLog("Lunar nametag timer smoke test failed installing render hook");
        return;
    }

    const bool previousNametagEnabled = g_guiTimerNametagEnabled;
    const int previousNametagPosition = g_guiTimerNametagPosition;
    const bool previousTimerActive = g_timerActive;
    const bool previousBetweenRounds = g_betweenRoundsTimerActive;
    const double previousTimerStartSeconds = g_timerStartSeconds;
    const int previousExplosionSeconds = g_explosionSeconds;
    const LARGE_INTEGER previousExplosionSetAt = g_explosionSetAt;

    g_guiTimerNametagEnabled = true;
    g_guiTimerNametagPosition = TIMER_NAMETAG_POSITION_SUFFIX;
    g_betweenRoundsTimerActive = false;
    g_timerActive = true;
    g_timerStartSeconds = 3.0;
    g_explosionSeconds = 3;
    QueryPerformanceCounter(&g_explosionSetAt);
    InterlockedExchange(&g_lunarNametagTimerDispatchCount, 0);
    InterlockedExchange(&g_lunarAdventureNametagDispatchCount, 0);
    DebugLog("Lunar nametag timer smoke test begin start=3.000");
    const bool startSampleReady = LogLunarNametagTimerSmokeSample("start");
    Sleep(2200);
    g_guiTimerNametagPosition = TIMER_NAMETAG_POSITION_PREFIX;
    InterlockedExchange(&g_lunarAdventureNametagDispatchCount, 0);
    const bool endSampleReady = LogLunarNametagTimerSmokeSample("end");
    DebugLog("Lunar nametag timer smoke test end remaining=%.3f", GetDecimalSeconds());

    g_guiTimerNametagEnabled = previousNametagEnabled;
    g_guiTimerNametagPosition = previousNametagPosition;
    g_timerActive = previousTimerActive;
    g_betweenRoundsTimerActive = previousBetweenRounds;
    g_timerStartSeconds = previousTimerStartSeconds;
    g_explosionSeconds = previousExplosionSeconds;
    g_explosionSetAt = previousExplosionSetAt;

    DebugLog("Lunar nametag timer smoke test cleanup complete samples=%d",
        startSampleReady && endSampleReady ? 1 : 0);
}

void ShutdownInjectedJavaCallbacks() {
    if (g_env) {
        jclass helperClasses[] = {
            g_nameTagHelperClass,
            g_publicWinsScoreFormatHelperClass,
            g_publicWinsRenderedNameHelperClass,
            g_publicWinsRenderedComponentHelperClass,
            g_lunarAdventureNameHelperClass,
            g_mutedVoicePacketFilterHelperClass,
        };
        for (jclass helperClass : helperClasses) {
            if (!helperClass) continue;
            g_env->UnregisterNatives(helperClass);
            if (g_env->ExceptionCheck()) g_env->ExceptionClear();
        }
    }

    if (g_jvmti) {
        InterlockedExchange(&g_runtimeClassCaptureEnabled, 0);
        g_jvmti->SetEventNotificationMode(
            JVMTI_DISABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, nullptr);
        g_jvmti->SetEventNotificationMode(
            JVMTI_DISABLE, JVMTI_EVENT_BREAKPOINT, nullptr);
        jvmtiEventCallbacks callbacks = {};
        g_jvmti->SetEventCallbacks(&callbacks, sizeof(callbacks));
        g_sharedClassFileHookInstalled = false;
    }
    DebugLog("Injected Java native bindings and JVMTI callbacks disabled");
}

DWORD WINAPI MainThread(LPVOID lpParam) {
    QueryPerformanceFrequency(&g_perfFreq);
    Sleep(3000);

    ResetDebugLogFile();
    DebugLog("MainThread start module=%p", lpParam);
    g_prevUnhandledExceptionFilter = SetUnhandledExceptionFilter(FullscreenCrashUnhandledExceptionFilter);
    DebugLog("Unhandled exception filter installed previous=%p logPath=%s", g_prevUnhandledExceptionFilter, g_debugLogPath);

    if (MH_Initialize() != MH_OK) return 0;
    DebugLog("MinHook initialized");

    g_config.Load(kTimerOverlayConfigPath);
    g_config.scale = ClampFloat(g_config.scale, kTimerScaleMin, kTimerScaleMax);
    LoadToolSettings();
    SaveToolSettings(); // Rewrite legacy settings without removed feature keys.
    DebugLog("Settings loaded timerCrosshairMode=%d timerEnabled=%d timerObsScreenshots=%d timerDecimals=%d timerEditDefaultScoreboard=%d snaplookEnabled=%d extrasWheat=%d extrasBeacon=%d extrasDisableTagScoreboard=%d extrasMutedVoice=%d extrasMutedVoiceHideMuteReminder=%d scale=%.3f",
        g_guiTimerCrosshairMode ? 1 : 0,
        g_guiTimerEnabled ? 1 : 0,
        g_guiTimerObsScreenshotsEnabled ? 1 : 0,
        g_guiTimerDecimalPlaces,
        g_guiTimerEditDefaultScoreboard ? 1 : 0,
        g_guiSnaplookEnabled ? 1 : 0,
        g_guiExtrasForceWheatStage1 ? 1 : 0,
        g_guiExtrasHideBeaconBeams ? 1 : 0,
        g_guiExtrasDisableTagScoreboard ? 1 : 0,
        g_guiExtrasMutedVoice ? 1 : 0,
        g_guiExtrasMutedVoiceHideMuteReminder ? 1 : 0,
        g_config.scale);

    if (!AttachToJVM()) {
        DebugLog("AttachToJVM failed");
        return 0;
    }
    DebugLog("AttachToJVM succeeded jvm=%p env=%p", g_jvm, g_env);
    InitSnaplookJNI();
    InitSpeedSlownessJNI();
    InitTntVisualJNI();
    InitMutedVoiceChatJNI(g_env);
    if (g_guiPublicWinsEnabled) {
        StartPublicWinsWorker();
    }
    if (g_guiExtrasMutedVoice && g_guiExtrasMutedVoiceHideMuteReminder) {
        InitMutedVoicePacketFilter(g_env);
    }
    DebugLog("JNI init status snaplook(inited=%d failed=%d) speedSlowness(inited=%d failed=%d) tntVisual(inited=%d failed=%d)",
        g_slInited ? 1 : 0,
        g_slFailed ? 1 : 0,
        g_speedSlownessJNI.inited ? 1 : 0,
        g_speedSlownessJNI.failed ? 1 : 0,
        g_tntVisualJNI.inited ? 1 : 0,
        g_tntVisualJNI.failed ? 1 : 0);

    HMODULE hGdi = GetModuleHandleA("gdi32.dll");
    void* pSwap = hGdi ? GetProcAddress(hGdi, "SwapBuffers") : nullptr;
    if (pSwap) MH_CreateHook(pSwap, &hk_SwapBuffers, reinterpret_cast<void**>(&o_SwapBuffers));
    HMODULE hOpenGL = GetModuleHandleA("opengl32.dll");
    void* pGlClear = hOpenGL ? GetProcAddress(hOpenGL, "glClear") : nullptr;
    if (pGlClear) MH_CreateHook(pGlClear, &hk_glClear, reinterpret_cast<void**>(&o_glClear));
    void* pGlOrtho = hOpenGL ? GetProcAddress(hOpenGL, "glOrtho") : nullptr;
    if (pGlOrtho) MH_CreateHook(pGlOrtho, &hk_glOrtho, reinterpret_cast<void**>(&o_glOrtho));
    DebugLog("Hook targets swap=%p clear=%p ortho=%p", pSwap, pGlClear, pGlOrtho);

    MH_EnableHook(MH_ALL_HOOKS);
    DebugLog("Hooks enabled swapOrig=%p clearOrig=%p orthoOrig=%p", o_SwapBuffers, o_glClear, o_glOrtho);
    EnsureWndProcHooked();
    RunLunarCompatibilitySmokeTest();
    RunLunarNametagTimerSmokeTest();

    g_guiThreadHandle = CreateThread(nullptr, 0, GuiThread, nullptr, 0, &g_guiThreadId);
    DebugLog("GUI thread handle=%p id=%lu", g_guiThreadHandle, g_guiThreadId);

    if (g_guiExtrasMutedVoice) {
        WriteMutedVoicePartyOwnerConfig(g_guiMutedVoicePartyOwner);
        StartMutedVoiceWorker();
    }

    int hookCheckCounter = 0;

    while (!IsModuleUnloadRequested()) {
        if (GetAsyncKeyState(VK_END) & 1) {
            RequestModuleUnload("end_key");
            break;
        }
        TryRecoverRuntimeJNI();

        if (GetAsyncKeyState(VK_INSERT) & 1) {
            g_guiTimerEnabled = !g_guiTimerEnabled;
            SaveToolSettings();
            RequestGuiRepaint();
        }

        // If snaplook was disabled mid-activation, restore perspective
        if (!IsPerspectiveModuleEnabled() && g_snaplookActive) {
            SetPerspective(g_snaplookSavedPerspective);
            g_snaplookActive = false;
        }

        // Snaplook: hold Left Alt for front-facing camera
        if (IsPerspectiveModuleEnabled() && g_slInited && !g_slFailed) {
            bool snaplookHeld = IsKeybindDown(g_snaplookKeybind);
            if (snaplookHeld && !g_snaplookActive) {
                int cur = GetPerspective();
                if (cur >= 0) {
                    g_snaplookSavedPerspective = cur;
                    if (cur != g_snaplookCameraMode) SetPerspective(g_snaplookCameraMode);
                    g_snaplookActive = true;
                }
            }
            else if (!snaplookHeld && g_snaplookActive) {
                SetPerspective(g_snaplookSavedPerspective);
                g_snaplookActive = false;
            }
            else if (snaplookHeld && g_snaplookActive) {
                int cur = GetPerspective();
                if (cur >= 0 && cur != g_snaplookCameraMode) SetPerspective(g_snaplookCameraMode);
            }
        }

        hookCheckCounter++;
        if (hookCheckCounter >= 10) { hookCheckCounter = 0; EnsureWndProcHooked(); }

        if (g_env) {
            ULONGLONG nowMs = GetTickCount64();
            bool inTntTagGame = false;
            bool inHypixelTntTagGame = false;
            std::string timer = ReadExplosionTimer(&inTntTagGame, &inHypixelTntTagGame);
            if (inTntTagGame) {
                g_lastTntTagContextSeenMs = nowMs;
                if (inHypixelTntTagGame) g_lastHypixelTntTagContextSeenMs = nowMs;
                else if (g_lastHypixelTntTagContextSeenMs != 0 &&
                    nowMs - g_lastHypixelTntTagContextSeenMs <= kTntTagContextGraceMs) {
                    inHypixelTntTagGame = true;
                }
                else {
                    g_lastHypixelTntTagContextSeenMs = 0;
                }
            }
            else if (g_lastTntTagContextSeenMs != 0 &&
                nowMs - g_lastTntTagContextSeenMs <= kTntTagContextGraceMs) {
                inTntTagGame = true;
                inHypixelTntTagGame = g_lastHypixelTntTagContextSeenMs != 0 &&
                    nowMs - g_lastHypixelTntTagContextSeenMs <= kTntTagContextGraceMs;
            }
            else {
                g_lastTntTagContextSeenMs = 0;
                g_lastHypixelTntTagContextSeenMs = 0;
            }
            InterlockedExchange(&g_tntTagGameActive, inTntTagGame ? 1 : 0);
            InterlockedExchange(&g_hypixelTntTagGameActive, inHypixelTntTagGame ? 1 : 0);
            if (g_guiPublicWinsEnabled && inHypixelTntTagGame) {
                EnsurePublicWinsTabNameHook(g_env);
                EnsurePublicWinsApiTabHook(g_env);
                EnsurePublicWinsScoreboardFormatHook(g_env);
                EnsurePublicWinsRenderedNameHook(g_env);
            }
            if (g_guiPublicWinsEnabled && inHypixelTntTagGame &&
                (g_lastPublicWinsPrefetchMs == 0 || nowMs - g_lastPublicWinsPrefetchMs >= 1000)) {
                QueuePublicWinsForWorldPlayers();
                g_lastPublicWinsPrefetchMs = nowMs;
            }
            else if (!g_guiPublicWinsEnabled || !inHypixelTntTagGame) {
                g_lastPublicWinsPrefetchMs = 0;
            }
            PollSpeedTransitionDiagnostic(inTntTagGame);
            UpdateTagScoreboardVisibility(g_guiExtrasDisableTagScoreboard && inTntTagGame);

            char colorCode = 'a';
            std::string seconds = timer.empty() ? "" : ExtractSeconds(timer, colorCode);
            bool hasTimerNumber = !seconds.empty();
            int newSeconds = hasTimerNumber ? atoi(seconds.c_str()) : -1;

            if (inTntTagGame && hasTimerNumber && newSeconds > 0) {
                bool phaseChanged = g_betweenRoundsTimerActive;
                g_betweenRoundsTimerActive = false;
                g_betweenRoundsStartedAtMs = 0;
                g_roundTimerObserved = true;
                g_lastRoundTimerSeenMs = nowMs;

                if (!g_timerActive || phaseChanged || newSeconds != g_explosionSeconds) {
                    g_explosionSeconds = newSeconds;
                    g_timerStartSeconds = (double)newSeconds;
                    g_explosionColorCode = colorCode;
                    QueryPerformanceCounter(&g_explosionSetAt);
                }
                g_timerActive = true;
            }
            else {
                double currentRemaining = g_timerActive ? GetDecimalSeconds() : -1.0;
                bool reachedRoundEnd = hasTimerNumber && newSeconds <= 0;
                bool disappearedNearRoundEnd = !hasTimerNumber &&
                    !g_betweenRoundsTimerActive &&
                    g_timerActive &&
                    currentRemaining >= 0.0 && currentRemaining <= 1.25 &&
                    g_lastRoundTimerSeenMs != 0 &&
                    (nowMs - g_lastRoundTimerSeenMs) <= 1500;

                if (inHypixelTntTagGame && g_roundTimerObserved && !g_betweenRoundsTimerActive &&
                    (reachedRoundEnd || disappearedNearRoundEnd)) {
                    g_betweenRoundsTimerActive = true;
                    g_betweenRoundsStartedAtMs = nowMs;
                    g_explosionSeconds = (int)std::ceil(kBetweenRoundTimerSeconds);
                    g_timerStartSeconds = kBetweenRoundTimerSeconds;
                    g_explosionColorCode = '6';
                    QueryPerformanceCounter(&g_explosionSetAt);
                    g_timerActive = true;
                    DebugLog("Between-round timer started duration=%.1f trigger=%s",
                        kBetweenRoundTimerSeconds,
                        reachedRoundEnd ? "timer-zero" : "timer-missing-near-zero");
                }

                if (!inTntTagGame) {
                    g_explosionSeconds = -1;
                    g_timerStartSeconds = -1.0;
                    g_timerActive = false;
                    g_betweenRoundsTimerActive = false;
                    g_betweenRoundsStartedAtMs = 0;
                    g_roundTimerObserved = false;
                    g_lastRoundTimerSeenMs = 0;
                }
                else if (g_betweenRoundsTimerActive) {
                    if (g_betweenRoundsStartedAtMs != 0 &&
                        (nowMs - g_betweenRoundsStartedAtMs) >=
                        (ULONGLONG)(kBetweenRoundTimerSeconds * 1000.0)) {
                        g_explosionSeconds = -1;
                        g_timerStartSeconds = -1.0;
                        g_timerActive = false;
                    }
                }
                else if (!hasTimerNumber &&
                    (g_lastRoundTimerSeenMs == 0 || (nowMs - g_lastRoundTimerSeenMs) > 500)) {
                    g_explosionSeconds = -1;
                    g_timerStartSeconds = -1.0;
                    g_timerActive = false;
                }
            }

            bool shouldShowNametagTimer = g_guiTimerNametagEnabled &&
                g_timerActive && !g_betweenRoundsTimerActive;
            if (shouldShowNametagTimer) {
                if (g_lastTimerNametagUpdateMs == 0 || (nowMs - g_lastTimerNametagUpdateMs) >= 100) {
                    if (IsLunarNamedClient()) {
                        // Lunar retains the first ScorePlayerTeam-formatted nametag in
                        // parts of its render pipeline. Patch the transformed living-
                        // entity renderer and compute the suffix every rendered frame.
                        if (!g_teamSuffixCache.empty()) ApplyTimerToPlayerTeams(false);
                        EnsurePublicWinsRenderedNameHook(g_env);
                    }
                    else {
                        ApplyTimerToPlayerTeams(true);
                    }
                    g_lastTimerNametagUpdateMs = nowMs;
                }
            }
            else {
                if (!g_teamSuffixCache.empty()) ApplyTimerToPlayerTeams(false);
                g_lastTimerNametagUpdateMs = 0;
            }

            bool shouldEditDefaultScoreboard = g_guiTimerEditDefaultScoreboard &&
                NormalizeTimerDecimalPlaces(g_guiTimerDecimalPlaces) > 0 &&
                g_timerActive;
            if (shouldEditDefaultScoreboard) {
                if (g_lastDefaultScoreboardBetweenRounds != g_betweenRoundsTimerActive) {
                    if (!g_scoreboardTimerLineCache.empty()) ApplyDefaultScoreboardTimerEdit(false);
                    g_lastDefaultScoreboardTimerUpdateMs = 0;
                    g_lastDefaultScoreboardBetweenRounds = g_betweenRoundsTimerActive;
                }
                if (g_lastDefaultScoreboardTimerUpdateMs == 0 || (nowMs - g_lastDefaultScoreboardTimerUpdateMs) >= 100) {
                    ApplyDefaultScoreboardTimerEdit(
                        true,
                        GetDecimalSeconds(),
                        g_betweenRoundsTimerActive);
                    g_lastDefaultScoreboardTimerUpdateMs = nowMs;
                }
            }
            else {
                if (!g_scoreboardTimerLineCache.empty()) ApplyDefaultScoreboardTimerEdit(false);
                g_lastDefaultScoreboardTimerUpdateMs = 0;
                g_lastDefaultScoreboardBetweenRounds = g_betweenRoundsTimerActive;
            }

            // Wins are appended when vanilla formats the current scoreboard
            // team name. Never move players out of Hypixel's live teams: those
            // memberships carry the red TNT name and [IT] state.
            if (!g_publicWinsTeamFormatCache.empty()) ApplyPublicWinsToPlayerTeams(false);
            g_lastPublicWinsTeamUpdateMs = 0;

            PollSpeedSlownessChatAlerts(g_guiSpeedSlownessEnabled);
            FlushMutedVoiceLocalChatQueue(g_env);
        }
        Sleep(1);
    }

    // Restore perspective if snaplook was active when unloading
    if (g_snaplookActive) {
        SetPerspective(g_snaplookSavedPerspective);
        g_snaplookActive = false;
    }
    if (!g_teamSuffixCache.empty()) ApplyTimerToPlayerTeams(false);
    if (!g_scoreboardTimerLineCache.empty()) ApplyDefaultScoreboardTimerEdit(false);
    if (!g_publicWinsTeamFormatCache.empty()) ApplyPublicWinsToPlayerTeams(false);
    RestoreStoredTagScoreboardVisibility();
    if (g_tntVisualJNI.wheatApplied || g_tntVisualJNI.beaconApplied) {
        InterlockedExchange(&g_tntVisualRestoreCompleted, 0);
        InterlockedExchange(&g_tntVisualRestoreRequested, 1);
        ULONGLONG restoreDeadline = GetTickCount64() + 500;
        while (InterlockedCompareExchange(&g_tntVisualRestoreCompleted, 0, 0) == 0 && GetTickCount64() < restoreDeadline) {
            Sleep(10);
        }
    }
    RestorePublicWinsTabNameHook(g_env);
    StopPublicWinsWorker();
    ShutdownMutedVoicePacketFilter();
    ShutdownInjectedJavaCallbacks();
    StopMutedVoiceWorker();

    // Redefined methods already executing on a render/network thread can remain
    // as obsolete frames briefly. Give them time to return after unregistering
    // native helpers before this DLL's code pages are released.
    Sleep(500);

    InterlockedExchange(&g_shutdownRequested, 1);
    InterlockedExchange(&g_tntTagGameActive, 0);
    InterlockedExchange(&g_hypixelTntTagGameActive, 0);
    DebugLog("MainThread shutdown requested");

    MH_DisableHook(MH_ALL_HOOKS);
    UnhookWndProc();
    ShutdownGuiThread();
    MH_Uninitialize();
    SetUnhandledExceptionFilter(g_prevUnhandledExceptionFilter);
    DebugLog("MainThread shutdown complete restoring previous exception filter=%p", g_prevUnhandledExceptionFilter);
    ReleaseSpeedTransitionDiagnosticJNI();
    if (g_jvm) g_jvm->DetachCurrentThread();
    FreeLibraryAndExitThread((HMODULE)lpParam, 0);
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        g_moduleHandle = hinstDLL;
        DisableThreadLibraryCalls(hinstDLL);
        CreateThread(nullptr, 0, MainThread, hinstDLL, 0, nullptr);
    }
    return TRUE;
}
