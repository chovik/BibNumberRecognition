#include "opencv2/objdetect/objdetect.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

#include <boost/algorithm/string/trim.hpp>

#include "pipeline.h"
#include "facedetection.h"
#include "textdetection.h"

#include "stdio.h"

#undef min
#undef max


namespace pipeline {

	/// <summary>
	/// Computes area of torso according to the size of face.
	/// </summary>
static cv::Rect ComputeTorsoArea(cv::Rect faceArea, cv::Rect imageRectangle)
{
	double torsoWidth = faceArea.width * (7.0 / 3.0);
	double torsoHeight = faceArea.height * 4;
	double faceCenterX = faceArea.x + (faceArea.width * 0.5);
	double faceCenterY = faceArea.y + (faceArea.height * 0.5);

	double torsoHalfWidth = torsoWidth * 0.5;
	double torsoHalfHeight = torsoHeight * 0.5;

	double torsoX = faceCenterX - torsoHalfWidth;
	double torsoY = faceCenterY + (2 * 0.5 * faceArea.height);

	if (torsoX < 0)
	{
		double diffX = -torsoX;
		torsoX = 0;
		torsoWidth -= diffX;
	}

	if (torsoY < 0)
	{
		double diffY = -torsoY;
		torsoY = 0;
		torsoHeight -= diffY;
	}

	//if (torsoX + torsoWidth > imageRectangle.Width)
	//{
	//    var diffY = -torsoY;
	//    torsoY = 0;
	//    torsoHeight -= diffY;
	//}


	//if (torsoY + torsoHeight > imageRectangle.Height)
	//{
	//    torsoWidth
	//}

	cv::Rect torsoRectangle = cv::Rect((int)round(torsoX), (int)round(torsoY), (int)round(torsoWidth), (int)round(torsoHeight));

	if (torsoRectangle.x + torsoRectangle.width > imageRectangle.width)
	{
		double rightDiff = (torsoRectangle.x + torsoRectangle.width) - imageRectangle.width;
		torsoRectangle.width -= (int)round(rightDiff);

		if (torsoRectangle.width < 0)
		{
			torsoRectangle.width = 0;
		}
	}

	if (torsoRectangle.y + torsoRectangle.height > imageRectangle.height)
	{
		double bottomDiff = (torsoRectangle.y + torsoRectangle.height) - imageRectangle.height;
		torsoRectangle.height -= (int)round(bottomDiff);

		if (torsoRectangle.height < 0)
		{
			torsoRectangle.height = 0;
		}
	}

	return torsoRectangle;
}

static void vectorAtoi(std::vector<int>&numbers, std::vector<std::string>&text)
{
	for (std::vector<std::string>::iterator it = text.begin(); it != text.end();
					it++) {
		boost::algorithm::trim(*it);
		numbers.push_back(atoi(it->c_str()));
	}
}

/// <summary>
/// Resizes input if the width is greater than 1200px.
/// </summary>
cv::Mat ResizeInput(cv::Mat& img)
{
	cv::Mat resizedImg = img;
	if (resizedImg.cols > 1200)
	{
		double scale = (double)1200 / resizedImg.cols;
		resizedImg = cv::Mat(cvRound(img.rows * scale), cvRound(img.cols * scale), img.type());
		cv::resize(img, resizedImg, resizedImg.size(), 0, 0, cv::INTER_LINEAR);
	}
	return resizedImg;
}

//Runs detection/recognition algorithm on the input and returns 0 if process is successful.
// img
int Pipeline::processImage(
		cv::Mat& img,
		std::string svmModel,
		std::vector<int>& bibNumbers) {

	cv::Mat resizedImg = ResizeInput(img);
#if 0
	int res;
	const double scale = 1;
	std::vector<cv::Rect> faces;
	const static cv::Scalar colors[] = { CV_RGB(0, 0, 255), CV_RGB(0, 128, 255),
			CV_RGB(0, 255, 255), CV_RGB(0, 255, 0), CV_RGB(255, 128, 0), CV_RGB(
					255, 255, 0), CV_RGB(255, 0, 0), CV_RGB(255, 0, 255) };
	
	//first we resize the image to speed up the algorithm
	

	res = facedetection::processImage(resizedImg, faces);
	if (res < 0) {
		std::cerr << "ERROR: Could not proceed to face detection" << std::endl;
		return -1;
	}


	int i = 0;
	for (std::vector<cv::Rect>::const_iterator r = faces.begin(); r != faces.end();
			r++, i++) 
	{
		cv::Mat smallImgROI;
		std::vector < cv::Rect > nestedObjects;
		cv::Point center;
		cv::Scalar color = colors[i % 8];

		cv::Rect roi = ComputeTorsoArea(*r, cvRect(0, 0, resizedImg.cols, resizedImg.rows));

		//rectangle( img, roi, color, 3, 8, 0);
		if (roi.x > 0 && roi.x < resizedImg.rows
			&& roi.y > 0 && roi.y < resizedImg.cols)
		{
			cv::Mat subImage(resizedImg, roi);
			IplImage ipl_img = subImage;
			if ( //(i==10) &&
				(1)) {
				std::vector<std::string> text;
				const struct TextDetectionParams params = {
					1, /* darkOnLight */
					20, /* maxStrokeLength */
					11, /* minCharacterHeight */
					100, /* maxImgWidthToTextRatio */
					15, /* maxAngle */
					0, /* topBorder: don't discard anything */
					0,  /* bottomBorder: don't discard anything */
					2, /* min chain length */
				};
				std::vector<Chain> chains;
				std::vector<std::pair<Point2d, Point2d> > compBB;
				std::vector<std::pair<CvPoint, CvPoint> > chainBB;
				std::cout << "Pipeline::processImage - textDetector.detect" << std::endl;
				textDetector.detect(&ipl_img, params, chains, compBB, chainBB);
				textRecognizer.recognize(&ipl_img, params, svmModel, chains, compBB, chainBB, text);
				vectorAtoi(bibNumbers, text);
				char filename[100];

				sprintf(filename, "torso-%d.png", i);

				std::cout << "Pipeline::processImage - saving torso " << filename << " - " << faces.size() << std::endl;
				cv::imwrite(filename, subImage);
				std::cout << "Pipeline::processImage - saving torso END" << std::endl;

			}
		}
	}
#else
	IplImage ipl_img = resizedImg;
	std::vector<std::string> text;
	struct TextDetectionParams params = {
						1, /* darkOnLight */
						30, /* maxStrokeLength */
						11, /* minCharacterHeight */
						100, /* maxImgWidthToTextRatio */
						45, /* maxAngle */
						0, /* topBorder: discard top 10% */
						0,  /* bottomBorder: discard bottom 5% */
						3, /* min chain len */
						0, /* verify with SVM model up to this chain len */
						0, /* height needs to be this large to verify with model */
						img.rows * 5/1000
				};

	if (!svmModel.empty())
	{
		/* lower min chain len */
		params.minChainLen = 2;
		/* verify with SVM model up to this chain len */
		params.modelVerifLenCrit = 2;
		/* height needs to be this large to verify with model */
		params.modelVerifMinHeight = 15;
	}

	std::vector<Chain> chains;
	std::vector<std::pair<Point2d, Point2d> > compBB;
	std::vector<std::pair<CvPoint, CvPoint> > chainBB;
	textDetector.detect(&ipl_img, params, chains, compBB, chainBB);
	textRecognizer.recognize(&ipl_img, params, svmModel, chains, compBB, chainBB, text);
	vectorAtoi(bibNumbers, text);
#endif
	cv::imwrite("face-detection.png", resizedImg);

	return 0;

}

} /* namespace pipeline */

