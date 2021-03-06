cmake_minimum_required(VERSION 3.5)
project(canopen_chain_node)

find_package(ament_cmake REQUIRED)
find_package(ament_index_cpp REQUIRED)
find_package(class_loader REQUIRED)

find_package(canopen_master REQUIRED)

find_package(diagnostic_updater REQUIRED)
find_package(pluginlib REQUIRED)
find_package(rclcpp REQUIRED)
find_package(socketcan_interface REQUIRED)
find_package(std_msgs REQUIRED)
find_package(std_srvs REQUIRED)
find_package(canopen_msgs REQUIRED)

find_package(Boost REQUIRED
  COMPONENTS
    filesystem
    chrono
    thread
)

include_directories(
  include
)

# create ament index resource which references the libraries in the binary dir
# set(node_plugins "")

# canopen_ros_chain
add_library(canopen_ros_chain SHARED
  src/ros_chain.cpp
)
# target_compile_definitions(canopen_ros_chain
#   PRIVATE "COMPOSITION_BUILDING_DLL")

target_link_libraries(canopen_ros_chain
  ${Boost_LIBRARIES}
)

ament_target_dependencies(canopen_ros_chain
  "class_loader"
  "canopen_master"
  "canopen_msgs"
  "std_srvs"
  "std_msgs"
  "diagnostic_updater"
  "socketcan_interface"
)
# rclcpp_register_node_plugins(canopen_ros_chain "composition::RosChain")
# set(node_plugins "${node_plugins}composition::RosChain;$<TARGET_FILE:canopen_ros_chain>\n")

# canopen_chain_node
add_executable(${PROJECT_NAME}
  src/chain_node.cpp
)
target_link_libraries(${PROJECT_NAME}
  ${Boost_LIBRARIES}
  canopen_ros_chain
)

ament_target_dependencies(${PROJECT_NAME}
  "canopen_master"
  "diagnostic_updater"
  "socketcan_interface"
  "std_srvs"
  "rclcpp"
  "canopen_msgs"
)

# since the package installs libraries without exporting them
# it needs to make sure that the library path is being exported
if(NOT WIN32)
  ament_environment_hooks(
    "${ament_cmake_package_templates_ENVIRONMENT_HOOK_LIBRARY_PATH}")
endif()


# canopen_sync_node
add_executable(canopen_sync_node
  src/sync_node.cpp
)
target_link_libraries(canopen_sync_node
  ${Boost_LIBRARIES}
)

ament_target_dependencies(canopen_sync_node
  canopen_master
  diagnostic_updater
  socketcan_interface
  rclcpp
)

ament_export_dependencies(
  ament_cmake
  canopen_master
  diagnostic_updater
  socketcan_interface
  std_srvs
  rclcpp
  canopen_msgs
  Boost
)

ament_export_include_directories(include)
ament_export_libraries(canopen_ros_chain)

install(TARGETS
  canopen_ros_chain
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

install(
  TARGETS
    ${PROJECT_NAME}
    canopen_sync_node
  DESTINATION lib/${PROJECT_NAME}
)

install(
  DIRECTORY include/
  DESTINATION include
)

# Install launch files.
install(DIRECTORY
  launch
  DESTINATION share/${PROJECT_NAME}/
)

install(
  DIRECTORY test
  DESTINATION share/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
