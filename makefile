
main:
	x86_64-w64-mingw32-g++ -Wall -std=c++17 \
		main.cpp -o main.exe \
		-I./include \
		-I/src/build/x86_64-w64-mingw32/ \
		-I/src/build/x86_64-w64-mingw32/include/ \
		-I/src/build/x86_64-w64-mingw32/include/opencv4/ \
		-I/src/build/x86_64-w64-mingw32/include/opencv4/opencv2/ \
		-I/src/build/x86_64-w64-mingw32/include/opencv4/opencv2/core/ \
		-I/src/build/x86_64-w64-mingw32/include/opencv4/opencv2/imgproc/ \
		-I/src/build/x86_64-w64-mingw32/include/opencv4/opencv2/imgcodecs/ \
		-L/src/build/x86_64-w64-mingw32/lib \
		-L/src/build/x86_64-w64-mingw32/include/ \
		-L/src/build/x86_64-w64-mingw32/lib/opencv4/3rdparty/ \
		-lopencv_imgcodecs490 -lopencv_imgproc490 -lopencv_core490 \
		-l:libIlmImf.a -l:libzlib.a -l:liblibopenjp2.a \
		-l:liblibjpeg-turbo.a -l:liblibpng.a -l:liblibtiff.a -l:liblibwebp.a \
		-l:libtesseract53.a -l:libleptonica-1.84.1.a \
		-lshcore -ld3d11 -ldxgi -lole32 -luuid -l:libpng16.a -l:libjpeg.a -lzlibstatic -lws2_32 \
		-lpthread -static-libgcc -static-libstdc++ -lgdi32 -fopenmp -static


test_capture:
	x86_64-w64-mingw32-g++ -Wall -std=c++17 \
		test_capture.cpp -o test_capture.exe \
		-I./include \
		-I/src/build/x86_64-w64-mingw32/ \
		-I/src/build/x86_64-w64-mingw32/include/ \
		-I/src/build/x86_64-w64-mingw32/include/opencv4/ \
		-I/src/build/x86_64-w64-mingw32/include/opencv4/opencv2/ \
		-I/src/build/x86_64-w64-mingw32/include/opencv4/opencv2/core/ \
		-I/src/build/x86_64-w64-mingw32/include/opencv4/opencv2/imgproc/ \
		-I/src/build/x86_64-w64-mingw32/include/opencv4/opencv2/imgcodecs/ \
		-L/src/build/x86_64-w64-mingw32/lib \
		-L/src/build/x86_64-w64-mingw32/include/ \
		-L/src/build/x86_64-w64-mingw32/lib/opencv4/3rdparty/ \
		-lopencv_imgcodecs490 -lopencv_imgproc490 -lopencv_core490 \
		-l:libIlmImf.a -l:libzlib.a -l:liblibopenjp2.a \
		-l:liblibjpeg-turbo.a -l:liblibpng.a -l:liblibtiff.a -l:liblibwebp.a \
		-l:libtesseract53.a -l:libleptonica-1.84.1.a \
		-lshcore -ld3d11 -ldxgi -lole32 -luuid -l:libpng16.a -l:libjpeg.a -lzlibstatic -lws2_32 \
		-lpthread -static-libgcc -static-libstdc++ -lgdi32 -fopenmp -static


capture_actions:
	x86_64-w64-mingw32-g++ -Wall -std=c++17 \
		capture_actions.cpp -o capture_actions.exe \
		-I./include \
		-I/src/build/x86_64-w64-mingw32/ \
		-I/src/build/x86_64-w64-mingw32/include/ \
		-I/src/build/x86_64-w64-mingw32/include/opencv4/ \
		-I/src/build/x86_64-w64-mingw32/include/opencv4/opencv2/ \
		-I/src/build/x86_64-w64-mingw32/include/opencv4/opencv2/core/ \
		-I/src/build/x86_64-w64-mingw32/include/opencv4/opencv2/imgproc/ \
		-I/src/build/x86_64-w64-mingw32/include/opencv4/opencv2/imgcodecs/ \
		-L/src/build/x86_64-w64-mingw32/lib \
		-L/src/build/x86_64-w64-mingw32/include/ \
		-L/src/build/x86_64-w64-mingw32/lib/opencv4/3rdparty/ \
		-lopencv_imgcodecs490 -lopencv_imgproc490 -lopencv_core490 \
		-l:libIlmImf.a -l:libzlib.a -l:liblibopenjp2.a \
		-l:liblibjpeg-turbo.a -l:liblibpng.a -l:liblibtiff.a -l:liblibwebp.a \
		-l:libtesseract53.a -l:libleptonica-1.84.1.a \
		-lshcore -ld3d11 -ldxgi -lole32 -luuid -l:libpng16.a -l:libjpeg.a -lzlibstatic -lws2_32 \
		-lpthread -static-libgcc -static-libstdc++ -lgdi32 -fopenmp -static
