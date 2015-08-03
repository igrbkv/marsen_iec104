#include <stddef.h>

/* Текущие данные
 *
 */

typedef struct _dsp_data {
unsigned long	dwIndex	;
unsigned long	dwBranchStartTimeS	;
unsigned long	dw3SecOffsetUs	;
unsigned long	dw3SecDurationUs	;
unsigned long	dw3SecCounter	;
unsigned char	ab200msSyncMode[16]	;
unsigned long	dwDeviceSerial	;
unsigned long	dwDeviceLastTestTimeS	;
unsigned long	dwDeviceFirmware	;
unsigned long	dwLastRtcSyncTimeS	;
signed short	iInstallationName[64];	
float	fMainsFreq;	
float	afMainsFreq10Sec[60]	;
float	afVoltageRms3SecWin[10]	;
float	afVoltageAvg3SecWin[10]	;
float	afVoltageRect3SecWin[10]	;
float	afCurrentRms3SecWin[4]	;
float	afCurrentAvgSecWin[4]	;
float	afCurrentRect3SecWin[4]	;
float	afPower[16]	;
float	afPowerCurrentDelayed[16]	;
float	afVoltageRmsPeriod[10*360]	;
float	afCurrentRmsPeriod[4*360]	;
float	afVoltageHsg3Sec[10*50]	;
float	afVoltageHsgMax3Sec[10*50]	;
float	afCurrentHsg3Sec[4*50]	;
float	afCurrentHsgMax3Sec[4*50]	;
float	afVoltageIhg3Sec[10*51]	;
float	afVoltageIhgMax3Sec[10*51]	;
float	afCurrentIhg3Sec[4*51]	;
float	afCurrentIhgMax3Sec[4*51]	;
float	afUxUBins[10*2*50]	;
float	afIxIBins[10*2*50]	;
float	afUxIBins[16*2*50]	;
float	afUSignals[10*35]	;
float	afUSignalsMax[10*35]	;
float	afISignals[4*35]	;
float	afISignalsMax[4*35]	;
float	afFlickerInstMax[10]	;
float	afFlickerSt[10]	;
} dsp_data;

typedef enum INF_EL_TYPE {
	IET_R32
} INF_EL_TYPE;

typedef struct _inf_obj_t {
	int address;
	int count;
	int offset;
	INF_EL_TYPE type;
	int group;
	int cyclic;
} inf_obj_t;

inf_obj_t avaliable_objs[] = {
	{1, 1, offsetof(struct _dsp_data, fMainsFreq), IET_R32, 1, 0},
	{2, 10, offsetof(struct _dsp_data, afVoltageRms3SecWin), IET_R32, 1, 0},
	{12, 4,  offsetof(struct _dsp_data, afCurrentRms3SecWin), IET_R32, 1, 0},
	{16, 16,  offsetof(struct _dsp_data, afPower), IET_R32, 1, 0},
	{32, 16,  offsetof(struct _dsp_data, afPowerCurrentDelayed), IET_R32, 1, 0},
};


