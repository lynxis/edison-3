/*
 * (c) MediaTek Inc. 2010
 */


#ifndef _GC2236MIPI_SENSOR_H
#define _GC2236MIPI_SENSOR_H

#define GC2236MIPI_DEBUG
#define GC2236MIPI_DRIVER_TRACE
//#define GC2236MIPI_TEST_PATTEM
//#define SENSORDB printk
//#define SENSORDB(x,...)

#define GC2236MIPI_FACTORY_START_ADDR 0
#define GC2236MIPI_ENGINEER_START_ADDR 10
 
typedef enum GC2236MIPI_group_enum
{
  GC2236MIPI_PRE_GAIN = 0,
  GC2236MIPI_CMMCLK_CURRENT,
  GC2236MIPI_FRAME_RATE_LIMITATION,
  GC2236MIPI_REGISTER_EDITOR,
  GC2236MIPI_GROUP_TOTAL_NUMS
} GC2236MIPI_FACTORY_GROUP_ENUM;

typedef enum GC2236MIPI_register_index
{
  GC2236MIPI_SENSOR_BASEGAIN = GC2236MIPI_FACTORY_START_ADDR,
  GC2236MIPI_PRE_GAIN_R_INDEX,
  GC2236MIPI_PRE_GAIN_Gr_INDEX,
  GC2236MIPI_PRE_GAIN_Gb_INDEX,
  GC2236MIPI_PRE_GAIN_B_INDEX,
  GC2236MIPI_FACTORY_END_ADDR
} GC2236MIPI_FACTORY_REGISTER_INDEX;

typedef enum GC2236MIPI_engineer_index
{
  GC2236MIPI_CMMCLK_CURRENT_INDEX = GC2236MIPI_ENGINEER_START_ADDR,
  GC2236MIPI_ENGINEER_END
} GC2236MIPI_FACTORY_ENGINEER_INDEX;

typedef struct _sensor_data_struct
{
  SENSOR_REG_STRUCT reg[GC2236MIPI_ENGINEER_END];
  SENSOR_REG_STRUCT cct[GC2236MIPI_FACTORY_END_ADDR];
} sensor_data_struct;

#define GC2236_MIPI_2_Lane
#define GC2236MIPI_SENSOR_ID            GC2236_SENSOR_ID


/* SENSOR PREVIEW/CAPTURE VT CLOCK */
#ifdef GC2236_MIPI_2_Lane
#define GC2236MIPI_PREVIEW_CLK                   30000000
#define GC2236MIPI_CAPTURE_CLK                    30000000
#else
#define GC2236MIPI_PREVIEW_CLK                   21000000
#define GC2236MIPI_CAPTURE_CLK                    21000000
#endif
#ifdef GC2236_MIPI_2_Lane
#define GC2236MIPI_COLOR_FORMAT                    SENSOR_OUTPUT_FORMAT_RAW_B
#else
#define GC2236MIPI_COLOR_FORMAT                    SENSOR_OUTPUT_FORMAT_RAW8_B
#endif

#define GC2236MIPI_MIN_ANALOG_GAIN				1	/* 1x */
#define GC2236MIPI_MAX_ANALOG_GAIN				6	/* 6x */


/* FRAME RATE UNIT */
#define GC2236MIPI_FPS(x)                          (10 * (x))

/* SENSOR PIXEL/LINE NUMBERS IN ONE PERIOD */
#ifdef GC2236_MIPI_2_Lane
#define GC2236MIPI_FULL_PERIOD_PIXEL_NUMS          1107
#define GC2236MIPI_FULL_PERIOD_LINE_NUMS           1387

#define GC2236MIPI_VIDEO_PERIOD_PIXEL_NUMS          1107
#define GC2236MIPI_VIDEO_PERIOD_LINE_NUMS           1387

#define GC2236MIPI_PV_PERIOD_PIXEL_NUMS            1107
#define GC2236MIPI_PV_PERIOD_LINE_NUMS             1387
#else
#define GC2236MIPI_FULL_PERIOD_PIXEL_NUMS          1050
#define GC2236MIPI_FULL_PERIOD_LINE_NUMS           1258

#define GC2236MIPI_VIDEO_PERIOD_PIXEL_NUMS          1050
#define GC2236MIPI_VIDEO_PERIOD_LINE_NUMS           1258

#define GC2236MIPI_PV_PERIOD_PIXEL_NUMS            1050
#define GC2236MIPI_PV_PERIOD_LINE_NUMS             1258
#endif
/* SENSOR START/END POSITION */
#define GC2236MIPI_FULL_X_START                    8
#define GC2236MIPI_FULL_Y_START                    6
#define GC2236MIPI_IMAGE_SENSOR_FULL_WIDTH         (1600 - 16)
#define GC2236MIPI_IMAGE_SENSOR_FULL_HEIGHT        (1200 - 12)

#define GC2236MIPI_VIDEO_X_START                      8
#define GC2236MIPI_VIDEO_Y_START                      6
#define GC2236MIPI_IMAGE_SENSOR_VIDEO_WIDTH           (1600 - 16)
#define GC2236MIPI_IMAGE_SENSOR_VIDEO_HEIGHT          (1200  - 12)

#define GC2236MIPI_PV_X_START                      16
#define GC2236MIPI_PV_Y_START                      12
#define GC2236MIPI_IMAGE_SENSOR_PV_WIDTH           (1600 - 32)
#define GC2236MIPI_IMAGE_SENSOR_PV_HEIGHT          (1200  - 24)

/* SENSOR READ/WRITE ID */
#define GC2236MIPI_WRITE_ID (0x78)
#define GC2236MIPI_READ_ID  (0x79)

/* SENSOR ID */
//#define GC2236MIPI_SENSOR_ID						(0x2236)

/* SENSOR PRIVATE STRUCT */
typedef enum {
    SENSOR_MODE_INIT = 0,
    SENSOR_MODE_PREVIEW,
    SENSOR_MODE_VIDEO,
    SENSOR_MODE_CAPTURE
} GC2236MIPI_SENSOR_MODE;

typedef enum{
	GC2236MIPI_IMAGE_NORMAL = 0,
	GC2236MIPI_IMAGE_H_MIRROR,
	GC2236MIPI_IMAGE_V_MIRROR,
	GC2236MIPI_IMAGE_HV_MIRROR
}GC2236MIPI_IMAGE_MIRROR;

typedef struct GC2236MIPI_sensor_STRUCT
{
	MSDK_SENSOR_CONFIG_STRUCT cfg_data;
	sensor_data_struct eng; /* engineer mode */
	MSDK_SENSOR_ENG_INFO_STRUCT eng_info;
	GC2236MIPI_SENSOR_MODE sensorMode;
	GC2236MIPI_IMAGE_MIRROR Mirror;
	kal_bool pv_mode;
	kal_bool video_mode;
	kal_bool NightMode;
	kal_uint16 normal_fps; /* video normal mode max fps */
	kal_uint16 night_fps; /* video night mode max fps */
	kal_uint16 FixedFps;
	kal_uint16 shutter;
	kal_uint16 gain;
	kal_uint32 pclk;
	kal_uint16 frame_height;
	kal_uint16 frame_height_BackUp;
	kal_uint16 line_length;  
	kal_uint16 Prv_line_length;
} GC2236MIPI_sensor_struct;

typedef enum GC2236MIPI_GainMode_Index
{
	GC2236MIPI_Analogic_Gain = 0,
	GC2236MIPI_Digital_Gain
}GC2236MIPI_GainMode_Index;
//export functions
UINT32 GC2236MIPIOpen(void);
UINT32 GC2236MIPIControl(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 GC2236MIPIFeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId, UINT8 *pFeaturePara,UINT32 *pFeatureParaLen);
UINT32 GC2236MIPIGetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_INFO_STRUCT *pSensorInfo, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 GC2236MIPIGetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution);
UINT32 GC2236MIPIClose(void);

#define Sleep(ms) mdelay(ms)

#endif 




