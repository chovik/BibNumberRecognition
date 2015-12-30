// This is the main DLL file.

#include "stdafx.h"

#include "BibNumberWrapper.h"

#include <vector>
#include <msclr\marshal_cppstd.h>
#include "pipeline.h"
#include "batch.h"


double BibNumberWrapper::Class1::DetectNumbers(System::String^ filename)
{
	std::string unmanagedFileName = msclr::interop::marshal_as<std::string>(filename);
	pipeline::Pipeline pipeline;
	std::vector<int> bibNumbers;
	int res = batch::processSingleImage(unmanagedFileName, NULL, pipeline, bibNumbers);
	return res;
}

