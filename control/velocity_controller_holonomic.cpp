/*******************************************************************************
 * Copyright (c) 2009-2016, MAV'RIC Development Team
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************/

/*******************************************************************************
 * \file velocity_controller_copter.cpp
 *
 * \author MAV'RIC Team
 * \author Julien Lecoeur
 *
 * \brief A velocity controller for holonomic hovering platform capable of generating 3D thrust
 *
 * \details It takes a velocity command as input and computes an attitude
 * command and thrust command as output.
 *
 ******************************************************************************/

#include <array>

#include "control/velocity_controller_holonomic.hpp"
#include "util/coord_conventions.hpp"

Velocity_controller_holonomic::Velocity_controller_holonomic(const args_t& args, const conf_t& config) :
    Velocity_controller_copter(args, config),
    use_3d_thrust_(0),
    use_3d_thrust_threshold_(1),
    accel_threshold_3d_thrust_(0.3f)
{
    pid_controller_init(&attitude_offset_pid_[X], &config.attitude_offset_config[X]);
    pid_controller_init(&attitude_offset_pid_[Y], &config.attitude_offset_config[Y]);
}


bool Velocity_controller_holonomic::compute_attitude_and_thrust_from_desired_accel(const std::array<float,3>& accel_vector,
                                                                                attitude_command_t& attitude_command,
                                                                                thrust_command_t& thrust_command)
{
    // Decide whether control will use 3D thrust or not
    bool do_3d_thrust = false;
    if ((use_3d_thrust_ == true) ||
        (   (use_3d_thrust_threshold_ == true)
         && (maths_fast_sqrt(SQR(accel_vector[X]) + SQR(accel_vector[Y])) < accel_threshold_3d_thrust_))
       )
    {
        do_3d_thrust = true;
    }

    if (do_3d_thrust)
    {

        // Desired thrust in local frame
        float thrust_lf[3] = {  accel_vector[X],
                                accel_vector[Y],
                                accel_vector[Z] + thrust_hover_point_};

        // Stay horizontal, with commanded heading
        attitude_command = coord_conventions_quaternion_from_rpy( 0.0f,
                                                                  0.0f,
                                                                  velocity_command_.heading );
        // attitude_command = coord_conventions_quaternion_from_rpy( pid_controller_update(&attitude_offset_pid_[X], thrust_lf[Y]),
        //                                                           pid_controller_update(&attitude_offset_pid_[Y], -thrust_lf[X]),
        //                                                           velocity_command_.heading );

        // Rotate it to get thrust command in body frame
        quaternions_rotate_vector(quaternions_inverse(ahrs_.attitude()), thrust_lf, thrust_command.xyz.data());

        return true;
    }
    else
    {
        return Velocity_controller_copter::compute_attitude_and_thrust_from_desired_accel(accel_vector, attitude_command, thrust_command);
    }
}
