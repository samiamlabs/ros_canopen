#ifndef CANOPEN_MOTOR_NODE_HANDLE_LAYER_H_
#define CANOPEN_MOTOR_NODE_HANDLE_LAYER_H_

#include <memory>
#include <unordered_map>

#include <atomic>
#include <functional>
#include <boost/thread/mutex.hpp>

#include <hardware_interface/robot_hardware.hpp>
#include <hardware_interface/types/hardware_interface_return_values.hpp>

#include <filters/filter_chain.hpp>

// #include <hardware_interface/joint_command_interface.h>
// #include <hardware_interface/joint_state_interface.h>
// #include <joint_limits_interface/joint_limits_interface.h>

#include <canopen_master/objdict.hpp>
#include <canopen_master/layer.hpp>
#include <canopen_402/base.hpp>
#include <canopen_motor_node/unit_converter.hpp>
#include <canopen_motor_node/handle_layer_base.hpp>
#include <std_srvs/srv/trigger.hpp>

namespace canopen {

class LimitsHandleBase {
public:
    // virtual void enforce(const ros::Duration &period) = 0;
    virtual void reset() = 0;
    virtual ~LimitsHandleBase();
};
typedef std::shared_ptr<LimitsHandleBase> LimitsHandleBaseSharedPtr;

class ObjectVariables {
    const ObjectStorageSharedPtr storage_;
    struct Getter {
        std::shared_ptr<double> val_ptr;
        std::function<bool(double&)> func;
        bool operator ()() { return func(*val_ptr); }
        template<typename T> Getter(const ObjectStorage::Entry<T> &entry): func(std::bind(&Getter::readObject<T>, entry, std::placeholders::_1)), val_ptr(new double) { }
        template<typename T> static bool readObject(ObjectStorage::Entry<T> &entry, double &res){
            T val;
            if(!entry.get(val)) return false;
            res = val;
            return true;
        }
        operator double*() const { return val_ptr.get(); }
    };
    typedef std::unordered_map<ObjectDict::Key, Getter, ObjectDict::KeyHash> GetterMap;
    GetterMap getters_;
    boost::mutex mutex_;
public:
    template<const uint16_t dt> static double* func(ObjectVariables &list, const canopen::ObjectDict::Key &key){
        typedef typename ObjectStorage::DataType<dt>::type type;
        return list.getters_.insert(std::make_pair(key, Getter(list.storage_->entry<type>(key)))).first->second;
    }

    ObjectVariables(const ObjectStorageSharedPtr storage) : storage_(storage) {}
    bool sync(){
        boost::mutex::scoped_lock lock(mutex_);
        bool ok = true;
        for(GetterMap::iterator it = getters_.begin(); it != getters_.end(); ++it){
            ok = it->second() && ok;
        }
        return ok;
    }

    double * getVariable(const std::string &n) {
        boost::mutex::scoped_lock lock(mutex_);
        try{
            if(n.find("obj") == 0){
                canopen::ObjectDict::Key key(n.substr(3));
                GetterMap::const_iterator it = getters_.find(key);
                if(it != getters_.end()) return it->second;
                return canopen::branch_type<ObjectVariables, double * (ObjectVariables &list, const canopen::ObjectDict::Key &k)>(storage_->dict_->get(key)->data_type)(*this, key);
            }
        }
        catch( const std::exception &e){
            // ROS_ERROR_STREAM("Could not find variable '" << n << "', reason: " << boost::diagnostic_information(e));
            printf("Could not find varaible");
        }
        return 0;
    }
};

template<> inline double* ObjectVariables::func<canopen::ObjectDict::DEFTYPE_VISIBLE_STRING >(ObjectVariables &, const canopen::ObjectDict::Key &){ return 0; }
template<> inline double* ObjectVariables::func<canopen::ObjectDict::DEFTYPE_OCTET_STRING >(ObjectVariables &, const canopen::ObjectDict::Key &){ return 0; }
template<> inline double* ObjectVariables::func<canopen::ObjectDict::DEFTYPE_UNICODE_STRING >(ObjectVariables &, const canopen::ObjectDict::Key &){ return 0; }
template<> inline double* ObjectVariables::func<canopen::ObjectDict::DEFTYPE_DOMAIN >(ObjectVariables &, const canopen::ObjectDict::Key &){ return 0; }


class HandleLayer: public canopen::HandleLayerBase {
    double pos_, vel_, eff_;
    double cmd_pos_, cmd_vel_, cmd_eff_;

    ObjectVariables variables_;
    std::unique_ptr<UnitConverter>  conv_target_pos_, conv_target_vel_, conv_target_eff_;
    std::unique_ptr<UnitConverter>  conv_pos_, conv_vel_, conv_eff_;
    std::unique_ptr<UnitConverter>  conv_pos_offset_;

    filters::FilterChain<double> filter_pos_, filter_vel_, filter_eff_;
    // XmlRpc::XmlRpcValue options_;

    hardware_interface::JointCommandHandle joint_command_handle_;
    hardware_interface::JointStateHandle joint_state_handle_;

    // hardware_interface::JointStateHandle jsh_;
    // hardware_interface::JointHandle jph_, jvh_, jeh_;
    // std::atomic<hardware_interface::JointHandle*> jh_;
    // std::atomic<bool> forward_command_;

    // typedef std::unordered_map< MotorBase::OperationMode,hardware_interface::JointHandle* > CommandMap;
    // CommandMap commands_;

    // template <typename T> hardware_interface::JointHandle* addHandle( T &iface, hardware_interface::JointHandle *jh,  const std::vector<MotorBase::OperationMode> & modes){
    //
    //     bool supported = false;
    //     for(size_t i=0; i < modes.size(); ++i){
    //         if(motor_->isModeSupported(modes[i])){
    //             supported = true;
    //             break;
    //         }
    //     }
    //     if(!supported) return 0;
    //
    //     iface.registerHandle(*jh);
    //
    //     for(size_t i=0; i < modes.size(); ++i){
    //         commands_[modes[i]] = jh;
    //     }
    //     return jh;
    // }

    bool select(const canopen::MotorBase::OperationMode &m);
    // std::vector<LimitsHandleBaseSharedPtr> limits_;
    // bool enable_limits_;
public:
    // HandleLayer(const std::string &name, const canopen::MotorBaseSharedPtr & motor, const canopen::ObjectStorageSharedPtr storage,  XmlRpc::XmlRpcValue & options);
    HandleLayer(const std::string & name, const std::string & node_name, const canopen::MotorBaseSharedPtr & motor, const canopen::ObjectStorageSharedPtr storage, const std::string effort2download);

    static double * assignVariable(const std::string &name, double * ptr, const std::string &req) { return name == req ? ptr : 0; }

    CanSwitchResult canSwitch(const canopen::MotorBase::OperationMode &m);
    bool switchMode(const canopen::MotorBase::OperationMode &m);
    bool forwardForMode(const canopen::MotorBase::OperationMode &m);

    // void registerJointStateHandle(RobotLayer robot_layer) {
    //   printf("register joint state in handle layer\n");
    // }

    virtual hardware_interface::JointStateHandle *getJointStateHandle()
    {
      return &joint_state_handle_;
    }

    virtual hardware_interface::JointCommandHandle *getJointCommandHandle()
    {
      return &joint_command_handle_;
    }
    // void registerHandle(hardware_interface::JointStateInterface &iface){
    //     iface.registerHandle(jsh_);
    // }
    // hardware_interface::JointHandle* registerHandle(hardware_interface::PositionJointInterface &iface,
    //                                                 const joint_limits_interface::JointLimits &limits,
    //                                                 const joint_limits_interface::SoftJointLimits *soft_limits = 0);
    // hardware_interface::JointHandle* registerHandle(hardware_interface::VelocityJointInterface &iface,
    //                                                 const joint_limits_interface::JointLimits &limits,
    //                                                 const joint_limits_interface::SoftJointLimits *soft_limits = 0);
    // hardware_interface::JointHandle* registerHandle(hardware_interface::EffortJointInterface &iface,
    //                                                 const joint_limits_interface::JointLimits &limits,
    //                                                 const joint_limits_interface::SoftJointLimits *soft_limits = 0);
    //
    // void enforceLimits(const ros::Duration &period, bool reset);
    void enableLimits(bool enable);

    bool prepareFilters(canopen::LayerStatus &status);

    virtual void setEnableRosControlCommand(bool value)
    {
      enable_ros_control_command_ = value;
    }

    void setJointToZero();

private:

    virtual void handleRead(canopen::LayerStatus &status, const LayerState &current_state);
    virtual void handleWrite(canopen::LayerStatus &status, const LayerState &current_state);
    virtual void handleInit(canopen::LayerStatus &status);
    virtual void handleDiag(canopen::LayerReport &report) { /* nothing to do */ }
    virtual void handleShutdown(canopen::LayerStatus &status) { /* nothing to do */ }
    virtual void handleHalt(canopen::LayerStatus &status) { /* TODO */ }
    virtual void handleRecover(canopen::LayerStatus &status) { /* nothing to do */ }

    bool enable_ros_control_command_;
};

typedef std::shared_ptr<HandleLayer> HandleLayerSharedPtr;

}  // namespace canopen



#endif /* INCLUDE_CANOPEN_MOTOR_NODE_HANDLE_LAYER_H_ */
