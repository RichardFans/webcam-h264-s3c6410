## To test CAM + MFC + TVOUT

# 1. Copy files
cp ./Makefile_cam_mfc_tvout ../../APPLICATIONS
cp ./cam_dec_preview_tv.c ../../APPLICATIONS
cp ./test_tv.c ../../APPLICATIONS
cp ./videodev2_s3c_tv.h ../../APPLICATIONS/Common

# 2. make
cd ../../APPLICATIONS
make -f Makefile_cam_mfc_tvout clean
make -f Makefile_cam_mfc_tvout
