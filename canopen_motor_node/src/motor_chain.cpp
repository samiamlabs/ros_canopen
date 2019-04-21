#include <canopen_motor_node/handle_layer.hpp>
#include <canopen_motor_node/motor_chain.hpp>

using namespace canopen;

// TODO(sam): implement this
class RosSettings : public Settings {
public:
  RosSettings() {}
  // XmlRpcSettings(const XmlRpc::XmlRpcValue &v) : value_(v) {}
  // XmlRpcSettings& operator=(const XmlRpc::XmlRpcValue &v) { value_ = v;
  // return *this; }
private:
  virtual bool getRepr(const std::string &n, std::string &repr) const {
    // if(value_.hasMember(n)){
    //     std::stringstream sstr;
    //     sstr << const_cast< XmlRpc::XmlRpcValue &>(value_)[n]; // does not
    //     write since already existing
    //     repr = sstr.str();
    //     return true;
    // }
    return false;
  }
  // XmlRpc::XmlRpcValue value_;
};

// MotorChain::MotorChain(const ros::NodeHandle &nh, const ros::NodeHandle
// &nh_priv) :
//         RosChain(nh, nh_priv), motor_allocator_("canopen_402",
//         "canopen::MotorBase::Allocator") {}

MotorChain::MotorChain(std::string node_name)
    : RosChain(node_name),
      motor_allocator_("canopen_402", "canopen::MotorBase::Allocator") {}

bool MotorChain::nodeAdded(const canopen::NodeSharedPtr &node,
                           const LoggerSharedPtr &logger) {
  RCLCPP_INFO(this->get_logger(), "adding node");

  std::string name = "joint_m0";
  // std::string name = params["name"];
  std::string &joint = name;
  // if(params.hasMember("joint")) joint.assign(params["joint"]);
  //
  // if(!robot_layer_->getJoint(joint)){
  //     ROS_ERROR_STREAM("joint " + joint + " was not found in URDF");
  //     return false;
  // }
  //

  std::string alloc_name = "canopen_402/Motor402Allocator";
  // if(params.hasMember("motor_allocator"))
  // alloc_name.assign(params["motor_allocator"]);

  RosSettings settings;
  // if(params.hasMember("motor_layer")) settings = params["motor_layer"];

  MotorBaseSharedPtr motor;

  try {
    motor = motor_allocator_.allocateInstance(alloc_name, name + "_motor",
                                              node->getStorage(), settings);
  } catch (const std::exception &e) {
    std::string info = boost::diagnostic_information(e);
    RCLCPP_ERROR(this->get_logger(), info);
    return false;
  }

  if (!motor) {
    RCLCPP_ERROR(this->get_logger(), "could not allocate motor");
    return false;
  }

  RCLCPP_INFO(this->get_logger(), "adding motor");

  motor->registerDefaultModes(node->getStorage());
  motors_->add(motor);
  logger->add(motor);

  // HandleLayerSharedPtr handle = std::make_shared<HandleLayer>(joint, motor,
  // node->getStorage(), params);
  HandleLayerSharedPtr handle =
      std::make_shared<HandleLayer>(joint, motor, node->getStorage());

  // canopen::LayerStatus s;
  // if(!handle->prepareFilters(s)){
  //     ROS_ERROR_STREAM(s.reason());
  //     return false;
  // }

  robot_layer_->add(joint, handle);
  logger->add(handle);

  setup_debug_interface(node, motor);

  return true;
}

bool MotorChain::setup_chain() {
  RCLCPP_INFO(this->get_logger(), "setting up chain");
  motors_.reset(new LayerGroupNoDiag<MotorBase>("402 Layer"));
  robot_layer_.reset(new RobotLayer(this->get_logger()));

  if (RosChain::setup_chain()) {
    add(motors_);
    // add(robot_layer_);
    RCLCPP_INFO(this->get_logger(), "chain setup successful");

    // if(!nh_.param("use_realtime_period", false)){
    //     dur.fromSec(boost::chrono::duration<double>(update_duration_).count());
    //     ROS_INFO_STREAM("Using fixed control period: " << dur);
    // }else{
    //     ROS_INFO("Using real-time control period");
    // }
    // cm_.reset(new ControllerManagerLayer(robot_layer_, nh_, dur));
    // add(cm_);

    return true;
  }

  RCLCPP_INFO(this->get_logger(), "chain setup failed");

  return false;
}

void MotorChain::handleWrite(LayerStatus &status,
                             const LayerState &current_state) {
  RosChain::handleWrite(status, current_state);
  for (const auto &handle : robot_layer_->handles_) {
    publish_all_debug(handle.second->motor_);
  }
}

bool MotorChain::setup_debug_interface(const canopen::NodeSharedPtr &node,
                                       MotorBaseSharedPtr motor) {
  // TODO(sam): get node name
  std::string node_name = "motor_0";

  // TODO(sam): replace with custom services?

  auto set_object_publishers_callback = [this, node_name, node](
      const canopen_msgs::msg::DebugPublishers::SharedPtr msg) -> void {

    RCLCPP_INFO(this->get_logger(), "clearing object publishers");
    publishers_.clear();
    bool force = true;

    for (const std::string &object_name : msg->data) {
      RCLCPP_INFO(this->get_logger(), "setting debug publishers: [%s]",
                  object_name.c_str());

      PublishFuncType pub = createPublishFunc(node_name + "/obj" + object_name,
                                              node, object_name, force);

      if (!pub) {
        RCLCPP_ERROR(this->get_logger(),
                     "%s could not create publisher for object: '%s'",
                     node_name.c_str(), object_name.c_str());
      }
      this->publishers_.push_back(pub);
    }
  };

  set_debug_publishers_sub_ =
      create_subscription<canopen_msgs::msg::DebugPublishers>(
          node_name + "/set_debug_publishers", set_object_publishers_callback);

  RCLCPP_INFO(this->get_logger(), "setting up debug interface for node: %s",
              node_name.c_str());

  state_pub_ = create_publisher<std_msgs::msg::Int32>(node_name + "/state");

  operation_mode_pub_ =
      create_publisher<std_msgs::msg::Int32>(node_name + "/operation_mode");

  auto switch_state_callback =
      [this, motor](const std_msgs::msg::Int32::SharedPtr msg) -> void {
    RCLCPP_INFO(this->get_logger(), "switching to state: [%d]", msg->data);
    if (!motor->switchState((State402::InternalState)msg->data)) {
      RCLCPP_WARN(this->get_logger(), "switching state failed");
    }

    publish_all_debug(motor);
  };

  switch_state_sub_ = create_subscription<std_msgs::msg::Int32>(
      node_name + "/switch_state", switch_state_callback);

  auto switch_operation_mode_callback =
      [this, motor](const std_msgs::msg::Int32::SharedPtr msg) -> void {
    RCLCPP_INFO(this->get_logger(), "switching to operation mode: [%d]",
                msg->data);
    MotorBase::OperationMode operation_mode =
        (MotorBase::OperationMode)msg->data;

    if (!motor->enterModeAndWait(operation_mode)) {
      RCLCPP_ERROR(this->get_logger(), "could not enter mode: %d",
                   (int)operation_mode);
    }

    publish_all_debug(motor);
  };

  switch_operation_mode_sub_ = create_subscription<std_msgs::msg::Int32>(
      node_name + "/switch_operation_mode", switch_operation_mode_callback);

  auto set_target_position_callback =
      [this, motor](const std_msgs::msg::Float32::SharedPtr msg) -> void {
    RCLCPP_INFO(this->get_logger(), "setting target: [%f]", msg->data);
    motor->setTarget(msg->data);
  };

  set_target_position_sub_ = create_subscription<std_msgs::msg::Float32>(
      node_name + "/set_target_position", set_target_position_callback);
}

void MotorChain::publish_all_debug(MotorBaseSharedPtr motor) {
  std::shared_ptr<std_msgs::msg::Int32> msg;

  msg = std::make_shared<std_msgs::msg::Int32>();
  msg->data = (int)motor->state_handler_.getState();
  state_pub_->publish(msg);

  msg = std::make_shared<std_msgs::msg::Int32>();
  msg->data = motor->op_mode_display_atomic_;
  operation_mode_pub_->publish(msg);
}
