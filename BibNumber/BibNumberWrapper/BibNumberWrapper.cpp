// This is the main DLL file.

#include "stdafx.h"

#include "BibNumberWrapper.h"

#include <vector>
#include <msclr\marshal_cppstd.h>
#include "pipeline.h"
#include "batch.h"

//#include "opencv2/objdetect/objdetect.hpp"
//#include "opencv2/highgui/highgui.hpp"
//#include "opencv2/imgproc/imgproc.hpp"


double BibNumberWrapper::Class1::DetectNumbers(System::String^ filename)
{
	
	std::string unmanagedFileName = msclr::interop::marshal_as<std::string>(filename);
	pipeline::Pipeline pipeline;
	std::vector<int> bibNumbers;
	//cv::Mat matc = cv::imread(unmanagedFileName, 1);
	int res = batch::processSingleImage(unmanagedFileName, "", pipeline, bibNumbers);
	return res;
}

