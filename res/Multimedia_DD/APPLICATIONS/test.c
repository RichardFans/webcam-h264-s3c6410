#include <stdio.h>
#include <stdlib.h>

#include "display_test.h"
#include "display_4_windows.h"
#include "display_optimization1.h"
#include "display_optimization2.h"
#include "cam_encoder_test.h"
#include "cam_enc_dec_test.h"
#include "cam_dec_preview.h"
#include "capture.h"
#include "jpeg_display.h"


static void print_menu(void);
static int getlcdsize();
int main(int argc, char **argv)
{
	int	num;

	while(1)
	{
		system("clear");
		print_menu();
		
		scanf("%d", &num);
		fflush(stdin);

		
		switch(num)
		{
			case 1:
				
				Forlinx_Test_Display_H264(argc, argv, getlcdsize());
				break;
			case 2:
				Forlinx_Test_Display_MPEG4(argc, argv, getlcdsize());
				break;
			case 3:
				Forlinx_Test_Display_H263(argc, argv, getlcdsize());
				break;
			case 4:
				Forlinx_Test_Display_VC1(argc, argv, getlcdsize());
				break;
			case 5:
				Forlinx_Test_Display_4_Windows(argc, argv, getlcdsize());
				break;
			case 6:
				Forlinx_Test_Display_Optimization1(argc, argv, getlcdsize());
				break;
			case 7:
				Forlinx_Test_Display_Optimization2(argc, argv, getlcdsize());
				break;
			case 8:
				Forlinx_Test_Cam_Encoding(argc, argv, getlcdsize());
                                break;
			case 9:
				Forlinx_Test_Cam_Dec_Preview(argc, argv, getlcdsize());
				break;
			case 10:
				Forlinx_Test_Cam_Enc_Dec(argc, argv, getlcdsize());
				break;
			case 11:
				Forlinx_Test_Capture(argc, argv, getlcdsize());
				break;
			case 12:
				Forlinx_Test_Jpeg_Display(argc, argv, getlcdsize());
				break;
			case 13:
				exit(0);
				break;
			default:
				printf("Number is wrong\n");
				exit(0);
				break;
		}
	}


	return 0;
}

static void print_menu(void)
{
	printf("========= S3C6410 Media Demo Application =========\n");
        printf("=  Modify by Forlinx Embedded 2012-01-06               =\n");
	printf("=                                                =\n");
	printf("=  1.   H.264 display                            =\n");
	printf("=  2.   MPEG4 display                            =\n");
	printf("=  3.   H.263 display                            =\n");
	printf("=  4.   VC-1  display                            =\n");
	printf("=  5.   4-windows display                        =\n");
	printf("=  6.   Display using local path                 =\n");
	printf("=  7.   Display using double buffering           =\n");
	printf("=  8.   Camera preview & MFC encoding            =\n");
	printf("=  9.   MFC decoding & Camera preview            =\n");
	printf("=  10.  Camera preview & MFC encoding/decoding   =\n");
	printf("=  11.  Camera input and JPEG encoding           =\n");
	printf("=  12.  JPEG decoding and display                =\n");
	printf("=  13.  Exit                                     =\n");
	printf("=                                                =\n");
	printf("==================================================\n");
	printf("Select number --> ");
}
//	"setlcd 35          -- set LCD as 3.5 inches screen 320x240\n"
//	"setlcd 43          -- set LCD as 4.3 inches screen 480x272\n"
//	"setlcd 56          -- set LCD 5.6 inches screen 640x480\n"
//	"setlcd 70          -- set LCD 7.0 inches screen 800x480\n"
//	"setlcd VGA800      -- set LCD 8.0 inches screen 800x600\n"
//	"setlcd VGA1024     -- set LCD screen with size 1024x768\n"	


static void print_menu_lcd(void)
{
        printf("============ Select your LCD size ================\n");
        printf("=                                                 =\n");
        printf("=  1.   3.5\"		-- 320x240                =\n");
        printf("=  2.   4.3\"		-- 480x272                =\n");
        printf("=  3.   5.6\"		-- 640x480                =\n");
        printf("=  4.   7.0\"		-- 800x480                =\n");
        printf("=  5.   8.0/10.4\"	-- 800x600                =\n");
        printf("=  6.   XVGA		-- 1024X768               =\n");
        printf("=                                                 =\n");
        printf("==================================================\n");
        printf("Select number --> ");
}

static int getlcdsize()
{
	int lcdnum;
	print_menu_lcd();
	scanf("%d", &lcdnum);

				
	if((lcdnum<1) || (lcdnum>6))
		lcdnum = 2;
	return (lcdnum -1);
}

