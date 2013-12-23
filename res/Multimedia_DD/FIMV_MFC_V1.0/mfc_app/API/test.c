#include "line_buf_test.h"
#include "ring_buf_test.h"
#include "encoder_test.h"
#include "display_test.h"
#include "demo.h"
#include "display_optimization1.h"
#include "display_optimization2.h"

int main(int argc, char **argv)
{
	//Test_H263_Decoder_Line_Buffer(argc, argv);
	//Test_H264_Decoder_Line_Buffer(argc, argv);
	//Test_MPEG4_Decoder_Line_Buffer(argc, argv);
	//Test_VC1_Decoder_Line_Buffer(argc, argv);
	//Test_Decoder_Ring_Buffer(argc, argv);
	Test_Display_H264(argc, argv);
	//Test_Display_MPEG4(argc, argv);
	//Test_Display_H263(argc, argv);
	//Test_Display_VC1(argc, argv);
	//Test_H264_Encoder(argc, argv);
	//Test_MPEG4_Encoder(argc, argv);	
	//Test_H263_Encoder(argc, argv);
	//Demo(argc, argv);
	//Test_Display_Optimization1(argc, argv);
	//Test_Display_Optimization2(argc, argv);

	return 0;
}


