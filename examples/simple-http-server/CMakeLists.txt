add_executable(simple-http-server until.c server.c connection.c http.c request.c config.c)

target_link_libraries(simple-http-server ae)
