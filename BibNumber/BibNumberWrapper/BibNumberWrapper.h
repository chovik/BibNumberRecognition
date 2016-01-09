// BibNumberWrapper.h

#pragma once

#include <vector>

typedef System::Collections::Generic::List<int> MyList;
typedef std::vector<int> MyVector;

namespace BibNumberWrapper {

	public ref class Class1
	{
		// TODO: Add your methods for this class here.
	public:
		MyList^ DetectNumbers(System::String^ filename);
	};
}
