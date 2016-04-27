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
 * \file navigation.c
 *
 * \author MAV'RIC Team
 * \author Nicolas Dousse
 *
 * \brief Waypoint navigation controller
 *
 ******************************************************************************/


#include "automatic_navigation/navigation.hpp"
#include "hal/common/time_keeper.hpp"

extern "C"
{
#include "util/print_util.h"
#include "util/constants.h"
}

//------------------------------------------------------------------------------
// PRIVATE FUNCTIONS DECLARATION
//------------------------------------------------------------------------------
/**
 * \brief                   Computes the relative position and distance to the given way point
 *
 * \param   waypoint_pos        Local coordinates of the waypoint
 * \param   rel_pos         Array to store the relative 3D position of the waypoint
 * \param   local_pos       The 3D array of the actual position
 *
 * \return                  Distance to waypoint squared
 */
static float navigation_set_rel_pos_n_dist2wp(float waypoint_pos[], float rel_pos[], const float local_pos[3]);


//------------------------------------------------------------------------------
// PRIVATE FUNCTIONS IMPLEMENTATION
//------------------------------------------------------------------------------

static float navigation_set_rel_pos_n_dist2wp(float waypoint_pos[], float rel_pos[], const float local_pos[3])
{
    float dist2wp_sqr;

    rel_pos[X] = (float)(waypoint_pos[X] - local_pos[X]);
    rel_pos[Y] = (float)(waypoint_pos[Y] - local_pos[Y]);
    rel_pos[Z] = (float)(waypoint_pos[Z] - local_pos[Z]);

    dist2wp_sqr = vectors_norm_sqr(rel_pos);

    return dist2wp_sqr;
}


void Navigation::set_speed_command(float rel_pos[])
{
    float  norm_rel_dist;
    float v_desired = 0.0f;

    float dir_desired_sg[3];
    float rel_heading;

    mav_mode_t mode = state.mav_mode();

    norm_rel_dist = sqrt(dist2wp_sqr);

    // calculate dir_desired in local frame
    aero_attitude_t attitude_yaw = coord_conventions_quat_to_aero(qe);
    attitude_yaw.rpy[0] = 0.0f;
    attitude_yaw.rpy[1] = 0.0f;
    attitude_yaw.rpy[2] = -attitude_yaw.rpy[2];
    quat_t q_rot = coord_conventions_quaternion_from_aero(attitude_yaw);

    quaternions_rotate_vector(q_rot, rel_pos, dir_desired_sg);

    // Avoiding division by zero
    if (norm_rel_dist < 0.0005f)
    {
        norm_rel_dist += 0.0005f;
    }

    // Normalisation of the goal direction
    dir_desired_sg[X] /= norm_rel_dist;
    dir_desired_sg[Y] /= norm_rel_dist;
    dir_desired_sg[Z] /= norm_rel_dist;

    if ((mav_modes_is_auto(mode) && ((state.nav_plan_active && (internal_state_ == NAV_NAVIGATING)) || (internal_state_ == NAV_STOP_THERE))) || ((state.mav_state_ == MAV_STATE_CRITICAL) && (critical_behavior == Navigation::FLY_TO_HOME_WP)))
    {

        if (((maths_f_abs(rel_pos[X]) <= 1.0f) && (maths_f_abs(rel_pos[Y]) <= 1.0f)) || ((maths_f_abs(rel_pos[X]) <= 5.0f) && (maths_f_abs(rel_pos[Y]) <= 5.0f) && (maths_f_abs(rel_pos[Z]) >= 3.0f)))
        {
            rel_heading = 0.0f;
        }
        else
        {
            rel_heading = maths_calc_smaller_angle(atan2(rel_pos[Y], rel_pos[X]) - position_estimation.local_position.heading);
        }

        wpt_nav_controller.clip_max = cruise_speed;
        v_desired = pid_controller_update_dt(&wpt_nav_controller, norm_rel_dist, dt);
    }
    else
    {
        rel_heading = 0.0f;
        hovering_controller.clip_max = cruise_speed;
        v_desired = pid_controller_update_dt(&hovering_controller, norm_rel_dist, dt);
    }

    if (v_desired *  maths_f_abs(dir_desired_sg[Z]) > max_climb_rate)
    {
        v_desired = max_climb_rate / maths_f_abs(dir_desired_sg[Z]);
    }


    // Scaling of the goal direction by the desired speed
    dir_desired_sg[X] *= v_desired;
    dir_desired_sg[Y] *= v_desired;
    dir_desired_sg[Z] *= v_desired;

    // loop_count++;
    // loop_count = loop_count % 50;
    // if (loop_count == 0)
    // {
    //  // print_util_dbg_print("Desired_vel_sg(x100): (");
    //  // print_util_dbg_print_num(dir_desired_sg[X] * 100,10);
    //  // print_util_dbg_print_num(dir_desired_sg[Y] * 100,10);
    //  // print_util_dbg_print_num(dir_desired_sg[Z] * 100,10);
    //  // print_util_dbg_print("). \r\n");
    //  print_util_dbg_print("rel_heading(x100): ");
    //  print_util_dbg_print_num(rel_heading,10);
    //  print_util_dbg_print("\r\n");
    //  print_util_dbg_print("nav state: ");
    //  print_util_dbg_print_num(internal_state_,10);
    //  print_util_dbg_print("\r\n");
    //  // print_util_dbg_print("Actual_vel_bf(x100): (");
    //  // print_util_dbg_print_num(position_estimation.vel_bf[X] * 100,10);
    //  // print_util_dbg_print_num(position_estimation.vel_bf[Y] * 100,10);
    //  // print_util_dbg_print_num(position_estimation.vel_bf[Z] * 100,10);
    //  // print_util_dbg_print("). \r\n");
    //  // print_util_dbg_print("Actual_pos(x100): (");
    //  // print_util_dbg_print_num(position_estimation.local_position.pos[X] * 100,10);
    //  // print_util_dbg_print_num(position_estimation.local_position.pos[Y] * 100,10);
    //  // print_util_dbg_print_num(position_estimation.local_position.pos[Z] * 100,10);
    //  // print_util_dbg_print("). \r\n");
    // }

    controls_nav.tvel[X] = dir_desired_sg[X];
    controls_nav.tvel[Y] = dir_desired_sg[Y];
    controls_nav.tvel[Z] = dir_desired_sg[Z];
    controls_nav.rpy[YAW] = kp_yaw * rel_heading;

    if ((internal_state_ == NAV_LANDING) && (auto_landing_behavior == Navigation::DESCENT_TO_GND))
    {
        // Constant velocity to the ground
        controls_nav.tvel[Z] = 0.3f;
    }
}

void Navigation::run()
{
    float rel_pos[3];

    // Control in translational speed of the platform
    dist2wp_sqr = navigation_set_rel_pos_n_dist2wp(goal.pos,
                              rel_pos,
                              position_estimation.local_position.pos);
    set_speed_command(rel_pos);

    controls_nav.theading = goal.heading;
}

//------------------------------------------------------------------------------
// PUBLIC FUNCTIONS IMPLEMENTATION
//------------------------------------------------------------------------------

Navigation::Navigation(control_command_t& controls_nav, const quat_t& qe, const Position_estimation& position_estimation, State& state, Mavlink_stream& mavlink_stream, conf_t nav_config) :
    qe(qe),
    controls_nav(controls_nav),
    position_estimation(position_estimation),
    state(state),
    mavlink_stream(mavlink_stream)
{
    //navigation controller init
    controls_nav.rpy[ROLL] = 0.0f;
    controls_nav.rpy[PITCH] = 0.0f;
    controls_nav.rpy[YAW] = 0.0f;
    controls_nav.tvel[X] = 0.0f;
    controls_nav.tvel[Y] = 0.0f;
    controls_nav.tvel[Z] = 0.0f;
    controls_nav.theading = 0.0f;
    controls_nav.thrust = -1.0f;
    controls_nav.control_mode = VELOCITY_COMMAND_MODE;
    controls_nav.yaw_mode = YAW_ABSOLUTE;

    goal.pos[X] = 0.0f;
    goal.pos[Y] = 0.0f;
    goal.pos[Z] = 0.0f;
    goal.heading = 0.0f;

    last_update = 0;

    dist2wp_sqr = 0.0f;

    wpt_nav_controller = nav_config.wpt_nav_controller;
    hovering_controller = nav_config.hovering_controller;

    dist2vel_gain = nav_config.dist2vel_gain;
    cruise_speed = nav_config.cruise_speed;
    max_climb_rate = nav_config.max_climb_rate;

    soft_zone_size = nav_config.soft_zone_size;

    alt_lpf = nav_config.alt_lpf;
    LPF_gain = nav_config.LPF_gain;
    kp_yaw = nav_config.kp_yaw;

    loop_count = 0;

    dt = 0.004;
}


bool Navigation::update(void)
{
    mav_mode_t mode_local = state.mav_mode();

    uint32_t t = time_keeper_get_us();

    dt = (float)(t - last_update) / 1000000.0f;
    last_update = t;

    switch (state.mav_state_)
    {
        case MAV_STATE_STANDBY:
            controls_nav.tvel[X] = 0.0f;
            controls_nav.tvel[Y] = 0.0f;
            controls_nav.tvel[Z] = 0.0f;
            break;

        case MAV_STATE_ACTIVE:
            if (internal_state_ > NAV_ON_GND)
            {
                run();
            }
            break;

        case MAV_STATE_CRITICAL:
            // In MAV_MODE_VELOCITY_CONTROL, MAV_MODE_POSITION_HOLD and MAV_MODE_GPS_NAVIGATION
            if (mav_modes_is_stabilise(mode_local))
            {
                if ((internal_state_ == NAV_NAVIGATING) || (internal_state_ == NAV_LANDING))
                {

                    run();

                    if (state.out_of_fence_2)
                    {
                        // Constant velocity to the ground
                        controls_nav.tvel[Z] = 1.0f;
                    }

                }
            }
            break;

        default:
            controls_nav.tvel[X] = 0.0f;
            controls_nav.tvel[Y] = 0.0f;
            controls_nav.tvel[Z] = 0.0f;
            break;
    }

    return true;
}

