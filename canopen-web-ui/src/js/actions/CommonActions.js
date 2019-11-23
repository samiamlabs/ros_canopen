import dispatcher from '../dispatcher';

import RosLib from 'roslib';
import { Collection } from 'immutable';

class CommonActions {
    constructor() {
        this.createRosClient('localhost', '9090');
    }

    createRosClient = (hostname, port) => {
        const rosUrl = 'ws://' + hostname + ':' + port;

        let rosClient = new RosLib.Ros();

        rosClient.on('connection', () => {
            console.log('Connected to ros-web-bridge server.');
            dispatcher.dispatch({
                type: 'ROS_CONNECTION_STATUS',
                rosConnectionStatus: 'connected'
            });
            this.rosClient = rosClient;
            this.connectRos();
            this.refreshState();
        });

        rosClient.on('error', (error) => {
            console.log('Error connecting to websocket server: ', error);

            dispatcher.dispatch({
                type: 'ROS_CONNECTION_STATUS',
                rosConnectionStatus: 'error'
            });
        });

        rosClient.on('close', () => {
            console.log('Disconnected from websocket server.');
            dispatcher.dispatch({
                type: 'ROS_CONNECTION_STATUS',
                rosConnectionStatus: 'disconnected'
            });
        });

        rosClient.connect(rosUrl);

        dispatcher.dispatch({
            type: 'ROS_CLIENT',
            rosClient: rosClient
        });
    }

    connectRos = () => {
        this.namespace = '';
        this.nodeName = 'canopen_chain'

        this.createServiceClients();
        this.createSubscriptions();
    }

    createServiceClients = () => {
        this.changeLifecycleChaneStateService = new RosLib.Service({
            ros: this.rosClient,
            name: this.nodeName + '/change_state',
            serviceType: 'lifecycle_msgs/srv/ChangeState'
        });

        this.getAvailableLilfecycleTransitionsService = new RosLib.Service({
            ros: this.rosClient,
            name: this.nodeName + '/get_available_transitions',
            serviceType: 'lifecycle_msgs/srv/GetAvailableTransitions'
        });

        this.getLifecycleStateService = new RosLib.Service({
            ros: this.rosClient,
            name: this.nodeName + '/get_state',
            serviceType: 'lifecycle_msgs/srv/GetState'
        });

        this.listParametersService = new RosLib.Service({
            ros: this.rosClient,
            name: this.nodeName + '/list_parameters',
            serviceType: 'rcl_interfaces/srv/ListParameters'
        });

        this.getParametersService = new RosLib.Service({
            ros: this.rosClient,
            name: this.nodeName + '/get_parameters',
            serviceType: 'rcl_interfaces/srv/GetParameters'
        });

        this.setParametersService = new RosLib.Service({
            ros: this.rosClient,
            name: this.nodeName + '/set_parameters',
            serviceType: 'rcl_interfaces/srv/SetParameters'
        });
    }

    createSubscriptions = () => {
        const rosoutTopic = new RosLib.Topic({
            ros: this.rosClient,
            name: 'rosout',
            messageType: 'rcl_interfaces/msg/Log'
        });

        rosoutTopic.subscribe(message => {
            dispatcher.dispatch({
                type: 'ROSOUT_MSG',
                rosout: message
            });
        });
    }

    refreshState = () => {
        this.refreshLifecycleState();
    }

    refreshLifecycleState = () => {
        this.callGetAvailableLifecycleTransitionsService();
        this.callGetLifecycleStateService();
        this.refreshParameters();
    }

    refreshParameters = () => {
        const request = new RosLib.ServiceRequest({
            prefixes: [],
            depth: 0
        });

        this.listParametersService.callService(request, response => {
            const { names } = response.result;

            const request = new RosLib.ServiceRequest({
                names
            });
            this.getParametersService.callService(request, response => {
                const { values } = response;
                dispatcher.dispatch({
                    type: 'PARAMETER_VALUES',
                    names,
                    values
                });
            });

        });
    }


    callGetLifecycleStateService = () => {
        const request = new RosLib.ServiceRequest({});
        this.getLifecycleStateService.callService(request, response => {
            dispatcher.dispatch({
                type: 'LIFECYCLE_STATE',
                currentState: response.current_state
            });
        });
    }

    callLifecycleChangeStateService = (transition) => {
        const request = new RosLib.ServiceRequest({
            transition: {
                id: '',
                label: transition
            }
        });

        this.changeLifecycleChaneStateService.callService(request, result => {
            this.refreshLifecycleState();
        })
    }

    callGetAvailableLifecycleTransitionsService = () => {
        const request = new RosLib.ServiceRequest({});
        this.getAvailableLilfecycleTransitionsService.callService(request, result => {
            dispatcher.dispatch({
                type: 'LIFECYCLE_AVAILABLE_TRANSITIONS',
                availableTransitions: result.available_transitions
            });
        })
    }

    clearRosoutMessages = () => {
        dispatcher.dispatch({
            type: 'CLEAR_ROSOUT_MSGS'
        });
    }

    updateRosParameter = (newData, oldData) => {
        // TODO(sam): share these between action and store
        // const PARAMETER_NOT_SET = 0
        const PARAMETER_BOOL = 1
        const PARAMETER_INTEGER = 2
        const PARAMETER_DOUBLE = 3
        const PARAMETER_STRING = 4
        // const PARAMETER_BYTE_ARRAY = 5
        // const PARAMETER_BOOL_ARRAY = 6
        // const PARAMETER_INTEGER_ARRAY = 7
        // const PARAMETER_DOUBLE_ARRAY = 8
        const PARAMETER_STRING_ARRAY = 9

        if (oldData.name !== newData.name) {
            console.warn("Changing the name of ROS parameters is not supported yet!");
        } else {
            const { type } = newData;
            const valueString = newData.valueString;
            const parameterValue = { type };
            switch (type) {
                case PARAMETER_BOOL:
                    {
                        parameterValue.bool_value = (valueString === 'true');
                        break;
                    }
                case PARAMETER_INTEGER:
                    {
                        parameterValue.integer_value = parseInt(valueString);
                        break;
                    }
                case PARAMETER_DOUBLE:
                    {
                        parameterValue.double_value = parseFloat(valueString);
                        break;
                    }
                case PARAMETER_STRING:
                    {
                        parameterValue.string_value = valueString;
                        break;
                    }
                case PARAMETER_STRING_ARRAY:
                    {
                        parameterValue.string_array_value = JSON.parse(valueString);
                        break;
                    }
                default:
                    {
                        console.warn('Writing parameter type ' + newData.typeName + ' is not supported yet!');
                        return;
                    }
            }

            const request = new RosLib.ServiceRequest({
                parameters: [{
                    name: newData.name,
                    value: parameterValue
                }]
            });

            this.setParametersService.callService(request, response => {
                console.log()
                this.refreshParameters();
            });
        }

    }

}

const commonActions = new CommonActions();

export default commonActions;