#include "MainGame.h"


int main(int argumentCount, char* arguments[])
{
	try
	{
		const std::string startupScene = argumentCount > 1
			? arguments[1]
			: std::string{};

		MainGame game({
			{1024, 768},
			dx3d::Logger::LogLevel::Info,
			startupScene
		});
		game.run();
	}
	catch (const std::runtime_error&)
	{
		return EXIT_FAILURE;
	}
	catch (const std::invalid_argument&)
	{
		return EXIT_FAILURE;
	}
	catch (const std::exception&)
	{
		return EXIT_FAILURE;
	}
	catch (...)
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
