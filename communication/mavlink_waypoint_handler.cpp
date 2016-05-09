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
 * \file mavlink_waypoint_handler.c
 *
 * \author MAV'RIC Team
 * \author Nicolas Dousse
 *
 * \brief The MAVLink waypoint handler
 *
 ******************************************************************************/


#include "communication/mavlink_waypoint_handler.hpp"
#include <cstdlib>
#include "hal/common/time_keeper.hpp"

extern "C"
{
#include "util/print_util.h"
#include "util/maths.h"
#include "util/constants.h"
}

/**
 * \brief   Set the waypoint depending on the reference frame defined in the current_waypoint_ structure
 *
 * \param   waypoint_handler        The pointer to the waypoint handler structure
 * \param   origin                  The coordinates (latitude, longitude and altitude in global frame) of the local frame's origin
 *
 * \return  The waypoint in local coordinate frame
 */
static local_position_t waypoint_handler_set_waypoint_from_frame(Mavlink_waypoint_handler::waypoint_struct_t* current_waypoint_, global_position_t origin);

//------------------------------------------------------------------------------
// PRIVATE FUNCTIONS IMPLEMENTATION
//------------------------------------------------------------------------------


void Mavlink_waypoint_handler::send_count(Mavlink_waypoint_handler* waypoint_handler, uint32_t sysid, mavlink_message_t* msg)
{
    mavlink_mission_request_list_t packet;

    mavlink_msg_mission_request_list_decode(msg, &packet);

    // Check if this message is for this system and subsystem
    if (((uint8_t)packet.target_system == (uint8_t)sysid)
            && ((uint8_t)packet.target_component == (uint8_t)MAV_COMP_ID_MISSIONPLANNER))
    {
        mavlink_message_t _msg;
        mavlink_msg_mission_count_pack(sysid,
                                       waypoint_handler->mavlink_stream_.compid(),
                                       &_msg,
                                       msg->sysid,
                                       msg->compid,
                                       waypoint_handler->waypoint_count_);
        waypoint_handler->mavlink_stream_.send(&_msg);

        if (waypoint_handler->waypoint_count_ != 0)
        {
            waypoint_handler->waypoint_sending_ = true;
            waypoint_handler->waypoint_receiving_ = false;
            waypoint_handler->start_timeout_ = time_keeper_get_ms();
        }

        waypoint_handler->sending_waypoint_num_ = 0;
        print_util_dbg_print("Will send ");
        print_util_dbg_print_num(waypoint_handler->waypoint_count_, 10);
        print_util_dbg_print(" waypoints\r\n");
    }
}

void Mavlink_waypoint_handler::send_waypoint(Mavlink_waypoint_handler* waypoint_handler, uint32_t sysid, mavlink_message_t* msg)
{
    if (waypoint_handler->waypoint_sending_)
    {
        mavlink_mission_request_t packet;

        mavlink_msg_mission_request_decode(msg, &packet);

        print_util_dbg_print("Asking for waypoint number ");
        print_util_dbg_print_num(packet.seq, 10);
        print_util_dbg_print("\r\n");

        // Check if this message is for this system and subsystem
        if (((uint8_t)packet.target_system == (uint8_t)sysid)
                && ((uint8_t)packet.target_component == (uint8_t)MAV_COMP_ID_MISSIONPLANNER))
        {
            waypoint_handler->sending_waypoint_num_ = packet.seq;
            if (waypoint_handler->sending_waypoint_num_ < waypoint_handler->waypoint_count_)
            {
                //  Prototype of the function "mavlink_msg_mission_item_send" found in mavlink_msg_mission_item.h :
                // mavlink_msg_mission_item_send (  mavlink_channel_t chan, uint8_t target_system, uint8_t target_component, uint16_t seq,
                //                                  uint8_t frame, uint16_t command, uint8_t current, uint8_t autocontinue, float param1,
                //                                  float param2, float param3, float param4, float x, float y, float z)
                mavlink_message_t _msg;
                mavlink_msg_mission_item_pack(sysid,
                                              waypoint_handler->mavlink_stream_.compid(),
                                              &_msg,
                                              msg->sysid,
                                              msg->compid,
                                              packet.seq,
                                              waypoint_handler->waypoint_list[waypoint_handler->sending_waypoint_num_].frame,
                                              waypoint_handler->waypoint_list[waypoint_handler->sending_waypoint_num_].command,
                                              waypoint_handler->waypoint_list[waypoint_handler->sending_waypoint_num_].current,
                                              waypoint_handler->waypoint_list[waypoint_handler->sending_waypoint_num_].autocontinue,
                                              waypoint_handler->waypoint_list[waypoint_handler->sending_waypoint_num_].param1,
                                              waypoint_handler->waypoint_list[waypoint_handler->sending_waypoint_num_].param2,
                                              waypoint_handler->waypoint_list[waypoint_handler->sending_waypoint_num_].param3,
                                              waypoint_handler->waypoint_list[waypoint_handler->sending_waypoint_num_].param4,
                                              waypoint_handler->waypoint_list[waypoint_handler->sending_waypoint_num_].x,
                                              waypoint_handler->waypoint_list[waypoint_handler->sending_waypoint_num_].y,
                                              waypoint_handler->waypoint_list[waypoint_handler->sending_waypoint_num_].z);
                waypoint_handler->mavlink_stream_.send(&_msg);

                print_util_dbg_print("Sending waypoint ");
                print_util_dbg_print_num(waypoint_handler->sending_waypoint_num_, 10);
                print_util_dbg_print("\r\n");

                waypoint_handler->start_timeout_ = time_keeper_get_ms();
            }
        }
    }
}

void Mavlink_waypoint_handler::receive_ack_msg(Mavlink_waypoint_handler* waypoint_handler, uint32_t sysid, mavlink_message_t* msg)
{
    mavlink_mission_ack_t packet;

    mavlink_msg_mission_ack_decode(msg, &packet);

    // Check if this message is for this system and subsystem
    if (((uint8_t)packet.target_system == (uint8_t)sysid)
            && ((uint8_t)packet.target_component == (uint8_t)MAV_COMP_ID_MISSIONPLANNER))
    {
        waypoint_handler->waypoint_sending_ = false;
        waypoint_handler->sending_waypoint_num_ = 0;
        print_util_dbg_print("Acknowledgment received, end of waypoint sending.\r\n");
    }
}

void Mavlink_waypoint_handler::receive_count(Mavlink_waypoint_handler* waypoint_handler, uint32_t sysid, mavlink_message_t* msg)
{
    mavlink_mission_count_t packet;

    mavlink_msg_mission_count_decode(msg, &packet);

    print_util_dbg_print("Count:");
    print_util_dbg_print_num(packet.count, 10);
    print_util_dbg_print("\r\n");

    // Check if this message is for this system and subsystem
    if (((uint8_t)packet.target_system == (uint8_t)sysid)
            && ((uint8_t)packet.target_component == (uint8_t)MAV_COMP_ID_MISSIONPLANNER))
    {
        if (waypoint_handler->waypoint_receiving_ == false)
        {
            // comment these lines if you want to add new waypoints to the list instead of overwriting them
            waypoint_handler->waypoint_onboard_count_ = 0;
            waypoint_handler->waypoint_count_ = 0;
            //---//

            if ((packet.count + waypoint_handler->waypoint_count_) > MAX_WAYPOINTS)
            {
                packet.count = MAX_WAYPOINTS - waypoint_handler->waypoint_count_;
            }
            waypoint_handler->waypoint_count_ =  packet.count + waypoint_handler->waypoint_count_;
            print_util_dbg_print("Receiving ");
            print_util_dbg_print_num(packet.count, 10);
            print_util_dbg_print(" new waypoints. ");
            print_util_dbg_print("New total number of waypoints:");
            print_util_dbg_print_num(waypoint_handler->waypoint_count_, 10);
            print_util_dbg_print("\r\n");

            waypoint_handler->waypoint_receiving_   = true;
            waypoint_handler->waypoint_sending_     = false;
            waypoint_handler->waypoint_request_number_ = 0;


            waypoint_handler->start_timeout_ = time_keeper_get_ms();
        }

        mavlink_message_t _msg;
        mavlink_msg_mission_request_pack(sysid,
                                         waypoint_handler->mavlink_stream_.compid(),
                                         &_msg,
                                         msg->sysid,
                                         msg->compid,
                                         waypoint_handler->waypoint_request_number_);
        waypoint_handler->mavlink_stream_.send(&_msg);

        print_util_dbg_print("Asking for waypoint ");
        print_util_dbg_print_num(waypoint_handler->waypoint_request_number_, 10);
        print_util_dbg_print("\r\n");
    }

}

void Mavlink_waypoint_handler::receive_waypoint(Mavlink_waypoint_handler* waypoint_handler, uint32_t sysid, mavlink_message_t* msg)
{
    mavlink_mission_item_t packet;

    mavlink_msg_mission_item_decode(msg, &packet);

    // Check if this message is for this system and subsystem
    if (((uint8_t)packet.target_system == (uint8_t)sysid)
            && ((uint8_t)packet.target_component == (uint8_t)MAV_COMP_ID_MISSIONPLANNER))
    {
        waypoint_handler->start_timeout_ = time_keeper_get_ms();

        waypoint_struct_t new_waypoint;

        new_waypoint.command = packet.command;

        new_waypoint.x = packet.x; // longitude
        new_waypoint.y = packet.y; // latitude
        new_waypoint.z = packet.z; // altitude

        new_waypoint.autocontinue = packet.autocontinue;
        new_waypoint.frame = packet.frame;

        new_waypoint.current = packet.current;

        new_waypoint.param1 = packet.param1;
        new_waypoint.param2 = packet.param2;
        new_waypoint.param3 = packet.param3;
        new_waypoint.param4 = packet.param4;

        print_util_dbg_print("New waypoint received ");
        //print_util_dbg_print("(");
        //print_util_dbg_print_num(new_waypoint.x,10);
        //print_util_dbg_print(", ");
        //print_util_dbg_print_num(new_waypoint.y,10);
        //print_util_dbg_print(", ");
        //print_util_dbg_print_num(new_waypoint.z,10);
        //print_util_dbg_print(") Autocontinue:");
        //print_util_dbg_print_num(new_waypoint.autocontinue,10);
        //print_util_dbg_print(" Frame:");
        //print_util_dbg_print_num(new_waypoint.frame,10);
        //print_util_dbg_print(" Current :");
        //print_util_dbg_print_num(packet.current,10);
        //print_util_dbg_print(" Seq :");
        //print_util_dbg_print_num(packet.seq,10);
        //print_util_dbg_print(" command id :");
        //print_util_dbg_print_num(packet.command,10);
        print_util_dbg_print(" requested num :");
        print_util_dbg_print_num(waypoint_handler->waypoint_request_number_, 10);
        print_util_dbg_print(" receiving num :");
        print_util_dbg_print_num(packet.seq, 10);
        //print_util_dbg_print(" is it receiving :");
        //print_util_dbg_print_num(waypoint_handler->waypoint_receiving_,10); // boolean value
        print_util_dbg_print("\r\n");

        //current = 2 is a flag to tell us this is a "guided mode" waypoint and not for the mission
        if (packet.current == 2)
        {
            // verify we received the command;
            mavlink_message_t _msg;
            mavlink_msg_mission_ack_pack(sysid,
                                         waypoint_handler->mavlink_stream_.compid(),
                                         &_msg,
                                         msg->sysid,
                                         msg->compid,
                                         MAV_MISSION_UNSUPPORTED);
            waypoint_handler->mavlink_stream_.send(&_msg);
        }
        else if (packet.current == 3)
        {
            //current = 3 is a flag to tell us this is a alt change only

            // verify we received the command
            mavlink_message_t _msg;
            mavlink_msg_mission_ack_pack(waypoint_handler->mavlink_stream_.sysid(),
                                         waypoint_handler->mavlink_stream_.compid(),
                                         &_msg,
                                         msg->sysid,
                                         msg->compid,
                                         MAV_MISSION_UNSUPPORTED);
            waypoint_handler->mavlink_stream_.send(&_msg);
        }
        else
        {
            // Check if receiving waypoints
            if (waypoint_handler->waypoint_receiving_)
            {
                // check if this is the requested waypoint
                if (packet.seq == waypoint_handler->waypoint_request_number_)
                {
                    print_util_dbg_print("Receiving good waypoint, number ");
                    print_util_dbg_print_num(waypoint_handler->waypoint_request_number_, 10);
                    print_util_dbg_print(" of ");
                    print_util_dbg_print_num(waypoint_handler->waypoint_count_ - waypoint_handler->waypoint_onboard_count_, 10);
                    print_util_dbg_print("\r\n");

                    waypoint_handler->waypoint_list[waypoint_handler->waypoint_onboard_count_ + waypoint_handler->waypoint_request_number_] = new_waypoint;
                    waypoint_handler->waypoint_request_number_++;

                    if ((waypoint_handler->waypoint_onboard_count_ + waypoint_handler->waypoint_request_number_) == waypoint_handler->waypoint_count_)
                    {
                        MAV_MISSION_RESULT type = MAV_MISSION_ACCEPTED;

                        mavlink_message_t _msg;
                        mavlink_msg_mission_ack_pack(waypoint_handler->mavlink_stream_.sysid(),
                                                     waypoint_handler->mavlink_stream_.compid(),
                                                     &_msg,
                                                     msg->sysid,
                                                     msg->compid, type);
                        waypoint_handler->mavlink_stream_.send(&_msg);

                        print_util_dbg_print("flight plan received!\n");
                        waypoint_handler->waypoint_receiving_ = false;
                        waypoint_handler->waypoint_onboard_count_ = waypoint_handler->waypoint_count_;

                        waypoint_handler->start_wpt_time_ = time_keeper_get_ms();

                        waypoint_handler->state_.nav_plan_active = false;
                        waypoint_handler->nav_plan_init();
                    }
                    else
                    {
                        mavlink_message_t _msg;
                        mavlink_msg_mission_request_pack(waypoint_handler->mavlink_stream_.sysid(),
                                                         waypoint_handler->mavlink_stream_.compid(),
                                                         &_msg,
                                                         msg->sysid,
                                                         msg->compid,
                                                         waypoint_handler->waypoint_request_number_);
                        waypoint_handler->mavlink_stream_.send(&_msg);

                        print_util_dbg_print("Asking for waypoint ");
                        print_util_dbg_print_num(waypoint_handler->waypoint_request_number_, 10);
                        print_util_dbg_print("\n");
                    }
                } //end of if (packet.seq == waypoint_handler->waypoint_request_number_)
                else
                {
                    MAV_MISSION_RESULT type = MAV_MISSION_INVALID_SEQUENCE;

                    mavlink_message_t _msg;
                    mavlink_msg_mission_ack_pack(waypoint_handler->mavlink_stream_.sysid(),
                                                 waypoint_handler->mavlink_stream_.compid(),
                                                 &_msg,
                                                 msg->sysid,
                                                 msg->compid,
                                                 type);
                    waypoint_handler->mavlink_stream_.send(&_msg);
                }
            } //end of if (waypoint_handler->waypoint_receiving_)
            else
            {
                MAV_MISSION_RESULT type = MAV_MISSION_ERROR;
                print_util_dbg_print("Not ready to receive waypoints right now!\r\n");

                mavlink_message_t _msg;
                mavlink_msg_mission_ack_pack(waypoint_handler->mavlink_stream_.sysid(),
                                             waypoint_handler->mavlink_stream_.compid(),
                                             &_msg,
                                             msg->sysid,
                                             msg->compid,
                                             type);
                waypoint_handler->mavlink_stream_.send(&_msg);
            } //end of else of if (waypoint_handler->waypoint_receiving_)
        } //end of else (packet.current != 2 && !=3 )
    } //end of if this message is for this system and subsystem
}

void Mavlink_waypoint_handler::set_current_waypoint(Mavlink_waypoint_handler* waypoint_handler, uint32_t sysid, mavlink_message_t* msg)
{
    mavlink_mission_set_current_t packet;

    mavlink_msg_mission_set_current_decode(msg, &packet);

    // Check if this message is for this system and subsystem
    if (((uint8_t)packet.target_system == (uint8_t)sysid)
            && ((uint8_t)packet.target_component == (uint8_t)MAV_COMP_ID_MISSIONPLANNER))
    {
        if (packet.seq < waypoint_handler->waypoint_count_)
        {
            for (int32_t i = 0; i < waypoint_handler->waypoint_count_; i++)
            {
                waypoint_handler->waypoint_list[i].current = 0;
            }

            waypoint_handler->waypoint_list[packet.seq].current = 1;

            mavlink_message_t _msg;
            mavlink_msg_mission_current_pack(sysid,
                                             waypoint_handler->mavlink_stream_.compid(),
                                             &_msg,
                                             packet.seq);
            waypoint_handler->mavlink_stream_.send(&_msg);

            print_util_dbg_print("Set current waypoint to number");
            print_util_dbg_print_num(packet.seq, 10);
            print_util_dbg_print("\r\n");

            waypoint_handler->start_wpt_time_ = time_keeper_get_ms();

            waypoint_handler->state_.nav_plan_active = false;
            waypoint_handler->nav_plan_init();
        }
        else
        {
            mavlink_message_t _msg;
            mavlink_msg_mission_ack_pack(waypoint_handler->mavlink_stream_.sysid(),
                                         waypoint_handler->mavlink_stream_.compid(),
                                         &_msg,
                                         msg->sysid,
                                         msg->compid,
                                         MAV_CMD_ACK_ERR_ACCESS_DENIED);
            waypoint_handler->mavlink_stream_.send(&_msg);
        }
    } //end of if this message is for this system and subsystem
}

mav_result_t Mavlink_waypoint_handler::set_current_waypoint_from_parameter(Mavlink_waypoint_handler* waypoint_handler, mavlink_command_long_t* packet)
{
    mav_result_t result;
    uint16_t new_current = 0;

    print_util_dbg_print("All MAVs: Return to first waypoint.\r\n");

    if (new_current < waypoint_handler->waypoint_count_)
    {
        for (uint8_t i = 0; i < waypoint_handler->waypoint_count_; i++)
        {
            waypoint_handler->waypoint_list[i].current = 0;
        }
        waypoint_handler->waypoint_list[new_current].current = 1;

        mavlink_message_t msg;
        mavlink_msg_mission_current_pack(waypoint_handler->mavlink_stream_.sysid(),
                                         waypoint_handler->mavlink_stream_.compid(),
                                         &msg,
                                         new_current);
        waypoint_handler->mavlink_stream_.send(&msg);

        print_util_dbg_print("Set current waypoint to number");
        print_util_dbg_print_num(new_current, 10);
        print_util_dbg_print("\r\n");

        waypoint_handler->start_wpt_time_ = time_keeper_get_ms();

        waypoint_handler->state_.nav_plan_active = false;
        waypoint_handler->nav_plan_init();

        result = MAV_RESULT_ACCEPTED;
    }
    else
    {
        result = MAV_RESULT_DENIED;
    }

    return result;
}


void Mavlink_waypoint_handler::clear_waypoint_list(Mavlink_waypoint_handler* waypoint_handler, uint32_t sysid, mavlink_message_t* msg)
{
    mavlink_mission_clear_all_t packet;

    mavlink_msg_mission_clear_all_decode(msg, &packet);

    // Check if this message is for this system and subsystem
    if (((uint8_t)packet.target_system == (uint8_t)sysid)
            && ((uint8_t)packet.target_component == (uint8_t)MAV_COMP_ID_MISSIONPLANNER))
    {
        if (waypoint_handler->waypoint_count_ > 0)
        {
            waypoint_handler->waypoint_count_ = 0;
            waypoint_handler->waypoint_onboard_count_ = 0;
            waypoint_handler->state_.nav_plan_active = false;
            waypoint_handler->hold_waypoint_set_ = false;

            mavlink_message_t _msg;
            mavlink_msg_mission_ack_pack(waypoint_handler->mavlink_stream_.sysid(),
                                         waypoint_handler->mavlink_stream_.compid(),
                                         &_msg,
                                         msg->sysid,
                                         msg->compid,
                                         MAV_CMD_ACK_OK);
            waypoint_handler->mavlink_stream_.send(&_msg);

            print_util_dbg_print("Cleared Waypoint list.\r\n");
        }
    }
}


void Mavlink_waypoint_handler::set_home(Mavlink_waypoint_handler* waypoint_handler, uint32_t sysid, mavlink_message_t* msg)
{
    mavlink_set_gps_global_origin_t packet;

    if (!waypoint_handler->state_.is_armed())
    {
        mavlink_msg_set_gps_global_origin_decode(msg, &packet);

        // Check if this message is for this system and subsystem
        // Due to possible bug from QGroundControl, no check of target_component and compid
        if ((uint8_t)packet.target_system == (uint8_t)sysid)
        {
            print_util_dbg_print("Set new home location.\r\n");
            waypoint_handler->position_estimation_.local_position.origin.latitude = (double) packet.latitude / 10000000.0f;
            waypoint_handler->position_estimation_.local_position.origin.longitude = (double) packet.longitude / 10000000.0f;
            waypoint_handler->position_estimation_.local_position.origin.altitude = (float) packet.altitude / 1000.0f;

            print_util_dbg_print("New Home location: (");
            print_util_dbg_print_num(waypoint_handler->position_estimation_.local_position.origin.latitude * 10000000.0f, 10);
            print_util_dbg_print(", ");
            print_util_dbg_print_num(waypoint_handler->position_estimation_.local_position.origin.longitude * 10000000.0f, 10);
            print_util_dbg_print(", ");
            print_util_dbg_print_num(waypoint_handler->position_estimation_.local_position.origin.altitude * 1000.0f, 10);
            print_util_dbg_print(")\r\n");


            waypoint_handler->position_estimation_.set_new_fence_origin();

            mavlink_message_t _msg;
            mavlink_msg_gps_global_origin_pack(waypoint_handler->mavlink_stream_.sysid(),
                                               waypoint_handler->mavlink_stream_.compid(),
                                               &_msg,
                                               waypoint_handler->position_estimation_.local_position.origin.latitude * 10000000.0f,
                                               waypoint_handler->position_estimation_.local_position.origin.longitude * 10000000.0f,
                                               waypoint_handler->position_estimation_.local_position.origin.altitude * 1000.0f);
            waypoint_handler->mavlink_stream_.send(&_msg);
        }
    }
}

mav_result_t Mavlink_waypoint_handler::continue_to_next_waypoint(Mavlink_waypoint_handler* waypoint_handler, mavlink_command_long_t* packet)
{
    mav_result_t result;
    bool force_next = false;
    uint32_t time_from_start_wpt = time_keeper_get_ms() - waypoint_handler->start_wpt_time_;
    uint32_t time_wpt_limit = 5000;

    if (packet->param3 == 1)
    {
        // QGroundControl sends every message twice,
        //  therefore we do this test to avoid continuing two times in a row towards next waypoint
        if (time_from_start_wpt > time_wpt_limit) // 5 seconds
        {
            force_next = true;
        }
    }

    if ((waypoint_handler->waypoint_count_ > 0) && ((!waypoint_handler->state_.nav_plan_active) || force_next))
    {
        print_util_dbg_print("All vehicles: Navigating to next waypoint.\r\n");

        waypoint_handler->waypoint_list[waypoint_handler->current_waypoint_index_].current = 0;

        print_util_dbg_print("Continuing towards waypoint Nr");

        waypoint_handler->start_wpt_time_ = time_keeper_get_ms();

        if (waypoint_handler->current_waypoint_index_ == (waypoint_handler->waypoint_count_ - 1))
        {
            waypoint_handler->current_waypoint_index_ = 0;
        }
        else
        {
            waypoint_handler->current_waypoint_index_++;
        }
        print_util_dbg_print_num(waypoint_handler->current_waypoint_index_, 10);
        print_util_dbg_print("\r\n");
        waypoint_handler->waypoint_list[waypoint_handler->current_waypoint_index_].current = 1;
        waypoint_handler->current_waypoint_ = waypoint_handler->waypoint_list[waypoint_handler->current_waypoint_index_];
        waypoint_handler->waypoint_coordinates_ = waypoint_handler_set_waypoint_from_frame(&waypoint_handler->current_waypoint_, waypoint_handler->position_estimation_.local_position.origin);

        mavlink_message_t msg;
        mavlink_msg_mission_current_pack(waypoint_handler->mavlink_stream_.sysid(),
                                         waypoint_handler->mavlink_stream_.compid(),
                                         &msg,
                                         waypoint_handler->current_waypoint_index_);
        waypoint_handler->mavlink_stream_.send(&msg);

        waypoint_handler->state_.nav_plan_active = true;

        result = MAV_RESULT_ACCEPTED;
    }
    else
    {
        result = MAV_RESULT_TEMPORARILY_REJECTED;

        print_util_dbg_print("Not ready to switch to next waypoint. Either no waypoint loaded or flying towards one\r\n");
    }

    // To avoid a MAV_RESULT_TEMPORARILY_REJECTED for the second message and thus
    //  a bad information to the user on the ground, if two messages are received
    //  in a short time interval, we still show the result as MAV_RESULT_ACCEPTED
    if (time_from_start_wpt < time_wpt_limit)
    {
        result = MAV_RESULT_ACCEPTED;
    }

    return result;
}

mav_result_t Mavlink_waypoint_handler::is_arrived(Mavlink_waypoint_handler* waypoint_handler, mavlink_command_long_t* packet)
{
    mav_result_t result;

    if (packet->param2 == 32)
    {
        if (waypoint_handler->waypoint_list[waypoint_handler->current_waypoint_index_].current == 0)
        {
            result = MAV_RESULT_ACCEPTED;
        }
        else
        {
            result = MAV_RESULT_TEMPORARILY_REJECTED;
        }
    }
    else
    {
        result = MAV_RESULT_DENIED;
    }

    return result;
}

bool Mavlink_waypoint_handler::take_off_handler()
{
    bool result = false;

    if (!hold_waypoint_set_)
    {
        print_util_dbg_print("Automatic take-off, will hold position at: (");
        print_util_dbg_print_num(position_estimation_.local_position.pos[X], 10);
        print_util_dbg_print(", ");
        print_util_dbg_print_num(position_estimation_.local_position.pos[Y], 10);
        print_util_dbg_print(", ");
        print_util_dbg_print_num(-10.0f, 10);
        print_util_dbg_print("), with heading of: ");
        print_util_dbg_print_num((int32_t)(position_estimation_.local_position.heading * 180.0f / 3.14f), 10);
        print_util_dbg_print("\r\n");

        waypoint_hold_coordinates = position_estimation_.local_position;
        waypoint_hold_coordinates.pos[Z] = -config_.auto_take_off_altitude;     // change sign since NED!

        aero_attitude_t aero_attitude;
        aero_attitude = coord_conventions_quat_to_aero(ahrs_.qe);
        waypoint_hold_coordinates.heading = aero_attitude.rpy[2];

        navigation_.dist2wp_sqr = 100.0f; // same position, 10m above => dist_sqr = 100.0f

        hold_waypoint_set_ = true;
    }

    if (mode_change())
    {
        if (navigation_.dist2wp_sqr <= 16.0f)
        {
            result = true;

            print_util_dbg_print("Automatic take-off finished, dist2wp_sqr (10x):");
            print_util_dbg_print_num(navigation_.dist2wp_sqr * 10.0f, 10);
            print_util_dbg_print(".\r\n");
        }
    }

    return result;
}

mav_result_t Mavlink_waypoint_handler::start_stop_navigation(Mavlink_waypoint_handler* waypoint_handler, mavlink_command_long_t* packet)
{
    mav_result_t result = MAV_RESULT_UNSUPPORTED;

    if (packet->param1 == MAV_GOTO_DO_HOLD)
    {
        if (packet->param2 == MAV_GOTO_HOLD_AT_CURRENT_POSITION)
        {
            waypoint_handler->hold_init(waypoint_handler->position_estimation_.local_position);

            waypoint_handler->navigation_.internal_state_ = Navigation::NAV_STOP_ON_POSITION;

            result = MAV_RESULT_ACCEPTED;
        }
        else if (packet->param2 == MAV_GOTO_HOLD_AT_SPECIFIED_POSITION)
        {
            //navigation.stop_nav_there = true;
            waypoint_handler->navigation_.internal_state_ = Navigation::NAV_STOP_THERE;

            waypoint_struct_t waypoint;

            waypoint.frame = packet->param3;
            waypoint.param4 = packet->param4;
            waypoint.x = packet->param5;
            waypoint.y = packet->param6;
            waypoint.z = packet->param7;

            local_position_t waypoint_goal = waypoint_handler_set_waypoint_from_frame(&waypoint, waypoint_handler->position_estimation_.local_position.origin);
            waypoint_handler->hold_init(waypoint_goal);

            result = MAV_RESULT_ACCEPTED;
        }
    }
    else if (packet->param1 == MAV_GOTO_DO_CONTINUE)
    {

        if (mav_modes_is_auto(waypoint_handler->last_mode_))  // WHY USE LAST_MODE RATHER THAN STATE->MODE?
        {
            waypoint_handler->navigation_.internal_state_ = Navigation::NAV_NAVIGATING;
        }
        else if (mav_modes_is_guided(waypoint_handler->last_mode_))  // WHY USE LAST_MODE RATHER THAN STATE->MODE?
        {
            waypoint_handler->navigation_.internal_state_ = Navigation::NAV_HOLD_POSITION;
        }


        result = MAV_RESULT_ACCEPTED;
    }

    return result;
}

mav_result_t Mavlink_waypoint_handler::set_auto_takeoff(Mavlink_waypoint_handler* waypoint_handler, mavlink_command_long_t* packet)
{
    mav_result_t result;

    if (waypoint_handler->navigation_.internal_state_ == Navigation::NAV_ON_GND)
    {
        print_util_dbg_print("Starting automatic take-off from button\r\n");
        waypoint_handler->navigation_.internal_state_ = Navigation::NAV_TAKEOFF;
        waypoint_handler->hold_waypoint_set_ = false;

        result = MAV_RESULT_ACCEPTED;
    }
    else
    {
        result = MAV_RESULT_DENIED;
    }

    return result;
}

mav_result_t Mavlink_waypoint_handler::set_auto_landing(Mavlink_waypoint_handler* waypoint_handler, mavlink_command_long_t* packet)
{
    mav_result_t result;


    if ((waypoint_handler->navigation_.internal_state_ == Navigation::NAV_NAVIGATING) || (waypoint_handler->navigation_.internal_state_ == Navigation::NAV_HOLD_POSITION)
        || (waypoint_handler->navigation_.internal_state_ == Navigation::NAV_STOP_ON_POSITION) || (waypoint_handler->navigation_.internal_state_ == Navigation::NAV_STOP_THERE))
    {
        result = MAV_RESULT_ACCEPTED;

        waypoint_handler->navigation_.auto_landing_behavior = Navigation::DESCENT_TO_SMALL_ALTITUDE;
        waypoint_handler->auto_landing_next_state_ = false;

        waypoint_handler->navigation_.internal_state_ = Navigation::NAV_LANDING;

        print_util_dbg_print("Auto-landing procedure initialised.\r\n");
    }
    else
    {
        result = MAV_RESULT_DENIED;
    }

    return result;
}

void Mavlink_waypoint_handler::auto_landing_handler()
{
    float rel_pos[3];

    bool next_state_ = false;

    if (!auto_landing_next_state_)
    {
        auto_landing_next_state_ = true;

        switch (navigation_.auto_landing_behavior)
        {
            case Navigation::DESCENT_TO_SMALL_ALTITUDE:
                print_util_dbg_print("Cust: descent to small alt");
                state_.mav_mode_custom &= static_cast<mav_mode_custom_t>(0xFFFFFFE0);
                state_.mav_mode_custom |= CUST_DESCENT_TO_SMALL_ALTITUDE;
                waypoint_hold_coordinates = position_estimation_.local_position;
                waypoint_hold_coordinates.pos[Z] = -5.0f;
                break;

            case Navigation::DESCENT_TO_GND:
                print_util_dbg_print("Cust: descent to gnd");
                state_.mav_mode_custom &= static_cast<mav_mode_custom_t>(0xFFFFFFE0);
                state_.mav_mode_custom |= CUST_DESCENT_TO_GND;
                waypoint_hold_coordinates = position_estimation_.local_position;
                waypoint_hold_coordinates.pos[Z] = 0.0f;
                navigation_.alt_lpf = position_estimation_.local_position.pos[2];
                break;
        }

        for (uint8_t i = 0; i < 3; i++)
        {
            rel_pos[i] = waypoint_hold_coordinates.pos[i] - position_estimation_.local_position.pos[i];
        }

        navigation_.dist2wp_sqr = vectors_norm_sqr(rel_pos);
    }

    if (navigation_.auto_landing_behavior == Navigation::DESCENT_TO_GND)
    {
        navigation_.alt_lpf = navigation_.LPF_gain * (navigation_.alt_lpf) + (1.0f - navigation_.LPF_gain) * position_estimation_.local_position.pos[2];
        if ((position_estimation_.local_position.pos[2] > -0.1f) && (maths_f_abs(position_estimation_.local_position.pos[2] - navigation_.alt_lpf) <= 0.2f))
        {
            // Disarming
            next_state_ = true;
        }
    }

    if (navigation_.auto_landing_behavior == Navigation::DESCENT_TO_SMALL_ALTITUDE)
    {
        if ((navigation_.dist2wp_sqr < 3.0f) && (maths_f_abs(position_estimation_.local_position.pos[2] - waypoint_hold_coordinates.pos[2]) < 0.5f))
        {
            next_state_ = true;
        }
    }

    if (next_state_)
    {
        auto_landing_next_state_ = false;

        switch (navigation_.auto_landing_behavior)
        {
            case Navigation::DESCENT_TO_SMALL_ALTITUDE:
                print_util_dbg_print("Automatic-landing: descent_to_GND\r\n");
                navigation_.auto_landing_behavior = Navigation::DESCENT_TO_GND;
                break;

            case Navigation::DESCENT_TO_GND:
                print_util_dbg_print("Auto-landing: disarming motors \r\n");
                navigation_.auto_landing_behavior = Navigation::DESCENT_TO_SMALL_ALTITUDE;
                //state_.mav_mode_custom = CUSTOM_BASE_MODE;
                hold_waypoint_set_ = false;
                navigation_.internal_state_ = Navigation::NAV_ON_GND;
                state_.set_armed(false);
                state_.mav_state_ = MAV_STATE_STANDBY;
                break;
        }
    }
}

void Mavlink_waypoint_handler::state_machine()
{
    mav_mode_t mode_local = state_.mav_mode();

    float thrust;

    bool takeoff_result = false;
    bool new_mode = true;

    switch (navigation_.internal_state_)
    {
        case Navigation::NAV_ON_GND:
            thrust = manual_control_.get_thrust();

            if (thrust > -0.7f)
            {
                if (mav_modes_is_guided(mode_local) || mav_modes_is_auto(mode_local))
                {
                    hold_waypoint_set_ = false;
                    navigation_.internal_state_ = Navigation::NAV_TAKEOFF;
                }
                else
                {
                    navigation_.internal_state_ = Navigation::NAV_MANUAL_CTRL;
                }
            }
            break;

        case Navigation::NAV_TAKEOFF:
            takeoff_result = take_off_handler();

            navigation_.goal = waypoint_hold_coordinates;

            if (takeoff_result)
            {
                if (mav_modes_is_auto(mode_local))
                {
                    navigation_.internal_state_ = Navigation::NAV_NAVIGATING;
                }
                else if (mav_modes_is_guided(mode_local))
                {
                    navigation_.internal_state_ = Navigation::NAV_HOLD_POSITION;
                }
            }

            if ((!mav_modes_is_guided(mode_local)) && (!mav_modes_is_auto(mode_local)))
            {
                navigation_.internal_state_ = Navigation::NAV_MANUAL_CTRL;
            }
            break;

        case Navigation::NAV_MANUAL_CTRL:
            if (mav_modes_is_auto(mode_local))
            {
                navigation_.internal_state_ = Navigation::NAV_NAVIGATING;
            }
            else if (mav_modes_is_guided(mode_local))
            {
                print_util_dbg_print("Switching to Navigation::NAV_HOLD_POSITION from Navigation::NAV_MANUAL_CTRL\r\n");
                hold_init(position_estimation_.local_position);
                navigation_.internal_state_ = Navigation::NAV_HOLD_POSITION;
            }

            navigation_.critical_behavior = Navigation::CLIMB_TO_SAFE_ALT;
            critical_next_state_ = false;
            navigation_.auto_landing_behavior = Navigation::DESCENT_TO_SMALL_ALTITUDE;
            break;

        case Navigation::NAV_NAVIGATING:
            if (!mav_modes_is_auto(last_mode_))
            {
                new_mode = mode_change();
            }
            waypoint_navigation_handler(new_mode);

            navigation_.goal = waypoint_coordinates_;

            if (!mav_modes_is_auto(mode_local))
            {
                if (mav_modes_is_guided(mode_local))
                {
                    print_util_dbg_print("Switching to Navigation::NAV_HOLD_POSITION from Navigation::NAV_NAVIGATING\r\n");
                    waypoint_hold_coordinates = position_estimation_.local_position;
                    navigation_.internal_state_ = Navigation::NAV_HOLD_POSITION;
                }
                else
                {
                    print_util_dbg_print("Switching to Navigation::NAV_MANUAL_CTRL from Navigation::NAV_NAVIGATING\r\n");
                    navigation_.internal_state_ = Navigation::NAV_MANUAL_CTRL;
                }
            }

            break;

        case Navigation::NAV_HOLD_POSITION:
            navigation_.goal = waypoint_hold_coordinates;

            if (mav_modes_is_auto(mode_local))
            {
                print_util_dbg_print("Switching to Navigation::NAV_NAVIGATING from Navigation::NAV_HOLD_POSITION\r\n");
                navigation_.internal_state_ = Navigation::NAV_NAVIGATING;
            }
            else if (!mav_modes_is_guided(mode_local))
            {
                print_util_dbg_print("Switching to Navigation::NAV_MANUAL_CTRL from Navigation::NAV_HOLD_POSITION\r\n");
                navigation_.internal_state_ = Navigation::NAV_MANUAL_CTRL;
            }
            break;

        case Navigation::NAV_STOP_ON_POSITION:
            navigation_.goal = waypoint_hold_coordinates;

            if ((!mav_modes_is_auto(mode_local)) && (!mav_modes_is_guided(mode_local)))
            {
                navigation_.internal_state_ = Navigation::NAV_MANUAL_CTRL;
            }
            break;

        case Navigation::NAV_STOP_THERE:
            stopping_handler();

            navigation_.goal = waypoint_hold_coordinates;

            if ((!mav_modes_is_auto(mode_local)) && (!mav_modes_is_guided(mode_local)))
            {
                navigation_.internal_state_ = Navigation::NAV_MANUAL_CTRL;
            }
            break;

        case Navigation::NAV_LANDING:
            auto_landing_handler();

            navigation_.goal = waypoint_hold_coordinates;

            if ((!mav_modes_is_auto(mode_local)) && (!mav_modes_is_guided(mode_local)))
            {
                navigation_.internal_state_ = Navigation::NAV_MANUAL_CTRL;
            }
            break;
    }
}

void Mavlink_waypoint_handler::stopping_handler()
{
    float dist2wp_sqr;
    float rel_pos[3];

    rel_pos[X] = (float)(waypoint_hold_coordinates.pos[X] - position_estimation_.local_position.pos[X]);
    rel_pos[Y] = (float)(waypoint_hold_coordinates.pos[Y] - position_estimation_.local_position.pos[Y]);
    rel_pos[Z] = (float)(waypoint_hold_coordinates.pos[Z] - position_estimation_.local_position.pos[Z]);

    dist2wp_sqr = vectors_norm_sqr(rel_pos);
    if (dist2wp_sqr < 25.0f)
    {
        //navigation_.stop_nav = true;
        navigation_.internal_state_ = Navigation::NAV_STOP_ON_POSITION;
    }
}

void Mavlink_waypoint_handler::critical_handler()
{
    float rel_pos[3];
    bool next_state_ = false;

    //Check whether we entered critical mode due to a battery low level or a lost
    // connection with the GND station or are out of fence control
    if (state_.battery_.is_low() ||
            state_.connection_lost ||
            state_.out_of_fence_2 ||
            position_estimation_.healthy() == false)
    {
        if (navigation_.critical_behavior != Navigation::CRITICAL_LAND)
        {
            navigation_.critical_behavior = Navigation::CRITICAL_LAND;
            critical_next_state_ = false;
        }
    }

    if (!(critical_next_state_))
    {
        critical_next_state_ = true;

        aero_attitude_t aero_attitude;
        aero_attitude = coord_conventions_quat_to_aero(navigation_.qe);
        waypoint_critical_coordinates_.heading = aero_attitude.rpy[2];

        switch (navigation_.critical_behavior)
        {
            case Navigation::CLIMB_TO_SAFE_ALT:
                print_util_dbg_print("Climbing to safe alt...\r\n");
                state_.mav_mode_custom |= CUST_CRITICAL_CLIMB_TO_SAFE_ALT;

                waypoint_critical_coordinates_.pos[X] = position_estimation_.local_position.pos[X];
                waypoint_critical_coordinates_.pos[Y] = position_estimation_.local_position.pos[Y];
                waypoint_critical_coordinates_.pos[Z] = -30.0f;

                break;

            case Navigation::FLY_TO_HOME_WP:
                state_.mav_mode_custom &= ~CUST_CRITICAL_CLIMB_TO_SAFE_ALT;
                state_.mav_mode_custom |= CUST_CRITICAL_FLY_TO_HOME_WP;

                waypoint_critical_coordinates_.pos[X] = 0.0f;
                waypoint_critical_coordinates_.pos[Y] = 0.0f;
                waypoint_critical_coordinates_.pos[Z] = -30.0f;
                break;

            case Navigation::HOME_LAND:
                state_.mav_mode_custom &= ~CUST_CRITICAL_FLY_TO_HOME_WP;
                state_.mav_mode_custom |= CUST_CRITICAL_LAND;
                waypoint_critical_coordinates_.pos[X] = 0.0f;
                waypoint_critical_coordinates_.pos[Y] = 0.0f;
                waypoint_critical_coordinates_.pos[Z] = 5.0f;
                navigation_.alt_lpf = position_estimation_.local_position.pos[2];
                break;

            case Navigation::CRITICAL_LAND:
                print_util_dbg_print("Critical land...\r\n");
                state_.mav_mode_custom &= static_cast<mav_mode_custom_t>(0xFFFFFFE0);
                state_.mav_mode_custom |= CUST_CRITICAL_LAND;
                waypoint_critical_coordinates_.pos[X] = position_estimation_.local_position.pos[X];
                waypoint_critical_coordinates_.pos[Y] = position_estimation_.local_position.pos[Y];
                waypoint_critical_coordinates_.pos[Z] = 5.0f;
                navigation_.alt_lpf = position_estimation_.local_position.pos[2];
                break;
        }

        for (uint8_t i = 0; i < 3; i++)
        {
            rel_pos[i] = waypoint_critical_coordinates_.pos[i] - position_estimation_.local_position.pos[i];
        }
        navigation_.dist2wp_sqr = vectors_norm_sqr(rel_pos);
    }

    if (navigation_.critical_behavior == Navigation::CRITICAL_LAND || navigation_.critical_behavior == Navigation::HOME_LAND)
    {
        navigation_.alt_lpf = navigation_.LPF_gain * navigation_.alt_lpf + (1.0f - navigation_.LPF_gain) * position_estimation_.local_position.pos[2];
        if ((position_estimation_.local_position.pos[2] > -0.1f) && (maths_f_abs(position_estimation_.local_position.pos[2] - navigation_.alt_lpf) <= 0.2f))
        {
            // Disarming
            next_state_ = true;
        }
    }

    if ((navigation_.critical_behavior == Navigation::CLIMB_TO_SAFE_ALT) || (navigation_.critical_behavior == Navigation::FLY_TO_HOME_WP))
    {
        if (navigation_.dist2wp_sqr < 3.0f)
        {
            next_state_ = true;
        }
    }

    if (next_state_)
    {
        critical_next_state_ = false;
        switch (navigation_.critical_behavior)
        {
            case Navigation::CLIMB_TO_SAFE_ALT:
                print_util_dbg_print("Critical State! Flying to home waypoint.\r\n");
                navigation_.critical_behavior = Navigation::FLY_TO_HOME_WP;
                break;

            case Navigation::FLY_TO_HOME_WP:
                if (state_.out_of_fence_1)
                {
                    //stop auto navigation_, to prevent going out of fence 1 again
                    waypoint_hold_coordinates = waypoint_critical_coordinates_;
                    navigation_.internal_state_ = Navigation::NAV_STOP_ON_POSITION;
                    stopping_handler();
                    state_.out_of_fence_1 = false;
                    navigation_.critical_behavior = Navigation::CLIMB_TO_SAFE_ALT;
                    state_.mav_state_ = MAV_STATE_ACTIVE;
                    state_.mav_mode_custom &= ~CUST_CRITICAL_FLY_TO_HOME_WP;
                }
                else
                {
                    print_util_dbg_print("Critical State! Performing critical landing.\r\n");
                    navigation_.critical_behavior = Navigation::HOME_LAND;
                }
                break;

            case Navigation::HOME_LAND:
            case Navigation::CRITICAL_LAND:
                print_util_dbg_print("Critical State! Landed, switching off motors, Emergency mode.\r\n");
                navigation_.critical_behavior = Navigation::CLIMB_TO_SAFE_ALT;
                //state_.mav_mode_custom = CUSTOM_BASE_MODE;
                navigation_.internal_state_ = Navigation::NAV_ON_GND;
                state_.set_armed(false);
                state_.mav_state_ = MAV_STATE_EMERGENCY;
                break;
        }
    }
}

void Mavlink_waypoint_handler::waypoint_navigation_handler(bool reset_hold_wpt)
{
    if (!reset_hold_wpt)
    {
        hold_waypoint_set_ = false;
    }

    if (state_.nav_plan_active)
    {
        float rel_pos[3];

        for (uint8_t i = 0; i < 3; i++)
        {
            rel_pos[i] = waypoint_coordinates_.pos[i] - position_estimation_.local_position.pos[i];
        }
        navigation_.dist2wp_sqr = vectors_norm_sqr(rel_pos);

        if (navigation_.dist2wp_sqr < (current_waypoint_.param2 * current_waypoint_.param2))
        {
            print_util_dbg_print("Waypoint Nr");
            print_util_dbg_print_num(current_waypoint_index_, 10);
            print_util_dbg_print(" reached, distance:");
            print_util_dbg_print_num(sqrt(navigation_.dist2wp_sqr), 10);
            print_util_dbg_print(" less than :");
            print_util_dbg_print_num(current_waypoint_.param2, 10);
            print_util_dbg_print(".\r\n");

            mavlink_message_t msg;
            mavlink_msg_mission_item_reached_pack(mavlink_stream_.sysid(),
                                                  mavlink_stream_.compid(),
                                                  &msg,
                                                  current_waypoint_index_);
            mavlink_stream_.send(&msg);

            travel_time_ = time_keeper_get_ms() - start_wpt_time_;

            waypoint_list[current_waypoint_index_].current = 0;
            if ((current_waypoint_.autocontinue == 1) && (waypoint_count_ > 1))
            {
                print_util_dbg_print("Autocontinue towards waypoint Nr");

                start_wpt_time_ = time_keeper_get_ms();

                if (current_waypoint_index_ == (waypoint_count_ - 1))
                {
                    current_waypoint_index_ = 0;
                }
                else
                {
                    current_waypoint_index_++;
                }
                print_util_dbg_print_num(current_waypoint_index_, 10);
                print_util_dbg_print("\r\n");
                waypoint_list[current_waypoint_index_].current = 1;
                current_waypoint_ = waypoint_list[current_waypoint_index_];

                waypoint_coordinates_ = waypoint_handler_set_waypoint_from_frame(&current_waypoint_, position_estimation_.local_position.origin);

                mavlink_message_t msg;
                mavlink_msg_mission_current_pack(mavlink_stream_.sysid(),
                                                 mavlink_stream_.compid(),
                                                 &msg,
                                                 current_waypoint_index_);
                mavlink_stream_.send(&msg);

            }
            else
            {
                state_.nav_plan_active = false;
                print_util_dbg_print("Stop\r\n");
            }
        }
    }
    else
    {
        if (!hold_waypoint_set_)
        {
            hold_waypoint_set_ = true;
            waypoint_coordinates_ = position_estimation_.local_position;
        }
    }
}

bool Mavlink_waypoint_handler::mode_change()
{
    return mav_modes_are_equal_autonomous_modes(state_.mav_mode(), last_mode_);
}

void Mavlink_waypoint_handler::control_time_out_waypoint_msg()
{
    if (waypoint_sending_ || waypoint_receiving_)
    {
        uint32_t tnow = time_keeper_get_ms();

        if ((tnow - start_timeout_) > timeout_max_waypoint_)
        {
            start_timeout_ = tnow;
            if (waypoint_sending_)
            {
                waypoint_sending_ = false;
                print_util_dbg_print("Sending waypoint timeout\r\n");
            }
            if (waypoint_receiving_)
            {
                waypoint_receiving_ = false;

                print_util_dbg_print("Receiving waypoint timeout\r\n");
                waypoint_count_ = 0;
                waypoint_onboard_count_ = 0;
            }
        }
    }
}

static local_position_t waypoint_handler_set_waypoint_from_frame(Mavlink_waypoint_handler::waypoint_struct_t* current_waypoint_, global_position_t origin)
{
    global_position_t waypoint_global;
    local_position_t waypoint_coor;
    global_position_t origin_relative_alt;

    for (uint8_t i = 0; i < 3; i++)
    {
        waypoint_coor.pos[i] = 0.0f;
    }
    waypoint_coor.origin = origin;
    waypoint_coor.heading = maths_deg_to_rad(current_waypoint_->param4);

    switch (current_waypoint_->frame)
    {
        case MAV_FRAME_GLOBAL:
            waypoint_global.latitude    = current_waypoint_->x;
            waypoint_global.longitude   = current_waypoint_->y;
            waypoint_global.altitude    = current_waypoint_->z;
            waypoint_global.heading     = maths_deg_to_rad(current_waypoint_->param4);
            waypoint_coor = coord_conventions_global_to_local_position(waypoint_global, origin);

            print_util_dbg_print("waypoint_global: lat (x1e7):");
            print_util_dbg_print_num(waypoint_global.latitude * 10000000, 10);
            print_util_dbg_print(" long (x1e7):");
            print_util_dbg_print_num(waypoint_global.longitude * 10000000, 10);
            print_util_dbg_print(" alt (x1000):");
            print_util_dbg_print_num(waypoint_global.altitude * 1000, 10);
            print_util_dbg_print(" waypoint_coor: x (x100):");
            print_util_dbg_print_num(waypoint_coor.pos[X] * 100, 10);
            print_util_dbg_print(", y (x100):");
            print_util_dbg_print_num(waypoint_coor.pos[Y] * 100, 10);
            print_util_dbg_print(", z (x100):");
            print_util_dbg_print_num(waypoint_coor.pos[Z] * 100, 10);
            print_util_dbg_print(" localOrigin lat (x1e7):");
            print_util_dbg_print_num(origin.latitude * 10000000, 10);
            print_util_dbg_print(" long (x1e7):");
            print_util_dbg_print_num(origin.longitude * 10000000, 10);
            print_util_dbg_print(" alt (x1000):");
            print_util_dbg_print_num(origin.altitude * 1000, 10);
            print_util_dbg_print("\r\n");
            break;

        case MAV_FRAME_LOCAL_NED:
            waypoint_coor.pos[X] = current_waypoint_->x;
            waypoint_coor.pos[Y] = current_waypoint_->y;
            waypoint_coor.pos[Z] = current_waypoint_->z;
            waypoint_coor.heading = maths_deg_to_rad(current_waypoint_->param4);
            waypoint_coor.origin = coord_conventions_local_to_global_position(waypoint_coor);
            break;

        case MAV_FRAME_MISSION:
            // Problem here: rec is not defined here
            //mavlink_msg_mission_ack_send(MAVLINK_COMM_0,rec->msg.sysid,rec->msg.compid,MAV_CMD_ACK_ERR_NOT_SUPPORTED);
            break;
        case MAV_FRAME_GLOBAL_RELATIVE_ALT:
            waypoint_global.latitude = current_waypoint_->x;
            waypoint_global.longitude = current_waypoint_->y;
            waypoint_global.altitude = current_waypoint_->z;
            waypoint_global.heading     = maths_deg_to_rad(current_waypoint_->param4);

            origin_relative_alt = origin;
            origin_relative_alt.altitude = 0.0f;
            waypoint_coor = coord_conventions_global_to_local_position(waypoint_global, origin_relative_alt);

            waypoint_coor.heading = maths_deg_to_rad(current_waypoint_->param4);

            print_util_dbg_print("LocalOrigin: lat (x1e7):");
            print_util_dbg_print_num(origin_relative_alt.latitude * 10000000, 10);
            print_util_dbg_print(" long (x1e7):");
            print_util_dbg_print_num(origin_relative_alt.longitude * 10000000, 10);
            print_util_dbg_print(" global alt (x1000):");
            print_util_dbg_print_num(origin.altitude * 1000, 10);
            print_util_dbg_print(" waypoint_coor: x (x100):");
            print_util_dbg_print_num(waypoint_coor.pos[X] * 100, 10);
            print_util_dbg_print(", y (x100):");
            print_util_dbg_print_num(waypoint_coor.pos[Y] * 100, 10);
            print_util_dbg_print(", z (x100):");
            print_util_dbg_print_num(waypoint_coor.pos[Z] * 100, 10);
            print_util_dbg_print("\r\n");

            break;
        case MAV_FRAME_LOCAL_ENU:
            // Problem here: rec is not defined here
            //mavlink_msg_mission_ack_send(MAVLINK_COMM_0,rec->msg.sysid,rec->msg.compid,MAV_CMD_ACK_ERR_NOT_SUPPORTED);
            break;

    }

    return waypoint_coor;
}

//------------------------------------------------------------------------------
// PUBLIC FUNCTIONS IMPLEMENTATION
//------------------------------------------------------------------------------

Mavlink_waypoint_handler::Mavlink_waypoint_handler(Position_estimation& position_estimation_, Navigation& navigation_, const ahrs_t& ahrs_, State& state_, const Manual_control& manual_control_, Mavlink_message_handler& message_handler, const Mavlink_stream& mavlink_stream_, conf_t config):
            waypoint_count_(0),
            current_waypoint_index_(0),
            hold_waypoint_set_(false),
            start_wpt_time_(time_keeper_get_ms()),
            mavlink_stream_(mavlink_stream_),
            state_(state_),
            navigation_(navigation_),
            position_estimation_(position_estimation_),
            waypoint_sending_(false),
            waypoint_receiving_(false),
            sending_waypoint_num_(0),
            waypoint_request_number_(0),
            waypoint_onboard_count_(0),
            start_timeout_(time_keeper_get_ms()),
            timeout_max_waypoint_(10000),
            travel_time_(0),
            critical_next_state_(false),
            auto_landing_next_state_(0),
            last_mode_(state_.mav_mode()),
            ahrs_(ahrs_),
            manual_control_(manual_control_),
            config_(config)
{
    bool init_success = true;

    navigation_.internal_state_ = Navigation::NAV_ON_GND;
    navigation_.critical_behavior = Navigation::CLIMB_TO_SAFE_ALT;
    navigation_.auto_landing_behavior = Navigation::DESCENT_TO_SMALL_ALTITUDE;

    // Add callbacks for waypoint handler messages requests
    Mavlink_message_handler::msg_callback_t callback;

    callback.message_id     = MAVLINK_MSG_ID_MISSION_ITEM; // 39
    callback.sysid_filter   = MAVLINK_BASE_STATION_ID;
    callback.compid_filter  = MAV_COMP_ID_ALL;
    callback.function       = (Mavlink_message_handler::msg_callback_func_t)      &receive_waypoint;
    callback.module_struct  = (Mavlink_message_handler::handling_module_struct_t) this;
    init_success &= message_handler.add_msg_callback(&callback);

    callback.message_id     = MAVLINK_MSG_ID_MISSION_REQUEST; // 40
    callback.sysid_filter   = MAVLINK_BASE_STATION_ID;
    callback.compid_filter  = MAV_COMP_ID_ALL;
    callback.function       = (Mavlink_message_handler::msg_callback_func_t)      &send_waypoint;
    callback.module_struct  = (Mavlink_message_handler::handling_module_struct_t) this;
    init_success &= message_handler.add_msg_callback(&callback);

    callback.message_id     = MAVLINK_MSG_ID_MISSION_SET_CURRENT; // 41
    callback.sysid_filter   = MAVLINK_BASE_STATION_ID;
    callback.compid_filter  = MAV_COMP_ID_ALL;
    callback.function       = (Mavlink_message_handler::msg_callback_func_t)      &set_current_waypoint;
    callback.module_struct  = (Mavlink_message_handler::handling_module_struct_t) this;
    init_success &= message_handler.add_msg_callback(&callback);

    callback.message_id     = MAVLINK_MSG_ID_MISSION_REQUEST_LIST; // 43
    callback.sysid_filter   = MAVLINK_BASE_STATION_ID;
    callback.compid_filter  = MAV_COMP_ID_ALL;
    callback.function       = (Mavlink_message_handler::msg_callback_func_t)      &send_count;
    callback.module_struct  = (Mavlink_message_handler::handling_module_struct_t) this;
    init_success &= message_handler.add_msg_callback(&callback);

    callback.message_id     = MAVLINK_MSG_ID_MISSION_COUNT; // 44
    callback.sysid_filter   = MAVLINK_BASE_STATION_ID;
    callback.compid_filter  = MAV_COMP_ID_ALL;
    callback.function       = (Mavlink_message_handler::msg_callback_func_t)      &receive_count;
    callback.module_struct  = (Mavlink_message_handler::handling_module_struct_t) this;
    init_success &= message_handler.add_msg_callback(&callback);

    callback.message_id     = MAVLINK_MSG_ID_MISSION_CLEAR_ALL; // 45
    callback.sysid_filter   = MAVLINK_BASE_STATION_ID;
    callback.compid_filter  = MAV_COMP_ID_ALL;
    callback.function       = (Mavlink_message_handler::msg_callback_func_t)      &clear_waypoint_list;
    callback.module_struct  = (Mavlink_message_handler::handling_module_struct_t) this;
    init_success &= message_handler.add_msg_callback(&callback);

    callback.message_id     = MAVLINK_MSG_ID_MISSION_ACK; // 47
    callback.sysid_filter   = MAVLINK_BASE_STATION_ID;
    callback.compid_filter  = MAV_COMP_ID_ALL;
    callback.function       = (Mavlink_message_handler::msg_callback_func_t)      &receive_ack_msg;
    callback.module_struct  = (Mavlink_message_handler::handling_module_struct_t) this;
    init_success &= message_handler.add_msg_callback(&callback);

    callback.message_id     = MAVLINK_MSG_ID_SET_GPS_GLOBAL_ORIGIN; // 48
    callback.sysid_filter   = MAVLINK_BASE_STATION_ID;
    callback.compid_filter  = MAV_COMP_ID_ALL;
    callback.function       = (Mavlink_message_handler::msg_callback_func_t)      &set_home;
    callback.module_struct  = (Mavlink_message_handler::handling_module_struct_t) this;
    init_success &= message_handler.add_msg_callback(&callback);

    // Add callbacks for waypoint handler commands requests
    Mavlink_message_handler::cmd_callback_t callbackcmd;

    callbackcmd.command_id = MAV_CMD_NAV_RETURN_TO_LAUNCH; // 20
    callbackcmd.sysid_filter = MAVLINK_BASE_STATION_ID;
    callbackcmd.compid_filter = MAV_COMP_ID_ALL;
    callbackcmd.compid_target = MAV_COMP_ID_MISSIONPLANNER; // 190
    callbackcmd.function = (Mavlink_message_handler::cmd_callback_func_t)           &set_current_waypoint_from_parameter;
    callbackcmd.module_struct = (Mavlink_message_handler::handling_module_struct_t) this;
    init_success &= message_handler.add_cmd_callback(&callbackcmd);

    callbackcmd.command_id = MAV_CMD_NAV_LAND; // 21
    callbackcmd.sysid_filter = MAVLINK_BASE_STATION_ID;
    callbackcmd.compid_filter = MAV_COMP_ID_ALL;
    callbackcmd.compid_target = MAV_COMP_ID_ALL; // 0
    callbackcmd.function = (Mavlink_message_handler::cmd_callback_func_t)           &set_auto_landing;
    callbackcmd.module_struct = (Mavlink_message_handler::handling_module_struct_t) this;
    init_success &= message_handler.add_cmd_callback(&callbackcmd);

    callbackcmd.command_id = MAV_CMD_NAV_TAKEOFF; // 22
    callbackcmd.sysid_filter = MAVLINK_BASE_STATION_ID;
    callbackcmd.compid_filter = MAV_COMP_ID_ALL;
    callbackcmd.compid_target = MAV_COMP_ID_ALL; // 0
    callbackcmd.function = (Mavlink_message_handler::cmd_callback_func_t)           &set_auto_takeoff;
    callbackcmd.module_struct =                                 this;
    init_success &= message_handler.add_cmd_callback(&callbackcmd);

    callbackcmd.command_id = MAV_CMD_MISSION_START; // 300
    callbackcmd.sysid_filter = MAVLINK_BASE_STATION_ID;
    callbackcmd.compid_filter = MAV_COMP_ID_ALL;
    callbackcmd.compid_target = MAV_COMP_ID_MISSIONPLANNER; // 190
    callbackcmd.function = (Mavlink_message_handler::cmd_callback_func_t)           &continue_to_next_waypoint;
    callbackcmd.module_struct = (Mavlink_message_handler::handling_module_struct_t) this;
    init_success &= message_handler.add_cmd_callback(&callbackcmd);

    callbackcmd.command_id = MAV_CMD_CONDITION_DISTANCE; // 114
    callbackcmd.sysid_filter = MAVLINK_BASE_STATION_ID;
    callbackcmd.compid_filter = MAV_COMP_ID_ALL;
    callbackcmd.compid_target = MAV_COMP_ID_MISSIONPLANNER; // 190
    callbackcmd.function = (Mavlink_message_handler::cmd_callback_func_t)           &is_arrived;
    callbackcmd.module_struct = (Mavlink_message_handler::handling_module_struct_t) this;
    init_success &= message_handler.add_cmd_callback(&callbackcmd);

    callbackcmd.command_id = MAV_CMD_OVERRIDE_GOTO; // 252
    callbackcmd.sysid_filter = MAVLINK_BASE_STATION_ID;
    callbackcmd.compid_filter = MAV_COMP_ID_ALL;
    callbackcmd.compid_target = MAV_COMP_ID_ALL; // 0
    callbackcmd.function = (Mavlink_message_handler::cmd_callback_func_t)           &start_stop_navigation;
    callbackcmd.module_struct = (Mavlink_message_handler::handling_module_struct_t) this;
    init_success &= message_handler.add_cmd_callback(&callbackcmd);

    init_homing_waypoint();
    nav_plan_init();

    if(!init_success)
    {
        print_util_dbg_print("[MAVLINK_WAYPOINT_HANDLER] constructor: ERROR\r\n");
    }
}


bool Mavlink_waypoint_handler::update(Mavlink_waypoint_handler* waypoint_handler)
{
    mav_mode_t mode_local = waypoint_handler->state_.mav_mode();


    switch (waypoint_handler->state_.mav_state_)
    {
        case MAV_STATE_STANDBY:
            waypoint_handler->navigation_.internal_state_ = Navigation::NAV_ON_GND;
            waypoint_handler->hold_waypoint_set_ = false;
            waypoint_handler->navigation_.critical_behavior = Navigation::CLIMB_TO_SAFE_ALT;
            waypoint_handler->critical_next_state_ = false;
            waypoint_handler->navigation_.auto_landing_behavior = Navigation::DESCENT_TO_SMALL_ALTITUDE;
            break;

        case MAV_STATE_ACTIVE:
            waypoint_handler->navigation_.critical_behavior = Navigation::CLIMB_TO_SAFE_ALT;
            waypoint_handler->critical_next_state_ = false;

            if (!waypoint_handler->state_.nav_plan_active)
            {
                waypoint_handler->nav_plan_init();
            }

            waypoint_handler->state_machine();
            break;

        case MAV_STATE_CRITICAL:
            // In MAV_MODE_VELOCITY_CONTROL, MAV_MODE_POSITION_HOLD and MAV_MODE_GPS_NAVIGATION
            if (mav_modes_is_stabilize(mode_local))
            {
                if ((waypoint_handler->navigation_.internal_state_ == Navigation::NAV_NAVIGATING) || (waypoint_handler->navigation_.internal_state_ == Navigation::NAV_LANDING))
                {
                    waypoint_handler->critical_handler();

                    waypoint_handler->navigation_.goal = waypoint_handler->waypoint_critical_coordinates_;
                }
            }
            break;

        default:
            waypoint_handler->navigation_.internal_state_ = Navigation::NAV_ON_GND;
            break;
    }

    waypoint_handler->last_mode_ = mode_local;

    waypoint_handler->control_time_out_waypoint_msg();

    return true;
}

void Mavlink_waypoint_handler::init_homing_waypoint()
{
    waypoint_struct_t waypoint;

    waypoint_count_ = 1;

    waypoint_onboard_count_ = waypoint_count_;

    //Set home waypoint
    waypoint.autocontinue = 0;
    waypoint.current = 1;
    waypoint.frame = MAV_FRAME_LOCAL_NED;
    waypoint.command = MAV_CMD_NAV_WAYPOINT;

    waypoint.x = 0.0f;
    waypoint.y = 0.0f;
    waypoint.z = -config_.auto_take_off_altitude;;

    waypoint.param1 = 10; // Hold time in decimal seconds
    waypoint.param2 = 2; // Acceptance radius in meters
    waypoint.param3 = 0; //  0 to pass through the WP, if > 0 radius in meters to pass by WP. Positive value for clockwise orbit, negative value for counter-clockwise orbit. Allows trajectory control.
    waypoint.param4 = 0; // Desired yaw angle at MISSION (rotary wing)

    waypoint_list[0] = waypoint;
}


void Mavlink_waypoint_handler::nav_plan_init()
{
    float rel_pos[3];

    if ((waypoint_count_ > 0)
            && (position_estimation_.init_gps_position || mav_modes_is_hil(state_.mav_mode()))
            && (waypoint_receiving_ == false))
    {
        for (uint8_t i = 0; i < waypoint_count_; i++)
        {
            if ((waypoint_list[i].current == 1) && (!state_.nav_plan_active))
            {
                current_waypoint_index_ = i;
                current_waypoint_ = waypoint_list[current_waypoint_index_];
                waypoint_coordinates_ = waypoint_handler_set_waypoint_from_frame(&current_waypoint_, position_estimation_.local_position.origin);

                print_util_dbg_print("Waypoint Nr");
                print_util_dbg_print_num(i, 10);
                print_util_dbg_print(" set,\r\n");

                state_.nav_plan_active = true;

                for (uint8_t j = 0; j < 3; j++)
                {
                    rel_pos[j] = waypoint_coordinates_.pos[j] - position_estimation_.local_position.pos[j];
                }
                navigation_.dist2wp_sqr = vectors_norm_sqr(rel_pos);
            }
        }
    }
}

void Mavlink_waypoint_handler::hold_init(local_position_t local_pos)
{
    hold_waypoint_set_ = true;

    waypoint_hold_coordinates = local_pos;

    //waypoint_hold_coordinates.heading = coord_conventions_get_yaw(ahrs_->qe);
    //waypoint_hold_coordinates.heading = local_pos.heading;

    print_util_dbg_print("Position hold at: (");
    print_util_dbg_print_num(waypoint_hold_coordinates.pos[X], 10);
    print_util_dbg_print(", ");
    print_util_dbg_print_num(waypoint_hold_coordinates.pos[Y], 10);
    print_util_dbg_print(", ");
    print_util_dbg_print_num(waypoint_hold_coordinates.pos[Z], 10);
    print_util_dbg_print(", ");
    print_util_dbg_print_num((int32_t)(waypoint_hold_coordinates.heading * 180.0f / 3.14f), 10);
    print_util_dbg_print(")\r\n");

}

void Mavlink_waypoint_handler::send_nav_time(const Mavlink_stream* mavlink_stream_, mavlink_message_t* msg)
{
    mavlink_msg_named_value_int_pack(mavlink_stream_->sysid(),
                                     mavlink_stream_->compid(),
                                     msg,
                                     time_keeper_get_ms(),
                                     "travel_time_",
                                     travel_time_);
}
