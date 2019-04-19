
#include <canopen_motor_node/motor_chain.hpp>
#include <canopen_motor_node/handle_layer.hpp>

using namespace canopen;


// TODO(sam): implement this
class RosSettings : public Settings{
public:
    RosSettings() {}
    // XmlRpcSettings(const XmlRpc::XmlRpcValue &v) : value_(v) {}
    // XmlRpcSettings& operator=(const XmlRpc::XmlRpcValue &v) { value_ = v; return *this; }
private:
    virtual bool getRepr(const std::string &n, std::string & repr) const {
        // if(value_.hasMember(n)){
        //     std::stringstream sstr;
        //     sstr << const_cast< XmlRpc::XmlRpcValue &>(value_)[n]; // does not write since already existing
        //     repr = sstr.str();
        //     return true;
        // }
        return false;
    }
    // XmlRpc::XmlRpcValue value_;
};


// MotorChain::MotorChain(const ros::NodeHandle &nh, const ros::NodeHandle &nh_priv) :
//         RosChain(nh, nh_priv), motor_allocator_("canopen_402", "canopen::MotorBase::Allocator") {}

MotorChain::MotorChain(std::string node_name) :
        RosChain(node_name), motor_allocator_("canopen_402", "canopen::MotorBase::Allocator") {}

bool MotorChain::nodeAdded(const canopen::NodeSharedPtr & node, const LoggerSharedPtr & logger)
{
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
  // if(params.hasMember("motor_allocator")) alloc_name.assign(params["motor_allocator"]);

  RosSettings settings;
  // if(params.hasMember("motor_layer")) settings = params["motor_layer"];

  MotorBaseSharedPtr motor;

  try{
      motor = motor_allocator_.allocateInstance(alloc_name, name + "_motor", node->getStorage(), settings);
  }
  catch( const std::exception &e){
      std::string info = boost::diagnostic_information(e);
      RCLCPP_ERROR(this->get_logger(), info);
      return false;
  }

  if(!motor){
      RCLCPP_ERROR(this->get_logger(), "could not allocate motor");
      return false;
  }

  RCLCPP_INFO(this->get_logger(), "adding motor");

  motor->registerDefaultModes(node->getStorage());
  motors_->add(motor);
  logger->add(motor);

  // HandleLayerSharedPtr handle = std::make_shared<HandleLayer>(joint, motor, node->getStorage(), params);
  HandleLayerSharedPtr handle = std::make_shared<HandleLayer>(joint, motor, node->getStorage());

  // canopen::LayerStatus s;
  // if(!handle->prepareFilters(s)){
  //     ROS_ERROR_STREAM(s.reason());
  //     return false;
  // }

  robot_layer_->add(joint, handle);
  logger->add(handle);

  return true;
}

bool MotorChain::setup_chain() {
    RCLCPP_INFO(this->get_logger(),
      "setting up chain");
    motors_.reset(new LayerGroupNoDiag<MotorBase>("402 Layer"));
    robot_layer_.reset(new RobotLayer(this->get_logger()));

    if(RosChain::setup_chain())
    {
      add(motors_);
        // add(robot_layer_);
      RCLCPP_INFO(this->get_logger(),
        "chain setup successful");

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

    RCLCPP_INFO(this->get_logger(),
      "chain setup failed");

    return false;
}
