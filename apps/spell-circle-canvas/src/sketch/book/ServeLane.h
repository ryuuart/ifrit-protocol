#pragma once

/** @file
 * The served lane: Sketchbook with no window, a host a client drives
 * over the protocol for as long as the process runs.
 */

#include <sigilmaterial/stock/Stock.h>

#include <filesystem>
#include <future>

struct Arguments;

/** A HEADLESS HOST ON THE PROTOCOL, what `--headless --inspect` with no
 *  plate directory asks for: the registry, session and clock agents
 *  mounted on one dispatcher, the endpoint on loopback at the port
 *  `--inspect` named, its address written to `<state>/protocol-address`
 *  before the first frame, and a loop that dispatches the hub and turns
 *  the agents' frame until the process is told to end — by an interrupt
 *  or a termination signal — when the address file is taken back.
 *  A file a client opens by path is compiled with @p flagsFile.
 *  1 where no endpoint could listen; 0 otherwise. */
int runServe(const Arguments& args, const std::filesystem::path& flagsFile,
             std::future<sigil::material::WarmupResult>& materialWarmup);
