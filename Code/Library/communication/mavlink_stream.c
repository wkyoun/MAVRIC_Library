/**
 * \page The MAV'RIC License
 *
 * The MAV'RIC Framework
 *
 * Copyright © 2011-2014
 *
 * Laboratory of Intelligent Systems, EPFL
 */


/**
 * \file mavlink_stream.c
 * 
 * A wrapper for mavlink to use the stream interface
 */


#include "mavlink_stream.h"
#include "buffer.h"
#include "onboard_parameters.h"
#include "print_util.h"

#include "mavlink_message_handler.h"


mavlink_system_t mavlink_system;
byte_stream_t* mavlink_out_stream;


//------------------------------------------------------------------------------
// PUBLIC FUNCTIONS IMPLEMENTATION
//------------------------------------------------------------------------------

void mavlink_comm_send_ch(mavlink_channel_t chan, uint8_t ch)
{
	if (chan == MAVLINK_COMM_0)
	{
		mavlink_out_stream->put(mavlink_out_stream->data, ch);
	}
}


void mavlink_stream_init(mavlink_stream_t* mavlink_stream, const mavlink_stream_conf_t* config)
{	
	mavlink_out_stream                = config->down_stream;
	mavlink_stream->out_stream        = config->down_stream;
	mavlink_stream->in_stream         = config->up_stream;
	mavlink_stream->message_available = false;
	
	mavlink_system.sysid              = config->sysid; // System ID, 1-255
	mavlink_system.compid             = config->compid; // Component/Subsystem ID, 1-255
}


void mavlink_stream_receive(mavlink_stream_t* mavlink_stream) 
{
	uint8_t byte;
	byte_stream_t* stream = mavlink_stream->in_stream;
	mavlink_received_t* rec = &mavlink_stream->rec;

	if(mavlink_stream->message_available == false)
	{
		while(stream->bytes_available(stream->data) > 0) 
		{
			byte = stream->get(stream->data);
			if(mavlink_parse_char(MAVLINK_COMM_0, byte, &rec->msg, &rec->status)) 
			{
				mavlink_stream->message_available = true;
			}
		}
	}
}


void mavlink_stream_flush(mavlink_stream_t* mavlink_stream) 
{
	byte_stream_t* stream = mavlink_stream->out_stream;

	if (stream->flush != NULL) 
	{
		stream->flush(stream->data);	
	}
}