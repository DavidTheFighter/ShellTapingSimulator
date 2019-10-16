
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream> 
#include <fstream>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <random>
#include <iomanip>

#include <lodepng.h>
#include <heatmap.h>
#include <colorschemes/RdYlBu.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

#include <JobSystem.h>
#include <ShellSimulation.h>
#include <EvolutionSimulation.h>

enum OutputType
{
	OUTPUT_TYPE_LAYERMAP_PNG,
	OUTPUT_TYPE_HEATMAP_PNG
};

// -- Command line inputs -- //

std::string inputConfigFile = "sim-config.json";
std::string inputShellConfigFile = "shell-config.json";

bool findTapingConfig = false;
int32_t calcError = -1; // When not searching, computes and prints the error of the computed layermap, if -1 no error is calculated, if positive then that is the target number of layers

void parseCommandLineArgs(int argc, char *argv[]);
void printHelp();
void writeOutput(std::string file, OutputType type, uint16_t *layermapImage, uint32_t layermapSize);

ShellConfig loadShellConfig(const std::string &file);
SimulationConfig loadSimulationConfig(const std::string &file);
EvolutionConfig loadEvolutionConfig(const std::string &file);

int main(int argc, char *argv[])
{
	parseCommandLineArgs(argc, argv);

	std::unique_ptr<JobSystem> jobSystemInstance(new JobSystem(16));
	JobSystem::setInstance(jobSystemInstance.get());

	SimulationConfig simConfig = loadSimulationConfig(inputConfigFile);

	if (findTapingConfig)
	{
		EvolutionConfig evolutionConfig = loadEvolutionConfig(inputConfigFile);
		std::unique_ptr<EvolutionSimulation> simulation(new EvolutionSimulation());

		simulation->simulateEvolution(simConfig, evolutionConfig);
	}
	else
	{
		ShellConfig shellConfig = loadShellConfig(inputShellConfigFile);
		std::unique_ptr<ShellSimulation> simulation(new ShellSimulation());

		std::vector<uint16_t> layermapImage(simConfig.layermapSize * simConfig.layermapSize, 0);
		std::vector<uint16_t> scratchLayermapImage(simConfig.layermapSize * simConfig.layermapSize, 0);

		simulation->simulateTaping(shellConfig, simConfig, layermapImage.data(), scratchLayermapImage.data());

		writeOutput("heatmap.png", OUTPUT_TYPE_HEATMAP_PNG, layermapImage.data(), simConfig.layermapSize);
		writeOutput("layermap.png", OUTPUT_TYPE_LAYERMAP_PNG, layermapImage.data(), simConfig.layermapSize);
		std::cout << "Finished simulation and wrote output to files \"heatmap.png\" and \"layermap.png\" " << std::endl;

		if (calcError > 0)
		{
			uint32_t error = simulation->computeLayermapError(simConfig, uint32_t(calcError), layermapImage.data());
			std::cout << "Error of simulation is: " << error << std::endl;
		}
	}

	system("pause");

	return 0;
}

void parseCommandLineArgs(int argc, char *argv[])
{
	for (int i = 0; i < argc; i++)
	{
		if (strcmp(argv[i], "--help") == 0)
		{
			printHelp();
			exit(0);
		}
		else if (strcmp(argv[i], "--find") == 0)
		{
			findTapingConfig = true;
		}
		else if (strcmp(argv[i], "-i") == 0 && i < argc - 1)
		{
			inputConfigFile = argv[i + 1];
			i++;
		}
		else if (strcmp(argv[i], "-s") == 0 && i < argc - 1)
		{
			inputShellConfigFile = argv[i + 1];
			i++;
		}
		else if (strcmp(argv[i], "-e") == 0 && i < argc - 1)
		{
			calcError = std::stoi(argv[i + 1]);
			i++;
		}
	}
}

void printHelp()
{
	std::cout << "--help\t\tBring up this help menu" << std::endl;
	std::cout << "--find\t\tRun an algorithm to find the best taping method given the configured parameters" << std::endl;
	std::cout << "-i <file>\tLoads <file> as the simulation config file, defaults to \"shell-config.json\"" << std::endl;
	std::cout << "-s <file>\tLoads <file> as the shell config file, defaults to \"shell-config.json\"" << std::endl;
	std::cout << "-e <layers>\tCalculates the error of the shell config given a number of layers" << std::endl;
}

void writeOutput(std::string file, OutputType type, uint16_t *layermapImage, uint32_t layermapSize)
{
	switch (type)
	{
		case OUTPUT_TYPE_LAYERMAP_PNG:
		{
			std::vector<uint8_t> outputImage(layermapSize * layermapSize);

			for (uint32_t i = 0; i < layermapSize * layermapSize; i++)
				outputImage[i] = uint8_t(std::min<uint32_t>(layermapImage[i], 255));

			lodepng::encode(file, outputImage.data(), layermapSize, layermapSize, LCT_GREY, 8);

			break;
		}
		case OUTPUT_TYPE_HEATMAP_PNG:
		{
			heatmap_t *heatmap = heatmap_new(layermapSize, layermapSize);
			heatmap_stamp_t *stamp = heatmap_stamp_gen(1);

			for (uint32_t x = 0; x < layermapSize; x++)
				for (uint32_t y = 0; y < layermapSize; y++)
					for (uint8_t i = 0; i < layermapImage[y * layermapSize + x]; i++)
						heatmap_add_point_with_stamp(heatmap, x, y, stamp);

			std::vector<uint8_t> heatmapImage(layermapSize * layermapSize * 4);
			heatmap_render_to(heatmap, heatmap_cs_RdYlBu_discrete, &heatmapImage[0]);

			lodepng::encode(file, heatmapImage.data(), layermapSize, layermapSize);

			break;
		}
	}
}

ShellConfig loadShellConfig(const std::string &file)
{
	std::ifstream fileStream(file);

	if (!fileStream.is_open())
	{
		std::cout << "Failed to open shell config file: \"" << file << "\"!" << std::endl;
		exit(-1);
	}

	json jsonConfig;

	try
	{
		fileStream >> jsonConfig;
	}
	catch (std::exception &e)
	{
		std::cout << e.what() << std::endl;
		fileStream.close();
		exit(-1);
	}

	ShellConfig shellConfig = {};
	shellConfig.numAngles = jsonConfig["numAngles"];
	shellConfig.shellDiameter = jsonConfig["shellDiameter"];
	shellConfig.tapeWidth = jsonConfig["tapeWidth"];
	shellConfig.shellChuckDiameter = jsonConfig["shellChuckDiameter"];

	json shellArmAngles = jsonConfig["shellArmAngles"];
	json shellStepperSpeedFraction = jsonConfig["shellStepperSpeedFraction"];
	json rimRotationsUntilNextAngle = jsonConfig["rimRotationsUntilNextAngle"];

	for (auto &elem : shellArmAngles)
		shellConfig.shellArmAngles.push_back(elem);

	for (auto &elem : shellStepperSpeedFraction)
		shellConfig.shellStepperSpeed.push_back((1.0f / (elem * 2.0f)));

	for (auto &elem : rimRotationsUntilNextAngle)
		shellConfig.rimRotationsUntilNextAngle.push_back(elem * 2.0f);

	fileStream.close();

	return shellConfig;
}

SimulationConfig loadSimulationConfig(const std::string &file)
{
	std::ifstream fileStream(file);

	if (!fileStream.is_open())
	{
		std::cout << "Failed to open simulation config file: \"" << file << "\"!" << std::endl;
		exit(-1);
	}

	json jsonConfig;

	try
	{
		fileStream >> jsonConfig;
	}
	catch (std::exception &e)
	{
		std::cout << e.what() << std::endl;
		fileStream.close();
		exit(-1);
	}

	SimulationConfig simConfig = {};
	simConfig.layermapSize = jsonConfig["layermapSize"];
	simConfig.mapFillPrecisionMult = jsonConfig["mapFillPrecisionMult"];
	simConfig.errorCalcYAxisSweeps = jsonConfig["errorCalcYAxisSweeps"];

	fileStream.close();

	return simConfig;
}

EvolutionConfig loadEvolutionConfig(const std::string &file)
{
	std::ifstream fileStream(file);

	if (!fileStream.is_open())
	{
		std::cout << "Failed to open evolution config file: \"" << file << "\"!" << std::endl;
		exit(-1);
	}

	json jsonConfig;

	try
	{
		fileStream >> jsonConfig;
	}
	catch (std::exception &e)
	{
		std::cout << e.what() << std::endl;
		fileStream.close();
		exit(-1);
	}

	EvolutionConfig evolutionConfig = {};

	if (jsonConfig.contains("SearchConfig"))
	{
		json configEntry = jsonConfig["SearchConfig"];
		evolutionConfig.numAngles = configEntry["numAngles"];
		evolutionConfig.shellDiameter = configEntry["shellDiameter"];
		evolutionConfig.tapeWidth = configEntry["tapeWidth"];
		evolutionConfig.shellChuckDiameter = configEntry["shellChuckDiameter"];
		evolutionConfig.targetLayers = configEntry["targetLayers"];
		evolutionConfig.maxGenerations = configEntry["maxGenerations"];
		evolutionConfig.populationSize = configEntry["populationSize"];
		evolutionConfig.elitePercentage = configEntry["elitePercentage"];
		evolutionConfig.randomPercentage = configEntry["randomPercentage"];
		evolutionConfig.maxMutationPercentage = configEntry["maxMutationPercentage"];
		evolutionConfig.minShellArmAngle = configEntry["minShellArmAngle"];
		//volutionConfig.maxIterations = configEntry["maxIterations"];

		for (auto &elem : configEntry["minShellArmAngles"])
			evolutionConfig.minShellArmAngles.push_back(elem);

		for (auto &elem : configEntry["maxShellArmAngles"])
			evolutionConfig.maxShellArmAngles.push_back(elem);

		for (auto &elem : configEntry["minShellStepperSpeedFraction"])
			evolutionConfig.minShellStepperSpeed.push_back(elem);

		for (auto &elem : configEntry["maxShellStepperSpeedFraction"])
			evolutionConfig.maxShellStepperSpeed.push_back(elem);
	}
	else
	{
		std::cout << "Could not find \"SearchConfig\" entry in config file!" << std::endl;
	}

	fileStream.close();

	return evolutionConfig;
}