#pragma once

#include "filters.hpp"
#include "types.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class HandleEnumApp {
public:
    using Parser = CliOptions;

    int run(int argc, char* argv[]);

private:
    [[nodiscard]] HandleInfo map_to_info(const nt::RawHandle& raw_handle) const;
    static void sort_handles(std::vector<HandleInfo>& handles, SortField sort_by);
    void build_filters(const Parser& parsed_args);

    std::vector<std::unique_ptr<IHandleFilter>> m_filters;
    mutable std::unordered_map<uint32_t, std::string> m_process_name_cache;
};
