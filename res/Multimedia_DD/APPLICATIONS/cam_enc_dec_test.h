#ifndef __SAMSUNG_SYSLSI_APDEV_CAM_ENC_DEC_TEST_H__
#define __SAMSUNG_SYSLSI_APDEV_CAM_ENC_DEC_TEST_H__


typedef struct{
	unsigned int	vir_addr;
	int				frame_number;
	int				size;
} q_instance;


#ifdef __cplusplus
extern "C" {
#endif


int Forlinx_Test_Cam_Enc_Dec(int argc, char **argv, int lcdnum);


#ifdef __cplusplus
}
#endif


#endif /* __SAMSUNG_SYSLSI_APDEV_CAM_ENC_DEC_TEST_H__ */

