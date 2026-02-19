#pragma once

#include "filters.hpp"
#include "types.hpp"

#include <memory>
#include <vector>

class HandleEnumApp {
public:
    using Parser = CliOptions;

    int run(int argc, char* argv[]);

private:
    void build_filters(const Parser& parsed_args);

    std::vector<std::unique_ptr<IHandleFilter>> m_filters;
};
