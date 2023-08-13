/****************************************************************************
 * Title                 :   Codan Argus Infusyon Pomp
 * Author                :   Cemal KÖR
 * Notes                 :   ARGUSprotocolPublic.docm v5.06 (www.codanargus.com)
*****************************************************************************/

/****************************************************************************/
#ifndef DEVICE_H_
#define DEVICE_H_
/******************************************************************************/
#ifdef __cplusplus
extern "C"
{
#endif

/******************************************************************************
 * Defines
 ******************************************************************************/
/* The first byte has to be a Control-Escape CEh and the content-byte has to be modified by binary XOR 20h.*/
#define BEGIN_OF_FRAME		0xB5
#define END_OF_FRAME		0xD7
#define XOR_VALUE 			0x20

#define CONTROL_ESCAPE 		0xCE

#define DEFAULT 			0x00
#define HOST				0xF0

/*
The single devices can have any receiver-ID
(We prefer to use receiver-ID=01h for single Devices).
*/
#define RECEIVER_ID_ADDR	0x01

/*
For the sender-ID we prefer to use F0h because some
addresses are reserved and can�t be used
especially the docking station addresses
00h, 10h, 20h, 30h.
*/
#define SENDER_ID_ADDR		HOST

/*
VER current protocol version = 0, Hi-Nibble
*/
#define VER_SEQ_VERSION 	0

#define ESP_LOG_ENABLE 		0

/******************************************************************************
 * Macros
 ******************************************************************************/
#define MAIN_OPERATION		0
#define BATTERY_OPERATION 	1

#define UNSELECTED 			0
#define BARCODE 			1
#define PC_REMOTE 			2

#define RUNNING 			0
#define STOP_KVO			1
#define STOP 				2
#define ALARM 				3

#define ON 					0
#define CONFIG 				2
#define OFF 				3

/******************************************************************************
 * Typedefs
 ******************************************************************************/
typedef struct __attribute__((packed, aligned(1)))
{
	uint8_t reserved : 1;
	/*----------------*/
	/*bits_1;
		0= Mains operation
		1= Battery operation
	*/
	uint8_t bits_1 : 1;
	/*bits_2;
		00= unselected
		01= BARCODE sel.
		10= PC-REMOTE
		11= reserved
	*/
	uint8_t bits_2 : 2;
	/*bits_3
		00= running
		01= STOP&KVO
		10= STOP
		11= ALARM
	*/
	uint8_t bits_3 : 2;
	/*bits_4;
		00= on
		01= x
		10= CONFIG
		11= OFF
	*/
	uint8_t bits_4 : 2;
} Main_StateBitsTypedefStruct;

typedef enum
{
	GET_PARAMETERS_PUMP 	= 0,
	GET_STATES 				= 1,
	READ_VALUE 				= 2,
	READ_MEDICATION 		= 0x22,
}CommandsTypedefEnum;

typedef enum
{
	RTCCLOCK 			= 0x00,
	UNIQUE_HARDWARE_ID 	= 0x01,
	READ_STAFF_ID 		= 0x30,	//Not avaible for A717V
	READ_PATIENT_ID 	= 0x31,
	CUSTOMER_RECORD 	= 0x32,
	BARCODE_TEXT 		= 0x1F,	//Not avaible for A717V
}ReadValueIDCommandsTypedefEnum;

typedef enum __attribute__((packed, aligned(1)))
{
	NO_NAME		= 0x00,
	A707V 		= 0x01,
	A708V 		= 0x02,
	A717V 		= 0x03,
	A718V 		= 0x04,
	A71xV 		= 0x05,			// (generic device type for A717 and A718)
	A600S 		= 0x41,
	A606S 		= 0x42,
	A616S 		= 0x44,
	A100MSingle = 0x80,
	A100MMaster = 0x81,
	A100MSlave	= 0x82,
	A60MSingle	= 0x84,
	A60MMaster	= 0x85,
	A60MSlave	= 0x86,
	A300M		= 0x87,
	A500M		= 0x88,
	A600M		= 0x89,
}DeviceTypeEnum;

/* States details for the Get States */
typedef struct __attribute__((packed, aligned(1)))
{
	uint8_t Save : 1;
	uint8_t Therapy : 1;
	uint8_t Priming : 1;
	uint8_t Reserve : 1;
	uint8_t Ready_Strt : 1; // Not avaible for A717V
	uint8_t Bolus : 1;
	uint8_t Kvo : 1;
	uint8_t Stop_ : 1;
} RUNStatesTypedefStruct;

typedef struct __attribute__((packed, aligned(1)))
{
	uint8_t Peristatic : 1;
	uint8_t Barcode_Scanned : 1;
	uint8_t NC : 1;			 // Not avaible for A717V
	uint8_t SYR_CHANGED : 1; // Not avaible for A717V
	uint8_t DLOCK : 1;
	uint8_t FAST_START : 1;
	uint8_t REMOTE : 1;
	uint8_t BATTERY : 1;
} ModeStatesTypedefStruct;

typedef struct __attribute__((packed, aligned(1)))
{
	uint8_t BATT_DEFECT : 1; // Not avaible for A717V
	uint8_t BATT_LO : 1;
	uint8_t COMM_TO : 1;
	uint8_t NC : 1;			 // Not avaible for A717V
	uint8_t SYR_ALARM : 1;	 // Not avaible for A717V
	uint8_t TIMER_ALARM : 1; // Not avaible for A717V
	uint8_t INFUSION_CPL : 1;
	uint8_t ALARM_ : 1;
} CommonAlarmTypedefstruct;

typedef struct __attribute__((packed, aligned(1)))
{
	uint8_t PRESS_OUT : 1;
	uint8_t PRESS_IN : 1;
	uint8_t NC : 1;			   // Not avaible for A717V
	uint8_t INVALID_BAHUT : 1; // Not avaible for A717V
	uint8_t DROPS_TOO_LESS : 1;
	uint8_t DROPS_TOO_MANY : 1;
	uint8_t AIRAL : 1;
	uint8_t DOOR_ALARM : 1;
} SpecificAlarmStatesTypedefStruct;

typedef struct __attribute__((packed, aligned(1)))
{
	uint8_t SYR_NEAR_EMPTY : 1;
	uint8_t INF_NEAR_END : 1;
	uint8_t NC : 4;
	uint8_t STB_ALARM : 1;
	uint8_t BATT_NEAR_EMPTY : 1;
} PreAlarmStatesTypedefStruct;

typedef struct __attribute__((packed, aligned(1)))
{
	uint8_t PRESS_DOWN_STRT : 1;
	uint8_t PRESS_UP_STRT : 1;
	uint8_t SYR_CLUTCH : 1;
	uint8_t BAAREL : 1;
	uint8_t PHASE_ACTIVE : 1;
	uint8_t PHASE_TIVA : 1;
	uint8_t AIR : 1;
	uint8_t DOOR : 1;
} Status1TypedefStruct;

typedef struct __attribute__((packed, aligned(1)))
{
	uint8_t MASSC_ACTIVE : 1;
	uint8_t REQ_PW : 1;
	uint8_t NC : 2;
	uint8_t REQ_VOLUME : 1;
	uint8_t REQ_MEDSEL : 1;
	uint8_t REQ_THERAPY : 1;
	uint8_t ALARM_ACKN : 1;
} Status2TypedefStruct;

typedef struct
{
	uint8_t Seconds;
	uint8_t Minutes;
	uint8_t Hours;
	uint8_t Weekday;
	uint8_t Date;
	uint8_t Month;
	uint8_t Year;
} RTCClockTypedefStruct;

typedef struct
{
	DeviceTypeEnum Device;
	uint8_t SWVersionMajor;
	uint8_t SWVersionMinor;
	uint8_t SerialNumber[4];
	uint8_t SetRate[2];
	uint8_t Volume[3];
	uint8_t Med_No;
	uint8_t IVSetOrSyringe;
	uint8_t InfusionTime[3];
	uint8_t PatientWeight[2];
} GetParametersPumpTypedefStruct;

typedef struct
{
	DeviceTypeEnum Device;
	uint8_t SWVersionMajor;
	uint8_t SWVersionMinor;
	uint32_t SerialNumber;
	uint16_t SetRate;
	uint32_t Volume;
	uint8_t Med_No;
	uint8_t IVSetOrSyringe;
	uint32_t InfusionTime;
	uint16_t PatientWeight;
} PumpParametersTypedefStruct;

typedef struct
{
	uint8_t InfusedVolume[3];
	uint8_t StatesDetail[7];
	uint8_t PressureLimit[2];
	uint8_t CurrentPressure[2];
	uint8_t ActualActiveInfusionRate[2];
	uint8_t InfusedBolusVolume[3];
	uint8_t TimeofinfusedVolume[3];
	uint8_t RemainingInfusionVolume[3];
	uint8_t RemainingInfusionTime[3];
	uint8_t RemainingVolume[3];
	uint8_t RemainingTime[3];
} GetStatesTypedefStruct;

typedef struct
{
	uint32_t InfusedVolume;
	uint16_t PressureLimit;
	uint16_t CurrentPressure;
	uint16_t ActualActiveInfusionRate;
	uint32_t InfusedBolusVolume;
	uint32_t TimeofinfusedVolume;
	uint32_t RemainingInfusionVolume;
	uint32_t RemainingInfusionTime;
	uint32_t RemainingVolume;
	uint32_t RemainingTime;
} InfusionPumpStatesTypedefStruct;

typedef struct
{
	uint8_t TherapyName;
	uint8_t TherapyName_filledwith0[22];
	uint8_t Department_Name_filledwith0[22];
	uint8_t TherapyId[4];
	uint8_t MedDBPoolVersion[4];
} ReadMedicationTypedefStruct;

typedef enum
{
	STEP_IDLE 					= 0,
	STEP_GET_PARAMETERS_PUMP	= 1,
	STEP_GET_STATE				= 2,
	STEP_READ_VALUE				= 3,
	STEP_READ_MEDICATION		= 4,
	STEP_RTCCLOCK				= 5,
	STEP_UNIQUE_HARDWARE_ID		= 6,
	STEP_READ_PATIENT_ID		= 7,
	STEP_CUSTOMER_RECORD		= 8,
}RequestStepsTyedefEnum;

typedef struct
{
	/* States details for the Get States */
	Main_StateBitsTypedefStruct Main_StateBits;
	RUNStatesTypedefStruct RUNStates;
	ModeStatesTypedefStruct ModeStates;
	CommonAlarmTypedefstruct CommonAlarm;
	SpecificAlarmStatesTypedefStruct SpecificAlarm;
	PreAlarmStatesTypedefStruct PreAlarm;
	Status1TypedefStruct Status1;
	Status2TypedefStruct Status2;

	/* Codes for the Read Value - Command 02h: */
	RTCClockTypedefStruct RTCClock;
	uint8_t UniqueHardWareID[6];
	uint8_t StaffID[64];
	uint8_t PatientID[64];

	uint8_t RecordNumber;
	uint8_t MoreData;
	uint8_t CustomerRecord[55];

	uint8_t VER2_SEQ;
	uint8_t ReceiverIDAddr;
	uint8_t SenderIDAddr;
	GetParametersPumpTypedefStruct ParametersPump;
	PumpParametersTypedefStruct PumpParameter;
	GetStatesTypedefStruct GetStates;
	InfusionPumpStatesTypedefStruct InfusionPumpStates;
	ReadMedicationTypedefStruct ReadMedication;

} ARGUSTypedefStruct;

/******************************************************************************
 * Externs
 ******************************************************************************/
extern ARGUSTypedefStruct ARGUS;

/******************************************************************************
 * Function Prototypes
 ******************************************************************************/
uint8_t ParseMessage(uint8_t Data);

/******************************************************************************/
#ifdef __cplusplus
} // extern "C"
#endif

#endif /*File_H_*/

/*** END OF FILE **************************************************************/

/*************** NOTES ********************************************************
* Notes 1:
		The ARGUS Protocol is defined as a Request/Response protocol and should be used that way. The
		A100M, A60M docking station version sends response automatically all second for each plugged in
		pump! Depending of the Docking station configuration the states response is also send
		automatically!
		Please do not use the automatic response (in newer versions this will be switched off). For now
		ignore all response messages the docking is sending with receiver-ID 00h and 10h. Instead use your
		own interval to request the information�s.
* Notes 2:
		Permission for devices A71xV
		The Permission setting above will be introduced in next version.
* Notes 3:
		Communication Rules
		The device ignores frames if:
		- no correct BOF / EOF framing detected
		- content is too short or too long
		- has wrong CRC-8
		- non-matching version field
* Notes 4:
		Send Packed;
			[0] VER2/SEQ
			[1] Receiver
			[2] Sender
			[3] Length
			[4] Cmd
			[5] Data
			[6.. x-1] Data
			[x] CRC8
 ******************************************************************************/
