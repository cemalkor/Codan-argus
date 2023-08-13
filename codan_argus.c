/******************************************************************************
 * Title                 :   Codan Argus Infusyon Pomp
 * Author                :   Cemal KÖR
 * Notes                 :   ARGUSprotocolPublic.docm v5.06 (www.codanargus.com)
 *******************************************************************************/

/******************************************************************************
 * Includes
 ******************************************************************************/
#include "codan_argus.h"

/******************************************************************************
 * Module Typedefs
 ******************************************************************************/
ARGUSTypedefStruct ARGUS;
RequestStepsTyedefEnum RequestSteps = STEP_IDLE;
extern ErrFlagStructTypedef ErrFlag;
extern SentToDataForInfusPompTypedefStruct SentToDataForInfusPomp;

/******************************************************************************
 * Function Prototypes
 ******************************************************************************/
void Device_Task(void *pvParameters);
uint8_t ParseMessage(uint8_t Data);
void SendToDevice(uint8_t *ucRxData, uint32_t len);

static uint8_t ParseData(uint8_t *Data);
static void Device_GetStates(void);
static void Device_ReadMedication(void);
static void Device_GetParametersPump(void);
// static void Device_ReadCustomerRecord(uint8_t RecordNumber);		/* Not Used */
static void Device_ReadValue(ReadValueIDCommandsTypedefEnum ReadValueIDCommands);
static uint8_t CalcCrc8(uint8_t *data, int start, int length);
static void CreateMessageAndSend(uint8_t *OptionalParameter, uint8_t Size, CommandsTypedefEnum Cmd);

/******************************************************************************
 * Module Variable Definitions
 ******************************************************************************/
static uint8_t SendPacked[BUFFER_SIZE];
static uint8_t ReceiveBuffer[BUFFER_SIZE] = {0};
static uint32_t ReceiveBufferCounter = 0;
static uint8_t parseflag = 0;
static uint8_t parse_flag = 0;

static const uint8_t cArgusCRC8_TABLE[] =
	{
		0, 94, 188, 226, 97, 63, 221, 131, 194, 156,
		126, 32, 163, 253, 31, 65, 157, 195, 33, 127,
		252, 162, 64, 30, 95, 1, 227, 189, 62, 96,
		130, 220, 35, 125, 159, 193, 66, 28, 254, 160,
		225, 191, 93, 3, 128, 222, 60, 98, 190, 224,
		2, 92, 223, 129, 99, 61, 124, 34, 192, 158,
		29, 67, 161, 255, 70, 24, 250, 164, 39, 121,
		155, 197, 132, 218, 56, 102, 229, 187, 89, 7,
		219, 133, 103, 57, 186, 228, 6, 88, 25, 71,
		165, 251, 120, 38, 196, 154, 101, 59, 217, 135,
		4, 90, 184, 230, 167, 249, 27, 69, 198, 152,
		122, 36, 248, 166, 68, 26, 153, 199, 37, 123,
		58, 100, 134, 216, 91, 5, 231, 185, 140, 210,
		48, 110, 237, 179, 81, 15, 78, 16, 242, 172,
		47, 113, 147, 205, 17, 79, 173, 243, 112, 46,
		204, 146, 211, 141, 111, 49, 178, 236, 14, 80,
		175, 241, 19, 77, 206, 144, 114, 44, 109, 51,
		209, 143, 12, 82, 176, 238, 50, 108, 142, 208,
		83, 13, 239, 177, 240, 174, 76, 18, 145, 207,
		45, 115, 202, 148, 118, 40, 171, 245, 23, 73,
		8, 86, 180, 234, 105, 55, 213, 139, 87, 9,
		235, 181, 54, 104, 138, 212, 149, 203, 41, 119,
		244, 170, 72, 22, 233, 183, 85, 11, 136, 214,
		52, 106, 43, 117, 151, 201, 74, 20, 246, 168,
		116, 42, 200, 150, 21, 75, 169, 247, 182, 232,
		10, 84, 215, 137, 107, 53};

/******************************************************************************
 * Module Tasks
 ******************************************************************************/
void Device_Task(void *pvParameters)
{ /* Sorgu Yapılması Gerektiğinde Kullanılan Task */

	// vTaskSuspend(NULL);
	while (1)
	{
		switch (RequestSteps)
		{
		case STEP_IDLE:
			RequestSteps = STEP_GET_PARAMETERS_PUMP;

			break;

		case STEP_GET_PARAMETERS_PUMP:
			Device_GetParametersPump();
			RequestSteps = STEP_GET_STATE;

			break;

		case STEP_GET_STATE:
			Device_GetStates();
			RequestSteps = STEP_GET_PARAMETERS_PUMP;

			break;

		case STEP_READ_VALUE:

			break;

		case STEP_READ_MEDICATION:
			Device_ReadMedication();
			RequestSteps = STEP_RTCCLOCK;
			break;

		case STEP_RTCCLOCK:
			Device_ReadValue(RTCCLOCK);
			RequestSteps = STEP_UNIQUE_HARDWARE_ID;
			break;

		case STEP_UNIQUE_HARDWARE_ID:
			Device_ReadValue(UNIQUE_HARDWARE_ID);
			RequestSteps = STEP_READ_PATIENT_ID;
			break;

		case STEP_READ_PATIENT_ID:
			Device_ReadValue(READ_PATIENT_ID);
			RequestSteps = STEP_CUSTOMER_RECORD;
			break;

		case STEP_CUSTOMER_RECORD:
			Device_ReadValue(CUSTOMER_RECORD);
			RequestSteps = STEP_GET_PARAMETERS_PUMP;
			break;
		}
		vTaskDelay(3000 / portTICK_RATE_MS);
	}
}

/******************************************************************************
 * Function Definitions
 ******************************************************************************/
uint8_t ParseMessage(uint8_t Data)
{/* Mesaj Paketini Parse Etme Fonksiyonu */

	parseflag = 0;
	ErrFlag.errorflag = 0;
	ReceiveBuffer[ReceiveBufferCounter] = Data;

	if (ReceiveBuffer[0] == BEGIN_OF_FRAME)
	{
		if (ReceiveBuffer[4] > 100) // Mesaj boyutu sınırı aşarsa sıfırla
		{
			/* Wrong Message */
			memset(ReceiveBuffer, 0, sizeof(ReceiveBuffer));
			ReceiveBufferCounter = 0;
			ErrFlag.errorflag = 1;

			return parseflag;
		}
		if (ReceiveBuffer[4] == (ReceiveBufferCounter - 1) && ReceiveBuffer[4] != 0)
		{

			if (ReceiveBuffer[ReceiveBufferCounter] == END_OF_FRAME)
			{

				if (CalcCrc8(&ReceiveBuffer[1], 0, (ReceiveBuffer[4] - 1)) == ReceiveBuffer[ReceiveBufferCounter - 1])
				{
					parseflag = ParseData(&ReceiveBuffer[1]);

					/* Correct Message and clear buffer */
					memset(ReceiveBuffer, 0, sizeof(ReceiveBuffer));
					ReceiveBufferCounter = 0;
					return parseflag;
				}
				else
				{
					/* Wrong Message */
					memset(ReceiveBuffer, 0, sizeof(ReceiveBuffer));
					ReceiveBufferCounter = 0;
					ErrFlag.errorflag = 1;

					return parseflag;
				}
			}
			else
			{
				/* Wrong Message */
				memset(ReceiveBuffer, 0, sizeof(ReceiveBuffer));
				ReceiveBufferCounter = 0;
				ErrFlag.errorflag = 1;

				return parseflag;
			}
		}
		ReceiveBufferCounter++;
	}
	return parseflag;
}

void SendToDevice(uint8_t *ucRxData, uint32_t len)
{ /* Cihaza Veri Gönderme Fonksiyonu */

	xStreamBufferSend(xTxStreamBuffer,
					  (void *)ucRxData,
					  len,
					  portMAX_DELAY);
}

static uint8_t ParseData(uint8_t *Data)
{ /* Mesaj Paketindeki Parametre Verilerinin Parse Edilme Fonksiyonu */

	parse_flag = 0;

	/* Cihazın normalde verdiği parametre birimleri (CİHAZIN VERMEDİĞİ PARAMETRELERİ TANIMLAMAYINIZ!!!) */
	sprintf(SentToDataForInfusPomp.Flowrate_UNIT, "ml/s");
	sprintf(SentToDataForInfusPomp.TotalVolume_UNIT, "ml");
	sprintf(SentToDataForInfusPomp.SendVolume_UNIT, "ml");
	sprintf(SentToDataForInfusPomp.RemainingVolume_UNIT, "ml");
	sprintf(SentToDataForInfusPomp.TimeofinfusedVolume_UNIT, "s");
	sprintf(SentToDataForInfusPomp.RemainingInfusionTime_UNIT, "s");

	ARGUS.VER2_SEQ = Data[0];
	ARGUS.ReceiverIDAddr = Data[1];
	ARGUS.SenderIDAddr = Data[2];

	memcpy(&ARGUS.Main_StateBits, &Data[5], 1);

	switch ((CommandsTypedefEnum)Data[4])
	{
	case GET_PARAMETERS_PUMP:

		memcpy(&ARGUS.ParametersPump, &Data[6], 19); // 19 byte veri geldiği için

		ARGUS.PumpParameter.Device = ARGUS.ParametersPump.Device;
		ARGUS.PumpParameter.SWVersionMajor = ARGUS.ParametersPump.SWVersionMajor;
		ARGUS.PumpParameter.SWVersionMinor = ARGUS.ParametersPump.SWVersionMinor;
		SentToDataForInfusPomp.Flowrate = (((ARGUS.ParametersPump.SetRate[0] << 8) + ARGUS.ParametersPump.SetRate[1]) / 10.0);
		ARGUS.PumpParameter.SerialNumber = (ARGUS.ParametersPump.SerialNumber[0] << 24) + (ARGUS.ParametersPump.SerialNumber[1] << 16) + (ARGUS.ParametersPump.SerialNumber[2] << 8) + ARGUS.ParametersPump.SerialNumber[3];
		SentToDataForInfusPomp.TotalVolume = ((ARGUS.ParametersPump.Volume[0] << 16) + (ARGUS.ParametersPump.Volume[1] << 8) + (ARGUS.ParametersPump.Volume[2])) / 10.0;
		ARGUS.PumpParameter.IVSetOrSyringe = ARGUS.ParametersPump.IVSetOrSyringe;
		ARGUS.PumpParameter.InfusionTime = (ARGUS.ParametersPump.InfusionTime[0] << 16) + (ARGUS.ParametersPump.InfusionTime[1] << 8) + (ARGUS.ParametersPump.InfusionTime[2]);
		ARGUS.PumpParameter.PatientWeight = (ARGUS.ParametersPump.PatientWeight[0] << 8) + (ARGUS.ParametersPump.PatientWeight[1]);

		switch (Data[18])/*Medicine NO*/
		{
		case 0:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "*");
			break;
		case 1:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Actilyse");
			break;
		case 2:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Adrenaline0.1");
			break;
		case 3:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Adrenaline0.2");
			break;
		case 4:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Ajmalin");
			break;
		case 5:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Alfentanil");
			break;
		case 6:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Alupent");
			break;
		case 7:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Ambroxol");
			break;
		case 8:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Amiodaron");
			break;
		case 9:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Amphotericine");
			break;
		case 10:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Aprotinin");
			break;
		case 11:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Atracurium");
			break;
		case 12:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Bretylium");
			break;
		case 13:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Bupivacne");
			break;
		case 14:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Ceruletid");
			break;
		case 15:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Clonidin");
			break;
		case 16:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Diltiazem");
			break;
		case 17:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Dobutamin");
			break;
		case 18:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Dopamine");
			break;
		case 19:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Dopexamine");
			break;
		case 20:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Esmolol");
			break;
		case 21:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Fentanyl");
			break;
		case 22:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Flecainide");
			break;
		case 23:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Fluimucil");
			break;
		case 24:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Flumazen");
			break;
		case 25:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Furosemid");
			break;
		case 26:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Glucose30");
			break;
		case 27:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Glucose5");
			break;
		case 28:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Heparin");
			break;
		case 29:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Hydrocor");
			break;
		case 30:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Insulin");
			break;
		case 31:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Isoprena");
			break;
		case 32:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "KCL");
			break;
		case 33:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Ketamin");
			break;
		case 34:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Labetalol");
			break;
		case 35:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Lidocain");
			break;
		case 36:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Liothyronin");
			break;
		case 37:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Magnesium");
			break;
		case 38:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Midazolam");
			break;
		case 39:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Milrinone");
			break;
		case 40:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Morphin");
			break;
		case 41:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Nacl0.9");
			break;
		case 42:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Nalbuphin");
			break;
		case 43:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Naloxone");
			break;
		case 44:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Neostigm");
			break;
		case 45:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Nicardipine");
			break;
		case 46:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Nifedipin");
			break;
		case 47:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Nimodipin");
			break;
		case 48:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Nitroprussiate");
			break;
		case 49:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Noradrenalin");
			break;
		case 50:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Omeprazole");
			break;
		case 51:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Pancuron");
			break;
		case 52:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Pentoxityllin");
			break;
		case 53:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Phentolamine");
			break;
		case 54:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Phenylephrin");
			break;
		case 55:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Procainamide");
			break;
		case 56:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Propafenon");
			break;
		case 57:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Propofol");
			break;
		case 58:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Rapilysin");
			break;
		case 59:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Remifentanyl");
			break;
		case 60:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Risordan");
			break;
		case 61:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Ropivacane");
			break;
		case 62:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Salbutamol");
			break;
		case 63:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Somatostatin");
			break;
		case 64:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Streptokinase");
			break;
		case 65:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Sufentanil");
			break;
		case 66:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Terbutaline");
			break;
		case 67:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Theopyllin");
			break;
		case 68:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Thiopent");
			break;
		case 69:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Tirofiban");
			break;
		case 70:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Trinitri");
			break;
		case 71:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Urapidil");
			break;
		case 72:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Urokinase");
			break;
		case 73:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Vasopressine");
			break;
		case 74:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Vecuronium");
			break;
		case 75:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Verapamil");
			break;
		case 91:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "UserM 1");
			break;
		case 92:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "UserM 2");
			break;
		case 93:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "UserM 3");
			break;
		case 94:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "UserM 4");
			break;
		case 95:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "UserM 5");
			break;
		case 96:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "UserM 6");
			break;
		case 97:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "UserM 7");
			break;
		case 98:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "UserM 8");
			break;
		case 99:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "UserM 9");
			break;
		case 100:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "UserM 10");
			break;
		case 101:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "UserM 11");
			break;
		case 102:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "UserM 12");
			break;
		case 103:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "UserM 13");
			break;
		case 104:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "UserM 14");
			break;
		case 105:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "UserM 15");
			break;
		case 106:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "UserM 16");
			break;
		case 107:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "UserM 17");
			break;
		case 108:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "UserM 18");
			break;
		case 109:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "UserM 19");
			break;
		case 110:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "UserM 20");
			break;
		case 111:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "UserM 21");
			break;
		case 112:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "UserM 22");
			break;
		case 113:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "UserM 23");
			break;
		case 114:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "UserM 24");
			break;
		case 115:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "UserM 25");
			break;
		case 116:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "UserM 26");
			break;
		case 117:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "UserM 27");
			break;
		case 118:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "UserM 28");
			break;
		case 119:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "UserM 29");
			break;
		case 120:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "UserM 30");
			break;
		case 121:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "UserM 31");
			break;
		case 122:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "UserM 32");
			break;
		case 123:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Blood");
			break;
		case 124:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Nacl0.45");
			break;
		case 125:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Glucose10");
			break;
		case 126:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Mannitol");
			break;
		case 127:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Glucose");
			break;
		case 128:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Hartmann");
			break;
		case 129:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "T.P.N");
			break;
		case 130:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Immunoglobulin");
			break;
		case 131:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Cytarabine");
			break;
		case 132:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Methotrexate");
			break;
		case 133:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Fludarabine");
			break;
		case 134:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Cladribine");
			break;
		case 135:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Hydroxycarbamide");
			break;
		case 136:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Raltitrexed");
			break;
		case 137:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Gemcitabine");
			break;
		case 138:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Capecitabine");
			break;
		case 139:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Tegafur");
			break;
		case 140:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Alimta");
			break;
		case 141:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Adriamycine");
			break;
		case 142:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Epirubicine");
			break;
		case 143:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Pirarubicine");
			break;
		case 144:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Mitoxantrone");
			break;
		case 145:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Dactinom");
			break;
		case 146:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Daunorub");
			break;
		case 147:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Doxorubicin");
			break;
		case 148:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Ecteinascidine");
			break;
		case 149:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Oxaliplatine");
			break;
		case 150:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Cisplatine");
			break;
		case 151:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Carboplatin");
			break;
		case 152:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Carmustine");
			break;
		case 153:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Chlormethine");
			break;
		case 154:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Dacarbazine");
			break;
		case 155:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Cyclophosphamide");
			break;
		case 156:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Thiotepa");
			break;
		case 157:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Lomustine");
			break;
		case 158:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Fotemustine");
			break;
		case 160:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Mitomycine");
			break;
		case 161:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Streptozocine");
			break;
		case 162:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Procarbazine");
			break;
		case 163:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Temozolomide");
			break;
		case 164:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Paclitaxel");
			break;
		case 165:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Docetaxel");
			break;
		case 166:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Vindesine");
			break;
		case 167:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Vincristine");
			break;
		case 168:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Vinblastine");
			break;
		case 169:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Vinorelbine");
			break;
		case 170:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Etoposide");
			break;
		case 171:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Irinotecan");
			break;
		case 172:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Topotecan");
			break;
		case 173:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Mitoguazone");
			break;
		case 174:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "L-asparaginase");
			break;
		case 175:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Bleomycine");
			break;
		case 176:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Plicamycin");
			break;
		case 177:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Rituximab");
			break;
		case 178:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Trastuzumab");
			break;
		case 179:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Edrecolomab");
			break;
		case 180:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Alizapride");
			break;
		case 181:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Granisetron");
			break;
		case 182:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Metoclopramide");
			break;
		case 183:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Ondansetron");
			break;
		case 184:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Metopimazine");
			break;
		case 185:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Str36000");
			break;
		case 186:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Stre6000");
			break;
		case 187:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Stre3000");
			break;
		case 188:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Stre1000");
			break;
		case 189:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Doxapramhcl");
			break;
		case 190:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Droperidol");
			break;
		case 191:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Methohexitalsodium");
			break;
		case 192:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Mivacuriumchloride");
			break;
		case 193:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Rocuronium-bromide");
			break;
		case 194:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Succinylcholine-chloride");
			break;
		case 195:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Cefazolinsodium");
			break;
		case 196:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Alteplase");
			break;
		case 200:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Fluorouracil");
			break;
		case 203:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Aminophylline");
			break;
		case 204:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Amrinonelactate");
			break;
		case 205:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Methyldopatehcl");
			break;
		case 206:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Nitroclycerin");
			break;
		case 207:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Prostaglandin-e1");
			break;
		case 208:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Tolazolin-hcl");
			break;
		case 209:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Trimethaphan-camsylate");
			break;
		case 210:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Oxytocin");
			break;
		case 211:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Ritodrine-hcl");
			break;
		case 212:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "IsoproterenolHCL");
			break;
		case 213:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Metaraminolbitartrate");
			break;
		case 214:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Norepinephrinebitartrate");
			break;
		case 215:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Antibiotic");
			break;
		case 216:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Arterialline");
			break;
		case 217:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Centralline");
			break;
		case 218:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Epidural");
			break;
		case 219:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Lipids");
			break;
		case 220:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Maintenanceline");
			break;
		case 221:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "U.A.C");
			break;
		case 222:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "U.V.C");
			break;
		case 223:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Clomethiazol");
			break;
		case 224:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Omipressin");
			break;
		case 225:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Nutriflex");
			break;
		case 226:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Glycerin");
			break;
		case 227:/*Medicine NO*/
			sprintf(SentToDataForInfusPomp.MedicineName, "Actrapid");
			break;

		default:
			break;
		}

		parse_flag = 1;
		if (ARGUS.ParametersPump.Device != A717V || ARGUS.ParametersPump.Device != A71xV)
		{
			/* Wrong Device */
		}
		break;

	case GET_STATES:

		if (ARGUS.ParametersPump.SWVersionMajor >= 5)
		{
			if (ARGUS.ParametersPump.SWVersionMinor >= 6)
			{
				memcpy(&ARGUS.GetStates, &Data[6], 34); // 34 byte veri geldiği için

				SentToDataForInfusPomp.SendVolume = ((ARGUS.GetStates.InfusedVolume[0] << 16) + (ARGUS.GetStates.InfusedVolume[1] << 8) + ARGUS.GetStates.InfusedVolume[2]) / 10.0;
				ARGUS.InfusionPumpStates.PressureLimit = (ARGUS.GetStates.PressureLimit[0] << 8) + ARGUS.GetStates.PressureLimit[1];
				ARGUS.InfusionPumpStates.CurrentPressure = (ARGUS.GetStates.CurrentPressure[0] << 8) + ARGUS.GetStates.CurrentPressure[1];
				ARGUS.InfusionPumpStates.ActualActiveInfusionRate = (ARGUS.GetStates.ActualActiveInfusionRate[0] << 8) + ARGUS.GetStates.ActualActiveInfusionRate[1];
				ARGUS.InfusionPumpStates.InfusedBolusVolume = (ARGUS.GetStates.InfusedBolusVolume[0] << 8) + (ARGUS.GetStates.InfusedBolusVolume[1] << 8) + ARGUS.GetStates.InfusedBolusVolume[2];
				SentToDataForInfusPomp.TimeofinfusedVolume = ((ARGUS.GetStates.TimeofinfusedVolume[0] << 16) + (ARGUS.GetStates.TimeofinfusedVolume[1] << 8) + ARGUS.GetStates.TimeofinfusedVolume[2]) / 10.0;
				SentToDataForInfusPomp.RemainingVolume = ((ARGUS.GetStates.RemainingInfusionVolume[0] << 16) + (ARGUS.GetStates.RemainingInfusionVolume[1] << 8) + ARGUS.GetStates.RemainingInfusionVolume[2]) / 10.0;
				SentToDataForInfusPomp.RemainingInfusionTime = ((ARGUS.GetStates.RemainingInfusionTime[0] << 16) + (ARGUS.GetStates.RemainingInfusionTime[1] << 8) + ARGUS.GetStates.RemainingInfusionTime[2]) / 10.0;
				ARGUS.InfusionPumpStates.RemainingVolume = (ARGUS.GetStates.RemainingVolume[0] << 8) + (ARGUS.GetStates.RemainingVolume[1] << 8) + ARGUS.GetStates.RemainingVolume[2];
				ARGUS.InfusionPumpStates.RemainingTime = (ARGUS.GetStates.RemainingTime[0] << 8) + (ARGUS.GetStates.RemainingTime[1] << 8) + ARGUS.GetStates.RemainingTime[2];
				SentToDataForInfusPomp.Alarm = (ARGUS.GetStates.StatesDetail[2] << 24) + (ARGUS.GetStates.StatesDetail[3] << 16) + (ARGUS.GetStates.StatesDetail[4] << 8) + (ARGUS.GetStates.StatesDetail[6]);
				SentToDataForInfusPomp.InfusMode = (ARGUS.GetStates.StatesDetail[0] << 8) + ARGUS.GetStates.StatesDetail[1];

				parse_flag = 1;
			}
			else if (ARGUS.ParametersPump.SWVersionMinor == 5)
			{
				memcpy(&ARGUS.GetStates, &Data[6], 14); // 14 byte veri geldiği için
			}
			else
			{
				/* Nothing */
			}
		}
		break;

	case READ_VALUE:

		switch ((ReadValueIDCommandsTypedefEnum)Data[6])
		{
		case RTCCLOCK:
			memcpy(&ARGUS.RTCClock, &Data[7], 7);
			break;
		case UNIQUE_HARDWARE_ID:
			memcpy(&ARGUS.UniqueHardWareID[0], &Data[7], 6);
			break;
		case READ_STAFF_ID:
			memcpy(&ARGUS.StaffID[0], &Data[7], (Data[3] - 7));
			break;
		case READ_PATIENT_ID:
			memcpy(&ARGUS.PatientID[0], &Data[7], (Data[3] - 7));
			break;
		case CUSTOMER_RECORD:
			ARGUS.RecordNumber = Data[7];
			ARGUS.MoreData = Data[8];
			memcpy(&ARGUS.CustomerRecord, &Data[9], (Data[3] - 9));
			break;
		case BARCODE_TEXT:
			break;
		default:
			break;
		}
		break;

	case READ_MEDICATION:

		if (ARGUS.ParametersPump.SWVersionMajor >= 5)
		{
			if (ARGUS.ParametersPump.SWVersionMinor >= 6)
			{
				memcpy(&ARGUS.ReadMedication, &Data[6], 53);
			}
			else if (ARGUS.ParametersPump.SWVersionMinor == 5)
			{
				memcpy(&ARGUS.ReadMedication, &Data[6], 45);
			}
			else
			{
				/* Nothing */
			}
		}
		break;

	default:
		break;
	}

	return parse_flag;
}

static void Device_GetStates()
{ /* GetStates Parametrelerinin İstendiği Fonksiyon */

	static uint8_t data;
	data = 0;
	
	CreateMessageAndSend(&data, 0, GET_STATES);
}

static void Device_ReadMedication()
{ /* ReadMedication Parametrelerinin İstendiği Fonksiyon */

	static uint8_t data;
	data = 0; // Therapy Data
	
	CreateMessageAndSend(&data, 1, READ_MEDICATION);
}

static void Device_GetParametersPump()
{ /* GetParameters Parametrelerinin İstendiği Fonksiyon */

	static uint8_t data;
	data = 0;
	
	CreateMessageAndSend(&data, 0, GET_PARAMETERS_PUMP);
}

/* static void Device_ReadCustomerRecord(uint8_t RecordNumber)		// Not Used
{ // ReadCustomerRecord Parametrelerinin İstendiği Fonksiyon

	static uint8_t data[2];
	data[0] = CUSTOMER_RECORD;
	data[1] = RecordNumber;
	
	CreateMessageAndSend(data, 2, READ_VALUE);
}
*/

static void Device_ReadValue(ReadValueIDCommandsTypedefEnum ReadValueIDCommands)
{ /* ReadValue Parametrelerinin İstendiği Fonksiyon */

	static uint8_t data;
	data = (uint8_t)ReadValueIDCommands;

	CreateMessageAndSend(&data, 1, READ_VALUE);
}

static void CreateMessageAndSend(uint8_t *OptionalParameter, uint8_t Size, CommandsTypedefEnum Cmd)
{/* Request Yapılacagı Zaman Cihaza Gönderilecek Mesaj Paketinin Oluşturulma Fonksiyonu */

	/*Length, incl. VER/SEQ...CRC8 (max 64 Bytes)*/
	if (Size > 56)
		return;
	/*
		The first byte has to be a Control-Escape CEh and the content-byte
		has to be modified by binary XOR 20h.
	*/
	/*--- BOF ---	*/

	SendPacked[0] = BEGIN_OF_FRAME;
	/*--- EOF ---*/
	SendPacked[Size + 7] = END_OF_FRAME;
	/*--- HEADER ---
	[0] 			VER/SEQ
	[1] 			Receiver
	[2] 			Sender
	[3] 			Length
	[4] 			Cmd
	*/
	SendPacked[1] = VER_SEQ_VERSION;
	SendPacked[2] = ARGUS.ReceiverIDAddr;
	SendPacked[3] = ARGUS.SenderIDAddr;
	SendPacked[4] = Size + 6;
	SendPacked[5] = (uint8_t)Cmd;
	/*--- DATA ----*/
	/*
	[5] 			Data
	[6.. x-1] 		Data
	*/
	if (Size > 0)
	{
		uint8_t i;
		for (i = 0; i < Size; i++)
			SendPacked[i + 6] = OptionalParameter[i];
	}
	/*--- CRC8 ---
	[x]	 			CRC8*/
	SendPacked[Size + 6] = CalcCrc8(&SendPacked[1], 0, (Size + 5));

	/* NOT: Sorgu yapilacagi zaman asagidaki fonksiyon acilacak */
	// SendToDevice(SendPacked, SendPacked[4]);
}

static uint8_t CalcCrc8(uint8_t *data, int start, int length)
{ /* CRC8 Hesaplamasının Fonksiyonu */
	
	uint8_t crc = 0xff;
	for (int i = start; i < start + length; i++)
	{
		crc ^= data[i];
		crc = cArgusCRC8_TABLE[crc];
	}
	return crc;
}

/*************** END OF FILE **************************************************/

/*************** NOTES ********************************************************
 * 1-
 ******************************************************************************/
