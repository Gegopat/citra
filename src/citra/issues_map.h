// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <unordered_map>
#include "common/common_types.h"

struct Issue {
    std::string data;
    int number{};
    enum class Type {
        Normal,
        GitHub,
    };
    Type type{Type::Normal};
};

static inline Issue GitHubIssue(const std::string& repo, int number) {
    return Issue{repo, number, Issue::Type::GitHub};
}

static const std::unordered_map<u64, std::vector<Issue>> issues_map{{
    // DSiMenuPlusPlus
    {0x0004000004395500, {Issue{"Citra crashes with Unknown result status"}}},

    // White Haired Cat Girl
    {0x000400000D5CDB00, {Issue{"Needs SOC:GetAddrInfo"}}},

    // Boot NTR Selector
    {0x000400000EB00000, {Issue{"Load fails (NTR is already running)"}}},

    // Detective Pikachu
    {0x00040000001C1E00, {GitHubIssue("citra-emu/citra", 3612)}},

    // Pokémon Sun
    {0x0004000000164800,
     {GitHubIssue("citra-emu/citra", 2311), Issue{"Needs Nintendo Network support"}}},

    // Pokémon Moon
    {0x0004000000175E00,
     {GitHubIssue("citra-emu/citra", 2311), Issue{"Needs Nintendo Network support"}}},

    // Pokémon Omega Ruby
    {0x000400000011C400,
     {GitHubIssue("citra-emu/citra", 4092), Issue{"Needs Nintendo Network support"}}},

    // Pokémon Alpha Sapphire
    {0x000400000011C500,
     {GitHubIssue("citra-emu/citra", 4092), Issue{"Needs Nintendo Network support"}}},

    // Pokémon Ultra Sun
    {0x00040000001B5000, {Issue{"Needs Nintendo Network support"}}},

    // Pokémon Ultra Moon
    {0x00040000001B5100, {Issue{"Needs Nintendo Network support"}}},

    // Pokémon X
    {0x0004000000055D00, {Issue{"Needs Nintendo Network support"}}},

    // Pokémon Y
    {0x0004000000055E00, {Issue{"Needs Nintendo Network support"}}},

    // Test Program (CTRAging)
    {0x000400000F980000, {Issue{"Crashes Citra when booting"}}},

    // Animal Crossing: Happy Home Designer (EUR)
    {0x000400000014F200, {Issue{"Needs Nintendo Network support"}}},

    // Animal Crossing: New Leaf (EUR)
    {0x0004000000086400, {Issue{"Needs Microphone"}, Issue{"Needs Nintendo Network support"}}},

    // Animal Crossing: New Leaf - Welcome amiibo (EUR)
    {0x0004000000198F00,
     {Issue{"Needs Microphone"}, Issue{"Needs Nintendo Network support"},
      GitHubIssue("citra-emu/citra", 3438)}},

    // nintendogs + cats (Golden Retriever & New Friends) (EUR)
    {0x0004000000030C00, {Issue{"Needs Microphone"}, Issue{"Needs Pedometer"}}},

    // Super Mario Maker (EUR)
    {0x00040000001A0500, {Issue{"Needs Nintendo Network support"}}},

    // Tomodachi Life (EUR)
    {0x000400000008C400,
     {Issue{"Lip syncing glitchy when singing"}, GitHubIssue("citra-emu/citra", 4320),
      Issue{"Exchange Miis or Other Items - The name of the island flashes"},
      GitHubIssue("citra-emu/citra", 4449)}},

    // The Legend of Zelda: Tri Force Heroes (EUR)
    {0x0004000000177000, {Issue{"Needs DLP"}, Issue{"Needs Nintendo Network support"}}},

    // Photos with Mario (EUR)
    {0x0004000000130600, {GitHubIssue("citra-emu/citra", 3347)}},

    // Mario Kart 7 (EUR)
    {0x0004000000030700, {Issue{"Needs DLP"}, Issue{"Needs Nintendo Network support"}}},

    // nintendogs + cats (Toy Poodle & New Friends) (EUR)
    {0x0004000000031600, {Issue{"Needs Microphone"}, Issue{"Needs Pedometer"}}},

    // Teenage Mutant Ninja Turtles: Master Splinter's Training Pack (EUR)
    {0x0004000000170B00, {Issue{"Softlocks when selecting a game"}}},

    // Captain Toad: Treasure Tracker (EUR)
    {0x00040000001CB200, {Issue{"Needs Microphone"}}},

    // Mii Maker (EUR)
    {0x0004001000022700,
     {GitHubIssue("citra-emu/citra", 3897), GitHubIssue("citra-emu/citra", 3903)}},

    // Download Play (EUR)
    {0x0004001000022100, {Issue{"Needs DLP"}}},

    // Nintendo Network ID Settings (EUR)
    {0x000400100002C100, {Issue{"Needs Nintendo Network support"}}},

    // Super Mario Maker (USA)
    {0x00040000001A0400, {Issue{"Needs Nintendo Network support"}}},

    // Animal Crossing: Happy Home Designer (USA)
    {0x000400000014F100, {Issue{"Needs Nintendo Network support"}}},

    // Photos with Mario (USA)
    {0x0004000000130500, {GitHubIssue("citra-emu/citra", 3348)}},

    // Animal Crossing: New Leaf (USA)
    {0x0004000000086300, {Issue{"Needs Microphone"}, Issue{"Needs Nintendo Network support"}}},

    // Tomodachi Life (USA)
    {0x000400000008C300,
     {Issue{"Lip syncing glitchy when singing"}, GitHubIssue("citra-emu/citra", 4320),
      Issue{"Exchange Miis or Other Items - The name of the island flashes"},
      GitHubIssue("citra-emu/citra", 4449)}},

    // Mario Kart 7 (USA)
    {0x0004000000030800, {Issue{"Needs DLP"}, Issue{"Needs Nintendo Network support"}}},

    // Minecraft (USA)
    {0x00040000001B8700, {Issue{"Softlock when saving"}}},

    // Sonic Generations (USA)
    {0x0004000000062300, {Issue{"Multiplayer needs deprecated UDS"}}},

    // Luigi's Mansion (USA)
    {0x00040000001D1900, {GitHubIssue("citra-emu/citra", 4330)}},

    // Swapdoodle (USA)
    {0x00040000001A2D00, {GitHubIssue("citra-emu/citra", 4388)}},

    // とびだせ どうぶつの森 (JPN)
    {0x0004000000086200, {Issue{"Needs Microphone"}, Issue{"Needs Nintendo Network support"}}},

    // とびだせ どうぶつの森 amiibo+ (JPN)
    {0x0004000000198D00,
     {Issue{"Needs Microphone"}, Issue{"Needs Nintendo Network support"},
      GitHubIssue("citra-emu/citra", 3438)}},

    // スーパーマリオメーカー for ニンテンドー3DS (JPN)
    {0x00040000001A0300, {Issue{"Needs Nintendo Network support"}}},

    // 튀어나와요 동물의 숲 (KOR)
    {0x0004000000086500, {Issue{"Needs Microphone"}, Issue{"Needs Nintendo Network support"}}},

    // Animal Crossing: New Leaf - Welcome amiibo (KOR)
    {0x0004000000199000,
     {Issue{"Needs Microphone"}, Issue{"Needs Nintendo Network support"},
      GitHubIssue("citra-emu/citra", 3438)}},

    // Super Mario Maker (KOR)
    {0x00040000001BB800, {Issue{"Needs Nintendo Network support"}}},
}};
