#ifndef _CUST_BATTERY_METER_TABLE_H
#define _CUST_BATTERY_METER_TABLE_H

#include <mach/mt_typedefs.h>

// ============================================================
// define
// ============================================================
#define BAT_NTC_10 1
#define BAT_NTC_47 0

#if (BAT_NTC_10 == 1)
#define RBAT_PULL_UP_R             16900	
#define RBAT_PULL_DOWN_R	   27000	
#endif

#if (BAT_NTC_47 == 1)
#define RBAT_PULL_UP_R             61900	
#define RBAT_PULL_DOWN_R	  100000	
#endif
#define RBAT_PULL_UP_VOLT          1800



// ============================================================
// ENUM
// ============================================================

// ============================================================
// structure
// ============================================================

// ============================================================
// typedef
// ============================================================
typedef struct _BATTERY_PROFILE_STRUC
{
    kal_int32 percentage;
    kal_int32 voltage;
} BATTERY_PROFILE_STRUC, *BATTERY_PROFILE_STRUC_P;

typedef struct _R_PROFILE_STRUC
{
    kal_int32 resistance; // Ohm
    kal_int32 voltage;
} R_PROFILE_STRUC, *R_PROFILE_STRUC_P;

typedef enum
{
    T1_0C,
    T2_25C,
    T3_50C
} PROFILE_TEMPERATURE;

// ============================================================
// External Variables
// ============================================================

// ============================================================
// External function
// ============================================================

// ============================================================
// <DOD, Battery_Voltage> Table
// ============================================================
#if (BAT_NTC_10 == 1)
    BATT_TEMPERATURE Batt_Temperature_Table[] = {
        {-20,68237},
        {-15,53650},
        {-10,42506},
        { -5,33892},
        {  0,27219},
        {  5,22021},
        { 10,17926},
        { 15,14674},
        { 20,12081},
        { 25,10000},
        { 30,8315},
        { 35,6948},
        { 40,5834},
        { 45,4917},
        { 50,4161},
        { 55,3535},
        { 60,3014}
    };
#endif

#if (BAT_NTC_47 == 1)
    BATT_TEMPERATURE Batt_Temperature_Table[] = {
        {-20,483954},
        {-15,360850},
        {-10,271697},
        { -5,206463},
        {  0,158214},
        {  5,122259},
        { 10,95227},
        { 15,74730},
        { 20,59065},
        { 25,47000},
        { 30,37643},
        { 35,30334},
        { 40,24591},
        { 45,20048},
        { 50,16433},
        { 55,13539},
        { 60,11210}        
    };
#endif

// T0 -10C
BATTERY_PROFILE_STRUC battery_profile_t0[] =
{
	{0  , 4180},
	{1  , 4172},
	{2  , 4163},
	{3  , 4146},
	{4  , 4138},
	{5  , 4130},
	{6  , 4116},
	{7  , 4109},
	{8  , 4101},
	{9  , 4087},
	{10 , 4081},
	{11 , 4074},
	{12 , 4061},
	{13 , 4055},
	{14 , 4048},
	{15 , 4036},
	{16 , 4030},
	{17 , 4024},
	{18 , 4012},
	{19 , 4007},
	{20 , 4001},
	{21 , 3990},
	{22 , 3985},
	{23 , 3980},
	{24 , 3969},
	{25 , 3965},
	{26 , 3960},
	{27 , 3951},
	{28 , 3947},
	{29 , 3942},
	{30 , 3933},
	{31 , 3929},
	{32 , 3924},
	{33 , 3916},
	{34 , 3912},
	{35 , 3907},
	{36 , 3899},
	{37 , 3894},
	{38 , 3889},
	{39 , 3878},
	{40 , 3872},
	{41 , 3865},
	{42 , 3853},
	{43 , 3848},
	{44 , 3842},
	{45 , 3834},
	{46 , 3831},
	{47 , 3827},
	{48 , 3820},
	{49 , 3818},
	{50 , 3815},
	{51 , 3809},
	{52 , 3807},
	{53 , 3805},
	{54 , 3800},
	{55 , 3798},
	{56 , 3796},
	{57 , 3792},
	{58 , 3791},
	{59 , 3789},
	{60 , 3786},
	{61 , 3785},
	{62 , 3783},
	{63 , 3780},
	{64 , 3779},
	{65 , 3778},
	{66 , 3776},
	{67 , 3775},
	{68 , 3774},
	{69 , 3772},
	{70 , 3770},
	{71 , 3768},
	{72 , 3764},
	{73 , 3762},
	{74 , 3759},
	{75 , 3754},
	{76 , 3751},
	{77 , 3748},
	{78 , 3743},
	{79 , 3741},
	{80 , 3738},
	{81 , 3733},
	{82 , 3730},
	{83 , 3726},
	{84 , 3717},
	{85 , 3714},
	{86 , 3710},
	{87 , 3702},
	{88 , 3696},
	{89 , 3689},
	{90 , 3684},
	{91 , 3683},
	{92 , 3681},
	{93 , 3676},
	{94 , 3672},
	{95 , 3668},
	{96 , 3637},
	{97 , 3604},
	{98 , 3571},
	{99 , 3477},
	{100, 3400},
};

// T1 0C 
BATTERY_PROFILE_STRUC battery_profile_t1[] =
{
	{0  , 4180},
	{1  , 4172},
	{2  , 4163},
	{3  , 4146},
	{4  , 4138},
	{5  , 4130},
	{6  , 4116},
	{7  , 4109},
	{8  , 4101},
	{9  , 4087},
	{10 , 4081},
	{11 , 4074},
	{12 , 4061},
	{13 , 4055},
	{14 , 4048},
	{15 , 4036},
	{16 , 4030},
	{17 , 4024},
	{18 , 4012},
	{19 , 4007},
	{20 , 4001},
	{21 , 3990},
	{22 , 3985},
	{23 , 3980},
	{24 , 3969},
	{25 , 3965},
	{26 , 3960},
	{27 , 3951},
	{28 , 3947},
	{29 , 3942},
	{30 , 3933},
	{31 , 3929},
	{32 , 3924},
	{33 , 3916},
	{34 , 3912},
	{35 , 3907},
	{36 , 3899},
	{37 , 3894},
	{38 , 3889},
	{39 , 3878},
	{40 , 3872},
	{41 , 3865},
	{42 , 3853},
	{43 , 3848},
	{44 , 3842},
	{45 , 3834},
	{46 , 3831},
	{47 , 3827},
	{48 , 3820},
	{49 , 3818},
	{50 , 3815},
	{51 , 3809},
	{52 , 3807},
	{53 , 3805},
	{54 , 3800},
	{55 , 3798},
	{56 , 3796},
	{57 , 3792},
	{58 , 3791},
	{59 , 3789},
	{60 , 3786},
	{61 , 3785},
	{62 , 3783},
	{63 , 3780},
	{64 , 3779},
	{65 , 3778},
	{66 , 3776},
	{67 , 3775},
	{68 , 3774},
	{69 , 3772},
	{70 , 3770},
	{71 , 3768},
	{72 , 3764},
	{73 , 3762},
	{74 , 3759},
	{75 , 3754},
	{76 , 3751},
	{77 , 3748},
	{78 , 3743},
	{79 , 3741},
	{80 , 3738},
	{81 , 3733},
	{82 , 3730},
	{83 , 3726},
	{84 , 3717},
	{85 , 3714},
	{86 , 3710},
	{87 , 3702},
	{88 , 3696},
	{89 , 3689},
	{90 , 3684},
	{91 , 3683},
	{92 , 3681},
	{93 , 3676},
	{94 , 3672},
	{95 , 3668},
	{96 , 3637},
	{97 , 3604},
	{98 , 3571},
	{99 , 3477},
	{100, 3400},
};

// T2 25C
BATTERY_PROFILE_STRUC battery_profile_t2[] =
{
	{0  , 4180},
	{1  , 4172},
	{2  , 4163},
	{3  , 4146},
	{4  , 4138},
	{5  , 4130},
	{6  , 4116},
	{7  , 4109},
	{8  , 4101},
	{9  , 4087},
	{10 , 4081},
	{11 , 4074},
	{12 , 4061},
	{13 , 4055},
	{14 , 4048},
	{15 , 4036},
	{16 , 4030},
	{17 , 4024},
	{18 , 4012},
	{19 , 4007},
	{20 , 4001},
	{21 , 3990},
	{22 , 3985},
	{23 , 3980},
	{24 , 3969},
	{25 , 3965},
	{26 , 3960},
	{27 , 3951},
	{28 , 3947},
	{29 , 3942},
	{30 , 3933},
	{31 , 3929},
	{32 , 3924},
	{33 , 3916},
	{34 , 3912},
	{35 , 3907},
	{36 , 3899},
	{37 , 3894},
	{38 , 3889},
	{39 , 3878},
	{40 , 3872},
	{41 , 3865},
	{42 , 3853},
	{43 , 3848},
	{44 , 3842},
	{45 , 3834},
	{46 , 3831},
	{47 , 3827},
	{48 , 3820},
	{49 , 3818},
	{50 , 3815},
	{51 , 3809},
	{52 , 3807},
	{53 , 3805},
	{54 , 3800},
	{55 , 3798},
	{56 , 3796},
	{57 , 3792},
	{58 , 3791},
	{59 , 3789},
	{60 , 3786},
	{61 , 3785},
	{62 , 3783},
	{63 , 3780},
	{64 , 3779},
	{65 , 3778},
	{66 , 3776},
	{67 , 3775},
	{68 , 3774},
	{69 , 3772},
	{70 , 3770},
	{71 , 3768},
	{72 , 3764},
	{73 , 3762},
	{74 , 3759},
	{75 , 3754},
	{76 , 3751},
	{77 , 3748},
	{78 , 3743},
	{79 , 3741},
	{80 , 3738},
	{81 , 3733},
	{82 , 3730},
	{83 , 3726},
	{84 , 3717},
	{85 , 3714},
	{86 , 3710},
	{87 , 3702},
	{88 , 3696},
	{89 , 3689},
	{90 , 3684},
	{91 , 3683},
	{92 , 3681},
	{93 , 3676},
	{94 , 3672},
	{95 , 3668},
	{96 , 3637},
	{97 , 3604},
	{98 , 3571},
	{99 , 3477},
	{100, 3400},
};

// T3 50C
BATTERY_PROFILE_STRUC battery_profile_t3[] =
{
	{0  , 4180},
	{1  , 4172},
	{2  , 4163},
	{3  , 4146},
	{4  , 4138},
	{5  , 4130},
	{6  , 4116},
	{7  , 4109},
	{8  , 4101},
	{9  , 4087},
	{10 , 4081},
	{11 , 4074},
	{12 , 4061},
	{13 , 4055},
	{14 , 4048},
	{15 , 4036},
	{16 , 4030},
	{17 , 4024},
	{18 , 4012},
	{19 , 4007},
	{20 , 4001},
	{21 , 3990},
	{22 , 3985},
	{23 , 3980},
	{24 , 3969},
	{25 , 3965},
	{26 , 3960},
	{27 , 3951},
	{28 , 3947},
	{29 , 3942},
	{30 , 3933},
	{31 , 3929},
	{32 , 3924},
	{33 , 3916},
	{34 , 3912},
	{35 , 3907},
	{36 , 3899},
	{37 , 3894},
	{38 , 3889},
	{39 , 3878},
	{40 , 3872},
	{41 , 3865},
	{42 , 3853},
	{43 , 3848},
	{44 , 3842},
	{45 , 3834},
	{46 , 3831},
	{47 , 3827},
	{48 , 3820},
	{49 , 3818},
	{50 , 3815},
	{51 , 3809},
	{52 , 3807},
	{53 , 3805},
	{54 , 3800},
	{55 , 3798},
	{56 , 3796},
	{57 , 3792},
	{58 , 3791},
	{59 , 3789},
	{60 , 3786},
	{61 , 3785},
	{62 , 3783},
	{63 , 3780},
	{64 , 3779},
	{65 , 3778},
	{66 , 3776},
	{67 , 3775},
	{68 , 3774},
	{69 , 3772},
	{70 , 3770},
	{71 , 3768},
	{72 , 3764},
	{73 , 3762},
	{74 , 3759},
	{75 , 3754},
	{76 , 3751},
	{77 , 3748},
	{78 , 3743},
	{79 , 3741},
	{80 , 3738},
	{81 , 3733},
	{82 , 3730},
	{83 , 3726},
	{84 , 3717},
	{85 , 3714},
	{86 , 3710},
	{87 , 3702},
	{88 , 3696},
	{89 , 3689},
	{90 , 3684},
	{91 , 3683},
	{92 , 3681},
	{93 , 3676},
	{94 , 3672},
	{95 , 3668},
	{96 , 3637},
	{97 , 3604},
	{98 , 3571},
	{99 , 3477},
	{100, 3400},
};

// battery profile for actual temperature. The size should be the same as T1, T2 and T3
BATTERY_PROFILE_STRUC battery_profile_temperature[] =
{
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },  
	{0  , 0 }, 
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },  
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
};    

// ============================================================
// <Rbat, Battery_Voltage> Table
// ============================================================
// T0 -10C
R_PROFILE_STRUC r_profile_t0[] =
{
	{118, 4180},
	{118, 4172},
	{118, 4163},
	{118, 4146},
	{118, 4138},
	{118, 4130},
	{122, 4116},
	{121, 4109},
	{120, 4101},
	{120, 4087},
	{121, 4081},
	{122, 4074},
	{123, 4061},
	{123, 4055},
	{123, 4048},
	{123, 4036},
	{124, 4030},
	{125, 4024},
	{125, 4012},
	{125, 4007},
	{125, 4001},
	{127, 3990},
	{127, 3985},
	{128, 3980},
	{128, 3969},
	{129, 3965},
	{130, 3960},
	{132, 3951},
	{132, 3947},
	{133, 3942},
	{135, 3933},
	{135, 3929},
	{135, 3924},
	{138, 3916},
	{138, 3912},
	{138, 3907},
	{142, 3899},
	{141, 3894},
	{140, 3889},
	{138, 3878},
	{136, 3872},
	{133, 3865},
	{128, 3853},
	{126, 3848},
	{123, 3842},
	{122, 3834},
	{122, 3831},
	{122, 3827},
	{120, 3820},
	{120, 3818},
	{120, 3815},
	{120, 3809},
	{120, 3807},
	{120, 3805},
	{120, 3800},
	{120, 3798},
	{120, 3796},
	{120, 3792},
	{121, 3791},
	{122, 3789},
	{122, 3786},
	{122, 3785},
	{122, 3783},
	{122, 3780},
	{122, 3779},
	{123, 3778},
	{125, 3776},
	{125, 3775},
	{125, 3774},
	{127, 3772},
	{125, 3770},
	{123, 3768},
	{122, 3764},
	{121, 3762},
	{120, 3759},
	{122, 3754},
	{121, 3751},
	{120, 3748},
	{120, 3743},
	{120, 3741},
	{120, 3738},
	{122, 3733},
	{121, 3730},
	{120, 3726},
	{120, 3717},
	{120, 3714},
	{120, 3710},
	{123, 3702},
	{122, 3696},
	{120, 3689},
	{122, 3684},
	{122, 3683},
	{123, 3681},
	{127, 3676},
	{129, 3672},
	{132, 3668},
	{132, 3637},
	{133, 3604},
	{135, 3571},
	{145, 3477},
	{150, 3400},
};

// T1 0C
R_PROFILE_STRUC r_profile_t1[] =
{
	{118, 4180},
	{118, 4172},
	{118, 4163},
	{118, 4146},
	{118, 4138},
	{118, 4130},
	{122, 4116},
	{121, 4109},
	{120, 4101},
	{120, 4087},
	{121, 4081},
	{122, 4074},
	{123, 4061},
	{123, 4055},
	{123, 4048},
	{123, 4036},
	{124, 4030},
	{125, 4024},
	{125, 4012},
	{125, 4007},
	{125, 4001},
	{127, 3990},
	{127, 3985},
	{128, 3980},
	{128, 3969},
	{129, 3965},
	{130, 3960},
	{132, 3951},
	{132, 3947},
	{133, 3942},
	{135, 3933},
	{135, 3929},
	{135, 3924},
	{138, 3916},
	{138, 3912},
	{138, 3907},
	{142, 3899},
	{141, 3894},
	{140, 3889},
	{138, 3878},
	{136, 3872},
	{133, 3865},
	{128, 3853},
	{126, 3848},
	{123, 3842},
	{122, 3834},
	{122, 3831},
	{122, 3827},
	{120, 3820},
	{120, 3818},
	{120, 3815},
	{120, 3809},
	{120, 3807},
	{120, 3805},
	{120, 3800},
	{120, 3798},
	{120, 3796},
	{120, 3792},
	{121, 3791},
	{122, 3789},
	{122, 3786},
	{122, 3785},
	{122, 3783},
	{122, 3780},
	{122, 3779},
	{123, 3778},
	{125, 3776},
	{125, 3775},
	{125, 3774},
	{127, 3772},
	{125, 3770},
	{123, 3768},
	{122, 3764},
	{121, 3762},
	{120, 3759},
	{122, 3754},
	{121, 3751},
	{120, 3748},
	{120, 3743},
	{120, 3741},
	{120, 3738},
	{122, 3733},
	{121, 3730},
	{120, 3726},
	{120, 3717},
	{120, 3714},
	{120, 3710},
	{123, 3702},
	{122, 3696},
	{120, 3689},
	{122, 3684},
	{122, 3683},
	{123, 3681},
	{127, 3676},
	{129, 3672},
	{132, 3668},
	{132, 3637},
	{133, 3604},
	{135, 3571},
	{145, 3477},
	{150, 3400},
};

// T2 25C
R_PROFILE_STRUC r_profile_t2[] =
{
	{118, 4180},
	{118, 4172},
	{118, 4163},
	{118, 4146},
	{118, 4138},
	{118, 4130},
	{122, 4116},
	{121, 4109},
	{120, 4101},
	{120, 4087},
	{121, 4081},
	{122, 4074},
	{123, 4061},
	{123, 4055},
	{123, 4048},
	{123, 4036},
	{124, 4030},
	{125, 4024},
	{125, 4012},
	{125, 4007},
	{125, 4001},
	{127, 3990},
	{127, 3985},
	{128, 3980},
	{128, 3969},
	{129, 3965},
	{130, 3960},
	{132, 3951},
	{132, 3947},
	{133, 3942},
	{135, 3933},
	{135, 3929},
	{135, 3924},
	{138, 3916},
	{138, 3912},
	{138, 3907},
	{142, 3899},
	{141, 3894},
	{140, 3889},
	{138, 3878},
	{136, 3872},
	{133, 3865},
	{128, 3853},
	{126, 3848},
	{123, 3842},
	{122, 3834},
	{122, 3831},
	{122, 3827},
	{120, 3820},
	{120, 3818},
	{120, 3815},
	{120, 3809},
	{120, 3807},
	{120, 3805},
	{120, 3800},
	{120, 3798},
	{120, 3796},
	{120, 3792},
	{121, 3791},
	{122, 3789},
	{122, 3786},
	{122, 3785},
	{122, 3783},
	{122, 3780},
	{122, 3779},
	{123, 3778},
	{125, 3776},
	{125, 3775},
	{125, 3774},
	{127, 3772},
	{125, 3770},
	{123, 3768},
	{122, 3764},
	{121, 3762},
	{120, 3759},
	{122, 3754},
	{121, 3751},
	{120, 3748},
	{120, 3743},
	{120, 3741},
	{120, 3738},
	{122, 3733},
	{121, 3730},
	{120, 3726},
	{120, 3717},
	{120, 3714},
	{120, 3710},
	{123, 3702},
	{122, 3696},
	{120, 3689},
	{122, 3684},
	{122, 3683},
	{123, 3681},
	{127, 3676},
	{129, 3672},
	{132, 3668},
	{132, 3637},
	{133, 3604},
	{135, 3571},
	{145, 3477},
	{150, 3400},
};

// T3 50C
R_PROFILE_STRUC r_profile_t3[] =
{
	{118, 4180},
	{118, 4172},
	{118, 4163},
	{118, 4146},
	{118, 4138},
	{118, 4130},
	{122, 4116},
	{121, 4109},
	{120, 4101},
	{120, 4087},
	{121, 4081},
	{122, 4074},
	{123, 4061},
	{123, 4055},
	{123, 4048},
	{123, 4036},
	{124, 4030},
	{125, 4024},
	{125, 4012},
	{125, 4007},
	{125, 4001},
	{127, 3990},
	{127, 3985},
	{128, 3980},
	{128, 3969},
	{129, 3965},
	{130, 3960},
	{132, 3951},
	{132, 3947},
	{133, 3942},
	{135, 3933},
	{135, 3929},
	{135, 3924},
	{138, 3916},
	{138, 3912},
	{138, 3907},
	{142, 3899},
	{141, 3894},
	{140, 3889},
	{138, 3878},
	{136, 3872},
	{133, 3865},
	{128, 3853},
	{126, 3848},
	{123, 3842},
	{122, 3834},
	{122, 3831},
	{122, 3827},
	{120, 3820},
	{120, 3818},
	{120, 3815},
	{120, 3809},
	{120, 3807},
	{120, 3805},
	{120, 3800},
	{120, 3798},
	{120, 3796},
	{120, 3792},
	{121, 3791},
	{122, 3789},
	{122, 3786},
	{122, 3785},
	{122, 3783},
	{122, 3780},
	{122, 3779},
	{123, 3778},
	{125, 3776},
	{125, 3775},
	{125, 3774},
	{127, 3772},
	{125, 3770},
	{123, 3768},
	{122, 3764},
	{121, 3762},
	{120, 3759},
	{122, 3754},
	{121, 3751},
	{120, 3748},
	{120, 3743},
	{120, 3741},
	{120, 3738},
	{122, 3733},
	{121, 3730},
	{120, 3726},
	{120, 3717},
	{120, 3714},
	{120, 3710},
	{123, 3702},
	{122, 3696},
	{120, 3689},
	{122, 3684},
	{122, 3683},
	{123, 3681},
	{127, 3676},
	{129, 3672},
	{132, 3668},
	{132, 3637},
	{133, 3604},
	{135, 3571},
	{145, 3477},
	{150, 3400},
};

// r-table profile for actual temperature. The size should be the same as T1, T2 and T3
R_PROFILE_STRUC r_profile_temperature[] =
{
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },  
	{0  , 0 }, 
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },  
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
	{0  , 0 },
};    

// ============================================================
// function prototype
// ============================================================
int fgauge_get_saddles(void);
BATTERY_PROFILE_STRUC_P fgauge_get_profile(kal_uint32 temperature);

int fgauge_get_saddles_r_table(void);
R_PROFILE_STRUC_P fgauge_get_profile_r_table(kal_uint32 temperature);

#endif	//#ifndef _CUST_BATTERY_METER_TABLE_H
