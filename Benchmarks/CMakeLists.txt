add_executable(CustomLibrariesBenchmark Benchmark.cpp)
target_link_libraries(CustomLibrariesBenchmark PRIVATE
    CustomLibraries::CustomLibraries
)
target_compile_features(CustomLibrariesBenchmark PRIVATE cxx_std_23)
