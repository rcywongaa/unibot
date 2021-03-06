#include <cmath>

#include <gflags/gflags.h>
#include "drake/common/text_logging_gflags.h"
#include "drake/multibody/plant/multibody_plant.h"
#include "drake/systems/framework/diagram_builder.h"
#include "drake/common/eigen_types.h"
#include "drake/multibody/tree/revolute_joint.h"

#include "meta.hpp"
#include "drake_util.hpp"
#include "unibot_util.hpp"
#include "acrobot_util.hpp"
#include "mip_util.hpp"
#include "StateConverter.hpp"
#include "Inspector.hpp"

using namespace Eigen;
using namespace drake;
using namespace drake::systems;
using namespace drake::multibody;

DEFINE_double(target_realtime_rate, 1.0,
        "Desired rate relative to real time.  See documentation for "
        "Simulator::set_target_realtime_rate() for details.");

const double w_r = 0.2; // wheel radius
const double w_m = 0.2; // wheel mass
const double l1_m = 0.1; // link1 mass
const double l1_l = 1.0; // link1 length
const double l2_m = 0.08; // link2 mass
const double l2_l = 0.8; // link2 length
const double p_m = 0.5; // point mass

const unsigned int Q_W_IDX = 0;
const unsigned int Q_X_IDX = 1;
const unsigned int Q_Y_IDX = 2;
const unsigned int Q_Z_IDX = 3;
const unsigned int X_IDX = 4;
const unsigned int Y_IDX = 5;
const unsigned int Z_IDX = 6;
const unsigned int ALPHA_IDX = 7; // link1-link2 joint
const unsigned int BETA_IDX = 8; // link1-wheel joint
const unsigned int AA_D_X_IDX = 9; // X component of angular velocity in angle-axis form
const unsigned int AA_D_Y_IDX = 10; // Y component of angular velocity in angle-axis form
const unsigned int AA_D_Z_IDX = 11; // Z component of angular velocity in angle-axis form
const unsigned int X_D_IDX = 12;
const unsigned int Y_D_IDX = 13;
const unsigned int Z_D_IDX = 14;
const unsigned int ALPHA_D_IDX = 15;
const unsigned int BETA_D_IDX = 16;
const unsigned int STATE_SIZE = 17;

template <typename T>
void unibot_to_acrobot_state(const Eigen::VectorBlock<const VectorX<T>>& state, Eigen::VectorBlock<VectorX<T>>& output)
{
    Eigen::Quaternion<T> body_rotation(state[Q_W_IDX], state[Q_X_IDX], state[Q_Y_IDX], state[Q_Z_IDX]);
    Vector3<T> angle_axis_d(state[AA_D_X_IDX], state[AA_D_Y_IDX], state[AA_D_Z_IDX]);
    Vector3<T> rpy_d = body_rotation.inverse() * angle_axis_d;
    drake::math::RollPitchYaw<T> rpy(body_rotation);
    output[0] = rpy.roll_angle(); // roll of the rod
    output[1] = state[ALPHA_IDX]; // phi (angle between link1 and link2)
    output[2] = rpy_d[0]; // roll_dot
    output[3] = state[ALPHA_D_IDX]; // phi_dot
}

template <typename T>
void unibot_to_mip_state(const Eigen::VectorBlock<const VectorX<T>>& state, Eigen::VectorBlock<VectorX<T>>& output)
{
    Eigen::Quaternion<T> body_rotation(state[Q_W_IDX], state[Q_X_IDX], state[Q_Y_IDX], state[Q_Z_IDX]);
    Vector3<T> angle_axis_d(state[AA_D_X_IDX], state[AA_D_Y_IDX], state[AA_D_Z_IDX]);
    Vector3<T> rpy_d = body_rotation.inverse() * angle_axis_d;
    drake::math::RollPitchYaw<T> rpy(body_rotation);
    output[0] = rpy.pitch_angle(); // theta (pitch of the rod)
    output[1] = output[0] + state[BETA_IDX]; // psi (wheel angle) = pitch of rod + rod-wheel angle
    output[2] = rpy_d[1]; // theta_dot
    output[3] = output[2] + state[BETA_D_IDX]; // psi_dot
}

template <typename T>
void inspect_unibot(const Eigen::VectorBlock<const VectorX<T>>& state)
{
    ;
    //Eigen::Quaternion<T> body_rotation(state[Q_W_IDX], state[Q_X_IDX], state[Q_Y_IDX], state[Q_Z_IDX]);
    //Vector3<T> angle_axis_d(state[AA_D_X_IDX], state[AA_D_Y_IDX], state[AA_D_Z_IDX]);
    //Vector3<T> rotated_angle_axis_d = body_rotation.inverse() * angle_axis_d;
    //printf("roll_d = %f\n", rotated_angle_axis_d[0]);
    //printf("pitch_d = %f\n", rotated_angle_axis_d[1]);
    //printf("yaw_d = %f\n", rotated_angle_axis_d[2]);
}

template <typename T>
void inspect_torques(const Eigen::VectorBlock<const VectorX<T>>& input)
{
    ;
    //printf("alpha: %f\n", input[0]);
    //printf("beta: %f\n", input[1]);
}

int main(int argc, char* argv[])
{
    gflags::SetUsageMessage(
            "A unibot demo using Drake's MultibodyPlant,"
            "with SceneGraph visualization. "
            "Launch drake-visualizer before running this example.");
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    drake::logging::HandleSpdlogGflags();

    DiagramBuilder<double> builder;

    MultibodyPlant<double>& plant = create_default_plant(getResDir() + "unibot.sdf", builder, 0.00);

    printf("plant.get_state_output_port().size() = %d\n", plant.get_state_output_port().size());
    printf("plant num positions = %d\n", plant.num_positions());
    printf("plant num velocities = %d\n", plant.num_velocities());

    ConversionFunc unibot_to_acrobot_func(
            unibot_to_acrobot_state<double>,
            unibot_to_acrobot_state<drake::AutoDiffXd>,
            unibot_to_acrobot_state<drake::symbolic::Expression>);
    auto unibot_acrobot_converter = builder.AddSystem(std::make_unique<StateConverter<double>>(unibot_to_acrobot_func, STATE_SIZE, 4));
    unibot_acrobot_converter->set_name("unibot_acrobot_converter");

    ConversionFunc unibot_to_mip_func(
            unibot_to_mip_state<double>,
            unibot_to_mip_state<drake::AutoDiffXd>,
            unibot_to_mip_state<drake::symbolic::Expression>);
    auto unibot_mip_converter = builder.AddSystem(std::make_unique<StateConverter<double>>(unibot_to_mip_func, STATE_SIZE, 4));
    unibot_mip_converter->set_name("unibot_mip_converter");

    auto acrobot_controller = builder.AddSystem(MakeAcrobotLQRController(getResDir() + "unibot_acrobot.sdf"));
    acrobot_controller->set_name("acrobot_controller");

    InspectionFunc inspect_unibot_func(
            inspect_unibot<double>,
            inspect_unibot<drake::AutoDiffXd>,
            inspect_unibot<drake::symbolic::Expression>);
    auto unibot_inspector = builder.AddSystem(std::make_unique<Inspector<double>>(inspect_unibot_func, STATE_SIZE));
    unibot_inspector->set_name("unibot_inspector");

    InspectionFunc inspect_torque_func(
            inspect_torques<double>,
            inspect_torques<drake::AutoDiffXd>,
            inspect_torques<drake::symbolic::Expression>);
    auto torque_inspector = builder.AddSystem(std::make_unique<Inspector<double>>(inspect_torque_func, 2));
    torque_inspector->set_name("torque_inspector");

    MIPConfiguration mip_config;
    mip_config.M_w = w_m; // wheel mass
    mip_config.M_r = l1_m + l2_m + p_m; // total rod mass
    mip_config.R = w_r; // wheel radius
    mip_config.L = l1_l/2.0; // half rod length
    mip_config.I_w = 0.5*w_m*std::pow(w_r, 2); // wheel inertia
    mip_config.I_r = l1_m*std::pow(l1_l, 2)/3.0 + l2_m*((std::pow(l2_l, 2)/3.0) + std::pow(l1_l - l2_l, 2)) + p_m*std::pow(l1_l - l2_l, 2); // rod inertia
    auto mip_controller = builder.AddSystem(std::make_unique<MIPController<double>>(mip_config, 2.0*M_PI));
    mip_controller->set_name("mip_controller");

    auto torque_converter = builder.AddSystem(std::make_unique<TorqueCombiner<double>>());
    torque_converter->set_name("torque_converter");

    builder.Connect(plant.get_state_output_port(), unibot_acrobot_converter->get_input_port());
    builder.Connect(unibot_acrobot_converter->get_output_port(), acrobot_controller->get_input_port());
    builder.Connect(acrobot_controller->get_output_port(), torque_converter->get_acrobot_input_port());

    builder.Connect(plant.get_state_output_port(), unibot_inspector->get_input_port());
    builder.Connect(unibot_inspector->get_output_port(), unibot_mip_converter->get_input_port());
    builder.Connect(unibot_mip_converter->get_output_port(), mip_controller->mip_state_input());
    builder.Connect(mip_controller->torque_output(), torque_converter->get_mip_input_port());

    builder.Connect(torque_converter->get_torque_output_port(), torque_inspector->get_input_port());
    builder.Connect(torque_inspector->get_output_port(), plant.get_actuation_input_port());

    auto diagram = builder.Build();

    std::unique_ptr<systems::Context<double>> diagram_context = diagram->CreateDefaultContext();

    Context<double>& torque_converter_context = diagram->GetMutableSubsystemContext(*torque_converter, diagram_context.get());
    //torque_converter_context.FixInputPort(torque_converter->get_mip_input_port().get_index(), Vector1d::Zero());
    //torque_converter_context.FixInputPort(torque_converter->get_acrobot_input_port().get_index(), Vector1d::Zero());

    VectorX<double> initial_state(Eigen::Matrix<double, STATE_SIZE, 1>::Zero());
    drake::math::RollPitchYaw<double> initial_rpy(0.0, 0.10*M_PI, 0.0);
    Eigen::Quaternion<double> q = initial_rpy.ToQuaternion();
    initial_state[Q_W_IDX] = q.w();
    initial_state[Q_X_IDX] = q.x();
    initial_state[Q_Y_IDX] = q.y();
    initial_state[Q_Z_IDX] = q.z();
    initial_state[Z_IDX] = w_r;
    Context<double>& plant_context = diagram->GetMutableSubsystemContext(plant, diagram_context.get());
    VectorBase<double>& state = plant_context.get_mutable_continuous_state_vector();
    state.SetFromVector(initial_state);

    start_simulation(*diagram, std::move(diagram_context), FLAGS_target_realtime_rate);

    return 0;
}

