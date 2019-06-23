#ifndef POAC_SUBCMD_INSTALL_HPP
#define POAC_SUBCMD_INSTALL_HPP

#include <iostream>
#include <vector>
#include <string>
#include <string_view>
#include <fstream>
#include <map>
#include <regex>
#include <optional>
#include <cstdlib>

#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <yaml-cpp/yaml.h>

#include "../io.hpp"
#include "../core/except.hpp"
#include "../core/deper/resolver.hpp"
#include "../core/deper/lock.hpp"
#include "../util.hpp"

namespace poac::subcmd {
    namespace _install {
        void stream_deps(YAML::Emitter& out, const core::deper::resolver::Activated& deps) {
            out << YAML::Key << "dependencies";
            out << YAML::Value << YAML::BeginMap;

            for (const auto& dep : deps) {
                out << YAML::Key << dep.name;
                out << YAML::Value << YAML::BeginMap;

                out << YAML::Key << "version";
                out << YAML::Value << dep.version;

                out << YAML::Key << "source";
                out << YAML::Value << dep.source;

                if (!dep.deps.empty()) {
                    stream_deps(out, dep.deps);
                }
                out << YAML::EndMap;
            }
            out << YAML::EndMap;
        }

        void create_lock_file(const std::string& timestamp, const core::deper::resolver::Activated& activated_deps) {
            // Create a poac.lock
            if (std::ofstream ofs("poac.lock"); ofs) {
                ofs << "# Please do not edit this file.\n";
                YAML::Emitter out;
                out << YAML::BeginMap;

                out << YAML::Key << "timestamp";
                out << YAML::Value << timestamp;

                stream_deps(out, activated_deps);
                ofs << out.c_str() << '\n';
            }
        }


        // Copy package to ./deps
        bool copy_to_current(const std::string& from, const std::string& to) {
            namespace path = io::path;
            const auto from_path = path::poac_cache_dir / from;
            const auto to_path = path::current_deps_dir / to;
            return path::recursive_copy(from_path, to_path);
        }

        void echo_install_status(const bool res, const std::string& n, const std::string& v, const std::string& s) {
            namespace cli = io::cli;
            const std::string status = n + " " + v + " (from: " + s + ")";
            std::cout << '\r' << cli::clr_line << (res ? cli::fetched : cli::fetch_failed) << status << std::endl;
        }

        void fetch_packages( // TODO: そもそも関数が長くてキモい．
                const core::deper::resolver::Backtracked& deps,
                const bool quite,
                const bool verbose)
        {
            namespace name = core::name;
            namespace path = io::path;
            namespace resolver = core::deper::resolver;

            int exists_count = 0;
            for (const auto& [name, dep] : deps) {
                const auto cache_name = name::to_cache(dep.source, name, dep.version);
                const auto current_name = name::to_current(dep.source, name, dep.version);
                const bool is_cached = resolver::cache::resolve(cache_name);

                if (verbose) {
                    std::cout << "NAME: " << name << "\n"
                              << "  VERSION: " <<  dep.version << "\n"
                              << "  SOURCE: " << dep.source << "\n"
                              << "  CACHE_NAME: " << cache_name << "\n"
                              << "  CURRENT_NAME: " << current_name << "\n"
                              << "  IS_CACHED: " << is_cached << "\n"
                              << std::endl;
                }

                if (resolver::current::resolve(current_name)) {
                    ++exists_count;
                    continue;
                }
                else if (is_cached) {
                    const bool res = copy_to_current(cache_name, current_name);
                    if (!quite) {
                        echo_install_status(res, name, dep.version, dep.source);
                    }
                }
                else if (dep.source == "poac") {
                    const auto pkg_dir = path::poac_cache_dir / cache_name;
                    const auto tar_dir = pkg_dir.string() + ".tar.gz";
                    const std::string target = resolver::archive_url(name, dep.version);

                    { // Get archive file
                        std::ofstream output_file(tar_dir, std::ios::out | std::ios::binary);
                        const io::net::requests req{};
                        req.get(target, {}, std::move(output_file));
                    }
                    // If res is true, does not execute func. (short-circuit evaluation)
                    bool result = io::tar::extract_spec_rm(tar_dir, pkg_dir);
                    result = result && copy_to_current(cache_name, current_name);

                    if (!quite) {
                        echo_install_status(result, name, dep.version, dep.source);
                    }
                }
                else if (dep.source == "github") {
                    util::shell clone_cmd = resolver::github::clone_command(name, dep.version);
                    clone_cmd += (path::poac_cache_dir / cache_name).string();
                    clone_cmd = clone_cmd.to_dev_null().stderr_to_stdout();

                    bool result = static_cast<bool>(clone_cmd.exec()); // true == error
                    result = !result && copy_to_current(cache_name, current_name);

                    if (!quite) {
                        echo_install_status(result, name, dep.version, dep.source);
                    }
                }
                else {
                    // If called this, it is a bug.
                    namespace except = core::except;
                    throw except::error("Unexcepted error");
                }
            }
            if (exists_count == static_cast<int>(deps.size())) {
                std::cout << io::cli::warning << "Already installed" << std::endl;
            }
        }

        core::deper::resolver::Deps
        resolve_packages(const std::map<std::string, YAML::Node>& node) {
            namespace except = core::except;
            namespace name = core::name;
            namespace resolver = core::deper::resolver;

            resolver::Deps deps;

            // Even if a package of the same name is written, it is excluded.
            // However, it can not deal with duplication of other information (e.g. version etc.).
            for (const auto& [name, next_node] : node) {
                // itr->first: itr->second
                const auto [source, parsed_name] = name::get_source(name);
                const auto interval = name::get_version(next_node, source);

                if (source == "poac" || source == "github") {
                    deps.push_back({ {parsed_name}, {interval}, {source} });
                }
                else { // unknown source
                    throw except::error("Unknown source");
                }
            }
            return deps;
        }

        core::deper::resolver::Package<core::deper::resolver::Name, core::deper::resolver::Interval, core::deper::resolver::Source>
        parse_arg_package(const std::string& v) {
            namespace except = core::except;
            namespace name = core::name;

            name::validate_package_name(v);

            const std::string NAME = "([a-z|\\d|\\-|_|\\/]*)";
            std::smatch match;
            if (std::regex_match(v, std::regex("^" + NAME + "$"))) { // TODO: 厳しくする
                const auto [source, parsed_name] = name::get_source(v);
                return { {parsed_name}, {"latest"}, {source} };
            }
            else if (std::regex_match(v, match, std::regex("^" + NAME + "=(.*)$"))) {
                const auto name = match[1].str();
                const auto [source, parsed_name] = name::get_source(name);
                const auto interval = match[2].str();
                return { {parsed_name}, {interval}, {source} };
            }
            else {
                throw except::error("Invalid arguments");
            }
        }

        std::string convert_to_interval(const std::string &version) {
            core::deper::semver::Version upper(version);
            upper.major += 1;
            upper.minor = 0;
            upper.patch = 0;
            return ">=" + version + " and <" + upper.get_version();
        }


        template<typename VS>
        int _main(VS&& argv) {
            namespace fs = boost::filesystem;
            namespace except = core::except;
            namespace path = io::path;
            namespace yaml = io::yaml;
            namespace cli = io::cli;
            namespace resolver = core::deper::resolver;


            fs::create_directories(path::poac_cache_dir);
            auto node = yaml::load_config();
            std::string timestamp = yaml::get_timestamp();
            const bool quite = util::argparse::use_rm(argv, "-q", "--quite");
            const bool verbose = util::argparse::use_rm(argv, "-v", "--verbose") && !quite;

            // load lock file
            resolver::Resolved resolved_deps{};
            bool load_lock = false;
            if (argv.empty()) { // 引数からの指定の時，lockファイルを無視する
                if (const auto locked_deps = core::deper::lock::load(timestamp)) {
                    resolved_deps = *locked_deps;
                    load_lock = true;
                }
            }

            // YAML::Node -> resolver:Deps
            resolver::Deps deps;
            if (!argv.empty()) {
                for (const auto& v : argv) {
                    deps.push_back(parse_arg_package(v));
                }
            }
            if (!load_lock) {
                if (const auto deps_node = yaml::get<std::map<std::string, YAML::Node>>(node, "deps")) {
                    const auto resolved_packages = resolve_packages(*deps_node);
                    deps.insert(deps.end(), resolved_packages.begin(), resolved_packages.end());
                }
                else if (argv.empty()) { // 引数から指定しておらず(poac install)，poac.ymlにdeps keyが存在しない
                    throw except::error(
                            "Required key `deps` does not exist in poac.yml.\n"
                            "Please refer to https://doc.poac.pm");
                }
            }

            // resolve dependency
            if (!quite) {
                std::cout << cli::status << "Resolving dependencies..." << std::endl;
            }
            if (!load_lock) {
                resolved_deps = resolver::resolve(deps);
            }

            // download packages
            if (!quite) {
                std::cout << cli::status << "Fetching..." << std::endl;
                std::cout << std::endl;
            }
            fs::create_directories(path::current_deps_dir);
            fetch_packages(resolved_deps.backtracked, quite, verbose);
            if (!quite) {
                std::cout << std::endl;
                cli::status_done();
            }

            // Rewrite poac.yml
            bool fix_yml = false;
            for (const auto& d : deps) {
                if (d.interval == "latest") {
                    node["deps"][d.name] = convert_to_interval(resolved_deps.backtracked[d.name].version);
                    fix_yml = true;
                }
            }
            if (!argv.empty()) {
                fix_yml = true;
                for (const auto& d : deps) {
                    if (d.interval != "latest") {
                        node["deps"][d.name] = d.interval;
                    }
                }
            }
            if (fix_yml) {
                if (std::ofstream ofs("poac.yml"); ofs) {
                    ofs << node;
                }
                timestamp = yaml::get_timestamp();
            }

            if (!load_lock) {
                create_lock_file(timestamp, resolved_deps.activated);
            }

            return EXIT_SUCCESS;
        }
    }

    struct install {
        static std::string summary() {
            return "Install packages";
        }
        static std::string options() {
            return "-v | --verbose, -q | --quite, [args]";
        }
        template<typename VS>
        int operator()(VS&& argv) {
            return _install::_main(std::forward<VS>(argv));
        }
    };
} // end namespace
#endif // !POAC_SUBCMD_INSTALL_HPP
