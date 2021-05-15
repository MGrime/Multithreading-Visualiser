#include <iostream>
#include <memory>

#include "simulator.hpp"

int main()
{
	try
	{
		std::unique_ptr<simulator> mySim = std::make_unique<simulator>();

		mySim->run();
	}
	catch (std::exception& e)
	{
		std::cerr << "Something went wrong! Msg: " << e.what();
		return -1;
	}

	return 0;
}
