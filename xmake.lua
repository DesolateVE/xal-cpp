add_rules("mode.debug", "mode.release")

add_requires("nlohmann_json", "cpp-httplib", "openssl3", "cpr")
add_requireconfs("cpp-httplib", {configs = {ssl = true}})
add_requireconfs("cpr", {configs = {ssl = true}})
add_requireconfs("*", {configs = {vs_runtime = "MD"}})

target("MSLogin")
    set_kind("shared")
    set_languages("clatest", "cxxlatest")
    add_files("src/**.cpp")
    add_includedirs("src", {public = true})
    add_packages("nlohmann_json", "cpp-httplib", "boost", "openssl3", "cpr")
    add_defines("WIN32_LEAN_AND_MEAN", "NOMINMAX", "MSLOGIN_EXPORTS")
    