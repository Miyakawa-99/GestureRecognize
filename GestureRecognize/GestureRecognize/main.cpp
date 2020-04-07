#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <array>
#define _USE_MATH_DEFINES
#include <cmath>
#include <atlbase.h>
#include <Windows.h>
#include <Kinect.h>
#include <Kinect.VisualGestureBuilder.h>


//#include <opencv2/opencv.hpp>

#define ERROR_CHECK( ret )											\
	if( FAILED( ret ) ){											\
		std::stringstream ss;										\
		ss << "failed " #ret " " << std::hex << ret << std::endl;	\
		throw std::runtime_error( ss.str().c_str() );				\
		}

class KinectApp
{
private:

	CComPtr<IKinectSensor> kinect;

	CComPtr<IColorFrameReader> colorFrameReader;
	CComPtr<IBodyFrameReader> bodyFrameReader;
	std::vector<BYTE> colorBuffer;
	int colorWidth;
	int colorHeight;
	unsigned int colorBytesPerPixel;
	ColorImageFormat colorFormat = ColorImageFormat::ColorImageFormat_Bgra;
	//cv::Mat colorImage;

	std::array<CComPtr<IVisualGestureBuilderFrameReader>, BODY_COUNT> gestureFrameReader;
	std::vector<CComPtr<IGesture>> gestures;

	/*std::array<cv::Scalar, BODY_COUNT> colors;
	int font = cv::FONT_HERSHEY_SIMPLEX;
	int offset;*/

public:

	void initialize()
	{
		// Sensor���擾
		ERROR_CHECK(GetDefaultKinectSensor(&kinect));

		ERROR_CHECK(kinect->Open());
		BOOLEAN isOpen;
		ERROR_CHECK(kinect->get_IsOpen(&isOpen));
		if (!isOpen) {
			throw std::runtime_error("failed IKinectSensor::get_IsOpen( &isOpen )");
		}
		// Color Frame Source���擾�AColor Frame Reader���J��
		CComPtr<IColorFrameSource> colorFrameSource;
		ERROR_CHECK(kinect->get_ColorFrameSource(&colorFrameSource));
		ERROR_CHECK(colorFrameSource->OpenReader(&colorFrameReader));

		CComPtr<IFrameDescription> colorFrameDescription;
		ERROR_CHECK(colorFrameSource->CreateFrameDescription(colorFormat, &colorFrameDescription));
		ERROR_CHECK(colorFrameDescription->get_Width(&colorWidth));
		ERROR_CHECK(colorFrameDescription->get_Height(&colorHeight));
		ERROR_CHECK(colorFrameDescription->get_BytesPerPixel(&colorBytesPerPixel));
		colorBuffer.resize(colorWidth * colorHeight * colorBytesPerPixel);

		// Body Frame Source���擾�ABody Frame Reader���J��
		CComPtr<IBodyFrameSource> bodyFrameSource;
		ERROR_CHECK(kinect->get_BodyFrameSource(&bodyFrameSource));
		ERROR_CHECK(bodyFrameSource->OpenReader(&bodyFrameReader));

		// Gesture�̏�����
		initializeGesture();
		// Lookup Table
		/*colors[0] = cv::Scalar(255, 0, 0);
		colors[1] = cv::Scalar(0, 255, 0);
		colors[2] = cv::Scalar(0, 0, 255);
		colors[3] = cv::Scalar(255, 255, 0);
		colors[4] = cv::Scalar(255, 0, 255);
		colors[5] = cv::Scalar(0, 255, 255);*/
	}

	void run()
	{
		while (1) {
			update();
			//draw();
			//printf("update!\n");

			/*int key = cv::waitKey(10);
			if (key == VK_ESCAPE) {
				cv::destroyAllWindows();
				break;
			}*/
		}
	}

private:
	//�o�^����Ă���gesture���擾���ēo�^�A�L���ɂ���
	inline void initializeGesture()
	{
		for (int count = 0; count < BODY_COUNT; count++) {
			// Gesture Frame Source���擾
			CComPtr<IVisualGestureBuilderFrameSource> gestureFrameSource;
			ERROR_CHECK(CreateVisualGestureBuilderFrameSource(kinect, 0, &gestureFrameSource));

			// Gesture Frame Reader���J��
			ERROR_CHECK(gestureFrameSource->OpenReader(&gestureFrameReader[count]));
		}

		// �t�@�C��(*.gbd)����Gesture Database��ǂݍ���
		CComPtr<IVisualGestureBuilderDatabase> gestureDatabase;
		ERROR_CHECK(CreateVisualGestureBuilderDatabaseInstanceFromFile(L"RaiseHand.gbd", &gestureDatabase));

		// Gesture Database�Ɋ܂܂��Gesture�̐����擾
		UINT gestureCount;
		ERROR_CHECK(gestureDatabase->get_AvailableGesturesCount(&gestureCount));

		// Gesture���擾
		gestures.resize(gestureCount);
		ERROR_CHECK(gestureDatabase->get_AvailableGestures(gestureCount, &gestures[0]));
		for (int count = 0; count < BODY_COUNT; count++) {
			// Gesture��o�^
			CComPtr<IVisualGestureBuilderFrameSource> gestureFrameSource;
			ERROR_CHECK(gestureFrameReader[count]->get_VisualGestureBuilderFrameSource(&gestureFrameSource));
			ERROR_CHECK(gestureFrameSource->AddGestures(gestureCount, &gestures[0].p));

			// Gesture�̗L����
			for (const CComPtr<IGesture> g : gestures) {
				ERROR_CHECK(gestureFrameSource->SetIsEnabled(g, TRUE));
			}
		}
	}

	void update()
	{
		//updateColorFrame();
		updateBodyFrame();
		updateGestureFrame();
	}

	/*void updateColorFrame()
	{
		CComPtr<IColorFrame> colorFrame;
		HRESULT ret = colorFrameReader->AcquireLatestFrame(&colorFrame);
		if (FAILED(ret)) {
			return;
		}

		ERROR_CHECK(colorFrame->CopyConvertedFrameDataToArray(static_cast<UINT>(colorBuffer.size()), &colorBuffer[0], colorFormat));

		colorImage = cv::Mat(colorHeight, colorWidth, CV_8UC4, &colorBuffer[0]);
	}*/

	//body����擾����trackingID��o�^
	void updateBodyFrame()
	{
		CComPtr<IBodyFrame> bodyFrame;
		HRESULT ret = bodyFrameReader->AcquireLatestFrame(&bodyFrame);
		if (FAILED(ret)) {
			return;
		}

		std::array<CComPtr<IBody>, BODY_COUNT> bodies;
		ERROR_CHECK(bodyFrame->GetAndRefreshBodyData(BODY_COUNT, &bodies[0]));
		for (int count = 0; count < BODY_COUNT; count++) {
			CComPtr<IBody> body = bodies[count];
			BOOLEAN tracked;
			ERROR_CHECK(body->get_IsTracked(&tracked));
			if (!tracked) {
				continue;
			}

			// Tracking ID�̎擾
			UINT64 trackingId;
			ERROR_CHECK(body->get_TrackingId(&trackingId));

			// Tracking ID�̓o�^
			CComPtr<IVisualGestureBuilderFrameSource> gestureFrameSource;
			ERROR_CHECK(gestureFrameReader[count]->get_VisualGestureBuilderFrameSource(&gestureFrameSource));
			gestureFrameSource->put_TrackingId(trackingId);
		}
	}

	//trackingID���L���ł��邱�Ƃ��m�F�Agesture�̔F�����ʂ��擾
	void updateGestureFrame()
	{
		//offset = 0;
		for (int count = 0; count < BODY_COUNT; count++) {
			// �ŐV��Gesture Frame���擾
			CComPtr<IVisualGestureBuilderFrame> gestureFrame;
			HRESULT ret = gestureFrameReader[count]->CalculateAndAcquireLatestFrame(&gestureFrame);
			if (FAILED(ret)) {
				continue;
			}

			// Tracking ID�̓o�^�m�F
			BOOLEAN tracked;
			ERROR_CHECK(gestureFrame->get_IsTrackingIdValid(&tracked));
			if (!tracked) {
				continue;
			}

			// Gesture�̔F�����ʂ��擾
			for (const CComPtr<IGesture> g : gestures) {
				//printf("recognition!");
				result(gestureFrame, g, count);
			}
		}
	}

	//result�擾�Adiscrete or countinuous
	inline void result(const CComPtr<IVisualGestureBuilderFrame>& gestureFrame, const CComPtr<IGesture>& gesture, const int count)
	{
		// Gesture�̎��(Discrete or Continuous)���擾
		GestureType gestureType;
		ERROR_CHECK(gesture->get_GestureType(&gestureType));
		switch (gestureType) {
		case GestureType::GestureType_Discrete:
		{
			// Discrete Gesture�̔F�����ʂ��擾
			CComPtr<IDiscreteGestureResult> gestureResult;
			ERROR_CHECK(gestureFrame->get_DiscreteGestureResult(gesture, &gestureResult));

			// ���o���ʂ��擾
			BOOLEAN detected;
			ERROR_CHECK(gestureResult->get_Detected(&detected));
			if (!detected) {
				break;
			}

			// ���o���ʂ̐M���l���擾
			float confidence;
			ERROR_CHECK(gestureResult->get_Confidence(&confidence));

			// ���ʂ̕`��
			std::string discrete = gesture2string(gesture) + " : Detected (" + std::to_string(confidence) + ")";
			/*cv::putText(colorImage, discrete, cv::Point(50, 50 + offset), font, 1.0f, colors[count], 2, CV_AA);
			offset += 30;*/
			break;
		}
		case GestureType::GestureType_Continuous:
		{
			// Continuous Gesture�̔F�����ʂ��擾
			CComPtr<IContinuousGestureResult> gestureResult;
			ERROR_CHECK(gestureFrame->get_ContinuousGestureResult(gesture, &gestureResult));

			// �i�x���擾
			float progress;
			ERROR_CHECK(gestureResult->get_Progress(&progress));

			// ���ʂ̕`��
			std::string continuous = gesture2string(gesture) + " : Progress " + std::to_string(progress);
			/*cv::putText(colorImage, continuous, cv::Point(50, 50 + offset), font, 1.0f, colors[count], 2, CV_AA);
			offset += 30;*/
			break;
		}
		default:
			break;
		}
	}

	inline std::string gesture2string(const CComPtr<IGesture>& gesture)
	{
		// Gesture�̖��O���擾
		std::wstring buffer(BUFSIZ, L'\0');
		ERROR_CHECK(gesture->get_Name(BUFSIZ, &buffer[0]));

		const std::wstring temp = trim(&buffer[0]);
		const std::string name(temp.begin(), temp.end());
		printf("GestureResult:");
		std::cout << name << "\n";
		return name;
	}

	inline std::wstring trim(const std::wstring& str)
	{
		const std::wstring::size_type last = str.find_last_not_of(L" ");
		if (last == std::wstring::npos) {
			throw std::runtime_error("failed " __FUNCTION__);
		}
		return str.substr(0, last + 1);
	}

	/*void draw()
	{
		drawGestureFrame();
	}

	void drawGestureFrame()
	{
		// Gesture�̕\��
		if (!colorImage.empty()) {
			cv::imshow("Gesture", colorImage);
		}
	}*/
};

void main()
{
	try {
		KinectApp app;
		app.initialize();
		app.run();
	}
	catch (std::exception & ex) {
		std::cout << ex.what() << std::endl;
	}
}