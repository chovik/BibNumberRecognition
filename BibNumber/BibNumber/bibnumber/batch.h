#ifndef BATCH_H
#define BATCH_H

#include <string>
#include <vector>
#include <boost/filesystem.hpp>
#include "pipeline.h"
#include "opencv2/objdetect/objdetect.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

namespace batch
{
	bool isImageFile(std::string name);
	std::vector<boost::filesystem::path> getImageFiles(std::string dir);
	int process(std::string inputName, std::string svmModel);

	static int processSingleImage(
		std::string fileName,
		std::string svmModel,
		pipeline::Pipeline &pipeline,
		std::vector<int>& bibNumbers)
	{
		int res;

		std::cout << "Processing file " << fileName << std::endl;

		/* open image */
		cv::Mat image = cv::imread(fileName, 1);
		if (image.empty()) {
			std::cerr << "ERROR:Failed to open image file" << std::endl;
			return -1;
		}

		/* process image */
		res = pipeline.processImage(image, svmModel, bibNumbers);
		if (res < 0) {
			std::cerr << "ERROR: Could not process image" << std::endl;
			return -1;
		}

		/* remove duplicates */
		std::sort(bibNumbers.begin(), bibNumbers.end());
		bibNumbers.erase(std::unique(bibNumbers.begin(), bibNumbers.end()),
			bibNumbers.end());

		/* display result */
		std::cout << "Read: [";
		for (std::vector<int>::iterator it = bibNumbers.begin();
			it != bibNumbers.end(); ++it) {
			std::cout << " " << *it;
		}
		std::cout << "]" << std::endl;

		return res;
	}
}

#endif /* #ifndef BATCH_H */
