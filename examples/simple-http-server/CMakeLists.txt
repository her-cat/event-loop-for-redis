add_executable(simple-http-server until.c main.c connection.c)

target_link_libraries(simple-http-server ae)
