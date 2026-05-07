#include <iostream>

#include "app_config.hpp"
#include "server.hpp"
#include "utils.hpp"

int main() {
    try {
        const auto baseDir = get_executable_directory();
        const auto config = load_config(baseDir);
        run_server(config);
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Falha ao iniciar a aplicacao: " << ex.what() << std::endl;
        return 1;
    }
}

