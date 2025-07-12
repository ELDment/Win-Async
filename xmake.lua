add_rules("mode.debug", "mode.release")

set_project("CppCoroutine")
set_version("1.0.0")
set_languages("cxx17")

add_cxflags("cl::/utf-8", "cl::/await:strict")
add_cxflags("gcc::-finput-charset=UTF-8", "gcc::-fexec-charset=UTF-8")
add_cxflags("clang::-finput-charset=UTF-8", "clang::-fexec-charset=UTF-8")

if is_mode("debug") then
    add_defines("DEBUG_COROUTINE")
end

target("coroutine")
    set_kind("static")
    add_files(
        "src/coroutine.cpp",
        "src/scheduler.cpp",
        "src/exception_handler.cpp"
    )
    add_includedirs("include")

target("benchmark")
    set_kind("binary")
    add_files("test/benchmark.cpp")
    add_includedirs("include")
    add_deps("coroutine")