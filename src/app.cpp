#include "app.hpp"

#include "cli_parser.hpp"
#include "nt.hpp"
#include "string_utils.hpp"

#include <algorithm>
#include <cstdlib>
#include <format>
#include <iostream>
#include <limits>
#include <memory>
#include <ranges>
#include <string>
#include <vector>

HandleInfo HandleEnumApp::map_to_info(const nt::RawHandle& raw_handle) const {
    const uint32_t pid = raw_handle.processId > static_cast<std::uintptr_t>(std::numeric_limits<uint32_t>::max())
        ? std::numeric_limits<uint32_t>::max()
        : static_cast<uint32_t>(raw_handle.processId);

    const auto type_result = nt::query_object_type(raw_handle);
    const auto name_result = nt::query_object_name(raw_handle);

    return HandleInfo{
        .pid = pid,
        .processName = "N/A",
        .handleType = type_result ? *type_result : "N/A",
        .objectName = name_result ? *name_result : "N/A",
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

    if (options.verbose) {
        std::cout << "Verbose mode is ON\n";
    }

    if (options.pid) {
        std::cout << std::format("Filtering by PID: {}\n", *options.pid);
    }

    std::cout << std::format("Retrieved {} system handles.\n", handles_result->size());

    if (options.showCountOnly) {
        std::cout << std::format("Matching handles: {}\n", filtered_handles.size());
        return EXIT_SUCCESS;
    }

    std::vector<HandleInfo> mapped_handles;
    mapped_handles.reserve(filtered_handles.size());

    for (const nt::RawHandle& raw_handle : filtered_handles) {
        mapped_handles.push_back(map_to_info(raw_handle));
    }

    sort_handles(mapped_handles, options.sortBy);

    std::cout << std::format("{:<8} {:<24} {}\n", "PID", "Type", "Name");
    for (const HandleInfo& handle : mapped_handles) {
        std::cout << std::format("{:<8} {:<24} {}\n",
                                 handle.pid,
                                 handle.handleType,
                                 handle.objectName);
    }

    std::cout << std::format("Matching handles: {}\n", mapped_handles.size());

    return EXIT_SUCCESS;
}
