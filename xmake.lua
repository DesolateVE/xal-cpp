add_rules("mode.debug", "mode.release")

add_requires("nlohmann_json", "cpp-httplib", "boost")
add_requireconfs("cpp-httplib", {configs = {ssl = true}})
add_requireconfs("boost", {configs = {all = true}})
-- add_requireconfs("*", {configs = {shared = true}})

target("MSLoginLib")
    set_kind("static")
    set_languages("clatest", "cxxlatest", {public = true})
    add_files("src/**.cpp|test/*.cpp")
    add_headerfiles("src/(**.hpp)")
    add_packages("nlohmann_json", "cpp-httplib", "boost", {public = true})
    add_defines("UNICODE", "_UNICODE", "WIN32_LEAN_AND_MEAN", "NOMINMAX")

target("MSLoginTest")
    set_kind("binary")
    add_files("src/test/*.cpp")
    add_deps("MSLoginLib")
