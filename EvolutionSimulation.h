#pragma once

#include <cstdint>
#include <vector>

#include <ShellSimulation.h>

struct EvolutionConfig
{
	uint32_t numAngles; // Number of angles/applications per taping session
	float shellDiameter; // Outside diameter of the shell hemisphere in inches
	float tapeWidth; // Width of the tape used in inches
	float shellChuckDiameter; // The diameter of the chuck (what holds the shell in place) in diameter

	uint32_t targetLayers; // The number of layers intended to be put on the shell
	uint32_t maxGenerations; // The maximum number of generations to simulate before exiting
	uint32_t populationSize; // How many members of the populatin to maintain
	float elitePercentage; // The number of the best in the population to preserve each generation (aka they aren't killed off)
	float randomPercentage; // The percentage each generation that is filled with a random genome, to keep the gene pool fresh
	float maxMutationPercentage; // The max percentage to mutate each gene in a population member when reproducing
	float minShellArmAngle; // The minimum angle physically allowed

	// The min and max angles per application that will be searched
	std::vector<float> minShellArmAngles;
	std::vector<float> maxShellArmAngles;

	// The min and max stepper speeds per angle/application
	std::vector<float> minShellStepperSpeed;
	std::vector<float> maxShellStepperSpeed;
};

struct PopulationMember
{
	ShellConfig config;
	uint32_t fitness;
};

struct EvolutionFitnessJobData
{
	ShellSimulation simulator;
	std::vector<PopulationMember> *population;
	SimulationConfig simConfig;
	EvolutionConfig evoConfig;
	uint32_t threadNum;
	std::vector<uint16_t> layermap, scratchLayermap;
};

class EvolutionSimulation
{
public:

	EvolutionSimulation();
	virtual ~EvolutionSimulation();

	/*
	Simulates a population's evolution to find the optimal settings/config for the parameters specified. Writes the best
	to a specified output file.
	*/
	void simulateEvolution(const SimulationConfig &simConfig, const EvolutionConfig &evoConfig);

private:
	std::vector<PopulationMember> initializePopulation(const EvolutionConfig &evoConfig);
	void simulateNaturalSelection(std::vector<PopulationMember> &population, const EvolutionConfig &evoConfig);
	PopulationMember breedPopulationMembers(const PopulationMember &first, const PopulationMember &second, const EvolutionConfig &evoConfig);
	
};

