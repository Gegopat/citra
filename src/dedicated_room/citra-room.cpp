// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <asl/CmdArgs.h>
#include <asl/TextFile.h>
#include <enet/enet.h>
#include <fmt/format.h>
#include "common/common_types.h"
#include "common/scm_rev.h"
#include "common/string_util.h"
#include "network/room.h"

static void PrintHelp(const char* argv0) {
    std::cout << "Usage: " << argv0
              << " [options] <filename>\n"
                 "-room-name         The name of the room\n"
                 "-room-description  The room description\n"
                 "-port              The port used for the room\n"
                 "-max-members       The maximum number of members for this room\n"
                 "-announce!         Create a public room\n"
                 "-password          The password for the room\n"
                 "-creator           The creator of the room\n"
                 "-ban-list-file     The file for storing the room ban list\n"
                 "-h, --help         Display this help and exit\n"
                 "-v, --version      Output version information and exit\n";
}

static void PrintVersion() {
    std::cout << "Citra dedicated room " << Common::g_scm_branch << " " << Common::g_scm_desc
              << " Libnetwork: " << Network::NetworkVersion - 0xFF00 << std::endl;
}

static Network::Room::BanList LoadBanList(const std::string& path) {
    asl::TextFile file{path.c_str(), asl::File::READ};
    if (!file) {
        std::cout << "Couldn't open ban list!\n\n";
        return {};
    }
    Network::Room::BanList ban_list;
    while (!file.end()) {
        std::string line{file.readLine()};
        line.erase(std::remove(line.begin(), line.end(), '\0'), line.end());
        line = Common::StripSpaces(line);
        if (!line.empty())
            ban_list.emplace_back(std::move(line));
    }
    return ban_list;
}

static void SaveBanList(const Network::Room::BanList& ban_list, const std::string& path) {
    asl::TextFile file{path.c_str(), asl::File::WRITE};
    if (!file) {
        std::cout << "Couldn't save ban list!\n\n";
        return;
    }
    // IP ban list
    for (const auto& ip : ban_list)
        file << ip.c_str() << "\n";
    file.flush();
}

/// Application entry point
int main(int argc, char** argv) {
    asl::CmdArgs args{argc, argv};
    std::string room_name{args["room-name"]};
    std::string room_description{args["room-description"]};
    u32 port{static_cast<u32>(args("port", asl::String{Network::DefaultRoomPort}).toInt())};
    u32 max_members{static_cast<u32>(args("max-members", "16").toInt())};
    std::string password{args["password"]};
    std::string creator{args["creator"]};
    std::string ban_list_file{args["ban-list-file"]};
    bool announce{args.is("announce")};
    if (args.is("help")) {
        PrintHelp(argv[0]);
        return 0;
    }
    if (args.is("version")) {
        PrintVersion();
        return 0;
    }
    if (room_name.empty()) {
        std::cout << "Room name is empty!\n\n";
        PrintHelp(argv[0]);
        return -1;
    }
    if (port == 0 || port > 65535) {
        std::cout << "Port needs to be in the range 1 - 65535!\n\n";
        PrintHelp(argv[0]);
        return -1;
    }
    if (creator.empty()) {
        std::cout << "Creator is empty!\n\n";
        PrintHelp(argv[0]);
        return -1;
    }
    if (ban_list_file.empty())
        std::cout << "Ban list file not set!\nThis should get set to load and save room ban "
                     "list.\nSet with -ban-list-file <file>\n\n";
    // Load the ban list
    Network::Room::BanList ban_list;
    if (!ban_list_file.empty())
        ban_list = LoadBanList(ban_list_file);
    if (enet_initialize() != 0) {
        std::cout << "Error when initializing ENet\n\n";
        return -1;
    }
    Network::Room room;
    room.SetErrorCallback([&announce](const Common::WebResult& result) {
        if (result.result_code != Common::WebResult::Code::Success)
            announce = false;
        std::cout << result.result_string << std::endl;
    });
    if (!room.Create(announce, room_name, room_description, creator, port, password, max_members,
                     ban_list)) {
        std::cout << "Failed to create room!\n\n";
        return -1;
    }
    std::cout << fmt::format("Hosting a {} room\nRoom is open. Close with Q+Enter...\n\n",
                             announce ? "public" : "private");
    while (room.IsOpen()) {
        std::string in;
        std::cin >> in;
        if (in.size() > 0) {
            // Save the ban list
            if (!ban_list_file.empty())
                SaveBanList(room.GetBanList(), ban_list_file);
            room.Destroy();
            return 0;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
    }
    // Save the ban list
    if (!ban_list_file.empty())
        SaveBanList(room.GetBanList(), ban_list_file);
    room.Destroy();
    enet_deinitialize();
    return 0;
}
