#include "EvolutionSimulation.h"

#include <algorithm>
#include <iostream>
#include <fstream>
#include <iomanip>

#include <nlohmann/json.hpp>
#include <JobSystem.h>

using json = nlohmann::json;

EvolutionSimulation::EvolutionSimulation()
{

}

EvolutionSimulation::~EvolutionSimulation()
{

}

void EvolutionSimulation_evaluatePopulationFitnessJob(Job *job)
{
	EvolutionFitnessJobData &jobData = *reinterpret_cast<EvolutionFitnessJobData *>(job->usrData);
	const float threadPopulationChunkSize = jobData.population->size() / float(JobSystem::get()->getWorkerCount());
 
	for (uint32_t i = uint32_t(threadPopulationChunkSize * jobData.threadNum); i < std::min<uint32_t>(uint32_t(threadPopulationChunkSize * (jobData.threadNum + 1)), jobData.population->size()); i++)
	{
		memset(jobData.layermap.data(), 0, jobData.layermap.size() * sizeof(jobData.layermap[0]));
		memset(jobData.scratchLayermap.data(), 0, jobData.scratchLayermap.size() * sizeof(jobData.scratchLayermap[0]));

		jobData.simulator.simulateTaping((*jobData.population)[i].config, jobData.simConfig, jobData.layermap.data(), jobData.scratchLayermap.data());
		(*jobData.population)[i].fitness = jobData.simulator.computeLayermapError(jobData.simConfig, jobData.evoConfig.targetLayers, jobData.layermap.data());
	}
}

void EvolutionSimulation::simulateEvolution(const SimulationConfig &simConfig, const EvolutionConfig &evoConfig)
{
	std::vector<PopulationMember> population = initializePopulation(evoConfig);

	std::vector<EvolutionFitnessJobData> jobsData;

	for (uint32_t t = 0; t < JobSystem::get()->getWorkerCount(); t++)
	{
		EvolutionFitnessJobData jobData = {};
		jobData.population = &population;
		jobData.simConfig = simConfig;
		jobData.evoConfig = evoConfig;
		jobData.threadNum = t;
		jobData.layermap = std::vector<uint16_t>(simConfig.layermapSize * simConfig.layermapSize);
		jobData.scratchLayermap = std::vector<uint16_t>(simConfig.layermapSize * simConfig.layermapSize);

		jobsData.push_back(jobData);
	}

	for (uint32_t g = 0; g < evoConfig.maxGenerations; g++)
	{
		std::vector<Job *> jobs;

		for (uint32_t t = 0; t < JobSystem::get()->getWorkerCount(); t++)
		{
			jobs.push_back(JobSystem::get()->allocateJob(&EvolutionSimulation_evaluatePopulationFitnessJob));
			jobs.back()->usrData = reinterpret_cast<void *>(&jobsData[t]);
		}

		JobSystem::get()->runJobs(jobs);

		for (size_t j = 0; j < jobs.size(); j++)
			JobSystem::get()->waitForJob(jobs[j], true);

		// Sort smallest to largest
		std::sort(population.begin(), population.end(), [](const PopulationMember &first, const PopulationMember &second)
			{
				return first.fitness < second.fitness;
			});

		json shellConfigJSON;
		shellConfigJSON["numAngles"] = population[0].config.numAngles;
		shellConfigJSON["shellDiameter"] = population[0].config.shellDiameter;
		shellConfigJSON["tapeWidth"] = population[0].config.tapeWidth;
		shellConfigJSON["shellChuckDiameter"] = population[0].config.shellChuckDiameter;
		shellConfigJSON["shellArmAngles"] = population[0].config.shellArmAngles;

		std::vector<float> shellStepperSpeedFraction;

		for (uint32_t a = 0; a < population[0].config.numAngles; a++)
			shellStepperSpeedFraction.push_back(0.5f / population[0].config.shellStepperSpeed[a]);

		shellConfigJSON["shellStepperSpeedFraction"] = shellStepperSpeedFraction;
		shellConfigJSON["rimRotationsUntilNextAngle"] = shellStepperSpeedFraction;

		std::ofstream lowestErrorResultFile("best-config.json");

		if (!lowestErrorResultFile.is_open())
		{
			std::cout << "Failed to open file stream for output file: \"" << "best-config.json" << "\" to write results of simulation!" << std::endl;
			exit(-1);
		}

		lowestErrorResultFile << std::setw(4) << shellConfigJSON << std::endl;
		lowestErrorResultFile.close();

		std::cout << "Generation " << g << ", best fitness: " << population[0].fitness << ", saved to \"best-config.json\"" << std::endl;

		simulateNaturalSelection(population, evoConfig);
	}
}

void EvolutionSimulation::simulateNaturalSelection(std::vector<PopulationMember> &population, const EvolutionConfig &evoConfig)
{
	uint32_t eliteCount = uint32_t(evoConfig.populationSize * evoConfig.elitePercentage);
	uint32_t randomCount = uint32_t(evoConfig.populationSize * evoConfig.randomPercentage);

	// Kill off the last "randomPercentage" and replace them with random members to keep the gene pool interesting
	EvolutionConfig randomPopulationConfig = evoConfig;
	randomPopulationConfig.populationSize = randomCount;
	std::vector<PopulationMember> randomPopulation = initializePopulation(randomPopulationConfig);

	// Kill off the middle percentage and replace them with children
	std::vector<PopulationMember> children;

	for (uint32_t c = 0; c < evoConfig.populationSize - eliteCount - randomCount; c++)
		children.push_back(breedPopulationMembers(population[rand() % (evoConfig.populationSize - randomCount)], population[rand() % (evoConfig.populationSize - randomCount)], evoConfig));

	// Replace the old population with the new members (excluding the elite)
	population.erase(population.begin() + eliteCount, population.end());
	population.insert(population.end(), children.begin(), children.end());
	population.insert(population.end(), randomPopulation.begin(), randomPopulation.end());
}

PopulationMember EvolutionSimulation::breedPopulationMembers(const PopulationMember &first, const PopulationMember &second, const EvolutionConfig &evoConfig)
{
	PopulationMember member = {};
	member.fitness = 0;
	member.config.numAngles = evoConfig.numAngles;
	member.config.shellDiameter = evoConfig.shellDiameter;
	member.config.tapeWidth = evoConfig.tapeWidth;
	member.config.shellChuckDiameter = evoConfig.shellChuckDiameter;

	for (uint32_t a = 0; a < member.config.numAngles; a++)
	{
		// Breed angle and speed, it may technically be more correct to pick the gene of ONE parent, but for this application it may be better to randomly lerp between parents
		float angleLerp = rand() / float(RAND_MAX);
		float speedLerp = rand() / float(RAND_MAX);
		float bredAngle = first.config.shellArmAngles[a] * (1.0f - angleLerp) + second.config.shellArmAngles[a] * angleLerp;
		float bredSpeed = first.config.shellStepperSpeed[a] * (1.0f - speedLerp) + second.config.shellStepperSpeed[a] * speedLerp;

		// Mutate the angle and speed a little
		bredAngle *= 1.0f + evoConfig.maxMutationPercentage * (rand() % 10 < 5 ? 1.0f : -1.0f) * (rand() / float(RAND_MAX));
		bredSpeed *= 1.0f + evoConfig.maxMutationPercentage * (rand() % 10 < 5 ? 1.0f : -1.0f) * (rand() / float(RAND_MAX));

		// Keep the angle within the physical limits
		bredAngle = std::max(std::min(bredAngle, 90.0f), evoConfig.minShellArmAngle);

		member.config.shellArmAngles.push_back(bredAngle);
		member.config.shellStepperSpeed.push_back(bredSpeed);
		member.config.rimRotationsUntilNextAngle.push_back(1.0f / bredSpeed);
	}

	return member;
}

std::vector<PopulationMember> EvolutionSimulation::initializePopulation(const EvolutionConfig &evoConfig)
{
	std::vector<PopulationMember> population(evoConfig.populationSize);
	uint32_t populationSizeSqrt = uint32_t(std::sqrt(evoConfig.populationSize));

	for (uint32_t i = 0; i < evoConfig.populationSize; i++)
	{
		ShellConfig shellConfig = {};
		shellConfig.numAngles = evoConfig.numAngles;
		shellConfig.shellDiameter = evoConfig.shellDiameter;
		shellConfig.tapeWidth = evoConfig.tapeWidth;
		shellConfig.shellChuckDiameter = evoConfig.shellChuckDiameter;

		for (uint32_t a = 0; a < shellConfig.numAngles; a++)
		{
			float lerpFactor = float(i % populationSizeSqrt) / float(populationSizeSqrt - 1);
			float speedLerpFactor = float(i / populationSizeSqrt) / float(populationSizeSqrt);
			float angle = evoConfig.minShellArmAngles[a] * (1.0f - lerpFactor) + evoConfig.maxShellArmAngles[a] * lerpFactor;
			float speed = evoConfig.minShellStepperSpeed[a] * (1.0f - speedLerpFactor) + evoConfig.maxShellStepperSpeed[a] * speedLerpFactor;

			shellConfig.shellArmAngles.push_back(angle);
			shellConfig.shellStepperSpeed.push_back((1.0f / speed) * 0.5f);
			shellConfig.rimRotationsUntilNextAngle.push_back((speed) * 0.5f);
		}

		PopulationMember &member = population[i];
		member.config = shellConfig;
		member.fitness = 0;
	}

	return population;
}
