#include <boost/algorithm/string/trim.hpp>

#include <tesseract/baseapi.h>
#include <tesseract/strngs.h>
#include <tesseract/genericvector.h>

#include <opencv/cv.h>
#include <opencv/highgui.h>
#include <opencv2/ml/ml.hpp>

#include "train.h"

#include "textrecognition.h"
#include "log.h"
#include "stdio.h"

#define PI 3.14159265

/// <summary>
/// Fixes wrongly recognized numbers. If the recognized character is similiar to a digit, character is replaced by the digit.
/// </summary>
/// <param name="s">string to fix. string where characters will be replaced if they are similiar to a digit.</param>
/// <returns>srings where characters similiar to the digit are fixed and changed to the digit. 
/// A is replaced by 4, B by 8, g by 9, I, l by 1 and T by7</returns>
static std::string result_correction(const std::string& s) {
	std::string result = "";
	std::locale loc;
	std::string::const_iterator it = s.begin();
	bool numberVisited = false;
	bool processingEnd = false;
	while (it != s.end())
	{
		if (!std::isdigit(*it, loc))
		{
			if (*it == 'A')
			{
				numberVisited = true;
				result += '4';
			}
			else if (*it == 'B')
			{
				numberVisited = true;
				result += '8';
			}
			else if (*it == 'g')
			{
				numberVisited = true;
				result += '9';
			}
			else if (*it == 'I')
			{
				numberVisited = true;
				result += '1';
			}
			else if (*it == 'l')
			{
				numberVisited = true;
				result += '1';
			}
			else if (*it == 'T')
			{
				numberVisited = true;
				result += '7';
			}
			else
			{
				result += *it;
			}
		}
		else
		{
			result += *it;
		}

		++it;
	}

	return result;
}

/// <summary>
/// Removes characters, which are not digits and are in the prefix or postfix.
/// CD123HG is changed to 123. 
/// This method is used when in the created chain are not only digits but there is some object at the beginning or at the end of chain.
/// </summary>
/// <param name="s">string to process</param>
/// <returns>string with removed non-digit characters.  </returns>
static std::string trim_number(const std::string& s) {
	std::string result = "";
	std::locale loc;
	std::string::const_iterator it = s.begin();
	bool numberVisited = false;
	bool processingEnd = false;
	while (it != s.end())
	{
		if (!std::isdigit(*it, loc))
		{
			if (*it == 'A')
			{
				numberVisited = true;
				result += '4';
			}
			else if(*it == 'B')
			{
				numberVisited = true;
				result += '8';
			}
			else
			if (numberVisited)
			{
				processingEnd = true;
			}
		}
		else
		{
			if (processingEnd)
			{
				result = s;
				break;
			}
			numberVisited = true;
			result += *it;
		}

		++it;
	}
		
	return result;
}

/// <summary>
/// Checks if the string is number or not.
/// </summary>
/// <param name="s">string to be checked</param>
/// <returns>true if string is valid number. false otherwise</returns>
static bool is_number(const std::string& s) {
	std::locale loc;
	std::string::const_iterator it = s.begin();
	while (it != s.end() && std::isdigit(*it, loc))
		++it;
	return !s.empty() && it == s.end();
}

/// <summary>
/// Gets absolute value.
/// </summary>
/// <param name="x">input value</param>
/// <returns>absolute value</returns>
static double absd(double x) {
	return x > 0 ? x : -x;
}

/// <summary>
/// Gets square value.
/// </summary>
/// <param name="x">input number</param>
/// <returns>x^2</returns>
static inline double square(double x) {
	return x * x;
}

/// <summary>
/// Gets the bounding box.
/// </summary>
/// <param name="vec">points</param>
/// <param name="clip">The clip.</param>
/// <returns></returns>
static cv::Rect getBoundingBox(std::vector<cv::Point> vec, cv::Size clip) {
	int minx = clip.width - 1, miny = clip.height - 1, maxx = 0, maxy = 0;
	for (std::vector<cv::Point>::iterator it = vec.begin(); it != vec.end();
			it++) {
		if (it->x < minx)
			minx = std::max(it->x, 0);
		if (it->y < miny)
			miny = std::max(it->y, 0);
		if (it->x > maxx)
			maxx = std::min(it->x, clip.width - 1);
		if (it->y > maxy)
			maxy = std::min(it->y, clip.height - 1);
	}
	return cv::Rect(cv::Point(minx, miny), cv::Point(maxx, maxy));
}

cv::Scalar getMSSIM(const cv::Mat& i1, const cv::Mat& i2) {
	const double C1 = 6.5025, C2 = 58.5225;
	/***************************** INITS **********************************/
	int d = CV_32F;

	cv::Mat I1, I2;
	i1.convertTo(I1, d);           // cannot calculate on one byte large values
	i2.convertTo(I2, d);

	cv::Mat I2_2 = I2.mul(I2);        // I2^2
	cv::Mat I1_2 = I1.mul(I1);        // I1^2
	cv::Mat I1_I2 = I1.mul(I2);        // I1 * I2

	/***********************PRELIMINARY COMPUTING ******************************/

	cv::Mat mu1, mu2;   //
	cv::GaussianBlur(I1, mu1, cv::Size(11, 11), 1.5);
	cv::GaussianBlur(I2, mu2, cv::Size(11, 11), 1.5);

	cv::Mat mu1_2 = mu1.mul(mu1);
	cv::Mat mu2_2 = mu2.mul(mu2);
	cv::Mat mu1_mu2 = mu1.mul(mu2);

	cv::Mat sigma1_2, sigma2_2, sigma12;

	cv::GaussianBlur(I1_2, sigma1_2, cv::Size(11, 11), 1.5);
	sigma1_2 -= mu1_2;

	cv::GaussianBlur(I2_2, sigma2_2, cv::Size(11, 11), 1.5);
	sigma2_2 -= mu2_2;

	cv::GaussianBlur(I1_I2, sigma12, cv::Size(11, 11), 1.5);
	sigma12 -= mu1_mu2;

	///////////////////////////////// FORMULA ////////////////////////////////
	cv::Mat t1, t2, t3;

	t1 = 2 * mu1_mu2 + C1;
	t2 = 2 * sigma12 + C2;
	t3 = t1.mul(t2);              // t3 = ((2*mu1_mu2 + C1).*(2*sigma12 + C2))

	t1 = mu1_2 + mu2_2 + C1;
	t2 = sigma1_2 + sigma2_2 + C2;
	t1 = t1.mul(t2);   // t1 =((mu1_2 + mu2_2 + C1).*(sigma1_2 + sigma2_2 + C2))

	cv::Mat ssim_map;
	divide(t3, t1, ssim_map);      // ssim_map =  t3./t1;

	cv::Scalar mssim = mean(ssim_map); // mssim = average of ssim map
	return mssim;
}

namespace textrecognition {

TextRecognizer::TextRecognizer() {
	GenericVector<STRING> pars_keys;
	GenericVector<STRING> pars_vals;
	/*pars_keys.push_back("load_system_dawg");
	 pars_vals.push_back("F");
	 pars_keys.push_back("load_freq_dawg");
	 pars_vals.push_back("F");
	 pars_keys.push_back("load_punc_dawg");
	 pars_vals.push_back("F");
	 pars_keys.push_back("load_number_dawg");
	 pars_vals.push_back("F");
	 pars_keys.push_back("load_unambig_dawg");
	 pars_vals.push_back("F");
	 pars_keys.push_back("load_bigram_dawg");
	 pars_vals.push_back("F");
	 pars_keys.push_back("load_fixed_length_dawgs");
	 pars_vals.push_back("F");*/
	tess.Init(NULL, "eng", tesseract::OEM_DEFAULT, NULL, 0, &pars_keys,
			&pars_vals, false);
#if 0
	tess.SetVariable("tessedit_char_whitelist", "0123456789");
#endif
	tess.SetVariable("tessedit_write_images", "true");
	tess.SetPageSegMode(tesseract::PSM_SINGLE_WORD);

	/* initialize sequence ids */
	bsid = 0;
	dsid = 0;
}

TextRecognizer::~TextRecognizer(void) {
	tess.Clear();
	tess.End();
}

/// <summary>
/// Checks the height of the chain. 
/// </summary>
/// <param name="chainIndex">Index of the chain.</param>
/// <param name="params">The parameters that are used to verify height of chain.</param>
/// <param name="chains">Found chains that consist of connected components. Connected components are character candidates.</param>
/// <param name="compBB">Areas of connected components. Every item in compBB represents area of one connected component.</param>
/// <param name="chainBB">Areas of chains. Every item in chainBB represents area of one chain.  Area of a chain is computed by union of all areas of connected components that are part of the chain</param>
/// <returns>If chain contains component of lower height than required minimum returns false. Otherwise returns true.</returns>
bool CheckChainHeight(int chainIndex, const struct TextDetectionParams &params,
	std::vector<Chain> &chains,
	std::vector<std::pair<Point2d, Point2d> > &compBB,
	std::vector<std::pair<CvPoint, CvPoint> > &chainBB)
{
	int minHeight = chainBB[chainIndex].second.y - chainBB[chainIndex].first.y;
	//iterated through all components in the chain to find the component with lowest height 
	for (unsigned j = 0; j < chains[chainIndex].components.size(); j++) {
		minHeight = std::min(minHeight,
			compBB[chains[chainIndex].components[j]].second.y
			- compBB[chains[chainIndex].components[j]].first.y);
	}

	if (minHeight < params.minCharacterheight) {
		LOGL(LOG_CHAINS,
			"Reject chain # " << chainIndex << " minHeight=" << minHeight << "<" << params.minCharacterheight);
		return false;
	}

	return true;
}

/// <summary>
/// Gets the and binarize only selected components.
/// </summary>
/// <param name="componentsImg">image where the selected components will be rendered</param>
/// <param name="grayMat">The gray mat.</param>
/// <param name="compCoords">vector will be filled with coordinates of selected componets.</param>
/// <param name="chainIndex">Index of the chain.</param>
/// <param name="params">The parameters that are used to verify height of chain.</param>
/// <param name="chains">Found chains that consist of connected components. Connected components are character candidates.</param>
/// <param name="compBB">Areas of connected components. Every item in compBB represents area of one connected component.</param>
/// <param name="chainBB">Areas of chains. Every item in chainBB represents area of one chain.  Area of a chain is computed by union of all areas of connected components that are part of the chain</param>
void GetAndBinarizeOnlySelectedComponents(cv::Mat& componentsImg, cv::Mat& grayMat, std::vector<cv::Point>& compCoords, int chainIndex, const struct TextDetectionParams &params,
	std::vector<Chain> &chains,
	std::vector<std::pair<Point2d, Point2d> > &compBB,
	std::vector<std::pair<CvPoint, CvPoint> > &chainBB)
{
	int i = chainIndex;
	for (unsigned int j = 0; j < chains[i].components.size(); j++)
	{
		int component_id = chains[i].components[j];
		cv::Rect roi = cv::Rect(compBB[component_id].first.x,
			compBB[component_id].first.y,
			compBB[component_id].second.x
			- compBB[component_id].first.x,
			compBB[component_id].second.y
			- compBB[component_id].first.y);
		cv::Mat componentRoi = grayMat(roi);

		compCoords.push_back(
			cv::Point(compBB[component_id].first.x,
			compBB[component_id].first.y));
		compCoords.push_back(
			cv::Point(compBB[component_id].second.x,
			compBB[component_id].second.y));
		compCoords.push_back(
			cv::Point(compBB[component_id].first.x,
			compBB[component_id].second.y));
		compCoords.push_back(
			cv::Point(compBB[component_id].second.x,
			compBB[component_id].first.y));

		cv::Mat thresholded;
		cv::threshold(componentRoi, thresholded, 0 // the value doesn't matter for Otsu thresholding
			, 255 // we could choose any non-zero value. 255 (white) makes it easy to see the binary image
			, cv::THRESH_OTSU | cv::THRESH_BINARY_INV);

		IplImage * thresholdedImage = cvCreateImage(cvSize(thresholded.cols, thresholded.rows), IPL_DEPTH_32F, 1);

		IplImage* thresh = cvCloneImage(&(IplImage)thresholded);

		for (int row = 0; row < thresholded.rows; row++)
		{
			for (int col = 0; col < thresholded.cols; col++)
			{
				CV_IMAGE_ELEM(thresholdedImage, float, row, col) = thresholded.at<byte>(row, col);
			}
		}
		std::vector<Ray> rays;
		std::vector<std::vector<Point2d> > components = findLegallyConnectedComponents(thresholdedImage, rays, thresh);

		int maxInnerComponentArea = 0;
		int maxInnerComponentIndex = -1;
		int currentInnerComponentIndex = -1;
		for (std::vector<std::vector<Point2d> >::iterator it = components.begin();
			it != components.end(); it++)
		{
			currentInnerComponentIndex++;
			float mean, variance, median;
			int minx, miny, maxx, maxy;
			float meanColor, varianceColor, medianColor;
			componentStats(thresholdedImage, (*it), mean, variance, median, minx, miny,
				maxx, maxy,
				meanColor, varianceColor, medianColor, thresh);

			Point2d bb1;
			bb1.x = minx;
			bb1.y = miny;

			Point2d bb2;
			bb2.x = maxx;
			bb2.y = maxy;
			std::pair<Point2d, Point2d> pair(bb1, bb2);

			int width = maxx - minx + 1;
			int height = maxy - miny + 1;

			int area = width * height;

			if (area > maxInnerComponentArea)
			{
				maxInnerComponentArea = area;
				maxInnerComponentIndex = currentInnerComponentIndex;
			}
		}

		IplImage * thresholdedMaxComp = cvCreateImage(cvSize(componentRoi.cols, componentRoi.rows), IPL_DEPTH_8U, 1);
		cvSet(thresholdedMaxComp, cvScalar(0));

		if (maxInnerComponentIndex >= 0)
		{
			std::vector<Point2d> maxComp = components[maxInnerComponentIndex];
			for (std::vector<Point2d>::iterator pit = maxComp.begin(); pit != maxComp.end(); pit++)
			{
				CV_IMAGE_ELEM(thresholdedMaxComp, byte, pit->y, pit->x) = 255;
			}
		}


#if 0
		cv::Moments mu = cv::moments(thresholded, true);
		std::cout << "mu02=" << mu.mu02 << " mu11=" << mu.mu11 << " skew="
			<< mu.mu11 / mu.mu02 << std::endl;
#endif

		if (roi.width != 0
			&& roi.height != 0
			&& thresholdedMaxComp->width != 0
			&& thresholdedMaxComp->height != 0)
		{
			cv::Mat thresholdMat(thresholdedMaxComp);
			thresholdMat.copyTo(componentsImg(roi));
		}
		
	}
}

void CheckRecognizedString(char* out,
	int chainIndex,
	const struct TextDetectionParams &params,
	std::vector<Chain> &chains,
	std::vector<std::pair<Point2d, Point2d> > &compBB,
	std::vector<std::pair<CvPoint, CvPoint> > &chainBB,
	std::vector<std::string>& text)
{
	int i = chainIndex;
	if (strlen(out) == 0) {
		return;
	}
	std::string s_out(out);
	boost::algorithm::trim(s_out);

	//if (s_out.size() != chains[i].components.size()) {
	//	LOGL(LOG_TEXTREC,
	//		"Text size mismatch: expected " << chains[i].components.size() << " digits, got '" << s_out << "' (" << s_out.size() << " digits)");
	//	return;
	//}
	/* if first character is a '0' we have a partially occluded number */
	if (s_out[0] == '0')
	{
		LOGL(LOG_TEXTREC, "Text begins with '0' (partially occluded)");
		return;
	}

	if (!is_number(s_out))
	{
		std::string correctedOut = result_correction(s_out);
		std::string newOut = trim_number(correctedOut);
		if (!is_number(newOut))
		{
			LOGL(LOG_TEXTREC, "Text is not a number ('" << s_out << "')");
			return;
		}
		else
		{
			s_out = newOut;
		}
	}

	/* all fine, add this bib number */
bibnumber_succ:
	text.push_back(s_out);
	LOGL(LOG_TEXTREC, "Bib number: '" << s_out << "'");
}


/// <summary>
/// This is the main method used to recognize numbers on the input image.
/// </summary>
/// <param name="input">the input image used to recognize numbers</param>
/// <param name="params">The parameters that are used to verify if the found result is valid or not.
/// Main reason to use this parameters is to avoid false detected numbers.</param>
/// <param name="svmModel">The SVM model.</param>
/// <param name="chains">Found chains that consist of connected components. Connected components are character candidates.</param>
/// <param name="compBB">Areas of connected components. Every item in compBB represents area of one connected component.</param>
/// <param name="chainBB">Areas of chains. Every item in chainBB represents area of one chain.  Area of a chain is computed by union of all areas of connected components that are part of the chain</param>
/// <param name="text">collection which will be filled by found numbers</param>
/// <returns>
/// 0 if no error occured
/// </returns>
int TextRecognizer::recognize(IplImage *input,
		const struct TextDetectionParams &params, std::string svmModel,
		std::vector<Chain> &chains,
		std::vector<std::pair<Point2d, Point2d> > &compBB,
		std::vector<std::pair<CvPoint, CvPoint> > &chainBB,
		std::vector<std::string>& text) {
	CvSize size = cvGetSize(input);
	
	//checks if image is not empty
	if (size.height > 0
		&& size.width > 0)
	{		
		//converts image to grayscale
		IplImage * grayImage = cvCreateImage(cvGetSize(input), IPL_DEPTH_8U, 1);
		cvCvtColor(input, grayImage, CV_RGB2GRAY);

		for (unsigned int i = 0; i < chainBB.size(); i++)
		{
			cv::Point center = cv::Point(
				(chainBB[i].first.x + chainBB[i].second.x) / 2,
				(chainBB[i].first.y + chainBB[i].second.y) / 2);

			//checks if the width of the chain is not too big
			if (chainBB[i].second.x - chainBB[i].first.x
				< input->width / params.maxImgWidthToTextRatio)
			{
				LOGL(LOG_TXT_ORIENT,
					"Reject chain #" << i << " width=" << (chainBB[i].second.x - chainBB[i].first.x) << "<" << (input->width / params.maxImgWidthToTextRatio));
				continue;
			}

			/* eliminates chains with components of lower height than required minimum */
			CheckChainHeight(i, params, chains, compBB, chainBB);

			/* invert direction if angle is in 3rd/4th quadrants */
			if (chains[i].direction.x < 0) {
				chains[i].direction.x = -chains[i].direction.x;
				chains[i].direction.y = -chains[i].direction.y;
			}

			/* work out chain angle */
			double theta_deg = 180
				* atan2(chains[i].direction.y, chains[i].direction.x) / PI;

			//if (absd(theta_deg) > params.maxAngle) {
			//	LOGL(LOG_TXT_ORIENT,
			//		"Chain angle " << theta_deg << " exceeds max " << params.maxAngle);
			//	continue;
			//}

			LOGL(LOG_TXT_ORIENT,
				"Chain #" << i << " Angle: " << theta_deg << " degrees");

			/* create copy of input image including only the selected components
			first image is thresholded with Otsu and then the largest connected components is found. 
			This connected components will be passed to Tesseract OCR library to recognize numbers.
			*/
			cv::Mat inputMat = cv::Mat(input);
			cv::Mat grayMat = cv::Mat(grayImage);
			cv::Mat componentsImg = cv::Mat::zeros(grayMat.rows, grayMat.cols,
				grayMat.type());
			std::vector<cv::Point> compCoords;
			GetAndBinarizeOnlySelectedComponents(componentsImg, grayMat, compCoords, i, params, chains, compBB, chainBB);
			cv::imwrite("bib-components.png", componentsImg);

			cv::Mat rotMatrix = cv::getRotationMatrix2D(center, theta_deg, 1.0);

			cv::Mat rotatedMat = cv::Mat::zeros(grayMat.rows, grayMat.cols,
				grayMat.type());
			cv::warpAffine(componentsImg, rotatedMat, rotMatrix, rotatedMat.size());
			cv::imwrite("bib-rotated.png", rotatedMat);

			/* rotate each component coordinates */
			const int border = 3;
			cv::transform(compCoords, compCoords, rotMatrix);
			/* find bounding box of rotated components */
			cv::Rect roi = getBoundingBox(compCoords,
				cv::Size(input->width, input->height));
			/* ROI area can be null if outside of clipping area */
			if ((roi.width == 0) || (roi.height == 0))
				continue;
			LOGL(LOG_TEXTREC, "ROI = " << roi);
			cv::Mat mat = cv::Mat::zeros(roi.height + 2 * border,
				roi.width + 2 * border, grayMat.type());
			cv::Mat tmp = rotatedMat(roi);
			/* copy bounded box from rotated mat to new mat with borders - borders are needed
			 * to improve OCR success rate
			 */
			tmp.copyTo(
				mat(
				cv::Rect(cv::Point(border, border),
				cv::Point(roi.width + border,
				roi.height + border))));

			/* resize image to improve OCR success rate */
			float upscale = 3.0;
			cv::resize(mat, mat, cvSize(0, 0), upscale, upscale);
			/* erode text to get rid of thin joints */
			int s = (int)(0.05 * mat.rows); /* 5% of up-scaled size) */
			cv::Mat elem = cv::getStructuringElement(cv::MORPH_ELLIPSE,
				cv::Size(2 * s + 1, 2 * s + 1), cv::Point(s, s));
			//cv::erode(mat, mat, elem);
			cv::imwrite("bib-tess-input.png", mat);

			// Pass it to Tesseract API
			tess.SetImage((uchar*)mat.data, mat.cols, mat.rows, 1, mat.step1());
			// Get the text
			char* out = tess.GetUTF8Text();
			CheckRecognizedString(out, i, params, chains, compBB, chainBB, text);	
			free(out);
		}

		cvReleaseImage(&grayImage);
		std::cout << "recognize END--- " << std::endl;
	}

	return 0;

}

} /* namespace textrecognition */

