/*
 * imu.c
 *
 *  Created on: Mar 7, 2010
 *      Author: felix
 */
#include "imu.h"

#include "qfilter.h"
#include "delay.h"
#include "itg3200_driver.h"
#include "adxl345_driver.h"
#include "time_keeper.h"


int ic;
void init_imu (Imu_Data_t *imu1) {
	
	init_itg3200_slow();
	
	init_adxl345_slow();

	calibrate_Gyros(imu1);
	imu1->raw_scale[0] =  -12600.0; ///823.6511   
	imu1->raw_scale[1] =  -12600.0;
	imu1->raw_scale[2] =  12600.0;
	imu1->raw_scale[3] =  -260.0;
	imu1->raw_scale[4] =  260.0;
	imu1->raw_scale[5] =  -260.0;
	
	//myquad
	imu1->raw_bias[3]=11.0;
	imu1->raw_bias[4]=10.0;
	imu1->raw_bias[5]=-19.0;
	
	//Geraud
//	imu1->raw_bias[3]=6.0;
//	imu1->raw_bias[4]=16.0;
//	imu1->raw_bias[5]=-34.0;
	
	qfInit(&imu1->attitude, imu1->raw_scale, imu1->raw_bias);
	imu1->attitude.calibration_level=OFF;
	
}


void imu_get_raw_data(Imu_Data_t *imu1) {
	int i=0;
	gyro_data* gyros=get_gyro_data_slow();

	acc_data* accs=get_acc_data_slow();

	imu1->raw_channels[GYRO_OFFSET+IMU_X]=(float)gyros->axes[RAW_GYRO_X];
	imu1->raw_channels[GYRO_OFFSET+IMU_Y]=(float)gyros->axes[RAW_GYRO_Y];
	imu1->raw_channels[GYRO_OFFSET+IMU_Z]=(float)gyros->axes[RAW_GYRO_Z];

	imu1->raw_channels[ACC_OFFSET+IMU_X]=(float)accs->axes[RAW_ACC_X];
	imu1->raw_channels[ACC_OFFSET+IMU_Y]=(float)accs->axes[RAW_ACC_Y];
	imu1->raw_channels[ACC_OFFSET+IMU_Z]=(float)accs->axes[RAW_ACC_Z];
		
}

void calibrate_Gyros(Imu_Data_t *imu1) {
	int i,j;
	imu_get_raw_data(imu1);
	for (j=0; j<3; j++) {
		imu1->raw_bias[j]=(float)imu1->raw_channels[j];
	}
	
	for (i=0; i<200; i++) {
		imu_get_raw_data(imu1);
		for (j=0; j<3; j++) {
			imu1->raw_bias[j]=(0.9*imu1->raw_bias[j]+0.1*(float)imu1->raw_channels[j]);
		}
		delay_ms(10);
	}


}

void imu_update(Imu_Data_t *imu1){
	uint32_t t=get_time_ticks();
	
	imu_get_raw_data(imu1);
	imu1->dt=ticks_to_seconds(t - imu1->last_update);
	imu1->last_update=t;
	qfilter(&imu1->attitude, &imu1->raw_channels, imu1->dt);
}




