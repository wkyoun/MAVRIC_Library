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
 * \file ins_kf.cpp
 *
 * \author MAV'RIC Team
 * \author Julien Lecoeur
 * \author Simon Pyroth
 *
 * \brief   Kalman filter for position estimation
 *
 ******************************************************************************/


#include "sensing/ins_kf.hpp"
#include "util/coord_conventions.hpp"

//------------------------------------------------------------------------------
// PUBLIC FUNCTIONS IMPLEMENTATION
//------------------------------------------------------------------------------

INS_kf::INS_kf( State& state,
                const Gps& gps,
                const Gps_mocap& gps_mocap,
                const Barometer& barometer,
                const Sonar& sonar,
                const PX4Flow& flow,
                const AHRS& ahrs,
                const conf_t config):
    Kalman<11,3,3>( {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},                                              // x
                    Mat<11,11>(100, true),                                                          // P
                    { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                                              // F (will be updated)
                      0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                      0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
                      0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,
                      0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0,
                      0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0,
                      0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0,
                      0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,
                      0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,
                      0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,
                      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
                    (0.0f),                                                                           // Q (will be updated)
                    { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                                                // H (GPS pos)
                      0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                      0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0 },
                    {SQR(config.sigma_gps_xy), 0,                        0,                           // R (GPS pos)
                     0,                        SQR(config.sigma_gps_xy), 0,
                     0,                        0,                        SQR(config.sigma_gps_z)},
                    Mat<11,3>(0.0f)),                                                                 // B (will be updated)
    INS(config.origin),
    config_(config),
    state_(state),
    gps_(gps),
    gps_mocap_(gps_mocap),
    barometer_(barometer),
    sonar_(sonar),
    flow_(flow),
    ahrs_(ahrs),
    H_gpsvel_({ 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0}),
    R_gpsvel_({ SQR(config.sigma_gps_velxy),  0,                            0,
                0,                            SQR(config.sigma_gps_velxy),  0,
                0,                            0,                            SQR(config.sigma_gps_velz)}),
    R_mocap_({  SQR(config.sigma_gps_mocap),  0,                            0,
                0,                            SQR(config.sigma_gps_mocap),  0,
                0,                            0,                            SQR(config.sigma_gps_mocap)}),
    H_baro_({0, 0, -1, 0, 0, 0, 0, 0, 0, 0, -1}),
    R_baro_({ SQR(config.sigma_baro) }),
    H_sonar_({0, 0, -1, 1, 0, 0, 0, 0, 0, 0, 0}),
    R_sonar_({ SQR(config.sigma_sonar) }),
    H_flow_({0, 0, 0,  0, 1, 0, 0, 0, 0, 0, 0,
            0, 0, 0,  0, 0, 1, 0, 0, 0, 0, 0,
            // 0, 0, -1, 1, 0, 0, 0, 0, 0, 0, 0}),
            0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0}),
    R_flow_({ SQR(config.sigma_flow), 0,                        0,
             0,                       SQR(config.sigma_flow),   0,
             0,                       0,                        SQR(config.sigma_sonar)}),
    last_accel_update_s_(0.0f),
    last_sonar_update_s_(0.0f),
    last_flow_update_s_(0.0f),
    last_baro_update_s_(0.0f),
    last_gps_pos_update_s_(0.0f),
    last_gps_vel_update_s_(0.0f),
    last_gps_mocap_update_s_(0.0f),
    first_fix_received_(false),
    dt_(0.0f),
    last_update_(0.0f)
{
    // Init the filter
    init();
    init_flag = 0;
}


void INS_kf::init(void)
{
    // Initialization is done, no need to do once more
    init_flag = 0;

    // Init state
    x_ = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    // Init covariance
    P_ = Mat<11,11>(1000.0f, true);


    // Update last time to avoid glitches at initilaization
    last_update_ = time_keeper_get_s();
}


float INS_kf::last_update_s(void) const
{
    float last_update_s = 0.0f;

    last_update_s = maths_f_max(last_update_s, last_sonar_update_s_);
    //last_update_s = maths_f_max(last_update_s, last_flow_update_s_);
    last_update_s = maths_f_max(last_update_s, last_baro_update_s_);
    last_update_s = maths_f_max(last_update_s, last_gps_pos_update_s_);
    last_update_s = maths_f_max(last_update_s, last_gps_vel_update_s_);

    return last_update_s;
}


std::array<float,3> INS_kf::position_lf(void) const
{
    return std::array<float,3>{{x_[0], x_[1], x_[2]}};
}


std::array<float,3> INS_kf::velocity_lf(void) const
{
    return std::array<float,3>{{x_[4], x_[5], x_[6]}};
}


float INS_kf::absolute_altitude(void) const
{
    return -x_[2] + origin().altitude;
}


bool INS_kf::is_healthy(INS::healthy_t type) const
{
    bool ret = false;

    float now     = time_keeper_get_s();
    float timeout = 1.0f;  // timeout after 1 second

    switch(type)
    {
        case INS::healthy_t::XY_VELOCITY:
            ret = ( ((gps_.fix() >= FIX_2D) && ( (now - last_gps_vel_update_s_) < timeout) )
              //|| ( flow_.healthy()      && ( (now - last_flow_update_s_)    < timeout) )
              );

        break;

        case INS::healthy_t::Z_VELOCITY:
            ret = ( ((gps_.fix() >= FIX_3D) && ( (now - last_gps_vel_update_s_) < timeout) ) ||
                    //( flow_.healthy()      && ( (now - last_flow_update_s_)    < timeout) ) ||
                    ( sonar_.healthy()     && ( (now - last_sonar_update_s_)   < timeout) ) );
        break;

        case INS::healthy_t::XYZ_VELOCITY:
            ret = ( ((gps_.fix() >= FIX_3D) && ( (now - last_gps_vel_update_s_) < timeout) )
              //|| ( flow_.healthy()      && ( (now - last_flow_update_s_)    < timeout) )
              );
        break;

        case INS::healthy_t::XY_REL_POSITION:
            ret = ( ((gps_.fix() >= FIX_2D) && ( (now - last_gps_pos_update_s_) < timeout) )
              //|| ( flow_.healthy()      && ( (now - last_flow_update_s_)    < timeout) )
              );
        break;

        case INS::healthy_t::Z_REL_POSITION:
            ret = (
                    //( flow_.healthy()      && ( (now - last_flow_update_s_)    < timeout) ) ||
                    ( sonar_.healthy()     && ( (now - last_sonar_update_s_)   < timeout) ) );
        break;

        case INS::healthy_t::XYZ_REL_POSITION:
            ret = ( ((gps_.fix() >= FIX_3D) && ( (now - last_gps_pos_update_s_) < timeout) )
              //|| ( flow_.healthy()      && ( (now - last_flow_update_s_)    < timeout) )
              );
        break;

        case INS::healthy_t::XY_ABS_POSITION:
            ret = ( ((gps_.fix() >= FIX_3D) && ( (now - last_gps_pos_update_s_) < timeout) ) );
        break;

        case INS::healthy_t::Z_ABS_POSITION:
            ret = ( ((gps_.fix() >= FIX_3D) && ( (now - last_gps_pos_update_s_) < timeout) ) ||
                  ( ( (now - last_baro_update_s_) < timeout   ) ) );
        break;

        case INS::healthy_t::XYZ_ABS_POSITION:
            ret = ( ((gps_.fix() >= FIX_3D) && ( (now - last_gps_pos_update_s_) < timeout) ));
        break;
    }

    return ret;
}


bool INS_kf::update(void)
{
    // Check if an initialization has to be done because we just received a fix
    if (   (gps_.healthy() || gps_mocap_.healthy())
        && (first_fix_received_ == false) )
    {
        first_fix_received_ = true;
        init();
    }

    // Check is an initialization was issued via telemetry
    if (init_flag == 1)
    {
        init();
    }

    // Prediction step
    if (ahrs_.is_healthy())
    {
        if (last_accel_update_s_ < ahrs_.last_update_s())
        {
            // Update the delta time (in second)
            float now       = time_keeper_get_s();
            dt_             = now - last_update_;
            last_update_    = now;

            // Make the prediciton
            predict_kf();

            // update timimg
            last_accel_update_s_ = ahrs_.last_update_s();
        }
    }
    else
    {
        init();
    }


    // Measure from gps
    if (gps_.healthy())
    {
        // GPS Position
        if (last_gps_pos_update_s_ < (float)(gps_.last_position_update_us())/1e6f)
        {
            // Do kalman update
            update_gps_pos();

            // Update timing
            last_gps_pos_update_s_ = (float)(gps_.last_position_update_us())/1e6f;
        }

        // GPS velocity
        if (last_gps_vel_update_s_ < (float)(gps_.last_velocity_update_us())/1e6f)
        {
            // Do kalman update
            update_gps_vel();

            // Update timing
            last_gps_vel_update_s_ = (float)(gps_.last_velocity_update_us())/1e6f;
        }
    }

    // Measure from gps moacp
    if (gps_mocap_.healthy())
    {
        if (last_gps_mocap_update_s_ < (float)(gps_mocap_.last_position_update_us())/1e6f)
        {
            // Do kalman update
            update_gps_mocap();

            // Update timing
            last_gps_mocap_update_s_ = (float)(gps_mocap_.last_position_update_us())/1e6f;
        }
    }

    // Measure from barometer
    if(true) // TODO: Add healthy function into barometer
    {
       if (last_baro_update_s_ < (float)(barometer_.last_update_us())/1e6f)
       {
          // Do kalman update
          update_barometer();

          // Update timing
          last_baro_update_s_ = (float)(barometer_.last_update_us())/1e6f;
       }
    }


    // Measure from sonar (only if small angles, to avoid peaks)
    aero_attitude_t current_attitude = coord_conventions_quat_to_aero(ahrs_.attitude());
    if ( (maths_f_abs(current_attitude.rpy[ROLL])  < PI / 9.0f) &&
         (maths_f_abs(current_attitude.rpy[PITCH]) < PI / 9.0f)    )
    {
        if (last_sonar_update_s_ < (float)(sonar_.last_update_us())/1e6f)
        {
            // Do kalman update
            update_sonar();

            // Update timing
            last_sonar_update_s_ = (float)(sonar_.last_update_us())/1e6f;
        }
    }

    // Measure from optic-flow
    if (flow_.healthy())
    {
        if (last_flow_update_s_ < flow_.last_update_s())
        {
            // Do kalman update
            update_flow();

            // Update timing
            last_flow_update_s_ = flow_.last_update_s();
        }
    }

    return true;
}

//------------------------------------------------------------------------------
// PRIVATE FUNCTIONS IMPLEMENTATION
//------------------------------------------------------------------------------

void INS_kf::predict_kf(void)
{
    // Recompute the variable model matrices
    // Get time
    //float dt = 0.004; //dt_;
    float dt = dt_;
    float dt2 = (dt*dt)/2.0f;

    // Get attitude quaternion
    quat_t q = ahrs_.attitude();
    float q0 = q.s;
    float q1 = q.v[0];
    float q2 = q.v[1];
    float q3 = q.v[2];

    // Compute coefficients
    float ax = q0*q0 + q1*q1 - q2*q2 - q3*q3;
    float bx = 2.0f*(-q0*q3 + q1*q2);
    float cx = 2.0f*(q0*q2 + q1*q3);
    float ay = 2.0f*(q0*q3 + q1*q2);
    float by = q0*q0 - q1*q1 + q2*q2 - q3*q3;
    float cy = 2.0f*(-q0*q1 + q2*q3);
    float az = 2.0f*(-q0*q2 + q1*q3);
    float bz = 2.0f*(q0*q1 + q2*q3);
    float cz = q0*q0 - q1*q1 - q2*q2 + q3*q3;

    // Model dynamics (modify only the non-constant terms)
    F_.insert_inplace<0,4>(Mat<3,3>({ dt,  0,  0,
                                       0, dt,  0,
                                       0,  0, dt }));
    F_.insert_inplace<0,7>(Mat<3,3>({ -ax*dt2, -bx*dt2,  -cx*dt2,
                                      -ay*dt2, -by*dt2,  -cy*dt2,
                                      -az*dt2, -bz*dt2,  -cz*dt2 }));
    F_.insert_inplace<4,7>(Mat<3,3>({ -ax*dt,  -bx*dt, -cx*dt,
                                      -ay*dt,  -by*dt, -cy*dt,
                                      -az*dt,  -bz*dt, -cz*dt}));

    // Input (modify only the non-constant terms)
    B_.insert_inplace<0,0>(Mat<3,3>({ ax*dt2, bx*dt2, cx*dt2,
                                      ay*dt2, by*dt2, cy*dt2,
                                      az*dt2, bz*dt2, cz*dt2 }));
    B_.insert_inplace<4,0>(Mat<3,3>({ ax*dt, bx*dt,  cx*dt,
                                      ay*dt, by*dt,  cy*dt,
                                      az*dt, bz*dt,  cz*dt }));

    // Recompute process noise matrix
    // Coefficients
    float axx = ax*ax + bx*bx + cx*cx;
    float ayy = ay*ay + by*by + cy*cy;
    float azz = az*az + bz+bz + cz*cz;
    float axy = ax*ay + bx*by + cx*cy;
    float axz = ax*az + bx*bz + cx*cz;
    float ayz = ay*az + by*bz + cy*cz;
    float sz2 = SQR(config_.sigma_z_gnd);
    float sa2 = SQR(config_.sigma_bias_acc);
    float sb2 = SQR(config_.sigma_bias_baro);
    float su2 = SQR(config_.sigma_acc);
    float sau2 = sa2 + su2;

    // Time constants
    float dt520 = (dt*dt*dt*dt*dt)/20.0f;
    float dt48 = (dt*dt*dt*dt)/8.0f;
    float dt36 = (dt*dt*dt)/6.0f;
    float dt33 = (dt*dt*dt)/3.0f;
    float dt22 = (dt*dt)/2.0f;

    // Matrix
    // TODO: Improve this, using fixed matrix and multiply only terms with axx, axy, ...
    Q_ = Mat<11,11>({ dt520*axx*sau2, dt520*axy*sau2, dt520*axz*sau2, 0,      dt48*axx*sau2,  dt48*axy*sau2,  dt48*axz*sau2,  -dt36*ax*sa2, -dt36*bx*sa2, -dt36*cx*sa2, 0,
                      dt520*axy*sau2, dt520*ayy*sau2, dt520*ayz*sau2, 0,      dt48*axy*sau2,  dt48*ayy*sau2,  dt48*ayz*sau2,  -dt36*ay*sa2, -dt36*by*sa2, -dt36*cy*sa2, 0,
                      dt520*axz*sau2, dt520*ayz*sau2, dt520*azz*sau2, 0,      dt48*axz*sau2,  dt48*ayz*sau2,  dt48*azz*sau2,  -dt36*az*sa2, -dt36*bz*sa2, -dt36*cz*sa2, 0,
                      0,              0,              0,              dt*sz2, 0,              0,              0,              0,            0,            0,            0,
                      dt48*axx*sau2,  dt48*axy*sau2,  dt48*axz*sau2,  0,      dt33*axx*sau2,  dt33*axy*sau2,  dt33*axz*sau2,  -dt22*ax*sa2, -dt22*bx*sa2, -dt22*cx*sa2, 0,
                      dt48*axy*sau2,  dt48*ayy*sau2,  dt48*ayz*sau2,  0,      dt33*axy*sau2,  dt33*ayy*sau2,  dt33*ayz*sau2,  -dt22*ay*sa2, -dt22*by*sa2, -dt22*cy*sa2, 0,
                      dt48*axz*sau2,  dt48*ayz*sau2,  dt48*azz*sau2,  0,      dt33*axz*sau2,  dt33*ayz*sau2,  dt33*azz*sau2,  -dt22*az*sa2, -dt22*bz*sa2, -dt22*cz*sa2, 0,
                      -dt36*ax*sa2,   -dt36*ay*sa2,   -dt36*az*sa2,   0,      -dt22*ax*sa2,   -dt22*ay*sa2,   -dt22*az*sa2,   dt*sa2,       0,            0,            0,
                      -dt36*bx*sa2,   -dt36*by*sa2,   -dt36*bz*sa2,   0,      -dt22*bx*sa2,   -dt22*by*sa2,   -dt22*bz*sa2,   0,            dt*sa2,       0,            0,
                      -dt36*cx*sa2,   -dt36*cy*sa2,   -dt36*cz*sa2,   0,      -dt22*cx*sa2,   -dt22*cy*sa2,   -dt22*cz*sa2,   0,            0,            dt*sa2,       0,
                      0,              0,              0,              0,      0,              0,              0,              0,            0,            0,            dt*sb2 });

    // Compute default KF prediciton step (using local accelerations as input, warning z acceleration sign)
    predict({ahrs_.linear_acceleration()[0], ahrs_.linear_acceleration()[1], ahrs_.linear_acceleration()[2]});
}


void INS_kf::update_gps_pos(void)
{
    // Get local position from gps
    local_position_t  gps_local;
    global_position_t gps_global = gps_.position_gf();
    coord_conventions_global_to_local_position(gps_global, origin(), gps_local);

    // Recompute the measurement noise matrix
    R_ = Mat<3,3>({ SQR(config_.sigma_gps_xy), 0,                         0,
                    0,                         SQR(config_.sigma_gps_xy), 0,
                    0,                         0,                         SQR(config_.sigma_gps_z)});

    // Run kalman update using default matrices
    Kalman<11,3,3>::update({gps_local[0], gps_local[1], gps_local[2]});
}


void INS_kf::update_gps_vel(void)
{
    // Get velocity from GPS
    std::array<float,3> gps_velocity = gps_.velocity_lf();

    // Recompute the measurement noise matrix
    R_gpsvel_ = Mat<3,3>({ SQR(config_.sigma_gps_velxy),  0,                            0,
                           0,                             SQR(config_.sigma_gps_velxy), 0,
                           0,                             0,                            SQR(config_.sigma_gps_velz)});

    // Run kalman update
    Kalman<11,3,3>::update(Mat<3,1>(gps_velocity),
                           H_gpsvel_,
                           R_gpsvel_);
}


void INS_kf::update_gps_mocap(void)
{
    // Get local position from gps
    local_position_t  gps_local;
    global_position_t gps_global = gps_mocap_.position_gf();
    coord_conventions_global_to_local_position(gps_global, origin(), gps_local);

    // Recompute the measurement noise matrix
    R_ = Mat<3,3>({ SQR(config_.sigma_gps_mocap), 0,                         0,
                    0,                         SQR(config_.sigma_gps_mocap), 0,
                    0,                         0,                         SQR(config_.sigma_gps_mocap)});

    // Run kalman update using default matrices
    Kalman<11,3,3>::update({gps_local[0], gps_local[1], gps_local[2]});
}


void INS_kf::update_barometer(void)
{
    // Recompute the measurement noise matrix
    R_baro_ = Mat<1,1>({ SQR(config_.sigma_baro) });

    // Run kalman Update
    float z_baro = barometer_.altitude_gf_raw() - origin().altitude;
    Kalman<11,3,3>::update(Mat<1,1>(z_baro),
                           H_baro_,
                           R_baro_);
}


void INS_kf::update_sonar(void)
{
    float sigma_sonar;
    float z_sonar;

    // If armed, use real measurement and adapt sigma in function of healthiness
    if(state_.is_armed())
    {
        z_sonar = sonar_.distance();
        if(sonar_.healthy())
        {
            sigma_sonar = config_.sigma_sonar;
        }
        else
        {
            sigma_sonar = 0.3f;
        }
    }
    // If unarmed, force measurement to 0, with very good confidence
    else
    {
        z_sonar = 0.0f;
        sigma_sonar = 0.0001f;
    }

    // Recompute the measurement noise matrix
    R_sonar_ = Mat<1,1>({ SQR(sigma_sonar) });

    // Run kalman Update
    Kalman<11,3,3>::update(Mat<1,1>(z_sonar),
                           H_sonar_,
                           R_sonar_);
}


void INS_kf::update_flow(void)
{
    // Get XY velocity in NED frame
    float sigma_flow = config_.sigma_flow;
    float vel_lf[3];
    float vel_bf[3] = {flow_.velocity_x(), flow_.velocity_y(), 0.0f};
    quaternions_rotate_vector(coord_conventions_quaternion_from_rpy(0.0f,
                                                                    0.0f,
                                                                    coord_conventions_get_yaw(ahrs_.attitude())),
                              vel_bf,
                              vel_lf);

    // Get sonar measure
    float sigma_sonar = config_.sigma_sonar;
    float z_sonar     = flow_.ground_distance();
    if (state_.is_armed() == false)
    {
        // If unarmed, force measurement to 0, with very good confidence
        if (z_sonar <= 0.32f)
        {
            z_sonar     = 0.0f;
            vel_lf[0]   = 0.0f;
            vel_lf[1]   = 0.0f;
            // sigma_sonar = 0.0001f;
            // sigma_flow  = 0.0001f;
        }
    }

    // Recompute the measurement noise matrix
    R_flow_(0,0) = SQR(sigma_flow);
    R_flow_(1,1) = SQR(sigma_flow);
    R_flow_(2,2) = SQR(sigma_sonar);

    // Do update
    Kalman<11,3,3>::update(Mat<3,1>({vel_lf[0], vel_lf[1], z_sonar}),
                           H_flow_,
                           R_flow_);
}
