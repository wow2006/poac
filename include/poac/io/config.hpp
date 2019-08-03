#ifndef POAC_IO_CONFIG_HPP
#define POAC_IO_CONFIG_HPP

#include <algorithm>
#include <string>
#include <string_view>
#include <unordered_map>
#include <optional>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/algorithm/string/join.hpp>
#include <toml.hpp>

#include <poac/core/except.hpp>
#include <poac/io/path.hpp>

namespace poac::io::config {
    namespace detail {
        inline boost::system::error_code ec{};

        [[noreturn]] inline void
        rethrow_bad_cast(const std::string& what) {
            throw toml::type_error(
                    "[error] value type should be" + what.substr(what.rfind(' ', what.find('\n'))));
        }

        //
        // find and force T type
        // If it cannot converted to T, throw exception.
        //
        template <typename T, typename C,
                template <typename...> class M,
                template <typename...> class V>
        decltype(toml::get<T>(std::declval<const toml::basic_value<C, M, V>&>()))
        find_force(const toml::basic_value<C, M, V>& v, const toml::key& key) {
            try {
                return toml::find<T>(v, key);
            } catch (const toml::type_error& e) {
                rethrow_bad_cast(e.what());
            }
        }
        template <typename T, typename C,
                template <typename...> class M,
                template <typename...> class V>
        decltype(toml::get<T>(std::declval<toml::basic_value<C, M, V>&>()))
        find_force(toml::basic_value<C, M, V>& v, const toml::key& key) {
            try {
                return toml::find<T>(v, key);
            } catch (const toml::type_error& e) {
                rethrow_bad_cast(e.what());
            }
        }
        template <typename T, typename C,
                template <typename...> class M,
                template <typename...> class V>
        decltype(toml::get<T>(std::declval<toml::basic_value<C, M, V>&&>()))
        find_force(toml::basic_value<C, M, V>&& v, const toml::key& key) {
            try {
                return toml::find<T>(std::move(v), key);
            } catch (const toml::type_error& e) {
                rethrow_bad_cast(e.what());
            }
        }

        template <typename T, typename C, template <typename...> class M,
                template <typename...> class V, typename... Ts>
        decltype(toml::get<T>(std::declval<const toml::basic_value<C, M, V>&>()))
        find_force(const toml::basic_value<C, M, V>& v, const toml::key& key, Ts&&... keys) {
            return find_force<T>(find_force(v, key), std::forward<Ts>(keys)...);
        }
        template <typename T, typename C, template <typename...> class M,
                template <typename...> class V, typename... Ts>
        decltype(toml::get<T>(std::declval<toml::basic_value<C, M, V>&>()))
        find_force(toml::basic_value<C, M, V>& v, const toml::key& key, Ts&&... keys) {
            return find_force<T>(find_force(v, key), std::forward<Ts>(keys)...);
        }
        template <typename T, typename C, template <typename...> class M,
                template <typename...> class V, typename... Ts>
        decltype(toml::get<T>(std::declval<toml::basic_value<C, M, V>&&>()))
        find_force(toml::basic_value<C, M, V>&& v, const toml::key& key, Ts&&... keys) {
            return find_force<T>(find_force(std::move(v), key), std::forward<Ts>(keys)...);
        }

        //
        // find and check possible values
        // If value cannot convert to T, or if value does not exist, throw exception.
        //
        template <typename T, typename C, template <typename...> class M, template <typename...> class V>
        decltype(toml::get<T>(std::declval<const toml::basic_value<C, M, V>&>()))
        find_enum(const toml::basic_value<C, M, V>& v, const toml::key& key, std::vector<T>&& pv) {
            const T value = find_force<T>(v, key);
            if (std::any_of(pv.cbegin(), pv.cend(), [&](T x){ return x == value; })) {
                return value;
            } else {
                std::vector<std::string> pvs(pv.size());
                std::transform(pv.cbegin(), pv.cend(), pvs.begin(), [](T x){ return std::to_string(x); });
                const auto f = "[error] value should be any of [" + boost::algorithm::join(pvs, ", ") + "]";
                throw toml::type_error(toml::format_error(
                        f, toml::get<toml::table>(v).at(key),
                        "one of the above listed is required"));
            }
        }
        template <typename T, typename C, template <typename...> class M, template <typename...> class V>
        decltype(toml::get<T>(std::declval<toml::basic_value<C, M, V>&>()))
        find_enum(toml::basic_value<C, M, V>& v, const toml::key& key, std::vector<T>&& pv) {
            const T value = find_force<T>(v, key);
            if (std::any_of(pv.cbegin(), pv.cend(), [&](T x){ return x == value; })) {
                return value;
            } else {
                std::vector<std::string> pvs(pv.size());
                std::transform(pv.cbegin(), pv.cend(), pvs.begin(), [](T x){ return std::to_string(x); });
                const auto f = "[error] value should be any of [" + boost::algorithm::join(pvs, ", ") + "]";
                throw toml::type_error(toml::format_error(
                        f, toml::get<toml::table>(v).at(key),
                        "one of the above listed is required"));
            }
        }
        template <typename T, typename C, template <typename...> class M, template <typename...> class V>
        decltype(toml::get<T>(std::declval<toml::basic_value<C, M, V>&&>()))
        find_enum(toml::basic_value<C, M, V>&& v, const toml::key& key, std::vector<T>&& pv) {
            const T value = find_force<T>(std::move(v), key);
            if (std::any_of(pv.cbegin(), pv.cend(), [&](T x){ return x == value; })) {
                return value;
            } else {
                std::vector<std::string> pvs(pv.size());
                std::transform(pv.cbegin(), pv.cend(), pvs.begin(), [](T x){ return std::to_string(x); });
                const auto f = "[error] value should be any of [" + boost::algorithm::join(pvs, ", ") + "]";
                throw toml::type_error(toml::format_error(
                        f, toml::get<toml::table>(v).at(key),
                        "one of the above listed is required"));
            }
        }

        //
        // find and check possible values
        // If it cannot converted to T, throw exception, and if the value does not exist, return std::nullopt.
        //
        template <typename T, typename C, template <typename...> class M, template <typename...> class V>
        std::optional<std::remove_reference_t<
            decltype(toml::get<T>(std::declval<const toml::basic_value<C, M, V>&>()))
        >>
        find_enum_opt(const toml::basic_value<C, M, V>& v, const toml::key& key, std::vector<T>&& pv) {
            try {
                return find_enum<T>(v, key, std::move(pv));
            } catch (const toml::type_error& e) {
                throw e;
            } catch (...) {
                return std::nullopt;
            }
        }
        template <typename T, typename C, template <typename...> class M, template <typename...> class V>
        std::optional<std::remove_reference_t<
            decltype(toml::get<T>(std::declval<toml::basic_value<C, M, V>&>()))
        >>
        find_enum_opt(toml::basic_value<C, M, V>& v, const toml::key& key, std::vector<T>&& pv) {
            try {
                return find_enum<T>(v, key, std::move(pv));
            } catch (const toml::type_error& e) {
                throw e;
            } catch (...) {
                return std::nullopt;
            }
        }
        template <typename T, typename C, template <typename...> class M, template <typename...> class V>
        std::optional<std::remove_reference_t<
            decltype(toml::get<T>(std::declval<toml::basic_value<C, M, V>&&>()))
        >>
        find_enum_opt(toml::basic_value<C, M, V>&& v, const toml::key& key, std::vector<T>&& pv) {
            try {
                return find_enum<T>(std::move(v), key, std::move(pv));
            } catch (const toml::type_error& e) {
                throw e;
            } catch (...) {
                return std::nullopt;
            }
        }

        //
        // find and force T type
        // If it cannot converted to T, throw exception, and if the value does not exist, return std::nullopt.
        //
        template <typename T, typename C, template <typename...> class M, template <typename...> class V>
        std::optional<std::remove_reference_t<
            decltype(toml::get<T>(std::declval<const toml::basic_value<C, M, V>&>()))
        >>
        find_force_opt(const toml::basic_value<C, M, V>& v, const toml::key& key) {
            try {
                return find_force<T>(v, key);
            } catch (const toml::type_error& e) {
                throw e;
            } catch (...) {
                return std::nullopt;
            }
        }
        template <typename T, typename C, template <typename...> class M, template <typename...> class V>
        std::optional<std::remove_reference_t<
            decltype(toml::get<T>(std::declval<toml::basic_value<C, M, V>&>()))
        >>
        find_force_opt(toml::basic_value<C, M, V>& v, const toml::key& key) {
            try {
                return find_force<T>(v, key);
            } catch (const toml::type_error& e) {
                throw e;
            } catch (...) {
                return std::nullopt;
            }
        }
        template <typename T, typename C, template <typename...> class M, template <typename...> class V>
        std::optional<std::remove_reference_t<
            decltype(toml::get<T>(std::declval<toml::basic_value<C, M, V>&&>()))
        >>
        find_force_opt(toml::basic_value<C, M, V>&& v, const toml::key& key) {
            try {
                return find_force<T>(std::move(v), key);
            } catch (const toml::type_error& e) {
                throw e;
            } catch (...) {
                return std::nullopt;
            }
        }

        template <typename T, typename C, template <typename...> class M,
                template <typename...> class V, typename... Ts>
        std::optional<std::remove_reference_t<
            decltype(toml::get<T>(std::declval<const toml::basic_value<C, M, V>&>()))
        >>
        find_force_opt(const toml::basic_value<C, M, V>& v, const toml::key& key, Ts&&... keys) {
            return find_force_opt<T>(find_force_opt(v, key), std::forward<Ts>(keys)...);
        }
        template <typename T, typename C, template <typename...> class M,
                template <typename...> class V, typename... Ts>
        std::optional<std::remove_reference_t<
            decltype(toml::get<T>(std::declval<toml::basic_value<C, M, V>&>()))
        >>
        find_force_opt(toml::basic_value<C, M, V>& v, const toml::key& key, Ts&&... keys) {
            return find_force_opt<T>(find_force_opt(v, key), std::forward<Ts>(keys)...);
        }
        template <typename T, typename C, template <typename...> class M,
                template <typename...> class V, typename... Ts>
        std::optional<std::remove_reference_t<
            decltype(toml::get<T>(std::declval<toml::basic_value<C, M, V>&&>()))
        >>
        find_force_opt(toml::basic_value<C, M, V>&& v, const toml::key& key, Ts&&... keys) {
            return find_force_opt<T>(find_force_opt(std::move(v), key), std::forward<Ts>(keys)...);
        }

        //
        // find as optional
        // If value cannot convert to T, or if value does not exist, return std::nullopt.
        //
        template <typename T, typename C, template <typename...> class M, template <typename...> class V>
        std::optional<std::remove_reference_t<
            decltype(toml::get<T>(std::declval<const toml::basic_value<C, M, V>&>()))
        >>
        find_opt(const toml::basic_value<C, M, V>& v, const toml::key& key) noexcept {
            try {
                return find_force_opt<T>(v, key);
            } catch (...) {
                return std::nullopt;
            }
        }
        template <typename T, typename C, template <typename...> class M, template <typename...> class V>
        std::optional<std::remove_reference_t<
            decltype(toml::get<T>(std::declval<toml::basic_value<C, M, V>&>()))
        >>
        find_opt(toml::basic_value<C, M, V>& v, const toml::key& key) noexcept {
            try {
                return find_force_opt<T>(v, key);
            } catch (...) {
                return std::nullopt;
            }
        }
        template <typename T, typename C, template <typename...> class M, template <typename...> class V>
        std::optional<std::remove_reference_t<
            decltype(toml::get<T>(std::declval<toml::basic_value<C, M, V>&&>()))
        >>
        find_opt(toml::basic_value<C, M, V>&& v, const toml::key& key) noexcept {
            try {
                return find_force_opt<T>(std::move(v), key);
            } catch (...) {
                return std::nullopt;
            }
        }

        template <typename T, typename C, template <typename...> class M,
                template <typename...> class V, typename... Ts>
        std::optional<std::remove_reference_t<
            decltype(toml::get<T>(std::declval<const toml::basic_value<C, M, V>&>()))
        >>
        find_opt(const toml::basic_value<C, M, V>& v, const toml::key& key, Ts&&... keys) noexcept {
            return find_opt<T>(find_opt(v, key), std::forward<Ts>(keys)...);
        }
        template <typename T, typename C, template <typename...> class M,
                template <typename...> class V, typename... Ts>
        std::optional<std::remove_reference_t<
            decltype(toml::get<T>(std::declval<toml::basic_value<C, M, V>&>()))
        >>
        find_opt(toml::basic_value<C, M, V>& v, const toml::key& key, Ts&&... keys) noexcept {
            return find_opt<T>(find_opt(v, key), std::forward<Ts>(keys)...);
        }
        template <typename T, typename C, template <typename...> class M,
                template <typename...> class V, typename... Ts>
        std::optional<std::remove_reference_t<
            decltype(toml::get<T>(std::declval<toml::basic_value<C, M, V>&&>()))
        >>
        find_opt(toml::basic_value<C, M, V>&& v, const toml::key& key, Ts&&... keys) noexcept {
            return find_opt<T>(find_opt(std::move(v), key), std::forward<Ts>(keys)...);
        }

        std::optional<std::string>
        validate_config(const boost::filesystem::path& base = path::current) noexcept {
            const auto config_path = base / "poac.toml";
            if (boost::filesystem::exists(config_path, ec)) {
                return config_path.string();
            } else {
                return std::nullopt;
            }
        }
    }

#define PARSE_FIELD( fn, value, key ) \
key = fn<decltype(key)>(value, #key)

#define PARSE_FIELD_OPT2( fn, value, key, field ) \
field = fn<decltype(field)::value_type>(value, key)

#define PARSE_FIELD_OPT( fn, value, key ) \
PARSE_FIELD_OPT2(fn, value, #key, key)


    // https://doc.poac.pm/en/reference/manifest.html#the-package-section
    struct Package {
        std::string name; // required
        std::string version; // required
        std::optional<std::vector<std::string>> authors; // optional
        std::uint16_t cpp; // optional with default(17)
        std::optional<std::string> build; // optional
        std::optional<std::string> links; // optional
        std::optional<std::string> description; // optional
        std::optional<std::string> documentation; // optional
        std::optional<std::string> homepage; // optional
        std::optional<std::string> repository; // optional
        std::optional<std::string> readme; // optional
        std::optional<std::string> license; // optional
        std::optional<std::string> license_file; // optional

        void from_toml(const toml::value& v) {
            name = detail::find_force<decltype(name)>(v, "name");
            PARSE_FIELD(detail::find_force, v, version);
            PARSE_FIELD_OPT(detail::find_force_opt, v, authors);
            cpp = detail::find_enum_opt<decltype(cpp)>(v, "cpp", {98, 3, 11, 14, 17, 20}).value_or(17);
            PARSE_FIELD_OPT(detail::find_force_opt, v, build);
            PARSE_FIELD_OPT(detail::find_force_opt, v, links);
            PARSE_FIELD_OPT(detail::find_force_opt, v, description);
            PARSE_FIELD_OPT(detail::find_force_opt, v, documentation);
            PARSE_FIELD_OPT(detail::find_force_opt, v, homepage);
            PARSE_FIELD_OPT(detail::find_force_opt, v, repository);
            PARSE_FIELD_OPT(detail::find_force_opt, v, readme);
            PARSE_FIELD_OPT(detail::find_force_opt, v, license);
            PARSE_FIELD_OPT2(detail::find_force_opt, v, "license-file", license_file);
        }

        toml::table into_toml() const {
            toml::table t{};
            t.emplace("name", name);
            t.emplace("version", version);
            if (authors.has_value()) {
                t.emplace("authors", authors.value());
            }
            t.emplace("cpp", cpp);
            if (build.has_value()) {
                t.emplace("build", build.value());
            }
            if (links.has_value()) {
                t.emplace("links", links.value());
            }
            if (description.has_value()) {
                t.emplace("description", description.value());
            }
            if (documentation.has_value()) {
                t.emplace("documentation", documentation.value());
            }
            if (homepage.has_value()) {
                t.emplace("homepage", homepage.value());
            }
            if (repository.has_value()) {
                t.emplace("repository", repository.value());
            }
            if (readme.has_value()) {
                t.emplace("readme", readme.value());
            }
            if (license.has_value()) {
                t.emplace("license", license.value());
            }
            if (license_file.has_value()) {
                t.emplace("license-file", license_file.value());
            }
            return t;
        }
    };

    struct Config {
        struct Build {
            enum class System {
                Poac,
                CMake,
            };
            struct Bin {
                std::optional<std::string> path;
                std::optional<std::string> name;
                std::optional<std::string> link;

                template <typename C, template <typename ...> class M, template <typename ...> class V>
                void from_toml(const toml::basic_value<C, M, V>& v) noexcept;
                toml::table into_toml() const;
            };
            struct Properties {
                std::optional<std::vector<std::string>> definitions;
                std::optional<std::vector<std::string>> options;
                std::optional<std::vector<std::string>> libraries;

                template <typename C, template <typename ...> class M, template <typename ...> class V>
                void from_toml(const toml::basic_value<C, M, V>& v) noexcept;
                toml::table into_toml() const;
            };

            std::optional<System> system;
            std::optional<std::vector<Bin>> bins;
            std::optional<Properties> properties;

            template <typename C, template <typename ...> class M, template <typename ...> class V>
            void from_toml(const toml::basic_value<C, M, V>& v) noexcept;
            toml::table into_toml() const;
        };

        Package package;
        std::optional<std::uint16_t> cpp;
        std::optional<std::unordered_map<std::string, std::string>> dependencies;
        std::optional<std::unordered_map<std::string, std::string>> dev_dependencies;
        std::optional<std::unordered_map<std::string, std::string>> build_dependencies;
        std::optional<Build> build;
//        std::optional<std::unordered_map<std::string, toml::value>> target;

        template <typename C, template <typename ...> class M, template <typename ...> class V>
        void from_toml(const toml::basic_value<C, M, V>& v);
        toml::table into_toml() const;
    };

    namespace detail {
        std::optional<Config::Build::System>
        to_build_system(const std::optional<std::string>& str) noexcept {
            if (!str.has_value()) {
                // If not specified poac will be selected as default.
                return Config::Build::System::Poac;
            } else if (str.value() == "poac") {
                return Config::Build::System::Poac;
            } else if (str.value() == "cmake") {
                return Config::Build::System::CMake;
            } else {
                return std::nullopt;
            }
        }

        std::string
        to_string(Config::Build::System system) noexcept {
            switch (system) {
                case Config::Build::System::Poac:
                    return "poac";
                case Config::Build::System::CMake:
                    return "cmake";
            }
        }
    }

    template <typename C, template <typename ...> class M, template <typename ...> class V>
    void Config::from_toml(const toml::basic_value<C, M, V>& v) {
        PARSE_FIELD(toml::find, v, package);
//        PARSE_FIELD_OPT(detail::find_force_opt, v, cpp);
        std::cout << package.cpp << std::endl;



        // TODO: ここで，cfgのパースをする..?? -> tomlとして，exceptionを出したい．

        for (const auto& [key, value] : toml::find<toml::table>(v, "target")) {
            std::cout << key << std::endl;
            if (key == "cfg(os = \"macos\")") {
                for (const auto& [key2, value2] : toml::find<toml::table>(value, "profile")) {
                    std::cout << key2 << std::endl;
                }
            }
            // TODO: keyを一個ずつ，parseしていく！！！ -> もし，存在しないものとか，文法エラーは，toml::format_errorとしてthrow
        }

        PARSE_FIELD_OPT(detail::find_force_opt, v, dependencies);
        PARSE_FIELD_OPT2(detail::find_force_opt, v, "dev-dependencies", dev_dependencies);
        PARSE_FIELD_OPT2(detail::find_force_opt, v, "build-dependencies", build_dependencies);
        PARSE_FIELD_OPT(detail::find_force_opt, v, build);
    }
    toml::table Config::into_toml() const {
        toml::table t{};
        if (cpp.has_value()) {
            t.emplace("cpp", cpp.value());
        }
        if (dependencies.has_value()) {
            t.emplace("dependencies", dependencies.value());
        }
        if (dev_dependencies.has_value()) {
            t.emplace("dev-dependencies", dev_dependencies.value());
        }
        if (build_dependencies.has_value()) {
            t.emplace("build-dependencies", build_dependencies.value());
        }
        if (build.has_value()) {
            t.emplace("build", build.value());
        }
        return t;
    }

    template <typename C, template <typename ...> class M, template <typename ...> class V>
    void Config::Build::from_toml(const toml::basic_value<C, M, V>& v) noexcept {
        system = detail::to_build_system(detail::find_opt<std::string>(v, "system"));
        bins = detail::find_opt<std::vector<Bin>>(v, "bin");
        properties = detail::find_opt<Properties>(v, "properties");
    }
    toml::table Config::Build::into_toml() const {
        toml::table t{};
        if (system.has_value()) {
            t.emplace("system", detail::to_string(system.value()));
        }
        if (bins.has_value()) {
            t.emplace("bin", bins.value());
        }
        if (properties.has_value()) {
            t.emplace("properties", properties.value());
        }
        return t;
    }

    template <typename C, template <typename ...> class M, template <typename ...> class V>
    void Config::Build::Bin::from_toml(const toml::basic_value<C, M, V>& v) noexcept {
        path = detail::find_opt<std::string>(v, "path");
        name = detail::find_opt<std::string>(v, "name");
        link = detail::find_opt<std::string>(v, "link");
    }
    toml::table Config::Build::Bin::into_toml() const {
        toml::table t{};
        if (path.has_value()) {
            t.emplace("path", path.value());
        }
        if (name.has_value()) {
            t.emplace("name", name.value());
        }
        if (link.has_value()) {
            t.emplace("link", link.value());
        }
        return t;
    }

    template <typename C, template <typename ...> class M, template <typename ...> class V>
    void Config::Build::Properties::from_toml(const toml::basic_value<C, M, V>& v) noexcept {
        definitions = detail::find_opt<std::vector<std::string>>(v, "definitions");
        options = detail::find_opt<std::vector<std::string>>(v, "options");
        libraries = detail::find_opt<std::vector<std::string>>(v, "libraries");
    }
    toml::table Config::Build::Properties::into_toml() const {
        toml::table t{};
        if (definitions.has_value()) {
            t.emplace("definitions", definitions.value());
        }
        if (options.has_value()) {
            t.emplace("options", options.value());
        }
        if (libraries.has_value()) {
            t.emplace("libraries", libraries.value());
        }
        return t;
    }

    template <typename C>
    std::optional<C>
    load_toml(const boost::filesystem::path& base, const std::string& fname) {
        if (boost::filesystem::exists(base / fname, detail::ec)) {
            const auto config_toml = toml::parse<toml::preserve_comments>(fname);
            const auto config = toml::get<C>(config_toml);
            return config;
        } else {
            return std::nullopt;
        }
    }

    std::optional<Config>
    load(const boost::filesystem::path& base = path::current) {
         return load_toml<Config>(base, "poac.toml");
    }

    std::string
    get_timestamp() {
        if (const auto filename = config::detail::validate_config()) {
            return std::to_string(boost::filesystem::last_write_time(filename.value(), config::detail::ec));
        } else {
            throw core::except::error(
                    core::except::msg::does_not_exist("poac.toml"), "\n",
                    core::except::msg::please_exec("`poac init` or `poac new $PROJNAME`"));
        }
    }
} // end namespace
#endif // !POAC_IO_CONFIG_HPP
