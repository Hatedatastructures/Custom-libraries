add_executable(HttpsServer HttpsServer.cpp)
target_link_libraries(HttpsServer PRIVATE CustomLibraries::CustomLibraries)

add_executable(HttpsClient HttpsClient.cpp)
target_link_libraries(HttpsClient PRIVATE CustomLibraries::CustomLibraries)
