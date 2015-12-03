/*
 * (c) MediaTek Inc. 2010
 */


#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include <asm/system.h>

#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "ov5694mipi_Sensor.h"
#include "ov5694mipi_Camera_Sensor_para.h"
#include "ov5694mipi_CameraCustomized.h"

#define OV5694MIPI_DRIVER_TRACE
#define OV5694MIPI_DEBUG
#ifdef OV5694MIPI_DEBUG
#define SENSORDB(fmt, arg...) printk("%s: " fmt "\n", __FUNCTION__ ,##arg)
#else
#define SENSORDB(x,...)
#endif

typedef enum
{
    OV5694MIPI_SENSOR_MODE_INIT,
    OV5694MIPI_SENSOR_MODE_PREVIEW,  
    OV5694MIPI_SENSOR_MODE_CAPTURE,
    OV5694MIPI_SENSOR_MODE_VIDEO,
} OV5694MIPI_SENSOR_MODE;

/* SENSOR PRIVATE STRUCT */
typedef struct OV5694MIPI_sensor_STRUCT
{
    MSDK_SENSOR_CONFIG_STRUCT cfg_data;
    sensor_data_struct eng; /* engineer mode */
    MSDK_SENSOR_ENG_INFO_STRUCT eng_info;
    kal_uint8 mirror;

    OV5694MIPI_SENSOR_MODE sensor_mode;
    
    kal_bool video_mode;
    kal_bool NightMode;
    kal_uint16 normal_fps; /* video normal mode max fps */
    kal_uint16 night_fps; /* video night mode max fps */
    kal_uint16 FixedFps;
    kal_uint16 shutter;
    kal_uint16 gain;
    kal_uint32 pclk;
    kal_uint16 frame_length;
    kal_uint16 line_length;

    kal_uint16 dummy_pixel;
    kal_uint16 dummy_line;

	kal_uint8 i2c_write_id;
	kal_uint8 i2c_read_id;
} OV5694MIPI_sensor_struct;


static MSDK_SCENARIO_ID_ENUM mCurrentScenarioId = MSDK_SCENARIO_ID_CAMERA_PREVIEW;
static kal_bool OV5694MIPIAutoFlickerMode = KAL_FALSE;
static int ov5694_otp_data_year=0;
static int ov5694_otp_data_month=0;
static int ov5694_otp_data_day=0;


extern int iReadRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u8 * a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u16 i2cId);

void OV5694MIPISetMaxFrameRate(UINT16 u2FrameRate);

static DEFINE_SPINLOCK(ov5694mipi_drv_lock);


static OV5694MIPI_sensor_struct OV5694MIPI_sensor =
{
    .eng =
    {
        .reg = CAMERA_SENSOR_REG_DEFAULT_VALUE,
        .cct = CAMERA_SENSOR_CCT_DEFAULT_VALUE,
    },
    .eng_info =
    {
        .SensorId = 128,
        .SensorType = CMOS_SENSOR,
        .SensorOutputDataFormat = OV5694MIPI_COLOR_FORMAT,
    },
    .sensor_mode = OV5694MIPI_SENSOR_MODE_INIT,
    .shutter = 0x3D0,  
    .gain = 0x100,
    .pclk = OV5694MIPI_PREVIEW_CLK,
    .frame_length = OV5694MIPI_PV_PERIOD_LINE_NUMS,
    .line_length = OV5694MIPI_PV_PERIOD_PIXEL_NUMS,
    .dummy_pixel = 0,
    .dummy_line = 0,

    .i2c_write_id = 0x6c,
    .i2c_read_id = 0x6d,
};

kal_bool OV5694MIPIDuringTestPattern = KAL_FALSE;
#define OV5694MIPI_TEST_PATTERN_CHECKSUM (0xf7375923)


kal_uint16 OV5694MIPI_read_cmos_sensor(kal_uint32 addr)
{
    kal_uint16 get_byte=0;

    char puSendCmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };
    iReadRegI2C(puSendCmd, 2, (u8*)&get_byte, 1, OV5694MIPI_sensor.i2c_write_id);

    return get_byte;
}

void OV5694MIPI_write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
    char puSendCmd[3] = {(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF)};
    iWriteRegI2C(puSendCmd, 3, OV5694MIPI_sensor.i2c_write_id);
}
#ifdef OV5694_OTP
// index: index of otp group. (1, 2, 3)
// return: 0, group index is empty
// 1, group index has invalid data
// 2, group index has valid data
int ov5694_check_otp_wb(int index)
{
	int flag, i;
	int bank, address;
	// select bank index
	bank = 0xc0 | index;
	OV5694MIPI_write_cmos_sensor(0x3d84, bank);
	// read otp into buffer
	OV5694MIPI_write_cmos_sensor(0x3d81, 0x01);
	msleep(5);
	// read flag
	address = 0x3d00;
	flag = OV5694MIPI_read_cmos_sensor(address);
	flag = flag & 0xc0;
	// clear otp buffer
	for (i=0;i<16;i++)
	{
		OV5694MIPI_write_cmos_sensor(0x3d00 + i, 0x00);
	}
	if (flag == 0x00)
	{
		return 0;
	}
	else if (flag & 0x80)
	{
		return 1;
	}
	else
	{
		return 2;
	}
}
// index: index of otp group. (1, 2, 3)
// return: 0, group index is empty
// 1, group index has invalid data
// 2, group index has valid data
int ov5694_check_otp_lenc(int index)
{
	int flag, i, bank;
	int address;
	// select bank: 4, 8, 12
	bank = 0xc0 | (index * 4);
	OV5694MIPI_write_cmos_sensor(0x3d84, bank);
	// read otp into buffer
	OV5694MIPI_write_cmos_sensor(0x3d81, 0x01);
	msleep(5);
	// read flag
	address = 0x3d00;
	flag = OV5694MIPI_read_cmos_sensor(address);
	flag = flag & 0xc0;
	// clear otp buffer
	for (i=0;i<16;i++)
	{
		OV5694MIPI_write_cmos_sensor(0x3d00 + i, 0x00);
	}
	if (flag == 0x00)
	{
		return 0;
	}
	else if (flag & 0x80)
	{
		return 1;
	}
	else
	{
		return 2;
	}
}
// index: index of otp group. (1, 2, 3)
// code: 0 for start code, 1 for stop code
// return: 0, group index is empty
// 1, group index has invalid data
// 2, group index has valid data
int ov5694_check_otp_VCM(int index, int code)
{
	int flag, i, bank;
	int address;
	// select bank: 16
	bank = 0xc0 + 16;
	OV5694MIPI_write_cmos_sensor(0x3d84, bank);
	// read otp into buffer
	OV5694MIPI_write_cmos_sensor(0x3d81, 0x01);
	msleep(5);
	// read flag
	address = 0x3d00 + (index-1)*4 + code*2;
	flag = OV5694MIPI_read_cmos_sensor(address);
	flag = flag & 0xc0;
	// clear otp buffer
	for (i=0;i<16;i++)
	{
		OV5694MIPI_write_cmos_sensor(0x3d00 + i, 0x00);
	}
	if (flag == 0x00)
	{
		return 0;
	}
	else if (flag & 0x80)
	{
		return 1;
	}
	else
	{
		return 2;
	}
}
// index: index of otp group. (1, 2, 3)
// otp_ptr: pointer of otp_struct
// return: 0,
int ov5694_read_otp_wb(int index, struct ov5694_otp_struct *otp_ptr)
{
	int i, bank;
	int address;
	int temp;
	// select bank index
	bank = 0xc0 | index;
	OV5694MIPI_write_cmos_sensor(0x3d84, bank);
	// read otp into buffer
	OV5694MIPI_write_cmos_sensor(0x3d81, 0x01);
	msleep(5);
	address = 0x3d00;
	(*otp_ptr).module_integrator_id = OV5694MIPI_read_cmos_sensor(address + 1);
	(*otp_ptr).lens_id = OV5694MIPI_read_cmos_sensor(address + 2);
	(*otp_ptr).production_year = OV5694MIPI_read_cmos_sensor(address + 3);
	(*otp_ptr).production_month = OV5694MIPI_read_cmos_sensor(address + 4);
	(*otp_ptr).production_day = OV5694MIPI_read_cmos_sensor(address + 5);
	printk("honghaishen_ov5694 year is %d, month is %d, day is %d",(*otp_ptr).production_year,(*otp_ptr).production_month,(*otp_ptr).production_day);
	temp = OV5694MIPI_read_cmos_sensor(address + 10);
	(*otp_ptr).rg_ratio = (OV5694MIPI_read_cmos_sensor(address + 6)<<2) + ((temp>>6) & 0x03);
	(*otp_ptr).bg_ratio = (OV5694MIPI_read_cmos_sensor(address + 7)<<2) + ((temp>>4) & 0x03);
	(*otp_ptr).light_rg = (OV5694MIPI_read_cmos_sensor(address + 8) <<2) + ((temp>>2) &
	0x03);
	(*otp_ptr).light_bg = (OV5694MIPI_read_cmos_sensor(address + 9)<<2) + (temp & 0x03);
	(*otp_ptr).user_data[0] = OV5694MIPI_read_cmos_sensor(address + 11);
	(*otp_ptr).user_data[1] = OV5694MIPI_read_cmos_sensor(address + 12);
	(*otp_ptr).user_data[2] = OV5694MIPI_read_cmos_sensor(address + 13);
	(*otp_ptr).user_data[3] = OV5694MIPI_read_cmos_sensor(address + 14);
	(*otp_ptr).user_data[4] = OV5694MIPI_read_cmos_sensor(address + 15);
	// clear otp buffer
	for (i=0;i<16;i++)
	{
		OV5694MIPI_write_cmos_sensor(0x3d00 + i, 0x00);
	}
	return 0;
}
// index: index of otp group. (1, 2, 3)
// otp_ptr: pointer of otp_struct
// return: 0,
int ov5694_read_otp_lenc(int index, struct ov5694_otp_struct *otp_ptr)
{
	int bank, i;
	int address;
	// select bank: 4, 8, 12
	bank = 0xc0 | (index * 4);
	OV5694MIPI_write_cmos_sensor(0x3d84, bank);
	// read otp into buffer
	OV5694MIPI_write_cmos_sensor(0x3d81, 0x01);
	msleep(5);
	address = 0x3d01;
	for(i=0;i<15;i++)
	{
		(* otp_ptr).lenc[i]=OV5694MIPI_read_cmos_sensor(address);
		address++;
	}
	// clear otp buffer
	for (i=0;i<16;i++)
	{
		OV5694MIPI_write_cmos_sensor(0x3d00 + i, 0x00);
	}
	// select 2nd bank
	bank++;
	OV5694MIPI_write_cmos_sensor(0x3d84, bank);
	// read otp
	OV5694MIPI_write_cmos_sensor(0x3d81, 0x01);
	msleep(5);
	address = 0x3d00;
	for(i=15;i<31;i++)
	{
		(* otp_ptr).lenc[i]=OV5694MIPI_read_cmos_sensor(address);
		address++;
	}
	// clear otp buffer
	for (i=0;i<16;i++)
	{
		OV5694MIPI_write_cmos_sensor(0x3d00 + i, 0x00);
	}
	// select 3rd bank
	bank++;
	OV5694MIPI_write_cmos_sensor(0x3d84, bank);
	// read otp
	OV5694MIPI_write_cmos_sensor(0x3d81, 0x01);
	msleep(5);
	address = 0x3d00;
	for(i=31;i<47;i++)
	{
		(* otp_ptr).lenc[i]=OV5694MIPI_read_cmos_sensor(address);
		address++;
	}
	// clear otp buffer
	for (i=0;i<16;i++)
	{
		OV5694MIPI_write_cmos_sensor(0x3d00 + i, 0x00);
	}
	// select 4th bank
	bank++;
	OV5694MIPI_write_cmos_sensor(0x3d84, bank);
	// read otp
	OV5694MIPI_write_cmos_sensor(0x3d81, 0x01);
	msleep(5);
	address = 0x3d00;
	for(i=47;i<62;i++)
	{
		(* otp_ptr).lenc[i]=OV5694MIPI_read_cmos_sensor(address);
		address++;
	}
	// clear otp buffer
	for (i=0;i<16;i++)
	{
		OV5694MIPI_write_cmos_sensor(0x3d00 + i, 0x00);
	}
	return 0;
}
// index: index of otp group. (1, 2, 3)
// code: 0 start code, 1 stop code
// return: 0
int ov5694_read_otp_VCM(int index, int code, struct ov5694_otp_struct * otp_ptr)
{
	int vcm, i, bank;
	int address;
	// select bank: 16
	bank = 0xc0 + 16;
	OV5694MIPI_write_cmos_sensor(0x3d84, bank);
	// read otp into buffer
	OV5694MIPI_write_cmos_sensor(0x3d81, 0x01);
	msleep(1);
	// read flag
	address = 0x3d00 + (index-1)*4 + code*2;
	vcm = OV5694MIPI_read_cmos_sensor(address);
	vcm = (vcm & 0x03) + (OV5694MIPI_read_cmos_sensor(address+1)<<2);
	if(code==1)
	{
		(* otp_ptr).VCM_end = vcm;
	}
	else
	{
		(* otp_ptr).VCM_start = vcm;
	}
	// clear otp buffer
	for (i=0;i<16;i++)
	{
		OV5694MIPI_write_cmos_sensor(0x3d00 + i, 0x00);
	}
	return 0;
}
// R_gain, sensor red gain of AWB, 0x400 =1
// G_gain, sensor green gain of AWB, 0x400 =1
// B_gain, sensor blue gain of AWB, 0x400 =1
// return 0;
int ov5694_update_awb_gain(int R_gain, int G_gain, int B_gain)
{
	if (R_gain>0x400)
	{
		OV5694MIPI_write_cmos_sensor(0x3400, R_gain>>8);
		OV5694MIPI_write_cmos_sensor(0x3401, R_gain & 0x00ff);
	}
	if (G_gain>0x400)
	{
		OV5694MIPI_write_cmos_sensor(0x3402, G_gain>>8);
		OV5694MIPI_write_cmos_sensor(0x3403, G_gain & 0x00ff);
	}
	if (B_gain>0x400)
	{
		OV5694MIPI_write_cmos_sensor(0x3404, B_gain>>8);
		OV5694MIPI_write_cmos_sensor(0x3405, B_gain & 0x00ff);
	}
	return 0;
}
// otp_ptr: pointer of otp_struct
int ov5694_update_lenc(struct ov5694_otp_struct * otp_ptr)
{
	int i, temp;
	temp = OV5694MIPI_read_cmos_sensor(0x5000);
	temp = 0x80 | temp;
	for(i=0;i<62;i++)
	{
		OV5694MIPI_write_cmos_sensor(0x5800 + i, (*otp_ptr).lenc[i]);
	}
	OV5694MIPI_write_cmos_sensor(0x5000, temp);
	return 0;
}
// call this function after OV5690/OV5694 initialization
// return value: 0 update success
// 1, no OTP
int ov5694_read_otp_date()
{
	struct ov5694_otp_struct current_otp;
	int i;
	int otp_index;
	int temp;
	int R_gain, G_gain, B_gain, G_gain_R, G_gain_B;
	int rg,bg;
	// R/G and B/G of current camera module is read out from sensor OTP
	// check first OTP with valid data
	OV5694MIPI_write_cmos_sensor(0x0100, 0x01);
	msleep(20);
	for(i=1;i<=3;i++)
	{
		temp = ov5694_check_otp_wb(i);
		if (temp == 2)
		{
			otp_index = i;
			printk("honghaishen_ov5694 read date index is %d \n",otp_index);
			break;
		}
	}
	if (i>3)
	{
	// no valid wb OTP data
	printk("honghaishen_ov5694 camera read data fail \n");
		return 1;
	}
	ov5694_read_otp_wb(otp_index, &current_otp);
	OV5694MIPI_write_cmos_sensor(0x0100, 0x00);
	printk("honghaishen_ov5694 read hou 1 year is %d, month is %d, day is %d",current_otp.production_year,current_otp.production_month,current_otp.production_day);
	ov5694_otp_data_year=current_otp.production_year;
	ov5694_otp_data_month=current_otp.production_month;
	ov5694_otp_data_day=current_otp.production_day;
	printk("honghaishen_ov5694 read hou 2 year is %d, month is %d, day is %d",ov5694_otp_data_year,ov5694_otp_data_month,ov5694_otp_data_day);
	return 0;
	

}
int ov5694_update_otp_wb()
{
	struct ov5694_otp_struct current_otp;
	int i;
	int otp_index;
	int temp;
	int R_gain, G_gain, B_gain, G_gain_R, G_gain_B;
	int rg,bg;
	int rg_type=RG_Ratio_Typical;
	int bg_type=BG_Ratio_Typical;
	
	// R/G and B/G of current camera module is read out from sensor OTP
	// check first OTP with valid data
	for(i=1;i<=3;i++)
	{
		temp = ov5694_check_otp_wb(i);
		if (temp == 2)
		{
			otp_index = i;
			printk("honghaishen_ov5694 wb index is %d \n",otp_index);
			break;
		}
	}
	if (i>3)
	{
	// no valid wb OTP data
		return 1;
	}
	ov5694_read_otp_wb(otp_index, &current_otp);
	if((current_otp.rg_ratio==0)||(current_otp.bg_ratio==0))
	{
	    rg = RG_Ratio_Typical;
	    bg = RG_Ratio_Typical;
	}
	else
	{
	    rg = current_otp.rg_ratio;
	    bg = current_otp.bg_ratio;
	}

	printk("honghaishen_ov5694 wb rg is %d , bg is %d ",rg,bg);
	printk("honghaishen_ov5694 wb rg_type is %d , bg_type is %d ",rg_type,bg_type);
	//calculate G gain
	//0x400 = 1x gain
	if(bg < BG_Ratio_Typical)
	{
		if (rg< RG_Ratio_Typical)
		{
			// current_otp.bg_ratio < BG_Ratio_typical &&
			// current_otp.rg_ratio < RG_Ratio_typical
			G_gain = 0x400;
			B_gain = 0x400 * BG_Ratio_Typical / bg;
			R_gain = 0x400 * RG_Ratio_Typical / rg;
		}
		else
		{
			// current_otp.bg_ratio < BG_Ratio_typical &&
			// current_otp.rg_ratio >= RG_Ratio_typical
			R_gain = 0x400;
			G_gain = 0x400 * rg / RG_Ratio_Typical;
			B_gain = G_gain * BG_Ratio_Typical /bg;
		}
	}
	else
	{
		if (rg < RG_Ratio_Typical)
		{
			// current_otp.bg_ratio >= BG_Ratio_typical &&
			// current_otp.rg_ratio < RG_Ratio_typical
			B_gain = 0x400;
			G_gain = 0x400 * bg / BG_Ratio_Typical;
			R_gain = G_gain * RG_Ratio_Typical / rg;
		}
		else
		{
			// current_otp.bg_ratio >= BG_Ratio_typical &&
			// current_otp.rg_ratio >= RG_Ratio_typical
			G_gain_B = 0x400 * bg / BG_Ratio_Typical;
			G_gain_R = 0x400 * rg / RG_Ratio_Typical;
			if(G_gain_B > G_gain_R )
			{
				B_gain = 0x400;
				G_gain = G_gain_B;
				R_gain = G_gain * RG_Ratio_Typical /rg;
			}
			else
			{
				R_gain = 0x400;
				G_gain = G_gain_R;
				B_gain = G_gain * BG_Ratio_Typical / bg;
			}
		}
	}
	ov5694_update_awb_gain(R_gain, G_gain, B_gain);
	return 0;
}
// call this function after OV5690/OV5694 initialization
// return value: 0 update success
// 1, no OTP
int ov5694_update_otp_lenc()
{
	struct ov5694_otp_struct current_otp;
	int i;
	int otp_index;
	int temp;
	// check first lens correction OTP with valid data
	for(i=1;i<=3;i++)
	{
		temp = ov5694_check_otp_lenc(i);
		if (temp == 2)
		{
			otp_index = i;
			printk("honghaishen_ov5694 lenc index is %d \n",otp_index);
			break;
		}
	}
	if (i>3)
	{
		// no valid LSC OTP data
		printk("honghaishen_ov5694 lenc index is no \n");
		return 1;
	}
	ov5694_read_otp_lenc(otp_index, &current_otp);
	ov5694_update_lenc(&current_otp);
	// success
	return 0;
}
// call this function after OV5690/OV5694 initialization
// return value: 1 use CP data from REG3D0A
// 2 use Module data from REG3D0A
// 0 data ErRoR
int ov5694_update_blc_ratio()
{
	int K;
	int temp;
	OV5694MIPI_write_cmos_sensor(0x3d84, 0xdf);
	OV5694MIPI_write_cmos_sensor(0x3d81, 0x01);
	msleep(5);
	K = OV5694MIPI_read_cmos_sensor(0x3d0b);
	if (K != 0)
	{
		if (K >= 0x15 && K <= 0x40)
		{
			// auto load mode
			temp = OV5694MIPI_read_cmos_sensor(0x4008);
			temp &= 0xfb;
			OV5694MIPI_write_cmos_sensor(0x4008, temp);
			temp = OV5694MIPI_read_cmos_sensor(0x4000);
			temp &= 0xf7;
			OV5694MIPI_write_cmos_sensor(0x4000, temp);
			return 2;
		}
	}
	K = OV5694MIPI_read_cmos_sensor(0x3d0a);
	if (K >= 0x10 && K <= 0x40)
	{
		// manual load mode
		OV5694MIPI_write_cmos_sensor(0x4006, K);
		temp = OV5694MIPI_read_cmos_sensor(0x4008);
		temp &= 0xfb;
		OV5694MIPI_write_cmos_sensor(0x4008, temp);
		temp = OV5694MIPI_read_cmos_sensor(0x4000);
		temp |= 0x08;
		OV5694MIPI_write_cmos_sensor(0x4000, temp);
		return 1;
	}
	else
	{
		// set to default
		OV5694MIPI_write_cmos_sensor(0x4006, 0x20);
		temp = OV5694MIPI_read_cmos_sensor(0x4008);
		temp &= 0xfb;
		OV5694MIPI_write_cmos_sensor(0x4008, temp);
		temp = OV5694MIPI_read_cmos_sensor(0x4000);
		temp |= 0x08;
		OV5694MIPI_write_cmos_sensor(0x4000, temp);
		return 0;
	}
}


#endif
static void OV5694MIPI_Write_Shutter(kal_uint16 iShutter)
{
#if 0
    kal_uint16 extra_line = 0;
#endif
    
    kal_uint16 frame_length = 0;

    #ifdef OV5694MIPI_DRIVER_TRACE
        SENSORDB("iShutter =  %d", iShutter);
    #endif
    
    /* 0x3500, 0x3501, 0x3502 will increase VBLANK to get exposure larger than frame exposure */
    /* AE doesn't update sensor gain at capture mode, thus extra exposure lines must be updated here. */
    if (!iShutter) iShutter = 1;

    if(OV5694MIPIAutoFlickerMode){
        if(OV5694MIPI_sensor.video_mode == KAL_FALSE){
            if(mCurrentScenarioId == MSDK_SCENARIO_ID_CAMERA_ZSD)
                OV5694MIPISetMaxFrameRate(296);
            else
                OV5694MIPISetMaxFrameRate(296);
        } 
        else
        {
            if(OV5694MIPI_sensor.FixedFps == 30)
                OV5694MIPISetMaxFrameRate(300);
            else if (OV5694MIPI_sensor.FixedFps == 15)
                OV5694MIPISetMaxFrameRate(150);
        }
    }
    else
    {
        if(OV5694MIPI_sensor.video_mode == KAL_FALSE){
            if(mCurrentScenarioId == MSDK_SCENARIO_ID_CAMERA_ZSD)
                OV5694MIPISetMaxFrameRate(300);
            else
                OV5694MIPISetMaxFrameRate(300);
        } 
        else
        {
            if(OV5694MIPI_sensor.FixedFps == 30)
                OV5694MIPISetMaxFrameRate(300);
            else if (OV5694MIPI_sensor.FixedFps == 15)
                OV5694MIPISetMaxFrameRate(150);
        }    
    }

    // OV Recommend Solution
    // if shutter bigger than frame_length, should extend frame length first
#if 0

	if(iShutter > (OV5694MIPI_sensor.frame_length - 4))
		extra_line = iShutter - (OV5694MIPI_sensor.frame_length - 4);
	else
	    extra_line = 0;

	// Update Extra shutter
	OV5694MIPI_write_cmos_sensor(0x350c, (extra_line >> 8) & 0xFF);	
	OV5694MIPI_write_cmos_sensor(0x350d, (extra_line) & 0xFF); 
	
#endif

#if 1  

    if(iShutter > OV5694MIPI_sensor.frame_length - 4)
        frame_length = iShutter + 4;
    else
        frame_length = OV5694MIPI_sensor.frame_length;

    // Extend frame length
    OV5694MIPI_write_cmos_sensor(0x380e, frame_length >> 8);
    OV5694MIPI_write_cmos_sensor(0x380f, frame_length & 0xFF);
    
#endif

    // Update Shutter
    OV5694MIPI_write_cmos_sensor(0x3502, (iShutter << 4) & 0xFF);
    OV5694MIPI_write_cmos_sensor(0x3501, (iShutter >> 4) & 0xFF);     
    OV5694MIPI_write_cmos_sensor(0x3500, (iShutter >> 12) & 0x0F);  
}   /*  OV5694MIPI_Write_Shutter  */

static void OV5694MIPI_Set_Dummy(const kal_uint16 iDummyPixels, const kal_uint16 iDummyLines)
{
    kal_uint16 line_length, frame_length;

    #ifdef OV5694MIPI_DRIVER_TRACE
        SENSORDB("iDummyPixels = %d, iDummyLines = %d ", iDummyPixels, iDummyLines);
    #endif

    if (OV5694MIPI_SENSOR_MODE_PREVIEW == OV5694MIPI_sensor.sensor_mode)
    {
        line_length = OV5694MIPI_PV_PERIOD_PIXEL_NUMS + iDummyPixels;
        frame_length = OV5694MIPI_PV_PERIOD_LINE_NUMS + iDummyLines;
    }
    else if(OV5694MIPI_SENSOR_MODE_VIDEO == OV5694MIPI_sensor.sensor_mode)
    {
        line_length = OV5694MIPI_VIDEO_PERIOD_PIXEL_NUMS + iDummyPixels;
        frame_length = OV5694MIPI_VIDEO_PERIOD_LINE_NUMS + iDummyLines;
    }    
    else
    {
        line_length = OV5694MIPI_FULL_PERIOD_PIXEL_NUMS + iDummyPixels;
        frame_length = OV5694MIPI_FULL_PERIOD_LINE_NUMS + iDummyLines;
    }

    SENSORDB("line_length = %d, frame_length = %d ", line_length, frame_length);

    OV5694MIPI_sensor.dummy_pixel = iDummyPixels;
    OV5694MIPI_sensor.dummy_line = iDummyLines;
    OV5694MIPI_sensor.line_length = line_length;
    OV5694MIPI_sensor.frame_length = frame_length;

    OV5694MIPI_write_cmos_sensor(0x380e, frame_length >> 8);
    OV5694MIPI_write_cmos_sensor(0x380f, frame_length & 0xFF);     
    OV5694MIPI_write_cmos_sensor(0x380c, line_length >> 8);
    OV5694MIPI_write_cmos_sensor(0x380d, line_length & 0xFF);
  
}   /*  OV5694MIPI_Set_Dummy  */


void OV5694MIPISetMaxFrameRate(UINT16 u2FrameRate)
{
    kal_int16 dummy_line;
    kal_uint16 frame_length = OV5694MIPI_sensor.frame_length;
    unsigned long flags;

    #ifdef OV5694MIPI_DRIVER_TRACE
        SENSORDB("u2FrameRate = %d ", u2FrameRate);
    #endif

    frame_length= (10 * OV5694MIPI_sensor.pclk) / u2FrameRate / OV5694MIPI_sensor.line_length;

    spin_lock_irqsave(&ov5694mipi_drv_lock, flags);
    OV5694MIPI_sensor.frame_length = frame_length;
    spin_unlock_irqrestore(&ov5694mipi_drv_lock, flags);

    if (OV5694MIPI_SENSOR_MODE_PREVIEW == OV5694MIPI_sensor.sensor_mode)
    {
        dummy_line = frame_length - OV5694MIPI_PV_PERIOD_LINE_NUMS;
    }
    else if(OV5694MIPI_SENSOR_MODE_VIDEO == OV5694MIPI_sensor.sensor_mode)
    {
        dummy_line = frame_length - OV5694MIPI_VIDEO_PERIOD_LINE_NUMS;
    }    
    else
    {
        dummy_line = frame_length - OV5694MIPI_FULL_PERIOD_LINE_NUMS;
    }

#if 0
    if(mCurrentScenarioId == MSDK_SCENARIO_ID_CAMERA_ZSD)
        dummy_line = frame_length - OV5694MIPI_FULL_PERIOD_LINE_NUMS;
    else
        dummy_line = frame_length - OV5694MIPI_PV_PERIOD_LINE_NUMS;
#endif

    if (dummy_line < 0) dummy_line = 0;

    OV5694MIPI_Set_Dummy(OV5694MIPI_sensor.dummy_pixel, dummy_line); /* modify dummy_pixel must gen AE table again */   
}   /*  OV5694MIPISetMaxFrameRate  */


/*************************************************************************
* FUNCTION
*   OV5694MIPI_SetShutter
*
* DESCRIPTION
*   This function set e-shutter of OV5694MIPI to change exposure time.
*
* PARAMETERS
*   iShutter : exposured lines
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void set_OV5694MIPI_shutter(kal_uint16 iShutter)
{
    unsigned long flags;
    
    spin_lock_irqsave(&ov5694mipi_drv_lock, flags);
    OV5694MIPI_sensor.shutter = iShutter;
    spin_unlock_irqrestore(&ov5694mipi_drv_lock, flags);  

    OV5694MIPI_Write_Shutter(iShutter);
    
}   /*  Set_OV5694MIPI_Shutter */

#if 0
static kal_uint16 OV5694MIPI_Reg2Gain(const kal_uint8 iReg)
{
    kal_uint16 iGain ;
    /* Range: 1x to 32x */
    iGain = (iReg >> 4) * BASEGAIN + (iReg & 0xF) * BASEGAIN / 16; 
    return iGain ;
}
#endif

 kal_uint16 OV5694MIPI_Gain2Reg(const kal_uint16 iGain)
{
    kal_uint16 iReg = 0x0000;
    
    iReg = ((iGain / BASEGAIN) << 4) + ((iGain % BASEGAIN) * 16 / BASEGAIN);
    iReg = iReg & 0xFFFF;
    return (kal_uint16)iReg;
}

/*************************************************************************
* FUNCTION
*   OV5694MIPI_SetGain
*
* DESCRIPTION
*   This function is to set global gain to sensor.
*
* PARAMETERS
*   iGain : sensor global gain(base: 0x40)
*
* RETURNS
*   the actually gain set to sensor.
*
* GLOBALS AFFECTED
*
*************************************************************************/
kal_uint16 OV5694MIPI_SetGain(kal_uint16 iGain)
{
    kal_uint16 iRegGain;

    OV5694MIPI_sensor.gain = iGain;

    /* 0x350A[0:1], 0x350B[0:7] AGC real gain */
    /* [0:3] = N meams N /16 X  */
    /* [4:9] = M meams M X       */
    /* Total gain = M + N /16 X   */

    //
    if(iGain < BASEGAIN || iGain > 32 * BASEGAIN){
        SENSORDB("Error gain setting");

        if(iGain < BASEGAIN) iGain = BASEGAIN;
        if(iGain > 32 * BASEGAIN) iGain = 32 * BASEGAIN;        
    }
 
    iRegGain = OV5694MIPI_Gain2Reg(iGain);

    #ifdef OV5694MIPI_DRIVER_TRACE
        SENSORDB("iGain = %d , iRegGain = 0x%x ", iGain, iRegGain);
    #endif

    OV5694MIPI_write_cmos_sensor(0x350a, iRegGain >> 8);
    OV5694MIPI_write_cmos_sensor(0x350b, iRegGain & 0xFF);    
    
    return iGain;
}   /*  OV5694MIPI_SetGain  */


void OV5694MIPI_Set_Mirror_Flip(kal_uint8 image_mirror)
{
    SENSORDB("image_mirror = %d", image_mirror);

    /********************************************************
       *
       *   0x3820[2] ISP Vertical flip
       *   0x3820[1] Sensor Vertical flip
       *
       *   0x3821[2] ISP Horizontal mirror
       *   0x3821[1] Sensor Horizontal mirror
       *
       *   ISP and Sensor flip or mirror register bit should be the same!!
       *
       ********************************************************/
    
	switch (image_mirror)
	{
		case IMAGE_NORMAL:
		    OV5694MIPI_write_cmos_sensor(0x3820,((OV5694MIPI_read_cmos_sensor(0x3820) & 0xF9) | 0x00));
		    OV5694MIPI_write_cmos_sensor(0x3821,((OV5694MIPI_read_cmos_sensor(0x3821) & 0xF9) | 0x06));
		    break;
		case IMAGE_H_MIRROR:
		    OV5694MIPI_write_cmos_sensor(0x3820,((OV5694MIPI_read_cmos_sensor(0x3820) & 0xF9) | 0x00));
		    OV5694MIPI_write_cmos_sensor(0x3821,((OV5694MIPI_read_cmos_sensor(0x3821) & 0xF9) | 0x00));
		    break;
		case IMAGE_V_MIRROR:
		    OV5694MIPI_write_cmos_sensor(0x3820,((OV5694MIPI_read_cmos_sensor(0x3820) & 0xF9) | 0x06));
		    OV5694MIPI_write_cmos_sensor(0x3821,((OV5694MIPI_read_cmos_sensor(0x3821) & 0xF9) | 0x06));		
		    break;
		case IMAGE_HV_MIRROR:
		    OV5694MIPI_write_cmos_sensor(0x3820,((OV5694MIPI_read_cmos_sensor(0x3820) & 0xF9) | 0x06));
		    OV5694MIPI_write_cmos_sensor(0x3821,((OV5694MIPI_read_cmos_sensor(0x3821) & 0xF9) | 0x00));
		    break;
		default:
		    SENSORDB("Error image_mirror setting");
	}

}

/*************************************************************************
* FUNCTION
*   OV5694MIPI_NightMode
*
* DESCRIPTION
*   This function night mode of OV5694MIPI.
*
* PARAMETERS
*   bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void OV5694MIPI_night_mode(kal_bool enable)
{
/*No Need to implement this function*/ 
}   /*  OV5694MIPI_night_mode  */


/* write camera_para to sensor register */
static void OV5694MIPI_camera_para_to_sensor(void)
{
    kal_uint32 i;

    SENSORDB("OV5694MIPI_camera_para_to_sensor\n");

    for (i = 0; 0xFFFFFFFF != OV5694MIPI_sensor.eng.reg[i].Addr; i++)
    {
        OV5694MIPI_write_cmos_sensor(OV5694MIPI_sensor.eng.reg[i].Addr, OV5694MIPI_sensor.eng.reg[i].Para);
    }
    for (i = OV5694MIPI_FACTORY_START_ADDR; 0xFFFFFFFF != OV5694MIPI_sensor.eng.reg[i].Addr; i++)
    {
        OV5694MIPI_write_cmos_sensor(OV5694MIPI_sensor.eng.reg[i].Addr, OV5694MIPI_sensor.eng.reg[i].Para);
    }
    OV5694MIPI_SetGain(OV5694MIPI_sensor.gain); /* update gain */
}

/* update camera_para from sensor register */
static void OV5694MIPI_sensor_to_camera_para(void)
{
    kal_uint32 i,temp_data;

    SENSORDB("OV5694MIPI_sensor_to_camera_para\n");

    for (i = 0; 0xFFFFFFFF != OV5694MIPI_sensor.eng.reg[i].Addr; i++)
    {
        temp_data =OV5694MIPI_read_cmos_sensor(OV5694MIPI_sensor.eng.reg[i].Addr);
     
        spin_lock(&ov5694mipi_drv_lock);
        OV5694MIPI_sensor.eng.reg[i].Para = temp_data;
        spin_unlock(&ov5694mipi_drv_lock);
    }
    for (i = OV5694MIPI_FACTORY_START_ADDR; 0xFFFFFFFF != OV5694MIPI_sensor.eng.reg[i].Addr; i++)
    {
        temp_data =OV5694MIPI_read_cmos_sensor(OV5694MIPI_sensor.eng.reg[i].Addr);
    
        spin_lock(&ov5694mipi_drv_lock);
        OV5694MIPI_sensor.eng.reg[i].Para = temp_data;
        spin_unlock(&ov5694mipi_drv_lock);
    }
}

/* ------------------------ Engineer mode ------------------------ */
inline static void OV5694MIPI_get_sensor_group_count(kal_int32 *sensor_count_ptr)
{

    SENSORDB("OV5694MIPI_get_sensor_group_count\n");

    *sensor_count_ptr = OV5694MIPI_GROUP_TOTAL_NUMS;
}

inline static void OV5694MIPI_get_sensor_group_info(MSDK_SENSOR_GROUP_INFO_STRUCT *para)
{

    SENSORDB("OV5694MIPI_get_sensor_group_info\n");

    switch (para->GroupIdx)
    {
    case OV5694MIPI_PRE_GAIN:
        sprintf(para->GroupNamePtr, "CCT");
        para->ItemCount = 5;
        break;
    case OV5694MIPI_CMMCLK_CURRENT:
        sprintf(para->GroupNamePtr, "CMMCLK Current");
        para->ItemCount = 1;
        break;
    case OV5694MIPI_FRAME_RATE_LIMITATION:
        sprintf(para->GroupNamePtr, "Frame Rate Limitation");
        para->ItemCount = 2;
        break;
    case OV5694MIPI_REGISTER_EDITOR:
        sprintf(para->GroupNamePtr, "Register Editor");
        para->ItemCount = 2;
        break;
    default:
        ASSERT(0);
  }
}

inline static void OV5694MIPI_get_sensor_item_info(MSDK_SENSOR_ITEM_INFO_STRUCT *para)
{

    const static kal_char *cct_item_name[] = {"SENSOR_BASEGAIN", "Pregain-R", "Pregain-Gr", "Pregain-Gb", "Pregain-B"};
    const static kal_char *editer_item_name[] = {"REG addr", "REG value"};
  
    SENSORDB("OV5694MIPI_get_sensor_item_info");

    switch (para->GroupIdx)
    {
    case OV5694MIPI_PRE_GAIN:
        switch (para->ItemIdx)
        {
        case OV5694MIPI_SENSOR_BASEGAIN:
        case OV5694MIPI_PRE_GAIN_R_INDEX:
        case OV5694MIPI_PRE_GAIN_Gr_INDEX:
        case OV5694MIPI_PRE_GAIN_Gb_INDEX:
        case OV5694MIPI_PRE_GAIN_B_INDEX:
            break;
        default:
            ASSERT(0);
        }
        sprintf(para->ItemNamePtr, cct_item_name[para->ItemIdx - OV5694MIPI_SENSOR_BASEGAIN]);
        para->ItemValue = OV5694MIPI_sensor.eng.cct[para->ItemIdx].Para * 1000 / BASEGAIN;
        para->IsTrueFalse = para->IsReadOnly = para->IsNeedRestart = KAL_FALSE;
        para->Min = OV5694MIPI_MIN_ANALOG_GAIN * 1000;
        para->Max = OV5694MIPI_MAX_ANALOG_GAIN * 1000;
        break;
    case OV5694MIPI_CMMCLK_CURRENT:
        switch (para->ItemIdx)
        {
        case 0:
            sprintf(para->ItemNamePtr, "Drv Cur[2,4,6,8]mA");
            switch (OV5694MIPI_sensor.eng.reg[OV5694MIPI_CMMCLK_CURRENT_INDEX].Para)
            {
            case ISP_DRIVING_2MA:
                para->ItemValue = 2;
                break;
            case ISP_DRIVING_4MA:
                para->ItemValue = 4;
                break;
            case ISP_DRIVING_6MA:
                para->ItemValue = 6;
                break;
            case ISP_DRIVING_8MA:
                para->ItemValue = 8;
                break;
            default:
                ASSERT(0);
            }
            para->IsTrueFalse = para->IsReadOnly = KAL_FALSE;
            para->IsNeedRestart = KAL_TRUE;
            para->Min = 2;
            para->Max = 8;
            break;
        default:
            ASSERT(0);
        }
        break;
    case OV5694MIPI_FRAME_RATE_LIMITATION:
        switch (para->ItemIdx)
        {
        case 0:
            sprintf(para->ItemNamePtr, "Max Exposure Lines");
            para->ItemValue = 5998;
            break;
        case 1:
            sprintf(para->ItemNamePtr, "Min Frame Rate");
            para->ItemValue = 5;
            break;
        default:
            ASSERT(0);
        }
        para->IsTrueFalse = para->IsNeedRestart = KAL_FALSE;
        para->IsReadOnly = KAL_TRUE;
        para->Min = para->Max = 0;
        break;
    case OV5694MIPI_REGISTER_EDITOR:
        switch (para->ItemIdx)
        {
        case 0:
        case 1:
            sprintf(para->ItemNamePtr, editer_item_name[para->ItemIdx]);
            para->ItemValue = 0;
            para->IsTrueFalse = para->IsReadOnly = para->IsNeedRestart = KAL_FALSE;
            para->Min = 0;
            para->Max = (para->ItemIdx == 0 ? 0xFFFF : 0xFF);
            break;
        default:
            ASSERT(0);
        }
        break;
    default:
        ASSERT(0);
  }
}

inline static kal_bool OV5694MIPI_set_sensor_item_info(MSDK_SENSOR_ITEM_INFO_STRUCT *para)
{
    kal_uint16 temp_para;

    SENSORDB("OV5694MIPI_set_sensor_item_info\n");

    switch (para->GroupIdx)
    {
    case OV5694MIPI_PRE_GAIN:
        switch (para->ItemIdx)
        {
        case OV5694MIPI_SENSOR_BASEGAIN:
        case OV5694MIPI_PRE_GAIN_R_INDEX:
        case OV5694MIPI_PRE_GAIN_Gr_INDEX:
        case OV5694MIPI_PRE_GAIN_Gb_INDEX:
        case OV5694MIPI_PRE_GAIN_B_INDEX:
            spin_lock(&ov5694mipi_drv_lock);
            OV5694MIPI_sensor.eng.cct[para->ItemIdx].Para = para->ItemValue * BASEGAIN / 1000;
            spin_unlock(&ov5694mipi_drv_lock);
            OV5694MIPI_SetGain(OV5694MIPI_sensor.gain); /* update gain */
            break;
        default:
            ASSERT(0);
        }
        break;
    case OV5694MIPI_CMMCLK_CURRENT:
        switch (para->ItemIdx)
        {
        case 0:
            switch (para->ItemValue)
            {
            case 2:
                temp_para = ISP_DRIVING_2MA;
                break;
            case 3:
            case 4:
                temp_para = ISP_DRIVING_4MA;
                break;
            case 5:
            case 6:
                temp_para = ISP_DRIVING_6MA;
                break;
            default:
                temp_para = ISP_DRIVING_8MA;
                break;
            }
            spin_lock(&ov5694mipi_drv_lock);
            //OV5694MIPI_set_isp_driving_current(temp_para);
            OV5694MIPI_sensor.eng.reg[OV5694MIPI_CMMCLK_CURRENT_INDEX].Para = temp_para;
            spin_unlock(&ov5694mipi_drv_lock);
            break;
        default:
            ASSERT(0);
        }
        break;
    case OV5694MIPI_FRAME_RATE_LIMITATION:
        ASSERT(0);
        break;
    case OV5694MIPI_REGISTER_EDITOR:
        switch (para->ItemIdx)
        {
        static kal_uint32 fac_sensor_reg;
        case 0:
            if (para->ItemValue < 0 || para->ItemValue > 0xFFFF) return KAL_FALSE;
            fac_sensor_reg = para->ItemValue;
            break;
        case 1:
            if (para->ItemValue < 0 || para->ItemValue > 0xFF) return KAL_FALSE;
            OV5694MIPI_write_cmos_sensor(fac_sensor_reg, para->ItemValue);
            break;
        default:
            ASSERT(0);
        }
        break;
    default:
        ASSERT(0);
    }

    return KAL_TRUE;
}

static void OV5694MIPI_Sensor_Init(void)
{
    SENSORDB("Enter!");

   /*****************************************************************************
    0x3098[0:1] pll3_prediv
    pll3_prediv_map[] = {2, 3, 4, 6} 
    
    0x3099[0:4] pll3_multiplier
    pll3_multiplier
    
    0x309C[0] pll3_rdiv
    pll3_rdiv + 1
    
    0x309A[0:3] pll3_sys_div
    pll3_sys_div + 1
    
    0x309B[0:1] pll3_div
    pll3_div[] = {2, 2, 4, 5}
    
    VCO = XVCLK * 2 / pll3_prediv * pll3_multiplier * pll3_rdiv
    sysclk = VCO * 2 * 2 / pll3_sys_div / pll3_div

    XVCLK = 24 MHZ
    0x3098, 0x03
    0x3099, 0x1e
    0x309a, 0x02
    0x309b, 0x01
    0x309c, 0x00


    VCO = 24 * 2 / 6 * 31 * 1
    sysclk = VCO * 2  * 2 / 3 / 2
    sysclk = 160 MHZ
    */

    OV5694MIPI_write_cmos_sensor(0x0103,0x01);  // Software Reset

    OV5694MIPI_write_cmos_sensor(0x3001,0x0a);  // FSIN output, FREX input
    OV5694MIPI_write_cmos_sensor(0x3002,0x80);  // Vsync output, Href, Frex, strobe, fsin, ilpwm input
    OV5694MIPI_write_cmos_sensor(0x3006,0x00);  // output value of GPIO
    
    OV5694MIPI_write_cmos_sensor(0x3011,0x21);  //MIPI 2 lane enable
    OV5694MIPI_write_cmos_sensor(0x3012,0x09);  //MIPI 10-bit
    OV5694MIPI_write_cmos_sensor(0x3013,0x10);  //Drive 1x
    OV5694MIPI_write_cmos_sensor(0x3014,0x00);
    OV5694MIPI_write_cmos_sensor(0x3015,0x08);  //MIPI on
    OV5694MIPI_write_cmos_sensor(0x3016,0xf0);
    OV5694MIPI_write_cmos_sensor(0x3017,0xf0);
    OV5694MIPI_write_cmos_sensor(0x3018,0xf0);
    OV5694MIPI_write_cmos_sensor(0x301b,0xb4);
    OV5694MIPI_write_cmos_sensor(0x301d,0x02);
    OV5694MIPI_write_cmos_sensor(0x3021,0x00);  //internal regulator on
    OV5694MIPI_write_cmos_sensor(0x3022,0x01);
    OV5694MIPI_write_cmos_sensor(0x3028,0x44);
    OV5694MIPI_write_cmos_sensor(0x3098,0x03);  //PLL
    OV5694MIPI_write_cmos_sensor(0x3099,0x1e);  //PLL
    OV5694MIPI_write_cmos_sensor(0x309a,0x02);  //PLL  0x02(20140110)
    OV5694MIPI_write_cmos_sensor(0x309b,0x01);  //PLL
    OV5694MIPI_write_cmos_sensor(0x309c,0x00);  //PLL
    OV5694MIPI_write_cmos_sensor(0x30a0,0xd2);
    OV5694MIPI_write_cmos_sensor(0x30a2,0x01);
    OV5694MIPI_write_cmos_sensor(0x30b2,0x00);
    OV5694MIPI_write_cmos_sensor(0x30b3,0x6a);  //PLL 50 64 6c 0x6a(20140110)
    OV5694MIPI_write_cmos_sensor(0x30b4,0x03);  //PLL
    OV5694MIPI_write_cmos_sensor(0x30b5,0x04);  //PLL
    OV5694MIPI_write_cmos_sensor(0x30b6,0x01);  //PLL
    OV5694MIPI_write_cmos_sensor(0x3104,0x21);  //sclk from dac_pll
    OV5694MIPI_write_cmos_sensor(0x3106,0x00);  //sclk_sel from pll_clk
    OV5694MIPI_write_cmos_sensor(0x3400,0x04);  //MWB R H
    OV5694MIPI_write_cmos_sensor(0x3401,0x00);  //MWB R L
    OV5694MIPI_write_cmos_sensor(0x3402,0x04);  //MWB G H
    OV5694MIPI_write_cmos_sensor(0x3403,0x00);  //MWB G L
    OV5694MIPI_write_cmos_sensor(0x3404,0x04);  //MWB B H
    OV5694MIPI_write_cmos_sensor(0x3405,0x00);  //MWB B L
    OV5694MIPI_write_cmos_sensor(0x3406,0x01);  //MWB gain enable
    OV5694MIPI_write_cmos_sensor(0x3500,0x00);  //exposure HH
    OV5694MIPI_write_cmos_sensor(0x3501,0x3d);  //exposure H   3d
    OV5694MIPI_write_cmos_sensor(0x3502,0x00);  //exposure L
    OV5694MIPI_write_cmos_sensor(0x3503,0x07);  //gain 1 frame latch, VTS manual, AGC manual, AEC manual
    OV5694MIPI_write_cmos_sensor(0x3504,0x00);  //manual gain H
    OV5694MIPI_write_cmos_sensor(0x3505,0x00);  //manual gain L
    OV5694MIPI_write_cmos_sensor(0x3506,0x00);  //short exposure HH
    OV5694MIPI_write_cmos_sensor(0x3507,0x02);  //short exposure H
    OV5694MIPI_write_cmos_sensor(0x3508,0x00);  //short exposure L
    OV5694MIPI_write_cmos_sensor(0x3509,0x10);  //use real ggain
    OV5694MIPI_write_cmos_sensor(0x350a,0x00);  //gain H 
    OV5694MIPI_write_cmos_sensor(0x350b,0x40);  //gain L
    
    OV5694MIPI_write_cmos_sensor(0x3600,0xbc);  
    OV5694MIPI_write_cmos_sensor(0x3601,0x0a);  //analog
    OV5694MIPI_write_cmos_sensor(0x3602,0x38);
    OV5694MIPI_write_cmos_sensor(0x3612,0x80);
    OV5694MIPI_write_cmos_sensor(0x3620,0x44);
    OV5694MIPI_write_cmos_sensor(0x3621,0xb5);
    OV5694MIPI_write_cmos_sensor(0x3622,0x0c);
    
    OV5694MIPI_write_cmos_sensor(0x3625,0x10);
    OV5694MIPI_write_cmos_sensor(0x3630,0x55);
    OV5694MIPI_write_cmos_sensor(0x3631,0xf4);
    OV5694MIPI_write_cmos_sensor(0x3632,0x00);
    OV5694MIPI_write_cmos_sensor(0x3633,0x34);
    OV5694MIPI_write_cmos_sensor(0x3634,0x02);
    OV5694MIPI_write_cmos_sensor(0x364d,0x0d);
    OV5694MIPI_write_cmos_sensor(0x364f,0xdd);
    OV5694MIPI_write_cmos_sensor(0x3660,0x04);
    OV5694MIPI_write_cmos_sensor(0x3662,0x10);
    OV5694MIPI_write_cmos_sensor(0x3663,0xf1);
    OV5694MIPI_write_cmos_sensor(0x3665,0x00);
    OV5694MIPI_write_cmos_sensor(0x3666,0x20);
    OV5694MIPI_write_cmos_sensor(0x3667,0x00);
    OV5694MIPI_write_cmos_sensor(0x366a,0x80);
    OV5694MIPI_write_cmos_sensor(0x3680,0xe0);
    OV5694MIPI_write_cmos_sensor(0x3681,0x00);  //analog
    OV5694MIPI_write_cmos_sensor(0x3700,0x42);  //sensor control
    OV5694MIPI_write_cmos_sensor(0x3701,0x14);
    OV5694MIPI_write_cmos_sensor(0x3702,0xa0);
    OV5694MIPI_write_cmos_sensor(0x3703,0xd8);
    OV5694MIPI_write_cmos_sensor(0x3704,0x78);
    OV5694MIPI_write_cmos_sensor(0x3705,0x02);
    OV5694MIPI_write_cmos_sensor(0x3708,0xe6);  //e6 //e2
    OV5694MIPI_write_cmos_sensor(0x3709,0xc7);  //c7 //c3
    OV5694MIPI_write_cmos_sensor(0x370a,0x00);
    OV5694MIPI_write_cmos_sensor(0x370b,0x20);
    OV5694MIPI_write_cmos_sensor(0x370c,0x0c);
    OV5694MIPI_write_cmos_sensor(0x370d,0x11);
    OV5694MIPI_write_cmos_sensor(0x370e,0x00);
    OV5694MIPI_write_cmos_sensor(0x370f,0x40);
    OV5694MIPI_write_cmos_sensor(0x3710,0x00);
    OV5694MIPI_write_cmos_sensor(0x371a,0x1c);
    OV5694MIPI_write_cmos_sensor(0x371b,0x05);
    OV5694MIPI_write_cmos_sensor(0x371c,0x01);
    OV5694MIPI_write_cmos_sensor(0x371e,0xa1);
    OV5694MIPI_write_cmos_sensor(0x371f,0x0c);
    OV5694MIPI_write_cmos_sensor(0x3721,0x00);
    OV5694MIPI_write_cmos_sensor(0x3724,0x10);
    OV5694MIPI_write_cmos_sensor(0x3726,0x00);
    OV5694MIPI_write_cmos_sensor(0x372a,0x01);
    OV5694MIPI_write_cmos_sensor(0x3730,0x10);
    OV5694MIPI_write_cmos_sensor(0x3738,0x22);
    OV5694MIPI_write_cmos_sensor(0x3739,0xe5);
    OV5694MIPI_write_cmos_sensor(0x373a,0x50);
    OV5694MIPI_write_cmos_sensor(0x373b,0x02);
    OV5694MIPI_write_cmos_sensor(0x373c,0x41);
    OV5694MIPI_write_cmos_sensor(0x373f,0x02);
    OV5694MIPI_write_cmos_sensor(0x3740,0x42);
    OV5694MIPI_write_cmos_sensor(0x3741,0x02);
    OV5694MIPI_write_cmos_sensor(0x3742,0x18);
    OV5694MIPI_write_cmos_sensor(0x3743,0x01);
    OV5694MIPI_write_cmos_sensor(0x3744,0x02);
    OV5694MIPI_write_cmos_sensor(0x3747,0x10);
    OV5694MIPI_write_cmos_sensor(0x374c,0x04);
    OV5694MIPI_write_cmos_sensor(0x3751,0xf0);
    OV5694MIPI_write_cmos_sensor(0x3752,0x00);
    OV5694MIPI_write_cmos_sensor(0x3753,0x00);
    OV5694MIPI_write_cmos_sensor(0x3754,0xc0);
    OV5694MIPI_write_cmos_sensor(0x3755,0x00);
    OV5694MIPI_write_cmos_sensor(0x3756,0x1a);
    OV5694MIPI_write_cmos_sensor(0x3758,0x00);
    OV5694MIPI_write_cmos_sensor(0x3759,0x0f);
    OV5694MIPI_write_cmos_sensor(0x376b,0x44);
    OV5694MIPI_write_cmos_sensor(0x375c,0x04);
    OV5694MIPI_write_cmos_sensor(0x3774,0x10);
    OV5694MIPI_write_cmos_sensor(0x3776,0x00);
    OV5694MIPI_write_cmos_sensor(0x377f,0x08);  //sensor control
    OV5694MIPI_write_cmos_sensor(0x3780,0x22);  //PSRAM control
    OV5694MIPI_write_cmos_sensor(0x3781,0x0c);
    OV5694MIPI_write_cmos_sensor(0x3784,0x2c);
    OV5694MIPI_write_cmos_sensor(0x3785,0x1e);
    OV5694MIPI_write_cmos_sensor(0x378f,0xf5);
    OV5694MIPI_write_cmos_sensor(0x3791,0xb0);
    OV5694MIPI_write_cmos_sensor(0x3795,0x00);
    OV5694MIPI_write_cmos_sensor(0x3796,0x64);
    OV5694MIPI_write_cmos_sensor(0x3797,0x11);
    OV5694MIPI_write_cmos_sensor(0x3798,0x30);
    OV5694MIPI_write_cmos_sensor(0x3799,0x41);
    OV5694MIPI_write_cmos_sensor(0x379a,0x07);
    OV5694MIPI_write_cmos_sensor(0x379b,0xb0);
    OV5694MIPI_write_cmos_sensor(0x379c,0x0c);  //PSRAM control                       
    OV5694MIPI_write_cmos_sensor(0x37c5,0x00);  //sensor FREX exp HH                  
    OV5694MIPI_write_cmos_sensor(0x37c6,0x00);  //sensor FREX exp H                   
    OV5694MIPI_write_cmos_sensor(0x37c7,0x00);  //sensor FREX exp L                   
    OV5694MIPI_write_cmos_sensor(0x37c9,0x00);  //strobe Width HH                     
    OV5694MIPI_write_cmos_sensor(0x37ca,0x00);  //strobe Width H                      
    OV5694MIPI_write_cmos_sensor(0x37cb,0x00);  //strobe Width L                      
    OV5694MIPI_write_cmos_sensor(0x37de,0x00);  //sensor FREX PCHG Width H            
    OV5694MIPI_write_cmos_sensor(0x37df,0x00);  //sensor FREX PCHG Width L
    
    OV5694MIPI_write_cmos_sensor(0x3800,0x00);  //X start H                           
    OV5694MIPI_write_cmos_sensor(0x3801,0x00);  //X start L                           
    OV5694MIPI_write_cmos_sensor(0x3802,0x00);  //Y start H                           
    OV5694MIPI_write_cmos_sensor(0x3803,0x00);  //Y start L                           
    OV5694MIPI_write_cmos_sensor(0x3804,0x0a);  //X end H                             
    OV5694MIPI_write_cmos_sensor(0x3805,0x3f);  //X end L                             
    OV5694MIPI_write_cmos_sensor(0x3806,0x07);  //Y end H                             
    OV5694MIPI_write_cmos_sensor(0x3807,0xa3);  //Y end L                             
    OV5694MIPI_write_cmos_sensor(0x3808,0x05);  //X output size H  2592             //05  0a
    OV5694MIPI_write_cmos_sensor(0x3809,0x10);  //X output size L                       //10  20
    OV5694MIPI_write_cmos_sensor(0x380a,0x03);  //Y output size H  1944             //03  07
    OV5694MIPI_write_cmos_sensor(0x380b,0xcc);  //Y output size L                       //cc   98
    OV5694MIPI_write_cmos_sensor(0x380c,0x0a);  //HTS H  2688                             
    OV5694MIPI_write_cmos_sensor(0x380d,0x80);  //HTS H                               
    OV5694MIPI_write_cmos_sensor(0x380e,0x07);  //HTS L  1984                           //05
    OV5694MIPI_write_cmos_sensor(0x380f,0xc0);  //HTS L                                     //d0
    
    OV5694MIPI_write_cmos_sensor(0x3810,0x00);  //timing ISP x win H                  
    OV5694MIPI_write_cmos_sensor(0x3811,0x08);  //timing ISP x win L     //02             
    OV5694MIPI_write_cmos_sensor(0x3812,0x00);  //timing ISP y win H                  
    OV5694MIPI_write_cmos_sensor(0x3813,0x02);  //timing ISP y win L                  
    OV5694MIPI_write_cmos_sensor(0x3814,0x31);  //timing x inc                                        //31  11
    OV5694MIPI_write_cmos_sensor(0x3815,0x31);  //timing y ing                                        //31  11

    /********************************************************
       *
       *   0x3820[2] ISP Vertical flip
       *   0x3820[1] Sensor Vertical flip
       *
       *   0x3821[2] ISP Horizontal mirror
       *   0x3821[1] Sensor Horizontal mirror
       *
       *   ISP and Sensor flip or mirror register bit should be the same!!
       *
       ********************************************************/
    OV5694MIPI_write_cmos_sensor(0x3820,0x04);  //v fast bin on, v flip off, v bin off           //04  00
    OV5694MIPI_write_cmos_sensor(0x3821,0x1f);  //hsync on, mirror on, hbin on              //1f   1e
    
    OV5694MIPI_write_cmos_sensor(0x3823,0x00);
    OV5694MIPI_write_cmos_sensor(0x3824,0x00);
    OV5694MIPI_write_cmos_sensor(0x3825,0x00);
    OV5694MIPI_write_cmos_sensor(0x3826,0x00);
    OV5694MIPI_write_cmos_sensor(0x3827,0x00);
    OV5694MIPI_write_cmos_sensor(0x382a,0x04);
    OV5694MIPI_write_cmos_sensor(0x3a04,0x06);
    OV5694MIPI_write_cmos_sensor(0x3a05,0x14);
    OV5694MIPI_write_cmos_sensor(0x3a06,0x00);
    OV5694MIPI_write_cmos_sensor(0x3a07,0xfe);
    OV5694MIPI_write_cmos_sensor(0x3b00,0x00);  //strobe off          
    OV5694MIPI_write_cmos_sensor(0x3b02,0x00);  //strobe dummy line H     
    OV5694MIPI_write_cmos_sensor(0x3b03,0x00);  //strobe dummy line L     
    OV5694MIPI_write_cmos_sensor(0x3b04,0x00);  //strobe at next frame    
    OV5694MIPI_write_cmos_sensor(0x3b05,0x00);  //strobe pulse width      
    OV5694MIPI_write_cmos_sensor(0x3e07,0x20);
    OV5694MIPI_write_cmos_sensor(0x4000,0x08);
    OV5694MIPI_write_cmos_sensor(0x4001,0x04);  //BLC start line                                                                         
    OV5694MIPI_write_cmos_sensor(0x4002,0x45);  //BLC auto enable, do 5 frames                                                           
    OV5694MIPI_write_cmos_sensor(0x4004,0x08);  //8 black lines                                                                          
    OV5694MIPI_write_cmos_sensor(0x4005,0x18);  //don't output black line, apply one channel offset to all, BLC triggered by gain change,
    OV5694MIPI_write_cmos_sensor(0x4006,0x20);  //ZLINE COEF                                                                             
    OV5694MIPI_write_cmos_sensor(0x4008,0x24);                                                                                       
    OV5694MIPI_write_cmos_sensor(0x4009,0x40);  //black level target                                                                     
    OV5694MIPI_write_cmos_sensor(0x400c,0x00);  //BLC man level0 H                                                                       
    OV5694MIPI_write_cmos_sensor(0x400d,0x00);  //BLC man level0 L                                                                       
    OV5694MIPI_write_cmos_sensor(0x4058,0x00);                                                                                         
    OV5694MIPI_write_cmos_sensor(0x404e,0x37);  //BLC maximum black level                                                                
    OV5694MIPI_write_cmos_sensor(0x404f,0x8f);  //BLC stable range                                                                       
    OV5694MIPI_write_cmos_sensor(0x4058,0x00);                                                                                     
    OV5694MIPI_write_cmos_sensor(0x4101,0xb2);                                                                                   
    OV5694MIPI_write_cmos_sensor(0x4303,0x00);  //test pattern off                                                                       
    OV5694MIPI_write_cmos_sensor(0x4304,0x08);  //test pattern option                                                                    
    OV5694MIPI_write_cmos_sensor(0x4307,0x30);                                                                                       
    OV5694MIPI_write_cmos_sensor(0x4311,0x04);  //Vsync width H                                                                          
    OV5694MIPI_write_cmos_sensor(0x4315,0x01);  //Vsync width L                                                                          
    OV5694MIPI_write_cmos_sensor(0x4511,0x05);                                                                                      
    OV5694MIPI_write_cmos_sensor(0x4512,0x01); 

    OV5694MIPI_write_cmos_sensor(0x4800,0x14);  // MIPI line sync enable
    
    OV5694MIPI_write_cmos_sensor(0x4806,0x00);                                                                                      
    OV5694MIPI_write_cmos_sensor(0x4816,0x52);  //embedded line data type                                                                
    OV5694MIPI_write_cmos_sensor(0x481f,0x30);  //max clk_prepare                                                                        
    OV5694MIPI_write_cmos_sensor(0x4826,0x2c);  //hs prepare min                                                                         
    OV5694MIPI_write_cmos_sensor(0x4831,0x64);  //UI hs prepare     
    OV5694MIPI_write_cmos_sensor(0x4d00,0x04);  //temprature sensor 
    OV5694MIPI_write_cmos_sensor(0x4d01,0x71);                 
    OV5694MIPI_write_cmos_sensor(0x4d02,0xfd);                  
    OV5694MIPI_write_cmos_sensor(0x4d03,0xf5);                  
    OV5694MIPI_write_cmos_sensor(0x4d04,0x0c);                  
    OV5694MIPI_write_cmos_sensor(0x4d05,0xcc);  //temperature sensor
    OV5694MIPI_write_cmos_sensor(0x4837,0x09);  //MIPI global timing  //0d 0a
    OV5694MIPI_write_cmos_sensor(0x5000,0x06);  //BPC on, WPC on    
    OV5694MIPI_write_cmos_sensor(0x5001,0x01);  //MWB on            
    OV5694MIPI_write_cmos_sensor(0x5002,0x00);  //scal off          
    OV5694MIPI_write_cmos_sensor(0x5003,0x20);
    OV5694MIPI_write_cmos_sensor(0x5046,0x0a);  //SOF auto mode 
    OV5694MIPI_write_cmos_sensor(0x5013,0x00);            
    OV5694MIPI_write_cmos_sensor(0x5046,0x0a);  //SOF auto mode 

    //update DPC settings
    OV5694MIPI_write_cmos_sensor(0x5003,0x20);
    OV5694MIPI_write_cmos_sensor(0x5780,0xfc);  //DPC   
    OV5694MIPI_write_cmos_sensor(0x5781,0x13);
    OV5694MIPI_write_cmos_sensor(0x5782,0x03);
    OV5694MIPI_write_cmos_sensor(0x5786,0x20);
    OV5694MIPI_write_cmos_sensor(0x5787,0x40);
    OV5694MIPI_write_cmos_sensor(0x5788,0x08);
    OV5694MIPI_write_cmos_sensor(0x5789,0x08);
    OV5694MIPI_write_cmos_sensor(0x578a,0x02);
    OV5694MIPI_write_cmos_sensor(0x578b,0x01);
    OV5694MIPI_write_cmos_sensor(0x578c,0x01);
    OV5694MIPI_write_cmos_sensor(0x578d,0x0c);
    OV5694MIPI_write_cmos_sensor(0x578e,0x02);
    OV5694MIPI_write_cmos_sensor(0x578f,0x01);
    OV5694MIPI_write_cmos_sensor(0x5790,0x01);
    OV5694MIPI_write_cmos_sensor(0x5791,0xff); //DPC
    
    OV5694MIPI_write_cmos_sensor(0x5842,0x01);  //LENC BR Hscale H
    OV5694MIPI_write_cmos_sensor(0x5843,0x2b);  //LENC BR Hscale L
    OV5694MIPI_write_cmos_sensor(0x5844,0x01);  //LENC BR Vscale H
    OV5694MIPI_write_cmos_sensor(0x5845,0x92);  //LENC BR Vscale L
    OV5694MIPI_write_cmos_sensor(0x5846,0x01);  //LENC G Hscale H 
    OV5694MIPI_write_cmos_sensor(0x5847,0x8f);  //LENC G Hscale L 
    OV5694MIPI_write_cmos_sensor(0x5848,0x01);  //LENC G Vscale H 
    OV5694MIPI_write_cmos_sensor(0x5849,0x0c);  //LENC G Vscale L 
    OV5694MIPI_write_cmos_sensor(0x5e00,0x00);  //test pattern off
    OV5694MIPI_write_cmos_sensor(0x5e10,0x0c);
    OV5694MIPI_write_cmos_sensor(0x0100,0x01);  //wake up
    #ifdef OV5694_OTP
	ov5694_update_otp_wb();
	ov5694_update_otp_lenc();
    #endif
}   /*  OV5694MIPI_Sensor_Init  */


static void OV5694MIPI_Preview_Setting(void)
{
    //5.1.2 FQPreview 1296x972 30fps 24M MCLK 2lane 864Mbps/lane
    OV5694MIPI_write_cmos_sensor(0x0100,0x00);  //software sleep

    OV5694MIPI_write_cmos_sensor(0x3708,0xe6);  //sensor control
    OV5694MIPI_write_cmos_sensor(0x3709,0xc7);  //sensor control
    OV5694MIPI_write_cmos_sensor(0x3803,0x00);  //timing Y start L
    OV5694MIPI_write_cmos_sensor(0x3806,0x07);  //timing Y end H
    OV5694MIPI_write_cmos_sensor(0x3807,0xa3);  //timing Y end L
    
    OV5694MIPI_write_cmos_sensor(0x3808,0x05);  //X output size H  1296
    OV5694MIPI_write_cmos_sensor(0x3809,0x10);  //X output size L
    OV5694MIPI_write_cmos_sensor(0x380a,0x03);  //Y output size H  972
    OV5694MIPI_write_cmos_sensor(0x380b,0xcc);  //Y output size L
    
    //OV5694MIPI_write_cmos_sensor(0x380c,0x0a);  //HTS H  2688
    //OV5694MIPI_write_cmos_sensor(0x380d,0x80);  //HTS L
    //OV5694MIPI_write_cmos_sensor(0x380e,0x07);  //VTS H  1984
    //OV5694MIPI_write_cmos_sensor(0x380f,0xc0);  //VTS L
    OV5694MIPI_write_cmos_sensor(0x380c, ((OV5694MIPI_PV_PERIOD_PIXEL_NUMS >> 8) & 0xFF)); // hts = 2688
    OV5694MIPI_write_cmos_sensor(0x380d, (OV5694MIPI_PV_PERIOD_PIXEL_NUMS & 0xFF));        // hts
    OV5694MIPI_write_cmos_sensor(0x380e, ((OV5694MIPI_PV_PERIOD_LINE_NUMS >> 8) & 0xFF));  // vts = 1984
    OV5694MIPI_write_cmos_sensor(0x380f, (OV5694MIPI_PV_PERIOD_LINE_NUMS & 0xFF));         // vts    
    
    OV5694MIPI_write_cmos_sensor(0x3810,0x00);  //timing ISP x win H
    OV5694MIPI_write_cmos_sensor(0x3811,0x08);  //timing ISP x win L
    OV5694MIPI_write_cmos_sensor(0x3812,0x00);  //timing ISP y win H
    OV5694MIPI_write_cmos_sensor(0x3813,0x02);  //timing ISP y win L
    OV5694MIPI_write_cmos_sensor(0x3814,0x31);  //timing X inc
    OV5694MIPI_write_cmos_sensor(0x3815,0x31);  //timing Y inc

    /********************************************************
       *
       *   0x3820[2] ISP Vertical flip
       *   0x3820[1] Sensor Vertical flip
       *
       *   0x3821[2] ISP Horizontal mirror
       *   0x3821[1] Sensor Horizontal mirror
       *
       *   ISP and Sensor flip or mirror register bit should be the same!!
       *
       ********************************************************/    
    OV5694MIPI_write_cmos_sensor(0x3820,0x04);  //v fast bin on, v flip off, v bin off
    OV5694MIPI_write_cmos_sensor(0x3821,0x1f);  //hsync on, mirror on, hbin on
  
    
    OV5694MIPI_write_cmos_sensor(0x3a04,0x06);
    OV5694MIPI_write_cmos_sensor(0x3a05,0x14);
    OV5694MIPI_write_cmos_sensor(0x5002,0x00);  //scale off

    OV5694MIPI_write_cmos_sensor(0x0100,0x01);  //wake up   
}   /*  OV5694MIPI_Capture_Setting  */

static void OV5694MIPI_Capture_Setting(void)
{
    //5.1.3 Capture 2592x1944 30fps 24M MCLK 2lane 864Mbps/lane
    OV5694MIPI_write_cmos_sensor(0x0100,0x00);  //software sleep
    
    OV5694MIPI_write_cmos_sensor(0x3708,0xe2);  //sensor control
    OV5694MIPI_write_cmos_sensor(0x3709,0xc3);  //sensor control
    OV5694MIPI_write_cmos_sensor(0x3803,0x00);  //timing Y start L
    OV5694MIPI_write_cmos_sensor(0x3806,0x07);  //timing Y end H
    OV5694MIPI_write_cmos_sensor(0x3807,0xa3);  //timing Y end L
    
    OV5694MIPI_write_cmos_sensor(0x3808,0x0a);  //X output size H  2592
    OV5694MIPI_write_cmos_sensor(0x3809,0x20);  //X output size L
    OV5694MIPI_write_cmos_sensor(0x380a,0x07);  //Y output size H  1944
    OV5694MIPI_write_cmos_sensor(0x380b,0x98);  //Y output size L
    
    //OV5694MIPI_write_cmos_sensor(0x380c,0x0a);  //HTS H 2688
    //OV5694MIPI_write_cmos_sensor(0x380d,0x80);  //HTS L
    //OV5694MIPI_write_cmos_sensor(0x380e,0x07);  //VTS H 1984
    //OV5694MIPI_write_cmos_sensor(0x380f,0xc0);  //VTS L
    OV5694MIPI_write_cmos_sensor(0x380c, ((OV5694MIPI_FULL_PERIOD_PIXEL_NUMS >> 8) & 0xFF)); // hts = 2688
    OV5694MIPI_write_cmos_sensor(0x380d, (OV5694MIPI_FULL_PERIOD_PIXEL_NUMS & 0xFF));        // hts
    OV5694MIPI_write_cmos_sensor(0x380e, ((OV5694MIPI_FULL_PERIOD_LINE_NUMS >> 8) & 0xFF));  // vts = 1984
    OV5694MIPI_write_cmos_sensor(0x380f, (OV5694MIPI_FULL_PERIOD_LINE_NUMS & 0xFF));         // vts      
    
    OV5694MIPI_write_cmos_sensor(0x3810,0x00);  //timing ISP x win H
    OV5694MIPI_write_cmos_sensor(0x3811,0x10);  //timing ISP x win L
    OV5694MIPI_write_cmos_sensor(0x3812,0x00);  //timing ISP y win H
    OV5694MIPI_write_cmos_sensor(0x3813,0x06);  //timing ISP y win L
    OV5694MIPI_write_cmos_sensor(0x3814,0x11);  //timing X inc
    OV5694MIPI_write_cmos_sensor(0x3815,0x11);  //timing Y inc

    /********************************************************
       *
       *   0x3820[2] ISP Vertical flip
       *   0x3820[1] Sensor Vertical flip
       *
       *   0x3821[2] ISP Horizontal mirror
       *   0x3821[1] Sensor Horizontal mirror
       *
       *   ISP and Sensor flip or mirror register bit should be the same!!
       *
       ********************************************************/
    OV5694MIPI_write_cmos_sensor(0x3820,0x00);  //v fast vbin on, flip off, v bin on
    OV5694MIPI_write_cmos_sensor(0x3821,0x1e);  //hsync on, h mirror on, h bin on


    
    OV5694MIPI_write_cmos_sensor(0x3a04,0x06);
    OV5694MIPI_write_cmos_sensor(0x3a05,0x14);
    OV5694MIPI_write_cmos_sensor(0x5002,0x00);  //scale off
    
    OV5694MIPI_write_cmos_sensor(0x0100,0x01);  //wake up
}

#ifdef VIDEO_720P

static void OV5694MIPI_Video_720P_Setting(void)
{
    SENSORDB("Enter!");

    //5.1.4 Video BQ720p Full FOV 30fps 24M MCLK 2lane 864Mbps/lane
    OV5694MIPI_write_cmos_sensor(0x0100,0x00);  //software sleep
    
    OV5694MIPI_write_cmos_sensor(0x3708,0xe6);  //sensor control
    OV5694MIPI_write_cmos_sensor(0x3709,0xc3);  //sensor control
    OV5694MIPI_write_cmos_sensor(0x3803,0xf4);  //timing Y start L
    OV5694MIPI_write_cmos_sensor(0x3806,0x06);  //timing Y end H
    OV5694MIPI_write_cmos_sensor(0x3807,0xab);  //timing Y end L
    
    OV5694MIPI_write_cmos_sensor(0x3808,0x05);  //X output size H  1280
    OV5694MIPI_write_cmos_sensor(0x3809,0x00);  //X output size L
    OV5694MIPI_write_cmos_sensor(0x380a,0x02);  //Y output size H  720
    OV5694MIPI_write_cmos_sensor(0x380b,0xd0);  //Y output size L
    
    //OV5694MIPI_write_cmos_sensor(0x380c,0x0d);  //HTS H  2688
    //OV5694MIPI_write_cmos_sensor(0x380d,0xb0);  //HTS L
    //OV5694MIPI_write_cmos_sensor(0x380e,0x05);  //VTS H  1984
    //OV5694MIPI_write_cmos_sensor(0x380f,0xf0);  //VTS L
    OV5694MIPI_write_cmos_sensor(0x380c, ((OV5694MIPI_VIDEO_PERIOD_PIXEL_NUMS >> 8) & 0xFF)); // hts = 2688
    OV5694MIPI_write_cmos_sensor(0x380d, (OV5694MIPI_VIDEO_PERIOD_PIXEL_NUMS & 0xFF));        // hts
    OV5694MIPI_write_cmos_sensor(0x380e, ((OV5694MIPI_VIDEO_PERIOD_LINE_NUMS >> 8) & 0xFF));  // vts = 1984
    OV5694MIPI_write_cmos_sensor(0x380f, (OV5694MIPI_VIDEO_PERIOD_LINE_NUMS & 0xFF));         // vts     
    
    OV5694MIPI_write_cmos_sensor(0x3810,0x00);  //timing ISP x win H
    OV5694MIPI_write_cmos_sensor(0x3811,0x08);  //timing ISP x win L
    OV5694MIPI_write_cmos_sensor(0x3812,0x00);  //timing ISP y win H
    OV5694MIPI_write_cmos_sensor(0x3813,0x02);  //timing ISP y win L
    OV5694MIPI_write_cmos_sensor(0x3814,0x31);  //timing X inc
    OV5694MIPI_write_cmos_sensor(0x3815,0x31);  //timing Y inc

    /********************************************************
       *
       *   0x3820[2] ISP Vertical flip
       *   0x3820[1] Sensor Vertical flip
       *
       *   0x3821[2] ISP Horizontal mirror
       *   0x3821[1] Sensor Horizontal mirror
       *
       *   ISP and Sensor flip or mirror register bit should be the same!!
       *
       ********************************************************/
    OV5694MIPI_write_cmos_sensor(0x3820,0x01);  //v fast vbin of, flip off, v bin on
    OV5694MIPI_write_cmos_sensor(0x3821,0x1f);  //hsync on, h mirror on, h bin on
    
    OV5694MIPI_write_cmos_sensor(0x3a04,0x06);
    OV5694MIPI_write_cmos_sensor(0x3a05,0x14);
    OV5694MIPI_write_cmos_sensor(0x5002,0x00);  //scale off
    
    OV5694MIPI_write_cmos_sensor(0x0100,0x01);  //wake up

    SENSORDB("Exit!");
}

#elif defined VIDEO_1080P

static void OV5694MIPI_Video_1080P_Setting(void)
{
    SENSORDB("Enter!");
    
    //5.1.5 Video 1080p 30fps 24M MCLK 2lane 864Mbps/lane
    OV5694MIPI_write_cmos_sensor(0x0100,0x00);  //software sleep

    OV5694MIPI_write_cmos_sensor(0x3708,0xe2);  //sensor control
    OV5694MIPI_write_cmos_sensor(0x3709,0xc3);  //sensor control
    OV5694MIPI_write_cmos_sensor(0x3803,0xf8);  //timing Y start L
    OV5694MIPI_write_cmos_sensor(0x3806,0x06);  //timing Y end H
    OV5694MIPI_write_cmos_sensor(0x3807,0xab);  //timing Y end L

    OV5694MIPI_write_cmos_sensor(0x3808,0x07);  //X output size H  1920
    OV5694MIPI_write_cmos_sensor(0x3809,0x80);  //X output size L
    OV5694MIPI_write_cmos_sensor(0x380a,0x04);  //Y output size H  1080
    OV5694MIPI_write_cmos_sensor(0x380b,0x38);  //Y output size L

    //OV5694MIPI_write_cmos_sensor(0x380c,0x0a);  //HTS H  2688
    //OV5694MIPI_write_cmos_sensor(0x380d,0x80);  //HTS L
    //OV5694MIPI_write_cmos_sensor(0x380e,0x07);  //VTS H  1984
    //OV5694MIPI_write_cmos_sensor(0x380f,0xc0);  //VYS L
    OV5694MIPI_write_cmos_sensor(0x380c, ((OV5694MIPI_VIDEO_PERIOD_PIXEL_NUMS >> 8) & 0xFF)); // hts = 2688
    OV5694MIPI_write_cmos_sensor(0x380d, (OV5694MIPI_VIDEO_PERIOD_PIXEL_NUMS & 0xFF));        // hts
    OV5694MIPI_write_cmos_sensor(0x380e, ((OV5694MIPI_VIDEO_PERIOD_LINE_NUMS >> 8) & 0xFF));  // vts = 1984
    OV5694MIPI_write_cmos_sensor(0x380f, (OV5694MIPI_VIDEO_PERIOD_LINE_NUMS & 0xFF));         // vts       

    OV5694MIPI_write_cmos_sensor(0x3810,0x00);  //timing ISP x win H
    OV5694MIPI_write_cmos_sensor(0x3811,0x02);  //timing ISP x win L
    OV5694MIPI_write_cmos_sensor(0x3812,0x00);  //timing ISP y win H
    OV5694MIPI_write_cmos_sensor(0x3813,0x02);  //timing ISP y win L
    OV5694MIPI_write_cmos_sensor(0x3814,0x11);  //timing X inc
    OV5694MIPI_write_cmos_sensor(0x3815,0x11);  //timing Y inc

    /********************************************************
       *
       *   0x3820[2] ISP Vertical flip
       *   0x3820[1] Sensor Vertical flip
       *
       *   0x3821[2] ISP Horizontal mirror
       *   0x3821[1] Sensor Horizontal mirror
       *
       *   ISP and Sensor flip or mirror register bit should be the same!!
       *
       ********************************************************/
    OV5694MIPI_write_cmos_sensor(0x3820,0x00);  //v bin off
    OV5694MIPI_write_cmos_sensor(0x3821,0x1e);  //hsync on, h mirror on, h bin off
    
    OV5694MIPI_write_cmos_sensor(0x3a04,0x06);
    OV5694MIPI_write_cmos_sensor(0x3a05,0x14);
    OV5694MIPI_write_cmos_sensor(0x5002,0x80);  //scale on

    OV5694MIPI_write_cmos_sensor(0x0100,0x01);  //wake up

    SENSORDB("Exit!");
}

#else

static void OV5694MIPI_Video_Setting(void)
{
    SENSORDB("Enter!");
    
    //5.1.3 Video 2592x1944 30fps 24M MCLK 2lane 864Mbps/lane
    OV5694MIPI_write_cmos_sensor(0x0100,0x00);  //software sleep

    OV5694MIPI_write_cmos_sensor(0x3708,0xe2);  //sensor control
    OV5694MIPI_write_cmos_sensor(0x3709,0xc3);  //sensor control
    OV5694MIPI_write_cmos_sensor(0x3803,0x00);  //timing Y start L
    OV5694MIPI_write_cmos_sensor(0x3806,0x07);  //timing Y end H
    OV5694MIPI_write_cmos_sensor(0x3807,0xa3);  //timing Y end L

    OV5694MIPI_write_cmos_sensor(0x3808,0x0a);  //X output size H  2592
    OV5694MIPI_write_cmos_sensor(0x3809,0x20);  //X output size L
    OV5694MIPI_write_cmos_sensor(0x380a,0x07);  //Y output size H  1944
    OV5694MIPI_write_cmos_sensor(0x380b,0x98);  //Y output size L

    //OV5694MIPI_write_cmos_sensor(0x380c,0x0a);  //HTS H 2688
    //OV5694MIPI_write_cmos_sensor(0x380d,0x80);  //HTS L
    //OV5694MIPI_write_cmos_sensor(0x380e,0x07);  //VTS H 1984
    //OV5694MIPI_write_cmos_sensor(0x380f,0xc0);  //VTS L
    OV5694MIPI_write_cmos_sensor(0x380c, ((OV5694MIPI_VIDEO_PERIOD_PIXEL_NUMS >> 8) & 0xFF)); // hts = 2688
    OV5694MIPI_write_cmos_sensor(0x380d, (OV5694MIPI_VIDEO_PERIOD_PIXEL_NUMS & 0xFF));        // hts
    OV5694MIPI_write_cmos_sensor(0x380e, ((OV5694MIPI_VIDEO_PERIOD_LINE_NUMS >> 8) & 0xFF));  // vts = 1984
    OV5694MIPI_write_cmos_sensor(0x380f, (OV5694MIPI_VIDEO_PERIOD_LINE_NUMS & 0xFF));         // vts       

    OV5694MIPI_write_cmos_sensor(0x3810,0x00);  //timing ISP x win H
    OV5694MIPI_write_cmos_sensor(0x3811,0x10);  //timing ISP x win L
    OV5694MIPI_write_cmos_sensor(0x3812,0x00);  //timing ISP y win H
    OV5694MIPI_write_cmos_sensor(0x3813,0x06);  //timing ISP y win L
    OV5694MIPI_write_cmos_sensor(0x3814,0x11);  //timing X inc
    OV5694MIPI_write_cmos_sensor(0x3815,0x11);  //timing Y inc

    /********************************************************
       *
       *   0x3820[2] ISP Vertical flip
       *   0x3820[1] Sensor Vertical flip
       *
       *   0x3821[2] ISP Horizontal mirror
       *   0x3821[1] Sensor Horizontal mirror
       *
       *   ISP and Sensor flip or mirror register bit should be the same!!
       *
       ********************************************************/
    OV5694MIPI_write_cmos_sensor(0x3820,0x00);  //v fast vbin on, flip off, v bin on
    OV5694MIPI_write_cmos_sensor(0x3821,0x1e);  //hsync on, h mirror on, h bin on
    
    OV5694MIPI_write_cmos_sensor(0x3a04,0x06);
    OV5694MIPI_write_cmos_sensor(0x3a05,0x14);
    OV5694MIPI_write_cmos_sensor(0x5002,0x00);  //scale off

    OV5694MIPI_write_cmos_sensor(0x0100,0x01);  //wake up

    SENSORDB("Exit!");
}

#endif


/*************************************************************************
* FUNCTION
*   OV5694MIPIOpen
*
* DESCRIPTION
*   This function initialize the registers of CMOS sensor
*
* PARAMETERS
*   None
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 OV5694MIPIOpen(void)
{
    const kal_uint8 i2c_addr[] = {OV5694MIPI_WRITE_ID_1, OV5694MIPI_WRITE_ID_2};
	kal_uint16 i;
    kal_uint16 sensor_id = 0;
	int is_ov5694=1;

    //ov5694 have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address
	for(i = 0; i < sizeof(i2c_addr) / sizeof(i2c_addr[0]); i++)
	{
		spin_lock(&ov5694mipi_drv_lock);
		OV5694MIPI_sensor.i2c_write_id = i2c_addr[i];
		spin_unlock(&ov5694mipi_drv_lock);

		sensor_id = ((OV5694MIPI_read_cmos_sensor(0x300A) << 8) | OV5694MIPI_read_cmos_sensor(0x300B));
		if(sensor_id == 0x5690)
			break;
	}   
    
    SENSORDB("i2c write address is %x", OV5694MIPI_sensor.i2c_write_id);  

    // check if sensor ID correct 
    SENSORDB("sensor_id = 0x%x ", sensor_id);
    if (sensor_id != 0x5690){
        return ERROR_SENSOR_CONNECT_FAIL;
    }
	#ifdef OV5694_OTP
    else
	{
	
	ov5694_read_otp_date();
	printk("honghaishen_ov5694 otp year is %d, month is %d, day is %d \n",ov5694_otp_data_year,ov5694_otp_data_month,ov5694_otp_data_day);
	if( (14*13*32 + 7*32 + 18) > (ov5694_otp_data_year*13*32 + ov5694_otp_data_month * 32 + ov5694_otp_data_day) )  // < 20140717
		is_ov5694=0;
	if( ((14*13*32 + 11*32 + 14) > (ov5694_otp_data_year*13*32 + ov5694_otp_data_month * 32 + ov5694_otp_data_day) )   // 10141110>x> 20141028
		&& ((14*13*32 + 10*32 + 28) < (ov5694_otp_data_year*13*32 + ov5694_otp_data_month * 32 + ov5694_otp_data_day) ) )
		is_ov5694=0;
	printk("honghaishen_ov5694 is_ov5694 is %d \n",is_ov5694);
	if(is_ov5694==0)
	{
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	
	}
	#endif
    
    /* initail sequence write in  */
    OV5694MIPI_Sensor_Init();

    spin_lock(&ov5694mipi_drv_lock);
    OV5694MIPIDuringTestPattern = KAL_FALSE;
    OV5694MIPIAutoFlickerMode = KAL_FALSE;
    OV5694MIPI_sensor.sensor_mode = OV5694MIPI_SENSOR_MODE_INIT;
    OV5694MIPI_sensor.shutter = 0x3D0;
    OV5694MIPI_sensor.gain = 0x100;
    OV5694MIPI_sensor.pclk = OV5694MIPI_PREVIEW_CLK;
    OV5694MIPI_sensor.frame_length = OV5694MIPI_PV_PERIOD_LINE_NUMS;
    OV5694MIPI_sensor.line_length = OV5694MIPI_PV_PERIOD_PIXEL_NUMS;
    OV5694MIPI_sensor.dummy_pixel = 0;
    OV5694MIPI_sensor.dummy_line = 0;
    spin_unlock(&ov5694mipi_drv_lock);

    return ERROR_NONE;
}   /*  OV5694MIPIOpen  */


/*************************************************************************
* FUNCTION
*   OV5694GetSensorID
*
* DESCRIPTION
*   This function get the sensor ID 
*
* PARAMETERS
*   *sensorID : return the sensor ID 
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 OV5694GetSensorID(UINT32 *sensorID) 
{
    const kal_uint8 i2c_addr[] = {OV5694MIPI_WRITE_ID_1, OV5694MIPI_WRITE_ID_2};
	kal_uint16 i;
	#ifdef OV5694_OTP
	int is_ov5694 = 1;
	#endif

    //ov5694 have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address
	for(i = 0; i < sizeof(i2c_addr) / sizeof(i2c_addr[0]); i++)
	{
		spin_lock(&ov5694mipi_drv_lock);
		OV5694MIPI_sensor.i2c_write_id = i2c_addr[i];
		spin_unlock(&ov5694mipi_drv_lock);

		*sensorID = ((OV5694MIPI_read_cmos_sensor(0x300A) << 8) | OV5694MIPI_read_cmos_sensor(0x300B));
		if(*sensorID == 0x5690)
			break;
	}   
    
    SENSORDB("i2c write address is %x", OV5694MIPI_sensor.i2c_write_id);
    SENSORDB("Sensor ID: 0x%x ", *sensorID);
        
    if (*sensorID != 0x5690) {
        // if Sensor ID is not correct, Must set *sensorID to 0xFFFFFFFF 
        *sensorID = 0xFFFFFFFF;
        return ERROR_SENSOR_CONNECT_FAIL;
    }
	#ifdef OV5694_OTP
    else
	{
	
	ov5694_read_otp_date();
	printk("honghaishen_ov5694 otp year is %d, month is %d, day is %d \n",ov5694_otp_data_year,ov5694_otp_data_month,ov5694_otp_data_day);
	if( (14*13*32 + 7*32 + 18) > (ov5694_otp_data_year*13*32 + ov5694_otp_data_month * 32 + ov5694_otp_data_day) ) //  x < 20140717
		is_ov5694=0;
	if( ((14*13*32 + 11*32 + 14) > (ov5694_otp_data_year*13*32 + ov5694_otp_data_month * 32 + ov5694_otp_data_day) )  // 20141110>x>=20141028
		&& ((14*13*32 + 10*32 + 27) < (ov5694_otp_data_year*13*32 + ov5694_otp_data_month * 32 + ov5694_otp_data_day) ) )
		is_ov5694=0;
	printk("honghaishen_ov5694 is_ov5694 is %d \n",is_ov5694);
	if(is_ov5694==0)
	{
	    *sensorID = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	else 
		*sensorID = OV5694_SENSOR_ID;
	
	}
	#endif
 
    return ERROR_NONE;
}

/*************************************************************************
* FUNCTION
*   OV5694MIPIClose
*
* DESCRIPTION
*   
*
* PARAMETERS
*   None
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 OV5694MIPIClose(void)
{

    /*No Need to implement this function*/ 
    
    return ERROR_NONE;
}   /*  OV5694MIPIClose  */


/*************************************************************************
* FUNCTION
* OV5694MIPIPreview
*
* DESCRIPTION
*   This function start the sensor preview.
*
* PARAMETERS
*   *image_window : address pointer of pixel numbers in one period of HSYNC
*  *sensor_config_data : address pointer of line numbers in one period of VSYNC
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 OV5694MIPIPreview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    SENSORDB("Enter!");

	if(OV5694MIPI_SENSOR_MODE_PREVIEW == OV5694MIPI_sensor.sensor_mode)
	{
		// do nothing
		// FOR CCT PREVIEW
	}
	else
	{
		OV5694MIPI_Preview_Setting();
	}

    spin_lock(&ov5694mipi_drv_lock);
    OV5694MIPI_sensor.sensor_mode = OV5694MIPI_SENSOR_MODE_PREVIEW;
    OV5694MIPI_sensor.pclk = OV5694MIPI_PREVIEW_CLK;
    OV5694MIPI_sensor.video_mode = KAL_FALSE;
    OV5694MIPI_sensor.line_length = OV5694MIPI_PV_PERIOD_PIXEL_NUMS;
    OV5694MIPI_sensor.frame_length = OV5694MIPI_PV_PERIOD_LINE_NUMS;    
    spin_unlock(&ov5694mipi_drv_lock);

    SENSORDB("Exit!");
    
    return ERROR_NONE;
}   /*  OV5694MIPIPreview   */

UINT32 OV5694MIPIZSDPreview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    SENSORDB("Enter!");

    OV5694MIPI_Capture_Setting();

    spin_lock(&ov5694mipi_drv_lock);
    OV5694MIPI_sensor.sensor_mode = OV5694MIPI_SENSOR_MODE_CAPTURE;
    OV5694MIPI_sensor.pclk = OV5694MIPI_CAPTURE_CLK;
    OV5694MIPI_sensor.video_mode = KAL_FALSE;
    OV5694MIPI_sensor.line_length = OV5694MIPI_FULL_PERIOD_PIXEL_NUMS;
    OV5694MIPI_sensor.frame_length = OV5694MIPI_FULL_PERIOD_LINE_NUMS;    
    spin_unlock(&ov5694mipi_drv_lock);

    SENSORDB("Exit!");

    return ERROR_NONE;
}   /*  OV5694MIPIZSDPreview   */

/*************************************************************************
* FUNCTION
*   OV5694MIPICapture
*
* DESCRIPTION
*   This function setup the CMOS sensor in capture MY_OUTPUT mode
*
* PARAMETERS
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 OV5694MIPICapture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                          MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    SENSORDB("Enter!");

    if(OV5694MIPI_SENSOR_MODE_CAPTURE != OV5694MIPI_sensor.sensor_mode)
    {
        OV5694MIPI_Capture_Setting();
    }
        
    spin_lock(&ov5694mipi_drv_lock);
    OV5694MIPI_sensor.sensor_mode = OV5694MIPI_SENSOR_MODE_CAPTURE;
    OV5694MIPI_sensor.pclk = OV5694MIPI_CAPTURE_CLK;
    OV5694MIPI_sensor.video_mode = KAL_FALSE;
    OV5694MIPIAutoFlickerMode = KAL_FALSE; 
    OV5694MIPI_sensor.line_length = OV5694MIPI_FULL_PERIOD_PIXEL_NUMS;
    OV5694MIPI_sensor.frame_length = OV5694MIPI_FULL_PERIOD_LINE_NUMS;    
    spin_unlock(&ov5694mipi_drv_lock);

    SENSORDB("Exit!");
    
    return ERROR_NONE;
}   /* OV5694MIPI_Capture() */


UINT32 OV5694MIPIVideo(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    SENSORDB("Enter!");

    #ifdef VIDEO_720P
        OV5694MIPI_Video_720P_Setting();
    #elif defined VIDEO_1080P
        OV5694MIPI_Video_1080P_Setting();
    #else
        OV5694MIPI_Video_Setting();
        //OV5694MIPI_Preview_Setting();
    #endif

    spin_lock(&ov5694mipi_drv_lock);
    OV5694MIPI_sensor.sensor_mode = OV5694MIPI_SENSOR_MODE_VIDEO;
    OV5694MIPI_sensor.pclk = OV5694MIPI_VIDEO_CLK;
    OV5694MIPI_sensor.video_mode = KAL_TRUE;
    
    OV5694MIPI_sensor.line_length = OV5694MIPI_VIDEO_PERIOD_PIXEL_NUMS;
    OV5694MIPI_sensor.frame_length = OV5694MIPI_VIDEO_PERIOD_LINE_NUMS;    
    spin_unlock(&ov5694mipi_drv_lock);

    SENSORDB("Exit!");
    
    return ERROR_NONE;
}   /*  OV5694MIPIPreview   */



UINT32 OV5694MIPIGetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution)
{
#ifdef OV5694MIPI_DRIVER_TRACE
        SENSORDB("Enter");
#endif

    pSensorResolution->SensorFullWidth = OV5694MIPI_IMAGE_SENSOR_FULL_WIDTH;
    pSensorResolution->SensorFullHeight = OV5694MIPI_IMAGE_SENSOR_FULL_HEIGHT;
    
    pSensorResolution->SensorPreviewWidth = OV5694MIPI_IMAGE_SENSOR_PV_WIDTH;
    pSensorResolution->SensorPreviewHeight = OV5694MIPI_IMAGE_SENSOR_PV_HEIGHT;

    pSensorResolution->SensorVideoWidth = OV5694MIPI_IMAGE_SENSOR_VIDEO_WIDTH;
    pSensorResolution->SensorVideoHeight = OV5694MIPI_IMAGE_SENSOR_VIDEO_HEIGHT;		
    
    return ERROR_NONE;
}   /*  OV5694MIPIGetResolution  */

UINT32 OV5694MIPIGetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId,
                      MSDK_SENSOR_INFO_STRUCT *pSensorInfo,
                      MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
#ifdef OV5694MIPI_DRIVER_TRACE
    SENSORDB("ScenarioId = %d", ScenarioId);
#endif

    switch(ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_ZSD:
            pSensorInfo->SensorPreviewResolutionX = OV5694MIPI_IMAGE_SENSOR_FULL_WIDTH; /* not use */
            pSensorInfo->SensorPreviewResolutionY = OV5694MIPI_IMAGE_SENSOR_FULL_HEIGHT; /* not use */
            pSensorInfo->SensorCameraPreviewFrameRate = 15; /* not use */
            break;

        default:
            pSensorInfo->SensorPreviewResolutionX = OV5694MIPI_IMAGE_SENSOR_PV_WIDTH; /* not use */
            pSensorInfo->SensorPreviewResolutionY = OV5694MIPI_IMAGE_SENSOR_PV_HEIGHT; /* not use */
            pSensorInfo->SensorCameraPreviewFrameRate = 30; /* not use */
            break;
    }

    pSensorInfo->SensorFullResolutionX = OV5694MIPI_IMAGE_SENSOR_FULL_WIDTH; /* not use */
    pSensorInfo->SensorFullResolutionY = OV5694MIPI_IMAGE_SENSOR_FULL_HEIGHT; /* not use */

    pSensorInfo->SensorVideoFrameRate = 30; /* not use */
	pSensorInfo->SensorStillCaptureFrameRate= 30; /* not use */
	pSensorInfo->SensorWebCamCaptureFrameRate= 30; /* not use */

    pSensorInfo->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW; /* not use */
    pSensorInfo->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW; // inverse with datasheet
    pSensorInfo->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorInterruptDelayLines = 4; /* not use */
    pSensorInfo->SensorResetActiveHigh = FALSE; /* not use */
    pSensorInfo->SensorResetDelayCount = 5; /* not use */

    pSensorInfo->SensroInterfaceType = SENSOR_INTERFACE_TYPE_MIPI;

    pSensorInfo->SensorOutputDataFormat = OV5694MIPI_COLOR_FORMAT;

    pSensorInfo->CaptureDelayFrame = 2; 
    pSensorInfo->PreviewDelayFrame = 2; 
    pSensorInfo->VideoDelayFrame = 2; 

    pSensorInfo->SensorMasterClockSwitch = 0; /* not use */
    pSensorInfo->SensorDrivingCurrent = ISP_DRIVING_6MA;
    
    pSensorInfo->AEShutDelayFrame = 0;          /* The frame of setting shutter default 0 for TG int */
    pSensorInfo->AESensorGainDelayFrame = 0;    /* The frame of setting sensor gain */
    pSensorInfo->AEISPGainDelayFrame = 2;  
    
    //pSensorInfo->MIPIsensorType = MIPI_OPHY_CSI2; 

    switch (ScenarioId)
    {
	    case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	    case MSDK_SCENARIO_ID_CAMERA_ZSD:
			pSensorInfo->SensorClockFreq = 24;
			pSensorInfo->SensorClockDividCount = 3; /* not use */
			pSensorInfo->SensorClockRisingCount = 0;
			pSensorInfo->SensorClockFallingCount = 2; /* not use */
			pSensorInfo->SensorPixelClockCount = 3; /* not use */
			pSensorInfo->SensorDataLatchCount = 2; /* not use */
	        pSensorInfo->SensorGrabStartX = OV5694MIPI_FULL_START_X; 
	        pSensorInfo->SensorGrabStartY = OV5694MIPI_FULL_START_Y;

            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_2_LANE;         
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0; 
            pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 14; 
            pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorWidthSampling = 0;  // 0 is default 1x
            pSensorInfo->SensorHightSampling = 0;   // 0 is default 1x 
            pSensorInfo->SensorPacketECCOrder = 1;

	        break;
	    case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			pSensorInfo->SensorClockFreq = 24;
			pSensorInfo->SensorClockDividCount = 3; /* not use */
			pSensorInfo->SensorClockRisingCount = 0;
			pSensorInfo->SensorClockFallingCount = 2; /* not use */
			pSensorInfo->SensorPixelClockCount = 3; /* not use */
			pSensorInfo->SensorDataLatchCount = 2; /* not use */
	        pSensorInfo->SensorGrabStartX = OV5694MIPI_VIDEO_START_X; 
	        pSensorInfo->SensorGrabStartY = OV5694MIPI_VIDEO_START_Y;

            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_2_LANE;         
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0; 
            pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 14; 
            pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorWidthSampling = 0;  // 0 is default 1x
            pSensorInfo->SensorHightSampling = 0;   // 0 is default 1x 
            pSensorInfo->SensorPacketECCOrder = 1;

	        break;            
	    default:
	        pSensorInfo->SensorClockFreq = 24;
			pSensorInfo->SensorClockDividCount = 3; /* not use */
			pSensorInfo->SensorClockRisingCount = 0;
			pSensorInfo->SensorClockFallingCount = 2; /* not use */
			pSensorInfo->SensorPixelClockCount = 3; /* not use */
			pSensorInfo->SensorDataLatchCount = 2; /* not use */
	        pSensorInfo->SensorGrabStartX = OV5694MIPI_PV_START_X; 
	        pSensorInfo->SensorGrabStartY = OV5694MIPI_PV_START_Y;

            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_2_LANE;         
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0; 
            pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 14; 
            pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorWidthSampling = 0;  // 0 is default 1x
            pSensorInfo->SensorHightSampling = 0;   // 0 is default 1x 
            pSensorInfo->SensorPacketECCOrder = 1;

	        break;
    }
	
	return ERROR_NONE;
}   /*  OV5694MIPIGetInfo  */


UINT32 OV5694MIPIControl(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow,
                      MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
    SENSORDB("ScenarioId = %d", ScenarioId);

    mCurrentScenarioId =ScenarioId;
    
    switch (ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            OV5694MIPIPreview(pImageWindow, pSensorConfigData);
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            OV5694MIPIVideo(pImageWindow, pSensorConfigData);
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            OV5694MIPICapture(pImageWindow, pSensorConfigData);
            break;
        case MSDK_SCENARIO_ID_CAMERA_ZSD:
            OV5694MIPIZSDPreview(pImageWindow, pSensorConfigData);
            break;      
        default:
            SENSORDB("Error ScenarioId setting");
            return ERROR_INVALID_SCENARIO_ID;
    }

    return ERROR_NONE;
}   /* OV5694MIPIControl() */



UINT32 OV5694MIPISetVideoMode(UINT16 u2FrameRate)
{
    SENSORDB("u2FrameRate = %d ", u2FrameRate);

    // SetVideoMode Function should fix framerate
    if(u2FrameRate < 5){
        // Dynamic frame rate
        return ERROR_NONE;
    }

    spin_lock(&ov5694mipi_drv_lock);
    OV5694MIPI_sensor.video_mode = KAL_TRUE;
    spin_unlock(&ov5694mipi_drv_lock);

    if(u2FrameRate == 30){
        spin_lock(&ov5694mipi_drv_lock);
        OV5694MIPI_sensor.NightMode = KAL_FALSE;
        spin_unlock(&ov5694mipi_drv_lock);
    }else if(u2FrameRate == 15){
        spin_lock(&ov5694mipi_drv_lock);
        OV5694MIPI_sensor.NightMode = KAL_TRUE;
        spin_unlock(&ov5694mipi_drv_lock);
    }else{
        // fixed other frame rate
    }

    spin_lock(&ov5694mipi_drv_lock);
    OV5694MIPI_sensor.FixedFps = u2FrameRate;
    spin_unlock(&ov5694mipi_drv_lock);

    if((u2FrameRate == 30)&&(OV5694MIPIAutoFlickerMode == KAL_TRUE))
        u2FrameRate = 296;
    else if ((u2FrameRate == 15)&&(OV5694MIPIAutoFlickerMode == KAL_TRUE))
        u2FrameRate = 146;
    else
        u2FrameRate = 10 * u2FrameRate;
    
    OV5694MIPISetMaxFrameRate(u2FrameRate);


    return ERROR_NONE;
}

UINT32 OV5694MIPISetAutoFlickerMode(kal_bool bEnable, UINT16 u2FrameRate)
{
    SENSORDB("bEnable = %d, u2FrameRate = %d ", bEnable, u2FrameRate);

    if(bEnable){
        spin_lock(&ov5694mipi_drv_lock);
        OV5694MIPIAutoFlickerMode = KAL_TRUE;
        spin_unlock(&ov5694mipi_drv_lock);

        #if 0
        if((OV5694MIPI_sensor.FixedFps == 30)&&(OV5694MIPI_sensor.video_mode==KAL_TRUE))
            OV5694MIPISetMaxFrameRate(296);
        else if((OV5694MIPI_sensor.FixedFps == 15)&&(OV5694MIPI_sensor.video_mode==KAL_TRUE))
            OV5694MIPISetMaxFrameRate(148);
        #endif
        
    }else{ //Cancel Auto flick
        spin_lock(&ov5694mipi_drv_lock);
        OV5694MIPIAutoFlickerMode = KAL_FALSE;
        spin_unlock(&ov5694mipi_drv_lock);
        
        #if 0
        if((OV5694MIPI_sensor.FixedFps == 30)&&(OV5694MIPI_sensor.video_mode==KAL_TRUE))
            OV5694MIPISetMaxFrameRate(300);
        else if((OV5694MIPI_sensor.FixedFps == 15)&&(OV5694MIPI_sensor.video_mode==KAL_TRUE))
            OV5694MIPISetMaxFrameRate(150);
        #endif
    }

    return ERROR_NONE;
}


UINT32 OV5694MIPISetMaxFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 frameRate) 
{
    kal_uint32 pclk;
	kal_int16 dummyLine;
	kal_uint16 lineLength,frameHeight;
  
	SENSORDB("scenarioId = %d, frame rate = %d", scenarioId, frameRate);

    switch (scenarioId) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			pclk = OV5694MIPI_PREVIEW_CLK;
			lineLength = OV5694MIPI_PV_PERIOD_PIXEL_NUMS;
			frameHeight = (10 * pclk)/frameRate/lineLength;
			dummyLine = frameHeight - OV5694MIPI_PV_PERIOD_LINE_NUMS;
            if (dummyLine < 0) dummyLine = 0;
			OV5694MIPI_Set_Dummy(0, dummyLine);			
			break;			
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			pclk = OV5694MIPI_VIDEO_CLK;
			lineLength = OV5694MIPI_VIDEO_PERIOD_PIXEL_NUMS;
			frameHeight = (10 * pclk)/frameRate/lineLength;
			dummyLine = frameHeight - OV5694MIPI_VIDEO_PERIOD_LINE_NUMS;
            if (dummyLine < 0) dummyLine = 0;
			OV5694MIPI_Set_Dummy(0, dummyLine);			
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:			
			pclk = OV5694MIPI_CAPTURE_CLK;
			lineLength = OV5694MIPI_FULL_PERIOD_PIXEL_NUMS;
			frameHeight = (10 * pclk)/frameRate/lineLength;
			dummyLine = frameHeight - OV5694MIPI_FULL_PERIOD_LINE_NUMS;
            if (dummyLine < 0) dummyLine = 0;
			OV5694MIPI_Set_Dummy(0, dummyLine);			
			break;		
        case MSDK_SCENARIO_ID_CAMERA_3D_PREVIEW: //added
            break;
        case MSDK_SCENARIO_ID_CAMERA_3D_VIDEO:
			break;
        case MSDK_SCENARIO_ID_CAMERA_3D_CAPTURE: //added   
			break;		
		default:
			break;
	}	
	return ERROR_NONE;
}


UINT32 OV5694MIPIGetDefaultFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 *pframeRate) 
{
    SENSORDB("scenarioId = %d", scenarioId);

	switch (scenarioId) {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            *pframeRate = 300;
            break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
            *pframeRate = 300;
            break;		
        case MSDK_SCENARIO_ID_CAMERA_3D_PREVIEW: //added
        case MSDK_SCENARIO_ID_CAMERA_3D_VIDEO:
        case MSDK_SCENARIO_ID_CAMERA_3D_CAPTURE: //added   
            *pframeRate = 300;
			break;		
		default:
			break;
	}

	return ERROR_NONE;
}

UINT32 OV5694MIPI_SetTestPatternMode(kal_bool bEnable)
{
    SENSORDB("bEnable: %d", bEnable);

	if(bEnable) 
	{
	    spin_lock(&ov5694mipi_drv_lock);
        OV5694MIPIDuringTestPattern = KAL_TRUE;
        spin_unlock(&ov5694mipi_drv_lock);

        // 0x5E00[8]: 1 enable,  0 disable
        // 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK
        OV5694MIPI_write_cmos_sensor(0x5E00, 0x80);
	}
	else        
	{
	    spin_lock(&ov5694mipi_drv_lock);
        OV5694MIPIDuringTestPattern = KAL_FALSE;
        spin_unlock(&ov5694mipi_drv_lock);

        // 0x5E00[8]: 1 enable,  0 disable
        // 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK
        OV5694MIPI_write_cmos_sensor(0x5E00, 0x00);
	}
    
    return ERROR_NONE;
}

UINT32 OV5694MIPIFeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId,
                             UINT8 *pFeaturePara,UINT32 *pFeatureParaLen)
{
    UINT16 *pFeatureReturnPara16=(UINT16 *) pFeaturePara;
    UINT16 *pFeatureData16=(UINT16 *) pFeaturePara;
    UINT32 *pFeatureReturnPara32=(UINT32 *) pFeaturePara;
    UINT32 *pFeatureData32=(UINT32 *) pFeaturePara;
    //UINT32 OV5694MIPISensorRegNumber;
    //UINT32 i;
    //PNVRAM_SENSOR_DATA_STRUCT pSensorDefaultData=(PNVRAM_SENSOR_DATA_STRUCT) pFeaturePara;
    //MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData=(MSDK_SENSOR_CONFIG_STRUCT *) pFeaturePara;
    MSDK_SENSOR_REG_INFO_STRUCT *pSensorRegData=(MSDK_SENSOR_REG_INFO_STRUCT *) pFeaturePara;
    //MSDK_SENSOR_GROUP_INFO_STRUCT *pSensorGroupInfo=(MSDK_SENSOR_GROUP_INFO_STRUCT *) pFeaturePara;
    //MSDK_SENSOR_ITEM_INFO_STRUCT *pSensorItemInfo=(MSDK_SENSOR_ITEM_INFO_STRUCT *) pFeaturePara;
    //MSDK_SENSOR_ENG_INFO_STRUCT *pSensorEngInfo=(MSDK_SENSOR_ENG_INFO_STRUCT *) pFeaturePara;

    #ifdef OV5694MIPI_DRIVER_TRACE
        //SENSORDB("FeatureId = %d", FeatureId);
    #endif

    switch (FeatureId)
    {
        case SENSOR_FEATURE_GET_RESOLUTION:
            *pFeatureReturnPara16++=OV5694MIPI_IMAGE_SENSOR_FULL_WIDTH;
            *pFeatureReturnPara16=OV5694MIPI_IMAGE_SENSOR_FULL_HEIGHT;
            *pFeatureParaLen=4;
            break;
        case SENSOR_FEATURE_GET_PERIOD:
            switch(mCurrentScenarioId)
            {
                case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
                case MSDK_SCENARIO_ID_CAMERA_ZSD:
                    *pFeatureReturnPara16++ = OV5694MIPI_FULL_PERIOD_PIXEL_NUMS;
                    *pFeatureReturnPara16 = OV5694MIPI_sensor.frame_length;
                    *pFeatureParaLen=4;
                    break;
                case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
                    *pFeatureReturnPara16++ = OV5694MIPI_VIDEO_PERIOD_PIXEL_NUMS;
                    *pFeatureReturnPara16 = OV5694MIPI_sensor.frame_length;
                    *pFeatureParaLen=4;
                    break;
                default:
                    *pFeatureReturnPara16++ = OV5694MIPI_PV_PERIOD_PIXEL_NUMS;
                    *pFeatureReturnPara16 = OV5694MIPI_sensor.frame_length;
                    *pFeatureParaLen=4;
                    break;
            }
            break;
        case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
            switch(mCurrentScenarioId)
            {
                case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
                case MSDK_SCENARIO_ID_CAMERA_ZSD: 
                    *pFeatureReturnPara32 = OV5694MIPI_CAPTURE_CLK;
                    *pFeatureParaLen=4;
                    break;
                case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
                    *pFeatureReturnPara32 = OV5694MIPI_VIDEO_CLK;
                    *pFeatureParaLen=4;
                    break;                  
                default:
                    *pFeatureReturnPara32 = OV5694MIPI_PREVIEW_CLK;
                    *pFeatureParaLen=4;
                    break;
            }
            break;
        case SENSOR_FEATURE_SET_ESHUTTER:
            set_OV5694MIPI_shutter(*pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_NIGHTMODE:
            OV5694MIPI_night_mode((BOOL) *pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_GAIN:       
            OV5694MIPI_SetGain((UINT16) *pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_FLASHLIGHT:
            break;
        case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
            break;
        case SENSOR_FEATURE_SET_REGISTER:                       
            OV5694MIPI_write_cmos_sensor(pSensorRegData->RegAddr, pSensorRegData->RegData);
            break;
        case SENSOR_FEATURE_GET_REGISTER:
            pSensorRegData->RegData = OV5694MIPI_read_cmos_sensor(pSensorRegData->RegAddr);
            break;
        case SENSOR_FEATURE_SET_CCT_REGISTER:
            memcpy(&OV5694MIPI_sensor.eng.cct, pFeaturePara, sizeof(OV5694MIPI_sensor.eng.cct));
            break;
            break;
        case SENSOR_FEATURE_GET_CCT_REGISTER:
            if (*pFeatureParaLen >= sizeof(OV5694MIPI_sensor.eng.cct) + sizeof(kal_uint32))
            {
              *((kal_uint32 *)pFeaturePara++) = sizeof(OV5694MIPI_sensor.eng.cct);
              memcpy(pFeaturePara, &OV5694MIPI_sensor.eng.cct, sizeof(OV5694MIPI_sensor.eng.cct));
            }
            break;
        case SENSOR_FEATURE_SET_ENG_REGISTER:
            memcpy(&OV5694MIPI_sensor.eng.reg, pFeaturePara, sizeof(OV5694MIPI_sensor.eng.reg));
            break;
        case SENSOR_FEATURE_GET_ENG_REGISTER:
            if (*pFeatureParaLen >= sizeof(OV5694MIPI_sensor.eng.reg) + sizeof(kal_uint32))
            {
              *((kal_uint32 *)pFeaturePara++) = sizeof(OV5694MIPI_sensor.eng.reg);
              memcpy(pFeaturePara, &OV5694MIPI_sensor.eng.reg, sizeof(OV5694MIPI_sensor.eng.reg));
            }
        case SENSOR_FEATURE_GET_REGISTER_DEFAULT:
            ((PNVRAM_SENSOR_DATA_STRUCT)pFeaturePara)->Version = NVRAM_CAMERA_SENSOR_FILE_VERSION;
            ((PNVRAM_SENSOR_DATA_STRUCT)pFeaturePara)->SensorId = OV5694_SENSOR_ID;
            memcpy(((PNVRAM_SENSOR_DATA_STRUCT)pFeaturePara)->SensorEngReg, &OV5694MIPI_sensor.eng.reg, sizeof(OV5694MIPI_sensor.eng.reg));
            memcpy(((PNVRAM_SENSOR_DATA_STRUCT)pFeaturePara)->SensorCCTReg, &OV5694MIPI_sensor.eng.cct, sizeof(OV5694MIPI_sensor.eng.cct));
            *pFeatureParaLen = sizeof(NVRAM_SENSOR_DATA_STRUCT);
            break;
        case SENSOR_FEATURE_GET_CONFIG_PARA:
            memcpy(pFeaturePara, &OV5694MIPI_sensor.cfg_data, sizeof(OV5694MIPI_sensor.cfg_data));
            *pFeatureParaLen = sizeof(OV5694MIPI_sensor.cfg_data);
            break;
        case SENSOR_FEATURE_CAMERA_PARA_TO_SENSOR:
             OV5694MIPI_camera_para_to_sensor();
            break;
        case SENSOR_FEATURE_SENSOR_TO_CAMERA_PARA:
            OV5694MIPI_sensor_to_camera_para();
            break;                          
        case SENSOR_FEATURE_GET_GROUP_COUNT:
            OV5694MIPI_get_sensor_group_count((kal_uint32 *)pFeaturePara);
            *pFeatureParaLen = 4;
            break;    
        case SENSOR_FEATURE_GET_GROUP_INFO:
            OV5694MIPI_get_sensor_group_info((MSDK_SENSOR_GROUP_INFO_STRUCT *)pFeaturePara);
            *pFeatureParaLen = sizeof(MSDK_SENSOR_GROUP_INFO_STRUCT);
            break;
        case SENSOR_FEATURE_GET_ITEM_INFO:
            OV5694MIPI_get_sensor_item_info((MSDK_SENSOR_ITEM_INFO_STRUCT *)pFeaturePara);
            *pFeatureParaLen = sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
            break;
        case SENSOR_FEATURE_SET_ITEM_INFO:
            OV5694MIPI_set_sensor_item_info((MSDK_SENSOR_ITEM_INFO_STRUCT *)pFeaturePara);
            *pFeatureParaLen = sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
            break;
        case SENSOR_FEATURE_GET_ENG_INFO:
            memcpy(pFeaturePara, &OV5694MIPI_sensor.eng_info, sizeof(OV5694MIPI_sensor.eng_info));
            *pFeatureParaLen = sizeof(OV5694MIPI_sensor.eng_info);
            break;
        case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
            // get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE
            // if EEPROM does not exist in camera module.
            *pFeatureReturnPara32=LENS_DRIVER_ID_DO_NOT_CARE;
            *pFeatureParaLen=4;
            break;
        case SENSOR_FEATURE_SET_VIDEO_MODE:
            OV5694MIPISetVideoMode(*pFeatureData16);
            break; 
        case SENSOR_FEATURE_CHECK_SENSOR_ID:
            OV5694GetSensorID(pFeatureReturnPara32); 
            break; 
        case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
            OV5694MIPISetAutoFlickerMode((BOOL)*pFeatureData16,*(pFeatureData16+1));
            break;
		case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
			OV5694MIPISetMaxFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32, *(pFeatureData32+1));
			break;
		case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
			OV5694MIPIGetDefaultFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32, (MUINT32 *)(*(pFeatureData32+1)));
			break;
        case SENSOR_FEATURE_SET_TEST_PATTERN:
            OV5694MIPI_SetTestPatternMode((BOOL)*pFeatureData16);
            break;
        case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE: //for factory mode auto testing             
            *pFeatureReturnPara32= OV5694MIPI_TEST_PATTERN_CHECKSUM;
            *pFeatureParaLen=4;                             
            break;            
        default:
            break;
    }
  
    return ERROR_NONE;
}   /*  OV5694MIPIFeatureControl()  */

SENSOR_FUNCTION_STRUCT  SensorFuncOV5694MIPI=
{
    OV5694MIPIOpen,
    OV5694MIPIGetInfo,
    OV5694MIPIGetResolution,
    OV5694MIPIFeatureControl,
    OV5694MIPIControl,
    OV5694MIPIClose
};

UINT32 OV5694_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
    /* To Do : Check Sensor status here */
    if (pfFunc!=NULL)
        *pfFunc=&SensorFuncOV5694MIPI;

    return ERROR_NONE;
}   /*  OV5694MIPISensorInit  */




