#ifndef POAC_OPTION_VERSION_HPP
#define POAC_OPTION_VERSION_HPP

#include <iostream>
#include <cstdlib>


namespace poac::option {
    struct version {
        static std::string summary() {
            return "Show the current poac version";
        }
        static std::string options() {
            return "<Nothing>";
        }
        template<typename VS>
        int operator()([[maybe_unused]] VS&& argv) {
            std::cout << POAC_VERSION << std::endl;
            return EXIT_SUCCESS;
        }
    };
} // end namespace
#endif // !POAC_OPTION_VERSION_HPP
