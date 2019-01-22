
add_executable(tcp_echo tcp_echo.c)
target_link_libraries(tcp_echo bee -levent)

add_executable(udp_echo udp_echo.c)
target_link_libraries(udp_echo bee -levent)

add_executable(mcast_receive mcast_receive.c)
target_link_libraries(mcast_receive bee -levent)

add_executable(httpd httpd.c)
target_link_libraries(httpd bee beehelper -levent)

add_executable(telnetd telnetd.c)
target_link_libraries(telnetd bee -levent)

add_executable(bee_httpd bee_httpd.c)
target_link_libraries(bee_httpd bee beehelper -levent)

add_executable(bee_cli bee_cli.c)
target_link_libraries(bee_cli bee beehelper -levent)
