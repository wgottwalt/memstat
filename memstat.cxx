/*
 *  memstat - little memory usage showing tool
 *  Copyright (C) 2020 Wilken 'Akiko' Gottwalt <akiko@linux-addicted.net>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <getopt.h>
#include <unistd.h>

const int64_t PageSize = getpagesize() / 1024;

const std::string None("none");
const std::string Shared("shared");
const std::string Private("private");
const std::string Rss("rss");
const std::string Swap("swap");
const std::string Name("name");

const std::string OptHelp("help");
const std::string OptSwap("swap");
const std::string OptSort("sort");

struct Process {
    std::string name;
    int64_t priv;
    int64_t shared;
    int64_t rss;
    int64_t pss;
    int64_t swap;
    int64_t swap_pss;
};

using ShProc = std::shared_ptr<Process>;
using ProcList = std::vector<ShProc>;

namespace Concept
{
    template <typename T>
    concept Integer = std::is_integral_v<T>;
}

//--- options ---

enum class Sort : uint8_t {
    None, // PID actually
    Shared,
    Private,
    Rss,
    Swap,
    Name,
};

struct Config {
    bool help;
    bool swap;
    Sort sort;
};

static const struct option arg_table[] = {
    { OptHelp.c_str(), no_argument, nullptr, 'h' },
    { OptSwap.c_str(), no_argument, nullptr, 'S' },
    { OptSort.c_str(), required_argument, nullptr, 's' },
};

void help(const std::string &name)
{
    std::cout << "usage: " << name << " <options>\n"
              << "Version 0.1\n"
              << "Shows detailed memory usage of all processes.\n"
              << "\n"
              << "options:\n"
              << "  -h, --help      show this help screen\n"
              << "  -S, --swap      show swap usage\n"
              << "  -s, --sort <s>  sort by shared, private, rss, swap or name\n"
              << std::endl;
}

void parseCommandLine(const int32_t argc, char **argv, Config &config)
{
    bool process_args = true;

    while (process_args)
    {
        int32_t index = 0;
        int32_t argument = 0;

        argument = getopt_long(argc, argv, "hSs:", arg_table, &index);
        switch (argument)
        {
            case 'h':
                config.help = true;
                break;

            case 'S':
                config.swap = true;
                break;

            case 's':
            {
                const std::string arg(optarg);

                if (arg == Shared)
                    config.sort = Sort::Shared;
                else if (arg == Private)
                    config.sort = Sort::Private;
                else if (arg == Rss)
                    config.sort = Sort::Rss;
                else if (arg == Swap)
                    config.sort = Sort::Swap;
                else if (arg == Name)
                    config.sort = Sort::Name;

                break;
            }

            case -1:
            default:
                process_args = false;
        }
    }
}

//--- getting the data ---

ShProc getProcessData(const std::filesystem::path &pid_dir)
{
    const std::filesystem::directory_entry comm(pid_dir.string() + "/comm");
    std::filesystem::directory_entry smaps(pid_dir.string() + "/smaps_rollup");
    Process process = { "", 0, 0, 0, 0, 0, 0 };

    if (comm.exists() && comm.is_regular_file())
    {
        if (std::ifstream ifile(std::filesystem::path(comm).string());
            ifile.is_open() && ifile.good())
        {
            std::getline(ifile, process.name);
            ifile.close();
        }
    }

    if (!smaps.exists() || !smaps.is_regular_file())
        smaps = std::filesystem::directory_entry(pid_dir.string() + "/smaps");

    if (smaps.exists() && smaps.is_regular_file())
    {
        if (std::ifstream ifile(std::filesystem::path(smaps).string());
            ifile.is_open() && ifile.good())
        {
            std::string line = "";
            std::string junk = "";
            int64_t shared = 0;
            int64_t priv = 0;
            int64_t rss = 0;
            int64_t pss = 0;
            int64_t swap = 0;
            int64_t swap_pss = 0;

            while (std::getline(ifile, line))
            {
                std::stringstream strm(line);
                std::string_view view(line);

                if (view.substr(0, 6) == "Shared")
                {
                    strm >> junk >> shared;
                    process.shared += shared;
                }
                else if (view.substr(0, 7) == "Private")
                {
                    strm >> junk >> priv;
                    process.priv += priv;
                }
                else if (view.substr(0, 3) == "Rss")
                {
                    strm >> junk >> rss;
                    process.rss += rss;
                }
                else if (view.substr(0, 3) == "Pss")
                {
                    strm >> junk >> pss;
                    process.pss += pss;
                }
                else if (view.substr(0, 5) == "Swap:")
                {
                    strm >> junk >> swap;
                    process.swap += swap;
                }
                else if (view.substr(0, 8) == "SwapPss:")
                {
                    strm >> junk >> swap_pss;
                    process.swap_pss += swap_pss;
                }
            }
            ifile.close();
        }
    }

    return std::make_shared<Process>(process);
}

void createProcessList(ProcList &list)
{
    auto isNumber = [](const std::string &str)
    {
        for (auto chr : str)
            if (!std::isdigit(chr))
                return false;

        return true;
    };

    if (std::filesystem::directory_entry proc("/proc"); proc.exists() && proc.is_directory())
    {
        for (auto &dentry : std::filesystem::directory_iterator(proc))
        {
            if (dentry.exists() && dentry.is_directory() && isNumber(dentry.path().filename()))
            {
                if (auto process = getProcessData(dentry.path()); process->rss)
                    list.push_back(getProcessData(dentry.path()));
            }
        }
    }
}

//--- main ---

template <Concept::Integer T>
constexpr inline T lenOfNum(const T value)
{
    const T tmp = value / 10;

    if (tmp > 0)
        return 1 + lenOfNum(tmp);

    return 1;
};

template <Concept::Integer T>
constexpr inline std::string spaceGen(const T size)
{
    std::string tmp(size, ' ');
    return tmp;
};

int32_t main(int32_t argc, char **argv)
{
    Config config = { false, false, Sort::None };
    ProcList proc_list;
    Process proc_lens;

    parseCommandLine(argc, argv, config);
    createProcessList(proc_list);

    proc_lens = { "", 0, 0, 0, 0, 0, 0 };

    for (auto entry : proc_list)
    {
        proc_lens.priv = std::max(lenOfNum(entry->priv) + 5, proc_lens.priv);
        proc_lens.shared = std::max(lenOfNum(entry->shared) + 5, proc_lens.shared);
        proc_lens.rss = std::max(lenOfNum(entry->rss) + 5, proc_lens.rss);
        proc_lens.swap = std::max(lenOfNum(entry->swap) + 5, proc_lens.swap);
    }

    switch (config.sort)
    {
        case Sort::Shared:
            std::sort(proc_list.begin(), proc_list.end(), [](auto entry1, auto entry2)
            {
                return entry1->shared < entry2->shared;
            });
            break;

        case Sort::Private:
            std::sort(proc_list.begin(), proc_list.end(), [](auto entry1, auto entry2)
            {
                return entry1->priv < entry2->priv;
            });
            break;

        case Sort::Rss:
            std::sort(proc_list.begin(), proc_list.end(), [](auto entry1, auto entry2)
            {
                return entry1->rss < entry2->rss;
            });
            break;

        case Sort::Swap:
            std::sort(proc_list.begin(), proc_list.end(), [](auto entry1, auto entry2)
            {
                return entry1->swap < entry2->swap;
            });
            break;

        case Sort::Name:
            std::sort(proc_list.begin(), proc_list.end(), [](auto entry1, auto entry2)
            {
                return entry1->name < entry2->name;
            });
            break;

        case Sort::None:
        default:
            break;
    }

    std::cout << spaceGen(proc_lens.shared - Shared.size() + 4) << Shared
              << spaceGen(proc_lens.priv - Private.size() + 4) << Private
              << spaceGen(proc_lens.rss - Rss.size() + 4) << Rss;
    if (config.swap)
        std::cout << spaceGen(proc_lens.swap - Swap.size() + 4) << Swap;
    std::cout << "  " << Name << "\n"
              << "---------------------------------------------------------------------------------"
              << std::endl;

    for (auto entry : proc_list)
    {
        std::cout << std::setw(proc_lens.shared) << entry->shared << " KiB"
                  << std::setw(proc_lens.priv) << entry->priv << " KiB"
                  << std::setw(proc_lens.rss) << entry->rss << " KiB";
        if (config.swap)
            std::cout << std::setw(proc_lens.swap) << entry->swap << " KiB";
        std::cout << "  " << entry->name << std::endl;
    }

    return 0;
}
