include(GoogleTest)

add_executable(wrl_tests
    test_helpers.cpp
    test_physics.cpp
    test_planning.cpp
    test_sensors.cpp
    test_tasks.cpp
    test_env.cpp
    test_session.cpp
    test_generator.cpp)
target_link_libraries(wrl_tests PRIVATE wrl_core wrl_options GTest::gtest_main)
gtest_discover_tests(wrl_tests DISCOVERY_TIMEOUT 60)

if(WRL_BUILD_AGENT)
    add_executable(wrl_agent_tests test_sac.cpp)
    target_link_libraries(wrl_agent_tests PRIVATE wrl_agent wrl_options GTest::gtest_main)
    gtest_discover_tests(wrl_agent_tests DISCOVERY_TIMEOUT 60)
endif()
