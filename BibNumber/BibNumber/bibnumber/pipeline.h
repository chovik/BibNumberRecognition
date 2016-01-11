#ifndef PIPELINE_H
#define PIPELINE_H

#include "opencv2/imgproc/imgproc.hpp"
#include "textdetection.h"
#include "textrecognition.h"

namespace pipeline
{
	class Pipeline {
	public:		
		/// <summary>
		/// Processes the image to detect and recognize bib numbers.
		/// </summary>
		/// <param name="img">The image that is used to detect and recognize bibnumbers</param>
		/// <param name="svmModel">The SVM model.</param>
		/// <param name="bibNumbers">The collection of found bibnumbers.</param>
		/// <returns>0 if no error occured during the process.</returns>
		int processImage(cv::Mat& img, std::string svmModel, std::vector<int>& bibNumbers);
	private:
		textdetection::TextDetector textDetector;
		textrecognition::TextRecognizer textRecognizer;
	};

}

#endif /* #ifndef PIPELINE_H */

