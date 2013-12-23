#ifndef __SAMSUNG_SYSLSI_APDEV_DISPLAY_TEST_H__
#define __SAMSUNG_SYSLSI_APDEV_DISPLAY_TEST_H__


typedef struct{
	unsigned int	phy_addr;
	unsigned int	vir_addr;
	int				frame_number;
} q_instance;


#ifdef __cplusplus
extern "C" {
#endif


int Test_Display_Optimization2(int argc, char **argv);


#ifdef __cplusplus
}
#endif


#endif /* __SAMSUNG_SYSLSI_APDEV_DISPLAY_TEST_H__ */

