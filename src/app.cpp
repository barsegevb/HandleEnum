#include "app.hpp"

#include "cli_parser.hpp"
#include "nt.hpp"
#include "printer.hpp"
#include "string_utils.hpp"

#include <algorithm>
#include <cstdlib>
#include <format>
#include <iostream>
#include <limits>
#include <memory>
#include <ranges>
#include <string>
#include <unordered_set>
#include <vector>

const std::string& HandleEnumApp::get_cached_process_name(const uint32_t pid) {
    if (const auto cache_it = m_process_name_cache.find(pid); cache_it != m_process_name_cache.end()) {
        return cache_it->second;
    }

    const auto [inserted_it, inserted] = m_process_name_cache.emplace(pid, nt::get_process_name_by_pid(pid));
    (void)inserted;
    return inserted_it->second;
}

HandleInfo HandleEnumApp::map_to_info(const nt::RawHandle& raw_handle) {
    const uint32_t pid = raw_handle.processId > static_cast<std::uintptr_t>(std::numeric_limits<uint32_t>::max())
        ? std::numeric_limits<uint32_t>::max()
        : static_cast<uint32_t>(raw_handle.processId);

    const std::string& process_name = get_cached_process_name(pid);

    const auto type_result = nt::query_object_type(raw_handle);
    const std::string handle_type = type_result ? *type_result : "N/A";
    
    std::string object_name;
    
    // Refined Anti-deadlock bypass: skip name queries ONLY for risky pipes/sockets
    bool is_risky_pipe = false;
    
    if (handle_type == "File") {
        if (raw_handle.grantedAccess == 0x0012019F || 
            raw_handle.grantedAccess == 0x001A019F ||
            raw_handle.grantedAccess == 0x00120189 ||
            raw_handle.grantedAccess == 0x00100000) {
            is_risky_pipe = true;
        }
    }
    
    if (is_risky_pipe) {
        object_name = "Locked (Anti-Deadlock)";
    } else {
        const auto name_result = nt::query_object_name(raw_handle);
        object_name = name_result ? *name_result : "N/A";
    }

    return HandleInfo{
        .pid = pid,
        .processName = process_name,
        .handleType = handle_type,
        .objectName = object_name,
        .grantedAccess = raw_handle.grantedAccess,
        .objectAddress = raw_handle.objectAddress,
        .handleValue = raw_handle.handleValue,
        .objectTypeIndex = raw_handle.objectTypeIndex,
        .handleAttributes = raw_handle.handleAttributes
    };
}

void HandleEnumApp::sort_handles(std::vector<HandleInfo>& handles, const SortField sort_by) {
    switch (sort_by) {
    case SortField::Pid:
        std::ranges::sort(handles, [](const HandleInfo& left, const HandleInfo& right) {
            if (left.pid != right.pid) {
                return left.pid < right.pid;
            }
            return left.handleValue < right.handleValue;
        });
        break;
    case SortField::Type:
        std::ranges::sort(handles, [](const HandleInfo& left, const HandleInfo& right) {
            const std::string left_type = utils::to_lower_ascii(left.handleType);
            const std::string right_type = utils::to_lower_ascii(right.handleType);
            if (left_type != right_type) {
                return left_type < right_type;
            }
            if (left.pid != right.pid) {
                return left.pid < right.pid;
            }
            return left.handleValue < right.handleValue;
        });
        break;
    case SortField::Name:
        std::ranges::sort(handles, [](const HandleInfo& left, const HandleInfo& right) {
            const std::string left_name = utils::to_lower_ascii(left.objectName);
            const std::string right_name = utils::to_lower_ascii(right.objectName);
            if (left_name != right_name) {
                return left_name < right_name;
            }
            if (left.pid != right.pid) {
                return left.pid < right.pid;
            }
            return left.handleValue < right.handleValue;
        });
        break;
    }
}

void HandleEnumApp::build_filters(const Parser& parsed_args) {
    m_filters.clear();

    if (parsed_args.pid.has_value()) {
        m_filters.push_back(std::make_unique<PidFilter>(*parsed_args.pid));
    }

    if (parsed_args.handleType.has_value()) {
        m_filters.push_back(std::make_unique<TypeFilter>(*parsed_args.handleType));
    }

    if (parsed_args.objectName.has_value()) {
        m_filters.push_back(std::make_unique<NameFilter>(*parsed_args.objectName));
    }
}

int HandleEnumApp::run(int argc, char* argv[]) {
    auto parse_result = cli::parse(argc, argv);
    if (!parse_result) {
        if (parse_result.error() == "help") {
            cli::print_help();
            return EXIT_SUCCESS;
        }

        std::cerr << std::format("Error: {}\n", parse_result.error());
        return EXIT_FAILURE;
    }

    const Parser& options = parse_result.value();
    build_filters(options);

    if (auto privilege_result = nt::enable_debug_privilege(); !privilege_result) {
        std::cerr << std::format("Warning: failed to enable SeDebugPrivilege ({})\n",
                                 privilege_result.error().message());
    }

    auto handles_result = nt::query_system_handles();
    if (!handles_result) {
        std::cerr << std::format("Error: failed to query system handles ({})\n",
                                 handles_result.error().message());
        return EXIT_FAILURE;
    }

    const std::size_t total_raw_count = handles_result->size();

    std::vector<nt::RawHandle> filtered_handles;
    filtered_handles.reserve(handles_result->size());

    for (const nt::RawHandle& handle : *handles_result) {
        bool keep = true;
        for (const auto& filter : m_filters) {
            if (!filter->match(handle)) {
                keep = false;
                break;
            }
        }

        if (keep) {
            filtered_handles.push_back(handle);
        }
    }

    if (options.showCountOnly) {
        const HandlePrinter printer;
        printer.print_count_only(options, total_raw_count, filtered_handles.size());
        return EXIT_SUCCESS;
    }

    m_process_name_cache.clear();
    m_process_name_cache.reserve(filtered_handles.size());

    std::unordered_set<uint32_t> unique_pids;
    unique_pids.reserve(filtered_handles.size());

    for (const nt::RawHandle& raw_handle : filtered_handles) {
        const uint32_t pid = raw_handle.processId > static_cast<std::uintptr_t>(std::numeric_limits<uint32_t>::max())
            ? std::numeric_limits<uint32_t>::max()
            : static_cast<uint32_t>(raw_handle.processId);
        unique_pids.insert(pid);
    }

    for (const uint32_t pid : unique_pids) {
        m_process_name_cache.emplace(pid, nt::get_process_name_by_pid(pid));
    }

    const HandlePrinter printer;

    if (options.sortBy == SortField::Pid) {
        // Streaming mode: print handles as they're processed
        if (options.verbose) {
            std::cout << "Verbose mode is ON\n";
        }
        if (options.pid) {
            std::cout << std::format("Filtering by PID: {}\n", *options.pid);
        }
        std::cout << std::format("Retrieved {} system handles.\n", total_raw_count);
        printer.print_header();

        std::size_t matching_count = 0;
        for (const nt::RawHandle& raw_handle : filtered_handles) {
            HandleInfo handle_info = map_to_info(raw_handle);
            printer.print_row(handle_info);
            ++matching_count;
        }

        std::cout << std::format("Matching handles: {}\n", matching_count);
    } else {
        // Batch mode: collect all, sort, then print
        std::vector<HandleInfo> mapped_handles;
        mapped_handles.reserve(filtered_handles.size());

        for (const nt::RawHandle& raw_handle : filtered_handles) {
            mapped_handles.push_back(map_to_info(raw_handle));
        }

        sort_handles(mapped_handles, options.sortBy);
        printer.print_results(mapped_handles, options, total_raw_count);
    }

    return EXIT_SUCCESS;
}
