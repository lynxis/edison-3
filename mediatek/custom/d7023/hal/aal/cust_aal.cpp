#include "cust_aal.h"

int lcmCnt = 1;

struct CUST_AAL_ALS_DATA aALSCalData =
{
    // als
    {0, 6, 9, 17, 38, 74, 116, 342, 778, 1386, 1914, 2234},
    // lux
    {0, 136, 218, 312, 730, 1400, 2250, 4286, 5745, 9034, 11000, 12250},
};

struct CUST_AAL_LCM_DATA aLCMCalData[] =
{
    // LCM 0
    {
        {
              0.29,  0.46,  0.96,  1.99,  3.63,  5.88, 8.88, 12.83, 17.61, 23.14, 29.51,
            36.62, 44.44, 52.98, 62.18, 71.86, 82.39, 93.4, 105.0, 117.4, 129.9, 141.8,
           153.0, 164.5, 176.4, 188.8, 201.5, 214.8, 228.6, 242.7, 256.9, 266.8, 276.4
        },
    }
};

struct CUST_AAL_PARAM aAALParam[] =
{
    // LCM 0    
    { 5, 10, 10, 6, 1, 5 },
};
