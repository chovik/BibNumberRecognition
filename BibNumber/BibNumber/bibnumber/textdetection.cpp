/*
 Copyright 2012 Andrew Perrault and Saurav Kumar.

 This file is part of DetectText.

 DetectText is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 DetectText is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with DetectText.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "..\stdafx.h"
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/connected_components.hpp>
#include <boost/unordered_map.hpp>
#include <boost/graph/floyd_warshall_shortest.hpp>
#include <boost/numeric/ublas/matrix.hpp>
#include <boost/numeric/ublas/io.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <cassert>
#include <cmath>
#include <iostream>
//#include <opencv2/imgproc/imgproc.hpp>
#include <opencv/cv.h>
#include <opencv/highgui.h>
#include <opencv/cxcore.h>
#include <math.h>
#include <time.h>
#include <utility>
#include <algorithm>
#include <vector>
#include "textdetection.h"

#include "log.h"

#define PI 3.14159265

#define COM_MAX_MEDIAN_RATIO (3.0)
#define COM_MAX_DIM_RATIO (2.0)
#define COM_MAX_DIST_RATIO (0.8)
#define COM_MAX_ASPECT_RATIO (4) //it must be great number because of digit 1
#define COM_MAX_WIDTH_TO_HEIGHT_RATIO (1.3)

#undef min
#undef max

static inline int square(int x) {
	return x * x;
}

static int inline ratio_within(float ratio, float max_ratio) {
	return ((ratio <= max_ratio) && (ratio >= 1 / max_ratio));
}

std::vector<std::pair<CvPoint, CvPoint> > findBoundingBoxes(
		std::vector<Chain> & chains,
		std::vector<std::pair<Point2d, Point2d> > & compBB, IplImage * output) {
	std::vector<std::pair<CvPoint, CvPoint> > bb;
	bb.reserve(chains.size());
	for (std::vector<Chain>::iterator chainit = chains.begin();
			chainit != chains.end(); chainit++) {
		int minx = output->width;
		int miny = output->height;
		int maxx = 0;
		int maxy = 0;
		for (std::vector<int>::const_iterator cit = chainit->components.begin();
				cit != chainit->components.end(); cit++) {
			miny = std::min(miny, compBB[*cit].first.y);
			minx = std::min(minx, compBB[*cit].first.x);
			maxy = std::max(maxy, compBB[*cit].second.y);
			maxx = std::max(maxx, compBB[*cit].second.x);
		}
		CvPoint p0 = cvPoint(minx, miny);
		CvPoint p1 = cvPoint(maxx, maxy);
		std::pair<CvPoint, CvPoint> pair(p0, p1);
		bb.push_back(pair);
	}
	return bb;
}

std::vector<std::pair<CvPoint, CvPoint> > findBoundingBoxes(
		std::vector<std::vector<Point2d> > & components, IplImage * output) {
	std::vector<std::pair<CvPoint, CvPoint> > bb;
	bb.reserve(components.size());
	for (std::vector<std::vector<Point2d> >::iterator compit =
			components.begin(); compit != components.end(); compit++) {
		int minx = output->width;
		int miny = output->height;
		int maxx = 0;
		int maxy = 0;
		for (std::vector<Point2d>::iterator it = compit->begin();
				it != compit->end(); it++) {
			miny = std::min(miny, it->y);
			minx = std::min(minx, it->x);
			maxy = std::max(maxy, it->y);
			maxx = std::max(maxx, it->x);
		}
		CvPoint p0 = cvPoint(minx, miny);
		CvPoint p1 = cvPoint(maxx, maxy);
		std::pair<CvPoint, CvPoint> pair(p0, p1);
		bb.push_back(pair);
	}
	return bb;
}

/// <summary>
/// Normalizes the image.
/// </summary>
/// <param name="input">The input.</param>
/// <param name="output">The output.</param>
void normalizeImage(IplImage * input, IplImage * output) {
	assert(input->depth == IPL_DEPTH_32F);
	assert(input->nChannels == 1);
	assert(output->depth == IPL_DEPTH_32F);
	assert(output->nChannels == 1);
	float maxVal = 0;
	float minVal = 1e100;
	for (int row = 0; row < input->height; row++) {
		const float* ptr = (const float*) (input->imageData
				+ row * input->widthStep);
		for (int col = 0; col < input->width; col++) {
			if (*ptr < 0) {
			} else {
				maxVal = std::max(*ptr, maxVal);
				minVal = std::min(*ptr, minVal);
			}
			ptr++;
		}
	}

	float difference = maxVal - minVal;
	for (int row = 0; row < input->height; row++) {
		const float* ptrin = (const float*) (input->imageData
				+ row * input->widthStep);\
		float* ptrout = (float*) (output->imageData + row * output->widthStep);\
		for (int col = 0; col < input->width; col++) {
			if (*ptrin < 0) {
				*ptrout = 1;
			} else {
				*ptrout = ((*ptrin) - minVal) / difference;
			}
			ptrout++;
			ptrin++;
		}
	}
}

void renderComponents(IplImage * SWTImage,
		std::vector<std::vector<Point2d> > & components, IplImage * output) {
	cvZero(output);
	for (std::vector<std::vector<Point2d> >::iterator it = components.begin();
			it != components.end(); it++) {
		for (std::vector<Point2d>::iterator pit = it->begin(); pit != it->end();
				pit++) {
			CV_IMAGE_ELEM(output, float, pit->y, pit->x) = CV_IMAGE_ELEM(
					SWTImage, float, pit->y, pit->x);
		}
	}
	for (int row = 0; row < output->height; row++) {
		float* ptr = (float*) (output->imageData + row * output->widthStep);
		for (int col = 0; col < output->width; col++) {
			if (*ptr == 0) {
				*ptr = -1;
			}
			ptr++;
		}
	}
	float maxVal = 0;
	float minVal = 1e100;
	for (int row = 0; row < output->height; row++) {
		const float* ptr = (const float*) (output->imageData
				+ row * output->widthStep);
		for (int col = 0; col < output->width; col++) {
			if (*ptr == 0) {
			} else {
				maxVal = std::max(*ptr, maxVal);
				minVal = std::min(*ptr, minVal);
			}
			ptr++;
		}
	}
	float difference = maxVal - minVal;
	for (int row = 0; row < output->height; row++) {
		float* ptr = (float*) (output->imageData + row * output->widthStep);\
		for (int col = 0; col < output->width; col++) {
			if (*ptr < 1) {
				*ptr = 1;
			} else {
				*ptr = ((*ptr) - minVal) / difference;
			}
			ptr++;
		}
	}

}

void renderComponentsWithBoxes(IplImage * SWTImage,
		std::vector<std::vector<Point2d> > & components,
		std::vector<std::pair<Point2d, Point2d> > & compBB, IplImage * output) {
	IplImage * outTemp = cvCreateImage(cvGetSize(output), IPL_DEPTH_32F, 1);

	renderComponents(SWTImage, components, outTemp);
	std::vector<std::pair<CvPoint, CvPoint> > bb;
	bb.reserve(compBB.size());
	for (std::vector<std::pair<Point2d, Point2d> >::iterator it =
			compBB.begin(); it != compBB.end(); it++) {
		CvPoint p0 = cvPoint(it->first.x, it->first.y);
		CvPoint p1 = cvPoint(it->second.x, it->second.y);
		std::pair<CvPoint, CvPoint> pair(p0, p1);
		bb.push_back(pair);
	}

	IplImage * out = cvCreateImage(cvGetSize(output), IPL_DEPTH_8U, 1);
	cvConvertScale(outTemp, out, 255, 0);
	cvCvtColor(out, output, CV_GRAY2RGB);
	cvReleaseImage ( &outTemp );
	cvReleaseImage ( &out );

	int count = 0;
	for (std::vector<std::pair<CvPoint, CvPoint> >::iterator it = bb.begin();
			it != bb.end(); it++) {
		CvScalar c;
		if (count % 3 == 0)
			c = cvScalar(255, 0, 0);
		else if (count % 3 == 1)
			c = cvScalar(0, 255, 0);
		else
			c = cvScalar(0, 0, 255);

		char txt[MAX_PATH];
		sprintf(txt, "%d", count);
		cv::Mat tmp_mat = cv::Mat(output);
		cv::rectangle(tmp_mat, it->first, it->second, c);
		cv::putText(tmp_mat, txt, it->first, cv::FONT_HERSHEY_SIMPLEX, 0.3, c);
		//free(txt);
		count++;
	}
}

void renderChainsWithBoxes(IplImage * SWTImage,
		std::vector<std::vector<Point2d> > & components,
		std::vector<Chain> & chains,
		std::vector<std::pair<Point2d, Point2d> > & compBB,
		std::vector<std::pair<CvPoint, CvPoint> > & bb,
		IplImage * output) {
	// keep track of included components
	std::vector<bool> included;
	included.reserve(components.size());
	for (unsigned int i = 0; i != components.size(); i++) {
		included.push_back(false);
	}
	for (std::vector<Chain>::iterator it = chains.begin(); it != chains.end();
			it++) {
		for (std::vector<int>::iterator cit = it->components.begin();
				cit != it->components.end(); cit++) {
			included[*cit] = true;
		}
	}
	std::vector<std::vector<Point2d> > componentsRed;
	for (unsigned int i = 0; i != components.size(); i++) {
		if (included[i]) {
			componentsRed.push_back(components[i]);
		}
	}
	IplImage * outTemp = cvCreateImage(cvGetSize(output), IPL_DEPTH_32F, 1);

	LOGL(LOG_CHAINS, componentsRed.size() << " components after chaining");

	renderComponents(SWTImage, componentsRed, outTemp);

	bb = findBoundingBoxes(chains, compBB, outTemp);

	IplImage * out = cvCreateImage(cvGetSize(output), IPL_DEPTH_8U, 1);
	cvConvertScale(outTemp, out, 255, 0);
	cvCvtColor(out, output, CV_GRAY2RGB);
	cvReleaseImage(&out);
	cvReleaseImage(&outTemp);
}

void renderChains(IplImage * SWTImage,
		std::vector<std::vector<Point2d> > & components,
		std::vector<Chain> & chains, IplImage * output) {
	// keep track of included components
	std::vector<bool> included;
	included.reserve(components.size());
	for (unsigned int i = 0; i != components.size(); i++) {
		included.push_back(false);
	}
	for (std::vector<Chain>::iterator it = chains.begin(); it != chains.end();
			it++) {
		for (std::vector<int>::iterator cit = it->components.begin();
				cit != it->components.end(); cit++) {
			included[*cit] = true;
		}
	}
	std::vector<std::vector<Point2d> > componentsRed;
	for (unsigned int i = 0; i != components.size(); i++) {
		if (included[i]) {
			componentsRed.push_back(components[i]);
		}
	}
	LOGL(LOG_CHAINS, componentsRed.size() << " components after chaining");
	IplImage * outTemp = cvCreateImage(cvGetSize(output), IPL_DEPTH_32F, 1);
	renderComponents(SWTImage, componentsRed, outTemp);
	cvConvertScale(outTemp, output, 255, 0);
	cvReleaseImage(&outTemp);
}

namespace textdetection {

TextDetector::TextDetector()
{
}

TextDetector::~TextDetector(void)
{
}

/// <summary>
/// Detects connected components on the input image.
/// </summary>
/// <param name="input">The input.</param>
/// <param name="params">The parameters that are used to ignore components that do not satisfied requirements</param>
/// <param name="chains">chains that was created by joining connected components</param>
/// <param name="compBB">rectangle areas of connected components. will be filled in the method.</param>
/// <param name="chainBB">rectangle area of chains. will be filled in the method.</param>
void TextDetector::detect(IplImage * input,
		const struct TextDetectionParams &params,
		std::vector<Chain> &chains,
		std::vector<std::pair<Point2d, Point2d> > &compBB,
		std::vector<std::pair<CvPoint, CvPoint> > &chainBB) {
	assert(input->depth == IPL_DEPTH_8U);
	assert(input->nChannels == 3);
	CvSize size = cvGetSize(input);
	if (size.height > 0
		&& size.width > 0)
	{
		cv::Mat inputMat2(input, false);
		cv::Mat outputMat1;
		EdgePreservingSmoothingRGB(inputMat2);
		//cv::GaussianBlur(input, input, cv::Size(5, 5), 0);
		ImageSegmentationFloodFill(inputMat2);
		// Convert to grayscale
		IplImage * grayImage = cvCreateImage(cvGetSize(input), IPL_DEPTH_8U, 1);
		cvCvtColor(input, grayImage, CV_RGB2GRAY);
		//swtDepthMatrix(grayImage, NULL);
		IplImage * edgeSmoothedImage = cvCreateImage(cvGetSize(input), IPL_DEPTH_8U, 1);
		EdgePreservingSmoothing(grayImage, edgeSmoothedImage);
		cv::Mat edgeSmoothMat(edgeSmoothedImage, false);
		cvSaveImage("edgeSmoothedImage.png", edgeSmoothedImage);

		cv::Mat gray;
		cv::Mat inputMat(input, true);
		//cv::GaussianBlur(inputMat, inputMat, cv::Size(5, 5), 0);
		cv::cvtColor(inputMat, gray, CV_RGB2GRAY);
		// Create Canny Image
		double threshold_low = 175;
		double threshold_high = 320;
		//IplImage * edgeImage = cvCreateImage(cvGetSize(input), IPL_DEPTH_8U, 1);
		cv::Mat edge;
		AutoCanny(&edgeSmoothMat, &edge);
		//cvCanny(grayImage, edgeImage, threshold_low, threshold_high, 3);


		IplImage* edgeImage = cvCloneImage(&(IplImage)edge);

		cvSaveImage("canny.png", edgeImage);

		// Create gradient X, gradient Y
		IplImage * gaussianImage = cvCreateImage(cvGetSize(input), IPL_DEPTH_32F,
			1);
		cvConvertScale(edgeSmoothedImage, gaussianImage, 1. / 255., 0);
		cvSmooth(gaussianImage, gaussianImage, CV_GAUSSIAN, 5, 5);
		IplImage * gradientX = cvCreateImage(cvGetSize(input), IPL_DEPTH_32F, 1);
		IplImage * gradientY = cvCreateImage(cvGetSize(input), IPL_DEPTH_32F, 1);
		cvSobel(gaussianImage, gradientX, 1, 0, CV_SCHARR);
		cvSobel(gaussianImage, gradientY, 0, 1, CV_SCHARR);

		cvSmooth(gradientX, gradientX, 3, 3);
		cvSmooth(gradientY, gradientY, 3, 3);
		cvReleaseImage(&gaussianImage);

		// Calculate SWT and return ray vectors
		std::vector<Ray> rays;
		IplImage * SWTImage = cvCreateImage(cvGetSize(input), IPL_DEPTH_32F, 1);
		for (int row = 0; row < input->height; row++) {
			float* ptr = (float*)(SWTImage->imageData + row * SWTImage->widthStep);
			for (int col = 0; col < input->width; col++) {
				*ptr++ = -1;
			}
		}

		strokeWidthTransform(edgeImage, gradientX, gradientY, params, SWTImage,
			rays);


		//cvConvertScale(gradientX, gradientX, 255., 0);
		//cvConvertScale(gradientY, gradientY, 255., 0);
		//cvSaveImage("gradientX.png", gradientX);
		//cvSaveImage("gradientY.png", gradientY);


		cvSaveImage("SWT_0.png", SWTImage);
		SWTMedianFilter(SWTImage, rays);
		cvSaveImage("SWT_1.png", SWTImage);

		IplImage * output2 = cvCreateImage(cvGetSize(input), IPL_DEPTH_32F, 1);
		normalizeImage(SWTImage, output2);
		cvSaveImage("SWT_2.png", output2);
		IplImage * saveSWT = cvCreateImage(cvGetSize(input), IPL_DEPTH_8U, 1);
		cvConvertScale(output2, saveSWT, 255, 0);
		cvSaveImage("SWT.png", saveSWT);
		cvReleaseImage(&output2);
		cvReleaseImage(&saveSWT);

		// Calculate legally connected components from SWT and gradient image.
		// return type is a vector of vectors, where each outer vector is a component and
		// the inner vector contains the (y,x) of each pixel in that component.
		cvSaveImage("grayImg.png", grayImage);
	
		std::vector<std::vector<Point2d> > components =
			findLegallyConnectedComponents(SWTImage, rays, edgeSmoothedImage);

	
		IplImage * connectedComponentsImg = cvCreateImage(cvGetSize(input), 8U, 3);
		//cvCopy(SWTImage, connectedComponentsImg, NULL);
		for (std::vector<std::vector<Point2d> >::iterator it = components.begin();
			it != components.end(); it++)
		{
			float mean, variance, median;
			float meanColor, varianceColor, medianColor;
			int minx, miny, maxx, maxy;
			componentStats(SWTImage, (*it), mean, variance, median, minx, miny,
				maxx, maxy,
				meanColor, varianceColor, medianColor, edgeSmoothedImage);

			Point2d bb1;
			bb1.x = minx;
			bb1.y = miny;

			Point2d bb2;
			bb2.x = maxx;
			bb2.y = maxy;
			std::pair<Point2d, Point2d> pair(bb1, bb2);

			compBB.push_back(pair);
		}

		renderComponentsWithBoxes(SWTImage, components, compBB, connectedComponentsImg);
		cvSaveImage("component-all.png", connectedComponentsImg);
		cvReleaseImage(&connectedComponentsImg);
		compBB.clear();

		// Filter the components
		std::vector<std::vector<Point2d> > validComponents;
		std::vector<Point2dFloat> compCenters;
		std::vector<float> compMedians;
		std::vector<Point2d> compDimensions;
		filterComponents(SWTImage, components, validComponents, compCenters,
			compMedians, compDimensions, compBB, params, edgeSmoothedImage);

		IplImage * output3 = cvCreateImage(cvGetSize(input), 8U, 3);
		renderComponentsWithBoxes(SWTImage, validComponents, compBB, output3);
		cvSaveImage("components.png", output3);
		cvReleaseImage(&output3);

		// Make chains of components
		chains = makeChains(input, validComponents, compCenters, compMedians,
			compDimensions, params);

		IplImage * output = cvCreateImage(cvGetSize(grayImage), IPL_DEPTH_8U, 3);
		renderChainsWithBoxes(SWTImage, validComponents, chains, compBB, chainBB, output);
		cvSaveImage("text-boxes.png", output);



		cvReleaseImage(&output);
		cvReleaseImage(&gradientX);
		cvReleaseImage(&gradientY);
		cvReleaseImage(&SWTImage);
		cvReleaseImage(&edgeImage);
		cvReleaseImage(&grayImage);
	}
	
	return;
}

} /* namespace textdetection */

void AutoCanny(cv::Mat * img, cv::Mat * edge)
{
	double med = median(img);
	double sigma = 0.27;
	double lower = std::max(0.0, (1.0 - sigma) * med);
	double upper = std::min(255.0, (1.0 + sigma) * med);
	cv::Canny(*img, *edge, lower, upper, 3, true);
	//blurCannyImage.Save("blurCanny-c-" + index + ".jpg");
}

// Code is copied from https://gist.github.com/heisters/9cd68181397fbd35031b
// calculates the median value of a single channel
// based on https://github.com/arnaudgelas/OpenCVExamples/blob/master/cvMat/Statistics/Median/Median.cpp
double median(cv::Mat * channel)
{
	double m = (channel->rows*channel->cols) / 2;
	int bin = 0;
	double med = -1.0;

	int histSize = 256;
	float range[] = { 0, 256 };
	const float* histRange = { range };
	bool uniform = true;
	bool accumulate = false;
	cv::Mat hist;

	cv::calcHist(channel, 1, 0, cv::Mat(), hist, 1, &histSize, &histRange, uniform, accumulate);

	for (int i = 0; i < histSize && med < 0.0; ++i)
	{
		bin += cvRound(hist.at< float >(i));
		if (bin > m && med < 0.0)
			med = i;
	}

	return med;
}

void swtDepthMatrix(IplImage * img, IplImage * swtImage)
{
	IplImage * threshold = cvCreateImage(cvGetSize(img), IPL_DEPTH_8U, 1);
	cvAdaptiveThreshold(img, threshold, 255, CV_ADAPTIVE_THRESH_MEAN_C, CV_THRESH_BINARY, 11, 5);
	cvSaveImage("threshold-adaptive.jpg", threshold);
	IplImage * distances = cvCreateImage(cvGetSize(img), IPL_DEPTH_8U, 1);
	cvDistTransform(threshold, distances);
	cvSaveImage("distances.jpg", threshold);
	
	//TODO round distances

}

void ImageSegmentationFloodFill(cv::Mat img)
{ 
	//IplImage* output = cvCloneImage(img);
	cv::Mat outputMat = img.clone();
	int colorIndex = 0;
	cv::RNG rng(0xFFFFFFFF);
	//connectedComp.
	std::vector<std::vector<LineSegment*>> lineSegments;
	double diffTolerance = 35;
	for (int row = 0; row < img.rows; row++)
	{
		lineSegments.push_back(std::vector<LineSegment*>());
		int x = 0;
		FloodRow(outputMat.row(row), cv::Point(x, 0), diffTolerance);
		//while (x <= img->width - 1)
		//{
		//	cv::Rect connectedComp;
		//	int icolor = (unsigned)rng;
		//	cv::Scalar randomColor(icolor & 255, (icolor >> 8) & 255, (icolor >> 16) & 255);
			//FloodRow(outputMat.row(row), cv::Point(x, 0), diffTolerance);
			//cv::floodFill(outputMat.row(row), cv::Point(x, 0), randomColor, &connectedComp, cv::Scalar(diffTolerance, diffTolerance, diffTolerance, diffTolerance), cv::Scalar(diffTolerance, diffTolerance, diffTolerance, diffTolerance));

		//	connectedComp.y = row;
		//	int meanReadSum = 0;
		//	int meanGreenSum = 0;
		//	int meanBlueSum = 0;
		//	int meanCount = connectedComp.width;
		//	double meanRead = 0;
		//	double meanGreen = 0;
		//	double meanBlue = 0;

		//	for (int meanX = x; meanX < x + connectedComp.width + 1; meanX++)
		//	{
		//		char* val = img->imageData + img->widthStep * row + meanX * 3;//CV_IMAGE_ELEM(img, int, row, meanX);
		//		meanReadSum += (int)val[0];
		//		meanGreenSum += (int)val[1];
		//		meanBlueSum += (int)val[2];
		//	}

		//	meanRead = meanReadSum / meanCount;
		//	meanGreen = meanGreenSum / meanCount;
		//	meanBlue = meanBlueSum / meanCount;

		//	colorIndex++;
		//	LineSegment* lineSegment = new LineSegment();
		//	lineSegment->Color = randomColor;
		//	lineSegment->MeanRed = meanRead;
		//	lineSegment->MeanGreen = meanGreen;
		//	lineSegment->MeanBlue = meanBlue;
		//	lineSegment->Rect = connectedComp;
		//	lineSegments[row].push_back(lineSegment);
		//	x = x + connectedComp.width + 1;
		//}
	}

	cv::imwrite("floodfill-pre.bmp", outputMat);

	int rows = lineSegments.size();
	for (int row = 0; row < lineSegments.size(); row++)
	{
		std::vector<LineSegment*> rowComponents = lineSegments[row];
		int topRow = row - 1;
		int bottomRow = row + 1;

		/*if (topRow >= 0)
		{
			std::vector<LineSegment> topComponents = lineSegments[topRow];
			int topLastIndex = 0;

			for (int segmentIndex = 0; segmentIndex < rowComponents.size(); segmentIndex++)
			{
				LineSegment segment = rowComponents[segmentIndex];
				for (topLastIndex; topLastIndex <= topComponents.size(); topLastIndex++)
				{
					LineSegment topSegment = topComponents[topLastIndex];

					if (topSegment.Rect.x <= (segment.Rect.x + segment.Rect.width)
						&& (topSegment.Rect.x + topSegment.Rect.width) >= segment.Rect.x)
					{
						if (topSegment.Mean >= (segment.Mean - 30)
							&& topSegment.Mean <= (segment.Mean + 30))
						{
							topSegment.Color = segment.Color;
						}
					}
				}
			}
		}*/

		if (bottomRow < rows)
		{
			std::vector<LineSegment*> bottomComponents = lineSegments[bottomRow];
			int bottomLastIndex = 0;

			for (int segmentIndex = 0; segmentIndex < rowComponents.size(); segmentIndex++)
			{
				LineSegment* segment = rowComponents[segmentIndex];
				for (bottomLastIndex; bottomLastIndex < bottomComponents.size(); bottomLastIndex++)
				{
					LineSegment* bottomSegment = bottomComponents[bottomLastIndex];


					if (bottomSegment->Rect.x <= (segment->Rect.x + segment->Rect.width)
						&& (bottomSegment->Rect.x + bottomSegment->Rect.width) >= segment->Rect.x)
					{
						if (bottomSegment->MeanRed >= (segment->MeanRed - 50)
							&& bottomSegment->MeanRed <= (segment->MeanRed + 50)
							&&
							bottomSegment->MeanGreen >= (segment->MeanGreen - 50)
							&& bottomSegment->MeanGreen <= (segment->MeanGreen + 50)
							&& 
							bottomSegment->MeanBlue >= (segment->MeanBlue - 50)
							&& bottomSegment->MeanBlue <= (segment->MeanBlue + 50))
						{
							bottomSegment->Color = segment->Color;
							//bottomComponents[segmentIndex] = bottomSegment;
						}
						else
						{

						}
					}
					else
					{
						break;
					}
				}
			}
		}
	}


	for (int row = 0; row < lineSegments.size(); row++)
	{
		std::vector<LineSegment*> rowComponents = lineSegments[row];

		for (int segmentIndex = 0; segmentIndex < rowComponents.size(); segmentIndex++)
		{
			LineSegment* segment = rowComponents[segmentIndex];
			cv::rectangle(outputMat, segment->Rect, segment->Color);
		}
	}

	cv::imwrite("floodfill.bmp", outputMat);
}

int FloodRow(cv::Mat row, cv::Point startPoint, double toleratedDiff)
{
	int width = 0;
	std::vector<int> componentIndexes;
	int meanRed = 0;
	int meanGreen = 0;
	int meanBlue = 0;
	std::vector<LineSegment*> components;
	int componentIndex = 0;

	int x = startPoint.x;
	cv::RNG rng(0xFFFFFFFF);

	if (x < row.cols)
	{
		cv::Vec3b color = row.at<cv::Vec3b>(cv::Point(x, 0));
		byte red = color.val[0]; //row.data + row.at(widthStep * 0 + x * 3;//CV_IMAGE_ELEM(img, int, row, meanX);
		byte blue = color.val[1];
		byte green = color.val[2];
		meanRed = red;
		meanGreen = green;
		meanBlue = blue;
		int icolor = (unsigned)rng;
		cv::Scalar randomColor(icolor & 255, (icolor >> 8) & 255, (icolor >> 16) & 255);
		LineSegment* lineSegment = new LineSegment();
		lineSegment->Color = randomColor;
		componentIndexes.push_back(x);
		components.push_back(lineSegment);
		components[0]->startX = x;
	}

	int gapSize = 0;
	x++;
	for (x; x < row.cols; x++)
	{
		int rightX = x;

		double currMeanRed = meanRed / componentIndexes.size();
		double currMeanGreen = meanGreen / componentIndexes.size();
		double currMeanBlue = meanBlue / componentIndexes.size();

		if (rightX < row.cols)
		{
			//cv::Vec3b color = row.at<cv::Vec3b>(cv::Point(x, 0));
			//byte red = color.val[0]; //row.data + row.at(widthStep * 0 + x * 3;//CV_IMAGE_ELEM(img, int, row, meanX);
			//byte green = color.val[1];
			//byte blue = color.val[2];
			

			cv::Vec3b rightColor = row.at<cv::Vec3b>(cv::Point(rightX, 0));
			byte rightRed = rightColor.val[0]; //row.data + row.at(widthStep * 0 + x * 3;//CV_IMAGE_ELEM(img, int, row, meanX);
			byte rightGreen = rightColor.val[1];
			byte rightBlue = rightColor.val[2];

			int diffRed = rightRed - currMeanRed;
			int diffGreen = rightGreen - currMeanGreen;
			int diffBlue = rightBlue - currMeanBlue;
			double distance = sqrt(diffRed*diffRed + diffGreen*diffGreen + diffBlue*diffBlue);

			if (distance < toleratedDiff)
			{
				componentIndexes.push_back(rightX);
				meanRed += rightRed;
				meanGreen += rightGreen;
				meanBlue += rightBlue;
			}
			else
			{
				gapSize++;

				if (gapSize > 3)
				{
					int lastIndex = x - gapSize;
					LineSegment* comp = components[componentIndex];
					comp->Rect = cv::Rect(cv::Point(comp->startX, 0), cv::Point(lastIndex, 1));
					comp->MeanRed = currMeanRed;
					comp->MeanGreen = currMeanGreen;
					comp->MeanBlue = currMeanBlue;
					
					x = x - gapSize + 1;
					meanRed = 0;
					meanGreen = 0;
					meanBlue = 0;
					LineSegment* lineSegment = new LineSegment();
					int icolor = (unsigned)rng;
					cv::Scalar randomColor(icolor & 255, (icolor >> 8) & 255, (icolor >> 16) & 255);
					lineSegment->Color = randomColor;
					components.push_back(lineSegment);
					componentIndex++;
					components[componentIndex]->startX = x;//x++
					componentIndexes.clear(); 
					componentIndexes.push_back(x);

					cv::Vec3b color = row.at<cv::Vec3b>(cv::Point(x, 0));
					byte red = color.val[0]; //row.data + row.at(widthStep * 0 + x * 3;//CV_IMAGE_ELEM(img, int, row, meanX);
					byte blue = color.val[1];
					byte green = color.val[2];
					meanRed = red;
					meanGreen = green;
					meanBlue = blue;
					gapSize = 0;
				}
			}
		}		
	}

	for (int compIndex = 0; compIndex < components.size(); compIndex++)
	{
		LineSegment* comp = components[compIndex];
		cv::rectangle(row, comp->Rect, comp->Color);
	}

	return width;
}

void strokeWidthTransform(IplImage * edgeImage, IplImage * gradientX,
		IplImage * gradientY, const struct TextDetectionParams &params,
		IplImage * SWTImage, std::vector<Ray> & rays) {
	// First pass
	float prec = .05;
	for (int row = 0; row < edgeImage->height; row++) {
		const uchar* ptr = (const uchar*) (edgeImage->imageData
				+ row * edgeImage->widthStep);
		for (int col = 0; col < edgeImage->width; col++) {
			if (*ptr > 0) {
				Ray r;

				Point2d p;
				p.x = col;
				p.y = row;
				r.p = p;
				std::vector<Point2d> points;
				points.push_back(p);

				float curX = (float) col + 0.5;
				float curY = (float) row + 0.5;
				int curPixX = col;
				int curPixY = row;
				float G_x = CV_IMAGE_ELEM(gradientX, float, row, col);
				float G_y = CV_IMAGE_ELEM(gradientY, float, row, col);
				// normalize gradient
				float mag = sqrt((G_x * G_x) + (G_y * G_y));
				if (params.darkOnLight) {
					G_x = -G_x / mag;
					G_y = -G_y / mag;
				} else {
					G_x = G_x / mag;
					G_y = G_y / mag;

				}
				while (true) {
					curX += G_x * prec;
					curY += G_y * prec;
					if ((int) (floor(curX)) != curPixX
							|| (int) (floor(curY)) != curPixY) {
						curPixX = (int) (floor(curX));
						curPixY = (int) (floor(curY));
						// check if pixel is outside boundary of image
						if (curPixX < 0 || (curPixX >= SWTImage->width)
								|| curPixY < 0
								|| (curPixY >= SWTImage->height)) {
							break;
						}
						Point2d pnew;
						pnew.x = curPixX;
						pnew.y = curPixY;
						points.push_back(pnew);

						if (CV_IMAGE_ELEM(edgeImage, uchar, curPixY, curPixX)
								> 0) {
							r.q = pnew;
							// dot product
							float G_xt = CV_IMAGE_ELEM(gradientX, float,
									curPixY, curPixX);
							float G_yt = CV_IMAGE_ELEM(gradientY, float,
									curPixY, curPixX);
							mag = sqrt((G_xt * G_xt) + (G_yt * G_yt));
							if (params.darkOnLight) {
								G_xt = -G_xt / mag;
								G_yt = -G_yt / mag;
							} else {
								G_xt = G_xt / mag;
								G_yt = G_yt / mag;

							}

							if (acos(G_x * -G_xt + G_y * -G_yt) < PI / 2.0) {
								float length =
										sqrt(
												((float) r.q.x - (float) r.p.x)
														* ((float) r.q.x
																- (float) r.p.x)
														+ ((float) r.q.y
																- (float) r.p.y)
																* ((float) r.q.y
																		- (float) r.p.y));
								if (length > params.maxStrokeLength)
									break;

								for (std::vector<Point2d>::iterator pit =
										points.begin(); pit != points.end();
										pit++) {
									if (CV_IMAGE_ELEM(SWTImage, float, pit->y,
											pit->x) < 0) {
										CV_IMAGE_ELEM(SWTImage, float, pit->y, pit->x) =
												length;
									} else {
										CV_IMAGE_ELEM(SWTImage, float, pit->y, pit->x) =
												std::min(length,
														CV_IMAGE_ELEM(SWTImage,
																float, pit->y,
																pit->x));
									}
								}
								r.points = points;
								rays.push_back(r);
							}
							break;
						}
					}
				}
			}
			ptr++;
		}
	}

}

void SWTMedianFilter(IplImage * SWTImage, std::vector<Ray> & rays) {
	for (std::vector<Ray>::iterator rit = rays.begin(); rit != rays.end();
			rit++) {
		for (std::vector<Point2d>::iterator pit = rit->points.begin();
				pit != rit->points.end(); pit++) {
			pit->SWT = CV_IMAGE_ELEM(SWTImage, float, pit->y, pit->x);
		}
		std::sort(rit->points.begin(), rit->points.end(), &Point2dSort);
		float median = (rit->points[rit->points.size() / 2]).SWT;
		for (std::vector<Point2d>::iterator pit = rit->points.begin();
				pit != rit->points.end(); pit++) {
			CV_IMAGE_ELEM(SWTImage, float, pit->y, pit->x) = std::min(pit->SWT,
					median);
		}
	}

}

bool Point2dSort(const Point2d &lhs, const Point2d &rhs) {
	return lhs.SWT < rhs.SWT;
}

std::vector<std::vector<Point2d> > findLegallyConnectedComponents(
		IplImage * SWTImage, std::vector<Ray> &rays,
		IplImage * gray) {
	boost::unordered_map<int, int> map;
	boost::unordered_map<int, Point2d> revmap;

	typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::undirectedS> Graph;
	int num_vertices = 0;
	// Number vertices for graph.  Associate each point with number
	for (int row = 0; row < SWTImage->height; row++) {
		float * ptr = (float*) (SWTImage->imageData + row * SWTImage->widthStep);
		for (int col = 0; col < SWTImage->width; col++) {
			if (*ptr > 0) {
				map[row * SWTImage->width + col] = num_vertices;
				Point2d p;
				p.x = col;
				p.y = row;
				revmap[num_vertices] = p;
				num_vertices++;
			}
			ptr++;
		}
	}

	Graph g(num_vertices);

	for (int row = 0; row < SWTImage->height; row++) {
		float * ptr = (float*) (SWTImage->imageData + row * SWTImage->widthStep);
		for (int col = 0; col < SWTImage->width; col++) {
			float val = *ptr;
			if (val > 0) {
				float currentPixelColor = CV_IMAGE_ELEM(gray, byte, row, col);
				// check pixel to the right, right-down, down, left-down
				int this_pixel = map[row * SWTImage->width + col];
				if (col + 1 < SWTImage->width) {
					float right = CV_IMAGE_ELEM(SWTImage, float, row, col + 1);
					

					if (right > 0)
					{
						float rightPixelColor = CV_IMAGE_ELEM(gray, byte, row, col + 1);
						float colorDiff = abs(currentPixelColor - rightPixelColor);
						bool hasSimiliarStrokeWidth = false;

						if (right > val)
						{
							hasSimiliarStrokeWidth = (right / val <= 3.0);
						}
						else
						{
							hasSimiliarStrokeWidth = (val / right <= 3.0);
						}

						if (hasSimiliarStrokeWidth)
						{
							boost::add_edge(this_pixel,
											map.at(row * SWTImage->width + col + 1), g);
						}
					}
						
				}
				if (row + 1 < SWTImage->height) {
					if (col + 1 < SWTImage->width) {
						float right_down = CV_IMAGE_ELEM(SWTImage, float,
								row + 1, col + 1);
						if (right_down > 0
								&& ((*ptr) / right_down <= 3.0
										|| right_down / (*ptr) <= 3.0))
							boost::add_edge(this_pixel,
									map.at(
											(row + 1) * SWTImage->width + col
													+ 1), g);
					}
					float down = CV_IMAGE_ELEM(SWTImage, float, row + 1, col);

					if (down > 0)
					{
						float downPixelColor = CV_IMAGE_ELEM(gray, byte, row + 1, col);
						bool hasSimiliarStrokeWidth = false;

						if (down > val)
						{
							hasSimiliarStrokeWidth = (down / val <= 3.0);
						}
						else
						{
							hasSimiliarStrokeWidth = (val / down <= 3.0);
						}

						float colorDiff = abs(currentPixelColor - downPixelColor);

						if (hasSimiliarStrokeWidth)
						{
							boost::add_edge(this_pixel,
											map.at((row + 1) * SWTImage->width + col), g);
						}
					}
						
					if (col - 1 >= 0) {
						float left_down = CV_IMAGE_ELEM(SWTImage, float,
								row + 1, col - 1);
						if (left_down > 0
								&& ((*ptr) / left_down <= 3.0
										|| left_down / (*ptr) <= 3.0))
							boost::add_edge(this_pixel,
									map.at(
											(row + 1) * SWTImage->width + col
													- 1), g);
					}
				}
			}
			ptr++;
		}
	}

	std::vector<int> c(num_vertices);

	int num_comp = connected_components(g, &c[0]);

	std::vector<std::vector<Point2d> > components;
	components.reserve(num_comp);
	LOGL(LOG_COMPONENTS,
			"Before filtering, " << num_comp << " components and " << num_vertices << " vertices");
	for (int j = 0; j < num_comp; j++) {
		std::vector<Point2d> tmp;
		components.push_back(tmp);
	}
	for (int j = 0; j < num_vertices; j++) {
		Point2d p = revmap[j];
		(components[c[j]]).push_back(p);
	}

	return components;
}

Point2d createPoint2d(int x, int y)
{
	Point2d p;

	p.x = x;
	p.y = y;

	return p;
}

int ComputeManhattanColorDistance(IplImage * img, Point2d center, float p)
{
	std::vector<Point2d> neighbours;
	GetNeighbours(center, neighbours);
	float centerValue = CV_IMAGE_ELEM(img, byte, center.y, center.x);
	std::vector<float> coeficients;
	float sumCoeficients = 0;

	for (int dIndex = 0; dIndex < 8; dIndex++)
	{
		Point2d point = neighbours[dIndex];
		float pointValue = CV_IMAGE_ELEM(img, byte, point.y, point.x);
		float d = abs(centerValue - pointValue) / 255.0;
		float c = pow((1 - d), p);
		coeficients.push_back(c);
		sumCoeficients += c;
	}

	float newValue = 0;

	for (int dIndex = 0; dIndex < 8; dIndex++)
	{
		Point2d point = neighbours[dIndex];
		float pointValue = CV_IMAGE_ELEM(img, byte, point.y, point.x);
		newValue += round(coeficients[dIndex] * (1 / sumCoeficients) * pointValue);
	}

	if (newValue > 255)
	{
		newValue = 255;
	}

	return newValue;
}

cv::Vec3b ComputeManhattanColorDistanceRGB(cv::Mat img, Point2d center, float p)
{
	std::vector<Point2d> neighbours;
	GetNeighbours(center, neighbours);
	cv::Vec3b rgbCenter = img.at<cv::Vec3b>(cv::Point(center.x, center.y));
	float centerValueRed = rgbCenter[0];
	float centerValueGreen = rgbCenter[1];
	float centerValueBlue = rgbCenter[2];
	std::vector<float> coeficients;
	float sumCoeficients = 0;

	for (int dIndex = 0; dIndex < 8; dIndex++)
	{
		Point2d point = neighbours[dIndex];
		cv::Vec3b rgb = img.at<cv::Vec3b>(cv::Point(center.x, center.y));
		int red = rgb[0];
		int green = rgb[1];
		int blue = rgb[2];

		float d = (float)(abs(centerValueRed - red) + abs(centerValueGreen - green) + abs(centerValueBlue - blue)) / (3 * 255);
		//float pointValue = CV_IMAGE_ELEM(img, byte, point.y, point.x);
		//float d = abs(centerValue - pointValue) / 255.0;
		float c = pow((1 - d), p);
		coeficients.push_back(c);
		sumCoeficients += c;
	}

	float newRed = 0;
	float newGreen = 0;
	float newBlue = 0;

	for (int dIndex = 0; dIndex < 8; dIndex++)
	{
		Point2d point = neighbours[dIndex];
		cv::Vec3b rgb = img.at<cv::Vec3b>(cv::Point(center.x, center.y));
		float red = rgb[0];
		float green = rgb[1];
		float blue = rgb[2];

		newRed += (int)round(coeficients[dIndex] * (1 / sumCoeficients) * red);
		newGreen += (int)round(coeficients[dIndex] * (1 / sumCoeficients) * green);
		newBlue += (int)round(coeficients[dIndex] * (1 / sumCoeficients) * blue);


	}

	if (newRed > 255)
	{
		newRed = 255;
	}

	if (newGreen > 255)
	{
		newGreen = 255;
	}

	if (newBlue > 255)
	{
		newBlue = 255;
	}

	return cv::Vec3b(newRed, newGreen, newBlue);
}

void EdgePreservingSmoothingRGB(cv::Mat img)
{
	int cols = img.cols;
	int rows = img.rows;

	for (int i = 0; i < 1; i++)
	{
		for (int rowIndex = 0; rowIndex < rows; rowIndex++)
		{
			for (int columnIndex = 0; columnIndex < cols; columnIndex++)
			{
				if (rowIndex == 0
					|| rowIndex == rows - 1
					|| columnIndex == 0
					|| columnIndex == cols - 1)
				{
					cv::Vec3b color(0, 0, 0);
					img.at<cv::Vec3b>(cv::Point(columnIndex, rowIndex)) = color;
					//CV_IMAGE_ELEM(img, byte, rowIndex, columnIndex) = 0;
					continue;
				}

				cv::Vec3b oldValue = img.at<cv::Vec3b>(cv::Point(columnIndex, rowIndex));
				//byte oldValue = CV_IMAGE_ELEM(img, byte, rowIndex, columnIndex);

				if (oldValue[0] == 0
					&& oldValue[1] == 0
					&& oldValue[2] == 0)
				{
					//var newValue = ComputeManhattanColorDistancesBW(img, new System.Drawing.Point(rowIndex, columnIndex), 10);
					img.at<cv::Vec3b>(cv::Point(columnIndex, rowIndex)) = oldValue;
				}
				else
				{
					cv::Vec3b newValue = ComputeManhattanColorDistanceRGB(img, createPoint2d(columnIndex, rowIndex), 10);
					
					img.at<cv::Vec3b>(cv::Point(columnIndex, rowIndex)) = newValue; //CV_IMAGE_ELEM(output, byte, rowIndex, columnIndex) = ToByte(newValue);
				}
			}
		}
	}
}


void EdgePreservingSmoothing(IplImage * img, IplImage * output)
{
	int cols = img->width;
	int rows = img->height;

	for (int i = 0; i < 1; i++)
	{
		for (int rowIndex = 0; rowIndex < rows; rowIndex++)
		{
			for (int columnIndex = 0; columnIndex < cols; columnIndex++)
			{
				if (rowIndex == 0
					|| rowIndex == rows - 1
					|| columnIndex == 0
					|| columnIndex == cols - 1)
				{
					CV_IMAGE_ELEM(img, byte, rowIndex, columnIndex) = 0;
					continue;
				}

				byte oldValue = CV_IMAGE_ELEM(img, byte, rowIndex, columnIndex);

				if (oldValue == 0)
				{
					//var newValue = ComputeManhattanColorDistancesBW(img, new System.Drawing.Point(rowIndex, columnIndex), 10);
					CV_IMAGE_ELEM(output, byte, rowIndex, columnIndex) = oldValue;
				}
				else
				{
					int newValue = ComputeManhattanColorDistance(img, createPoint2d(columnIndex, rowIndex), 10);
					CV_IMAGE_ELEM(output, byte, rowIndex, columnIndex) = ToByte(newValue);
				}
			}
		}
	}
}

byte ToByte(float value)
{
	int rounded = (int)round(value);

	if (rounded > 255)
	{
		rounded = 255;
	}
	else if (rounded < 0)
	{
		rounded = 0;
	}

	return (byte)rounded;
}


void GetNeighbours(Point2d point, std::vector<Point2d> & neighbours)
{
	neighbours.clear();

	neighbours.push_back(createPoint2d(
		point.x - 1,
		point.y - 1));

	neighbours.push_back(createPoint2d(
		point.x,
		point.y - 1));

	neighbours.push_back(createPoint2d(
		point.x + 1,
		point.y - 1)); 
		
	neighbours.push_back(createPoint2d(
		point.x - 1,
		point.y));

	neighbours.push_back(createPoint2d(
		point.x,
		point.y));

	neighbours.push_back(createPoint2d(
		point.x + 1,
		point.y));

	neighbours.push_back(createPoint2d(
		point.x - 1,
		point.y + 1));

	neighbours.push_back(createPoint2d(
		point.x,
		point.y + 1));

	neighbours.push_back(createPoint2d(
		point.x + 1,
		point.y + 1));
}

/// <summary>Computes statistics of components values that are used for filtering.
/// </summary>
/// <param name="SWTImage">The SWT image.</param>
/// <param name="component">The component.</param>
/// <param name="mean">The mean.</param>
/// <param name="variance">The variance.</param>
/// <param name="median">The median.</param>
/// <param name="minx">The minx.</param>
/// <param name="miny">The miny.</param>
/// <param name="maxx">The maxx.</param>
/// <param name="maxy">The maxy.</param>
/// <param name="meanColor">Color of the mean.</param>
/// <param name="varianceColor">Color of the variance.</param>
/// <param name="medianColor">Color of the median.</param>
/// <param name="img">The img.</param>
void componentStats(IplImage * SWTImage, const std::vector<Point2d> & component,
		float & mean, float & variance, float & median, int & minx, int & miny,
		int & maxx, int & maxy,
		float & meanColor, float & varianceColor, float & medianColor, IplImage * img) {
	std::vector<float> temp;
	std::vector<float> tempColor;
	temp.reserve(component.size());
	tempColor.reserve(component.size());
	mean = 0;
	variance = 0;
	minx = 1000000;
	miny = 1000000;
	maxx = 0;
	maxy = 0;

	meanColor = 0;
	varianceColor = 0;
	medianColor = 0;

	for (std::vector<Point2d>::const_iterator it = component.begin();
			it != component.end(); it++) {
		float t = CV_IMAGE_ELEM(SWTImage, float, it->y, it->x);
		float color = CV_IMAGE_ELEM(img, byte, it->y, it->x);
		mean += t;
		meanColor += color;
		temp.push_back(t);
		tempColor.push_back(color);
		miny = std::min(miny, it->y);
		minx = std::min(minx, it->x);
		maxy = std::max(maxy, it->y);
		maxx = std::max(maxx, it->x);
	}
	mean = mean / ((float) component.size());
	meanColor = meanColor / ((float)component.size());
	for (std::vector<float>::const_iterator it = temp.begin(); it != temp.end();
			it++) {
		variance += (*it - mean) * (*it - mean);
	}

	for (std::vector<float>::const_iterator it = tempColor.begin(); it != tempColor.end();
		it++) {
		varianceColor += (*it - meanColor) * (*it - meanColor);
	}

	
	variance = variance / ((float) component.size());
	varianceColor = varianceColor / ((float)component.size());
	std::sort(temp.begin(), temp.end());
	std::sort(tempColor.begin(), tempColor.end());
	median = temp[temp.size() / 2];
	medianColor = tempColor[tempColor.size() / 2];
}
#define NO_FILTER
/// <summary>
/// Filters the components accoridng to the requirements.
/// </summary>
/// <param name="SWTImage">The SWT image.</param>
/// <param name="components">The components.</param>
/// <param name="validComponents">The valid components.</param>
/// <param name="compCenters">The comp centers.</param>
/// <param name="compMedians">The comp medians.</param>
/// <param name="compDimensions">The comp dimensions.</param>
/// <param name="compBB">The comp bb.</param>
/// <param name="params">The parameters.</param>
/// <param name="img">The img.</param>
void filterComponents(IplImage * SWTImage,
		std::vector<std::vector<Point2d> > & components,
		std::vector<std::vector<Point2d> > & validComponents,
		std::vector<Point2dFloat> & compCenters,
		std::vector<float> & compMedians, std::vector<Point2d> & compDimensions,
		std::vector<std::pair<Point2d, Point2d> > & compBB,
		const struct TextDetectionParams &params,
		IplImage * img) {
	validComponents.reserve(components.size());
	compCenters.reserve(components.size());
	compMedians.reserve(components.size());
	compDimensions.reserve(components.size());
	// bounding boxes
	compBB.reserve(components.size());
	int compIndex = -1;
	for (std::vector<std::vector<Point2d> >::iterator it = components.begin();
			it != components.end(); it++) {
		compIndex++;
		// compute the stroke width mean, variance, median
		float mean, variance, median;
		float meanColor, varianceColor, medianColor;
		int minx, miny, maxx, maxy;
		componentStats(SWTImage, (*it), mean, variance, median, minx, miny,
			maxx, maxy, meanColor, varianceColor, medianColor, img);
#ifndef NO_FILTER
		// check if variance is less than half the mean
		if (variance > 0.5 * mean) {
			continue;
		}
#endif

		/*for (std::vector<Point2d>::iterator currentIt = it->begin(); currentIt != it->end(); currentIt++)
		{
			if (it->size() > 1 && (currentIt != it->end() - 1))
			{
				byte currentValue = CV_IMAGE_ELEM(img, byte, currentIt->y, currentIt->x);
				std::vector<Point2d>::iterator nextIt = currentIt + 1;
				byte nextValue = CV_IMAGE_ELEM(img, byte, nextIt->y, nextIt->x);
				byte diff = abs(currentValue - nextValue);

			}
			
		}*/

		float length = (float) (maxx - minx + 1);
		float width = (float) (maxy - miny + 1);

		// check font height
		if (width > 300) {
			continue;
		}

		if (width < params.minCCHeight)
		{
			continue;
		}

		// check borders
		if ((miny < params.topBorder)
				|| (maxy > SWTImage->height - params.bottomBorder)) {
			continue;
		}

		float area = length * width;
		// compute the rotated bounding box
		float increment = 1. / 36.;
		for (float theta = increment * PI; theta < PI / 2.0;
				theta += increment * PI) {
			float xmin, xmax, ymin, ymax, xtemp, ytemp, ltemp, wtemp;
			xmin = 1000000;
			ymin = 1000000;
			xmax = 0;
			ymax = 0;
			for (unsigned int i = 0; i < (*it).size(); i++) {
				xtemp = (*it)[i].x * cos(theta) + (*it)[i].y * -sin(theta);
				ytemp = (*it)[i].x * sin(theta) + (*it)[i].y * cos(theta);
				xmin = std::min(xtemp, xmin);
				xmax = std::max(xtemp, xmax);
				ymin = std::min(ytemp, ymin);
				ymax = std::max(ytemp, ymax);
			}
			ltemp = xmax - xmin + 1;
			wtemp = ymax - ymin + 1;
			if (ltemp * wtemp < area) {
				area = ltemp * wtemp;
				length = ltemp;
				width = wtemp;
			}
		}

		// check if the aspect ratio is between the allowed range
		if (!ratio_within(length / width, COM_MAX_ASPECT_RATIO)) {
			continue;
		}

		//double widthToLength = width / length;

		//if (widthToLength < COM_MAX_WIDTH_TO_HEIGHT_RATIO)
		//{
		//	continue;
		//}

		// compute the diameter TODO finish
		// compute dense representation of component
		std::vector<std::vector<float> > denseRepr;
		denseRepr.reserve(maxx - minx + 1);
		for (int i = 0; i < maxx - minx + 1; i++) {
			std::vector<float> tmp;
			tmp.reserve(maxy - miny + 1);
			denseRepr.push_back(tmp);
			for (int j = 0; j < maxy - miny + 1; j++) {
				denseRepr[i].push_back(0);
			}
		}
		for (std::vector<Point2d>::iterator pit = it->begin(); pit != it->end();
				pit++) {
			(denseRepr[pit->x - minx])[pit->y - miny] = 1;
		}
		// create graph representing components
		/*const int num_nodes = it->size();

		 E edges[] = { E(0,2),
		 E(1,1), E(1,3), E(1,4),
		 E(2,1), E(2,3),
		 E(3,4),
		 E(4,0), E(4,1) };

		 Graph G(edges + sizeof(edges) / sizeof(E), weights, num_nodes);
		 */
		Point2dFloat center;
		center.x = ((float) (maxx + minx)) / 2.0;
		center.y = ((float) (maxy + miny)) / 2.0;

		Point2d dimensions;
		dimensions.x = maxx - minx + 1;
		dimensions.y = maxy - miny + 1;

		Point2d bb1;
		bb1.x = minx;
		bb1.y = miny;

		Point2d bb2;
		bb2.x = maxx;
		bb2.y = maxy;
		std::pair<Point2d, Point2d> pair(bb1, bb2);

		compBB.push_back(pair);
		compDimensions.push_back(dimensions);
		compMedians.push_back(median);
		compCenters.push_back(center);
		validComponents.push_back(*it);
	}
	std::vector<std::vector<Point2d> > tempComp;
	std::vector<Point2d> tempDim;
	std::vector<float> tempMed;
	std::vector<Point2dFloat> tempCenters;
	std::vector<std::pair<Point2d, Point2d> > tempBB;
	tempComp.reserve(validComponents.size());
	tempCenters.reserve(validComponents.size());
	tempDim.reserve(validComponents.size());
	tempMed.reserve(validComponents.size());
	tempBB.reserve(validComponents.size());
	for (unsigned int i = 0; i < validComponents.size(); i++) {
		int count = 0;
		for (unsigned int j = 0; j < validComponents.size(); j++) {
			if (i != j)
			{
			std::pair<Point2d, Point2d> compBBi = compBB[i];
			std::pair<Point2d, Point2d> compBBj = compBB[j];

				if (compBB[i].first.x <= compCenters[j].x
						&& compBB[i].second.x >= compCenters[j].x
						&& compBB[i].first.y <= compCenters[j].y
						&& compBB[i].second.y >= compCenters[j].y) {
					count++;
				}
			}
		}
		/*if (count < 2) { - commented becuase small parts (1x*1px) caused that correct components were ignored*/
			tempComp.push_back(validComponents[i]);
			tempCenters.push_back(compCenters[i]);
			tempMed.push_back(compMedians[i]);
			tempDim.push_back(compDimensions[i]);
			tempBB.push_back(compBB[i]);
		//}
	}
	validComponents = tempComp;
	compDimensions = tempDim;
	compMedians = tempMed;
	compCenters = tempCenters;
	compBB = tempBB;

	compDimensions.reserve(tempComp.size());
	compMedians.reserve(tempComp.size());
	compCenters.reserve(tempComp.size());
	validComponents.reserve(tempComp.size());
	compBB.reserve(tempComp.size());

	LOGL(LOG_COMPONENTS,
			"After filtering " << validComponents.size() << " components");

	for (unsigned int i = 0; i < validComponents.size(); i++) {
		LOGL(LOG_COMPONENTS,
				"Component (" << i << "): dim=" << compDimensions[i].x << "*" << compDimensions[i].y << " median=" << compMedians[i] << " bb=(" << compBB[i].first.x << "," << compBB[i].first.y << ")->(" << compBB[i].second.x << "," << compBB[i].second.y << ")");

	}
}

bool sharesOneEnd(Chain c0, Chain c1) {
	if (c0.p == c1.p || c0.p == c1.q || c0.q == c1.q || c0.q == c1.p) {
		return true;
	} else {
		return false;
	}
}

bool chainSortDist(const Chain &lhs, const Chain &rhs) {
	return lhs.dist < rhs.dist;
}

bool chainSortLength(const Chain &lhs, const Chain &rhs) {
	return lhs.components.size() > rhs.components.size();
}

bool includes(std::vector<int> v, std::vector<int> V)
{
	if (v.size() > V.size())
		return false;

	for (int i=0,iend=v.size();i<iend;i++)
	{
		int j;
		int jend = V.size();
		for (j=0;j<jend;j++)
		{
			if (v[i] == V[j])
				break;
		}
		if (j==jend)
			return false;
	}
	return true;
}
/// <summary>
/// Joins connected components to chains if the requirements to join are satisfied.
/// </summary>
/// <param name="colorImage">The color image.</param>
/// <param name="components">The components.</param>
/// <param name="compCenters">centers of components</param>
/// <param name="compMedians">medians of components</param>
/// <param name="compDimensions">dimensions of compoenents</param>
/// <param name="params">requirements that are checked before two components are joined to chain</param>
/// <returns></returns>
std::vector<Chain> makeChains(IplImage * colorImage,
		std::vector<std::vector<Point2d> > & components,
		std::vector<Point2dFloat> & compCenters,
		std::vector<float> & compMedians, std::vector<Point2d> & compDimensions,
		const struct TextDetectionParams &params) {
	assert(compCenters.size() == components.size());
	// make vector of color averages
	std::vector<Point3dFloat> colorAverages;
	colorAverages.reserve(components.size());
	for (std::vector<std::vector<Point2d> >::iterator it = components.begin();
			it != components.end(); it++) {
		Point3dFloat mean;
		mean.x = 0;
		mean.y = 0;
		mean.z = 0;
		int num_points = 0;
		for (std::vector<Point2d>::iterator pit = it->begin(); pit != it->end();
				pit++) {
			mean.x += (float) CV_IMAGE_ELEM(colorImage, unsigned char, pit->y,
					(pit->x) * 3);
			mean.y += (float) CV_IMAGE_ELEM(colorImage, unsigned char, pit->y,
					(pit->x) * 3 + 1);
			mean.z += (float) CV_IMAGE_ELEM(colorImage, unsigned char, pit->y,
					(pit->x) * 3 + 2);
			num_points++;
		}
		mean.x = mean.x / ((float) num_points);
		mean.y = mean.y / ((float) num_points);
		mean.z = mean.z / ((float) num_points);
		colorAverages.push_back(mean);
	}

	// form all eligible pairs and calculate the direction of each
	std::vector<Chain> chains;
	for (unsigned int i = 0; i < components.size(); i++) {
		for (unsigned int j = i + 1; j < components.size(); j++) {
			float iCompMedian = compMedians[i];
			float jCompMedian = compMedians[j];

			float compMediansRatio = iCompMedian / jCompMedian;
			float compDimRatioY = ((float) compDimensions[i].y)
					/ compDimensions[j].y;
			float compDimRatioX = ((float) compDimensions[i].x)
					/ compDimensions[j].x;
			float dist = square(compCenters[i].x - compCenters[j].x)
					+ square(compCenters[i].y - compCenters[j].y);
			float colorDist = square(colorAverages[i].x - colorAverages[j].x)
					+ square(colorAverages[i].y - colorAverages[j].y)
					+ square(colorAverages[i].z - colorAverages[j].z);
#if 0
			float maxDim = (float) square(
					std::max(std::min(compDimensions[i].x, compDimensions[i].y),
							std::min(compDimensions[j].x,
									compDimensions[j].y)));
#else
			float maxDim = (float) square(
								std::min(compDimensions[i].y,compDimensions[j].y));

#endif
			LOGL(LOG_COMP_PAIRS,
					"Pair (" << i << ":" << j << "): dist=" << dist << " colorDist=" << colorDist << " maxDim=" << maxDim << " compMediansRatio=" << compMediansRatio << " compDimRatioX=" << compDimRatioX << " compDimRatioY=" << compDimRatioY);

			if (ratio_within(compMediansRatio, COM_MAX_MEDIAN_RATIO)
					&& (ratio_within(compDimRatioY, COM_MAX_DIM_RATIO))
					&& (ratio_within(compDimRatioX, COM_MAX_DIM_RATIO))) {

				if (dist / maxDim < COM_MAX_DIST_RATIO /*&& colorDist < 6000*/) {
					Chain c;
					c.p = i;
					c.q = j;
					std::vector<int> comps;
					comps.push_back(c.p);
					comps.push_back(c.q);
					c.components = comps;
					c.dist = dist;
					float d_x = (compCenters[i].x - compCenters[j].x);
					float d_y = (compCenters[i].y - compCenters[j].y);
					/*
					 float d_x = (compBB[i].first.x - compBB[j].second.x);
					 float d_y = (compBB[i].second.y - compBB[j].second.y);
					 */
					float mag = sqrt(d_x * d_x + d_y * d_y);
					d_x = d_x / mag;
					d_y = d_y / mag;
					Point2dFloat dir;
					dir.x = d_x;
					dir.y = d_y;
					c.direction = dir;
					chains.push_back(c);

				}
			}
		}
	}

	/* print pairs */
	for (unsigned int j = 0; j < chains.size(); j++) {
		LOG(LOG_COMP_PAIRS, "Pair" << j <<":");
		for (unsigned int i = 0; i < chains[j].components.size(); i++) {
			LOG(LOG_COMP_PAIRS, chains[j].components[i] << ",");
		}
		LOGL(LOG_COMP_PAIRS, "");
	}

	LOGL(LOG_COMP_PAIRS, chains.size() << " eligible pairs");

	std::sort(chains.begin(), chains.end(), &chainSortDist);

	const float strictness = PI / 10.0;
	//merge chains
	int merges = 1;
	while (merges > 0) {
		for (unsigned int i = 0; i < chains.size(); i++) {
			chains[i].merged = false;
		}
		merges = 0;
		std::vector<Chain> newchains;
		for (unsigned int i = 0; i < chains.size(); i++) {
			for (unsigned int j = 0; j < chains.size(); j++) {
				if (i != j) {
					Chain iChain = chains[i];
					Chain jChain = chains[j];
					if (!iChain.merged && !jChain.merged
						&& sharesOneEnd(iChain, jChain)) {
						if (iChain.p == jChain.p) 
						{
							float diffDirectionX = iChain.direction.x
								* -jChain.direction.x;
							float diffDirectionY = iChain.direction.y
								* -jChain.direction.y;
							float diffSum = diffDirectionX + diffDirectionY;

							float acosVal = SafeAcos(diffSum);
							if (acosVal < strictness) {
								/*      if (chains[i].p == chains[i].q || chains[j].p == chains[j].q) {
								 std::cout << "CRAZY ERROR" << std::endl;
								 } else if (chains[i].p == chains[j].p && chains[i].q == chains[j].q) {
								 std::cout << "CRAZY ERROR" << std::endl;
								 } else if (chains[i].p == chains[j].q && chains[i].q == chains[j].p) {
								 std::cout << "CRAZY ERROR" << std::endl;
								 }
								 std::cerr << 1 <<std::endl;

								 std::cerr << chains[i].p << " " << chains[i].q << std::endl;
								 std::cerr << chains[j].p << " " << chains[j].q << std::endl;
								 std::cerr << compCenters[chains[i].q].x << " " << compCenters[chains[i].q].y << std::endl;
								 std::cerr << compCenters[chains[i].p].x << " " << compCenters[chains[i].p].y << std::endl;
								 std::cerr << compCenters[chains[j].q].x << " " << compCenters[chains[j].q].y << std::endl;
								 std::cerr << std::endl; */

								chains[i].p = chains[j].q;
								for (std::vector<int>::iterator it =
										chains[j].components.begin();
										it != chains[j].components.end();
										it++) {
									chains[i].components.push_back(*it);
								}
								float d_x = (compCenters[chains[i].p].x
										- compCenters[chains[i].q].x);
								float d_y = (compCenters[chains[i].p].y
										- compCenters[chains[i].q].y);
								chains[i].dist = d_x * d_x + d_y * d_y;

								float mag = sqrt(d_x * d_x + d_y * d_y);
								d_x = d_x / mag;
								d_y = d_y / mag;
								Point2dFloat dir;
								dir.x = d_x;
								dir.y = d_y;
								chains[i].direction = dir;
								chains[j].merged = true;
								merges++;
								/*j=-1;
								 i=0;
								 if (i == chains.size() - 1) i=-1;
								 std::stable_sort(chains.begin(), chains.end(), &chainSortLength);*/
							}
						} else if (chains[i].p == chains[j].q) {

							float diffDirectionX = iChain.direction.x
								* jChain.direction.x;
							float diffDirectionY = iChain.direction.y
								* jChain.direction.y;
							float diffSum = diffDirectionX + diffDirectionY;

							float acosVal = SafeAcos(diffSum);

							if (acosVal < strictness) 
							{
								/*
								 if (chains[i].p == chains[i].q || chains[j].p == chains[j].q) {
								 std::cout << "CRAZY ERROR" << std::endl;
								 } else if (chains[i].p == chains[j].p && chains[i].q == chains[j].q) {
								 std::cout << "CRAZY ERROR" << std::endl;
								 } else if (chains[i].p == chains[j].q && chains[i].q == chains[j].p) {
								 std::cout << "CRAZY ERROR" << std::endl;
								 }
								 std::cerr << 2 <<std::endl;

								 std::cerr << chains[i].p << " " << chains[i].q << std::endl;
								 std::cerr << chains[j].p << " " << chains[j].q << std::endl;
								 std::cerr << chains[i].direction.x << " " << chains[i].direction.y << std::endl;
								 std::cerr << chains[j].direction.x << " " << chains[j].direction.y << std::endl;
								 std::cerr << compCenters[chains[i].q].x << " " << compCenters[chains[i].q].y << std::endl;
								 std::cerr << compCenters[chains[i].p].x << " " << compCenters[chains[i].p].y << std::endl;
								 std::cerr << compCenters[chains[j].p].x << " " << compCenters[chains[j].p].y << std::endl;
								 std::cerr << std::endl; */

								chains[i].p = chains[j].p;
								for (std::vector<int>::iterator it =
										chains[j].components.begin();
										it != chains[j].components.end();
										it++) {
									chains[i].components.push_back(*it);
								}
								float d_x = (compCenters[chains[i].p].x
										- compCenters[chains[i].q].x);
								float d_y = (compCenters[chains[i].p].y
										- compCenters[chains[i].q].y);
								float mag = sqrt(d_x * d_x + d_y * d_y);
								chains[i].dist = d_x * d_x + d_y * d_y;

								d_x = d_x / mag;
								d_y = d_y / mag;

								Point2dFloat dir;
								dir.x = d_x;
								dir.y = d_y;
								chains[i].direction = dir;
								chains[j].merged = true;
								merges++;
								/*j=-1;
								 i=0;
								 if (i == chains.size() - 1) i=-1;
								 std::stable_sort(chains.begin(), chains.end(), &chainSortLength); */
							}
						} else if (chains[i].q == chains[j].p) 
						{
							float diffDirectionX = iChain.direction.x
								* jChain.direction.x;
							float diffDirectionY = iChain.direction.y
								* jChain.direction.y;
							float diffSum = diffDirectionX + diffDirectionY;

							float acosVal = SafeAcos(diffSum);
							if (acosVal < strictness) {
								/*                           if (chains[i].p == chains[i].q || chains[j].p == chains[j].q) {
								 std::cout << "CRAZY ERROR" << std::endl;
								 } else if (chains[i].p == chains[j].p && chains[i].q == chains[j].q) {
								 std::cout << "CRAZY ERROR" << std::endl;
								 } else if (chains[i].p == chains[j].q && chains[i].q == chains[j].p) {
								 std::cout << "CRAZY ERROR" << std::endl;
								 }
								 std::cerr << 3 <<std::endl;

								 std::cerr << chains[i].p << " " << chains[i].q << std::endl;
								 std::cerr << chains[j].p << " " << chains[j].q << std::endl;

								 std::cerr << compCenters[chains[i].p].x << " " << compCenters[chains[i].p].y << std::endl;
								 std::cerr << compCenters[chains[i].q].x << " " << compCenters[chains[i].q].y << std::endl;
								 std::cerr << compCenters[chains[j].q].x << " " << compCenters[chains[j].q].y << std::endl;
								 std::cerr << std::endl; */
								chains[i].q = chains[j].q;
								for (std::vector<int>::iterator it =
										chains[j].components.begin();
										it != chains[j].components.end();
										it++) {
									chains[i].components.push_back(*it);
								}
								float d_x = (compCenters[chains[i].p].x
										- compCenters[chains[i].q].x);
								float d_y = (compCenters[chains[i].p].y
										- compCenters[chains[i].q].y);
								float mag = sqrt(d_x * d_x + d_y * d_y);
								chains[i].dist = d_x * d_x + d_y * d_y;

								d_x = d_x / mag;
								d_y = d_y / mag;
								Point2dFloat dir;
								dir.x = d_x;
								dir.y = d_y;

								chains[i].direction = dir;
								chains[j].merged = true;
								merges++;
								/*j=-1;
								 i=0;
								 if (i == chains.size() - 1) i=-1;
								 std::stable_sort(chains.begin(), chains.end(), &chainSortLength); */
							}
						} else if (chains[i].q == chains[j].q) {

							float diffDirectionX = iChain.direction.x
								* -jChain.direction.x;
							float diffDirectionY = iChain.direction.y
								* -jChain.direction.y;
							float diffSum = diffDirectionX + diffDirectionY;

							float acosVal = SafeAcos(diffSum);

							if (acosVal < strictness) {
								/*           if (chains[i].p == chains[i].q || chains[j].p == chains[j].q) {
								 std::cout << "CRAZY ERROR" << std::endl;
								 } else if (chains[i].p == chains[j].p && chains[i].q == chains[j].q) {
								 std::cout << "CRAZY ERROR" << std::endl;
								 } else if (chains[i].p == chains[j].q && chains[i].q == chains[j].p) {
								 std::cout << "CRAZY ERROR" << std::endl;
								 }
								 std::cerr << 4 <<std::endl;
								 std::cerr << chains[i].p << " " << chains[i].q << std::endl;
								 std::cerr << chains[j].p << " " << chains[j].q << std::endl;
								 std::cerr << compCenters[chains[i].p].x << " " << compCenters[chains[i].p].y << std::endl;
								 std::cerr << compCenters[chains[i].q].x << " " << compCenters[chains[i].q].y << std::endl;
								 std::cerr << compCenters[chains[j].p].x << " " << compCenters[chains[j].p].y << std::endl;
								 std::cerr << std::endl; */
								chains[i].q = chains[j].p;
								for (std::vector<int>::iterator it =
										chains[j].components.begin();
										it != chains[j].components.end();
										it++) {
									chains[i].components.push_back(*it);
								}
								float d_x = (compCenters[chains[i].p].x
										- compCenters[chains[i].q].x);
								float d_y = (compCenters[chains[i].p].y
										- compCenters[chains[i].q].y);
								chains[i].dist = d_x * d_x + d_y * d_y;

								float mag = sqrt(d_x * d_x + d_y * d_y);
								d_x = d_x / mag;
								d_y = d_y / mag;
								Point2dFloat dir;
								dir.x = d_x;
								dir.y = d_y;
								chains[i].direction = dir;
								chains[j].merged = true;
								merges++;
								/*j=-1;
								 i=0;
								 if (i == chains.size() - 1) i=-1;
								 std::stable_sort(chains.begin(), chains.end(), &chainSortLength);*/
							}
						}
					}
				}
			}
		}
		for (unsigned int i = 0; i < chains.size(); i++) {
			if (!chains[i].merged) {
				newchains.push_back(chains[i]);
			}
		}
		chains = newchains;
		std::stable_sort(chains.begin(), chains.end(), &chainSortLength);
	}

	std::vector<Chain> newchains;
	newchains.reserve(chains.size());

	/* sort chains and remove duplicate components within chains */
	for (std::vector<Chain>::iterator cit = chains.begin(); cit != chains.end();
			cit++) {

		/* remove duplicate components */
		std::sort(cit->components.begin(), cit->components.end());
		cit->components.erase(
		std::unique(cit->components.begin(), cit->components.end()),
		cit->components.end());
	}

	/* now add all chains */
	for (int i=0,iend=chains.size(); i<iend; i++)
	{
		/* only add chains longer than minimum size */
		if (chains[i].components.size() < params.minChainLen) {
			LOGL(LOG_CHAINS, "Reject chain " << i << " on size ");
			break;
		}

		/* now make sure that chain is not already included
		 * in another chain */
		int j;
		for (j=0; j<iend; j++)
		{
			if ( (i!=j) && (includes(chains[i].components, chains[j].components)))
				break;
		}
		if (j<iend)
		{
			LOGL(LOG_CHAINS, "Reject chain " << i << " already included in chain " << j);
			break;
		}

		/* all good, add that chain */
		newchains.push_back(chains[i]);
	}

	chains = newchains;

	/* print chains */
	for (unsigned int j = 0; j < chains.size(); j++) {
		LOG(LOG_CHAINS, "Chain" << j <<":");
		for (unsigned int i = 0; i < chains[j].components.size(); i++) {
			LOG(LOG_CHAINS, chains[j].components[i] << ",");
		}
		LOGL(LOG_CHAINS, "");
	}

	LOGL(LOG_CHAINS, chains.size() << " chains after merging");

	return chains;
}

float SafeAcos(float x)
{
	if (x < -1.0) x = -1.0;
	else if (x > 1.0) x = 1.0;
	return acos(x);
}